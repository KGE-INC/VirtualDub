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

#include "filter.h"
#include "filters.h"

extern FilterDefinition filterDef_average, filterDef_reduceby2,
						filterDef_convolute, filterDef_sharpen,
						filterDef_brightcont, filterDef_emboss,
						filterDef_null, filterDef_grayscale,
						filterDef_reduce2hq,
						filterDef_threshold, filterDef_resize,
						filterDef_fill, filterDef_tsoften,
						filterDef_flipv, filterDef_fliph,
						filterDef_tv, filterDef_smoother,
						filterDef_deinterlace,
						filterDef_rotate,
						filterDef_invert,
						filterDef_rotate2,
						filterDef_levels,
						filterDef_fieldswap,
						filterDef_blur,
						filterDef_blurhi;

static FilterDefinition *builtin_filters[]={
	&filterDef_average,
	&filterDef_reduceby2,
	&filterDef_convolute,
	&filterDef_sharpen,
	&filterDef_brightcont,
	&filterDef_emboss,
	&filterDef_null,
	&filterDef_grayscale,
	&filterDef_reduce2hq,
	&filterDef_threshold,
	&filterDef_resize,
	&filterDef_fill,
	&filterDef_tsoften,
	&filterDef_flipv,
	&filterDef_fliph,
	&filterDef_tv,
	&filterDef_smoother,
	&filterDef_deinterlace,
	&filterDef_rotate,
	&filterDef_invert,
	&filterDef_rotate2,
	&filterDef_levels,
	&filterDef_fieldswap,
	&filterDef_blur,
	&filterDef_blurhi,
	NULL
};

void InitBuiltinFilters() {
	FilterDefinition *last=NULL, *cur, **cpp;

	filter_list = NULL;

	cpp = builtin_filters;
	while(cur = *cpp++) {
		if (!last) filter_list = cur;
		else last->next = cur;
		cur->prev = last;

		last = cur;
	}
	last->next = NULL;
}
