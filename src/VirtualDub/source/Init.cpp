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
#include <vd2/Dita/services.h>
#include <vd2/Riza/display.h>
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
#include "RTProfileDisplay.h"
#include "plugins.h"

#include "project.h"
#include "projectui.h"
#include "capture.h"
#include "captureui.h"

#ifdef _DEBUG
	#define VD_GENERIC_BUILD_NAMEW	L"debug"
	#define VD_BUILD_NAMEW			L"debug"
#else
	#define VD_GENERIC_BUILD_NAMEW	L"release"

	#if defined(_M_AMD64)
		#define VD_BUILD_NAMEW		L"release-AMD64"
	#elif defined(__INTEL_COMPILER)
		#define VD_BUILD_NAMEW		L"release-P4"
	#else
		#define VD_BUILD_NAMEW		L"release"
	#endif
#endif

#if defined(_M_AMD64)
	#define VD_COMPILE_TARGETW		L"AMD64"
	#define VD_EXEFILE_NAMEA		"Veedub64.exe"
	#define VD_CLIEXE_NAMEA			"vdub64.exe"
#elif defined(__INTEL_COMPILER)
	#define VD_COMPILE_TARGETW		L"Pentium 4"
	#define VD_EXEFILE_NAMEA		"VeedubP4.exe"
	#define VD_CLIEXE_NAMEA			"vdubp4.exe"
#else
	#define VD_COMPILE_TARGETW		L"80x86"
	#define VD_EXEFILE_NAMEA		"VirtualDub.exe"
	#define VD_CLIEXE_NAMEA			"vdub.exe"
#endif

///////////////////////////////////////////////////////////////////////////

extern void InitBuiltinFilters();
extern void VDInitBuiltinAudioFilters();
extern void VDInitBuiltinVideoFilters();
extern void VDInitInputDrivers();
extern void VDInitExternalCallTrap();
extern void VDInitVideoCodecBugTrap();

///////////////////////////////////////////////////////////////////////////

extern LONG __stdcall CrashHandlerHook(EXCEPTION_POINTERS *pExc);
extern LONG __stdcall CrashHandler(struct _EXCEPTION_POINTERS *ExceptionInfo, bool allowForcedExit);
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

extern vdrefptr<IVDCaptureProject> g_capProject;
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
	} __except(CrashHandler((EXCEPTION_POINTERS*)_exception_info(), true)) {
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
	} __except(CrashHandler((EXCEPTION_POINTERS*)_exception_info(), true)) {
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
	SetUnhandledExceptionFilter(CrashHandlerHook);
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
			L"VirtualDub CLI Video Processor Version 1.7.0 (build %lu/" VD_GENERIC_BUILD_NAMEW L") for " VD_COMPILE_TARGETW
			,1
			,&version_num));
	VDLog(kVDLogInfo, VDswprintf(
			L"Copyright (C) Avery Lee 1998-2006. Licensed under GNU General Public License\n"
			,1
			,&version_num));

	// prep system stuff

	VDCHECKPOINT;

	AVIFileInit();

	VDRegistryAppKey::setDefaultKey("Software\\Freeware\\VirtualDub\\");
	VDLoadFilespecSystemData();

	// initialize filters, job system, MRU list, help system

	InitBuiltinFilters();
	VDInitBuiltinAudioFilters();
	VDInitBuiltinVideoFilters();
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

	AVIFileExit();

	_CrtCheckMemory();

	VDCHECKPOINT;

	VDSaveFilespecSystemData();
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

	ATOM RegisterAudioDisplayControl();
	if (!RegisterAudioDisplayControl()) return false;

	if (!VDRegisterVideoDisplayControl()) return false;
	if (!RegisterFilterGraphControl()) return false;
	if (!RegisterLogWindowControl()) return false;
	if (!RegisterRTProfileDisplayControl()) return false;
	if (!RegisterVideoWindow()) return false;

	extern bool VDRegisterUIFrameWindow();
	if (!VDRegisterUIFrameWindow()) return false;

	extern bool VDRegisterParameterCurveControl();
	if (!VDRegisterParameterCurveControl()) return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////

bool InitInstance( HANDLE hInstance, int nCmdShow) {
	wchar_t buf[256];

	VDStringW versionFormat(VDLoadStringW32(IDS_TITLE_INITIAL));

	swprintf(buf, sizeof buf / sizeof buf[0], versionFormat.c_str(), version_num,
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

	VDUIFrame *pFrame = VDUIFrame::GetFrame(g_hWnd);
	pFrame->SetRegistryName("Main window");
	pFrame->RestorePlacement();

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


class VDConsoleLogger : public IVDLogger {
public:
	void AddLogEntry(int severity, const VDStringW& s);
	void Write(const wchar_t *s, size_t len);
} g_VDConsoleLogger;

void VDConsoleLogger::AddLogEntry(int severity, const VDStringW& s) {
	const size_t len = s.length();
	const wchar_t *text = s.data();
	const wchar_t *end = text + len;

	// Don't annotate lines in this routine. We print some 'errors' in
	// the cmdline handling that aren't really errors, and would have
	// to be revisited.
	for(;;) {
		const wchar_t *t = text;

		while(t != end && *t != '\n')
			++t;

		Write(text, t-text);
		if (t == end)
			break;

		text = t+1;
	}
}

void VDConsoleLogger::Write(const wchar_t *text, size_t len) {
	DWORD actual;

	if (!len) {
		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "\r\n", 2, &actual, NULL);
	}

	int mblen = WideCharToMultiByte(CP_ACP, 0, text, len, NULL, 0, NULL, NULL);

	char *buf = (char *)alloca(mblen + 2);

	mblen = WideCharToMultiByte(CP_ACP, 0, text, len, buf, mblen, NULL, NULL);

	if (mblen) {
		buf[mblen] = '\r';
		buf[mblen+1] = '\n';

		WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, mblen+2, &actual, NULL);
	}
}

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

	bool consoleMode = false;
	int argsFound = 0;
	int rc = -1;

	JobLockDubber();

	VDAutoLogDisplay disp;

	try {
		while(*s) {
			while(iswspace(*s))
				++s;

			if (!*s) break;

			++argsFound;

			if (*s == L'/') {
				++s;
				if (*s == L'?')
					throw MyError(
					//   12345678901234567890123456789012345678901234567890123456789012345678901234567890
						"Command-line flags:\n"
						"\n"
						"  /b <src-dir> <dst-dir>    Add batch entries for a directory\n"
						"  /c                        Clear job list\n"
						"  /capture                  Switch to capture mode\n"
						"  /capchannel <ch> [<freq>] Set capture channel (opt. frequency in MHz)\n"
						"  /capdevice <devname>      Set capture device\n"
						"  /capfile <filename>       Set capture filename\n"
						"  /capfileinc <filename>    Set capture filename and bump until clear\n"
						"  /capstart [<time>[s]]     Capture with optional time limit\n"
						"                            (default is minutes, use 's' for seconds)\n"
						"  /cmd <command>            Run quick script command\n"
						"  /F <filter>               Load filter\n"
						"  /h                        Disable exception filter\n"
						"  /hexedit [<filename>]     Open hex editor\n"
						"  /i <script> [<args...>]   Invoke script with arguments\n"
						"  /p <src> <dst>            Add a batch entry for a file\n"
						"  /queryVersion             Return build number\n"
						"  /r                        Run job queue\n"
						"  /s <script>               Run a script\n"
						"  /x                        Exit when complete\n"
						);

				// parse out the switch name
				const wchar_t *switchStart = s;
				while(*s && iswalnum(*s))
					++s;

				token.assign(switchStart, s-switchStart);

				if (token == L"b") {
					VDStringW path2;

					if (!ParseArgument(s, token) || !ParseArgument(s, path2))
						throw MyError("Command line error: syntax is /b <src_dir> <dst_dir>");

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
						throw MyError("Command line error: not in capture mode");

					if (!ParseArgument(s, token))
						throw MyError("Command line error: syntax is /capchannel [antenna:|cable:]<channel>");

					if (!wcsncmp(token.c_str(), L"antenna:", 8)) {
						g_capProjectUI->SetTunerInputMode(false);
						token = VDStringW(token.c_str() + 8);
					} else if (!wcsncmp(token.c_str(), L"cable:", 6)) {
						g_capProjectUI->SetTunerInputMode(true);
						token = VDStringW(token.c_str() + 6);
					}

					g_capProjectUI->SetTunerChannel(_wtoi(token.c_str()));

					if (ParseArgument(s, token))
						g_capProjectUI->SetTunerExactFrequency(VDRoundToInt(wcstod(token.c_str(), NULL) * 1000000));
				}
				else if (token == L"capdevice") {
					if (!g_capProjectUI)
						throw MyError("Command line error: not in capture mode");

					if (!ParseArgument(s, token))
						throw MyError("Command line error: syntax is /capdevice <device>");

					if (!g_capProjectUI->SetDriver(token.c_str()))
						throw MyError("Unable to initialize capture device: %ls\n", token.c_str());
				}
				else if (token == L"capfile") {
					if (!g_capProjectUI)
						throw MyError("Command line error: not in capture mode");

					if (!ParseArgument(s, token))
						throw MyError("Command line error: syntax is /capfile <filename>");

					g_capProjectUI->SetCaptureFile(token.c_str());
				}
				else if (token == L"capfileinc") {
					if (!g_capProjectUI)
						throw MyError("Command line error: not in capture mode");

					if (!ParseArgument(s, token))
						throw MyError("Command line error: syntax is /capfileinc <filename>");

					g_capProjectUI->SetCaptureFile(token.c_str());
					g_capProject->IncrementFileIDUntilClear();
				}
				else if (token == L"capstart") {
					if (!g_capProjectUI)
						throw MyError("Command line error: not in capture mode");

					if (ParseArgument(s, token)) {
						int multiplier = 60;

						if (!token.empty() && token[token.size()-1] == 's') {
							token.resize(token.size()-1);
							multiplier = 1;
						}

						int limit = multiplier*_wtoi(token.c_str());

						g_capProjectUI->SetTimeLimit(limit);
					}

					g_capProjectUI->Capture();
				}
				else if (token == L"console") {
					consoleMode = true;
					VDAttachLogger(&g_VDConsoleLogger, false, true);
					// don't count the /console flag as an argument that does work
					--argsFound;
				}
				else if (token == L"cmd") {
					if (!ParseArgument(s, token))
						throw MyError("Command line error: syntax is /cmd <script>");
					const size_t len = token.size();
					for(int i=0; i<len; ++i)
						if (token[i] == '\'')
							token[i] = '"';
					token.append(L';');
					RunScriptMemory((char *)VDTextWToA(token).c_str());
				}
				else if (token == L"fsck") {
					crash();
				}
				else if (token == L"F") {
					if (!ParseArgument(s, token))
						throw MyError("Command line error: syntax is /F <filter>");

					VDAddPluginModule(token.c_str());

					guiSetStatus("Loaded external filter module: %s", 255, VDTextWToA(token).c_str());
				}
				else if (token == L"h") {
					SetUnhandledExceptionFilter(NULL);
				}
				else if (token == L"hexedit") {
					if (ParseArgument(s, token))
						HexEdit(NULL, VDTextWToA(token).c_str());
					else
						HexEdit(NULL, NULL);
				}
				else if (token == L"i") {
					VDStringW filename;

					if (!ParseArgument(s, filename))
						throw MyError("Command line error: syntax is /i <script> [<args>...]");

					g_VDStartupArguments.clear();
					while(ParseArgument(s, token))
						g_VDStartupArguments.push_back(VDTextWToA(token));

					RunScript(filename.c_str());
				}
				else if (token == L"p") {
					VDStringW path2;

					if (!ParseArgument(s, token) || !ParseArgument(s, path2))
						throw MyError("Command line error: syntax is /p <src_file> <dst_file>");

					JobAddBatchFile(token.c_str(), path2.c_str());
				}
				else if (token == L"queryVersion") {
					rc = version_num;
					break;
				}
				else if (token == L"r") {
					JobUnlockDubber();
					JobRunList();
					JobLockDubber();
				}
				else if (token == L"s") {
					if (!ParseArgument(s, token))
						throw MyError("Command line error: syntax is /s <script>");

					RunScript(token.c_str());
				}
				else if (token == L"vtprofile") {
					g_bEnableVTuneProfiling = true;
				}
				else if (token == L"w") {
					g_fWine = true;
				}
				else if (token == L"x") {
					fExitOnDone = true;

					// don't count the /x flag as an argument that does work
					--argsFound;
				} else
					throw MyError("Command line error: unknown switch /%ls", token.c_str());

				// Toss remaining garbage.
				while(*s && *s != L' ' && *s != '/' && *s != '-')
					++s;

			} else {
				if (ParseArgument(s, token)) {
					if (g_capProjectUI)
						g_capProjectUI->SetCaptureFile(token.c_str());
					else
						g_project->Open(token.c_str());
				} else
					++s;
			}
		}

		if (!argsFound && consoleMode)
			throw MyError(
				"This application allows usage of VirtualDub from the command line. To use\n"
				"the program interactively, launch "VD_EXEFILE_NAMEA" directly.\n"
				"\n"
				"Usage: "VD_CLIEXE_NAMEA" ( /<switches> | video-file ) ...\n"
				"       "VD_CLIEXE_NAMEA" /? for help\n");

		if (!consoleMode)
			disp.Post((VDGUIHandle)g_hWnd);
	} catch(const MyUserAbortError&) {
		if (consoleMode) {
			VDLog(kVDLogInfo, VDStringW(L"Operation was aborted by user."));

			rc = 1;
		}
	} catch(const MyError& e) {
		if (consoleMode) {
			const char *err = e.gets();

			if (err)
				VDLog(kVDLogError, VDTextAToW(err));

			rc = 5;
		} else
			e.post(g_hWnd, g_szError);
	}
	JobUnlockDubber();

	if (rc >= 0)
		return rc;

	if (fExitOnDone)
		return 0;

	return -1;
}
