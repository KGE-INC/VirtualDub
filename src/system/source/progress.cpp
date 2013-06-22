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

#include "stdafx.h"
#include <stdio.h>
#include <stdarg.h>

#include <vd2/system/tls.h>
#include <vd2/system/progress.h>
#include <vd2/system/error.h>
#include <vd2/system/atomic.h>
#include <vd2/system/thread.h>

void ProgressSetHandler(IProgress *pp) {
	g_tlsdata.pProgress = pp;
	g_tlsdata.pbAbort = NULL;
	if (pp)
		g_tlsdata.pbAbort = pp->ProgressGetAbortFlag();
	g_tlsdata.pAbortSignal = NULL;
}

IProgress *ProgressGetHandler() {
	return g_tlsdata.pProgress;
}

bool ProgressCheckAbort() {
	return 0 != *g_tlsdata.pbAbort;
}

void ProgressSetAbort(bool bNewValue) {
	if (bNewValue) {
		*g_tlsdata.pbAbort = true;

		if (g_tlsdata.pAbortSignal)
			g_tlsdata.pAbortSignal->signal();
	} else {
		*g_tlsdata.pbAbort = false;

		if (g_tlsdata.pAbortSignal)
			g_tlsdata.pAbortSignal->unsignal();
	}
}

VDSignalPersistent *ProgressGetAbortSignal() {
	if (!g_tlsdata.pAbortSignal)
		g_tlsdata.pAbortSignal = g_tlsdata.pProgress->ProgressGetAbortSignal();
	return g_tlsdata.pAbortSignal;
}

void ProgressError(const MyError& e) {
	g_tlsdata.pProgress->Error(e.gets());
}

void ProgressWarning(const char *format, ...) {
	IProgress *pProgress = g_tlsdata.pProgress;
	if (!pProgress)
		return;

	char buf[2048];
	va_list val;

	va_start(val, format);
	vsprintf(buf, format, val);
	va_end(val);

	pProgress->Warning(buf);
}

void ProgressOutput(const char *format, ...) {
	IProgress *pProgress = g_tlsdata.pProgress;
	if (!pProgress)
		return;
	char buf[2048];
	va_list val;

	va_start(val, format);
	vsprintf(buf, format, val);
	va_end(val);

	pProgress->Output(buf);
}

bool ProgressQuery(bool fDefault, const char *format, ...) {
	IProgress *pProgress = g_tlsdata.pProgress;
	if (!pProgress)
		return false;
	char buf[2048];
	va_list val;

	va_start(val, format);
	vsprintf(buf, format, val);
	va_end(val);

	return pProgress->Query(buf, fDefault);
}

void ProgressStart(long lMax, const char *caption, const char *progresstext, const char *format, ...) {
	IProgress *pProgress = g_tlsdata.pProgress;
	if (!pProgress)
		return;
	char buf[2048];
	va_list val;

	va_start(val, format);
	vsprintf(buf, format, val);
	va_end(val);

	pProgress->ProgressStart(buf, caption, progresstext, lMax);
}

void ProgressAdvance(long lNewValue) {
	IProgress *pProgress = g_tlsdata.pProgress;
	if (!pProgress)
		return;
	pProgress->ProgressAdvance(lNewValue);
}

void ProgressEnd() {
	IProgress *pProgress = g_tlsdata.pProgress;
	if (!pProgress)
		return;
	pProgress->ProgressEnd();
}
