//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#pragma warning(disable: 4786)		// SHUT UP

#include <stdafx.h>
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <vd2/system/filesys.h>
#include <vd2/system/file.h>
#include <vd2/Dita/services.h>
#include "project.h"
#include "gui.h"
#include "VideoSource.h"
#include "AudioSource.h"
#include "HexViewer.h"
#include "InputFile.h"
#include "prefs.h"
#include "dub.h"
#include "DubOutput.h"
#include "DubStatus.h"
#include "PositionControl.h"
#include "filter.h"
#include "filtdlg.h"
#include "command.h"
#include "job.h"
#include "optdlg.h"
#include "server.h"
#include "capture.h"
#include "script.h"
#include "SceneDetector.h"
#include "MRUList.h"
#include "auxdlg.h"
#include "oshelper.h"
#include "resource.h"

///////////////////////////////////////////////////////////////////////////

extern const char g_szError[];
extern const char g_szWarning[];

extern HINSTANCE g_hInst;

extern VDProject *g_project;
extern InputFileOptions	*g_pInputOpts;

DubSource::ErrorMode	g_videoErrorMode			= DubSource::kErrorModeReportAll;
DubSource::ErrorMode	g_audioErrorMode			= DubSource::kErrorModeReportAll;

extern bool				g_fDropFrames;
extern bool				g_fSwapPanes;
extern bool				g_bExit;

extern bool g_fJobMode;
extern bool g_fJobAborted;

extern wchar_t g_szInputAVIFile[MAX_PATH];
extern wchar_t g_szInputWAVFile[MAX_PATH];

extern void CPUTest();
extern void PreviewAVI(HWND, DubOptions *, int iPriority=0, bool fProp=false);

///////////////////////////////////////////////////////////////////////////

namespace {

	void CopyFrameToClipboard(HWND hwnd, const VBitmap& vbm) {
		if (OpenClipboard(hwnd)) {
			if (EmptyClipboard()) {
				BITMAPINFOHEADER bih;
				long lFormatSize;
				HANDLE hMem;
				void *lpvMem;

				vbm.MakeBitmapHeaderNoPadding(&bih);
				lFormatSize = bih.biSize;

				if (hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, bih.biSizeImage + lFormatSize)) {
					if (lpvMem = GlobalLock(hMem)) {
						memcpy(lpvMem, &bih, lFormatSize);

						VBitmap((char *)lpvMem + lFormatSize, &bih).BitBlt(0, 0, &vbm, 0, 0, -1, -1); 

						GlobalUnlock(lpvMem);
						SetClipboardData(CF_DIB, hMem);
						CloseClipboard();
						return;
					}
					GlobalFree(hMem);
				}
			}
			CloseClipboard();
		}
	}
}

///////////////////////////////////////////////////////////////////////////

VDProject::VDProject()
	: mhwnd(0)
	, mpSceneDetector(0)
	, mSceneShuttleMode(0)
	, mSceneShuttleAdvance(0)
	, mSceneShuttleCounter(0)
	, mpDubStatus(0)
	, mposCurrentFrame(0)
	, mposSelectionStart(0)
	, mposSelectionEnd(0)
	, mLastDisplayedInputFrame(-1)
{
}

VDProject::~VDProject() {
}

bool VDProject::Attach(HWND hwnd) {	
	mhwnd = hwnd;
	return true;
}

void VDProject::Detach() {
	mhwnd = 0;
}

bool VDProject::Tick() {
	if (inputVideoAVI && mSceneShuttleMode) {
		if (!mpSceneDetector) {
			if (!(mpSceneDetector = new SceneDetector(inputVideoAVI->getImageFormat()->biWidth, inputVideoAVI->getImageFormat()->biHeight)))
				return false;

			mpSceneDetector->SetThresholds(g_prefs.scene.iCutThreshold, g_prefs.scene.iFadeThreshold);
		}

		SceneShuttleStep();
		return true;
	} else {
		if (mpSceneDetector) {
			delete mpSceneDetector;
			mpSceneDetector = NULL;
		}
		return false;
	}
}

VDPosition VDProject::GetCurrentFrame() {
	return mposCurrentFrame;
}

VDPosition VDProject::GetFrameCount() {
	return inputSubset ? inputSubset->getTotalFrames() : 0;
}

void VDProject::ClearSelection() {
	mposSelectionStart = mposSelectionEnd = 0;
	g_dubOpts.video.lStartOffsetMS = 0;
	g_dubOpts.video.lEndOffsetMS = 0;
	UISelectionStartUpdated();
	UISelectionEndUpdated();
}

bool VDProject::IsSelectionEmpty() {
	return mposSelectionStart == mposSelectionEnd;
}

VDPosition VDProject::GetSelectionStartFrame() {
	return mposSelectionStart;
}

VDPosition VDProject::GetSelectionEndFrame() {
	return mposSelectionEnd;
}

bool VDProject::IsClipboardEmpty() {
	return mClipboard.empty();
}

void VDProject::Cut() {
	Copy();
	Delete();
}

void VDProject::Copy() {
	if (inputSubset)
		mClipboard.assign(*inputSubset, mposSelectionStart, mposSelectionEnd - mposSelectionStart);
}

void VDProject::Paste() {
	if (inputSubset) {
		inputSubset->insert(mposCurrentFrame, mClipboard);
		UITimelineUpdated();
		ClearSelection();
	}
}

void VDProject::Delete() {
	if (!inputSubset)
		return;

	VDPosition pos = GetCurrentFrame();
	VDPosition start = GetSelectionStartFrame();
	VDPosition end = GetSelectionEndFrame();

	if (!IsSelectionEmpty())
		inputSubset->deleteRange(start, end-start);
	else {
		start = pos;
		inputSubset->deleteRange(pos, 1);
	}

	UITimelineUpdated();
	ClearSelection();
	MoveToFrame(start);
}

void VDProject::MaskSelection(bool bNewMode) {
	if (!inputSubset)
		return;

	VDPosition pos = GetCurrentFrame();
	VDPosition start = GetSelectionStartFrame();
	VDPosition end = GetSelectionEndFrame();

	if (!IsSelectionEmpty())
		inputSubset->setRange(start, end-start, bNewMode);
	else {
		start = pos;
		inputSubset->setRange(pos, 1, bNewMode);
	}

	UITimelineUpdated();
	ClearSelection();
	MoveToFrame(start);
}

void VDProject::DisplayFrame(VDPosition pos, bool bDispInput) {
	VDPosition original_pos = pos;

	if (!g_dubOpts.video.fShowInputFrame && !g_dubOpts.video.fShowOutputFrame)
		return;

	try {
		BITMAPINFOHEADER *dcf;
		void *lpBits;
		long limit = inputVideoAVI->getEnd();

		int len = 1;

		pos = inputSubset->lookupRange(pos, len);

		if (pos < 0)
			pos = inputVideoAVI->getEnd();

		bool bShowOutput = !mSceneShuttleMode && !g_dubber && g_dubOpts.video.fShowOutputFrame;

		if (mLastDisplayedInputFrame != pos || !inputVideoAVI->isFrameBufferValid() || (bShowOutput && !filters.isRunning())) {
			if (bDispInput)
				mLastDisplayedInputFrame = pos;

			dcf = inputVideoAVI->getDecompressedFormat();

			if (pos >= inputVideoAVI->getEnd()) {
				if (g_dubOpts.video.fShowInputFrame && bDispInput)
					UIRefreshInputFrame(false);
				if (bShowOutput)
					UIRefreshOutputFrame(false);
			} else {
				if (bShowOutput && !filters.isRunning())
					CPUTest();

				lpBits = inputVideoAVI->getFrame(pos);
				if (!lpBits)
					return;

				if (g_dubOpts.video.fShowInputFrame && bDispInput)
					UIRefreshInputFrame(true);

				if (bShowOutput) {
					const VDFraction framerate(inputVideoAVI->getRate());

					mfsi.lCurrentFrame				= original_pos;
					mfsi.lMicrosecsPerFrame			= (long)framerate.scale64ir(1000000);
					mfsi.lCurrentSourceFrame		= pos;
					mfsi.lMicrosecsPerSrcFrame		= (long)framerate.scale64ir(1000000);
					mfsi.lSourceFrameMS				= framerate.scale64ir(mfsi.lCurrentSourceFrame * (sint64)1000);
					mfsi.lDestFrameMS				= framerate.scale64ir(mfsi.lCurrentFrame * (sint64)1000);

					if (!filters.isRunning()) {
						filters.initLinearChain(&g_listFA, (Pixel *)(dcf+1), dcf->biWidth, dcf->biHeight, 32, 16+8*g_dubOpts.video.outputDepth);
						if (filters.ReadyFilters(&mfsi))
							throw MyError("can't initialize filters");
					}

					filters.InputBitmap()->BitBlt(0, 0, &VBitmap(lpBits, dcf), 0, 0, -1, -1);

					filters.RunFilters();

					UIRefreshOutputFrame(true);
				}
			}
		}

	} catch(const MyError& e) {
//		e.post(hWnd, szError);
		const char *src = e.gets();
		char *dst = strdup(src);

		if (!dst)
			guiSetStatus("%s", 255, e.gets());
		else {
			for(char *t = dst; *t; ++t)
				if (*t == '\n')
					*t = ' ';

			guiSetStatus("%s", 255, dst);
			free(dst);
		}
		SceneShuttleStop();
	}
}

///////////////////////////////////////////////////////////////////////////

void VDProject::Quit() {
	DestroyWindow(mhwnd);
}

void VDProject::Open(const wchar_t *pFilename, IVDInputDriver *pSelectedDriver, bool fExtendedOpen, bool fQuiet, bool fAutoscan, const char *pInputOpts) {
	Close();

	try {
		// attempt to determine input file type

		VDStringW filename(VDGetFullPath(VDStringW(pFilename)));

		if (!pSelectedDriver) {
			char buf[64];
			char endbuf[64];
			DWORD dwActual;

			memset(buf, 0, sizeof buf);
			memset(endbuf, 0, sizeof endbuf);

			VDFile file(filename.c_str());

			dwActual = file.readData(buf, 64);

			if (dwActual <= 64)
				memcpy(endbuf, buf, dwActual);
			else {
				file.seek(-64, nsVDFile::kSeekEnd);
				file.read(endbuf, 64);
			}

			// The Avisynth script:
			//
			//	Version
			//
			// is only 9 bytes...

			if (!dwActual)
				throw MyError("Can't open \"%s\": The file is empty.", VDTextWToA(filename).c_str());

			file.closeNT();

			// attempt detection

			tVDInputDrivers inputDrivers;
			VDGetInputDrivers(inputDrivers, IVDInputDriver::kF_Video);

			tVDInputDrivers::const_iterator it(inputDrivers.begin()), itEnd(inputDrivers.end());

			int fitquality = -1000;

			for(; it!=itEnd; ++it) {
				IVDInputDriver *pDriver = *it;

				int result = pDriver->DetectBySignature(buf, dwActual, endbuf, dwActual, 0);

				if (result > 0 && fitquality < 1) {
					pSelectedDriver = pDriver;
					fitquality = 1;
				} else if (!result && fitquality < 0) {
					pSelectedDriver = pDriver;
					fitquality = 0;
				} else if (fitquality < -1 && pDriver->DetectByFilename(filename.c_str())) {
					pSelectedDriver = pDriver;
					fitquality = -1;
				}
			}

			if (!pSelectedDriver)
				throw MyError("Cannot detect file type of \"%s\".", VDTextWToA(filename).c_str());
		}

		// open file

		inputAVI = pSelectedDriver->CreateInputFile((fQuiet?IVDInputDriver::kOF_Quiet:0) + (fAutoscan?IVDInputDriver::kOF_AutoSegmentScan:0));
		if (!inputAVI) throw MyMemoryError();

		// Extended open?

		if (fExtendedOpen)
			g_pInputOpts = inputAVI->promptForOptions(mhwnd);
		else if (pInputOpts)
			g_pInputOpts = inputAVI->createOptions(pInputOpts);

		if (g_pInputOpts)
			inputAVI->setOptions(g_pInputOpts);

		inputAVI->Init(filename.c_str());

		inputAudioAVI = inputAVI->audioSrc;
		inputVideoAVI = inputAVI->videoSrc;

		if (!inputVideoAVI->setDecompressedFormat(24))
			if (!inputVideoAVI->setDecompressedFormat(32))
				if (!inputVideoAVI->setDecompressedFormat(16))
					inputVideoAVI->setDecompressedFormat(8);

		inputVideoAVI->setDecodeErrorMode(g_videoErrorMode);

		if (inputAudioAVI)
			inputAudioAVI->setDecodeErrorMode(g_audioErrorMode);

		// How many items did we get?

		{
			InputFilenameNode *pnode = inputAVI->listFiles.AtHead();
			InputFilenameNode *pnode_next;
			int nFiles = 0;

			while(pnode_next = pnode->NextFromHead()) {
				++nFiles;
				pnode = pnode_next;
			}

			if (nFiles > 1)
				guiSetStatus("Autoloaded %d segments (last was \"%s\")", 255, nFiles, pnode->NextFromTail()->name);
		}

		// Set current filename

		wcscpy(g_szInputAVIFile, filename.c_str());

		if (!(inputSubset = new FrameSubset(inputVideoAVI->getLength())))
			throw MyMemoryError();

		ClearSelection();
		g_dubOpts.video.lStartOffsetMS = g_dubOpts.video.lEndOffsetMS = 0;
		RemakePositionSlider();
		SetAudioSource();
		RecalcPositionTimeConstant();
		UISourceFileUpdated();
		UIVideoSourceUpdated();
		MoveToFrame(0);
	} catch(const MyError&) {
		Close();
		throw;
	}
}

void VDProject::PreviewInput() {
	SetAudioSource();

	LONG lStart = g_project->GetCurrentFrame();
	DubOptions dubOpt(g_dubOpts);

	LONG preload = inputAudio && inputAudio->getWaveFormat()->wFormatTag != WAVE_FORMAT_PCM ? 1000 : 500;

	if (dubOpt.audio.preload > preload)
		dubOpt.audio.preload = preload;

	dubOpt.audio.enabled				= TRUE;
	dubOpt.audio.interval				= 1;
	dubOpt.audio.is_ms					= FALSE;
	dubOpt.video.lStartOffsetMS		= inputVideoAVI->samplesToMs(lStart);

	dubOpt.audio.fStartAudio			= TRUE;
	dubOpt.audio.new_rate				= 0;
	dubOpt.audio.newPrecision			= DubAudioOptions::P_NOCHANGE;
	dubOpt.audio.newChannels			= DubAudioOptions::C_NOCHANGE;
	dubOpt.audio.volume				= 0;
	dubOpt.audio.bUseAudioFilterGraph	= false;

	switch(g_prefs.main.iPreviewDepth) {
	case PreferencesMain::DEPTH_DISPLAY:
		{
			DEVMODE dm;
			dm.dmSize = sizeof(DEVMODE);
			dm.dmDriverExtra = 0;
			if (!EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm))
				dm.dmBitsPerPel = 16;

			switch(dm.dmBitsPerPel) {
			case 24:
			case 32:
				dubOpt.video.inputDepth = DubVideoOptions::D_24BIT;
				break;
			default:
				dubOpt.video.inputDepth = DubVideoOptions::D_16BIT;
				break;
			}
		}
		break;
	case PreferencesMain::DEPTH_FASTEST:
	case PreferencesMain::DEPTH_16BIT:
		dubOpt.video.inputDepth = DubVideoOptions::D_16BIT;
		break;
	case PreferencesMain::DEPTH_24BIT:
		dubOpt.video.inputDepth = DubVideoOptions::D_24BIT;
		break;

	// Ignore: PreferencesMain::DEPTH_OUTPUT

	};
	dubOpt.video.outputDepth			= dubOpt.video.inputDepth;

	dubOpt.video.mode					= DubVideoOptions::M_SLOWREPACK;
	dubOpt.video.fShowInputFrame		= TRUE;
	dubOpt.video.fShowOutputFrame		= FALSE;
	dubOpt.video.frameRateDecimation	= 1;
	dubOpt.video.lEndOffsetMS			= 0;

	dubOpt.audio.mode					= DubAudioOptions::M_FULL;

	dubOpt.fShowStatus = false;
	dubOpt.fMoveSlider = true;

	if (lStart < inputSubset->getTotalFrames())
		PreviewAVI(mhwnd, &dubOpt, g_prefs.main.iPreviewPriority);
}

void VDProject::PreviewOutput() {
	SetAudioSource();

	LONG lStart = g_project->GetCurrentFrame();
	DubOptions dubOpt(g_dubOpts);

	LONG preload = inputAudio && inputAudio->getWaveFormat()->wFormatTag != WAVE_FORMAT_PCM ? 1000 : 500;

	if (dubOpt.audio.preload > preload)
		dubOpt.audio.preload = preload;

	dubOpt.audio.enabled				= TRUE;
	dubOpt.audio.interval				= 1;
	dubOpt.audio.is_ms					= FALSE;
	dubOpt.video.lStartOffsetMS		= inputVideoAVI->samplesToMs(lStart);

	dubOpt.fShowStatus = false;
	dubOpt.fMoveSlider = true;

	if (lStart < inputSubset->getTotalFrames())
		PreviewAVI(mhwnd, &dubOpt, g_prefs.main.iPreviewPriority);
}

void VDProject::PreviewAll() {
	PreviewAVI(mhwnd, NULL, g_prefs.main.iPreviewPriority);
}

void VDProject::Close() {
	CloseAVI();
	UIVideoSourceUpdated();
	UISourceFileUpdated();
}

void VDProject::StartServer() {
	SetAudioSource();
	HWND hwnd = mhwnd;
	Detach();

	ActivateFrameServerDialog(hwnd);

	Attach(hwnd);
	UIVideoSourceUpdated();
	UISourceFileUpdated();
	UITimelineUpdated();
}

void VDProject::SwitchToCaptureMode() {
	CPUTest();

	HWND hwnd = mhwnd;
	Detach();
	Capture(hwnd);
	Attach(hwnd);

	UIVideoSourceUpdated();		// necessary because filters can be changed in capture mode
}

void VDProject::ShowInputInfo() {
	if (inputAVI)
		inputAVI->InfoDialog(mhwnd);
}

void VDProject::SetVideoMode(int mode) {
	g_dubOpts.video.mode = mode;
}

void VDProject::CopySourceFrameToClipboard() {
	if (!inputVideoAVI || !inputVideoAVI->isFrameBufferValid())
		return;

	CopyFrameToClipboard(mhwnd, VBitmap(inputVideoAVI->getFrameBuffer(), inputVideoAVI->getDecompressedFormat()));
}

void VDProject::CopyOutputFrameToClipboard() {
	if (!filters.isRunning())
		return;
	CopyFrameToClipboard(mhwnd, *filters.LastBitmap());
}

void VDProject::SetAudioSourceNone() {
	audioInputMode = AUDIOIN_NONE;
	CloseWAV();
	SetAudioSource();
}

void VDProject::SetAudioSourceNormal() {
	audioInputMode = AUDIOIN_AVI;
	CloseWAV();
	SetAudioSource();
}

void VDProject::SetAudioMode(int mode) {
	g_dubOpts.audio.mode = mode;
}

void VDProject::SetSelectionStart() {
	if (inputAVI)
		SetSelectionStart(GetCurrentFrame());
}

void VDProject::SetSelectionStart(VDPosition pos) {
	if (inputAVI) {
		if (pos < 0)
			pos = 0;
		if (pos > GetFrameCount())
			pos = GetFrameCount();
		mposSelectionStart = pos;
		g_dubOpts.video.lStartOffsetMS = inputVideoAVI->samplesToMs(pos);

		UISelectionStartUpdated();
	}
}

void VDProject::SetSelectionEnd() {
	if (inputAVI)
		SetSelectionEnd(GetCurrentFrame());
}

void VDProject::SetSelectionEnd(VDPosition pos) {
	if (inputAVI) {
		if (pos < 0)
			pos = 0;
		if (pos > GetFrameCount())
			pos = GetFrameCount();

		mposSelectionEnd = pos;
		g_dubOpts.video.lEndOffsetMS = inputVideoAVI->samplesToMs(GetFrameCount() - pos);

		UISelectionEndUpdated();
	}
}

void VDProject::MoveToFrame(VDPosition frame) {
	if (inputVideoAVI) {
		frame = std::max<VDPosition>(0, std::min<VDPosition>(frame, inputSubset->getTotalFrames()));

		mposCurrentFrame = frame;
		UICurrentPositionUpdated();
		DisplayFrame(frame);
	}
}

void VDProject::MoveToStart() {
	if (inputVideoAVI)
		MoveToFrame(0);
}

void VDProject::MoveToPrevious() {
	if (inputVideoAVI)
		MoveToFrame(GetCurrentFrame() - 1);
}

void VDProject::MoveToNext() {
	if (inputVideoAVI)
		MoveToFrame(GetCurrentFrame() + 1);
}

void VDProject::MoveToEnd() {
	if (inputVideoAVI)
		MoveToFrame(inputSubset->getTotalFrames());
}

void VDProject::MoveToSelectionStart() {
	if (inputVideoAVI) {
		VDPosition pos = GetSelectionStartFrame();

		if (pos >= 0)
			MoveToFrame(pos);
	}
}

void VDProject::MoveToSelectionEnd() {
	if (inputVideoAVI) {
		VDPosition pos = GetSelectionEndFrame();

		if (pos >= 0)
			MoveToFrame(pos);
	}
}

void VDProject::MoveToPreviousKey() {
	if (!inputVideoAVI)
		return;

	LONG lSample = (LONG)GetCurrentFrame();

	if (lSample <= 0)
		lSample = 0;
	else {
		int offset;
		FrameSubset::iterator it(inputSubset->findNode(offset, lSample)), itBegin(inputSubset->begin()), itEnd(inputSubset->end());

		do {
			if (it!=itEnd) {
				const FrameSubsetNode& fsn0 = *it;

				if (!fsn0.bMask) {
					lSample = inputVideoAVI->prevKey(fsn0.start + offset) - fsn0.start;

					if (lSample >= 0)
						break;
				}
			}

			while(it != itBegin) {
				--it;
				const FrameSubsetNode& fsn = *it;

				if (!fsn.bMask) {
					lSample = inputVideoAVI->nearestKey(fsn.start + fsn.len - 1) - fsn.start;

					if (lSample >= 0)
						break;
				}

				lSample = 0;
			}
		} while(false);

		while(it != itBegin) {
			--it;
			const FrameSubsetNode& fsn2 = *it;

			lSample += fsn2.len;
		}
	}

	MoveToFrame(lSample);
}

void VDProject::MoveToNextKey() {
	if (!inputVideoAVI)
		return;

	long lSampleOld = (long)GetCurrentFrame();
	long lSample = lSampleOld;

	if (lSample >= inputSubset->getTotalFrames() - 1)
		lSample = inputSubset->getTotalFrames();
	else {
		int offset;
		FrameSubset::iterator it(inputSubset->findNode(offset, lSample)), itBegin(inputSubset->begin()), itEnd(inputSubset->end());

		do {
			if (it==itEnd) {
				VDASSERT(false);
				lSample = inputSubset->getTotalFrames();
				break;
			}

			const FrameSubsetNode& fsn0 = *it;

			if (!fsn0.bMask) {
				lSample = inputVideoAVI->nextKey(fsn0.start + offset) - fsn0.start;

				if (lSample >= 0 && lSample < fsn0.len)
					break;
			}

			while(++it != itEnd) {
				const FrameSubsetNode& fsn = *it;

				if (!fsn.bMask) {
					lSample = 0;
					if (inputVideoAVI->isKey(fsn.start))
						break;

					lSample = inputVideoAVI->nextKey(fsn.start) - fsn.start;

					if (lSample >= 0 && lSample < fsn.len)
						break;
				}

				lSample = 0;
			}
		} while(false);

		while(it != itBegin) {
			--it;
			const FrameSubsetNode& fsn2 = *it;

			lSample += fsn2.len;
		}
	}

	MoveToFrame(lSample);
}

void VDProject::MoveBackSome() {
	if (inputVideoAVI)
		MoveToFrame(GetCurrentFrame() - 50);
}

void VDProject::MoveForwardSome() {
	if (inputVideoAVI)
		MoveToFrame(GetCurrentFrame() + 50);
}

void VDProject::StartSceneShuttleReverse() {
	if (!inputVideoAVI)
		return;
	mSceneShuttleMode = -1;
	UIShuttleModeUpdated();
}

void VDProject::StartSceneShuttleForward() {
	if (!inputVideoAVI)
		return;
	mSceneShuttleMode = +1;
	UIShuttleModeUpdated();
}

void VDProject::MoveToPreviousRange() {
	if (inputAVI) {
		long lSample = GetCurrentFrame();
		int offset;

		FrameSubset::iterator pfsn = inputSubset->findNode(offset, lSample);

		if (pfsn != inputSubset->end() && pfsn != inputSubset->begin()) {
			--pfsn;
			lSample -= offset;

			MoveToFrame(lSample - pfsn->len);
			guiSetStatus("Previous output range %d-%d: %sed source frames %d-%d", 255, lSample - pfsn->len, lSample-1, pfsn->bMask ? "mask" : "includ", pfsn->start, pfsn->start + pfsn->len - 1);
			return;
		}
	}
	guiSetStatus("No previous edit range.", 255);
}

void VDProject::MoveToNextRange() {
	if (inputAVI) {
		long lSample = GetCurrentFrame();
		int offset;

		FrameSubset::const_iterator pfsn = inputSubset->findNode(offset, lSample);

		if (pfsn != inputSubset->end()) {
			lSample = lSample - offset + pfsn->len;
			++pfsn;

			if (pfsn != inputSubset->end()) {
				MoveToFrame(lSample);
				guiSetStatus("Next output range %d-%d: %sed source frames %d-%d", 255, lSample, lSample+pfsn->len-1, pfsn->bMask ? "mask" : "includ", pfsn->start, pfsn->start + pfsn->len - 1);
				return;
			}
		}
	}
	guiSetStatus("No next edit range.", 255);
}

void VDProject::MoveToPreviousDrop() {
	if (inputAVI) {
		LONG lSample = GetCurrentFrame();

		while(--lSample >= 0) {
			int err;
			long lBytes, lSamples;

			err = inputVideoAVI->read(inputSubset->lookupFrame(lSample), 1, NULL, 0, &lBytes, &lSamples);
			if (err != AVIERR_OK)
				break;

			if (!lBytes) {
				MoveToFrame(lSample);
				break;
			}
		}

		if (lSample < 0)
			guiSetStatus("No previous dropped frame found.", 255);
	}
}

void VDProject::MoveToNextDrop() {
	if (inputAVI) {
		LONG lSample = GetCurrentFrame();

		while(++lSample < inputSubset->getTotalFrames()) {
			int err;
			long lBytes, lSamples;

			err = inputVideoAVI->read(inputSubset->lookupFrame(lSample), 1, NULL, 0, &lBytes, &lSamples);
			if (err != AVIERR_OK)
				break;

			if (!lBytes) {
				MoveToFrame(lSample);
				break;
			}
		}

		if (lSample >= inputSubset->getTotalFrames())
			guiSetStatus("No next dropped frame found.", 255);
	}
}

void VDProject::ResetTimeline() {
	if (inputAVI) {
		delete inputSubset;
		inputSubset = NULL;
		if (!(inputSubset = new FrameSubset(inputVideoAVI->getLength())))
			throw MyMemoryError();
		RemakePositionSlider();
	}
}

void VDProject::ResetTimelineWithConfirmation() {
	if (inputAVI && inputSubset) {
		if (IDOK == MessageBox(mhwnd, "Discard edits and reset timeline?", g_szWarning, MB_OKCANCEL|MB_TASKMODAL|MB_SETFOREGROUND)) {
			ResetTimeline();
		}
	}
}

void VDProject::ScanForErrors() {
	if (inputVideoAVI) {
		ScanForUnreadableFrames(inputSubset, inputVideoAVI);
	}
}

void VDProject::RunOperation(IVDDubberOutputSystem *pOutputSystem, BOOL fAudioOnly, DubOptions *pOptions, int iPriority, bool fPropagateErrors, long lSpillThreshold, long lSpillFrameThreshold) {
	bool fError = false;
	MyError prop_err;
	DubOptions *opts;

	{
		const wchar_t *pOpType = pOutputSystem->IsRealTime() ? L"preview" : L"dub";
		VDLog(kVDLogMarker, VDswprintf(L"Beginning %ls operation.", 1, &pOpType));
	}

	try {
		VDAutoLogDisplay disp;

		filters.DeinitFilters();
		filters.DeallocateBuffers();

		SetAudioSource();

		CPUTest();

		// Create a dubber.

		if (!pOptions) {
			g_dubOpts.video.fShowDecompressedFrame = g_drawDecompressedFrame;
			g_dubOpts.fShowStatus = !!g_showStatusWindow;
		}

		opts = pOptions ? pOptions : &g_dubOpts;
		opts->perf.fDropFrames = g_fDropFrames;

		if (!(g_dubber = CreateDubber(opts)))
			throw MyMemoryError();

		// Create dub status window

		mpDubStatus = CreateDubStatusHandler();

		if (opts->fMoveSlider)
			mpDubStatus->SetPositionCallback(g_fJobMode ? JobPositionCallback : StaticPositionCallback, this);

		// Initialize the dubber.

		if (opts->audio.bUseAudioFilterGraph)
			g_dubber->SetAudioFilterGraph(g_audioFilterGraph);

		g_dubber->SetStatusHandler(mpDubStatus);
		g_dubber->SetInputFile(inputAVI);

		if (!pOutputSystem->IsRealTime() && g_ACompressionFormat)
			g_dubber->SetAudioCompression(g_ACompressionFormat, g_ACompressionFormatSize);

		// As soon as we call Init(), this value is no longer ours to free.

		UISetDubbingMode(true, pOutputSystem->IsRealTime());

		if (lSpillThreshold) {
			g_dubber->EnableSpill((__int64)(lSpillThreshold-1) << 20, lSpillFrameThreshold);
			g_dubber->Init(inputVideoAVI, inputAudio, pOutputSystem, &g_Vcompression);
		} else if (fAudioOnly == 2) {
			g_dubber->SetPhantomVideoMode();
			g_dubber->Init(inputVideoAVI, inputAudio, pOutputSystem, &g_Vcompression);
		} else {
			g_dubber->Init(inputVideoAVI, inputAudio, pOutputSystem, &g_Vcompression);
		}

		_RPT0(0,"Starting dub.\n");

		if (!pOptions) RedrawWindow(mhwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);

		g_dubber->Go(iPriority);

		UIRunDubMessageLoop();

		g_dubber->Stop();

		if (g_dubber->isAbortedByUser()) {
			g_fJobAborted = true;
		} else if (!fPropagateErrors)
			disp.Post((VDGUIHandle)mhwnd);

	} catch(char *s) {
		if (fPropagateErrors) {
			prop_err.setf(s);
			fError = true;
		} else
			MyError(s).post(mhwnd,g_szError);
	} catch(MyError& err) {
		if (fPropagateErrors) {
			prop_err.TransferFrom(err);
			fError = true;
		} else
			err.post(mhwnd,g_szError);
	}

	g_dubber->SetStatusHandler(NULL);
	delete mpDubStatus;
	mpDubStatus = NULL;

	_CrtCheckMemory();

	_RPT0(0,"Ending dub.\n");

	delete g_dubber;
	g_dubber = NULL;

	if (!inputVideoAVI->setDecompressedFormat(24))
		if (!inputVideoAVI->setDecompressedFormat(32))
			if (!inputVideoAVI->setDecompressedFormat(16))
				inputVideoAVI->setDecompressedFormat(8);

	UISetDubbingMode(false, false);

	VDLog(kVDLogMarker, VDStringW(L"Ending operation."));

	if (g_bExit)
		PostQuitMessage(0);
	else if (fError && fPropagateErrors)
		throw prop_err;
}

void VDProject::AbortOperation() {
	if (g_dubber)
		g_dubber->Abort();
}

///////////////////////////////////////////////////////////////////////////

void VDProject::SceneShuttleStop() {
	if (mSceneShuttleMode) {
		mSceneShuttleMode = 0;
		mSceneShuttleAdvance = 0;
		mSceneShuttleCounter = 0;

		UIShuttleModeUpdated();

		if (inputVideoAVI)
			MoveToFrame(GetCurrentFrame());
	}
}

void VDProject::SceneShuttleStep() {
	if (!inputVideoAVI)
		SceneShuttleStop();

	LONG lSample = GetCurrentFrame() + mSceneShuttleMode;
	long ls2 = inputSubset->lookupFrame(lSample);

	if (!inputVideoAVI || ls2 < inputVideoAVI->getStart() || ls2 >= inputVideoAVI->getEnd()) {
		SceneShuttleStop();
		return;
	}

	if (mSceneShuttleAdvance < 1280)
		++mSceneShuttleAdvance;

	mSceneShuttleCounter += 32;

	if (mSceneShuttleCounter >= mSceneShuttleAdvance) {
		mSceneShuttleCounter = 0;
		DisplayFrame(lSample, true);
	} else
		DisplayFrame(lSample, false);

	mposCurrentFrame = lSample;
	UICurrentPositionUpdated();

	if (mpSceneDetector->Submit(&VBitmap(inputVideoAVI->getFrameBuffer(), inputVideoAVI->getDecompressedFormat()))) {
		SceneShuttleStop();
	}
}

void VDProject::StaticPositionCallback(VDPosition start, VDPosition cur, VDPosition end, int progress, void *cookie) {
	VDProject *pthis = (VDProject *)cookie;
	VDPosition frame = std::max<VDPosition>(0, std::min<VDPosition>(cur, inputSubset->getTotalFrames()));

	pthis->mposCurrentFrame = frame;
	pthis->UICurrentPositionUpdated();
}
