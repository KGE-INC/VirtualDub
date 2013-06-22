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
#include "filter.h"
#include "ScriptValue.h"
#include "ScriptInterpreter.h"
#include "ScriptError.h"

extern HINSTANCE g_hInst;

extern "C" void asm_brightcont1_run(
		void *dst,
		unsigned long width,
		unsigned long height,
		unsigned long stride,
		unsigned long multiplier,
		unsigned long adder1,
		unsigned long adder2
		);

extern "C" void asm_brightcont2_run(
		void *dst,
		unsigned long width,
		unsigned long height,
		unsigned long stride,
		unsigned long multiplier,
		unsigned long adder1,
		unsigned long adder2
		);

///////////////////////////////////

struct MyFilterData {
	LONG bright;
	LONG cont;
	IFilterPreview *ifp;
};

int brightcont_run(const FilterActivation *fa, const FilterFunctions *ff) {	
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	if (mfd->bright>=0)
		asm_brightcont2_run(
				fa->src.data,
				fa->src.w,
				fa->src.h,
				fa->src.pitch,
				mfd->cont,
				mfd->bright*0x00100010L,
				mfd->bright*0x00001000L
				);
	else
		asm_brightcont1_run(
				fa->src.data,
				fa->src.w,
				fa->src.h,
				fa->src.pitch,
				mfd->cont,
				(-mfd->bright)*0x00100010L,
				(-mfd->bright)*0x00001000L
				);

	return 0;
}

long brightcont_param(FilterActivation *fa, const FilterFunctions *ff) {
	fa->dst.offset	= fa->src.offset;
	fa->dst.modulo	= fa->src.modulo;
	fa->dst.pitch	= fa->src.pitch;
	return 0;
}

//////////////////

static int brightcont_init(FilterActivation *fa, const FilterFunctions *ff) {
	((MyFilterData *)fa->filter_data)->bright = 0;
	((MyFilterData *)fa->filter_data)->cont = 16;

	return 0;
}

static INT_PTR CALLBACK brightcontDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_INITDIALOG:
			{
				MyFilterData *mfd = (MyFilterData *)lParam;
				HWND hWnd;

				hWnd = GetDlgItem(hDlg, IDC_BRIGHTNESS);
				SendMessage(hWnd, TBM_SETTICFREQ, 16, 0);
				SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 512));
				SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->bright+256);

				hWnd = GetDlgItem(hDlg, IDC_CONTRAST);
				SendMessage(hWnd, TBM_SETTICFREQ, 4, 0);
				SendMessage(hWnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(0, 32));
				SendMessage(hWnd, TBM_SETPOS, (WPARAM)TRUE, mfd->cont);

				SetWindowLongPtr(hDlg, DWLP_USER, (LONG)mfd);

				hWnd = GetDlgItem(hDlg, IDC_PREVIEW);
				if (mfd->ifp) {
					EnableWindow(hWnd, TRUE);
					mfd->ifp->InitButton(GetDlgItem(hDlg, IDC_PREVIEW));
				}
			}
            return TRUE;

        case WM_COMMAND:                      
            if (LOWORD(wParam) == IDOK) {
				MyFilterData *mfd = (struct MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);

				mfd->bright = SendMessage(GetDlgItem(hDlg, IDC_BRIGHTNESS), TBM_GETPOS, 0, 0)-256;
				mfd->cont = SendMessage(GetDlgItem(hDlg, IDC_CONTRAST), TBM_GETPOS, 0, 0);

				EndDialog(hDlg, 0);
				SetWindowLong(hDlg, DWL_MSGRESULT, 0);
				return TRUE;
			} else if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, 1);
				SetWindowLong(hDlg, DWL_MSGRESULT, 0);
                return TRUE;
			} else if (LOWORD(wParam) == IDC_PREVIEW) {
				MyFilterData *mfd = (struct MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);
				if (mfd->ifp)
					mfd->ifp->Toggle(hDlg);

				SetWindowLong(hDlg, DWL_MSGRESULT, 0);
				return TRUE;
            }
            break;

        case WM_HSCROLL:
			if (lParam) {
				MyFilterData *mfd = (struct MyFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);
				HWND hwndScroll = (HWND)lParam;
				UINT id = GetWindowLong(hwndScroll, GWL_ID);

				if (id == IDC_BRIGHTNESS) {
					int bright = SendMessage(hwndScroll, TBM_GETPOS, 0, 0)-256;
					if (mfd->bright != bright) {
						mfd->bright = bright;

						if (mfd->ifp)
							mfd->ifp->RedoFrame();
					}
				} else if (id == IDC_CONTRAST) {
					int cont = SendMessage(hwndScroll, TBM_GETPOS, 0, 0);
					if (mfd->cont != cont) {
						mfd->cont = cont;

						if (mfd->ifp)
							mfd->ifp->RedoFrame();
					}
				}
					

				SetWindowLong(hDlg, DWL_MSGRESULT, 0);
				return TRUE;
			}
			break;
    }
    return FALSE;
}

static int brightcont_config(FilterActivation *fa, const FilterFunctions *ff, HWND hWnd) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;
	mfd->ifp = fa->ifp;

	MyFilterData tmp(*mfd);

	if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_BRIGHTCONT), hWnd, brightcontDlgProc, (LONG)fa->filter_data))
		return true;

	*mfd = tmp;
	return false;
}

static void brightcont_string(const FilterActivation *fa, const FilterFunctions *ff, char *buf) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	sprintf(buf," (bright %+d%%, cont %d%%)", (mfd->bright*25)/64, (mfd->cont*25)/4);
}

static void brightcont_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	mfd->bright	= argv[0].asInt();
	mfd->cont	= argv[1].asInt();
}

static ScriptFunctionDef brightcont_func_defs[]={
	{ (ScriptFunctionPtr)brightcont_script_config, "Config", "0ii" },
	{ NULL },
};

static CScriptObject brightcont_obj={
	NULL, brightcont_func_defs
};

static bool brightcont_script_line(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	MyFilterData *mfd = (MyFilterData *)fa->filter_data;

	_snprintf(buf, buflen, "Config(%d,%d)", mfd->bright, mfd->cont);

	return true;
}

FilterDefinition filterDef_brightcont={
	0,0,NULL,
	"brightness/contrast",
	"Adjusts brightness and contrast of an image linearly.\n\n[Assembly optimized] [MMX optimized]",
	NULL,NULL,
	sizeof(MyFilterData),
	brightcont_init,
	NULL,
	brightcont_run,
	brightcont_param,
	brightcont_config,
	brightcont_string,
	NULL,
	NULL,
	&brightcont_obj,
	brightcont_script_line,
};