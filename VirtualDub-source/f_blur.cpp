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

#include "Effect.h"
#include "e_blur.h"

//#define USE_ASM

struct FilterBlurData {
	VEffect *effect;
};

static int blur_run(const FilterActivation *fa, const FilterFunctions *ff) {
	FilterBlurData *pfbd = (FilterBlurData *)fa->filter_data;

	if (pfbd->effect)
		pfbd->effect->run(&fa->dst);

	return 0;
}

static int blur_start(FilterActivation *fa, const FilterFunctions *ff) {
	FilterBlurData *pfbd = (FilterBlurData *)fa->filter_data;

	if (!(pfbd->effect = VCreateEffectBlur(&fa->dst)))
		return 1;

	return 0;
}

static long blur_param(FilterActivation *fa, const FilterFunctions *ff) {
	return 0;
}

static int blur_start2(FilterActivation *fa, const FilterFunctions *ff) {
	FilterBlurData *pfbd = (FilterBlurData *)fa->filter_data;

	if (!(pfbd->effect = VCreateEffectBlurHi(&fa->dst)))
		return 1;

	return 0;
}

static int blur_stop(FilterActivation *fa, const FilterFunctions *ff) {
	FilterBlurData *pfbd = (FilterBlurData *)fa->filter_data;

	delete pfbd->effect;
	pfbd->effect;

	return 0;
}


extern FilterDefinition filterDef_blur={
	0,0,NULL,
	"blur",
	"Applies a radius-1 gaussian blur to the image.",
	NULL,NULL,
	sizeof(FilterBlurData),
	NULL,NULL,
	blur_run,
	blur_param,
	NULL,
	NULL,
	blur_start,
	blur_stop
};

extern FilterDefinition filterDef_blurhi={
	0,0,NULL,
	"blur more",
	"Applies a radius-2 gaussian blur to the image.",
	NULL,NULL,
	sizeof(FilterBlurData),
	NULL,NULL,
	blur_run,
	blur_param,
	NULL,
	NULL,
	blur_start2,
	blur_stop
};
