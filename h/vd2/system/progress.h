#ifndef f_SYSTEM_PROGRESS_H
#define f_SYSTEM_PROGRESS_H

#include <vd2/system/error.h>

class VDAtomicInt;
class VDSignalPersistent;

class IProgress {
public:
	virtual void Error(const char *)=0;
	virtual void Warning(const char *)=0;
	virtual bool Query(const char *query, bool fDefault)=0;
	virtual void ProgressStart(const char *text, const char *caption, const char *progtext, long lMax)=0;
	virtual void ProgressAdvance(long)=0;
	virtual void ProgressEnd()=0;
	virtual void Output(const char *text)=0;
	virtual VDAtomicInt *ProgressGetAbortFlag()=0;
	virtual VDSignalPersistent *ProgressGetAbortSignal()=0;
};


void ProgressSetHandler(IProgress *pp);
IProgress *ProgressGetHandler();

bool ProgressCheckAbort();
void ProgressSetAbort(bool bNewValue);
VDSignalPersistent *ProgressGetAbortSignal();
void ProgressError(const class MyError&);
void ProgressWarning(const char *format, ...);
void ProgressOutput(const char *format, ...);
bool ProgressQuery(bool fDefault, const char *format, ...);
void ProgressStart(long lMax, const char *caption, const char *progresstext, const char *format, ...);
void ProgressAdvance(long lNewValue);
void ProgressEnd();


class VDProgress {
public:
	VDProgress(long lMax, const char *caption, const char *progresstext, const char *format, ...) {
		ProgressStart(lMax, caption, progresstext, format);
	}

	~VDProgress() {
		ProgressEnd();
	}

	void advance(long v) {
		ProgressAdvance(v);
	}
};

class VDProgressAbortable {
public:
	VDProgressAbortable(long lMax, const char *caption, const char *progresstext, const char *format, ...) {
		ProgressStart(lMax, caption, progresstext, format);
		ProgressSetAbort(false);
	}

	~VDProgressAbortable() {
		ProgressEnd();
	}

	void advance(long v) {
		if (ProgressCheckAbort())
			throw MyUserAbortError();
		ProgressAdvance(v);
	}
};

#endif
