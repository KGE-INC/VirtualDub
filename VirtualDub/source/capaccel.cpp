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

#include "stdafx.h"

#include <windows.h>
#include <vfw.h>

#include "capaccel.h"
#include "gui.h"

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

	__try {
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
	} __except(1) {
	}

	FreeLibrary((HMODULE)pBase);

	return g_bAVICapPatched;
}
