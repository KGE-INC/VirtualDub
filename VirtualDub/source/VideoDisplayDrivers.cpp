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
#define DIRECTDRAW_VERSION 0x0300
#define INITGUID
#include <vector>
#include <ddraw.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/log.h>
#include "VideoDisplay.h"
#include "VideoDisplayDrivers.h"
#include "VBitmap.h"

extern HINSTANCE g_hInst;


#if 0
	#define DEBUG_LOG(x) VDLog(kVDLogInfo, VDStringW(L##x))
#else
	#define DEBUG_LOG(x)
#endif

///////////////////////////////////////////////////////////////////////////

void VDMemcpyRect(void *dst, ptrdiff_t dststride, const void *src, ptrdiff_t srcstride, size_t w, size_t h) {
	if (w <= 0 || h <= 0)
		return;

	if (w == srcstride && w == dststride)
		memcpy(dst, src, w*h);
	else {
		char *dst2 = (char *)dst;
		const char *src2 = (const char *)src;

		do {
			memcpy(dst2, src2, w);
			dst2 += dststride;
			src2 += srcstride;
		} while(--h);
	}
}

namespace {
	struct YCbCrFormatInfo {
		ptrdiff_t	ystep;
		ptrdiff_t	cstep;
		ptrdiff_t	yinc[4];
		ptrdiff_t	cinc[4];
		sint8		ypos[4];
		sint8		cbpos[4];
		sint8		crpos[4];
	};

	YCbCrFormatInfo		g_formatInfo_YUV422_YUYV = { -8, -8, {-1,-1,-1,-1}, {-1,-1,-1,-1}, {0,2,4,6}, {1,1,5,5}, {3,3,7,7}};
	YCbCrFormatInfo		g_formatInfo_YUV422_UYVY = { -8, -8, {-1,-1,-1,-1}, {-1,-1,-1,-1}, {1,3,5,7}, {0,0,4,4}, {2,2,6,6}};

	uint16 ycbcr_to_1555(uint8 y, uint8 cb0, uint8 cr0) {
		const int cr = (int)cr0 - 128;
		const int cb = (int)cb0 - 128;
		const int i = ((int)y - 16) * 76309;
		int r = i + cr * 104597;
		int g = i + cb * -25674 + cr * -53279;
		int b = i + cb * 132201;

		if ((unsigned)r >= 0x1000000)
			r = ~r >> 31;
		if ((unsigned)g >= 0x1000000)
			g = ~g >> 31;
		if ((unsigned)b >= 0x1000000)
			b = ~b >> 31;

		return ((r & 0xf80000) >> 9) + ((g & 0xf80000) >> 14) + (b >> 19);
	}

	uint16 ycbcr_to_565(uint8 y, uint8 cb0, uint8 cr0) {
		const int cr = (int)cr0 - 128;
		const int cb = (int)cb0 - 128;
		const int i = ((int)y - 16) * 76309;
		int r = i + cr * 104597;
		int g = i + cb * -25674 + cr * -53279;
		int b = i + cb * 132201;

		if ((unsigned)r >= 0x1000000)
			r = ~r >> 31;
		if ((unsigned)g >= 0x1000000)
			g = ~g >> 31;
		if ((unsigned)b >= 0x1000000)
			b = ~b >> 31;

		return ((r & 0xf80000) >> 8) + ((g & 0xfc0000) >> 13) + (b >> 19);
	}

	void ycbcr_to_888(uint8 *dst, uint8 y, uint8 cb0, uint8 cr0) {
		const int cr = (int)cr0 - 128;
		const int cb = (int)cb0 - 128;
		const int i = ((int)y - 16) * 76309;
		int r = i + cr * 104597;
		int g = i + cb * -25674 + cr * -53279;
		int b = i + cb * 132201;

		if ((unsigned)r >= 0x1000000)
			r = ~r >> 31;
		if ((unsigned)g >= 0x1000000)
			g = ~g >> 31;
		if ((unsigned)b >= 0x1000000)
			b = ~b >> 31;

		dst[0] = (uint8)(b >> 16);
		dst[1] = (uint8)(g >> 16);
		dst[2] = (uint8)(r >> 16);
	}

	uint32 ycbcr_to_8888(uint8 y, uint8 cb0, uint8 cr0) {
		const int cr = (int)cr0 - 128;
		const int cb = (int)cb0 - 128;
		const int i = y * 76309;
		int r = i + cr * 104597;
		int g = i + cb * -25674 + cr * -53279;
		int b = i + cb * 132201;

		if ((unsigned)r >= 0x1000000)
			r = ~r >> 31;
		if ((unsigned)g >= 0x1000000)
			g = ~g >> 31;
		if ((unsigned)b >= 0x1000000)
			b = ~b >> 31;

		return (r & 0xff0000) + ((g & 0xff0000) >> 8) + (b >> 16);
	}

	void VDYCbCrToRGB1555Generic(void *dst, ptrdiff_t dststride, const void *yrow, ptrdiff_t ystride, const void *crrow, const void *cbrow, ptrdiff_t cstride, int w, int h, const YCbCrFormatInfo& formatinfo) {
		const ptrdiff_t ystep	= formatinfo.ystep;
		const ptrdiff_t cstep	= formatinfo.cstep;
		const ptrdiff_t ypos0	= formatinfo.ypos[0];
		const ptrdiff_t ypos1	= formatinfo.ypos[1];
		const ptrdiff_t ypos2	= formatinfo.ypos[2];
		const ptrdiff_t ypos3	= formatinfo.ypos[3];
		const ptrdiff_t crpos0	= formatinfo.crpos[0];
		const ptrdiff_t crpos1	= formatinfo.crpos[1];
		const ptrdiff_t crpos2	= formatinfo.crpos[2];
		const ptrdiff_t crpos3	= formatinfo.crpos[3];
		const ptrdiff_t cbpos0	= formatinfo.cbpos[0];
		const ptrdiff_t cbpos1	= formatinfo.cbpos[1];
		const ptrdiff_t cbpos2	= formatinfo.cbpos[2];
		const ptrdiff_t cbpos3	= formatinfo.cbpos[3];

		yrow	= (char *)yrow - ystep * ((w-1) >> 2);
		crrow	= (char *)crrow - cstep * ((w-1) >> 2);
		cbrow	= (char *)cbrow - cstep * ((w-1) >> 2);
		dst		= (char *)dst + 2*(w & ~3) - 8;

		int y = 0;
		do {
			const uint8 *ysrc	= (const uint8 *)yrow;
			const uint8 *crsrc	= (const uint8 *)crrow;
			const uint8 *cbsrc	= (const uint8 *)cbrow;
			uint16 *out = (uint16 *)dst;
			int w2 = -w;

			switch(w2 & 3) {
				do {
			case 0:	out[3] = ycbcr_to_1555(ysrc[ypos3], cbsrc[cbpos3], crsrc[crpos3]);
			case 1:	out[2] = ycbcr_to_1555(ysrc[ypos2], cbsrc[cbpos2], crsrc[crpos2]);
			case 2:	out[1] = ycbcr_to_1555(ysrc[ypos1], cbsrc[cbpos1], crsrc[crpos1]);
			case 3:	out[0] = ycbcr_to_1555(ysrc[ypos0], cbsrc[cbpos0], crsrc[crpos0]);
					out -= 4;
					ysrc += ystep;
					crsrc += cstep;
					cbsrc += cstep;
				} while((w2 += 4) < 0);
			}

			dst		= (char *)dst + dststride;
			yrow	= (const char *)yrow + (ystride & formatinfo.yinc[y & 3]);
			crrow	= (const char *)crrow + (cstride & formatinfo.cinc[y & 3]);
			cbrow	= (const char *)cbrow + (cstride & formatinfo.cinc[y & 3]);
		} while(++y < h);
	}

	void VDYCbCrToRGB565Generic(void *dst, ptrdiff_t dststride, const void *yrow, ptrdiff_t ystride, const void *crrow, const void *cbrow, ptrdiff_t cstride, int w, int h, const YCbCrFormatInfo& formatinfo) {
		const ptrdiff_t ystep	= formatinfo.ystep;
		const ptrdiff_t cstep	= formatinfo.cstep;
		const ptrdiff_t ypos0	= formatinfo.ypos[0];
		const ptrdiff_t ypos1	= formatinfo.ypos[1];
		const ptrdiff_t ypos2	= formatinfo.ypos[2];
		const ptrdiff_t ypos3	= formatinfo.ypos[3];
		const ptrdiff_t crpos0	= formatinfo.crpos[0];
		const ptrdiff_t crpos1	= formatinfo.crpos[1];
		const ptrdiff_t crpos2	= formatinfo.crpos[2];
		const ptrdiff_t crpos3	= formatinfo.crpos[3];
		const ptrdiff_t cbpos0	= formatinfo.cbpos[0];
		const ptrdiff_t cbpos1	= formatinfo.cbpos[1];
		const ptrdiff_t cbpos2	= formatinfo.cbpos[2];
		const ptrdiff_t cbpos3	= formatinfo.cbpos[3];

		yrow	= (char *)yrow - ystep * ((w-1) >> 2);
		crrow	= (char *)crrow - cstep * ((w-1) >> 2);
		cbrow	= (char *)cbrow - cstep * ((w-1) >> 2);
		dst		= (char *)dst + 2*(w & ~3) - 8;

		int y = 0;
		do {
			const uint8 *ysrc = (const uint8 *)yrow;
			const uint8 *crsrc = (const uint8 *)crrow;
			const uint8 *cbsrc = (const uint8 *)cbrow;
			uint16 *out = (uint16 *)dst;
			int w2 = -w;

			switch(w2 & 3) {
				do {
			case 0:	out[3] = ycbcr_to_565(ysrc[ypos3], cbsrc[cbpos3], crsrc[crpos3]);
			case 1:	out[2] = ycbcr_to_565(ysrc[ypos2], cbsrc[cbpos2], crsrc[crpos2]);
			case 2:	out[1] = ycbcr_to_565(ysrc[ypos1], cbsrc[cbpos1], crsrc[crpos1]);
			case 3:	out[0] = ycbcr_to_565(ysrc[ypos0], cbsrc[cbpos0], crsrc[crpos0]);
					out -= 4;
					ysrc += ystep;
					crsrc += cstep;
					cbsrc += cstep;
				} while((w2 += 4) < 0);
			}

			dst		= (char *)dst + dststride;
			yrow	= (const char *)yrow + (ystride & formatinfo.yinc[y & 3]);
			crrow	= (const char *)crrow + (cstride & formatinfo.cinc[y & 3]);
			cbrow	= (const char *)cbrow + (cstride & formatinfo.cinc[y & 3]);
		} while(++y < h);
	}

	void VDYCbCrToRGB888Generic(void *dst, ptrdiff_t dststride, const void *yrow, ptrdiff_t ystride, const void *crrow, const void *cbrow, ptrdiff_t cstride, int w, int h, const YCbCrFormatInfo& formatinfo) {
		const ptrdiff_t ystep	= formatinfo.ystep;
		const ptrdiff_t cstep	= formatinfo.cstep;
		const ptrdiff_t ypos0	= formatinfo.ypos[0];
		const ptrdiff_t ypos1	= formatinfo.ypos[1];
		const ptrdiff_t ypos2	= formatinfo.ypos[2];
		const ptrdiff_t ypos3	= formatinfo.ypos[3];
		const ptrdiff_t crpos0	= formatinfo.crpos[0];
		const ptrdiff_t crpos1	= formatinfo.crpos[1];
		const ptrdiff_t crpos2	= formatinfo.crpos[2];
		const ptrdiff_t crpos3	= formatinfo.crpos[3];
		const ptrdiff_t cbpos0	= formatinfo.cbpos[0];
		const ptrdiff_t cbpos1	= formatinfo.cbpos[1];
		const ptrdiff_t cbpos2	= formatinfo.cbpos[2];
		const ptrdiff_t cbpos3	= formatinfo.cbpos[3];

		yrow	= (char *)yrow - ystep * ((w-1) >> 2);
		crrow	= (char *)crrow - cstep * ((w-1) >> 2);
		cbrow	= (char *)cbrow - cstep * ((w-1) >> 2);
		dst		= (char *)dst + 3*(w & ~3) - 12;

		int y = 0;
		do {
			const uint8 *ysrc	= (const uint8 *)yrow;
			const uint8 *crsrc	= (const uint8 *)crrow;
			const uint8 *cbsrc	= (const uint8 *)cbrow;
			uint8 *out = (uint8 *)dst;
			int w2 = -w;

			switch(w2 & 3) {
				do {
			case 0:	ycbcr_to_888(out+9, ysrc[ypos3], cbsrc[cbpos3], crsrc[crpos3]);
			case 1:	ycbcr_to_888(out+6, ysrc[ypos2], cbsrc[cbpos2], crsrc[crpos2]);
			case 2:	ycbcr_to_888(out+3, ysrc[ypos1], cbsrc[cbpos1], crsrc[crpos1]);
			case 3:	ycbcr_to_888(out, ysrc[ypos0], cbsrc[cbpos0], crsrc[crpos0]);
					out -= 12;
					ysrc += ystep;
					crsrc += cstep;
					cbsrc += cstep;
				} while((w2 += 4) < 0);
			}

			dst		= (char *)dst + dststride;
			yrow	= (const char *)yrow + (ystride & formatinfo.yinc[y & 3]);
			crrow	= (const char *)crrow + (cstride & formatinfo.cinc[y & 3]);
			cbrow	= (const char *)cbrow + (cstride & formatinfo.cinc[y & 3]);
		} while(++y < h);
	}

	void VDYCbCrToRGB8888Generic(void *dst, ptrdiff_t dststride, const void *yrow, ptrdiff_t ystride, const void *crrow, const void *cbrow, ptrdiff_t cstride, int w, int h, const YCbCrFormatInfo& formatinfo) {
		const ptrdiff_t ystep	= formatinfo.ystep;
		const ptrdiff_t cstep	= formatinfo.cstep;
		const ptrdiff_t ypos0	= formatinfo.ypos[0];
		const ptrdiff_t ypos1	= formatinfo.ypos[1];
		const ptrdiff_t ypos2	= formatinfo.ypos[2];
		const ptrdiff_t ypos3	= formatinfo.ypos[3];
		const ptrdiff_t crpos0	= formatinfo.crpos[0];
		const ptrdiff_t crpos1	= formatinfo.crpos[1];
		const ptrdiff_t crpos2	= formatinfo.crpos[2];
		const ptrdiff_t crpos3	= formatinfo.crpos[3];
		const ptrdiff_t cbpos0	= formatinfo.cbpos[0];
		const ptrdiff_t cbpos1	= formatinfo.cbpos[1];
		const ptrdiff_t cbpos2	= formatinfo.cbpos[2];
		const ptrdiff_t cbpos3	= formatinfo.cbpos[3];

		yrow	= (char *)yrow - ystep * ((w-1) >> 2);
		crrow	= (char *)crrow - cstep * ((w-1) >> 2);
		cbrow	= (char *)cbrow - cstep * ((w-1) >> 2);
		dst		= (char *)dst + 4*(w & ~3) - 16;

		int y = 0;
		do {
			const uint8 *ysrc	= (const uint8 *)yrow;
			const uint8 *crsrc	= (const uint8 *)crrow;
			const uint8 *cbsrc	= (const uint8 *)cbrow;
			uint32 *out = (uint32 *)dst;
			int w2 = -w;

			switch(w2 & 3) {
				do {
			case 0:	out[3] = ycbcr_to_8888(ysrc[ypos3], cbsrc[cbpos3], crsrc[crpos3]);
			case 1:	out[2] = ycbcr_to_8888(ysrc[ypos2], cbsrc[cbpos2], crsrc[crpos2]);
			case 2:	out[1] = ycbcr_to_8888(ysrc[ypos1], cbsrc[cbpos1], crsrc[crpos1]);
			case 3:	out[0] = ycbcr_to_8888(ysrc[ypos0], cbsrc[cbpos0], crsrc[crpos0]);
					out -= 4;
					ysrc += ystep;
					crsrc += cstep;
					cbsrc += cstep;
				} while((w2 += 4) < 0);
			}

			dst		= (char *)dst + dststride;
			yrow	= (const char *)yrow + (ystride & formatinfo.yinc[y & 3]);
			crrow	= (const char *)crrow + (cstride & formatinfo.cinc[y & 3]);
			cbrow	= (const char *)cbrow + (cstride & formatinfo.cinc[y & 3]);
		} while(++y < h);
	}
}

namespace {

	#define TABROW(x)	TABENT(x+0),TABENT(x+1),TABENT(x+2),TABENT(x+3),TABENT(x+4),TABENT(x+5),TABENT(x+6),TABENT(x+7),TABENT(x+8),TABENT(x+9),TABENT(x+10),TABENT(x+11),TABENT(x+12),TABENT(x+13),TABENT(x+14),TABENT(x+15)
	#define TABLE		TABROW(0x00),TABROW(0x10),TABROW(0x20),TABROW(0x30),TABROW(0x40),TABROW(0x50),TABROW(0x60),TABROW(0x70),TABROW(0x80),TABROW(0x90),TABROW(0xA0),TABROW(0xB0),TABROW(0xC0),TABROW(0xD0),TABROW(0xE0),TABROW(0xF0),TABROW(0x100),TABROW(0x110),TABROW(0x120)

	// d     = spacing between shades
	// n     = number of shades
	//
	// require: dn = 255

	const uint8 rdithertab8[256+48]={
	#define	TABENT(x)	((x) > 255 ? 5*36 : (((x)*5) / 255)*36)
		TABLE
	#undef TABENT
	};
	const uint8 gdithertab8[256+48]={
	#define	TABENT(x)	((x) > 255 ? 5* 6 : (((x)*5) / 255)*6)
		TABLE
	#undef TABENT
	};
	const uint8 bdithertab8[256+48]={
	#define	TABENT(x)	((x) > 255 ? 5* 1 : (((x)*5) / 255)*1)
		TABLE
	#undef TABENT
	};
	#undef TABROW
	#undef TABLE
}

// 0 8 2 A
// C 4 E 6
// 3 B 1 9
// F 7 D 5

template<int d0, int d1, int d2, int d3>
struct VDDitherUtils {
	enum {
		rb0 = d0*51/16,
		rb1 = d1*51/16,
		rb2 = d2*51/16,
		rb3 = d3*51/16,
		g0 = d0*51/16,
		g1 = d1*51/16,
		g2 = d2*51/16,
		g3 = d3*51/16,
	};

	static void DoSpan16To8(uint8 *dstp, const uint16 *srcp, int w2, const uint8 *pLogPal) {
		uint32 px;

		switch(w2 & 3) {
			do {
		case 0:	px = srcp[0];
				dstp[w2  ] = pLogPal[rdithertab8[rb0 + ((px&0x7c00) >> 7)] + gdithertab8[g0 + ((px&0x03e0) >> 2)] + bdithertab8[rb0 + ((px&0x001f) << 3)]];
		case 1:	px = srcp[1];
				dstp[w2+1] = pLogPal[rdithertab8[rb1 + ((px&0x7c00) >> 7)] + gdithertab8[g1 + ((px&0x03e0) >> 2)] + bdithertab8[rb1 + ((px&0x001f) << 3)]];
		case 2:	px = srcp[2];
				dstp[w2+2] = pLogPal[rdithertab8[rb2 + ((px&0x7c00) >> 7)] + gdithertab8[g2 + ((px&0x03e0) >> 2)] + bdithertab8[rb2 + ((px&0x001f) << 3)]];
		case 3:	px = srcp[3];
				dstp[w2+3] = pLogPal[rdithertab8[rb3 + ((px&0x7c00) >> 7)] + gdithertab8[g3 + ((px&0x03e0) >> 2)] + bdithertab8[rb3 + ((px&0x001f) << 3)]];

				srcp += 4;
			} while((w2 += 4) < 0);
		}
	}

	static void DoSpan24To8(uint8 *dstp, const uint8 *srcp, int w2, const uint8 *pLogPal) {
		switch(w2 & 3) {
			do {
		case 0:	dstp[w2  ] = pLogPal[rdithertab8[rb0+srcp[ 2]] + gdithertab8[g0+srcp[ 1]] + bdithertab8[rb0+srcp[ 0]]];
		case 1:	dstp[w2+1] = pLogPal[rdithertab8[rb1+srcp[ 5]] + gdithertab8[g1+srcp[ 4]] + bdithertab8[rb1+srcp[ 3]]];
		case 2:	dstp[w2+2] = pLogPal[rdithertab8[rb2+srcp[ 8]] + gdithertab8[g2+srcp[ 7]] + bdithertab8[rb2+srcp[ 6]]];
		case 3:	dstp[w2+3] = pLogPal[rdithertab8[rb3+srcp[11]] + gdithertab8[g3+srcp[10]] + bdithertab8[rb3+srcp[ 9]]];

				srcp += 12;
			} while((w2 += 4) < 0);
		}
	}

	static void DoSpan32To8(uint8 *dstp, const uint8 *srcp, int w2, const uint8 *pLogPal) {
		switch(w2 & 3) {
			do {
		case 0:	dstp[w2  ] = pLogPal[rdithertab8[rb0+srcp[ 2]] + gdithertab8[g0+srcp[ 1]] + bdithertab8[rb0+srcp[ 0]]];
		case 1:	dstp[w2+1] = pLogPal[rdithertab8[rb1+srcp[ 6]] + gdithertab8[g1+srcp[ 5]] + bdithertab8[rb1+srcp[ 4]]];
		case 2:	dstp[w2+2] = pLogPal[rdithertab8[rb2+srcp[10]] + gdithertab8[g2+srcp[ 9]] + bdithertab8[rb2+srcp[ 8]]];
		case 3:	dstp[w2+3] = pLogPal[rdithertab8[rb3+srcp[14]] + gdithertab8[g3+srcp[13]] + bdithertab8[rb3+srcp[12]]];

				srcp += 16;
			} while((w2 += 4) < 0);
		}
	}
};

void VDDitherImage16To8(VBitmap& dst, const VBitmap& src, const uint8 *pLogPal) {
	int h = dst.h;
	int w = dst.w;

	uint8 *dstp0 = (uint8 *)dst.data;
	const uint16 *srcp0 = (const uint16 *)src.data;

	do {
		int w2 = -w;

		uint8 *dstp = dstp0 + w - (w2&3);
		const uint16 *srcp = srcp0;

		switch(h & 3) {
			case 0: VDDitherUtils< 0, 8, 2,10>::DoSpan16To8(dstp, srcp, w2, pLogPal); break;
			case 1: VDDitherUtils<12, 4,14, 6>::DoSpan16To8(dstp, srcp, w2, pLogPal); break;
			case 2: VDDitherUtils< 3,11, 1, 9>::DoSpan16To8(dstp, srcp, w2, pLogPal); break;
			case 3: VDDitherUtils<15, 7,13, 5>::DoSpan16To8(dstp, srcp, w2, pLogPal); break;
		}

		dstp0 += dst.pitch;
		srcp0 = (const uint16 *)((const char *)srcp0 + src.pitch);
	} while(--h);
}

void VDDitherImage24To8(VBitmap& dst, const VBitmap& src, const uint8 *pLogPal) {
	int h = dst.h;
	int w = dst.w;

	uint8 *dstp0 = (uint8 *)dst.data;
	const uint8 *srcp0 = (const uint8 *)src.data;

	do {
		int w2 = -w;

		uint8 *dstp = dstp0 + w - (w2&3);
		const uint8 *srcp = srcp0;

		switch(h & 3) {
			case 0: VDDitherUtils< 0, 8, 2,10>::DoSpan24To8(dstp, srcp, w2, pLogPal); break;
			case 1: VDDitherUtils<12, 4,14, 6>::DoSpan24To8(dstp, srcp, w2, pLogPal); break;
			case 2: VDDitherUtils< 3,11, 1, 9>::DoSpan24To8(dstp, srcp, w2, pLogPal); break;
			case 3: VDDitherUtils<15, 7,13, 5>::DoSpan24To8(dstp, srcp, w2, pLogPal); break;
		}

		dstp0 += dst.pitch;
		srcp0 += src.pitch;
	} while(--h);
}

void VDDitherImage32To8(VBitmap& dst, const VBitmap& src, const uint8 *pLogPal) {
	int h = dst.h;
	int w = dst.w;

	uint8 *dstp0 = (uint8 *)dst.data;
	const uint8 *srcp0 = (const uint8 *)src.data;

	do {
		int w2 = -w;

		uint8 *dstp = dstp0 + w - (w2&3);
		const uint8 *srcp = srcp0;

		switch(h & 3) {
			case 0: VDDitherUtils< 0, 8, 2,10>::DoSpan32To8(dstp, srcp, w2, pLogPal); break;
			case 1: VDDitherUtils<12, 4,14, 6>::DoSpan32To8(dstp, srcp, w2, pLogPal); break;
			case 2: VDDitherUtils< 3,11, 1, 9>::DoSpan32To8(dstp, srcp, w2, pLogPal); break;
			case 3: VDDitherUtils<15, 7,13, 5>::DoSpan32To8(dstp, srcp, w2, pLogPal); break;
		}

		dstp0 += dst.pitch;
		srcp0 += src.pitch;
	} while(--h);
}

void VDDitherImage(VBitmap& dst, const VBitmap& src, const uint8 *pLogPal) {
	VDASSERT(dst.w == src.w && dst.h == src.h);

	if (dst.w<=0 || dst.h<=0)
		return;

	if (dst.depth == 8) {
		switch(src.depth) {
		case 16:
			VDDitherImage16To8(dst, src, pLogPal);
			break;
		case 24:
			VDDitherImage24To8(dst, src, pLogPal);
			break;
		case 32:
			VDDitherImage32To8(dst, src, pLogPal);
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////

#include <GL/GL.h>
#include <vd2/external/glATI.h>			// I feel dirty using this, but I can't figure out the license for the SGI glext.h pointed to by NVIDIA.

// from OpenGL 1.2 EXT_packed_pixels

#ifndef GL_UNSIGNED_SHORT_5_6_5
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif
#ifndef GL_UNSIGNED_SHORT_5_6_5_REV
#define GL_UNSIGNED_SHORT_5_6_5_REV 0x8364
#endif
#ifndef GL_UNSIGNED_SHORT_1_5_5_5_REV
#define GL_UNSIGNED_SHORT_1_5_5_5_REV 0x8366
#endif

struct VDVideoDisplayGLTable {
	HGLRC (APIENTRY *pwglCreateContext)(HDC hdc);
	BOOL (APIENTRY *pwglDeleteContext)(HGLRC hglrc);
	BOOL (APIENTRY *pwglMakeCurrent)(HDC hdc, HGLRC hglrc);
	void (APIENTRY *pglBegin)(GLenum mode);
	void (APIENTRY *pglBindTexture)(GLenum target, GLuint texture);
	void (APIENTRY *pglColor4d)(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
	void (APIENTRY *pglDeleteTextures)(GLsizei n, const GLuint *textures);
	void (APIENTRY *pglDisable)(GLenum cap);
	void (APIENTRY *pglEnable)(GLenum cap);
	void (APIENTRY *pglEnd)();
	void (APIENTRY *pglFlush)();
	void (APIENTRY *pglGenTextures)(GLsizei n, GLuint *textures);
	GLenum (APIENTRY *pglGetError)();
	void (APIENTRY *pglGetIntegerv)(GLenum pname, GLint *params);
	const GLubyte *(APIENTRY *pglGetString)(GLenum name);
	void (APIENTRY *pglLoadIdentity)();
	void (APIENTRY *pglLoadMatrixd)(const GLdouble *m);
	void (APIENTRY *pglMatrixMode)(GLenum target);
	void (APIENTRY *pglPixelStorei)(GLenum pname, GLint param);
	void (APIENTRY *pglTexCoord2d)(GLdouble s, GLdouble t);
	void (APIENTRY *pglTexEnvi)(GLenum target, GLenum pname, GLint param);
	void (APIENTRY *pglTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
	void (APIENTRY *pglTexParameterfv)(GLenum target, GLenum pname, const GLfloat *params);
	void (APIENTRY *pglTexParameteri)(GLenum target, GLenum pname, GLint param);
	void (APIENTRY *pglTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
	void (APIENTRY *pglVertex2d)(GLdouble x, GLdouble y);
	void (APIENTRY *pglViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
};

static const char *const sOpenGLFuncs[]={
	"wglCreateContext",
	"wglDeleteContext",
	"wglMakeCurrent",
	"glBegin",
	"glBindTexture",
	"glColor4d",
	"glDeleteTextures",
	"glDisable",
	"glEnable",
	"glEnd",
	"glFlush",
	"glGenTextures",
	"glGetError",
	"glGetIntegerv",
	"glGetString",
	"glLoadIdentity",
	"glLoadMatrixd",
	"glMatrixMode",
	"glPixelStorei",
	"glTexCoord2d",
	"glTexEnvi",
	"glTexImage2D",
	"glTexParameterfv",
	"glTexParameteri",
	"glTexSubImage2D",
	"glVertex2d",
	"glViewport",
};

static int g_openGLref;
static HMODULE g_hmodOpenGL;
static VDVideoDisplayGLTable g_openGLtable;

VDVideoDisplayGLTable *VDInitOpenGL() {
	if (!g_openGLref) {
		g_hmodOpenGL = LoadLibrary("opengl32");
		if (!g_hmodOpenGL)
			return NULL;

		for(int i=0; i<sizeof sOpenGLFuncs / sizeof sOpenGLFuncs[0]; ++i) {
			FARPROC fp = GetProcAddress(g_hmodOpenGL, sOpenGLFuncs[i]);

			if (!fp) {
				FreeLibrary(g_hmodOpenGL);
				return NULL;
			}

			((FARPROC *)&g_openGLtable)[i] = fp;
		}
	}

	++g_openGLref;
	return &g_openGLtable;
}

void VDShutdownOpenGL() {
	if (!--g_openGLref)
		FreeLibrary(g_hmodOpenGL);
}

///////////////////////////////////////////////////////////////////////////

class VDVideoTextureTilePatternOpenGL {
public:
	struct TileInfo {
		float	mInvU;
		float	mInvV;
		int		mSrcX;
		int		mSrcY;
		int		mSrcW;
		int		mSrcH;
		bool	mbLeft;
		bool	mbTop;
		bool	mbRight;
		bool	mbBottom;
		GLuint	mTextureID;
	};

	void Init(VDVideoDisplayGLTable *pgl, int w, int h, bool bPackedPixelsSupported, bool bEdgeClampSupported, bool bMipmapGenerationSupported);
	void Shutdown(VDVideoDisplayGLTable *pgl);

	bool IsInited() const { return !mTextures.empty(); }

	void GetTileInfo(TileInfo*& pInfo, int& nTiles);

protected:
	int			mTextureTilesW;
	int			mTextureTilesH;
	int			mTextureSize;
	double		mTextureSizeInv;
	int			mTextureLastW;
	int			mTextureLastH;
	double		mTextureLastWInvPow2;
	double		mTextureLastHInvPow2;
	std::vector<GLuint>		mTextures;
	std::vector<TileInfo>	mTileInfo;

	bool		mbPackedPixelsSupported;
	bool		mbEdgeClampSupported;
	bool		mbMipmapGenerationSupported;
};

void VDVideoTextureTilePatternOpenGL::Init(VDVideoDisplayGLTable *pgl, int w, int h, bool bPackedPixelsSupported, bool bEdgeClampSupported, bool bMipmapGenerationSupported) {
	mbPackedPixelsSupported		= bPackedPixelsSupported;
	mbEdgeClampSupported		= bEdgeClampSupported;
	mbMipmapGenerationSupported = bMipmapGenerationSupported;

	GLint maxsize;
	pgl->pglGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxsize);

	mTextureSize	= maxsize;
	mTextureSizeInv	= 1.0 / maxsize;
	mTextureTilesW	= (w  - 1 + (maxsize-2)) / (maxsize - 1);
	mTextureTilesH	= (h - 1 + (maxsize-2)) / (maxsize - 1);

	int ntiles = mTextureTilesW * mTextureTilesH;
	int xlast = w - (mTextureTilesW - 1)*(maxsize - 1);
	int ylast = h - (mTextureTilesH - 1)*(maxsize - 1);
	int xlasttex = 1;
	int ylasttex = 1;

	while(xlasttex < xlast)
		xlasttex += xlasttex;
	while(ylasttex < ylast)
		ylasttex += ylasttex;

	int largestW = mTextureSize;
	int largestH = mTextureSize;

	if (mTextureTilesW == 1)
		largestW = xlasttex;
	if (mTextureTilesH == 1)
		largestH = ylasttex;

	mTextureLastW = xlast;
	mTextureLastH = ylast;
	mTextureLastWInvPow2	= 1.0 / xlasttex;
	mTextureLastHInvPow2	= 1.0 / ylasttex;

	mTextures.resize(ntiles);

	pgl->pglGenTextures(ntiles, &mTextures[0]);

	vdautoblockptr zerobuffer(malloc(4 * largestW * largestH));
	memset(zerobuffer, 0, 4 * largestW * largestH);

	pgl->pglPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	pgl->pglPixelStorei(GL_UNPACK_ROW_LENGTH, 0);


	int tile = 0;
	for(int y = 0; y < mTextureTilesH; ++y) {
		for(int x = 0; x < mTextureTilesW; ++x, ++tile) {
			pgl->pglBindTexture(GL_TEXTURE_2D, mTextures[tile]);

			if (mbMipmapGenerationSupported) {
				pgl->pglTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
				pgl->pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			} else {
				pgl->pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			}
			pgl->pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			if (mbEdgeClampSupported) {
				pgl->pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE_EXT);
				pgl->pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE_EXT);
			} else {
				pgl->pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
				pgl->pglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

				static const float black[4]={0.f,0.f,0.f,0.f};
				pgl->pglTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, black);
			}

			int w = (x==mTextureTilesW-1) ? xlasttex : maxsize;
			int h = (y==mTextureTilesH-1) ? ylasttex : maxsize;

			if (w==maxsize && h==maxsize)
				pgl->pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
			else
				pgl->pglTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, zerobuffer.get());

			TileInfo info;

			info.mInvU	= 1.0f / w;
			info.mInvV	= 1.0f / h;
			info.mSrcX	= mTextureSize * x;
			info.mSrcY	= mTextureSize * y;
			info.mSrcW	= (x==mTextureTilesW-1) ? xlast : mTextureSize;
			info.mSrcH	= (y==mTextureTilesH-1) ? ylast : mTextureSize;
			info.mbLeft		= x<=0;
			info.mbTop		= y<=0;
			info.mbRight	= (x==mTextureTilesW-1);
			info.mbBottom	= (y==mTextureTilesH-1);
			info.mTextureID	= mTextures[tile];

			mTileInfo.push_back(info);
		}
	}
}

void VDVideoTextureTilePatternOpenGL::Shutdown(VDVideoDisplayGLTable *pgl) {
	if (mTextures.empty()) {
		int nTextures = mTextures.size();

		if (mbMipmapGenerationSupported) {
			for(int i=0; i<nTextures; ++i) {
				pgl->pglBindTexture(GL_TEXTURE_2D, mTextures[i]);
				pgl->pglTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);
			}
		}

		pgl->pglDeleteTextures(nTextures, &mTextures[0]);
		std::vector<GLuint>().swap(mTextures);
	}
	std::vector<TileInfo>().swap(mTileInfo);
}

void VDVideoTextureTilePatternOpenGL::GetTileInfo(TileInfo*& pInfo, int& nTiles) {
	pInfo = &mTileInfo[0];
	nTiles = mTileInfo.size();
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDisplayMinidriverOpenGL : public IVDVideoDisplayMinidriver {
public:
	VDVideoDisplayMinidriverOpenGL();
	~VDVideoDisplayMinidriverOpenGL();

	bool Init(HWND hwnd, const VDVideoDisplaySourceInfo& info);
	void Shutdown();

	bool ModifySource(const VDVideoDisplaySourceInfo& info);

	bool IsValid() { return mbValid; }

	bool Tick(int id) { return true; }
	bool Resize();
	bool Update(FieldMode);
	void Refresh(FieldMode);
	bool Paint(HDC hdc, const RECT& rClient) { return true; }
	void SetLogicalPalette(const uint8 *pLogicalPalette) {}

protected:
	void Upload(VDVideoDisplaySourceInfo& source, VDVideoTextureTilePatternOpenGL& texPattern);

	static ATOM VDVideoDisplayMinidriverOpenGL::Register();
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	bool OnOpenGLInit();
	void OnDestroy();
	void OnPaint();

	
	HWND		mhwnd;
	HWND		mhwndOGL;
	VDVideoDisplayGLTable	*mpgl;
	HGLRC		mhglrc;
	bool		mbValid;
	bool		mbVerticalFlip;

	VDVideoTextureTilePatternOpenGL		mTexPattern[2];
	VDVideoDisplaySourceInfo			mSource;
};

#define MYWM_OGLINIT		(WM_USER + 0x180)

IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverOpenGL() {
	return new VDVideoDisplayMinidriverOpenGL;
}

VDVideoDisplayMinidriverOpenGL::VDVideoDisplayMinidriverOpenGL()
	: mhwndOGL(0)
	, mpgl(0)
	, mhglrc(0)
	, mbValid(false)
{
}

VDVideoDisplayMinidriverOpenGL::~VDVideoDisplayMinidriverOpenGL() {
}

bool VDVideoDisplayMinidriverOpenGL::Init(HWND hwnd, const VDVideoDisplaySourceInfo& info) {
	mSource = info;
	mhwnd = hwnd;

	// OpenGL doesn't allow upside-down texture uploads, so we simply
	// upload the surface inverted and then reinvert on display.
	mbVerticalFlip = false;
	if (mSource.stride < 0) {
		mSource.data = (const char *)mSource.data + mSource.stride*(mSource.h - 1);
		mSource.stride = -mSource.stride;
		mbVerticalFlip = true;
	}

	RECT r;
	GetClientRect(mhwnd, &r);

	static ATOM wndClass = Register();

 	mpgl = VDInitOpenGL();

	if (mpgl) {
		// We have to create a separate window because the NVIDIA driver subclasses the
		// window and doesn't unsubclass it even after the OpenGL context is deleted.
		// If we use the main window instead then the app will bomb the moment we unload
		// OpenGL.

		mhwndOGL = CreateWindowEx(WS_EX_TRANSPARENT, (LPCSTR)wndClass, "", WS_CHILD|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0, r.right, r.bottom, mhwnd, NULL, g_hInst, this);
		if (mhwndOGL) {
			if (SendMessage(mhwndOGL, MYWM_OGLINIT, 0, 0)) {
				mbValid = false;
				return true;
			}

			DestroyWindow(mhwndOGL);
			mhwndOGL = 0;
		}

		VDShutdownOpenGL();
		mpgl = 0;
	}

	return false;
}

void VDVideoDisplayMinidriverOpenGL::Shutdown() {
	if (mpgl) {
		if (mhwndOGL) {
			DestroyWindow(mhwndOGL);
			mhwndOGL = 0;
		}
		VDShutdownOpenGL();
		mpgl = 0;
	}
	mbValid = false;
}

bool VDVideoDisplayMinidriverOpenGL::ModifySource(const VDVideoDisplaySourceInfo& info) {
	if (!mpgl)
		return false;

	if (info.w == mSource.w && info.h == mSource.h && info.format == mSource.format && info.bInterlaced == mSource.bInterlaced) {
		mSource = info;
		// OpenGL doesn't allow upside-down texture uploads, so we simply
		// upload the surface inverted and then reinvert on display.
		mbVerticalFlip = false;
		if (mSource.stride < 0) {
			mSource.data = (const char *)mSource.data + mSource.stride*(mSource.h - 1);
			mSource.stride = -mSource.stride;
			mbVerticalFlip = true;
		}
		return true;
	}

	return false;
}

bool VDVideoDisplayMinidriverOpenGL::Resize() {
	if (mhwndOGL) {
		RECT r;

		GetClientRect(mhwnd, &r);
		SetWindowPos(mhwndOGL, 0, 0, 0, r.right, r.bottom, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);
	}

	return true;
}

bool VDVideoDisplayMinidriverOpenGL::Update(FieldMode fieldmode) {
	if (!mpgl)
		return false;

	if (HDC hdc = GetDC(mhwndOGL)) {
		mpgl->pwglMakeCurrent(hdc, mhglrc);
		VDASSERT(mpgl->pglGetError() == GL_NO_ERROR);

		if (mSource.bInterlaced) {
			if (fieldmode == kAllFields || fieldmode == kEvenFieldsOnly) {
				VDVideoDisplaySourceInfo evenFieldSrc(mSource);

				evenFieldSrc.h = (evenFieldSrc.h+1) >> 1;
				evenFieldSrc.stride += evenFieldSrc.stride;

				Upload(evenFieldSrc, mTexPattern[0]);
			}
			if (fieldmode == kAllFields || fieldmode == kOddFieldsOnly) {
				VDVideoDisplaySourceInfo oddFieldSrc(mSource);

				oddFieldSrc.data = (const char *)oddFieldSrc.data + oddFieldSrc.stride;
				oddFieldSrc.h = (oddFieldSrc.h+1) >> 1;
				oddFieldSrc.stride += oddFieldSrc.stride;

				Upload(oddFieldSrc, mTexPattern[1]);
			}
		} else {
			Upload(mSource, mTexPattern[0]);
		}

		VDASSERT(mpgl->pglGetError() == GL_NO_ERROR);

		mpgl->pglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
//		mpgl->pwglMakeCurrent(NULL, NULL);

		mbValid = true;

		ReleaseDC(mhwndOGL, hdc);
	}

	return true;
}

void VDVideoDisplayMinidriverOpenGL::Refresh(FieldMode) {
	if (mbValid) {
		InvalidateRect(mhwndOGL, NULL, FALSE);
		UpdateWindow(mhwndOGL);
	}
}

void VDVideoDisplayMinidriverOpenGL::Upload(VDVideoDisplaySourceInfo& source, VDVideoTextureTilePatternOpenGL& texPattern) {
	VDVideoTextureTilePatternOpenGL::TileInfo *pTiles;
	int nTiles;

	mpgl->pglPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	switch(source.format) {
	case IVDVideoDisplay::kFormatRGB1555:
	case IVDVideoDisplay::kFormatRGB565:
		mpgl->pglPixelStorei(GL_UNPACK_ROW_LENGTH, source.stride >> 1);
		break;
	case IVDVideoDisplay::kFormatRGB888:
		mpgl->pglPixelStorei(GL_UNPACK_ROW_LENGTH, source.stride / 3);
		break;
	case IVDVideoDisplay::kFormatRGB8888:
		mpgl->pglPixelStorei(GL_UNPACK_ROW_LENGTH, source.stride >> 2);
		break;
	}

	texPattern.GetTileInfo(pTiles, nTiles);

	for(int tile=0; tile<nTiles; ++tile) {
		VDVideoTextureTilePatternOpenGL::TileInfo& tile = *pTiles++;

		mpgl->pglBindTexture(GL_TEXTURE_2D, tile.mTextureID);

		switch(source.format) {
		case IVDVideoDisplay::kFormatRGB1555:
			mpgl->pglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tile.mSrcW, tile.mSrcH, GL_BGRA_EXT, GL_UNSIGNED_SHORT_1_5_5_5_REV, (const char *)source.data + (source.stride*tile.mSrcY + tile.mSrcX*2));
			break;
		case IVDVideoDisplay::kFormatRGB565:
			mpgl->pglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tile.mSrcW, tile.mSrcH, GL_BGR_EXT, GL_UNSIGNED_SHORT_5_6_5_REV, (const char *)source.data + (source.stride*tile.mSrcY + tile.mSrcX*2));
			break;
		case IVDVideoDisplay::kFormatRGB888:
			mpgl->pglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tile.mSrcW, tile.mSrcH, GL_BGR_EXT, GL_UNSIGNED_BYTE, (const char *)source.data + (source.stride*tile.mSrcY + tile.mSrcX*3));
			break;
		case IVDVideoDisplay::kFormatRGB8888:
			mpgl->pglTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tile.mSrcW, tile.mSrcH, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (const char *)source.data + (source.stride*tile.mSrcY + tile.mSrcX*4));
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////

ATOM VDVideoDisplayMinidriverOpenGL::Register() {
	WNDCLASS wc;

	wc.style			= CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc		= StaticWndProc;
	wc.cbClsExtra		= 0;
	wc.cbWndExtra		= sizeof(VDVideoDisplayMinidriverOpenGL *);
	wc.hInstance		= g_hInst;
	wc.hIcon			= 0;
	wc.hCursor			= 0;
	wc.hbrBackground	= 0;
	wc.lpszMenuName		= 0;
	wc.lpszClassName	= "phaeronOpenGLVideoDisplay";

	return RegisterClass(&wc);
}

LRESULT CALLBACK VDVideoDisplayMinidriverOpenGL::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDVideoDisplayMinidriverOpenGL *pThis = (VDVideoDisplayMinidriverOpenGL *)GetWindowLongPtr(hwnd, 0);

	switch(msg) {
	case WM_NCCREATE:
		pThis = (VDVideoDisplayMinidriverOpenGL *)((LPCREATESTRUCT)lParam)->lpCreateParams;
		SetWindowLongPtr(hwnd, 0, (DWORD_PTR)pThis);
		pThis->mhwndOGL = hwnd;
		break;
	}

	return pThis ? pThis->WndProc(msg, wParam, lParam) : DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT VDVideoDisplayMinidriverOpenGL::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case MYWM_OGLINIT:
		return OnOpenGLInit();
	case WM_DESTROY:
		OnDestroy();
		break;
	case WM_PAINT:
		OnPaint();
		return 0;
	case WM_NCHITTEST:
		return HTTRANSPARENT;
	}

	return DefWindowProc(mhwndOGL, msg, wParam, lParam);
}

bool VDVideoDisplayMinidriverOpenGL::OnOpenGLInit() {
	if (HDC hdc = GetDC(mhwndOGL)) {
		PIXELFORMATDESCRIPTOR pfd = {sizeof(PIXELFORMATDESCRIPTOR)};

		pfd.nVersion		= 1;
		pfd.dwFlags			= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_GENERIC_ACCELERATED | PFD_DEPTH_DONTCARE | PFD_STEREO_DONTCARE;
		pfd.iPixelType		= PFD_TYPE_RGBA;
		pfd.iLayerType		= PFD_MAIN_PLANE;

		int formatIndex = ChoosePixelFormat(hdc, &pfd);

		if (formatIndex) {
			if (SetPixelFormat(hdc, formatIndex, &pfd)) {
				mhglrc = mpgl->pwglCreateContext(hdc);
				if (mhglrc) {
					mpgl->pwglMakeCurrent(hdc, mhglrc);

					VDDEBUG("VideoDisplay: OpenGL version string: [%s]\n", g_openGLtable.pglGetString(GL_VERSION));

					const GLubyte *pExtensions = mpgl->pglGetString(GL_EXTENSIONS);

					std::vector<char> extstr(strlen((const char *)pExtensions)+1);
					std::copy(pExtensions, pExtensions + extstr.size(), &extstr[0]);

					char *s = &extstr[0];

					bool bPackedPixelsSupported = false;
					bool bEdgeClampSupported = false;
					bool bMipmapGenerationSupported = false;

					while(const char *tok = strtok(s, " ")) {
						if (!strcmp(tok, "GL_EXT_packed_pixels"))
							bPackedPixelsSupported = true;
						else if (!strcmp(tok, "GL_EXT_texture_edge_clamp"))
							bEdgeClampSupported = true;
						else if (!strcmp(tok, "GL_SGIS_generate_mipmap"))
							bMipmapGenerationSupported = true;
						s = NULL;
					}

					if (mSource.bInterlaced) {
						mTexPattern[0].Init(mpgl, mSource.w, (mSource.h+1)>>1, bPackedPixelsSupported, bEdgeClampSupported, bMipmapGenerationSupported);
						mTexPattern[1].Init(mpgl, mSource.w, mSource.h>>1, bPackedPixelsSupported, bEdgeClampSupported, bMipmapGenerationSupported);
					} else
						mTexPattern[0].Init(mpgl, mSource.w, mSource.h, bPackedPixelsSupported, bEdgeClampSupported, bMipmapGenerationSupported);

					VDASSERT(mpgl->pglGetError() == GL_NO_ERROR);

					mpgl->pwglMakeCurrent(NULL, NULL);
					ReleaseDC(mhwndOGL, hdc);

					VDDEBUG("VideoDisplay: Using OpenGL for %dx%d display.\n", mSource.w, mSource.h);
					return true;
				}
			}
		}

		ReleaseDC(mhwndOGL, hdc);
	}

	return false;
}

void VDVideoDisplayMinidriverOpenGL::OnDestroy() {
	if (mhglrc) {
		if (mTexPattern[0].IsInited() || mTexPattern[1].IsInited()) {
			if (HDC hdc = GetDC(mhwndOGL)) {
				mpgl->pwglMakeCurrent(hdc, mhglrc);
				mTexPattern[0].Shutdown(mpgl);
				mTexPattern[1].Shutdown(mpgl);
				mpgl->pwglMakeCurrent(NULL, NULL);
			}
		}

		mpgl->pwglDeleteContext(mhglrc);
		mhglrc = 0;
	}
}

void VDVideoDisplayMinidriverOpenGL::OnPaint() {
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(mhwndOGL, &ps);

	if (!hdc)
		return;

	RECT r;
	GetClientRect(mhwndOGL, &r);

	if (mhglrc) {
		mpgl->pwglMakeCurrent(hdc, mhglrc);

		mpgl->pglViewport(0, 0, r.right, r.bottom);

		mpgl->pglMatrixMode(GL_PROJECTION);
		mpgl->pglLoadIdentity();
		mpgl->pglMatrixMode(GL_MODELVIEW);

		mpgl->pglDisable(GL_ALPHA_TEST);
		mpgl->pglDisable(GL_DEPTH_TEST);
		mpgl->pglDisable(GL_STENCIL_TEST);
		mpgl->pglDisable(GL_BLEND);
		mpgl->pglDisable(GL_CULL_FACE);
		mpgl->pglEnable(GL_DITHER);
		mpgl->pglEnable(GL_TEXTURE_2D);

		VDVideoTextureTilePatternOpenGL::TileInfo *pTiles;
		int nTiles;

		if (mSource.bInterlaced) {
			const int dstW = r.right;
			const int dstH = r.bottom;

			const double viewmat[16]={
				2.0 / mSource.w,
				0,
				0,
				0,

				0,
				(mbVerticalFlip ? +2.0 : -2.0) / dstH,
				0,
				0,

				0,
				0,
				0,
				0.0,

				-1.0,
				(mbVerticalFlip ? -1.0 : +1.0),
				-0.5,
				+1.0,
			};

			mpgl->pglLoadMatrixd(viewmat);

			for(int field=0; field<2; ++field) {
				mTexPattern[field].GetTileInfo(pTiles, nTiles);

				for(int tile=0; tile<nTiles; ++tile) {
					VDVideoTextureTilePatternOpenGL::TileInfo& tile = *pTiles++;		

					double	xbase	= tile.mSrcX;
					double	ybase	= tile.mSrcY;

					int		w = tile.mSrcW;
					int		h = tile.mSrcH;
					double	iw = tile.mInvU;
					double	ih = tile.mInvV;

					double	px1 = tile.mbLeft ? 0 : 0.5;
					double	py1 = tile.mbTop ? 0 : 0.5;
					double	px2 = w - (tile.mbRight ? 0 : 0.5f);
					double	py2 = h - (tile.mbBottom ? 0 : 0.5f);

					double	u1 = iw * px1;
					double	u2 = iw * px2;

					ih *= mSource.h / (double)dstH * 0.5;

					mpgl->pglBindTexture(GL_TEXTURE_2D, tile.mTextureID);

					int ytop	= VDRoundToInt(ceil(((ybase + py1)*2 + field - 0.5) * (dstH / (double)mSource.h) - 0.5));
					int ybottom	= VDRoundToInt(ceil(((ybase + py2)*2 + field - 0.5) * (dstH / (double)mSource.h) - 0.5));

					if ((ytop^field) & 1)
						++ytop;

					mpgl->pglBegin(GL_QUADS);
					mpgl->pglColor4d(1.0f, 1.0f, 1.0f, 1.0f);

					for(int ydst = ytop; ydst < ybottom; ydst += 2) {
						mpgl->pglTexCoord2d(u1, (ydst  -ybase-field+0.5)*ih);		mpgl->pglVertex2d(xbase + px1, ydst);
						mpgl->pglTexCoord2d(u1, (ydst+1-ybase-field+0.5)*ih);		mpgl->pglVertex2d(xbase + px1, ydst+1);
						mpgl->pglTexCoord2d(u2, (ydst+1-ybase-field+0.5)*ih);		mpgl->pglVertex2d(xbase + px2, ydst+1);
						mpgl->pglTexCoord2d(u2, (ydst  -ybase-field+0.5)*ih);		mpgl->pglVertex2d(xbase + px2, ydst);
					}

					mpgl->pglEnd();
				}
			}
		} else {
			const double viewmat[16]={
				2.0 / mSource.w,
				0,
				0,
				0,

				0,
				(mbVerticalFlip ? +2.0 : -2.0) / mSource.h,
				0,
				0,

				0,
				0,
				0,
				0.0,

				-1.0,
				(mbVerticalFlip ? -1.0 : +1.0),
				-0.5,
				+1.0,
			};

			mpgl->pglLoadMatrixd(viewmat);

			mTexPattern[0].GetTileInfo(pTiles, nTiles);
			for(int tile=0; tile<nTiles; ++tile) {
				VDVideoTextureTilePatternOpenGL::TileInfo& tile = *pTiles++;		

				double	xbase	= tile.mSrcX;
				double	ybase	= tile.mSrcY;

				int		w = tile.mSrcW;
				int		h = tile.mSrcH;
				double	iw = tile.mInvU;
				double	ih = tile.mInvV;

				double	px1 = tile.mbLeft ? 0 : 0.5;
				double	py1 = tile.mbTop ? 0 : 0.5;
				double	px2 = w - (tile.mbRight ? 0 : 0.5f);
				double	py2 = h - (tile.mbBottom ? 0 : 0.5f);

				double	u1 = iw * px1;
				double	v1 = ih * py1;
				double	u2 = iw * px2;
				double	v2 = ih * py2;

				mpgl->pglBindTexture(GL_TEXTURE_2D, tile.mTextureID);

				mpgl->pglBegin(GL_QUADS);
				mpgl->pglColor4d(1.0f, 1.0f, 1.0f, 1.0f);
				mpgl->pglTexCoord2d(u1, v1);		mpgl->pglVertex2d(xbase + px1, ybase + py1);
				mpgl->pglTexCoord2d(u1, v2);		mpgl->pglVertex2d(xbase + px1, ybase + py2);
				mpgl->pglTexCoord2d(u2, v2);		mpgl->pglVertex2d(xbase + px2, ybase + py2);
				mpgl->pglTexCoord2d(u2, v1);		mpgl->pglVertex2d(xbase + px2, ybase + py1);
				mpgl->pglEnd();
			}
		}

		VDASSERT(mpgl->pglGetError() == GL_NO_ERROR);

		mpgl->pglFlush();

		SwapBuffers(hdc);

//		mpgl->pwglMakeCurrent(NULL, NULL);
	}

	EndPaint(mhwndOGL, &ps);
}


///////////////////////////////////////////////////////////////////////////

class IVDDirectDrawClient {
public:
	virtual void DirectDrawShutdown() = 0;
	virtual void DirectDrawPrimaryRestored() = 0;
};

class IVDDirectDrawManager {
public:
	virtual IDirectDraw2 *GetDDraw() = 0;
	virtual const DDCAPS& GetCaps() = 0;
	virtual IDirectDrawSurface2 *GetPrimary() = 0;
	virtual const DDSURFACEDESC& GetPrimaryDesc() = 0;
	virtual bool Restore() = 0;
};

class VDDirectDrawManager : public IVDDirectDrawManager {
public:
	bool Init(IVDDirectDrawClient *pClient);
	void Shutdown(IVDDirectDrawClient *pClient);

	IDirectDraw2 *GetDDraw() { return mpdd; }
	const DDCAPS& GetCaps() { return mCaps; }

	IDirectDrawSurface2 *GetPrimary() { return mpddsPrimary; }
	const DDSURFACEDESC& GetPrimaryDesc() { return mPrimaryDesc; }

	bool Restore();

protected:
	bool InitPrimary();
	void ShutdownPrimary();


	int mInitCount;

	HMODULE					mhmodDD;

	IDirectDraw2			*mpdd;
	IDirectDrawSurface2		*mpddsPrimary;

	DDSURFACEDESC			mPrimaryDesc;
	DDCAPS					mCaps;

	typedef std::vector<IVDDirectDrawClient *> tClients;
	tClients mClients;
};

bool VDDirectDrawManager::Init(IVDDirectDrawClient *pClient) {
	if (mInitCount) {
		++mInitCount;
		mClients.push_back(pClient);
		return true;
	}

	mhmodDD = LoadLibrary("ddraw");

	if (!mhmodDD)
		return false;

	do {
		typedef HRESULT (WINAPI *tpDirectDrawCreate)(GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter);
		tpDirectDrawCreate pDirectDrawCreate = (tpDirectDrawCreate)GetProcAddress(mhmodDD, "DirectDrawCreate");

		if (!pDirectDrawCreate)
			break;

		IDirectDraw *pdd;
		HRESULT hr;

		// create DirectDraw object
		if (FAILED(pDirectDrawCreate(NULL, &pdd, NULL))) {
			DEBUG_LOG("VideoDriver/DDraw: Couldn't create DirectDraw2 object\n");
			break;
		}

		// query up to IDirectDraw2 (DirectX 3)
		hr = pdd->QueryInterface(IID_IDirectDraw2, (void **)&mpdd);
		pdd->Release();

		if (FAILED(hr))
			break;

		// get caps
		memset(&mCaps, 0, sizeof mCaps);
		mCaps.dwSize = sizeof(DDCAPS);
		hr = mpdd->GetCaps(&mCaps, NULL);
		if (FAILED(hr)) {
			DEBUG_LOG("VideoDriver/DDraw: Couldn't get caps\n");
			break;
		}

		// set cooperative level
//		hr = mpdd->SetCooperativeLevel(NULL, DDSCL_FPUPRESERVE | DDSCL_NORMAL | DDSCL_NOWINDOWCHANGES);
		hr = mpdd->SetCooperativeLevel(NULL, DDSCL_NORMAL);
		if (FAILED(hr)) {
			DEBUG_LOG("VideoDriver/DDraw: Couldn't set cooperative level\n");
			break;
		}

		// attempt to create primary surface
		if (!InitPrimary())
			break;

		mInitCount = 1;
		mClients.push_back(pClient);
		return true;
	} while(false);

	Shutdown(NULL);
	return false;
}

bool VDDirectDrawManager::InitPrimary() {
	do {
		// attempt to create primary surface
		DDSURFACEDESC ddsdPri = {sizeof(DDSURFACEDESC)};
		IDirectDrawSurface *pdds;

		ddsdPri.dwFlags				= DDSD_CAPS;
		ddsdPri.ddsCaps.dwCaps		= DDSCAPS_PRIMARYSURFACE;
		ddsdPri.ddpfPixelFormat.dwSize	= sizeof(DDPIXELFORMAT);

		if (FAILED(mpdd->CreateSurface(&ddsdPri, &pdds, NULL))) {
			DEBUG_LOG("VideoDriver/DDraw: Couldn't create primary surface\n");
			break;
		}

		// query up to IDirectDrawSurface2 (DX3)
		HRESULT hr = pdds->QueryInterface(IID_IDirectDrawSurface2, (void **)&mpddsPrimary);
		pdds->Release();

		if (FAILED(hr))
			break;

		// get surface desc
		if (FAILED(mpddsPrimary->GetSurfaceDesc(&ddsdPri))) {
			DEBUG_LOG("VideoDriver/DDraw: Couldn't get primary desc\n");
			break;
		}

		mPrimaryDesc = ddsdPri;

		return true;
	} while(false);

	ShutdownPrimary();
	return false;
}

bool VDDirectDrawManager::Restore() {
	if (mpddsPrimary) {
		if (!mpddsPrimary->IsLost())
			return true;

		VDDEBUG("VDDirectDraw: Primary surface restore requested.\n");

		HRESULT hr = mpddsPrimary->Restore();

		if (FAILED(hr)) {
			VDDEBUG("VDDirectDraw: Primary surface restore failed -- tearing down DirectDraw!\n");

			for(tClients::iterator it(mClients.begin()), itEnd(mClients.end()); it!=itEnd; ++it) {
				IVDDirectDrawClient *pClient = *it;

				pClient->DirectDrawShutdown();
			}

			if (!mInitCount) {
				VDDEBUG("VDDirectDraw: All clients vacated.\n");
				return false;
			}

			Shutdown(NULL);
			if (!Init(NULL)) {
				VDDEBUG("VDDirectDraw: Couldn't resurrect DirectDraw!\n");
				return false;
			}
		}
	} else {
		if (!InitPrimary())
			return false;
	}

	VDDEBUG("VDDirectDraw: Primary surface restore complete.\n");
	for(tClients::iterator it(mClients.begin()), itEnd(mClients.end()); it!=itEnd; ++it) {
		IVDDirectDrawClient *pClient = *it;

		pClient->DirectDrawPrimaryRestored();
	}

	return true;
}

void VDDirectDrawManager::ShutdownPrimary() {
	if (mpddsPrimary) {
		mpddsPrimary->Release();
		mpddsPrimary = 0;
	}
}

void VDDirectDrawManager::Shutdown(IVDDirectDrawClient *pClient) {
	if (pClient) {
		tClients::iterator it(std::find(mClients.begin(), mClients.end(), pClient));

		if (it != mClients.end()) {
			*it = mClients.back();
			mClients.pop_back();
		}

		if (--mInitCount)
			return;
	}

	ShutdownPrimary();

	if (mpdd) {
		mpdd->Release();
		mpdd = 0;
	}

	if (mhmodDD) {
		FreeLibrary(mhmodDD);
		mhmodDD = 0;
	}	
}

VDDirectDrawManager g_ddman;

IVDDirectDrawManager *VDInitDirectDraw(IVDDirectDrawClient *pClient) {
	VDASSERT(pClient);
	return g_ddman.Init(pClient) ? &g_ddman : NULL;
}

void VDShutdownDirectDraw(IVDDirectDrawClient *pClient) {
	VDASSERT(pClient);
	g_ddman.Shutdown(pClient);
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDisplayMinidriverDirectDraw : public IVDVideoDisplayMinidriver, protected IVDDirectDrawClient {
public:
	VDVideoDisplayMinidriverDirectDraw();
	~VDVideoDisplayMinidriverDirectDraw();

	bool Init(HWND hwnd, const VDVideoDisplaySourceInfo& info);
	void Shutdown();

	bool ModifySource(const VDVideoDisplaySourceInfo& info);

	bool IsValid();

	bool Tick(int id);
	bool Resize();
	bool Update(FieldMode);
	void Refresh(FieldMode);
	bool Paint(HDC hdc, const RECT& rClient);
	void SetLogicalPalette(const uint8 *pLogicalPalette) { mpLogicalPalette = pLogicalPalette; }

protected:
	enum {
		kOverlayUpdateTimerId = 100
	};

	void DirectDrawShutdown() {
		Shutdown();
		mbReset = true;
	}

	void DirectDrawPrimaryRestored() {
		memset(&mLastDisplayRect, 0, sizeof mLastDisplayRect);
	}

	bool InitOverlay();
	bool InitOffscreen();
	void ShutdownDisplay();
	void InternalRefresh(const RECT& rClient, FieldMode mode);
	bool InternalBlt(IDirectDrawSurface2 *&pDest, RECT *prDst, RECT *prSrc);

	HWND		mhwnd;
	IVDDirectDrawManager	*mpddman;
	IDirectDrawClipper	*mpddc;
	IDirectDrawSurface2	*mpddsBitmap;
	IDirectDrawSurface2	*mpddsOverlay;
	int			mPrimaryFormat;
	int			mPrimaryW;
	int			mPrimaryH;
	const uint8 *mpLogicalPalette;

	RECT		mLastDisplayRect;
	UINT		mOverlayUpdateTimer;

	COLORREF	mChromaKey;
	unsigned	mRawChromaKey;

	bool		mbReset;
	bool		mbValid;
	bool		mbRepaintOnNextUpdate;

	DDCAPS		mCaps;
	VDVideoDisplaySourceInfo	mSource;
};

IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverDirectDraw() {
	return new VDVideoDisplayMinidriverDirectDraw;
}

VDVideoDisplayMinidriverDirectDraw::VDVideoDisplayMinidriverDirectDraw()
	: mhwnd(0)
	, mpddman(0)
	, mpddc(0)
	, mpddsBitmap(0)
	, mpddsOverlay(0)
	, mOverlayUpdateTimer(0)
	, mbReset(false)
	, mbValid(false)
	, mbRepaintOnNextUpdate(false)
{
}

VDVideoDisplayMinidriverDirectDraw::~VDVideoDisplayMinidriverDirectDraw() {
}

bool VDVideoDisplayMinidriverDirectDraw::Init(HWND hwnd, const VDVideoDisplaySourceInfo& info) {
	mhwnd	= hwnd;
	mSource	= info;

	do {
		mpddman = VDInitDirectDraw(this);
		if (!mpddman)
			break;

		mCaps = mpddman->GetCaps();

		const DDSURFACEDESC& ddsdPri = mpddman->GetPrimaryDesc();

		mPrimaryW = ddsdPri.dwWidth;
		mPrimaryH = ddsdPri.dwHeight;

		if (InitOverlay() || InitOffscreen())
			return true;

	} while(false);

	Shutdown();
	return false;
}

bool VDVideoDisplayMinidriverDirectDraw::InitOverlay() {
	DWORD dwFourCC;

	switch(mSource.format) {
	case IVDVideoDisplay::kFormatYUV422_YUYV:
		dwFourCC = MAKEFOURCC('Y', 'U', 'Y', '2');
		break;

	case IVDVideoDisplay::kFormatYUV422_UYVY:
		dwFourCC = MAKEFOURCC('U', 'Y', 'V', 'Y');
		break;

	default:
		return false;
	}

	do {
		// create overlay surface
		DDSURFACEDESC ddsdOff = {sizeof(DDSURFACEDESC)};

		ddsdOff.dwFlags						= DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
		ddsdOff.dwWidth						= mSource.w;
		ddsdOff.dwHeight					= mSource.h;
		ddsdOff.ddsCaps.dwCaps				= DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
		ddsdOff.ddpfPixelFormat.dwSize		= sizeof(DDPIXELFORMAT);
		ddsdOff.ddpfPixelFormat.dwFlags		= DDPF_FOURCC;
		ddsdOff.ddpfPixelFormat.dwFourCC	= dwFourCC;

		if (mCaps.dwCaps & DDCAPS_ALIGNSIZESRC) {
			ddsdOff.dwWidth += mCaps.dwAlignSizeSrc - 1;
			ddsdOff.dwWidth -= ddsdOff.dwWidth % mCaps.dwAlignSizeSrc;
		}

		IDirectDrawSurface *pdds;

		if (FAILED(mpddman->GetDDraw()->CreateSurface(&ddsdOff, &pdds, NULL))) {
			DEBUG_LOG("VideoDisplay/DDraw: Overlay surface creation failed\n");
			break;
		}

		HRESULT hr = pdds->QueryInterface(IID_IDirectDrawSurface2, (void **)&mpddsOverlay);
		pdds->Release();

		if (FAILED(hr))
			break;

		// Do not allow colorkey if the primary surface is paletted, as we may not be able
		// to reliably choose the correct color.
		mChromaKey = 0;

		if (!(mpddman->GetPrimaryDesc().ddpfPixelFormat.dwFlags & (DDPF_PALETTEINDEXED8|DDPF_PALETTEINDEXED4))) {
			if (mCaps.dwCKeyCaps & DDCKEYCAPS_DESTOVERLAY) {
				const DDSURFACEDESC& ddsdPri = mpddman->GetPrimaryDesc();

				mRawChromaKey = ddsdPri.ddpfPixelFormat.dwGBitMask & ~(ddsdPri.ddpfPixelFormat.dwGBitMask >> 1);
				mChromaKey = RGB(0,128,0);
			}
		}

		mOverlayUpdateTimer = SetTimer(mhwnd, kOverlayUpdateTimerId, 100, NULL);
		memset(&mLastDisplayRect, 0, sizeof mLastDisplayRect);

		VDDEBUG("VideoDisplay: Using DirectDraw overlay for %dx%d display.\n", mSource.w, mSource.h);
		DEBUG_LOG("VideoDisplay/DDraw: Overlay surface creation successful\n");

		mbRepaintOnNextUpdate = true;
		mbValid = false;
		return true;
	} while(false);

	ShutdownDisplay();
	return false;
}

bool VDVideoDisplayMinidriverDirectDraw::InitOffscreen() {
	HRESULT hr;

	do {
		const DDPIXELFORMAT& pf = mpddman->GetPrimaryDesc().ddpfPixelFormat;

		// determine primary surface pixel format
		if (pf.dwFlags & DDPF_PALETTEINDEXED8) {
			mPrimaryFormat = IVDVideoDisplay::kFormatPal8;
			VDDEBUG("VideoDisplay/DirectDraw: Display is 8-bit paletted.\n");
		} else if (pf.dwFlags & DDPF_RGB) {
			if (   pf.dwRGBBitCount == 16 && pf.dwRBitMask == 0x7c00 && pf.dwGBitMask == 0x03e0 && pf.dwBBitMask == 0x001f) {
				mPrimaryFormat = IVDVideoDisplay::kFormatRGB1555;
				VDDEBUG("VideoDisplay/DirectDraw: Display is 16-bit xRGB (1-5-5-5).\n");
			} else if (pf.dwRGBBitCount == 16 && pf.dwRBitMask == 0xf800 && pf.dwGBitMask == 0x07e0 && pf.dwBBitMask == 0x001f) {
				mPrimaryFormat = IVDVideoDisplay::kFormatRGB565;
				VDDEBUG("VideoDisplay/DirectDraw: Display is 16-bit RGB (5-6-5).\n");
			} else if (pf.dwRGBBitCount == 24 && pf.dwRBitMask == 0xff0000 && pf.dwGBitMask == 0x00ff00 && pf.dwBBitMask == 0x0000ff) {
				mPrimaryFormat = IVDVideoDisplay::kFormatRGB888;
				VDDEBUG("VideoDisplay/DirectDraw: Display is 24-bit RGB (8-8-8).\n");
			} else if (pf.dwRGBBitCount == 32 && pf.dwRBitMask == 0xff0000 && pf.dwGBitMask == 0x00ff00 && pf.dwBBitMask == 0x0000ff) {
				mPrimaryFormat = IVDVideoDisplay::kFormatRGB8888;
				VDDEBUG("VideoDisplay/DirectDraw: Display is 32-bit xRGB (8-8-8-8).\n");
			} else
				break;
		} else
			break;

		if (mPrimaryFormat != mSource.format && !mSource.bAllowConversion) {
			VDDEBUG("VideoDisplay/DirectDraw: Display is not compatible with source and conversion is disallowed.\n");
			return false;
		}

		// attempt to create clipper
		if (FAILED(mpddman->GetDDraw()->CreateClipper(0, &mpddc, 0)))
			break;

		if (FAILED(mpddc->SetHWnd(0, mhwnd)))
			break;

		// create bitmap surface
		DDSURFACEDESC ddsdOff = {sizeof(DDSURFACEDESC)};

		ddsdOff.dwFlags					= DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
		ddsdOff.dwWidth					= mSource.w;
		ddsdOff.dwHeight				= mSource.h;
		ddsdOff.ddsCaps.dwCaps			= DDSCAPS_OFFSCREENPLAIN;
		ddsdOff.ddpfPixelFormat			= pf;

		IDirectDrawSurface *pdds;

		if (FAILED(mpddman->GetDDraw()->CreateSurface(&ddsdOff, &pdds, NULL))) {
			DEBUG_LOG("VideoDriver/DDraw: Couldn't create offscreen surface\n");
			break;
		}

		hr = pdds->QueryInterface(IID_IDirectDrawSurface2, (void **)&mpddsBitmap);
		pdds->Release();

		if (FAILED(hr))
			break;

		mChromaKey = 0;
		mbValid = false;
		mbRepaintOnNextUpdate = false;

		DEBUG_LOG("VideoDriver/DDraw: Offscreen initialization successful\n");
		VDDEBUG("VideoDisplay: Using DirectDraw offscreen surface for %dx%d display.\n", mSource.w, mSource.h);
		return true;
	} while(false); 

	ShutdownDisplay();
	return false;
}

void VDVideoDisplayMinidriverDirectDraw::ShutdownDisplay() {
	if (mpddc) {
		mpddc->Release();
		mpddc = 0;
	}

	if (mpddsBitmap) {
		mpddsBitmap->Release();
		mpddsBitmap = 0;
	}

	if (mpddsOverlay) {
		mpddsOverlay->Release();
		mpddsOverlay = 0;
	}

	mbValid = false;
}

void VDVideoDisplayMinidriverDirectDraw::Shutdown() {
	ShutdownDisplay();
	
	if (mpddman) {
		VDShutdownDirectDraw(this);
		mpddman = 0;
	}
}

bool VDVideoDisplayMinidriverDirectDraw::ModifySource(const VDVideoDisplaySourceInfo& info) {
	if (!mpddsBitmap && !mpddsOverlay)
		return false;

	if (mSource.w == info.w && mSource.h == info.h && mSource.format == info.format) {
		mSource = info;
		return true;
	}

	return false;
}

bool VDVideoDisplayMinidriverDirectDraw::IsValid() {
	return mbValid && ((mpddsOverlay && DD_OK == mpddsOverlay->IsLost()) || (mpddsBitmap && DD_OK == mpddsBitmap->IsLost()));
}

bool VDVideoDisplayMinidriverDirectDraw::Tick(int id) {
	if (id == kOverlayUpdateTimerId) {
		RECT r;
		GetClientRect(mhwnd, &r);
		MapWindowPoints(mhwnd, NULL, (LPPOINT)&r, 2);

		if (memcmp(&r, &mLastDisplayRect, sizeof(RECT)))
			Resize();
	}

	return !mbReset;
}

bool VDVideoDisplayMinidriverDirectDraw::Resize() {
	if (mpddsOverlay) {
		do {
			RECT rDst0;

			GetClientRect(mhwnd, &rDst0);
			MapWindowPoints(mhwnd, NULL, (LPPOINT)&rDst0, 2);

			// destination clipping
			RECT rDst = rDst0;
			const int dstw = rDst.right - rDst.left;
			const int dsth = rDst.bottom - rDst.top;

			if (rDst.left < 0)
				rDst.left = 0;

			if (rDst.top < 0)
				rDst.top = 0;

			if (rDst.right > mPrimaryW)
				rDst.right = mPrimaryW;

			if (rDst.bottom > mPrimaryH)
				rDst.bottom = mPrimaryH;

			if (rDst.bottom <= rDst.top || rDst.right <= rDst.left)
				break;

			// source clipping
			RECT rSrc = {
				(rDst.left   - rDst0.left) * mSource.w / dstw,
				(rDst.top    - rDst0.top ) * mSource.h / dsth,
				(rDst.right  - rDst0.left) * mSource.w / dstw,
				(rDst.bottom - rDst0.top ) * mSource.h / dsth,
			};

			// source alignment
			if (mCaps.dwCaps & DDCAPS_ALIGNBOUNDARYSRC) {
				int align = mCaps.dwAlignBoundarySrc;
				rSrc.left -= rSrc.left % align;
			}

			if (mCaps.dwCaps & DDCAPS_ALIGNSIZESRC) {
				int w = rSrc.right - rSrc.left;

				w -= w % mCaps.dwAlignSizeSrc;

				rSrc.right = rSrc.left + w;
			}

			// destination alignment
			if (mCaps.dwCaps & DDCAPS_ALIGNBOUNDARYDEST) {
				int align = mCaps.dwAlignBoundaryDest;

				rDst.left += align-1;
				rDst.left -= rDst.left % align;
			}

			if (mCaps.dwCaps & DDCAPS_ALIGNSIZEDEST) {
				int w = rDst.right - rDst.left;

				w -= w % mCaps.dwAlignSizeDest;

				if (w <= 0)
					break;

				rDst.right = rDst.left + w;
			}

			DWORD dwFlags = DDOVER_SHOW | DDOVER_DDFX;
			DDOVERLAYFX ddfx = {sizeof(DDOVERLAYFX)};

			if (mChromaKey) {
				dwFlags |= DDOVER_KEYDESTOVERRIDE;
				ddfx.dckDestColorkey.dwColorSpaceLowValue = mRawChromaKey;
				ddfx.dckDestColorkey.dwColorSpaceHighValue = mRawChromaKey;
			}

			if (mCaps.dwFXCaps & DDFXCAPS_OVERLAYARITHSTRETCHY)
				ddfx.dwFlags |= DDOVERFX_ARITHSTRETCHY;


			IDirectDrawSurface2 *pDest = mpddman->GetPrimary();
			HRESULT hr = mpddsOverlay->UpdateOverlay(&rSrc, pDest, &rDst, dwFlags, &ddfx);

			if (FAILED(hr)) {
				if (hr == DDERR_SURFACELOST) {
					if (FAILED(mpddsOverlay->Restore()))
						return false;

					if (pDest->IsLost() && FAILED(mpddman->Restore()))
						return false;

					memset(&mLastDisplayRect, 0, sizeof mLastDisplayRect);
				}
			} else
				mLastDisplayRect = rDst0;
			return !mbReset;
		} while(false);

		mpddsOverlay->UpdateOverlay(NULL, mpddman->GetPrimary(), NULL, DDOVER_HIDE, NULL);
	}

	return !mbReset;
}

bool VDVideoDisplayMinidriverDirectDraw::Update(FieldMode fieldmode) {
	HRESULT hr;
	DDSURFACEDESC ddsd = { sizeof(DDSURFACEDESC) };

	ddsd.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	
	static DWORD dwLockFlags = GetVersion() & 0x80000000 ? DDLOCK_WRITEONLY | DDLOCK_NOSYSLOCK | DDLOCK_WAIT : DDLOCK_WRITEONLY | DDLOCK_WAIT;

	IDirectDrawSurface2 *pTarget = mpddsBitmap ? mpddsBitmap : mpddsOverlay;

	if (!pTarget)
		return false;

	while(FAILED(hr = pTarget->Lock(NULL, &ddsd, dwLockFlags, 0))) {
		if (hr == DDERR_SURFACELOST) {
			if (!mpddman->Restore())
				break;

			hr = pTarget->Restore();
			if (FAILED(hr))
				break;

			memset(&mLastDisplayRect, 0, sizeof mLastDisplayRect);
		}
	}

	if (FAILED(hr))
		return false;

	VDVideoDisplaySourceInfo source(mSource);

	char *dst = (char *)ddsd.lpSurface;
	ptrdiff_t dstpitch = ddsd.lPitch;

	if (source.bInterlaced && fieldmode != kAllFields) {
		if (fieldmode == kOddFieldsOnly) {
			source.data = (const char *)source.data + source.stride;
			source.h >>= 1;
			dst += dstpitch;
		} else {
			source.h = (source.h + 1) >> 1;
		}

		source.stride += source.stride;
		dstpitch += dstpitch;
	}

	if (mpddsBitmap && source.format != mPrimaryFormat) {
		int mSrcDepth = 16;
		int mDstDepth = 16;

		switch(source.format) {
		case IVDVideoDisplay::kFormatRGB1555:
			mSrcDepth = 16;
			break;
		case IVDVideoDisplay::kFormatRGB888:
			mSrcDepth = 24;
			break;
		case IVDVideoDisplay::kFormatRGB8888:
			mSrcDepth = 32;
			break;
		}

		switch(mPrimaryFormat) {
		case IVDVideoDisplay::kFormatPal8:
			mDstDepth = 8;
			break;
		case IVDVideoDisplay::kFormatRGB1555:
			mDstDepth = 16;
			break;
		case IVDVideoDisplay::kFormatRGB888:
			mDstDepth = 24;
			break;
		case IVDVideoDisplay::kFormatRGB8888:
			mDstDepth = 32;
			break;
		}

		VBitmap srcbm, dstbm;

		srcbm.data		= (Pixel32 *)source.data;
		srcbm.w			= source.w;
		srcbm.h			= source.h;
		srcbm.pitch		= source.stride;
		srcbm.depth		= mSrcDepth;

		dstbm.data		= (Pixel32 *)dst;
		dstbm.pitch		= dstpitch;
		dstbm.w			= source.w;
		dstbm.h			= source.h;
		dstbm.depth		= mDstDepth;

		if (mPrimaryFormat == IVDVideoDisplay::kFormatPal8)
			VDDitherImage(dstbm, srcbm, mpLogicalPalette);
		else if (mPrimaryFormat == IVDVideoDisplay::kFormatRGB565)
			dstbm.BitBlt565(0, 0, &srcbm, 0, 0, dstbm.w, dstbm.h);
		else
			dstbm.BitBlt(0, 0, &srcbm, 0, 0, dstbm.w, dstbm.h);
	} else {
		VDMemcpyRect(dst, dstpitch, source.data, source.stride, source.bpr, source.h);
	}
	pTarget->Unlock(0);

	mbValid = true;

	return !mbReset;
}

void VDVideoDisplayMinidriverDirectDraw::Refresh(FieldMode mode) {
	if (mbValid) {
		if (mpddsOverlay) {
			Tick(kOverlayUpdateTimerId);
			if (mbRepaintOnNextUpdate) {
				InvalidateRect(mhwnd, NULL, TRUE);
				mbRepaintOnNextUpdate = false;
			}
		} else {
			RECT r;
			GetClientRect(mhwnd, &r);
			InternalRefresh(r, mode);
		}
	}
}

bool VDVideoDisplayMinidriverDirectDraw::Paint(HDC hdc, const RECT& rClient) {
	if (mpddsOverlay) {
		if (mChromaKey) {
			SetBkColor(hdc, mChromaKey);
			ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &rClient, "", 0, NULL);
		}
	} else {
		InternalRefresh(rClient, kAllFields);
	}

	return !mbReset;
}

void VDVideoDisplayMinidriverDirectDraw::InternalRefresh(const RECT& rClient, FieldMode mode) {
	RECT rDst = rClient;

	MapWindowPoints(mhwnd, NULL, (LPPOINT)&rDst, 2);

	IDirectDrawSurface2 *pDest = mpddman->GetPrimary();

	pDest->SetClipper(mpddc);

	if (!mSource.bInterlaced)
		InternalBlt(pDest, &rDst, NULL);
	else {
		uint32 vinc		= (mSource.h << 16) / rClient.bottom;
		uint32 vaccum	= vinc >> 1;
		uint32 vtlimit	= (((mSource.h + 1) >> 1) << 17) - 1;
		int fieldbase	= (mode == kOddFieldsOnly ? 1 : 0);
		int ystep		= (mode == kAllFields) ? 1 : 2;

		vaccum += vinc*fieldbase;
		vinc *= ystep;

		for(int y = fieldbase; y < rClient.bottom; y += ystep) {
			int v;

			if (y & 1) {
				uint32 vt = vaccum < 0x8000 ? 0 : vaccum - 0x8000;

				v = (y&1) + ((vt>>16) & ~1);
			} else {
				uint32 vt = vaccum + 0x8000;

				if (vt > vtlimit)
					vt = vtlimit;

				v = (vt>>16) & ~1;
			}

			RECT rDstTemp = { rDst.left, rDst.top+y, rDst.right, rDst.top+y+1 };
			RECT rSrcTemp = { 0, v, mSource.w, v+1 };

			if (!InternalBlt(pDest, &rDstTemp, &rSrcTemp))
				break;

			vaccum += vinc;
		}
	}
	
	if (pDest)
		pDest->SetClipper(NULL);
}

bool VDVideoDisplayMinidriverDirectDraw::InternalBlt(IDirectDrawSurface2 *&pDest, RECT *prDst, RECT *prSrc) {
	HRESULT hr;

	while(FAILED(hr = pDest->Blt(prDst, mpddsBitmap, prSrc, DDBLT_ASYNC | DDBLT_WAIT, NULL))) {
		if (hr != DDERR_SURFACELOST)
			break;

		if (mpddsBitmap->IsLost()) {
			mpddsBitmap->Restore();
			break;
		}

		if (pDest->IsLost()) {
			pDest->SetClipper(NULL);
			pDest = NULL;

			if (!mpddman->Restore())
				return false;

			if (mbReset)
				return false;

			pDest = mpddman->GetPrimary();
			pDest->SetClipper(mpddc);
		}
	}

	return SUCCEEDED(hr);
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDisplayMinidriverGDI : public IVDVideoDisplayMinidriver {
public:
	VDVideoDisplayMinidriverGDI();
	~VDVideoDisplayMinidriverGDI();

	bool Init(HWND hwnd, const VDVideoDisplaySourceInfo& info);
	void Shutdown();

	bool ModifySource(const VDVideoDisplaySourceInfo& info);

	bool IsValid() { return mbValid; }

	bool Tick(int id) { return true; }
	bool Resize() { return true; }
	bool Update(FieldMode);
	void Refresh(FieldMode);
	bool Paint(HDC hdc, const RECT& rClient);
	void SetLogicalPalette(const uint8 *pLogicalPalette) { mpLogicalPalette = pLogicalPalette; }

protected:
	HWND		mhwnd;
	HDC			mhdc;
	HBITMAP		mhbm;
	HGDIOBJ		mhbmOld;
	void *		mpBitmapBits;
	ptrdiff_t	mPitch;
	HPALETTE	mpal;
	const uint8 *mpLogicalPalette;
	bool		mbPaletted;
	bool		mbValid;

	uint8		mIdentTab[256];

	VDVideoDisplaySourceInfo	mSource;

	void InternalRefresh(HDC hdc, const RECT& rClient, FieldMode mode);
};

IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverGDI() {
	return new VDVideoDisplayMinidriverGDI;
}

VDVideoDisplayMinidriverGDI::VDVideoDisplayMinidriverGDI()
	: mhwnd(0)
	, mhdc(0)
	, mhbm(0)
	, mpal(0)
	, mbValid(false)
{
}

VDVideoDisplayMinidriverGDI::~VDVideoDisplayMinidriverGDI() {
}

bool VDVideoDisplayMinidriverGDI::Init(HWND hwnd, const VDVideoDisplaySourceInfo& info) {
	if (!info.bAllowConversion) {
		switch(info.format) {
		case IVDVideoDisplay::kFormatYUV422_YUYV:
		case IVDVideoDisplay::kFormatYUV422_UYVY:
			return false;
		}
	}
	
	mhwnd	= hwnd;
	mSource	= info;

	if (HDC hdc = GetDC(mhwnd)) {
		mhdc = CreateCompatibleDC(hdc);

		if (mhdc) {
			bool bPaletted = 0 != (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE);

			mbPaletted = bPaletted;

			if (bPaletted) {
				struct {
					BITMAPINFOHEADER hdr;
					RGBQUAD pal[256];
				} bih;

				bih.hdr.biSize			= sizeof(BITMAPINFOHEADER);
				bih.hdr.biWidth			= mSource.w;
				bih.hdr.biHeight		= mSource.h;
				bih.hdr.biPlanes		= 1;
				bih.hdr.biCompression	= BI_RGB;
				bih.hdr.biBitCount		= 8;

				mPitch = ((mSource.w + 3) & ~3);
				bih.hdr.biSizeImage		= mPitch * mSource.h;
				bih.hdr.biClrUsed		= 216;
				bih.hdr.biClrImportant	= 216;

				for(int i=0; i<216; ++i) {
					bih.pal[i].rgbRed	= (i / 36) * 51;
					bih.pal[i].rgbGreen	= ((i%36) / 6) * 51;
					bih.pal[i].rgbBlue	= (i%6) * 51;
					bih.pal[i].rgbReserved = 0;
				}

				for(int j=0; j<256; ++j)
					mIdentTab[j] = j;

				mhbm = CreateDIBSection(hdc, (const BITMAPINFO *)&bih, DIB_RGB_COLORS, &mpBitmapBits, mSource.pSharedObject, mSource.sharedOffset);
			} else {
				BITMAPV4HEADER bih = {0};

				bih.bV4Size				= sizeof(BITMAPINFOHEADER);
				bih.bV4Width			= mSource.w;
				bih.bV4Height			= mSource.h;
				bih.bV4Planes			= 1;
				bih.bV4V4Compression	= BI_RGB;
				bih.bV4BitCount			= mSource.bpp << 3;

				switch(mSource.format) {
				case IVDVideoDisplay::kFormatRGB1555:
				case IVDVideoDisplay::kFormatRGB888:
				case IVDVideoDisplay::kFormatRGB8888:
					break;
				case IVDVideoDisplay::kFormatYUV422_YUYV:
				case IVDVideoDisplay::kFormatYUV422_UYVY:
				case IVDVideoDisplay::kFormatRGB565:
					bih.bV4V4Compression	= BI_BITFIELDS;
					bih.bV4RedMask			= 0xf800;
					bih.bV4GreenMask		= 0x07e0;
					bih.bV4BlueMask			= 0x001f;
					break;
				default:
					return false;
				}

				mPitch = ((mSource.w * mSource.bpp + 3) & ~3);
				bih.bV4SizeImage		= mPitch * mSource.h;
				mhbm = CreateDIBSection(hdc, (const BITMAPINFO *)&bih, DIB_RGB_COLORS, &mpBitmapBits, mSource.pSharedObject, mSource.sharedOffset);
			}

			if (mhbm) {
				mhbmOld = SelectObject(mhdc, mhbm);

				if (mhbmOld) {
					ReleaseDC(mhwnd, hdc);
					VDDEBUG("VideoDisplay: Using GDI for %dx%d display.\n", mSource.w, mSource.h);
					mbValid = (mSource.pSharedObject != 0);
					return true;
				}

				if (mSource.pSharedObject && mSource.sharedOffset >= 65536)
					UnmapViewOfFile(mpBitmapBits);		// Workaround for GDI memory leak in NT4

				DeleteObject(mhbm);
				mhbm = 0;
			}
			DeleteDC(mhdc);
			mhdc = 0;
		}

		ReleaseDC(mhwnd, hdc);
	}

	Shutdown();
	return false;
}

void VDVideoDisplayMinidriverGDI::Shutdown() {
	if (mhbm) {
		SelectObject(mhdc, mhbmOld);
		DeleteObject(mhbm);
		if (mSource.pSharedObject && mSource.sharedOffset >= 65536)
			UnmapViewOfFile(mpBitmapBits);		// Workaround for GDI memory leak in NT4
		mhbm = 0;
	}

	if (mhdc) {
		DeleteDC(mhdc);
		mhdc = 0;
	}

	mbValid = false;
}

bool VDVideoDisplayMinidriverGDI::ModifySource(const VDVideoDisplaySourceInfo& info) {
	if (!mhdc)
		return false;

	if (!mSource.pSharedObject && mSource.w == info.w && mSource.h == info.h && mSource.format == info.format) {
		mSource = info;
		return true;
	}

	return false;
}

bool VDVideoDisplayMinidriverGDI::Update(FieldMode fieldmode) {
	if (!mSource.pSharedObject) {
		GdiFlush();

		VDVideoDisplaySourceInfo source(mSource);

		char *dst = (char *)mpBitmapBits + mPitch*(mSource.h - 1);
		ptrdiff_t dstpitch = -mPitch;

		if (source.bInterlaced && fieldmode != kAllFields) {
			if (fieldmode == kOddFieldsOnly) {
				source.data = (const char *)source.data + source.stride;
				source.h >>= 1;
				dst += dstpitch;
			} else {
				source.h = (source.h + 1) >> 1;
			}

			source.stride += source.stride;
			dstpitch += dstpitch;
		}

		if (mbPaletted) {
			int mSrcDepth;

			switch(source.format) {
			case IVDVideoDisplay::kFormatRGB1555:
				mSrcDepth = 16;
				break;
			case IVDVideoDisplay::kFormatRGB888:
				mSrcDepth = 24;
				break;
			case IVDVideoDisplay::kFormatRGB8888:
				mSrcDepth = 32;
				break;
			}

			VBitmap srcbm, dstbm;

			srcbm.data		= (Pixel32 *)source.data;
			srcbm.w			= source.w;
			srcbm.h			= source.h;
			srcbm.pitch		= source.stride;
			srcbm.depth		= mSrcDepth;

			dstbm.data		= (Pixel32 *)dst;
			dstbm.pitch		= dstpitch;
			dstbm.w			= source.w;
			dstbm.h			= source.h;
			dstbm.depth		= 8;

			VDDitherImage(dstbm, srcbm, mIdentTab);
		} else {
			switch(source.format) {
			case IVDVideoDisplay::kFormatYUV422_UYVY:
				VDYCbCrToRGB565Generic(dst, dstpitch, source.data, source.stride, source.data, source.data, source.stride, source.w, source.h, g_formatInfo_YUV422_UYVY);
				break;
			case IVDVideoDisplay::kFormatYUV422_YUYV:
				VDYCbCrToRGB565Generic(dst, dstpitch, source.data, source.stride, source.data, source.data, source.stride, source.w, source.h, g_formatInfo_YUV422_YUYV);
				break;
			default:
				VDMemcpyRect(dst, dstpitch, source.data, source.stride, source.bpr, source.h);
				break;
			}
		}

		mbValid = true;
	}

	return true;
}

void VDVideoDisplayMinidriverGDI::Refresh(FieldMode mode) {
	if (mbValid) {
		if (HDC hdc = GetDC(mhwnd)) {
			RECT r;

			GetClientRect(mhwnd, &r);
			InternalRefresh(hdc, r, mode);
			ReleaseDC(mhwnd, hdc);
		}
	}
}

bool VDVideoDisplayMinidriverGDI::Paint(HDC hdc, const RECT& rClient) {
	InternalRefresh(hdc, rClient, kAllFields);
	return true;
}

void VDVideoDisplayMinidriverGDI::InternalRefresh(HDC hdc, const RECT& rClient, FieldMode mode) {
	SetStretchBltMode(hdc, COLORONCOLOR);

	if (mSource.bInterlaced) {
		uint32 vinc		= (mSource.h << 16) / rClient.bottom;
		uint32 vaccum	= vinc >> 1;
		uint32 vtlimit	= (((mSource.h + 1) >> 1) << 17) - 1;
		int fieldbase	= (mode == kOddFieldsOnly ? 1 : 0);
		int ystep		= (mode == kAllFields) ? 1 : 2;

		vaccum += vinc*fieldbase;
		vinc *= ystep;

		for(int y = fieldbase; y < rClient.bottom; y += ystep) {
			int v;

			if (y & 1) {
				uint32 vt = vaccum < 0x8000 ? 0 : vaccum - 0x8000;

				v = (y&1) + ((vt>>16) & ~1);
			} else {
				uint32 vt = vaccum + 0x8000;

				if (vt > vtlimit)
					vt = vtlimit;

				v = (vt>>16) & ~1;
			}

			StretchBlt(hdc, 0, y, rClient.right, 1, mhdc, 0, v, mSource.w, 1, SRCCOPY);
			vaccum += vinc;
		}
	} else {
		StretchBlt(hdc, 0, 0, rClient.right, rClient.bottom, mhdc, 0, 0, mSource.w, mSource.h, SRCCOPY);
	}
}