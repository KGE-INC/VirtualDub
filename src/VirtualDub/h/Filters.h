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

#ifndef f_FILTERS_H
#define f_FILTERS_H

#include <malloc.h>

#include <windows.h>

#include <list>
#include <vector>
#include <vd2/system/list.h>
#include <vd2/system/error.h>
#include <vd2/system/VDString.h>
#include <vd2/system/refcount.h>
#include <vd2/VDLib/ParameterCurve.h>
#include "VBitmap.h"
#include "FilterSystem.h"
#include "filter.h"
#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "gui.h"

//////////////////

struct CScriptObject;
class IVDVideoDisplay;
class IVDPositionControl;
struct VDWaveFormat;
class VDTimeline;

//////////////////

#ifndef f_FILTER_GLOBALS
#define EXTERN extern
#define INIT(x)
#else
#define EXTERN
#define INIT(x) =x
#endif

///////////////////

VDWaveFormat *VDCopyWaveFormat(const VDWaveFormat *pFormat);

///////////////////

class FilterDefinitionInstance;

class VFBitmapInternal : public VBitmap {
public:
	// Must match layout of VFBitmap!
	enum {
		NEEDS_HDC		= 0x00000001L,
	};

	DWORD	dwFlags;
	HDC		hdc;
};

class FilterInstance : public ListNode, public FilterActivation {
private:
	FilterInstance& operator=(const FilterInstance&);		// outlaw copy assignment
public:
	VFBitmapInternal realSrc, realDst, realLast;
	int mBlendBuffer;
	LONG flags;
	HBITMAP hbmDst, hbmLast;
	HGDIOBJ hgoDst, hgoLast;
	void *pvDstView, *pvLastView;
	int srcbuf, dstbuf;
	int origw, origh;
	bool fNoDeinit;

	FilterStateInfo *pfsiDelayRing;
	FilterStateInfo *pfsiDelayInput;
	FilterStateInfo fsiDelay;
	FilterStateInfo fsiDelayOutput;
	int nDelayRingPos;
	int nDelayRingSize;

	VDStringW	mFilterName;

	std::vector<VDScriptFunctionDef>	mScriptFunc;
	VDScriptObject	mScriptObj;

	///////

	FilterInstance(const FilterInstance& fi);
	FilterInstance(FilterDefinitionInstance *);
	~FilterInstance();

	FilterInstance *Clone();
	void Destroy();
	void ForceNoDeinit();

	VDParameterCurve *GetAlphaParameterCurve() const { return mpAlphaCurve; }
	void SetAlphaParameterCurve(VDParameterCurve *p) { mpAlphaCurve = p; }

protected:
	static void ConvertParameters(CScriptValue *dst, const VDScriptValue *src, int argc);
	static void ScriptFunctionThunkVoid(IVDScriptInterpreter *, VDScriptValue *, int);
	static void ScriptFunctionThunkInt(IVDScriptInterpreter *, VDScriptValue *, int);
	static void ScriptFunctionThunkVariadic(IVDScriptInterpreter *, VDScriptValue *, int);

	FilterDefinitionInstance *mpFDInst;

	vdrefptr<VDParameterCurve> mpAlphaCurve;
};

class FilterPreview : public IVDFilterPreview2 {
private:
	HWND hdlg, hwndButton;
	wchar_t	mButtonAccelerator;

	HWND hwndParent;
	HWND	mhwndPosition;
	IVDPositionControl	*mpPosition;
	HWND	mhwndDisplay;
	IVDVideoDisplay *mpDisplay;
	HWND	mhwndToolTip;
	FilterSystem filtsys;
	BITMAPINFOHEADER bih;
	FilterStateInfo fsi;
	List *pFilterList;
	FilterInstance *pfiThisFilter;
	VDTimeline *mpTimeline;

	FilterPreviewButtonCallback		pButtonCallback;
	void							*pvButtonCBData;
	FilterPreviewSampleCallback		pSampleCallback;
	void							*pvSampleCBData;

	MyError		mFailureReason;

	ModelessDlgNode		mDlgNode;

	static INT_PTR CALLBACK StaticDlgProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam);
	BOOL DlgProc(UINT message, WPARAM wParam, LPARAM lParam);

	void OnInit();
	void OnResize();
	void OnPaint();
	void OnVideoResize(bool bInitial);
	void OnVideoRedraw();
	bool OnCommand(UINT);

	VDPosition FetchFrame();
	VDPosition FetchFrame(VDPosition);

	void UpdateButton();

public:
	FilterPreview(List *, FilterInstance *);
	~FilterPreview();

	void SetButtonCallback(FilterPreviewButtonCallback, void *);
	void SetSampleCallback(FilterPreviewSampleCallback, void *);

	bool isPreviewEnabled();
	bool IsPreviewDisplayed();
	void InitButton(HWND);
	void Toggle(HWND);
	void Display(HWND, bool);
	void RedoFrame();
	void UndoSystem();
	void RedoSystem();
	void Close();
	bool SampleCurrentFrame();
	long SampleFrames();
};

//////////

EXTERN List				g_listFA;

EXTERN FilterSystem filters;

FilterDefinition *	FilterAdd(FilterModule *fm, FilterDefinition *pfd, int fd_len);
void				FilterAddBuiltin(const FilterDefinition *pfd);
void				FilterRemove(FilterDefinition *fd);

struct FilterBlurb {
	FilterDefinitionInstance	*key;
	VDStringA					name;
	VDStringA					author;
	VDStringA					description;
};

void				FilterEnumerateFilters(std::list<FilterBlurb>& blurbs);


LONG FilterGetSingleValue(HWND hWnd, LONG cVal, LONG lMin, LONG lMax, char *title);

#undef EXTERN
#undef INIT

#endif
