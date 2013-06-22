//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2004 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "stdafx.h"
#include <vd2/Riza/capdriver.h>
#include <vd2/Dita/services.h>
#include <vd2/system/atomic.h>
#include <vd2/system/error.h>
#include <vd2/system/vdstring.h>
#include <vd2/system/refcount.h>
#include <vd2/system/registry.h>
#include <vd2/system/time.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/VDRingBuffer.h>
#include "InputFile.h"
#include "VideoDisplay.h"
#include "AVIAudioOutput.h"

using namespace nsVDCapture;

extern HINSTANCE g_hInst;

extern const char g_szError[];

class VDCaptureDriverEmulation : public IVDCaptureDriver, public IVDTimerCallback {
	VDCaptureDriverEmulation(const VDCaptureDriverEmulation&);
	VDCaptureDriverEmulation& operator=(const VDCaptureDriverEmulation&);
public:
	VDCaptureDriverEmulation();
	~VDCaptureDriverEmulation();

	void	SetCallback(IVDCaptureDriverCallback *pCB);
	bool	Init(VDGUIHandle hParent);
	void	Shutdown();

	void	SetDisplayMode(nsVDCapture::DisplayMode m);
	nsVDCapture::DisplayMode		GetDisplayMode();

	void	SetDisplayRect(const vdrect32& r);
	vdrect32	GetDisplayRectAbsolute();
	void	SetDisplayVisibility(bool vis);

	void	SetFramePeriod(sint32 ms);
	sint32	GetFramePeriod();

	uint32	GetPreviewFrameCount();

	bool	GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat);
	bool	SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size);

	bool	IsAudioCapturePossible();
	bool	IsAudioCaptureEnabled();
	void	SetAudioCaptureEnabled(bool b);

	bool	GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat);
	bool	SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size);

	bool	IsDriverDialogSupported(nsVDCapture::DriverDialog dlg);
	void	DisplayDriverDialog(nsVDCapture::DriverDialog dlg);

	bool	CaptureStart();
	void	CaptureStop();
	void	CaptureAbort();

protected:
	void	UpdateDisplayMode();
	void	CloseInputFile();
	void	OpenInputFile(const wchar_t *fn);
	void	TimerCallback();

	static LRESULT CALLBACK StaticMessageWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	HWND	mhwnd;
	HWND	mhwndParent;
	HWND	mhwndMessages;
	IVDCaptureDriverCallback	*mpCB;
	bool	mbAudioCaptureEnabled;
	bool	mbCapturing;
	VDAtomicInt	mDisplayMode;

	vdrefptr<InputFile>	mpInputFile;
	IVDVideoSource	*mpVideo;
	AudioSource		*mpAudio;
	VDPosition		mAudioPos;

	IVDVideoDisplay	*mpDisplay;

	VDCallbackTimer	mFrameTimer;
	VDPosition		mFrame;
	VDPosition		mFrameCount;

	VDTime			mCaptureStart;

	AVIAudioOutput	mAudioOutput;
	UINT			mAudioTimer;
	uint32			mAudioSampleSize;
	vdblock<char>	mAudioClientBuffer;
	VDRingBuffer<char>	mAudioRecordBuffer;

	VDAtomicInt		mPreviewFrameCount;

	static ATOM sMsgWndClass;
};

ATOM VDCaptureDriverEmulation::sMsgWndClass = NULL;

VDCaptureDriverEmulation::VDCaptureDriverEmulation()
	: mhwnd(NULL)
	, mhwndParent(NULL)
	, mhwndMessages(NULL)
	, mpCB(NULL)
	, mbAudioCaptureEnabled(true)
	, mbCapturing(false)
	, mDisplayMode(kDisplayNone)
	, mpVideo(NULL)
	, mpAudio(NULL)
	, mpDisplay(NULL)
	, mAudioOutput(0, 0)
	, mAudioTimer(0)
{
}

VDCaptureDriverEmulation::~VDCaptureDriverEmulation() {
	Shutdown();
}

void VDCaptureDriverEmulation::SetCallback(IVDCaptureDriverCallback *pCB) {
	mpCB = pCB;
}

bool VDCaptureDriverEmulation::Init(VDGUIHandle hParent) {
	if (!sMsgWndClass) {
		WNDCLASS wc;

		wc.style			= 0;
		wc.lpfnWndProc		= StaticMessageWndProc;
		wc.cbClsExtra		= 0;
		wc.cbWndExtra		= sizeof(void *);
		wc.hInstance		= g_hInst;
		wc.hIcon			= NULL;
		wc.hCursor			= NULL;
		wc.hbrBackground	= NULL;
		wc.lpszMenuName		= NULL;
		wc.lpszClassName	= "RizaAudioEmulator";

		sMsgWndClass = RegisterClass(&wc);
		if (!sMsgWndClass)
			return false;
	}

	mhwndParent = (HWND)hParent;

	mhwndMessages = CreateWindowEx(0, (LPCTSTR)sMsgWndClass, "", WS_POPUP, 0, 0, 0, 0, NULL, NULL, g_hInst, NULL);
	if (!mhwndMessages)
		return false;

	SetWindowLongPtr(mhwndMessages, 0, (LONG_PTR)this);

	mhwnd = CreateWindowEx(0, VIDEODISPLAYCONTROLCLASS, "", WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 64, 64, mhwndParent, (HMENU)0, g_hInst, NULL);
	if (!mhwnd) {
		DestroyWindow(mhwnd);
		return false;
	}

	mpDisplay = VDGetIVideoDisplay(mhwnd);
	mPreviewFrameCount = 0;

	VDRegistryAppKey key("Capture");
	VDStringW filename;

	if (key.getString("Emulation file", filename))
		OpenInputFile(filename.c_str());

	return true;
}

void VDCaptureDriverEmulation::Shutdown() {
	CloseInputFile();

	mpDisplay = NULL;

	if (mhwnd) {
		DestroyWindow(mhwnd);
		mhwnd = NULL;
	}

	if (mhwndMessages) {
		DestroyWindow(mhwndMessages);
		mhwndMessages = NULL;
	}
}

void VDCaptureDriverEmulation::SetDisplayMode(DisplayMode mode) {
	if (mode == mDisplayMode)
		return;

	mDisplayMode = mode;

	UpdateDisplayMode();
}

DisplayMode VDCaptureDriverEmulation::GetDisplayMode() {
	return (DisplayMode)(int)mDisplayMode;
}

void VDCaptureDriverEmulation::SetDisplayRect(const vdrect32& r) {
	SetWindowPos(mhwnd, NULL, r.left, r.top, r.width(), r.height(), SWP_NOZORDER|SWP_NOACTIVATE);
}

vdrect32 VDCaptureDriverEmulation::GetDisplayRectAbsolute() {
	RECT r;
	GetWindowRect(mhwnd, &r);
	MapWindowPoints(GetParent(mhwnd), NULL, (LPPOINT)&r, 2);
	return vdrect32(r.left, r.top, r.right, r.bottom);
}

void VDCaptureDriverEmulation::SetDisplayVisibility(bool vis) {
	ShowWindow(mhwnd, vis ? SW_SHOWNA : SW_HIDE);
}

void VDCaptureDriverEmulation::SetFramePeriod(sint32 ms) {
}

sint32 VDCaptureDriverEmulation::GetFramePeriod() {
	return mpVideo ? (sint32)mpVideo->asStream()->getRate().scale64ir(1000000) : 1000000 / 15;
}

uint32 VDCaptureDriverEmulation::GetPreviewFrameCount() {
	return mPreviewFrameCount;
}

bool VDCaptureDriverEmulation::GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat) {
	if (!mpVideo) {
		vformat.resize(sizeof(BITMAPINFOHEADER));
		vformat->biSize = sizeof(BITMAPINFOHEADER);
		vformat->biWidth = 320;
		vformat->biHeight = 240;
		vformat->biPlanes = 1;
		vformat->biBitCount = 32;
		vformat->biSizeImage = 320*240*4;
		vformat->biCompression = BI_RGB;
		vformat->biXPelsPerMeter = 0;
		vformat->biYPelsPerMeter = 0;
		vformat->biClrUsed = 0;
		vformat->biClrImportant = 0;
		return true;
	} else {
		const BITMAPINFOHEADER *format = mpVideo->getDecompressedFormat();

		vformat.assign(format, VDGetSizeOfBitmapHeaderW32(format));
		return true;
	}
}

bool VDCaptureDriverEmulation::SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size) {
	if (mpVideo) {
		mpDisplay->Reset();
		bool success = mpVideo->setDecompressedFormat(pbih);
		UpdateDisplayMode();
		return success;
	}
	return false;
}

bool VDCaptureDriverEmulation::IsAudioCapturePossible() {
	return mpAudio != 0;
}

bool VDCaptureDriverEmulation::IsAudioCaptureEnabled() {
	return mpAudio != 0 && mbAudioCaptureEnabled;
}

void VDCaptureDriverEmulation::SetAudioCaptureEnabled(bool b) {
	mbAudioCaptureEnabled = b;
}

bool VDCaptureDriverEmulation::GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat) {
	if (mpAudio) {
		aformat.assign(mpAudio->getWaveFormat(), mpAudio->getFormatLen());
		return true;
	}
	return false;
}

bool VDCaptureDriverEmulation::SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size) {
	return false;
}

bool VDCaptureDriverEmulation::IsDriverDialogSupported(DriverDialog dlg) {
	return dlg == kDialogVideoSource;
}

void VDCaptureDriverEmulation::DisplayDriverDialog(DriverDialog dlg) {
	VDASSERT(IsDriverDialogSupported(dlg));

	if (dlg == kDialogVideoSource) {
		tVDInputDrivers inDrivers;
		std::vector<int> xlat;

		VDGetInputDrivers(inDrivers, IVDInputDriver::kF_Video);
		VDStringW fileFilters(VDMakeInputDriverFileFilter(inDrivers, xlat));

		const VDStringW fn(VDGetLoadFileName('cpem', (VDGUIHandle)mhwndParent, L"Load video capture emulation clip", fileFilters.c_str(), NULL, NULL, NULL));

		if (!fn.empty()) {
			CloseInputFile();
			try {
				OpenInputFile(fn.c_str());

				VDRegistryAppKey key("Capture");

				key.setString("Emulation file", fn.c_str());
			} catch(const MyError& e) {
				e.post(mhwndParent, g_szError);
			}
		}
	}
}

bool VDCaptureDriverEmulation::CaptureStart() {
	if (!VDINLINEASSERTFALSE(mbCapturing)) {
		mbCapturing = true;

		if (mpCB)
			mpCB->CapBegin(0);

		mAudioRecordBuffer.Flush();
		mCaptureStart = VDGetPreciseTick();
	}

	return mbCapturing;
}

void VDCaptureDriverEmulation::CaptureStop() {
	if (VDINLINEASSERT(mbCapturing)) {
		if (mpCB)
			mpCB->CapEnd();
		mbCapturing = false;
	}
}

void VDCaptureDriverEmulation::CaptureAbort() {
	if (mbCapturing) {
		if (mpCB)
			mpCB->CapEnd();
		mbCapturing = false;
	}
}

void VDCaptureDriverEmulation::UpdateDisplayMode() {
	switch(mDisplayMode) {
	case kDisplayHardware:
	case kDisplaySoftware:
		if (mpVideo) {
			mpDisplay->SetSourcePersistent(true, mpVideo->getTargetFormat());
			ShowWindow(mhwnd, SW_SHOWNA);
			break;
		}
		// fall through
	case kDisplayNone:
	case kDisplayAnalyze:
		mpDisplay->Reset();
		ShowWindow(mhwnd, SW_HIDE);
		break;
	}
}

void VDCaptureDriverEmulation::CloseInputFile() {
	mFrameTimer.Shutdown();
	if (mAudioTimer) {
		KillTimer(mhwndMessages, mAudioTimer);
		mAudioTimer = 0;
	}

	if (mpDisplay)
		mpDisplay->Reset();

	mAudioOutput.stop();
	mAudioClientBuffer.clear();
	mAudioRecordBuffer.Shutdown();

	mpInputFile = NULL;
	mpAudio = NULL;
	mpVideo = NULL;
}

void VDCaptureDriverEmulation::OpenInputFile(const wchar_t *fn) {
	IVDInputDriver *pDriver = VDAutoselectInputDriverForFile(fn);

	mpInputFile = pDriver->CreateInputFile(0);
	if (!mpInputFile)
		throw MyMemoryError();

	mpInputFile->Init(fn);

	mpVideo = mpInputFile->videoSrc;
	mpAudio = mpInputFile->audioSrc;

	if (mpVideo) {
		mFrame = 0;
		mFrameCount = mpVideo->asStream()->getLength();

		mpVideo->setTargetFormat(0);
		UpdateDisplayMode();

		mFrameTimer.Init(this, (sint32)mpVideo->asStream()->getRate().scale64ir(1000));
	}

	if (mpAudio) {
		const WAVEFORMATEX *pwfex = mpAudio->getWaveFormat();

		if (pwfex->wFormatTag == WAVE_FORMAT_PCM) {
			sint32 audioSamplesPerBlock = (pwfex->nAvgBytesPerSec / 5 + pwfex->nBlockAlign - 1) / pwfex->nBlockAlign;
			sint32 bytesPerBlock = audioSamplesPerBlock * pwfex->nBlockAlign;

			mAudioOutput.setBuffering(bytesPerBlock, 2);
			mAudioOutput.init(pwfex);
			mAudioOutput.start();
			mAudioClientBuffer.resize(bytesPerBlock);
			mAudioRecordBuffer.Init(bytesPerBlock * 2);

			mAudioSampleSize = pwfex->nBlockAlign;

			mAudioTimer = SetTimer(mhwndMessages, 1, 10, NULL);
			mAudioPos = 0;
		} else
			mpAudio = NULL;
	}
}

void VDCaptureDriverEmulation::TimerCallback() {
	PostMessage(mhwndMessages, WM_APP, 0, 0);
}

LRESULT CALLBACK VDCaptureDriverEmulation::StaticMessageWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_NCCREATE)
		SetWindowLongPtr(hwnd, 0, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
	else if (msg == WM_APP) {
		VDCaptureDriverEmulation *const pThis = (VDCaptureDriverEmulation *)GetWindowLongPtr(hwnd, 0);

		int m = pThis->mDisplayMode;

		VDTime clk = (VDTime)((VDGetPreciseTick() - pThis->mCaptureStart) * 1000000.0 / VDGetPreciseTicksPerSecond());

		if (pThis->mpAudio) {
			double audioRate = (double)pThis->mpAudio->getRate();
			VDPosition samples = (VDPosition)(pThis->mAudioOutput.position() / 1000.0 * audioRate) % pThis->mpAudio->getLength();
			pThis->mFrame = (VDPosition)(samples / audioRate * (double)pThis->mpVideo->asStream()->getRate());
		}

		try {
			pThis->mpVideo->getFrame(pThis->mFrame);

			if (pThis->mbCapturing) {
				const VDPixmap& px = pThis->mpVideo->getTargetFormat();
				pThis->mpCB->CapProcessData(0, pThis->mpVideo->getFrameBuffer(), (px.pitch<0?-px.pitch:px.pitch) * px.h, clk, true, clk);
			}
		} catch(const MyError&) {
			m = 0;
		}

		if (!pThis->mpAudio && ++pThis->mFrame >= pThis->mFrameCount)
			pThis->mFrame = 0;

		if (m) {
			++pThis->mPreviewFrameCount;
			if (!pThis->mbCapturing && m == kDisplayAnalyze) {
				const VDPixmap& px = pThis->mpVideo->getTargetFormat();
				pThis->mpCB->CapProcessData(-1, pThis->mpVideo->getFrameBuffer(), (px.pitch < 0 ? -px.pitch : px.pitch) * px.h, clk, true, clk);
			}

			if (m == kDisplayHardware || m == kDisplaySoftware)
				pThis->mpDisplay->PostUpdate();
		}

	} else if (msg == WM_TIMER) {
		VDCaptureDriverEmulation *const pThis = (VDCaptureDriverEmulation *)GetWindowLongPtr(hwnd, 0);

		while(long availbytes = pThis->mAudioOutput.avail()) {
			char buf[8192];
			uint32 bytes, samples;

			if (pThis->mpAudio->read(pThis->mAudioPos, availbytes, buf, availbytes, &bytes, &samples) || !samples) {
				if (!pThis->mAudioPos)
					break;
				pThis->mAudioPos = 0;
				continue;
			}

			pThis->mAudioOutput.write(buf, bytes, INFINITE);
			pThis->mAudioPos += samples;

			// attempt to fill audio recording buffer... if full, send out a client block
			if (pThis->mbCapturing) {
				const char *src = buf;
				while(bytes > 0) {
					int actualBytes;
					void *dst = pThis->mAudioRecordBuffer.LockWrite(bytes, actualBytes);

					if (!actualBytes) {
						VDTime clk = (VDTime)((VDGetPreciseTick() - pThis->mCaptureStart) * 1000000.0 / VDGetPreciseTicksPerSecond());
						const uint32 size = pThis->mAudioClientBuffer.size();
						char *data = pThis->mAudioClientBuffer.data();
						pThis->mAudioRecordBuffer.Read(data, size);

#if 0
						uint32 newsize = size - (size >> 4);
						newsize -= newsize % pThis->mAudioSampleSize;
						pThis->mpCB->CapProcessData(1, data, newsize, clk, false, clk);
#else
						pThis->mpCB->CapProcessData(1, data, size, clk, false, clk);
#endif
					}

					memcpy(dst, src, actualBytes);
					src += actualBytes;
					bytes -= actualBytes;
					pThis->mAudioRecordBuffer.UnlockWrite(actualBytes);
				}
			}
		}
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////

class VDCaptureSystemEmulation : public IVDCaptureSystem {
public:
	VDCaptureSystemEmulation();
	~VDCaptureSystemEmulation();

	void EnumerateDrivers();

	int GetDeviceCount();
	const wchar_t *GetDeviceName(int index);

	IVDCaptureDriver *CreateDriver(int deviceIndex);

protected:
	HMODULE	mhmodAVICap;
	int mDriverCount;
	VDStringW mDrivers[10];
};

IVDCaptureSystem *VDCreateCaptureSystemEmulation() {
	return new VDCaptureSystemEmulation;
}

VDCaptureSystemEmulation::VDCaptureSystemEmulation() {
}

VDCaptureSystemEmulation::~VDCaptureSystemEmulation() {
}

void VDCaptureSystemEmulation::EnumerateDrivers() {
}

int VDCaptureSystemEmulation::GetDeviceCount() {
	return 1;
}

const wchar_t *VDCaptureSystemEmulation::GetDeviceName(int index) {
	return L"Video file (Emulation)";
}

IVDCaptureDriver *VDCaptureSystemEmulation::CreateDriver(int index) {
	if ((unsigned)index >= 1)
		return NULL;

	return new VDCaptureDriverEmulation();
}
