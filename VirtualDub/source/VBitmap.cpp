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
#include <new>
#include "VirtualDub.h"


#include "convert.h"
#include "VBitmap.h"
#include "Histogram.h"
#include <vd2/system/cpuaccel.h>
#include "resample.h"

#define FP_EPSILON (1e-30)

extern "C" void __cdecl asm_resize_nearest(
		Pixel32 *dst,
		Pixel32 *src,
		long width,
		PixDim height,
		PixOffset dstpitch,
		PixOffset srcpitch,
		unsigned long xaccum,
		unsigned long yaccum,
		unsigned long xfrac,
		unsigned long yfrac,
		long xistep,
		PixOffset yistep,
		Pixel32 *precopysrc,
		unsigned long precopy,
		Pixel32 *postcopysrc,
		unsigned long postcopy);

extern "C" void __cdecl asm_resize_bilinear(
		void *dst,
		void *src,
		long w,
		PixDim h,
		PixOffset dstpitch,
		PixOffset srcpitch,
		unsigned long xaccum,
		unsigned long yaccum,
		unsigned long xfrac,
		unsigned long yfrac,
		long xistep,
		PixOffset yistep,
		Pixel32 *precopysrc,
		unsigned long precopy,
		Pixel32 *postcopysrc,
		unsigned long postcopy);

extern "C" void __cdecl asm_bitmap_xlat1(Pixel32 *dst, Pixel32 *src,
		PixOffset dpitch, PixOffset spitch,
		PixDim w,
		PixDim h,
		const Pixel8 *tbl);

extern "C" void __cdecl asm_bitmap_xlat3(Pixel32 *dst, Pixel32 *src,
		PixOffset dpitch, PixOffset spitch,
		PixDim w,
		PixDim h,
		const Pixel32 *tbl);

///////////////////////////////////////////////////////////////////////////

VBitmap::VBitmap(void *lpData, BITMAPINFOHEADER *bmih) {
	init(lpData, bmih);
}

VBitmap::VBitmap(void *data, PixDim w, PixDim h, int depth) {
	init(data, w, h, depth);
}

///////////////////////////////////////////////////////////////////////////

VBitmap& VBitmap::init(void *lpData, BITMAPINFOHEADER *bmih) {
	data			= (Pixel *)lpData;
	palette			= (Pixel *)(bmih+1);
	depth			= bmih->biBitCount;
	w				= bmih->biWidth;
	h				= bmih->biHeight;
	offset			= 0;
	AlignTo4();

	return *this;
}

VBitmap& VBitmap::init(void *data, PixDim w, PixDim h, int depth) {
	this->data		= (Pixel32 *)data;
	this->palette	= NULL;
	this->depth		= depth;
	this->w			= w;
	this->h			= h;
	this->offset	= 0;
	AlignTo8();

	return *this;
}

void VBitmap::MakeBitmapHeader(BITMAPINFOHEADER *bih) const {
	bih->biSize				= sizeof(BITMAPINFOHEADER);
	bih->biBitCount			= depth;
	bih->biPlanes			= 1;
	bih->biCompression		= BI_RGB;

	if (pitch == ((w*bih->biBitCount + 31)/32) * 4)
		bih->biWidth		= w;
	else
		bih->biWidth		= pitch*8 / depth;

	bih->biHeight			= h;
	bih->biSizeImage		= pitch*h;
	bih->biClrUsed			= 0;
	bih->biClrImportant		= 0;
	bih->biXPelsPerMeter	= 0;
	bih->biYPelsPerMeter	= 0;
}

void VBitmap::MakeBitmapHeaderNoPadding(BITMAPINFOHEADER *bih) const {
	bih->biSize				= sizeof(BITMAPINFOHEADER);
	bih->biBitCount			= depth;
	bih->biPlanes			= 1;
	bih->biCompression		= BI_RGB;
	bih->biWidth			= w;
	bih->biHeight			= h;
	bih->biSizeImage		= (((w*bih->biBitCount + 31)/32) * 4)*h;
	bih->biClrUsed			= 0;
	bih->biClrImportant		= 0;
	bih->biXPelsPerMeter	= 0;
	bih->biYPelsPerMeter	= 0;
}

void VBitmap::AlignTo4() {
	pitch		= PitchAlign4();
	modulo		= Modulo();
	size		= Size();
}

void VBitmap::AlignTo8() {
	pitch		= PitchAlign8();
	modulo		= Modulo();
	size		= Size();
}

///////////////////////////////////////////////////////////////////////////

bool VBitmap::dualrectclip(PixCoord& x2, PixCoord& y2, const VBitmap *src, PixCoord& x1, PixCoord& y1, PixDim& dx, PixDim& dy) const {
	if (dx == -1) dx = src->w;
	if (dy == -1) dy = src->h;

	// clip to source bitmap

	if (x1 < 0) { dx+=x1; x2-=x1; x1=0; }
	if (y1 < 0) { dy+=y1; y2-=y1; y1=0; }
	if (x1+dx > src->w) dx=src->w-x1;
	if (y1+dy > src->h) dy=src->h-y1;

	// clip to destination bitmap

	if (x2 < 0) { dx+=x2; x1-=x2; x2=0; }
	if (y2 < 0) { dy+=y2; y1-=y2; y2=0; }
	if (x2+dx > w) dx=w-x2;
	if (y2+dy > h) dy=h-y2;

	// anything left to blit?

	if (dx<=0 || dy<=0)
		return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////

void VBitmap::BitBlt(PixCoord x2, PixCoord y2, const VBitmap *src, PixDim x1, PixDim y1, PixDim dx, PixDim dy) const {
	static void (*converters[3][3])(void *dest, long dest_pitch, void *src, long src_pitch, long width, long height) = {
		{ DIBconvert_16_to_16, DIBconvert_24_to_16, DIBconvert_32_to_16, },
		{ DIBconvert_16_to_24, DIBconvert_24_to_24, DIBconvert_32_to_24, },
		{ DIBconvert_16_to_32, DIBconvert_24_to_32, DIBconvert_32_to_32, },
	};

	Pixel *dstp, *srcp;

	_ASSERT(depth >= 16);	// we only blit to 16/24/32

	if (!dualrectclip(x2, y2, src, x1, y1, dx, dy))
		return;

	// compute coordinates

	srcp = src->Address(x1, y1+dy-1);
	dstp = Address(x2, y2+dy-1);

	// are we blitting from an 8-bit bitmap?

	CHECK_FPU_STACK

	if (src->depth == 8)
		switch(depth) {
		case 32:
			DIBconvert_8_to_32(dstp, pitch, srcp, src->pitch, dx, dy, src->palette);
			break;
		case 24:
			DIBconvert_8_to_24(dstp, pitch, srcp, src->pitch, dx, dy, src->palette);
			break;
		case 16:
			DIBconvert_8_to_16(dstp, pitch, srcp, src->pitch, dx, dy, src->palette);
			break;
		}
	else {
		converters[depth/8-2][src->depth/8-2](dstp, pitch, srcp, src->pitch, dx, dy);
	}

	CHECK_FPU_STACK
}

void VBitmap::BitBltDither(PixCoord x2, PixCoord y2, const VBitmap *src, PixDim x1, PixDim y1, PixDim dx, PixDim dy, bool to565) const {

	// Right now, we can only dither 32->16

	if (src->depth != 32 || depth != 16) {
		BitBlt(x2, y2, src, x1, y1, dx, dy);
		return;
	}

	// Do the blit

	Pixel *dstp, *srcp;

	if (!dualrectclip(x2, y2, src, x1, y1, dx, dy))
		return;

	// compute coordinates

	srcp = src->Address(x1, y1+dy-1);
	dstp = Address(x2, y2+dy-1);

	// do the blit

	CHECK_FPU_STACK

	if (to565)
		DIBconvert_32_to_16_565_dithered(dstp, pitch, srcp, src->pitch, dx, dy);
	else
		DIBconvert_32_to_16_dithered(dstp, pitch, srcp, src->pitch, dx, dy);

	CHECK_FPU_STACK
}

namespace {
	void DIBconvert_8_to_16_565(void *dst0, ptrdiff_t dstpitch, const void *src0, ptrdiff_t srcpitch, int w, int h, const Pixel32 pal[256]) {
		uint16 *dst = (uint16 *)dst0 + w;
		const uint8 *src = (const uint8 *)src0 + w;

		do {
			int w2 = -w;

			do {
				const unsigned px = pal[src[w2]];

				dst[w2] = (uint16)(((px>>8)&0xf800) + ((px>>5)&0x07e0) + ((px>>3)&0x001f));
			} while(++w2);

			src = (const uint8 *)((const char *)src + srcpitch);
			dst = (uint16 *)((const char *)dst + dstpitch);
		} while(--h);
	}

	void DIBconvert_16_to_16_565(void *dst0, ptrdiff_t dstpitch, const void *src0, ptrdiff_t srcpitch, int w, int h) {
		uint16 *dst = (uint16 *)dst0 + w;
		const uint16 *src = (const uint16 *)src0 + w;

		do {
			int w2 = -w;

			do {
				const unsigned px = src[w2];

				dst[w2] = (uint16)(px + (px & 0x7fe0) + ((px >> 4)&0x20));
			} while(++w2);

			src = (const uint16 *)((const char *)src + srcpitch);
			dst = (uint16 *)((const char *)dst + dstpitch);
		} while(--h);
	}

	void DIBconvert_24_to_16_565(void *dst0, ptrdiff_t dstpitch, const void *src0, ptrdiff_t srcpitch, int w, int h) {
		uint16 *dst = (uint16 *)dst0 + w;
		const uint8 *src = (const uint8 *)src0;

		const ptrdiff_t srcmodulo = srcpitch - w*3;

		do {
			int w2 = -w;

			do {
				const unsigned r = ((uint32)src[2] & 0xf8) << 8;
				const unsigned g = ((uint32)src[1] & 0xfc) << 3;
				const unsigned b = ((uint32)src[0] & 0xf8) >> 3;
				src += 3;

				dst[w2] = (uint16)(r+g+b);
			} while(++w2);

			src = (const uint8 *)((const char *)src + srcmodulo);
			dst = (uint16 *)((const char *)dst + dstpitch);
		} while(--h);
	}

}

void VBitmap::BitBlt565(PixCoord x2, PixCoord y2, const VBitmap *src, PixDim x1, PixDim y1, PixDim dx, PixDim dy) const {

	if (depth != 16) {
		BitBlt(x2, y2, src, x1, y1, dx, dy);
		return;
	}

	// Do the blit

	Pixel *dstp, *srcp;

	if (!dualrectclip(x2, y2, src, x1, y1, dx, dy))
		return;

	// anything left to blit?

	if (dx<=0 || dy<=0) return;

	// compute coordinates

	srcp = src->Address(x1, y1+dy-1);
	dstp = Address(x2, y2+dy-1);

	// do the blit

	CHECK_FPU_STACK

	switch(src->depth) {
	case 8:
		DIBconvert_8_to_16_565(dstp, pitch, srcp, src->pitch, dx, dy, src->palette);
		break;
	case 16:
		DIBconvert_16_to_16_565(dstp, pitch, srcp, src->pitch, dx, dy);
		break;
	case 24:
		DIBconvert_24_to_16_565(dstp, pitch, srcp, src->pitch, dx, dy);
		break;
	case 32:
		DIBconvert_32_to_16_565(dstp, pitch, srcp, src->pitch, dx, dy);
		break;
	}

	CHECK_FPU_STACK
}

bool VBitmap::BitBltXlat1(PixCoord x2, PixCoord y2, const VBitmap *src, PixCoord x1, PixCoord y1, PixDim dx, PixDim dy, const Pixel8 *tbl) const {
	if (depth != 32)
		return false;

	if (!dualrectclip(x2, y2, src, x1, y1, dx, dy))
		return false;

	// do the translate

	asm_bitmap_xlat1(
			this->Address32(x2, y2+dy-1)+dx,
			src ->Address32(x1, y1+dy-1)+dx,
			this->pitch,
			src->pitch,
			-4*dx, dy, tbl);

	return true;
}

bool VBitmap::BitBltXlat3(PixCoord x2, PixCoord y2, const VBitmap *src, PixCoord x1, PixCoord y1, PixDim dx, PixDim dy, const Pixel32 *tbl) const {
	if (depth != 32)
		return false;

	if (!dualrectclip(x2, y2, src, x1, y1, dx, dy))
		return false;

	// do the translate

	asm_bitmap_xlat3(
			this->Address32(x2, y2+dy-1)+dx,
			src ->Address32(x1, y1+dy-1)+dx,
			this->pitch,
			src->pitch,
			-4*dx, dy, tbl);

	return true;
}

///////////////////////////////////////////////////////////////////////////

bool VBitmap::StretchBltNearestFast(PixCoord x2, PixCoord y2, PixDim dx, PixDim dy,
						const VBitmap *src, double x1, double y1, double dx1, double dy1) const {

	// No format conversions!!

	if (src->depth != depth)
		return false;

	// Right now, only do 32-bit stretch.  (24-bit is a pain, 16-bit is slow.)

	if (depth != 32)
		return false;

	// Compute clipping parameters.

	ResampleInfo horiz, vert;

	if (!horiz.init(x2, dx, x1+0.5, dx1, w, src->w, 1, false, false))
		return false;

	if (!vert.init(y2, dy, y1+0.5, dy1, h, src->h, 1, false, false))
		return false;

	// Call texturing routine.

	if (vert.clip.precopy)
		asm_resize_nearest(
				Address32(horiz.x1_int + horiz.clip.precopy + horiz.clip.unclipped, vert.x1_int),			// destination pointer, right side
				src->Address32(horiz.u0_int.hi, 0),
				-horiz.clip.unclipped*4,	// -width*4
				vert.clip.precopy,			// height
				-pitch,						// dstpitch
				0,							// srcpitch
				horiz.u0_int.lo,			// xaccum
				0,							// yaccum
				horiz.dudx_int.lo,			// xfrac
				0,							// yfrac
				horiz.dudx_int.hi,			// xinc
				0,							// yinc
				src->Address32(0, 0),			// precopysrc
				horiz.clip.precopy,				// precopy
				src->Address32(src->w-1, 0),	// postcopysrc
				horiz.clip.postcopy			// postcopy
				);

	asm_resize_nearest(
			Address32(horiz.x1_int + horiz.clip.precopy + horiz.clip.unclipped, vert.x1_int + vert.clip.precopy),			// destination pointer, right side
			src->Address32(horiz.u0_int.hi, vert.u0_int.hi),
			-horiz.clip.unclipped*4,		// -width*4
			vert.clip.unclipped,			// height
			-pitch,							// dstpitch
			-src->pitch,					// srcpitch
			horiz.u0_int.lo,				// xaccum
			vert.u0_int.lo,					// yaccum
			horiz.dudx_int.lo,				// xfrac
			vert.dudx_int.lo,				// yfrac
			horiz.dudx_int.hi,				// xinc
			-vert.dudx_int.hi * src->pitch,	// yinc
			src->Address32(0, vert.u0_int.hi),
			horiz.clip.precopy,				// precopy
			src->Address32(src->w-1, vert.u0_int.hi),
			horiz.clip.postcopy				// postcopy
			);

	if (vert.clip.postcopy)
		asm_resize_nearest(
				Address32(horiz.x1_int + horiz.clip.precopy + horiz.clip.unclipped, vert.x1_int + vert.clip.precopy + vert.clip.unclipped),			// destination pointer, right side
				src->Address32(horiz.u0_int.hi, src->h - 1),
				-horiz.clip.unclipped*4,	// -width*4
				vert.clip.postcopy,			// height
				-pitch,						// dstpitch
				0,							// srcpitch
				horiz.u0_int.lo,			// xaccum
				0,							// yaccum
				horiz.dudx_int.lo,			// xfrac
				0,							// yfrac
				horiz.dudx_int.hi,			// xinc
				0,							// yinc
				src->Address32(0, src->h - 1),
				horiz.clip.precopy,			// precopy
				src->Address32(src->w - 1, src->h - 1),
				horiz.clip.postcopy);		// postcopy

	return true;
}

bool VBitmap::StretchBltBilinearFast(PixCoord x2, PixCoord y2, PixDim dx, PixDim dy,
						const VBitmap *src, double x1, double y1, double dx1, double dy1) const {

	// No format conversions!!

	if (src->depth != depth)
		return false;

	// Right now, only do 32-bit stretch.  (24-bit is a pain, 16-bit is slow.)

	if (depth != 32)
		return false;

	// Compute clipping parameters.

	ResampleInfo horiz, vert;

	if (!horiz.init(x2, dx, x1 + (1.0 / 32.0), dx1, w, src->w, 2, false, false))
		return false;

	if (!vert.init(y2, dy, y1 + (1.0 / 32.0), dy1, h, src->h, 2, false, false))
		return false;

	// Call texturing routine.

	int xprecopy = horiz.clip.preclip + horiz.clip.precopy;
	int xpostcopy = horiz.clip.postclip + horiz.clip.postcopy;
	int yprecopy = vert.clip.preclip + vert.clip.precopy;
	int ypostcopy = vert.clip.postclip + vert.clip.postcopy;

	if (yprecopy)
		asm_resize_bilinear(
				Address32(horiz.x1_int + xprecopy + horiz.clip.unclipped, vert.x1_int),			// destination pointer, right side
				src->Address32(horiz.u0_int.hi, 0),
				-horiz.clip.unclipped*4,	// -width*4
				yprecopy,					// height
				-pitch,						// dstpitch
				0,							// srcpitch
				horiz.u0_int.lo,			// xaccum
				0,							// yaccum
				horiz.dudx_int.lo,			// xfrac
				0,							// yfrac
				horiz.dudx_int.hi,			// xinc
				0,							// yinc
				src->Address32(0, 0),			// precopysrc
				-xprecopy*4,				// precopy
				src->Address32(src->w-1, 0),	// postcopysrc
				-xpostcopy*4			// postcopy
				);

	asm_resize_bilinear(
			Address32(horiz.x1_int + xprecopy + horiz.clip.unclipped, vert.x1_int + yprecopy),			// destination pointer, right side
			src->Address32(horiz.u0_int.hi, vert.u0_int.hi),
			-horiz.clip.unclipped*4,		// -width*4
			vert.clip.unclipped,			// height
			-pitch,							// dstpitch
			-src->pitch,					// srcpitch
			horiz.u0_int.lo,				// xaccum
			vert.u0_int.lo,					// yaccum
			horiz.dudx_int.lo,				// xfrac
			vert.dudx_int.lo,				// yfrac
			horiz.dudx_int.hi,				// xinc
			-vert.dudx_int.hi * src->pitch,	// yinc
			src->Address32(0, vert.u0_int.hi),
			-xprecopy*4,				// precopy
			src->Address32(src->w-1, vert.u0_int.hi),
			-xpostcopy*4			// postcopy
			);

	if (ypostcopy)
		asm_resize_bilinear(
				Address32(horiz.x1_int + xprecopy + horiz.clip.unclipped, vert.x1_int + yprecopy + vert.clip.unclipped),			// destination pointer, right side
				src->Address32(horiz.u0_int.hi, src->h - 1),
				-horiz.clip.unclipped*4,	// -width*4
				ypostcopy,					// height
				-pitch,						// dstpitch
				0,							// srcpitch
				horiz.u0_int.lo,			// xaccum
				0,							// yaccum
				horiz.dudx_int.lo,			// xfrac
				0,							// yfrac
				horiz.dudx_int.hi,			// xinc
				0,							// yinc
				src->Address32(0, src->h - 1),
				-xprecopy*4,			// precopy
				src->Address32(src->w - 1, src->h - 1),
				-xpostcopy*4);		// postcopy

	return true;
}

bool VBitmap::RectFill(PixCoord x, PixCoord y, PixDim dx, PixDim dy, Pixel32 c) const {

	if (depth != 32)
		return false;

	// Do the blit

	Pixel32 *dstp;

	if (dx == -1) dx = w;
	if (dy == -1) dy = h;

	// clip to destination bitmap

	if (x < 0) { dx+=x; x=0; }
	if (y < 0) { dy+=y; y=0; }
	if (x+dx > w) dx=w-x;
	if (y+dy > h) dy=h-y;

	// anything left to fill?

	if (dx<=0 || dy<=0) return false;

	// compute coordinates

	dstp = Address32(x, y+dy-1);

	// do the fill

	do {
		PixDim dxt = dx;
		Pixel32 *dst2 = dstp;

		do {
			*dst2++ = c;
		} while(--dxt);

		dstp = (Pixel32 *)((char *)dstp + pitch);
	} while(--dy);

	return true;
}

bool VBitmap::Histogram(PixCoord x, PixCoord y, PixCoord dx, PixCoord dy, long *pHisto, int iHistoType) const {
	static const unsigned short pixmasks[3]={
		0x7c00,
		0x03e0,
		0x001f,
	};

	if (depth != 32 && depth != 24 && depth != 16)
		return false;

	// Do the blit

	Pixel32 *dstp;

	if (dx == -1) dx = w;
	if (dy == -1) dy = h;

	// clip to bitmap

	if (x < 0) { dx+=x; x=0; }
	if (y < 0) { dy+=y; y=0; }
	if (x+dx > w) dx=w-x;
	if (y+dy > h) dy=h-y;

	// anything left to histogram?

	if (dx<=0 || dy<=0) return false;

	// compute coordinates

	dstp = Address(x, y+dy-1);

	// do the histogram

	switch(iHistoType) {
	case HISTO_LUMA:
		switch(depth) {
		case 16:	asm_histogram16_run(dstp, dx, dy, pitch, pHisto, 0x7FFF); break;
		case 24:	asm_histogram_gray24_run(dstp, dx, dy, pitch, pHisto); break;
		case 32:	asm_histogram_gray_run(dstp, dx, dy, pitch, pHisto); break;
		default:	__assume(false);
		}
		break;

	case HISTO_GRAY:
		// FIXME: Really lame algorithm.

		switch(depth) {
		case 16:
			asm_histogram16_run(dstp, dx, dy, pitch, pHisto, pixmasks[0]);
			asm_histogram16_run(dstp, dx, dy, pitch, pHisto, pixmasks[1]);
			asm_histogram16_run(dstp, dx, dy, pitch, pHisto, pixmasks[2]);
			break;
		case 24:
			asm_histogram_color24_run((char *)dstp + 0, dx, dy, pitch, pHisto);
			asm_histogram_color24_run((char *)dstp + 1, dx, dy, pitch, pHisto);
			asm_histogram_color24_run((char *)dstp + 2, dx, dy, pitch, pHisto);
			break;
		case 32:
			asm_histogram_color_run((char *)dstp + 0, dx, dy, pitch, pHisto);
			asm_histogram_color_run((char *)dstp + 1, dx, dy, pitch, pHisto);
			asm_histogram_color_run((char *)dstp + 2, dx, dy, pitch, pHisto);
			break;
		default:	__assume(false);
		}
		break;

	case HISTO_RED:
	case HISTO_GREEN:
	case HISTO_BLUE:
		switch(depth) {
		case 16:	asm_histogram16_run(dstp, dx, dy, pitch, pHisto, pixmasks[iHistoType-HISTO_RED]); break;
		case 24:	asm_histogram_color24_run((char *)dstp + (2-(iHistoType-HISTO_RED)), dx, dy, pitch, pHisto); break;
		case 32:	asm_histogram_color_run((char *)dstp + (2-(iHistoType-HISTO_RED)), dx, dy, pitch, pHisto); break;
		default:	__assume(false);
		}
		break;
	}

	return true;
}


///////////////////////////////////////////////////////////////////////////

extern "C" unsigned long YUV_Y_table[];
extern "C" unsigned long YUV_U_table[];
extern "C" unsigned long YUV_V_table[];
extern "C" unsigned long YUV_Y2_table[];
extern "C" unsigned long YUV_U2_table[];
extern "C" unsigned long YUV_V2_table[];
extern "C" unsigned char YUV_clip_table[];
extern "C" unsigned char YUV_clip_table16[];

extern "C" void asm_convert_yuy2_bgr16(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr16_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr24(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr24_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr32(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr32_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);

extern "C" void asm_convert_yuy2_fullscale_bgr16(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_fullscale_bgr16_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_fullscale_bgr24(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_fullscale_bgr24_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_fullscale_bgr32(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_fullscale_bgr32_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);


bool VBitmap::BitBltFromYUY2(PixCoord x2, PixCoord y2, const VBitmap *src, PixCoord x1, PixCoord y1, PixDim dx, PixDim dy) const {
	unsigned char *srcp, *dstp;

	if (depth != 32 && depth != 24 && depth != 16)
		return false;

	if (!dualrectclip(x2, y2, src, x1, y1, dx, dy))
		return false;

	// We only support even widths.

	if (x2 & 1) {
		++x2;
		--dx;
	}

	if (dx & 1)
		--dx;

	if (dx<=0)
		return false;

	// compute coordinates

	srcp = (unsigned char *)src->Address16i(x1, y1+dy-1);
	dstp = (unsigned char *)Address(x2, y2+dy-1);

	// Do blit

	PixOffset srcmod = -src->pitch;
	PixOffset dstmod = pitch;

	dx >>= 1;

	switch(depth) {
	case 16:
		srcmod -= 4*dx;
		dstmod -= 4*dx;

#if 0
		do {
			w = dx;
			do {
				unsigned char Y1, Y2, U, V;
				unsigned long Y1c, Y2c, Uc, Vc;

				Y1 = srcp[0];
				U  = srcp[1];
				Y2 = srcp[2];
				V  = srcp[3];

				Uc = YUV_U_table[U];
				Vc = YUV_V_table[V];
				Y1c = YUV_Y_table[Y1];
				Y2c = YUV_Y_table[Y2];

				((short *)dstp)[0]	= (unsigned long)YUV_clip_table16[((Uc + Y1c) >> 16) - 0x3f00]
									+ ((unsigned long)YUV_clip_table16[((Uc + Vc + Y1c) & 0xffff) - 0x3f00] << 5)
									+ ((unsigned long)YUV_clip_table16[((Vc + Y1c) >> 16) - 0x3f00] << 10);
				((short *)dstp)[1]	= (unsigned long)YUV_clip_table16[((Uc + Y2c) >> 16) - 0x3f00]
									+ ((unsigned long)YUV_clip_table16[((Uc + Vc + Y2c) & 0xffff) - 0x3f00] << 5)
									+ ((unsigned long)YUV_clip_table16[((Vc + Y2c) >> 16) - 0x3f00] << 10);

				srcp += 4;
				dstp += 4;

			} while(--w);

			srcp += srcmod;
			dstp += dstmod;

		} while(--dy);
#else
		if (MMX_enabled)
			asm_convert_yuy2_bgr16_MMX(dstp, srcp, dx, dy, dstmod, srcmod);
		else
			asm_convert_yuy2_bgr16(dstp, srcp, dx, dy, dstmod, srcmod);
#endif

		break;
	case 24:
		srcmod -= 4*dx;
		dstmod -= 6*dx;

#if 0
		do {
			w = dx;
			do {
				unsigned char Y1, Y2, U, V;
				unsigned long Y1c, Y2c, Uc, Vc;

				Y1 = srcp[0];
				U  = srcp[1];
				Y2 = srcp[2];
				V  = srcp[3];

				Uc = YUV_U_table[U];
				Vc = YUV_V_table[V];
				Y1c = YUV_Y_table[Y1];
				Y2c = YUV_Y_table[Y2];

				dstp[0] = YUV_clip_table[((Uc + Y1c) >> 16) - 0x3f00];
				dstp[1] = YUV_clip_table[((Uc + Vc + Y1c) & 0xffff) - 0x3f00];
				dstp[2] = YUV_clip_table[((Vc + Y1c) >> 16) - 0x3f00];
				dstp[3] = YUV_clip_table[((Uc + Y2c) >> 16) - 0x3f00];
				dstp[4] = YUV_clip_table[((Uc + Vc + Y2c) & 0xffff) - 0x3f00];
				dstp[5] = YUV_clip_table[((Vc + Y2c) >> 16) - 0x3f00];

				srcp += 4;
				dstp += 6;

			} while(--w);

			srcp += srcmod;
			dstp += dstmod;

		} while(--dy);
#else
		if (MMX_enabled)
			asm_convert_yuy2_bgr24_MMX(dstp, srcp, dx, dy, dstmod, srcmod);
		else
			asm_convert_yuy2_bgr24(dstp, srcp, dx, dy, dstmod, srcmod);
#endif

		break;
	case 32:
		srcmod -= 4*dx;
		dstmod -= 8*dx;
#if 0
		do {
			w = dx;
			do {
				unsigned char Y1, Y2, U, V;
				unsigned long Y1c, Y2c, Uc, Vc;

				Y1 = srcp[0];
				U  = srcp[1];
				Y2 = srcp[2];
				V  = srcp[3];

				Uc = YUV_U_table[U];
				Vc = YUV_V_table[V];
				Y1c = YUV_Y_table[Y1];
				Y2c = YUV_Y_table[Y2];

				dstp[0] = YUV_clip_table[((Uc + Y1c) >> 16) - 0x3f00];
				dstp[1] = YUV_clip_table[((Uc + Vc + Y1c) & 0xffff) - 0x3f00];
				dstp[2] = YUV_clip_table[((Vc + Y1c) >> 16) - 0x3f00];
				dstp[4] = YUV_clip_table[((Uc + Y2c) >> 16) - 0x3f00];
				dstp[5] = YUV_clip_table[((Uc + Vc + Y2c) & 0xffff) - 0x3f00];
				dstp[6] = YUV_clip_table[((Vc + Y2c) >> 16) - 0x3f00];

				srcp += 4;
				dstp += 4;

			} while(--w);

			srcp += srcmod;
			dstp += dstmod;

		} while(--dy);
#else
		if (MMX_enabled)
			asm_convert_yuy2_bgr32_MMX(dstp, srcp, dx, dy, dstmod, srcmod);
		else
			asm_convert_yuy2_bgr32(dstp, srcp, dx, dy, dstmod, srcmod);
#endif

		break;
	}

	CHECK_FPU_STACK
	return true;
}

bool VBitmap::BitBltFromYUY2Fullscale(PixCoord x2, PixCoord y2, const VBitmap *src, PixCoord x1, PixCoord y1, PixDim dx, PixDim dy) const {
	unsigned char *srcp, *dstp;

	if (depth != 32 && depth != 24 && depth != 16)
		return false;

	if (!dualrectclip(x2, y2, src, x1, y1, dx, dy))
		return false;

	// We only support even widths.

	if (x2 & 1) {
		++x2;
		--dx;
	}

	if (dx & 1)
		--dx;

	if (dx<=0)
		return false;

	// compute coordinates

	srcp = (unsigned char *)src->Address16i(x1, y1+dy-1);
	dstp = (unsigned char *)Address(x2, y2+dy-1);

	// Do blit

	PixOffset srcmod = -src->pitch;
	PixOffset dstmod = pitch;

	dx >>= 1;

	switch(depth) {
	case 16:
		srcmod -= 4*dx;
		dstmod -= 4*dx;

		if (MMX_enabled)
			asm_convert_yuy2_fullscale_bgr16_MMX(dstp, srcp, dx, dy, dstmod, srcmod);
		else
			asm_convert_yuy2_fullscale_bgr16(dstp, srcp, dx, dy, dstmod, srcmod);

		break;
	case 24:
		srcmod -= 4*dx;
		dstmod -= 6*dx;

		if (MMX_enabled)
			asm_convert_yuy2_fullscale_bgr24_MMX(dstp, srcp, dx, dy, dstmod, srcmod);
		else
			asm_convert_yuy2_fullscale_bgr24(dstp, srcp, dx, dy, dstmod, srcmod);

		break;
	case 32:
		srcmod -= 4*dx;
		dstmod -= 8*dx;

		if (MMX_enabled)
			asm_convert_yuy2_fullscale_bgr32_MMX(dstp, srcp, dx, dy, dstmod, srcmod);
		else
			asm_convert_yuy2_fullscale_bgr32(dstp, srcp, dx, dy, dstmod, srcmod);

		break;
	}

	CHECK_FPU_STACK
	return true;
}

///////////////////////////////////////////////////////////////////////////

typedef unsigned char YUVPixel;

extern "C" void asm_YUVtoRGB32_row(
		void *ARGB1_pointer,
		void *ARGB2_pointer,
		YUVPixel *Y1_pointer,
		YUVPixel *Y2_pointer,
		YUVPixel *U_pointer,
		YUVPixel *V_pointer,
		long width
		);

extern "C" void asm_YUVtoRGB24_row(
		void *ARGB1_pointer,
		void *ARGB2_pointer,
		YUVPixel *Y1_pointer,
		YUVPixel *Y2_pointer,
		YUVPixel *U_pointer,
		YUVPixel *V_pointer,
		long width
		);

extern "C" void asm_YUVtoRGB16_row(
		void *ARGB1_pointer,
		void *ARGB2_pointer,
		YUVPixel *Y1_pointer,
		YUVPixel *Y2_pointer,
		YUVPixel *U_pointer,
		YUVPixel *V_pointer,
		long width
		);

bool VBitmap::BitBltFromI420(PixCoord x2, PixCoord y2, const VBitmap *src, PixCoord x1, PixCoord y1, PixDim dx, PixDim dy) const {
	unsigned char *srcy, *srcu, *srcv, *dstp;

	if (depth != 32 && depth != 24 && depth != 16)
		return false;

	if (!dualrectclip(x2, y2, src, x1, y1, dx, dy))
		return false;

	// We only support even heights, and widths that are a multiple of 8.

	if (x2 & 1) {
		++x2;
		--dx;
	}

	if (y2 & 1) {
		++y2;
		--dy;
	}

	dx &= -8;
	dy &= -2;

	if (dx<=0 || dy<=0)
		return false;

	// compute coordinates - work upside-down

	srcy = (unsigned char *)src->data + x2 + (y2+dy-1)*src->w;
	srcu = (unsigned char *)src->data + x2 + src->h*src->w + ((y2+dy)/2-1)*(src->w/2);
	srcv = (unsigned char *)src->data + x2 + src->h*src->w + (src->w*src->h)/4 + ((y2+dy)/2-1)*(src->w/2);

	dstp = (unsigned char *)Address(x2, y2+dy-1);

	// Do blit

	PixOffset srcmod = -src->pitch;
	PixOffset dstmod = pitch;

	switch(depth) {
	case 16:
		do {
			asm_YUVtoRGB16_row(dstp+pitch, dstp, srcy-src->w, srcy, srcu, srcv, dx/2);

			dstp += 2*pitch;
			srcy -= src->w*2;
			srcu -= src->w/2;
			srcv -= src->w/2;
		} while(dy-=2);
		break;

	case 24:
		do {
			asm_YUVtoRGB24_row(dstp+pitch, dstp, srcy-src->w, srcy, srcu, srcv, dx/2);

			dstp += 2*pitch;
			srcy -= src->w*2;
			srcu += src->w/2;
			srcv += src->w/2;
		} while(dy-=2);
		break;

	case 32:
		do {
			asm_YUVtoRGB32_row(dstp+pitch, dstp, srcy-src->w, srcy, srcu, srcv, dx/2);

			dstp += 2*pitch;
			srcy -= src->w*2;
			srcu -= src->w/2;
			srcv -= src->w/2;
		} while(dy-=2);
		break;

	default:
		__assume(false);
	}

	if (MMX_enabled)
		__asm emms

	if (ISSE_enabled)
		__asm sfence

	CHECK_FPU_STACK
	return true;
}
