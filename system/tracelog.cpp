#if 0

#include <windows.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/List.h>

///////////////////////////////////////////////////////////////////////////
//
// Trace log support.
//
// This only works under Windows NT. :(
//
// It is vital that we generate no constructors here!

// !!caution!!	We can deadlock if certain Win32 functions are called while
//				holding this critsec, since the TLS function locks the
//				process critical section.  That means you grab the csect,
//				do something entirely in local code, and get the f*ck out
//				of it as fast as possible.

static CRITICAL_SECTION g_csTraceLog;

__declspec(thread) volatile VDThreadTraceLog g_tracelog;

static __declspec(thread) int g_tracelogidx;

#define MAX_TRACE_THREADS		(64)

static struct {
	DWORD dwThreadId;
	HANDLE hThreadId;
	VDThreadTraceLog *pTraceLog;
} g_tracetable[MAX_TRACE_THREADS];


static void NTAPI ThreadLogTLSHook(void *hModule, unsigned long reason, void *reserved) {
	int i;

	switch(reason) {
	case DLL_PROCESS_ATTACH:
		InitializeCriticalSection(&g_csTraceLog);
		/* fall through */
	case DLL_THREAD_ATTACH:
		EnterCriticalSection(&g_csTraceLog);

		for(i=0; i<MAX_TRACE_THREADS; ++i)
			if (!g_tracetable[i].dwThreadId)
				break;

		g_tracelogidx = -1;

		if (i<MAX_TRACE_THREADS) {

			if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_tracetable[i].hThreadId, NULL, FALSE, DUPLICATE_SAME_ACCESS)) {
				g_tracelogidx = i;
				g_tracetable[i].dwThreadId = GetCurrentThreadId();
				g_tracetable[i].pTraceLog = const_cast<VDThreadTraceLog *>(&g_tracelog);
			}
		}

		LeaveCriticalSection(&g_csTraceLog);

		break;
	case DLL_THREAD_DETACH:

		EnterCriticalSection(&g_csTraceLog);
		if (g_tracelogidx >= 0)
			g_tracetable[g_tracelogidx].dwThreadId = 0;
		CloseHandle(g_tracetable[g_tracelogidx].hThreadId);
		LeaveCriticalSection(&g_csTraceLog);
		break;
	}
}

static void VDSuspendResumeAllOtherLoggedThreads(bool bSuspend) {
	DWORD dwMyThreadId = GetCurrentThreadId();

	EnterCriticalSection(&g_csTraceLog);
	for(int i=0; i<MAX_TRACE_THREADS; ++i)
		if (g_tracetable[i].dwThreadId && g_tracetable[i].dwThreadId != dwMyThreadId) {
			if (bSuspend)
				SuspendThread(g_tracetable[i].hThreadId);
			else
				ResumeThread(g_tracetable[i].hThreadId);
		}
	LeaveCriticalSection(&g_csTraceLog);
}

static BOOL CALLBACK VDThreadTraceDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	char buf[1024];

	switch(msg) {
	case WM_INITDIALOG:
		{
			DWORD dwMyThreadId = GetCurrentThreadId();
			HWND hwndItem = GetDlgItem(hdlg, 500);
			int i;

			for(i=0; i<MAX_TRACE_THREADS; ++i)
				if (g_tracetable[i].dwThreadId && g_tracetable[i].dwThreadId != dwMyThreadId) {
					wsprintf(buf, "Thread %08lx - %s", g_tracetable[i].dwThreadId, g_tracetable[i].pTraceLog->desc ? g_tracetable[i].pTraceLog->desc : "(unknown thread)");

					SendMessage(hwndItem, CB_SETITEMDATA, SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)buf), i);
				}
		}
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDCANCEL:
			EndDialog(hdlg, 0);
			return TRUE;
		case 500:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				HWND hwndItem = GetDlgItem(hdlg, 501);
				VDThreadTraceLog *pLog = g_tracetable[SendMessage((HWND)lParam, CB_GETITEMDATA, SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0), 0)].pTraceLog;

				SendMessage(hwndItem, LB_RESETCONTENT, 0, 0);

				for(int i=1; i<=VDTRACELOG_SIZE; ++i) {
					const char *psz = pLog->log[(pLog->idx + i) & (VDTRACELOG_SIZE-1)].s;
					const long v = pLog->log[(pLog->idx + i) & (VDTRACELOG_SIZE-1)].v;

					if (psz) {
						wsprintf(buf, psz, v);
						SendMessage(hwndItem, LB_ADDSTRING, 0, (LPARAM)buf);
					}
				}
			}
			return TRUE;
		}
		return TRUE;
	}
	return FALSE;
}

static DWORD WINAPI VDThreadTraceStart(LPVOID lpTemplate) {
	VDSuspendResumeAllOtherLoggedThreads(true);
	DialogBox(GetModuleHandle(NULL), (LPCTSTR)lpTemplate, NULL, VDThreadTraceDlgProc);
	VDSuspendResumeAllOtherLoggedThreads(false);

	return 0;
}

void VDThreadTrace(LPCTSTR lpTemplate) {
	DWORD dwThreadId;

	HANDLE hThread = CreateThread(NULL, 0, VDThreadTraceStart, (void *)lpTemplate, 0, &dwThreadId);

	if (hThread)
		CloseHandle(hThread);
}

///////////////////////////////////////////////////////////////////////////
//
// To hook all the thread logs together, we need to be able to intercept thread
// creation and destruction.  The easiest way to do this is to hook into the TLS
// callbacks.  VC++ doesn't allow us to use thread local constructors, so we should
// be able to hook into the TLS constructor list, marked by the .CRT$XLA and .CRT$XLZ
// segments.  Unfortunately, the .CRT$XLA segment starts with a null longword which
// immediately terminates the list.
//
// Thus, as usual, we do it ourself.  This section replaces tlssup.obj in the
// run-time library.

extern "C" extern unsigned long _tls_index=0;

static PIMAGE_TLS_CALLBACK tls_functions[]={
	ThreadLogTLSHook,
	NULL
};

// I think these were meant for thread local hooks, but they don't work properly
// because of the stupid zero. :(

#pragma data_seg(".CRT$XLA")
void (__cdecl *__xl_a)()=0;
#pragma data_seg(".CRT$XLZ")
void (__cdecl *__xl_z)()=0;

// Presumably, the TLS area that is copied between all functions.  These need
// to be non-const, because const data doesn't need to be TL, does it?

#pragma data_seg(".tls")
extern "C" int _tls_start=0;
#pragma data_seg(".tls$ZZZ")
extern "C" int _tls_end=0;

extern "C" const struct _IMAGE_TLS_DIRECTORY32 _tls_used = {
	(DWORD)&_tls_start,
	(DWORD)&_tls_end,
#ifndef INVALID_SET_FILE_POINTER		// VC6 includes
	(LPDWORD)&_tls_index,
	(void(__stdcall**)(void*,DWORD,void *))tls_functions,
#else		// PlatSDK includes
	(DWORD)&_tls_index,
	(DWORD)tls_functions,
#endif
	0,
	0
};

#pragma data_seg()

#endif
