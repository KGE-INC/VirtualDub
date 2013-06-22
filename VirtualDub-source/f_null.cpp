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

int null_run(const FilterActivation *fa, const FilterFunctions *ff) {

	// gee, this is such a *hard* filter... I don't know if I can handle it!

	return 0;
}

long null_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.offset	= fa->src.offset;
	fa->dst.modulo	= fa->src.modulo;
	fa->dst.pitch	= fa->src.pitch;
	return 0;
}

FilterDefinition filterDef_null={
	0,0,NULL,
	"null transform",
	"Copies source to destination, allowing for clipping without any real work.\n\n[You expect me to optimize this?]",
	NULL,NULL,
	0,
	NULL,NULL,
	null_run,
	null_param,
	NULL,
	NULL,
};