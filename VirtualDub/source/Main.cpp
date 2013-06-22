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

#include "stdafx.h"

#include <windows.h>

#include "resource.h"
#include "prefs.h"
#include "oshelper.h"
#include "gui.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/memory.h>
#include <vd2/system/filesys.h>
#include <vd2/system/registry.h>
#include <vd2/system/vdalloc.h>
#include <vd2/Dita/services.h>
#include "Dub.h"
#include "DubOutput.h"
#include "command.h"
#include "job.h"
#include "project.h"
#include "projectui.h"
#include "crash.h"

#include "PositionControl.h"
#include "InputFile.h"
#include <vd2/system/error.h>

///////////////////////////////////////////////////////////////////////////

enum {
	kFileDialog_Config			= 'conf',
	kFileDialog_ImageDst		= 'imgd'
};

///////////////////////////////////////////////////////////////////////////

HINSTANCE	g_hInst;
HWND		g_hWnd =NULL;
HACCEL		g_hAccelMain;

bool				g_fDropFrames			= false;
bool				g_fSwapPanes			= false;
bool				g_bExit					= false;

vdautoptr<VDProjectUI> g_projectui;
VDProject *g_project;

wchar_t g_szInputAVIFile[MAX_PATH];
wchar_t g_szInputWAVFile[MAX_PATH];
wchar_t g_szFile[MAX_PATH];

extern const char g_szError[]="VirtualDub Error";
extern const char g_szWarning[]="VirtualDub Warning";

///////////////////////////

extern bool Init(HINSTANCE hInstance, LPCWSTR lpCmdLine, int nCmdShow);
extern void Deinit();

void OpenAVI(int index, bool extended_opt);
void PreviewAVI(HWND, DubOptions *, int iPriority=0, bool fProp=false);
void SaveAVI(HWND, bool);
void SaveSegmentedAVI(HWND);
void CPUTest();
void SaveImageSeq(HWND);
void SaveConfiguration(HWND);

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
    LPSTR /*lpCmdLine*/, int nCmdShow )
{
	MSG msg;

	Init(hInstance, GetCommandLineW(), nCmdShow);

	// Load a file on the command line.

	if (*g_szFile)
		try {
			g_project->Open(g_szFile);
		} catch(const MyError& e) {
			e.post(g_hWnd, g_szError);
		}

    // Acquire and dispatch messages until a WM_QUIT message is received.

	for(;;) {
		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				goto wm_quit_detected;

			if (guiCheckDialogs(&msg))
				continue;

			HWND hwndRoot = VDGetAncestorW32(msg.hwnd, GA_ROOT);

			if (hwndRoot == g_hWnd && TranslateAccelerator(g_hWnd, g_hAccelMain, &msg))
				continue;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (!g_project->Tick())
			WaitMessage();
	}
wm_quit_detected:

	Deinit();

	VDCHECKPOINT;

    return (msg.wParam);           // Returns the value from PostQuitMessage.

}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////

char PositionFrameTypeCallback(HWND hwnd, void *pvData, long pos) {
	try {
		if (inputVideoAVI) {
			bool bMasked;
			long nFrame = inputSubset->lookupFrame(pos, bMasked);

			return bMasked ? 'M' : inputVideoAVI->getFrameTypeChar(nFrame);
		} else
			return 0;
	} catch(const MyError&) {
		return 0;
	}
}


//////////////////////////////////////////////////////////////////////






extern const wchar_t fileFilters0[]=
		L"Audio-Video Interleave (*.avi)\0"			L"*.avi\0"
		L"All files (*.*)\0"						L"*.*\0"
		;

extern const wchar_t fileFiltersAppend[]=
		L"VirtualDub/AVI_IO video segment (*.avi)\0"	L"*.avi\0"
		L"All files (*.*)\0"							L"*.*\0"
		;

static const wchar_t fileFiltersSaveConfig[]=
		L"VirtualDub configuration (*.vcf)\0"		L"*.vcf\0"
		L"Sylia script for VirtualDub (*.syl)\0"	L"*.syl\0"
		L"All files (*.*)\0"						L"*.*\0"
		;

  
void OpenAVI(bool ext_opt) {
	bool fExtendedOpen = false;
	bool fAutoscan = false;

	IVDInputDriver *pDriver = 0;

	std::vector<int> xlat;
	tVDInputDrivers inputDrivers;

	VDGetInputDrivers(inputDrivers, IVDInputDriver::kF_Video);

	VDStringW fileFilters(VDMakeInputDriverFileFilter(inputDrivers, xlat));

	static const VDFileDialogOption sOptions[]={
		{ VDFileDialogOption::kBool, 0, L"Ask for e&xtended options after this dialog", 0, 0 },
		{ VDFileDialogOption::kBool, 1, L"Automatically load linked segments", 0, 0 },
		{ VDFileDialogOption::kSelectedFilter, 2, 0, 0, 0 },
		{0}
	};

	int optVals[3]={0,1,0};

	VDStringW fname(VDGetLoadFileName(VDFSPECKEY_LOADVIDEOFILE, (VDGUIHandle)g_hWnd, L"Open video file", fileFilters.c_str(), NULL, sOptions, optVals));

	if (fname.empty())
		return;

	fExtendedOpen = !!optVals[0];
	fAutoscan = !!optVals[1];

	if (xlat[optVals[2]-1] >= 0)
		pDriver = inputDrivers[xlat[optVals[2]-1]];

	VDAutoLogDisplay logDisp;
	g_project->Open(fname.c_str(), pDriver, fExtendedOpen, false, fAutoscan);
	logDisp.Post((VDGUIHandle)g_hWnd);
}

////////////////////////////////////

void SaveAVI(HWND hWnd, bool fUseCompatibility) {
	if (!inputVideoAVI) {
		MessageBox(hWnd, "No input video stream to process.", g_szError, MB_OK);
		return;
	}

	static const VDFileDialogOption sOptions[]={
		{ VDFileDialogOption::kBool, 0, L"Don't run this job now; &add it to job control so I can run it in batch mode.", 0, 0 },
		{0}
	};

	int optVals[1]={0};

	VDStringW fname(VDGetSaveFileName(VDFSPECKEY_SAVEVIDEOFILE, (VDGUIHandle)hWnd, fUseCompatibility ? L"Save AVI 1.0 File" : L"Save AVI 2.0 File", fileFilters0, L"avi", sOptions, optVals));
	if (!fname.empty()) {
		bool fAddAsJob = !!optVals[0];

		if (fAddAsJob) {
			JobAddConfiguration(&g_dubOpts, g_szInputAVIFile, NULL, fname.c_str(), fUseCompatibility, &inputAVI->listFiles, 0, 0);
		} else {
			SaveAVI(fname.c_str(), false, NULL, fUseCompatibility);
		}
	}
}

///////////////////////////////////////////////////////////////////////////

static const char g_szRegKeyPersistence[]="Persistence";
static const char g_szRegKeyRunAsJob[]="Run as job";
static const char g_szRegKeySegmentFrameCount[]="Segment frame limit";
static const char g_szRegKeyUseSegmentFrameCount[]="Use segment frame limit";
static const char g_szRegKeySegmentSizeLimit[]="Segment size limit";
static const char g_szRegKeySaveSelectionAndEditList[]="Save edit list";
  
void SaveSegmentedAVI(HWND hWnd) {
	if (!inputVideoAVI) {
		MessageBox(hWnd, "No input video stream to process.", g_szError, MB_OK);
		return;
	}

	static const VDFileDialogOption sOptions[]={
		{ VDFileDialogOption::kBool, 0, L"Don't run this job now; &add it to job control so I can run it in batch mode.", 0, 0 },
		{ VDFileDialogOption::kEnabledInt, 1, L"&Limit number of video frames per segment:", 1, 0x7fffffff },
		{ VDFileDialogOption::kInt, 3, L"File segment &size limit in MB (50-2048):", 50, 2048 },
		{0}
	};

	VDRegistryAppKey key(g_szRegKeyPersistence);
	int optVals[4]={
		key.getBool(g_szRegKeyRunAsJob, false),
		key.getBool(g_szRegKeyUseSegmentFrameCount, false),
		key.getInt(g_szRegKeySegmentFrameCount, 100),
		key.getInt(g_szRegKeySegmentSizeLimit, 2000),
	};

	VDStringW fname(VDGetSaveFileName(VDFSPECKEY_SAVEVIDEOFILE, (VDGUIHandle)hWnd, L"Save segmented AVI", fileFiltersAppend, L"avi", sOptions, optVals));

	if (!fname.empty()) {
		key.setBool(g_szRegKeyRunAsJob, !!optVals[0]);
		key.setBool(g_szRegKeyUseSegmentFrameCount, !!optVals[1]);
		if (optVals[1])
			key.setInt(g_szRegKeySegmentFrameCount, optVals[2]);
		key.setInt(g_szRegKeySegmentSizeLimit, optVals[3]);

		char szFile[MAX_PATH];

		strcpy(szFile, VDTextWToA(fname).c_str());

		{
			char szPrefixBuffer[MAX_PATH], szPattern[MAX_PATH*2], *t, *t2, c;
			const char *s;
			int nMatchCount = 0;

			t = VDFileSplitPath(szFile);
			t2 = VDFileSplitExt(t);

			if (!stricmp(t2, ".avi")) {
				while(t2>t && isdigit((unsigned)t2[-1]))
					--t2;

				if (t2>t && t2[-1]=='.')
					strcpy(t2, "avi");
			}

			strcpy(szPrefixBuffer, szFile);
			VDFileSplitExt(szPrefixBuffer)[0] = 0;

			s = VDFileSplitPath(szPrefixBuffer);
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
					,VDFileSplitPath(szPrefixBuffer)))
					return;

				h = FindFirstFile(szPrefixBuffer, &wfd);
				if (h != INVALID_HANDLE_VALUE) {
					strcat(szPattern, ".%d.av%c");

					t = VDFileSplitPath(szPrefixBuffer);

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

		if (!!optVals[0]) {
			JobAddConfiguration(&g_dubOpts, g_szInputAVIFile, NULL, fname.c_str(), true, &inputAVI->listFiles, optVals[3], optVals[1] ? optVals[2] : 0);
		} else {
			SaveSegmentedAVI(fname.c_str(), false, NULL, optVals[3], optVals[1] ? optVals[2] : 0);
		}
	}
}

///////////////////////////////////////////////////////////////////////////

void PreviewAVI(HWND hWnd, DubOptions *quick_options, int iPriority, bool fProp) {
	if (!inputVideoAVI)
		throw MyError("No input video stream to process.");

	bool bPreview = g_dubOpts.audio.enabled;
	g_dubOpts.audio.enabled = TRUE;

	VDAVIOutputPreviewSystem outpreview;

	g_project->RunOperation(&outpreview, FALSE, quick_options, iPriority, fProp, 0, 0);

	g_dubOpts.audio.enabled = bPreview;
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

	VDFastMemcpyAutodetect();
}

/////////////////////////////

class VDSaveImageSeqDialogW32 : public VDDialogBaseW32 {
public:
	VDSaveImageSeqDialogW32();
	~VDSaveImageSeqDialogW32();

	BOOL DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	void UpdateFilenames();

	char szPrefix[MAX_PATH];
	char szPostfix[MAX_PATH];
	char szDirectory[MAX_PATH];
	char szFormat[MAX_PATH];
	int digits;
	long lFirstFrame, lLastFrame;
	bool bSaveAsTGA;
	bool bRunAsJob;
};

VDSaveImageSeqDialogW32::VDSaveImageSeqDialogW32()
	: VDDialogBaseW32(IDD_AVIOUTPUTIMAGES_FORMAT)
	, digits(0)
	, lFirstFrame(0)
	, lLastFrame(0)
{
	szPrefix[0] = 0;
	szPostfix[0] = 0;
	szDirectory[0] = 0;
	szFormat[0] = 0;
}
VDSaveImageSeqDialogW32::~VDSaveImageSeqDialogW32() {}

void VDSaveImageSeqDialogW32::UpdateFilenames() {
	char buf[512], *s;

	strcpy(szFormat, szDirectory);

	s = szFormat;
	while(*s) ++s;
	if (s>szFormat && s[-1]!=':' && s[-1]!='\\')
		*s++ = '\\';

	strcpy(s, szPrefix);
	while(*s) ++s;

	char *pCombinedPrefixPt = s;

	*s++ = '%';
	*s++ = '0';
	*s++ = '*';
	*s++ = 'l';
	*s++ = 'd';

	strcpy(s, szPostfix);

	sprintf(buf, szFormat, digits, lFirstFrame);
	SetDlgItemText(mhdlg, IDC_STATIC_FIRSTFRAMENAME, buf);
	sprintf(buf, szFormat, digits, lLastFrame);
	SetDlgItemText(mhdlg, IDC_STATIC_LASTFRAMENAME, buf);

	*pCombinedPrefixPt = 0;
}

BOOL VDSaveImageSeqDialogW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
	UINT uiTemp;
	BOOL fSuccess;

	switch(message) {
	case WM_INITDIALOG:
		SetDlgItemText(mhdlg, IDC_FILENAME_PREFIX, szPrefix);
		SetDlgItemText(mhdlg, IDC_FILENAME_SUFFIX, szPostfix);
		SetDlgItemInt(mhdlg, IDC_FILENAME_DIGITS, digits, FALSE);
		SetDlgItemText(mhdlg, IDC_DIRECTORY, szDirectory);
		CheckDlgButton(mhdlg, bSaveAsTGA ? IDC_FORMAT_TGA : IDC_FORMAT_BMP, BST_CHECKED);
		UpdateFilenames();

		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {

		case IDC_FILENAME_PREFIX:
			if (HIWORD(wParam) != EN_CHANGE) break;
			SendMessage((HWND)lParam, WM_GETTEXT, sizeof szPrefix, (LPARAM)szPrefix);
			UpdateFilenames();
			return TRUE;

		case IDC_FILENAME_SUFFIX:
			if (HIWORD(wParam) != EN_CHANGE) break;
			SendMessage((HWND)lParam, WM_GETTEXT, sizeof szPostfix, (LPARAM)szPostfix);
			UpdateFilenames();
			return TRUE;

		case IDC_FILENAME_DIGITS:
			if (HIWORD(wParam) != EN_CHANGE) break;
			uiTemp = GetDlgItemInt(mhdlg, IDC_FILENAME_DIGITS, &fSuccess, FALSE);
			if (fSuccess) {
				digits = uiTemp;

				if (digits > 15)
					digits = 15;

				UpdateFilenames();
			}
			return TRUE;

		case IDC_DIRECTORY:
			if (HIWORD(wParam) != EN_CHANGE) break;
			SendMessage((HWND)lParam, WM_GETTEXT, sizeof szDirectory, (LPARAM)szDirectory);
			UpdateFilenames();
			return TRUE;

		case IDC_SELECT_DIR:
			{
				const VDStringW dir(VDGetDirectory(kFileDialog_ImageDst, (VDGUIHandle)mhdlg, L"Select a directory for saved images"));

				if (!dir.empty())
					SetDlgItemText(mhdlg, IDC_DIRECTORY, VDTextWToA(dir).c_str());
			}
			return TRUE;

		case IDC_FORMAT_TGA:
			if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
				bSaveAsTGA = true;
				if (!stricmp(szPostfix, ".bmp")) {
					SetDlgItemText(mhdlg, IDC_FILENAME_SUFFIX, ".tga");
				}
			}
			return TRUE;

		case IDC_FORMAT_BMP:
			if (SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
				bSaveAsTGA = false;
				if (!stricmp(szPostfix, ".tga")) {
					SetDlgItemText(mhdlg, IDC_FILENAME_SUFFIX, ".bmp");
				}
			}
			return TRUE;

		case IDC_ADD_AS_JOB:
			bRunAsJob = !!SendMessage((HWND)lParam, BM_GETCHECK, 0, 0);
			return TRUE;

		case IDOK:
			End(TRUE);
			return TRUE;

		case IDCANCEL:
			End(FALSE);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

void SaveImageSeq(HWND hwnd) {
	VDSaveImageSeqDialogW32 dlg;

	if (!inputVideoAVI) {
		MessageBox(hwnd, "No input video stream to process.", g_szError, MB_OK);
		return;
	}

	dlg.bSaveAsTGA = true;
	strcpy(dlg.szPostfix,".tga");
	dlg.lFirstFrame	= 0;
	dlg.lLastFrame	= inputSubset->getTotalFrames()-1;
	dlg.bRunAsJob = false;

	if (dlg.ActivateDialog((VDGUIHandle)hwnd)) {
		if (dlg.bRunAsJob)
			JobAddConfigurationImages(&g_dubOpts, g_szInputAVIFile, NULL, VDTextAToW(dlg.szFormat).c_str(), VDTextAToW(dlg.szPostfix).c_str(), dlg.digits, dlg.bSaveAsTGA, &inputAVI->listFiles);
		else
			SaveImageSequence(VDTextAToW(dlg.szFormat).c_str(), VDTextAToW(dlg.szPostfix).c_str(), dlg.digits, false, NULL, dlg.bSaveAsTGA);
	}
}

/////////////////////////////////////////////////////////////////////////////////

void SaveConfiguration(HWND hWnd) {
	static const VDFileDialogOption sOptions[]={
		{ VDFileDialogOption::kBool, 0, L"Include selection and edit list", 0, 0 },
		{0}
	};

	VDRegistryAppKey key(g_szRegKeyPersistence);
	int optVals[1]={
		key.getBool(g_szRegKeySaveSelectionAndEditList, false),
	};

	const VDStringW filename(VDGetSaveFileName(kFileDialog_Config, (VDGUIHandle)hWnd, L"Save Configuration", fileFiltersSaveConfig, L"vcf", sOptions, optVals));

	if (!filename.empty()) {
		key.setBool(g_szRegKeySaveSelectionAndEditList, !!optVals[0]);

		FILE *f = NULL;
		try {
			f = fopen(VDTextWToA(filename).c_str(), "w");

			if (!f)
				throw MyError("Cannot open output file: %s.", strerror(errno));

			JobWriteConfiguration(f, &g_dubOpts, !!optVals[0]);

			fclose(f);
			f = NULL;
		} catch(const MyError& e) {
			e.post(NULL, g_szError);
		}

		if (f)
			fclose(f);
	}
}
