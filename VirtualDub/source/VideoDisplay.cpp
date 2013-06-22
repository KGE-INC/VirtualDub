//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#include "stdafx.h"
#include <vector>
#include <algorithm>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <vd2/system/vdtypes.h>

#include "VideoDisplay.h"
#include "VideoDisplayDrivers.h"

extern HINSTANCE g_hInst;

static const char g_szVideoDisplayControlName[] = "phaeronVideoDisplay";

extern void VDMemcpyRect(void *dst, ptrdiff_t dststride, const void *src, ptrdiff_t srcstride, size_t w, size_t h);

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
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDisplayWindow : public IVDVideoDisplay {
public:
	static ATOM Register();

protected:
	VDVideoDisplayWindow(HWND hwnd);
	~VDVideoDisplayWindow();

	bool SetSource(const void *data, ptrdiff_t stride, int w, int h, int format, void *pSharedObject, ptrdiff_t sharedOffset, bool bAllowConversion, bool bInterlaced);
	void Update(FieldMode);
	void Reset();
	void Cache();
	void SetCallback(IVDVideoDisplayCallback *pcb);
	void LockAcceleration(bool locked);

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void OnPaint();
	bool SyncSetSource(const VDVideoDisplaySourceInfo& params);
	void SyncReset();
	bool SyncInit();
	void SyncUpdate(FieldMode);
	void SyncCache();
	void SyncDisplayChange();
	void SyncForegroundChange(bool bForeground);
	void SyncRealizePalette();
	void VerifyDriverResult(bool result);
	static void StaticRemapPalette();
	static bool StaticCheckPaletted();
	static void StaticCreatePalette();

	static LRESULT CALLBACK StaticLookoutWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	enum {
		kReinitDisplayTimerId = 500
	};

	HWND		mhwnd;
	HPALETTE	mhOldPalette;

	VDVideoDisplaySourceInfo	mSource;

	IVDVideoDisplayMinidriver *mpMiniDriver;
	UINT	mReinitDisplayTimer;

	IVDVideoDisplayCallback		*mpCB;
	int		mInhibitRefresh;

	bool	mbLockAcceleration;

	typedef std::vector<char> tCachedImage;
	tCachedImage		mCachedImage;

	typedef std::vector<VDVideoDisplayWindow *> tDisplayWindows;
	static tDisplayWindows	sDisplayWindows;
	static HWND				shwndLookout;
	static HPALETTE			shPalette;
	static uint8			sLogicalPalette[256];
};

VDVideoDisplayWindow::tDisplayWindows	VDVideoDisplayWindow::sDisplayWindows;
HWND									VDVideoDisplayWindow::shwndLookout;
HPALETTE								VDVideoDisplayWindow::shPalette;
uint8									VDVideoDisplayWindow::sLogicalPalette[256];

///////////////////////////////////////////////////////////////////////////

ATOM VDVideoDisplayWindow::Register() {
	WNDCLASS wc;

	wc.style			= 0;
	wc.lpfnWndProc		= StaticLookoutWndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= 0;
	wc.hInstance		= g_hInst;
	wc.hIcon			= 0;
	wc.hCursor			= 0;
	wc.hbrBackground	= 0;
	wc.lpszMenuName		= 0;
	wc.lpszClassName	= "phaeronVideoDisplayLookout";

	if (!RegisterClass(&wc))
		return NULL;

	wc.style			= CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc		= StaticWndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= sizeof(VDVideoDisplayWindow *);
	wc.hInstance		= g_hInst;
	wc.hIcon			= 0;
	wc.hCursor			= 0;
	wc.hbrBackground	= 0;
	wc.lpszMenuName		= 0;
	wc.lpszClassName	= g_szVideoDisplayControlName;

	return RegisterClass(&wc);
}

IVDVideoDisplay *VDGetIVideoDisplay(HWND hwnd) {
	return static_cast<IVDVideoDisplay *>(reinterpret_cast<VDVideoDisplayWindow*>(GetWindowLongPtr(hwnd, 0)));
}

bool VDRegisterVideoDisplayControl() {
	return 0 != VDVideoDisplayWindow::Register();
}

///////////////////////////////////////////////////////////////////////////

VDVideoDisplayWindow::VDVideoDisplayWindow(HWND hwnd)
	: mhwnd(hwnd)
	, mhOldPalette(0)
	, mpMiniDriver(0)
	, mReinitDisplayTimer(0)
	, mpCB(0)
	, mInhibitRefresh(0)
	, mbLockAcceleration(false)
{
	mSource.data = 0;

	sDisplayWindows.push_back(this);
	if (!shwndLookout)
		shwndLookout = CreateWindow("phaeronVideoDisplayLookout", "", WS_POPUP, 0, 0, 0, 0, NULL, NULL, g_hInst, 0);
}

VDVideoDisplayWindow::~VDVideoDisplayWindow() {
	tDisplayWindows::iterator it(std::find(sDisplayWindows.begin(), sDisplayWindows.end(), this));

	if (it != sDisplayWindows.end()) {
		*it = sDisplayWindows.back();
		sDisplayWindows.pop_back();

		if (sDisplayWindows.empty()) {
			if (shPalette) {
				DeleteObject(shPalette);
				shPalette = 0;
			}

			VDASSERT(shwndLookout);
			DestroyWindow(shwndLookout);
			shwndLookout = NULL;
		}
	} else {
		VDASSERT(false);
	}
}

///////////////////////////////////////////////////////////////////////////

#define MYWM_SETSOURCE		(WM_USER + 0x100)
#define MYWM_UPDATE			(WM_USER + 0x101)
#define MYWM_CACHE			(WM_USER + 0x102)
#define MYWM_RESET			(WM_USER + 0x103)

bool VDVideoDisplayWindow::SetSource(const void *data, ptrdiff_t stride, int w, int h, int format, void *pObject, ptrdiff_t offset, bool bAllowConversion, bool bInterlaced) {
	VDVideoDisplaySourceInfo params;

	params.data		= data;
	params.stride	= stride;
	params.w		= w;
	params.h		= h;
	params.format	= format;
	params.pSharedObject	= pObject;
	params.sharedOffset		= offset;
	params.bAllowConversion	= bAllowConversion;
	params.bPersistent		= false;
	params.bInterlaced		= bInterlaced;

	switch(format) {
	case kFormatRGB1555:
	case kFormatRGB565:
		params.bpp = 2;
		params.bpr = 2 * w;
		break;
	case kFormatRGB888:
		params.bpp = 3;
		params.bpr = 3 * w;
		break;
	case kFormatRGB8888:
		params.bpp = 4;
		params.bpr = 4 * w;
		break;
	case kFormatYUV422_YUYV:
	case kFormatYUV422_UYVY:
		params.bpp = 2;
		params.bpr = 2 * (w + (w&1));
		break;
	}

	return 0 != SendMessage(mhwnd, MYWM_SETSOURCE, bAllowConversion, (LPARAM)&params);
}

void VDVideoDisplayWindow::Update(FieldMode fieldmode) {
	SendMessage(mhwnd, MYWM_UPDATE, fieldmode, 0);
}

void VDVideoDisplayWindow::Cache() {
	SendMessage(mhwnd, MYWM_CACHE, 0, 0);
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

///////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK VDVideoDisplayWindow::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDVideoDisplayWindow *pThis = (VDVideoDisplayWindow *)GetWindowLongPtr(hwnd, 0);

	switch(msg) {
	case WM_NCCREATE:
		pThis = new VDVideoDisplayWindow(hwnd);
		SetWindowLongPtr(hwnd, 0, (DWORD_PTR)pThis);
		break;
	case WM_NCDESTROY:
		if (pThis)
			pThis->SyncReset();
		delete pThis;
		SetWindowLongPtr(hwnd, 0, 0);
		break;
	}

	return pThis ? pThis->WndProc(msg, wParam, lParam) : DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT VDVideoDisplayWindow::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_DESTROY:
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
		return SyncSetSource(*(const VDVideoDisplaySourceInfo *)lParam);
	case MYWM_UPDATE:
		SyncUpdate((FieldMode)wParam);
		return 0;
	case MYWM_RESET:
		SyncReset();
		return 0;
	case WM_SIZE:
		if (mpMiniDriver)
			VerifyDriverResult(mpMiniDriver->Resize());
		break;
	case WM_TIMER:
		if (wParam == mReinitDisplayTimer) {
			SyncInit();
			return 0;
		} else {
			if (mpMiniDriver)
				VerifyDriverResult(mpMiniDriver->Tick((int)wParam));
		}
		break;
	case WM_NCHITTEST:
		return HTTRANSPARENT;
	}

	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDVideoDisplayWindow::OnPaint() {

	++mInhibitRefresh;

	if (mpMiniDriver && !mpMiniDriver->IsValid()) {
		if (!mSource.bPersistent || !mpMiniDriver->Update(IVDVideoDisplayMinidriver::kAllFields)) {
			if (mpCB)
				mpCB->DisplayRequestUpdate(this);
		}
	}

	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(mhwnd, &ps);

	if (hdc) {
		RECT r;

		GetClientRect(mhwnd, &r);

		if (mpMiniDriver)
			VerifyDriverResult(mpMiniDriver->Paint(hdc, r));
		else
			FillRect(hdc, &r, (HBRUSH)(COLOR_3DFACE + 1));

		EndPaint(mhwnd, &ps);
	}

	--mInhibitRefresh;
}

bool VDVideoDisplayWindow::SyncSetSource(const VDVideoDisplaySourceInfo& params) {
	tCachedImage().swap(mCachedImage);

	mSource = params;

	if (mpMiniDriver && mpMiniDriver->ModifySource(mSource))
		return true;

	SyncReset();
	return SyncInit();
}

void VDVideoDisplayWindow::SyncReset() {
	if (mpMiniDriver) {
		mpMiniDriver->Shutdown();
		delete mpMiniDriver;
		mpMiniDriver = 0;
	}
}

bool VDVideoDisplayWindow::SyncInit() {
	if (!mSource.data)
		return true;

	VDASSERT(!mpMiniDriver);

	bool bIsForeground = VDIsForegroundTask();

	do {
		if (mbLockAcceleration || bIsForeground) {
#if 0
			mpMiniDriver = VDCreateVideoDisplayMinidriverOpenGL();
			if (mpMiniDriver->Init(mhwnd, mSource))
				break;
			SyncReset();
#endif

			mpMiniDriver = VDCreateVideoDisplayMinidriverDirectDraw();
			if (mpMiniDriver->Init(mhwnd, mSource))
				break;
			SyncReset();

		} else {
			VDDEBUG("VideoDisplay: Application in background -- disabling accelerated preview.\n");
		}

		mpMiniDriver = VDCreateVideoDisplayMinidriverGDI();
		if (mpMiniDriver->Init(mhwnd, mSource))
			break;

		VDDEBUG("VideoDisplay: No driver was able to handle the requested format!\n");
		SyncReset();
	} while(false);

	if (mpMiniDriver) {
		if (mReinitDisplayTimer)
			KillTimer(mhwnd, mReinitDisplayTimer);

		if (StaticCheckPaletted()) {
			if (!shPalette)
				StaticCreatePalette();

			SyncRealizePalette();
		} else {
			if (mSource.bPersistent)
				SyncUpdate(kAllFields);
			else if (mpCB)
				mpCB->DisplayRequestUpdate(this);
		}
	}

	return mpMiniDriver != 0;
}

void VDVideoDisplayWindow::SyncUpdate(FieldMode mode) {
	if (mSource.data && !mpMiniDriver)
		SyncInit();

	if (mpMiniDriver) {
		if (mpMiniDriver->Update((IVDVideoDisplayMinidriver::FieldMode)mode)) {
			if (!mInhibitRefresh)
				mpMiniDriver->Refresh((IVDVideoDisplayMinidriver::FieldMode)mode);
		}
	}
}

void VDVideoDisplayWindow::SyncCache() {
	if (mSource.data) {
		ptrdiff_t bpr8 = (mSource.bpr + 7) & ~7;

		mCachedImage.resize(bpr8 * mSource.h);

		VDMemcpyRect(&mCachedImage[0], bpr8, mSource.data, mSource.stride, mSource.bpr, mSource.h);

		mSource.data		= &mCachedImage[0];
		mSource.stride		= bpr8;
		mSource.bPersistent	= true;
	}

	if (mSource.data && !mpMiniDriver)
		SyncInit();
}

void VDVideoDisplayWindow::SyncDisplayChange() {
	if (mhOldPalette && !shPalette) {
		if (HDC hdc = GetDC(mhwnd)) {
			SelectPalette(hdc, mhOldPalette, FALSE);
			mhOldPalette = 0;
			ReleaseDC(mhwnd, hdc);
		}
	}
	if (!mhOldPalette && shPalette) {
		if (HDC hdc = GetDC(mhwnd)) {
			mhOldPalette = SelectPalette(hdc, shPalette, FALSE);
			ReleaseDC(mhwnd, hdc);
		}
	}
	if (!mReinitDisplayTimer) {
		SyncReset();
		if (!SyncInit())
			mReinitDisplayTimer = SetTimer(mhwnd, kReinitDisplayTimerId, 500, NULL);
	}
}

void VDVideoDisplayWindow::SyncForegroundChange(bool bForeground) {
	if (!mbLockAcceleration)
		SyncReset();

	SyncRealizePalette();
}

void VDVideoDisplayWindow::SyncRealizePalette() {
	if (HDC hdc = GetDC(mhwnd)) {
		HPALETTE pal = SelectPalette(hdc, shPalette, FALSE);
		if (!mhOldPalette)
			mhOldPalette = pal;
		RealizePalette(hdc);
		StaticRemapPalette();

		if (mpMiniDriver) {
			mpMiniDriver->SetLogicalPalette(sLogicalPalette);
			if (mSource.bPersistent)
				SyncUpdate(kAllFields);
			else if (mpCB)
				mpCB->DisplayRequestUpdate(this);
		}

		ReleaseDC(mhwnd, hdc);
	}
}

void VDVideoDisplayWindow::VerifyDriverResult(bool result) {
	if (!result) {
		if (mpMiniDriver) {
			mpMiniDriver->Shutdown();
			delete mpMiniDriver;
			mpMiniDriver = 0;
		}

		if (!mReinitDisplayTimer)
			mReinitDisplayTimer = SetTimer(mhwnd, kReinitDisplayTimerId, 500, NULL);
	}
}

void VDVideoDisplayWindow::StaticRemapPalette() {
	PALETTEENTRY pal[216];
	struct {
		LOGPALETTE hdr;
		PALETTEENTRY palext[255];
	} physpal;

	physpal.hdr.palVersion = 0x0300;
	physpal.hdr.palNumEntries = 256;

	int i;

	for(i=0; i<216; ++i) {
		pal[i].peRed	= (i / 36) * 51;
		pal[i].peGreen	= ((i%36) / 6) * 51;
		pal[i].peBlue	= (i%6) * 51;
	}

	for(i=0; i<256; ++i) {
		physpal.hdr.palPalEntry[i].peRed	= 0;
		physpal.hdr.palPalEntry[i].peGreen	= 0;
		physpal.hdr.palPalEntry[i].peBlue	= i;
		physpal.hdr.palPalEntry[i].peFlags	= PC_EXPLICIT;
	}

	if (HDC hdc = GetDC(0)) {
		GetSystemPaletteEntries(hdc, 0, 256, physpal.hdr.palPalEntry);
		ReleaseDC(0, hdc);
	}

	if (HPALETTE hpal = CreatePalette(&physpal.hdr)) {
		for(i=0; i<252; ++i) {
			sLogicalPalette[i] = GetNearestPaletteIndex(hpal, RGB(pal[i].peRed, pal[i].peGreen, pal[i].peBlue));
#if 0
			VDDEBUG("[%3d %3d %3d] -> [%3d %3d %3d] : error(%+3d %+3d %+3d)\n"
					, pal[i].peRed
					, pal[i].peGreen
					, pal[i].peBlue
					, physpal.hdr.palPalEntry[sLogicalPalette[i]].peRed
					, physpal.hdr.palPalEntry[sLogicalPalette[i]].peGreen
					, physpal.hdr.palPalEntry[sLogicalPalette[i]].peBlue
					, pal[i].peRed - physpal.hdr.palPalEntry[sLogicalPalette[i]].peRed
					, pal[i].peGreen - physpal.hdr.palPalEntry[sLogicalPalette[i]].peGreen
					, pal[i].peBlue - physpal.hdr.palPalEntry[sLogicalPalette[i]].peBlue);
#endif
		}

		DeleteObject(hpal);
	}
}

bool VDVideoDisplayWindow::StaticCheckPaletted() {
	bool bPaletted = false;

	if (HDC hdc = GetDC(0)) {
		if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
			bPaletted = true;
		ReleaseDC(0, hdc);
	}

	return bPaletted;
}

void VDVideoDisplayWindow::StaticCreatePalette() {
	struct {
		LOGPALETTE hdr;
		PALETTEENTRY palext[255];
	} pal;

	pal.hdr.palVersion = 0x0300;
	pal.hdr.palNumEntries = 216;

	for(int i=0; i<216; ++i) {
		pal.hdr.palPalEntry[i].peRed	= (i / 36) * 51;
		pal.hdr.palPalEntry[i].peGreen	= ((i%36) / 6) * 51;
		pal.hdr.palPalEntry[i].peBlue	= (i%6) * 51;
		pal.hdr.palPalEntry[i].peFlags	= 0;
	}

	shPalette = CreatePalette(&pal.hdr);
}

LRESULT CALLBACK VDVideoDisplayWindow::StaticLookoutWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_DISPLAYCHANGE:
			{
				bool bPaletted = StaticCheckPaletted();

				if (bPaletted && !shPalette) {
					StaticCreatePalette();
				}

				for(tDisplayWindows::const_iterator it(sDisplayWindows.begin()), itEnd(sDisplayWindows.end()); it!=itEnd; ++it) {
					VDVideoDisplayWindow *p = *it;

					p->SyncDisplayChange();
				}

				if (!bPaletted && shPalette) {
					DeleteObject(shPalette);
					shPalette = 0;
				}
			}
			break;
		case WM_ACTIVATEAPP:
			{
				for(tDisplayWindows::const_iterator it(sDisplayWindows.begin()), itEnd(sDisplayWindows.end()); it!=itEnd; ++it) {
					VDVideoDisplayWindow *p = *it;

					p->SyncForegroundChange(wParam != 0);
				}
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
					for(tDisplayWindows::const_iterator it(sDisplayWindows.begin()), itEnd(sDisplayWindows.end()); it!=itEnd; ++it) {
						VDVideoDisplayWindow *p = *it;

						p->SyncRealizePalette();
					}
				}
			}
			break;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}
