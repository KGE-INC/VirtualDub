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

#include <vd2/system/vdtypes.h>

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

#include "misc.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/resample.h>
#include "resource.h"
#include "gui.h"
#include "filter.h"
#include "vbitmap.h"

extern HINSTANCE g_hInst;

///////////////////////

enum {
	FILTER_NONE				= 0,
	FILTER_BILINEAR			= 1,
	FILTER_BICUBIC			= 2,
	FILTER_TABLEBILINEAR	= 3,
	FILTER_TABLEBICUBIC075	= 4,
	FILTER_TABLEBICUBIC060	= 5,
	FILTER_TABLEBICUBIC100	= 6,
	FILTER_LANCZOS3			= 7
};

static char *filter_names[]={
	"Nearest neighbor",
	"Bilinear",
	"Bicubic",
	"Precise bilinear",
	"Precise bicubic (A=-0.75)",
	"Precise bicubic (A=-0.60)",
	"Precise bicubic (A=-1.00)",
	"Lanczos3"
};

struct VDResizeFilterData {
	double new_x, new_y;
	long new_xf, new_yf;
	int filter_mode;
	COLORREF	rgbColor;

	HBRUSH		hbrColor;
	IFilterPreview *ifp;

	IVDPixmapResampler *resampler;

	bool	fLetterbox;
	bool	fInterlaced;
};

////////////////////

int revcolor(int c) {
	return ((c>>16)&0xff) | (c&0xff00) | ((c&0xff)<<16);
}

////////////////////

static int resize_run(const FilterActivation *fa, const FilterFunctions *ff) {
	VDResizeFilterData *mfd = (VDResizeFilterData *)fa->filter_data;

	double dstw = mfd->new_x;
	double dsth = mfd->new_y;
	Pixel *dst, *src;

	dst = fa->dst.data;
	src = fa->src.data;

	double framew = dstw;
	double frameh = dsth;

	if (mfd->fLetterbox) {
		framew = mfd->new_xf;
		frameh = mfd->new_yf;
	}

	int x1 = (int)ceil((framew - dstw)*0.5 - 0.5);
	int y1 = (int)ceil((frameh - dsth)*0.5 - 0.5);
	int x2 = (int)ceil((framew + dstw)*0.5 - 0.5);
	int y2 = (int)ceil((frameh + dsth)*0.5 - 0.5);

	// Draw letterbox bound

	if (mfd->fLetterbox) {
		Pixel *dst2 = dst, *dst3;
		Pixel fill = revcolor(mfd->rgbColor);

		long w1, w2, w, h;

		h = y1;
		if (h>0) do {
			dst3  = dst2;
			w = fa->dst.w;
			do {
				*dst3++ = fill;
			} while(--w);

			dst2 = (Pixel32 *)((char *)dst2 + fa->dst.pitch);
		} while(--h);

		w1 = x1;
		w2 = fa->dst.w - x2;

		h = y2-y1;
		do {
			dst3 = dst2;

			// fill left

			w = w1;
			if (w) do {
				*dst3++ = fill;
			} while(--w);

			// skip center

			dst3 += (int)mfd->new_x;

			// fill right

			w = w2;
			if (w) do {
				*dst3++ = fill;
			} while(--w);


			dst2 = (Pixel32 *)((char *)dst2 + fa->dst.pitch);
		} while(--h);

		h = fa->dst.h - y2;
		if (h>0) do {
			dst3  = dst2;
			w = fa->dst.w;
			do {
				*dst3++ = fill;
			} while(--w);

			dst2 = (Pixel32 *)((char *)dst2 + fa->dst.pitch);
		} while(--h);
	}

	if (mfd->fInterlaced) {
		VDPixmap vbHalfSrc, vbHalfDst;

		// Top field

		vbHalfSrc		= VDAsPixmap((VBitmap&)fa->src);
		vbHalfSrc.pitch *= 2;
		vbHalfSrc.h		= (fa->src.h + 1) >> 1;

		vbHalfDst		= VDAsPixmap((VBitmap&)fa->dst);
		vbHalfDst.pitch *= 2;
		vbHalfDst.h		= (fa->dst.h + 1) >> 1;

		double dx = (fa->dst.w - dstw) * 0.5;
		double dy = (fa->dst.h - dsth) * 0.25 + 0.25;

		mfd->resampler->Process(vbHalfDst, dx, dy, dx + mfd->new_x, dy + mfd->new_y*0.5, vbHalfSrc, 0, 0.25);

		// Bottom field

		vdptrstep(vbHalfSrc.data, -fa->src.pitch);
		vbHalfSrc.h		= fa->src.h >> 1;
		vdptrstep(vbHalfDst.data, -fa->dst.pitch);
		vbHalfDst.h		= fa->dst.h >> 1;

		dy -= 0.5;
		mfd->resampler->Process(vbHalfDst, dx, dy, dx + mfd->new_x, dy + mfd->new_y*0.5, vbHalfSrc, 0, -0.25);
	} else {
		VDPixmap pxdst(VDAsPixmap(*(VBitmap *)&fa->dst));
		VDPixmap pxsrc(VDAsPixmap(*(VBitmap *)&fa->src));

		double dx = (fa->dst.w - dstw) * 0.5;
		double dy = (fa->dst.h - dsth) * 0.5;

		mfd->resampler->Process(pxdst, dx, dy, dx + mfd->new_x, dy + mfd->new_y, pxsrc, 0, 0);
	}

	return 0;
}

static long resize_param(FilterActivation *fa, const FilterFunctions *ff) {
	VDResizeFilterData *mfd = (VDResizeFilterData *)fa->filter_data;

	int dstw = (int)ceil(mfd->new_x);
	int dsth = (int)ceil(mfd->new_y);

	if (mfd->fLetterbox) {
		fa->dst.w		= std::max<int>(dstw, mfd->new_xf);
		fa->dst.h		= std::max<int>(dsth, mfd->new_yf);
	} else {
		fa->dst.w		= dstw;
		fa->dst.h		= dsth;
	}

	fa->dst.AlignTo8();

	return FILTERPARAM_SWAP_BUFFERS;
}

namespace {
	void VDSetDlgItemFloatW32(HWND hdlg, UINT id, double v) {
		char buf[512];

		sprintf(buf, "%g", v);
		SetDlgItemText(hdlg, id, buf);
	}

	double VDGetDlgItemFloatW32(HWND hdlg, UINT id, BOOL *success) {
		char buf[512];

		*success = FALSE;

		if (GetDlgItemText(hdlg, id, buf, sizeof buf)) {
			double v;
			if (1 == sscanf(buf, " %lg", &v)) {
				*success = TRUE;
				return v;
			}
		}

		return 0;
	}
};


static INT_PTR APIENTRY resizeDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	VDResizeFilterData *mfd = (struct VDResizeFilterData *)GetWindowLongPtr(hDlg, DWLP_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			{
				HWND hwndItem;
				int i;

				mfd = (VDResizeFilterData *)lParam;

				VDSetDlgItemFloatW32(hDlg, IDC_WIDTH, mfd->new_x);
				VDSetDlgItemFloatW32(hDlg, IDC_HEIGHT, mfd->new_y);
				SetDlgItemInt(hDlg, IDC_FRAMEWIDTH, mfd->new_xf, FALSE);
				SetDlgItemInt(hDlg, IDC_FRAMEHEIGHT, mfd->new_yf, FALSE);

				hwndItem = GetDlgItem(hDlg, IDC_FILTER_MODE);

				for(i=0; i<(sizeof filter_names/sizeof filter_names[0]); i++)
					SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)filter_names[i]);

				SendMessage(hwndItem, CB_SETCURSEL, mfd->filter_mode, 0);

				CheckDlgButton(hDlg, IDC_INTERLACED, mfd->fInterlaced ? BST_CHECKED : BST_UNCHECKED);

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

				SetWindowLongPtr(hDlg, DWLP_USER, (LONG)mfd);

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
					double new_x;
					BOOL success;

					new_x = VDGetDlgItemFloatW32(hDlg, IDC_WIDTH, &success);
					if (!success || new_x < 16) {
						SetFocus((HWND)lParam);
						MessageBeep(MB_ICONQUESTION);
						return TRUE;
					}

					if (mfd->new_x != new_x) {
						mfd->ifp->UndoSystem();
						mfd->new_x = new_x;
						mfd->ifp->RedoSystem();
					}
				}
				return TRUE;

			case IDC_HEIGHT:
				if (HIWORD(wParam) == EN_KILLFOCUS) {
					double new_y;
					BOOL success;

					new_y = VDGetDlgItemFloatW32(hDlg, IDC_HEIGHT, &success);
					if (!success || new_y < 16) {
						SetFocus((HWND)lParam);
						MessageBeep(MB_ICONQUESTION);
						return TRUE;
					}

					if (mfd->new_y != new_y) {
						mfd->ifp->UndoSystem();
						mfd->new_y = new_y;
						mfd->ifp->RedoSystem();
					}
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

			case IDC_INTERLACED:
				if (HIWORD(wParam) == BN_CLICKED) {
					BOOL f = IsDlgButtonChecked(hDlg, IDC_INTERLACED);

					mfd->ifp->UndoSystem();
					mfd->fInterlaced = !!f;

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
	VDResizeFilterData *mfd = (VDResizeFilterData *)fa->filter_data;
	VDResizeFilterData mfd2 = *mfd;
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
	VDResizeFilterData *mfd = (VDResizeFilterData *)fa->filter_data;

	if (mfd->fLetterbox)
		wsprintf(buf, " (%s, lbox %dx%d #%06x)", filter_names[mfd->filter_mode],
				mfd->new_xf, mfd->new_yf, revcolor(mfd->rgbColor));
	else
		wsprintf(buf, " (%s)", filter_names[mfd->filter_mode]);
}

static int resize_start(FilterActivation *fa, const FilterFunctions *ff) {
	VDResizeFilterData *mfd = (VDResizeFilterData *)fa->filter_data;
	double dstw = mfd->new_x;
	double dsth = mfd->new_y;

	if (dstw<16 || dsth<16)
		return 1;

	IVDPixmapResampler::FilterMode fmode;
	bool bInterpolationOnly = true;

	mfd->resampler = VDCreatePixmapResampler();

	switch(mfd->filter_mode) {
	case FILTER_NONE:
		fmode = IVDPixmapResampler::kFilterPoint;
		break;
	case FILTER_TABLEBILINEAR:
		bInterpolationOnly = false;
	case FILTER_BILINEAR:
		fmode = IVDPixmapResampler::kFilterLinear;
		break;
	case FILTER_TABLEBICUBIC060:
		mfd->resampler->SetSplineFactor(-0.60);
		fmode = IVDPixmapResampler::kFilterCubic;
		bInterpolationOnly = false;
		break;
	case FILTER_TABLEBICUBIC075:
		bInterpolationOnly = false;
	case FILTER_BICUBIC:
		mfd->resampler->SetSplineFactor(-0.75);
		fmode = IVDPixmapResampler::kFilterCubic;
		break;
	case FILTER_TABLEBICUBIC100:
		bInterpolationOnly = false;
		mfd->resampler->SetSplineFactor(-1.0);
		fmode = IVDPixmapResampler::kFilterCubic;
		break;
	case FILTER_LANCZOS3:
		bInterpolationOnly = false;
		fmode = IVDPixmapResampler::kFilterLanczos3;
		break;
	}

	if (mfd->fInterlaced)
		mfd->resampler->Init(dstw, dsth * 0.5f, nsVDPixmap::kPixFormat_XRGB8888, fa->src.w, fa->src.h * 0.5f, nsVDPixmap::kPixFormat_XRGB8888, fmode, fmode, bInterpolationOnly);
	else
		mfd->resampler->Init(dstw, dsth, nsVDPixmap::kPixFormat_XRGB8888, fa->src.w, fa->src.h, nsVDPixmap::kPixFormat_XRGB8888, fmode, fmode, bInterpolationOnly);

	return 0;
}

static int resize_stop(FilterActivation *fa, const FilterFunctions *ff) {
	VDResizeFilterData *mfd = (VDResizeFilterData *)fa->filter_data;

	delete mfd->resampler;	mfd->resampler = NULL;

	return 0;
}

static void resize_script_config(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
	FilterActivation *fa = (FilterActivation *)lpVoid;

	VDResizeFilterData *mfd = (VDResizeFilterData *)fa->filter_data;

	mfd->new_x	= argv[0].asDouble();
	mfd->new_y	= argv[1].asDouble();

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
			VDSCRIPT_EXT_ERROR(FCALL_UNKNOWN_STR);
	}

	mfd->fInterlaced = false;

	if (mfd->filter_mode & 128) {
		mfd->fInterlaced = true;
		mfd->filter_mode &= 127;
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
	{ (ScriptFunctionPtr)resize_script_config, "Config", "0ddi" },
	{ (ScriptFunctionPtr)resize_script_config, NULL, "0dds" },
	{ (ScriptFunctionPtr)resize_script_config, NULL, "0ddiiii" },
	{ (ScriptFunctionPtr)resize_script_config, NULL, "0ddsiii" },
	{ NULL },
};

static CScriptObject resize_obj={
	NULL, resize_func_defs
};

static bool resize_script_line(FilterActivation *fa, const FilterFunctions *ff, char *buf, int buflen) {
	VDResizeFilterData *mfd = (VDResizeFilterData *)fa->filter_data;
	int filtmode = mfd->filter_mode + (mfd->fInterlaced ? 128 : 0);

	if (mfd->fLetterbox)
		_snprintf(buf, buflen, "Config(%g,%g,%d,%d,%d,0x%06x)", mfd->new_x, mfd->new_y, filtmode, mfd->new_xf, mfd->new_yf,
			revcolor(mfd->rgbColor));
	else
		_snprintf(buf, buflen, "Config(%g,%g,%d)", mfd->new_x, mfd->new_y, filtmode);

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
	sizeof(VDResizeFilterData),
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