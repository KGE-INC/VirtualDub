#include <windows.h>
#include <commctrl.h>

#include "w32trackbar.h"

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlTrackbarW32
//
///////////////////////////////////////////////////////////////////////////

VDUIControlTrackbarW32::VDUIControlTrackbarW32(int minv, int maxv)
	: minval(minv)
	, maxval(maxv)
{
}

VDUIControlTrackbarW32::~VDUIControlTrackbarW32() {
}

bool VDUIControlTrackbarW32::Create(IVDUIControl *pControl) {
	if (VDUIControlBase::Create(pControl)
		&& _Create(0, TRACKBAR_CLASSA, TRACKBAR_CLASSW, 0, 0)) {

		SendMessage(mhwnd, TBM_SETRANGE, (WPARAM)TRUE, MAKELONG(minval, maxval));
		return true;
	}

	return false;
}

IVDUIControlNativeCallback *VDUIControlTrackbarW32::AsNativeCallback() {
	return this;
}

IVDUISlider *VDUIControlTrackbarW32::AsUISlider() {
	return this;
}

void VDUIControlTrackbarW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	RECT rcMin;

	rcMin.left = rcMin.top = 0;
	rcMin.right = 16;
	rcMin.bottom = 24;

	if (mhwnd) {
		AdjustWindowRectEx(&rcMin, GetWindowLong(mhwnd, GWL_STYLE), FALSE, GetWindowLong(mhwnd, GWL_EXSTYLE));
	}

	mLayoutSpecs.minsize.w = rcMin.left + rcMin.right;
	mLayoutSpecs.minsize.h = rcMin.top + rcMin.bottom;
}

nsVDUI::eLinkMethod VDUIControlTrackbarW32::GetLinkMethod() {
	return nsVDUI::kLinkMethodInt;
}

int VDUIControlTrackbarW32::GetStatei() {
	if (!mhwnd)
		return 0;

	return SendMessage(mhwnd, TBM_GETPOS, 0, 0);
}

void VDUIControlTrackbarW32::SetStatei(int i) {
	if (!mhwnd)
		return;

	if (i >= maxval)
		i = maxval;

	if (i < minval)
		i = minval;

	SendMessage(mhwnd, TBM_SETPOS, TRUE, i);
}

void VDUIControlTrackbarW32::SetRange(int begin, int end) {
	SendMessage(mhwnd, TBM_SETRANGEMIN, FALSE, begin);
	SendMessage(mhwnd, TBM_SETRANGEMAX, TRUE, end);
}

void VDUIControlTrackbarW32::Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw() {
}

void VDUIControlTrackbarW32::Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw() {
}

void VDUIControlTrackbarW32::Dispatch_WM_HVSCROLL(IVDUICallback *pCB, UINT code) throw() {
	pCB->UIEvent(this, mID, IVDUICallback::kEventSelect, GetStatei());
	ProcessLinks();
}
