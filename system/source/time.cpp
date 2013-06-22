#include <new>

#include <windows.h>
#include <mmsystem.h>

#include <vd2/system/time.h>
#include <vd2/system/thread.h>

uint32 VDGetCurrentTick() {
	return (uint32)GetTickCount();
}

uint32 VDGetPreciseTick() {
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return li.QuadPart;
}

double VDGetPreciseTicksPerSecond() {
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	return freq.QuadPart;
}

VDCallbackTimer::VDCallbackTimer()
	: mTimerID(NULL)
	, mTimerPeriod(0)
	, mpExitSucceeded(NULL)
{
}

VDCallbackTimer::~VDCallbackTimer() throw() {
	Shutdown();

	delete mpExitSucceeded;
}

// We are playing with a bit of fire here -- only Windows XP has the
// TIME_KILL_SYNCHRONOUS flag, so we must make sure to shut things
// down properly.  The only way to do this right is to do the kill
// in the callback itself; otherwise, we run into this situation:
//
// 1) Callback starts executing, but is suspended by scheduler.
// 2) Timer is killed.
// 3) Callback executes.

bool VDCallbackTimer::Init(IVDTimerCallback *pCB, int period_ms) {
	VDASSERTCT(sizeof mTimerID == sizeof(UINT));

	Shutdown();

	mpCB = pCB;
	mbExit = false;
	mpExitSucceeded = new_nothrow VDSignal;

	if (mpExitSucceeded) {
		UINT accuracy = period_ms / 2;

		if (TIMERR_NOERROR == timeBeginPeriod(accuracy)) {
			mTimerPeriod = accuracy;

			mTimerID = timeSetEvent(period_ms, period_ms, StaticTimerCallback, (DWORD_PTR)this, TIME_CALLBACK_FUNCTION | TIME_PERIODIC);

			return mTimerID != 0;
		}
	}

	Shutdown();

	return false;
}

void VDCallbackTimer::Shutdown() {
	if (mTimerID) {
		mbExit = true;

		mpExitSucceeded->wait();
		mTimerID = 0;
	}

	if (mTimerPeriod) {
		timeEndPeriod(mTimerPeriod);
		mTimerPeriod = 0;
	}

	if (mpExitSucceeded) {
		delete mpExitSucceeded;
		mpExitSucceeded = NULL;
	}
}

bool VDCallbackTimer::IsTimerRunning() const {
	return mTimerID != 0;
}

// This prototype is deliberately different than the one in the header
// file to avoid having to pull in windows.h for clients; if Microsoft
// changes the definitions of UINT and DWORD, this won't compile.
void CALLBACK VDCallbackTimer::StaticTimerCallback(UINT id, UINT, DWORD_PTR thisPtr, DWORD_PTR, DWORD_PTR) {
	VDCallbackTimer *pThis = (VDCallbackTimer *)thisPtr;

	if (pThis->mbExit) {
		timeKillEvent(id);
		pThis->mpExitSucceeded->signal();
	} else
		pThis->mpCB->TimerCallback();
}
