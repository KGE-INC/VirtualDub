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
#include <vd2/system/thread.h>
#include <vd2/system/profile.h>
#include <vd2/system/registry.h>
#include <vd2/system/filesys.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/VDString.h>
#include <vd2/Dita/resources.h>
#include "crash.h"
#include "DubSource.h"

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
#include "captureui.h"

///////////////////////////////////////////////////////////////////////////

extern void InitBuiltinFilters();
extern void VDInitBuiltinAudioFilters();
extern void VDInitInputDrivers();
extern void VDInitExternalCallTrap();
extern void VDInitVideoCodecBugTrap();

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

extern VDProject *g_project;
vdrefptr<VDProjectUI> g_projectui;

extern vdrefptr<IVDCaptureProjectUI> g_capProjectUI;

extern DubSource::ErrorMode	g_videoErrorMode;
extern DubSource::ErrorMode	g_audioErrorMode;

extern wchar_t g_szFile[MAX_PATH];

extern const char g_szError[];

bool g_fWine = false;
bool g_bEnableVTuneProfiling;

void (*g_pPostInitRoutine)();

///////////////////////////////////////////////////////////////////////////

namespace {
	typedef std::list<VDStringA> tArguments;

	tArguments g_VDStartupArguments;
}

const char *VDGetStartupArgument(int index) {
	tArguments::const_iterator it(g_VDStartupArguments.begin()), itEnd(g_VDStartupArguments.end());

	for(; it!=itEnd && index; ++it, --index)
		;

	if (it == itEnd)
		return NULL;

	return (*it).c_str();
}

void VDterminate() {
	vdprotected("processing call to terminate() (probably caused by exception within destructor)") {
#if _MSC_VER >= 1300
		__debugbreak();
#else
		__asm int 3
#endif
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
	} __except(CrashHandler((EXCEPTION_POINTERS*)_exception_info())) {
	}
}

static void crash() {
	vdprotected("deliberately trying to crash")
		crash2();
}

#endif

extern "C" void WinMainCRTStartup();

#if defined(__INTEL_COMPILER)
extern "C" void __declspec(naked) __stdcall VeedubWinMain() {
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
}
#endif

void VDInitAppResources() {
	HRSRC hResource = FindResource(NULL, MAKEINTRESOURCE(IDR_RESOURCES), "STUFF");

	if (!hResource)
		return;

	HGLOBAL hGlobal = LoadResource(NULL, hResource);
	if (!hGlobal)
		return;

	LPVOID lpData = LockResource(hGlobal);
	if (!lpData)
		return;

	VDLoadResources(0, lpData, SizeofResource(NULL, hResource));
}

bool Init(HINSTANCE hInstance, int nCmdShow) {

//#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);
//#endif

	VDSetThreadDebugName(GetCurrentThreadId(), "Main");

	// setup crash traps
	SetUnhandledExceptionFilter(CrashHandler);
	set_terminate(VDterminate);

	VDInitExternalCallTrap();
	VDInitVideoCodecBugTrap();

	// initialize globals
    g_hInst = hInstance;

	// initialize TLS trace system
	VDSetThreadInitHook(VDThreadInitHandler);

	// initialize TLS for main thread
	VDInitThreadData("Main thread");

	// initialize resource system
	VDInitResourceSystem();
	VDInitAppResources();

	// announce startup
	VDLog(kVDLogInfo, VDswprintf(
			L"Starting up: VirtualDub build %lu/"
#ifdef DEBUG
			L"debug"
#elif defined(_M_AMD64)
			L"release-AMD64"
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
	{
		VDRegistryAppKey key("Preferences");
		unsigned errorMode;

		errorMode = key.getInt("Edit: Video error mode");
		if (errorMode < DubSource::kErrorModeCount)
			g_videoErrorMode = (DubSource::ErrorMode)errorMode;

		errorMode = key.getInt("Edit: Audio error mode");
		if (errorMode < DubSource::kErrorModeCount)
			g_audioErrorMode = (DubSource::ErrorMode)errorMode;
	}


	// initialize interface

	VDCHECKPOINT;

    if (!InitApplication(hInstance))
            return (FALSE);              

	VDInstallModelessDialogHookW32();

	// display welcome requester
	Welcome();

	// Announce experimentality.
	AnnounceExperimental();

    // Create the main window.

    if (!InitInstance(hInstance, nCmdShow))
        return (FALSE);

	// Autoload filters.

	VDCHECKPOINT;

	vdprotected("autoloading filters at startup") {
		int f, s;

		VDLoadPlugins(VDMakePath(VDGetProgramPath().c_str(), L"plugins"), s, f);

		if (s || f)
			guiSetStatus("Autoloaded %d filters (%d failed).", 255, s, f);
	}

	// Detect DivX.

	DetectDivX();

	// All done!

	VDCHECKPOINT;

	if (g_pPostInitRoutine)
		g_pPostInitRoutine();

	return true;
}

///////////////////////////////////////////////////////////////////////////

void Deinit() {
	FilterInstance *fa;

	VDCHECKPOINT;

	g_project->CloseAVI();
	g_project->CloseWAV();

	g_projectui->Detach();
	g_projectui = NULL;
	g_project = NULL;

	VDUIFrame::DestroyAll();

	filters.DeinitFilters();

	VDCHECKPOINT;

	while(fa = (FilterInstance *)g_listFA.RemoveHead()) {
		fa->Destroy();
	}

	VDCHECKPOINT;

	VDDeinitPluginSystem();

	VDCHECKPOINT;

	CloseJobWindow();
	DeinitJobSystem();

	g_VDStartupArguments.clear();

	VDCHECKPOINT;

	if (g_ACompressionFormat)
		freemem(g_ACompressionFormat);

	if (g_Vcompression.dwFlags & ICMF_COMPVARS_VALID)
		FreeCompressor(&g_Vcompression);

	if (compInstalled)
		ICRemove(ICTYPE_VIDEO, 'TSDV', 0);

	VDDeinstallModelessDialogHookW32();

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

	extern bool VDRegisterUIFrameWindow();
	if (!VDRegisterUIFrameWindow()) return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////

bool InitInstance( HANDLE hInstance, int nCmdShow) {
	wchar_t buf[256];

	VDStringW versionFormat(VDLoadStringW32(IDS_TITLE_INITIAL));

	swprintf(buf, versionFormat.c_str(), version_num,
#ifdef _DEBUG
		L"debug"
#elif defined(_M_AMD64)
		L"release-AMD64"
#elif defined(__INTEL_COMPILER)
		L"release-P4"
#else
		L"release"
#endif
		);

    // Create a main window for this application instance. 
	if (GetVersion() < 0x80000000) {
		g_hWnd = CreateWindowW(
			(LPCWSTR)VDUIFrame::Class(),
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
			(LPCSTR)VDUIFrame::Class(),
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
	g_projectui->Attach((VDGUIHandle)g_hWnd);

    // Make the window visible; update its client area; and return "success".
    ShowWindow(g_hWnd, nCmdShow);  
    UpdateWindow(g_hWnd);          

	VDSetWindowTextW32(g_hWnd, buf);

    return (TRUE);               

}

///////////////////////////////////////////////////////////////////////////

namespace {
	bool ParseArgument(const wchar_t *&s, VDStringW& parm) {
		while(iswspace(*s))
			++s;

		if (!*s || *s == L'/')
			return false;

		const wchar_t *start;
		const wchar_t *end;

		parm.clear();

		start = s;
		while(*s && *s != L' ' && *s != L'/' && *s != L',') {			
			if (*s == L'"') {
				if (s != start)
					parm.append(start, s-start);

				start = ++s;
				while(*s && *s != L'"')
					++s;
				end = s;

				if (end != start)
					parm.append(start, end-start);

				if (*s)
					++s;

				start = s;
			} else
				++s;
		}

		if (s != start)
			parm.append(start, s-start);

		if (*s == L',')
			++s;

		return true;
	}

	bool CheckForSwitch(const wchar_t *&s, const wchar_t *token) {
		const size_t len = wcslen(token);
		if (wcsncmp(s, token, len))
			return false;

		const wchar_t c = s[len];

		if (c && c != L' ' && c != L'"' && c != L'/')
			return false;

		s += len;

		return true;
	}
}

int VDProcessCommandLine(const wchar_t *lpCmdLine) {
	const wchar_t *const cmdline = lpCmdLine;

	const wchar_t *s;
	static const wchar_t seps[] = L" \t\n\r";
	bool fExitOnDone = false;

	// parse cmdline looking for switches
	//
	//	/s						run script
	//	/i <script> <params...>	run script with parameters
	//	/c						clear job list
	//	/b<srcdir>,<dstdir>		add directory batch process to job list
	//	/r						run job list
	//	/x						exit when jobs complete
	//	/h						disable crash handler
	//	/fsck					test crash handler
	//	/vtprofile				enable VTune profiling

	s = cmdline;
	g_szFile[0] = 0;

	VDStringW token;

	VDStringW progName;
	ParseArgument(s, token);

	try {
		while(*s) {
			while(iswspace(*s))
				++s;

			if (!*s) break;

			if (*s == L'/') {
				++s;

				// parse out the switch name
				const wchar_t *switchStart = s;
				while(*s && iswalnum(*s))
					++s;

				token.assign(switchStart, s-switchStart);

				if (token == L"b") {
					VDStringW path2;

					if (!ParseArgument(s, token) || !ParseArgument(s, path2))
						throw "Command line error: syntax is /b <src_dir> <dst_dir>";

					JobAddBatchDirectory(token.c_str(), path2.c_str());
				}
				else if (token == L"c") {
					JobClearList();
				}
				else if (token == L"capture") {
					VDUIFrame *pFrame = VDUIFrame::GetFrame(g_hWnd);
					pFrame->SetNextMode(1);
				}
				else if (token == L"capchannel") {
					if (!g_capProjectUI)
						throw "Command line error: not in capture mode";

					if (!ParseArgument(s, token))
						throw "Command line error: syntax is /capchannel <channel>";

					g_capProjectUI->SetTunerChannel(_wtoi(token.c_str()));
				}
				else if (token == L"capdevice") {
					if (!g_capProjectUI)
						throw "Command line error: not in capture mode";

					if (!ParseArgument(s, token))
						throw "Command line error: syntax is /capdevice <device>";

					g_capProjectUI->SetDriver(token.c_str());
				}
				else if (token == L"capfile") {
					if (!g_capProjectUI)
						throw "Command line error: not in capture mode";

					if (!ParseArgument(s, token))
						throw "Command line error: syntax is /capfile <filename>";

					g_capProjectUI->SetCaptureFile(token.c_str());
				}
				else if (token == L"capstart") {
					if (!g_capProjectUI)
						throw "Command line error: not in capture mode";

					if (ParseArgument(s, token)) {
						int limit = 60*_wtoi(token.c_str());

						g_capProjectUI->SetTimeLimit(limit);
					}

					g_capProjectUI->Capture();
				}
				else if (token == L"fsck") {
					crash();
				}
				else if (token == L"F") {
					if (!ParseArgument(s, token))
						throw "Command line error: syntax is /F <filter>";

					VDAddPluginModule(token.c_str());

					guiSetStatus("Loaded external filter module: %s", 255, VDTextWToA(token).c_str());
					break;
				}
				else if (token == L"h") {
					SetUnhandledExceptionFilter(NULL);
				}
				else if (token == L"i") {
					VDStringW filename;

					if (!ParseArgument(s, filename))
						throw "Command line error: syntax is /i <script> [<args>...]";

					g_VDStartupArguments.clear();
					while(ParseArgument(s, token))
						g_VDStartupArguments.push_back(VDTextWToA(token));

					JobLockDubber();
					RunScript(filename.c_str());
					JobUnlockDubber();
				}
				else if (token == L"p") {
					VDStringW path2;

					if (!ParseArgument(s, token) || !ParseArgument(s, path2))
						throw "Command line error: syntax is /p <src_file> <dst_file>";

					JobAddBatchFile(token.c_str(), path2.c_str());
				}
				else if (token == L"queryVersion") {
					return version_num;
				}
				else if (token == L"r") {
					JobRunList();
				}
				else if (token == L"s") {
					if (!ParseArgument(s, token))
						throw "Command line error: syntax is /s <script>";

					JobLockDubber();
					RunScript(token.c_str());
					JobUnlockDubber();
				}
				else if (token == L"vtprofile") {
					g_bEnableVTuneProfiling = true;
				}
				else if (token == L"w") {
					g_fWine = true;
				}
				else if (token == L"x") {
					fExitOnDone = true;
				} else
					throw "???";

				// Toss remaining garbage.
				while(*s && *s != L' ' && *s != '/' && *s != '-')
					++s;

			} else {
				if (ParseArgument(s, token))
					g_project->Open(token.c_str());
				else
					++s;
			}
		}
	} catch(const char *s) {
		MessageBox(NULL, s, g_szError, MB_OK);
	} catch(const MyError& e) {
		e.post(g_hWnd, g_szError);
	}

	if (fExitOnDone)
		return 0;

	return -1;
}
