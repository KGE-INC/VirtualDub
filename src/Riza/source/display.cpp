//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2005 Avery Lee
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

#include <vector>
#include <algorithm>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <vd2/system/atomic.h>
#include <vd2/system/thread.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/VDString.h>
#include <vd2/system/w32assist.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>

#include <vd2/Riza/display.h>
#include "displaydrv.h"

#define VDDEBUG_DISP (void)sizeof printf
//#define VDDEBUG_DISP VDDEBUG

extern const char g_szVideoDisplayControlName[] = "phaeronVideoDisplay";

extern void VDMemcpyRect(void *dst, ptrdiff_t dststride, const void *src, ptrdiff_t srcstride, size_t w, size_t h);

extern IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverD3DFX();

///////////////////////////////////////////////////////////////////////////

namespace {
	bool VDIsForegroundTask() {
		HWND hwndFore = GetForegroundWindow();

		if (!hwndFore)
			return false;

		DWORD dwProcessId = 0;
		GetWindowThreadProcessId(hwndFore, &dwProcessId);

		return dwProcessId == GetCurrentProcessId();
	}

	bool VDIsTerminalServicesClient() {
		if ((sint32)(GetVersion() & 0x000000FF) >= 0x00000005) {
			return GetSystemMetrics(SM_REMOTESESSION) != 0;		// Requires Windows NT SP4 or later.
		}

		return false;	// Ignore Windows 95/98/98SE/ME/NT3/NT4.  (Broken on NT4 Terminal Server, but oh well.)
	}
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDisplayManager;

class VDVideoDisplayClient : public vdlist_node {
public:
	VDVideoDisplayClient();
	~VDVideoDisplayClient();

	void Attach(VDVideoDisplayManager *pManager);
	void Detach(VDVideoDisplayManager *pManager);
	void SetPreciseMode(bool enabled);
	void SetPeriodicTimer();

	const uint8 *GetLogicalPalette() const;
	HPALETTE	GetPalette() const;
	void RemapPalette();

	virtual void OnTick() {}
	virtual void OnDisplayChange() {}
	virtual void OnForegroundChange(bool foreground) {}
	virtual void OnRealizePalette() {}

protected:
	VDVideoDisplayManager	*mpManager;

	bool	mbPreciseMode;
	uint32	mLastTick;
};

class VDVideoDisplayManager : public VDThread {
public:
	VDVideoDisplayManager();
	~VDVideoDisplayManager();

	bool	Init();

	void	RemoteCall(void (*function)(void *), void *data);

	void	AddClient(VDVideoDisplayClient *pClient);
	void	RemoveClient(VDVideoDisplayClient *pClient);
	void	ModifyPreciseMode(bool enabled);

	void RemapPalette();
	HPALETTE	GetPalette() const { return mhPalette; }
	const uint8 *GetLogicalPalette() const { return mLogicalPalette; }

protected:
	void	ThreadRun();

	bool	RegisterWindowClass();
	void	UnregisterWindowClass();

	bool	IsDisplayPaletted();
	void	CreateDitheringPalette();
	void	DestroyDitheringPalette();
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	int		mPreciseModeCount;
	uint32	mPreciseModePeriod;

	HPALETTE	mhPalette;
	ATOM		mWndClass;
	HWND		mhwnd;

	bool		mbAppActive;

	typedef vdlist<VDVideoDisplayClient> Clients;
	Clients		mClients;

	VDSignal			mStarted;
	VDCriticalSection	mMutex;

	struct RemoteCallNode : vdlist_node {
		void (*mpFunction)(void *data);
		void *mpData;
		VDSignal mSignal;
	};

	typedef vdlist<RemoteCallNode> RemoteCalls;
	RemoteCalls	mRemoteCalls;

	uint8	mLogicalPalette[256];
};

///////////////////////////////////////////////////////////////////////////

VDVideoDisplayClient::VDVideoDisplayClient()
	: mpManager(NULL)
	, mbPreciseMode(false)
{
}

VDVideoDisplayClient::~VDVideoDisplayClient() {
}

void VDVideoDisplayClient::Attach(VDVideoDisplayManager *pManager) {
	VDASSERT(!mpManager);
	mpManager = pManager;
	if (mbPreciseMode)
		mpManager->ModifyPreciseMode(true);
}

void VDVideoDisplayClient::Detach(VDVideoDisplayManager *pManager) {
	VDASSERT(mpManager == pManager);
	if (mbPreciseMode)
		mpManager->ModifyPreciseMode(false);
	mpManager = NULL;
}

void VDVideoDisplayClient::SetPreciseMode(bool enabled) {
	if (mbPreciseMode == enabled)
		return;

	mbPreciseMode = enabled;
	mpManager->ModifyPreciseMode(enabled);
}

const uint8 *VDVideoDisplayClient::GetLogicalPalette() const {
	return mpManager->GetLogicalPalette();
}

HPALETTE VDVideoDisplayClient::GetPalette() const {
	return mpManager->GetPalette();
}

void VDVideoDisplayClient::RemapPalette() {
	mpManager->RemapPalette();
}

///////////////////////////////////////////////////////////////////////////

VDVideoDisplayManager::VDVideoDisplayManager()
	: mPreciseModeCount(0)
	, mPreciseModePeriod(0)
	, mhPalette(NULL)
	, mWndClass(NULL)
	, mhwnd(NULL)
	, mbAppActive(false)
{
}

VDVideoDisplayManager::~VDVideoDisplayManager() {
	VDASSERT(mClients.empty());

	if (isThreadAttached()) {
		PostThreadMessage(getThreadID(), WM_QUIT, 0, 0);
		ThreadWait();
	}
}

bool VDVideoDisplayManager::Init() {
	ThreadStart();
	mStarted.wait();
	return true;
}

void VDVideoDisplayManager::RemoteCall(void (*function)(void *), void *data) {
	RemoteCallNode node;
	node.mpFunction = function;
	node.mpData = data;

	vdsynchronized(mMutex) {
		mRemoteCalls.push_back(&node);
	}

	PostThreadMessage(getThreadID(), WM_NULL, 0, 0);

	HANDLE h = node.mSignal.getHandle();
	for(;;) {
		DWORD dwResult = MsgWaitForMultipleObjects(1, &h, FALSE, INFINITE, QS_SENDMESSAGE);

		if (dwResult != WAIT_OBJECT_0+1)
			break;

		MSG msg;
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE | PM_QS_SENDMESSAGE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

void VDVideoDisplayManager::AddClient(VDVideoDisplayClient *pClient) {
	mClients.push_back(pClient);
	pClient->Attach(this);
}

void VDVideoDisplayManager::RemoveClient(VDVideoDisplayClient *pClient) {
	pClient->Detach(this);
	mClients.erase(mClients.fast_find(pClient));
}

void VDVideoDisplayManager::ModifyPreciseMode(bool enabled) {
	if (enabled) {
		int rc = ++mPreciseModeCount;
		VDASSERT(rc < 100000);
		if (rc == 1) {
			TIMECAPS tc;
			if (!mPreciseModePeriod &&
				TIMERR_NOERROR == ::timeGetDevCaps(&tc, sizeof tc) &&
				TIMERR_NOERROR == ::timeBeginPeriod(tc.wPeriodMin))
			{
				mPreciseModePeriod = tc.wPeriodMin;
				SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
			}
		}
	} else {
		int rc = --mPreciseModeCount;
		VDASSERT(rc >= 0);
		if (!rc) {
			if (mPreciseModePeriod) {
				timeEndPeriod(mPreciseModePeriod);
				mPreciseModePeriod = 0;
			}
		}
	}
}

void VDVideoDisplayManager::ThreadRun() {
	if (RegisterWindowClass()) {
		mhwnd = CreateWindowEx(WS_EX_NOPARENTNOTIFY, (LPCTSTR)mWndClass, "", WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, NULL, NULL, VDGetLocalModuleHandleW32(), this);

		if (mhwnd) {
			MSG msg;
			PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);
			mStarted.signal();

			for(;;) {
				DWORD ret = MsgWaitForMultipleObjects(0, NULL, TRUE, 1, QS_ALLINPUT);

				if (ret == WAIT_OBJECT_0) {
					bool success = false;
					while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
						if (msg.message == WM_QUIT)
							goto xit;
						success = true;
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}

					vdsynchronized(mMutex) {
						while(!mRemoteCalls.empty()) {
							RemoteCallNode *rcn = mRemoteCalls.back();
							mRemoteCalls.pop_back();
							rcn->mpFunction(rcn->mpData);
							rcn->mSignal.signal();
						}
					}

					if (success)
						continue;

					ret = WAIT_TIMEOUT;
					::Sleep(1);
				}
				
				if (ret == WAIT_TIMEOUT) {
					Clients::iterator it(mClients.begin()), itEnd(mClients.end());
					for(; it!=itEnd; ++it) {
						VDVideoDisplayClient *pClient = *it;

						pClient->OnTick();
					}
				} else
					break;
			}
xit:
			DestroyWindow(mhwnd);
			mhwnd = NULL;
		}
	}
	UnregisterWindowClass();
}

bool VDVideoDisplayManager::RegisterWindowClass() {
	WNDCLASS wc;
	HMODULE hInst = VDGetLocalModuleHandleW32();

	wc.style			= 0;
	wc.lpfnWndProc		= StaticWndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= sizeof(VDVideoDisplayManager *);
	wc.hInstance		= hInst;
	wc.hIcon			= 0;
	wc.hCursor			= 0;
	wc.hbrBackground	= 0;
	wc.lpszMenuName		= 0;

	char buf[64];
	sprintf(buf, "VDVideoDisplayManager(%p)", this);
	wc.lpszClassName	= buf;

	mWndClass = RegisterClass(&wc);

	return mWndClass != NULL;
}

void VDVideoDisplayManager::UnregisterWindowClass() {
	if (mWndClass) {
		HMODULE hInst = VDGetLocalModuleHandleW32();
		UnregisterClass((LPCTSTR)mWndClass, hInst);
		mWndClass = NULL;
	}
}

void VDVideoDisplayManager::RemapPalette() {
	PALETTEENTRY pal[216];
	struct {
		LOGPALETTE hdr;
		PALETTEENTRY palext[255];
	} physpal;

	physpal.hdr.palVersion = 0x0300;
	physpal.hdr.palNumEntries = 256;

	int i;

	for(i=0; i<216; ++i) {
		pal[i].peRed	= (BYTE)((i / 36) * 51);
		pal[i].peGreen	= (BYTE)(((i%36) / 6) * 51);
		pal[i].peBlue	= (BYTE)((i%6) * 51);
	}

	for(i=0; i<256; ++i) {
		physpal.hdr.palPalEntry[i].peRed	= 0;
		physpal.hdr.palPalEntry[i].peGreen	= 0;
		physpal.hdr.palPalEntry[i].peBlue	= (BYTE)i;
		physpal.hdr.palPalEntry[i].peFlags	= PC_EXPLICIT;
	}

	if (HDC hdc = GetDC(0)) {
		GetSystemPaletteEntries(hdc, 0, 256, physpal.hdr.palPalEntry);
		ReleaseDC(0, hdc);
	}

	if (HPALETTE hpal = CreatePalette(&physpal.hdr)) {
		for(i=0; i<216; ++i) {
			mLogicalPalette[i] = (uint8)GetNearestPaletteIndex(hpal, RGB(pal[i].peRed, pal[i].peGreen, pal[i].peBlue));
		}

		DeleteObject(hpal);
	}
}

bool VDVideoDisplayManager::IsDisplayPaletted() {
	bool bPaletted = false;

	if (HDC hdc = GetDC(0)) {
		if (GetDeviceCaps(hdc, BITSPIXEL) <= 8)		// RC_PALETTE doesn't seem to be set if you switch to 8-bit in Win98 without rebooting.
			bPaletted = true;
		ReleaseDC(0, hdc);
	}

	return bPaletted;
}

void VDVideoDisplayManager::CreateDitheringPalette() {
	if (mhPalette)
		return;

	struct {
		LOGPALETTE hdr;
		PALETTEENTRY palext[255];
	} pal;

	pal.hdr.palVersion = 0x0300;
	pal.hdr.palNumEntries = 216;

	for(int i=0; i<216; ++i) {
		pal.hdr.palPalEntry[i].peRed	= (BYTE)((i / 36) * 51);
		pal.hdr.palPalEntry[i].peGreen	= (BYTE)(((i%36) / 6) * 51);
		pal.hdr.palPalEntry[i].peBlue	= (BYTE)((i%6) * 51);
		pal.hdr.palPalEntry[i].peFlags	= 0;
	}

	mhPalette = CreatePalette(&pal.hdr);
}

void VDVideoDisplayManager::DestroyDitheringPalette() {
	if (mhPalette) {
		DeleteObject(mhPalette);
		mhPalette = NULL;
	}
}

LRESULT CALLBACK VDVideoDisplayManager::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_NCCREATE) {
		const CREATESTRUCT& cs = *(const CREATESTRUCT *)lParam;

		SetWindowLongPtr(hwnd, 0, (LONG_PTR)cs.lpCreateParams);
	} else {
		VDVideoDisplayManager *pThis = (VDVideoDisplayManager *)GetWindowLongPtr(hwnd, 0);

		if (pThis)
			return pThis->WndProc(hwnd, msg, wParam, lParam);
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK VDVideoDisplayManager::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_CREATE:
			SetTimer(hwnd, 100, 500, NULL);
			break;

		case WM_TIMER:
			{
				bool appActive = VDIsForegroundTask();

				if (mbAppActive != appActive) {
					mbAppActive = appActive;

					for(Clients::iterator it(mClients.begin()), itEnd(mClients.end()); it!=itEnd; ++it) {
						VDVideoDisplayClient *p = *it;

						p->OnForegroundChange(appActive);
					}
				}
			}
			break;

		case WM_DISPLAYCHANGE:
			{
				bool bPaletted = IsDisplayPaletted();

				if (bPaletted)
					CreateDitheringPalette();

				for(Clients::iterator it(mClients.begin()), itEnd(mClients.end()); it!=itEnd; ++it) {
					VDVideoDisplayClient *p = *it;

					p->OnDisplayChange();
				}

				if (!bPaletted)
					DestroyDitheringPalette();
			}
			break;

		// Yes, believe it or not, we still support palettes, even when DirectDraw is active.
		// Why?  Very occasionally, people still have to run in 8-bit mode, and a program
		// should still display something half-decent in that case.  Besides, it's kind of
		// neat to be able to dither in safe mode.
		case WM_PALETTECHANGED:
			{
				DWORD dwProcess;

				GetWindowThreadProcessId((HWND)wParam, &dwProcess);

				if (dwProcess != GetCurrentProcessId()) {
					for(Clients::iterator it(mClients.begin()), itEnd(mClients.end()); it!=itEnd; ++it) {
						VDVideoDisplayClient *p = *it;

						p->OnRealizePalette();
					}
				}
			}
			break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////

VDVideoDisplayFrame::VDVideoDisplayFrame()
	: mRefCount(0)
{
}

VDVideoDisplayFrame::~VDVideoDisplayFrame() {
}

int VDVideoDisplayFrame::AddRef() {
	return ++mRefCount;
}

int VDVideoDisplayFrame::Release() {
	int rc = --mRefCount;

	if (!rc)
		delete this;

	return rc;
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDisplayWindow : public IVDVideoDisplay, public IVDVideoDisplayMinidriverCallback, public VDVideoDisplayClient {
public:
	static ATOM Register();

protected:
	VDVideoDisplayWindow(HWND hwnd, const CREATESTRUCT& createInfo);
	~VDVideoDisplayWindow();

	void SetSourceMessage(const wchar_t *msg);
	void SetSourcePalette(const uint32 *palette, int count);
	bool SetSource(bool bAutoUpdate, const VDPixmap& src, void *pSharedObject, ptrdiff_t sharedOffset, bool bAllowConversion, bool bInterlaced);
	bool SetSourcePersistent(bool bAutoUpdate, const VDPixmap& src, bool bAllowConversion, bool bInterlaced);
	void SetSourceSubrect(const vdrect32 *r);
	void SetSourceSolidColor(uint32 color);
	void PostBuffer(VDVideoDisplayFrame *);
	bool RevokeBuffer(VDVideoDisplayFrame **ppFrame);
	void FlushBuffers();
	void Update(int);
	void Destroy();
	void Reset();
	void Cache();
	void SetCallback(IVDVideoDisplayCallback *pcb);
	void LockAcceleration(bool locked);
	FilterMode GetFilterMode();
	void SetFilterMode(FilterMode mode);
	float GetSyncDelta() const { return mSyncDelta; }

	void OnTick() {
		if (mpMiniDriver)
			mpMiniDriver->Poll();
	}

protected:
	void ReleaseActiveFrame();
	void RequestNextFrame();
	void DispatchNextFrame();

protected:
	static LRESULT CALLBACK StaticChildWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT ChildWndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void OnChildPaint();

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void OnPaint();
	void SyncSetSourceMessage(const wchar_t *);
	bool SyncSetSource(bool bAutoUpdate, const VDVideoDisplaySourceInfo& params);
	void SyncReset();
	bool SyncInit(bool bAutoRefresh, bool bAllowNonpersistentSource);
	void SyncUpdate(int);
	void SyncCache();
	void SyncSetFilterMode(FilterMode mode);
	void OnDisplayChange();
	void OnForegroundChange(bool bForeground);
	void OnRealizePalette();
	bool InitMiniDriver();
	void ShutdownMiniDriver();
	void VerifyDriverResult(bool result);

protected:
	enum {
		kReinitDisplayTimerId = 500
	};

	HWND		mhwnd;
	HWND		mhwndChild;
	HPALETTE	mhOldPalette;

	VDCriticalSection			mMutex;
	vdlist<VDVideoDisplayFrame>	mPendingFrames;
	vdlist<VDVideoDisplayFrame>	mIdleFrames;
	VDVideoDisplayFrame			*mpActiveFrame;
	VDVideoDisplaySourceInfo	mSource;

	IVDVideoDisplayMinidriver *mpMiniDriver;
	UINT	mReinitDisplayTimer;

	IVDVideoDisplayCallback		*mpCB;
	int		mInhibitRefresh;

	VDAtomicFloat	mSyncDelta;

	FilterMode	mFilterMode;
	bool	mbLockAcceleration;

	bool		mbIgnoreMouse;
	bool		mbUseSubrect;
	vdrect32	mSourceSubrect;
	VDStringW	mMessage;

	uint32		mSolidColorBuffer;

	VDPixmapBuffer		mCachedImage;

	uint32	mSourcePalette[256];

	static ATOM				sChildWindowClass;

public:
	static bool		sbEnableDX;
	static bool		sbEnableDXOverlay;
	static bool		sbEnableD3D;
	static bool		sbEnableD3DFX;
	static bool		sbEnableOGL;
	static bool		sbEnableTS;
};

ATOM									VDVideoDisplayWindow::sChildWindowClass;
bool VDVideoDisplayWindow::sbEnableDX = true;
bool VDVideoDisplayWindow::sbEnableDXOverlay = true;
bool VDVideoDisplayWindow::sbEnableD3D;
bool VDVideoDisplayWindow::sbEnableD3DFX;
bool VDVideoDisplayWindow::sbEnableOGL;
bool VDVideoDisplayWindow::sbEnableTS;

///////////////////////////////////////////////////////////////////////////

void VDVideoDisplaySetFeatures(bool enableDirectX, bool enableDirectXOverlay, bool enableTermServ, bool enableOpenGL, bool enableDirect3D, bool enableDirect3DFX) {
	VDVideoDisplayWindow::sbEnableDX = enableDirectX;
	VDVideoDisplayWindow::sbEnableDXOverlay = enableDirectXOverlay;
	VDVideoDisplayWindow::sbEnableD3D = enableDirect3D;
	VDVideoDisplayWindow::sbEnableD3DFX = enableDirect3DFX;
	VDVideoDisplayWindow::sbEnableOGL = enableOpenGL;
	VDVideoDisplayWindow::sbEnableTS = enableTermServ;
}

///////////////////////////////////////////////////////////////////////////

ATOM VDVideoDisplayWindow::Register() {
	WNDCLASS wc;
	HMODULE hInst = VDGetLocalModuleHandleW32();

	if (!sChildWindowClass) {
		wc.style			= CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc		= StaticChildWndProc;
		wc.cbClsExtra		= 0;
		wc.cbWndExtra		= sizeof(VDVideoDisplayWindow *);
		wc.hInstance		= hInst;
		wc.hIcon			= 0;
		wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground	= 0;
		wc.lpszMenuName		= 0;
		wc.lpszClassName	= "phaeronVideoDisplayChild";

		sChildWindowClass = RegisterClass(&wc);
		if (!sChildWindowClass)
			return NULL;
	}

	wc.style			= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc		= StaticWndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= sizeof(VDVideoDisplayWindow *);
	wc.hInstance		= hInst;
	wc.hIcon			= 0;
	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground	= 0;
	wc.lpszMenuName		= 0;
	wc.lpszClassName	= g_szVideoDisplayControlName;

	return RegisterClass(&wc);
}

IVDVideoDisplay *VDGetIVideoDisplay(VDGUIHandle hwnd) {
	return static_cast<IVDVideoDisplay *>(reinterpret_cast<VDVideoDisplayWindow*>(GetWindowLongPtr((HWND)hwnd, 0)));
}

bool VDRegisterVideoDisplayControl() {
	return 0 != VDVideoDisplayWindow::Register();
}

///////////////////////////////////////////////////////////////////////////

VDVideoDisplayWindow::VDVideoDisplayWindow(HWND hwnd, const CREATESTRUCT& createInfo)
	: mhwnd(hwnd)
	, mhwndChild(NULL)
	, mhOldPalette(0)
	, mpMiniDriver(0)
	, mReinitDisplayTimer(0)
	, mpCB(0)
	, mInhibitRefresh(0)
	, mSyncDelta(0.0f)
	, mFilterMode(kFilterAnySuitable)
	, mbLockAcceleration(false)
	, mbIgnoreMouse(false)
	, mbUseSubrect(false)
	, mpActiveFrame(NULL)
{
	mSource.pixmap.data = 0;

	if (createInfo.hwndParent) {
		DWORD dwThreadId = GetWindowThreadProcessId(createInfo.hwndParent, NULL);
		if (dwThreadId == GetCurrentThreadId())
			mbIgnoreMouse = true;
	}

	VDVideoDisplayManager *vdm = (VDVideoDisplayManager *)createInfo.lpCreateParams;
	vdm->AddClient(this);
}

VDVideoDisplayWindow::~VDVideoDisplayWindow() {
	mpManager->RemoveClient(this);
}

///////////////////////////////////////////////////////////////////////////

#define MYWM_SETSOURCE		(WM_USER + 0x100)
#define MYWM_UPDATE			(WM_USER + 0x101)
#define MYWM_CACHE			(WM_USER + 0x102)
#define MYWM_RESET			(WM_USER + 0x103)
#define MYWM_SETSOURCEMSG	(WM_USER + 0x104)
#define MYWM_PROCESSNEXTFRAME	(WM_USER+0x105)
#define MYWM_DESTROY		(WM_USER + 0x106)
#define MYWM_SETFILTERMODE	(WM_USER + 0x107)

void VDVideoDisplayWindow::SetSourceMessage(const wchar_t *msg) {
	SendMessage(mhwnd, MYWM_SETSOURCEMSG, 0, (LPARAM)msg);
}

void VDVideoDisplayWindow::SetSourcePalette(const uint32 *palette, int count) {
	memcpy(mSourcePalette, palette, 4*std::min<int>(count, 256));
}

bool VDVideoDisplayWindow::SetSource(bool bAutoUpdate, const VDPixmap& src, void *pObject, ptrdiff_t offset, bool bAllowConversion, bool bInterlaced) {
	// We do allow data to be NULL for set-without-load.
	if (src.data)
		VDAssertValidPixmap(src);

	VDVideoDisplaySourceInfo params;

	params.pixmap			= src;
	params.pSharedObject	= pObject;
	params.sharedOffset		= offset;
	params.bAllowConversion	= bAllowConversion;
	params.bPersistent		= pObject != 0;
	params.bInterlaced		= bInterlaced;

	const VDPixmapFormatInfo& info = VDPixmapGetInfo(src.format);
	params.bpp = info.qsize >> info.qhbits;
	params.bpr = (((src.w-1) >> info.qwbits)+1) * info.qsize;

	params.mpCB				= this;

	return 0 != SendMessage(mhwnd, MYWM_SETSOURCE, bAutoUpdate, (LPARAM)&params);
}

bool VDVideoDisplayWindow::SetSourcePersistent(bool bAutoUpdate, const VDPixmap& src, bool bAllowConversion, bool bInterlaced) {
	// We do allow data to be NULL for set-without-load.
	if (src.data)
		VDAssertValidPixmap(src);

	VDVideoDisplaySourceInfo params;

	params.pixmap			= src;
	params.pSharedObject	= NULL;
	params.sharedOffset		= 0;
	params.bAllowConversion	= bAllowConversion;
	params.bPersistent		= true;
	params.bInterlaced		= bInterlaced;

	const VDPixmapFormatInfo& info = VDPixmapGetInfo(src.format);
	params.bpp = info.qsize >> info.qhbits;
	params.bpr = (((src.w-1) >> info.qwbits)+1) * info.qsize;
	params.mpCB				= this;

	return 0 != SendMessage(mhwnd, MYWM_SETSOURCE, bAutoUpdate, (LPARAM)&params);
}

void VDVideoDisplayWindow::SetSourceSubrect(const vdrect32 *r) {
	if (r) {
		mbUseSubrect = true;
		mSourceSubrect = *r;
	} else
		mbUseSubrect = false;

	if (mpMiniDriver) {
		if (!mpMiniDriver->SetSubrect(r))
			SyncReset();
	}
}

void VDVideoDisplayWindow::SetSourceSolidColor(uint32 color) {
	mSolidColorBuffer = color;

	VDPixmap srcbm;
	srcbm.data = &mSolidColorBuffer;
	srcbm.pitch = 0;
	srcbm.format = nsVDPixmap::kPixFormat_XRGB8888;
	srcbm.w = 1;
	srcbm.h = 1;
	SetSourcePersistent(true, srcbm, true, false);
}

void VDVideoDisplayWindow::PostBuffer(VDVideoDisplayFrame *p) {
	p->AddRef();

	bool wasIdle = false;
	vdsynchronized(mMutex) {
		if (!mpMiniDriver || !mpActiveFrame && mPendingFrames.empty())
			wasIdle = true;

		mPendingFrames.push_back(p);
	}

	if (wasIdle)
		PostMessage(mhwnd, MYWM_PROCESSNEXTFRAME, 0, 0);
}

bool VDVideoDisplayWindow::RevokeBuffer(VDVideoDisplayFrame **ppFrame) {
	VDVideoDisplayFrame *p = NULL;
	vdsynchronized(mMutex) {
		if (!mPendingFrames.empty() && mPendingFrames.front() != mPendingFrames.back()) {
			p = mPendingFrames.back();
			mPendingFrames.pop_back();
		} else if (!mIdleFrames.empty()) {
			p = mIdleFrames.front();
			mIdleFrames.pop_front();
		}
	}

	if (!p)
		return false;

	*ppFrame = p;
	return true;
}

void VDVideoDisplayWindow::FlushBuffers() {
	// wait for any current frame to clear
	vdlist<VDVideoDisplayFrame> frames;
	for(;;) {
		bool idle;
		vdsynchronized(mMutex) {
			// clear existing pending frames so the display doesn't start another render
			if (!mPendingFrames.empty())
				frames.splice(frames.end(), mIdleFrames);

			idle = !mpActiveFrame;
		}

		if (idle)
			break;

		::Sleep(1);
		OnTick();
	}

	vdsynchronized(mMutex) {
		frames.splice(frames.end(), mIdleFrames);
		frames.splice(frames.end(), mPendingFrames);
	}

	while(!frames.empty()) {
		VDVideoDisplayFrame *p = frames.back();
		frames.pop_back();

		p->Release();
	}
}

void VDVideoDisplayWindow::Update(int fieldmode) {
	SendMessage(mhwnd, MYWM_UPDATE, fieldmode, 0);
}

void VDVideoDisplayWindow::Cache() {
	SendMessage(mhwnd, MYWM_CACHE, 0, 0);
}

void VDVideoDisplayWindow::Destroy() {
	SendMessage(mhwnd, MYWM_DESTROY, 0, 0);
}

void VDVideoDisplayWindow::Reset() {
	SendMessage(mhwnd, MYWM_RESET, 0, 0);
}

void VDVideoDisplayWindow::SetCallback(IVDVideoDisplayCallback *pCB) {
	mpCB = pCB;
}

void VDVideoDisplayWindow::LockAcceleration(bool locked) {
	mbLockAcceleration = locked;
}

IVDVideoDisplay::FilterMode VDVideoDisplayWindow::GetFilterMode() {
	return mFilterMode;
}

void VDVideoDisplayWindow::SetFilterMode(FilterMode mode) {
	SendMessage(mhwnd, MYWM_SETFILTERMODE, 0, (LPARAM)mode);
}

void VDVideoDisplayWindow::ReleaseActiveFrame() {
	vdsynchronized(mMutex) {
		if (mpActiveFrame) {
			if (mpActiveFrame->mFlags & kAutoFlipFields) {
				mpActiveFrame->mFlags ^= 3;
				mpActiveFrame->mFlags &= ~(kAutoFlipFields | kFirstField);
				mPendingFrames.push_front(mpActiveFrame);
			} else
				mIdleFrames.push_front(mpActiveFrame);

			mpActiveFrame = NULL;
		}
	}
}

void VDVideoDisplayWindow::RequestNextFrame() {
	PostMessage(mhwnd, MYWM_PROCESSNEXTFRAME, 0, 0);
}

void VDVideoDisplayWindow::DispatchNextFrame() {
	vdsynchronized(mMutex) {
		VDASSERT(!mpActiveFrame);
		if (!mPendingFrames.empty()) {
			mpActiveFrame = mPendingFrames.front();
			mPendingFrames.pop_front();
		}
	}

	if (mpActiveFrame) {
		SetSource(false, mpActiveFrame->mPixmap, NULL, 0, mpActiveFrame->mbAllowConversion, mpActiveFrame->mbInterlaced);
		SyncUpdate(mpActiveFrame->mFlags);
	}
}

///////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK VDVideoDisplayWindow::StaticChildWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDVideoDisplayWindow *pThis = (VDVideoDisplayWindow *)GetWindowLongPtr(hwnd, 0);

	switch(msg) {
	case WM_NCCREATE:
		pThis = (VDVideoDisplayWindow *)(((LPCREATESTRUCT)lParam)->lpCreateParams);
		pThis->mhwndChild = hwnd;
		SetWindowLongPtr(hwnd, 0, (DWORD_PTR)pThis);
		break;
	case WM_NCDESTROY:
		SetWindowLongPtr(hwnd, 0, (DWORD_PTR)NULL);
		break;
	}

	return pThis ? pThis->ChildWndProc(msg, wParam, lParam) : DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT VDVideoDisplayWindow::ChildWndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_PAINT:
		OnChildPaint();
		return 0;
	case WM_NCHITTEST:
		if (mbIgnoreMouse)
			return HTTRANSPARENT;
		break;
	case WM_ERASEBKGND:
		return FALSE;
	}

	return DefWindowProc(mhwndChild, msg, wParam, lParam);
}

void VDVideoDisplayWindow::OnChildPaint() {
	++mInhibitRefresh;

	bool bDisplayOK = false;

	if (mpMiniDriver) {
		if (mpMiniDriver->IsValid())
			bDisplayOK = true;
		else if (mSource.pixmap.data && mSource.bPersistent && !mpMiniDriver->Update(IVDVideoDisplayMinidriver::kModeAllFields))
			bDisplayOK = true;
	}

	if (!bDisplayOK) {
		--mInhibitRefresh;
		if (mpCB)
			mpCB->DisplayRequestUpdate(this);
		else if (mSource.pixmap.data && mSource.bPersistent) {
			SyncReset();
			SyncInit(true, false);
		}
		++mInhibitRefresh;
	}

	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(mhwndChild, &ps);

	if (hdc) {
		RECT r;

		GetClientRect(mhwndChild, &r);

		if (mpMiniDriver && mpMiniDriver->IsValid())
			VerifyDriverResult(mpMiniDriver->Paint(hdc, r, IVDVideoDisplayMinidriver::kModeAllFields));

		EndPaint(mhwndChild, &ps);
	}


	--mInhibitRefresh;
}

///////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK VDVideoDisplayWindow::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDVideoDisplayWindow *pThis = (VDVideoDisplayWindow *)GetWindowLongPtr(hwnd, 0);

	switch(msg) {
	case WM_NCCREATE:
		pThis = new VDVideoDisplayWindow(hwnd, *(const CREATESTRUCT *)lParam);
		SetWindowLongPtr(hwnd, 0, (DWORD_PTR)pThis);
		break;
	case WM_NCDESTROY:
		if (pThis)
			pThis->SyncReset();
		delete pThis;
		pThis = NULL;
		SetWindowLongPtr(hwnd, 0, 0);
		break;
	}

	return pThis ? pThis->WndProc(msg, wParam, lParam) : DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT VDVideoDisplayWindow::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_DESTROY:
		SyncReset();

		if (mReinitDisplayTimer) {
			KillTimer(mhwnd, mReinitDisplayTimer);
			mReinitDisplayTimer = 0;
		}

		if (mhOldPalette) {
			DeleteObject(mhOldPalette);
			mhOldPalette = 0;
		}

		break;
	case WM_PAINT:
		OnPaint();
		return 0;
	case MYWM_SETSOURCE:
		return SyncSetSource(wParam != 0, *(const VDVideoDisplaySourceInfo *)lParam);
	case MYWM_UPDATE:
		SyncUpdate((FieldMode)wParam);
		return 0;
	case MYWM_DESTROY:
		SyncReset();
		DestroyWindow(mhwnd);
		return 0;
	case MYWM_RESET:
		SyncReset();
		mSource.pixmap.data = NULL;
		return 0;
	case MYWM_SETSOURCEMSG:
		SyncSetSourceMessage((const wchar_t *)lParam);
		return 0;
	case MYWM_PROCESSNEXTFRAME:
		if (!mpMiniDriver || !mpMiniDriver->IsFramePending()) {
			bool newframe;
			vdsynchronized(mMutex) {
				newframe = !mpActiveFrame;
			}

			if (newframe)
				DispatchNextFrame();
		}
		return 0;
	case MYWM_SETFILTERMODE:
		SyncSetFilterMode((FilterMode)lParam);
		return 0;
	case WM_SIZE:
		if (mhwndChild)
			SetWindowPos(mhwndChild, NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOMOVE|SWP_NOCOPYBITS|SWP_NOZORDER|SWP_NOACTIVATE);
		if (mpMiniDriver)
			VerifyDriverResult(mpMiniDriver->Resize());
		break;
	case WM_TIMER:
		if (wParam == mReinitDisplayTimer) {
			SyncInit(true, false);
			return 0;
		} else {
			if (mpMiniDriver)
				VerifyDriverResult(mpMiniDriver->Tick((int)wParam));
		}
		break;
	case WM_NCHITTEST:
		if (mbIgnoreMouse) {
			LRESULT lr = DefWindowProc(mhwnd, msg, wParam, lParam);

			if (lr != HTCLIENT)
				return lr;
			return HTTRANSPARENT;
		}
		break;
	}

	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDVideoDisplayWindow::OnPaint() {

	++mInhibitRefresh;

	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(mhwnd, &ps);

	if (hdc) {
		RECT r;

		GetClientRect(mhwnd, &r);

		FillRect(hdc, &r, (HBRUSH)(COLOR_3DFACE + 1));
		if (!mMessage.empty()) {
			HGDIOBJ hgo = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
			SetBkMode(hdc, TRANSPARENT);
			VDDrawTextW32(hdc, mMessage.data(), mMessage.size(), &r, DT_CENTER | DT_VCENTER | DT_NOPREFIX | DT_WORDBREAK);
			SelectObject(hdc, hgo);
		}

		EndPaint(mhwnd, &ps);
	}

	--mInhibitRefresh;
}

bool VDVideoDisplayWindow::SyncSetSource(bool bAutoUpdate, const VDVideoDisplaySourceInfo& params) {
	mCachedImage.clear();

	mSource = params;
	mMessage.clear();

	if (mpMiniDriver && mpMiniDriver->ModifySource(mSource)) {
		mSource.bAllowConversion = true;

		if (bAutoUpdate)
			SyncUpdate(kAllFields);
		return true;
	}

	SyncReset();
	if (!SyncInit(bAutoUpdate, true))
		return false;

	mSource.bAllowConversion = true;
	return true;
}

void VDVideoDisplayWindow::SyncReset() {
	if (mpMiniDriver) {
		ShutdownMiniDriver();
		delete mpMiniDriver;
		mpMiniDriver = NULL;

		SetPreciseMode(false);
	}
}

void VDVideoDisplayWindow::SyncSetSourceMessage(const wchar_t *msg) {
	if (!mpMiniDriver && mMessage == msg)
		return;

	SyncReset();
	mSource.pixmap.format = 0;
	mMessage = msg;
	InvalidateRect(mhwnd, NULL, TRUE);
}

bool VDVideoDisplayWindow::SyncInit(bool bAutoRefresh, bool bAllowNonpersistentSource) {
	if (!mSource.pixmap.data || !mSource.pixmap.format)
		return true;

	VDASSERT(!mpMiniDriver);

	bool bIsForeground = VDIsForegroundTask();

	do {
		if (sbEnableTS || !VDIsTerminalServicesClient()) {
			if (mbLockAcceleration || !mSource.bAllowConversion || bIsForeground) {
				// The 3D drivers don't currently support subrects.
				if (sbEnableDX) {
					if (!mbUseSubrect) {
						if (sbEnableOGL) {
							mpMiniDriver = VDCreateVideoDisplayMinidriverOpenGL();
							if (InitMiniDriver())
								break;
							SyncReset();
						}

						if (sbEnableD3D) {
							if (sbEnableD3DFX)
								mpMiniDriver = VDCreateVideoDisplayMinidriverD3DFX();
							else
								mpMiniDriver = VDCreateVideoDisplayMinidriverDX9();
							if (InitMiniDriver())
								break;
							SyncReset();
						}
					}

					mpMiniDriver = VDCreateVideoDisplayMinidriverDirectDraw(sbEnableDXOverlay);
					if (InitMiniDriver())
						break;
					SyncReset();
				}

			} else {
				VDDEBUG_DISP("VideoDisplay: Application in background -- disabling accelerated preview.\n");
			}
		}

		mpMiniDriver = VDCreateVideoDisplayMinidriverGDI();
		if (InitMiniDriver())
			break;

		VDDEBUG_DISP("VideoDisplay: No driver was able to handle the requested format! (%d)\n", mSource.pixmap.format);
		SyncReset();
	} while(false);

	if (mpMiniDriver) {
		mpMiniDriver->SetLogicalPalette(GetLogicalPalette());
		mpMiniDriver->SetFilterMode((IVDVideoDisplayMinidriver::FilterMode)mFilterMode);
		mpMiniDriver->SetSubrect(mbUseSubrect ? &mSourceSubrect : NULL);

		if (mReinitDisplayTimer)
			KillTimer(mhwnd, mReinitDisplayTimer);

		if (bAutoRefresh) {
			if (mSource.bPersistent || bAllowNonpersistentSource)
				SyncUpdate(kAllFields);
			else if (mpCB)
				mpCB->DisplayRequestUpdate(this);
		}
	}

	return mpMiniDriver != 0;
}

void VDVideoDisplayWindow::SyncUpdate(int mode) {
	if (mSource.pixmap.data && !mpMiniDriver) {
		mSyncDelta = 0.0f;
		SyncInit(true, true);
		return;
	}

	if (mpMiniDriver) {
		SetPreciseMode(0 != (mode & kVSync));

		if (mode & kVisibleOnly) {
			bool bVisible = true;

			if (HDC hdc = GetDCEx(mhwnd, NULL, 0)) {
				RECT r;
				GetClientRect(mhwnd, &r);
				bVisible = 0 != RectVisible(hdc, &r);
				ReleaseDC(mhwnd, hdc);
			}

			mode = (FieldMode)(mode & ~kVisibleOnly);

			if (!bVisible)
				return;
		}

		mSyncDelta = 0.0f;

		bool success = mpMiniDriver->Update((IVDVideoDisplayMinidriver::UpdateMode)mode);
		ReleaseActiveFrame();
		if (success) {
			if (!mInhibitRefresh) {
				mpMiniDriver->Refresh((IVDVideoDisplayMinidriver::UpdateMode)mode);
				mSyncDelta = mpMiniDriver->GetSyncDelta();

				if (!mpMiniDriver->IsFramePending())
					RequestNextFrame();
			}
		}
	}
}

void VDVideoDisplayWindow::SyncCache() {
	if (mSource.pixmap.data && mSource.pixmap.data != mCachedImage.data) {
		mCachedImage.assign(mSource.pixmap);

		mSource.pixmap		= mCachedImage;
		mSource.bPersistent	= true;
	}

	if (mSource.pixmap.data && !mpMiniDriver)
		SyncInit(true, true);
}

void VDVideoDisplayWindow::SyncSetFilterMode(FilterMode mode) {
	if (mFilterMode != mode) {
		mFilterMode = mode;

		if (mpMiniDriver) {
			mpMiniDriver->SetFilterMode((IVDVideoDisplayMinidriver::FilterMode)mode);
			InvalidateRect(mhwnd, NULL, FALSE);
			InvalidateRect(mhwndChild, NULL, FALSE);
		}
	}
}

void VDVideoDisplayWindow::OnDisplayChange() {
	HPALETTE hPal = GetPalette();
	if (mhOldPalette && !hPal) {
		if (HDC hdc = GetDC(mhwnd)) {
			SelectPalette(hdc, mhOldPalette, FALSE);
			mhOldPalette = 0;
			ReleaseDC(mhwnd, hdc);
		}
	}
	if (!mhOldPalette && hPal) {
		if (HDC hdc = GetDC(mhwnd)) {
			mhOldPalette = SelectPalette(hdc, hPal, FALSE);
			ReleaseDC(mhwnd, hdc);
		}
	}
	if (!mReinitDisplayTimer) {
		SyncReset();
		if (!SyncInit(true, false))
			mReinitDisplayTimer = SetTimer(mhwnd, kReinitDisplayTimerId, 500, NULL);
	}
}

void VDVideoDisplayWindow::OnForegroundChange(bool bForeground) {
	if (!mbLockAcceleration)
		SyncReset();

	OnRealizePalette();
}

void VDVideoDisplayWindow::OnRealizePalette() {
	if (HDC hdc = GetDC(mhwnd)) {
		HPALETTE newPal = GetPalette();
		HPALETTE pal = SelectPalette(hdc, newPal, FALSE);
		if (!mhOldPalette)
			mhOldPalette = pal;
		RealizePalette(hdc);
		RemapPalette();

		if (mpMiniDriver) {
			mpMiniDriver->SetLogicalPalette(GetLogicalPalette());
			if (mSource.bPersistent)
				SyncUpdate(kAllFields);
			else if (mpCB)
				mpCB->DisplayRequestUpdate(this);
		}

		ReleaseDC(mhwnd, hdc);
	}
}

bool VDVideoDisplayWindow::InitMiniDriver() {
	if (mhwndChild) {
		DestroyWindow(mhwndChild);
		mhwndChild = NULL;
	}

	RECT r;
	GetClientRect(mhwnd, &r);
	mhwndChild = CreateWindowEx(WS_EX_NOPARENTNOTIFY, (LPCTSTR)sChildWindowClass, "", WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, r.right, r.bottom, mhwnd, NULL, VDGetLocalModuleHandleW32(), this);
	if (!mhwndChild)
		return false;

	if (!mpMiniDriver->Init(mhwndChild, mSource)) {
		DestroyWindow(mhwndChild);
		mhwndChild = NULL;
		return false;
	}

	return true;
}

void VDVideoDisplayWindow::ShutdownMiniDriver() {
	if (mhwndChild) {
		if (mpMiniDriver)
			mpMiniDriver->Shutdown();
	}
}

void VDVideoDisplayWindow::VerifyDriverResult(bool result) {
	if (!result) {
		if (mpMiniDriver) {
			ShutdownMiniDriver();
			delete mpMiniDriver;
			mpMiniDriver = 0;
		}

		if (!mReinitDisplayTimer)
			mReinitDisplayTimer = SetTimer(mhwnd, kReinitDisplayTimerId, 500, NULL);
	}
}

///////////////////////////////////////////////////////////////////////////////

vdautoptr<VDVideoDisplayManager> g_pVDVideoDisplayManager;

VDVideoDisplayManager *VDGetVideoDisplayManager() {
	if (!g_pVDVideoDisplayManager) {
		g_pVDVideoDisplayManager = new VDVideoDisplayManager;
		g_pVDVideoDisplayManager->Init();
	}

	return g_pVDVideoDisplayManager;
}

VDGUIHandle VDCreateDisplayWindowW32(uint32 dwExFlags, uint32 dwFlags, int x, int y, int width, int height, VDGUIHandle hwndParent) {
	VDVideoDisplayManager *vdm = VDGetVideoDisplayManager();

	if (!vdm)
		return NULL;

	struct RemoteCreateCall {
		DWORD dwExFlags;
		DWORD dwFlags;
		int x;
		int y;
		int width;
		int height;
		HWND hwndParent;
		VDVideoDisplayManager *vdm;
		HWND hwndResult;

		static void Dispatch(void *p0) {
			RemoteCreateCall *p = (RemoteCreateCall *)p0;
			p->hwndResult = CreateWindowEx(p->dwExFlags, g_szVideoDisplayControlName, "", p->dwFlags, p->x, p->y, p->width, p->height, p->hwndParent, NULL, VDGetLocalModuleHandleW32(), p->vdm);
		}
	} rmc = {dwExFlags, dwFlags | WS_CLIPCHILDREN, x, y, width, height, (HWND)hwndParent, vdm};

	vdm->RemoteCall(RemoteCreateCall::Dispatch, &rmc);
	return (VDGUIHandle)rmc.hwndResult;
}
