#include "w32group.h"

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlGroupW32
//
///////////////////////////////////////////////////////////////////////////

VDUIControlGroupW32::VDUIControlGroupW32()
	: mpChildControl(NULL)
{
}

VDUIControlGroupW32::~VDUIControlGroupW32() {
}

bool VDUIControlGroupW32::Create(IVDUIControl *pControl) {
	return VDUIControlBase::Create(pControl)
		&& _Create(0, "BUTTON", L"BUTTON", BS_GROUPBOX|WS_TABSTOP);
}

void VDUIControlGroupW32::Destroy() {
	if (mpChildControl)
		mpChildControl->Destroy();

	VDUIControlBase::Destroy();
}

IVDUISet *VDUIControlGroupW32::AsUISet() {
	return this;
}

void VDUIControlGroupW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	VDUIRect r = {0,6,6,12};

	GetBase()->MapUnitsToPixels(r);

	r.x2 += r.x2;

	mLayoutSpecs.minsize.w	= r.x2;
	mLayoutSpecs.minsize.h	= r.y1 + r.y2;

	if (mpChildControl) {
		VDUILayoutSpecs ls;

		ls.minsize.w = parentConstraints.minsize.w - mLayoutSpecs.minsize.w;
		ls.minsize.h = parentConstraints.minsize.h - mLayoutSpecs.minsize.h;

		mpChildControl->PreLayout(ls);

		const VDUILayoutSpecs& specs = mpChildControl->GetLayoutSpecs();

		mLayoutSpecs.minsize.w += specs.minsize.w;
		mLayoutSpecs.minsize.h += specs.minsize.h;
	}
}

void VDUIControlGroupW32::PostLayoutBase(const VDUIRect& target) {
	VDUIControlBaseW32::PostLayoutBase(target);

	if (mpChildControl) {
		VDUIRect r = {0,6,6,12};

		GetBase()->MapUnitsToPixels(r);

		VDUIRect rDest = { target.x1 + r.x2, target.y1 + r.y2, target.x2 - r.x2, target.y2 - r.y1 };

		mpChildControl->PostLayout(rDest);
	}
}

bool VDUIControlGroupW32::Add(IVDUIControl *pControl) {
	if (mpChildControl)
		mpChildControl->Destroy();

	mpChildControl = pControl;
	if (!pControl->Create(this)) {
		mpChildControl = NULL;
		return false;
	}

	GetBase()->AddNonlocal(pControl);

	return true;
}

void VDUIControlGroupW32::Remove(IVDUIControl *pControl) {
	if (mpChildControl)
		mpChildControl->Destroy();

	mpChildControl = NULL;
}

void VDUIControlGroupW32::Show(bool b) {
	VDUIControlBaseW32::Show(b);

	if (mpChildControl)
		mpChildControl->Show(mpChildControl->IsVisible());
}

void VDUIControlGroupW32::Enable(bool b) {
	VDUIControlBaseW32::Enable(b);

	if (mpChildControl)
		mpChildControl->Enable(mpChildControl->IsEnabled());
}
