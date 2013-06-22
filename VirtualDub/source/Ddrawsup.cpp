//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#define _WIN32_DCOM

#include <windows.h>
#include <objbase.h>

#define DIRECTDRAW_VERSION 0x0300

#include <ddraw.h>

#include "ddrawsup.h"
#include "vbitmap.h"

static BOOL com_initialized = FALSE;
static IDirectDraw2 *lpdd = NULL;
static DDCAPS g_devCaps;
static DDSURFACEDESC ddsdPrimary;
static IDirectDrawSurface *lpddsPrimary = NULL;

HRESULT InitCOM() {
	HMODULE hModOLE = LoadLibrary("ole32.dll");
	HRESULT hr;
	
	if (hModOLE) {
		FARPROC fp = GetProcAddress(hModOLE, "CoInitializeEx");

		if (fp) {
			hr = ((HRESULT (__stdcall *)(void *, DWORD))fp)(NULL, COINIT_MULTITHREADED);
			FreeLibrary(hModOLE);

			return hr;
		}

		FreeLibrary(hModOLE);
	}

	return CoInitialize(NULL);
}

bool DDrawDetect() {
	HRESULT res;

	// first, try to initialize COM...

	_RPT0(0,"DirectDraw support: Initializing COM\n");

	if (FAILED(CoInitialize(NULL)))
//	if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)))
		return false;

	// good, now let's get a IDirectDraw2 object

	_RPT0(0,"DirectDraw support: Getting IDirectDraw2 interface\n");

	res = CoCreateInstance(CLSID_DirectDraw, NULL, CLSCTX_ALL, IID_IDirectDraw2, (LPVOID *)&lpdd);
	if (!FAILED(res))
		res = lpdd->Initialize(NULL);

	if (FAILED(res)) {
		CoUninitialize();
		return false;	// damn!
	}

	// looks like we have directdraw support....

	lpdd->Release();
	lpdd = NULL;

	CoUninitialize();

	return true;
}

BOOL DDrawInitialize(HWND hwnd) {
	HRESULT res;

	// first, try to initialize COM...

	_RPT0(0,"DirectDraw support: Initializing COM\n");

	if (FAILED(CoInitialize(NULL)))
		return FALSE;

	com_initialized = TRUE;

	// good, now let's get a IDirectDraw2 object

	_RPT0(0,"DirectDraw support: Getting IDirectDraw2 interface\n");

	res = CoCreateInstance(CLSID_DirectDraw, NULL, CLSCTX_ALL, IID_IDirectDraw2, (LPVOID *)&lpdd);
	if (!FAILED(res))
		res = lpdd->Initialize(NULL);

	if (FAILED(res)) return FALSE;	// damn!

	// retrieve dev caps

	// why the hell can't I get caps on my laptop!?

#if 0
	g_devCaps.dwSize = sizeof(DDCAPS);

	res = lpdd->GetCaps(&g_devCaps, NULL);
	if (FAILED(res))
		return FALSE;
#endif
	// set cooperative level

	if (DD_OK != lpdd->SetCooperativeLevel(hwnd, DDSCL_NORMAL))
		return FALSE;

	// create primary surface

	_RPT0(0,"Creating primary surface\n");

	memset(&ddsdPrimary, 0, sizeof(ddsdPrimary));
	ddsdPrimary.dwSize = sizeof(ddsdPrimary);
	ddsdPrimary.dwFlags = DDSD_CAPS;
	ddsdPrimary.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
	res = lpdd->CreateSurface(&ddsdPrimary, &lpddsPrimary, NULL );

	if (res != DD_OK)
		return FALSE;

	return TRUE;
}


void DDrawDeinitialize() {
	// Deallocate primary surface

	if (lpddsPrimary) {
		lpddsPrimary->Release();
		lpddsPrimary = NULL;
	}

	// Deallocate the IDirectDraw2 object

	if (lpdd) {
		lpdd->Release();

		lpdd = NULL;
	}

	// Deinitialize COM

	if (com_initialized) {
		CoUninitialize();

		com_initialized = FALSE;
	}

}

IDirectDraw2 *DDrawObtainInterface() {
	return lpdd;
}

IDirectDrawSurface *DDrawObtainPrimary() {
	return lpddsPrimary;
}

///////////////////////////////////////////////////////////////////////////

class DDrawSurface : public IDDrawSurface {
private:
	IDirectDrawSurface *lpdds;

	bool fIsLocked;

public:

	DDrawSurface(IDirectDrawSurface *lpdds);
	~DDrawSurface();
	bool Lock(VBitmap *pvbm);
	bool LockInverted(VBitmap *pvbm);
	void Unlock();
	void SetColorKey(COLORREF rgb);
	void MoveOverlay(long x, long y);
	void SetOverlayPos(RECT *pr);

	IDirectDrawSurface *getSurface();

};

IDDrawSurface *CreateDDrawSurface(IDirectDrawSurface *lpdds) {
	return new DDrawSurface(lpdds);
}

DDrawSurface::DDrawSurface(IDirectDrawSurface *lpdds) {
	this->lpdds = lpdds;
	fIsLocked = false;
}

DDrawSurface::~DDrawSurface() {
	Unlock();
	lpdds->Release();
}

bool DDrawSurface::Lock(VBitmap *pvbm) {
	HRESULT res;
	DDSURFACEDESC ddsd;

	if (fIsLocked)
		return false;

	// *sigh*...

	memset(&ddsd, 0, sizeof ddsd);
	ddsd.dwSize = sizeof ddsd;

	while(DD_OK != (res = lpdds->Lock(NULL, &ddsd, DDLOCK_SURFACEMEMORYPTR|DDLOCK_WRITEONLY/*|DDLOCK_NOSYSLOCK*/, NULL))) {
//	while(DD_OK != (res = lpdds->Lock(NULL, &ddsd, DDLOCK_WRITEONLY | DDLOCK_NOSYSLOCK, NULL))) {

		if (res == DDERR_SURFACELOST) {
			res = lpdds->Restore();

			if (res != DD_OK)
				return false;
		} else if (res == DDERR_WASSTILLDRAWING)
			continue;

		// WinNT is lame -- doesn't like NOSYSLOCK.

		res = lpdds->Lock(NULL, &ddsd, DDLOCK_WRITEONLY, NULL );

		if (res == DD_OK)
			break;

		if (res == DDERR_SURFACELOST) {

			// dammit!!!!

			res = lpdds->Restore();

			if (res != DD_OK)
				return false;

		} else if (res != DDERR_WASSTILLDRAWING) {
			return false;
		}
	}

	// surface locked... translate DDSURFACEDESC struct to VBitmap

	pvbm->data		= (Pixel *)ddsd.lpSurface;
	pvbm->palette	= NULL;
	pvbm->w			= ddsd.dwWidth;
	pvbm->h			= ddsd.dwHeight;
	pvbm->pitch		= ddsd.lPitch;
	pvbm->depth		= ddsd.ddpfPixelFormat.dwRGBBitCount;
	pvbm->modulo	= ddsd.lPitch - (ddsd.dwWidth*ddsd.ddpfPixelFormat.dwRGBBitCount)/8;
	pvbm->size		= pvbm->pitch * pvbm->h;
	pvbm->offset	= 0;

	fIsLocked = true;

	return true;
}

bool DDrawSurface::LockInverted(VBitmap *pvbm) {
	if (!Lock(pvbm))
		return false;

	pvbm->modulo	= pvbm->modulo-2*pvbm->pitch;
	pvbm->pitch		= -pvbm->pitch;
	pvbm->data		= (Pixel *)((char *)pvbm->data + pvbm->size + pvbm->pitch);

	return true;
}

void DDrawSurface::Unlock() {

	if (!fIsLocked)
		return;

	// not like we care if an unlock fails...

	lpdds->Unlock(NULL);

	fIsLocked = false;
}

void DDrawSurface::SetColorKey(COLORREF rgb) {
	DDCOLORKEY ddck;

	ddck.dwColorSpaceLowValue = rgb;
	ddck.dwColorSpaceHighValue = rgb;

	lpdds->SetColorKey(DDCKEY_DESTOVERLAY, &ddck);
}

void DDrawSurface::MoveOverlay(long x, long y) {
	if (FAILED(lpdds->SetOverlayPosition(x, y)))
		VDASSERT(false);
}

void DDrawSurface::SetOverlayPos(RECT *pr) {
	RECT rDst = *pr;
	HRESULT res;

	DDSURFACEDESC ddsdOverlay = { sizeof(DDSURFACEDESC) };
	DDSURFACEDESC ddsdPrimary = { sizeof(DDSURFACEDESC) };
	if (FAILED(lpdds->GetSurfaceDesc(&ddsdOverlay)))
		return;
	if (FAILED(DDrawObtainPrimary()->GetSurfaceDesc(&ddsdPrimary)))
		return;

	do {
		const int dw = rDst.right - rDst.left;
		const int dh = rDst.bottom - rDst.top;

		if (dw<=0 || dh<=0)
			res = lpdds->UpdateOverlay(NULL, lpddsPrimary, NULL, DDOVER_HIDE, NULL);
		else {
			int clipX1		= rDst.left;
			int clipY1		= rDst.top;
			int clipX2		= (int)ddsdPrimary.dwWidth - rDst.right;
			int clipY2		= (int)ddsdPrimary.dwHeight - rDst.bottom;

			if ((clipX1 | clipY1 | clipX2 | clipY2) >= 0)
				res = lpdds->UpdateOverlay(NULL, lpddsPrimary, &rDst, DDOVER_SHOW, NULL);
			else {
				const int sw = (int)ddsdOverlay.dwWidth;
				const int sh = (int)ddsdOverlay.dwHeight;

				clipX1 &= clipX1>>31;
				clipX2 &= clipX2>>31;
				clipY1 &= clipY1>>31;
				clipY2 &= clipY2>>31;

				RECT rDstClipped = {
					rDst.left - clipX1,
					rDst.top - clipY1,
					rDst.right + clipX2,
					rDst.bottom + clipY2
				};

				RECT rSrcClipped = {
					(-clipX1 * sw + (dw>>1)) / dw,
					(-clipY1 * sh + (dh>>1)) / dh,
					sw + (clipX2 * sw + (dw>>1)) / dw,
					sh + (clipY2 * sh + (dh>>1)) / dh,
				};

				res = lpdds->UpdateOverlay(&rSrcClipped, lpddsPrimary, &rDstClipped, DDOVER_SHOW, NULL);
			}
		}

		if (res == DDERR_SURFACELOST)
			res = lpdds->Restore();
		else
			break;
	} while(SUCCEEDED(res));
}

IDirectDrawSurface *DDrawSurface::getSurface() {
	return lpdds;
}
