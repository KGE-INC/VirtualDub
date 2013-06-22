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

#include <string.h>

#include "filter.h"

static int flipv_run(const FilterActivation *fa, const FilterFunctions *ff) {
	Pixel *src = (Pixel *)fa->src.data;
	Pixel *dst = (Pixel *)((char *)fa->dst.data + fa->dst.pitch*(fa->dst.h-1));
	unsigned long h;
	unsigned long pitch = fa->src.pitch;

	h = fa->dst.h;
	do {
		memcpy(dst, src, fa->src.w * 4);
		src = (Pixel *)((char *)src + fa->src.pitch);
		dst = (Pixel *)((char *)dst - fa->dst.pitch);
	} while(--h);

	return 0;
}

FilterDefinition filterDef_flipv={
	0,0,NULL,
	"flip vertically",
	"Vertically flips an image.\n\n",
	NULL,NULL,
	0,
	NULL,NULL,
	flipv_run
};

////////////////////////////////////////////////////////////

static int fliph_run(const FilterActivation *fa, const FilterFunctions *ff) {
	Pixel *src = fa->src.data, *srct;
	Pixel *dst = fa->dst.data-1;
	unsigned long h, w;
	unsigned long pitch = fa->src.pitch;

	h = fa->dst.h;
	do {
		srct = src;
		w = fa->dst.w;
		do {
			dst[w] = *srct++;
		} while(--w);
		src = (Pixel *)((char *)src + fa->src.pitch);
		dst = (Pixel *)((char *)dst + fa->dst.pitch);
	} while(--h);

	return 0;
}

FilterDefinition filterDef_fliph={
	0,0,NULL,
	"flip horizontally",
	"Horizontally flips an image.\n\n",
	NULL,NULL,
	0,
	NULL,NULL,
	fliph_run
};