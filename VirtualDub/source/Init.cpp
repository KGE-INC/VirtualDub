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
#include <eh.h>

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
#include <vd2/system/vdalloc.h>
#include <vd2/system/tls.h>
#include <vd2/system/profile.h>
#include <vd2/system/registry.h>
#include <vd2/system/filesys.h>
#include <vd2/Dita/resources.h>
#include "crash.h"

#include "ClippingControl.h"
#include "PositionControl.h"
#include "LevelControl.h"
#include "HexViewer.h"
#include "FilterGraph.h"
#include "LogWindow.h"
#include "VideoWindow.h"
#include "AudioDisplay.h"
#include "VideoDisplay.h"
#include "RTProfileDisplay.h"
#include "plugins.h"

#include "project.h"
#include "projectui.h"

///////////////////////////////////////////////////////////////////////////

extern void InitBuiltinFilters();
extern void VDInitBuiltinAudioFilters();
extern void VDInitAppStringTables();
extern void VDInitInputDrivers();

///////////////////////////////////////////////////////////////////////////

extern LONG __stdcall CrashHandler(struct _EXCEPTION_POINTERS *ExceptionInfo);
extern void FreeCompressor(COMPVARS *pCompVars);
extern LONG APIENTRY MainWndProc( HWND hWnd, UINT message, UINT wParam, LONG lParam);
extern void DetectDivX();

bool InitApplication(HINSTANCE hInstance);
bool InitInstance( HANDLE hInstance, int nCmdShow);
void ParseCommandLine(const wchar_t *lpCmdLine);

///////////////////////////////////////////////////////////////////////////

static BOOL compInstalled;	// yuck

extern "C" unsigned long version_num;

extern HINSTANCE	g_hInst;
extern HWND			g_hWnd;
extern HACCEL		g_hAccelMain;

extern VDProject *g_project;
extern vdautoptr<VDProjectUI> g_projectui;

extern wchar_t g_szFile[MAX_PATH];

static const char szAppName[]="VirtualDub";
static const wchar_t szAppNameW[]=L"VirtualDub";
extern const char g_szError[];

bool g_fWine = false;

///////////////////////////////////////////////////////////////////////////

void VDterminate() {
	vdprotected("processing call to terminate() (probably caused by exception within destructor)") {
		__asm int 3
	}
}

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

bool Init(HINSTANCE hInstance, LPCWSTR lpCmdLine, int nCmdShow) {

//#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
//#endif

	// setup crash traps
	SetUnhandledExceptionFilter(CrashHandler);
	set_terminate(VDterminate);

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
	VDInitInputDrivers();

	if (!InitJobSystem())
		return FALSE;

	LoadPreferences();

	// initialize interface

	VDCHECKPOINT;

    if (!InitApplication(hInstance))
            return (FALSE);              

	// display welcome requester
	Welcome();

	// Announce experimentality.
	AnnounceExperimental();

    // Create the main window.

    if (!InitInstance(hInstance, nCmdShow))
        return (FALSE);

	DragAcceptFiles(g_hWnd, TRUE);

	// Autoload filters.

	VDCHECKPOINT;

	vdprotected("autoloading filters at startup") {
		int f, s;

		VDLoadPlugins(VDMakePath(VDGetProgramPath(), VDStringW(L"plugins")));

		s = FilterAutoloadModules(f);

		if (s || f)
			guiSetStatus("Autoloaded %d filters (%d failed).", 255, s, f);
	}

	// Detect DivX.

	DetectDivX();

	while(iswspace(*lpCmdLine))
		++lpCmdLine;

#if 0
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
	} else
#endif
	ParseCommandLine(lpCmdLine);

	// All done!

	VDCHECKPOINT;

	return true;
}

///////////////////////////////////////////////////////////////////////////

void Deinit() {
	FilterInstance *fa;

	VDCHECKPOINT;

	g_projectui->Detach();
	g_projectui = 0;
	g_project = 0;
	DragAcceptFiles(g_hWnd, FALSE);

	filters.DeinitFilters();

	VDCHECKPOINT;

	while(fa = (FilterInstance *)g_listFA.RemoveHead()) {
		fa->Destroy();
	}

	VDCHECKPOINT;

	FilterUnloadAllModules();
	VDDeinitPluginSystem();

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

	_CrtCheckMemory();

	VDCHECKPOINT;

	VDDeinitResourceSystem();
	VDDeinitProfilingSystem();

	VDCHECKPOINT;
}

///////////////////////////////////////////////////////////////////////////

LRESULT APIENTRY DummyWndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return IsWindowUnicode(hWnd) ? DefWindowProcW(hWnd, message, wParam, lParam) : DefWindowProcA(hWnd, message, wParam, lParam);
}

bool InitApplication(HINSTANCE hInstance) {
	// register controls

	InitCommonControls();

	if (!RegisterClippingControl()) return false;
	if (!RegisterPositionControl()) return false;
	if (!RegisterLevelControl()) return false;
	if (!RegisterHexEditor()) return false;
	if (!RegisterAudioDisplayControl()) return false;
	if (!VDRegisterVideoDisplayControl()) return false;
	if (!RegisterFilterGraphControl()) return false;
	if (!RegisterLogWindowControl()) return false;
	if (!RegisterRTProfileDisplayControl()) return false;
	if (!RegisterVideoWindow()) return false;

	// Load accelerators.

	if (!(g_hAccelMain = LoadAccelerators(g_hInst, MAKEINTRESOURCE(IDR_IDLE_KEYS)))) return false;

	union {
		WNDCLASSA a;
		WNDCLASSW w;
	} wc;

    wc.a.style			= CS_OWNDC;
    wc.a.lpfnWndProc	= DummyWndProc;
    wc.a.cbClsExtra		= 0;
    wc.a.cbWndExtra		= 0;
    wc.a.hInstance		= hInstance;
    wc.a.hIcon			= LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_VIRTUALDUB));
    wc.a.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wc.a.hbrBackground	= (HBRUSH)(COLOR_3DFACE+1); //GetStockObject(LTGRAY_BRUSH); 

	if (GetVersion() < 0x80000000) {
	    wc.w.lpszMenuName	= MAKEINTRESOURCEW(IDR_MAIN_MENU);
		wc.w.lpszClassName	= szAppNameW;

		return !!RegisterClassW(&wc.w);
	} else {
	    wc.a.lpszMenuName	= MAKEINTRESOURCEA(IDR_MAIN_MENU);
		wc.a.lpszClassName	= szAppName;

		return !!RegisterClassA(&wc.a);
	}
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
	if (GetVersion() < 0x80000000) {
		g_hWnd = CreateWindowW(
			szAppNameW,
			L"",
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
	} else {
		g_hWnd = CreateWindowA(
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
	}

    // If window could not be created, return "failure".
    if (!g_hWnd)
        return (FALSE);

	g_projectui = new VDProjectUI;
	g_project = &*g_projectui;
	g_projectui->Attach(g_hWnd);

    // Make the window visible; update its client area; and return "success".
    ShowWindow(g_hWnd, nCmdShow);  
    UpdateWindow(g_hWnd);          

	SetWindowText(g_hWnd, buf);

    return (TRUE);               

}

///////////////////////////////////////////////////////////////////////////

void ParseCommandLine(const wchar_t *lpCmdLine) {

	// skip program name
	if (*lpCmdLine == L'"') {
		++lpCmdLine;
		while(*lpCmdLine && *lpCmdLine++ != '"')
			;
	} else
		while(*lpCmdLine && !iswspace(*lpCmdLine))
			++lpCmdLine;

	wchar_t *const cmdline = wcsdup(lpCmdLine);
	if (!cmdline) return;

	wchar_t *token, *s;
	static const wchar_t seps[] = L" \t\n\r";
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
			wchar_t *t;
			bool quoted = false;
			bool restore_slash = false;

			while(isspace(*s))
				++s;

			if (!*s) break;

			token = s;

			if (*s == L'"') {
				s = ++token;

				while(*s && *s!=L'"')
					++s;

				if (*s)
					*s++=0;

			} else {
				if (*s == L'/')
					++s;

				while(*s && (quoted || (!isspace(*s) && *s!=L'/'))) {
					if (*s == L'"')
						quoted = !quoted;

					++s;
				}

				if (*s) {
					restore_slash = (*s==L'/');
					*s++ = 0;
				}
			}

			_RPT1(0,"token [%s]\n", token);
			if (*token == L'-' || *token == L'/') {
				switch(token[1]) {
				case 's':

					t = token + 2;
					if (*t == L'"') {
						++t;
						while(*t && *t != L'"')
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
						const wchar_t *arg1, *arg2;

						// dequote first token

						t = token+2;

						if (*t == L'"') {
							arg1 = ++t;
							while(*t && *t!=L'"')
								++t;

							if (*t)
								*t++ = 0;
						} else {
							arg1 = t;
							while(*t && *t!=L',')
								++t;
						}

						if (*t++ != ',')
							throw "Command line error: /p format is /p<src_file>,<dst_file>";

						// dequote second token

						arg2 = t;

						if (*t == L'"') {
							arg2 = ++t;

							while(*t && *t!=L'"')
								++t;

							if (*t)
								*t++ = 0;
						} else {
							while(*t && *t!=L',')
								++t;
						}

						if (!*arg2)
							throw "Command line error: /p format is /p<src_file>,<dst_file>";

						JobAddBatchFile(arg1, arg2);
					}
					break;
				case 'b':
					{
						wchar_t *arg1, *arg2;

						// dequote first token

						t = token+2;

						if (*t == L'"') {
							arg1 = ++t;
							while(*t && *t!=L'"')
								++t;

							if (*t)
								*t++ = 0;
						} else {
							arg1 = t;
							while(*t && *t!=L',')
								++t;
						}

						if (*t++ != L',')
							throw "Command line error: /b format is /b<src_dir>,<dst_dir>";

						// dequote second token

						arg2 = t;

						if (*t == L'"') {
							arg2 = ++t;

							while(*t && *t!=L'"')
								++t;

							if (*t)
								*t++ = 0;
						} else {
							while(*t && *t!=L',')
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
					if (!wcsicmp(token+2, L"sck"))
						crash();
					break;

				case 'F':
					try {
						const VDStringA filenameA(VDTextWToA(token+2));
						FilterLoadModule(filenameA.c_str());

						guiSetStatus("Loaded external filter module: %s", 255, filenameA.c_str());
					} catch(const MyError& e) {
						e.post(g_hWnd, g_szError);
					}
					break;
				}
			} else
				wcscpy(g_szFile, token);

			if (restore_slash)
				*--s=L'/';
		}
	} catch(const char *s) {
		MessageBox(NULL, s, g_szError, MB_OK);
	}

	free(cmdline);

	if (fExitOnDone) {
		Deinit();
		ExitProcess(0);
	}
}
