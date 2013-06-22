#include <windows.h>
#include <commctrl.h>

#include "w32edit.h"

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlEditW32
//
///////////////////////////////////////////////////////////////////////////

VDUIControlEditW32::VDUIControlEditW32(int maxlen)
	: mnMaxChars(maxlen)
	, mbCaptionCacheDirty(false)
	, mbInhibitCallbacks(true)
{
	if (mnMaxChars < 0)
		mnMaxChars = 0;
}
VDUIControlEditW32::~VDUIControlEditW32() {}

IVDUIControlNativeCallback *VDUIControlEditW32::AsNativeCallback() {
	return this;
}

IVDUIField *VDUIControlEditW32::AsUIField() {
	return this;
}

bool VDUIControlEditW32::Create(IVDUIControl *pControl) {
	if (VDUIControlBase::Create(pControl)
		&& _Create(0, "EDIT", L"EDIT", ES_AUTOHSCROLL, WS_EX_CLIENTEDGE)) {

		SendMessage(mhwnd, EM_LIMITTEXT, mnMaxChars, 0);

		mbInhibitCallbacks = false;

		return true;
	}

	return false;
}

void VDUIControlEditW32::Destroy() {
	UpdateCaptionCache();
	mbCaptionCacheDirty = false;

	VDUIControlBaseW32::Destroy();
}

void VDUIControlEditW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	SIZE siz = _SizeText(0, 0, 0);
	RECT r = { 0, 0, siz.cx, siz.cy };
	VDUIRect rPad = { 0, 0, 8, 12 };

	GetBase()->MapUnitsToPixels(rPad);

	AdjustWindowRectEx(&r, GetWindowLong(mhwnd, GWL_STYLE), FALSE, GetWindowLong(mhwnd, GWL_EXSTYLE));

	mLayoutSpecs.minsize.w	= (r.right - r.left) + 2*GetSystemMetrics(SM_CXEDGE);
	mLayoutSpecs.minsize.h	= rPad.h();
}

void VDUIControlEditW32::SetTextw(const wchar_t *text) throw() {
	mbInhibitCallbacks = true;
	VDUIControlBaseW32::SetTextw(text);
	mbInhibitCallbacks = false;
	mbCaptionCacheDirty = false;
}

int VDUIControlEditW32::GetTextw(wchar_t *dstbuf, int max_len) throw() {
	if (mbCaptionCacheDirty) {
		UpdateCaptionCache();
		mbCaptionCacheDirty = false;
	}
	return VDUIControlBaseW32::GetTextw(dstbuf, max_len);
}

int VDUIControlEditW32::GetTextLengthw() throw() {
	if (mbCaptionCacheDirty) {
		UpdateCaptionCache();
		mbCaptionCacheDirty = false;
	}

	return VDUIControlBaseW32::GetTextLengthw();
}

void VDUIControlEditW32::Select(int first, int last) {
	SendMessage(mhwnd, EM_SETSEL, first, last);
}

void VDUIControlEditW32::Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw() {
	if (code == EN_CHANGE) {
		mbCaptionCacheDirty = true;
		if (!mbInhibitCallbacks)
			pCB->UIEvent(this, mID, IVDUICallback::kEventSelect, 0);
	}
}

void VDUIControlEditW32::Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw() {
}

void VDUIControlEditW32::Dispatch_WM_HVSCROLL(IVDUICallback *, UINT code) throw() {
}

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlEditIntW32
//
//	A rather large irritation with standard Win32 edit controls is that
//	the ES_NUMBER style also prohibits the minus sign (-) from being
//	typed -- so we use a regular edit control and subclass it.  This
//	doesn't prevent you from pasting non-numbers into the edit field,
//	but then again, neither does ES_NUMBER.  (Try it!)
//
///////////////////////////////////////////////////////////////////////////

VDUIControlEditIntW32::VDUIControlEditIntW32(int minval, int maxval)
	: mnMin(minval)
	, mnMax(maxval)
	, mhwndSpin(NULL)
	, mbCaptionCacheDirty(false)
	, mbInhibitCallbacks(true)
{
	VDASSERT(minval <= maxval);
}
VDUIControlEditIntW32::~VDUIControlEditIntW32() {}

///////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK VDUIControlEditIntW32::NumericEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDUIControlEditIntW32 *pControl = (VDUIControlEditIntW32 *)GetWindowLong(hwnd, GWL_USERDATA);

	// I'll probably burn in localization hell for this....

	if (msg == WM_CHAR) {
		DWORD start, end;

		CallWindowProc(pControl->mOldWndProc, hwnd, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

		if ((unsigned)wParam >= ' ' && (wParam < '0' || wParam > '9') && (wParam!='-' || start)) {
			MessageBeep(MB_ICONEXCLAMATION);
			return 0;
		}
	}

	return CallWindowProc(pControl->mOldWndProc, hwnd, msg, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////

IVDUIControlNativeCallback *VDUIControlEditIntW32::AsNativeCallback() {
	return this;
}

IVDUIField *VDUIControlEditIntW32::AsUIField() {
	return this;
}

nsVDUI::eLinkMethod VDUIControlEditIntW32::GetLinkMethod() {
	return nsVDUI::kLinkMethodInt;
}

int VDUIControlEditIntW32::GetStatei() {
	int i = 0;

	if (mbCaptionCacheDirty) {
		UpdateCaptionCache();
		mbCaptionCacheDirty = false;
	}

	i = wcstol(mCaption, NULL, 10);

	if (i > mnMax)
		i = mnMax;

	if (i < mnMin)
		i = mnMin;

	return i;
}

void VDUIControlEditIntW32::SetStatei(int i) {
	wchar_t buf[16];

	swprintf(buf, L"%d", i);

	mbInhibitCallbacks = true;
	SetTextw(buf);
	mbInhibitCallbacks = false;
	mbCaptionCacheDirty = false;
}

bool VDUIControlEditIntW32::Create(IVDUIControl *pControl) {
	if (VDUIControlBase::Create(pControl)
		&& _Create(0, "EDIT", L"EDIT", ES_AUTOHSCROLL, WS_EX_CLIENTEDGE)) {

		// subclass the edit control

		mOldWndProc = (WNDPROC)GetWindowLong(mhwnd, GWL_WNDPROC);
		SetWindowLong(mhwnd, GWL_WNDPROC, (LONG)NumericEditProc);

		// try to create the spin control -- it's not terribly traumatic if
		// we can't, though....

		bool bUseExpandedRange = nsVDUI::isCommonControlsAtLeast(4, 71);

		if (bUseExpandedRange || (mnMin >= UD_MINVAL && mnMax <= UD_MAXVAL)) {
			HWND hwndParent = (HWND)GetBase()->AsControl()->GetRawHandle();

			if (nsVDUI::isWindows9x()) {
				mhwndSpin = CreateWindowExA(WS_EX_NOPARENTNOTIFY, UPDOWN_CLASSA, "", WS_VISIBLE|WS_BORDER|WS_CHILD|UDS_SETBUDDYINT|UDS_ALIGNRIGHT|UDS_ARROWKEYS|UDS_AUTOBUDDY|UDS_NOTHOUSANDS, 0, 0, 0, 0,
					hwndParent, (HMENU)0, GetModuleHandle(NULL), NULL);
			} else {
				mhwndSpin = CreateWindowExW(WS_EX_NOPARENTNOTIFY, UPDOWN_CLASSW, L"", WS_VISIBLE|WS_BORDER|WS_CHILD|UDS_SETBUDDYINT|UDS_ALIGNRIGHT|UDS_ARROWKEYS|UDS_AUTOBUDDY|UDS_NOTHOUSANDS, 0, 0, 0, 0,
					hwndParent, (HMENU)0, GetModuleHandle(NULL), NULL);

				SendMessageW(mhwndSpin, UDM_SETUNICODEFORMAT, TRUE, 0);
			}

			if (mhwndSpin) {
				SetWindowLong(mhwndSpin, GWL_USERDATA, (LPARAM)static_cast<VDUIControlBaseW32 *>(this));

				if (bUseExpandedRange)
					SendMessage(mhwndSpin, UDM_SETRANGE32, mnMin, mnMax);
				else
					SendMessage(mhwndSpin, UDM_SETRANGE, 0, MAKELONG(mnMax, mnMin));
			}
		}

		mbInhibitCallbacks = false;

		return true;
	}

	return false;
}

void VDUIControlEditIntW32::Destroy() {
	UpdateCaptionCache();

	if (mhwndSpin)
		DestroyWindow(mhwndSpin);

	VDUIControlBaseW32::Destroy();
}

void VDUIControlEditIntW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	SIZE siz = _SizeText(0, 0, 0);
	RECT r = { 0, 0, siz.cx, siz.cy };
	VDUIRect rPad = { 0, 0, 8, 12 };

	GetBase()->MapUnitsToPixels(rPad);

	AdjustWindowRectEx(&r, GetWindowLong(mhwnd, GWL_STYLE), FALSE, GetWindowLong(mhwnd, GWL_EXSTYLE));

	mLayoutSpecs.minsize.w	= (r.right - r.left) + 2*GetSystemMetrics(SM_CXEDGE);
	mLayoutSpecs.minsize.h	= rPad.h();
}

void VDUIControlEditIntW32::PostLayoutBase(const VDUIRect& target) {
	VDUIControlBaseW32::PostLayoutBase(target);

	if (mhwndSpin)
		SendMessage(mhwndSpin, UDM_SETBUDDY, (WPARAM)mhwnd, 0);
}

void VDUIControlEditIntW32::Select(int first, int last) {
	SendMessage(mhwnd, EM_SETSEL, first, last);
}

void VDUIControlEditIntW32::Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw() {
	if (code == EN_CHANGE) {
		mbCaptionCacheDirty = true;
		if (!mbInhibitCallbacks)
			pCB->UIEvent(this, mID, IVDUICallback::kEventSelect, 0);
	}
}

void VDUIControlEditIntW32::Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw() {
}

void VDUIControlEditIntW32::Dispatch_WM_HVSCROLL(IVDUICallback *, UINT code) throw() {
	ProcessLinks();
}
