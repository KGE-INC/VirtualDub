#include <algorithm>
#include "ctl_set.h"

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlHorizontalSet
//
///////////////////////////////////////////////////////////////////////////

VDUIControlHorizontalSet::VDUIControlHorizontalSet() {
	mAlignX = nsVDUI::kFill;
	mAlignY = nsVDUI::kFill;
}

VDUIControlHorizontalSet::~VDUIControlHorizontalSet() {
}

void VDUIControlHorizontalSet::Destroy() {
	while(!mChildren.empty()) {
		mChildren.front()->Destroy();
		mChildren.pop_front();
	}

	VDUIControlBase::Destroy();
}

IVDUISet *VDUIControlHorizontalSet::AsUISet() {
	return this;
}

void VDUIControlHorizontalSet::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	VDUIRect pad = {0, 0, 3, 3};
	nsVDUI::eCompressType lastCompressType = nsVDUI::kCompressTypeLimit;

	GetBase()->MapUnitsToPixels(pad);

	mLayoutSpecs.minsize.w = 0;
	mLayoutSpecs.minsize.h = 0;
	mnFillCount = 0;

	for(tChildList::iterator it = mChildren.begin(); it != mChildren.end(); ++it) {
		IVDUIControl *pControl = (*it);
		nsVDUI::eAlign alignX, alignY;
		nsVDUI::eCompressType compressType = pControl->GetCompressType();

		if (lastCompressType != compressType || lastCompressType == nsVDUI::kCompressNone)
			mLayoutSpecs.minsize.w += pad.x2;

		lastCompressType = compressType;

		pControl->GetAlignment(alignX, alignY);
		if ((alignX & nsVDUI::kAlignTypeMask) == nsVDUI::kFill)
			++mnFillCount;
		else {
			pControl->PreLayout(parentConstraints);

			const VDUILayoutSpecs& specs = pControl->GetLayoutSpecs();

			mLayoutSpecs.minsize.w += specs.minsize.w;
			if (mLayoutSpecs.minsize.h < specs.minsize.h)
				mLayoutSpecs.minsize.h = specs.minsize.h;
		}
	}

	if (!mChildren.empty())
		mLayoutSpecs.minsize.w -= pad.x2;

	int fillSize = std::max<int>(0, parentConstraints.minsize.w - mLayoutSpecs.minsize.w);
	int fillLeft = mnFillCount;

	if (fillLeft) {
		VDUILayoutSpecs constraints(parentConstraints);

		constraints.minsize.w = fillSize / fillLeft;

		for(tChildList::iterator it2 = mChildren.begin(); it2 != mChildren.end(); ++it2) {
			IVDUIControl *pControl = (*it2);
			nsVDUI::eAlign alignX, alignY;

			pControl->GetAlignment(alignX, alignY);

			if ((alignX & nsVDUI::kAlignTypeMask) == nsVDUI::kFill) {

				pControl->PreLayout(constraints);

				const VDUILayoutSpecs& specs = pControl->GetLayoutSpecs();

				mLayoutSpecs.minsize.w += specs.minsize.w;
				if (mLayoutSpecs.minsize.h < specs.minsize.h)
					mLayoutSpecs.minsize.h = specs.minsize.h;
			}
		}
	}

	// cache this since our minsize will be whacked by alignment specs
	mComponentWidth = mLayoutSpecs.minsize.w;
}

void VDUIControlHorizontalSet::PostLayoutBase(const VDUIRect& target) {
	VDUIRect pad = {0, 0, 3, 3};
	nsVDUI::eCompressType lastCompressType = nsVDUI::kCompressTypeLimit;

	GetBase()->MapUnitsToPixels(pad);

	int x = target.x1 - pad.x2;
	int spill = target.w() - mComponentWidth;
	int fillleft = mnFillCount;

	for(tChildList::iterator it = mChildren.begin(); it != mChildren.end(); ++it) {
		IVDUIControl *pControl = (*it);
		nsVDUI::eAlign alignX, alignY;
		nsVDUI::eCompressType compressType = pControl->GetCompressType();

		if (lastCompressType != compressType || lastCompressType == nsVDUI::kCompressNone)
			x += pad.x2;

		lastCompressType = compressType;

		pControl->GetAlignment(alignX, alignY);

		const VDUILayoutSpecs& specs = pControl->GetLayoutSpecs();
		int w = specs.minsize.w;

		if ((alignX & nsVDUI::kAlignTypeMask) == nsVDUI::kFill) {
			int span = (spill + fillleft - 1) / fillleft;

			w += span;
			spill -= span;
			--fillleft;
		}

		VDUIRect rDest = {x, target.y1, x+w, target.y2};

		pControl->PostLayout(rDest);

		x += w;
	}
}

bool VDUIControlHorizontalSet::Add(IVDUIControl *pControl) {
	mChildren.push_back(pControl);

	if (!pControl->Create(this)) {
		mChildren.pop_back();
		return false;
	}

	GetBase()->AddNonlocal(pControl);

	return true;
}

void VDUIControlHorizontalSet::Remove(IVDUIControl *pControl) {
	tChildList::iterator it = std::find(mChildren.begin(), mChildren.end(), pControl);

	if (it != mChildren.end()) {
		pControl->Destroy();
		mChildren.erase(it);
	} else
		VDASSERT(false);
}

void VDUIControlHorizontalSet::Show(bool b) {
	VDUIControlBase::Show(b);
	tChildList::iterator it = mChildren.begin(), itEnd = mChildren.end();

	for(; it!=itEnd; ++it)
		(*it)->Show((*it)->IsVisible());
}

void VDUIControlHorizontalSet::Enable(bool b) {
	VDUIControlBase::Enable(b);
	tChildList::iterator it = mChildren.begin(), itEnd = mChildren.end();

	for(; it!=itEnd; ++it)
		(*it)->Enable((*it)->IsEnabled());
}

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlVerticalSet
//
///////////////////////////////////////////////////////////////////////////

VDUIControlVerticalSet::VDUIControlVerticalSet() {
	mAlignX = nsVDUI::kFill;
	mAlignY = nsVDUI::kFill;
}

VDUIControlVerticalSet::~VDUIControlVerticalSet() {
}

void VDUIControlVerticalSet::Destroy() {
	while(!mChildren.empty()) {
		mChildren.front()->Destroy();
		mChildren.pop_front();
	}

	VDUIControlBase::Destroy();
}

IVDUISet *VDUIControlVerticalSet::AsUISet() {
	return this;
}

void VDUIControlVerticalSet::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	VDUIRect pad = {0, 0, 3, 3};
	nsVDUI::eCompressType lastCompressType = nsVDUI::kCompressTypeLimit;

	GetBase()->MapUnitsToPixels(pad);

	mLayoutSpecs.minsize.w = 0;
	mLayoutSpecs.minsize.h = 0;
	mnFillCount = 0;

	for(tChildList::iterator it = mChildren.begin(); it != mChildren.end(); ++it) {
		IVDUIControl *pControl = (*it);
		nsVDUI::eAlign alignX, alignY;
		nsVDUI::eCompressType compressType = pControl->GetCompressType();

		if (lastCompressType != compressType || lastCompressType == nsVDUI::kCompressNone)
			mLayoutSpecs.minsize.h += pad.y2;

		lastCompressType = compressType;

		pControl->PreLayout(parentConstraints);
		pControl->GetAlignment(alignX, alignY);

		if (alignY == nsVDUI::kFill)
			++mnFillCount;

		const VDUILayoutSpecs& specs = pControl->GetLayoutSpecs();

		mLayoutSpecs.minsize.h += specs.minsize.h;
		if (mLayoutSpecs.minsize.w < specs.minsize.w)
			mLayoutSpecs.minsize.w = specs.minsize.w;
	}

	if (!mChildren.empty())
		mLayoutSpecs.minsize.h -= pad.y2;

	// cache this since our minsize will be whacked by alignment specs
	mComponentHeight = mLayoutSpecs.minsize.h;
}

void VDUIControlVerticalSet::PostLayoutBase(const VDUIRect& target) {
	VDUIRect pad = {0, 0, 3, 3};
	nsVDUI::eCompressType lastCompressType = nsVDUI::kCompressTypeLimit;

	GetBase()->MapUnitsToPixels(pad);

	int y = target.y1 - pad.y2;
	int spill = target.h() - mComponentHeight;
	int fillleft = mnFillCount;

	for(tChildList::iterator it = mChildren.begin(); it != mChildren.end(); ++it) {
		IVDUIControl *pControl = (*it);
		nsVDUI::eAlign alignX, alignY;
		nsVDUI::eCompressType compressType = pControl->GetCompressType();

		if (lastCompressType != compressType || lastCompressType == nsVDUI::kCompressNone)
			y += pad.y2;

		lastCompressType = compressType;

		pControl->GetAlignment(alignX, alignY);

		const VDUILayoutSpecs& specs = pControl->GetLayoutSpecs();
		int h = specs.minsize.h;

		if (alignY == nsVDUI::kFill) {
			int span = (spill + fillleft - 1) / fillleft;

			h += span;
			spill -= span;
			--fillleft;
		}

		VDUIRect rDest = {target.x1, y, target.x2, y+h};

		pControl->PostLayout(rDest);

		y += h;
	}
}

bool VDUIControlVerticalSet::Add(IVDUIControl *pControl) {
	mChildren.push_back(pControl);

	if (!pControl->Create(this)) {
		mChildren.pop_back();
		return false;
	}

	GetBase()->AddNonlocal(pControl);

	return true;
}

void VDUIControlVerticalSet::Remove(IVDUIControl *pControl) {
	tChildList::iterator it = std::find(mChildren.begin(), mChildren.end(), pControl);

	if (it != mChildren.end()) {
		pControl->Destroy();
		mChildren.erase(it);
	} else
		VDASSERT(false);
}

void VDUIControlVerticalSet::Show(bool b) {
	VDUIControlBase::Show(b);
	tChildList::iterator it = mChildren.begin(), itEnd = mChildren.end();

	for(; it!=itEnd; ++it)
		(*it)->Show((*it)->IsVisible());
}

void VDUIControlVerticalSet::Enable(bool b) {
	VDUIControlBase::Enable(b);

	tChildList::iterator it = mChildren.begin(), itEnd = mChildren.end();

	for(; it!=itEnd; ++it)
		(*it)->Enable((*it)->IsEnabled());
}
