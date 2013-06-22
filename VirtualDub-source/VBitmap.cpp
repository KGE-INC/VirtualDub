//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2000 Avery Lee
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

#include "VirtualDub.h"

#include <crtdbg.h>
#include <math.h>

#include "convert.h"
#include "VBitmap.h"
#include "Histogram.h"
#include "cpuaccel.h"

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
		PixOffset yistep);

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
		long precopy,
		long postcopy,
		void *srclimit);

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

VBitmap::VBitmap(void *lpData, BITMAPINFOHEADER *bmih) throw() {
	init(lpData, bmih);
}

VBitmap::VBitmap(void *data, PixDim w, PixDim h, int depth) throw() {
	init(data, w, h, depth);
}

///////////////////////////////////////////////////////////////////////////

VBitmap& VBitmap::init(void *lpData, BITMAPINFOHEADER *bmih) throw() {
	data			= (Pixel *)lpData;
	palette			= (Pixel *)(bmih+1);
	depth			= bmih->biBitCount;
	w				= bmih->biWidth;
	h				= bmih->biHeight;
	offset			= 0;
	AlignTo4();

	return *this;
}

VBitmap& VBitmap::init(void *data, PixDim w, PixDim h, int depth) throw() {
	this->data		= (Pixel32 *)data;
	this->palette	= NULL;
	this->depth		= depth;
	this->w			= w;
	this->h			= h;
	this->offset	= 0;
	AlignTo8();

	return *this;
}

void VBitmap::MakeBitmapHeader(BITMAPINFOHEADER *bih) const throw() {
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

void VBitmap::MakeBitmapHeaderNoPadding(BITMAPINFOHEADER *bih) const throw() {
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

void VBitmap::AlignTo4() throw() {
	pitch		= PitchAlign4();
	modulo		= Modulo();
	size		= Size();
}

void VBitmap::AlignTo8() throw() {
	pitch		= PitchAlign8();
	modulo		= Modulo();
	size		= Size();
}

///////////////////////////////////////////////////////////////////////////

bool VBitmap::dualrectclip(PixCoord& x2, PixCoord& y2, const VBitmap *src, PixCoord& x1, PixCoord& y1, PixDim& dx, PixDim& dy) const throw() {
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

void VBitmap::BitBlt(PixCoord x2, PixCoord y2, const VBitmap *src, PixDim x1, PixDim y1, PixDim dx, PixDim dy) const throw() {
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

void VBitmap::BitBltDither(PixCoord x2, PixCoord y2, const VBitmap *src, PixDim x1, PixDim y1, PixDim dx, PixDim dy, bool to565) const throw() {

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

void VBitmap::BitBlt565(PixCoord x2, PixCoord y2, const VBitmap *src, PixDim x1, PixDim y1, PixDim dx, PixDim dy) const throw() {

	// Right now, we can only convert 32->16/565

	if (src->depth != 32 || depth != 16) {
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

	DIBconvert_32_to_16_565(dstp, pitch, srcp, src->pitch, dx, dy);

	CHECK_FPU_STACK
}

bool VBitmap::BitBltXlat1(PixCoord x2, PixCoord y2, const VBitmap *src, PixCoord x1, PixCoord y1, PixDim dx, PixDim dy, const Pixel8 *tbl) const throw() {
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

bool VBitmap::BitBltXlat3(PixCoord x2, PixCoord y2, const VBitmap *src, PixCoord x1, PixCoord y1, PixDim dx, PixDim dy, const Pixel32 *tbl) const throw() {
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

static __int64 sbnf_correct(double x, double y) {
	__int64 v;

	x = x * 4294967296.0;

	if (x < 0.0)
		v = (__int64)(x - 0.5);
	else
		v = (__int64)(x + 0.5);

	if (y<0.0 && v) ++v;
	if (y>0.0 && v) --v;

	return v;
}

bool VBitmap::StretchBltNearestFast(PixCoord x2, PixCoord y2, PixDim dx, PixDim dy,
						const VBitmap *src, double x1, double y1, double dx1, double dy1) const throw() {

	// No format conversions!!

	if (src->depth != depth)
		return false;

	// Right now, only do 32-bit stretch.  (24-bit is a pain, 16-bit is slow.)

	if (depth != 32)
		return false;

	// Funny values?
	
	if (dx == 0 || dy == 0)
		return false;

	// Check for destination flips.

	if (dx < 0) {
		x2 += dx;
		dx = -dx;
		x1 += dx1;
		dx1 = -dx1;
	}
	if (dy < 0) {
		y2 += dy;
		dy = -dy;
		y1 += dy1;
		dy1 = -dy1;
	}

	// Check for destination clipping and abort if so.

	if (x2 < 0 || y2 < 0 || x2+dx > w || y2+dy > h)
		return false;

	// Check for source clipping.  Trickier, since we permit source flips.

	if (x1 < 0.0 || y1 < 0.0 || x1+dx1 < 0.0 || y1+dy1 < 0.0)
		return false;

	if (x1 > src->w || y1 > src->h || x1+dx1 > src->w || y1+dy1 > src->h)
		return false;

	// Compute step values.

	__int64 xfrac64, yfrac64, xaccum64, yaccum64;
	unsigned long xfrac, yfrac;
	int xistep;
	PixOffset yistep;

/*	xfrac64 = sbnf_correct(dx1, dx1) / dx;	// round toward zero to avoid exceeding buffer
	yfrac64 = sbnf_correct(dy1, dy1) / dy;*/
	xfrac64 = dx1*4294967296.0 / dx;	// round toward zero to avoid exceeding buffer
	yfrac64 = dy1*4294967296.0 / dy;

	xfrac = (unsigned long)xfrac64;
	yfrac = (unsigned long)yfrac64;

	xistep = (long)(xfrac64 >> 32);			// round toward -oo
	yistep = (long)(yfrac64 >> 32) * src->pitch;

	xaccum64 = sbnf_correct(x1, -dx1) + xfrac64/2;
	yaccum64 = sbnf_correct(y1, -dy1) + yfrac64/2;

	// Call texturing routine.

	yaccum64 += (dy-1)*yfrac64;

	asm_resize_nearest(
			Address32i(x2, y2) + dx,			// destination pointer, right side
			src->Address32i((long)(xaccum64>>32), (src->h-1) - (long)(yaccum64>>32)),
			-dx*4,
			dy,
			pitch,
			src->pitch,
			(unsigned long)xaccum64,
			~(unsigned long)yaccum64,
			xfrac,
			yfrac,
			xistep,
			yistep);

	return true;
}

bool VBitmap::StretchBltBilinearFast(PixCoord x2, PixCoord y2, PixDim dx, PixDim dy,
						const VBitmap *src, double x1, double y1, double dx1, double dy1) const throw() {

	// No format conversions!!

	if (src->depth != depth)
		return false;

	// Right now, only do 32-bit stretch.  (24-bit is a pain, 16-bit is slow.)

	if (depth != 32)
		return false;

	// Funny values?
	
	if (dx == 0 || dy == 0)
		return false;

	// Check for destination flips.

	if (dx < 0) {
		x2 += dx;
		dx = -dx;
		x1 += dx1;
		dx1 = -dx1;
	}
	if (dy < 0) {
		y2 += dy;
		dy = -dy;
		y1 += dy1;
		dy1 = -dy1;
	}

	// Check for destination clipping and abort if so.

	if (x2 < 0 || y2 < 0 || x2+dx > w || y2+dy > h)
		return false;

	// Check for source clipping.  Trickier, since we permit source flips.

	if (x1 < 0.0 || y1 < 0.0 || x1+dx1 < 0.0 || y1+dy1 < 0.0)
		return false;

	if (x1 > src->w || y1 > src->h || x1+dx1 > src->w || y1+dy1 > src->h)
		return false;

	// Compute step values.

	__int64 xfrac64, yfrac64, xaccum64, yaccum64;
	unsigned long xfrac, yfrac;
	int xistep;
	PixOffset yistep;

	xfrac64 = (fabs(dx1)<1.0 ? 0.0 : dx1<0.0 ? dx1+1.0 : dx1-1.0)*4294967296.0 / (dx-1);	// round toward zero to avoid exceeding buffer
	yfrac64 = (fabs(dy1)<1.0 ? 0.0 : dy1<0.0 ? dy1+1.0 : dy1-1.0)*4294967296.0 / (dy-1);

	xfrac = (unsigned long)xfrac64;
	yfrac = (unsigned long)yfrac64;

	xistep = (long)(xfrac64 >> 32);			// round toward -oo
	yistep = (long)(yfrac64 >> 32) * src->pitch;

	xaccum64 = (__int64)floor(x1*4294967296.0 + 0.5);
	yaccum64 = (__int64)floor(y1*4294967296.0 + 0.5);

	if (dx1<0)
		xaccum64 -= 0x100000000i64;

	if (dy1<0)
		yaccum64 -= 0x100000000i64;
	else
		yaccum64 += 0x0ffffffffi64;

	// Determine border sizes.  We have to copy pixels when xaccum >= (w-1)<<32
	// or yaccum >= (h-1)<<32;

	int xprecopy=0, xpostcopy=0;
	__int64 xborderval, yborderval;

	xborderval = ((__int64)(src->w-1)<<32);
	yborderval = ((__int64)(src->h-1)<<32);

	if (xfrac64 < 0) {
		if (xaccum64 >= xborderval) {
			xprecopy = (xaccum64 - (xborderval-1) + xfrac64 - 1) / xfrac64 + 1;
		}

		if (xprecopy > dx)
			xprecopy = dx;

	} else {
		if (xaccum64 + xfrac64*(dx-1) >= xborderval) {
			xpostcopy = dx - ((xborderval - xaccum64 - 1)/xfrac64 + 1);
		}

		if (xpostcopy > dx)
			xpostcopy = dx;
	}

	// Call texturing routine.

	if (dy) {
		dx -= xprecopy + xpostcopy;

		yaccum64 += (dy-1)*yfrac64;

		asm_resize_bilinear(
				Address32i(x2, y2) + (dx+xprecopy),			// destination pointer, right side
				src->Address32i((long)(xaccum64>>32), (src->h - 1) - (long)(yaccum64>>32)),
				-dx*4,
				dy,
				pitch,
				src->pitch,
				(unsigned long)xaccum64,
				~(unsigned long)yaccum64,
				xfrac,
				yfrac,
				xistep,
				yistep,
				-xprecopy*4,
				-xpostcopy*4,
				src->Address32i(0, src->h-1));
	}

	return true;
}

bool VBitmap::RectFill(PixCoord x, PixCoord y, PixDim dx, PixDim dy, Pixel32 c) const throw() {

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

bool VBitmap::Histogram(PixCoord x, PixCoord y, PixCoord dx, PixCoord dy, long *pHisto, int iHistoType) const throw() {
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
extern "C" unsigned char YUV_clip_table[];
extern "C" unsigned char YUV_clip_table16[];

extern "C" void asm_convert_yuy2_bgr16(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr16_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr24(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr24_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr32(void *dst, void *src, int w, int h, long dstmod, long srcmod);
extern "C" void asm_convert_yuy2_bgr32_MMX(void *dst, void *src, int w, int h, long dstmod, long srcmod);


bool VBitmap::BitBltFromYUY2(PixCoord x2, PixCoord y2, const VBitmap *src, PixCoord x1, PixCoord y1, PixDim dx, PixDim dy) const throw() {
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

bool VBitmap::BitBltFromI420(PixCoord x2, PixCoord y2, const VBitmap *src, PixCoord x1, PixCoord y1, PixDim dx, PixDim dy) const throw() {
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

	CHECK_FPU_STACK
	return true;
}
