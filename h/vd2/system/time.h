#ifndef f_VD2_SYSTEM_TIME_H
#define f_VD2_SYSTEM_TIME_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/atomic.h>
#include <vd2/system/thread.h>

// VDGetCurrentTick: Retrieve current process timer, in milliseconds.  Should only
// be used for sparsing updates/checks, and not for precision timing.  Approximate
// resolution is 55ms under Win9x and 10ms under WinNT.
uint32 VDGetCurrentTick();

// VDGetPreciseTick: Retrieves high-performance timer (QueryPerformanceCounter in
// Win32).
uint32 VDGetPreciseTick();
double VDGetPreciseTicksPerSecond();

// VDCallbackTimer is an abstraction of the Windows multimedia timer.  As such, it
// is rather expensive to instantiate, and should only be used for critical timing
// needs... such as multimedia.  Basically, there should only really be one or two
// of these running.  Win32 typically implements these as separate threads
// triggered off a timer, so despite the outdated documentation -- which still hasn't
// been updated from Windows 3.1 -- you can call almost any function from the
// callback.  Execution time in the callback delays other timers, however, so the
// callback should still execute as quickly as possible.

class VDINTERFACE IVDTimerCallback {
public:
	virtual void TimerCallback() = 0;
};

class VDCallbackTimer {
public:
	VDCallbackTimer();
	~VDCallbackTimer() throw();

	bool Init(IVDTimerCallback *pCB, int period_ms);
	void Shutdown();

	bool IsTimerRunning() const;

private:
	IVDTimerCallback *mpCB;
	unsigned		mTimerID;
	unsigned		mTimerPeriod;

	VDSignal		*mpExitSucceeded;
	volatile bool	mbExit;				// this doesn't really need to be atomic -- think about it

#ifdef _M_AMD64
	static void __stdcall StaticTimerCallback(unsigned id, unsigned, unsigned __int64 thisPtr, unsigned __int64, unsigned __int64);
#else
	static void __stdcall StaticTimerCallback(unsigned id, unsigned, unsigned long thisPtr, unsigned long, unsigned long);
#endif
};

#endif
