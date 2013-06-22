#pragma warning(disable: 4786)

#include "ctl_grid.h"

VDUIGrid::VDUIGrid(int cols, int rows) throw()
		: mpColSpecs(new LineSpec[cols+1])		// +1 for end sentinel in both cases
		, mpRowSpecs(new LineSpec[rows+1])
		, mnCols(cols)
		, mnRows(rows)
		, mnXPad(0)
		, mnYPad(0)
{
	VDASSERT(rows>0 && cols>0);

	WipeAffinities(1);
}

VDUIGrid::~VDUIGrid() throw() {
	delete[] mpRowSpecs;
	delete[] mpColSpecs;
}

IVDUIGrid *VDUIGrid::AsUIGrid() {
	return this;
}

bool VDUIGrid::Create(IVDUIControl *pParent) {
	if (VDUIControlBase::Create(pParent)) {
		SetSpacing(3, 3);
		return true;
	}

	return false;
}

void VDUIGrid::Destroy() {
	for(tChildMap::iterator it = mChildren.begin(); it != mChildren.end(); ++it) {
		(*it).second.pControl->Destroy();
	}

	mChildren.clear();

	VDUIControlBase::Destroy();
}

void VDUIGrid::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	int i,j;

	for(i=0; i<mnCols; ++i)
		mpColSpecs[i].minsize = 0;

	for(j=0; j<mnRows; ++j)
		mpRowSpecs[j].minsize = 0;

	// Iterate over all children in the list and tally up their requirements.

	for(tChildMap::iterator it = mChildren.begin(); it != mChildren.end(); ++it) {
		IVDUIControl *pControl = (*it).second.pControl;
		const int x = (*it).first.first;
		const int y = (*it).first.second;
		const int w = (*it).second.w;
		const int h = (*it).second.h;

		pControl->PreLayout(parentConstraints);

		const VDUILayoutSpecs& specs = pControl->GetLayoutSpecs();

		// If the control only takes one grid cell, enforce its minimums
		// upon the row and column it sits in.  Otherwise, split the minima
		// amongst all rows and columns evenly -- this doesn't guarantee
		// the optimal grid size, but it's reasonable enough.

		if (w == 1) {
			if (mpColSpecs[x].minsize < specs.minsize.w)
				mpColSpecs[x].minsize = specs.minsize.w;
		} else {
			int left = specs.minsize.w;

			for(int wt=0; wt<w; ++wt) {
				int tc = (left + (w-wt) - 1) / (w-wt);

				if (mpColSpecs[x+wt].minsize < tc)
					mpColSpecs[x+wt].minsize = tc;

				left -= tc;
			}
		}

		if (h == 1) {
			if (mpRowSpecs[y].minsize < specs.minsize.h)
				mpRowSpecs[y].minsize = specs.minsize.h;
		} else {
			int left = specs.minsize.h;

			for(int ht=0; ht<h; ++ht) {
				int tc = (left + (h-ht) - 1) / (h-ht);

				if (mpRowSpecs[y+ht].minsize < tc)
					mpRowSpecs[y+ht].minsize = tc;

				left -= tc;
			}
		}

	}

	// Compute aggregate minima.

	mLayoutSpecs.minsize.w = (mnCols-1) * mnXPad;
	mLayoutSpecs.minsize.h = (mnRows-1) * mnYPad;

	mTotalRowAffinity = mTotalColAffinity = 0;

	for(i=0; i<mnCols; ++i) {
		mLayoutSpecs.minsize.w += mpColSpecs[i].minsize;
		mTotalColAffinity += mpColSpecs[i].affinity;
	}

	for(i=0; i<mnRows; ++i) {
		mLayoutSpecs.minsize.h += mpRowSpecs[i].minsize;
		mTotalRowAffinity += mpRowSpecs[i].affinity;
	}
}

void VDUIGrid::PostLayoutBase(const VDUIRect& target) {
	// Compute excesses and split amongst rows/cols as we compute each
	// position.

	int excessw = (target.x2 - target.x1) - mLayoutSpecs.minsize.w;
	int excessh = (target.y2 - target.y1) - mLayoutSpecs.minsize.h;
	int i, pos, affinity_left;

	if (excessw<0)
		excessw=0;

	if (excessh<0)
		excessh=0;

	pos = 0;
	affinity_left = mTotalColAffinity;
	for(i=0; i<mnCols; ++i) {
		int affinity = mpColSpecs[i].affinity;

		mpColSpecs[i].pos = pos;
		pos += mpColSpecs[i].minsize + mnXPad;

		if (affinity) {
			int slop = (excessw * affinity + affinity_left - 1) / affinity_left;

			pos += slop;
			excessw -= slop;
			affinity_left -= affinity;
		}
	}
	mpColSpecs[i].pos = pos;

	pos = 0;
	affinity_left = mTotalRowAffinity;
	for(i=0; i<mnRows; ++i) {
		int affinity = mpRowSpecs[i].affinity;

		mpRowSpecs[i].pos = pos;
		pos += mpRowSpecs[i].minsize + mnYPad;

		if (affinity) {
			int slop = (excessh * affinity + affinity_left - 1) / affinity_left;

			pos += slop;

			excessh -= slop;
			affinity_left -= affinity;
		}
	}
	mpRowSpecs[i].pos = pos;

	// Reposition all controls.

	for(tChildMap::iterator it = mChildren.begin(); it != mChildren.end(); ++it) {
		IVDUIControl *pControl = (*it).second.pControl;
		const int placeX = (*it).first.first;
		const int placeY = (*it).first.second;
		const int placeW = (*it).second.w;
		const int placeH = (*it).second.h;

		VDUIRect r = {
			target.x1 + mpColSpecs[placeX].pos,
			target.y1 + mpRowSpecs[placeY].pos,
			target.x1 + mpColSpecs[placeX + placeW].pos - mnXPad,
			target.y1 + mpRowSpecs[placeY + placeH].pos - mnYPad
		};

		pControl->PostLayout(r);
	}
}

bool VDUIGrid::Add(IVDUIControl *pControl, int x, int y, int w, int h) {
	Remove(x, y);

	ChildInfo cinfo = { pControl, w, h };

	std::pair<tChildMap::iterator, bool> rval;

	rval = mChildren.insert(tChildMap::value_type(tPlacement(x, y), cinfo));

	if (rval.second) {
		if (pControl->Create(this)) {
			GetBase()->AddNonlocal(pControl);
			return true;
		}

		mChildren.erase(rval.first);
	}

	return false;
}

void VDUIGrid::Remove(int x, int y) {
	mChildren.erase(tPlacement(x, y));
}

void VDUIGrid::SetSpacing(int hspacing, int vspacing) {
	VDUIRect r = { 0, 0, hspacing, vspacing };

	GetBase()->MapUnitsToPixels(r);

	if (hspacing >= 0)
		mnXPad = r.x2;

	if (vspacing >= 0)
		mnYPad = r.y2;
}

void VDUIGrid::WipeAffinities(int affinity) {
	int i;

	for(i=0; i<mnCols; ++i)
		mpColSpecs[i].affinity = affinity;

	for(i=0; i<mnRows; ++i)
		mpRowSpecs[i].affinity = affinity;
}

void VDUIGrid::SetRowAffinity(int row, int affinity) throw() {
	if (row >= 0 && row < mnRows) {
		mpRowSpecs[row].affinity = affinity;
	} else
		VDASSERT(false);
}

void VDUIGrid::SetColumnAffinity(int col, int affinity) throw() {
	if (col >= 0 && col < mnCols) {
		mpColSpecs[col].affinity = affinity;
	} else
		VDASSERT(false);
}

void VDUIGrid::Show(bool b) {
	VDUIControlBase::Show(b);
	tChildMap::iterator it = mChildren.begin(), itEnd = mChildren.end();

	for(; it!=itEnd; ++it)
		(*it).second.pControl->Show((*it).second.pControl->IsVisible());
}

void VDUIGrid::Enable(bool b) {
	VDUIControlBase::Enable(b);
	tChildMap::iterator it = mChildren.begin(), itEnd = mChildren.end();

	for(; it!=itEnd; ++it)
		(*it).second.pControl->Enable((*it).second.pControl->IsEnabled());
}
