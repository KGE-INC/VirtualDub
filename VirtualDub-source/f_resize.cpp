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

#include <windows.h>
#include <commctrl.h>
#include <crtdbg.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

#include "misc.h"
#include "cpuaccel.h"
#include "resource.h"
#include "gui.h"
#include "filter.h"
#include "resample.h"
#include "vbitmap.h"

extern HINSTANCE g_hInst;

///////////////////////

#if 0

__int64 last_upd;
__int64 profile_column=0;
__int64 profile_row=0;
int profile_column_count=0;
int profile_row_count=0;

#define PROFILE_START								\
		__int64 start_clock;						\
		__asm rdtsc									\
		__asm mov dword ptr start_clock+0,eax		\
		__asm mov dword ptr start_clock+4,edx

#define PROFILE_ADD(x)								\
		__int64 stop_clock;							\
		{__asm rdtsc									\
		__asm mov dword ptr stop_clock+0,eax		\
		__asm mov dword ptr stop_clock+4,edx}		\
		profile_##x += stop_clock - start_clock;	\
		profile_##x##_count ++;

static inline void stats_print() {
	__int64 clk;

	__asm rdtsc
	__asm mov dword ptr clk,eax
	__asm mov dword ptr clk+4,edx

	if (clk - last_upd > 450000000) {
		char buf[128];
		last_upd = clk;

		if (profile_row_count && profile_column_count) {
			sprintf(buf, "%I64d (%d), %I64d (%d) per scanline\n", profile_row/profile_row_count, profile_row_count, profile_column/profile_column_count, profile_column_count);
			profile_row = profile_column = 0;
			profile_row_count = profile_column_count = 0;
			OutputDebugString(buf);
		}
	}
}
#else
static inline void stats_print() {}
#define PROFILE_START
#define PROFILE_ADD(x)
#endif

///////////////////////

#define USE_ASM

extern "C" void asm_resize_interp_row_run(
			void *dst,
			void *src,
			unsigned long width,
			unsigned long xaccum,
			unsigned long x_inc);

extern "C" void asm_resize_interp_col_run(
			void *dst,
			void *src1,
			void *src2,
			unsigned long width,
			unsigned long yaccum);


extern "C" void asm_resize_ccint(Pixel *dst, Pixel *src1, Pixel *src2, Pixel *src3, Pixel *src4, long count, long xaccum, long xint, int *table);
extern "C" void asm_resize_ccint_col(Pixel *dst, Pixel *src1, Pixel *src2, Pixel *src3, Pixel *src4, long count, const int *table);

static void cc_row(Pixel *dst, Pixel *src, long w, long xs_left, long xs_right, long xaccum, long xinc, int *table);
static long accum_start(long inc);

///////////////////////

enum {
	FILTER_NONE				= 0,
	FILTER_BILINEAR			= 1,
	FILTER_BICUBIC			= 2,
	FILTER_TABLEBILINEAR	= 3,
	FILTER_TABLEBICUBIC		= 4
};

#define ACCEL_BICUBICBY2		(1)
#define ACCEL_BILINEARBY2		(2)

static char *filter_names[]={
	"Nearest neighbor",
	"Bilinear",
	"Bicubic",
	"Precise bilinear",
	"Precise bicubic",
};

typedef struct MyFilterData {
	long new_x, new_y, new_xf, new_yf;
	int filter_mode;
	COLORREF	rgbColor;

	HBRUSH		hbrColor;
	IFilterPreview *ifp;
	Pixel *cubic_rows;
	int *x_table;
	int *y_table;
	Pixel **row_table;
	int x_filtwidth;
	int y_filtwidth;
	int *cubic4_tbl;

	bool	fLetterbox;
} MyFilterData;

////////////////////

int revcolor(int c) {
	return ((c>>16)&0xff) | (c&0xff00) | ((c&0xff)<<16);
}

////////////////////

static const __int64 MMX_roundval = 0x0000200000002000i64;

static long __declspec(naked) resize_table_row_MMX(Pixel *out, Pixel *in, int *filter, int filter_width, PixDim w, long accum, long frac) {

	__asm {
		push		ebp
		push		esi
		push		edi
		push		edx
		push		ecx
		push		ebx

		mov			eax,[esp + 24 + 24]
		mov			ebp,[esp + 20 + 24]
		mov			ebx,[esp + 8 + 24]
		mov			edi,[esp + 4 + 24]

		mov			esi,eax
		mov			edx,eax

		mov			ecx,[esp + 16 + 24]
		shr			ecx,1
		mov			[esp+16+24],ecx
		test		ecx,1
		jnz			pixelloop_odd_pairs

pixelloop_even_pairs:
		shr			esi,14
		and			edx,0000ff00h
		and			esi,0fffffffch

		mov			ecx,[esp + 16 + 24]
		shr			edx,5
		add			esi,ebx
		imul		edx,ecx
		add			eax,[esp + 28 + 24]
		add			edx,[esp + 12 + 24]

		movq		mm6,MMX_roundval
		pxor		mm3,mm3
		shr			ecx,1
		movq		mm7,mm6
		pxor		mm2,mm2

coeffloop_unaligned_even_pairs:
		movd		mm0,[esi+0]
		paddd		mm7,mm2			;accumulate alpha/red (pixels 2/3)

		movd		mm1,[esi+4]
		paddd		mm6,mm3			;accumulate green/blue (pixels 2/3)

		movd		mm2,[esi+8]
		punpcklbw	mm0,mm1			;mm1=[a0][a1][r0][r1][g0][g1][b0][b1]

		movq		mm1,mm0			;mm0=[a0][a1][r0][r1][g0][g1][b0][b1]
		pxor		mm5,mm5

		movd		mm3,[esi+12]
		punpckhbw	mm0,mm5			;mm0=[ a0 ][ a1 ][ r0 ][ r1 ]

		pmaddwd		mm0,[edx]		;mm0=[a0*f0+a1*f1][r0*f0+r1*f1]
		punpcklbw	mm2,mm3			;mm2=[a2][a3][r2][r3][g2][g3][b2][b3]

		movq		mm3,mm2			;mm3=[a2][a3][r2][r3][g2][g3][b2][b3]
		punpcklbw	mm1,mm5			;mm1=[ g0 ][ g1 ][ b0 ][ b1 ]

		pmaddwd		mm1,[edx]		;mm1=[g0*f0+g1*f1][b0*f0+b1*f1]
		punpckhbw	mm2,mm5			;mm2=[ a2 ][ a3 ][ r0 ][ r1 ]

		pmaddwd		mm2,[edx+8]		;mm2=[a2*f2+a3*f3][r2*f2+r3*f3]
		punpcklbw	mm3,mm5			;mm3=[ g2 ][ g3 ][ b2 ][ b3 ]

		pmaddwd		mm3,[edx+8]		;mm3=[g2*f2+g3*f3][b2*f2+b3*f3]
		paddd		mm7,mm0			;accumulate alpha/red (pixels 0/1)

		paddd		mm6,mm1			;accumulate green/blue (pixels 0/1)
		add			edx,16

		add			esi,16
		dec			ecx

		jne			coeffloop_unaligned_even_pairs

		paddd		mm7,mm2			;accumulate alpha/red (pixels 2/3)
		paddd		mm6,mm3			;accumulate green/blue (pixels 2/3)

		psrad		mm7,14
		psrad		mm6,14

		packssdw	mm6,mm7
		add			edi,4

		packuswb	mm6,mm6
		dec			ebp

		mov			esi,eax
		mov			edx,eax

		movd		[edi-4],mm6
		jne			pixelloop_even_pairs

		pop			ebx
		pop			ecx
		pop			edx
		pop			edi
		pop			esi
		pop			ebp

		ret

;----------------------------------------------------------------

pixelloop_odd_pairs:
		shr			esi,14
		and			edx,0000ff00h
		and			esi,0fffffffch

		mov			ecx,[esp + 16 + 24]
		shr			edx,5
		add			esi,ebx
		imul		edx,ecx
		add			eax,[esp + 28 + 24]
		add			edx,[esp + 12 + 24]

		movq		mm6,MMX_roundval
		pxor		mm3,mm3
		shr			ecx,1
		pxor		mm2,mm2
		movq		mm7,mm6

coeffloop_unaligned_odd_pairs:
		movd		mm0,[esi+0]
		paddd		mm7,mm2			;accumulate alpha/red (pixels 2/3)

		movd		mm1,[esi+4]
		paddd		mm6,mm3			;accumulate green/blue (pixels 2/3)

		movd		mm2,[esi+8]
		punpcklbw	mm0,mm1			;mm1=[a0][a1][r0][r1][g0][g1][b0][b1]

		movq		mm1,mm0			;mm0=[a0][a1][r0][r1][g0][g1][b0][b1]
		pxor		mm5,mm5

		movd		mm3,[esi+12]
		punpckhbw	mm0,mm5			;mm0=[ a0 ][ a1 ][ r0 ][ r1 ]

		pmaddwd		mm0,[edx]		;mm0=[a0*f0+a1*f1][r0*f0+r1*f1]
		punpcklbw	mm2,mm3			;mm2=[a2][a3][r2][r3][g2][g3][b2][b3]

		movq		mm3,mm2			;mm3=[a2][a3][r2][r3][g2][g3][b2][b3]
		punpcklbw	mm1,mm5			;mm1=[ g0 ][ g1 ][ b0 ][ b1 ]

		pmaddwd		mm1,[edx]		;mm1=[g0*f0+g1*f1][b0*f0+b1*f1]
		punpckhbw	mm2,mm5			;mm2=[ a2 ][ a3 ][ r0 ][ r1 ]

		pmaddwd		mm2,[edx+8]		;mm2=[a2*f2+a3*f3][r2*f2+r3*f3]
		punpcklbw	mm3,mm5			;mm3=[ g2 ][ g3 ][ b2 ][ b3 ]

		pmaddwd		mm3,[edx+8]		;mm3=[g2*f2+g3*f3][b2*f2+b3*f3]
		paddd		mm7,mm0			;accumulate alpha/red (pixels 0/1)

		paddd		mm6,mm1			;accumulate green/blue (pixels 0/1)
		add			edx,16

		add			esi,16
		dec			ecx

		jne			coeffloop_unaligned_odd_pairs

		paddd		mm7,mm2			;accumulate alpha/red (pixels 2/3)
		paddd		mm6,mm3			;accumulate green/blue (pixels 2/3)

		;finish up odd pair

		movd		mm2,[esi+4]		;mm2 = [x0][r0][g0][b0]
		pxor		mm5,mm5
		movd		mm0,[esi]		;mm0 = [x1][r1][g1][b1]
		punpcklbw	mm0,mm2			;mm2 = [x0][x1][r0][r1][g0][g1][b0][b1]
		movq		mm1,mm0
		punpcklbw	mm0,mm5			;mm0 = [g0][g1][b0][b1]
		punpckhbw	mm1,mm5			;mm1 = [x0][x1][r0][r1]

		pmaddwd		mm0,[edx]
		pmaddwd		mm1,[edx]

		paddd		mm6,mm0
		paddd		mm7,mm1

		;combine into pixel

		psrad		mm6,14

		psrad		mm7,14

		packssdw	mm6,mm7
		add			edi,4

		packuswb	mm6,mm6
		dec			ebp

		mov			esi,eax
		mov			edx,eax

		movd		[edi-4],mm6
		jne			pixelloop_odd_pairs

		pop			ebx
		pop			ecx
		pop			edx
		pop			edi
		pop			esi
		pop			ebp

		ret


	}
}

static long __declspec(naked) resize_table_row_by2linear_MMX(Pixel *out, Pixel *in, PixDim w) {
	static const __int64 x0002000200020002 = 0x0002000200020002i64;

	__asm {
		push		ebp
		push		esi
		push		edi
		push		ebx

		mov			ebp,[esp + 12 + 16]		;ebp = pixel counter
		mov			eax,[esp + 16 + 16]		;eax = accumulator
		shl			ebp,2
		neg			ebp
		mov			edi,[esp +  4 + 16]		;edi = destination pointer
		mov			esi,[esp + 8 + 16]		;esi = source
		movq		mm6,x0002000200020002
		pxor		mm7,mm7

		;load row pointers!

		and			ebp,0fffffff8h
		jz			oddpixeltest

		sub			esi,ebp
		sub			edi,ebp
		sub			esi,ebp

		test		dword ptr [esp+8+16],4
		jnz			pixelloop_unaligned

pixelloop:
		movd		mm0,[esi+ebp*2+4]

		movq		mm1,[esi+ebp*2+8]
		punpcklbw	mm0,mm7

		movq		mm2,mm1
		punpcklbw	mm1,mm7

		movq		mm3,[esi+ebp*2+16]
		punpckhbw	mm2,mm7

		movq		mm4,mm3
		punpcklbw	mm3,mm7

		punpckhbw	mm4,mm7
		paddw		mm1,mm1

		paddw		mm3,mm3
		paddw		mm0,mm2

		paddw		mm2,mm4
		paddw		mm0,mm1

		paddw		mm2,mm3
		paddw		mm0,mm6

		paddw		mm2,mm6
		psraw		mm0,2

		psraw		mm2,2
		packuswb	mm0,mm0

		packuswb	mm2,mm2
		add			ebp,8

		movd		[edi+ebp-8],mm0

		movd		[edi+ebp-4],mm2
		jne			pixelloop
		jmp			short oddpixeltest

pixelloop_unaligned:
		movq		mm0,[esi+ebp*2+4]

		movq		mm1,mm0
		punpcklbw	mm0,mm7

		movq		mm2,[esi+ebp*2+12]
		punpckhbw	mm1,mm7

		movq		mm3,mm2
		punpcklbw	mm2,mm7

		movd		mm4,[esi+ebp*2+20]
		punpckhbw	mm3,mm7

		punpcklbw	mm4,mm7
		paddw		mm1,mm1

		paddw		mm3,mm3
		paddw		mm0,mm2

		paddw		mm2,mm4
		paddw		mm0,mm1

		paddw		mm2,mm3
		paddw		mm0,mm6

		paddw		mm2,mm6
		psraw		mm0,2

		psraw		mm2,2
		packuswb	mm0,mm0

		packuswb	mm2,mm2
		add			ebp,8

		movd		[edi+ebp-8],mm0

		movd		[edi+ebp-4],mm2
		jne			pixelloop_unaligned

		;odd pixel?

oddpixeltest:
		test		dword ptr [esp+12+16],1
		jz			xit

			movd		mm0,[esi+4]

			movd		mm1,[esi+8]
			punpcklbw	mm0,mm7
			movd		mm2,[esi+12]
			punpcklbw	mm1,mm7

			punpcklbw	mm2,mm7
			paddw		mm1,mm1

			paddw		mm0,mm2
			paddw		mm1,mm6

			paddw		mm1,mm0

			psraw		mm1,2

			packuswb	mm1,mm1

			movd		[edi],mm1

xit:
		pop			ebx
		pop			edi
		pop			esi
		pop			ebp

		ret
	}
}

static long __declspec(naked) resize_table_row_by2cubic_MMX(Pixel *out, Pixel *in, PixDim w, unsigned long accum, unsigned long fstep, unsigned long istep) {
	static const __int64 x0004000400040004 = 0x0004000400040004i64;
	__asm {
		push		ebp
		push		esi
		push		edi
		push		edx
		push		ecx
		push		ebx

		mov			ebp,[esp + 12 + 24]		;ebp = pixel counter
		mov			eax,[esp + 16 + 24]		;eax = accumulator
		shl			ebp,2
		mov			ebx,[esp + 20 + 24]		;ebx = fractional step
		neg			ebp
		mov			edi,[esp + 4 + 24]		;edi = destination pointer
		mov			ecx,[esp + 24 + 24]		;ecx = integer step

		;load row pointers!

		mov			esi,[esp + 8 + 24]		;esi = row pointer table

		sub			esi,ebp
		sub			esi,ebp

		sub			edi,ebp
		movq		mm6,x0004000400040004
		pxor		mm7,mm7

pixelloop:
		movd		mm0,[esi+ebp*2+4]

		movd		mm1,[esi+ebp*2+12]
		punpcklbw	mm0,mm7
		movd		mm2,[esi+ebp*2+16]
		punpcklbw	mm1,mm7
		movd		mm3,[esi+ebp*2+20]
		punpcklbw	mm2,mm7
		movd		mm4,[esi+ebp*2+28]
		punpcklbw	mm3,mm7

		punpcklbw	mm4,mm7
		psllw		mm2,3

		paddw		mm1,mm3
		paddw		mm0,mm4

		movq		mm3,mm1
		paddw		mm2,mm6

		psllw		mm1,2
		psubw		mm2,mm0

		paddw		mm1,mm3
		add			ebp,4

		paddw		mm2,mm1

		psraw		mm2,4

		packuswb	mm2,mm2

		movd		[edi+ebp-4],mm2
		jne			pixelloop

		pop			ebx
		pop			ecx
		pop			edx
		pop			edi
		pop			esi
		pop			ebp

		ret
	}
}

static void resize_table_row(Pixel *out, Pixel *in, int *filter, int filter_width, PixDim w, PixDim w_left, PixDim w_right, PixDim w2, long accum, long frac, int accel_code) {
	Pixel *in_bottom, *in_top;

	in_bottom = in;
	in_top = in + w2;
	in -= filter_width/2 - 1;

	if (w_left) do {
		int x, r, g, b;
		Pixel *in2;
		int *filter2;

		x = filter_width;
		in2 = in + (accum>>16);
		filter2 = filter + ((accum>>8) & 255)*filter_width;
		r = g = b = 0;

		if (MMX_enabled)
			do {
				Pixel c2, c1;
				int a1, a2;

				if (in2 < in_bottom)
					c1 = *in_bottom;
				else
					c1 =  *in2;
				++in2;
				if (in2 < in_bottom)
					c2 = *in_bottom;
				else
					c2 =  *in2;
				++in2;

				a1 = *filter2; filter2+=2;
				a2 = a1>>16;
				a1 = (signed short) a1;

				r += ((c1>>16)&255) * a1;
				g += ((c1>> 8)&255) * a1;
				b += ((c1    )&255) * a1;
				r += ((c2>>16)&255) * a2;
				g += ((c2>> 8)&255) * a2;
				b += ((c2    )&255) * a2;

			} while(x-=2);
		else
			do {
				Pixel c;
				int a;

				if (in2 < in_bottom)
					c = *in_bottom;
				else
					c =  *in2;
				++in2;

				a = *filter2++;

				r += ((c>>16)&255) * a;
				g += ((c>> 8)&255) * a;
				b += ((c    )&255) * a;

			} while(--x);

		r = (r + 8192)>>14;
		g = (g + 8192)>>14;
		b = (b + 8192)>>14;

		if (r<0) r=0; else if (r>255) r=255;
		if (g<0) g=0; else if (g>255) g=255;
		if (b<0) b=0; else if (b>255) b=255;

		*out++ = (r<<16) + (g<<8) + b;

		accum += frac;

	} while(--w_left);

	if (w)
		if (MMX_enabled) {
			PROFILE_START

			if (accel_code == ACCEL_BICUBICBY2) {
				resize_table_row_by2cubic_MMX(out, in + (accum>>16), w, accum<<16, frac<<16, frac>>16);

				accum += frac*w;
			} else if (accel_code == ACCEL_BILINEARBY2) {
				resize_table_row_by2linear_MMX(out, in + (accum>>16), w);

				accum += frac*w;
			} else
				accum = resize_table_row_MMX(out, in, filter, filter_width, w, accum, frac);

			PROFILE_ADD(row)

			out += w;
		} else
			do {
				int x, r, g, b;
				Pixel *in2;
				int *filter2;

				x = filter_width;
				in2 = in + (accum>>16);
				filter2 = filter + ((accum>>8) & 255)*filter_width;
				r = g = b = 0;
				do {
					Pixel c;
					int a;

					c =  *in2++;
					a = *filter2++;

					r += ((c>>16)&255) * a;
					g += ((c>> 8)&255) * a;
					b += ((c    )&255) * a;

				} while(--x);

				r = (r + 8192)>>14;
				g = (g + 8192)>>14;
				b = (b + 8192)>>14;

				if (r<0) r=0; else if (r>255) r=255;
				if (g<0) g=0; else if (g>255) g=255;
				if (b<0) b=0; else if (b>255) b=255;

				*out++ = (r<<16) + (g<<8) + b;

				accum += frac;
			} while(--w);

	if (w_right) do {
		int x, r, g, b;
		Pixel *in2;
		int *filter2;

		x = filter_width;
		in2 = in + (accum>>16);
		filter2 = filter + ((accum>>8) & 255)*filter_width;
		r = g = b = 0;

		if (MMX_enabled)
			do {
				Pixel c1, c2;
				int a1, a2;

				if (in2 >= in_top)
					c1 = in_top[-1];
				else
					c1 =  *in2++;
				if (in2 >= in_top)
					c2 = in_top[-1];
				else
					c2 =  *in2++;

				a1 = *filter2; filter2+=2;
				a2 = a1>>16;
				a1 = (signed short) a1;

				r += ((c1>>16)&255) * a1;
				g += ((c1>> 8)&255) * a1;
				b += ((c1    )&255) * a1;
				r += ((c2>>16)&255) * a2;
				g += ((c2>> 8)&255) * a2;
				b += ((c2    )&255) * a2;

			} while(x-=2);
		else
			do {
				Pixel c;
				int a;

				if (in2 >= in_top)
					c = in_top[-1];
				else
					c =  *in2++;

				a = *filter2++;

				r += ((c>>16)&255) * a;
				g += ((c>> 8)&255) * a;
				b += ((c    )&255) * a;

			} while(--x);

		r = (r + 8192)>>14;
		g = (g + 8192)>>14;
		b = (b + 8192)>>14;

		if (r<0) r=0; else if (r>255) r=255;
		if (g<0) g=0; else if (g>255) g=255;
		if (b<0) b=0; else if (b>255) b=255;

		*out++ = (r<<16) + (g<<8) + b;

		accum += frac;

	} while(--w_right);
}

static long __declspec(naked) resize_table_col_MMX(Pixel *out, Pixel **in_table, int *filter, int filter_width, PixDim w, long frac) {
	__asm {
		push		ebp
		push		esi
		push		edi
		push		edx
		push		ecx
		push		ebx

		mov			ebp,[esp + 20 + 24]		;ebp = pixel counter
		mov			edi,[esp + 4 + 24]		;edi = destination pointer

		mov			edx,[esp + 12 + 24]
		mov			eax,[esp + 24 + 24]
		shl			eax,2
		imul		eax,[esp + 16 + 24]
		add			edx,eax
		mov			[esp + 12 + 24], edx	;[esp+12+28] = filter pointer

		mov			ecx,[esp + 16 + 24]
		shr			ecx,1
		mov			[esp + 16 + 24],ecx		;ecx = filter pair count

		xor			ebx,ebx					;ebx = source offset 

		mov			ecx,[esp + 16 + 24]		;ecx = filter width counter
		mov			edx,[esp + 12 + 24]		;edx = filter bank pointer
pixelloop:
		mov			eax,[esp + 8 + 24]		;esi = row pointer table
		movq		mm6,MMX_roundval
		pxor		mm5,mm5
		movq		mm7,mm6
		pxor		mm0,mm0
		pxor		mm1,mm1
coeffloop:
		mov			esi,[eax]
		paddd		mm6,mm0

		movd		mm0,[esi+ebx]	;mm0 = [0][0][0][0][x0][r0][g0][b0]
		paddd		mm7,mm1

		mov			esi,[eax+4]
		add			eax,8

		movd		mm1,[esi+ebx]	;mm1 = [0][0][0][0][x1][r1][g1][b1]
		punpcklbw	mm0,mm1			;mm0 = [x0][x1][r0][r1][g0][g1][b0][b1]

		movq		mm1,mm0
		punpcklbw	mm0,mm5			;mm0 = [g1][g0][b1][b0]

		pmaddwd		mm0,[edx]
		punpckhbw	mm1,mm5			;mm1 = [x1][x0][r1][r0]

		pmaddwd		mm1,[edx]
		add			edx,8

		dec			ecx
		jne			coeffloop

		paddd		mm6,mm0
		paddd		mm7,mm1

		psrad		mm6,14
		psrad		mm7,14
		add			edi,4
		packssdw	mm6,mm7
		add			ebx,4
		packuswb	mm6,mm6
		dec			ebp

		mov			ecx,[esp + 16 + 24]		;ecx = filter width counter
		mov			edx,[esp + 12 + 24]		;edx = filter bank pointer

		movd		[edi-4],mm6
		jne			pixelloop

		pop			ebx
		pop			ecx
		pop			edx
		pop			edi
		pop			esi
		pop			ebp

		ret
	}
}

extern long __declspec(naked) resize_table_col_by2linear_MMX(Pixel *out, Pixel **in_table, PixDim w) {
	static const __int64 x0002000200020002 = 0x0002000200020002i64;
	__asm {
		push		ebp
		push		esi
		push		edi
		push		ebx

		mov			ebp,[esp + 12 + 16]		;ebp = pixel counter
		shl			ebp,2
		neg			ebp
		mov			edi,[esp + 4 + 16]		;edi = destination pointer

		;load row pointers!

		mov			esi,[esp + 8 + 16]		;esi = row pointer table

		mov			eax,[esi+4]
		mov			ebx,[esi+8]
		mov			ecx,[esi+12]

		and			ebp,0fffffff8h
		jz			oddpixelcheck

		sub			eax,ebp
		sub			ebx,ebp
		sub			ecx,ebp
		sub			edi,ebp
		movq		mm6,x0002000200020002
		pxor		mm7,mm7

pixelloop:
		movq		mm0,[eax+ebp]

		movq		mm1,[ebx+ebp]
		movq		mm3,mm0

		movq		mm2,[ecx+ebp]
		punpcklbw	mm0,mm7

		movq		mm4,mm1
		punpckhbw	mm3,mm7

		movq		mm5,mm2
		punpcklbw	mm1,mm7

		punpckhbw	mm4,mm7
		paddw		mm1,mm1

		punpcklbw	mm2,mm7
		paddw		mm4,mm4

		punpckhbw	mm5,mm7
		paddw		mm0,mm2

		paddw		mm3,mm5
		paddw		mm1,mm6

		paddw		mm4,mm6
		paddw		mm1,mm0

		paddw		mm4,mm3
		psraw		mm1,2

		psraw		mm4,2
		add			ebp,8

		packuswb	mm1,mm4

		movq		[edi+ebp-8],mm1
		jne			pixelloop

oddpixelcheck:
		test		dword ptr [esp+12+16],1
		jz			xit

		movd		mm0,[eax]

		movd		mm1,[ebx]
		punpcklbw	mm0,mm7
		movd		mm2,[ecx]
		punpcklbw	mm1,mm7

		punpcklbw	mm2,mm7
		paddw		mm1,mm1

		paddw		mm0,mm2
		paddw		mm1,mm6

		paddw		mm1,mm0

		psraw		mm1,2

		packuswb	mm1,mm1

		movd		[edi],mm1

xit:
		pop			ebx
		pop			edi
		pop			esi
		pop			ebp

		ret
	}
}

extern long __declspec(naked) resize_table_col_by2cubic_MMX(Pixel *out, Pixel **in_table, PixDim w) {
	static const __int64 x0008000800080008 = 0x0008000800080008i64;
	__asm {
		push		ebp
		push		esi
		push		edi
		push		edx
		push		ecx
		push		ebx

		mov			ebp,[esp + 12 + 24]		;ebp = pixel counter
		shl			ebp,2
		neg			ebp
		mov			edi,[esp + 4 + 24]		;edi = destination pointer

		;load row pointers!

		mov			esi,[esp + 8 + 24]		;esi = row pointer table

		mov			eax,[esi+4]
		mov			ebx,[esi+12]
		mov			ecx,[esi+16]
		mov			edx,[esi+20]
		mov			esi,[esi+28]

		sub			eax,ebp
		sub			ebx,ebp
		sub			ecx,ebp
		sub			edx,ebp
		sub			esi,ebp
		sub			edi,ebp
		movq		mm6,x0008000800080008
		pxor		mm7,mm7

pixelloop:
		movd		mm0,[eax+ebp]

		movd		mm1,[ebx+ebp]
		punpcklbw	mm0,mm7
		movd		mm2,[ecx+ebp]
		punpcklbw	mm1,mm7
		movd		mm3,[edx+ebp]
		punpcklbw	mm2,mm7
		movd		mm4,[esi+ebp]
		punpcklbw	mm3,mm7

		punpcklbw	mm4,mm7
		paddw		mm1,mm3

		movq		mm3,mm1
		psllw		mm1,2

		paddw		mm0,mm4
		psllw		mm2,3

		paddw		mm1,mm3
		paddw		mm2,mm6

		psubw		mm2,mm0
		paddw		mm2,mm1

		psraw		mm2,4

		packuswb	mm2,mm2
		add			ebp,4
		movd		[edi+ebp-4],mm2
		jne			pixelloop

		pop			ebx
		pop			ecx
		pop			edx
		pop			edi
		pop			esi
		pop			ebp

		ret
	}
}

static void resize_table_col(Pixel *out, Pixel **in_rows, int *filter, int filter_width, PixDim w, long frac, int accel_code) {
	int x;

	if (MMX_enabled) {
		PROFILE_START

		if (accel_code == ACCEL_BICUBICBY2)
			resize_table_col_by2cubic_MMX(out, in_rows, w);
		else if (accel_code == ACCEL_BILINEARBY2)
			resize_table_col_by2linear_MMX(out, in_rows, w);
		else
			resize_table_col_MMX(out, in_rows, filter, filter_width, w, frac);

		PROFILE_ADD(column)

		return;
	}

	x = 0;
	do {
		int x2, r, g, b;
		Pixel **in_row;
		int *filter2;

		x2 = filter_width;
		in_row = in_rows;
		filter2 = filter + frac*filter_width;
		r = g = b = 0;
		do {
			Pixel c;
			int a;

			c =  (*in_row++)[x];
			a = *filter2++;

			r += ((c>>16)&255) * a;
			g += ((c>> 8)&255) * a;
			b += ((c    )&255) * a;

		} while(--x2);

		r = (r + 8192)>>14;
		g = (g + 8192)>>14;
		b = (b + 8192)>>14;

		if (r<0) r=0; else if (r>255) r=255;
		if (g<0) g=0; else if (g>255) g=255;
		if (b<0) b=0; else if (b>255) b=255;

		*out++ = (r<<16) + (g<<8) + b;
	} while(++x < w);
}


#define RED(x) ((signed long)((x)>>16)&255)
#define GRN(x) ((signed long)((x)>> 8)&255)
#define BLU(x) ((signed long)(x)&255)

#if 0
#if 0
static inline long cerpclip(long y1, long y2, long y3, long y4, long d) {
	// floating-point:	y2 + d((y3-y1) + d((2y1-2y2+y3-y4) + d(-y1+y2-y3+y4)))
	// fixed-point:		(256*y2 + d((65536*(y3-y1) + d(256*(2y1-2y2+y3-y4) + d(-y1+y2-y3+y4)))>>16))>>8

	long t = 65536*y2 + d*((65536*(y3-y1) + d*(256*(2*y1-2*y2+y3-y4) + d*(-y1+y2-y3+y4)))>>8);

	return t<0 ? 0 : t>0xFFFFFF ? 0xFFFFFF : t;
//	return d<<16;//(y2*(256-d)+y3*d)<<8;
}
#else
static inline long cerpclip(long y1, long y2, long y3, long y4, long d) {
	//	65536*y2 + d*((65536*(y3-y1) + d*(256*(2*y1-2*y2+y3-y4) + d*(-y1+y2-y3+y4)))>>8);

	//	t1 = y1-y2;
	//	t2 = t1+y3-y4;
	//	res = 65536*y2 + d*((65536*(y3-y1) + d*(256*(t1+t2) - d*t2))>>8);
	__asm {
		mov		eax,y1
		mov		ebx,y3

		sub		eax,y2		;eax = t1
		sub		ebx,y4

		add		ebx,eax		;ebx = t2
		mov		ecx,d		;ecx = d

		add		eax,ebx		;eax = t1+t2
		mov		edx,y3		;edx = y3

		imul	ecx,ebx		;ecx = d*t2

		shl		eax,8		;eax = 256*(t1+t2)
		mov		ebx,y1		;ebx = y1

		sub		edx,ebx		;edx = y3-y1
		sub		eax,ecx		;eax = 256*(t1+t2) - d*t2

		imul	eax,d		;eax = d*(256*(t1+t2) - d*t2)

		shl		edx,16		;edx = 65536*(y3-y1)
		mov		ecx,y2		;ecx = y2

		shl		ecx,16		;ecx = 65536*y2
		add		eax,edx		;eax = 65536*(y3-y1) + d*(256*(t1+t2) - d*t2)

		sar		eax,8		;eax = (65536*(y3-y1) + d*(256*(t1+t2) - d*t2))>>8
		mov		edx,16777215

		imul	eax,d		;eax = d*((65536*(y3-y1) + d*(256*(t1+t2) - d*t2))>>8)

		add		eax,ecx		;eax = 65536*y2 + d*((65536*(y3-y1) + d*(256*(t1+t2) - d*t2))>>8);
		mov		ecx,80000000h

		add		ecx,eax		;ecx is negative if eax is okay, positive if not
		sub		edx,eax		;set carry if eax > FFFFFF

		sar		edx,31

		sar		ecx,31
		and		edx,00FFFFFFh

		and		eax,ecx

		or		eax,edx		;if eax>FFFFFF, clamp at FFFFFFFF

	}
}
#endif

static inline Pixel cc(Pixel y1, Pixel y2, Pixel y3, Pixel y4, long d) {
	long red, grn, blu;

	red = cerpclip(RED(y1),RED(y2),RED(y3),RED(y4),d);

//	if (red<0) red=0; else if (red>16777215) red=16777215;
//	if (red<0) red=0;
//	if (red>16777215) red=16777215;

	grn = cerpclip(GRN(y1),GRN(y2),GRN(y3),GRN(y4),d);

//	if (grn<0) grn=0; else if (grn>16777215) grn=16777215;
//	if (grn<0) grn=0;
//	if (grn>16777215) grn=16777215;

	blu = cerpclip(BLU(y1),BLU(y2),BLU(y3),BLU(y4),d);

//	if (blu<0) blu=0; else if (blu>16777215) blu=16777215;
//	if (blu<0) blu=0;
//	if (blu>16777215) blu=16777215;

	return (red & 0xFF0000) | ((grn>>8) & 0x00FF00) | (blu>>16);
}
#else
static inline Pixel cc(const Pixel *yptr, const int *tbl) {
	const Pixel y1 = yptr[0];
	const Pixel y2 = yptr[1];
	const Pixel y3 = yptr[2];
	const Pixel y4 = yptr[3];
	long red, grn, blu;

	red = RED(y1)*tbl[0] + RED(y2)*tbl[1] + RED(y3)*tbl[2] + RED(y4)*tbl[3];
	grn = GRN(y1)*tbl[0] + GRN(y2)*tbl[1] + GRN(y3)*tbl[2] + GRN(y4)*tbl[3];
	blu = BLU(y1)*tbl[0] + BLU(y2)*tbl[1] + BLU(y3)*tbl[2] + BLU(y4)*tbl[3];

	if (red<0) red=0; else if (red>4194303) red=4194303;
	if (grn<0) grn=0; else if (grn>4194303) grn=4194303;
	if (blu<0) blu=0; else if (blu>4194303) blu=4194303;

	return ((red<<2) & 0xFF0000) | ((grn>>6) & 0x00FF00) | (blu>>14);
}
static inline Pixel cc2(Pixel y1, Pixel y2, Pixel y3, Pixel y4, const int *tbl) {
	long red, grn, blu;

	red = RED(y1)*tbl[0] + RED(y2)*tbl[1] + RED(y3)*tbl[2] + RED(y4)*tbl[3];
	grn = GRN(y1)*tbl[0] + GRN(y2)*tbl[1] + GRN(y3)*tbl[2] + GRN(y4)*tbl[3];
	blu = BLU(y1)*tbl[0] + BLU(y2)*tbl[1] + BLU(y3)*tbl[2] + BLU(y4)*tbl[3];

	if (red<0) red=0; else if (red>4194303) red=4194303;
	if (grn<0) grn=0; else if (grn>4194303) grn=4194303;
	if (blu<0) blu=0; else if (blu>4194303) blu=4194303;

	return ((red<<2) & 0xFF0000) | ((grn>>6) & 0x00FF00) | (blu>>14);
}
#endif

#undef RED
#undef GRN
#undef BLU

#if 1
static void cc_row(Pixel *dst, Pixel *src, long w, long xs_left, long xs_right, long xaccum, long xinc, int *table) {

	src += xaccum>>16;
	xaccum&=0xffff;

	if (xs_left) {
		Pixel x[4] = { src[0], src[0], src[1], src[2] };

		do {
			*dst++ = cc(x, table + 4*((xaccum>>8)&0xff));

			xaccum += xinc;
			src += xaccum>>16;
			xaccum&=0xffff;
		} while(--xs_left);
	}

	if (!MMX_enabled) {
		do {
			*dst++ = cc(src-1, table + 4*((xaccum>>8)&0xff));

			xaccum += xinc;
			src += xaccum>>16;
			xaccum&=0xffff;
		} while(--w);
	} else {
		asm_resize_ccint(dst, src-1, src, src+1, src+2, w, xaccum, xinc, table+1024);

		dst += w;

		xaccum += xinc*w;
		src += xaccum>>16;
		xaccum &= 0xffff;
	}

	if (xs_right) do {
		Pixel x[4] = { src[-1], src[0], src[1], src[1] };

		*dst++ = cc(x, table + 4*((xaccum>>8)&0xff));

		xaccum += xinc;
		src += xaccum>>16;
		xaccum&=0xffff;
	} while(--xs_right);
}
#else
static void cc_row(Pixel *dst, Pixel *src, long w, long xs_left, long xs_right, long xaccum, long xinc) {

	src += xaccum>>16;
	xaccum&=0xffff;

	if (xs_left) do {
		*dst++ = cc(src[0], src[0], src[1], src[2], (xaccum>>8)&0xff);

		xaccum += xinc;
		src += xaccum>>16;
		xaccum&=0xffff;
	} while(--xs_left);

	if (!MMX_enabled) {
		do {
			*dst++ = cc(src[-1],src[0],src[1],src[2],(xaccum>>8)&0xff);

			xaccum += xinc;
			src += xaccum>>16;
			xaccum&=0xffff;
		} while(--w);
	} else {
		asm_resize_ccint(dst, src-1, src, src+1, src+2, w, xaccum, xinc+1024);

		dst += w;

		xaccum += xinc*w;
		src += xaccum>>16;
		xaccum &= 0xffff;
	}

	if (xs_right) do {
		*dst++ = cc(src[-1], src[0], src[1], src[1], (xaccum>>8)&0xff);

		xaccum += xinc;
		src += xaccum>>16;
		xaccum&=0xffff;
	} while(--xs_right);
}
#endif

static void bc_row(Pixel *dst, Pixel *src, long w, long xs_left, long xs_right, long xaccum, long xinc) {

	if (xs_left) do {
		*dst++ = *src;
	} while(--xs_left);

	src += xaccum>>16;
	xaccum&=0xffff;

	asm_resize_interp_row_run(dst, src, w, xaccum, xinc);

	dst += w;

	xaccum += xinc*w;
	src += xaccum>>16;
	xaccum &= 0xffff;

	if (xs_right) do {
		*dst++ = *src;
	} while(--xs_right);
}

//#pragma optimize("",off);

// Conditions we need to handle:
//
//	0x20000 (interpolation): offset should be 0x8000

long accum_start(long inc) {

	if (inc >= 0x10000) {
		inc &= 0xffff;

		if (!inc) inc = 0x10000;

		return inc/2;
	} else
		return 0;
}

static int resize_table(const FilterActivation *fa, const FilterFunctions *ff, Pixel *dst) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	PixDim dstw = mfd->new_x;
	PixDim dsth = mfd->new_y;
	PixDim w = dstw;
	PixDim h = dsth;
	Pixel *src;
	long	x_accum=0, x_inc;
	long	y_accum=0, y_inc;
	PixDim w_left, w_right, w_mid;
	int row = 0;
	int y = 0;
	int accel_col = 0, accel_row = 0;

	x_inc = (fa->src.w<<16) / dstw;
	y_inc = (fa->src.h<<16) / dsth;
	src = fa->src.data;

	if (mfd->filter_mode == FILTER_TABLEBICUBIC && y_inc == 0x20000L)
		accel_col = ACCEL_BICUBICBY2;

	if (mfd->filter_mode == FILTER_TABLEBILINEAR && y_inc == 0x20000L)
		accel_col = ACCEL_BILINEARBY2;

	if (mfd->filter_mode == FILTER_TABLEBICUBIC && x_inc == 0x20000L)
		accel_row = ACCEL_BICUBICBY2;

	if (mfd->filter_mode == FILTER_TABLEBILINEAR && x_inc == 0x20000L)
		accel_row = ACCEL_BILINEARBY2;

	if (mfd->x_table) {
		w_left = (0x10000 * ((mfd->x_filtwidth - 2)>>1) + x_inc-1)/x_inc;
		w_right = w_left;
	} else if (mfd->filter_mode == FILTER_TABLEBICUBIC) {
		long xt;

		x_accum = accum_start(x_inc);

		// Determine the 3 x segments (l, m, r)
		//
		// left: number of increments it takes to get to the 2nd pixel

		if (x_accum >= 0x10000L)
			w_left = 0;
		else
			w_left = (0xFFFF + x_inc - x_accum) / x_inc;

		// right: number of increments from (width-1) to expected last value, rounded up

		xt = x_accum + w*x_inc;
		if (xt >= (fa->src.w-1)*0x10000)
			w_right = (xt - (fa->src.w-1)*0x10000) / x_inc + 1;
		else
			w_right = 0;

	} else {
		long xt;

		x_accum = accum_start(x_inc);

		w_left = 0;

		// right: number of increments from (width) to expected last value, rounded up

		xt = x_accum + w*x_inc;
		if (xt >= fa->src.w*0x10000)
			w_right = (xt - fa->src.w*0x10000) / x_inc + 1;
		else
			w_right = 0;
	}

	w_mid = w - w_left - w_right;

	{
		int prefill = mfd->y_filtwidth/2 - 1;
		int i;


		for(i=prefill; i<mfd->y_filtwidth; i++) {
			if (mfd->x_table)
				resize_table_row(mfd->row_table[i], src, mfd->x_table, mfd->x_filtwidth, w_mid, w_left, w_right, fa->src.w, 0, x_inc, accel_row);
			else if (mfd->filter_mode == FILTER_TABLEBICUBIC)
				cc_row(mfd->row_table[i], src, w_mid, w_left, w_right, x_accum, x_inc, mfd->cubic4_tbl);
			else
				bc_row(mfd->row_table[i], src, w_mid, w_left, w_right, x_accum, x_inc);

			src = (Pixel *)((char *)src + fa->src.pitch);
			++y;
		}

		for(i=0; i<prefill; i++)
			memcpy(mfd->row_table[i], mfd->row_table[prefill], sizeof(Pixel)*dstw);
	}

	do {
		if (mfd->y_table)
			resize_table_col(dst, mfd->row_table + row, mfd->y_table, mfd->y_filtwidth, w, (y_accum>>8)&255, accel_col);
		else if (mfd->filter_mode == FILTER_TABLEBICUBIC) {
			int row2;

			row2 = (row - 4)&3;

			if (MMX_enabled)
				asm_resize_ccint_col(dst, mfd->row_table[row2], mfd->row_table[row2+1], mfd->row_table[row2+2], mfd->row_table[row2+3], w, mfd->cubic4_tbl+1024+4*((y_accum>>8)&0xff));
			else {
				Pixel32 *dst2 = dst;
				int x = 0;

				do {
					*dst2++ = cc2(mfd->row_table[row2+0][x], mfd->row_table[row2+1][x], mfd->row_table[row2+2][x], mfd->row_table[row2+3][x], mfd->cubic4_tbl + 4*((y_accum>>8)&0xff));
				} while(++x < w);
			}
		} else {
			asm_resize_interp_col_run(
					dst,
					mfd->row_table[row],
					mfd->row_table[row^1],
					w,
					y_accum);

		}

		y_accum += y_inc;

		while(y_accum >= 0x10000) {
			if (y < fa->src.h)
				if (mfd->x_table)
					resize_table_row(mfd->row_table[row], src, mfd->x_table, mfd->x_filtwidth, w_mid, w_left, w_right, fa->src.w, 0, x_inc, accel_row);
				else if (mfd->filter_mode == FILTER_TABLEBICUBIC)
					cc_row(mfd->row_table[row], src, w_mid, w_left, w_right, x_accum, x_inc, mfd->cubic4_tbl);
				else
					bc_row(mfd->row_table[row], src, w_mid, w_left, w_right, x_accum, x_inc);
			else
				memcpy(mfd->row_table[row], mfd->row_table[row ? row-1 : mfd->y_filtwidth-1], dstw*sizeof(Pixel));

			src = (Pixel *)((char *)src + fa->src.pitch);

			++y;
			y_accum -= 0x10000;
			if (++row >= mfd->y_filtwidth)
				row = 0;
		}

		dst = (Pixel *)((char *)dst + fa->dst.pitch);
	} while(--h);

	if (MMX_enabled)
		__asm emms

	return 0;
}

static int resize_run(const FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	unsigned long x_inc, y_inc;
	long dstw = mfd->new_x;
	long dsth = mfd->new_y;
	long h_margin=0, w_margin=0;
	Pixel *dst, *src;

	dst = fa->dst.data;
	src = fa->src.data;

	// Draw letterbox bound

	if (mfd->fLetterbox) {
		Pixel *dst2 = dst, *dst3;
		Pixel fill = revcolor(mfd->rgbColor);

		long w1, w2, w, h;

		w_margin = fa->dst.w - mfd->new_x;
		h_margin = fa->dst.h - mfd->new_y;

		if (w_margin < 0)
			w_margin = 0;

		if (h_margin < 0)
			h_margin = 0;

		h = h_margin/2;
		if (h>0) do {
			dst3  = dst2;
			w = fa->dst.w;
			do {
				*dst3++ = fill;
			} while(--w);

			dst2 = (Pixel32 *)((char *)dst2 + fa->dst.pitch);
		} while(--h);

		w1 = w_margin/2;
		w2 = w_margin - w_margin/2;

		h = mfd->new_y;
		do {
			dst3 = dst2;

			// fill left

			w = w1;
			if (w) do {
				*dst3++ = fill;
			} while(--w);

			// skip center

			dst3 += mfd->new_x;

			// fill right

			w = w2;
			if (w) do {
				*dst3++ = fill;
			} while(--w);


			dst2 = (Pixel32 *)((char *)dst2 + fa->dst.pitch);
		} while(--h);

		h = h_margin - h_margin/2;
		if (h>0) do {
			dst3  = dst2;
			w = fa->dst.w;
			do {
				*dst3++ = fill;
			} while(--w);

			dst2 = (Pixel32 *)((char *)dst2 + fa->dst.pitch);
		} while(--h);

		// offset resampled rectangle

		dst = (Pixel *)((char *)dst + fa->dst.pitch * (h_margin/2)) + (w_margin/2);
	}

	// Compute parameters

	if (dstw >= fa->src.w)
		x_inc = MulDivTrunc(fa->src.w-1, 0x10000L, dstw-1);
	else
		x_inc = MulDivTrunc(fa->src.w, 0x10000L, dstw);

	if (dsth >= fa->src.h)
		y_inc = MulDivTrunc(fa->src.h-1, 0x10000L, dsth-1);
	else
		y_inc = MulDivTrunc(fa->src.h, 0x10000L, dsth);

	switch(mfd->filter_mode) {
	case FILTER_BICUBIC:
		{
			long x_accum, y_accum, y_frac, xt;
			long w,h,max_h,x_left, x_right, x_mid;
			long width = dstw;
			Pixel *rsrc;
			int row_rotate=0;

			x_accum = accum_start(x_inc);
			y_accum = accum_start(y_inc);

			// Determine the 3 x segments (l, m, r)
			//
			// left: number of increments it takes to get to the 2nd pixel

			if (x_accum >= 0x10000L)
				x_left = 0;
			else
//				x_left = (0xFFFF + x_inc - (x_inc/2)) / x_inc;
				x_left = (0xFFFF + x_inc - x_accum) / x_inc;

			// right: number of increments from (width-1) to expected last value, rounded up

			xt = x_accum + width*x_inc;
			if (xt >= (fa->src.w-1)*0x10000)
				x_right = (xt - (fa->src.w-1)*0x10000) / x_inc + 1;
			else
				x_right = 0;

			x_mid = width - x_left - x_right;

			_ASSERT(x_left>=0);
			_ASSERT(x_right>=0);
			_ASSERT(x_mid>=0);

			// make sure our left and right values won't cause crashes

			_ASSERT(x_accum + x_left * x_inc >= 0x10000);
			_ASSERT(x_accum + (x_left+x_mid)*x_inc < (fa->src.w-1)*0x10000);

			// make sure our left and right values are optimal

			_ASSERT(!x_left || x_accum + (x_left-1)*x_inc < 0x10000);
			_ASSERT(!x_right || !x_mid || x_accum + (x_left+x_mid+1)*x_inc >= (fa->src.w-1)*0x10000);

			// Compute first 4 cubic rows

			cc_row(mfd->cubic_rows + 0*width, (Pixel *)((char *)src + (y_accum>=0x10000 ? -1 : 0)*fa->src.pitch), x_mid, x_left, x_right, x_accum, x_inc, mfd->cubic4_tbl);
			cc_row(mfd->cubic_rows + 1*width, (Pixel *)((char *)src +  0*fa->src.pitch), x_mid, x_left, x_right, x_accum, x_inc, mfd->cubic4_tbl);
			cc_row(mfd->cubic_rows + 2*width, (Pixel *)((char *)src +  1*fa->src.pitch), x_mid, x_left, x_right, x_accum, x_inc, mfd->cubic4_tbl);
			cc_row(mfd->cubic_rows + 3*width, (Pixel *)((char *)src +  2*fa->src.pitch), x_mid, x_left, x_right, x_accum, x_inc, mfd->cubic4_tbl);

			// max_h = (number of increments to get y to [height-1])

			max_h = (y_accum + (fa->src.h-1)*0x10000L + y_inc-1 ) / y_inc - 1;

			_ASSERT(max_h>=0);

			src = (Pixel *)((char *)src + (y_accum>>16) * fa->src.pitch);
			y_accum &= 0xffff;

			// Start the cubic!

			h = dsth;
			do {
				y_frac = ((y_accum>>8)&255);
				const int *co_ptr = mfd->cubic4_tbl + 4*y_frac;
				rsrc = mfd->cubic_rows;

				w = dstw;

				if (!MMX_enabled) {
					switch(row_rotate) {
					case 0:
						do {

	//	CODEGEN ERROR!	*dst++ = cc(rsrc[0], rsrc[width], rsrc[2*width], rsrc[3*width], y_frac);
	//
	//	Causes VC++ 6.0 in Release mode to push the same argument for rsrc[width] and rsrc[2*width]

							Pixel *row1 = rsrc + width;
							Pixel *row2 = row1 + width;
							Pixel *row3 = row2 + width;

							*dst++ = cc2(*rsrc, *row1, *row2, *row3, co_ptr);

							++rsrc;
						} while(--w);
						break;
					case 1:
						do {
							Pixel *row1 = rsrc + width;
							Pixel *row2 = row1 + width;
							Pixel *row3 = row2 + width;

							*dst++ = cc2(*row1, *row2, *row3, *rsrc, co_ptr);

							++rsrc;
						} while(--w);
						break;
					case 2:
						do {
							Pixel *row1 = rsrc + width;
							Pixel *row2 = row1 + width;
							Pixel *row3 = row2 + width;

							*dst++ = cc2(*row2, *row3, *rsrc, *row1, co_ptr);

							++rsrc;
						} while(--w);
						break;
					case 3:
						do {
							Pixel *row1 = rsrc + width;
							Pixel *row2 = row1 + width;
							Pixel *row3 = row2 + width;

							*dst++ = cc2(*row3, *rsrc, *row1, *row2, co_ptr);

							++rsrc;
						} while(--w);
						break;
					}
//					dst = (Pixel *)((char *)dst + fa->dst.modulo);
					dst = (Pixel *)((char *)(dst-dstw) + fa->dst.pitch);
				} else {
					switch(row_rotate) {
					case 0:
						asm_resize_ccint_col(dst, rsrc, rsrc+width, rsrc+2*width, rsrc+3*width, w, co_ptr + 1024);
						break;
					case 1:
						asm_resize_ccint_col(dst, rsrc+width, rsrc+2*width, rsrc+3*width, rsrc, w, co_ptr + 1024);
						break;
					case 2:
						asm_resize_ccint_col(dst, rsrc+2*width, rsrc+3*width, rsrc, rsrc+width, w, co_ptr + 1024);
						break;
					case 3:
						asm_resize_ccint_col(dst, rsrc+3*width, rsrc, rsrc+width, rsrc+2*width, w, co_ptr + 1024);
						break;
					}
					dst = (Pixel *)((char *)dst + fa->dst.pitch);
				}

				y_accum += y_inc;
				if (y_accum>>16) {
					int yd = y_accum>>16;
					Pixel *rsrc, *rdst;

					src = (Pixel *)((char *)src + yd * fa->src.pitch);
					y_accum &= 0xffff;

//					if (yd<4) {
//						memmove(mfd->cubic_rows, mfd->cubic_rows + width*yd, sizeof(Pixel)*width*(4-yd));
//					} else {
//						yd = 4;
//					}
					if (yd>4) yd=4;

					// If we're scaling down, we don't want to run off the bottom of the
					// bitmap.

					if (max_h > 0) {
						--max_h;

						rsrc = (Pixel *)((char *)src + fa->src.pitch * (3-yd));
						while(yd--) {
							rdst = mfd->cubic_rows + width*row_rotate;
							cc_row(rdst, rsrc, x_mid, x_left, x_right, accum_start(x_inc), x_inc, mfd->cubic4_tbl);
							row_rotate = (row_rotate+1) & 3;
							rsrc = (Pixel *)((char *)rsrc + fa->src.pitch);
						}
					}
				}
			} while(--h);
		}
		break;

	case FILTER_BILINEAR:
#ifdef USE_ASM
/*		asm_resize_interp_run(
				dst,
				src,
				dstw,
				dsth,
				fa->src.pitch,
				fa->dst.pitch,
				accum_start(x_inc), //<0x8000 ? x_inc/2 : 0,
				accum_start(y_inc), //<0x8000 ? y_inc/2 : 0,
				x_inc,
				y_inc);*/

		fa->dst.StretchBltBilinearFast(
				w_margin/2, h_margin/2,
				dstw, dsth,
				&fa->src,
				0.0, 0.0,
				fa->src.w, fa->src.h);

#else
		{
			y_accum = accum_start(y_inc);

			h = dsth;
			do {
				unsigned long y_frac = (y_accum>>12)&15;

				x_accum = accum_start(x_inc);
				w = dstw;
				do {
					unsigned long *srct = src + (x_accum>>16);
					unsigned long *srcb = (unsigned long *)((char *)srct + fa->src.pitch);
					unsigned long x_frac = (x_accum>>12)&15;

					*dst++	=  (((((srct[0]&0xff00ff)*(16-x_frac) + (srct[1]&0xff00ff)*x_frac)*(16-y_frac)
								+ ((srcb[0]&0xff00ff)*(16-x_frac) + (srcb[1]&0xff00ff)*x_frac)*y_frac) & 0xff00ff00)>>8)
							|  (((((srct[0]&0x00ff00)*(16-x_frac) + (srct[1]&0x00ff00)*x_frac)*(16-y_frac)
								+ ((srcb[0]&0x00ff00)*(16-x_frac) + (srcb[1]&0x00ff00)*x_frac)*y_frac) & 0x00ff0000)>>8);

					x_accum += x_inc;
				} while(--w);

				dst = (unsigned long *)((char *)dst + fa->dst.modulo);

				y_accum += y_inc;
				if (y_accum>>16) {
					src = (unsigned long *)((char *)src + (y_accum>>16) * fa->src.pitch);
					y_accum &= 0xffff;
				}
			} while(--h);
		}
#endif
		break;

	case FILTER_NONE:
		fa->dst.StretchBltNearestFast(
				w_margin/2, h_margin/2,
				dstw, dsth,
				&fa->src,
				0.0, 0.0,
				fa->src.w, fa->src.h);
		break;

	default:
		resize_table(fa, ff, dst);
		break;
	}

	stats_print();

	return 0;
}

static long resize_param(FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	if (!mfd) return 0;

	if (mfd->fLetterbox) {
		fa->dst.w		= max(mfd->new_x, mfd->new_xf);
		fa->dst.h		= max(mfd->new_y, mfd->new_yf);
	} else {
		fa->dst.w		= mfd->new_x;
		fa->dst.h		= mfd->new_y;
	}
	fa->dst.AlignTo8();

	return FILTERPARAM_SWAP_BUFFERS;
}

static BOOL APIENTRY resizeDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	MyFilterData *mfd = (struct MyFilterData *)GetWindowLong(hDlg, DWL_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			{
				HWND hwndItem;
				int i;

				mfd = (MyFilterData *)lParam;

				SetDlgItemInt(hDlg, IDC_WIDTH, mfd->new_x, FALSE);
				SetDlgItemInt(hDlg, IDC_HEIGHT, mfd->new_y, FALSE);
				SetDlgItemInt(hDlg, IDC_FRAMEWIDTH, mfd->new_xf, FALSE);
				SetDlgItemInt(hDlg, IDC_FRAMEHEIGHT, mfd->new_yf, FALSE);

				hwndItem = GetDlgItem(hDlg, IDC_FILTER_MODE);

				for(i=0; i<(sizeof filter_names/sizeof filter_names[0]); i++)
					SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)filter_names[i]);

				SendMessage(hwndItem, CB_SETCURSEL, mfd->filter_mode, 0);

				if (mfd->fLetterbox) {
					CheckDlgButton(hDlg, IDC_LETTERBOX, BST_CHECKED);
				} else {
					CheckDlgButton(hDlg, IDC_LETTERBOX, BST_UNCHECKED);
					EnableWindow(GetDlgItem(hDlg, IDC_FRAMEWIDTH), FALSE);
					EnableWindow(GetDlgItem(hDlg, IDC_FRAMEHEIGHT), FALSE);
					EnableWindow(GetDlgItem(hDlg, IDC_STATIC_FILLCOLOR), FALSE);
					EnableWindow(GetDlgItem(hDlg, IDC_COLOR), FALSE);
					EnableWindow(GetDlgItem(hDlg, IDC_PICKCOLOR), FALSE);
				}

				mfd->hbrColor = CreateSolidBrush(mfd->rgbColor);

				SetWindowLong(hDlg, DWL_USER, (LONG)mfd);

				mfd->ifp->InitButton(GetDlgItem(hDlg, IDC_PREVIEW));
			}
            return (TRUE);

        case WM_COMMAND:                      
			switch(LOWORD(wParam)) {
			case IDOK:
				mfd->ifp->Close();
				EndDialog(hDlg, 0);
				return TRUE;

			case IDCANCEL:
				mfd->ifp->Close();
                EndDialog(hDlg, 1);
                return TRUE;

			case IDC_WIDTH:
				if (HIWORD(wParam) == EN_KILLFOCUS) {
					long new_x;
					BOOL success;

					new_x = GetDlgItemInt(hDlg, IDC_WIDTH, &success, FALSE);
					if (!success || new_x < 16) {
						SetFocus((HWND)lParam);
						MessageBeep(MB_ICONQUESTION);
						return TRUE;
					}

					mfd->ifp->UndoSystem();
					mfd->new_x = new_x;
					mfd->ifp->RedoSystem();
				}
				return TRUE;

			case IDC_HEIGHT:
				if (HIWORD(wParam) == EN_KILLFOCUS) {
					long new_y;
					BOOL success;

					new_y = GetDlgItemInt(hDlg, IDC_HEIGHT, &success, FALSE);
					if (!success || new_y < 16) {
						SetFocus((HWND)lParam);
						MessageBeep(MB_ICONQUESTION);
						return TRUE;
					}

					mfd->ifp->UndoSystem();
					mfd->new_y = new_y;
					mfd->ifp->RedoSystem();
				}
				return TRUE;

			case IDC_FRAMEWIDTH:
				if (HIWORD(wParam) == EN_KILLFOCUS) {
					long new_xf;
					BOOL success;

					new_xf = GetDlgItemInt(hDlg, IDC_FRAMEWIDTH, &success, FALSE);
					if (!success || new_xf < mfd->new_x) {
						SetFocus((HWND)lParam);
						MessageBeep(MB_ICONQUESTION);
						return TRUE;
					}

					mfd->ifp->UndoSystem();
					mfd->new_xf = new_xf;
					mfd->ifp->RedoSystem();
				}
				return TRUE;

			case IDC_FRAMEHEIGHT:
				if (HIWORD(wParam) == EN_KILLFOCUS) {
					long new_yf;
					BOOL success;

					new_yf = GetDlgItemInt(hDlg, IDC_FRAMEHEIGHT, &success, FALSE);
					if (!success || new_yf < mfd->new_y) {
						SetFocus((HWND)lParam);
						MessageBeep(MB_ICONQUESTION);
						return TRUE;
					}

					mfd->ifp->UndoSystem();
					mfd->new_yf = new_yf;
					mfd->ifp->RedoSystem();
				}
				return TRUE;

			case IDC_PREVIEW:
				mfd->ifp->Toggle(hDlg);
				return TRUE;

			case IDC_FILTER_MODE:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					mfd->ifp->UndoSystem();
					mfd->filter_mode = SendDlgItemMessage(hDlg, IDC_FILTER_MODE, CB_GETCURSEL, 0, 0);
					mfd->ifp->RedoSystem();
				}
				return TRUE;

			case IDC_LETTERBOX:
				if (HIWORD(wParam) == BN_CLICKED) {
					BOOL f = IsDlgButtonChecked(hDlg, IDC_LETTERBOX);

					mfd->ifp->UndoSystem();
					mfd->fLetterbox = !!f;

					EnableWindow(GetDlgItem(hDlg, IDC_STATIC_FILLCOLOR), f);
					EnableWindow(GetDlgItem(hDlg, IDC_COLOR), f);
					EnableWindow(GetDlgItem(hDlg, IDC_PICKCOLOR), f);
					EnableWindow(GetDlgItem(hDlg, IDC_FRAMEWIDTH), f);
					EnableWindow(GetDlgItem(hDlg, IDC_FRAMEHEIGHT), f);

					if (mfd->fLetterbox) {
						if (mfd->new_xf < mfd->new_x) {
							mfd->new_xf = mfd->new_x;
							SetDlgItemInt(hDlg, IDC_FRAMEWIDTH, mfd->new_xf, FALSE);
						}

						if (mfd->new_yf < mfd->new_y) {
							mfd->new_yf = mfd->new_y;
							SetDlgItemInt(hDlg, IDC_FRAMEHEIGHT, mfd->new_yf, FALSE);
						}
					}
					mfd->ifp->RedoSystem();
				}
				return TRUE;

			case IDC_PICKCOLOR:
				if (guiChooseColor(hDlg, mfd->rgbColor)) {
					DeleteObject(mfd->hbrColor);
					mfd->hbrColor = CreateSolidBrush(mfd->rgbColor);
					RedrawWindow(GetDlgItem(hDlg, IDC_COLOR), NULL, NULL, RDW_ERASE|RDW_INVALIDATE|RDW_UPDATENOW);
				}
				break;
            }
            break;

		case WM_CTLCOLORSTATIC:
			if (GetWindowLong((HWND)lParam, GWL_ID) == IDC_COLOR)
				return (BOOL)mfd->hbrColor;
			break;
    }
    return FALSE;
}

static int resize_config(FilterActivation *fa, const FilterFunctions *ff, HWND hWnd) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	MyFilterData mfd2 = *mfd;
	int ret;

	mfd->hbrColor = NULL;
	mfd->ifp = fa->ifp;

	if (mfd->new_x < 16)
		mfd->new_x = 320;
	if (mfd->new_y < 16)
		mfd->new_y = 240;

	ret = DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_RESIZE), hWnd, resizeDlgProc, (LONG)mfd);

	if (mfd->hbrColor) {
		DeleteObject(mfd->hbrColor);
		mfd->hbrColor = NULL;
	}

	if (ret)
		*mfd = mfd2;

	return ret;
}

static void resize_string(const FilterActivation *fa, const FilterFunctions *ff, char *buf) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	if (mfd->fLetterbox)
		wsprintf(buf, " (%s, lbox %dx%d #%06x)", filter_names[mfd->filter_mode],
				mfd->new_xf, mfd->new_yf, revcolor(mfd->rgbColor));
	else
		wsprintf(buf, " (%s)", filter_names[mfd->filter_mode]);
}

// this is really stupid...

static int permute_index(int a, int b) {
	return (b-(a>>8)-1) + (a&255)*b;
}

static void normalize_table(int *table, int filtwidth) {
	int i, j, v, v2;

	for(i=0; i<256*filtwidth; i+=filtwidth) {
		v=0;
		v2=0;
		for(j=0; j<filtwidth; j++)
			v += table[i+j];

		for(j=0; j<filtwidth; j++)
			v2 += table[i+j] = MulDiv(table[i+j], 0x4000, v);

		v2 = 0x4000 - v2;

		if (MMX_enabled) {
			for(j=0; j<filtwidth; j+=2) {
				int a = table[i+j];
				int b = table[i+j+1];

				a = (a & 0xffff) | (b<<16);

				table[i+j+0] = a;
				table[i+j+1] = a;
			}
		}

		_RPT2(0,"table_error[%02x] = %04x\n", i, v2);
	}
}

static bool generate_cubic_table(MyFilterData *mfd) {
	if (!(mfd->cubic4_tbl = new int[256*4*2]))
		return false;

	MakeCubic4Table(mfd->cubic4_tbl, -0.75, false);
	MakeCubic4Table(mfd->cubic4_tbl + 1024, -0.75, true);

	return true;
}

static int resize_start(FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	long dstw = mfd->new_x;
	long dsth = mfd->new_y;

	if (dstw<16 || dsth<16)
		return 1;

	switch(mfd->filter_mode) {

	case FILTER_BICUBIC:

		if (!(mfd->cubic_rows = new Pixel[dstw * 4]))
			return 1;

		if (!generate_cubic_table(mfd))
			return 1;

		break;

	case FILTER_TABLEBILINEAR:
		{
			int i;
			long filtwidth_frac;
			double filtwidth_fracd;
			double filt_max;

			// 1:1 -> 2 input samples, max 16384
			// 2:1 -> 4 input samples, max 8192
			// 3:1 -> 6 input samples, max 5461

			if (fa->src.w > dstw) {

				filtwidth_fracd = fa->src.w*256.0/dstw;

				filtwidth_frac = (long)ceil(filtwidth_fracd);
				mfd->x_filtwidth = ((filtwidth_frac + 255) >> 8)<<1;

				_RPT1(0,"x_filtwidth: %d\n", mfd->x_filtwidth);

				if (!(mfd->x_table = new int[256 * mfd->x_filtwidth]))
					return 1;

				mfd->x_table[mfd->x_filtwidth-1] = 0;

				filt_max = (dstw*16384.0*16384.0)/fa->src.w;

				for(i=0; i<128*mfd->x_filtwidth; i++) {
					int y = 0;
					double d = i / filtwidth_fracd;

					if (d<1.0)
						y = (int)(0.5 + filt_max*(1.0 - d));

					mfd->x_table[permute_index(128*mfd->x_filtwidth + i, mfd->x_filtwidth)]
						= mfd->x_table[permute_index(128*mfd->x_filtwidth - i, mfd->x_filtwidth)]
						= y;
				}
			} else
				mfd->x_filtwidth = 2;



			if (fa->src.h > dsth) {
				filtwidth_fracd = fa->src.h*256.0/dsth;
				filtwidth_frac = (long)ceil(filtwidth_fracd);
//				filtwidth_frac = (fa->src.h*256 + (dsth-1))/dsth;
				mfd->y_filtwidth = ((filtwidth_frac + 255) >> 8)<<1;

				if (!(mfd->y_table = new int[256 * mfd->y_filtwidth]))
					return 1;

				mfd->y_table[mfd->y_filtwidth-1] = 0;

				filt_max = (dsth*16384.0*16384.0)/fa->src.h;

				for(i=0; i<128*mfd->y_filtwidth; i++) {
					int y = 0;
					double d = i / filtwidth_fracd;

					if (d<1.0)
						y = (int)(0.5 + filt_max*(1.0 - d));

//					if (i < filtwidth_frac)
//						y = (int)(0.5 + (filt_max*(filtwidth_frac - i))/filtwidth_frac);

					mfd->y_table[permute_index(128*mfd->y_filtwidth + i, mfd->y_filtwidth)]
						= mfd->y_table[permute_index(128*mfd->y_filtwidth - i, mfd->y_filtwidth)]
						= y;
				}

			} else
				mfd->y_filtwidth = 2;

			if (!(mfd->cubic_rows = new Pixel[((dstw+1)&~1) * mfd->y_filtwidth]))
				return 1;
			if (!(mfd->row_table = new Pixel *[2*mfd->y_filtwidth-1]))
				return 1;

			for(i=0; i<mfd->y_filtwidth; i++) {
				mfd->row_table[i] = mfd->cubic_rows + ((dstw+1)&~1) * i;
				if (i<mfd->y_filtwidth-1)
					mfd->row_table[i+mfd->y_filtwidth] = mfd->cubic_rows + ((dstw+1)&~1) * i;
			}
		}
		break;

	case FILTER_TABLEBICUBIC:
		{
			int i;
			long filtwidth_frac;
			double filt_max;

			// 1:1 -> 2 input samples, max 16384
			// 2:1 -> 4 input samples, max 8192
			// 3:1 -> 6 input samples, max 5461

			if (fa->src.w > dstw) {
				filtwidth_frac = (fa->src.w*256 + (dstw-1))/dstw;
				mfd->x_filtwidth = ((filtwidth_frac + 255) >> 8)<<2;

				_RPT1(0,"x_filtwidth: %d\n", mfd->x_filtwidth);

				if (!(mfd->x_table = new int[256 * mfd->x_filtwidth]))
					return 1;

				mfd->x_table[mfd->x_filtwidth-1] = 0;

				filt_max = (dstw*16384.0)/fa->src.w;

				for(i=0; i<128*mfd->x_filtwidth; i++) {
					int y = 0;
					double d = (double)i / (fa->src.w*256.0/dstw);

#define A (-0.75)
					if (d < 1.0)
						y = (int)floor(0.5 + (1.0 - (A+3.0)*d*d + (A+2.0)*d*d*d) * filt_max);
					else if (d < 2.0)
						y = (int)floor(0.5 + (-4.0*A + 8.0*A*d - 5.0*A*d*d + A*d*d*d) * filt_max);
#undef A

					mfd->x_table[permute_index(128*mfd->x_filtwidth + i, mfd->x_filtwidth)]
						= mfd->x_table[permute_index(128*mfd->x_filtwidth - i, mfd->x_filtwidth)]
						= y;
				}
			} else
				mfd->x_filtwidth = 4;


			if (fa->src.h > dsth) {

				filtwidth_frac = (fa->src.h*256 + (dsth-1))/dsth;
				mfd->y_filtwidth = ((filtwidth_frac + 255) >> 8)<<2;

				if (!(mfd->y_table = new int[256 * mfd->y_filtwidth]))
					return 1;

				mfd->y_table[mfd->y_filtwidth-1] = 0;

				filt_max = (dsth*16384.0)/fa->src.h;

				for(i=0; i<128*mfd->y_filtwidth; i++) {
					int y = 0;
					double d = (double)i / (fa->src.h*256.0/dsth);

#if 0
					if (d < 1.0)
						y = (int)floor(0.5 + (1.0 - 2.0*d*d + d*d*d) * filt_max);
					else if (d < 2.0)
						y = (int)floor(0.5 + (4.0 - 8.0*d + 5.0*d*d - d*d*d) * filt_max);
#else
#define A (-0.75)
					if (d < 1.0)
						y = (int)floor(0.5 + (1.0 - (A+3.0)*d*d + (A+2.0)*d*d*d) * filt_max);
					else if (d < 2.0)
						y = (int)floor(0.5 + (-4.0*A + 8.0*A*d - 5.0*A*d*d + A*d*d*d) * filt_max);
#undef A
					
#endif

					mfd->y_table[permute_index(128*mfd->y_filtwidth + i, mfd->y_filtwidth)]
						= mfd->y_table[permute_index(128*mfd->y_filtwidth - i, mfd->y_filtwidth)]
						= y;
				}
			} else
				mfd->y_filtwidth = 4;

			if (!(mfd->cubic_rows = new Pixel[((dstw+1)&~1) * mfd->y_filtwidth]))
				return 1;
			if (!(mfd->row_table = new Pixel *[2*mfd->y_filtwidth-1]))
				return 1;

			if (fa->src.h <= dsth || fa->src.w <= dstw)
				if (!generate_cubic_table(mfd))
					return 1;

			for(i=0; i<mfd->y_filtwidth; i++) {
				mfd->row_table[i] = mfd->cubic_rows + ((dstw+1)&~1) * i;
				if (i<mfd->y_filtwidth-1)
					mfd->row_table[i+mfd->y_filtwidth] = mfd->cubic_rows + ((dstw+1)&~1) * i;
			}

		}
		break;
	}

	// Normalize tables and reduce from 28-bit to 14-bit.

	if (mfd->x_table)
		normalize_table(mfd->x_table, mfd->x_filtwidth);

	if (mfd->y_table)
		normalize_table(mfd->y_table, mfd->y_filtwidth);

	return 0;
}

static int resize_stop(FilterActivation *fa, const FilterFunctions *ff) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	delete[] mfd->cubic_rows;	mfd->cubic_rows = NULL;
	delete[] mfd->x_table;		mfd->x_table = NULL;
	delete[] mfd->y_table;		mfd->y_table = NULL;
	delete[] mfd->row_table;	mfd->row_table = NULL;
	delete[] mfd->cubic4_tbl;	mfd->cubic4_tbl = NULL;

	return 0;
}

static void resize_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;

	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	mfd->new_x	= argv[0].asInt();
	mfd->new_y	= argv[1].asInt();

	if (argv[2].isInt())
		mfd->filter_mode = argv[2].asInt();
	else {
		char *s = *argv[2].asString();

		if (!stricmp(s, "point") || !stricmp(s, "nearest"))
			mfd->filter_mode = 0;
		else if (!stricmp(s, "bilinear"))
			mfd->filter_mode = 1;
		else if (!stricmp(s, "bicubic"))
			mfd->filter_mode = 2;
		else
			EXT_SCRIPT_ERROR(FCALL_UNKNOWN_STR);
	}

	mfd->fLetterbox = false;

	if (argc > 3) {
		mfd->new_xf = argv[3].asInt();
		mfd->new_yf = argv[4].asInt();
		mfd->fLetterbox = true;
		mfd->rgbColor = revcolor(argv[5].asInt());
	}
}

static ScriptFunctionDef resize_func_defs[]={
	{ (ScriptFunctionPtr)resize_script_config, "Config", "0iii" },
	{ (ScriptFunctionPtr)resize_script_config, NULL, "0iis" },
	{ (ScriptFunctionPtr)resize_script_config, NULL, "0iiiiii" },
	{ (ScriptFunctionPtr)resize_script_config, NULL, "0iisiii" },
	{ NULL },
};

static CScriptObject resize_obj={
	NULL, resize_func_defs
};

static bool resize_script_line(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	if (mfd->fLetterbox)
		_snprintf(buf, buflen, "Config(%d,%d,%d,%d,%d,0x%06x)", mfd->new_x, mfd->new_y, mfd->filter_mode, mfd->new_xf, mfd->new_yf,
			revcolor(mfd->rgbColor));
	else
		_snprintf(buf, buflen, "Config(%d,%d,%d)", mfd->new_x, mfd->new_y, mfd->filter_mode);

	return true;
}

FilterDefinition filterDef_resize={
	0,0,NULL,
	"resize",
	"Resizes the image to a new size."
#ifdef USE_ASM
			"\n\n[Assembly optimized] [FPU optimized] [MMX optimized]"
#endif
			,
	NULL,NULL,
	sizeof(MyFilterData),
	NULL,NULL,
	resize_run,
	resize_param,
	resize_config,
	resize_string,
	resize_start,
	resize_stop,

	&resize_obj,
	resize_script_line,
};