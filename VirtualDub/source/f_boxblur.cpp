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

#include <windows.h>
#include <commctrl.h>

#include "ScriptInterpreter.h"
#include "ScriptError.h"
#include "ScriptValue.h"

#include "resource.h"
#include "filter.h"
#include <vd2/system/cpuaccel.h>

extern HINSTANCE g_hInst;
extern "C" unsigned char YUV_clip_table[];

//#define DO_UNSHARP_FILTER

///////////////////////////////////////////////////////////////////////////

int boxInitProc(FilterActivation *fa, const FilterFunctions *ff);
int boxRunProc(const FilterActivation *fa, const FilterFunctions *ff);
int boxStartProc(FilterActivation *fa, const FilterFunctions *ff);
int boxEndProc(FilterActivation *fa, const FilterFunctions *ff);
long boxParamProc(FilterActivation *fa, const FilterFunctions *ff);
int boxConfigProc(FilterActivation *fa, const FilterFunctions *ff, HWND hwnd);
void boxStringProc(const FilterActivation *fa, const FilterFunctions *ff, char *str);
void boxScriptConfig(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc);
bool boxFssProc(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen);

///////////////////////////////////////////////////////////////////////////

typedef unsigned short Pixel16;

typedef struct BoxFilterData {
	IFilterPreview *ifp;
	Pixel32 *rows;
	Pixel16 *trow;
	int 	filter_width;
	int 	filter_power;
} BoxFilterData;

ScriptFunctionDef box_func_defs[]={
	{ (ScriptFunctionPtr)boxScriptConfig, "Config", "0ii" },
	{ NULL },
};

CScriptObject box_obj={
	NULL, box_func_defs
};

struct FilterDefinition filterDef_box = {

	NULL, NULL, NULL,		// next, prev, module
	"box blur",					// name
	"Performs a fast box, triangle, or cubic blur.",
							// desc
	NULL,					// maker
	NULL,					// private_data
	sizeof(BoxFilterData),	// inst_data_size

	boxInitProc,			// initProc
	NULL,					// deinitProc
	boxRunProc, 			// runProc
	boxParamProc,			// paramProc
	boxConfigProc,			// configProc
	boxStringProc,			// stringProc
	boxStartProc,			// startProc
	boxEndProc, 			// endProc

	&box_obj,				// script_obj
	boxFssProc, 			// fssProc

};

///////////////////////////////////////////////////////////////////////////

int boxInitProc(FilterActivation *fa, const FilterFunctions *ff) {
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;

	mfd->filter_width = 1;
	mfd->filter_power = 1;

	return 0;
}

int boxStartProc(FilterActivation *fa, const FilterFunctions *ff) {
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;

	if (mfd->filter_power < 1)
		mfd->filter_power = 1;

	if (mfd->filter_width < 1)
		mfd->filter_width = 1;

	mfd->trow = NULL;

	if (!(mfd->rows = (Pixel32 *)allocmem(((fa->dst.w+1)&-2)*4*fa->dst.h)))
		return 1;

	if (!(mfd->trow = (Pixel16 *)allocmem(fa->dst.w*8)))
		return 1;

	return 0;
}

static void box_filter_row(Pixel32 *dst, Pixel32 *src, int filtwidth, int cnt, int mult) {
	int i;
	unsigned r, g, b;
	Pixel32 A, B;

	A = src[0];
	r = (int)(unsigned char)(A>>16) * filtwidth;
	g = (int)(unsigned char)(A>> 8) * filtwidth;
	b = (int)(unsigned char)(A	  ) * filtwidth;

	i = filtwidth + 1;
	do {
		B = *src++;

		r += (int)(unsigned char)(B>>16);
		g += (int)(unsigned char)(B>> 8);
		b += (int)(unsigned char)(B    );
	} while(--i);

	i = filtwidth;

	do {
		*dst++ = ((r*mult)&0xff0000) + (((g*mult)>>8)&0xff00) + ((b*mult)>>16);

		B = src[0];
		++src;

		r = r - (int)(unsigned char)(A>>16) + (int)(unsigned char)(B>>16);
		g = g - (int)(unsigned char)(A>> 8) + (int)(unsigned char)(B>> 8);
		b = b - (int)(unsigned char)(A	  ) + (int)(unsigned char)(B	);

	} while(--i);

	i = cnt - 2*filtwidth - 1;
	do {
		*dst++ = ((r*mult)&0xff0000) + (((g*mult)>>8)&0xff00) + ((b*mult)>>16);

		A = src[-(2*filtwidth+1)];
		B = src[0];
		++src;

		r = r - (int)(unsigned char)(A>>16) + (int)(unsigned char)(B>>16);
		g = g - (int)(unsigned char)(A>> 8) + (int)(unsigned char)(B>> 8);
		b = b - (int)(unsigned char)(A	  ) + (int)(unsigned char)(B	);
	} while(--i);

	i = filtwidth;
	do {
		*dst++ = ((r*mult)&0xff0000) + (((g*mult)>>8)&0xff00) + ((b*mult)>>16);

		A = src[-(2*filtwidth+1)];
		++src;

		r = r - (int)(unsigned char)(A>>16) + (int)(unsigned char)(B>>16);
		g = g - (int)(unsigned char)(A>> 8) + (int)(unsigned char)(B>> 8);
		b = b - (int)(unsigned char)(A	  ) + (int)(unsigned char)(B	);

	} while(--i);

	*dst = ((r*mult)&0xff0000) + (((g*mult)>>8)&0xff00) + ((b*mult)>>16);
}

static void __declspec(naked) box_filter_row_MMX(Pixel32 *dst, Pixel32 *src, int filtwidth, int cnt, int divisor) {
	__asm {
		push		ebx

		mov			ecx,[esp+12+4]			;ecx = filtwidth
		mov			eax,[esp+8+4]			;eax = src
		movd		mm6,ecx
		pxor		mm7,mm7
		movd		mm5,[eax]				;A = source pixel
		punpcklwd	mm6,mm6
		pcmpeqw		mm4,mm4					;mm4 = all -1's
		punpckldq	mm6,mm6
		psubw		mm6,mm4					;mm6 = filtwidth+1
		punpcklbw	mm5,mm7					;mm5 = src[0] (word)
		movq		mm0,mm5					;mm0 = A
		pmullw		mm5,mm6					;mm5 = src[0]*(filtwidth+1)
		add			eax,4					;next source pixel
xloop1:
		movd		mm1,[eax]				;B = next source pixel
		pxor		mm7,mm7
		punpcklbw	mm1,mm7
		add			eax,4
		paddw		mm5,mm1
		dec			ecx
		jne			xloop1

		mov			ecx,[esp+12+4]			;ecx = filtwidth
		movd		mm6,[esp+20+4]
		punpcklwd	mm6,mm6
		mov			edx,[esp+4+4]			;edx = dst
		punpckldq	mm6,mm6
xloop2:
		movd		mm1,[eax]				;B = next source pixel
		movq		mm2,mm5					;mm1 = accum

		pmulhw		mm2,mm6
		punpcklbw	mm1,mm7

		psubw		mm5,mm0					;accum -= A
		add			eax,4

		paddw		mm5,mm1					;accum += B
		add			edx,4

		packuswb	mm2,mm2
		dec			ecx

		movd		[edx-4],mm2
		jne			xloop2

		;main loop.
		
		mov			ebx,[esp+12+4]			;ebx = filtwidth
		mov			ecx,[esp+16+4]			;ecx = cnt
		lea			ebx,[ebx+ebx+1]			;ebx = 2*filtwidth+1
		sub			ecx,ebx					;ecx = cnt - (2*filtwidth+1)
		shl			ebx,2
		neg			ebx						;ebx = -4*(2*filtwidth+1)
xloop3:
		movd		mm0,[eax+ebx]			;mm0 = A = src[-(2*filtwidth+1)]
		movq		mm2,mm5

		movd		mm1,[eax]				;mm0 = B = src[0]
		pmulhw		mm2,mm6

		punpcklbw	mm0,mm7
		add			edx,4

		punpcklbw	mm1,mm7
		add			eax,4

		psubw		mm5,mm0					;accum -= A
		packuswb	mm2,mm2					;pack finished pixel

		paddw		mm5,mm1					;accum += B
		dec			ecx

		movd		[edx-4],mm2
		jne			xloop3

		;finish up remaining pixels

		mov			ecx,[esp+12+4]			;ecx = filtwidth
xloop4:
		movd		mm0,[eax+ebx]			;mm0 = A = src[-(2*filtwidth+1)]
		movq		mm2,mm5

		pmulhw		mm2,mm6
		add			edx,4

		punpcklbw	mm0,mm7
		add			eax,4

		psubw		mm5,mm0					;accum -= A
		packuswb	mm2,mm2					;pack finished pixel

		paddw		mm5,mm1					;accum += B
		dec			ecx

		movd		[edx-4],mm2
		jne			xloop4

		pmulhw		mm5,mm6
		packuswb	mm5,mm5
		movd		[edx],mm5

		pop			ebx
		ret
	}
}

///////////////////////////////////////////////////////////////////////////

static void box_filter_mult_row(Pixel16 *dst, Pixel32 *src, int cnt, int mult) {
	Pixel32 A;

	if (MMX_enabled)
		__asm {
			mov			eax,src
			movd		mm6,mult
			mov			edx,dst
			punpcklwd	mm6,mm6
			mov			ecx,cnt
			punpckldq	mm6,mm6
xloop:
			movd		mm0,[eax+ecx*4-4]
			pxor		mm7,mm7

			punpcklbw	mm0,mm7

			pmullw		mm0,mm6

			movq		[edx+ecx*8-8],mm0

			dec			ecx
			jne			xloop
		}
	else
		do {
			A = *src++;

			dst[0] = (int)(unsigned char)(A>>16) * mult;
			dst[1] = (int)(unsigned char)(A>> 8) * mult;
			dst[2] = (int)(unsigned char)(A    ) * mult;

			dst += 3;
		} while(--cnt);
}

static void box_filter_add_row(Pixel16 *dst, Pixel32 *src, int cnt) {
	Pixel32 A;

	if (MMX_enabled)
		__asm {
			mov			eax,src
			mov			edx,dst
			mov			ecx,cnt
xloop:
			movd		mm0,[eax+ecx*4-4]
			pxor		mm7,mm7

			punpcklbw	mm0,mm7

			paddw		mm0,[edx+ecx*8-8]

			movq		[edx+ecx*8-8],mm0

			dec			ecx
			jne			xloop
		}
	else
		do {
			A = *src++;

			dst[0] += (int)(unsigned char)(A>>16);
			dst[1] += (int)(unsigned char)(A>> 8);
			dst[2] += (int)(unsigned char)(A	);

			dst += 3;
		} while(--cnt);
}

static void box_filter_produce_row(Pixel32 *dst, Pixel16 *tmp, Pixel32 *src_add, Pixel32 *src_sub, int cnt, int filter_width) {
	Pixel32 A, B;
	Pixel16 r, g, b;
	int mult = 0x10000 / (2*filter_width+1);

	if (MMX_enabled)
		__asm {
			mov			eax,src_add
			movd		mm6,mult
			mov			edx,tmp
			punpcklwd	mm6,mm6
			mov			ecx,cnt
			punpckldq	mm6,mm6
			mov			ebx,src_sub
			mov			edi,dst
xloop:
			movq		mm2,[edx+ecx*8-8]
			pxor		mm7,mm7

			movd		mm0,[eax+ecx*4-4]
			movq		mm3,mm2

			movd		mm1,[ebx+ecx*4-4]
			pmulhw		mm2,mm6

			punpcklbw	mm0,mm7
			;

			punpcklbw	mm1,mm7
			paddw		mm0,mm3

			psubw		mm0,mm1
			packuswb	mm2,mm2

			movq		[edx+ecx*8-8],mm0

			movd		[edi+ecx*4-4],mm2

			dec			ecx
			jne			xloop
		}
	else
		do {
			A = *src_add++;
			B = *src_sub++;

			r = tmp[0];
			g = tmp[1];
			b = tmp[2];

			*dst++	= ((r*mult) & 0xff0000)
					+(((g*mult) & 0xff0000) >> 8)
					+ ((b*mult) >> 16);

			tmp[0] = r + (int)(unsigned char)(A>>16) - (int)(unsigned char)(B>>16);
			tmp[1] = g + (int)(unsigned char)(A>> 8) - (int)(unsigned char)(B>> 8);
			tmp[2] = b + (int)(unsigned char)(A    ) - (int)(unsigned char)(B	 );

			tmp += 3;
		} while(--cnt);
}

static void box_filter_produce_row2(Pixel32 *dst, Pixel16 *tmp, int cnt, int filter_width) {
	Pixel16 r, g, b;
	int mult = 0x10000 / (2*filter_width+1);

	if (MMX_enabled)
		__asm {
			movd		mm6,mult
			mov			eax,tmp
			punpcklwd	mm6,mm6
			mov			ecx,cnt
			punpckldq	mm6,mm6
			mov			edx,dst
xloop:
			movq		mm2,[eax+ecx*8-8]
			pxor		mm7,mm7

			pmulhw		mm2,mm6

			packuswb	mm2,mm2

			movd		[edx+ecx*4-4],mm2

			dec			ecx
			jne			xloop
		}
	else
		do {
			r = tmp[0];
			g = tmp[1];
			b = tmp[2];

			*dst++	= ((r*mult) & 0xff0000)
					+(((g*mult) & 0xff0000) >> 8)
					+ ((b*mult) >> 16);

			tmp += 3;
		} while(--cnt);
}

///////////////////////////////////////////////////////////////////////

#ifdef DO_UNSHARP_FILTER

static const __int64 x0080w = 0x0080008000800080i64;

static void box_unsharp_produce_row(Pixel32 *dst, Pixel16 *tmp, Pixel32 *src_add, Pixel32 *src_sub, int cnt, int filter_width, Pixel32 *orig) {
	Pixel32 A, B;
	Pixel16 r, g, b;
	int mult = 0x10000 / (2*filter_width+1);

	if (MMX_enabled)
		__asm {
			mov			eax,src_add
			movd		mm6,mult
			mov			edx,tmp
			punpcklwd	mm6,mm6
			mov			ecx,cnt
			punpckldq	mm6,mm6
			mov			ebx,src_sub
			mov			edi,dst
			mov			esi,orig
			movq		mm4,x0080w
xloop:
			movq		mm2,[edx+ecx*8-8]
			pxor		mm7,mm7

			movd		mm0,[eax+ecx*4-4]
			movq		mm3,mm2

			movd		mm1,[ebx+ecx*4-4]
			pmulhw		mm2,mm6

			movd		mm5,[esi+ecx*4-4]
			punpcklbw	mm0,mm7

			punpcklbw	mm5,mm7
			paddw		mm0,mm3

			punpcklbw	mm1,mm7
			paddw		mm5,mm4

			psubw		mm0,mm1
			psubw		mm5,mm2

			packuswb	mm5,mm5

			movq		[edx+ecx*8-8],mm0

			movd		[edi+ecx*4-4],mm5

			dec			ecx
			jne			xloop
		}
	else {
		unsigned char *dst2 = (unsigned char *)dst;

		do {
			A = *src_add++;
			B = *src_sub++;

			r = tmp[0];
			g = tmp[1];
			b = tmp[2];

			dst2[2] = YUV_clip_table[256 + 128 - ((r*mult)>>16) + ((orig[0]&0xff0000)>>16)];
			dst2[1] = YUV_clip_table[256 + 128 - ((g*mult)>>16) + ((orig[0]&0x00ff00)>> 8)];
			dst2[0] = YUV_clip_table[256 + 128 - ((b*mult)>>16) + ((orig[0]&0x0000ff)    )];

			tmp[0] = r + (int)(unsigned char)(A>>16) - (int)(unsigned char)(B>>16);
			tmp[1] = g + (int)(unsigned char)(A>> 8) - (int)(unsigned char)(B>> 8);
			tmp[2] = b + (int)(unsigned char)(A    ) - (int)(unsigned char)(B	 );

			tmp += 3;
			++orig;
			dst2 += 4;
		} while(--cnt);
	}
}

static void box_unsharp_produce_row2(Pixel32 *dst, Pixel16 *tmp, int cnt, int filter_width, Pixel32 *orig) {
	Pixel16 r, g, b;
	int mult = 0x10000 / (2*filter_width+1);

	if (MMX_enabled)
		__asm {
			movd		mm6,mult
			mov			eax,tmp
			punpcklwd	mm6,mm6
			mov			ecx,cnt
			punpckldq	mm6,mm6
			mov			edx,dst
			mov			ebx,orig
			movq		mm4,x0080w
xloop:
			movq		mm2,[eax+ecx*8-8]
			pxor		mm7,mm7

			movd		mm3,[ebx+ecx*4-4]
			pmulhw		mm2,mm6

			punpcklbw	mm3,mm7

			paddw		mm3,mm4

			psubw		mm3,mm2

			packuswb	mm3,mm3

			movd		[edx+ecx*4-4],mm3

			dec			ecx
			jne			xloop
		}
	else {
		unsigned char *dst2 = (unsigned char *)dst;

		do {
			r = tmp[0];
			g = tmp[1];
			b = tmp[2];

			dst2[2] = YUV_clip_table[256 + 128 - ((r*mult)>>16) + ((orig[0]&0xff0000)>>16)];
			dst2[1] = YUV_clip_table[256 + 128 - ((g*mult)>>16) + ((orig[0]&0x00ff00)>> 8)];
			dst2[0] = YUV_clip_table[256 + 128 - ((b*mult)>>16) + ((orig[0]&0x0000ff)    )];

			tmp += 3;
			dst2 += 4;
			++orig;
		} while(--cnt);
	}
}
#endif

///////////////////////////////////////////////////////////////////////

static void box_do_vertical_pass(Pixel32 *dst, PixOffset dstpitch, Pixel32 *src, PixOffset srcpitch, Pixel16 *trow, int w, int h, int filtwidth) {
	Pixel32 *srch = src;
	int j;

	box_filter_mult_row(trow, src, w, filtwidth + 1);

	src = (Pixel32 *)((char *)src + srcpitch);

	for(j=0; j<filtwidth; j++) {
		box_filter_add_row(trow, src, w);

		src = (Pixel32 *)((char *)src + srcpitch);
	}

	for(j=0; j<filtwidth; j++) {
		box_filter_produce_row(dst, trow, src, srch, w, filtwidth);

		src = (Pixel32 *)((char *)src + srcpitch);
		dst = (Pixel32 *)((char *)dst + dstpitch);
	}
	
	for(j=0; j<h - (2*filtwidth+1); j++) {
		box_filter_produce_row(dst, trow, src, (Pixel32 *)((char *)src - srcpitch*(2*filtwidth+1)), w, filtwidth);

		src = (Pixel32 *)((char *)src + srcpitch);
		dst = (Pixel32 *)((char *)dst + dstpitch);
	}

	srch = (Pixel32 *)((char *)src - srcpitch);

	for(j=0; j<filtwidth; j++) {
		box_filter_produce_row(dst, trow, srch, (Pixel32 *)((char *)src - srcpitch*(2*filtwidth+1)), w, filtwidth);

		src = (Pixel32 *)((char *)src + srcpitch);
		dst = (Pixel32 *)((char *)dst + dstpitch);
	}

	box_filter_produce_row2(dst, trow, w, filtwidth);
}

#ifdef DO_UNSHARP_FILTER
static void box_do_vertical_unsharp_pass(Pixel32 *dst, PixOffset dstpitch, Pixel32 *src, PixOffset srcpitch, Pixel32 *orig, PixOffset origpitch, Pixel16 *trow, int w, int h, int filtwidth) {
	Pixel32 *srch = src;
	int j;

	box_filter_mult_row(trow, src, w, filtwidth + 1);

	src = (Pixel32 *)((char *)src + srcpitch);

	for(j=0; j<filtwidth; j++) {
		box_filter_add_row(trow, src, w);

		src = (Pixel32 *)((char *)src + srcpitch);
	}

	for(j=0; j<filtwidth; j++) {
		box_unsharp_produce_row(dst, trow, src, srch, w, filtwidth, orig);

		src = (Pixel32 *)((char *)src + srcpitch);
		dst = (Pixel32 *)((char *)dst + dstpitch);
		orig= (Pixel32 *)((char *)orig+ origpitch);
	}
	
	for(j=0; j<h - (2*filtwidth+1); j++) {
		box_unsharp_produce_row(dst, trow, src, (Pixel32 *)((char *)src - srcpitch*(2*filtwidth+1)), w, filtwidth, orig);

		src = (Pixel32 *)((char *)src + srcpitch);
		dst = (Pixel32 *)((char *)dst + dstpitch);
		orig= (Pixel32 *)((char *)orig+ origpitch);
	}

	srch = (Pixel32 *)((char *)src - srcpitch);

	for(j=0; j<filtwidth; j++) {
		box_unsharp_produce_row(dst, trow, srch, (Pixel32 *)((char *)src - srcpitch*(2*filtwidth+1)), w, filtwidth, orig);

		src = (Pixel32 *)((char *)src + srcpitch);
		dst = (Pixel32 *)((char *)dst + dstpitch);
		orig= (Pixel32 *)((char *)orig+ origpitch);
	}

	box_unsharp_produce_row2(dst, trow, w, filtwidth, orig);
}
#endif

int boxRunProc(const FilterActivation *fa, const FilterFunctions *ff) {
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;
	PixDim h;
	Pixel32 *src, *dst;
	Pixel32 *tmp;
	PixOffset srcpitch, dstpitch;
	int i;

	src = fa->src.data;
	dst = fa->dst.data;
	tmp = mfd->rows;

	// Horizontal filtering.

	void (*pRowFilt)(Pixel32*, Pixel32*, int, int, int) = MMX_enabled ? box_filter_row_MMX : box_filter_row;
	int mult = 0x10000 / (2*mfd->filter_width+1);

	h = fa->src.h;
	do {
		switch(mfd->filter_power) {
		case 1:
			pRowFilt(tmp, src, mfd->filter_width, fa->dst.w, mult);
			break;
		case 2:
			pRowFilt(tmp, src, mfd->filter_width, fa->dst.w, mult);
			pRowFilt(dst, tmp, mfd->filter_width, fa->dst.w, mult);
			break;
		case 3:
			pRowFilt(tmp, src, mfd->filter_width, fa->dst.w, mult);
			pRowFilt(dst, tmp, mfd->filter_width, fa->dst.w, mult);
			pRowFilt(tmp, dst, mfd->filter_width, fa->dst.w, mult);
			break;
		}

		src = (Pixel32 *)((char *)src + fa->src.pitch);
		dst = (Pixel32 *)((char *)dst + fa->dst.pitch);
		tmp += (fa->dst.w+1)&-2;
	} while(--h);

	// Vertical filtering.

	if (mfd->filter_power & 1) {
		src = mfd->rows;
		dst = fa->dst.data;
		srcpitch = ((fa->dst.w+1)&-2)*4;
		dstpitch = fa->dst.pitch;
	} else {
		src = fa->dst.data;
		dst = mfd->rows;
		srcpitch = fa->dst.pitch;
		dstpitch = ((fa->dst.w+1)&-2)*4;
	}

#ifndef DO_UNSHARP_FILTER
	for(i=0; i<mfd->filter_power; i++) {
		if (i & 1)
			box_do_vertical_pass(src, srcpitch, dst, dstpitch, mfd->trow, fa->dst.w, fa->dst.h, mfd->filter_width);
		else
			box_do_vertical_pass(dst, dstpitch, src, srcpitch, mfd->trow, fa->dst.w, fa->dst.h, mfd->filter_width);
	}
#else
	for(i=0; i<mfd->filter_power-1; i++) {
		if (i & 1)
			box_do_vertical_pass(src, srcpitch, dst, dstpitch, mfd->trow, fa->dst.w, fa->dst.h, mfd->filter_width);
		else
			box_do_vertical_pass(dst, dstpitch, src, srcpitch, mfd->trow, fa->dst.w, fa->dst.h, mfd->filter_width);
	}

	if (i & 1)
		box_do_vertical_unsharp_pass(src, srcpitch, dst, dstpitch, fa->src.data, fa->src.pitch, mfd->trow, fa->dst.w, fa->dst.h, mfd->filter_width);
	else
		box_do_vertical_unsharp_pass(dst, dstpitch, src, srcpitch, fa->src.data, fa->src.pitch, mfd->trow, fa->dst.w, fa->dst.h, mfd->filter_width);
#endif

	if (MMX_enabled)
		__asm emms

	return 0;
}


int boxEndProc(FilterActivation *fa, const FilterFunctions *ff) {
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;

	freemem(mfd->rows);	mfd->rows = NULL;
	freemem(mfd->trow);	mfd->trow = NULL;

	return 0;
}

long boxParamProc(FilterActivation *fa, const FilterFunctions *ff) {
#ifndef DO_UNSHARP_FILTER
	fa->dst.offset = fa->src.offset;
	return 0;
#else
	return FILTERPARAM_SWAP_BUFFERS;
#endif
}


BOOL CALLBACK boxConfigDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	BoxFilterData *mfd = (BoxFilterData *)GetWindowLong(hdlg, DWL_USER);
	HWND hwndInit;
	char buf[64];

	switch(msg) {
		case WM_INITDIALOG:
			SetWindowLong(hdlg, DWL_USER, lParam);
			mfd = (BoxFilterData *)lParam;

			hwndInit = GetDlgItem(hdlg, IDC_SLIDER_WIDTH);
			SendMessage(hwndInit, TBM_SETRANGE, TRUE, MAKELONG(1,48));
			SendMessage(hwndInit, TBM_SETPOS, TRUE, mfd->filter_width);

			hwndInit = GetDlgItem(hdlg, IDC_SLIDER_POWER);
			SendMessage(hwndInit, TBM_SETRANGE, TRUE, MAKELONG(1,3));
			SendMessage(hwndInit, TBM_SETPOS, TRUE, mfd->filter_power);

			mfd->ifp->InitButton(GetDlgItem(hdlg, IDC_PREVIEW));

			// cheat to initialize the labels

			mfd->filter_width = -1;
			mfd->filter_power = -1;

		case WM_HSCROLL:
			{
				static const char *const szPowers[]={ "1 - box", "2 - quadratic", "3 - cubic" };
				int new_width, new_power;

				new_width = SendDlgItemMessage(hdlg, IDC_SLIDER_WIDTH, TBM_GETPOS, 0, 0);
				new_power = SendDlgItemMessage(hdlg, IDC_SLIDER_POWER, TBM_GETPOS, 0, 0);

				if (new_width != mfd->filter_width || new_power != mfd->filter_power) {
					mfd->filter_width = new_width;
					mfd->filter_power = new_power;

					sprintf(buf, "radius %d", mfd->filter_width + mfd->filter_power - 1);
					SetDlgItemText(hdlg, IDC_STATIC_WIDTH, buf);

					SetDlgItemText(hdlg, IDC_STATIC_POWER, szPowers[new_power-1]);

					mfd->ifp->RedoFrame();
				}
			}
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				EndDialog(hdlg, 0);
				return TRUE;
			case IDCANCEL:
				EndDialog(hdlg, 1);
				return TRUE;
			case IDC_PREVIEW:
				mfd->ifp->Toggle(hdlg);
				return TRUE;
			}
			break;

	}

	return FALSE;
}

int boxConfigProc(FilterActivation *fa, const FilterFunctions *ff, HWND hwnd) {
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;
	BoxFilterData mfd_old;
	int res;

	mfd_old = *mfd;
	mfd->ifp = fa->ifp;

	res = DialogBoxParam(g_hInst,
			MAKEINTRESOURCE(IDD_FILTER_BOX), hwnd,
			boxConfigDlgProc, (LPARAM)mfd);

	if (res)
		*mfd = mfd_old;

	return res;
}

void boxStringProc(const FilterActivation *fa, const FilterFunctions *ff, char *str) {
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;

	wsprintf(str, " (radius %d, power %d)", mfd->filter_width+mfd->filter_power-1, mfd->filter_power);
}

void boxScriptConfig(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;

	mfd->filter_width	= argv[0].asInt();
	mfd->filter_power	= argv[1].asInt();
}

bool boxFssProc(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	BoxFilterData *mfd = (BoxFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d, %d)",
		mfd->filter_width,
		mfd->filter_power);

	return true;
}

