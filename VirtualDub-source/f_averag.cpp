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

#include "filter.h"

#define READSRC(byteoff, lineoff) (*(unsigned long *)((char *)src + pitch*(lineoff) + (byteoff*4)))

#define USE_ASM

extern "C" void asm_average_run(
		void *dst,
		void *src,
		unsigned long width,
		unsigned long height,
		unsigned long srcstride,
		unsigned long dststride);

static int avg_run(const FilterActivation *fa, const FilterFunctions *ff) {
	unsigned long w,h;
	unsigned long *src = (unsigned long *)fa->src.data, *dst = (unsigned long *)fa->dst.data;
	unsigned long pitch = fa->src.pitch;
	unsigned long rb, g;

	src -= pitch>>2;

	rb	=   ((READSRC( 0,1)&0xff00ff) + (READSRC( 0,2)&0xff00ff)
			+(READSRC( 1,1)&0xff00ff) + (READSRC( 1,2)&0xff00ff));
	g	=   ((READSRC( 0,1)&0x00ff00) + (READSRC( 0,2)&0x00ff00)
			+(READSRC( 1,1)&0x00ff00) + (READSRC( 1,2)&0x00ff00));
	*dst ++ = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);
	++src;

	w = fa->src.w-2;
	do {
		rb	=   ((READSRC(-1,1)&0xff00ff) + (READSRC(-1,2)&0xff00ff)
				+(READSRC( 0,1)&0xff00ff) + (READSRC( 0,2)&0xff00ff)
				+(READSRC( 1,1)&0xff00ff) + (READSRC( 1,2)&0xff00ff));
		g	=   ((READSRC(-1,1)&0x00ff00) + (READSRC(-1,2)&0x00ff00)
				+(READSRC( 0,1)&0x00ff00) + (READSRC( 0,2)&0x00ff00)
				+(READSRC( 1,1)&0x00ff00) + (READSRC( 1,2)&0x00ff00));
		*dst ++ = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);
		++src;
	} while(--w);

	rb	=   ((READSRC( 0,1)&0xff00ff) + (READSRC( 0,2)&0xff00ff)
			+(READSRC(-1,1)&0xff00ff) + (READSRC(-1,2)&0xff00ff));
	g	=   ((READSRC( 0,1)&0x00ff00) + (READSRC( 0,2)&0x00ff00)
			+(READSRC(-1,1)&0x00ff00) + (READSRC(-1,2)&0x00ff00));
	*dst ++ = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);
	++src;

	src += fa->src.modulo>>2;
	dst += fa->dst.modulo>>2;

#ifdef USE_ASM
	asm_average_run(dst+1, src+1, fa->src.w-2, fa->src.h-2, fa->src.pitch, fa->dst.pitch);
#endif

	h = fa->src.h-2;
	do {
		rb	=   ((READSRC( 0,0)&0xff00ff) + (READSRC( 0,1)&0xff00ff) + (READSRC( 0,2)&0xff00ff)
				+(READSRC( 1,0)&0xff00ff) + (READSRC( 1,1)&0xff00ff) + (READSRC( 1,2)&0xff00ff));
		g	=   ((READSRC( 0,0)&0x00ff00) + (READSRC( 0,1)&0x00ff00) + (READSRC( 0,2)&0x00ff00)
				+(READSRC( 1,0)&0x00ff00) + (READSRC( 1,1)&0x00ff00) + (READSRC( 1,2)&0x00ff00));
		*dst ++ = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);
		++src;
#ifndef USE_ASM
		w = fa->src.w-2;
		do {
#if 0
			rb	=   ((READSRC(-1,0)&0xff00ff) + (READSRC(-1,1)&0xff00ff) + (READSRC(-1,2)&0xff00ff)
					+(READSRC( 0,0)&0xff00ff) + (READSRC( 0,1)&0xff00ff) + (READSRC( 0,2)&0xff00ff)
					+(READSRC( 1,0)&0xff00ff) + (READSRC( 1,1)&0xff00ff) + (READSRC( 1,2)&0xff00ff));
			g	=   ((READSRC(-1,0)&0x00ff00) + (READSRC(-1,1)&0x00ff00) + (READSRC(-1,2)&0x00ff00)
					+(READSRC( 0,0)&0x00ff00) + (READSRC( 0,1)&0x00ff00) + (READSRC( 0,2)&0x00ff00)
					+(READSRC( 1,0)&0x00ff00) + (READSRC( 1,1)&0x00ff00) + (READSRC( 1,2)&0x00ff00));
			*dst ++ = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);
#else
			rb	=   ((READSRC(-1,0)&0xff00ff) + (READSRC(-1,1)&0xff00ff) + (READSRC(-1,2)&0xff00ff)
					+(READSRC( 0,0)&0xff00ff)                            + (READSRC( 0,2)&0xff00ff)
					+(READSRC( 1,0)&0xff00ff) + (READSRC( 1,1)&0xff00ff) + (READSRC( 1,2)&0xff00ff));
			g	=   ((READSRC(-1,0)&0x00ff00) + (READSRC(-1,1)&0x00ff00) + (READSRC(-1,2)&0x00ff00)
					+(READSRC( 0,0)&0x00ff00)                            + (READSRC( 0,2)&0x00ff00)
					+(READSRC( 1,0)&0x00ff00) + (READSRC( 1,1)&0x00ff00) + (READSRC( 1,2)&0x00ff00));

			rb = rb*28 + (src[pitch>>2]&0xff00ff)*32;
			g = g*28 + (src[pitch>>2]&0x00ff00)*32;

			*dst ++ = ((rb & 0xff00ff00) | (g & 0x00ff0000)) >> 8;
#endif
			++src;
		} while(--w);
#else
		src += fa->src.w-2;
		dst += fa->dst.w-2;
#endif

		rb	=   ((READSRC( 0,0)&0xff00ff) + (READSRC( 0,1)&0xff00ff) + (READSRC( 0,2)&0xff00ff)
				+(READSRC(-1,0)&0xff00ff) + (READSRC(-1,1)&0xff00ff) + (READSRC(-1,2)&0xff00ff));
		g	=   ((READSRC( 0,0)&0x00ff00) + (READSRC( 0,1)&0x00ff00) + (READSRC( 0,2)&0x00ff00)
				+(READSRC(-1,0)&0x00ff00) + (READSRC(-1,1)&0x00ff00) + (READSRC(-1,2)&0x00ff00));
		*dst ++ = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);
		++src;

		src += fa->src.modulo>>2;
		dst += fa->dst.modulo>>2;
	} while(--h);

	rb	=   ((READSRC( 0,0)&0xff00ff) + (READSRC( 0,1)&0xff00ff)
			+(READSRC( 1,0)&0xff00ff) + (READSRC( 1,1)&0xff00ff));
	g	=   ((READSRC( 0,0)&0x00ff00) + (READSRC( 0,1)&0x00ff00)
			+(READSRC( 1,0)&0x00ff00) + (READSRC( 1,1)&0x00ff00));
	*dst ++ = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);
	++src;

	w = fa->src.w-2;
	do {
		rb	=   ((READSRC(-1,0)&0xff00ff) + (READSRC(-1,1)&0xff00ff)
				+(READSRC( 0,0)&0xff00ff) + (READSRC( 0,1)&0xff00ff)
				+(READSRC( 1,0)&0xff00ff) + (READSRC( 1,1)&0xff00ff));
		g	=   ((READSRC(-1,0)&0x00ff00) + (READSRC(-1,1)&0x00ff00)
				+(READSRC( 0,0)&0x00ff00) + (READSRC( 0,1)&0x00ff00)
				+(READSRC( 1,0)&0x00ff00) + (READSRC( 1,1)&0x00ff00));
		*dst ++ = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);
		++src;
	} while(--w);

	rb	=   ((READSRC( 0,0)&0xff00ff) + (READSRC( 0,1)&0xff00ff)
			+(READSRC(-1,0)&0xff00ff) + (READSRC(-1,1)&0xff00ff));
	g	=   ((READSRC( 0,0)&0x00ff00) + (READSRC( 0,1)&0x00ff00)
			+(READSRC(-1,0)&0x00ff00) + (READSRC(-1,1)&0x00ff00));
	*dst ++ = ((rb/9) & 0x00ff0000) | ((rb & 0x0000ffff)/9) | ((g/9) & 0x00ff00);
	++src;

	return 0;
}

FilterDefinition filterDef_average={
	0,0,NULL,
	"3x3 average",
	"Replaces each pixel with the 3x3 average of its neighbors.\n\n[Assembly optimized] [MMX optimized]",
	NULL,NULL,
	0,
	NULL,NULL,
	avg_run
};