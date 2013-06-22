#include <math.h>

#include "control.h"

VDUIControlBase::VDUIControlBase()
	: mID(0)
	, mpBase(NULL)
	, mpParent(NULL)
	, mMinSize(0,0)
	, mMaxSize(0,0)
	, mAlignX(nsVDUI::kLeft)
	, mAlignY(nsVDUI::kTop)
	, mDesiredAspectRatio(1.0f)
	, mbActuallyEnabled(true)
	, mbEnabled(true)
	, mbActuallyVisible(true)
	, mbVisible(true)
{
}

void VDUIControlBase::PreLayout(const VDUILayoutSpecs& parentConstraints) {
	VDUILayoutSpecs constraints(parentConstraints);
	VDUILayoutSpecs lastSpecs;

	PreLayoutAttempt(constraints);
	lastSpecs = mLayoutSpecs;

	if (mMaxSize.w) {
		VDDEBUG("Attempting to crunch control - %d -> %d\n", mLayoutSpecs.minsize.w, mMaxSize.w);
		while(mLayoutSpecs.minsize.w > mMaxSize.w) {
			constraints.minsize.w = mLayoutSpecs.minsize.w - 1;
			PreLayoutAttempt(constraints);

			if (mLayoutSpecs.minsize.w >= lastSpecs.minsize.w) {
				mLayoutSpecs = lastSpecs;
				break;
			}
			VDDEBUG("  ...crunched to %d x %d\n", mLayoutSpecs.minsize.w, mLayoutSpecs.minsize.h);

			lastSpecs = mLayoutSpecs;
		}
	} else if (mMaxSize.h) {
		while(mLayoutSpecs.minsize.h > mMaxSize.h) {
			constraints.minsize.h = mLayoutSpecs.minsize.h - 1;
			PreLayoutAttempt(constraints);

			if (mLayoutSpecs.minsize.h >= lastSpecs.minsize.h) {
				mLayoutSpecs = lastSpecs;
				break;
			}

			lastSpecs = mLayoutSpecs;
		}
	}
}

void VDUIControlBase::PreLayoutAttempt(const VDUILayoutSpecs& parentConstraints) {
	PreLayoutBase(parentConstraints);

	int& w = mLayoutSpecs.minsize.w;
	int& h = mLayoutSpecs.minsize.h;

	if (w < mMinSize.w)
		w = mMinSize.w;

	if (h < mMinSize.h)
		h = mMinSize.h;

	if (w && h && ((mAlignX|mAlignY) & nsVDUI::kExpandFlag)) {
		float rCurrentAR = (float)w / (float)h;

		if (rCurrentAR > mDesiredAspectRatio) {			// wider/shorter than desired
			if (mAlignY & nsVDUI::kExpandFlag) {
				h = (int)ceil(w / mDesiredAspectRatio);
			}
		} else if (rCurrentAR < mDesiredAspectRatio) {	// narrower/taller than desired
			if (mAlignX & nsVDUI::kExpandFlag) {
				w = (int)ceil(h * mDesiredAspectRatio);
			}
		}
	}
}

void VDUIControlBase::PostLayout(const VDUIRect& target) {
	VDUIRect				r(target);
	int						alignx = mAlignX, aligny = mAlignY;
	const VDUILayoutSpecs&	specs = GetLayoutSpecs();

	alignx &= nsVDUI::kAlignTypeMask;
	aligny &= nsVDUI::kAlignTypeMask;

	if (alignx != nsVDUI::kFill) {
		int padX = ((target.w() - specs.minsize.w) * ((int)alignx - 1) + 1) >> 1;

		VDASSERT(padX >= 0);

		r.x1 += padX;
		r.x2 = r.x1 + specs.minsize.w;
	}

	if (aligny != nsVDUI::kFill) {
		int padY = ((target.h() - specs.minsize.h) * ((int)aligny - 1) + 1) >> 1;

		VDASSERT(padY >= 0);

		r.y1 += padY;
		r.y2 = r.y1 + specs.minsize.h;
	}

	PostLayoutBase(r);
}

void VDUIControlBase::Show(bool b) {
	mbActuallyVisible = mbVisible = b;
	if (mpParent)
		mbActuallyVisible &= mpParent->IsActuallyVisible();
}

void VDUIControlBase::Enable(bool b) {
	mbActuallyEnabled = mbEnabled = b;
	if (mpParent)
		mbActuallyEnabled &= mpParent->IsActuallyEnabled();
}
