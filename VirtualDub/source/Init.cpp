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

#include "stdafx.h"

#include <windows.h>
#include <commctrl.h>
#include <vfw.h>
#include <shellapi.h>

#include "resource.h"
#include "job.h"
#include "oshelper.h"
#include "prefs.h"
#include "auxdlg.h"
#include <vd2/system/error.h>
#include "gui.h"
#include "filters.h"
#include "command.h"
#include "ddrawsup.h"
#include "script.h"
#include <vd2/system/tls.h>
#include <vd2/system/profile.h>
#include <vd2/system/registry.h>
#include <vd2/Dita/resources.h>
#include "crash.h"

#include "ClippingControl.h"
#include "PositionControl.h"
#include "LevelControl.h"
#include "HexViewer.h"
#include "FilterGraph.h"
#include "LogWindow.h"
#include "AudioDisplay.h"
#include "VideoDisplay.h"
#include "RTProfileDisplay.h"
#include "MRUList.h"

///////////////////////////////////////////////////////////////////////////

extern void InitBuiltinFilters();
extern void VDInitBuiltinAudioFilters();
extern void VDInitAppStringTables();

///////////////////////////////////////////////////////////////////////////

extern LONG __stdcall CrashHandler(struct _EXCEPTION_POINTERS *ExceptionInfo);
extern void FreeCompressor(COMPVARS *pCompVars);
extern LONG APIENTRY MainWndProc( HWND hWnd, UINT message, UINT wParam, LONG lParam);
extern void DetectDivX();

bool InitApplication(HINSTANCE hInstance);
bool InitInstance( HANDLE hInstance, int nCmdShow);
void ParseCommandLine(char *lpCmdLine);

///////////////////////////////////////////////////////////////////////////

static BOOL compInstalled;	// yuck

extern "C" unsigned long version_num;

extern HINSTANCE	g_hInst;
extern HWND			g_hWnd;
extern HMENU		hMenuNormal, hMenuDub, g_hmenuDisplay;
extern HACCEL		g_hAccelMain;
extern MRUList		*mru_list;

extern char g_szFile[MAX_PATH];

static const char szAppName[]="VirtualDub";
extern const char g_szError[];

bool g_fWine = false;

///////////////////////////////////////////////////////////////////////////

#if 0

void crash() {
	__try {
		__asm xor ebx,ebx
		__asm mov eax,dword ptr [ebx]
		__asm mov dword ptr [ebx],eax
		__asm lock add dword ptr cs:[00000000h], 12345678h
		__asm movq xmm0, qword ptr [eax]
		__asm {
__emit 0x66
__emit 0x0f
__emit 0x6f
__emit 0x2d
__emit 0xf0
__emit 0x42
__emit 0x0e
__emit 0x10
		}
	} __except(CrashHandler((EXCEPTION_POINTERS*)_exception_info())) {
	}
}
#else

static void crash3() {
//	vdprotected("doing foo") {
		for(int i=0; i<10; ++i) {
		   vdprotected1("in foo iteration %d", int, i)
//			volatile VDProtectedAutoScope1<int> autoscope(VDProtectedAutoScopeData1<int>(__FILE__, __LINE__, "in foo iteration %d", i));
			
			   if (i == 5)
				   *(volatile char *)0 = 0;
		}
//	}
}

static void crash2() {
	__try {
		crash3();
#ifndef __INTEL_COMPILER		// figures... Intel C/C++ doesn't support 3DNow! opcodes.
		__asm pavgusb mm0, mm0
#endif
		__asm add byte ptr ds:[0], 0
	} __except(CrashHandler((EXCEPTION_POINTERS*)_exception_info())) {
	}
}

static void crash() {
	vdprotected("deliberately trying to crash")
		crash2();
}

#endif

extern "C" void WinMainCRTStartup();

extern "C" void __declspec(naked) __stdcall VeedubWinMain() {
#if defined(__INTEL_COMPILER)
	static const char g_szSSE2Error[]="This build of VirtualDub is optimized for the Pentium 4 processor and will not run on "
										"CPUs without SSE2 support. Your CPU does not appear to support SSE2, so you will need to run "
										"the regular VirtualDub build, which runs on all Pentium and higher CPUs.\n\n"
										"Choose OK if you want to try running this version anyway -- it'll likely crash -- or CANCEL "
										"to exit."
										;

	__asm {
		// The Intel compiler could use P4 instructions anywhere, so let's do this right.
		// A little assembly never hurt anyone anyway....

		push	ebp
		push	edi
		push	esi
		push	ebx

		;check for CPUID
		pushfd
		pop		eax
		or		eax,00200000h
		push	eax
		popfd
		pushfd
		pop		eax
		test	eax,00200000h
		jz		failed

		;MMX, SSE, and SSE2 bits must be set
		mov		eax, 1
		cpuid
		mov		eax,06800000h
		and		edx,eax
		cmp		edx,eax
		jne		failed

		;test for operating system SSE2 support
		push	offset exchandler
		push	dword ptr fs:[0]
		mov		dword ptr fs:[0],esp	;hook SEH chain

		xor		eax,eax
		andps	xmm0,xmm0

		pop		ecx						;remove SEH record
		pop		dword ptr fs:[0]

		or		eax,eax					;eax is set if exception occurred
		jnz		failed
appstart:
		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		jmp		WinMainCRTStartup

failed:
		push	MB_OKCANCEL | MB_ICONERROR
		push	offset g_szError
		push	offset g_szSSE2Error
		push	0
		call	dword ptr [MessageBoxA]
		cmp		eax,IDOK
		je		appstart
		mov		eax,20
		jmp		dword ptr [ExitProcess]

exchandler:
		mov		eax,[esp+12]			;get exception context
		mov		[eax+176],1				;set EAX in crash zone
		add		dword ptr [eax+184],3	;bump past SSE2 instruction
		mov		eax,0					;ExceptionContinueExecution
		ret
	}

#else
	__asm jmp WinMainCRTStartup
#endif
}

bool Init(HINSTANCE hInstance, LPSTR lpCmdLine, int nCmdShow) {

//#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
//#endif

	// setup crash trap
	SetUnhandledExceptionFilter(CrashHandler);

	// initialize globals
    g_hInst = hInstance;

	// initialize TLS trace system
	VDSetThreadInitHook(VDThreadInitHandler);

	// initialize TLS for main thread
	VDInitThreadData("Main thread");

	// initialize resource system
	VDInitResourceSystem();
	VDInitAppStringTables();

	// announce startup
	VDLog(kVDLogInfo, VDswprintf(
			L"Starting up: VirtualDub build %lu/"
#ifdef DEBUG
			L"debug"
#elif defined(__INTEL_COMPILER)
			L"release-P4"
#else
			L"release"
#endif
			,1
			,&version_num));

	// prep system stuff

	VDCHECKPOINT;

	AVIFileInit();
	VDRegistryAppKey::setDefaultKey("Software\\Freeware\\VirtualDub\\");

	// initialize filters, job system, MRU list, help system

	InitBuiltinFilters();
	VDInitBuiltinAudioFilters();

	if (!InitJobSystem())
		return FALSE;

	if (!(mru_list = new MRUList(4, "MRU List"))) return false;

	LoadPreferences();

	// initialize interface

	VDCHECKPOINT;

    if (!InitApplication(hInstance))
            return (FALSE);              

	// display welcome requester

	Welcome();

    // Create the main window.

    if (!InitInstance(hInstance, nCmdShow))
        return (FALSE);

	DragAcceptFiles(g_hWnd, TRUE);

	// Autoload filters.

	VDCHECKPOINT;

	vdprotected("autoloading filters at startup") {
		int f, s;

		s = FilterAutoloadModules(f);

		if (s || f)
			guiSetStatus("Autoloaded %d filters (%d failed).", 255, s, f);
	}

	// Detect DivX.

	DetectDivX();

	// attempt to initialize DirectDraw 2, if we have it

#ifdef ENABLE_DIRECTDRAW_SUPPORT
	EnableMenuItem(GetMenu(g_hWnd),ID_OPTIONS_ENABLEDIRECTDRAW, (DDrawDetect() ? (MF_BYCOMMAND|MF_ENABLED) : (MF_BYCOMMAND|MF_GRAYED)));
#endif

	_RPT1(0,"[%s]\n",lpCmdLine);

	while(isspace(*lpCmdLine)) ++lpCmdLine;

	if (*lpCmdLine=='&') {
		ICRemove(ICTYPE_VIDEO, 'TSDV', 0);
		compInstalled = ICInstall(ICTYPE_VIDEO, 'TSDV', (LPARAM)(lpCmdLine+1), 0, ICINSTALL_DRIVER);

		if (!compInstalled)
//			MessageBox(NULL, "Warning: Unable to load compressor.", szError, MB_OK);
			MyICError("External compressor", compInstalled).post(NULL, g_szError);
		else
			MessageBox(NULL, "External compressor loaded.", "Cool!", MB_OK);
	} else if (*lpCmdLine == ':') {
		if (lpCmdLine[1] && lpCmdLine[2] && lpCmdLine[3] && lpCmdLine[4]) {
			DWORD fccHandler = *(DWORD *)(lpCmdLine+1);
			HMODULE hmodVC = LoadLibrary(lpCmdLine+5);

			if (hmodVC) {
				DWORD pEntry = (DWORD)GetProcAddress(hmodVC, "DriverProc");

				if (pEntry) {
					BOOL b = ICInstall(ICTYPE_VIDEO, fccHandler, (LPARAM)pEntry, 0, ICINSTALL_FUNCTION);

					if (b)
						MessageBox(NULL, "External compressor loaded as function.", "Cool!", MB_OK);
				}
			}
		}
	} else if (*lpCmdLine == '!') {
		try {
			FilterLoadModule(lpCmdLine+1);

			guiSetStatus("Loaded external filter module: %s", 255, lpCmdLine+1);
		} catch(const MyError& e) {
			e.post(g_hWnd, g_szError);
		}
	} else
		ParseCommandLine(lpCmdLine);

	// All done!

	VDCHECKPOINT;

	return true;
}

///////////////////////////////////////////////////////////////////////////

void Deinit() {
	FilterInstance *fa;

	VDCHECKPOINT;

	DragAcceptFiles(g_hWnd, FALSE);

	filters.DeinitFilters();

	VDCHECKPOINT;

	while(fa = (FilterInstance *)g_listFA.RemoveHead()) {
		fa->Destroy();
	}

	VDCHECKPOINT;

	FilterUnloadAllModules();

	VDCHECKPOINT;

	CloseAVI();
	CloseWAV();

	VDCHECKPOINT;

	CloseJobWindow();
	DeinitJobSystem();

	VDCHECKPOINT;

	if (g_Vcompression.dwFlags & ICMF_COMPVARS_VALID)
		FreeCompressor(&g_Vcompression);

	if (compInstalled)
		ICRemove(ICTYPE_VIDEO, 'TSDV', 0);


	// deinitialize DirectDraw2

	VDCHECKPOINT;
	DDrawDeinitialize();

	AVIFileExit();

	delete mru_list;

	_CrtCheckMemory();

	VDCHECKPOINT;

	VDDeinitResourceSystem();
	VDDeinitProfilingSystem();

	VDCHECKPOINT;
}

///////////////////////////////////////////////////////////////////////////

bool InitApplication(HINSTANCE hInstance) {
    WNDCLASS  wc;

	// register controls

	InitCommonControls();

	if (!RegisterClippingControl()) return false;
	if (!RegisterPositionControl()) return false;
	if (!RegisterLevelControl()) return false;
	if (!RegisterHexEditor()) return false;
	if (!RegisterAudioDisplayControl()) return false;
	if (!RegisterVideoDisplayControl()) return false;
	if (!RegisterFilterGraphControl()) return false;
	if (!RegisterLogWindowControl()) return false;
	if (!RegisterRTProfileDisplayControl()) return false;

	// Load menus.

	if (!(hMenuNormal	= LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_MAIN_MENU    )))) return false;
	if (!(hMenuDub		= LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_DUB_MENU     )))) return false;
	if (!(g_hmenuDisplay= LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_DISPLAY_MENU )))) return false;

	// Load accelerators.

	if (!(g_hAccelMain = LoadAccelerators(g_hInst, MAKEINTRESOURCE(IDR_IDLE_KEYS)))) return false;

    wc.style = CS_OWNDC;
    wc.lpfnWndProc = (WNDPROC)MainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_VIRTUALDUB));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE+1); //GetStockObject(LTGRAY_BRUSH); 
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MAIN_MENU);
    wc.lpszClassName = szAppName;

    return !!RegisterClass(&wc);
}

///////////////////////////////////////////////////////////////////////////

bool InitInstance( HANDLE hInstance, int nCmdShow) {
	char buf[256];
	char buf2[256];

	LoadString(g_hInst, IDS_TITLE_INITIAL, buf2, sizeof buf2);

	wsprintf(buf, buf2, version_num,
#ifdef _DEBUG
		"debug"
#elif defined(__INTEL_COMPILER)
		"release-P4"
#else
		"release"
#endif
		);

    // Create a main window for this application instance. 
    g_hWnd = CreateWindow(
        szAppName,
        "",
        WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,            // Window style.
        CW_USEDEFAULT,                  // Default horizontal position.
        CW_USEDEFAULT,                  // Default vertical position.
        CW_USEDEFAULT,                  // Default width.
        CW_USEDEFAULT,                  // Default height.
        NULL,                           // Overlapped windows have no parent.
        NULL,                           // Use the window class menu.
        g_hInst,                        // This instance owns this window.
        NULL                            // Pointer not needed.
    );

    // If window could not be created, return "failure".
    if (!g_hWnd)
        return (FALSE);

    // Make the window visible; update its client area; and return "success".
    ShowWindow(g_hWnd, nCmdShow);  
    UpdateWindow(g_hWnd);          

	SendMessage(g_hWnd, WM_SETTEXT, 0, (LPARAM)buf);

    return (TRUE);               

}

///////////////////////////////////////////////////////////////////////////

void ParseCommandLine(char *lpCmdLine) {
	char *const cmdline = strdup(lpCmdLine);
	if (!cmdline) return;

	char *token, *s;
	static const char *seps = " \t\n\r";
	bool fExitOnDone = false;

	// parse cmdline looking for switches
	//
	//	/s						run script
	//	/c						clear job list
	//	/b<srcdir>,<dstdir>		add directory batch process to job list
	//	/r						run job list
	//	/x						exit when jobs complete
	//	/h						disable crash handler

	s = cmdline;
	g_szFile[0] = 0;

	try {
		while(*s) {
			char *t;
			bool quoted = false;
			bool restore_slash = false;

			while(isspace(*s))
				++s;

			if (!*s) break;

			token = s;

			if (*s == '"') {
				s = ++token;

				while(*s && *s!='"')
					++s;

				if (*s)
					*s++=0;

			} else {
				if (*s == '/')
					++s;

				while(*s && (quoted || (!isspace(*s) && *s!='/'))) {
					if (*s == '"')
						quoted = !quoted;

					++s;
				}

				if (*s) {
					restore_slash = (*s=='/');
					*s++ = 0;
				}
			}

			_RPT1(0,"token [%s]\n", token);

			if (*token == '-' || *token == '/') {
				switch(token[1]) {
				case 's':

					t = token + 2;
					if (*t == '"') {
						++t;
						while(*t && *t != '"')
							++t;
						*t = 0;

						t = token+3;
					}

					JobLockDubber();
					RunScript(t);
					JobUnlockDubber();
					break;
				case 'c':
					JobClearList();
					break;
				case 'r':
					JobRunList();
					break;
				case 'x':
					fExitOnDone = true;
					break;
				case 'h':
					SetUnhandledExceptionFilter(NULL);
					break;
				case 'p':
					{
						char *arg1, *arg2;

						// dequote first token

						t = token+2;

						if (*t == '"') {
							arg1 = ++t;
							while(*t && *t!='"')
								++t;

							if (*t)
								*t++ = 0;
						} else {
							arg1 = t;
							while(*t && *t!=',')
								++t;
						}

						if (*t++ != ',')
							throw "Command line error: /p format is /p<src_file>,<dst_file>";

						// dequote second token

						arg2 = t;

						if (*t == '"') {
							arg2 = ++t;

							while(*t && *t!='"')
								++t;

							if (*t)
								*t++ = 0;
						} else {
							while(*t && *t!=',')
								++t;
						}

						if (!*arg2)
							throw "Command line error: /p format is /p<src_file>,<dst_file>";

						JobAddBatchFile(arg1, arg2);
					}
					break;
				case 'b':
					{
						char *arg1, *arg2;

						// dequote first token

						t = token+2;

						if (*t == '"') {
							arg1 = ++t;
							while(*t && *t!='"')
								++t;

							if (*t)
								*t++ = 0;
						} else {
							arg1 = t;
							while(*t && *t!=',')
								++t;
						}

						if (*t++ != ',')
							throw "Command line error: /b format is /b<src_dir>,<dst_dir>";

						// dequote second token

						arg2 = t;

						if (*t == '"') {
							arg2 = ++t;

							while(*t && *t!='"')
								++t;

							if (*t)
								*t++ = 0;
						} else {
							while(*t && *t!=',')
								++t;
						}

						if (!*arg2)
							throw "Command line error: /b format is /b<src_dir>,<dst_dir>";

						JobAddBatchDirectory(arg1, arg2);
					}
					break;
				case 'w':
					g_fWine = true;
					break;

				case 'f':
					if (!stricmp(token+2, "sck"))
						crash();
					break;

				}
			} else
				strcpy(g_szFile, token);

			if (restore_slash)
				*--s='/';
		}
	} catch(char *s) {
		MessageBox(NULL, s, g_szError, MB_OK);
	}

	free(cmdline);

	if (fExitOnDone) {
		Deinit();
		ExitProcess(0);
	}
}
