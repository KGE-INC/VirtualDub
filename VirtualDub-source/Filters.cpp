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

#include "VirtualDub.h"

#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <crtdbg.h>

#include <windows.h>
#include <commctrl.h>
#include <vfw.h>

#include "PositionControl.h"
#include "ProgressDialog.h"
#include "FrameSubset.h"
#include "resource.h"
#include "error.h"
#include "cpuaccel.h"
#include "gui.h"
#include "oshelper.h"
#include "misc.h"

#define f_FILTER_GLOBALS
#include "filter.h"
#include "filters.h"
#include "optdlg.h"
#include "dub.h"
#include "VideoSource.h"

extern VideoSource *inputVideoAVI;
extern FrameSubset *inputSubset;

extern HINSTANCE	g_hInst;
extern "C" unsigned long version_num;

extern char PositionFrameTypeCallback(HWND hwnd, void *pvData, long pos);
extern void CPUTest();

/////////////////////////////////////


static bool isFPUEnabled() {
	return !!FPU_enabled;
}

static bool isMMXEnabled() {
	return !!MMX_enabled;
}

static void FilterThrowExcept(const char *format, ...) {
	va_list val;
	MyError e;

	va_start(val, format);
	e.vsetf(format, val);
	va_end(val);

	throw e;
}

static void FilterThrowExceptMemory() {
	throw MyMemoryError();
}

// This is really disgusting...

static void InitVTables(struct FilterVTbls *pvtbls) {
	pvtbls->pvtblVBitmap = *(void **)&VBitmap();
}

static long FilterGetCPUFlags() {
	return CPUGetEnabledExtensions();
}

static long FilterGetHostVersionInfo(char *buf, int len) {
	char tbuf[256];

	LoadString(g_hInst, IDS_TITLE_INITIAL, tbuf, sizeof tbuf);
	_snprintf(buf, len, tbuf, version_num,
#ifdef _DEBUG
		"debug"
#else
		"release"
#endif
		);

	return version_num;
}

FilterFunctions g_filterFuncs={
	FilterAdd,
	FilterRemove,
	isFPUEnabled,
	isMMXEnabled,
	InitVTables,
	FilterThrowExceptMemory,
	FilterThrowExcept,
	FilterGetCPUFlags,
	FilterGetHostVersionInfo,
};

/////////////////////////////////////

FilterActivation::FilterActivation(const FilterActivation& fa, VFBitmap& _dst, VFBitmap& _src, VFBitmap *_last) : dst(_dst), src(_src), last(_last) {
	filter			= fa.filter;
	filter_data		= fa.filter_data;
	x1				= fa.x1;
	y1				= fa.y1;
	x2				= fa.x2;
	y2				= fa.y2;
	pfsi			= fa.pfsi;
}

/////////////////////////////////////

FilterInstance::FilterInstance()
	: FilterActivation(realDst, realSrc, &realLast)
{
	src.hdc = NULL;
	dst.hdc = NULL;
	last->hdc = NULL;
	pfsiDelayRing = NULL;
}

FilterInstance::FilterInstance(const FilterInstance& fi)
	: FilterActivation(fi, realDst, realSrc, &realLast)
{
	realSrc		= fi.realSrc;
	realDst		= fi.realDst;
	realLast	= fi.realLast;
	flags		= fi.flags;
	hbmDst		= fi.hbmDst;
	hgoDst		= fi.hgoDst;
	pvDstView	= fi.pvDstView;
	pvLastView	= fi.pvLastView;
	srcbuf		= fi.srcbuf;
	dstbuf		= fi.dstbuf;
	fNoDeinit	= fi.fNoDeinit;
	pfsiDelayRing = NULL;
}

FilterInstance::FilterInstance(FilterDefinition *fd)
	: FilterActivation(realDst, realSrc, &realLast)
{
	filter = fd;
	src.hdc = NULL;
	dst.hdc = NULL;
	last->hdc = NULL;
	fNoDeinit = false;
	pfsiDelayRing = NULL;

	if (filter->inst_data_size) {
		if (!(filter_data = allocmem(filter->inst_data_size)))
			throw MyMemoryError();

		memset(filter_data, 0, filter->inst_data_size);

		if (fd->initProc)
			try {
				if (fd->initProc(this, &g_filterFuncs)) {
					if (filter->deinitProc)
						filter->deinitProc(this, &g_filterFuncs);

					freemem(filter_data);
					throw MyError("Filter failed to initialize.");
				}
			} catch(const MyError& e) {
				throw MyError("Cannot initialize filter '%s': %s", fd->name, e.gets());
			}
	} else
		filter_data = NULL;
}

FilterInstance::~FilterInstance() {
	if (!fNoDeinit)
		if (filter->deinitProc)
			filter->deinitProc(this, &g_filterFuncs);

	freemem(filter_data);
}

FilterInstance *FilterInstance::Clone() {
	FilterInstance *fi = new FilterInstance(*this);

	if (!fi) throw MyMemoryError();

	if (fi->filter_data) {
		fi->filter_data = allocmem(fi->filter->inst_data_size);

		if (!fi->filter_data)
			throw MyMemoryError();

		if (fi->filter->copyProc)
			fi->filter->copyProc(fi, &g_filterFuncs, fi->filter_data);
		else
			memcpy(fi->filter_data, filter_data, fi->filter->inst_data_size);
	}

	return fi;
}

void FilterInstance::Destroy() {
	delete this;
}

void FilterInstance::ForceNoDeinit() {
	fNoDeinit = true;
}

/////////////////////////////////////

int FilterAutoloadModules(int &fail_count) {
	char szFile[MAX_PATH], szFile2[MAX_PATH];
	char *lpszName;
	int success=0, fail=0;

	// splice program name to /PLUGINS.

	if (GetModuleFileName(NULL, szFile, sizeof szFile))
		if (GetFullPathName(szFile, sizeof szFile2, szFile2, &lpszName)) {
			HANDLE h;
			WIN32_FIND_DATA wfd;

			strcpy(lpszName, "plugins\\*");

			h = FindFirstFile(szFile2, &wfd);

			if (h != INVALID_HANDLE_VALUE)
				do {
					int l = strlen(wfd.cFileName);

					if (l>4 && !stricmp(wfd.cFileName+l-4, ".vdf")) {
						try {
							strcpy(lpszName+8, wfd.cFileName);

							FilterLoadModule(szFile2);
							++success;
						} catch(const MyError&) {
							++fail;
						}
					}
				} while(FindNextFile(h, &wfd));

			FindClose(h);
		}

	fail_count	= fail;
	return success;
}

void FilterLoadModule(char *szModule) {
	FilterModule *fm = new FilterModule;

	memset(fm, 0, sizeof(FilterModule));

	try {
		int ver_hi, ver_lo;

		if (!(fm->hInstModule = LoadLibrary(szModule)))
			throw MyWin32Error("Error opening module \"%s\": %%s",GetLastError(),szModule);

		fm->initProc   = (FilterModuleInitProc  )GetProcAddress(fm->hInstModule, "VirtualdubFilterModuleInit2");
		fm->deinitProc = (FilterModuleDeinitProc)GetProcAddress(fm->hInstModule, "VirtualdubFilterModuleDeinit");

		if (!fm->initProc) {
			void *fp = GetProcAddress(fm->hInstModule, "VirtualdubFilterModuleInit");

			if (fp)
				throw MyError(
					"This filter was created for VirtualDub 1.1 or earlier, and is not compatible with version 1.2 or later. "
					"Please contact the author for an updated version.");

			fp = GetProcAddress(fm->hInstModule, "NinaFilterModuleInit");

			if (fp)
				throw MyError("This filter will only work with VirtualDub 2.0 or above.");
		}

		if (!fm->initProc || !fm->deinitProc)
			throw MyError("Module \"%s\" does not contain VirtualDub filters.",szModule);

		ver_hi = VIRTUALDUB_FILTERDEF_VERSION;
		ver_lo = VIRTUALDUB_FILTERDEF_COMPATIBLE;

		if (fm->initProc(fm, &g_filterFuncs, ver_hi, ver_lo))
			throw MyError("Error initializing module \"%s\".",szModule);

		if (ver_hi < VIRTUALDUB_FILTERDEF_COMPATIBLE) {
			fm->deinitProc(fm, &g_filterFuncs);

			throw MyError(
				"This filter was created for an earlier, incompatible filter interface. As a result, it will not "
				"run correctly with this version of VirtualDub. Please contact the author for an updated version.");
		}

		if (ver_lo > VIRTUALDUB_FILTERDEF_VERSION) {
			fm->deinitProc(fm, &g_filterFuncs);

			throw MyError(
				"This filter uses too new of a filter interface!  You'll need to upgrade to a newer version of "
				"VirtualDub to use this filter."
				);
		}

		// link module into module list

		if (filter_module_list) filter_module_list->prev = fm;
		fm->next = filter_module_list;
		filter_module_list = fm;

	} catch(...) {
		if (fm->hInstModule)
			FreeLibrary(fm->hInstModule);

		delete fm;
		throw;
	}
}

void FilterUnloadModule(FilterModule *fm) {
	fm->deinitProc(fm, &g_filterFuncs);
	FreeLibrary(fm->hInstModule);

	if (fm->prev) fm->prev->next = fm->next; else filter_module_list = fm->next;
	if (fm->next) fm->next->prev = fm->prev;

	delete fm;
}

void FilterUnloadAllModules() {
	while(filter_module_list)
		FilterUnloadModule(filter_module_list);

}

FilterDefinition *FilterAdd(FilterModule *fm, FilterDefinition *pfd, int fd_len) {
	FilterDefinition *fd = new FilterDefinition;

	if (fd) {
		memset(fd, 0, sizeof(FilterDefinition));
		memcpy(fd, pfd, min(fd_len, sizeof(FilterDefinition)));
		fd->module	= fm;
		fd->prev	= NULL;
		fd->next	= filter_list;
		if (filter_list) filter_list->prev = fd;
		filter_list = fd;
	}

	return fd;
}

void FilterRemove(FilterDefinition *fd) {
	if (fd->prev) fd->prev->next = fd->next; else filter_list = fd->next;
	if (fd->next) fd->next->prev = fd->prev;
}

//////////////////

typedef struct FilterValueInit {
	LONG lMin, lMax;
	LONG cVal;
	char *title;
} FilterValueInit;

static BOOL APIENTRY FilterValueDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	FilterValueInit *fvi;

    switch (message)
    {
        case WM_INITDIALOG:
			fvi = (FilterValueInit *)lParam;
			SendMessage(hDlg, WM_SETTEXT, 0, (LPARAM)fvi->title);
			SendMessage(GetDlgItem(hDlg, IDC_SLIDER), TBM_SETRANGE, (WPARAM)FALSE, MAKELONG(fvi->lMin, fvi->lMax));
			SendMessage(GetDlgItem(hDlg, IDC_SLIDER), TBM_SETPOS, (WPARAM)TRUE, fvi->cVal); 
			SetWindowLong(hDlg, DWL_USER, (LONG)fvi);
            return (TRUE);

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				fvi = (FilterValueInit *)GetWindowLong(hDlg, DWL_USER);
				fvi->cVal = SendMessage(GetDlgItem(hDlg, IDC_SLIDER), TBM_GETPOS, 0,0);
				EndDialog(hDlg, TRUE);
				return TRUE;
			case IDCANCEL:
	            EndDialog(hDlg, FALSE);  
		        return TRUE;
			}
            break;

		case WM_NOTIFY:
			{
				HWND hwndItem = GetDlgItem(hDlg, IDC_SLIDER);
				SetDlgItemInt(hDlg, IDC_VALUE, SendMessage(hwndItem, TBM_GETPOS, 0,0), FALSE);
			}
			return TRUE;
    }
    return FALSE;
}

LONG FilterGetSingleValue(HWND hWnd, LONG cVal, LONG lMin, LONG lMax, char *title) {
	FilterValueInit fvi;
	char tbuf[128];

	fvi.cVal = cVal;
	fvi.lMin = lMin;
	fvi.lMax = lMax;
	fvi.title = tbuf;
	wsprintf(tbuf, "Filter: %s",title);
	DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_SINGVAR), hWnd, FilterValueDlgProc, (LPARAM)&fvi);
	return fvi.cVal;
}

///////////////////////////////////////////////////////////////////////

#define IDC_POSITION		(500)

BOOL CALLBACK FilterPreview::DlgProc(HWND hdlg, UINT message, UINT wParam, LONG lParam) {
	static HWND hwndTT;
	FilterPreview *fpd = (FilterPreview *)GetWindowLong(hdlg, DWL_USER);
	HDC hdc;
	HWND hwndItem;

	switch(message) {
	case WM_INITDIALOG:
		SetWindowLong(hdlg, DWL_USER, lParam);
		fpd = (FilterPreview *)lParam;

		fpd->bih.biWidth = fpd->bih.biHeight = 0;

		hwndItem = CreateWindow(POSITIONCONTROLCLASS, NULL, WS_CHILD|WS_VISIBLE, 0, 0, 0, 64, hdlg, (HMENU)IDC_POSITION, g_hInst, NULL);

		if (inputSubset) {
			SendMessage(hwndItem, PCM_SETRANGEMIN, FALSE, 0);
			SendMessage(hwndItem, PCM_SETRANGEMAX, TRUE, inputSubset->getTotalFrames()-1);
		} else {
			SendMessage(hwndItem, PCM_SETRANGEMIN, FALSE, inputVideoAVI->lSampleFirst);
			SendMessage(hwndItem, PCM_SETRANGEMAX, TRUE, inputVideoAVI->lSampleLast-1);
		}
		SendMessage(hwndItem, PCM_SETFRAMETYPECB, (WPARAM)&PositionFrameTypeCallback, 0);

		if (!inputVideoAVI->setDecompressedFormat(24))
			if (!inputVideoAVI->setDecompressedFormat(32))
				if (!inputVideoAVI->setDecompressedFormat(16))
					inputVideoAVI->setDecompressedFormat(8);

		hwndTT = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP,
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				hdlg, NULL, g_hInst, NULL);

		SetWindowPos(hwndTT, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);

		{
			TOOLINFO ti;

			ti.cbSize		= sizeof(TOOLINFO);
			ti.uFlags		= TTF_SUBCLASS;
			ti.hwnd			= hdlg;
			ti.uId			= 0;
			ti.rect.left	= 0;
			ti.rect.top		= 0;
			ti.rect.right	= 0;
			ti.rect.left	= 0;
			ti.hinst		= g_hInst;
			ti.lpszText		= LPSTR_TEXTCALLBACK;

			SendMessage(hwndTT, TTM_ADDTOOL, 0, (LPARAM)&ti);
		}

	case WM_USER+1:		// handle new size
		{
			RECT r;
			long w = 320, h = 240;
			bool fResize;
			TOOLINFO ti;
			long oldw = fpd->bih.biWidth;
			long oldh = fpd->bih.biHeight;

			VBitmap(NULL, 320, 240, 32).MakeBitmapHeader(&fpd->bih);

			try {
				BITMAPINFOHEADER *pbih = inputVideoAVI->getImageFormat();
				BITMAPINFOHEADER *pbih2 = inputVideoAVI->getDecompressedFormat();

				CPUTest();

				fpd->filtsys.initLinearChain(
						fpd->pFilterList,
						(Pixel *)((char *)pbih + pbih->biSize),
						pbih2->biWidth,
						pbih2->biHeight,
						pbih2->biBitCount,
						24);

				if (!fpd->filtsys.ReadyFilters(&fpd->fsi)) {
					VBitmap *vbm = fpd->filtsys.OutputBitmap();
					w = vbm->w;
					h = vbm->h;
					vbm->MakeBitmapHeader(&fpd->bih);
				}
			} catch(const MyError& e) {
				fpd->mFailureReason.assign(e);
				InvalidateRect(hdlg, NULL, TRUE);
			}

			fResize = oldw != w || oldh != h;

			// if necessary, resize window

			if (fResize) {
				r.left = r.top = 0;
				r.right = w + 8;
				r.bottom = h + 8 + 64;

				AdjustWindowRect(&r, GetWindowLong(hdlg, GWL_STYLE), FALSE);

				if (message == WM_INITDIALOG) {
					RECT rParent;
					UINT uiFlags = SWP_NOZORDER|SWP_NOACTIVATE;

					GetWindowRect(fpd->hwndParent, &rParent);

					if (rParent.right + 32 >= GetSystemMetrics(SM_CXSCREEN))
						uiFlags |= SWP_NOMOVE;

					SetWindowPos(hdlg, NULL,
							rParent.right + 16,
							rParent.top,
							r.right-r.left, r.bottom-r.top,
							uiFlags);
				} else
					SetWindowPos(hdlg, NULL, 0, 0, r.right-r.left, r.bottom-r.top, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);

				SetWindowPos(GetDlgItem(hdlg, IDC_POSITION), NULL, 0, h+8, w+8, 64, SWP_NOZORDER|SWP_NOACTIVATE);

				InvalidateRect(hdlg, NULL, TRUE);

				ti.cbSize		= sizeof(TOOLINFO);
				ti.uFlags		= 0;
				ti.hwnd			= hdlg;
				ti.uId			= 0;
				ti.rect.left	= 4;
				ti.rect.top		= 4;
				ti.rect.right	= 4 + w;
				ti.rect.bottom	= 4 + h;

				SendMessage(hwndTT, TTM_NEWTOOLRECT, 0, (LPARAM)&ti);

			}

			goto draw_new_frame;
		}

		return TRUE;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;

			hdc = BeginPaint(hdlg, &ps);

			if (fpd->filtsys.isRunning()) {
				Draw3DRect(hdc,	0, 0, fpd->bih.biWidth + 8, fpd->bih.biHeight + 8, FALSE);
				Draw3DRect(hdc,	3, 3, fpd->bih.biWidth + 2, fpd->bih.biHeight + 2, TRUE);
			}

			if (fpd->filtsys.isRunning()) {
				VBitmap *vbm = fpd->filtsys.OutputBitmap();

				DrawDibDraw(fpd->hdd, hdc, 4, 4, -1, -1, &fpd->bih, vbm->data, 0, 0, vbm->w, vbm->h, DDF_UPDATE);
			} else {
				RECT r;

				r.left = r.top = 4;
				r.right = 324;
				r.bottom = 244;

				FillRect(hdc, &r, (HBRUSH)(COLOR_3DFACE+1));
				SetBkMode(hdc, TRANSPARENT);
				SetTextColor(hdc, 0);

				HGDIOBJ hgoFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
				char buf[1024];
				const char *s = fpd->mFailureReason.gets();
				_snprintf(buf, sizeof buf, "Unable to start filters:\n%s", s?s:"(unknown)");
				DrawText(hdc, buf, -1, &r, DT_CENTER|DT_VCENTER|DT_WORDBREAK);
				SelectObject(hdc, hgoFont);
			}

			EndPaint(hdlg, &ps);
		}
		return TRUE;

	case WM_PALETTECHANGED:
		if ((HWND)wParam == hdlg)
			break;
	case WM_QUERYNEWPALETTE:
		if (fpd->filtsys.isRunning()) {
			if (fpd->hdd && (hdc = GetDC(hdlg))) {
				if (DrawDibRealize(fpd->hdd, hdc, FALSE) > 0) {
					VBitmap *vbm = fpd->filtsys.OutputBitmap();

					DrawDibDraw(fpd->hdd, hdc, 4, 4, -1, -1, &fpd->bih, vbm->data, 0, 0, vbm->w, vbm->h, DDF_UPDATE);
				}

				ReleaseDC(hdlg, hdc);
			}
		}
		return TRUE;

	case WM_NOTIFY:
		if (((NMHDR *)lParam)->hwndFrom == hwndTT) {
			TOOLTIPTEXT *pttt = (TOOLTIPTEXT *)lParam;
			POINT pt;

			if (pttt->hdr.code == TTN_NEEDTEXT) {
				VBitmap *vbm = fpd->filtsys.LastBitmap();

				GetCursorPos(&pt);
				ScreenToClient(hdlg, &pt);

				if (fpd->filtsys.isRunning() && pt.x>=4 && pt.y>=4 && pt.x < vbm->w+4 && pt.y < vbm->h+4) {
					pttt->lpszText = pttt->szText;
					wsprintf(pttt->szText, "pixel(%d,%d) = #%06lx", pt.x-4, pt.y-4, 0xffffff&*vbm->Address32(pt.x-4,pt.y-4));
				} else
					pttt->lpszText = "Preview image";
			}
		} else if (((NMHDR *)lParam)->idFrom == IDC_POSITION) {
			goto draw_new_frame;
		}
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDCANCEL) {

			if (fpd->hwndButton)
				SetWindowText(fpd->hwndButton, "Show preview");

			if (fpd->pButtonCallback)
				fpd->pButtonCallback(false, fpd->pvButtonCBData);

			DestroyWindow(hdlg);
			fpd->hdlg = NULL;
			return TRUE;
		} else if (LOWORD(wParam) != IDC_POSITION)
			return TRUE;

		guiPositionHandleCommand(wParam, lParam);

draw_new_frame:

	case WM_USER+0:		// redraw modified frame
		if (!fpd->filtsys.isRunning())
			return TRUE;

		fpd->FetchFrame();

		fpd->filtsys.RunFilters();

		fpd->filtsys.OutputBitmap()->BitBlt(0, 0, fpd->filtsys.LastBitmap(), 0, 0, -1, -1);

		if (fpd->hdd && (hdc = GetDC(hdlg))) {
			VBitmap *out = fpd->filtsys.OutputBitmap();

			DrawDibDraw(fpd->hdd, hdc, 4, 4, out->w, out->h, &fpd->bih, out->data, 0, 0, out->w, out->h, 0);
			ReleaseDC(hdlg, hdc);
		}


		return TRUE;
	}

	return FALSE;
}

FilterPreview::FilterPreview(List *pFilterList, FilterInstance *pfiThisFilter) {
	hdlg					= NULL;
	hdd						= NULL;
	this->pFilterList		= pFilterList;
	this->pfiThisFilter		= pfiThisFilter;
	hwndButton				= NULL;
	pButtonCallback			= NULL;
	pSampleCallback			= NULL;

	if (pFilterList) {
		hdd = DrawDibOpen();

		fsi.lMicrosecsPerFrame = MulDivUnsigned(inputVideoAVI->streamInfo.dwScale, 1000000U, inputVideoAVI->streamInfo.dwRate);

		if (g_dubOpts.video.frameRateNewMicroSecs > 0)
			fsi.lMicrosecsPerFrame = g_dubOpts.video.frameRateNewMicroSecs;

		fsi.lMicrosecsPerSrcFrame = fsi.lMicrosecsPerFrame;
	} else
		hdd = NULL;
}

FilterPreview::~FilterPreview() {
	if (hdlg)
		DestroyWindow(hdlg);

	if (hdd)
		DrawDibClose(hdd);
}

long FilterPreview::FetchFrame() {
	return FetchFrame(SendDlgItemMessage(hdlg, IDC_POSITION, PCM_GETPOS, 0, 0));
}

long FilterPreview::FetchFrame(long lPos) {
	try {
		fsi.lCurrentFrame			= lPos;
		fsi.lCurrentSourceFrame		= inputSubset ? inputSubset->lookupFrame(lPos) : lPos;
		fsi.lSourceFrameMS			= MulDiv(fsi.lCurrentSourceFrame, fsi.lMicrosecsPerSrcFrame, 1000);
		fsi.lDestFrameMS			= MulDiv(fsi.lCurrentFrame, fsi.lMicrosecsPerFrame, 1000);

		if (!inputVideoAVI->getFrame(fsi.lCurrentSourceFrame))
			return -1;

	} catch(const MyError&) {
		return -1;
	}

	filtsys.InputBitmap()->BitBlt(0, 0, &VBitmap(inputVideoAVI->getFrameBuffer(), inputVideoAVI->getDecompressedFormat()), 0, 0, -1, -1);

	return lPos;
}

bool FilterPreview::isPreviewEnabled() {
	return !!pFilterList;
}

void FilterPreview::SetButtonCallback(FilterPreviewButtonCallback pfpbc, void *pvData) {
	this->pButtonCallback	= pfpbc;
	this->pvButtonCBData	= pvData;
}

void FilterPreview::SetSampleCallback(FilterPreviewSampleCallback pfpsc, void *pvData) {
	this->pSampleCallback	= pfpsc;
	this->pvSampleCBData	= pvData;
}

void FilterPreview::InitButton(HWND hwnd) {
	hwndButton = hwnd;

	if (hwnd)
		EnableWindow(hwnd, pFilterList ? TRUE : FALSE);
}

void FilterPreview::Toggle(HWND hwndParent) {
	Display(hwndParent, !hdlg);
}

void FilterPreview::Display(HWND hwndParent, bool fDisplay) {
	if (fDisplay == !!hdlg)
		return;

	if (hdlg) {
		DestroyWindow(hdlg);
		hdlg = NULL;
		UndoSystem();
	} else if (pFilterList) {
		this->hwndParent = hwndParent;
		hdlg = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_PREVIEW), hwndParent, DlgProc, (LPARAM)this);
	}

	if (hwndButton)
		SetWindowText(hwndButton, hdlg ? "Hide preview" : "Show preview");

	if (pButtonCallback)
		pButtonCallback(!!hdlg, pvButtonCBData);
}

void FilterPreview::RedoFrame() {
	if (hdlg)
		SendMessage(hdlg, WM_USER+0, 0, 0);
}

void FilterPreview::RedoSystem() {
	if (hdlg)
		SendMessage(hdlg, WM_USER+1, 0, 0);
}

void FilterPreview::UndoSystem() {
	filtsys.DeinitFilters();
	filtsys.DeallocateBuffers();
}

void FilterPreview::Close() {
	InitButton(NULL);
	if (hdlg)
		Toggle(NULL);
	UndoSystem();
}

bool FilterPreview::SampleCurrentFrame() {
	long pos;

	if (!pFilterList || !hdlg || !pSampleCallback)
		return false;

	if (!filtsys.isRunning()) {
		RedoSystem();

		if (!filtsys.isRunning())
			return false;
	}

	pos = FetchFrame();

	if (pos >= 0) {
		filtsys.RunFilters(pfiThisFilter);
		if (inputSubset)
			pSampleCallback(&pfiThisFilter->src, pos, inputSubset->getTotalFrames(), pvSampleCBData);
		else
			pSampleCallback(&pfiThisFilter->src, pos-inputVideoAVI->lSampleFirst, inputVideoAVI->lSampleLast - inputVideoAVI->lSampleLast, pvSampleCBData);
	}

	RedoFrame();

	return true;
}

///////////////////////

#define FPSAMP_KEYONESEC		(1)
#define	FPSAMP_KEYALL			(2)
#define	FPSAMP_ALL				(3)

static BOOL CALLBACK SampleFramesDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			if (IsDlgButtonChecked(hdlg, IDC_ONEKEYPERSEC))
				EndDialog(hdlg, FPSAMP_KEYONESEC);
			else if (IsDlgButtonChecked(hdlg, IDC_ALLKEYS))
				EndDialog(hdlg, FPSAMP_KEYALL);
			else
				EndDialog(hdlg, FPSAMP_ALL);
			return TRUE;
		case IDCANCEL:
			EndDialog(hdlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

long FilterPreview::SampleFrames() {
	static const char *const szCaptions[]={
		"Sampling one keyframe per second",
		"Sampling keyframes only",
		"Sampling all frames",
	};

	int iMode;
	long lCount = 0;

	if (!pFilterList || !hdlg || !pSampleCallback)
		return -1;

	if (!filtsys.isRunning()) {
		RedoSystem();

		if (!filtsys.isRunning())
			return -1;
	}

	iMode = DialogBox(g_hInst, MAKEINTRESOURCE(IDD_FILTER_SAMPLE), hdlg, SampleFramesDlgProc);

	if (!iMode)
		return -1;

	// Time to do the actual sampling.

	long first, last;

	first = inputVideoAVI->lSampleFirst;
	last = inputVideoAVI->lSampleLast;

	try {
		ProgressDialog pd(hdlg, "Sampling input video", szCaptions[iMode-1], last-first, true);
		long lSample = first;
		long lSecondIncrement = inputVideoAVI->msToSamples(1000)-1;

		pd.setValueFormat("Sampling frame %ld of %ld");

		if (lSecondIncrement<0)
			lSecondIncrement = 0;

		while(lSample>=0 && lSample < last) {
			pd.advance(lSample - first);
			pd.check();

			long lSampleInSubset = lSample;

			if (inputSubset) {
				bool bMasked;
				lSampleInSubset = inputSubset->revLookupFrame(lSample, bMasked);
			}

			if (FetchFrame(lSampleInSubset)>=0) {
				filtsys.RunFilters(pfiThisFilter);
				pSampleCallback(&pfiThisFilter->src, lSample-first, last-first, pvSampleCBData);
				++lCount;
			}

			switch(iMode) {
			case FPSAMP_KEYONESEC:
				lSample += lSecondIncrement;
			case FPSAMP_KEYALL:
				lSample = inputVideoAVI->nextKey(lSample);
				break;
			case FPSAMP_ALL:
				++lSample;
				break;
			}
		}
	} catch(MyUserAbortError e) {

		/* so what? */

	} catch(const MyError& e) {
		e.post(hdlg, "Video sampling error");
	}

	RedoFrame();

	return lCount;
}
