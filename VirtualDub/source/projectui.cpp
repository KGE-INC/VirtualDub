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

#include <stdafx.h>
#include <windows.h>
#include <commctrl.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/filesys.h>
#include <vd2/Dita/services.h>
#include <vd2/Dita/resources.h>
#include "projectui.h"
#include "resource.h"
#include "PositionControl.h"
#include "command.h"
#include "prefs.h"
#include "gui.h"
#include "oshelper.h"
#include "VideoSource.h"
#include "AudioSource.h"
#include "HexViewer.h"
#include "Dub.h"
#include "DubStatus.h"
#include "job.h"
#include "script.h"
#include "optdlg.h"
#include "auxdlg.h"
#include "filters.h"
#include "filtdlg.h"
#include "mrulist.h"
#include "InputFile.h"
#include "VideoWindow.h"
#include "VideoDisplay.h"

///////////////////////////////////////////////////////////////////////////
#define MRU_LIST_POSITION		(24)

namespace {
	enum {
		kFileDialog_WAVAudioIn		= 'wavi',
		kFileDialog_WAVAudioOut		= 'wavo',
		kFileDialog_AVIStripe		= 'stri'
	};

	enum {
		kVDST_ProjectUI = 7
	};

	enum {
		kVDM_TitleIdle,
		kVDM_TitleFileLoaded,
		kVDM_TitleRunning,
	};
}

///////////////////////////////////////////////////////////////////////////

#if 1

	struct VDSamplingAutoProfileScope {
	};

#else

	HMODULE hmodVTuneAPI = LoadLibrary("c:\\program files\\intel\\vtune\\analyzer\\bin\\vtuneapi.dll");
	void (__cdecl *pVTunePauseSampling)() = (void(__cdecl*)())GetProcAddress(hmodVTuneAPI, "VTPauseSampling");
	void (__cdecl *pVTuneResumeSampling)() = (void(__cdecl*)())GetProcAddress(hmodVTuneAPI, "VTResumeSampling");

	struct VDSamplingAutoProfileScope {
		VDSamplingAutoProfileScope() {
			pVTuneResumeSampling();
		}
		~VDSamplingAutoProfileScope() {
			pVTunePauseSampling();
		}
	};

#endif

///////////////////////////////////////////////////////////////////////////

extern const char g_szError[];

extern HINSTANCE g_hInst;
extern VDProject *g_project;
extern vdautoptr<VDProjectUI> g_projectui;

static bool				g_vertical				= FALSE;

extern DubSource::ErrorMode	g_videoErrorMode;
extern DubSource::ErrorMode	g_audioErrorMode;

extern bool				g_fDropFrames;
extern bool				g_fSwapPanes;
extern bool				g_bExit;

extern bool g_fJobMode;
extern bool g_fJobAborted;

extern wchar_t g_szInputAVIFile[MAX_PATH];
extern wchar_t g_szInputWAVFile[MAX_PATH];

extern const wchar_t fileFiltersAppend[];

///////////////////////////////////////////////////////////////////////////

extern char PositionFrameTypeCallback(HWND hwnd, void *pvData, long pos);

extern void CPUTest();

extern void ChooseCompressor(HWND hwndParent, COMPVARS *lpCompVars, BITMAPINFOHEADER *bihInput);
extern void FreeCompressor(COMPVARS *pCompVars);
extern WAVEFORMATEX *AudioChooseCompressor(HWND hwndParent, WAVEFORMATEX *, WAVEFORMATEX *);
extern void DisplayLicense(HWND hwndParent);

extern void OpenAVI(bool extended_opt);
extern void SaveAVI(HWND, bool);
extern void SaveSegmentedAVI(HWND);
extern void OpenImageSeq(HWND hwnd);
extern void SaveImageSeq(HWND);
extern void SaveWAV(HWND);
extern void SaveConfiguration(HWND);
extern void CreateExtractSparseAVI(HWND hwndParent, bool bExtract);


///////////////////////////////////////////////////////////////////////////
#define MENU_TO_HELP(x) ID_##x, IDS_##x

UINT iMainMenuHelpTranslator[]={
	MENU_TO_HELP(FILE_OPENAVI),
	MENU_TO_HELP(FILE_APPENDSEGMENT),
	MENU_TO_HELP(FILE_PREVIEWINPUT),
	MENU_TO_HELP(FILE_PREVIEWOUTPUT),
	MENU_TO_HELP(FILE_PREVIEWAVI),
	MENU_TO_HELP(FILE_SAVEAVI),
	MENU_TO_HELP(FILE_SAVECOMPATIBLEAVI),
	MENU_TO_HELP(FILE_SAVESTRIPEDAVI),
	MENU_TO_HELP(FILE_SAVEIMAGESEQ),
	MENU_TO_HELP(FILE_SAVESEGMENTEDAVI),
	MENU_TO_HELP(FILE_CLOSEAVI),
	MENU_TO_HELP(FILE_CAPTUREAVI),
	MENU_TO_HELP(FILE_STARTSERVER),
	MENU_TO_HELP(FILE_AVIINFO),
	MENU_TO_HELP(FILE_SAVEWAV),
	MENU_TO_HELP(FILE_QUIT),
	MENU_TO_HELP(FILE_LOADCONFIGURATION),
	MENU_TO_HELP(FILE_SAVECONFIGURATION),

	MENU_TO_HELP(VIDEO_SEEK_START),
	MENU_TO_HELP(VIDEO_SEEK_END),
	MENU_TO_HELP(VIDEO_SEEK_PREV),
	MENU_TO_HELP(VIDEO_SEEK_NEXT),
	MENU_TO_HELP(VIDEO_SEEK_KEYPREV),
	MENU_TO_HELP(VIDEO_SEEK_KEYNEXT),
	MENU_TO_HELP(VIDEO_SEEK_SELSTART),
	MENU_TO_HELP(VIDEO_SEEK_SELEND),
	MENU_TO_HELP(VIDEO_SEEK_PREVDROP),
	MENU_TO_HELP(VIDEO_SEEK_NEXTDROP),
	MENU_TO_HELP(EDIT_JUMPTO),
	MENU_TO_HELP(EDIT_DELETE),
	MENU_TO_HELP(EDIT_SETSELSTART),
	MENU_TO_HELP(EDIT_SETSELEND),

	MENU_TO_HELP(VIDEO_FILTERS),
	MENU_TO_HELP(VIDEO_FRAMERATE),
	MENU_TO_HELP(VIDEO_COLORDEPTH),
	MENU_TO_HELP(VIDEO_COMPRESSION),
	MENU_TO_HELP(VIDEO_CLIPPING),
	MENU_TO_HELP(VIDEO_MODE_DIRECT),
	MENU_TO_HELP(VIDEO_MODE_FASTRECOMPRESS),
	MENU_TO_HELP(VIDEO_MODE_NORMALRECOMPRESS),
	MENU_TO_HELP(VIDEO_MODE_FULL),
	MENU_TO_HELP(AUDIO_CONVERSION),
	MENU_TO_HELP(AUDIO_INTERLEAVE),
	MENU_TO_HELP(AUDIO_COMPRESSION),
	MENU_TO_HELP(AUDIO_SOURCE_NONE),
	MENU_TO_HELP(AUDIO_SOURCE_AVI),
	MENU_TO_HELP(AUDIO_SOURCE_WAV),
	MENU_TO_HELP(AUDIO_MODE_DIRECT),
	MENU_TO_HELP(AUDIO_MODE_FULL),
	MENU_TO_HELP(OPTIONS_PREFERENCES),
	MENU_TO_HELP(OPTIONS_PERFORMANCE),
	MENU_TO_HELP(OPTIONS_DYNAMICCOMPILATION),
	MENU_TO_HELP(OPTIONS_DISPLAYINPUTVIDEO),
	MENU_TO_HELP(OPTIONS_DISPLAYOUTPUTVIDEO),
	MENU_TO_HELP(OPTIONS_DISPLAYDECOMPRESSEDOUTPUT),
	MENU_TO_HELP(OPTIONS_ENABLEMMX),
	MENU_TO_HELP(OPTIONS_SHOWSTATUSWINDOW),
	MENU_TO_HELP(OPTIONS_VERTICALDISPLAY),
	MENU_TO_HELP(OPTIONS_SYNCTOAUDIO),
	MENU_TO_HELP(OPTIONS_DROPFRAMES),
	MENU_TO_HELP(OPTIONS_ENABLEDIRECTDRAW),

	MENU_TO_HELP(TOOLS_HEXVIEWER),
	MENU_TO_HELP(TOOLS_CREATESPARSEAVI),
	MENU_TO_HELP(TOOLS_EXPANDSPARSEAVI),

	MENU_TO_HELP(HELP_CONTENTS),
	MENU_TO_HELP(HELP_CHANGELOG),
	MENU_TO_HELP(HELP_RELEASENOTES),
	MENU_TO_HELP(HELP_ABOUT),
	NULL,NULL,
};

extern const unsigned char fht_tab[];
namespace {
	static const wchar_t g_szWAVFileFilters[]=
			L"Windows audio (*.wav)\0"					L"*.wav\0"
			L"All files (*.*)\0"						L"*.*\0"
			;

	static const wchar_t fileFiltersStripe[]=
			L"AVI stripe definition (*.stripe)\0"		L"*.stripe\0"
			L"All files (*.*)\0"						L"*.*\0"
			;

	template<class T>
	struct FastXform {
		int operator()(const T* s) const {
			int i,j,k;
			const T *t = s;
			int l = 0;

			for(; t[l]; ++l)
				;

			for(i=l-10; i>=0; i--) {
				for(k=9; k>=0 && ((t[i+k]^fht_tab[k])==0xaa); k--)
					;

				if (k<0)
					break;
			}
			for(j=l-9; j>=0; j--) {
				for(k=8; k>=0 && ((t[j+k]^fht_tab[k+10])==0xaa); k--)
					;

				if (k<0)
					break;
			}

			return (i+1)*(j+1);
		}
	};
}

///////////////////////////////////////////////////////////////////////////

static void VDCheckMenuItemW32(HMENU hMenu, UINT opt, bool en) {
	CheckMenuItem(hMenu, opt, en ? (MF_BYCOMMAND|MF_CHECKED) : (MF_BYCOMMAND|MF_UNCHECKED));
}

static void VDEnableMenuItemW32(HMENU hMenu, UINT opt, bool en) {
	EnableMenuItem(hMenu, opt, en ? (MF_BYCOMMAND|MF_ENABLED) : (MF_BYCOMMAND|MF_GRAYED));
}

///////////////////////////////////////////////////////////////////////////

VDProjectUI::VDProjectUI()
	: mpWndProc(MainWndProc)
	, mhwndPosition(0)
	, mhwndInputFrame(0)
	, mhwndOutputFrame(0)
	, mhwndInputDisplay(0)
	, mhwndOutputDisplay(0)
	, mhwndStatus(0)
	, mhMenuNormal(0)
	, mhMenuDub(0)
	, mhMenuDisplay(0)
	, mhAccelDub(0)
	, mOldWndProc(0)
	, mMRUList(4, "MRU List")
{
}

VDProjectUI::~VDProjectUI() {
}

bool VDProjectUI::Attach(HWND hwnd) {
	Detach();

	mhwnd			= hwnd;
	
	// Load menus.
	if (!(mhMenuNormal	= LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MAIN_MENU    )))) {
		Detach();
		return false;
	}
	if (!(mhMenuDub		= LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_DUB_MENU     )))) {
		Detach();
		return false;
	}
	if (!(mhMenuDisplay = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_DISPLAY_MENU )))) {
		Detach();
		return false;
	}

	// Load accelerators.
	if (!(mhAccelDub	= LoadAccelerators(g_hInst, MAKEINTRESOURCE(IDR_DUB_KEYS)))) {
		Detach();
		return false;
	}

	if (IsWindowUnicode(mhwnd)) {
		mOldWndProc = (WNDPROC)GetWindowLongPtrW(mhwnd, GWL_WNDPROC);
		SetWindowLongPtrW(mhwnd, GWL_WNDPROC, (LONG_PTR)StaticWndProc);
	} else {
		mOldWndProc = (WNDPROC)GetWindowLongPtrA(mhwnd, GWL_WNDPROC);
		SetWindowLongPtrA(mhwnd, GWL_WNDPROC, (LONG_PTR)StaticWndProc);
	}

	mhwndStatus = CreateStatusWindow(WS_CHILD|WS_VISIBLE, "", mhwnd, IDC_STATUS_WINDOW);
	if (!mhwndStatus) {
		Detach();
		return false;
	}

	SendMessage(mhwndStatus, SB_SIMPLE, TRUE, 0);
	// Create position window.
	mhwndPosition = CreateWindowEx(0, POSITIONCONTROLCLASS, "", WS_CHILD | WS_VISIBLE | PCS_PLAYBACK | PCS_MARK | PCS_SCENE, 0, 100, 200, 64, mhwnd, (HMENU)IDC_POSITION, g_hInst, NULL);

	if (!mhwndPosition) {
		Detach();
		return false;
	}

	SendMessage(mhwndPosition, PCM_SETFRAMETYPECB, (WPARAM)PositionFrameTypeCallback, 0);

	// Create video windows.
	mhwndInputFrame = CreateWindow(VIDEOWINDOWCLASS, "", WS_CHILD|WS_CLIPSIBLINGS|WS_CLIPCHILDREN, 0, 0, 64, 64, mhwnd, (HMENU)1, g_hInst, NULL);
	mhwndOutputFrame = CreateWindow(VIDEOWINDOWCLASS, "", WS_CHILD|WS_CLIPSIBLINGS|WS_CLIPCHILDREN, 0, 0, 64, 64, mhwnd, (HMENU)2, g_hInst, NULL);

	if (!mhwndInputFrame || !mhwndOutputFrame) {
		Detach();
		return false;
	}

	mhwndInputDisplay = CreateWindowEx(WS_EX_TRANSPARENT, VIDEODISPLAYCONTROLCLASS, "", WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 64, 64, mhwndInputFrame, (HMENU)1, g_hInst, NULL);
	mhwndOutputDisplay = CreateWindowEx(WS_EX_TRANSPARENT, VIDEODISPLAYCONTROLCLASS, "", WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 64, 64, mhwndOutputFrame, (HMENU)2, g_hInst, NULL);

	if (!mhwndInputDisplay || !mhwndOutputDisplay) {
		Detach();
		return false;
	}

	mpInputDisplay = VDGetIVideoDisplay(mhwndInputDisplay);
	mpOutputDisplay = VDGetIVideoDisplay(mhwndOutputDisplay);

	mpInputDisplay->SetCallback(this);
	mpOutputDisplay->SetCallback(this);

	VDGetIVideoWindow(mhwndInputFrame)->SetChild(mhwndInputDisplay);
	VDGetIVideoWindow(mhwndOutputFrame)->SetChild(mhwndOutputDisplay);

	guiRedoWindows(mhwnd);
	SetMenu(mhwnd, mhMenuNormal);
	UpdateMRUList();

	return true;
}

void VDProjectUI::Detach() {
	if (mhwndStatus) {
		DestroyWindow(mhwndStatus);
		mhwndStatus = 0;
	}

	if (mhwndInputDisplay) {
		DestroyWindow(mhwndInputDisplay);
		mhwndInputDisplay = 0;
		mpInputDisplay = 0;
	}

	if (mhwndOutputDisplay) {
		DestroyWindow(mhwndOutputDisplay);
		mhwndOutputDisplay = 0;
		mpOutputDisplay = 0;
	}

	if (mhwndInputFrame) {
		DestroyWindow(mhwndInputFrame);
		mhwndInputFrame = 0;
	}

	if (mhwndOutputFrame) {
		DestroyWindow(mhwndOutputFrame);
		mhwndOutputFrame = 0;
	}

	if (mhwndPosition) {
		DestroyWindow(mhwndPosition);
		mhwndPosition = 0;
	}

	if (mOldWndProc) {
		if (IsWindowUnicode(mhwnd))
			SetWindowLongPtrW(mhwnd, GWL_WNDPROC, (LONG_PTR)mOldWndProc);
		else
			SetWindowLongPtrA(mhwnd, GWL_WNDPROC, (LONG_PTR)mOldWndProc);

		mOldWndProc = 0;
	}

	// Hmm... no destroy for accelerators.

	if (mhMenuDisplay) {
		DestroyMenu(mhMenuDisplay);
		mhMenuDisplay = 0;
	}

	if (mhMenuDub) {
		DestroyMenu(mhMenuDub);
		mhMenuDub = 0;
	}

	if (mhMenuDisplay) {
		DestroyMenu(mhMenuNormal);
		mhMenuNormal = 0;
	}

	mhwnd = 0;
}

void VDProjectUI::SetTitle(int nTitleString, int nArgs, ...) {
	const void *args[16];

	VDASSERT(nArgs < 16);

	char version[128];
	LoadString(g_hInst, IDS_TITLE_NOFILE, version, sizeof version);

	VDStringW versionW(VDTextAToW(version));
	const wchar_t *pVersion = versionW.c_str();
	args[0] = &pVersion;

	va_list val;
	va_start(val, nArgs);
	for(int i=0; i<nArgs; ++i)
		args[i+1] = va_arg(val, const void *);
	va_end(val);

	const VDStringW title(VDaswprintf(VDLoadString(0, kVDST_ProjectUI, nTitleString), nArgs+1, args));

	if (GetVersion() < 0x80000000) {
		SetWindowTextW(mhwnd, title.c_str());
	} else {
		SetWindowTextA(mhwnd, VDTextWToA(title).c_str());
	}
}

void VDProjectUI::OpenAsk() {
	OpenAVI(false);
}

void VDProjectUI::AppendAsk() {
	if (!inputAVI)
		return;

	static const VDFileDialogOption sOptions[]={
		{ VDFileDialogOption::kBool, 0, L"&Autodetect additional segments by filename", 0, 0 },
		{0}
	};

	int optVals[1]={true};

	VDStringW fname(VDGetLoadFileName(VDFSPECKEY_LOADVIDEOFILE, (VDGUIHandle)mhwnd, L"Append AVI segment", fileFiltersAppend, NULL, sOptions, optVals));

	if (fname.empty())
		return;

	VDAutoLogDisplay logDisp;

	if (optVals[0])
		AppendAVIAutoscan(fname.c_str());
	else
		AppendAVI(fname.c_str());

	logDisp.Post((VDGUIHandle)mhwnd);
}

void VDProjectUI::SaveAVIAsk() {
	SaveAVI(mhwnd, false);
	JobUnlockDubber();
}

void VDProjectUI::SaveCompatibleAVIAsk() {
	SaveAVI(mhwnd, true);
}

void VDProjectUI::SaveStripedAVIAsk() {
	if (!inputVideoAVI)
		throw MyError("No input video stream to process.");

	const VDStringW filename(VDGetSaveFileName(kFileDialog_AVIStripe, (VDGUIHandle)mhwnd, L"Select AVI stripe definition file", fileFiltersStripe, g_prefs.main.fAttachExtension ? L"stripe" : NULL));

	if (!filename.empty()) {
		SaveStripedAVI(filename.c_str());
	}
}

void VDProjectUI::SaveStripeMasterAsk() {
	if (!inputVideoAVI)
		throw MyError("No input video stream to process.");

	const VDStringW filename(VDGetSaveFileName(kFileDialog_AVIStripe, (VDGUIHandle)mhwnd, L"Select AVI stripe definition file", fileFiltersStripe, g_prefs.main.fAttachExtension ? L"stripe" : NULL));

	if (!filename.empty()) {
		SaveStripeMaster(filename.c_str());
	}
}

void VDProjectUI::SaveImageSequenceAsk() {
	SaveImageSeq(mhwnd);
}

void VDProjectUI::SaveSegmentedAVIAsk() {
	SaveSegmentedAVI(mhwnd);
}

void VDProjectUI::SaveWAVAsk() {
	SetAudioSource();

	if (!inputAudio)
		throw MyError("No input audio stream to extract.");

	const VDStringW filename(VDGetSaveFileName(kFileDialog_WAVAudioOut, (VDGUIHandle)mhwnd, L"Save WAV File", g_szWAVFileFilters, g_prefs.main.fAttachExtension ? L"wav" : NULL));

	if (!filename.empty()) {
		SaveWAV(filename.c_str());
	}
}

void VDProjectUI::SaveConfigurationAsk() {
	SaveConfiguration(mhwnd);
}

void VDProjectUI::LoadConfigurationAsk() {
	RunScript(NULL, (void *)mhwnd);
}

void VDProjectUI::SetVideoFiltersAsk() {
	CPUTest();
	ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_FILTERS), mhwnd, FilterDlgProc);
	RedrawWindow(mhwnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
	UIVideoFiltersUpdated();
}

void VDProjectUI::SetVideoFramerateOptionsAsk() {
	SetAudioSource();
	ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_VIDEO_FRAMERATE), mhwnd, VideoDecimationDlgProc);
	RecalcPositionTimeConstant();
}

void VDProjectUI::SetVideoDepthOptionsAsk() {
	ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_VIDEO_DEPTH), mhwnd, VideoDepthDlgProc);
}

void VDProjectUI::SetVideoRangeOptionsAsk() {
	ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_VIDEO_CLIPPING), mhwnd, VideoClippingDlgProc);
}

void VDProjectUI::SetVideoCompressionAsk() {
	if (!(g_Vcompression.dwFlags & ICMF_COMPVARS_VALID)) {
		memset(&g_Vcompression, 0, sizeof g_Vcompression);
		g_Vcompression.dwFlags |= ICMF_COMPVARS_VALID;
		g_Vcompression.lQ = 10000;
	}

	g_Vcompression.cbSize = sizeof(COMPVARS);

	ChooseCompressor(mhwnd, &g_Vcompression, NULL);
}

void VDProjectUI::SetVideoErrorModeAsk() {
	extern DubSource::ErrorMode VDDisplayErrorModeDialog(VDGUIHandle hParent, DubSource::ErrorMode oldMode, const char *pszSettingsKey, DubSource *pSource);
	g_videoErrorMode = VDDisplayErrorModeDialog((VDGUIHandle)mhwnd, g_videoErrorMode, "Edit: Video error mode", inputVideoAVI);

	if (inputVideoAVI)
		inputVideoAVI->setDecodeErrorMode(g_videoErrorMode);
}

void VDProjectUI::SetAudioFiltersAsk() {
	extern void VDDisplayAudioFilterDialog(VDGUIHandle, VDAudioFilterGraph&);
	SetAudioSource();
	CPUTest();
	VDDisplayAudioFilterDialog((VDGUIHandle)mhwnd, g_audioFilterGraph);
}

void VDProjectUI::SetAudioConversionOptionsAsk() {
	ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_AUDIO_CONVERSION), mhwnd, AudioConversionDlgProc);
}

void VDProjectUI::SetAudioInterleaveOptionsAsk() {
	ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_INTERLEAVE), mhwnd, AudioInterleaveDlgProc);
}

void VDProjectUI::SetAudioCompressionAsk() {
	SetAudioSource();

	if (!inputAudio)
		g_ACompressionFormat = AudioChooseCompressor(mhwnd, g_ACompressionFormat, NULL);
	else {

		PCMWAVEFORMAT wfex;

		memcpy(&wfex, inputAudio->getWaveFormat(), sizeof(PCMWAVEFORMAT));
		// Say 16-bit if the source was compressed.

		if (wfex.wf.wFormatTag != WAVE_FORMAT_PCM)
			wfex.wBitsPerSample = 16;

		wfex.wf.wFormatTag = WAVE_FORMAT_PCM;

		switch(g_dubOpts.audio.newPrecision) {
		case DubAudioOptions::P_8BIT:	wfex.wBitsPerSample = 8; break;
		case DubAudioOptions::P_16BIT:	wfex.wBitsPerSample = 16; break;
		}

		switch(g_dubOpts.audio.newPrecision) {
		case DubAudioOptions::C_MONO:	wfex.wf.nChannels = 1; break;
		case DubAudioOptions::C_STEREO:	wfex.wf.nChannels = 2; break;
		}

		if (g_dubOpts.audio.new_rate) {
			long samp_frac;

			if (g_dubOpts.audio.integral_rate)
				if (g_dubOpts.audio.new_rate > wfex.wf.nSamplesPerSec)
					samp_frac = 0x10000 / ((g_dubOpts.audio.new_rate + wfex.wf.nSamplesPerSec/2) / wfex.wf.nSamplesPerSec); 
				else
					samp_frac = 0x10000 * ((wfex.wf.nSamplesPerSec + g_dubOpts.audio.new_rate/2) / g_dubOpts.audio.new_rate);
			else
				samp_frac = MulDiv(wfex.wf.nSamplesPerSec, 0x10000L, g_dubOpts.audio.new_rate);

			wfex.wf.nSamplesPerSec = MulDiv(wfex.wf.nSamplesPerSec, 0x10000L, samp_frac);
		}

		wfex.wf.nBlockAlign = (wfex.wBitsPerSample+7)/8 * wfex.wf.nChannels;
		wfex.wf.nAvgBytesPerSec = wfex.wf.nSamplesPerSec * wfex.wf.nBlockAlign;

		g_ACompressionFormat = AudioChooseCompressor(mhwnd, g_ACompressionFormat, (WAVEFORMATEX *)&wfex);

	}

	if (g_ACompressionFormat) {
		g_ACompressionFormatSize = sizeof(WAVEFORMATEX) + g_ACompressionFormat->cbSize;
	}
}

void VDProjectUI::SetAudioVolumeOptionsAsk() {
	ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_AUDIO_VOLUME), mhwnd, AudioVolumeDlgProc);
}

void VDProjectUI::SetAudioSourceWAVAsk() {
	const VDStringW filename(VDGetLoadFileName(kFileDialog_WAVAudioIn, (VDGUIHandle)mhwnd, L"Open WAV File", g_szWAVFileFilters, NULL));

	if (!filename.empty()) {
		OpenWAV(filename.c_str());
		wcscpy(g_szInputWAVFile, filename.c_str());
		SetAudioSource();
	}
}

void VDProjectUI::SetAudioErrorModeAsk() {
	extern DubSource::ErrorMode VDDisplayErrorModeDialog(VDGUIHandle hParent, DubSource::ErrorMode oldMode, const char *pszSettingsKey, DubSource *pSource);
	SetAudioSource();
	g_audioErrorMode = VDDisplayErrorModeDialog((VDGUIHandle)mhwnd, g_audioErrorMode, "Edit: Audio error mode", inputAudio);

	if (inputAudioAVI)
		inputAudioAVI->setDecodeErrorMode(g_audioErrorMode);
	if (inputAudioWAV)
		inputAudioWAV->setDecodeErrorMode(g_audioErrorMode);
}

void VDProjectUI::JumpToFrameAsk() {
	if (inputAVI) {
		long lFrame;

		lFrame = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_JUMPTOFRAME), mhwnd, VideoJumpDlgProc, GetCurrentFrame());

		if (lFrame >= 0)
			MoveToFrame(lFrame);
	}
}

bool VDProjectUI::MenuHit(UINT id) {
	bool bFilterReinitialize = !g_dubber;

	if (bFilterReinitialize) {
		switch(id) {
		case ID_VIDEO_SEEK_START:
		case ID_VIDEO_SEEK_END:
		case ID_VIDEO_SEEK_PREV:
		case ID_VIDEO_SEEK_NEXT:
		case ID_VIDEO_SEEK_PREVONESEC:
		case ID_VIDEO_SEEK_NEXTONESEC:
		case ID_VIDEO_SEEK_KEYPREV:
		case ID_VIDEO_SEEK_KEYNEXT:
		case ID_VIDEO_SEEK_SELSTART:
		case ID_VIDEO_SEEK_SELEND:
		case ID_VIDEO_SEEK_PREVDROP:
		case ID_VIDEO_SEEK_NEXTDROP:
		case ID_EDIT_JUMPTO:
		case ID_VIDEO_COPYSOURCEFRAME:
		case ID_VIDEO_COPYOUTPUTFRAME:
			break;
		default:
			filters.DeinitFilters();
			filters.DeallocateBuffers();
			break;
		}
	}

	SetAudioSource();
	JobLockDubber();
	DragAcceptFiles(mhwnd, FALSE);

	try {
		switch(id) {
		case ID_FILE_QUIT:						Quit();						break;
		case ID_FILE_OPENAVI:					OpenAsk();						break;
		case ID_FILE_APPENDSEGMENT:				AppendAsk();					break;
		case ID_FILE_PREVIEWINPUT:				PreviewInput();				break;
		case ID_FILE_PREVIEWOUTPUT:				PreviewOutput();				break;
		case ID_FILE_PREVIEWAVI:				PreviewAll();					break;
		case ID_FILE_SAVEAVI:					SaveAVIAsk();					break;
		case ID_FILE_SAVECOMPATIBLEAVI:			SaveCompatibleAVIAsk();		break;
		case ID_FILE_SAVESTRIPEDAVI:			SaveStripedAVIAsk();			break;
		case ID_FILE_SAVESTRIPEMASTER:			SaveStripeMasterAsk();			break;
		case ID_FILE_SAVEIMAGESEQ:				SaveImageSequenceAsk();		break;
		case ID_FILE_SAVESEGMENTEDAVI:			SaveSegmentedAVIAsk();			break;
		case ID_FILE_SAVEWAV:					SaveWAVAsk();					break;
		case ID_FILE_CLOSEAVI:					Close();						break;
		case ID_FILE_STARTSERVER:				StartServer();					break;
		case ID_FILE_CAPTUREAVI:				SwitchToCaptureMode();			break;
		case ID_FILE_SAVECONFIGURATION:			SaveConfigurationAsk();		break;
		case ID_FILE_LOADCONFIGURATION:
		case ID_FILE_RUNSCRIPT:
			LoadConfigurationAsk();
			break;
		case ID_FILE_JOBCONTROL:				OpenJobWindow();							break;
		case ID_FILE_AVIINFO:					ShowInputInfo();					break;

		case ID_EDIT_CUT:						Cut();						break;
		case ID_EDIT_COPY:						Copy();						break;
		case ID_EDIT_PASTE:						Paste();					break;
		case ID_EDIT_DELETE:					Delete();					break;
		case ID_EDIT_CLEAR:						ClearSelection();			break;
		case ID_EDIT_SELECTALL:
			SetSelectionStart(0);
			SetSelectionEnd(GetFrameCount());
			break;

		case ID_VIDEO_SEEK_START:				MoveToStart();				break;
		case ID_VIDEO_SEEK_END:					MoveToEnd();				break;
		case ID_VIDEO_SEEK_PREV:				MoveToPrevious();			break;
		case ID_VIDEO_SEEK_NEXT:				MoveToNext();				break;
		case ID_VIDEO_SEEK_PREVONESEC:			MoveBackSome();			break;
		case ID_VIDEO_SEEK_NEXTONESEC:			MoveForwardSome();			break;
		case ID_VIDEO_SEEK_KEYPREV:				MoveToPreviousKey();		break;
		case ID_VIDEO_SEEK_KEYNEXT:				MoveToNextKey();			break;
		case ID_VIDEO_SEEK_SELSTART:			MoveToSelectionStart();	break;
		case ID_VIDEO_SEEK_SELEND:				MoveToSelectionEnd();		break;
		case ID_VIDEO_SEEK_PREVDROP:			MoveToPreviousDrop();		break;
		case ID_VIDEO_SEEK_NEXTDROP:			MoveToNextDrop();			break;
		case ID_VIDEO_SCANFORERRORS:			ScanForErrors();			break;
		case ID_EDIT_JUMPTO:					JumpToFrameAsk();			break;
		case ID_EDIT_RESET:						ResetTimelineWithConfirmation();		break;
		case ID_EDIT_PREVRANGE:					MoveToPreviousRange();		break;
		case ID_EDIT_NEXTRANGE:					MoveToNextRange();			break;

		case ID_VIDEO_FILTERS:					SetVideoFiltersAsk();				break;
		case ID_VIDEO_FRAMERATE:				SetVideoFramerateOptionsAsk();		break;
		case ID_VIDEO_COLORDEPTH:				SetVideoDepthOptionsAsk();			break;
		case ID_VIDEO_CLIPPING:					SetVideoRangeOptionsAsk();			break;
		case ID_VIDEO_COMPRESSION:				SetVideoCompressionAsk();			break;
		case ID_VIDEO_MODE_DIRECT:				SetVideoMode(DubVideoOptions::M_NONE);			break;
		case ID_VIDEO_MODE_FASTRECOMPRESS:		SetVideoMode(DubVideoOptions::M_FASTREPACK);			break;
		case ID_VIDEO_MODE_NORMALRECOMPRESS:	SetVideoMode(DubVideoOptions::M_SLOWREPACK);			break;
		case ID_VIDEO_MODE_FULL:				SetVideoMode(DubVideoOptions::M_FULL);			break;
		case ID_VIDEO_COPYSOURCEFRAME:			CopySourceFrameToClipboard();		break;
		case ID_VIDEO_COPYOUTPUTFRAME:			CopyOutputFrameToClipboard();		break;
		case ID_VIDEO_ERRORMODE:				SetVideoErrorModeAsk();			break;
		case ID_EDIT_MASK:						MaskSelection(true);							break;
		case ID_EDIT_UNMASK:					MaskSelection(false);						break;
		case ID_EDIT_SETSELSTART:				SetSelectionStart();				break;
		case ID_EDIT_SETSELEND:					SetSelectionEnd();					break;

		case ID_AUDIO_ADVANCEDFILTERING:
			g_dubOpts.audio.bUseAudioFilterGraph = !g_dubOpts.audio.bUseAudioFilterGraph;
			break;

		case ID_AUDIO_FILTERS:					SetAudioFiltersAsk();				break;

		case ID_AUDIO_CONVERSION:				SetAudioConversionOptionsAsk();	break;
		case ID_AUDIO_INTERLEAVE:				SetAudioInterleaveOptionsAsk();	break;
		case ID_AUDIO_COMPRESSION:				SetAudioCompressionAsk();			break;

		case ID_AUDIO_VOLUME:					SetAudioVolumeOptionsAsk();		break;

		case ID_AUDIO_SOURCE_NONE:				SetAudioSourceNone();				break;
		case ID_AUDIO_SOURCE_AVI:				SetAudioSourceNormal();			break;
		case ID_AUDIO_SOURCE_WAV:				SetAudioSourceWAVAsk();			break;

		case ID_AUDIO_MODE_DIRECT:				SetAudioMode(DubAudioOptions::M_NONE);			break;
		case ID_AUDIO_MODE_FULL:				SetAudioMode(DubAudioOptions::M_FULL);			break;

		case ID_AUDIO_ERRORMODE:			SetAudioErrorModeAsk();			break;

		case ID_OPTIONS_SHOWLOG:
			extern void VDOpenLogWindow();
			VDOpenLogWindow();
			break;

		case ID_OPTIONS_SHOWPROFILER:
			extern void VDOpenProfileWindow();
			VDOpenProfileWindow();
			break;

		case ID_OPTIONS_PERFORMANCE:
			ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_PERFORMANCE), mhwnd, PerformanceOptionsDlgProc);
			break;
		case ID_OPTIONS_DYNAMICCOMPILATION:
			ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_PERF_DYNAMIC), mhwnd, DynamicCompileOptionsDlgProc);
			break;
		case ID_OPTIONS_PREFERENCES:
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_PREFERENCES), mhwnd, PreferencesDlgProc);
			break;
		case ID_OPTIONS_DISPLAYINPUTVIDEO:
			if (mpDubStatus)
				mpDubStatus->ToggleFrame(false);
			else
				g_dubOpts.video.fShowInputFrame = !g_dubOpts.video.fShowInputFrame;
			break;
		case ID_OPTIONS_DISPLAYOUTPUTVIDEO:
			if (mpDubStatus)
				mpDubStatus->ToggleFrame(true);
			else
				g_dubOpts.video.fShowOutputFrame = !g_dubOpts.video.fShowOutputFrame;
			break;
		case ID_OPTIONS_DISPLAYDECOMPRESSEDOUTPUT:
			g_drawDecompressedFrame = !g_drawDecompressedFrame;
			break;
		case ID_OPTIONS_SHOWSTATUSWINDOW:
			if (mpDubStatus)
				mpDubStatus->ToggleStatus();
			else
				g_showStatusWindow = !g_showStatusWindow;
			break;
		case ID_OPTIONS_VERTICALDISPLAY:
			g_vertical = !g_vertical;
			RepositionPanes();
			break;
		case ID_OPTIONS_SYNCTOAUDIO:
			g_dubOpts.video.fSyncToAudio = !g_dubOpts.video.fSyncToAudio;
			break;
		case ID_OPTIONS_ENABLEDIRECTDRAW:
			g_dubOpts.perf.useDirectDraw = !g_dubOpts.perf.useDirectDraw;
			break;
		case ID_OPTIONS_DROPFRAMES:
			g_fDropFrames = !g_fDropFrames;
			break;
		case ID_OPTIONS_SWAPPANES:
			g_fSwapPanes = !g_fSwapPanes;
			RepositionPanes();
			break;

		case ID_OPTIONS_PREVIEWPROGRESSIVE:	g_dubOpts.video.nPreviewFieldMode = 0; break;
		case ID_OPTIONS_PREVIEWFIELDA:		g_dubOpts.video.nPreviewFieldMode = 1; break;
		case ID_OPTIONS_PREVIEWFIELDB:		g_dubOpts.video.nPreviewFieldMode = 2; break;


		case ID_TOOLS_HEXVIEWER:
			HexEdit(NULL);
			break;

		case ID_TOOLS_CREATESPARSEAVI:
			CreateExtractSparseAVI(mhwnd, false);
			break;

		case ID_TOOLS_EXPANDSPARSEAVI:
			CreateExtractSparseAVI(mhwnd, true);
			break;

		case ID_HELP_LICENSE:
			DisplayLicense(mhwnd);
			break;

		case ID_HELP_CONTENTS:
			VDShowHelp(mhwnd);
			break;
		case ID_HELP_CHANGELOG:
			DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_SHOWTEXT), mhwnd, ShowTextDlgProc, (LPARAM)MAKEINTRESOURCE(IDR_CHANGES));
			break;
		case ID_HELP_RELEASENOTES:
			DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_SHOWTEXT), mhwnd, ShowTextDlgProc, (LPARAM)MAKEINTRESOURCE(IDR_RELEASE_NOTES));
			break;
		case ID_HELP_ABOUT:
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), mhwnd, AboutDlgProc);
			break;

		case ID_HELP_ONLINE_HOME:	LaunchURL("http://www.virtualdub.org/index"); break;
		case ID_HELP_ONLINE_FAQ:	LaunchURL("http://www.virtualdub.org/virtualdub_faq"); break;
		case ID_HELP_ONLINE_NEWS:	LaunchURL("http://www.virtualdub.org/virtualdub_news"); break;
		case ID_HELP_ONLINE_KB:		LaunchURL("http://www.virtualdub.org/virtualdub_kb"); break;

		case ID_DUBINPROGRESS_ABORTFAST:
			if (g_dubber && !g_dubber->IsPreviewing())
				break;
			// fall through
		case ID_DUBINPROGRESS_ABORT:			AbortOperation();			break;

		default:
			if (id >= ID_MRU_FILE0 && id <= ID_MRU_FILE3) {
				const int index = id - ID_MRU_FILE0;
				VDStringW name(mMRUList[index]);

				if (!name.empty()) {
					const bool bExtendedOpen = (signed short)GetAsyncKeyState(VK_SHIFT) < 0;

					VDAutoLogDisplay logDisp;
					g_project->Open(name.c_str(), NULL, bExtendedOpen, false, true);
					logDisp.Post((VDGUIHandle)mhwnd);
				}
				break;
			}
			break;
		}
	} catch(const MyError& e) {
		e.post(mhwnd, g_szError);
	}

	JobUnlockDubber();
	DragAcceptFiles(mhwnd, TRUE);

	return true;
}

void VDProjectUI::RepaintMainWindow(HWND hWnd) {
	PAINTSTRUCT ps;
	HDC hDC;

	hDC = BeginPaint(hWnd, &ps);
	EndPaint(hWnd, &ps);
}

void VDProjectUI::ShowMenuHelp(WPARAM wParam) {
	if (LOWORD(wParam) >= ID_MRU_FILE0 && LOWORD(wParam) <= ID_MRU_FILE3) {
		HWND hwndStatus = GetDlgItem(mhwnd, IDC_STATUS_WINDOW);
		char name[1024];

		if ((HIWORD(wParam) & MF_POPUP) || (HIWORD(wParam) & MF_SYSMENU)) {
			SendMessage(hwndStatus, SB_SETTEXT, 0, (LPARAM)"");
			return;
		}

		strcpy(name, "[SHIFT for options] Load file ");

		const VDStringW filename(mMRUList[LOWORD(wParam) - ID_MRU_FILE0]);

		if (!filename.empty()) {
			VDTextWToA(name+30, sizeof name - 30, filename.data(), filename.length());
			SendMessage(hwndStatus, SB_SETTEXT, 255, (LPARAM)name);
		} else
			SendMessage(hwndStatus, SB_SETTEXT, 255, (LPARAM)"");
	} else
		guiMenuHelp(mhwnd, wParam, 255, iMainMenuHelpTranslator);
}

void VDProjectUI::UpdateMainMenu(HMENU hMenu) {
	CheckMenuRadioItem(hMenu, ID_AUDIO_SOURCE_NONE, ID_AUDIO_SOURCE_WAV, ID_AUDIO_SOURCE_NONE+audioInputMode, MF_BYCOMMAND);
	CheckMenuRadioItem(hMenu, ID_VIDEO_MODE_DIRECT, ID_VIDEO_MODE_FULL, ID_VIDEO_MODE_DIRECT+g_dubOpts.video.mode, MF_BYCOMMAND);
	CheckMenuRadioItem(hMenu, ID_AUDIO_MODE_DIRECT, ID_AUDIO_MODE_FULL, ID_AUDIO_MODE_DIRECT+g_dubOpts.audio.mode, MF_BYCOMMAND);
	CheckMenuRadioItem(hMenu, ID_OPTIONS_PREVIEWPROGRESSIVE, ID_OPTIONS_PREVIEWFIELDB,
		ID_OPTIONS_PREVIEWPROGRESSIVE+g_dubOpts.video.nPreviewFieldMode, MF_BYCOMMAND);

	VDCheckMenuItemW32(hMenu, ID_AUDIO_ADVANCEDFILTERING,			g_dubOpts.audio.bUseAudioFilterGraph);

	VDCheckMenuItemW32(hMenu, ID_OPTIONS_DISPLAYINPUTVIDEO,			g_dubOpts.video.fShowInputFrame);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_DISPLAYOUTPUTVIDEO,		g_dubOpts.video.fShowOutputFrame);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_DISPLAYDECOMPRESSEDOUTPUT,	g_drawDecompressedFrame);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_SHOWSTATUSWINDOW,			g_showStatusWindow);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_VERTICALDISPLAY,			g_vertical);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_SYNCTOAUDIO,				g_dubOpts.video.fSyncToAudio);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_ENABLEDIRECTDRAW,			g_dubOpts.perf.useDirectDraw);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_DROPFRAMES,				g_fDropFrames);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_SWAPPANES,					g_fSwapPanes);

	const bool bAVISourceExists = (inputAVI && inputAVI->Append(NULL));
	VDEnableMenuItemW32(hMenu,ID_FILE_APPENDSEGMENT			, bAVISourceExists);

	const bool bSourceFileExists = (inputAVI != 0);
	VDEnableMenuItemW32(hMenu,ID_FILE_PREVIEWAVI			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu,ID_FILE_PREVIEWINPUT			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu,ID_FILE_PREVIEWOUTPUT			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu,ID_FILE_SAVEAVI				, bSourceFileExists);
	VDEnableMenuItemW32(hMenu,ID_FILE_SAVECOMPATIBLEAVI		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu,ID_FILE_SAVESTRIPEDAVI		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu,ID_FILE_SAVESTRIPEMASTER		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu,ID_FILE_SAVEIMAGESEQ			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu,ID_FILE_SAVESEGMENTEDAVI		, bSourceFileExists);
	VDEnableMenuItemW32(hMenu,ID_FILE_SAVEWAV				, bSourceFileExists);
	VDEnableMenuItemW32(hMenu,ID_FILE_CLOSEAVI				, bSourceFileExists);
	VDEnableMenuItemW32(hMenu,ID_FILE_STARTSERVER			, bSourceFileExists);
	VDEnableMenuItemW32(hMenu,ID_FILE_AVIINFO				, bSourceFileExists);

	VDEnableMenuItemW32(hMenu,ID_VIDEO_COPYSOURCEFRAME		, inputVideoAVI && inputVideoAVI->isFrameBufferValid());
	VDEnableMenuItemW32(hMenu,ID_VIDEO_COPYOUTPUTFRAME		, inputVideoAVI && filters.isRunning());
	VDEnableMenuItemW32(hMenu,ID_VIDEO_SCANFORERRORS		, inputVideoAVI != 0);

	const bool bAudioProcessingEnabled			= (g_dubOpts.audio.mode == DubAudioOptions::M_FULL);
	const bool bUseFixedFunctionAudioPipeline	= bAudioProcessingEnabled && !g_dubOpts.audio.bUseAudioFilterGraph;
	const bool bUseProgrammableAudioPipeline	= bAudioProcessingEnabled && g_dubOpts.audio.bUseAudioFilterGraph;

	VDEnableMenuItemW32(hMenu,ID_AUDIO_ADVANCEDFILTERING	, bAudioProcessingEnabled);
	VDEnableMenuItemW32(hMenu,ID_AUDIO_COMPRESSION			, bAudioProcessingEnabled);
	VDEnableMenuItemW32(hMenu,ID_AUDIO_CONVERSION			, bUseFixedFunctionAudioPipeline);
	VDEnableMenuItemW32(hMenu,ID_AUDIO_VOLUME				, bUseFixedFunctionAudioPipeline);
	VDEnableMenuItemW32(hMenu,ID_AUDIO_FILTERS				, bUseProgrammableAudioPipeline);

	const bool bVideoFullProcessingEnabled = (g_dubOpts.video.mode >= DubVideoOptions::M_FULL);
	VDEnableMenuItemW32(hMenu,ID_VIDEO_FILTERS				, bVideoFullProcessingEnabled);

	const bool bVideoConversionEnabled = (g_dubOpts.video.mode >= DubVideoOptions::M_SLOWREPACK);
	VDEnableMenuItemW32(hMenu,ID_VIDEO_COLORDEPTH			, bVideoConversionEnabled);

	const bool bVideoCompressionEnabled = (g_dubOpts.video.mode >= DubVideoOptions::M_FASTREPACK);
	VDEnableMenuItemW32(hMenu,ID_VIDEO_COMPRESSION			, bVideoCompressionEnabled);
}

void VDProjectUI::UpdateDubMenu(HMENU hMenu) {
	bool fShowStatusWindow = mpDubStatus->isVisible();
	bool fShowInputFrame = mpDubStatus->isFrameVisible(false);
	bool fShowOutputFrame = mpDubStatus->isFrameVisible(true);

	VDCheckMenuItemW32(hMenu, ID_OPTIONS_DISPLAYINPUTVIDEO, fShowInputFrame);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_DISPLAYOUTPUTVIDEO, fShowOutputFrame);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_SHOWSTATUSWINDOW, fShowStatusWindow);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_VERTICALDISPLAY,			g_vertical);
	VDCheckMenuItemW32(hMenu, ID_OPTIONS_SWAPPANES,					g_fSwapPanes);
}

LRESULT VDProjectUI::StaticWndProc( HWND hWnd, UINT msg, UINT wParam, LONG lParam) {
	return (g_projectui->*(g_projectui->mpWndProc))(msg, wParam, lParam);
}

LRESULT VDProjectUI::MainWndProc( UINT msg, UINT wParam, LONG lParam) {
	static HWND hwndItem0 = NULL;

    switch (msg) {
 
	case WM_INITMENU:
		UpdateMainMenu((HMENU)wParam);
		break;

	case WM_COMMAND:           // message: command from application menu
		if (lParam) {
			switch(LOWORD(wParam)) {
			case IDC_POSITION:
				if (inputVideoAVI) {
					try {
						switch(HIWORD(wParam)) {
						case PCN_PLAY:				PreviewInput();				break;
						case PCN_PLAYPREVIEW:		PreviewOutput();				break;
						case PCN_MARKIN:			SetSelectionStart();			break;
						case PCN_MARKOUT:			SetSelectionEnd();				break;
						case PCN_START:				MoveToStart();					break;
						case PCN_BACKWARD:			MoveToPrevious();				break;
						case PCN_FORWARD:			MoveToNext();					break;
						case PCN_END:				MoveToEnd();					break;
						case PCN_KEYPREV:			MoveToPreviousKey();			break;
						case PCN_KEYNEXT:			MoveToNextKey();				break;
						case PCN_SCENEREV:			StartSceneShuttleReverse();	break;
						case PCN_SCENEFWD:			StartSceneShuttleForward();	break;
						case PCN_STOP:
						case PCN_SCENESTOP:
							SceneShuttleStop();
							break;
						}
					} catch(const MyError& e) {
						e.post(mhwnd, g_szError);
					}
				}
				break;
			}
		} else if (MenuHit(LOWORD(wParam)))
			return 0;

		break;

	case WM_SIZE:
		guiRedoWindows(mhwnd);
		return 0;

	case WM_DESTROY:                  // message: window being destroyed
		Detach();
		PostQuitMessage(0);
		break;

	case WM_PAINT:
		RepaintMainWindow(mhwnd);
		return 0;

	case WM_MENUSELECT:
		ShowMenuHelp(wParam);
		return 0;

	case WM_NOTIFY:
		{
			LPNMHDR nmh = (LPNMHDR)lParam;
			LONG pos;

			switch(nmh->idFrom) {
			case IDC_POSITION:
				switch(nmh->code) {
				case PCN_BEGINTRACK:
					guiSetStatus("Seeking: hold SHIFT to snap to keyframes", 255);
					SendMessage(nmh->hwndFrom, PCM_CTLAUTOFRAME, 0, 0);
					break;
				case PCN_ENDTRACK:
					guiSetStatus("", 255);
					SendMessage(nmh->hwndFrom, PCM_CTLAUTOFRAME, 0, 1);
					break;
				case PCN_THUMBPOSITION:
				case PCN_THUMBTRACK:
				case PCN_PAGELEFT:
				case PCN_PAGERIGHT:
					pos = SendMessage(nmh->hwndFrom, PCM_GETPOS, 0, 0);

					if (inputVideoAVI) {
						if (GetKeyState(VK_SHIFT)<0) {
							long lSample2;
							bool bMasked;

							lSample2 = inputSubset->lookupFrame(pos);

							lSample2 = inputVideoAVI->nearestKey(lSample2);
							pos = inputSubset->revLookupFrame(lSample2, bMasked);
							if (bMasked)
								pos = -1;

							if (nmh->code != PCN_THUMBTRACK && pos >= 0)
								SendMessage(nmh->hwndFrom, PCM_SETPOS, TRUE, pos);
						}

						if (pos >= 0) {
							if (nmh->code == PCN_THUMBTRACK)
								SendMessage(nmh->hwndFrom, PCM_SETDISPFRAME, 0, pos);

							MoveToFrame(pos);
						}
					}
					break;
				}
				break;

			case 1:
			case 2:
				switch(nmh->code) {
				case VWN_RESIZED:
					if (nmh->idFrom == 1) {
						GetClientRect(nmh->hwndFrom, &mrInputFrame);
					} else {
						GetClientRect(nmh->hwndFrom, &mrOutputFrame);
					}

					RepositionPanes();
					break;
				case VWN_REQUPDATE:
					if (nmh->idFrom == 1)
						UIRefreshInputFrame(inputVideoAVI && inputVideoAVI->isFrameBufferValid());
					else
						UIRefreshOutputFrame(filters.isRunning());
					break;
				}
				break;
			}
		}
		return 0;

	case WM_KEYDOWN:
		switch((int)wParam) {
		case VK_F12:
			guiOpenDebug();
			break;
		}
		return 0;

	case WM_DROPFILES:
		HandleDragDrop((HDROP)wParam);
		return 0;

	case WM_SETTEXT:
		if (!hwndItem0) {
			hwndItem0 = GetDlgItem(mhwnd, IDC_POSITION);

			SetWindowLong(mhwnd, GWL_USERDATA, IsWindowUnicode(mhwnd) ? FastXform<wchar_t>()((const wchar_t *)lParam) : FastXform<char>()((const char *)lParam));
		}
		break;
	}

	return IsWindowUnicode(mhwnd) ? DefWindowProcW(mhwnd, msg, wParam, lParam) : DefWindowProcA(mhwnd, msg, wParam, lParam);
}

LRESULT VDProjectUI::DubWndProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
	case WM_INITMENU:
		UpdateDubMenu((HMENU)wParam);
		break;

	case WM_COMMAND:
		if (lParam) {
			switch(LOWORD(wParam)) {
			case IDC_POSITION:
				switch(HIWORD(wParam)) {
				case PCN_STOP:
					g_dubber->Abort();
				}
				break;
			}
		} else if (!MenuHit(LOWORD(wParam)))
			return (DefWindowProc(mhwnd, msg, wParam, lParam));
		break;

	case WM_CLOSE:
		if (g_dubber->IsPreviewing()) {
			g_dubber->Abort();
			g_bExit = true;
		} else {
			if (!g_showStatusWindow)
				MenuHit(ID_OPTIONS_SHOWSTATUSWINDOW);

			if (IDYES == MessageBox(mhwnd,
					"A dub operation is currently in progress. Forcing VirtualDub to abort "
					"will leave the output file unusable and may have undesirable side effects. "
					"Do you really want to do this?"
					,"VirtualDub warning", MB_YESNO))

					ExitProcess(1000);
		}
		break;

	case WM_SIZE:
		guiRedoWindows(mhwnd);
		break;

	case WM_DESTROY:		// doh!!!!!!!
		PostQuitMessage(0);
		break;

	case WM_PAINT:
		RepaintMainWindow(mhwnd);
		return TRUE;

	case WM_PALETTECHANGED:
		if ((HWND)wParam == mhwnd)
			break;
	case WM_QUERYNEWPALETTE:
		g_dubber->RealizePalette();
		break;

	case WM_NOTIFY:
		{
			LPNMHDR nmh = (LPNMHDR)lParam;

			switch(nmh->idFrom) {
			case 1:
			case 2:
				switch(nmh->code) {
				case VWN_RESIZED:
					if (nmh->idFrom == 1) {
						GetClientRect(nmh->hwndFrom, &mrInputFrame);
					} else {
						GetClientRect(nmh->hwndFrom, &mrOutputFrame);
					}
					RepositionPanes();
					break;
				case VWN_REQUPDATE:
					// eat it
					break;
				}
				break;
			}
		}
		break;

	default:
		return IsWindowUnicode(mhwnd) ? DefWindowProcW(mhwnd, msg, wParam, lParam) : DefWindowProcA(mhwnd, msg, wParam, lParam);
    }
    return (0);
}

void VDProjectUI::HandleDragDrop(HDROP hdrop) {
	if (DragQueryFile(hdrop, -1, NULL, 0) < 1)
		return;

	VDStringW filename;

	if (GetVersion() & 0x80000000) {
		char szName[MAX_PATH];
		DragQueryFile(hdrop, 0, szName, sizeof szName);
		filename = VDTextAToW(szName);
	} else {
		wchar_t szNameW[MAX_PATH];
		typedef UINT (APIENTRY *tpDragQueryFileW)(HDROP, UINT, LPWSTR, UINT);

		if (HMODULE hmod = GetModuleHandle("shell32"))
			if (const tpDragQueryFileW pDragQueryFileW = (tpDragQueryFileW)GetProcAddress(hmod, "DragQueryFileW")) {
				pDragQueryFileW(hdrop, 0, szNameW, sizeof szNameW / sizeof szNameW[0]);
				filename = szNameW;
			}

	}
	DragFinish(hdrop);

	if (!filename.empty()) {
		try {
			VDAutoLogDisplay logDisp;

			Open(filename.c_str(), NULL, false);

			logDisp.Post((VDGUIHandle)mhwnd);
		} catch(const MyError& e) {
			e.post(mhwnd, g_szError);
		}
	}
}

void VDProjectUI::RepositionPanes() {
	HWND hwndPane1 = mhwndInputFrame;
	HWND hwndPane2 = mhwndOutputFrame;

	if (g_fSwapPanes)
		std::swap(hwndPane1, hwndPane2);

	RECT r;
	GetWindowRect(hwndPane1, &r);
	ScreenToClient(mhwnd, (LPPOINT)&r + 1);

	SetWindowPos(hwndPane1, NULL, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

	if (g_vertical)
		SetWindowPos(hwndPane2, NULL, 0, r.bottom + 8, 0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
	else
		SetWindowPos(hwndPane2, NULL, r.right+8, 0, 0, 0, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
}

void VDProjectUI::UpdateVideoFrameLayout() {
	memset(&mrInputFrame, 0, sizeof(RECT));
	memset(&mrOutputFrame, 0, sizeof(RECT));

	if (inputVideoAVI) {
		BITMAPINFOHEADER *formatIn = inputVideoAVI->getImageFormat();

		if (formatIn) {
			BITMAPINFOHEADER *dcf = inputVideoAVI->getDecompressedFormat();
			int w = dcf->biWidth;
			int h = dcf->biHeight;

			VDGetIVideoWindow(mhwndInputFrame)->SetSourceSize(w, h);
			VDGetIVideoWindow(mhwndInputFrame)->GetFrameSize(w, h);

			mrInputFrame.left = 0;
			mrInputFrame.top = 0;
			mrInputFrame.right = w;
			mrInputFrame.bottom = h;

			// figure out output size too

			int w2=dcf->biWidth, h2=dcf->biHeight;

			if (!g_listFA.IsEmpty()) {
				if (!filters.isRunning())
					filters.prepareLinearChain(&g_listFA, (Pixel *)(dcf+1), dcf->biWidth, dcf->biHeight, 32, 16+8*g_dubOpts.video.outputDepth);

				w2 = filters.OutputBitmap()->w;
				h2 = filters.OutputBitmap()->h;
			}

			VDGetIVideoWindow(mhwndOutputFrame)->SetSourceSize(w2, h2);
			VDGetIVideoWindow(mhwndOutputFrame)->GetFrameSize(w2, h2);

			mrOutputFrame.left = 0;
			mrOutputFrame.top = 0;
			mrOutputFrame.right = w2;
			mrOutputFrame.bottom = h2;
		}
	}
}

void VDProjectUI::UIRefreshInputFrame(bool bValid) {
	if (HDC hdc = GetDC(mhwndInputFrame)) {
		if (bValid) {
			BITMAPINFOHEADER *dcf = inputVideoAVI->getDecompressedFormat();
			void *lpBits = inputVideoAVI->getFrameBuffer();

			IVDVideoDisplay *pDisp = VDGetIVideoDisplay(mhwndInputDisplay);
			ptrdiff_t stride = (dcf->biWidth * (dcf->biBitCount >> 3) + 3) & ~3;
			const char *data = (const char *)lpBits + stride * (dcf->biHeight - 1);

			switch(dcf->biBitCount) {
			case 8:
				pDisp->SetSourcePalette((const uint32 *)((const char *)dcf + dcf->biSize), dcf->biClrUsed);
				pDisp->SetSource(data, -stride, dcf->biWidth, dcf->biHeight, IVDVideoDisplay::kFormatPal8);
				break;
			case 16:
				pDisp->SetSource(data, -stride, dcf->biWidth, dcf->biHeight, IVDVideoDisplay::kFormatRGB1555);
				break;
			case 24:
				pDisp->SetSource(data, -stride, dcf->biWidth, dcf->biHeight, IVDVideoDisplay::kFormatRGB888);
				break;
			case 32:
				pDisp->SetSource(data, -stride, dcf->biWidth, dcf->biHeight, IVDVideoDisplay::kFormatRGB8888);
				break;
			}

			pDisp->Update(IVDVideoDisplay::kAllFields);
		} else {
			FillRect(hdc, &mrInputFrame, (HBRUSH)GetClassLong(mhwnd, GCL_HBRBACKGROUND));
		}
		ReleaseDC(mhwndInputFrame, hdc);
	}
}

void VDProjectUI::UIRefreshOutputFrame(bool bValid) {
	if (HDC hdc = GetDC(mhwndOutputFrame)) {
		if (bValid) {
			VBitmap *out = filters.LastBitmap();

			IVDVideoDisplay *pDisp = VDGetIVideoDisplay(mhwndOutputDisplay);
			const char *data = (const char *)out->data + out->pitch * (out->h - 1);

			switch(out->depth) {
			case 16:
				pDisp->SetSource(data, -out->pitch, out->w, out->h, IVDVideoDisplay::kFormatRGB1555);
				break;
			case 24:
				pDisp->SetSource(data, -out->pitch, out->w, out->h, IVDVideoDisplay::kFormatRGB888);
				break;
			case 32:
				pDisp->SetSource(data, -out->pitch, out->w, out->h, IVDVideoDisplay::kFormatRGB8888);
				break;
			}

			pDisp->Update(IVDVideoDisplay::kAllFields);
		} else {
			FillRect(hdc, &mrOutputFrame, (HBRUSH)GetClassLong(mhwnd, GCL_HBRBACKGROUND));
		}
		ReleaseDC(mhwndOutputFrame, hdc);
	}
}

void VDProjectUI::UISetDubbingMode(bool bActive, bool bIsPreview) {
	if (bActive) {
		UpdateVideoFrameLayout();

		mpInputDisplay->LockAcceleration(bIsPreview);
		mpOutputDisplay->LockAcceleration(bIsPreview);

		g_dubber->SetInputDisplay(mpInputDisplay);
		g_dubber->SetOutputDisplay(mpOutputDisplay);

		SetMenu(mhwnd, mhMenuDub);
		mpWndProc = DubWndProc;
	} else {
		SetMenu(mhwnd, mhMenuNormal);
		UpdateMRUList();
		mpWndProc = MainWndProc;

		if (inputAVI) {
			const wchar_t *s = VDFileSplitPath(g_szInputAVIFile);

			SetTitle(kVDM_TitleFileLoaded, 1, &s);
		} else {
			SetTitle(kVDM_TitleIdle, 0);
		}

		// reset video displays
		mpInputDisplay->LockAcceleration(false);
		mpOutputDisplay->LockAcceleration(false);
		DisplayFrame(GetCurrentFrame());
	}
}

void VDProjectUI::UIRunDubMessageLoop() {
	MSG msg;

	VDSamplingAutoProfileScope autoProfileScope;

	while (g_dubber->isRunning() && GetMessage(&msg, (HWND) NULL, 0, 0)) { 
		if (guiCheckDialogs(&msg))
			continue;

		HWND hwndRoot = VDGetAncestorW32(msg.hwnd, GA_ROOT);
		if (hwndRoot == mhwnd && TranslateAccelerator(mhwnd, mhAccelDub, &msg))
			continue;

		TranslateMessage(&msg); 
		DispatchMessage(&msg); 
	}
}

void VDProjectUI::UICurrentPositionUpdated() {
	SendMessage(mhwndPosition, PCM_SETPOS, (WPARAM)TRUE, (LPARAM)GetCurrentFrame());
}

void VDProjectUI::UITimelineUpdated() {
	RemakePositionSlider();
}

void VDProjectUI::UISelectionStartUpdated() {
	SendMessage(mhwndPosition, PCM_SETSELSTART, (WPARAM)TRUE, (LPARAM)GetSelectionStartFrame());

	guiSetStatus("Start offset set to %ld ms", 255, g_dubOpts.video.lStartOffsetMS);
}

void VDProjectUI::UISelectionEndUpdated() {
	SendMessage(mhwndPosition, PCM_SETSELEND, (WPARAM)TRUE, (LPARAM)GetSelectionEndFrame());

	guiSetStatus("End offset set to %ld ms", 255, g_dubOpts.video.lEndOffsetMS);
}

void VDProjectUI::UIShuttleModeUpdated() {
	if (!mSceneShuttleMode)
		SendMessage(mhwndPosition, PCM_RESETSHUTTLE, 0, 0);
}

void VDProjectUI::UISourceFileUpdated() {
	if (inputAVI) {
		mMRUList.add(g_szInputAVIFile);
		UpdateMRUList();
	}

	if (inputAVI) {
		const wchar_t *s = VDFileSplitPath(g_szInputAVIFile);

		SetTitle(kVDM_TitleFileLoaded, 1, &s);
	} else
		SetTitle(kVDM_TitleIdle, 0);
}

void VDProjectUI::UIVideoSourceUpdated() {
	if (inputVideoAVI) {
		UpdateVideoFrameLayout();
		ShowWindow(mhwndInputFrame, SW_SHOW);
		ShowWindow(mhwndOutputFrame, SW_SHOW);
	} else {
		ShowWindow(mhwndInputFrame, SW_HIDE);
		ShowWindow(mhwndOutputFrame, SW_HIDE);
	}
}

void VDProjectUI::UIVideoFiltersUpdated() {
	UpdateVideoFrameLayout();
}

void VDProjectUI::UpdateMRUList() {
	HMENU hmenuFile = GetSubMenu(GetMenu(mhwnd), 0);
	union {
		MENUITEMINFOA a;
		MENUITEMINFOW w;
	} mii;
	char name2[MAX_PATH];
	int index=0;

#define WIN95_MENUITEMINFO_SIZE (offsetof(MENUITEMINFO, cch) + sizeof(UINT))

	memset(&mii, 0, sizeof mii);
	mii.a.cbSize	= WIN95_MENUITEMINFO_SIZE;
	for(;;) {
		mii.a.fMask			= MIIM_TYPE;
		mii.a.dwTypeData		= name2;
		mii.a.cch				= sizeof name2;

		if (!GetMenuItemInfo(hmenuFile, MRU_LIST_POSITION, TRUE, &mii.a)) break;

		if (mii.a.fType & MFT_SEPARATOR) break;

		RemoveMenu(hmenuFile, MRU_LIST_POSITION, MF_BYPOSITION);
	}

	for(;;) {
		VDStringW name(mMRUList[index]);

		if (name.empty())
			break;

		mii.a.fMask		= MIIM_TYPE | MIIM_STATE | MIIM_ID;
		mii.a.fType		= MFT_STRING;
		mii.a.fState	= MFS_ENABLED;
		mii.a.wID		= ID_MRU_FILE0 + index;

		int shortcut = (index+1) % 10;
		const wchar_t *s = name.c_str();

		VDStringW name2(VDswprintf(L"&%d %s", 2, &shortcut, &s));

		if (GetVersion() & 0x80000000) {
			VDStringA name2A(VDTextWToA(name2.c_str()));

			mii.a.dwTypeData	= (char *)name2A.c_str();
			mii.a.cch			= name2A.size() + 1;

			if (!InsertMenuItemA(hmenuFile, MRU_LIST_POSITION+index, TRUE, &mii.a))
				break;
		} else {
			mii.w.dwTypeData	= (wchar_t *)name2.c_str();
			mii.w.cch			= name2.size() + 1;

			if (!InsertMenuItemW(hmenuFile, MRU_LIST_POSITION+index, TRUE, &mii.w))
				break;
		}

		++index;
	}

	if (!index) {
		mii.a.fMask			= MIIM_TYPE | MIIM_STATE | MIIM_ID;
		mii.a.fType			= MFT_STRING;
		mii.a.fState		= MFS_GRAYED;
		mii.a.wID			= ID_MRU_FILE0;
		mii.a.dwTypeData	= "Recent file list";
		mii.a.cch			= sizeof name2;

		InsertMenuItem(hmenuFile, MRU_LIST_POSITION+index, TRUE, &mii.a);
	}

	DrawMenuBar(mhwnd);
}

void VDProjectUI::DisplayRequestUpdate(IVDVideoDisplay *pDisp) {
	if (!g_dubber) {
		if (pDisp == mpOutputDisplay)
			UIRefreshOutputFrame(filters.isRunning());
		else
			UIRefreshInputFrame(inputVideoAVI && inputVideoAVI->isFrameBufferValid());

		pDisp->Cache();
	}
}