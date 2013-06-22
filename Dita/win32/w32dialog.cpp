#pragma warning(disable: 4786)

#include <vd2/system/text.h>
#include <vd2/Dita/services.h>

#include "w32dialog.h"

///////////////////////////////////////////////////////////////////////////

static const struct {
	WORD		dlgVer;
	WORD		signature;
	DWORD		helpID;
	DWORD		exStyle;
	DWORD		style;
	WORD		cDlgItems;
	short		x;
	short		y;
	short		cx;
	short		cy;
	short		menu;
	short		windowClass;
	WCHAR		title;
	WORD		pointsize;
	WORD		weight;
	BYTE		italic;
	BYTE		charset;
	WCHAR		typeface[13];
} g_dummyDialogDef={
	1,					// dlgVer
	0xFFFF,				// signature
	0,					// helpID
	0,					// exStyle
	WS_VISIBLE|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_CLIPSIBLINGS|DS_NOIDLEMSG|DS_SETFONT,
	0,
	0,
	0,
	0,
	0,
	0,					// menu
	0,					// windowClass
	0,					// title
	8,					// pointsize
	0,					// weight
	0,					// italic
	0,					// charset
	L"MS Shell Dlg"		// typeface
}, g_dummyDialogDefChild={
	1,					// dlgVer
	0xFFFF,				// signature
	0,					// helpID
	0,					// exStyle
	WS_VISIBLE|WS_CHILD|WS_CLIPSIBLINGS|DS_NOIDLEMSG|DS_SETFONT|DS_CONTROL,
	0,
	0,
	0,
	0,
	0,
	0,					// menu
	0,					// windowClass
	0,					// title
	8,					// pointsize
	0,					// weight
	0,					// italic
	0,					// charset
	L"MS Shell Dlg"		// typeface
};

struct VDUIControlDialogW32CreateData {
	VDUIControlDialogW32 *pThis;
	IVDUIContext *pContext;
};

///////////////////////////////////////////////////////////////////////////

VDUIControlDialogW32::VDUIControlDialogW32(bool bChild)
	: mpLayoutControl(NULL)
	, mpCB(NULL)
	, mNextID(256)
	, mRetVal(0)
	, mInMessage(0)
	, mbDeferredDestroy(false)
	, mbChild(bChild)
{
}

VDUIControlDialogW32::~VDUIControlDialogW32() {
}

IVDUIControl *VDUIControlDialogW32::AsControl() {
	return this;
}

IVDUIBase *VDUIControlDialogW32::AsBase() {
	return this;
}

bool VDUIControlDialogW32::Create(IVDUIControl *pParent) {
	VDUIControlDialogW32CreateData data;

	data.pThis = this;
	data.pContext = VDGetUIContext();

	if (!(mhwnd = (nsVDUI::isWindows9x() ? CreateDialogIndirectParamA : CreateDialogIndirectParamW)
		(GetModuleHandle(NULL), (const DLGTEMPLATE *)&g_dummyDialogDefChild, (HWND)pParent->GetBase()->AsControl()->GetRawHandle(), StaticDlgProc, (LPARAM)&data)))
		return false;

	return VDUIControlBase::Create(pParent);	// NOTE: Deliberately skipping W32Base
}

void VDUIControlDialogW32::Destroy() {

	// Protect against recursion from notifications.

	++mInMessage;

	// Fire off a notification before any controls disappear.

	if (mpCB)
		mpCB->UIEvent(this, mID, IVDUICallback::kEventDestroy, mRetVal);

	// Destroy the main layout tree.

	if (mpLayoutControl)
		mpLayoutControl->Destroy();

	// Don't iterate over the loop in case some controls
	// destroy others (stupid, but probably should not
	// crash).

	while (!mControlMap.empty())
		(*mControlMap.begin()).second->Destroy();

	// Clear delayed destroy flag to prevent recursion.

	--mInMessage;
	mbDeferredDestroy = false;

	// Don't delete or DestroyWindow() modal dialogs!!

	if (!mpParent) {
		EndDialog(mhwnd, mRetVal);
		mhwnd = NULL;
	} else
		VDUIControlBaseW32::Destroy();
}

///////////////////////////////////////////////////////////////////////////

struct VDUIControlDialogW32ValidateData {
	HWND hdlg;
	HDC hdc;
};

static BOOL CALLBACK ValidateEnumerator(HWND hwnd, LPARAM pData) {
	VDUIControlDialogW32ValidateData& data = *(VDUIControlDialogW32ValidateData *)pData;
	HBRUSH hbrBackground = (HBRUSH)GetClassLong(hwnd, GCL_HBRBACKGROUND);

	if (hbrBackground) {
		RECT r;

		GetWindowRect(hwnd, &r);
		MapWindowPoints(NULL, data.hdlg, (LPPOINT)&r, 2);
		ExcludeClipRect(data.hdc, r.left, r.top, r.right, r.bottom);
	}

	return TRUE;
}

BOOL CALLBACK VDUIControlDialogW32::StaticDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDUIControlDialogW32 *pThis = (VDUIControlDialogW32 *)GetWindowLong(hdlg, DWL_USER);

	switch(msg) {
	case WM_INITDIALOG:
		{
			const VDUIControlDialogW32CreateData& data = *(VDUIControlDialogW32CreateData *)lParam;

			SetWindowLong(hdlg, DWL_USER, (LPARAM)data.pThis);
			pThis = (VDUIControlDialogW32 *)data.pThis;
			pThis->mhwnd = hdlg;

			// Invoke user callback to create controls

			if (pThis->mpCB && !pThis->mpCB->UIConstructModal(data.pContext, pThis)) {
				EndDialog(hdlg, -1);
				return FALSE;
			}

			if (!(GetWindowLong(hdlg, GWL_STYLE) & DS_CONTROL)) {
				VDUILayoutSpecs constraints;

				constraints.minsize.w = GetSystemMetrics(SM_CXSCREEN);
				constraints.minsize.h = GetSystemMetrics(SM_CYSCREEN);

				pThis->PreLayout(constraints);

				SetWindowPos(hdlg, NULL, 0, 0, pThis->mLayoutSpecs.minsize.w, pThis->mLayoutSpecs.minsize.h, SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);
			}
		}

		return FALSE;

	case WM_GETMINMAXINFO:
		{
			MINMAXINFO *pmmi = (MINMAXINFO *)lParam;

			pmmi->ptMinTrackSize.x = pThis->mLayoutSpecs.minsize.w;
			pmmi->ptMinTrackSize.y = pThis->mLayoutSpecs.minsize.h;

			if ((pThis->mAlignX & nsVDUI::kAlignTypeMask) != nsVDUI::kFill)
				pmmi->ptMaxTrackSize.x = pThis->mLayoutSpecs.minsize.w;

			if ((pThis->mAlignY & nsVDUI::kAlignTypeMask) != nsVDUI::kFill)
				pmmi->ptMaxTrackSize.y = pThis->mLayoutSpecs.minsize.h;
		}
		return TRUE;

	case WM_SIZE:
		if (!(GetWindowLong(hdlg, GWL_STYLE) & DS_CONTROL) && pThis->mpLayoutControl) {
			RECT rc;
			
			GetClientRect(hdlg, &rc);

			VDUIRect rcTarget;

			rcTarget.x1 = pThis->mrcInsets.left;
			rcTarget.y1 = pThis->mrcInsets.top;
			rcTarget.x2 = rc.right - pThis->mrcInsets.right;
			rcTarget.y2 = rc.bottom - pThis->mrcInsets.bottom;

			if (pThis->mpLayoutControl)
				pThis->mpLayoutControl->PostLayout(rcTarget);
		}
		return TRUE;

	case WM_COMMAND:
		if (lParam) {
			IVDUIControl *pControl = (IVDUIControl *)GetWindowLong((HWND)lParam, GWL_USERDATA);

			if (pControl) {
				IVDUIControlNativeCallbackW32 *pCB = static_cast<IVDUIControlNativeCallbackW32 *>(pControl->AsNativeCallback());

				if (pCB) {
					++pThis->mInMessage;
					pCB->Dispatch_WM_COMMAND(pThis->mpCB, HIWORD(wParam));
					--pThis->mInMessage;

					if (pThis->mbDeferredDestroy && !pThis->mInMessage)
						pThis->Destroy();
				}
			}
			return TRUE;
		} else if (LOWORD(wParam) == IDCANCEL) {
			pThis->mpCB->UIEvent(pThis, pThis->mID, IVDUICallback::kEventClose, 0);
			return TRUE;
		} else if (LOWORD(wParam) == IDOK) {
			pThis->mpCB->UIEvent(pThis, pThis->mID, IVDUICallback::kEventClose, 1);
			return TRUE;
		}
		break;

	case WM_NOTIFY:
		if (lParam) {
			const NMHDR *pnhdr = (const NMHDR *)lParam;
			IVDUIControl *pControl = (IVDUIControl *)GetWindowLong((HWND)pnhdr->hwndFrom, GWL_USERDATA);

			if (pControl) {
				IVDUIControlNativeCallbackW32 *pCB = static_cast<IVDUIControlNativeCallbackW32 *>(pControl->AsNativeCallback());

				if (pCB) {
					++pThis->mInMessage;
					pCB->Dispatch_WM_NOTIFY(pThis->mpCB, pnhdr->code, pnhdr);
					--pThis->mInMessage;

					if (pThis->mbDeferredDestroy && !pThis->mInMessage)
						pThis->Destroy();
				}
			}

			return TRUE;
		}
		break;

	case WM_NOTIFYFORMAT:
		SetWindowLong(hdlg, DWL_MSGRESULT, nsVDUI::isWindows9x() ? NFR_ANSI : NFR_UNICODE);
		return TRUE;

	case WM_HSCROLL:
	case WM_VSCROLL:
		if (lParam) {
			IVDUIControl *pControl = (IVDUIControl *)GetWindowLong((HWND)lParam, GWL_USERDATA);

			if (pControl) {
				IVDUIControlNativeCallbackW32 *pCB = static_cast<IVDUIControlNativeCallbackW32 *>(pControl->AsNativeCallback());

				if (pCB) {
					++pThis->mInMessage;
					pCB->Dispatch_WM_HVSCROLL(pThis->mpCB, LOWORD(wParam));
					--pThis->mInMessage;

					if (pThis->mbDeferredDestroy && !pThis->mInMessage)
						pThis->Destroy();
				}
			}

			return TRUE;
		}
		break;

	// We override WM_ERASEBKGND to avoid some annoying flicker issues.

	case WM_ERASEBKGND:
		{
			VDUIControlDialogW32ValidateData data = {hdlg, (HDC)wParam};

			EnumChildWindows(hdlg, ValidateEnumerator, (LPARAM)&data);
		}
		return FALSE;

	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////

void VDUIControlDialogW32::SetCallback(IVDUICallback *pCB) {
	mpCB = pCB;
}

int VDUIControlDialogW32::BeginModal(VDGUIHandle parent, IVDUIContext *pctx) {
	VDUIControlDialogW32CreateData data;

	data.pThis = this;
	data.pContext = pctx;

	return (nsVDUI::isWindows9x() ? DialogBoxIndirectParamA : DialogBoxIndirectParamW)
		(GetModuleHandle(NULL), (const DLGTEMPLATE *)&g_dummyDialogDef, (HWND)parent, StaticDlgProc, (LPARAM)&data);
}

void VDUIControlDialogW32::EndModal(int rv) {
	mRetVal = rv;
	if (mInMessage)
		mbDeferredDestroy = true;
	else
		Destroy();
}

void VDUIControlDialogW32::MapUnitsToPixels(VDUIRect& r) {
	RECT r2 = { r.x1, r.y1, r.x2, r.y2 };

	MapDialogRect(mhwnd, &r2);

	r.x1 = r2.left;
	r.y1 = r2.top;
	r.x2 = r2.right;
	r.y2 = r2.bottom;
}

void VDUIControlDialogW32::MapScreenToClient(VDUIPoint& pt) {
	POINT pt2 = { pt.x, pt.y };

	ScreenToClient(mhwnd, &pt2);

	pt.x = pt2.x;
	pt.y = pt2.y;
}

void VDUIControlDialogW32::MapScreenToClient(VDUIRect& r) {
	RECT r2 = { r.x1, r.y1, r.x2, r.y2 };

	MapWindowPoints(NULL, mhwnd, (LPPOINT)&r2, 2);

	r.x1 = r2.left;
	r.y1 = r2.top;
	r.x2 = r2.right;
	r.y2 = r2.bottom;
}

void VDUIControlDialogW32::MapClientToScreen(VDUIPoint& pt) {
	POINT pt2 = { pt.x, pt.y };

	ClientToScreen(mhwnd, &pt2);

	pt.x = pt2.x;
	pt.y = pt2.y;
}

void VDUIControlDialogW32::MapClientToScreen(VDUIRect& r) {
	RECT r2 = { r.x1, r.y1, r.x2, r.y2 };

	MapWindowPoints(mhwnd, NULL, (LPPOINT)&r2, 2);

	r.x1 = r2.left;
	r.y1 = r2.top;
	r.x2 = r2.right;
	r.y2 = r2.bottom;
}

IVDUIControl *VDUIControlDialogW32::GetControl(unsigned id) {
	tControlMap::iterator it = mControlMap.find(id);

	if (it == mControlMap.end())
		return NULL;
	else
		return (*it).second;
}

bool VDUIControlDialogW32::Add(IVDUIControl *pControl) {
	if (pControl->Create(this)) {
		mControlMap.insert(tControlMap::value_type(pControl->GetID(), pControl));
		return true;
	}

	return false;
}

void VDUIControlDialogW32::AddNonlocal(IVDUIControl *pControl) {
	mControlMap.insert(tControlMap::value_type(pControl->GetID(), pControl));
}

void VDUIControlDialogW32::Remove(IVDUIControl *pControl) {
	unsigned id = pControl->GetID();
	tControlMap::iterator it = mControlMap.find(id);

	while(it != mControlMap.end() && (*it).first == id) {
		if ((*it).second == pControl) {
			mControlMap.erase(it);
			break;
		}

		++it;
	}

	if (mpLayoutControl == pControl)
		mpLayoutControl = NULL;

	Unlink(pControl);
}

void VDUIControlDialogW32::SetLayoutControl(IVDUIControl *pControl) {
	mpLayoutControl = pControl;
}

unsigned VDUIControlDialogW32::CreateUniqueID() {
	return mNextID++;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlDialogW32		Link support
//
///////////////////////////////////////////////////////////////////////////

void VDUIControlDialogW32::_ProcessLinksb(IVDUIControl *pControl, tLinkList& linklist, bool bProcessStaticsOnly) {
	tLinkList::iterator it = linklist.begin();
	tLinkList::iterator itEnd = linklist.end();
	bool val = pControl->GetStateb();

	for(; it!=itEnd; ++it) {
		bool b = val;
		nsVDUI::eLinkType mask = (*it).second;

		if (bProcessStaticsOnly && nsVDUI::kLinkMethodStatic != (*it).first->GetLinkMethod())
			continue;

		if (mask & nsVDUI::kLinkInvert)
			b = !b;

		if (b && (mask & nsVDUI::kLinkAnd))
			continue;

		if (!b && (mask & nsVDUI::kLinkOr))
			continue;

		switch(mask & nsVDUI::kLinkTypeMask) {
		case nsVDUI::kLinkEnable:
			(*it).first->Enable(b);
			break;
		case nsVDUI::kLinkState:
			(*it).first->SetStateb(b);
			break;

		// This one is rather scary.

		case nsVDUI::kLinkBoolToInt:
			(*it).first->SetStatei(b ? (((int)mask & nsVDUI::kLinkHiMask)>>16) : (((int)mask & nsVDUI::kLinkLoMask)>>8));
			break;

		// These result in the destruction of the dialog, and thus
		// force a halt.

		case nsVDUI::kLinkCloseDialog:
			mpCB->UIEvent(this, mID, IVDUICallback::kEventClose, b);
			return;

		case nsVDUI::kLinkEndDialog:
			EndModal(b);
			return;

		default:
			VDASSERT(false);
		}

		if (mask & nsVDUI::kLinkRelink)
			(*it).first->ProcessLinks();
	}
}

void VDUIControlDialogW32::_ProcessLinksi(IVDUIControl *pControl, tLinkList& linklist, bool bProcessStaticsOnly) {
	tLinkList::iterator it = linklist.begin();
	tLinkList::iterator itEnd = linklist.end();
	int val = pControl->GetStatei();

	for(; it!=itEnd; ++it) {
		nsVDUI::eLinkType mask = (*it).second;

		if (bProcessStaticsOnly && nsVDUI::kLinkMethodStatic != (*it).first->GetLinkMethod())
			continue;

		switch(mask & nsVDUI::kLinkTypeMask) {
		case nsVDUI::kLinkInt:
			(*it).first->SetStatei(val);
			break;
		case nsVDUI::kLinkIntString:
			(*it).first->SetTextw(VDFastTextPrintfW(L"%ld", val));
			VDFastTextFree();
			break;
		case nsVDUI::kLinkIntSelectedEnable:
			{
				bool b = val >= 0;

				if (mask & nsVDUI::kLinkInvert)
					b = !b;

				if (b && (mask & nsVDUI::kLinkAnd))
					continue;

				if (!b && (mask & nsVDUI::kLinkOr))
					continue;

				(*it).first->Enable(b);
			}
			break;
		case nsVDUI::kLinkIntRangeEnable:
			{
				int lo = (mask>>8)&0xff;
				int hi = (mask>>16)&0xff;
				bool b = (val >= lo && val <= hi);

				if (mask & nsVDUI::kLinkInvert)
					b = !b;

				if (b && (mask & nsVDUI::kLinkAnd))
					continue;

				if (!b && (mask & nsVDUI::kLinkOr))
					continue;

				(*it).first->Enable(b);
			}
			break;
		default:
			VDASSERT(false);
		}

		if (mask & nsVDUI::kLinkRelink)
			(*it).first->ProcessLinks();
	}
}

void VDUIControlDialogW32::_ProcessLinks(IVDUIControl *pControl, tLinkList& linklist, bool bProcessStaticsOnly, bool bProcessOneShots) {
	switch(pControl->GetLinkMethod()) {
		case nsVDUI::kLinkMethodStatic:
			break;
		case nsVDUI::kLinkMethodBoolOneShot:
			if (!bProcessOneShots)
				break;
		case nsVDUI::kLinkMethodBool:
			_ProcessLinksb(pControl, linklist, bProcessStaticsOnly);
			break;
		case nsVDUI::kLinkMethodInt:
			_ProcessLinksi(pControl, linklist, bProcessStaticsOnly);
			break;
		default:
			VDASSERT(false);
	}
}

void VDUIControlDialogW32::ProcessLinksID(unsigned from_id) {
	IVDUIControl *pControl = GetControl(from_id);

	if (pControl)
		ProcessLinksPtr(pControl);
}

void VDUIControlDialogW32::ProcessLinksPtr(IVDUIControl *pControl) {
	tLinkMap::iterator it = mLinkMap.find(pControl);

	if (it != mLinkMap.end())
		_ProcessLinks((*it).first, (*it).second, false, true);
}

void VDUIControlDialogW32::ProcessLinksToStatics() {
	tLinkMap::iterator it = mLinkMap.begin();
	tLinkMap::iterator itEnd = mLinkMap.end();

	for(; it!=itEnd; ++it)
		_ProcessLinks((*it).first, (*it).second, true, false);
}

void VDUIControlDialogW32::ProcessAllLinks() {
	tLinkMap::iterator it = mLinkMap.begin();
	tLinkMap::iterator itEnd = mLinkMap.end();

	// Despite the name, we still don't process one-shot links.

	for(; it!=itEnd; ++it)
		_ProcessLinks((*it).first, (*it).second, false, false);
}

void VDUIControlDialogW32::Link(unsigned dst_id, nsVDUI::eLinkType type, unsigned src_id) {
	IVDUIControl *pControlA = GetControl(src_id);
	IVDUIControl *pControlB = GetControl(dst_id);

	if (pControlA && pControlB)
		mLinkMap[pControlA][pControlB] = type;
}

void VDUIControlDialogW32::Link(IVDUIControl *pControlB, nsVDUI::eLinkType type, IVDUIControl *pControlA) {
	VDASSERTPTR(pControlA);
	VDASSERTPTR(pControlB);

	mLinkMap[pControlA][pControlB] = type;
}

void VDUIControlDialogW32::Unlink(unsigned dst_id, nsVDUI::eLinkType type, unsigned src_id) {
	IVDUIControl *pControlA = GetControl(src_id);
	IVDUIControl *pControlB = GetControl(dst_id);

	if (pControlA && pControlB) {
		tLinkMap::iterator itA = mLinkMap.find(pControlA);

		if (itA != mLinkMap.end()) {
			tLinkList::iterator itB = (*itA).second.find(pControlB);

			if (itB != (*itA).second.end())
				(*itA).second.erase(itB);
		}
	}
}

void VDUIControlDialogW32::Unlink(unsigned dst_id) {
	IVDUIControl *pControl = GetControl(dst_id);

	if (pControl)
		Unlink(pControl);
}

void VDUIControlDialogW32::Unlink(IVDUIControl *pControl) {
	tLinkMap::iterator it = mLinkMap.begin();
	tLinkMap::iterator itEnd = mLinkMap.end();

	while(it != itEnd) {
		if ((*it).first == pControl)
			it = mLinkMap.erase(it);
		else {
			tLinkList& linklist = (*it).second;
			tLinkList::iterator itA = linklist.begin();
			tLinkList::iterator itB = linklist.end();

			while(itA != itB) {
				if ((*itA).first == pControl)
					itA = linklist.erase(itA);
				else
					++itA;
			}

			if (linklist.empty())
				it = mLinkMap.erase(it);
			else
				++it;
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlDialogW32		Layout support
//
///////////////////////////////////////////////////////////////////////////

void VDUIControlDialogW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	VDUILayoutSpecs rcConstraints(parentConstraints);
	RECT rcBorders = {0,0,0,0};
	RECT rcPad = {7,7,7,7};
	bool bModeless = (0 != (GetWindowLong(mhwnd, GWL_STYLE) & DS_CONTROL));

	// If the dialog is modal, compute the insets for the dialog.

	if (!bModeless) {
		MapDialogRect(mhwnd, &rcPad);

		mrcInsets = rcPad;

		AdjustWindowRectEx(&rcBorders, GetWindowLong(mhwnd, GWL_STYLE), FALSE, GetWindowLong(mhwnd, GWL_STYLE));

		// enlarge borders by pads
		rcBorders.left		= rcBorders.left   - rcPad.left;
		rcBorders.top		= rcBorders.top    - rcPad.top;
		rcBorders.right		= rcBorders.right  + rcPad.right;
		rcBorders.bottom	= rcBorders.bottom + rcPad.bottom;
	} else
		mrcInsets = rcBorders;

	// Shrink constraints by insets.

	mLayoutSpecs.minsize.w = rcBorders.right - rcBorders.left;
	mLayoutSpecs.minsize.h = rcBorders.bottom - rcBorders.top;

	rcConstraints.minsize.w -= mLayoutSpecs.minsize.w;
	rcConstraints.minsize.h -= mLayoutSpecs.minsize.h;

	// Layout the primary control.

	if (mpLayoutControl) {
		mpLayoutControl->PreLayout(rcConstraints);

		const VDUILayoutSpecs& prispecs = mpLayoutControl->GetLayoutSpecs();

		mLayoutSpecs.minsize.w += prispecs.minsize.w;
		mLayoutSpecs.minsize.h += prispecs.minsize.h;
	}

	// Check for other controls that are dropped into this dialog but do not
	// have layout parents.  Expand the control box to accommodate these controls.

	tControlMap::iterator it = mControlMap.begin();
	tControlMap::iterator itEnd = mControlMap.end();

	for(; it != itEnd; ++it) {
		IVDUIControl *pControl = (*it).second;

		if (!pControl->GetParent()) {
			VDUIRect rc;

			pControl->GetPosition(rc);
			MapClientToScreen(rc);

			if (mLayoutSpecs.minsize.w < rc.x2)
				mLayoutSpecs.minsize.w = rc.x2;

			if (mLayoutSpecs.minsize.h < rc.y2)
				mLayoutSpecs.minsize.h = rc.y2;
		}
	}
}

void VDUIControlDialogW32::PostLayoutBase(const VDUIRect& target) {
	if (!mpLayoutControl)
		return;

	SetWindowPos(mhwnd, NULL, target.x1, target.y1, target.w(), target.h(), SWP_NOACTIVATE|SWP_NOZORDER);

	VDUIRect rc;
	RECT r;

	GetClientRect(mhwnd, &r);

	rc.x1 = mrcInsets.left;
	rc.y1 = mrcInsets.top;
	rc.x2 = r.right - mrcInsets.right;
	rc.y2 = r.bottom - mrcInsets.bottom;

	mpLayoutControl->PostLayout(rc);
}

void VDUIControlDialogW32::Relayout() {
	VDUILayoutSpecs constraints;
	RECT r;

	constraints.minsize.w = GetSystemMetrics(SM_CXSCREEN);
	constraints.minsize.h = GetSystemMetrics(SM_CYSCREEN);

	PreLayout(constraints);

	GetClientRect(mhwnd, &r);

	int w = mLayoutSpecs.minsize.w;
	int h = mLayoutSpecs.minsize.h;

	if (w != r.right-r.left && h != r.bottom-r.top)
		SetWindowPos(mhwnd, NULL, 0, 0, w, h, SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);

	VDUIRect rcTarget;

	GetClientRect(mhwnd, &r);

	rcTarget.x1 = mrcInsets.left;
	rcTarget.y1 = mrcInsets.top;
	rcTarget.x2 = r.right - mrcInsets.right;
	rcTarget.y2 = r.bottom - mrcInsets.bottom;

	if (mpLayoutControl)
		mpLayoutControl->PostLayout(rcTarget);
}
