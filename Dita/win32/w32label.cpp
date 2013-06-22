#include "w32label.h"

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlLabelW32
//
///////////////////////////////////////////////////////////////////////////

VDUIControlLabelW32::VDUIControlLabelW32(int maxlen) : mMaxLen(maxlen) {}
VDUIControlLabelW32::~VDUIControlLabelW32() {}

bool VDUIControlLabelW32::Create(IVDUIControl *pControl) {
	if (VDUIControlBase::Create(pControl)
		&& _Create(0, "STATIC", L"STATIC", mMaxLen ? WS_TABSTOP|SS_LEFT : SS_CENTERIMAGE|WS_TABSTOP)) {

		if (mMaxLen) {
			VDUIRect rc = { 0, 0, mMaxLen, 1 };
			mpBase->MapUnitsToPixels(rc);

			mMaxLen = rc.w();
		}
		return true;
	}

	return false;
}

void VDUIControlLabelW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	SIZE siz = _SizeText(mMaxLen ? mMaxLen : 0, 0, 0);

	mLayoutSpecs.minsize.w	= siz.cx;
	mLayoutSpecs.minsize.h	= siz.cy;
}
