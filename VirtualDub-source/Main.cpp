//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#include "VirtualDub.h"

#include <stdio.h>
#include <process.h>
#include <crtdbg.h>

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <vfw.h>
#include <shellapi.h>
#include <shlobj.h>

#include "resource.h"
#include "convert.h"
#include "optdlg.h"
#include "prefs.h"
#include "filtdlg.h"
#include "filters.h"
#include "audio.h"
#include "oshelper.h"
#include "gui.h"
#include "ClippingControl.h"
#include "PositionControl.h"
#include "HexViewer.h"
#include "cpuaccel.h"
#include "capture.h"
#include "auxdlg.h"
#include "dub.h"
#include "DubStatus.h"
#include "mpeg.h"
#include "ddrawsup.h"
#include "server.h"
#include "script.h"
#include "command.h"
#include "job.h"
#include "autodetect.h"
#include "crash.h"

#include "AudioSource.h"
#include "VideoSource.h"
#include "AVIStripeSystem.h"
#include "AVIOutput.h"
#include "AVIOutputStriped.h"
#include "AVIOutputPreview.h"
#include "AVIOutputImages.h"
#include "AVIPipe.h"
#include "AsyncBlitter.h"
#include "InputFile.h"
#include "MRUList.h"
#include "SceneDetector.h"
#include "Error.h"
#include "FrameSubset.h"

///////////////////////////////////////////////////////////////////////////

#define MRU_LIST_POSITION		(22)

///////////////////////////////////////////////////////////////////////////

extern bool g_fJobMode;
extern bool g_fJobAborted;

HINSTANCE	g_hInst;
HWND		g_hWnd =NULL;
HMENU		hMenuNormal, hMenuDub, g_hmenuDisplay;
HACCEL		g_hAccelMain;

HDC					hDCWindow				= NULL;
HDRAWDIB			hDDWindow				= NULL;
HDRAWDIB			hDDWindow2				= NULL;

MRUList				*mru_list				= NULL;

SceneDetector		*g_sceneDetector		= NULL;

int					g_sceneShuttleMode		= 0;
int					g_sceneShuttleAdvance	= 0;
int					g_sceneShuttleCounter	= 0;

bool				g_fDropFrames			= false;
bool				g_fSwapPanes			= false;

IDubStatusHandler	*g_dubStatus			= NULL;

RECT	g_rInputFrame;
RECT	g_rOutputFrame;
int		g_iInputFrameShift = 0;
int		g_iOutputFrameShift = 0;

char g_szInputAVIFile[MAX_PATH];
char g_szInputAVIFileTitle[MAX_PATH];
char g_szInputWAVFile[MAX_PATH];
char g_szFile[MAX_PATH];
char g_msgBuf[128];
char g_szFileDescription[256];

extern const char g_szError[]="VirtualDub Error";
extern const char g_szWarning[]="VirtualDub Warning";
extern const char g_szOutOfMemory[]="Out of memory";
static char szWAVOutput[]="WAVOutput";

///////////////////////////

extern bool Init(HINSTANCE hInstance, LPSTR lpCmdLine, int nCmdShow);
extern void Deinit();
extern bool CoachCheckSaveOp(HWND hwndParent, DubOptions *dopt, COMPVARS *vcomp, WAVEFORMATEX *, List *);
extern void ChooseCompressor(HWND hwndParent, COMPVARS *lpCompVars, BITMAPINFOHEADER *bihInput);
extern void FreeCompressor(COMPVARS *pCompVars);
extern WAVEFORMATEX *AudioChooseCompressor(HWND hwndParent, WAVEFORMATEX *, WAVEFORMATEX *);
extern void DisplayLicense(HWND hwndParent);

LONG APIENTRY MainWndProc( HWND hWnd, UINT message, UINT wParam, LONG lParam);
void SceneShuttleStop();
void SceneShuttleStep();

void SetTitleByFile(HWND hWnd);
void RecalcFrameSizes();

void SetAudioSource();
void OpenAVI(int index, bool extended_opt);
void AppendAVI();
void PreviewAVI(HWND, DubOptions *, int iPriority=0, bool fProp=false);
void SaveAVI(HWND, bool);
void SaveSegmentedAVI(HWND);
void HandleDragDrop(HDROP hdrop);
void SaveStripedAVI(HWND);
void SaveStripeMaster(HWND);
void CPUTest();
void InitDubAVI(char *szFile, BOOL fAudioOnly, DubOptions *quick_options, int iPriority=0, bool fPropagateErrors = false, long lSpillThreshold=0, long lSpillFrameThreshold=0);
void OpenImageSeq(HWND hwnd);
void SaveImageSeq(HWND);
void SaveWAV(HWND);
void OpenWAV();
void DoDelete();
void DoMaskChange(bool bMask);
void SaveConfiguration(HWND);
void CreateExtractSparseAVI(HWND hwndParent, bool bExtract);

BOOL APIENTRY StatusDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam);
BOOL APIENTRY AVIInfoDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam);

//
//  FUNCTION: WinMain(HANDLE, HANDLE, LPSTR, int)
//
//  PURPOSE: Entry point for the application.
//
//  COMMENTS:
//
//	This function initializes the application and processes the
//	message loop.
//

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow )
{
	MSG msg;

	Init(hInstance, lpCmdLine, nCmdShow);

	// Load a file on the command line.

	VDCHECKPOINT;

	if (*g_szFile)
		try {
			char szFileTmp[MAX_PATH];
			char *t = NULL;

			strcpy(g_szInputAVIFile, g_szFile);
			GetFullPathName(g_szInputAVIFile, sizeof szFileTmp, szFileTmp, &t);
			if (t) {
				strcpy(g_szInputAVIFileTitle, t);

				OpenAVI(g_szFile, 0, false, false, NULL);
				SetTitleByFile(g_hWnd);
				RecalcFrameSizes();
				InvalidateRect(g_hWnd, NULL, TRUE);
			} else
				g_szFile[0] = g_szInputAVIFile[0] = 0;
		} catch(const MyError& e) {
			e.post(g_hWnd, g_szError);
		}

    // Acquire and dispatch messages until a WM_QUIT message is received.
	VDCHECKPOINT;

    while (GetMessage(&msg,NULL,0,0)) {

		if (guiCheckDialogs(&msg)) continue;

		if (!g_dubber) {
			HWND hwnd = msg.hwnd, hwndParent;

			while(hwndParent = GetParent(hwnd))
				hwnd = hwndParent;

			if (hwnd == g_hWnd && TranslateAccelerator(g_hWnd, g_hAccelMain, &msg)) continue;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (!g_dubber && inputVideoAVI && g_sceneShuttleMode) {
			if (!g_sceneDetector)
				if (!(g_sceneDetector = new SceneDetector(inputVideoAVI->getImageFormat()->biWidth, inputVideoAVI->getImageFormat()->biHeight)))
					continue;

			g_sceneDetector->SetThresholds(g_prefs.scene.iCutThreshold, g_prefs.scene.iFadeThreshold);

			while(g_sceneShuttleMode) {
				SceneShuttleStep();

				while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
					if (msg.message == WM_QUIT) goto wm_quit_detected;

					guiCheckDialogs(&msg);

					if (TranslateAccelerator(g_hWnd, g_hAccelMain, &msg)) continue;

					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}

			delete g_sceneDetector;
			g_sceneDetector = NULL;
		}
	}
wm_quit_detected:

	Deinit();

	VDCHECKPOINT;

    return (msg.wParam);           // Returns the value from PostQuitMessage.

}


//////////////////////////////////////////////////////////////////////

#define MENU_TO_HELP(x) ID_##x, IDS_##x

UINT iMainMenuHelpTranslator[]={
	MENU_TO_HELP(FILE_OPENAVI),
	MENU_TO_HELP(FILE_PREVIEWAVI),
	MENU_TO_HELP(FILE_SAVEAVI),
	MENU_TO_HELP(FILE_SAVECOMPATIBLEAVI),
	MENU_TO_HELP(FILE_SAVESTRIPEDAVI),
	MENU_TO_HELP(FILE_SAVEIMAGESEQ),
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
	MENU_TO_HELP(OPTIONS_SYNCHRONOUSBLIT),
	MENU_TO_HELP(OPTIONS_VERTICALDISPLAY),
	MENU_TO_HELP(OPTIONS_DRAWHISTOGRAMS),
	MENU_TO_HELP(OPTIONS_SYNCTOAUDIO),
	MENU_TO_HELP(OPTIONS_DROPFRAMES),
	MENU_TO_HELP(OPTIONS_ENABLEDIRECTDRAW),

	MENU_TO_HELP(HELP_CONTENTS),
	MENU_TO_HELP(HELP_CHANGELOG),
	MENU_TO_HELP(HELP_RELEASENOTES),
	MENU_TO_HELP(HELP_ABOUT),
	NULL,NULL,
};

//////////////////////////////////////////////////////////////////////

char PositionFrameTypeCallback(HWND hwnd, void *pvData, long pos) {
	try {
		if (inputVideoAVI)
			if (inputSubset) {
				bool bMasked;
				long nFrame = inputSubset->lookupFrame(pos, bMasked);

				return bMasked ? 'M' : inputVideoAVI->getFrameTypeChar(nFrame);
			} else
				return inputVideoAVI->getFrameTypeChar(pos);
		else
			return 0;
	} catch(const MyError&) {
		return 0;
	}
}

void DisplayFrame(HWND hWnd, LONG pos, bool bDispInput=true) {
	static FilterStateInfo fsi;
	long original_pos = pos;
	static int s_nLastFrame = -1;

	if (!g_dubOpts.video.fShowInputFrame && !g_dubOpts.video.fShowOutputFrame)
		return;


	try {
		BITMAPINFOHEADER *dcf;
		void *lpBits;
		long limit = inputVideoAVI->lSampleLast;

		if (inputSubset) {
			int len = 1;

			pos = inputSubset->lookupRange(pos, len);

			if (pos < 0)
				pos = inputVideoAVI->lSampleLast;
		}

		bool bShowOutput = !g_sceneShuttleMode && !g_dubber && g_dubOpts.video.fShowOutputFrame;

		if (s_nLastFrame != pos || !inputVideoAVI->isFrameBufferValid() || (bShowOutput && !filters.isRunning())) {

			if (bDispInput)
				s_nLastFrame = pos;

			dcf = inputVideoAVI->getDecompressedFormat();

			if (pos >= inputVideoAVI->lSampleLast) {
				FillRect(hDCWindow, &g_rInputFrame, (HBRUSH)GetClassLong(hWnd,GCL_HBRBACKGROUND));
			} else {
				BITMAPINFOHEADER bihOutput;
				VBitmap *out;

				VDCHECKPOINT;
				CHECK_FPU_STACK

				lpBits = inputVideoAVI->getFrame(pos);

				CHECK_FPU_STACK
				VDCHECKPOINT;

				if (!lpBits)
					return;

				if (g_dubOpts.video.fShowInputFrame && bDispInput)
					DrawDibDraw(
							hDDWindow,
							hDCWindow,
							g_rInputFrame.left, g_rInputFrame.top,
							g_rInputFrame.right-g_rInputFrame.left, g_rInputFrame.bottom-g_rInputFrame.top,
							dcf,
							lpBits,
							0, 0, 
							dcf->biWidth, dcf->biHeight,
							0);

				VDCHECKPOINT;

				if (bShowOutput) {
					if (!filters.isRunning()) {
						CPUTest();
						filters.initLinearChain(&g_listFA, (Pixel *)(dcf+1), dcf->biWidth, dcf->biHeight, 32, 16+8*g_dubOpts.video.outputDepth);
						if (filters.ReadyFilters(&fsi))
							throw MyError("can't initialize filters");
					}

					fsi.lCurrentFrame				= original_pos;
					fsi.lMicrosecsPerFrame			= MulDiv(inputVideoAVI->streamInfo.dwScale, 1000000, inputVideoAVI->streamInfo.dwRate);
					fsi.lCurrentSourceFrame			= pos;
					fsi.lMicrosecsPerSrcFrame		= MulDiv(inputVideoAVI->streamInfo.dwScale, 1000000, inputVideoAVI->streamInfo.dwRate);
					fsi.lSourceFrameMS				= MulDiv(fsi.lCurrentSourceFrame, fsi.lMicrosecsPerSrcFrame, 1000);
					fsi.lDestFrameMS				= MulDiv(fsi.lCurrentFrame, fsi.lMicrosecsPerFrame, 1000);

					filters.InputBitmap()->BitBlt(0, 0, &VBitmap(lpBits, dcf), 0, 0, -1, -1);

					filters.RunFilters();

					out = filters.LastBitmap();

					out->MakeBitmapHeader(&bihOutput);

					DrawDibDraw(
							hDDWindow2,
							hDCWindow,
							g_rOutputFrame.left, g_rOutputFrame.top,
							g_rOutputFrame.right-g_rOutputFrame.left, g_rOutputFrame.bottom-g_rOutputFrame.top,
							&bihOutput,
							out->data,
							0, 0, 
							out->w, out->h,
							0);
				}
				VDCHECKPOINT;
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

void SceneShuttleStop() {
	if (g_sceneShuttleMode) {
		HWND hwndPosition = GetDlgItem(g_hWnd, IDC_POSITION);
		LONG lSample = SendMessage(hwndPosition, PCM_GETPOS, 0, 0);

		SendMessage(hwndPosition, PCM_RESETSHUTTLE, 0, 0);
		g_sceneShuttleMode = 0;
		g_sceneShuttleAdvance = 0;
		g_sceneShuttleCounter = 0;

		if (inputVideoAVI)
			DisplayFrame(g_hWnd, lSample);
	}
}

void SceneShuttleStep() {
	if (!inputVideoAVI)
		SceneShuttleStop();

	HWND hwndPosition = GetDlgItem(g_hWnd, IDC_POSITION);
	LONG lSample = SendMessage(hwndPosition, PCM_GETPOS, 0, 0) + g_sceneShuttleMode;
	long ls2 = inputSubset ? inputSubset->lookupFrame(lSample) : lSample;

	if (!inputVideoAVI || ls2 < inputVideoAVI->lSampleFirst || ls2 >= inputVideoAVI->lSampleLast) {
		SceneShuttleStop();
		return;
	}

	if (g_sceneShuttleAdvance < 1280)
		++g_sceneShuttleAdvance;

	g_sceneShuttleCounter += 32;

	if (g_sceneShuttleCounter >= g_sceneShuttleAdvance) {
		g_sceneShuttleCounter = 0;
		DisplayFrame(g_hWnd, lSample, true);
	} else
		DisplayFrame(g_hWnd, lSample, false);

	SendMessage(hwndPosition, PCM_SETPOS, 0, (LPARAM)lSample);

	if (g_sceneDetector->Submit(&VBitmap(inputVideoAVI->getFrameBuffer(), inputVideoAVI->getDecompressedFormat()))) {
		SceneShuttleStop();
	}
}

void MenuMRUListUpdate(HWND hwnd) {
	HMENU hmenuFile = GetSubMenu(GetMenu(hwnd), 0);
	MENUITEMINFO mii;
	char name[MAX_PATH], name2[MAX_PATH];
	int index=0;

#define WIN95_MENUITEMINFO_SIZE (offsetof(MENUITEMINFO, cch) + sizeof(UINT))

	memset(&mii, 0, sizeof mii);
	mii.cbSize	= WIN95_MENUITEMINFO_SIZE;
	for(;;) {
		mii.fMask			= MIIM_TYPE;
		mii.dwTypeData		= name2;
		mii.cch				= sizeof name2;

		if (!GetMenuItemInfo(hmenuFile, MRU_LIST_POSITION, TRUE, &mii)) break;

		if (mii.fType & MFT_SEPARATOR) break;

		RemoveMenu(hmenuFile, MRU_LIST_POSITION, MF_BYPOSITION);
	}

	while(!mru_list->get(index, name, sizeof name)) {
		char *s = name;

		while(*s) ++s;
		while(s>name && s[-1]!='\\' && s[-1]!=':') --s;
		wsprintf(name2, "&%d %s", (index+1)%10, s);

		mii.fMask		= MIIM_TYPE | MIIM_STATE | MIIM_ID;
		mii.fType		= MFT_STRING;
		mii.fState		= MFS_ENABLED;
		mii.wID			= ID_MRU_FILE0 + index;
		mii.dwTypeData	= name2;
		mii.cch				= sizeof name2;

		if (!InsertMenuItem(hmenuFile, MRU_LIST_POSITION+index, TRUE, &mii))
			break;

		++index;
	}

	if (!index) {
		mii.fMask		= MIIM_TYPE | MIIM_STATE | MIIM_ID;
		mii.fType		= MFT_STRING;
		mii.fState		= MFS_GRAYED;
		mii.wID			= ID_MRU_FILE0;
		mii.dwTypeData	= "Recent file list";
		mii.cch			= sizeof name2;

		InsertMenuItem(hmenuFile, MRU_LIST_POSITION+index, TRUE, &mii);
	}
}

void RecalcFrameSizes() {
	RECT &rInputFrame = g_fSwapPanes ? g_rOutputFrame : g_rInputFrame;
	RECT &rOutputFrame = g_fSwapPanes ? g_rInputFrame : g_rOutputFrame;

	memset(&rInputFrame, 0, sizeof(RECT));
	memset(&rOutputFrame, 0, sizeof(RECT));

	if (inputVideoAVI) {
		BITMAPINFOHEADER *formatIn = inputVideoAVI->getImageFormat();

		if (formatIn) {
			BITMAPINFOHEADER *dcf = inputVideoAVI->getDecompressedFormat();
			int w = dcf->biWidth;
			int h = dcf->biHeight;
			int w2, h2;

			if (g_iInputFrameShift<0) {
				w >>= -g_iInputFrameShift;
				h >>= -g_iInputFrameShift;
			} else {
				w <<= g_iInputFrameShift;
				h <<= g_iInputFrameShift;
			}

			g_rInputFrame.left		= 6;
			g_rInputFrame.top		= 6;
			g_rInputFrame.right		= 6 + w;
			g_rInputFrame.bottom	= 6 + h;

			// figure out output size too

			if (!filters.isRunning()) {
				if (g_dubber)
					return;

				filters.prepareLinearChain(&g_listFA, (Pixel *)(dcf+1), dcf->biWidth, dcf->biHeight, 32, 16+8*g_dubOpts.video.outputDepth);
			}

			w2 = filters.OutputBitmap()->w;
			h2 = filters.OutputBitmap()->h;

			if (g_iOutputFrameShift<0) {
				w2 >>= -g_iOutputFrameShift;
				h2 >>= -g_iOutputFrameShift;
			} else {
				w2 <<= g_iOutputFrameShift;
				h2 <<= g_iOutputFrameShift;
			}

			// If frames are reversed, swap the sizes.

			if (g_fSwapPanes) {
				int t;

				t=w; w=w2; w2=t;
				t=h; h=h2; h2=t;
			}

			// Layout frames.

			rInputFrame.left	= 6;
			rInputFrame.top		= 6;
			rInputFrame.right	= 6 + w;
			rInputFrame.bottom	= 6 + h;

			if (g_vertical) {
				rOutputFrame.left	= rInputFrame.left;
				rOutputFrame.top	= rInputFrame.bottom + 12;
				rOutputFrame.right	= rOutputFrame.left + w2;
				rOutputFrame.bottom	= rOutputFrame.top + h2;
			} else {
				rOutputFrame.left	= rInputFrame.right + 12;
				rOutputFrame.top	= rInputFrame.top;
				rOutputFrame.right	= rOutputFrame.left + w2;
				rOutputFrame.bottom	= rOutputFrame.top + h2;
			}
		}
	}
}

void SetTitleByFile(HWND hWnd) {
	if (inputAVI)
		if (g_szFileDescription[0])
			guiSetTitle(hWnd, IDS_TITLE_IDLE2, g_szInputAVIFileTitle, g_szFileDescription);
		else
			guiSetTitle(hWnd, IDS_TITLE_IDLE, g_szInputAVIFileTitle);
	else
		guiSetTitle(hWnd, IDS_TITLE_NOFILE);
}

BOOL MenuHit(HWND hWnd, UINT id) {
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
	DragAcceptFiles(hWnd, FALSE);

	try {
		switch(id) {
		case ID_FILE_QUIT:
			DestroyWindow(hWnd);
			break;
		case ID_FILE_OPENAVI:
			OpenAVI(-1, false);
			RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
			SetTitleByFile(hWnd);
			RecalcFrameSizes();
			break;
		case ID_FILE_APPENDSEGMENT:
			AppendAVI();
			break;
		case ID_FILE_PREVIEWAVI:
			PreviewAVI(hWnd, NULL, g_prefs.main.iPreviewPriority);
			break;
		case ID_FILE_SAVEAVI:
			SaveAVI(hWnd, false);
			JobUnlockDubber();
			break;
		case ID_FILE_SAVECOMPATIBLEAVI:
			SaveAVI(hWnd, true);
			break;
		case ID_FILE_SAVESTRIPEDAVI:
			SaveStripedAVI(hWnd);
			break;
		case ID_FILE_SAVESTRIPEMASTER:
			SaveStripeMaster(hWnd);
			break;
		case ID_FILE_SAVEIMAGESEQ:
			SaveImageSeq(hWnd);
			break;
		case ID_FILE_SAVESEGMENTEDAVI:
			SaveSegmentedAVI(hWnd);
			break;
		case ID_FILE_SAVEWAV:
			SaveWAV(hWnd);
			break;
		case ID_FILE_CLOSEAVI:
			CloseAVI();
			RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
			SetTitleByFile(hWnd);
			break;
		case ID_FILE_STARTSERVER:
			SetAudioSource();
			ActivateFrameServerDialog(hWnd);
			MenuMRUListUpdate(hWnd);
			break;
		case ID_FILE_CAPTUREAVI:
			CPUTest();
			Capture(hWnd);
			MenuMRUListUpdate(hWnd);
			RecalcFrameSizes();		// necessary because filters can be changed in capture mode
			SetTitleByFile(hWnd);
			break;
		case ID_FILE_SAVECONFIGURATION:
			SaveConfiguration(hWnd);
			break;
		case ID_FILE_LOADCONFIGURATION:
		case ID_FILE_RUNSCRIPT:
			RunScript(NULL, (void *)hWnd);
			SetTitleByFile(hWnd);
			RecalcFrameSizes();
			break;
		case ID_FILE_JOBCONTROL:
			OpenJobWindow();
			break;
		case ID_FILE_AVIINFO:
	//		if (inputAVI) ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_AVI_INFO), hWnd, AVIInfoDlgProc);
			if (inputAVI)
				inputAVI->InfoDialog(hWnd);
			break;
		case ID_VIDEO_FILTERS:
			ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_FILTERS), hWnd, FilterDlgProc);
			RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
			RecalcFrameSizes();
			break;
		case ID_VIDEO_FRAMERATE:
			SetAudioSource();
			ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_VIDEO_FRAMERATE), hWnd, VideoDecimationDlgProc);
			RecalcPositionTimeConstant();
			break;
		case ID_VIDEO_COLORDEPTH:
			ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_VIDEO_DEPTH), hWnd, VideoDepthDlgProc);
			break;
		case ID_VIDEO_CLIPPING:
			ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_VIDEO_CLIPPING), hWnd, VideoClippingDlgProc);
			break;
		case ID_VIDEO_COMPRESSION:
			if (!(g_Vcompression.dwFlags & ICMF_COMPVARS_VALID)) {
				memset(&g_Vcompression, 0, sizeof g_Vcompression);
				g_Vcompression.dwFlags |= ICMF_COMPVARS_VALID;
				g_Vcompression.lQ = 10000;
			}

			g_Vcompression.cbSize = sizeof(COMPVARS);

			ChooseCompressor(hWnd, &g_Vcompression, NULL);

			break;
		case ID_VIDEO_MODE_DIRECT:
			g_dubOpts.video.mode = DubVideoOptions::M_NONE;
			break;
		case ID_VIDEO_MODE_FASTRECOMPRESS:
			g_dubOpts.video.mode = DubVideoOptions::M_FASTREPACK;
			break;
		case ID_VIDEO_MODE_NORMALRECOMPRESS:
			g_dubOpts.video.mode = DubVideoOptions::M_SLOWREPACK;
			break;
		case ID_VIDEO_MODE_FULL:
			g_dubOpts.video.mode = DubVideoOptions::M_FULL;
			break;
		case ID_VIDEO_COPYSOURCEFRAME:
			if (!inputVideoAVI || !inputVideoAVI->isFrameBufferValid())
				break;

			if (OpenClipboard(hWnd)) {
				if (EmptyClipboard()) {
					BITMAPINFOHEADER *pbih = inputVideoAVI->getDecompressedFormat();
					long lFormatSize = pbih->biSize + (pbih->biBitCount<=16 ? pbih->biClrUsed*sizeof(RGBQUAD) : 0);
					HANDLE hMem;
					void *lpvMem;

					if (hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, pbih->biSizeImage + lFormatSize)) {
						if (lpvMem = GlobalLock(hMem)) {
							memcpy(lpvMem, pbih, lFormatSize);
							memcpy((char *)lpvMem + lFormatSize, inputVideoAVI->getFrameBuffer(), pbih->biSizeImage);

							GlobalUnlock(lpvMem);
							SetClipboardData(CF_DIB, hMem);
							CloseClipboard();
							break;
						}
						GlobalFree(hMem);
					}
				}
				CloseClipboard();
			}
			break;
		case ID_VIDEO_COPYOUTPUTFRAME:
			if (!filters.isRunning())
				break;
			if (OpenClipboard(hWnd)) {
				if (EmptyClipboard()) {
					BITMAPINFOHEADER bih;
					long lFormatSize;
					HANDLE hMem;
					void *lpvMem;

					filters.LastBitmap()->MakeBitmapHeaderNoPadding(&bih);
					lFormatSize = bih.biSize;

					if (hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, bih.biSizeImage + lFormatSize)) {
						if (lpvMem = GlobalLock(hMem)) {
							memcpy(lpvMem, &bih, lFormatSize);

							VBitmap((char *)lpvMem + lFormatSize, &bih).BitBlt(0, 0, filters.LastBitmap(), 0, 0, -1, -1); 

							GlobalUnlock(lpvMem);
							SetClipboardData(CF_DIB, hMem);
							CloseClipboard();
							break;
						}
						GlobalFree(hMem);
					}
				}
				CloseClipboard();
			}
			break;

		case ID_EDIT_DELETE:
			DoDelete();
			break;

		case ID_EDIT_MASK:
			DoMaskChange(true);
			break;

		case ID_EDIT_UNMASK:
			DoMaskChange(false);
			break;

		case ID_EDIT_SETSELSTART:
			if (inputAVI) {
				LONG lSample = SendDlgItemMessage(hWnd, IDC_POSITION, PCM_GETPOS, 0, 0);

				SendDlgItemMessage(hWnd, IDC_POSITION, PCM_SETSELSTART, (WPARAM)TRUE, lSample);

				guiSetStatus("Start offset set to %ld ms", 255,
						g_dubOpts.video.lStartOffsetMS = inputVideoAVI->samplesToMs(lSample));
			}
			break;
		case ID_EDIT_SETSELEND:
			if (inputAVI) {
				LONG lSample = SendDlgItemMessage(hWnd, IDC_POSITION, PCM_GETPOS, 0, 0);

				SendDlgItemMessage(hWnd, IDC_POSITION, PCM_SETSELEND, (WPARAM)TRUE, lSample);

				guiSetStatus("End offset set to %ld ms", 255,
					g_dubOpts.video.lEndOffsetMS = inputVideoAVI->samplesToMs((inputSubset ? inputSubset->getTotalFrames() : (inputVideoAVI->lSampleLast - inputVideoAVI->lSampleFirst)) - lSample));
			}
			break;

		case ID_AUDIO_CONVERSION:
			ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_AUDIO_CONVERSION), hWnd, AudioConversionDlgProc);
			break;
		case ID_AUDIO_INTERLEAVE:
			ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_INTERLEAVE), hWnd, AudioInterleaveDlgProc);
			break;
		case ID_AUDIO_COMPRESSION:
			SetAudioSource();

			if (!inputAudio)
				g_ACompressionFormat = AudioChooseCompressor(hWnd, g_ACompressionFormat, NULL);
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

				g_ACompressionFormat = AudioChooseCompressor(hWnd, g_ACompressionFormat, (WAVEFORMATEX *)&wfex);

			}

			if (g_ACompressionFormat) {
				g_ACompressionFormatSize = sizeof(WAVEFORMATEX) + g_ACompressionFormat->cbSize;
			}
			break;

		case ID_AUDIO_VOLUME:
			ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_AUDIO_VOLUME), hWnd, AudioVolumeDlgProc);
			break;

		case ID_AUDIO_SOURCE_NONE:
			audioInputMode = AUDIOIN_NONE;
			CloseWAV();
			SetAudioSource();
			RecalcPositionTimeConstant();
			break;
		case ID_AUDIO_SOURCE_AVI:
			audioInputMode = AUDIOIN_AVI;
			CloseWAV();
			SetAudioSource();
			RecalcPositionTimeConstant();
			break;
		case ID_AUDIO_SOURCE_WAV:
			OpenWAV();
			SetAudioSource();
			RecalcPositionTimeConstant();
			break;

		case ID_AUDIO_MODE_DIRECT:
			g_dubOpts.audio.mode = DubAudioOptions::M_NONE;
			break;
		case ID_AUDIO_MODE_FULL:
			g_dubOpts.audio.mode = DubAudioOptions::M_FULL;
			break;

		case ID_OPTIONS_PERFORMANCE:
			ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_PERFORMANCE), hWnd, PerformanceOptionsDlgProc);
			break;
		case ID_OPTIONS_DYNAMICCOMPILATION:
			ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_PERF_DYNAMIC), hWnd, DynamicCompileOptionsDlgProc);
			break;
		case ID_OPTIONS_PREFERENCES:
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_PREFERENCES), hWnd, PreferencesDlgProc);
			break;
		case ID_OPTIONS_DISPLAYINPUTVIDEO:
			if (g_dubStatus)
				g_dubStatus->ToggleFrame(false);
			else
				g_dubOpts.video.fShowInputFrame = !g_dubOpts.video.fShowInputFrame;
			break;
		case ID_OPTIONS_DISPLAYOUTPUTVIDEO:
			if (g_dubStatus)
				g_dubStatus->ToggleFrame(true);
			else
				g_dubOpts.video.fShowOutputFrame = !g_dubOpts.video.fShowOutputFrame;
			break;
		case ID_OPTIONS_DISPLAYDECOMPRESSEDOUTPUT:
			g_drawDecompressedFrame = !g_drawDecompressedFrame;
			break;
		case ID_OPTIONS_SHOWSTATUSWINDOW:
			if (g_dubStatus)
				g_dubStatus->ToggleStatus();
			else
				g_showStatusWindow = !g_showStatusWindow;
			break;
		case ID_OPTIONS_SYNCHRONOUSBLIT:
			g_syncroBlit = !g_syncroBlit;
			break;
		case ID_OPTIONS_VERTICALDISPLAY:
			g_vertical = !g_vertical;
			RecalcFrameSizes();
			InvalidateRect(hWnd, NULL, TRUE);
			break;
		case ID_OPTIONS_DRAWHISTOGRAMS:
			g_dubOpts.video.fHistogram = !g_dubOpts.video.fHistogram;
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
			RecalcFrameSizes();
			InvalidateRect(hWnd, NULL, TRUE);
			break;

		case ID_OPTIONS_PREVIEWPROGRESSIVE:	g_dubOpts.video.nPreviewFieldMode = 0; break;
		case ID_OPTIONS_PREVIEWFIELDA:		g_dubOpts.video.nPreviewFieldMode = 1; break;
		case ID_OPTIONS_PREVIEWFIELDB:		g_dubOpts.video.nPreviewFieldMode = 2; break;


		case ID_TOOLS_HEXVIEWER:
			HexEdit(NULL);
			break;

		case ID_TOOLS_CREATESPARSEAVI:
			CreateExtractSparseAVI(hWnd, false);
			break;

		case ID_TOOLS_EXPANDSPARSEAVI:
			CreateExtractSparseAVI(hWnd, true);
			break;

		case ID_HELP_LICENSE:
			DisplayLicense(hWnd);
			break;

		case ID_HELP_CONTENTS:
			HelpShowHelp(hWnd);
			break;
		case ID_HELP_CHANGELOG:
			DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_SHOWTEXT), hWnd, ShowTextDlgProc, (LPARAM)MAKEINTRESOURCE(IDR_CHANGES));
			break;
		case ID_HELP_RELEASENOTES:
			DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_SHOWTEXT), hWnd, ShowTextDlgProc, (LPARAM)MAKEINTRESOURCE(IDR_RELEASE_NOTES));
			break;
		case ID_HELP_ABOUT:
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), hWnd, AboutDlgProc);
			break;

		case ID_HELP_ONLINE_HOME:	LaunchURL("http://www.virtualdub.org/index"); break;
		case ID_HELP_ONLINE_FAQ:	LaunchURL("http://www.virtualdub.org/virtualdub_faq"); break;
		case ID_HELP_ONLINE_NEWS:	LaunchURL("http://www.virtualdub.org/virtualdub_news"); break;
		case ID_HELP_ONLINE_KB:		LaunchURL("http://www.virtualdub.org/virtualdub_kb"); break;

		case ID_DUBINPROGRESS_ABORT:
			if (g_dubber) g_dubber->Abort();
			break;


		case ID_VIDEO_SEEK_START:
			PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_POSITION, PCN_START), (LPARAM)GetDlgItem(hWnd, IDC_POSITION));
			break;
		case ID_VIDEO_SEEK_END:
			PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_POSITION, PCN_END), (LPARAM)GetDlgItem(hWnd, IDC_POSITION));
			break;
		case ID_VIDEO_SEEK_PREV:
			PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_POSITION, PCN_BACKWARD), (LPARAM)GetDlgItem(hWnd, IDC_POSITION));
			break;
		case ID_VIDEO_SEEK_NEXT:
			PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_POSITION, PCN_FORWARD), (LPARAM)GetDlgItem(hWnd, IDC_POSITION));
			break;
		case ID_VIDEO_SEEK_PREVONESEC:
			if (inputVideoAVI) {
				LONG lSample = SendDlgItemMessage(hWnd, IDC_POSITION, PCM_GETPOS, 0, 0) - 50;

				if (lSample < 0)
					lSample = 0;

				SendDlgItemMessage(hWnd, IDC_POSITION, PCM_SETPOS, (WPARAM)TRUE, lSample);
				DisplayFrame(hWnd, lSample);
			}
			break;
		case ID_VIDEO_SEEK_NEXTONESEC:
			if (inputVideoAVI) {
				LONG lSample = SendDlgItemMessage(hWnd, IDC_POSITION, PCM_GETPOS, 0, 0) + 50;
				LONG lMax = (inputSubset ? inputSubset->getTotalFrames() : inputVideoAVI->lSampleLast);

				if (lSample > lMax)
					lSample = lMax;

				SendDlgItemMessage(hWnd, IDC_POSITION, PCM_SETPOS, (WPARAM)TRUE, lSample);
				DisplayFrame(hWnd, lSample);
			}
			break;
		case ID_VIDEO_SEEK_KEYPREV:
			PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_POSITION, PCN_KEYPREV), (LPARAM)GetDlgItem(hWnd, IDC_POSITION));
			break;
		case ID_VIDEO_SEEK_KEYNEXT:
			PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDC_POSITION, PCN_KEYNEXT), (LPARAM)GetDlgItem(hWnd, IDC_POSITION));
			break;
		case ID_VIDEO_SEEK_SELSTART:
			if (inputVideoAVI) {
				LONG lSample = SendDlgItemMessage(hWnd, IDC_POSITION, PCM_GETSELSTART, 0, 0);

				if (lSample >= 0) {
					SendDlgItemMessage(hWnd, IDC_POSITION, PCM_SETPOS, (WPARAM)TRUE, lSample);
					DisplayFrame(hWnd, lSample);
				}
			}
			break;
		case ID_VIDEO_SEEK_SELEND:
			if (inputVideoAVI) {
				LONG lSample = SendDlgItemMessage(hWnd, IDC_POSITION, PCM_GETSELEND, 0, 0);

				if (lSample >= 0) {
					SendDlgItemMessage(hWnd, IDC_POSITION, PCM_SETPOS, (WPARAM)TRUE, lSample);
					DisplayFrame(hWnd, lSample);
				}
			}
			break;
		case ID_VIDEO_SEEK_PREVDROP:
			if (inputAVI) {
				LONG lSample = SendDlgItemMessage(hWnd, IDC_POSITION, PCM_GETPOS, 0, 0);

				while(--lSample >= (inputSubset ? 0 : inputVideoAVI->lSampleFirst)) {
					int err;
					long lBytes, lSamples;

					err = inputVideoAVI->read(inputSubset ? inputSubset->lookupFrame(lSample) : lSample, 1, NULL, 0, &lBytes, &lSamples);
					if (err != AVIERR_OK)
						break;

					if (!lBytes) {
						SendDlgItemMessage(hWnd, IDC_POSITION, PCM_SETPOS, (WPARAM)TRUE, lSample);
						DisplayFrame(hWnd, lSample);
						break;
					}
				}

				if (lSample < (inputSubset ? 0 : inputVideoAVI->lSampleFirst))
					guiSetStatus("No previous dropped frame found.", 255);
			}
			break;

		case ID_VIDEO_SEEK_NEXTDROP:
			if (inputAVI) {
				LONG lSample = SendDlgItemMessage(hWnd, IDC_POSITION, PCM_GETPOS, 0, 0);

				while(++lSample < (inputSubset ? inputSubset->getTotalFrames() : inputVideoAVI->lSampleLast)) {
					int err;
					long lBytes, lSamples;

					err = inputVideoAVI->read(inputSubset ? inputSubset->lookupFrame(lSample) : lSample, 1, NULL, 0, &lBytes, &lSamples);
					if (err != AVIERR_OK)
						break;

					if (!lBytes) {
						SendDlgItemMessage(hWnd, IDC_POSITION, PCM_SETPOS, (WPARAM)TRUE, lSample);
						DisplayFrame(hWnd, lSample);
						break;
					}
				}

				if (lSample >= (inputSubset ? inputSubset->getTotalFrames() : inputVideoAVI->lSampleLast))
					guiSetStatus("No next dropped frame found.", 255);
			}
			break;

		case ID_VIDEO_SCANFORERRORS:
			if (inputVideoAVI) {
				EnsureSubset();
				ScanForUnreadableFrames(inputSubset, inputVideoAVI);
			}
			break;

		case ID_EDIT_JUMPTO:
			if (inputAVI) {
				long lFrame;

				lFrame = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_JUMPTOFRAME), g_hWnd, VideoJumpDlgProc, SendDlgItemMessage(g_hWnd, IDC_POSITION, PCM_GETPOS, 0, 0));

				if (lFrame >= 0) {
					SendDlgItemMessage(g_hWnd, IDC_POSITION, PCM_SETPOS, TRUE, lFrame);
					DisplayFrame(g_hWnd, SendDlgItemMessage(g_hWnd, IDC_POSITION, PCM_GETPOS, 0, 0));
				}
			}
			break;

		case ID_EDIT_RESET:
			if (inputAVI && inputSubset) {
				if (IDOK == MessageBox(g_hWnd, "Discard edits and reset timeline?", g_szWarning, MB_OK|MB_TASKMODAL|MB_SETFOREGROUND)) {
					delete inputSubset;
					inputSubset = NULL;
					RemakePositionSlider();
				}
			}
			break;

		case ID_EDIT_PREVRANGE:
			if (inputAVI && inputSubset) {
				long lSample = SendDlgItemMessage(hWnd, IDC_POSITION, PCM_GETPOS, 0, 0);
				int offset;

				FrameSubsetNode *pfsn = inputSubset->findNode(offset, lSample);

				if (pfsn) {
					FrameSubsetNode *pfsn_prev = pfsn->NextFromTail();

					if (pfsn_prev->NextFromTail()) {
						lSample -= offset;
						SendDlgItemMessage(g_hWnd, IDC_POSITION, PCM_SETPOS, TRUE, lSample - pfsn_prev->len);
						guiSetStatus("Previous output range %d-%d: %sed source frames %d-%d", 255, lSample - pfsn_prev->len, lSample-1, pfsn_prev->bMask ? "mask" : "includ", pfsn_prev->start, pfsn_prev->start + pfsn_prev->len - 1);
						break;
					}
				}
			}
			guiSetStatus("No previous edit range.", 255);
			break;

		case ID_EDIT_NEXTRANGE:
			if (inputAVI && inputSubset) {
				long lSample = SendDlgItemMessage(hWnd, IDC_POSITION, PCM_GETPOS, 0, 0);
				int offset;

				FrameSubsetNode *pfsn = inputSubset->findNode(offset, lSample);

				if (pfsn) {
					FrameSubsetNode *pfsn_next = pfsn->NextFromHead();

					if (pfsn_next->NextFromHead()) {
						lSample = lSample - offset + pfsn->len;
						SendDlgItemMessage(g_hWnd, IDC_POSITION, PCM_SETPOS, TRUE, lSample);
						guiSetStatus("Next output range %d-%d: %sed source frames %d-%d", 255, lSample, lSample+pfsn_next->len-1, pfsn_next->bMask ? "mask" : "includ", pfsn_next->start, pfsn_next->start + pfsn_next->len - 1);
						break;
					}
				}
			}
			guiSetStatus("No next edit range.", 255);
			break;

		default:
			if (id >= ID_MRU_FILE0 && id <= ID_MRU_FILE3) {
				OpenAVI(id - ID_MRU_FILE0, (signed short)GetAsyncKeyState(VK_SHIFT) < 0);
				RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
				RecalcFrameSizes();
				SetTitleByFile(hWnd);
				break;
			}
			break;
		}
	} catch(const MyError& e) {
		e.post(g_hWnd, g_szError);
	}

	JobUnlockDubber();
	DragAcceptFiles(hWnd, TRUE);

	return TRUE;
}

void RepaintMainWindow(HWND hWnd) {
	PAINTSTRUCT ps;
	HDC hDC;
	BITMAPINFOHEADER *formatIn;

	hDC = BeginPaint(hWnd, &ps);
	if (inputVideoAVI) {
		formatIn = inputVideoAVI->getImageFormat();
		if (formatIn) {
			BITMAPINFOHEADER *dcf = inputVideoAVI->getDecompressedFormat();

//			Draw3DRect(hDC, 2, 2, 8+formatIn->biWidth, 8+formatIn->biHeight, FALSE);
//			Draw3DRect(hDC, 5, 5, 2+formatIn->biWidth, 2+formatIn->biHeight, TRUE);

			Draw3DRect(hDC,
					g_rInputFrame.left-4,
					g_rInputFrame.top-4,
					(g_rInputFrame.right-g_rInputFrame.left) + 8,
					(g_rInputFrame.bottom-g_rInputFrame.top) + 8,
					FALSE);

			Draw3DRect(hDC,
					g_rInputFrame.left-1,
					g_rInputFrame.top-1,
					(g_rInputFrame.right-g_rInputFrame.left) + 2,
					(g_rInputFrame.bottom-g_rInputFrame.top) + 2,
					TRUE);

			// Skip the DrawDibDraw()s and filter processing if a dub is running.  Why?
			// The windows will likely get redrawn in 1/30s.  More importantly, we may be
			// using a format that GDI can't handle (like YUV), and we don't want
			// DrawDibDraw() loading all the codecs trying to find a codec to draw it.

			if (!g_dubber) {
				if (inputVideoAVI->isFrameBufferValid())
					DrawDibDraw(
							hDDWindow,
							hDC,
							g_rInputFrame.left, g_rInputFrame.top,
							(g_rInputFrame.right-g_rInputFrame.left), (g_rInputFrame.bottom-g_rInputFrame.top),
							dcf,
							inputVideoAVI->getFrameBuffer(),
							0, 0, 
							dcf->biWidth, dcf->biHeight,
							0);

				if (filters.isRunning()) {
					VBitmap *out;
					BITMAPINFOHEADER bihOutput;

					out = filters.LastBitmap();

					out->MakeBitmapHeader(&bihOutput);

					DrawDibDraw(
							hDDWindow2,
							hDCWindow,
							g_rOutputFrame.left, g_rOutputFrame.top,
							g_rOutputFrame.right-g_rOutputFrame.left, g_rOutputFrame.bottom-g_rOutputFrame.top,
							&bihOutput,
							out->data,
							0, 0, 
							out->w, out->h,
							0);
				}
			}

//			Draw3DRect(hDC, 2, 12+formatIn->biHeight, 258, 258,TRUE);
//		if (outputAVI && outputAVI->videoOut && (formatOut = outputAVI->videoOut->getImageFormat())) {
			Draw3DRect(hDC,
					g_rOutputFrame.left-4,
					g_rOutputFrame.top-4,
					(g_rOutputFrame.right-g_rOutputFrame.left) + 8,
					(g_rOutputFrame.bottom-g_rOutputFrame.top) + 8,
					FALSE);

			Draw3DRect(hDC,
					g_rOutputFrame.left-1,
					g_rOutputFrame.top-1,
					(g_rOutputFrame.right-g_rOutputFrame.left) + 2,
					(g_rOutputFrame.bottom-g_rOutputFrame.top) + 2,
					TRUE);
		}
	}
	EndPaint(hWnd, &ps);
}

void MainMenuHelp(HWND hwnd, WPARAM wParam) {
	if (LOWORD(wParam) >= ID_MRU_FILE0 && LOWORD(wParam) <= ID_MRU_FILE3) {
		HWND hwndStatus = GetDlgItem(hwnd, IDC_STATUS_WINDOW);
		char name[1024];

		if ((HIWORD(wParam) & MF_POPUP) || (HIWORD(wParam) & MF_SYSMENU)) {
			SendMessage(hwndStatus, SB_SETTEXT, 0, (LPARAM)"");
			return;
		}

		strcpy(name, "[SHIFT for options] Load file ");

		if (!mru_list->get(LOWORD(wParam) - ID_MRU_FILE0, name+30, sizeof name - 30)) {
			SendMessage(hwndStatus, SB_SETTEXT, 255, (LPARAM)name);
		} else
			SendMessage(hwndStatus, SB_SETTEXT, 255, (LPARAM)"");
	} else
		guiMenuHelp(hwnd, wParam, 255, iMainMenuHelpTranslator);
}

bool DoFrameRightClick(HWND hwnd, LPARAM lParam) {
	POINT pt = { LOWORD(lParam), HIWORD(lParam) };
	bool isInput, isOutput;

	isInput		= !!PtInRect(&g_rInputFrame, pt);
	isOutput	= !!PtInRect(&g_rOutputFrame, pt);

	if (isInput || isOutput) {
		UINT res;

		ClientToScreen(hwnd, &pt);

		res = TrackPopupMenu(GetSubMenu(g_hmenuDisplay, 0), TPM_LEFTALIGN|TPM_TOPALIGN|TPM_LEFTBUTTON|TPM_NONOTIFY|TPM_RETURNCMD,
			pt.x, pt.y, 0, hwnd, NULL);

		if (res >= ID_DISPLAY_QUARTER && res <= ID_DISPLAY_QUADRUPLE) {
			RECT r;

			if (isInput)
				g_iInputFrameShift = res - ID_DISPLAY_NORMAL;

			if (isOutput)
				g_iOutputFrameShift = res - ID_DISPLAY_NORMAL;

			RecalcFrameSizes();

			GetWindowRect(GetDlgItem(hwnd, IDC_POSITION), &r);

			ScreenToClient(hwnd, (LPPOINT)&r + 0);
			ScreenToClient(hwnd, (LPPOINT)&r + 1);

			r.bottom = r.top;
			r.right -= r.left;
			r.left = r.top = 0;

			InvalidateRect(hwnd, &r, TRUE);
			return true;
		}
	}
	return false;
}

LONG APIENTRY MainWndProc( HWND hWnd, UINT message, UINT wParam, LONG lParam)
{
	static HWND hwndItem0 = NULL;
	extern const unsigned char fht_tab[];

    switch (message) {
	case WM_CREATE:
		if (!(hDCWindow = GetDC(hWnd))) return -1;
		if (!(hDDWindow = DrawDibOpen())) return -1;
		if (!(hDDWindow2 = DrawDibOpen())) return -1;

		SetStretchBltMode(hDCWindow, STRETCH_DELETESCANS);

		{
			HWND hwndItem;
			static const int widths[]={-1};

			if (!(hwndItem = CreateStatusWindow(WS_CHILD|WS_VISIBLE, "", hWnd, IDC_STATUS_WINDOW)))
				return -1;

			SendMessage(hwndItem, SB_SIMPLE, TRUE, 0);

			if (!(hwndItem = CreateWindowEx(0, POSITIONCONTROLCLASS, "", WS_CHILD | WS_VISIBLE | PCS_PLAYBACK | PCS_MARK | PCS_SCENE, 0, 100, 200, 64, hWnd, (HMENU)IDC_POSITION, g_hInst, NULL)))
				return -1;

			SendMessage(hwndItem, PCM_SETFRAMETYPECB, (WPARAM)&PositionFrameTypeCallback, 0);
		}

		MenuMRUListUpdate(hWnd);		

//		SetTimer(hWnd, 1, 5000, (TIMERPROC)NULL);

		return 0;
 
	case WM_INITMENU:
		{
			HMENU hMenu = (HMENU)wParam;

			CheckMenuRadioItem(hMenu, ID_AUDIO_SOURCE_NONE, ID_AUDIO_SOURCE_WAV, ID_AUDIO_SOURCE_NONE+audioInputMode, MF_BYCOMMAND);
			CheckMenuRadioItem(hMenu, ID_VIDEO_MODE_DIRECT, ID_VIDEO_MODE_FULL, ID_VIDEO_MODE_DIRECT+g_dubOpts.video.mode, MF_BYCOMMAND);
			CheckMenuRadioItem(hMenu, ID_AUDIO_MODE_DIRECT, ID_AUDIO_MODE_FULL, ID_AUDIO_MODE_DIRECT+g_dubOpts.audio.mode, MF_BYCOMMAND);
			CheckMenuRadioItem(hMenu, ID_OPTIONS_PREVIEWPROGRESSIVE, ID_OPTIONS_PREVIEWFIELDB,
				ID_OPTIONS_PREVIEWPROGRESSIVE+g_dubOpts.video.nPreviewFieldMode, MF_BYCOMMAND);

			CheckMenuItem(hMenu, ID_OPTIONS_DISPLAYINPUTVIDEO	, MF_BYCOMMAND | (g_dubOpts.video.fShowInputFrame	? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenu, ID_OPTIONS_DISPLAYOUTPUTVIDEO	, MF_BYCOMMAND | (g_dubOpts.video.fShowOutputFrame	? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenu, ID_OPTIONS_DISPLAYDECOMPRESSEDOUTPUT, MF_BYCOMMAND | (g_drawDecompressedFrame ? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenu, ID_OPTIONS_SHOWSTATUSWINDOW	, MF_BYCOMMAND | (g_showStatusWindow	? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenu, ID_OPTIONS_SYNCHRONOUSBLIT		, MF_BYCOMMAND | (g_syncroBlit			? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenu, ID_OPTIONS_VERTICALDISPLAY		, MF_BYCOMMAND | (g_vertical			? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenu, ID_OPTIONS_DRAWHISTOGRAMS		, MF_BYCOMMAND | (g_dubOpts.video.fHistogram		? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenu, ID_OPTIONS_SYNCTOAUDIO			, MF_BYCOMMAND | (g_dubOpts.video.fSyncToAudio		? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenu, ID_OPTIONS_ENABLEDIRECTDRAW	, MF_BYCOMMAND | (g_dubOpts.perf.useDirectDraw		? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenu, ID_OPTIONS_DROPFRAMES			, MF_BYCOMMAND | (g_fDropFrames		? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenu, ID_OPTIONS_SWAPPANES			, MF_BYCOMMAND | (g_fSwapPanes		? MF_CHECKED : MF_UNCHECKED));

			DWORD dwEnableFlags = (inputAVI && inputAVI->Append(NULL) ? (MF_BYCOMMAND|MF_ENABLED) : (MF_BYCOMMAND|MF_GRAYED));

			EnableMenuItem(hMenu,ID_FILE_APPENDSEGMENT			, dwEnableFlags);

			dwEnableFlags = (inputAVI ? (MF_BYCOMMAND|MF_ENABLED) : (MF_BYCOMMAND|MF_GRAYED));

			EnableMenuItem(hMenu,ID_FILE_PREVIEWAVI				, dwEnableFlags);
			EnableMenuItem(hMenu,ID_FILE_SAVEAVI				, dwEnableFlags);
			EnableMenuItem(hMenu,ID_FILE_SAVECOMPATIBLEAVI		, dwEnableFlags);
			EnableMenuItem(hMenu,ID_FILE_SAVESTRIPEDAVI			, dwEnableFlags);
			EnableMenuItem(hMenu,ID_FILE_SAVESTRIPEMASTER		, dwEnableFlags);
			EnableMenuItem(hMenu,ID_FILE_SAVEIMAGESEQ			, dwEnableFlags);
			EnableMenuItem(hMenu,ID_FILE_SAVESEGMENTEDAVI		, dwEnableFlags);
			EnableMenuItem(hMenu,ID_FILE_SAVEWAV				, dwEnableFlags);
			EnableMenuItem(hMenu,ID_FILE_CLOSEAVI				, dwEnableFlags);
			EnableMenuItem(hMenu,ID_FILE_STARTSERVER			, dwEnableFlags);
			EnableMenuItem(hMenu,ID_FILE_AVIINFO				, dwEnableFlags);

			dwEnableFlags = ((g_dubOpts.audio.mode == DubAudioOptions::M_FULL) ? (MF_BYCOMMAND|MF_ENABLED) : (MF_BYCOMMAND|MF_GRAYED));

			EnableMenuItem(hMenu,ID_AUDIO_COMPRESSION			, dwEnableFlags);
			EnableMenuItem(hMenu,ID_AUDIO_CONVERSION			, dwEnableFlags);
			EnableMenuItem(hMenu,ID_AUDIO_VOLUME				, dwEnableFlags);

			dwEnableFlags = ((g_dubOpts.video.mode >= DubVideoOptions::M_FULL) ? (MF_BYCOMMAND|MF_ENABLED) : (MF_BYCOMMAND|MF_GRAYED));
			EnableMenuItem(hMenu,ID_VIDEO_FILTERS				, dwEnableFlags);
			dwEnableFlags = ((g_dubOpts.video.mode >= DubVideoOptions::M_SLOWREPACK) ? (MF_BYCOMMAND|MF_ENABLED) : (MF_BYCOMMAND|MF_GRAYED));
			EnableMenuItem(hMenu,ID_VIDEO_COLORDEPTH			, dwEnableFlags);
			dwEnableFlags = ((g_dubOpts.video.mode >= DubVideoOptions::M_FASTREPACK) ? (MF_BYCOMMAND|MF_ENABLED) : (MF_BYCOMMAND|MF_GRAYED));
			EnableMenuItem(hMenu,ID_VIDEO_COMPRESSION			, dwEnableFlags);
		}
		break;

	case WM_COMMAND:           // message: command from application menu
		if (lParam) {
			switch(LOWORD(wParam)) {
			case IDC_POSITION:
				if (inputVideoAVI) switch(HIWORD(wParam)) {
				case PCN_PLAY:
				case PCN_PLAYPREVIEW:
					SetAudioSource();

					try {
						LONG lStart = SendMessage((HWND)lParam, PCM_GETPOS, 0, 0);
						DubOptions *dubOpt = new DubOptions(g_dubOpts);
						LONG preload = inputAudio && inputAudio->getWaveFormat()->wFormatTag != WAVE_FORMAT_PCM ? 1000 : 500;

						if (!dubOpt) throw MyMemoryError();

						if (dubOpt->audio.preload > preload)
							dubOpt->audio.preload = preload;

						dubOpt->audio.enabled				= TRUE;
						dubOpt->audio.interval				= 1;
						dubOpt->audio.is_ms					= FALSE;
						dubOpt->video.lStartOffsetMS		= inputVideoAVI->samplesToMs(lStart);

						if (HIWORD(wParam) != PCN_PLAYPREVIEW) {
							dubOpt->audio.fStartAudio			= TRUE;
							dubOpt->audio.new_rate				= 0;
							dubOpt->audio.newPrecision			= DubAudioOptions::P_NOCHANGE;
							dubOpt->audio.newChannels			= DubAudioOptions::C_NOCHANGE;

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
										dubOpt->video.inputDepth = DubVideoOptions::D_24BIT;
										break;
									default:
										dubOpt->video.inputDepth = DubVideoOptions::D_16BIT;
										break;
									}
								}
								break;
							case PreferencesMain::DEPTH_FASTEST:
							case PreferencesMain::DEPTH_16BIT:
								dubOpt->video.inputDepth = DubVideoOptions::D_16BIT;
								break;
							case PreferencesMain::DEPTH_24BIT:
								dubOpt->video.inputDepth = DubVideoOptions::D_24BIT;
								break;

							// Ignore: PreferencesMain::DEPTH_OUTPUT

							};
							dubOpt->video.outputDepth			= dubOpt->video.inputDepth;

							dubOpt->video.mode					= DubVideoOptions::M_SLOWREPACK;
							dubOpt->video.fShowInputFrame		= TRUE;
							dubOpt->video.fShowOutputFrame		= FALSE;
							dubOpt->video.frameRateDecimation	= 1;
							dubOpt->video.lEndOffsetMS			= 0;

							dubOpt->audio.mode					= DubAudioOptions::M_FULL;
						}

						dubOpt->fShowStatus = false;
						dubOpt->fMoveSlider = true;

						if (lStart < inputVideoAVI->lSampleLast) {
							PreviewAVI(hWnd, dubOpt, g_prefs.main.iPreviewPriority);
							MenuMRUListUpdate(hWnd);
						}

						delete dubOpt;
					} catch(const MyError& e) {
						e.post(hWnd, g_szError);
					}
					break;
				case PCN_MARKIN:
					SendMessage(hWnd, WM_COMMAND, ID_EDIT_SETSELSTART, 0);
					break;
				case PCN_MARKOUT:
					SendMessage(hWnd, WM_COMMAND, ID_EDIT_SETSELEND, 0);
					break;
				case PCN_START:
					SendMessage((HWND)lParam, PCM_SETPOS, (WPARAM)TRUE, inputVideoAVI->lSampleFirst);
					DisplayFrame(hWnd, inputVideoAVI->lSampleFirst);
					break;
				case PCN_BACKWARD:
					{
						LONG lSample = SendMessage((HWND)lParam, PCM_GETPOS, 0, 0);

						if (lSample > inputVideoAVI->lSampleFirst) {
							SendMessage((HWND)lParam, PCM_SETPOS, (WPARAM)TRUE, lSample-1);
							DisplayFrame(hWnd, lSample-1);
						}
					}
					break;
				case PCN_FORWARD:
					{
						LONG lSample = SendMessage((HWND)lParam, PCM_GETPOS, 0, 0);

						if (lSample < inputVideoAVI->lSampleLast) {
							SendMessage((HWND)lParam, PCM_SETPOS, (WPARAM)TRUE, lSample+1);
							DisplayFrame(hWnd, lSample+1);
						}
					}
					break;
				case PCN_END:
					SendMessage((HWND)lParam, PCM_SETPOS, (WPARAM)TRUE, inputVideoAVI->lSampleLast);
					DisplayFrame(hWnd, inputVideoAVI->lSampleLast);
					break;

				case PCN_KEYPREV:
					{
						LONG lSample = SendMessage((HWND)lParam, PCM_GETPOS, 0, 0);

						if (inputSubset) {
							long lSample2;
							bool bMasked;

							lSample2 = inputSubset->lookupFrame(lSample);

							do {
								lSample2 = inputVideoAVI->prevKey(lSample2);
								lSample = inputSubset->revLookupFrame(lSample2, bMasked);
							} while(lSample2 >= 0 && (lSample < 0 || bMasked));
						} else
							lSample = inputVideoAVI->prevKey(lSample);

						if (lSample < 0) lSample = inputVideoAVI->lSampleFirst;

						SendMessage((HWND)lParam, PCM_SETPOS, (WPARAM)TRUE, lSample);
						DisplayFrame(hWnd, lSample);
					}
					break;
				case PCN_KEYNEXT:
					{
						long lSampleOld = SendMessage((HWND)lParam, PCM_GETPOS, 0, 0);
						long lSample = lSampleOld;

						if (inputSubset) {
							long lSample2;
							bool bMasked;

							lSample2 = inputSubset->lookupFrame(lSample);

							do {
								lSample2 = inputVideoAVI->nextKey(lSample2);
								lSample = inputSubset->revLookupFrame(lSample2, bMasked);
							} while(lSample2 >= 0 && (lSample <= lSampleOld || bMasked));
						} else
							lSample = inputVideoAVI->nextKey(lSample);

						if (lSample < 0) lSample = inputVideoAVI->lSampleLast;

						SendMessage((HWND)lParam, PCM_SETPOS, (WPARAM)TRUE, lSample);
						DisplayFrame(hWnd, lSample);
					}
					break;

				case PCN_SCENEREV:
					g_sceneShuttleMode = -1;
					break;

				case PCN_SCENEFWD:
					g_sceneShuttleMode = +1;
					break;

				case PCN_STOP:
				case PCN_SCENESTOP:
//					g_sceneShuttleMode = 0;
					SceneShuttleStop();
					break;

				}
				break;
			}
		} else if (!MenuHit(hWnd, LOWORD(wParam)))
			return (DefWindowProc(hWnd, message, wParam, lParam));
		break;

	case WM_SIZE:
		guiRedoWindows(hWnd);
		break;

	case WM_DESTROY:                  // message: window being destroyed
		if (hDDWindow) DrawDibClose(hDDWindow);
		if (hDDWindow2) DrawDibClose(hDDWindow2);
		if (hDCWindow) ReleaseDC(hWnd, hDCWindow);
		PostQuitMessage(0);
		break;

	case WM_PAINT:
		RepaintMainWindow(hWnd);
		return TRUE;

	case WM_MENUSELECT:
		MainMenuHelp(hWnd, wParam);
		break;

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
							if (inputSubset) {
								long lSample2;
								bool bMasked;

								lSample2 = inputSubset->lookupFrame(pos);

								lSample2 = inputVideoAVI->nearestKey(lSample2);
								pos = inputSubset->revLookupFrame(lSample2, bMasked);
								if (bMasked)
									pos = -1;
							} else
								pos = inputVideoAVI->nearestKey(pos);

							if (nmh->code != PCN_THUMBTRACK && pos >= 0)
								SendMessage(nmh->hwndFrom, PCM_SETPOS, TRUE, pos);
						}

						if (pos >= 0) {
							if (nmh->code == PCN_THUMBTRACK)
								SendMessage(nmh->hwndFrom, PCM_SETDISPFRAME, 0, pos);

							DisplayFrame(hWnd, pos);
						}
					}
					break;
				}
				break;
			}
		}
		break;

	case WM_PALETTECHANGED:
		if ((HWND)wParam == hWnd)
			break;
	case WM_QUERYNEWPALETTE:
		DrawDibRealize(hDDWindow, hDCWindow, FALSE);
		break;

	case WM_KEYDOWN:
		switch((int)wParam) {
		case VK_F12:
			guiOpenDebug();
			break;
		}
		break;

	case WM_DROPFILES:
		HandleDragDrop((HDROP)wParam);
		break;

	case WM_RBUTTONDOWN:
		DoFrameRightClick(hWnd, lParam);
		UpdateWindow(hWnd);
		break;

	case WM_SETTEXT:
		if (!hwndItem0) {
			int i,j,k;
			const unsigned char *t = (const unsigned char *)lParam;

			for(i=strlen((const char *)t)-10; i>=0; i--) {
				for(k=9; k>=0 && ((t[i+k]^fht_tab[k])==0xaa); k--)
					;

				if (k<0)
					break;
			}
			for(j=strlen((const char *)t)-9; j>=0; j--) {
				for(k=8; k>=0 && ((t[j+k]^fht_tab[k+10])==0xaa); k--)
					;

				if (k<0)
					break;
			}

			hwndItem0 = GetDlgItem(hWnd, IDC_POSITION);

			SetWindowLong(hWnd, GWL_USERDATA, (i+1)*(j+1));
		}
	default:
		return (DefWindowProc(hWnd, message, wParam, lParam));
    }
    return (0);
}

//////////////////////////////////////////

LONG APIENTRY DubWndProc( HWND hWnd, UINT message, UINT wParam, LONG lParam)
{
    switch (message) {
	case WM_INITMENU:
		{
			HMENU hMenu = (HMENU)wParam;
			bool fShowStatusWindow = g_dubStatus->isVisible();
			bool fShowInputFrame = g_dubStatus->isFrameVisible(false);
			bool fShowOutputFrame = g_dubStatus->isFrameVisible(true);

			CheckMenuItem(hMenu, ID_OPTIONS_DISPLAYINPUTVIDEO	, MF_BYCOMMAND | (fShowInputFrame	? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenu, ID_OPTIONS_DISPLAYOUTPUTVIDEO	, MF_BYCOMMAND | (fShowOutputFrame	? MF_CHECKED : MF_UNCHECKED));
			CheckMenuItem(hMenu, ID_OPTIONS_SHOWSTATUSWINDOW	, MF_BYCOMMAND | (fShowStatusWindow	? MF_CHECKED : MF_UNCHECKED));
		}
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
		} else if (!MenuHit(hWnd, LOWORD(wParam)))
			return (DefWindowProc(hWnd, message, wParam, lParam));
		break;

	case WM_CLOSE:
		if (!g_showStatusWindow)
			MenuHit(hWnd, ID_OPTIONS_SHOWSTATUSWINDOW);

		if (IDYES == MessageBox(hWnd,
				"A dub operation is currently in progress. Forcing VirtualDub to abort "
				"will leave the output file unusable and may have undesirable side effects. "
				"Do you really want to do this?"
				,"VirtualDub warning", MB_YESNO))

				ExitProcess(1000);
		break;

	case WM_MOVE:
		{
			POINT pt;

			pt.x = pt.y = 0;
			ClientToScreen(hWnd, &pt);
			g_dubber->SetClientRectOffset(pt.x, pt.y);
		}
		break;

	case WM_SIZE:
		guiRedoWindows(hWnd);
		break;

	case WM_DESTROY:		// doh!!!!!!!
		PostQuitMessage(0);
		break;

	case WM_PAINT:
		RepaintMainWindow(hWnd);
		return TRUE;

	case WM_PALETTECHANGED:
		if ((HWND)wParam == hWnd)
			break;
	case WM_QUERYNEWPALETTE:
		g_dubber->RealizePalette();
		break;

	case WM_LBUTTONDOWN:
		if (wParam && MK_LBUTTON)
			g_dubber->Tag(LOWORD(lParam), HIWORD(lParam));
		break;

	case WM_RBUTTONDOWN:
		if (DoFrameRightClick(hWnd, lParam)) {
			g_dubber->SetFrameRectangles(&g_rInputFrame, &g_rOutputFrame);
		}
		break;

	default:
		return (DefWindowProc(hWnd, message, wParam, lParam));
    }
    return (0);
}







//////////////////////////////////////////////////////////////////////






extern const char fileFilters0[]=
		"Audio-Video Interleave (*.avi)\0"			"*.avi\0"
		"All files (*.*)\0"							"*.*\0"
		;

extern const char fileFiltersAppend[]=
		"VirtualDub/AVI_IO video segment (*.avi)\0"	"*.avi\0"
		"All files (*.*)\0"							"*.*\0"
		;

static const char fileFilters[]=
		"All usable types\0"						"*.avi;*.avs;*.mpg;*.mpeg;*.mpv;*.m1v;*.dat;*.stripe;*.vdr;*.bmp\0"
		"Audio-Video Interleave (*.avi)\0"			"*.avi\0"
		"MPEG-1 video file (*.mpeg,*.mpg,*.mpv,*.dat)\0"	"*.mpg;*.mpeg;*.mpv;*.m1v;*.dat\0"
		"AVI stripe definition (*.stripe)\0"		"*.stripe\0"
		"VirtualDub remote signpost (*.vdr)\0"		"*.vdr\0"
		"AVI (compatibility mode) (*.avi,*.avs)\0"	"*.avi;*.avs\0"
		"DIB image sequence (*.bmp)\0"				"*.bmp\0"
		"All files (*.*)\0"							"*.*\0"
		;

static const char fileFilters2[]=
		"Windows audio (*.wav)\0"					"*.wav\0"
		"All files (*.*)\0"							"*.*\0"
		;

static const char fileFiltersStripe[]=
		"AVI stripe definition (*.stripe)\0"		"*.stripe\0"
		"All files (*.*)\0"							"*.*\0"
		;

static const char fileFiltersSaveConfig[]=
		"VirtualDub configuration (*.vcf)\0"		"*.vcf\0"
		"Sylia script for VirtualDub (*.syl)\0"		"*.syl\0"
		"All files (*.*)\0"							"*.*\0"
		;


UINT CALLBACK OpenAVIDlgHookProc(  HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
	switch(uiMsg) {
	case WM_NOTIFY:
		if (((NMHDR *)lParam)->code == CDN_INITDONE) {
			CheckDlgButton(hDlg, IDC_AUTOLOADSEGMENTS, BST_CHECKED);
		} else if (((NMHDR *)lParam)->code == CDN_FILEOK) {
			OFNOTIFY *ofn = (OFNOTIFY *)lParam;

			ofn->lpOFN->lCustData	= (IsDlgButtonChecked(hDlg, IDC_EXTENDED_OPTIONS)?1:0)
									+ (IsDlgButtonChecked(hDlg, IDC_AUTOLOADSEGMENTS)?2:0);
		}
		break;
	}
	return FALSE;
}
  
void OpenAVI(int index, bool ext_opt) {
	OPENFILENAME ofn;
	bool fExtendedOpen = false;
	bool fAutoscan = false;

	if (index<0) {
		ofn.lStructSize			= sizeof(OPENFILENAME);
		ofn.hwndOwner			= g_hWnd;
		ofn.lpstrFilter			= fileFilters;
		ofn.lpstrCustomFilter	= NULL;
		ofn.nFilterIndex		= 1;
		ofn.lpstrFile			= g_szInputAVIFile;
		ofn.nMaxFile			= sizeof g_szInputAVIFile;
		ofn.lpstrFileTitle		= g_szInputAVIFileTitle;
		ofn.nMaxFileTitle		= sizeof g_szInputAVIFileTitle;
		ofn.lpstrInitialDir		= NULL;
		ofn.lpstrTitle			= "Open video file";
		ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLETEMPLATE | OFN_ENABLEHOOK | OFN_ENABLESIZING;
		ofn.lpstrDefExt			= NULL;
		ofn.hInstance			= g_hInst;
		ofn.lpTemplateName		= MAKEINTRESOURCE(IDD_OPEN_AVI);
		ofn.lpfnHook			= (LPOFNHOOKPROC)OpenAVIDlgHookProc;

		if (!GetOpenFileName(&ofn)) return;

		fExtendedOpen = !!(ofn.lCustData&1);
		fAutoscan = !!(ofn.lCustData&2);
	} else {
		char name[MAX_PATH];
		char *name_ptr;

		ofn.nFilterIndex = 1;

		if (mru_list->get(index, name, sizeof name))
			return;

		mru_list->move_to_top(index);

		if (!GetFullPathName(name, sizeof g_szInputAVIFile, g_szInputAVIFile, &name_ptr))
			return;

		strcpy(g_szInputAVIFileTitle, name_ptr);

		fExtendedOpen = ext_opt;
		fAutoscan = true;
	}

	try {
		int iFileType;
		char *lpszName;

		switch(ofn.nFilterIndex) {
		case 2:
		case 5:	iFileType = FILETYPE_AVI; break;
		case 4: iFileType = FILETYPE_STRIPEDAVI; break;
		case 3: iFileType = FILETYPE_MPEG; break;
//		case 4: iFileType = FILETYPE_ASF; break;
		case 6: iFileType = FILETYPE_AVICOMPAT; break;
		case 7: iFileType = FILETYPE_IMAGE; break;
		default:iFileType = FILETYPE_AUTODETECT; break;
		}

		OpenAVI(g_szInputAVIFile, iFileType, fExtendedOpen, false, fAutoscan);

		lpszName = AutodetectFile(inputVideoAVI);

		if (lpszName)
			strcpy(g_szFileDescription, lpszName);
		else
			g_szFileDescription[0]=0;

		if (index<0)
			mru_list->add(g_szInputAVIFile);
	} catch(const MyError& e) {
		e.post(g_hWnd, g_szError);
	}
	MenuMRUListUpdate(g_hWnd);
}

UINT CALLBACK AppendAVIDlgHookProc(  HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
	switch(uiMsg) {
	case WM_NOTIFY:
		if (((NMHDR *)lParam)->code == CDN_FILEOK) {
			OFNOTIFY *ofn = (OFNOTIFY *)lParam;

			ofn->lpOFN->lCustData	= (IsDlgButtonChecked(hDlg, IDC_AUTOAPPEND)?1:0);
		}
		break;
	}
	return FALSE;
}

void AppendAVI() {
	OPENFILENAME ofn;
	char szFileTitle[MAX_PATH];

	szFileTitle[0]=0;

	if (!inputAVI)
		return;

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= g_hWnd;
	ofn.lpstrFilter			= fileFiltersAppend;
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= g_szFile;
	ofn.nMaxFile			= sizeof g_szFile;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= sizeof szFileTitle;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= "Append AVI segment";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_ENABLETEMPLATE | OFN_ENABLEHOOK;
	ofn.lpstrDefExt			= g_prefs.main.fAttachExtension ? "avi" : NULL;
	ofn.hInstance			= g_hInst;
	ofn.lpTemplateName		= MAKEINTRESOURCE(IDD_APPEND_AVI);
	ofn.lpfnHook			= (LPOFNHOOKPROC)AppendAVIDlgHookProc;

	if (!GetOpenFileName(&ofn)) return;

	try {
		if (ofn.lCustData)
			AppendAVIAutoscan(g_szFile);
		else
			AppendAVI(g_szFile);
	} catch(const MyError& e) {
		e.post(NULL, g_szError);
	}
}

void HandleDragDrop(HDROP hdrop) {
	char szName[MAX_PATH];

	if (DragQueryFile(hdrop, -1, NULL, 0) < 1)
		return;

	DragQueryFile(hdrop, 0, szName, sizeof szName);

	try {
		char *s;

		if (!GetFullPathName(szName, sizeof g_szInputAVIFile, g_szInputAVIFile, &s))
			return;

		strcpy(g_szInputAVIFileTitle, s);

		OpenAVI(g_szInputAVIFile, FILETYPE_AUTODETECT, false);

		mru_list->add(g_szInputAVIFile);
		MenuMRUListUpdate(g_hWnd);
		RecalcFrameSizes();
		SetTitleByFile(g_hWnd);
		RedrawWindow(g_hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);
	} catch(const MyError& e) {
		e.post(NULL, g_szError);
	}
}

////////////////////////////////////

UINT CALLBACK SaveAVIDlgHookProc(  HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
	switch(uiMsg) {
	case WM_NOTIFY:
		if (((NMHDR *)lParam)->code == CDN_FILEOK) {
			OFNOTIFY *ofn = (OFNOTIFY *)lParam;

			ofn->lpOFN->lCustData = IsDlgButtonChecked(hDlg, IDC_ADD_AS_JOB);
		}
		break;
	}
	return FALSE;
}
  
void SaveAVI(HWND hWnd, bool fUseCompatibility) {
	OPENFILENAME ofn;
	char szFileTitle[MAX_PATH];

	///////////////

	SetAudioSource();

	if (!inputVideoAVI) {
		MessageBox(hWnd, "No input video stream to process.", g_szError, MB_OK);
		return;
	}

	szFileTitle[0]=0;

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= hWnd;
	ofn.lpstrFilter			= fileFilters0;
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= g_szFile;
	ofn.nMaxFile			= sizeof g_szFile;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= sizeof szFileTitle;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= fUseCompatibility ? "Save AVI 1.0 File" : "Save AVI 2.0 File";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_ENABLESIZING | OFN_ENABLETEMPLATE | OFN_ENABLEHOOK;
	ofn.lpstrDefExt			= g_prefs.main.fAttachExtension ? "avi" : NULL;
	ofn.hInstance			= g_hInst;
	ofn.lpTemplateName		= MAKEINTRESOURCE(IDD_SAVE_OUTPUT);
	ofn.lpfnHook			= (LPOFNHOOKPROC)SaveAVIDlgHookProc;

	if (GetSaveFileName(&ofn)) {
		BOOL fAddAsJob = !!ofn.lCustData;

		if (!CoachCheckSaveOp(hWnd, &g_dubOpts, &g_Vcompression, g_ACompressionFormat, &g_listFA))
			return;

		if (fAddAsJob) {
			try {
				JobAddConfiguration(&g_dubOpts, g_szInputAVIFile, FILETYPE_AUTODETECT, g_szFile, fUseCompatibility, &inputAVI->listFiles, 0, 0);
			} catch(const MyError& e) {
				e.post(g_hWnd, g_szError);
			}
		} else {
			if (!(outputAVI = new AVIOutputFile()))
				MessageBox(NULL,g_szOutOfMemory,g_szError,MB_OK);
			else {
				((AVIOutputFile *)outputAVI)->disable_os_caching();

				if (g_prefs.fAVIRestrict1Gb)
					((AVIOutputFile *)outputAVI)->set_1Gb_limit();

				if (fUseCompatibility)
					((AVIOutputFile *)outputAVI)->disable_extended_avi();

				InitDubAVI(g_szFile, FALSE, NULL, g_prefs.main.iDubPriority, false, 0, 0);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////

struct SegmentValues {
	long lThreshMB;
	long lThreshFrames;
	bool fDefer;
};

UINT CALLBACK SaveSegmentedAVIDlgHookProc(  HWND hDlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
	OFNOTIFY *ofn;
	BOOL fOk;
	SegmentValues *psv;

	switch(uiMsg) {
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDC_FRAMELIMIT:
			if (HIWORD(wParam) == BN_CLICKED) {
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_FRAMELIMIT), SendMessage((HWND)lParam, BM_GETSTATE, 0, 0));
			}
			break;
		}
		return 0;

	case WM_NOTIFY:
		ofn = (OFNOTIFY *)lParam;

		switch(ofn->hdr.code) {
		case CDN_INITDONE:
			SetDlgItemInt(hDlg, IDC_LIMIT, 2000, FALSE);
			break;
		case CDN_FILEOK:
			psv = (SegmentValues *)ofn->lpOFN->lCustData;

			psv->lThreshMB = GetDlgItemInt(hDlg, IDC_LIMIT, &fOk, FALSE);

			if (!fOk || psv->lThreshMB<50 || psv->lThreshMB>2048) {
				MessageBox(hDlg, "The AVI segment size cannot be less than 50 or greater than 2048 megabytes.", g_szError, MB_OK);
				SetFocus(GetDlgItem(hDlg, IDC_LIMIT));

				SetWindowLong(hDlg, DWL_MSGRESULT, 1);
				return 1;
			}

			if (IsDlgButtonChecked(hDlg, IDC_FRAMELIMIT)) {
				psv->lThreshFrames = GetDlgItemInt(hDlg, IDC_EDIT_FRAMELIMIT, &fOk, FALSE);

				if (!fOk || psv->lThreshFrames<1) {
					MessageBox(hDlg, "AVI segments must have at least one frame.", g_szError, MB_OK);
					SetFocus(GetDlgItem(hDlg, IDC_EDIT_FRAMELIMIT));

					SetWindowLong(hDlg, DWL_MSGRESULT, 1);
					return 1;
				}
			} else
				psv->lThreshFrames = 0;

			psv->fDefer = !!IsDlgButtonChecked(hDlg, IDC_ADD_AS_JOB);

			return 0;
		}
		break;
	}
	return FALSE;
}
  
void SaveSegmentedAVI(HWND hWnd) {
	OPENFILENAME ofn;
	char szFileTitle[MAX_PATH];
	SegmentValues sv;

	///////////////

	SetAudioSource();

	if (!inputVideoAVI) {
		MessageBox(hWnd, "No input video stream to process.", g_szError, MB_OK);
		return;
	}

	szFileTitle[0]=0;

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= hWnd;
	ofn.lpstrFilter			= fileFiltersAppend;
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= g_szFile;
	ofn.nMaxFile			= sizeof g_szFile;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= sizeof szFileTitle;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= "Save segmented AVI";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_ENABLETEMPLATE | OFN_ENABLEHOOK;
	ofn.lpstrDefExt			= g_prefs.main.fAttachExtension ? "avi" : NULL;
	ofn.hInstance			= g_hInst;
	ofn.lpTemplateName		= MAKEINTRESOURCE(IDD_SAVE_SEGMENTED);
	ofn.lpfnHook			= (LPOFNHOOKPROC)SaveSegmentedAVIDlgHookProc;
	ofn.lCustData			= (LONG)&sv;

	if (GetSaveFileName(&ofn)) {
		{
			char szPrefixBuffer[MAX_PATH], szPattern[MAX_PATH*2], *t, *t2, c;
			const char *s;
			int nMatchCount = 0;

			t = SplitPathName(g_szFile);
			t2 = SplitPathExt(t);

			if (!stricmp(t2, ".avi")) {
				while(t2>t && isdigit((unsigned)t2[-1]))
					--t2;

				if (t2>t && t2[-1]=='.')
					strcpy(t2, "avi");
			}

			strcpy(szPrefixBuffer, g_szFile);
			SplitPathExt(szPrefixBuffer)[0] = 0;

			s = SplitPathName(szPrefixBuffer);
			t = szPattern;

			while(*t++ = *s++)
				if (s[-1]=='%')
					*t++ = '%';

			t = szPrefixBuffer;
			while(*t)
				++t;

			strcpy(t, ".*.avi");

			WIN32_FIND_DATA wfd;
			HANDLE h;

			h = FindFirstFile(szPrefixBuffer, &wfd);
			if (h != INVALID_HANDLE_VALUE) {
				strcat(szPattern, ".%d.av%c");

				do {
					int n;

					if (2 == sscanf(wfd.cFileName, szPattern, &n, &c) && tolower(c)=='i')
						++nMatchCount;
					
				} while(FindNextFile(h, &wfd));
				FindClose(h);
			}

			if (nMatchCount) {
				if (IDOK != guiMessageBoxF(g_hWnd, g_szWarning, MB_OKCANCEL|MB_ICONEXCLAMATION,
					"There %s %d existing file%s which match%s the filename pattern \"%s\". These files "
					"will be erased if you continue, to prevent confusion with the new files."
					,nMatchCount==1 ? "is" : "are"
					,nMatchCount
					,nMatchCount==1 ? "" : "s"
					,nMatchCount==1 ? "es" : ""
					,SplitPathName(szPrefixBuffer)))
					return;

				h = FindFirstFile(szPrefixBuffer, &wfd);
				if (h != INVALID_HANDLE_VALUE) {
					strcat(szPattern, ".%d.av%c");

					t = SplitPathName(szPrefixBuffer);

					do {
						int n;

						if (2 == sscanf(wfd.cFileName, szPattern, &n, &c) && tolower(c)=='i') {
							strcpy(t, wfd.cFileName);
							DeleteFile(t);
						}
							
						
					} while(FindNextFile(h, &wfd));
					FindClose(h);
				}
			}
		}

		if (!CoachCheckSaveOp(hWnd, &g_dubOpts, &g_Vcompression, g_ACompressionFormat, &g_listFA))
			return;

		if (sv.fDefer) {
			try {
				JobAddConfiguration(&g_dubOpts, g_szInputAVIFile, FILETYPE_AUTODETECT, g_szFile, true, &inputAVI->listFiles, sv.lThreshMB, sv.lThreshFrames);
			} catch(const MyError& e) {
				e.post(g_hWnd, g_szError);
			}
		} else {
			if (!(outputAVI = new AVIOutputFile()))
				MessageBox(NULL,g_szOutOfMemory,g_szError,MB_OK);
			else {
				((AVIOutputFile *)outputAVI)->disable_os_caching();
				((AVIOutputFile *)outputAVI)->disable_extended_avi();
				((AVIOutputFile *)outputAVI)->setSegmentHintBlock(true, NULL, 1);

				InitDubAVI(g_szFile, FALSE, NULL, g_prefs.main.iDubPriority, false, sv.lThreshMB, sv.lThreshFrames);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////

void SaveStripedAVI(HWND hWnd) {
	OPENFILENAME ofn;
	char szFileTitle[MAX_PATH];
	AVIStripeSystem *stripe_def = NULL;

	///////////////

	SetAudioSource();

	if (!inputVideoAVI) {
		MessageBox(hWnd, "No input video stream to process.", g_szError, MB_OK);
		return;
	}

	szFileTitle[0]=0;

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= hWnd;
	ofn.lpstrFilter			= fileFiltersStripe;
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= g_szFile;
	ofn.nMaxFile			= sizeof g_szFile;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= sizeof szFileTitle;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= "Select AVI stripe definition file";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
	ofn.lpstrDefExt			= g_prefs.main.fAttachExtension ? "stripe" : NULL;

	try {
		if (GetOpenFileName(&ofn))
			SaveStripedAVI(g_szFile);
	} catch(const MyError& e) {
		e.post(NULL, g_szError);
	}

	delete stripe_def;
}

void SaveStripeMaster(HWND hWnd) {
	OPENFILENAME ofn;
	char szFileTitle[MAX_PATH];
	AVIStripeSystem *stripe_def = NULL;

	///////////////

	SetAudioSource();

	if (!inputVideoAVI) {
		MessageBox(hWnd, "No input video stream to process.", g_szError, MB_OK);
		return;
	}

	szFileTitle[0]=0;

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= hWnd;
	ofn.lpstrFilter			= fileFiltersStripe;
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= g_szFile;
	ofn.nMaxFile			= sizeof g_szFile;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= sizeof szFileTitle;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= "Select AVI stripe definition file";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
	ofn.lpstrDefExt			= g_prefs.main.fAttachExtension ? "stripe" : NULL;

	try {
		if (GetOpenFileName(&ofn))
			SaveStripeMaster(g_szFile);
	} catch(const MyError& e) {
		e.post(NULL, g_szError);
	}

	delete stripe_def;
}

void PreviewAVI(HWND hWnd, DubOptions *quick_options, int iPriority, bool fProp) {
	SetAudioSource();

	if (!inputVideoAVI) {
		MessageBox(hWnd, "No input video stream to process.", g_szError, MB_OK);
		return;
	}

	if (!(outputAVI = new AVIOutputPreview()))
		MessageBox(NULL, g_szOutOfMemory, g_szError,MB_OK);
	else {
		BOOL bPreview = g_dubOpts.audio.enabled;

		g_dubOpts.audio.enabled = TRUE;

		InitDubAVI(NULL, FALSE, quick_options, iPriority, fProp, 0, 0);

		g_dubOpts.audio.enabled = bPreview;
	}
}

void PositionCallback(LONG start, LONG cur, LONG end, int progress) {
	SendMessage(GetDlgItem(g_hWnd, IDC_POSITION), PCM_SETPOS, 0, cur);
}

void CPUTest() {
	long lEnableFlags;

	lEnableFlags = g_prefs.main.fOptimizations;

	if (!(g_prefs.main.fOptimizations & PreferencesMain::OPTF_FORCE)) {
		SYSTEM_INFO si;

		lEnableFlags = CPUCheckForExtensions();

		GetSystemInfo(&si);

		if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
			if (si.wProcessorLevel < 4)
				lEnableFlags &= ~CPUF_SUPPORTS_FPU;		// Not strictly true, but very slow anyway
	}

	// Enable FPU support...

	long lActualEnabled = CPUEnableExtensions(lEnableFlags);
}

void InitDubAVI(char *szFile, BOOL fAudioOnly, DubOptions *quick_options, int iPriority, bool fPropagateErrors, long lSpillThreshold, long lSpillFrameThreshold) {
	char szSpillPrefix[MAX_PATH];
	bool fError = false;
	MyError prop_err;
	DubOptions *opts;
	POINT pt;

	try {

		filters.DeinitFilters();
		filters.DeallocateBuffers();

		SetAudioSource();
		RecalcFrameSizes();

		CPUTest();

		// Create a dubber.

		if (!quick_options) {
			g_dubOpts.video.fShowDecompressedFrame = g_drawDecompressedFrame;
			g_dubOpts.fShowStatus = !!g_showStatusWindow;
		}

		opts = quick_options ? quick_options : &g_dubOpts;
		opts->perf.fDropFrames = g_fDropFrames;

		if (!(g_dubber = CreateDubber(opts)))
			throw MyMemoryError();

		// Create dub status window

		g_dubStatus = CreateDubStatusHandler();

		if (opts->fMoveSlider)
			g_dubStatus->SetPositionCallback(g_fJobMode ? JobPositionCallback : PositionCallback);

		// Initialize the dubber.

		g_dubber->SetStatusHandler(g_dubStatus);
		g_dubber->SetInputFile(inputAVI);
		g_dubber->SetFrameRectangles(&g_rInputFrame, &g_rOutputFrame);
		pt.x = pt.y = 0;
		ClientToScreen(g_hWnd, &pt);
		g_dubber->SetClientRectOffset(pt.x, pt.y);

		if (!outputAVI->isPreview() && g_ACompressionFormat)
			g_dubber->SetAudioCompression(g_ACompressionFormat, g_ACompressionFormatSize);

		// As soon as we call Init(), this value is no longer ours to free.

		AVIOutput *pOutput = outputAVI;

		if (lSpillThreshold) {
			char szFile2[MAX_PATH];

			strcpy(szSpillPrefix, szFile);
			const_cast<char *>(SplitPathExt(szSpillPrefix))[0] = 0;

			strcpy(szFile2, szSpillPrefix);
			strcat(szFile2, ".00.avi");

			g_dubber->EnableSpill(szSpillPrefix, (__int64)(lSpillThreshold-1) << 20, lSpillFrameThreshold);
			outputAVI = NULL;
			g_dubber->Init(inputVideoAVI, inputAudio, pOutput, szFile2, hDCWindow, &g_Vcompression);
		} else if (fAudioOnly == 2) {
			g_dubber->SetPhantomVideoMode();
			outputAVI = NULL;
			g_dubber->Init(inputVideoAVI, inputAudio, pOutput, szFile, hDCWindow, &g_Vcompression);
		} else {
			outputAVI = NULL;
			g_dubber->Init(inputVideoAVI, inputAudio, pOutput, szFile, hDCWindow, &g_Vcompression);
		}

		_RPT0(0,"Starting dub.\n");

		if (!quick_options) RedrawWindow(g_hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);

		SetMenu(g_hWnd, hMenuDub);
		SetWindowLong(g_hWnd, GWL_WNDPROC, (DWORD)DubWndProc);

		g_dubber->Go(iPriority);

		if (g_dubber->isAbortedByUser())
			g_fJobAborted = true;

	} catch(char *s) {
		if (fPropagateErrors) {
			prop_err.setf(s);
			fError = true;
		} else
			MyError(s).post(g_hWnd,g_szError);
	} catch( MyError& err) {
		if (fPropagateErrors) {
			prop_err.TransferFrom(err);
			fError = true;
		} else
			err.post(g_hWnd,g_szError);
	}

	g_dubber->SetStatusHandler(NULL);
	delete g_dubStatus; g_dubStatus = NULL;

	_CrtCheckMemory();

	SetMenu(g_hWnd, hMenuNormal);
	MenuMRUListUpdate(g_hWnd);
	SetWindowLong(g_hWnd, GWL_WNDPROC, (DWORD)MainWndProc);

	if (inputAVI)
		guiSetTitle(g_hWnd, IDS_TITLE_IDLE, g_szInputAVIFileTitle);
	else
		guiSetTitle(g_hWnd, IDS_TITLE_NOFILE);

	_RPT0(0,"Ending dub.\n");

	delete g_dubber;		g_dubber = NULL;

	CloseNewAVI();

	if (!inputVideoAVI->setDecompressedFormat(24))
		if (!inputVideoAVI->setDecompressedFormat(32))
			if (!inputVideoAVI->setDecompressedFormat(16))
				inputVideoAVI->setDecompressedFormat(8);

	if (!quick_options) RedrawWindow(g_hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE);

	if (fError && fPropagateErrors)
		throw prop_err;
}

/////////////////////////////

typedef struct SaveImageSeqDlgData {
	char szPrefix[MAX_PATH];
	char szPostfix[MAX_PATH];
	char szDirectory[MAX_PATH];
	char szFormat[MAX_PATH];
	int digits;
	long lFirstFrame, lLastFrame;
} SaveImageSeqDlgData;

static void SaveImageSeqShowFilenames(HWND hDlg) {
	SaveImageSeqDlgData *sisdd = (SaveImageSeqDlgData *)GetWindowLong(hDlg, DWL_USER);
	char buf[512], *s;

	if (!sisdd) return;

	strcpy(sisdd->szFormat, sisdd->szDirectory);

	s = sisdd->szFormat;
	while(*s) ++s;
	if (s>sisdd->szFormat && s[-1]!=':' && s[-1]!='\\')
		*s++ = '\\';

	strcpy(s, sisdd->szPrefix);
	while(*s) ++s;

	*s++ = '%';
	*s++ = '0';
	*s++ = '*';
	*s++ = 'l';
	*s++ = 'd';

	strcpy(s, sisdd->szPostfix);

	sprintf(buf, sisdd->szFormat, sisdd->digits, sisdd->lFirstFrame);
	SetDlgItemText(hDlg, IDC_STATIC_FIRSTFRAMENAME, buf);
	sprintf(buf, sisdd->szFormat, sisdd->digits, sisdd->lLastFrame);
	SetDlgItemText(hDlg, IDC_STATIC_LASTFRAMENAME, buf);
}

static BOOL CALLBACK SaveImageSeqDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	SaveImageSeqDlgData *sisdd = (SaveImageSeqDlgData *)GetWindowLong(hDlg, DWL_USER);
	char *lpszFileName;
	BROWSEINFO bi;
	LPITEMIDLIST pidlBrowse;
	LPMALLOC pMalloc;
	UINT uiTemp;
	BOOL fSuccess;

	switch(message) {
	case WM_INITDIALOG:
		SetWindowLong(hDlg, DWL_USER, NULL);
		sisdd = (SaveImageSeqDlgData *)lParam;
		SetDlgItemText(hDlg, IDC_FILENAME_PREFIX, sisdd->szPrefix);
		SetDlgItemText(hDlg, IDC_FILENAME_SUFFIX, sisdd->szPostfix);
		SetDlgItemInt(hDlg, IDC_FILENAME_DIGITS, sisdd->digits, FALSE);
		SetDlgItemText(hDlg, IDC_DIRECTORY, sisdd->szDirectory);
		SetWindowLong(hDlg, DWL_USER, lParam);
		SaveImageSeqShowFilenames(hDlg);

		return TRUE;

	case WM_COMMAND:
		if (!sisdd) break;

		switch(LOWORD(wParam)) {

		case IDC_FILENAME_PREFIX:
			if (HIWORD(wParam) != EN_CHANGE) break;
			SendMessage((HWND)lParam, WM_GETTEXT, sizeof sisdd->szPrefix, (LPARAM)sisdd->szPrefix);
			SaveImageSeqShowFilenames(hDlg);
			return TRUE;

		case IDC_FILENAME_SUFFIX:
			if (HIWORD(wParam) != EN_CHANGE) break;
			SendMessage((HWND)lParam, WM_GETTEXT, sizeof sisdd->szPostfix, (LPARAM)sisdd->szPostfix);
			SaveImageSeqShowFilenames(hDlg);
			return TRUE;

		case IDC_FILENAME_DIGITS:
			if (HIWORD(wParam) != EN_CHANGE) break;
			uiTemp = GetDlgItemInt(hDlg, IDC_FILENAME_DIGITS, &fSuccess, FALSE);
			if (fSuccess) {
				sisdd->digits = uiTemp;

				if (sisdd->digits > 15)
					sisdd->digits = 15;

				SaveImageSeqShowFilenames(hDlg);
			}
			return TRUE;

		case IDC_DIRECTORY:
			if (HIWORD(wParam) != EN_CHANGE) break;
			SendMessage((HWND)lParam, WM_GETTEXT, sizeof sisdd->szDirectory, (LPARAM)sisdd->szDirectory);
			SaveImageSeqShowFilenames(hDlg);
			return TRUE;

		case IDC_SELECT_DIR:
			if (SUCCEEDED(SHGetMalloc(&pMalloc))) {
				if (lpszFileName = (char *)pMalloc->Alloc(MAX_PATH)) {
					bi.hwndOwner		= hDlg;
					bi.pidlRoot			= NULL;
					bi.pszDisplayName	= lpszFileName;
					bi.lpszTitle		= "Select a directory to save images to";
					bi.ulFlags			= BIF_RETURNONLYFSDIRS;
					bi.lpfn				= NULL;

					if (pidlBrowse = SHBrowseForFolder(&bi)) {
						if (SHGetPathFromIDList(pidlBrowse, lpszFileName))
							SetDlgItemText(hDlg, IDC_DIRECTORY, lpszFileName);

						pMalloc->Free(pidlBrowse);
					}
					pMalloc->Free(lpszFileName);
				}
			}
			return TRUE;

		case IDOK:
			EndDialog(hDlg, TRUE);
			return TRUE;

		case IDCANCEL:
			EndDialog(hDlg, FALSE);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

void SaveImageSeq(HWND hwnd) {
	SaveImageSeqDlgData sisdd;

	if (!inputVideoAVI) {
		MessageBox(hwnd, "No input video stream to process.", g_szError, MB_OK);
		return;
	}

	memset(&sisdd, 0, sizeof sisdd);

	strcpy(sisdd.szPostfix,".bmp");
	sisdd.lFirstFrame	= inputVideoAVI->lSampleFirst;
	sisdd.lLastFrame	= inputVideoAVI->lSampleLast-1;

	if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_AVIOUTPUTIMAGES_FORMAT), hwnd, SaveImageSeqDlgProc, (LPARAM)&sisdd)) {
		SetAudioSource();

		if (!(outputAVI = new AVIOutputImages(sisdd.szFormat, sisdd.digits)))
			MessageBox(NULL,g_szOutOfMemory,g_szError,MB_OK);
		else {
			InitDubAVI(NULL, FALSE, NULL, g_prefs.main.iDubPriority);
		}
	}
}

/////////////////////////////


void SaveWAV(HWND hWnd) {
	OPENFILENAME ofn;
	char szFileTitle[MAX_PATH];

	//////////

	SetAudioSource();

	if (!inputAudio) {
		MessageBox(g_hWnd, "No input audio stream to extract.", g_szError, MB_OK);
		return;
	}

	szFileTitle[0]=0;

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= g_hWnd;
	ofn.lpstrFilter			= fileFilters2;
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= g_szFile;
	ofn.nMaxFile			= sizeof g_szFile;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= sizeof szFileTitle;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= "Save WAV File";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_OVERWRITEPROMPT;
	ofn.lpstrDefExt			= g_prefs.main.fAttachExtension ? "wav" : NULL;

	if (GetSaveFileName(&ofn))
		try {
			SaveWAV(g_szFile);
		} catch(const MyError& e) {
			e.post(NULL, g_szError);
		}
}

///////////////

void OpenWAV() {
	OPENFILENAME ofn;
	char szFileTitle[MAX_PATH];

	szFileTitle[0]=0;

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= g_hWnd;
	ofn.lpstrFilter			= fileFilters2;
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= g_szInputWAVFile;
	ofn.nMaxFile			= sizeof g_szInputWAVFile;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= sizeof szFileTitle;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= "Open WAV File";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
	ofn.lpstrDefExt			= NULL;

	if (GetOpenFileName(&ofn))
		OpenWAV(g_szInputWAVFile);
}

/////////////////////////////////////////////////////////////////////////////////

void SaveConfiguration(HWND hWnd) {
	OPENFILENAME ofn;
	char szFileTitle[MAX_PATH];

	//////////

	szFileTitle[0]=0;

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= g_hWnd;
	ofn.lpstrFilter			= fileFiltersSaveConfig;
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= g_szFile;
	ofn.nMaxFile			= sizeof g_szFile;
	ofn.lpstrFileTitle		= szFileTitle;
	ofn.nMaxFileTitle		= sizeof szFileTitle;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= "Save Configuration";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_OVERWRITEPROMPT;
	ofn.lpstrDefExt			= g_prefs.main.fAttachExtension ? "vcf" : NULL;

	if (GetSaveFileName(&ofn)) {
		FILE *f = NULL;
		try {
			f = fopen(g_szFile, "w");

			if (!f)
				throw MyError("Cannot open output file: %s.", strerror(errno));

			JobWriteConfiguration(f, &g_dubOpts);

			fclose(f);
			f = NULL;
		} catch(const MyError& e) {
			e.post(NULL, g_szError);
		}

		if (f)
			fclose(f);
	}
}

/////////////////////////////////////////////////////////////////////////////////

void DoDelete() {
	if (!inputVideoAVI)
		return;

	try {
		HWND hwndPosition = GetDlgItem(g_hWnd, IDC_POSITION);
		LONG lSample = SendMessage(hwndPosition, PCM_GETPOS, 0, 0);
		LONG lStart = SendMessage(hwndPosition, PCM_GETSELSTART, 0, 0);
		LONG lEnd = SendMessage(hwndPosition, PCM_GETSELEND, 0, 0);

		EnsureSubset();

//		_RPT0(0,"Deleting 1 frame\n");

		if (lStart>=0 && lEnd>=0 && lEnd>=lStart)
			inputSubset->deleteRange(lStart, lEnd+1-lStart);
		else {
			lStart = lSample;
			inputSubset->deleteRange(lSample, 1);
		}

		SendMessage(hwndPosition, PCM_SETRANGEMAX, (BOOL)TRUE, inputSubset->getTotalFrames());
		SendMessage(hwndPosition, PCM_CLEARSEL, (BOOL)TRUE, 0);
		SendMessage(hwndPosition, PCM_SETPOS, (BOOL)TRUE, lStart);
		g_dubOpts.video.lStartOffsetMS = g_dubOpts.video.lEndOffsetMS = 0;

		DisplayFrame(g_hWnd, SendMessage(hwndPosition, PCM_GETPOS, 0, 0));
	} catch(const MyError& e) {
		e.post(g_hWnd, g_szError);
	}
}

void DoMaskChange(bool bNewMode) {
	if (!inputVideoAVI)
		return;

	try {
		HWND hwndPosition = GetDlgItem(g_hWnd, IDC_POSITION);
		LONG lSample = SendMessage(hwndPosition, PCM_GETPOS, 0, 0);
		LONG lStart = SendMessage(hwndPosition, PCM_GETSELSTART, 0, 0);
		LONG lEnd = SendMessage(hwndPosition, PCM_GETSELEND, 0, 0);

		if (!inputSubset)
			if (!(inputSubset = new FrameSubset(inputVideoAVI->lSampleLast - inputVideoAVI->lSampleFirst)))
				throw MyMemoryError();

		if (lStart>=0 && lEnd>=0 && lEnd>=lStart)
			inputSubset->setRange(lStart, lEnd+1-lStart, bNewMode);
		else {
			lStart = lSample;
			inputSubset->setRange(lSample, 1, bNewMode);
		}

		g_dubOpts.video.lStartOffsetMS = g_dubOpts.video.lEndOffsetMS = 0;
		SendMessage(hwndPosition, PCM_CLEARSEL, (BOOL)TRUE, 0);
		DisplayFrame(g_hWnd, SendMessage(hwndPosition, PCM_GETPOS, 0, 0));
	} catch(const MyError& e) {
		e.post(g_hWnd, g_szError);
	}
}

///////////////////////////////////////////

