#include <stdio.h>
#include <stdarg.h>

#include <vd2/system/tls.h>
#include <vd2/system/progress.h>
#include <vd2/system/error.h>
#include <vd2/system/VDAtomic.h>
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
	sprintf(buf, format, val);
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
