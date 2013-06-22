#include <process.h>

#include <windows.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/thread.h>
#include <vd2/system/tls.h>

VDThreadID VDGetCurrentThreadID() {
	return (VDThreadID)GetCurrentThreadId();
}

VDThread::VDThread(const char *pszDebugName)
	: mpszDebugName(pszDebugName)
	, mhThread(0)
	, mThreadID(0)
{
}

VDThread::~VDThread() throw() {
	if (isThreadAttached())
		ThreadWait();
}

bool VDThread::ThreadStart() {
	VDASSERT(!isThreadAttached());

	if (!isThreadAttached())
		mhThread = (void *)_beginthreadex(NULL, 0, StaticThreadStart, this, 0, &mThreadID);

	return mhThread != 0;
}

void VDThread::ThreadDetach() {
	if (isThreadAttached()) {
		CloseHandle((HANDLE)mhThread);
		mhThread = NULL;
		mThreadID = 0;
	}
}

void VDThread::ThreadWait() {
	if (isThreadAttached()) {
		WaitForSingleObject((HANDLE)mhThread, INFINITE);
		ThreadDetach();
		mThreadID = 0;
	}
}

bool VDThread::isThreadActive() {
	if (isThreadAttached()) {
		if (WAIT_TIMEOUT == WaitForSingleObject((HANDLE)mhThread, 0))
			return true;

		ThreadDetach();
		mThreadID = 0;
	}
	return false;
}

void VDThread::ThreadFinish() {
	_endthreadex(0);
}

void *VDThread::ThreadLocation() const {
	if (!isThreadAttached())
		return NULL;

	CONTEXT ctx;

	ctx.ContextFlags = CONTEXT_CONTROL;

	SuspendThread(mhThread);
	GetThreadContext(mhThread, &ctx);
	ResumeThread(mhThread);

	return (void *)ctx.Eip;
}

///////////////////////////////////////////////////////////////////////////
//
// This apparently came from one a talk by one of the Visual Studio
// developers, i.e. I didn't write it.

#define MS_VC_EXCEPTION 0x406d1388

typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;        // must be 0x1000
    LPCSTR szName;       // pointer to name (in same addr space)
    DWORD dwThreadID;    // thread ID (-1 caller thread)
    DWORD dwFlags;       // reserved for future use, most be zero
} THREADNAME_INFO;

static void SetThreadName(DWORD dwThreadID, LPCTSTR szThreadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = szThreadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;

    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(DWORD), (DWORD *)&info);
    } __except (EXCEPTION_CONTINUE_EXECUTION) {
    }
}


unsigned __stdcall VDThread::StaticThreadStart(void *pThisAsVoid) {
	VDThread *pThis = static_cast<VDThread *>(pThisAsVoid);

	// We cannot use mThreadID here because it might already have been
	// invalidated by a detach in the main thread.
	if (pThis->mpszDebugName)
		SetThreadName(GetCurrentThreadId(), pThis->mpszDebugName);

	VDInitThreadData(pThis->mpszDebugName);

	vdprotected1("running thread \"%.64s\"", const char *, pThis->mpszDebugName) {
		pThis->ThreadRun();
	}

	// NOTE: Do not put anything referencing this here, since our object
	//       may have been destroyed by the threaded code.

	VDDeinitThreadData();

	return 0;
}

///////////////////////////////////////////////////////////////////////////

VDSignal::VDSignal() {
	hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

VDSignalPersistent::VDSignalPersistent() {
	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
}

VDSignalBase::~VDSignalBase() {
	CloseHandle(hEvent);
}

void VDSignalBase::signal() {
	SetEvent(hEvent);
}

void VDSignalBase::wait() {
	WaitForSingleObject(hEvent, INFINITE);
}

bool VDSignalBase::check() {
	return WAIT_OBJECT_0 == WaitForSingleObject(hEvent, 0);
}

int VDSignalBase::wait(VDSignalBase *second) {
	HANDLE		hArray[16];
	DWORD		dwRet;

	hArray[0] = hEvent;
	hArray[1] = second->hEvent;

	dwRet = WaitForMultipleObjects(2, hArray, FALSE, INFINITE);

	return dwRet == WAIT_FAILED ? -1 : dwRet - WAIT_OBJECT_0;
}

int VDSignalBase::wait(VDSignalBase *second, VDSignalBase *third) {
	HANDLE		hArray[3];
	DWORD		dwRet;

	hArray[0] = hEvent;
	hArray[1] = second->hEvent;
	hArray[2] = third->hEvent;

	dwRet = WaitForMultipleObjects(3, hArray, FALSE, INFINITE);

	return dwRet == WAIT_FAILED ? -1 : dwRet - WAIT_OBJECT_0;
}

void VDSignalPersistent::unsignal() {
	ResetEvent(hEvent);
}

