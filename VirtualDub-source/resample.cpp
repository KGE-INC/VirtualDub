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

#include <crtdbg.h>

#include <math.h>

#include "resample.h"
#include "VBitmap.h"
#include "cpuaccel.h"

///////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////

extern "C" void __cdecl asm_resize_nearest(
		Pixel32 *dst,
		const Pixel32 *src,
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
		const Pixel32 *precopysrc,
		unsigned long precopy,
		const Pixel32 *postcopysrc,
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

extern "C" void asm_resize_interp_row_run(
			void *dst,
			const void *src,
			unsigned long width,
			__int64 xaccum,
			__int64 x_inc);

extern "C" void asm_resize_interp_col_run(
			void *dst,
			const void *src1,
			const void *src2,
			unsigned long width,
			unsigned long yaccum);

extern "C" void asm_resize_ccint(Pixel *dst, const Pixel *src1, const Pixel *src2, const Pixel *src3, const Pixel *src4, long count, long xaccum, long xint, const int *table);
extern "C" void asm_resize_ccint_col(Pixel *dst, const Pixel *src1, const Pixel *src2, const Pixel *src3, const Pixel *src4, long count, const int *table);
extern "C" void asm_resize_ccint_col_MMX(Pixel *dst, const Pixel *src1, const Pixel *src2, const Pixel *src3, const Pixel *src4, long count, const int *table);
extern "C" void asm_resize_ccint_col_SSE2(Pixel *dst, const Pixel *src1, const Pixel *src2, const Pixel *src3, const Pixel *src4, long count, const int *table);

extern "C" long resize_table_col_MMX(Pixel *out, const Pixel *const*in_table, const int *filter, int filter_width, PixDim w, long frac);
extern "C" long resize_table_col_by2linear_MMX(Pixel *out, const Pixel *const*in_table, PixDim w);
extern "C" long resize_table_col_by2cubic_MMX(Pixel *out, const Pixel *const*in_table, PixDim w);


///////////////////////////////////////////////////////////////////////////

//	void MakeCubic4Table(
//		int *table,			pointer to 256x4 int array
//		double A,			'A' value - determines characteristics
//		mmx_table);			generate interleaved table
//
//	Generates a table suitable for cubic 4-point interpolation.
//
//	Each 4-int entry is a set of four coefficients for a point
//	(n/256) past y[1].  They are in /16384 units.
//
//	A = -1.0 is the original VirtualDub bicubic filter, but it tends
//	to oversharpen video, especially on rotates.  Use A = -0.75
//	for a filter that resembles Photoshop's.


void MakeCubic4Table(int *table, double A, bool mmx_table) throw() {
	int i;

	for(i=0; i<256; i++) {
		double d = (double)i / 256.0;
		int y1, y2, y3, y4, ydiff;

		// Coefficients for all four pixels *must* add up to 1.0 for
		// consistent unity gain.
		//
		// Two good values for A are -1.0 (original VirtualDub bicubic filter)
		// and -0.75 (closely matches Photoshop).

		y1 = (int)floor(0.5 + (        +     A*d -       2.0*A*d*d +       A*d*d*d) * 16384.0);
		y2 = (int)floor(0.5 + (+ 1.0             -     (A+3.0)*d*d + (A+2.0)*d*d*d) * 16384.0);
		y3 = (int)floor(0.5 + (        -     A*d + (2.0*A+3.0)*d*d - (A+2.0)*d*d*d) * 16384.0);
		y4 = (int)floor(0.5 + (                  +           A*d*d -       A*d*d*d) * 16384.0);

		// Normalize y's so they add up to 16384.

		ydiff = (16384 - y1 - y2 - y3 - y4)/4;
		_ASSERT(ydiff > -16 && ydiff < 16);

		y1 += ydiff;
		y2 += ydiff;
		y3 += ydiff;
		y4 += ydiff;

		if (mmx_table) {
			table[i*4 + 0] = table[i*4 + 1] = (y2<<16) | (y1 & 0xffff);
			table[i*4 + 2] = table[i*4 + 3] = (y4<<16) | (y3 & 0xffff);
		} else {
			table[i*4 + 0] = y1;
			table[i*4 + 1] = y2;
			table[i*4 + 2] = y3;
			table[i*4 + 3] = y4;
		}
	}
}

const int *GetStandardCubic4Table() throw() {
	static int cubic4_tbl[256*4*2];

	if (!cubic4_tbl[1]) {
		MakeCubic4Table(cubic4_tbl, -0.75, false);
		MakeCubic4Table(cubic4_tbl+1024, -0.75, true);
	}

	return cubic4_tbl;
}

const int *GetBetterCubic4Table() throw() {
	static int cubic4_tbl[256*4*2];

	if (!cubic4_tbl[1]) {
		MakeCubic4Table(cubic4_tbl, -0.6, false);
		MakeCubic4Table(cubic4_tbl+1024, -0.6, true);
	}

	return cubic4_tbl;
}

///////////////////////////////////////////////////////////////////////////

void ResampleInfo::computeBounds(__int64 u, __int64 dudx, unsigned int dx, unsigned int kernel, unsigned long limit) {

	// The kernel length must always be even.  The precopy region covers all
	// pixels where the kernel is completely off to the left; this occurs when
	// u < -(kernel/2-1).
	//
	// We round the halfkernel value so that things work out for point-sampling
	// kernels (k=1).

	__int64 halfkernel = ((kernel+1)>>1);
	__int64 halfkernelm1 = halfkernel-1;

	if (u < (-halfkernelm1<<32)) {
		clip.precopy = ((-halfkernelm1<<32) - u + (dudx-1)) / dudx;
		if (clip.precopy > dx)
			clip.precopy = dx;
	} else
		clip.precopy = 0;

	// Preclip region occurs anytime the left side of the kernel is off the
	// border.  The kernel extends w/2-1 off the left, so that's how far we
	// need to step....
	//
	// Preclip regions don't occur with point sampling.

	if (kernel>1 && u < (halfkernelm1<<32)) {
		clip.preclip = ((halfkernelm1<<32) - u + (dudx-1)) / dudx;
		if (clip.preclip > dx)
			clip.preclip = dx;
	
		clip.preclip -= clip.precopy;
	} else
		clip.preclip = 0;

	// Postcopy region occurs if we step onto or past (limit + kernel/2 - 2).

	__int64 dulimit = u + dudx * (dx-1);

	if ((long)(dulimit>>32) >= (long)(limit+(long)(halfkernelm1-1))) {
		clip.postcopy = dx - ((((limit + halfkernelm1 - 1)<<32) - u - 1) / dudx + 1);

		if (clip.postcopy > dx)
			clip.postcopy = dx;
	} else
		clip.postcopy = 0;

	// Postclip region occurs if we step onto or past (limit - kernel/2 + 1).
	//
	// Postclip regions don't occur with point sampling.

	if (kernel>1 && (long)(dulimit>>32) >= (long)(limit-(long)halfkernelm1)) {
		clip.postclip = dx - ((((limit - halfkernelm1)<<32) - u - 1) / dudx + 1);

		if (clip.postclip > dx)
			clip.postclip = dx;

		clip.postclip -= clip.postcopy;
	} else
		clip.postclip = 0;

	clip.unclipped = dx - (clip.precopy + clip.preclip + clip.postcopy + clip.postclip);

	clip.allclip = 0;
	if (clip.unclipped < 0) {
		clip.allclip = dx - (clip.precopy + clip.postcopy);
		clip.postclip = clip.preclip = 0;
		clip.unclipped = 0;
	}

	clip.preclip2 = clip.postclip2 = 0;
}

void ResampleInfo::computeBounds4(__int64 u, __int64 dudx, unsigned int dx, unsigned long limit) {
	// The kernel length must always be even.  The precopy region covers all
	// pixels where the kernel is completely off to the left; this occurs when
	// u < -(kernel/2-1).
	//
	// We round the halfkernel value so that things work out for point-sampling
	// kernels (k=1).

	const __int64 halfkernel = 2;
	const __int64 halfkernelm1 = 1;

	if (u < (-halfkernelm1<<32))
		clip.precopy = ((-halfkernelm1<<32) - u + (dudx-1)) / dudx;
	else
		clip.precopy = 0;

	// Preclip region occurs anytime the left side of the kernel is off the
	// border.  The kernel extends 1 off the left, so that's how far we
	// need to step....

	// Preclip2: [a b|c d]		pass: u >= 0.0

	if (u < (halfkernelm1<<32))
		clip.preclip2 = (-u + (dudx-1)) / dudx - clip.precopy;
	else
		clip.preclip2 = 0;

	// Preclip: [a|b c d]		pass: u >= 1.0

	if (u < (halfkernelm1<<32))
		clip.preclip = ((halfkernelm1<<32) - u + (dudx-1)) / dudx - (clip.precopy+clip.preclip2);
	else
		clip.preclip = 0;

	// Postcopy region occurs if we step onto or past (limit).

	__int64 dulimit = u + dudx * (dx-1);

	if ((long)(dulimit>>32) >= (long)limit)
		clip.postcopy = dx - ((((__int64)limit<<32) - u - 1) / dudx + 1);
	else
		clip.postcopy = 0;

	// Postclip2: [a b|c d]		pass: u < limit-1

	if ((long)(dulimit>>32) >= (long)(limit-(long)halfkernelm1))
		clip.postclip2 = dx - clip.postcopy - ((((__int64)(limit-1)<<32) - u - 1) / dudx + 1);
	else
		clip.postclip2 = 0;

	// Postclip: [a b c|d]		pass: u < limit-2

	if ((long)(dulimit>>32) >= (long)(limit-(long)halfkernelm1-1))
		clip.postclip = dx - clip.postcopy - clip.postclip2 - ((((__int64)(limit-2)<<32) - u - 1) / dudx + 1);
	else
		clip.postclip = 0;

	clip.unclipped = dx - (clip.precopy + clip.preclip2 + clip.preclip + clip.postcopy + clip.postclip + clip.postclip2);

	clip.allclip = 0;
	if (clip.unclipped < 0) {
		clip.allclip = dx - (clip.precopy + clip.postcopy);

		clip.preclip = clip.preclip2 = clip.postclip2 = clip.postclip = 0;
		clip.unclipped = 0;
	}
}

bool ResampleInfo::init(double x, double dx, double u, double du, unsigned long xlimit, unsigned long ulimit, int kw, bool bMapCorners, bool bClip4) {
	
	// Undo any destination flips.
	
	if (dx < 0) {
		x += dx;
		dx = -dx;
	}
	
	// Precondition: destination area must not be empty.
	// Compute slopes.

   double dudx;
	
   if (bMapCorners) {
	   if (dx <= 1.0)
		   return false;
		   
      dudx = (du > 1.0 ? du - 1.0 : du < 1.0 ? du + 1.0 : 0.0)  / (dx-1.0);
   } else {
	   if (dx <= 0.0)
		   return false;
		   
	   dudx = du / dx;
	
	   // Prestep destination pixels so that we step on pixel centers.  We're going
	   // to be using Direct3D's screen-space system, where pixels sit on integer
	   // coordinates and the fill-convention is top-left.  However, OpenGL's system
	   // is easier to use from the client point of view, so we'll subtract 0.5 from
	   // all coordinates to compensate.  This means that a 1:1 blit of an 8x8
	   // rectangle should be (0,0)-(8,8) in both screen and texture space.

	   x -= 0.5;
	   u -= 0.5;
   }

	// Compute integer destination rectangle.

	x1_int = ceil(x);
	dx_int = ceil(x + dx) - x;

	// Clip destination rectangle.

	if (x1_int<0) {
		dx_int -= x1_int;
		x1_int = 0;
	}

	if (x1_int+dx_int > xlimit)
		dx_int = xlimit - x1_int;

	if (dx_int<=0)
		return false;

	// Prestep source.

	double prestep;

	prestep = (x1_int - x) * dudx;
	u += prestep;
	du -= prestep;

	// Compute integer step values.  Rounding toward zero is usually a pretty
	// safe bet.

	dudx_int.v = (__int64)(4294967296.0 * dudx);

	// Compute starting sampling coordinate.

	u0_int.v = (__int64)((u + u + dudx - 1.0)*2147483648.0);

	// Compute clipping parameters.

	if (bClip4)
		computeBounds4(u0_int.v, dudx_int.v, dx_int, ulimit);
	else
		computeBounds(u0_int.v, dudx_int.v, dx_int, kw, ulimit);

	// Advance source to beginning of clipped region.

	u0_int.v += dudx_int.v * clip.precopy;

	return true;
}

extern "C" long resize_table_row_MMX(Pixel *out, const Pixel *in, const int *filter, int filter_width, PixDim w, long accum, long frac);
extern "C" long resize_table_row_protected_MMX(Pixel *out, const Pixel *in, const int *filter, int filter_width, PixDim w, long accum, long frac, long limit);
extern "C" long resize_table_row_by2linear_MMX(Pixel *out, const Pixel *in, PixDim w);
extern "C" long resize_table_row_by2cubic_MMX(Pixel *out, const Pixel *in, PixDim w, unsigned long accum, unsigned long fstep, unsigned long istep);

void resize_table_row(Pixel *out, const Pixel *in, const int *filter, int filter_width, PixDim w, PixDim w_left, PixDim w_right, PixDim w_all, PixDim w2, long accum, long frac, int accel_code) {
	const Pixel *in0 = in;

	in -= filter_width/2 - 1;

	if (MMX_enabled) {
		if (w_all > 0) {
			accum = resize_table_row_protected_MMX(out, in0, filter, filter_width, w_all, accum - ((filter_width/2-1)<<16), frac, w2-1) + ((filter_width/2-1)<<16);
			out += w_all;
			return;
		}

		if (w_left > 0) {
			accum = resize_table_row_protected_MMX(out, in0, filter, filter_width, w_left, accum - ((filter_width/2-1)<<16), frac, w2-1) + ((filter_width/2-1)<<16);
			out += w_left;
		}

		if (w > 0) {
			PROFILE_START

/*			if (accel_code == ACCEL_BICUBICBY2) {
				resize_table_row_by2cubic_MMX(out, in + (accum>>16), w, accum<<16, frac<<16, frac>>16);

				accum += frac*w;
			} else if (accel_code == ACCEL_BILINEARBY2) {
				resize_table_row_by2linear_MMX(out, in + (accum>>16), w);

				accum += frac*w;
			} else*/
				accum = resize_table_row_MMX(out, in, filter, filter_width, w, accum, frac);

			PROFILE_ADD(row)

			out += w;
		}

		if (w_right > 0)
			resize_table_row_protected_MMX(out, in0, filter, filter_width, w_right, accum - ((filter_width/2-1)<<16), frac, w2-1);
	} else {
		const Pixel *in_bottom, *in_top;
		in_bottom = in0;
		in_top = in0 + w2;

		if (w_all > 0) {
			do {
				int x, r, g, b;
				const Pixel *in2;
				const int *filter2;

				x = filter_width;
				in2 = in + (accum>>16);
				filter2 = filter + ((accum>>8) & 255)*filter_width;
				r = g = b = 0;

				do {
					Pixel c;
					int a;

					if (in2 < in_bottom)
						c = *in_bottom;
					else if (in >= in_top)
						c =  in_top[-1];
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

			} while(--w_all);

			return;
		}

		if (w_left > 0)
			do {
				int x, r, g, b;
				const Pixel *in2;
				const int *filter2;

				x = filter_width;
				in2 = in + (accum>>16);
				filter2 = filter + ((accum>>8) & 255)*filter_width;
				r = g = b = 0;

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

		if (w > 0)
			do {
				int x, r, g, b;
				const Pixel *in2;
				const int *filter2;

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

		if (w_right > 0) do {
			int x, r, g, b;
			const Pixel *in2;
			const int *filter2;

			x = filter_width;
			in2 = in + (accum>>16);
			filter2 = filter + ((accum>>8) & 255)*filter_width;
			r = g = b = 0;

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
}

///////////////////////////////////////////////////////////////////////////

void resize_table_col(Pixel *out, const Pixel *const*in_rows, int *filter, int filter_width, PixDim w, long frac, int accel_code) {
	int x;

	if (MMX_enabled) {
		PROFILE_START

/*		if (accel_code == ACCEL_BICUBICBY2)
			resize_table_col_by2cubic_MMX(out, in_rows, w);
		else if (accel_code == ACCEL_BILINEARBY2)
			resize_table_col_by2linear_MMX(out, in_rows, w);
		else*/
			resize_table_col_MMX(out, in_rows, filter, filter_width, w, frac);

		PROFILE_ADD(column)

		return;
	}

	x = 0;
	do {
		int x2, r, g, b;
		const Pixel *const *in_row;
		const int *filter2;

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

#undef RED
#undef GRN
#undef BLU

void cc_row(Pixel *dst, const Pixel *src, long w, long xs_left2, long xs_left, long xs_right, long xs_right2, long xaccum, long xinc, const int *table) {

	src += xaccum>>16;
	xaccum&=0xffff;

	if (xs_left2) {
		Pixel x[4] = { src[1], src[1], src[1], src[2] };

		do {
			*dst++ = cc(x, table + 4*((xaccum>>8)&0xff));

			xaccum += xinc;
			src += xaccum>>16;
			xaccum&=0xffff;
		} while(--xs_left2);
	}

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

	if (xs_right2) do {
		Pixel x[4] = { src[-1], src[0], src[0], src[0] };

		*dst++ = cc(x, table + 4*((xaccum>>8)&0xff));

		xaccum += xinc;
		src += xaccum>>16;
		xaccum&=0xffff;
	} while(--xs_right2);
}

void cc_row_protected(Pixel *dst, const Pixel *src_low, const Pixel *src_high, long w, long xaccum, long xinc, const int *table) {
	const Pixel32 *src = src_low + (xaccum>>16);

	xaccum&=0xffff;

	if (w) {
		do {
			Pixel32 x[4];

			if (src-1 < src_low)
				x[0] = src_low[0];
			else if (src-1 >= src_high)
				x[0] = src_high[-1];
			else
				x[0] = src[-1];

			if (src < src_low)
				x[1] = src_low[0];
			else if (src >= src_high)
				x[1] = src_high[-1];
			else
				x[1] = src[0];

			if (src+1 < src_low)
				x[2] = src_low[0];
			else if (src+1 >= src_high)
				x[2] = src_high[-1];
			else
				x[2] = src[+1];

			if (src+2 < src_low)
				x[3] = src_low[0];
			else if (src+2 >= src_high)
				x[3] = src_high[-1];
			else
				x[3] = src[+2];

			*dst++ = cc(x, table + 4*((xaccum>>8)&0xff));

			xaccum += xinc;
			src += xaccum>>16;
			xaccum&=0xffff;
		} while(--w);
	}

}
///////////////////////////////////////////////////////////////////////////

Resampler::Resampler()
	: xtable(0)
	, ytable(0)
	, rows(0)
	, rowmem(0)
	, rowmemalloc(0)
{
}

Resampler::~Resampler() {
	Free();
}

void Resampler::Init(eFilter horiz_filt, eFilter vert_filt, double dx, double dy, double sx, double sy) {

	// Delete any previous allocations.

	Free();

	this->srcw = sx;
	this->srch = sy;
	this->dstw = dx;
	this->dsth = dy;

	// Compute number of rows we need to store.
	//
	// point:			none.
	// linearinterp:	2 if horiz != linearinterp
	// cubicinterp:		4
	// lineardecimate:	variable
	// cubicdecimate:	variable
	//
	// Create tables and get interpolation values.

	rowcount = 0;
	ubias = vbias = 1.0 / 512.0;

	switch(horiz_filt) {
	case eFilter::kPoint:
		xfiltwidth = 1;
		ubias = 1.0 / 2.0;
		break;
	case eFilter::kLinearDecimate:
		if (sx > dx) {
			xtable = _CreateLinearDecimateTable(dx, sx, xfiltwidth);
			break;
		}
		horiz_filt = eFilter::kLinearInterp;
	case eFilter::kLinearInterp:
		xfiltwidth = 2;
		break;
	case eFilter::kCubicDecimate060:
	case eFilter::kCubicDecimate075:
	case eFilter::kCubicDecimate100:
		if (sx > dx) {
			switch(horiz_filt) {
			case eFilter::kCubicDecimate060:
				xtable = _CreateCubicDecimateTable(dx, sx, xfiltwidth, -0.60);
				break;
			case eFilter::kCubicDecimate075:
				xtable = _CreateCubicDecimateTable(dx, sx, xfiltwidth, -0.75);
				break;
			case eFilter::kCubicDecimate100:
				xtable = _CreateCubicDecimateTable(dx, sx, xfiltwidth, -1.00);
				break;
			}
			break;
		}
		horiz_filt = eFilter::kCubicInterp060;
	case eFilter::kCubicInterp060:
		GetBetterCubic4Table();
		xfiltwidth = 4;
		break;
	case eFilter::kCubicInterp:
		GetStandardCubic4Table();
		xfiltwidth = 4;
		break;
	case eFilter::kLanzcos3:
		xtable = _CreateLanzcos3DecimateTable(dx, sx, xfiltwidth);
		rowcount = xfiltwidth;
		break;
	}

	switch(vert_filt) {
	case eFilter::kPoint:
		if (horiz_filt != eFilter::kPoint)
			rowcount = 1;
		yfiltwidth = 1;
		vbias = 1.0 / 2.0;
		break;
	case eFilter::kLinearDecimate:
		if (sy > dy) {
			ytable = _CreateLinearDecimateTable(dy, sy, yfiltwidth);
			rowcount = yfiltwidth;
			break;
		}
		vert_filt = eFilter::kLinearInterp;
	case eFilter::kLinearInterp:
		if (horiz_filt != eFilter::kLinearInterp)
			rowcount = 2;
		else
			vbias = 1.0/32.0;
		yfiltwidth = 2;
		break;
	case eFilter::kCubicDecimate060:
	case eFilter::kCubicDecimate075:
	case eFilter::kCubicDecimate100:
		if (sy > dy) {
			switch(vert_filt) {
			case eFilter::kCubicDecimate060:
				ytable = _CreateCubicDecimateTable(dy, sy, yfiltwidth, -0.60);
				break;
			case eFilter::kCubicDecimate075:
				ytable = _CreateCubicDecimateTable(dy, sy, yfiltwidth, -0.75);
				break;
			case eFilter::kCubicDecimate100:
				ytable = _CreateCubicDecimateTable(dy, sy, yfiltwidth, -1.00);
				break;
			}
			rowcount = yfiltwidth;
			break;
		}
		vert_filt = eFilter::kCubicInterp060;
	case eFilter::kCubicInterp060:
		GetBetterCubic4Table();
		yfiltwidth = 4;
		rowcount = 4;
		break;
	case eFilter::kCubicInterp:
		GetStandardCubic4Table();
		yfiltwidth = 4;
		rowcount = 4;
		break;
	case eFilter::kLanzcos3:
		ytable = _CreateLanzcos3DecimateTable(dy, sy, yfiltwidth);
		rowcount = yfiltwidth;
		break;
	}

	// Allocate the rows.

	if (rowcount) {
		rowpitch = ((int)ceil(dx)+1)&~1;

		rowmemalloc = new Pixel32[rowpitch * rowcount+2];
		rowmem = (Pixel32 *)(((long)rowmemalloc+7)&~7);
		rows = new Pixel32*[rowcount * 2];
	}

	this->nHorizFilt = horiz_filt;
	this->nVertFilt = vert_filt;
}

void Resampler::Free() {
	delete[] xtable;
	delete[] ytable;
	delete[] rowmemalloc;
	delete[] rows;

	xtable = ytable = NULL;
	rowmemalloc = NULL;
	rows = NULL;
}

void Resampler::_DoRow(Pixel32 *dstp, const Pixel32 *srcp, long srcw) {
	int x;

	for(x=0; x < horiz.clip.precopy; ++x)
		dstp[x] = srcp[0];

	if (xtable)
		resize_table_row(
			dstp + horiz.clip.precopy,
			srcp,
			xtable,
			xfiltwidth,
			horiz.clip.unclipped,
			horiz.clip.preclip,
			horiz.clip.postclip,
			horiz.clip.allclip,
			srcw,
			(long)(horiz.u0_int.v >> 16),
			(long)(horiz.dudx_int.v >> 16),
			0);
	else switch(nHorizFilt) {
		case eFilter::kPoint:
			asm_resize_nearest(
					dstp + horiz.clip.precopy + horiz.clip.unclipped,			// destination pointer, right side
					srcp + horiz.u0_int.hi,
					-horiz.clip.unclipped*4,		// -width*4
					1,								// height
					0,								// dstpitch
					0,								// srcpitch
					horiz.u0_int.lo,				// xaccum
					0,								// yaccum
					horiz.dudx_int.lo,				// xfrac
					0,								// yfrac
					horiz.dudx_int.hi,				// xinc
					0,								// yinc
					srcp,
					horiz.clip.precopy,				// precopy
					srcp + srcw - 1,
					horiz.clip.postcopy				// postcopy
					);
			break;

		case eFilter::kLinearInterp:
			asm_resize_interp_row_run(
				dstp + horiz.clip.precopy,
				srcp,
				horiz.clip.unclipped,
				horiz.u0_int.v,
				horiz.dudx_int.v);
			break;

		case eFilter::kCubicInterp:
			if (horiz.clip.allclip)
				cc_row_protected(dstp + horiz.clip.precopy,
					srcp, srcp+srcw,
					horiz.clip.allclip,
					(long)(horiz.u0_int.v >> 16),
					(long)(horiz.dudx_int.v >> 16),
					GetStandardCubic4Table());
			else
				cc_row(dstp + horiz.clip.precopy,
					srcp,
					horiz.clip.unclipped,
					horiz.clip.preclip2,
					horiz.clip.preclip,
					horiz.clip.postclip,
					horiz.clip.postclip2,
					(long)(horiz.u0_int.v >> 16),
					(long)(horiz.dudx_int.v >> 16),
					GetStandardCubic4Table());
			break;
		case eFilter::kCubicInterp060:
			if (horiz.clip.allclip)
				cc_row_protected(dstp + horiz.clip.precopy,
					srcp, srcp+srcw,
					horiz.clip.allclip,
					(long)(horiz.u0_int.v >> 16),
					(long)(horiz.dudx_int.v >> 16),
					GetBetterCubic4Table());
			else
				cc_row(dstp + horiz.clip.precopy,
					srcp,
					horiz.clip.unclipped,
					horiz.clip.preclip2,
					horiz.clip.preclip,
					horiz.clip.postclip,
					horiz.clip.postclip2,
					(long)(horiz.u0_int.v >> 16),
					(long)(horiz.dudx_int.v >> 16),
					GetBetterCubic4Table());
			break;
	};

	for(x=0; x < horiz.clip.postcopy; ++x)
		dstp[horiz.dx_int - horiz.clip.postcopy + x] = srcp[srcw-1];
}


bool Resampler::Process(const VBitmap *dst, double _x2, double _y2, const VBitmap *src, double _x1, double _y1, bool bMapCorners) throw() {

	// No format conversions!!

	if (src->depth != dst->depth)
		return false;

	// Right now, only do 32-bit stretch.  (24-bit is a pain, 16-bit is slow.)

	if (dst->depth != 32)
		return false;

	// Compute clipping parameters.

	if (!horiz.init(_x2, dstw, _x1 + ubias, srcw, dst->w, src->w, xfiltwidth, bMapCorners, nHorizFilt == eFilter::kCubicInterp || nHorizFilt == eFilter::kCubicInterp060))
		return false;

	if (!vert.init(_y2, dsth, _y1 + vbias, srch, dst->h, src->h, yfiltwidth, bMapCorners, false))
		return false;

	long dx = horiz.dx_int;
	long dy = vert.dx_int;

	// Call texturing routine.

	Pixel32 *dstp = dst->Address32(horiz.x1_int, vert.x1_int);
	Pixel32 *srcp;

	int xprecopy	= horiz.clip.precopy;
	int xpreclip2	= horiz.clip.preclip2;
	int xpreclip1	= horiz.clip.preclip;
	int xunclipped	= horiz.clip.unclipped;
	int xpostclip1	= horiz.clip.postclip;
	int xpostclip2	= horiz.clip.postclip2;
	int xpostcopy	= horiz.clip.postcopy;
	int xrightborder= xprecopy+xpreclip2+xpreclip1+xunclipped+xpostclip1+xpostclip2;

	int yprecopy	= vert.clip.precopy;
	int yinterp		= vert.clip.preclip + vert.clip.unclipped + vert.clip.postclip + vert.clip.allclip;
	int ypostcopy	= vert.clip.postcopy;

	__int64	xaccum	= horiz.u0_int.v;
	__int64	xinc	= horiz.dudx_int.v;
	__int64 yaccum	= vert.u0_int.v;
	__int64 yinc	= vert.dudx_int.v;
	int rowlastline = -1;
	int i, y;

	if (vert.clip.precopy || vert.clip.preclip || vert.clip.allclip) {
		srcp = src->Address32(0, 0);

		if (rows) {
			_DoRow(rowmem, srcp, src->w);

			for(i=0; i<rowcount*2; ++i)
				rows[i] = rowmem;

			rowlastline = 0;

			if (vert.clip.precopy) {
				for(y=0; y<yprecopy; ++y) {
					memcpy(dstp, rowmem, 4*dx);

					dstp = (Pixel32 *)((char *)dstp - dst->pitch);
				}
			}
		} else {
			Pixel32 *dstp0 = dstp;

			_DoRow(dstp0, srcp, src->w);

			dstp = (Pixel32 *)((char *)dstp - dst->pitch);

			if (vert.clip.precopy) {
				for(y=1; y<yprecopy; ++y) {
					memcpy(dstp, dstp0, 4*dx);

					dstp = (Pixel32 *)((char *)dstp - dst->pitch);
				}
			}
		}
	}

	if (nHorizFilt == nVertFilt && nHorizFilt == eFilter::kPoint) {
		asm_resize_nearest(
				dst->Address32(horiz.x1_int + horiz.clip.precopy + horiz.clip.unclipped, vert.x1_int + vert.clip.precopy),			// destination pointer, right side
				src->Address32(horiz.u0_int.hi, vert.u0_int.hi),
				-horiz.clip.unclipped*4,		// -width*4
				vert.clip.unclipped,			// height
				-dst->pitch,					// dstpitch
				-src->pitch,					// srcpitch
				horiz.u0_int.lo,				// xaccum
				vert.u0_int.lo,					// yaccum
				horiz.dudx_int.lo,				// xfrac
				vert.dudx_int.lo,				// yfrac
				horiz.dudx_int.hi,				// xinc
				-vert.dudx_int.hi * src->pitch,	// yinc
				src->Address32(0, vert.u0_int.hi),
				xprecopy,						// precopy
				src->Address32(src->w-1, vert.u0_int.hi),
				xpostcopy						// postcopy
				);

		dstp = (Pixel32 *)((char *)dstp - dst->pitch * yinterp);
	} else if (nHorizFilt == nVertFilt && nHorizFilt == eFilter::kLinearInterp) {
		asm_resize_bilinear(
				dst->Address32(horiz.x1_int + xprecopy + horiz.clip.unclipped, vert.x1_int + yprecopy),			// destination pointer, right side
				src->Address32(horiz.u0_int.hi, vert.u0_int.hi),
				-horiz.clip.unclipped*4,		// -width*4
				vert.clip.unclipped,			// height
				-dst->pitch,					// dstpitch
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

		dstp = (Pixel32 *)((char *)dstp - dst->pitch * yinterp);
	} else
		for(y=0; y<yinterp; ++y) {
			long lastline = (long)(yaccum >> 32) + rowcount/2;

			if (rowlastline < lastline) {
				int delta;
				int pos;

				if (rowlastline < lastline-rowcount)
					rowlastline = lastline-rowcount;

				delta = rowlastline - lastline;

				srcp = src->Address32(0, rowlastline+1);

				pos = rowlastline % rowcount;

				do {
					Pixel32 *row;

					++rowlastline;
					if (++pos >= rowcount)
						pos = 0;

					if (rowlastline >= src->h) {
						rows[pos] = rows[pos+rowcount] = rows[(pos+rowcount-1)%rowcount];
						continue;
					}

					rows[pos] = rows[pos+rowcount] = rowmem + rowpitch * pos;
					row = rows[pos];

					_DoRow(row, srcp, src->w);

					srcp = (Pixel32 *)((char *)srcp - src->pitch);
				} while(++delta);

				rowlastline = lastline;
			}

			int pos = (rowlastline+1) % rowcount;

			if (ytable)
				resize_table_col(dstp, rows+pos, ytable, yfiltwidth, dx, ((unsigned long)yaccum>>24), 0);
			else switch(nVertFilt) {
				case eFilter::kPoint:
					memcpy(dstp, rows[pos], dx*4);
					break;
				case eFilter::kLinearInterp:
					asm_resize_interp_col_run(
							dstp,
							rows[pos],
							rows[pos+1],
							dx,
							(unsigned long)yaccum >> 16);
					break;
				case eFilter::kCubicInterp:
					if (SSE2_enabled)
						asm_resize_ccint_col_SSE2(dstp, rows[pos], rows[pos+1], rows[pos+2], rows[pos+3], dx, GetStandardCubic4Table()+1024+4*((unsigned long)yaccum>>24));
					else if (MMX_enabled)
						asm_resize_ccint_col_MMX(dstp, rows[pos], rows[pos+1], rows[pos+2], rows[pos+3], dx, GetStandardCubic4Table()+1024+4*((unsigned long)yaccum>>24));
					else
						asm_resize_ccint_col(dstp, rows[pos], rows[pos+1], rows[pos+2], rows[pos+3], dx, GetStandardCubic4Table()+4*((unsigned long)yaccum>>24));
					break;
				case eFilter::kCubicInterp060:
					if (SSE2_enabled)
						asm_resize_ccint_col_SSE2(dstp, rows[pos], rows[pos+1], rows[pos+2], rows[pos+3], dx, GetBetterCubic4Table()+1024+4*((unsigned long)yaccum>>24));
					else if (MMX_enabled)
						asm_resize_ccint_col_MMX(dstp, rows[pos], rows[pos+1], rows[pos+2], rows[pos+3], dx, GetBetterCubic4Table()+1024+4*((unsigned long)yaccum>>24));
					else
						asm_resize_ccint_col(dstp, rows[pos], rows[pos+1], rows[pos+2], rows[pos+3], dx, GetBetterCubic4Table()+4*((unsigned long)yaccum>>24));
					break;
			}

			yaccum += yinc;
			dstp = (Pixel32 *)((char *)dstp - dst->pitch);
		}

	if (vert.clip.postcopy) {
		srcp = dstp;

		_DoRow(dstp, src->Address32(0,src->h-1), src->w);

		for(y=1; y<ypostcopy; ++y) {
			dstp = (Pixel32 *)((char *)dstp - dst->pitch);

			memcpy(dstp, srcp, 4*horiz.dx_int);
		}
	}

	if (MMX_enabled)
		__asm emms

	stats_print();

	return true;
}

///////////////////////////////////////////////////////////////////////////

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

#if 0
		for(j=0; j<filtwidth; j++)
			_RPT3(0, "table[%04x+%02x] = %04x\n", i, j, table[i+j]);
		Sleep(1);
#endif

		if (MMX_enabled) {
			for(j=0; j<filtwidth; j+=2) {
				int a = table[i+j];
				int b = table[i+j+1];

				a = (a & 0xffff) | (b<<16);

				table[i+j+0] = a;
				table[i+j+1] = a;
			}
		}

//		_RPT2(0,"table_error[%02x] = %04x\n", i, v2);
	}
}

int *Resampler::_CreateLinearDecimateTable(double dx, double sx, int& filtwidth) { 
	double filtwidth_fracd;
	long filtwidth_frac;
	double filt_max;
	int i;
	int *table;

	filtwidth_fracd = sx*256.0/dx;
	if (filtwidth_fracd < 256.0)
		filtwidth_fracd = 256.0;

	filtwidth_frac = (long)ceil(filtwidth_fracd);
	filtwidth = ((filtwidth_frac + 255) >> 8)<<1;

	if (!(table = new int[256 * filtwidth]))
		return NULL;

	table[filtwidth-1] = 0;

	if (sx <= dx)
		filt_max = 16384.0;
	else
		filt_max = (dx*16384.0)/sx;

	for(i=0; i<128*filtwidth; i++) {
		int y = 0;
		double d = i / filtwidth_fracd;

		if (d<1.0)
			y = (int)(0.5 + filt_max*(1.0 - d));

		table[permute_index(128*filtwidth + i, filtwidth)]
			= table[permute_index(128*filtwidth - i, filtwidth)]
			= y;
	}

	normalize_table(table, filtwidth);

	return table;
}

int *Resampler::_CreateCubicDecimateTable(double dx, double sx, int& filtwidth, double A) { 
	int i;
	long filtwidth_frac;
	double filtwidth_fracd;
	double filt_max;
	int *table;

	filtwidth_fracd = sx*256.0/dx;
	if (filtwidth_fracd < 256.0)
		filtwidth_fracd = 256.0;
	filtwidth_frac = (long)ceil(filtwidth_fracd);
	filtwidth = ((filtwidth_frac + 255) >> 8)<<2;

	if (!(table = new int[256 * filtwidth]))
		return NULL;

	table[filtwidth-1] = 0;

	if (sx <= dx)
		filt_max = 16384.0;
	else
		filt_max = (dx*16384.0)/sx;

	for(i=0; i<128*filtwidth; i++) {
		int y = 0;
		double d = (double)i / filtwidth_fracd;

		if (d < 1.0)
			y = (int)floor(0.5 + (1.0 - (A+3.0)*d*d + (A+2.0)*d*d*d) * filt_max);
		else if (d < 2.0)
			y = (int)floor(0.5 + (-4.0*A + 8.0*A*d - 5.0*A*d*d + A*d*d*d) * filt_max);

		table[permute_index(128*filtwidth + i, filtwidth)]
			= table[permute_index(128*filtwidth - i, filtwidth)]
			= y;
	}

	normalize_table(table, filtwidth);

	return table;
}

static inline double sinc(double x) {
	return fabs(x) < 1e-9 ? 1.0 : sin(x) / x;
}

int *Resampler::_CreateLanzcos3DecimateTable(double dx, double sx, int& filtwidth) { 
	int i;
	long filtwidth_frac;
	double filtwidth_fracd;
	double filt_max;
	int *table;

	filtwidth_fracd = sx*256.0/dx;
	if (filtwidth_fracd < 256.0)
		filtwidth_fracd = 256.0;

	filtwidth_frac = (long)ceil(filtwidth_fracd);
	filtwidth = ((filtwidth_frac + 255) >> 8)*6;

	if (!(table = new int[256 * filtwidth]))
		return NULL;

	table[filtwidth-1] = 0;

	if (sx <= dx)
		filt_max = 16384.0;
	else
		filt_max = (dx*16384.0)/sx;

	for(i=0; i<128*filtwidth; i++) {
		static const double pi  = 3.1415926535897932384626433832795;	// pi
		static const double pi3 = 1.0471975511965977461542144610932;	// pi/3
		int y = 0;
		double d = (double)i / filtwidth_fracd;

		if (d < 3.0)
			y = (int)floor(0.5 + sinc(pi*d) * sinc(pi3*d) * filt_max);

		table[permute_index(128*filtwidth + i, filtwidth)]
			= table[permute_index(128*filtwidth - i, filtwidth)]
			= y;
	}

	normalize_table(table, filtwidth);

	return table;
}
