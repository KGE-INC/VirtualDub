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

#include <stdarg.h>
#include <malloc.h>

#include <windows.h>
#include <commctrl.h>
#include <vfw.h>

#include "PositionControl.h"
#include "ProgressDialog.h"
#include "FrameSubset.h"
#include "resource.h"
#include <vd2/system/error.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/filesys.h>
#include <vd2/system/refcount.h>
#include <vd2/system/VDString.h>
#include <vd2/system/vdalloc.h>
#include "gui.h"
#include "oshelper.h"
#include "misc.h"

#define f_FILTER_GLOBALS
#include "filter.h"
#include "filters.h"
#include "optdlg.h"
#include "dub.h"
#include "VideoSource.h"
#include "VideoDisplay.h"

extern vdrefptr<VideoSource> inputVideoAVI;
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

///////////////////////////////////////////////////////////////////////////
//
//	FilterActivation
//
///////////////////////////////////////////////////////////////////////////

FilterActivation::FilterActivation(const FilterActivation& fa, VFBitmap& _dst, VFBitmap& _src, VFBitmap *_last) : dst(_dst), src(_src), last(_last) {
	filter			= fa.filter;
	filter_data		= fa.filter_data;
	x1				= fa.x1;
	y1				= fa.y1;
	x2				= fa.x2;
	y2				= fa.y2;
	pfsi			= fa.pfsi;
}

///////////////////////////////////////////////////////////////////////////
//
//	FilterModuleInstance
//
///////////////////////////////////////////////////////////////////////////

class FilterModuleInstance : public ListNode2<FilterModuleInstance> {
public:
	FilterModuleInstance(const VDStringW& filename);
	~FilterModuleInstance();

	void AttachToModule();
	void ReleaseModule();

	const VDStringW& GetName() const { return mFilename; }
	const FilterModule& GetInfo() const { return mModuleInfo; }

	bool IsLoaded() const { return mModuleInfo.hInstModule != 0; }

protected:
	void Load();
	void Unload();

	VDStringW		mFilename;
	FilterModule	mModuleInfo;
	VDAtomicInt		mRefCount;
};

FilterModuleInstance::FilterModuleInstance(const VDStringW& filename)
	: mFilename(filename)
	, mRefCount(0)
{
	memset(&mModuleInfo, 0, sizeof mModuleInfo);
}

FilterModuleInstance::~FilterModuleInstance() {
	Unload();
}

void FilterModuleInstance::AttachToModule() {
	if (!mRefCount.postinc())
		try {
			Load();
		} catch(...) {
			--mRefCount;
			throw;
		}
}

void FilterModuleInstance::ReleaseModule() {
	VDASSERT(mRefCount > 0);

	if (mRefCount.dectestzero())
		Unload();
}

void FilterModuleInstance::Load() {
	if (!mModuleInfo.hInstModule) {
		VDStringA nameA(VDTextWToA(mFilename));

		{
			VDExternalCodeBracket bracket(mFilename.c_str(), __FILE__, __LINE__);

			mModuleInfo.hInstModule = LoadLibrary(nameA.c_str());
		}

		if (!mModuleInfo.hInstModule)
			throw MyWin32Error("Cannot load filter module \"%s\": %%s", GetLastError(), nameA.c_str());

		try {
			mModuleInfo.initProc   = (FilterModuleInitProc  )GetProcAddress(mModuleInfo.hInstModule, "VirtualdubFilterModuleInit2");
			mModuleInfo.deinitProc = (FilterModuleDeinitProc)GetProcAddress(mModuleInfo.hInstModule, "VirtualdubFilterModuleDeinit");

			if (!mModuleInfo.initProc) {
				void *fp = GetProcAddress(mModuleInfo.hInstModule, "VirtualdubFilterModuleInit");

				if (fp)
					throw MyError(
						"This filter was created for VirtualDub 1.1 or earlier, and is not compatible with version 1.2 or later. "
						"Please contact the author for an updated version.");

				fp = GetProcAddress(mModuleInfo.hInstModule, "NinaFilterModuleInit");

				if (fp)
					throw MyError("This filter will only work with VirtualDub 2.0 or above.");
			}

			if (!mModuleInfo.initProc || !mModuleInfo.deinitProc)
				throw MyError("Module \"%s\" does not contain VirtualDub filters.", nameA.c_str());

			int ver_hi = VIRTUALDUB_FILTERDEF_VERSION;
			int ver_lo = VIRTUALDUB_FILTERDEF_COMPATIBLE;

			if (mModuleInfo.initProc(&mModuleInfo, &g_filterFuncs, ver_hi, ver_lo))
				throw MyError("Error initializing module \"%s\".",nameA.c_str());

			if (ver_hi < VIRTUALDUB_FILTERDEF_COMPATIBLE) {
				mModuleInfo.deinitProc(&mModuleInfo, &g_filterFuncs);

				throw MyError(
					"This filter was created for an earlier, incompatible filter interface. As a result, it will not "
					"run correctly with this version of VirtualDub. Please contact the author for an updated version.");
			}

			if (ver_lo > VIRTUALDUB_FILTERDEF_VERSION) {
				mModuleInfo.deinitProc(&mModuleInfo, &g_filterFuncs);

				throw MyError(
					"This filter uses too new of a filter interface!  You'll need to upgrade to a newer version of "
					"VirtualDub to use this filter."
					);
			}
		} catch(...) {
			FreeLibrary(mModuleInfo.hInstModule);
			mModuleInfo.hInstModule = NULL;
			throw;
		}
	}
}

void FilterModuleInstance::Unload() {
	if (mModuleInfo.hInstModule) {
		mModuleInfo.deinitProc(&mModuleInfo, &g_filterFuncs);

		{
			VDExternalCodeBracket bracket(mFilename.c_str(), __FILE__, __LINE__);
			FreeLibrary(mModuleInfo.hInstModule);
		}

		mModuleInfo.hInstModule = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	FilterDefinitionInstance
//
///////////////////////////////////////////////////////////////////////////

class FilterDefinitionInstance : public ListNode2<FilterDefinitionInstance> {
public:
	FilterDefinitionInstance(FilterModuleInstance *pfm);
	~FilterDefinitionInstance();

	void Assign(const FilterDefinition& def, int len);

	const FilterDefinition& Attach();
	void Detach();

	const FilterDefinition& GetDef() const { return mDef; }
	FilterModuleInstance *GetModule() const { return mpModule; }

	const VDStringA&	GetName() const { return mName; }
	const VDStringA&	GetAuthor() const { return mAuthor; }
	const VDStringA&	GetDescription() const { return mDescription; }

protected:
	FilterModuleInstance *const mpModule;
	FilterDefinition mDef;
	VDAtomicInt			mRefCount;
	VDStringA			mName;
	VDStringA			mAuthor;
	VDStringA			mDescription;
};

FilterDefinitionInstance::FilterDefinitionInstance(FilterModuleInstance *pfm)
	: mpModule(pfm)
	, mRefCount(0)
{
}

FilterDefinitionInstance::~FilterDefinitionInstance() {
	VDASSERT(mRefCount==0);
}

void FilterDefinitionInstance::Assign(const FilterDefinition& def, int len) {
	memset(&mDef, 0, sizeof mDef);
	memcpy(&mDef, &def, std::min<size_t>(sizeof mDef, len));

	mName			= def.name;
	mAuthor			= def.maker ? def.maker : "(internal)";
	mDescription	= def.desc;

	mDef.module		= const_cast<FilterModule *>(&mpModule->GetInfo());
}

const FilterDefinition& FilterDefinitionInstance::Attach() {
	if (mpModule)
		mpModule->AttachToModule();

	++mRefCount;

	return mDef;
}

void FilterDefinitionInstance::Detach() {
	VDASSERT(mRefCount.dec() >= 0);

	if (mpModule)
		mpModule->ReleaseModule();
}

///////////////////////////////////////////////////////////////////////////
//
//	FilterInstance
//
///////////////////////////////////////////////////////////////////////////

FilterInstance::FilterInstance(const FilterInstance& fi)
	: FilterActivation	(fi, realDst, realSrc, &realLast)
	, realSrc			(fi.realSrc)
	, realDst			(fi.realDst)
	, realLast			(fi.realLast)
	, flags				(fi.flags)
	, hbmDst			(fi.hbmDst)
	, hgoDst			(fi.hgoDst)
	, pvDstView			(fi.pvDstView)
	, pvLastView		(fi.pvLastView)
	, srcbuf			(fi.srcbuf)
	, dstbuf			(fi.dstbuf)
	, fNoDeinit			(fi.fNoDeinit)
	, pfsiDelayRing		(NULL)
	, mpFDInst			(fi.mpFDInst)
{
	filter = const_cast<FilterDefinition *>(&fi.mpFDInst->Attach());
}

FilterInstance::FilterInstance(FilterDefinitionInstance *fdi)
	: FilterActivation(realDst, realSrc, &realLast)
	, mpFDInst(fdi)
{
	filter = const_cast<FilterDefinition *>(&fdi->Attach());
	src.hdc = NULL;
	dst.hdc = NULL;
	last->hdc = NULL;
	fNoDeinit = false;
	pfsiDelayRing = NULL;

	if (filter->inst_data_size) {
		if (!(filter_data = allocmem(filter->inst_data_size)))
			throw MyMemoryError();

		memset(filter_data, 0, filter->inst_data_size);

		if (filter->initProc)
			try {
				if (filter->initProc(this, &g_filterFuncs)) {
					if (filter->deinitProc)
						filter->deinitProc(this, &g_filterFuncs);

					freemem(filter_data);
					throw MyError("Filter failed to initialize.");
				}
			} catch(const MyError& e) {
				throw MyError("Cannot initialize filter '%s': %s", filter->name, e.gets());
			}

		mFilterName = VDTextAToW(filter->name);
	} else
		filter_data = NULL;
}

FilterInstance::~FilterInstance() {
	if (!fNoDeinit)
		if (filter->deinitProc)
			filter->deinitProc(this, &g_filterFuncs);

	freemem(filter_data);

	mpFDInst->Detach();
}

FilterInstance *FilterInstance::Clone() {
	FilterInstance *fi = new FilterInstance(*this);

	if (!fi) throw MyMemoryError();

	if (fi->filter_data) {
		fi->filter_data = allocmem(fi->filter->inst_data_size);

		if (!fi->filter_data)
			throw MyMemoryError();

		if (fi->filter->copyProc)
			fi->filter->copyProc(this, &g_filterFuncs, fi->filter_data);
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

///////////////////////////////////////////////////////////////////////////
//
//	Filter global functions
//
///////////////////////////////////////////////////////////////////////////

static ListAlloc<FilterModuleInstance>		g_filterModules;
static ListAlloc<FilterDefinitionInstance>	g_filterDefs;

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

void FilterLoadModule(const char *szModule) {
	// convert to full path
	VDStringW filename(VDGetFullPath(VDTextAToW(szModule, -1)));

	// look for a module by that name
	for(List2<FilterModuleInstance>::fwit it(g_filterModules.begin()); it; ++it) {
		FilterModuleInstance& fmi = *it;

		if (!wcscmp(fmi.GetName().c_str(), filename.c_str()))
			return;		// Module already exists
	}

	vdprotected1("attempting to load module \"%S\"", const wchar_t *, filename.c_str()) {
		// create the module
		vdautoptr<FilterModuleInstance> fmi(new FilterModuleInstance(filename));

		// force the module to load to create filter entries
		g_filterModules.AddTail(fmi);

		try {
			fmi->AttachToModule();
			fmi->ReleaseModule();
		} catch(...) {
			fmi->Remove();
			throw;
		}

		fmi.release();
	}
}

void FilterUnloadModule(FilterModule *fm) {
	VDASSERT(false);
}

void FilterUnloadAllModules() {
	List2<FilterModuleInstance>::fwit it(g_filterModules.begin());

	for(; it; ++it) {
		FilterModuleInstance& fmi = *it;

		VDASSERT(!fmi.IsLoaded());
		fmi.Remove();
		delete &fmi;
	}
}

FilterDefinition *FilterAdd(FilterModule *fm, FilterDefinition *pfd, int fd_len) {
	List2<FilterModuleInstance>::fwit it(g_filterModules.begin());

	for(; it; ++it) {
		FilterModuleInstance& fmi = *it;

		if (&fmi.GetInfo() == fm) {
			List2<FilterDefinitionInstance>::fwit it2(g_filterDefs.begin());

			for(; it2; ++it2) {
				FilterDefinitionInstance& fdi = *it2;

				if (fdi.GetModule() == &fmi && fdi.GetName() == pfd->name) {
					fdi.Assign(*pfd, fd_len);
					return const_cast<FilterDefinition *>(&fdi.GetDef());
				}
			}

			vdautoptr<FilterDefinitionInstance> pfdi(new FilterDefinitionInstance(&fmi));
			pfdi->Assign(*pfd, fd_len);

			const FilterDefinition *pfdi2 = &pfdi->GetDef();
			g_filterDefs.AddTail(pfdi.release());

			return const_cast<FilterDefinition *>(pfdi2);
		}
	}

	return NULL;
}

void FilterAddBuiltin(const FilterDefinition *pfd) {
	vdautoptr<FilterDefinitionInstance> fdi(new FilterDefinitionInstance(NULL));
	fdi->Assign(*pfd, sizeof(FilterDefinition));

	g_filterDefs.AddTail(fdi.release());
}

void FilterRemove(FilterDefinition *fd) {
#if 0
	List2<FilterDefinitionInstance>::fwit it(g_filterDefs.begin());

	for(; it; ++it) {
		FilterDefinitionInstance& fdi = *it;

		if (&fdi.GetDef() == fd) {
			fdi.Remove();
			delete &fdi;
			return;
		}
	}
#endif
}

void FilterEnumerateFilters(std::list<FilterBlurb>& blurbs) {
	List2<FilterDefinitionInstance>::fwit it(g_filterDefs.begin());

	for(; it; ++it) {
		FilterDefinitionInstance& fd = *it;

		blurbs.push_back(FilterBlurb());
		FilterBlurb& fb = blurbs.back();

		fb.key			= &fd;
		fb.name			= fd.GetName();
		fb.author		= fd.GetAuthor();
		fb.description	= fd.GetDescription();
	}
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

BOOL CALLBACK FilterPreview::StaticDlgProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam) {
	FilterPreview *fpd = (FilterPreview *)GetWindowLong(hdlg, DWL_USER);

	if (message == WM_INITDIALOG) {
		SetWindowLongPtr(hdlg, DWL_USER, lParam);
		fpd = (FilterPreview *)lParam;
		fpd->hdlg = hdlg;
	}

	return fpd && fpd->DlgProc(message, wParam, lParam);
}

BOOL FilterPreview::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch(message) {
	case WM_INITDIALOG:
		OnInit();
		OnVideoResize(true);
		return TRUE;

	case WM_USER+1:		// handle new size
		OnVideoResize(false);
		return TRUE;

	case WM_SIZE:
		OnResize();
		return TRUE;

	case WM_PAINT:
		OnPaint();
		return TRUE;

	case WM_NOTIFY:
		if (((NMHDR *)lParam)->hwndFrom == mhwndToolTip) {
			TOOLTIPTEXT *pttt = (TOOLTIPTEXT *)lParam;
			POINT pt;

			if (pttt->hdr.code == TTN_NEEDTEXT) {
				VBitmap *vbm = filtsys.LastBitmap();

				GetCursorPos(&pt);
				ScreenToClient(hdlg, &pt);

				if (filtsys.isRunning() && pt.x>=4 && pt.y>=4 && pt.x < vbm->w+4 && pt.y < vbm->h+4) {
					pttt->lpszText = pttt->szText;
					wsprintf(pttt->szText, "pixel(%d,%d) = #%06lx", pt.x-4, pt.y-4, 0xffffff&*vbm->Address32(pt.x-4,pt.y-4));
				} else
					pttt->lpszText = "Preview image";
			}
		} else if (((NMHDR *)lParam)->idFrom == IDC_POSITION) {
			OnVideoRedraw();
		}
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDCANCEL) {

			if (hwndButton)
				SetWindowText(hwndButton, "Show preview");

			if (pButtonCallback)
				pButtonCallback(false, pvButtonCBData);

			DestroyWindow(hdlg);
			hdlg = NULL;
			return TRUE;
		} else if (LOWORD(wParam) != IDC_POSITION)
			return TRUE;

		guiPositionHandleCommand(wParam, lParam);
		break;

	case WM_USER+0:		// redraw modified frame
		OnVideoRedraw();
		return TRUE;
	}

	return FALSE;
}

void FilterPreview::OnInit() {
	bih.biWidth = bih.biHeight = 0;

	mhwndPosition = CreateWindow(POSITIONCONTROLCLASS, NULL, WS_CHILD|WS_VISIBLE, 0, 0, 0, 64, hdlg, (HMENU)IDC_POSITION, g_hInst, NULL);

	SendMessage(mhwndPosition, PCM_SETRANGEMIN, FALSE, 0);
	SendMessage(mhwndPosition, PCM_SETRANGEMAX, TRUE, inputSubset->getTotalFrames()-1);
	SendMessage(mhwndPosition, PCM_SETFRAMETYPECB, (WPARAM)&PositionFrameTypeCallback, 0);

	if (!inputVideoAVI->setDecompressedFormat(24))
		if (!inputVideoAVI->setDecompressedFormat(32))
			if (!inputVideoAVI->setDecompressedFormat(16))
				inputVideoAVI->setDecompressedFormat(8);

	mhwndDisplay = CreateWindow(VIDEODISPLAYCONTROLCLASS, NULL, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 0, 0, hdlg, NULL, g_hInst, NULL);
	if (mhwndDisplay)
		mpDisplay = VDGetIVideoDisplay(mhwndDisplay);

	mhwndToolTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			hdlg, NULL, g_hInst, NULL);

	SetWindowPos(mhwndToolTip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);

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

	SendMessage(mhwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ti);
}

void FilterPreview::OnResize() {
	RECT r;

	GetClientRect(hdlg, &r);

	SetWindowPos(mhwndPosition, NULL, 0, r.bottom - 64, r.right, 64, SWP_NOZORDER|SWP_NOACTIVATE);
	SetWindowPos(mhwndDisplay, NULL, 4, 4, r.right - 8, r.bottom - 72, SWP_NOZORDER|SWP_NOACTIVATE);

	InvalidateRect(hdlg, NULL, TRUE);
}

void FilterPreview::OnPaint() {
	PAINTSTRUCT ps;

	HDC hdc = BeginPaint(hdlg, &ps);

	if (!hdc)
		return;

	if (filtsys.isRunning()) {
		RECT r;

		GetClientRect(hdlg, &r);

		Draw3DRect(hdc,	0, 0, r.right, r.bottom - 64, FALSE);
		Draw3DRect(hdc,	3, 3, r.right - 6, r.bottom - 70, TRUE);
	}

	if (!filtsys.isRunning()) {
		RECT r;

		r.left = r.top = 4;
		r.right = 324;
		r.bottom = 244;

		FillRect(hdc, &r, (HBRUSH)(COLOR_3DFACE+1));
		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, 0);

		HGDIOBJ hgoFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
		char buf[1024];
		const char *s = mFailureReason.gets();
		_snprintf(buf, sizeof buf, "Unable to start filters:\n%s", s?s:"(unknown)");
		DrawText(hdc, buf, -1, &r, DT_CENTER|DT_VCENTER|DT_WORDBREAK);
		SelectObject(hdc, hgoFont);
	}

	EndPaint(hdlg, &ps);
}

void FilterPreview::OnVideoResize(bool bInitial) {
	RECT r;
	long w = 320, h = 240;
	bool fResize;
	TOOLINFO ti;
	long oldw = bih.biWidth;
	long oldh = bih.biHeight;

	VBitmap(NULL, 320, 240, 32).MakeBitmapHeader(&bih);

	try {
		BITMAPINFOHEADER *pbih = inputVideoAVI->getImageFormat();
		BITMAPINFOHEADER *pbih2 = inputVideoAVI->getDecompressedFormat();

		CPUTest();

		filtsys.initLinearChain(
				pFilterList,
				(Pixel *)((char *)pbih + pbih->biSize),
				pbih2->biWidth,
				pbih2->biHeight,
				pbih2->biBitCount,
				24);

		if (!filtsys.ReadyFilters(&fsi)) {
			VBitmap *vbm = filtsys.OutputBitmap();
			w = vbm->w;
			h = vbm->h;
			vbm->MakeBitmapHeader(&bih);
		}
	} catch(const MyError& e) {
		mFailureReason.assign(e);
		InvalidateRect(hdlg, NULL, TRUE);
	}

	fResize = oldw != w || oldh != h;

	// if necessary, resize window

	if (fResize) {
		r.left = r.top = 0;
		r.right = w + 8;
		r.bottom = h + 8 + 64;

		AdjustWindowRect(&r, GetWindowLong(hdlg, GWL_STYLE), FALSE);

		if (bInitial) {
			RECT rParent;
			UINT uiFlags = SWP_NOZORDER|SWP_NOACTIVATE;

			GetWindowRect(hwndParent, &rParent);

			if (rParent.right + 32 >= GetSystemMetrics(SM_CXSCREEN))
				uiFlags |= SWP_NOMOVE;

			SetWindowPos(hdlg, NULL,
					rParent.right + 16,
					rParent.top,
					r.right-r.left, r.bottom-r.top,
					uiFlags);
		} else
			SetWindowPos(hdlg, NULL, 0, 0, r.right-r.left, r.bottom-r.top, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);

		ti.cbSize		= sizeof(TOOLINFO);
		ti.uFlags		= 0;
		ti.hwnd			= hdlg;
		ti.uId			= 0;
		ti.rect.left	= 4;
		ti.rect.top		= 4;
		ti.rect.right	= 4 + w;
		ti.rect.bottom	= 4 + h;

		SendMessage(mhwndToolTip, TTM_NEWTOOLRECT, 0, (LPARAM)&ti);

	}

	OnVideoRedraw();
}

void FilterPreview::OnVideoRedraw() {
	if (!filtsys.isRunning())
		return;

	FetchFrame();

	try {
		filtsys.RunFilters();
		filtsys.OutputBitmap()->BitBlt(0, 0, filtsys.LastBitmap(), 0, 0, -1, -1);

		if (mpDisplay) {
			VBitmap *out = filtsys.OutputBitmap();

			mpDisplay->SetSource((char *)out->data + out->pitch*(out->h - 1), -out->pitch, out->w, out->h,
					  out->depth == 16 ? IVDVideoDisplay::kFormatRGB1555
					: out->depth == 24 ? IVDVideoDisplay::kFormatRGB888
					:                   IVDVideoDisplay::kFormatRGB8888);
			mpDisplay->Update(IVDVideoDisplay::kAllFields);
		}
	} catch(const MyError& e) {
		mFailureReason.assign(e);
		InvalidateRect(hdlg, NULL, TRUE);
	}
}

FilterPreview::FilterPreview(List *pFilterList, FilterInstance *pfiThisFilter)
	: mhwndPosition(NULL)
	, mhwndDisplay(NULL)
	, mpDisplay(NULL)
{
	hdlg					= NULL;
	this->pFilterList		= pFilterList;
	this->pfiThisFilter		= pfiThisFilter;
	hwndButton				= NULL;
	pButtonCallback			= NULL;
	pSampleCallback			= NULL;

	if (pFilterList) {
		fsi.lMicrosecsPerFrame = VDRoundToInt(1000000.0 / inputVideoAVI->getRate().asDouble());

		if (g_dubOpts.video.frameRateNewMicroSecs > 0)
			fsi.lMicrosecsPerFrame = g_dubOpts.video.frameRateNewMicroSecs;

		fsi.lMicrosecsPerSrcFrame = fsi.lMicrosecsPerFrame;
	}
}

FilterPreview::~FilterPreview() {
	if (hdlg)
		DestroyWindow(hdlg);
}

long FilterPreview::FetchFrame() {
	return FetchFrame(SendMessage(mhwndPosition, PCM_GETPOS, 0, 0));
}

long FilterPreview::FetchFrame(long lPos) {
	try {
		const VDFraction frameRate(inputVideoAVI->getRate());

		fsi.lCurrentFrame			= lPos;
		fsi.lCurrentSourceFrame		= inputSubset ? inputSubset->lookupFrame(lPos) : lPos;
		fsi.lSourceFrameMS			= (long)frameRate.scale64ir(fsi.lCurrentSourceFrame * (sint64)1000);
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
		hdlg = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_PREVIEW), hwndParent, StaticDlgProc, (LPARAM)this);
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
		pSampleCallback(&pfiThisFilter->src, pos, inputSubset->getTotalFrames(), pvSampleCBData);
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

	first = inputVideoAVI->getStart();
	last = inputVideoAVI->getEnd();

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
