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

#include <vd2/system/error.h>
#include "VBitmap.h"
#include "crash.h"

#include "filters.h"

#include "FilterSystem.h"

extern FilterFunctions g_filterFuncs;

///////////////////////////////////////////////////////////////////////////

class FilterSystemBitmap : public VFBitmap {
public:
	int				buffer;
	LONG			lMapOffset;
};

///////////////////////////////////////////////////////////////////////////

FilterSystem::FilterSystem() {
	bitmap = NULL;
	dwFlags = 0;
	lpBuffer = NULL;
	hFileShared = NULL;
	hdcSrc = NULL;
	hbmSrc = NULL;
	listFilters = NULL;
	fSharedWindow = false;
}

FilterSystem::~FilterSystem() {
	DeinitFilters();
	DeallocateBuffers();
	delete[] bitmap;
}

// prepareLinearChain(): init bitmaps in a linear filtering system

void FilterSystem::prepareLinearChain(List *listFA, Pixel *src_pal, PixDim src_width, PixDim src_height, int src_depth, int dest_depth) {
	FilterInstance *fa;
	DWORD flags, flags_accum=0;
	int last_bufferid = 0;

	if (dwFlags & FILTERS_INITIALIZED)
		return;

	DeallocateBuffers();

	AllocateVBitmaps(4);

	bitmap[0].w					= src_width;
	bitmap[0].h					= src_height;
	bitmap[0].palette			= src_pal;
	bitmap[0].buffer			= 0;
	bitmap[0].depth				= 32;
	bitmap[0].pitch				= bitmap[0].PitchAlign8();
	bitmap[0].modulo			= bitmap[0].Modulo();
	bitmap[0].size				= bitmap[0].pitch * bitmap[0].h;
	bitmap[0].offset			= 0;
	bitmap[0].buffer			= 0;

	bmLast = &bitmap[0];

	fa = (FilterInstance *)listFA->tail.next;

//	fSharedWindow = false;		CAN'T - NEED FOR OUTPUT DISPLAY
	fSharedWindow = true;
	lAdditionalBytes = 0;
	nFrameLag = 0;

	while(fa->next) {
		fa->src			= *bmLast;

		fa->origw		= fa->src.w;
		fa->origh		= fa->src.h;

		fa->src.w		-= fa->x1 + fa->x2;
		fa->src.h		-= fa->y1 + fa->y2;
		fa->src.depth	= 32;
		fa->src.modulo	= fa->src.Modulo();
		fa->src.offset	+= fa->y2 * fa->src.pitch + fa->x1*4;
		fa->src.size	= fa->src.pitch * fa->src.h;

		fa->realLast.w		= fa->src.w;
		fa->realLast.h		= fa->src.h;
		fa->realLast.offset	= 0;
		fa->realLast.depth	= 32;
		fa->realLast.AlignTo8();
		fa->realLast.dwFlags= 0;

		fa->dst			= fa->src;
		fa->dst.offset	= 0;

		fa->src.dwFlags	= 0;
		fa->src.hdc		= NULL;

		fa->dst.dwFlags	= 0;
		fa->dst.hdc		= NULL;

		fa->srcbuf		= last_bufferid;

		if (fa->filter->paramProc) {
			VDCHECKPOINT;
			flags = fa->filter->paramProc(fa, &g_filterFuncs);
			VDCHECKPOINT;

			if (flags & FILTERPARAM_NEEDS_LAST) {
				lAdditionalBytes += fa->realLast.size;
			}
		} else
			flags = FILTERPARAM_SWAP_BUFFERS;

		nFrameLag += (flags>>16);

		fa->flags = flags;

		flags &= 0x0000ffff;
		flags_accum |= flags;

		fa->dst.modulo	= fa->dst.Modulo();
		fa->dst.size	= fa->dst.Size();
		fa->dstbuf		= fa->srcbuf;

		if (flags & FILTERPARAM_SWAP_BUFFERS) {
			// Alternate between buffers 1 and 2

			if (!fa->srcbuf)
				fa->dstbuf = 1;
			else
				fa->dstbuf = 3-fa->srcbuf;
		}

		if (fa->dst.size+fa->dst.offset > bitmap[fa->dstbuf].size)
			bitmap[fa->dstbuf].size = fa->dst.size+fa->dst.offset;

		bmLast = &fa->dst;
		last_bufferid = fa->dstbuf;

		// Check if the filter needs a display context.  This requires us to
		// allocate a shared window instead of a private memory buffer.
		// Far more hazardous under Windows 95/98.

		if ((fa->src.dwFlags | fa->dst.dwFlags) & VFBitmap::NEEDS_HDC)
			fSharedWindow = true;

		// Next filter.

		fa = (FilterInstance *)fa->next;
	}

	// 2/3) Temp buffers
#if 0
	bitmap[1].depth			= 32;
	bitmap[1].w				= temp_width;
	bitmap[1].h				= temp_height;
	bitmap[1].AlignTo8();

	bitmap[2].depth			= 32;
	bitmap[2].w				= temp_width;
	bitmap[2].h				= temp_height;
	bitmap[2].AlignTo8();
#endif

	bitmap[3].w				= bmLast->w;
	bitmap[3].h				= bmLast->h;
	bitmap[3].depth			= dest_depth;
	bitmap[3].AlignTo4();

	fa = (FilterInstance *)listFA->tail.next;

	if (flags_accum & FILTERPARAM_NEEDS_LAST)
		if (!(fa->flags & FILTERPARAM_SWAP_BUFFERS)) {
			fa->srcbuf=2;
			fa->dstbuf=2;
			if (bitmap[2].size < bitmap[0].size) bitmap[2].size=bitmap[0].size;
		}
}

// initLinearChain(): prepare for a linear filtering system

void FilterSystem::initLinearChain(List *listFA, Pixel *src_pal, PixDim src_width, PixDim src_height, int src_depth, int dest_depth) {
	FilterInstance *fa;
	long lRequiredSize;
	long lLastBufPtr = 0; 
	int i;

	DeinitFilters();
	DeallocateBuffers();

	listFilters = listFA;

	// buffers required:
	//
	// 1) Input buffer (8/16/24/32 bits)
	// 2) Temp buffer #1 (32 bits)
	// 3) Temp buffer #2 (32 bits)
	// 4) Output buffer (8/16/24/32 bits)
	// 5) [Optional] Last input buffer (32 bits)
	//
	// All temporary buffers must be aligned on an 8-byte boundary, and all
	// pitches must be a multiple of 8 bytes.  The exceptions are the source
	// and destination buffers, which may have pitches that are only 4-byte
	// multiples.

	prepareLinearChain(listFA, src_pal, src_width, src_height, src_depth, dest_depth);

	lRequiredSize = lAdditionalBytes;

	for(i=0; i<iBitmapCount; i++) {

		_RPT4(0,"Buffer %d: %dx%d (%d bits), ", i, bitmap[i].w, bitmap[i].h, bitmap[i].depth);
		_RPT2(0,"pitch %d, offset %ld\n", bitmap[i].pitch, lRequiredSize);

		bitmap[i].buffer		= i;
		bitmap[i].lMapOffset	= lRequiredSize;

		lRequiredSize		+= bitmap[i].size;

		// align lRequiredSize up to next 8

		lRequiredSize = (lRequiredSize+7) & -8;

	}

	AllocateBuffers(lRequiredSize);

	for(i=0; i<iBitmapCount; i++) {
		bitmap[i].data			= (Pixel *)(lpBuffer + bitmap[i].lMapOffset);
	}

	fa = (FilterInstance *)listFA->tail.next;

	while(fa->next) {
		_RPT2(0,"src/data: %d/%d\n", fa->srcbuf, fa->dstbuf);
		fa->src.data		= (Pixel *)((char *)bitmap[fa->srcbuf].data + fa->src.offset);
		fa->dst.data		= (Pixel *)((char *)bitmap[fa->dstbuf].data + fa->dst.offset);

//		fa->src.hMapObject	= hFileShared;
//		fa->src.lMapOffset	= bitmap[fa->src.buffer].lMapOffset + fa->src.offset;
//		fa->dst.hMapObject	= hFileShared;
//		fa->dst.lMapOffset	= bitmap[fa->dst.buffer].lMapOffset + fa->dst.offset;

		fa = (FilterInstance *)fa->next;
	}

	// Does the first filter require a display context?

	HDC hdcDisplay = NULL;

	fa = (FilterInstance *)listFA->tail.next;

	try {
		if (fa->next && (fa->src.dwFlags & VFBitmap::NEEDS_HDC)) {
			BITMAPINFOHEADER bih;
			void *mem;

			bitmap[0].MakeBitmapHeader(&bih);

			if (!hdcDisplay) {
				hdcDisplay = CreateDC("DISPLAY", NULL, NULL, 0);

				if (!hdcDisplay)
					throw MyMemoryError();
			}

			hdcSrc = CreateCompatibleDC(hdcDisplay);

			if (!hdcSrc)
				throw MyMemoryError();

			hbmSrc = CreateDIBSection(hdcSrc, (BITMAPINFO *)&bih, DIB_RGB_COLORS, &mem, hFileShared, 0);

			if (!hbmSrc)
				throw MyMemoryError();

			hgoSrc = SelectObject(hdcSrc, hbmSrc);

			fa->src.hdc = hdcSrc;
		}

		// Check all subsequent filters; copy over display contexts from destinations
		// to sources and create new destination DCs as necessary

		fa = (FilterInstance *)listFA->tail.next;

		while(fa->next) {
			FilterInstance *fa_next = (FilterInstance *)fa->next;

			if ((fa->dst.dwFlags & VFBitmap::NEEDS_HDC) || (fa_next->next && (fa_next->src.dwFlags & VFBitmap::NEEDS_HDC))) {
				BITMAPINFOHEADER bih;

				fa->dst.MakeBitmapHeader(&bih);

				if (!hdcDisplay) {
					hdcDisplay = CreateDC("DISPLAY", NULL, NULL, 0);

					if (!hdcDisplay)
						throw MyMemoryError();
				}

				fa->dst.hdc = CreateCompatibleDC(hdcDisplay);

				if (!fa->dst.hdc)
					throw MyMemoryError();

				fa->hbmDst = CreateDIBSection(fa->dst.hdc, (BITMAPINFO *)&bih, DIB_RGB_COLORS, &fa->pvDstView,
					hFileShared, bitmap[fa->dstbuf].lMapOffset + fa->dst.offset);

				if (!fa->hbmDst)
					throw MyMemoryError();

				fa->hgoDst = SelectObject(fa->dst.hdc, fa->hbmDst);

				if (fa_next->next && (fa_next->src.dwFlags & VFBitmap::NEEDS_HDC))
					fa_next->src.hdc = fa->dst.hdc;
			}

			if (fa->flags & FILTERPARAM_NEEDS_LAST) {
				fa->last->data		 = (Pixel *)(lpBuffer + lLastBufPtr);

				if (fa->last->dwFlags & VFBitmap::NEEDS_HDC) {
					BITMAPINFOHEADER bih;

					fa->last->MakeBitmapHeader(&bih);

					if (!hdcDisplay) {
						hdcDisplay = CreateDC("DISPLAY", NULL, NULL, 0);

						if (!hdcDisplay)
							throw MyMemoryError();
					}

					fa->last->hdc = CreateCompatibleDC(hdcDisplay);

					if (!fa->last->hdc)
						throw MyMemoryError();

					fa->hbmLast = CreateDIBSection(fa->last->hdc, (BITMAPINFO *)&bih, DIB_RGB_COLORS, &fa->pvLastView,
						hFileShared, lLastBufPtr);

					if (!fa->hbmLast)
						throw MyMemoryError();

					fa->hgoLast = SelectObject(fa->last->hdc, fa->hbmLast);
				}
				lLastBufPtr += fa->last->size;
			}

			fa = fa_next;
		}
	} catch(const MyError&) {
		if (hdcDisplay)
			DeleteDC(hdcDisplay);

		throw;
	}

	if (hdcDisplay)
		DeleteDC(hdcDisplay);
}

int FilterSystem::ReadyFilters(FilterStateInfo *pfsi) {
	FilterInstance *fa = (FilterInstance *)listFilters->tail.next;
	int rcode = 0;

	if (dwFlags & FILTERS_INITIALIZED)
		return 0;

	dwFlags &= ~FILTERS_ERROR;

	FilterStateInfo *pfsiPrev = pfsi;

	try {
		while(fa->next) {
			int nDelay = fa->flags >> 16;

			if (nDelay) {
				fa->nDelayRingSize = nDelay;
				fa->nDelayRingPos = 0;
				fa->pfsiDelayRing = new FilterStateInfo[nDelay];

				fa->pfsiDelayInput = pfsiPrev;
				fa->pfsi = &fa->fsiDelay;
				pfsiPrev = &fa->fsiDelayOutput;
			} else
				fa->pfsi = pfsiPrev;

			VDCHECKPOINT;
			CHECK_FPU_STACK

			if (fa->filter->startProc)
				try {
					if (rcode = fa->filter->startProc(fa, &g_filterFuncs))
						break;
				} catch(const MyError& e) {
					throw MyError("Cannot start filter '%s': %s", fa->filter->name, e.gets());
				}

			CHECK_FPU_STACK

			fa = (FilterInstance *)fa->next;
		}

		VDCHECKPOINT;
	} catch(const MyError&) {
		DeinitFilters();
		throw;
	}

	dwFlags |= FILTERS_INITIALIZED;

	if (rcode)
		DeinitFilters();

	mbFirstFrame = true;

	return rcode;
}

int FilterSystem::RunFilters() {
	return RunFilters(NULL);
}

int FilterSystem::RunFilters(FilterInstance *pfiStopPoint) {

	if (listFilters->IsEmpty())
		return 0;

	if (dwFlags & FILTERS_ERROR)
		return -1;

	FilterInstance *fa = (FilterInstance *)listFilters->tail.next;
	int rcode = 0;

	if (!(dwFlags & FILTERS_INITIALIZED))
		return -1;

	if (fa->next && fa->srcbuf != 0)
		fa->src.BitBlt(0,0,&bitmap[0],0,0,-1,-1);

	while(fa->next && fa != pfiStopPoint) {

		if (fa->src.dwFlags & VFBitmap::NEEDS_HDC) {
			LONG comp;
			RECT r;

			r.left		=			  fa->x1; comp  = fa->x1;
			r.right		= fa->src.w	- fa->x2; comp |= fa->x2;
			r.top		=			  fa->y1; comp |= fa->y1;
			r.bottom	= fa->src.h	- fa->y2; comp |= fa->y2;

			if (comp) {
				IntersectClipRect(fa->src.hdc, r.left, r.top, r.right, r.bottom);
				SetWindowOrgEx(fa->src.hdc, fa->x1, fa->y1, NULL);
			}
		}

		if (fa->dst.dwFlags & VFBitmap::NEEDS_HDC) {
			SetViewportOrgEx(fa->dst.hdc, 0, 0, NULL);
			SelectClipRgn(fa->dst.hdc, NULL);
			IntersectClipRect(fa->dst.hdc, 0, 0, fa->dst.w, fa->dst.h);
		}

		// If the filter has a delay ring...

		if (fa->pfsiDelayRing) {
			if (mbFirstFrame) {
				for(int i=0; i<fa->nDelayRingSize; ++i)
					fa->pfsiDelayRing[i] = *fa->pfsiDelayInput;
			}

			// Create composite FilterStateInfo structure for lagged filter.

			const FilterStateInfo& fsiIn = *fa->pfsiDelayInput;
			FilterStateInfo& fsiOut = fa->pfsiDelayRing[fa->nDelayRingPos];

			fa->fsiDelay = fsiIn;
			fa->fsiDelay.lCurrentFrame = fsiOut.lCurrentFrame;
			fa->fsiDelay.lDestFrameMS = fsiOut.lDestFrameMS;

			// Send out old value, read in new value, and advance ring.

			fa->fsiDelayOutput = fsiOut;
			fsiOut = fsiIn;

			if (++fa->nDelayRingPos >= fa->nDelayRingSize)
				fa->nDelayRingPos = 0;
		}

		// Run the filter.

		VDCHECKPOINT;
		CHECK_FPU_STACK

		try {
			vdprotected1("running filter \"%s\"", const char *, fa->filter->name) {
				if (rcode = fa->filter->runProc(fa, &g_filterFuncs))
					break;
				CHECK_FPU_STACK
			}
		} catch(const MyError& e) {
			dwFlags |= FILTERS_ERROR;
			throw MyError("Error running filter '%s': %s", fa->filter->name, e.gets());
		}
		VDCHECKPOINT;
		CHECK_FPU_STACK

		if (fa->flags & FILTERPARAM_NEEDS_LAST)
			fa->realLast.BitBlt(0, 0, &fa->realSrc, 0, 0, -1, -1);

		fa = (FilterInstance *)fa->next;
	}

	mbFirstFrame = false;

	return rcode;
}

void FilterSystem::DeinitFilters() {
	if (!listFilters)
		return;

	FilterInstance *fa = (FilterInstance *)listFilters->tail.next;

	if (!(dwFlags & FILTERS_INITIALIZED))
		return;

	// send all filters a 'stop'

	while(fa->next) {
		VDCHECKPOINT;
		if (fa->filter->endProc)
			fa->filter->endProc(fa, &g_filterFuncs);

		delete[] fa->pfsiDelayRing;
		fa->pfsiDelayRing = NULL;

		fa = (FilterInstance *)fa->next;
	}
	VDCHECKPOINT;

	dwFlags &= ~FILTERS_INITIALIZED;

	delete[] bitmap;
	bitmap = NULL;
}

VFBitmap *FilterSystem::InputBitmap() {
	return &bitmap[0];
}

VFBitmap *FilterSystem::OutputBitmap() {
	return &bitmap[3];
}

VFBitmap *FilterSystem::LastBitmap() {
	return bmLast;
}

bool FilterSystem::isRunning() {
	return !!(dwFlags & FILTERS_INITIALIZED);
}

int FilterSystem::getFrameLag() {
	return nFrameLag;
}

void FilterSystem::getOutputMappingParams(HANDLE& hr, LONG& lr) {
	hr = hFileShared;
	lr = bitmap[3].lMapOffset + bitmap[3].offset;
}

/////////////////////////////////////////////////////////////////////////////
//
//	FilterSystem::private_methods
//
/////////////////////////////////////////////////////////////////////////////

void FilterSystem::AllocateVBitmaps(int count) {
	delete[] bitmap;

	if (!(bitmap = new FilterSystemBitmap[count])) throw MyMemoryError();
//	memset(bitmap, 0, sizeof(VBitmap) * count);

	for(int i=0; i<count; i++) {
		bitmap[i].data		= NULL;
		bitmap[i].palette	= NULL;
		bitmap[i].buffer	= 0;
		bitmap[i].depth		= 0;
		bitmap[i].w			= 0;
		bitmap[i].h			= 0;
		bitmap[i].pitch		= 0;
		bitmap[i].modulo	= 0;
		bitmap[i].size		= 0;
		bitmap[i].offset	= 0;
	}

	iBitmapCount = count;
}

void FilterSystem::AllocateBuffers(LONG lTotalBufferNeeded) {
	DeallocateBuffers();

	if (fSharedWindow) {

		if (!(hFileShared = CreateFileMapping((HANDLE)0xFFFFFFFF, NULL, PAGE_READWRITE, 0, lTotalBufferNeeded+8, NULL)))
			throw MyError("Could not allocate shared memory.");

		if (!(lpBuffer = (unsigned char *)MapViewOfFile(hFileShared, FILE_MAP_ALL_ACCESS, 0, 0, lTotalBufferNeeded+8)))
			throw MyError("Could not map shared memory into local space.");

	} else {

		if (!(lpBuffer = (unsigned char *)VirtualAlloc(NULL, lTotalBufferNeeded+8, MEM_COMMIT, PAGE_READWRITE)))
			throw MyMemoryError();

	}

	memset(lpBuffer, 0, lTotalBufferNeeded+8);
}

void FilterSystem::DeallocateBuffers() {
	if (listFilters) {
		// delete hdcs

		FilterInstance *fa = (FilterInstance *)listFilters->tail.next;

		while(fa->next) {
			if (fa->dst.hdc) {
				_RPT2(0,"Deleting dst display context from %p (%s)\n", fa, fa->filter->name);
				DeleteObject(SelectObject(fa->dst.hdc, fa->hgoDst));
				UnmapViewOfFile(fa->pvDstView);		// avoid NT4 GDI 64K memory leak bug
				DeleteDC(fa->dst.hdc);
				fa->dst.hdc = NULL;
			}
			if (fa->last->hdc) {
				_RPT2(0,"Deleting last display context from %p (%s)\n", fa, fa->filter->name);
				DeleteObject(SelectObject(fa->last->hdc, fa->hgoLast));
				UnmapViewOfFile(fa->pvLastView);		// avoid NT4 GDI 64K memory leak bug
				DeleteDC(fa->last->hdc);
				fa->last->hdc = NULL;
			}

			fa = (FilterInstance *)fa->next;
		}
	}

	if (hdcSrc) {
		DeleteObject(SelectObject(hdcSrc, hgoSrc));
		DeleteDC(hdcSrc);
		hdcSrc = NULL;
	}

	if (lpBuffer) {
		if (fSharedWindow)
			UnmapViewOfFile(lpBuffer);
		else
			VirtualFree(lpBuffer, 0, MEM_RELEASE);

		lpBuffer = NULL;
	}
	if (hFileShared) { CloseHandle(hFileShared); hFileShared = NULL; }
}

/////////////

void FilterSystem::ClearBufferLocks() {
	dwBufferLockFlags = 0;
}

void FilterSystem::LockBuffer(int buffer) {
	VDASSERT(buffer>=0 && buffer<iBitmapCount);
	VDASSERT(!IsBufferLocked(buffer));

	dwBufferLockFlags |= 1L<<buffer;
}

void FilterSystem::UnlockBuffer(int buffer) {
	VDASSERT(buffer>=0 && buffer<iBitmapCount);
	VDASSERT(IsBufferLocked(buffer));

	dwBufferLockFlags &= ~(1L<<buffer);
}

BOOL FilterSystem::IsBufferLocked(int buffer) {
	VDASSERT(buffer>=0 && buffer<iBitmapCount);

	return dwBufferLockFlags & (1L<<buffer);
}
