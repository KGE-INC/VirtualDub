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

#include "resource.h"
#include "filters.h"

#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>

extern HINSTANCE g_hInst;

extern "C" void asm_threshold_run(
		void *dst,
		unsigned long width,
		unsigned long height,
		unsigned long stride,
		unsigned long threshold
		);

///////////////////////////////////

struct MyFilterData {
	LONG threshold;
};

int threshold_run(const FilterActivation *fa, const FilterFunctions *ff) {	
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	asm_threshold_run(
			fa->src.data,
			fa->src.w,
			fa->src.h,
			fa->src.pitch,
			mfd->threshold
			);

	return 0;
}

long threshold_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.offset	= fa->src.offset;
	fa->dst.modulo	= fa->src.modulo;
	fa->dst.pitch	= fa->src.pitch;
	return 0;
}

//////////////////

static int threshold_config(FilterActivation *fa, const FilterFunctions *ff, VDXHWND hWnd) {
	if (!fa->filter_data) {
		if (!(fa->filter_data = (void *)new MyFilterData)) return 0;
		memset(fa->filter_data, 0, sizeof MyFilterData);
		((MyFilterData *)fa->filter_data)->threshold = 128;
	}

	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	struct local {
		static void Update(long value, void *pThis) {
			((MyFilterData *)pThis)->threshold = value;
		}
	};

	mfd->threshold = FilterGetSingleValue((HWND)hWnd, mfd->threshold, 0, 256, "threshold", fa->ifp, local::Update, mfd);

	return 0;
}

static void threshold_string(const FilterActivation *fa, const FilterFunctions *ff, char *buf) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	sprintf(buf," (%d%%)", (mfd->threshold*25)/64);
}

static void threshold_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;

	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	mfd->threshold = argv[0].asInt();
}

static ScriptFunctionDef threshold_func_defs[]={
	{ (ScriptFunctionPtr)threshold_script_config, "Config", "0i" },
	{ NULL },
};

static CScriptObject threshold_obj={
	NULL, threshold_func_defs
};

static bool threshold_script_line(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d)", mfd->threshold);

	return true;
}

FilterDefinition filterDef_threshold={
	0,0,NULL,
	"threshold",
	"Converts an image to black and white by comparing brightness values.\n\n[Assembly optimized]",
	NULL,NULL,
	sizeof(MyFilterData),
	NULL,NULL,
	threshold_run,
	threshold_param,
	threshold_config,
	threshold_string,
	NULL,
	NULL,
	&threshold_obj,
	threshold_script_line,
};