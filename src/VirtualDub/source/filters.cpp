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
#include <ctype.h>
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
#include <vd2/system/w32assist.h>
#include <vd2/Riza/display.h>
#include "gui.h"
#include "oshelper.h"
#include "misc.h"
#include "plugins.h"

#define f_FILTER_GLOBALS
#include "filter.h"
#include "filters.h"
#include "optdlg.h"
#include "dub.h"
#include "project.h"
#include "timeline.h"
#include "VideoSource.h"

extern vdrefptr<VideoSource> inputVideoAVI;

extern HINSTANCE	g_hInst;
extern "C" unsigned long version_num;
extern VDProject *g_project;

extern const VDScriptObject obj_VDVFiltInst;

extern IVDPositionControlCallback *VDGetPositionControlCallbackTEMP();

List			g_listFA;

FilterSystem	filters;

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
	VBitmap tmp;
	pvtbls->pvtblVBitmap = *(void **)&tmp;
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
//	FilterDefinitionInstance
//
///////////////////////////////////////////////////////////////////////////

class FilterDefinitionInstance : public ListNode2<FilterDefinitionInstance> {
public:
	FilterDefinitionInstance(VDExternalModule *pfm);
	~FilterDefinitionInstance();

	void Assign(const FilterDefinition& def, int len);

	const FilterDefinition& Attach();
	void Detach();

	const FilterDefinition& GetDef() const { return mDef; }
	VDExternalModule	*GetModule() const { return mpExtModule; }

	const VDStringA&	GetName() const { return mName; }
	const VDStringA&	GetAuthor() const { return mAuthor; }
	const VDStringA&	GetDescription() const { return mDescription; }

protected:
	VDExternalModule	*mpExtModule;
	FilterDefinition	mDef;
	VDAtomicInt			mRefCount;
	VDStringA			mName;
	VDStringA			mAuthor;
	VDStringA			mDescription;
};

FilterDefinitionInstance::FilterDefinitionInstance(VDExternalModule *pfm)
	: mpExtModule(pfm)
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

	mDef.module		= const_cast<FilterModule *>(&mpExtModule->GetFilterModuleInfo());
}

const FilterDefinition& FilterDefinitionInstance::Attach() {
	if (mpExtModule)
		mpExtModule->Lock();

	++mRefCount;

	return mDef;
}

void FilterDefinitionInstance::Detach() {
	VDASSERT(mRefCount.dec() >= 0);

	if (mpExtModule)
		mpExtModule->Unlock();
}

///////////////////////////////////////////////////////////////////////////
//
//	FilterInstanceAutoDeinit
//
///////////////////////////////////////////////////////////////////////////

class FilterInstanceAutoDeinit : public vdrefcounted<IVDRefCount> {};

///////////////////////////////////////////////////////////////////////////
//
//	FilterInstance
//
///////////////////////////////////////////////////////////////////////////

FilterInstance::FilterInstance(const FilterInstance& fi)
	: FilterActivation	(fi, (VFBitmap&)realDst, (VFBitmap&)realSrc, (VFBitmap*)&realLast)
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
	, pfsiDelayRing		(NULL)
	, mpFDInst			(fi.mpFDInst)
	, mpAutoDeinit		(fi.mpAutoDeinit)
	, mScriptFunc		(fi.mScriptFunc)
	, mScriptObj		(fi.mScriptObj)
	, mpAlphaCurve		(fi.mpAlphaCurve)
{
	if (mpAutoDeinit)
		mpAutoDeinit->AddRef();

	filter = const_cast<FilterDefinition *>(&fi.mpFDInst->Attach());
}

FilterInstance::FilterInstance(FilterDefinitionInstance *fdi)
	: FilterActivation((VFBitmap&)realDst, (VFBitmap&)realSrc, (VFBitmap*)&realLast)
	, mpFDInst(fdi)
	, mpAutoDeinit(NULL)
{
	filter = const_cast<FilterDefinition *>(&fdi->Attach());
	src.hdc = NULL;
	dst.hdc = NULL;
	last->hdc = NULL;
	pfsiDelayRing = NULL;

	if (filter->inst_data_size) {
		if (!(filter_data = allocmem(filter->inst_data_size)))
			throw MyMemoryError();

		memset(filter_data, 0, filter->inst_data_size);

		if (filter->initProc)
			try {
				vdrefptr<FilterInstanceAutoDeinit> autoDeinit;
				
				if (!filter->copyProc && filter->deinitProc)
					autoDeinit = new FilterInstanceAutoDeinit;

				if (filter->initProc(this, &g_filterFuncs)) {
					if (filter->deinitProc)
						filter->deinitProc(this, &g_filterFuncs);

					freemem(filter_data);
					throw MyError("Filter failed to initialize.");
				}

				mpAutoDeinit = autoDeinit.release();
			} catch(const MyError& e) {
				throw MyError("Cannot initialize filter '%s': %s", filter->name, e.gets());
			}

		mFilterName = VDTextAToW(filter->name);
	} else
		filter_data = NULL;


	// initialize script object
	mScriptObj.mpName		= NULL;
	mScriptObj.obj_list		= NULL;
	mScriptObj.Lookup		= NULL;
	mScriptObj.func_list	= NULL;
	mScriptObj.pNextObject	= &obj_VDVFiltInst;

	if (filter->script_obj) {
		const ScriptFunctionDef *pf = filter->script_obj->func_list;

		if (pf) {
			for(; pf->func_ptr; ++pf) {
				VDScriptFunctionDef def;

				def.arg_list	= pf->arg_list;
				def.name		= pf->name;

				switch(def.arg_list[0]) {
				default:
				case '0':
					def.func_ptr	= ScriptFunctionThunkVoid;
					break;
				case 'i':
					def.func_ptr	= ScriptFunctionThunkInt;
					break;
				case 'v':
					def.func_ptr	= ScriptFunctionThunkVariadic;
					break;
				}

				mScriptFunc.push_back(def);
			}

			VDScriptFunctionDef def_end = {NULL};
			mScriptFunc.push_back(def_end);

			mScriptObj.func_list	= &mScriptFunc[0];
		}
	}
}

FilterInstance::~FilterInstance() {
	if (mpAutoDeinit) {
		if (!mpAutoDeinit->Release())
			filter->deinitProc(this, &g_filterFuncs);
		mpAutoDeinit = NULL;
	} else if (filter->deinitProc) {
		filter->deinitProc(this, &g_filterFuncs);
	}

	freemem(filter_data);

	mpFDInst->Detach();
}

FilterInstance *FilterInstance::Clone() {
	FilterInstance *fi = new FilterInstance(*this);

	if (!fi) throw MyMemoryError();

	if (fi->filter_data) {
		fi->filter_data = allocmem(fi->filter->inst_data_size);

		if (!fi->filter_data) {
			delete fi;
			throw MyMemoryError();
		}

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

void FilterInstance::ConvertParameters(CScriptValue *dst, const VDScriptValue *src, int argc) {
	int idx = 0;
	while(argc-->0) {
		const VDScriptValue& v = *src++;

		switch(v.type) {
			case VDScriptValue::T_INT:
				*dst = CScriptValue(v.asInt());
				break;
			case VDScriptValue::T_STR:
				*dst = CScriptValue(v.asString());
				break;
			case VDScriptValue::T_LONG:
				*dst = CScriptValue(v.asLong());
				break;
			case VDScriptValue::T_DOUBLE:
				*dst = CScriptValue(v.asDouble());
				break;
			case VDScriptValue::T_VOID:
				*dst = CScriptValue();
				break;
			default:
				throw MyError("Script: Parameter %d is not of a supported type for filter configuration functions.");
				break;
		}

		++dst;
		++idx;
	}
}

void FilterInstance::ConvertValue(VDScriptValue& dst, const CScriptValue& v) {
	switch(v.type) {
		case VDScriptValue::T_INT:
			dst = VDScriptValue(v.asInt());
			break;
		case VDScriptValue::T_STR:
			dst = VDScriptValue(v.asString());
			break;
		case VDScriptValue::T_LONG:
			dst = VDScriptValue(v.asLong());
			break;
		case VDScriptValue::T_DOUBLE:
			dst = VDScriptValue(v.asDouble());
			break;
		case VDScriptValue::T_VOID:
		default:
			dst = VDScriptValue();
			break;
	}
}

namespace {
	class VDScriptInterpreterAdapter : public IScriptInterpreter{
	public:
		VDScriptInterpreterAdapter(IVDScriptInterpreter *p) : mpInterp(p) {}

		void ScriptError(int e) {
			mpInterp->ScriptError(e);
		}

		char** AllocTempString(long l) {
			return mpInterp->AllocTempString(l);
		}

	protected:
		IVDScriptInterpreter *mpInterp;
	};
}

void FilterInstance::ScriptFunctionThunkVoid(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	FilterInstance *const thisPtr = static_cast<FilterInstance *>((FilterActivation *)argv[-1].asObjectPtr());
	int funcidx = isi->GetCurrentMethod() - &thisPtr->mScriptFunc[0];

	const ScriptFunctionDef& fd = thisPtr->filter->script_obj->func_list[funcidx];
	ScriptVoidFunctionPtr pf = (ScriptVoidFunctionPtr)fd.func_ptr;

	std::vector<CScriptValue> params(argc ? argc : 1);

	ConvertParameters(&params[0], argv, argc);

	VDScriptInterpreterAdapter adapt(isi);
	pf(&adapt, static_cast<FilterActivation *>(thisPtr), &params[0], argc);
}

void FilterInstance::ScriptFunctionThunkInt(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	FilterInstance *const thisPtr = static_cast<FilterInstance *>((FilterActivation *)argv[-1].asObjectPtr());
	int funcidx = isi->GetCurrentMethod() - &thisPtr->mScriptFunc[0];

	const ScriptFunctionDef& fd = thisPtr->filter->script_obj->func_list[funcidx];
	ScriptIntFunctionPtr pf = (ScriptIntFunctionPtr)fd.func_ptr;

	std::vector<CScriptValue> params(argc ? argc : 1);

	ConvertParameters(&params[0], argv, argc);

	VDScriptInterpreterAdapter adapt(isi);
	int rval = pf(&adapt, static_cast<FilterActivation *>(thisPtr), &params[0], argc);

	argv[0] = rval;
}

void FilterInstance::ScriptFunctionThunkVariadic(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc) {
	FilterInstance *const thisPtr = static_cast<FilterInstance *>((FilterActivation *)argv[-1].asObjectPtr());
	int funcidx = isi->GetCurrentMethod() - &thisPtr->mScriptFunc[0];

	const ScriptFunctionDef& fd = thisPtr->filter->script_obj->func_list[funcidx];
	ScriptFunctionPtr pf = fd.func_ptr;

	std::vector<CScriptValue> params(argc ? argc : 1);

	ConvertParameters(&params[0], argv, argc);

	VDScriptInterpreterAdapter adapt(isi);
	CScriptValue v(pf(&adapt, static_cast<FilterActivation *>(thisPtr), &params[0], argc));

	ConvertValue(argv[0], v);
}

///////////////////////////////////////////////////////////////////////////
//
//	Filter global functions
//
///////////////////////////////////////////////////////////////////////////

static ListAlloc<FilterDefinitionInstance>	g_filterDefs;

FilterDefinition *FilterAdd(FilterModule *fm, FilterDefinition *pfd, int fd_len) {
	VDExternalModule *pExtModule = VDGetExternalModuleByFilterModule(fm);

	if (pExtModule) {
		List2<FilterDefinitionInstance>::fwit it2(g_filterDefs.begin());

		for(; it2; ++it2) {
			FilterDefinitionInstance& fdi = *it2;

			if (fdi.GetModule() == pExtModule && fdi.GetName() == pfd->name) {
				fdi.Assign(*pfd, fd_len);
				return const_cast<FilterDefinition *>(&fdi.GetDef());
			}
		}

		vdautoptr<FilterDefinitionInstance> pfdi(new FilterDefinitionInstance(pExtModule));
		pfdi->Assign(*pfd, fd_len);

		const FilterDefinition *pfdi2 = &pfdi->GetDef();
		g_filterDefs.AddTail(pfdi.release());

		return const_cast<FilterDefinition *>(pfdi2);
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

struct FilterValueInit {
	LONG lMin, lMax;
	LONG cVal;
	const char *title;
	IFilterPreview *ifp;
	void (*mpUpdateFunction)(long value, void *data);
	void *mpUpdateFunctionData;
};

static INT_PTR CALLBACK FilterValueDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	FilterValueInit *fvi;

    switch (message)
    {
        case WM_INITDIALOG:
			fvi = (FilterValueInit *)lParam;
			SendMessage(hDlg, WM_SETTEXT, 0, (LPARAM)fvi->title);
			SendMessage(GetDlgItem(hDlg, IDC_SLIDER), TBM_SETRANGE, (WPARAM)FALSE, MAKELONG(fvi->lMin, fvi->lMax));
			SendMessage(GetDlgItem(hDlg, IDC_SLIDER), TBM_SETPOS, (WPARAM)TRUE, fvi->cVal); 
			SetWindowLongPtr(hDlg, DWLP_USER, (LONG)fvi);

			if (fvi->ifp) {
				HWND hwndPreviewButton = GetDlgItem(hDlg, IDC_PREVIEW);
				EnableWindow(hwndPreviewButton, TRUE);
				fvi->ifp->InitButton(hwndPreviewButton);
			}
            return (TRUE);

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				fvi = (FilterValueInit *)GetWindowLongPtr(hDlg, DWLP_USER);
				fvi->cVal = SendMessage(GetDlgItem(hDlg, IDC_SLIDER), TBM_GETPOS, 0,0);
				EndDialog(hDlg, TRUE);
				break;
			case IDCANCEL:
	            EndDialog(hDlg, FALSE);  
				break;
			case IDC_PREVIEW:
				fvi = (FilterValueInit *)GetWindowLongPtr(hDlg, DWLP_USER);
				if (fvi->ifp)
					fvi->ifp->Toggle(hDlg);
				break;
			default:
				return FALSE;
			}
			SetWindowLongPtr(hDlg, DWLP_MSGRESULT, 0);
			return TRUE;

		case WM_HSCROLL:
			if (lParam) {
				HWND hwndScroll = (HWND)lParam;

				fvi = (FilterValueInit *)GetWindowLongPtr(hDlg, DWLP_USER);
				fvi->cVal = SendMessage(hwndScroll, TBM_GETPOS, 0, 0);

				if (fvi->mpUpdateFunction)
					fvi->mpUpdateFunction(fvi->cVal, fvi->mpUpdateFunctionData);

				if (fvi->ifp)
					fvi->ifp->RedoFrame();
			}
			SetWindowLongPtr(hDlg, DWLP_MSGRESULT, 0);
			return TRUE;
    }
    return FALSE;
}

LONG FilterGetSingleValue(HWND hWnd, LONG cVal, LONG lMin, LONG lMax, char *title, IFilterPreview *ifp, void (*pUpdateFunction)(long value, void *data), void *pUpdateFunctionData) {
	FilterValueInit fvi;
	VDStringA tbuf;
	tbuf.sprintf("Filter: %s", title);

	fvi.cVal = cVal;
	fvi.lMin = lMin;
	fvi.lMax = lMax;
	fvi.title = tbuf.c_str();
	fvi.ifp = ifp;
	fvi.mpUpdateFunction = pUpdateFunction;
	fvi.mpUpdateFunctionData = pUpdateFunctionData;

	if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_SINGVAR), hWnd, FilterValueDlgProc, (LPARAM)&fvi))
		return fvi.cVal;

	return cVal;
}

///////////////////////////////////////////////////////////////////////

#define IDC_FILTDLG_POSITION		(500)

INT_PTR CALLBACK FilterPreview::StaticDlgProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam) {
	FilterPreview *fpd = (FilterPreview *)GetWindowLongPtr(hdlg, DWLP_USER);

	if (message == WM_INITDIALOG) {
		SetWindowLongPtr(hdlg, DWLP_USER, lParam);
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

	case WM_DESTROY:
		if (mpDisplay) {
			mpDisplay->Destroy();
			mpDisplay = NULL;
			mhwndDisplay = NULL;
		}
		mDlgNode.Remove();
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
		} else if (((NMHDR *)lParam)->idFrom == IDC_FILTDLG_POSITION) {
			OnVideoRedraw();
		}
		return TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_FILTDLG_POSITION) {
			VDTranslatePositionCommand(hdlg, wParam, lParam);
			return TRUE;
		}
		return OnCommand(LOWORD(wParam));

	case WM_USER+0:		// redraw modified frame
		OnVideoRedraw();
		return TRUE;
	}

	return FALSE;
}

void FilterPreview::OnInit() {
	mpTimeline = &g_project->GetTimeline();

	bih.biWidth = bih.biHeight = 0;

	mhwndPosition = CreateWindow(POSITIONCONTROLCLASS, NULL, WS_CHILD|WS_VISIBLE, 0, 0, 0, 64, hdlg, (HMENU)IDC_FILTDLG_POSITION, g_hInst, NULL);
	mpPosition = VDGetIPositionControl((VDGUIHandle)mhwndPosition);

	mpPosition->SetRange(0, mpTimeline->GetLength());
	mpPosition->SetFrameTypeCallback(VDGetPositionControlCallbackTEMP());

	if (!inputVideoAVI->setDecompressedFormat(24))
		if (!inputVideoAVI->setDecompressedFormat(32))
			if (!inputVideoAVI->setDecompressedFormat(16))
				inputVideoAVI->setDecompressedFormat(8);

	mhwndDisplay = (HWND)VDCreateDisplayWindowW32(0, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 0, 0, (VDGUIHandle)hdlg);
	if (mhwndDisplay)
		mpDisplay = VDGetIVideoDisplay((VDGUIHandle)mhwndDisplay);

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

	mDlgNode.hdlg = hdlg;
	mDlgNode.mhAccel = LoadAccelerators(g_hInst, MAKEINTRESOURCE(IDR_PREVIEW_KEYS));
	guiAddModelessDialog(&mDlgNode);
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
	} else {
		RECT r;

		GetWindowRect(mhwndDisplay, &r);
		MapWindowPoints(NULL, hdlg, (LPPOINT)&r, 2);

		FillRect(hdc, &r, (HBRUSH)(COLOR_3DFACE+1));
		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, 0);

		HGDIOBJ hgoFont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
		char buf[1024];
		const char *s = mFailureReason.gets();
		_snprintf(buf, sizeof buf, "Unable to start filters:\n%s", s?s:"(unknown)");

		RECT r2 = r;
		DrawText(hdc, buf, -1, &r2, DT_CENTER|DT_WORDBREAK|DT_NOPREFIX|DT_CALCRECT);

		int text_h = r2.bottom - r2.top;
		int space_h = r.bottom - r.top;
		if (text_h < space_h)
			r.top += (space_h - text_h) >> 1;

		DrawText(hdc, buf, -1, &r, DT_CENTER|DT_WORDBREAK|DT_NOPREFIX);
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

		filtsys.initLinearChain(
				pFilterList,
				(Pixel *)((char *)pbih + pbih->biSize),
				pbih2->biWidth,
				abs(pbih2->biHeight),
				0);

		if (!filtsys.ReadyFilters(fsi)) {
			VBitmap *vbm = filtsys.OutputBitmap();
			w = vbm->w;
			h = vbm->h;
			vbm->MakeBitmapHeader(&bih);
		}
	} catch(const MyError& e) {
		mpDisplay->Reset();
		ShowWindow(mhwndDisplay, SW_HIDE);
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

	bool bSuccessful = FetchFrame() >= 0;

	try {
		if (bSuccessful)
			filtsys.RunFilters(fsi);
		else {
			const VBitmap& vb = *filtsys.LastBitmap();
			uint32 color = GetSysColor(COLOR_3DFACE);

			vb.RectFill(0, 0, vb.w, vb.h, ((color>>16)&0xff) + (color&0xff00) + ((color&0xff)<<16));
		}

		if (mpDisplay) {
			ShowWindow(mhwndDisplay, SW_SHOW);
			mpDisplay->SetSource(false, VDAsPixmap(*filtsys.LastBitmap()));
			mpDisplay->Update(IVDVideoDisplay::kAllFields);
		}
	} catch(const MyError& e) {
		mpDisplay->Reset();
		ShowWindow(mhwndDisplay, SW_HIDE);
		mFailureReason.assign(e);
		InvalidateRect(hdlg, NULL, TRUE);
		UndoSystem();
	}
}

bool FilterPreview::OnCommand(UINT cmd) {
	switch(cmd) {
	case IDCANCEL:
		if (pButtonCallback)
			pButtonCallback(false, pvButtonCBData);

		DestroyWindow(hdlg);
		hdlg = NULL;

		UpdateButton();
		return true;

	case ID_EDIT_JUMPTO:
		{
			extern VDPosition VDDisplayJumpToPositionDialog(VDGUIHandle hParent, VDPosition currentFrame, VideoSource *pVS, const VDFraction& realRate);

			VDPosition pos = VDDisplayJumpToPositionDialog((VDGUIHandle)hdlg, mpPosition->GetPosition(), inputVideoAVI, g_project->GetInputFrameRate());

			mpPosition->SetPosition(pos);
			OnVideoRedraw();
		}
		return true;

	default:
		if (VDHandleTimelineCommand(mpPosition, mpTimeline, cmd)) {
			OnVideoRedraw();
			return true;
		}
	}

	return false;
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
		fsi.flags = FilterStateInfo::kStatePreview;
	}
}

FilterPreview::~FilterPreview() {
	if (hdlg)
		DestroyWindow(hdlg);
}

VDPosition FilterPreview::FetchFrame() {
	return FetchFrame(mpPosition->GetPosition());
}

VDPosition FilterPreview::FetchFrame(VDPosition pos) {
	try {
		const VDFraction frameRate(inputVideoAVI->getRate());

		fsi.lCurrentFrame			= (long)pos;
		fsi.lCurrentSourceFrame		= (long)mpTimeline->TimelineToSourceFrame(pos);
		fsi.lSourceFrameMS			= (long)frameRate.scale64ir(fsi.lCurrentSourceFrame * (sint64)1000);
		fsi.lDestFrameMS			= MulDiv(fsi.lCurrentFrame, fsi.lMicrosecsPerFrame, 1000);

		if (!inputVideoAVI->getFrame(fsi.lCurrentSourceFrame))
			return -1;

	} catch(const MyError&) {
		return -1;
	}

	VBitmap srcbm((void *)inputVideoAVI->getFrameBuffer(), inputVideoAVI->getDecompressedFormat());
	filtsys.InputBitmap()->BitBlt(0, 0, &srcbm, 0, 0, -1, -1);

	return pos;
}

bool FilterPreview::isPreviewEnabled() {
	return !!pFilterList;
}

bool FilterPreview::IsPreviewDisplayed() {
	return hdlg != NULL;
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

	if (hwnd) {
		const VDStringW wintext(VDGetWindowTextW32(hwnd));

		// look for an accelerator
		mButtonAccelerator = 0;

		if (pFilterList) {
			int pos = wintext.find(L'&');
			if (pos != wintext.npos) {
				++pos;
				if (pos < wintext.size()) {
					wchar_t c = wintext[pos];

					if (iswalpha(c))
						mButtonAccelerator = towlower(c);
				}
			}
		}

		EnableWindow(hwnd, pFilterList ? TRUE : FALSE);
	}
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

	UpdateButton();

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
	if (!pFilterList || !hdlg || !pSampleCallback)
		return false;

	if (!filtsys.isRunning()) {
		RedoSystem();

		if (!filtsys.isRunning())
			return false;
	}

	VDPosition pos = FetchFrame();

	if (pos >= 0) {
		try {
			filtsys.RunFilters(fsi, pfiThisFilter);
			pSampleCallback(&pfiThisFilter->src, (long)pos, (long)mpTimeline->GetLength(), pvSampleCBData);
		} catch(const MyError& e) {
			e.post(hdlg, "Video sampling error");
		}
	}

	RedoFrame();

	return true;
}

void FilterPreview::UpdateButton() {
	if (hwndButton) {
		VDStringW text(hdlg ? L"Hide preview" : L"Show preview");

		if (mButtonAccelerator) {
			VDStringW::size_type pos = text.find(mButtonAccelerator);

			if (pos == VDStringW::npos)
				pos = text.find(towupper(mButtonAccelerator));

			if (pos != VDStringW::npos)
				text.insert(text.begin() + pos, L'&');
		}

		VDSetWindowTextW32(hwndButton, text.c_str());
	}
}

///////////////////////

#define FPSAMP_KEYONESEC		(1)
#define	FPSAMP_KEYALL			(2)
#define	FPSAMP_ALL				(3)

static INT_PTR CALLBACK SampleFramesDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
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

	VDPosition first, last;

	first = mpTimeline->GetStart();
	last = mpTimeline->GetEnd();

	try {
		ProgressDialog pd(hdlg, "Sampling input video", szCaptions[iMode-1], (long)(last-first), true);
		VDPosition lSample = first;
		VDPosition lSecondIncrement = inputVideoAVI->msToSamples(1000)-1;

		pd.setValueFormat("Sampling frame %ld of %ld");

		if (lSecondIncrement<0)
			lSecondIncrement = 0;

		while(lSample>=0 && lSample < last) {
			pd.advance((long)(lSample - first));
			pd.check();

			if (FetchFrame(lSample)>=0) {
				filtsys.RunFilters(fsi, pfiThisFilter);
				pSampleCallback(&pfiThisFilter->src, (long)(lSample-first), (long)(last-first), pvSampleCBData);
				++lCount;
			}

			switch(iMode) {
			case FPSAMP_KEYONESEC:
				lSample += lSecondIncrement;
			case FPSAMP_KEYALL:
				lSample = mpTimeline->GetNextKey(lSample);
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
