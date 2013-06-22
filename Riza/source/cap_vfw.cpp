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

#include <vd2/Riza/capdriver.h>
#include <vd2/system/vdstring.h>
#include <vd2/system/error.h>
#include <windows.h>
#include <vfw.h>

using namespace nsVDCapture;

extern HINSTANCE g_hInst;

static const CAPTUREPARMS g_defaultCaptureParms={
	1000000/15,		// 15fps
	FALSE,			// fMakeUserHitOKForCapture
	100,			// wPercentDropForError
	TRUE,			// callbacks won't work if Yield is TRUE
	324000,			// we like index entries
	4,				// wChunkGranularity
	FALSE,			// fUsingDOSMemory
	10,				// wNumVideoRequested
	TRUE,			// fCaptureAudio
	0,				// wNumAudioRequested
	0,				// vKeyAbort
	FALSE,			// fAbortLeftMouse
	FALSE,			// fAbortRightMouse
	FALSE,			// fLimitEnabled
	0,				// wTimeLimit
	FALSE,			// fMCIControl
	FALSE,			// fStepMCIDevice
	0,0,			// dwMCIStartTime, dwMCIStopTime
	FALSE,			// fStepCaptureAt2x
	10,				// wStepCaptureAverageFrames
	0,				// dwAudioBufferSize
	FALSE,			// fDisableWriteCache
	AVSTREAMMASTER_NONE,	//	AVStreamMaster
};

class VDCaptureDriverVFW : public IVDCaptureDriver {
	VDCaptureDriverVFW(const VDCaptureDriverVFW&);
	VDCaptureDriverVFW& operator=(const VDCaptureDriverVFW&);
public:
	VDCaptureDriverVFW(HMODULE hmodAVICap, int driverIndex);
	~VDCaptureDriverVFW();

	void	SetCallback(IVDCaptureDriverCallback *pCB);
	bool	Init(VDGUIHandle hParent);
	void	Shutdown();

	bool	IsHardwareDisplayAvailable();

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
	void	SetAudioAnalysisEnabled(bool b);

	void	GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats);

	bool	GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat);
	bool	SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size);

	bool	IsDriverDialogSupported(nsVDCapture::DriverDialog dlg);
	void	DisplayDriverDialog(nsVDCapture::DriverDialog dlg);

	bool	CaptureStart();
	void	CaptureStop();
	void	CaptureAbort();

protected:
	void	SyncCaptureStop();
	void	SyncCaptureAbort();
	bool	InitWaveAnalysis();
	void	ShutdownWaveAnalysis();

	static LRESULT CALLBACK ErrorCallback(HWND hWnd, int nID, LPCSTR lpsz);
	static LRESULT CALLBACK StatusCallback(HWND hWnd, int nID, LPCSTR lpsz);
	static LRESULT CALLBACK ControlCallback(HWND hwnd, int nState);
	static LRESULT CALLBACK PreviewCallback(HWND hWnd, VIDEOHDR *lpVHdr);
	static LRESULT CALLBACK VideoCallback(HWND hWnd, LPVIDEOHDR lpVHdr);
	static LRESULT CALLBACK WaveCallback(HWND hWnd, LPWAVEHDR lpWHdr);
	static LRESULT CALLBACK StaticMessageSinkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	HWND	mhwnd;
	HWND	mhwndParent;
	HWND	mhwndEventSink;
	HMODULE	mhmodAVICap;

	bool	mbCapturing;
	bool	mbAudioAnalysisEnabled;
	bool	mbAudioAnalysisActive;

	IVDCaptureDriverCallback	*mpCB;
	DisplayMode			mDisplayMode;

	int		mDriverIndex;
	VDAtomicInt	mPreviewFrameCount;

	HWAVEIN mhWaveIn;
	WAVEHDR	mWaveBufHdrs[2];
	vdblock<char>	mWaveBuffer;

	CAPDRIVERCAPS		mCaps;

	MyError	mCaptureError;

	static ATOM sMsgSinkClass;
};

ATOM VDCaptureDriverVFW::sMsgSinkClass;

VDCaptureDriverVFW::VDCaptureDriverVFW(HMODULE hmodAVICap, int driverIndex)
	: mhwnd(NULL)
	, mhwndParent(NULL)
	, mhwndEventSink(NULL)
	, mhmodAVICap(hmodAVICap)
	, mpCB(NULL)
	, mbCapturing(false)
	, mbAudioAnalysisEnabled(false)
	, mbAudioAnalysisActive(false)
	, mDisplayMode(kDisplayNone)
	, mDriverIndex(driverIndex)
	, mhWaveIn(NULL)
{
	memset(mWaveBufHdrs, 0, sizeof mWaveBufHdrs);
}

VDCaptureDriverVFW::~VDCaptureDriverVFW() {
	Shutdown();
}

void VDCaptureDriverVFW::SetCallback(IVDCaptureDriverCallback *pCB) {
	mpCB = pCB;
}

bool VDCaptureDriverVFW::Init(VDGUIHandle hParent) {
	mhwndParent = (HWND)hParent;

	if (!sMsgSinkClass) {
		WNDCLASS wc = { 0, StaticMessageSinkWndProc, 0, sizeof(VDCaptureDriverVFW *), g_hInst, NULL, NULL, NULL, NULL, "Riza VFW event sink" };

		sMsgSinkClass = RegisterClass(&wc);

		if (!sMsgSinkClass)
			return false;
	}

	// Create message sink.
	if (!(mhwndEventSink = CreateWindow((LPCTSTR)sMsgSinkClass, "", WS_POPUP, 0, 0, 0, 0, mhwndParent, NULL, g_hInst, this)))
		return false;

	typedef HWND (VFWAPI *tpcapCreateCaptureWindow)(LPCSTR lpszWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hwnd, int nID);
	const tpcapCreateCaptureWindow pcapCreateCaptureWindow = (tpcapCreateCaptureWindow)GetProcAddress(mhmodAVICap, "capCreateCaptureWindowA");
	if (!VDINLINEASSERT(pcapCreateCaptureWindow))
		return false;

	mhwnd = pcapCreateCaptureWindow("", WS_CHILD, 0, 0, 0, 0, mhwndParent, 0);
	if (!mhwnd)
		return false;

	if (!capDriverConnect(mhwnd, mDriverIndex)) {
		Shutdown();
		return false;
	}

	VDVERIFY(capDriverGetCaps(mhwnd, &mCaps, sizeof mCaps));

	capCaptureSetSetup(mhwnd, &g_defaultCaptureParms, sizeof g_defaultCaptureParms);

	mPreviewFrameCount = 0;

	capSetUserData(mhwnd, this);
	capSetCallbackOnError(mhwnd, ErrorCallback);
	capSetCallbackOnStatus(mhwnd, StatusCallback);
	capSetCallbackOnFrame(mhwnd, PreviewCallback);
	capSetCallbackOnCapControl(mhwnd, ControlCallback);
	capSetCallbackOnVideoStream(mhwnd, VideoCallback);
	capSetCallbackOnWaveStream(mhwnd, WaveCallback);

	capPreviewRate(mhwnd, 1000 / 15);

	return true;
}

void VDCaptureDriverVFW::Shutdown() {
	ShutdownWaveAnalysis();

	if (mhwndEventSink) {
		DestroyWindow(mhwndEventSink);
		mhwndEventSink = NULL;
	}

	if (mhwnd) {
		capDriverDisconnect(mhwnd);
		DestroyWindow(mhwnd);
		mhwnd = NULL;
	}
}

bool VDCaptureDriverVFW::IsHardwareDisplayAvailable() {
	return 0 != mCaps.fHasOverlay;
}

void VDCaptureDriverVFW::SetDisplayMode(DisplayMode mode) {
	if (mode == mDisplayMode)
		return;

	mDisplayMode = mode;

	switch(mode) {
	case kDisplayNone:
		capPreview(mhwnd, FALSE);
		capOverlay(mhwnd, FALSE);
		break;
	case kDisplayHardware:
		capPreview(mhwnd, FALSE);
		capOverlay(mhwnd, TRUE);
		break;
	case kDisplaySoftware:
	case kDisplayAnalyze:
		capOverlay(mhwnd, FALSE);
		capPreview(mhwnd, TRUE);
		break;
	}
}

DisplayMode VDCaptureDriverVFW::GetDisplayMode() {
	return mDisplayMode;
}

void VDCaptureDriverVFW::SetDisplayRect(const vdrect32& r) {
	SetWindowPos(mhwnd, NULL, r.left, r.top, r.width(), r.height(), SWP_NOZORDER|SWP_NOACTIVATE);
}

vdrect32 VDCaptureDriverVFW::GetDisplayRectAbsolute() {
	RECT r;
	GetWindowRect(mhwnd, &r);
	MapWindowPoints(GetParent(mhwnd), NULL, (LPPOINT)&r, 2);
	return vdrect32(r.left, r.top, r.right, r.bottom);
}

void VDCaptureDriverVFW::SetDisplayVisibility(bool vis) {
	ShowWindow(mhwnd, vis ? SW_SHOWNA : SW_HIDE);
}

void VDCaptureDriverVFW::SetFramePeriod(sint32 ms) {
	CAPTUREPARMS cp;

	if (capCaptureGetSetup(mhwnd, &cp, sizeof(CAPTUREPARMS))) {
		cp.dwRequestMicroSecPerFrame = ms;

		capCaptureSetSetup(mhwnd, &cp, sizeof(CAPTUREPARMS));
	}
}

sint32 VDCaptureDriverVFW::GetFramePeriod() {
	CAPTUREPARMS cp;

	if (VDINLINEASSERT(capCaptureGetSetup(mhwnd, &cp, sizeof(CAPTUREPARMS))))
		return cp.dwRequestMicroSecPerFrame;

	return 1000000 / 15;
}

uint32 VDCaptureDriverVFW::GetPreviewFrameCount() {
	return mPreviewFrameCount;
}

bool VDCaptureDriverVFW::GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat) {
	DWORD dwSize = capGetVideoFormatSize(mhwnd);

	vformat.resize(dwSize);

	VDVERIFY(capGetVideoFormat(mhwnd, vformat.data(), vformat.size()));
	return true;
}

bool VDCaptureDriverVFW::SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size) {
	capSetVideoFormat(mhwnd, (BITMAPINFOHEADER *)pbih, size);
	return true;
}

bool VDCaptureDriverVFW::IsAudioCapturePossible() {
	CAPSTATUS cs;

	if (VDINLINEASSERT(capGetStatus(mhwnd, &cs, sizeof(CAPSTATUS)))) {
		return cs.fAudioHardware != 0;
	}

	return false;
}

bool VDCaptureDriverVFW::IsAudioCaptureEnabled() {
	CAPTUREPARMS cp;

	if (VDINLINEASSERT(capCaptureGetSetup(mhwnd, &cp, sizeof(CAPTUREPARMS))))
		return cp.fCaptureAudio != 0;

	return false;
}

void VDCaptureDriverVFW::SetAudioCaptureEnabled(bool b) {
	CAPTUREPARMS cp;

	if (capCaptureGetSetup(mhwnd, &cp, sizeof(CAPTUREPARMS))) {
		cp.fCaptureAudio = b;

		capCaptureSetSetup(mhwnd, &cp, sizeof(CAPTUREPARMS));
	}
}

void VDCaptureDriverVFW::SetAudioAnalysisEnabled(bool b) {
	if (mbAudioAnalysisEnabled == b)
		return;

	mbAudioAnalysisEnabled = b;

	if (mbAudioAnalysisEnabled)
		InitWaveAnalysis();
	else
		ShutdownWaveAnalysis();
}

void VDCaptureDriverVFW::GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats) {
	static const int kSamplingRates[]={
		8000,
		11025,
		12000,
		16000,
		22050,
		24000,
		32000,
		44100,
		48000,
		96000,
		192000
	};

	static const int kChannelCounts[]={
		1,
		2
	};

	static const int kSampleDepths[]={
		8,
		16
	};

	for(int sridx=0; sridx < sizeof kSamplingRates / sizeof kSamplingRates[0]; ++sridx)
		for(int chidx=0; chidx < sizeof kChannelCounts / sizeof kChannelCounts[0]; ++chidx)
			for(int sdidx=0; sdidx < sizeof kSampleDepths / sizeof kSampleDepths[0]; ++sdidx) {
				WAVEFORMATEX wfex={
					WAVE_FORMAT_PCM,
					kChannelCounts[chidx],
					kSamplingRates[sridx],
					0,
					0,
					kSampleDepths[sdidx],
					0
				};

				wfex.nBlockAlign = wfex.nChannels * (wfex.wBitsPerSample >> 3);
				wfex.nAvgBytesPerSec = wfex.nBlockAlign * wfex.nSamplesPerSec;

				if (MMSYSERR_NOERROR ==waveInOpen(NULL, WAVE_MAPPER, &wfex, 0, 0, WAVE_FORMAT_QUERY | WAVE_FORMAT_DIRECT)) {
					aformats.push_back(vdstructex<WAVEFORMATEX>(&wfex, sizeof wfex));
				}
			}
}

bool VDCaptureDriverVFW::GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat) {
	DWORD dwSize = capGetAudioFormatSize(mhwnd);

	if (!dwSize)
		return false;

	aformat.resize(dwSize);

	if (!capGetAudioFormat(mhwnd, aformat.data(), aformat.size()))
		return false;

	return true;
}

bool VDCaptureDriverVFW::SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size) {
	ShutdownWaveAnalysis();
	bool success = 0 != capSetAudioFormat(mhwnd, (WAVEFORMATEX *)pwfex, size);
	if (mbAudioAnalysisEnabled)
		InitWaveAnalysis();
	return true;
}

bool VDCaptureDriverVFW::IsDriverDialogSupported(DriverDialog dlg) {
	switch(dlg) {
	case kDialogVideoFormat:
		return mCaps.fHasDlgVideoFormat != 0;
	case kDialogVideoSource:
		return mCaps.fHasDlgVideoSource != 0;
	case kDialogVideoDisplay:
		return mCaps.fHasDlgVideoDisplay != 0;
	}

	return false;
}

void VDCaptureDriverVFW::DisplayDriverDialog(DriverDialog dlg) {
	VDASSERT(IsDriverDialogSupported(dlg));

	switch(dlg) {
	case kDialogVideoFormat:
		capDlgVideoFormat(mhwnd);
		break;
	case kDialogVideoSource:
		capDlgVideoSource(mhwnd);
		break;
	case kDialogVideoDisplay:
		capDlgVideoDisplay(mhwnd);
		break;
	}
}

bool VDCaptureDriverVFW::CaptureStart() {
	ShutdownWaveAnalysis();

	// Aim for 0.1s audio buffers.
	CAPTUREPARMS cp;
	if (capCaptureGetSetup(mhwnd, &cp, sizeof(CAPTUREPARMS))) {
		if (cp.fCaptureAudio) {
			vdstructex<WAVEFORMATEX> wfex;
			if (GetAudioFormat(wfex)) {
				cp.wNumAudioRequested = 10;
				cp.dwAudioBufferSize = (wfex->nAvgBytesPerSec / 10 + wfex->nBlockAlign - 1);
				cp.dwAudioBufferSize -= cp.dwAudioBufferSize % wfex->nBlockAlign;

				capCaptureSetSetup(mhwnd, &cp, sizeof(CAPTUREPARMS));
			}
		}
	}

	if (!VDINLINEASSERTFALSE(mbCapturing)) {
		mbCapturing = !!capCaptureSequenceNoFile(mhwnd);

		if (!mbCapturing && mbAudioAnalysisEnabled)
			InitWaveAnalysis();
	}

	return mbCapturing;
}

void VDCaptureDriverVFW::CaptureStop() {
	SendMessage(mhwndEventSink, WM_APP+16, 0, 0);
}

void VDCaptureDriverVFW::CaptureAbort() {
	SendMessage(mhwndEventSink, WM_APP+17, 0, 0);
}

void VDCaptureDriverVFW::SyncCaptureStop() {
	if (VDINLINEASSERT(mbCapturing)) {
		capCaptureStop(mhwnd);
		mbCapturing = false;

		if (mbAudioAnalysisEnabled)
			InitWaveAnalysis();
	}
}

void VDCaptureDriverVFW::SyncCaptureAbort() {
	if (mbCapturing) {
		capCaptureAbort(mhwnd);
		mbCapturing = false;

		if (mbAudioAnalysisEnabled)
			InitWaveAnalysis();
	}
}

bool VDCaptureDriverVFW::InitWaveAnalysis() {
	vdstructex<WAVEFORMATEX> aformat;

	if (!GetAudioFormat(aformat))
		return false;

	uint32	blockSize = (aformat->nAvgBytesPerSec + 9) / 10 + aformat->nBlockAlign - 1;
	blockSize -= blockSize % aformat->nBlockAlign;

	mWaveBuffer.resize(blockSize*2);

	if (MMSYSERR_NOERROR != waveInOpen(&mhWaveIn, WAVE_MAPPER, aformat.data(), (DWORD_PTR)mhwndEventSink, 0, CALLBACK_WINDOW | WAVE_FORMAT_DIRECT))
		return false;

	mbAudioAnalysisActive = true;
	for(int i=0; i<2; ++i) {
		WAVEHDR& hdr = mWaveBufHdrs[i];

		hdr.lpData			= &mWaveBuffer[blockSize*i];
		hdr.dwBufferLength	= blockSize;
		hdr.dwBytesRecorded	= 0;
		hdr.dwFlags			= 0;
		hdr.dwLoops			= 0;
		if (MMSYSERR_NOERROR != waveInPrepareHeader(mhWaveIn, &hdr, sizeof(WAVEHDR))) {
			ShutdownWaveAnalysis();
			return false;
		}

		if (MMSYSERR_NOERROR != waveInAddBuffer(mhWaveIn, &hdr, sizeof(WAVEHDR))) {
			ShutdownWaveAnalysis();
			return false;
		}
	}

	if (MMSYSERR_NOERROR != waveInStart(mhWaveIn)) {
		ShutdownWaveAnalysis();
		return false;
	}

	return true;
}

void VDCaptureDriverVFW::ShutdownWaveAnalysis() {
	if (mhWaveIn) {
		mbAudioAnalysisActive = false;
		waveInReset(mhWaveIn);

		for(int i=0; i<2; ++i) {
			if (mWaveBufHdrs[i].dwFlags & WHDR_PREPARED)
				waveInUnprepareHeader(mhWaveIn, &mWaveBufHdrs[i], sizeof(WAVEHDR));
		}

		waveInClose(mhWaveIn);
		mhWaveIn = NULL;
	}

	mWaveBuffer.clear();
}

#pragma vdpragma_TODO("Need to find some way to propagate these errors back nicely")

LRESULT CALLBACK VDCaptureDriverVFW::ErrorCallback(HWND hwnd, int nID, LPCSTR lpsz) {
#if 0
	static const struct {
		int id;
		const char *szError;
	} g_betterCaptureErrors[]={
		{ 434,	"Warning: No frames captured.\n"
				"\n"
				"Make sure your capture card is functioning correctly and that a valid video source "
				"is connected.  You might also try turning off overlay, reducing the image size, or "
				"reducing the image depth to 24 or 16-bit." },
		{ 439,	"Error: Cannot find a driver to draw this non-RGB image format.  Preview and histogram functions will be unavailable." },
	};

	char buf[1024];
	int i;

	if (!nID) return 0;

	for(i=0; i<sizeof g_betterCaptureErrors/sizeof g_betterCaptureErrors[0]; i++)
		if (g_betterCaptureErrors[i].id == nID) {
			MessageBox(GetParent(hWnd), g_betterCaptureErrors[i].szError, "VirtualDub capture error", MB_OK);
			return 0;
		}

	buf[0] = 0;
	_snprintf(buf, sizeof buf / sizeof buf[0], "Error %d: %s", nID, lpsz);
	MessageBox(GetParent(hWnd), buf, "VirtualDub capture error", MB_OK);
	VDDEBUG("%s\n",buf);
#endif

	return 0;
}

LRESULT CALLBACK VDCaptureDriverVFW::StatusCallback(HWND hwnd, int nID, LPCSTR lpsz) {
	VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)capGetUserData(hwnd);

	if (nID == IDS_CAP_BEGIN) {
		CAPSTATUS capStatus;
		capGetStatus(hwnd, (LPARAM)&capStatus, sizeof(CAPSTATUS));

		if (pThis->mpCB) {
			try {
				pThis->mpCB->CapBegin((sint64)capStatus.dwCurrentTimeElapsedMS * 1000);
			} catch(MyError& e) {
				pThis->mCaptureError.TransferFrom(e);
				capCaptureAbort(hwnd);
				pThis->mbCapturing = false;
			}
		}
	} else if (nID == IDS_CAP_END) {
		if (pThis->mpCB) {
			pThis->mpCB->CapEnd(pThis->mCaptureError.gets() ? &pThis->mCaptureError : NULL);
			pThis->mbCapturing = false;
		}

		pThis->mCaptureError.discard();
	}

#if 0
	char buf[1024];

	// Intercept nID=510 (per frame info)

	if (nID == 510)
		return 0;

	if (nID) {
		buf[0] = 0;
		_snprintf(buf, sizeof buf / sizeof buf[0], "Status %d: %s", nID, lpsz);
		SendMessage(GetDlgItem(GetParent(hWnd), IDC_STATUS_WINDOW), SB_SETTEXT, 0, (LPARAM)buf);
		VDDEBUG("%s\n",buf);
	} else {
		SendMessage(GetDlgItem(GetParent(hWnd), IDC_STATUS_WINDOW), SB_SETTEXT, 0, (LPARAM)"");
	}

	return 0;
#endif

	return 0;
}

LRESULT CALLBACK VDCaptureDriverVFW::ControlCallback(HWND hwnd, int nState) {
	VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)capGetUserData(hwnd);

	if (pThis->mpCB)
		return pThis->mpCB->CapControl(nState == CONTROLCALLBACK_PREROLL);

	return TRUE;
}

LRESULT CALLBACK VDCaptureDriverVFW::PreviewCallback(HWND hwnd, VIDEOHDR *lpVHdr) {
	VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)capGetUserData(hwnd);
	if (pThis->mpCB && pThis->mDisplayMode == kDisplayAnalyze) {
		try {
			pThis->mpCB->CapProcessData(-1, lpVHdr->lpData, lpVHdr->dwBytesUsed, (sint64)lpVHdr->dwTimeCaptured * 1000, 0 != (lpVHdr->dwFlags & VHDR_KEYFRAME), 0);
		} catch(MyError& e) {
			pThis->mCaptureError.TransferFrom(e);
			capCaptureAbort(hwnd);
			pThis->mbCapturing = false;
		}
	}
	++pThis->mPreviewFrameCount;
	return 0;
}

LRESULT CALLBACK VDCaptureDriverVFW::VideoCallback(HWND hwnd, LPVIDEOHDR lpVHdr) {
	VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)capGetUserData(hwnd);

	CAPSTATUS capStatus;
	capGetStatus(hwnd, (LPARAM)&capStatus, sizeof(CAPSTATUS));

	if (pThis->mpCB) {
		try {
			pThis->mpCB->CapProcessData(0, lpVHdr->lpData, lpVHdr->dwBytesUsed, (sint64)lpVHdr->dwTimeCaptured * 1000, 0 != (lpVHdr->dwFlags & VHDR_KEYFRAME), (sint64)capStatus.dwCurrentTimeElapsedMS * 1000);
		} catch(MyError& e) {
			pThis->mCaptureError.TransferFrom(e);
			capCaptureAbort(hwnd);
			pThis->mbCapturing = false;
		}
	}
	return 0;
}

LRESULT CALLBACK VDCaptureDriverVFW::WaveCallback(HWND hwnd, LPWAVEHDR lpWHdr) {
	VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)capGetUserData(hwnd);

	CAPSTATUS capStatus;
	capGetStatus(hwnd, (LPARAM)&capStatus, sizeof(CAPSTATUS));

	if (pThis->mpCB) {
		try {
			pThis->mpCB->CapProcessData(1, lpWHdr->lpData, lpWHdr->dwBytesRecorded, -1, false, (sint64)capStatus.dwCurrentTimeElapsedMS * 1000);
		} catch(MyError& e) {
			pThis->mCaptureError.TransferFrom(e);
			capCaptureAbort(hwnd);
			pThis->mbCapturing = false;
		}
	}
	return 0;
}

LRESULT CALLBACK VDCaptureDriverVFW::StaticMessageSinkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_NCCREATE:
			SetWindowLongPtr(hwnd, 0, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
			break;
		case MM_WIM_DATA:
			{
				VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)GetWindowLongPtr(hwnd, 0);

				if (pThis->mpCB) {
					WAVEHDR& hdr = *(WAVEHDR *)lParam;

					if (pThis->mbAudioAnalysisActive) {
						// For some reason this is sometimes called after reset. Don't know why yet.
						if (hdr.dwBytesRecorded) {
							try {
								pThis->mpCB->CapProcessData(-2, hdr.lpData, hdr.dwBytesRecorded, -1, false, 0);
							} catch(const MyError&) {
								// eat the error
							}
						}

						waveInAddBuffer(pThis->mhWaveIn, &hdr, sizeof(WAVEHDR));
					}
				}
			}
			return 0;
		case WM_APP+16:
			{
				VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)GetWindowLongPtr(hwnd, 0);
				pThis->SyncCaptureStop();
			}
			return 0;
		case WM_APP+17:
			{
				VDCaptureDriverVFW *pThis = (VDCaptureDriverVFW *)GetWindowLongPtr(hwnd, 0);
				pThis->SyncCaptureAbort();
			}
			return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}


#if 0
LPARAM VDCaptureProject::CaptureWndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	if (mbPreviewAnalysisEnabled) {
		switch(msg) {
		case WM_ERASEBKGND:
			return 0;

		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc;

				hdc = BeginPaint(mhwndCapture, &ps);

				if (mDisplayChromaKey >= 0) {
					HBRUSH hbrColorKey;
					RECT r;

					if (hbrColorKey = CreateSolidBrush((COLORREF)mDisplayChromaKey)) {
						GetClientRect(mhwndCapture, &r);
						FillRect(hdc, &r, hbrColorKey);
						DeleteObject(hbrColorKey);
					}
				}

				EndPaint(mhwndCapture, &ps);
			}
			return 0;

		case WM_TIMER:
			RydiaEnableAVICapInvalidate(true);
			CallWindowProc(mOldCaptureProc, mhwndCapture, msg, wParam, lParam);
			RydiaEnableAVICapInvalidate(false);
			return 0;
		}
	}
	return CallWindowProc(mOldCaptureProc, mhwndCapture, msg, wParam, lParam);
}
#endif


///////////////////////////////////////////////////////////////////////////

class VDCaptureSystemVFW : public IVDCaptureSystem {
public:
	VDCaptureSystemVFW();
	~VDCaptureSystemVFW();

	void EnumerateDrivers();

	int GetDeviceCount();
	const wchar_t *GetDeviceName(int index);

	IVDCaptureDriver *CreateDriver(int deviceIndex);

protected:
	HMODULE	mhmodAVICap;
	int mDriverCount;
	VDStringW mDrivers[10];
};

IVDCaptureSystem *VDCreateCaptureSystemVFW() {
	return new VDCaptureSystemVFW;
}

VDCaptureSystemVFW::VDCaptureSystemVFW()
	: mhmodAVICap(LoadLibrary("avicap32"))
	, mDriverCount(0)
{
}

VDCaptureSystemVFW::~VDCaptureSystemVFW() {
	if (mhmodAVICap)
		FreeLibrary(mhmodAVICap);
}

void VDCaptureSystemVFW::EnumerateDrivers() {
	if (!mhmodAVICap)
		return;

	typedef BOOL (VFWAPI *tpcapGetDriverDescriptionA)(UINT wDriverIndex, LPSTR lpszName, INT cbName, LPSTR lpszVer, INT cbVer);

	const tpcapGetDriverDescriptionA pcapGetDriverDescriptionA = (tpcapGetDriverDescriptionA)GetProcAddress(mhmodAVICap, "capGetDriverDescriptionA");

	if (pcapGetDriverDescriptionA) {
		char buf[256], ver[256];

		mDriverCount = 0;
		while(mDriverCount < 10 && pcapGetDriverDescriptionA(mDriverCount, buf, sizeof buf, ver, sizeof ver)) {
			mDrivers[mDriverCount] = VDTextAToW(buf) + L" (VFW)";
			++mDriverCount;
		}
	}
}

int VDCaptureSystemVFW::GetDeviceCount() {
	return mDriverCount;
}

const wchar_t *VDCaptureSystemVFW::GetDeviceName(int index) {
	return (unsigned)index < mDriverCount ? mDrivers[index].c_str() : NULL;
}

IVDCaptureDriver *VDCaptureSystemVFW::CreateDriver(int index) {
	if ((unsigned)index >= mDriverCount)
		return NULL;

	return new VDCaptureDriverVFW(mhmodAVICap, index);
}
