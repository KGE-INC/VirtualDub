//	VirtualDub 2.0 (Nina) - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee, All Rights Reserved.
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

#include <stdlib.h>

#include <windows.h>
#include <vfw.h>
#include <ddraw.h>
#include <comdef.h>
#include <objbase.h>

#include "capaccel.h"
#include "gui.h"

// Function memcpy is better for large blocks of data, because it does
// nice alignment trix.

#pragma function(memcpy)

///////////////////////////////////////////////////////////////////////////

static bool __declspec(thread) g_bInhibitAVICapPreview;
static bool __declspec(thread) g_bInhibitAVICapInvalidate;

static BOOL __stdcall RydiaDrawDibDraw(HDRAWDIB hdd, HDC hdc, int xDst, int yDst, int dxDst, int dyDst,
							 LPBITMAPINFOHEADER lpbi, LPVOID lpBits, int xSrc, int ySrc,
							 int dxSrc, int dySrc, UINT wFlags) {

	if (g_bInhibitAVICapPreview)
		return true;

	return DrawDibDraw(hdd, hdc, xDst, yDst, dxDst, dyDst, lpbi, lpBits, xSrc, ySrc, dxSrc,
						dySrc, wFlags);

}

static BOOL __stdcall RydiaInvalidateRect(HWND hwnd, const RECT *lpRect, BOOL bErase) {
	if (g_bInhibitAVICapInvalidate)
		return true;

	return InvalidateRect(hwnd, lpRect, bErase);
}

static bool g_bAVICapPatched;

typedef unsigned short ushort;
typedef unsigned long ulong;

struct PEHeader {
	ulong		signature;
	ushort		machine;
	ushort		sections;
	ulong		timestamp;
	ulong		symbol_table;
	ulong		symbols;
	ushort		opthdr_size;
	ushort		characteristics;
};

struct PEImportDirectory {
	ulong		ilt_RVA;
	ulong		timestamp;
	ulong		forwarder_idx;
	ulong		name_RVA;
	ulong		iat_RVA;
};

struct PE32OptionalHeader {
	ushort		magic;					// 0
	char		major_linker_ver;		// 2
	char		minor_linker_ver;		// 3
	ulong		codesize;				// 4
	ulong		idatasize;				// 8
	ulong		udatasize;				// 12
	ulong		entrypoint;				// 16
	ulong		codebase;				// 20
	ulong		database;				// 24
	ulong		imagebase;				// 28
	ulong		section_align;			// 32
	ulong		file_align;				// 36
	ushort		majoros;				// 40
	ushort		minoros;				// 42
	ushort		majorimage;				// 44
	ushort		minorimage;				// 46
	ushort		majorsubsys;			// 48
	ushort		minorsubsys;			// 50
	ulong		reserved;				// 52
	ulong		imagesize;				// 56
	ulong		hdrsize;				// 60
	ulong		checksum;				// 64
	ushort		subsystem;				// 68
	ushort		characteristics;		// 70
	ulong		stackreserve;			// 72
	ulong		stackcommit;			// 76
	ulong		heapreserve;			// 80
	ulong		heapcommit;				// 84
	ulong		loaderflags;			// 88
	ulong		dictentries;			// 92

	// Not part of header, but it's convienent here

	ulong		export_RVA;				// 96
	ulong		export_size;			// 100
	ulong		import_RVA;				// 104
	ulong		import_size;			// 108
};

void RydiaEnableAVICapPreview(bool b) {
	g_bInhibitAVICapPreview = b;
}

void RydiaEnableAVICapInvalidate(bool b) {
	g_bInhibitAVICapInvalidate = b;
}

bool RydiaInitAVICapHotPatch() {
	if (g_bAVICapPatched)
		return true;

	// Attempt to find AVICap.

	const char *pBase = (const char *)LoadLibrary("avicap32.dll");

	if (!pBase)
		return false;

	try {
		do {
			// The PEheader offset is at hmod+0x3c.  Add the size of the optional header
			// to step to the section headers.

			const PEHeader *pHeader = (const PEHeader *)(pBase + ((const long *)pBase)[15]);

			if (pHeader->signature != 'EP')
				return NULL;

			// Verify the PE optional structure.

			const PEImportDirectory *pImportDir;

			if (pHeader->opthdr_size < 104)
				return NULL;

			// Find import header.

			int nImports;

			switch(*(short *)((char *)pHeader + sizeof(PEHeader))) {
			case 0x10b:		// PE32
				{
					const PE32OptionalHeader *pOpt = (PE32OptionalHeader *)((const char *)pHeader + sizeof(PEHeader));

					if (pOpt->dictentries < 2)
						return NULL;

					pImportDir = (const PEImportDirectory *)(pBase + pOpt->import_RVA);
					nImports = pOpt->import_size / sizeof(PEImportDirectory);
				}
				break;

			default:		// reject PE32+
				return NULL;
			}

			// Hmmm... no imports?

			if ((const char *)pImportDir == pBase)
				break;

			// Scan down the import entries.  We are looking for MSVFW32.

			int i;

			for(i=0; i<nImports; ++i) {
				if (!stricmp(pBase + pImportDir[i].name_RVA, "msvfw32.dll"))
					break;
			}

			if (i >= nImports)
				break;

			// Found it.  Start scanning MSVFW32 imports until we find DrawDibDraw.

			const long *pImports = (const long *)(pBase + pImportDir[i].ilt_RVA);
			const void **pVector = (const void **)(pBase + pImportDir[i].iat_RVA);

			while(*pImports) {
				if (*pImports >= 0) {
					const char *pName = pBase + *pImports + 2;

					if (!strcmp(pName, "DrawDibDraw")) {

						// Found it!  Reset the protection.

						DWORD dwOldProtect;

						if (VirtualProtect(pVector, 4, PAGE_EXECUTE_READWRITE, &dwOldProtect)) {
							*pVector = RydiaDrawDibDraw;

							VirtualProtect(pVector, 4, dwOldProtect, &dwOldProtect);
						}

						break;
					}
				}

				++pImports;
				++pVector;
			}

			// Now attempt to intercept InvalidateRect.

			for(i=0; i<nImports; ++i) {
				if (!stricmp(pBase + pImportDir[i].name_RVA, "user32.dll"))
					break;
			}

			if (i >= nImports)
				break;

			// Found it.  Start scanning USER32 imports until we find InvalidateRect.

			pImports = (const long *)(pBase + pImportDir[i].ilt_RVA);
			pVector = (const void **)(pBase + pImportDir[i].iat_RVA);

			while(*pImports) {
				if (*pImports >= 0) {
					const char *pName = pBase + *pImports + 2;

					if (!strcmp(pName, "InvalidateRect")) {

						// Found it!  Reset the protection.

						DWORD dwOldProtect;

						if (VirtualProtect(pVector, 4, PAGE_EXECUTE_READWRITE, &dwOldProtect)) {
							*pVector = RydiaInvalidateRect;

							VirtualProtect(pVector, 4, dwOldProtect, &dwOldProtect);
						}

						break;
					}
				}

				++pImports;
				++pVector;
			}
		} while(false);
	} catch(...) {
	}

	FreeLibrary((HMODULE)pBase);

	return g_bAVICapPatched;
}

///////////////////////////////////////////////////////////////////////////

RydiaDirectDrawContext::RydiaDirectDrawContext()
	: mbCOMInitialized(false)
	, mpdd(NULL)
	, mpddsPrimary(NULL)
	, mpddsOverlay(NULL)
	, mbSupportsColorKey(false)
	, mbSupportsArithStretchY(false)
	, mnColorKey(-1)
{
}

RydiaDirectDrawContext::~RydiaDirectDrawContext() {
	Shutdown();
}

bool RydiaDirectDrawContext::Init() {

	HRESULT hr;

	// Initialize COM

	if (FAILED(hr = CoInitialize(NULL))) {
		guiSetStatus("CoInitialize failed: %08lx", 0, hr);
		return false;
	}

	mbCOMInitialized = true;

	// Create DirectDraw3 object.

	hr = CoCreateInstance(CLSID_DirectDraw, NULL, CLSCTX_ALL, IID_IDirectDraw2,
		(void **)&mpdd);

	if (FAILED(hr)) {
		guiSetStatus("CoCreateInstance failed: %08lx", 0, hr);
		return Shutdown();
	}

	if (FAILED(mpdd->Initialize(NULL))) {
		guiSetStatus("IDirectDraw2::Initialize failed: %08lx", 0, hr);
		return Shutdown();
	}

	// Set cooperative mode.

	hr = mpdd->SetCooperativeLevel(NULL, DDSCL_NORMAL);

	if (FAILED(hr)) {
		guiSetStatus("IDirectDraw2::SetCooperativeLevel failed: %08lx", 0, hr);
		return Shutdown();
	}

	// Create primary surface.

	DDSURFACEDESC ddsd = { sizeof(DDSURFACEDESC) };

	ddsd.dwFlags					= DDSD_CAPS;
	ddsd.ddsCaps.dwCaps				= DDSCAPS_PRIMARYSURFACE;

	IDirectDrawSurface *lpdds;

	hr = mpdd->CreateSurface(&ddsd, &lpdds, NULL);

	if (FAILED(hr)) {
		guiSetStatus("IDirectDraw2::CreateSurface failed: %08lx", 0, hr);
		return Shutdown();
	}

	hr = lpdds->QueryInterface(IID_IDirectDrawSurface3, (void **)&mpddsPrimary);

	lpdds->Release();

	if (FAILED(hr)) {
		guiSetStatus("IDirectDrawSurface::QueryInterface failed: %08lx", 0, hr);
		return Shutdown();
	}

	DDCAPS ddcs;

	ddcs.dwSize = sizeof(DDCAPS);

	nAlignX = 1;
	nAlignW = 1;

	if (SUCCEEDED(mpdd->GetCaps(&ddcs, NULL))) {
		nAlignX = ddcs.dwAlignBoundaryDest;
		nAlignW = ddcs.dwAlignSizeDest;

		if (!nAlignX)
			nAlignX = 1;
		if (!nAlignW)
			nAlignW = 1;

		if (ddcs.dwCKeyCaps & (DDCKEYCAPS_DESTOVERLAYONEACTIVE | DDCKEYCAPS_DESTOVERLAY))
			mbSupportsColorKey = true;

		if (ddcs.dwFXCaps & DDFXCAPS_OVERLAYARITHSTRETCHY)
			mbSupportsArithStretchY = true;
	}

	// All done!

	return true;
}

bool RydiaDirectDrawContext::Shutdown() {
	DestroyOverlay();

	if (mpddsPrimary) {
		mpddsPrimary->Release();
		mpddsPrimary = NULL;
	}

	if (mpdd) {
		mpdd->Release();
		mpdd = NULL;
	}

	if (mbCOMInitialized) {
		CoUninitialize();
		mbCOMInitialized = false;
	}

	return false;
}

bool RydiaDirectDrawContext::isReady() {
	return mbCOMInitialized;
}

bool RydiaDirectDrawContext::CreateOverlay(int w, int h, int bitcount, DWORD fcc) {
	DDSURFACEDESC ddsd = { sizeof(DDSURFACEDESC) };

	switch(fcc) {
	case 'VNUY':								// YUNV -> YUY2
	case '224V':	fcc = '2YUY';				// V422 -> YUY2
	case '2YUY':	nOverlayBPP = 16; break;	// YUY2 (YUV 4:2:2)
	case 'YVYU':	nOverlayBPP = 16; break;	// UYVY (YUV 4:2:2)
	case 'UYVY':	nOverlayBPP = 16; break;	// YVYU (YUV 4:2:2)
	case 'P14Y':	nOverlayBPP = 12; break;	// Y41P (YUV 4:1:1)
	case BI_BITFIELDS:
		ddsd.ddpfPixelFormat.dwRGBBitCount		= 16;
		ddsd.ddpfPixelFormat.dwRBitMask			= 0xf800;
		ddsd.ddpfPixelFormat.dwGBitMask			= 0x07e0;
		ddsd.ddpfPixelFormat.dwBBitMask			= 0x001f;
		ddsd.ddpfPixelFormat.dwRGBAlphaBitMask	= 0x0000;
		break;
	case 0:
		fcc = 0;
		switch(bitcount) {
		case 32:
			ddsd.ddpfPixelFormat.dwRGBBitCount		= 32;
			ddsd.ddpfPixelFormat.dwRBitMask			= 0xf800;
			ddsd.ddpfPixelFormat.dwGBitMask			= 0x07e0;
			ddsd.ddpfPixelFormat.dwBBitMask			= 0x001f;
			ddsd.ddpfPixelFormat.dwRGBAlphaBitMask	= 0x0000;
			break;
		case 24:
			ddsd.ddpfPixelFormat.dwRGBBitCount		= 24;
			ddsd.ddpfPixelFormat.dwRBitMask			= 0x00ff0000;
			ddsd.ddpfPixelFormat.dwGBitMask			= 0x0000ff00;
			ddsd.ddpfPixelFormat.dwBBitMask			= 0x000000ff;
			ddsd.ddpfPixelFormat.dwRGBAlphaBitMask	= 0x00000000;
			break;
		case 16:
			ddsd.ddpfPixelFormat.dwRGBBitCount		= 16;
			ddsd.ddpfPixelFormat.dwRBitMask			= 0x7c00;
			ddsd.ddpfPixelFormat.dwGBitMask			= 0x03e0;
			ddsd.ddpfPixelFormat.dwBBitMask			= 0x001f;
			ddsd.ddpfPixelFormat.dwRGBAlphaBitMask	= 0x0000;
			break;
		}
		break;
	}

	DestroyOverlay();

	ddsd.dwFlags					= DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_CAPS;
	ddsd.dwWidth					= w;
	ddsd.dwHeight					= h;
	ddsd.ddsCaps.dwCaps				= DDSCAPS_VIDEOMEMORY | DDSCAPS_OVERLAY;
	ddsd.ddpfPixelFormat.dwSize		= sizeof(DDPIXELFORMAT);

	if (fcc) {
		ddsd.ddpfPixelFormat.dwFlags		= DDPF_FOURCC;
		ddsd.ddpfPixelFormat.dwFourCC		= fcc;
	} else {
		ddsd.ddpfPixelFormat.dwFlags		= DDPF_RGB;
	}

	for(;;) {
		IDirectDrawSurface *lpdds;

		HRESULT hr = mpdd->CreateSurface(&ddsd, &lpdds, NULL);

		if (SUCCEEDED(hr)) {
			hr = lpdds->QueryInterface(IID_IDirectDrawSurface3, (void **)&mpddsOverlay);

			lpdds->Release();

			if (SUCCEEDED(hr)) {
				DDCOLORKEY ddck;
				DDPIXELFORMAT ddpf;
				mnColorKey = -1;

				ddpf.dwSize = sizeof(DDPIXELFORMAT);

				if (mbSupportsColorKey && SUCCEEDED(mpddsPrimary->GetPixelFormat(&ddpf)) && (ddpf.dwFlags & DDPF_RGB)) {
					mnColorKey = ddpf.dwGBitMask | ddpf.dwBBitMask;

					ddck.dwColorSpaceLowValue = mnColorKey;
					ddck.dwColorSpaceHighValue = mnColorKey;

					if (FAILED(mpddsPrimary->SetColorKey(DDCKEY_DESTOVERLAY, &ddck)))
						mnColorKey = -1;
				}

				return true;
			}
		}

		// Try NVIDIA alternates if we failed.

		switch(fcc) {
		case '2YUY': fcc = 'VNUY'; continue;	// YUY2 -> YUNV
		case 'YVYU': fcc = 'VNYU'; continue;	// UYVY -> UYNV
		}

		return false;
	}
}

void RydiaDirectDrawContext::DestroyOverlay() {
	if (mpddsOverlay) {
		mpddsOverlay->Release();
		mpddsOverlay = NULL;
	}
}

bool RydiaDirectDrawContext::PositionOverlay(int x, int y, int w, int h) {
	RECT r;
	DWORD dwFlags = DDOVER_SHOW;
	DDOVERLAYFX ddof;

	if (!mpddsOverlay)
		return false;

	if (mbSupportsColorKey)
		dwFlags |= DDOVER_KEYDEST;

	if (mbSupportsArithStretchY) {
		ddof.dwSize = sizeof(DDOVERLAYFX);
		ddof.dwDDFX = DDOVERFX_ARITHSTRETCHY;
		dwFlags |= DDOVER_DDFX;
	}

	x = (x+nAlignX-1);
	r.left = x - x % nAlignX;
	r.right = r.left + w - w % nAlignW;
	r.top = y;
	r.bottom = y+h;

	HRESULT hr;
	if (FAILED(hr = mpddsOverlay->UpdateOverlay(NULL, mpddsPrimary, &r, dwFlags, &ddof))) {
		return false;
	}

	return true;
}

bool RydiaDirectDrawContext::LockAndLoad(const void *src0, int yoffset, int yskip) {
	HRESULT hr;
	DDSURFACEDESC ddsd;

	ddsd.dwSize = sizeof ddsd;

	for(;;) {
		hr = mpddsOverlay->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY | DDLOCK_SURFACEMEMORYPTR | DDLOCK_NOSYSLOCK, NULL);

		// Windows NT doesn't take NOSYSLOCK. #$*(#&$(

		if (hr == DDERR_INVALIDPARAMS)
			hr = mpddsOverlay->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY | DDLOCK_SURFACEMEMORYPTR, NULL);

		if (hr == DDERR_SURFACELOST) {
			mpddsOverlay->Restore();
			continue;
		}

		break;
	}

	if (FAILED(hr))
		return false;

	const char *src = (const char *)src0;
	char *dst = (char *)ddsd.lpSurface;
	long dpitch = ddsd.lPitch;
	long spitch = ((nOverlayBPP * ddsd.dwWidth + 31) >> 5)*4;
	long len = (nOverlayBPP * ddsd.dwWidth)>>3;

	src += spitch*yoffset;

	for(int y=0; y<ddsd.dwHeight; ++y) {
		memcpy(dst, src, len);
		dst += dpitch;
		src += spitch*yskip;
	}

	mpddsOverlay->Unlock(NULL);

	return true;
}
