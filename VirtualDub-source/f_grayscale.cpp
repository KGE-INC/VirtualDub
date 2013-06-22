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

#include <stdio.h>

#include <windows.h>
#include <commctrl.h>

#include "resource.h"
#include "filter.h"

extern HINSTANCE g_hInst;

extern "C" void asm_grayscale_run(
		void *dst,
		unsigned long width,
		unsigned long height,
		unsigned long stride
		);

///////////////////////////////////

int grayscale_run(const FilterActivation *fa, const FilterFunctions *ff) {	
	asm_grayscale_run(
			fa->src.data,
			fa->src.w,
			fa->src.h,
			fa->src.pitch
			);

	return 0;
}

long grayscale_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.offset	= fa->src.offset;
	fa->dst.modulo	= fa->src.modulo;
	fa->dst.pitch	= fa->src.pitch;
	return 0;
}

FilterDefinition filterDef_grayscale={
	0,0,NULL,
	"grayscale",
	"Rips the color out of your image.\n\n[Assembly optimized]",
	NULL,NULL,
	0,
	NULL,NULL,
	grayscale_run,
	grayscale_param,
	NULL,
	NULL,
};