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
uint64 VDGetPreciseTick();
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
