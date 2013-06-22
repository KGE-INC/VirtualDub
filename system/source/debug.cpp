//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#include <stdio.h>
#include <process.h>

#include <windows.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/List.h>
#include <vd2/system/thread.h>

#ifdef _DEBUG

class VDSafeMessageBoxThreadW32 : public VDThread {
public:
	VDSafeMessageBoxThreadW32(HWND hwndParent, const char *pszText, const char *pszCaption, DWORD dwFlags)
		: mhwndParent(hwndParent)
		, mpszText(pszText)
		, mpszCaption(pszCaption)
		, mdwFlags(dwFlags)
	{
	}

	DWORD GetResult() const { return mdwResult; }

protected:
	void ThreadRun() {
		mdwResult = MessageBox(mhwndParent, mpszText, mpszCaption, mdwFlags);
	}

	HWND mhwndParent;
	const char *const mpszText;
	const char *const mpszCaption;
	const DWORD mdwFlags;
	DWORD mdwResult;
};

UINT VDSafeMessageBoxW32(HWND hwndParent, const char *pszText, const char *pszCaption, DWORD dwFlags) {
	VDSafeMessageBoxThreadW32 mbox(hwndParent, pszText, pszCaption, dwFlags);

	mbox.ThreadStart();
	mbox.ThreadWait();
	return mbox.GetResult();
}

VDAssertResult VDAssert(const char *exp, const char *file, int line) {
	char szText[1024];

	VDDEBUG("%s(%d): Assert failed: %s\n", file, line, exp);

	wsprintf(szText,
		"Assert failed in module %s, line %d:\n"
		"\n"
		"\t%s\n"
		"\n"
		"Break into debugger?", file, line, exp);

	switch(VDSafeMessageBoxW32(NULL, szText, "Assert failure", MB_ABORTRETRYIGNORE|MB_ICONWARNING|MB_TASKMODAL)) {
	case IDABORT:
		return kVDAssertBreak;
	case IDRETRY:
		return kVDAssertContinue;
	default:
		VDNEVERHERE;
	case IDIGNORE:
		return kVDAssertIgnore;
	}
}

VDAssertResult VDAssertPtr(const char *exp, const char *file, int line) {
	char szText[1024];

	VDDEBUG("%s(%d): Assert failed: %s is not a valid pointer\n", file, line, exp);

	wsprintf(szText,
		"Assert failed in module %s, line %d:\n"
		"\n"
		"\t(%s) not a valid pointer\n"
		"\n"
		"Break into debugger?", file, line, exp);

	switch(VDSafeMessageBoxW32(NULL, szText, "Assert failure", MB_ABORTRETRYIGNORE|MB_ICONWARNING|MB_TASKMODAL)) {
	case IDABORT:
		return kVDAssertBreak;
	case IDRETRY:
		return kVDAssertContinue;
	default:
		VDNEVERHERE;
	case IDIGNORE:
		return kVDAssertIgnore;
	}
}

#endif

__declspec(thread) VDProtectedAutoScope *volatile g_protectedScopeLink;

void VDProtectedAutoScopeICLWorkaround() {}

#if 0			//def _DEBUG
static VDCriticalSection g_csDebug;
static VDSignal g_signalDebug;
static VDSignal g_signalDebugReturn;
static bool g_bDebugThreadStarted;
static char g_debugBuffer[128];
static int g_debugBufferPtr;

static void debugThread(void *) {
	for(;;) {
		g_signalDebug.wait();

		++g_csDebug;
		if (g_debugBufferPtr) {
			g_debugBuffer[g_debugBufferPtr] = 0;

			Sleep(0);
			OutputDebugString(g_debugBuffer);
			g_debugBufferPtr = 0;
		}
		--g_csDebug;

		g_signalDebugReturn.signal();
	}
}

void VDDebugPrint(const char *format, ...) {
	va_list val;
	int len;

	if (!g_bDebugThreadStarted) {
		g_bDebugThreadStarted = true;
		_beginthread(debugThread, 0, NULL);
	}

	va_start(val, format);

	++g_csDebug;
	len = _vsnprintf(g_debugBuffer + g_debugBufferPtr, sizeof g_debugBuffer - 1 - g_debugBufferPtr, format, val);

	while(len < 0 && g_debugBufferPtr) {
		--g_csDebug;
		g_signalDebug.signal();
		g_signalDebugReturn.wait();
		++g_csDebug;

		if (!g_debugBufferPtr) {
			len = _vsnprintf(g_debugBuffer, sizeof g_debugBuffer-1, format, val);
			break;
		}
	}

	if (len > 0) {
		g_debugBufferPtr += len;
		g_signalDebug.signal();
	}

	--g_csDebug;
	va_end(val);
}
#else
void VDDebugPrint(const char *format, ...) {
	char buf[4096];

	va_list val;
	va_start(val, format);
	_vsnprintf(buf, sizeof buf, format, val);
	va_end(val);
	Sleep(0);
	OutputDebugString(buf);
}
#endif
