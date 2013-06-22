#include "w32button.h"

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlButtonW32
//
///////////////////////////////////////////////////////////////////////////

VDUIControlButtonW32::VDUIControlButtonW32() {}
VDUIControlButtonW32::~VDUIControlButtonW32() {}

bool VDUIControlButtonW32::Create(IVDUIControl *pControl) {
	if (VDUIControlBase::Create(pControl)
		&& _Create(0, "BUTTON", L"BUTTON", mID == nsVDUI::kIDPrimary ? BS_DEFPUSHBUTTON : 0))
	{
		if (mID == nsVDUI::kIDPrimary)
			::SetFocus(mhwnd);

		return true;
	}

	return false;
}

IVDUIControlNativeCallback *VDUIControlButtonW32::AsNativeCallback() {
	return this;
}

void VDUIControlButtonW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	VDUIRect r = {0,0,8,14};

	GetBase()->MapUnitsToPixels(r);

	SIZE siz = _SizeText(parentConstraints.minsize.w, r.x2, r.y2);

	mLayoutSpecs.minsize.w	= r.x2 + siz.cx;
	mLayoutSpecs.minsize.h	= r.y2;
}

nsVDUI::eLinkMethod VDUIControlButtonW32::GetLinkMethod() {
	return nsVDUI::kLinkMethodBoolOneShot;
}

bool VDUIControlButtonW32::GetStateb() {
	return true;
}

void VDUIControlButtonW32::Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw() {
	if (code == BN_CLICKED) {
		ProcessLinks();
		pCB->UIEvent(this, mID, IVDUICallback::kEventSelect, 0);
	}
}

void VDUIControlButtonW32::Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw() {
}

void VDUIControlButtonW32::Dispatch_WM_HVSCROLL(IVDUICallback *, UINT code) throw() {
}

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlCheckboxW32
//
///////////////////////////////////////////////////////////////////////////

VDUIControlCheckboxW32::VDUIControlCheckboxW32() {}
VDUIControlCheckboxW32::~VDUIControlCheckboxW32() {}

bool VDUIControlCheckboxW32::Create(IVDUIControl *pControl) {
	return VDUIControlBase::Create(pControl)
		&& _Create(0, "BUTTON", L"BUTTON", BS_AUTOCHECKBOX|BS_TOP|BS_MULTILINE);
}

IVDUIControlNativeCallback *VDUIControlCheckboxW32::AsNativeCallback() {
	return this;
}

void VDUIControlCheckboxW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	VDUIRect r = {0,0,14,10};

	GetBase()->MapUnitsToPixels(r);

	mLayoutSpecs.minsize.w = r.x2;
	mLayoutSpecs.minsize.h = r.y2;

	SIZE siz = _SizeText(parentConstraints.minsize.w - r.x2, r.x2, r.y2);

	siz.cy += 2*GetSystemMetrics(SM_CYEDGE);

	mLayoutSpecs.minsize.w += siz.cx;
	if (mLayoutSpecs.minsize.h < siz.cy)
		mLayoutSpecs.minsize.h = siz.cy;
}

nsVDUI::eLinkMethod VDUIControlCheckboxW32::GetLinkMethod() {
	return nsVDUI::kLinkMethodBool;
}

nsVDUI::eCompressType VDUIControlCheckboxW32::GetCompressType() {
	return nsVDUI::kCompressCheckbox;
}

bool VDUIControlCheckboxW32::GetStateb() {
	if (mhwnd)
		return BST_CHECKED == SendMessage(mhwnd, BM_GETCHECK, 0, 0);

	return false;
}

void VDUIControlCheckboxW32::SetStateb(bool b) {
	if (mhwnd)
		SendMessage(mhwnd, BM_SETCHECK, b?BST_CHECKED:BST_UNCHECKED, 0);

}

void VDUIControlCheckboxW32::Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw() {
	if (code == BN_CLICKED) {
		ProcessLinks();
		if (pCB)
			pCB->UIEvent(this, mID, IVDUICallback::kEventSelect, GetStateb());
	}
}

void VDUIControlCheckboxW32::Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw() {
}

void VDUIControlCheckboxW32::Dispatch_WM_HVSCROLL(IVDUICallback *, UINT code) throw() {
}

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlOptionW32
//
///////////////////////////////////////////////////////////////////////////

VDUIControlOptionW32::VDUIControlOptionW32(IVDUIControl *pParent)
	: mpParentOption(static_cast<VDUIControlOptionW32 *>(pParent))
	, mnItems(1)
	, mnSelected(0)
{
	if (pParent)
		++mpParentOption->mnItems;
}

VDUIControlOptionW32::~VDUIControlOptionW32() {
}

bool VDUIControlOptionW32::Create(IVDUIControl *pControl) {
	if (VDUIControlBase::Create(pControl)
		&& _Create(mpBase->CreateUniqueID(), "BUTTON", L"BUTTON",
			mpParentOption	? (BS_AUTORADIOBUTTON|BS_TOP|BS_MULTILINE|WS_TABSTOP)
			: (BS_AUTORADIOBUTTON|BS_TOP|BS_MULTILINE|WS_GROUP)))
	{
#ifdef _DEBUG
		if (mpParentOption) {
			HWND hwndPrimary = (HWND)mpParentOption->GetRawHandle();

			VDASSERT(GetWindowLong(hwndPrimary, GWL_ID) + mpParentOption->mnItems - 1 == GetWindowLong(mhwnd, GWL_ID));
		}
#endif
		if (!mpParentOption)
			SendMessage(mhwnd, BM_SETCHECK, BST_CHECKED, 0);

		return true;
	}

	return false;
}

IVDUIControlNativeCallback *VDUIControlOptionW32::AsNativeCallback() {
	return this;
}

void VDUIControlOptionW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	VDUIRect r = {0,0,14,10};

	GetBase()->MapUnitsToPixels(r);

	mLayoutSpecs.minsize.w = r.x2;
	mLayoutSpecs.minsize.h = r.y2;

	SIZE siz = _SizeText(parentConstraints.minsize.w, r.x2, r.y2);

	siz.cy += 2*GetSystemMetrics(SM_CYEDGE);

	mLayoutSpecs.minsize.w += siz.cx;
	if (mLayoutSpecs.minsize.h < siz.cy)
		mLayoutSpecs.minsize.h = siz.cy;
}

nsVDUI::eLinkMethod VDUIControlOptionW32::GetLinkMethod() {
	return nsVDUI::kLinkMethodInt;
}

nsVDUI::eCompressType VDUIControlOptionW32::GetCompressType() {
	return nsVDUI::kCompressOption;
}

int VDUIControlOptionW32::GetStatei() {
	VDASSERT(!mpParentOption);

	return mnSelected;
}

void VDUIControlOptionW32::SetStatei(int i) {
	VDASSERT(!mpParentOption);

	if (i >= mnItems)
		i = mnItems - 1;

	if (i < 0)
		i = 0;

	if (mnSelected != i) {
		mnSelected = i;

		if (mhwnd) {
			int id = GetWindowLong(mhwnd, GWL_ID);

			CheckRadioButton(::GetParent(mhwnd), id, id+mnItems-1, id+i);
		}
	}
}

void VDUIControlOptionW32::Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw() {
	if (code == BN_CLICKED) {
		if (SendMessage(mhwnd, BM_GETCHECK, 0, 0) == BST_CHECKED) {
			int val = 0;
			
			if (mpParentOption) {
				val = GetWindowLong(mhwnd, GWL_ID) - GetWindowLong(mpParentOption->mhwnd, GWL_ID);

				if (val != mpParentOption->mnSelected) {
					mpParentOption->mnSelected = val;
					mpParentOption->ProcessLinks();
				}
			} else {
				if (mnSelected) {
					mnSelected = 0;
					ProcessLinks();
				}
			}
			if (pCB)
				pCB->UIEvent(this, mID - val, IVDUICallback::kEventSelect, val);
		}
	}
}

void VDUIControlOptionW32::Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw() {
}

void VDUIControlOptionW32::Dispatch_WM_HVSCROLL(IVDUICallback *, UINT code) throw() {
}

