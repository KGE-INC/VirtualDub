#include <list>

#include <vd2/system/VDAtomic.h>
#include <vd2/Dita/interface.h>

struct VDUINestingContext {
	IVDUIControl			*mpContainer;
	IVDUIGrid				*mpAsGrid;
	IVDUISet				*mpAsSet;
	nsVDUI::eAlign			mAlignX, mAlignY;
	int						mMinW, mMinH;
	int						mMaxW, mMaxH;
	int						mGridW, mGridH;
	int						mGridPosX, mGridPosY;
	bool					*mpGridMap;
	nsVDUI::eGridDirection	mGridDirection;

	VDUINestingContext() : mpGridMap(NULL) {}

	void set(IVDUIControl *pC) {
		mpContainer	= pC;
		mpAsGrid	= pC->AsUIGrid();
		mpAsSet		= pC->AsUISet();
		mAlignX		= nsVDUI::kAlignDefault;
		mAlignY		= nsVDUI::kAlignDefault;
		mMinW		= 0;
		mMinH		= 0;
		mMaxW		= 0;
		mMaxH		= 0;
		mpGridMap	= NULL;
		mGridPosX	= 0;
		mGridPosY	= 0;
		mGridDirection = nsVDUI::kHorizontalDirection;
	}

	~VDUINestingContext() {
		delete[] mpGridMap;
	}
};

class VDUIConstructor : public IVDUIConstructor {
private:
	VDAtomicInt mRefCount;

	IVDUIContext *mpContext;
	IVDUIControl *mpBaseControl;
	IVDUIControl *mpFirstOption;
	IVDUIControl *mpLastControl;
	unsigned	mNextOptionID;

	int			mFlowSpanW;
	int			mFlowSpanH;

	bool		mbErrorState;

	////

	typedef std::list<VDUINestingContext> tContextList;

	tContextList mContexts;

	////

	bool _AddControl(IVDUIControl *pControl, unsigned id, tCSW label);
	bool _PushContext(IVDUIControl *pControl, unsigned id, tCSW label);
	bool _ReplaceContext(IVDUIControl *pControl, unsigned id, tCSW label);
	void _PopContext();

public:
	int AddRef() { return mRefCount.inc(); }
	int Release() { int rv = mRefCount.dec(); if (!rv) delete this; return rv; }

	void Init(IVDUIContext *pContext, IVDUIControl *pBase);

	// Error detection

	bool GetErrorState() { return mbErrorState; }

	// Accessors

	IVDUIControl *GetLastControl() { return mpLastControl; }

	// Modifiers

	void SetAlignment	(int, int);
	void SetMinimumSize (int xa, int ya);
	void SetMaximumSize (int xa, int ya);

	// Basic controls

	void AddLabel		(unsigned id, tCSW label, int maxlen);
	void AddEdit		(unsigned id, tCSW label, int maxlen);
	void AddEditInt		(unsigned id, int initv, int minv, int maxv);
	void AddButton		(unsigned id, tCSW label);
	void AddCheckbox	(unsigned id, tCSW label);
	void AddListbox		(unsigned id, int minrows);
	void AddCombobox	(unsigned id, int minrows);
	void AddListView	(unsigned id, int minrows);
	void AddTrackbar	(unsigned id, int minv, int maxv);
	void AddFileControl	(unsigned id, tCSW label, int maxlen);

	// Grouped controls

	void BeginOptionSet	(unsigned id);
	void AddOption		(tCSW label);
	void EndOptionSet	();

	// Layout sets

	void BeginHorizSet	(unsigned id);
	void BeginVertSet	(unsigned id);
	void BeginGroupSet	(unsigned id, tCSW label);
	void EndSet			();

	// Grids

	void BeginGrid		(unsigned id, int cols, int rows, int xpad = -1, int ypad = -1, int default_affinity = -1);
	void SetGridDirection(nsVDUI::eGridDirection);
	void SpanNext		(int w, int h);
	void SetGridPos		(int x, int y);
	void SkipGrid		(int x, int y);
	void SetGridColumnAffinity(int x, int affinity);
	void SetGridRowAffinity(int y, int affinity);
	void EndGrid		();
};

///////////////////////////////////////////////////////////////////////////

IVDUIConstructor *VDCreateUIConstructor() {
	return new VDUIConstructor;
}

///////////////////////////////////////////////////////////////////////////

void VDUIConstructor::Init(IVDUIContext *pContext, IVDUIControl *pBase) {
	mpContext = pContext;
	mpBaseControl = pBase;
	mbErrorState = false;

	// Shove a vertset onto the stack.

	IVDUIControl *pControl = pContext->CreateVerticalSet();
	pControl->SetID(0);
	pControl->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);
	pBase->AsBase()->Add(pControl);
	pBase->AsBase()->SetLayoutControl(pControl);
	mContexts.push_front(VDUINestingContext());
	mContexts.front().set(pControl);
}

///////////////////////////////////////////////////////////////////////////

bool VDUIConstructor::_AddControl(IVDUIControl *pControl, unsigned id, tCSW label) {
	if (pControl) {
		VDUINestingContext& ctx = mContexts.front();

		if ((!ctx.mpGridMap || (ctx.mGridPosX+mFlowSpanW <= ctx.mGridW && ctx.mGridPosY+mFlowSpanH <= ctx.mGridH
				&& !ctx.mpGridMap[ctx.mGridPosX + ctx.mGridPosY * ctx.mGridW]))) {
			bool bSuccess = false;

			pControl->SetID(id);
			pControl->SetAlignment(ctx.mAlignX, ctx.mAlignY);
			pControl->SetMinimumSize(VDUISize(ctx.mMinW, ctx.mMinH));
			pControl->SetMaximumSize(VDUISize(ctx.mMaxW, ctx.mMaxH));

			if (ctx.mpAsGrid)
				bSuccess = ctx.mpAsGrid->Add(pControl, ctx.mGridPosX, ctx.mGridPosY, mFlowSpanW, mFlowSpanH);
			else if (ctx.mpAsSet)
				bSuccess = ctx.mpAsSet->Add(pControl);
			else
				VDASSERT(false);

			if (!bSuccess) {
				mbErrorState = true;
				return false;
			}

			if (label)
				pControl->SetTextw(label);

			// Autoincrement the control position for a grid.

			if (ctx.mpGridMap) {
				for(int j=0; j<mFlowSpanH; ++j)
					for(int i=0; i<mFlowSpanW; ++i)
						ctx.mpGridMap[(ctx.mGridPosX+i) + (ctx.mGridPosY+j)*ctx.mGridW] = true;

				if (ctx.mGridDirection == nsVDUI::kHorizontalDirection) {
					ctx.mGridPosX += mFlowSpanW;

					for(;;) {
						if (ctx.mGridPosX >= ctx.mGridW) {
							ctx.mGridPosX = 0;
							++ctx.mGridPosY;
						}

						if (ctx.mGridPosY >= ctx.mGridH || !ctx.mpGridMap[ctx.mGridPosX + ctx.mGridPosY * ctx.mGridW])
							break;

						++ctx.mGridPosX;
					}
				} else {
					ctx.mGridPosY += mFlowSpanH;

					for(;;) {
						if (ctx.mGridPosY >= ctx.mGridH) {
							ctx.mGridPosY = 0;
							++ctx.mGridPosX;
						}

						if (ctx.mGridPosX >= ctx.mGridW || !ctx.mpGridMap[ctx.mGridPosX + ctx.mGridPosY * ctx.mGridW])
							break;

						++ctx.mGridPosY;
					}
				}
			}

			mFlowSpanW = mFlowSpanH = 1;
			mpLastControl = pControl;
			return true;
		}

		pControl->Destroy();
	}

	mFlowSpanW = mFlowSpanH = 1;
	mbErrorState = true;

	return false;
}

bool VDUIConstructor::_PushContext(IVDUIControl *pControl, unsigned id, tCSW label) {
	if (_AddControl(pControl, id, label)) {
		mContexts.push_front(VDUINestingContext());

		mContexts.front().set(pControl);
		return true;
	}
	return false;
}

bool VDUIConstructor::_ReplaceContext(IVDUIControl *pControl, unsigned id, tCSW label) {
	VDASSERT(!mContexts.empty());

	VDUINestingContext& ctx = mContexts.front();

	if (!_AddControl(pControl, id, label))
		return false;

	ctx.set(pControl);

	return true;
}

void VDUIConstructor::_PopContext() {
	if (&mContexts.front() == &mContexts.back()) {
		mbErrorState = true;
	} else {
		mContexts.pop_front();
	}
}

///////////////////////////////////////////////////////////////////////////

void VDUIConstructor::SetAlignment(int xa, int ya) {
	VDUINestingContext& ctx = mContexts.front();

	if (xa)
		ctx.mAlignX = (nsVDUI::eAlign)xa;

	if (ya)
		ctx.mAlignY = (nsVDUI::eAlign)ya;
}

void VDUIConstructor::SetMinimumSize(int xa, int ya) {
	VDUINestingContext& ctx = mContexts.front();
	VDUIRect r = { 0, 0, xa, ya };

	mpBaseControl->AsBase()->MapUnitsToPixels(r);

	ctx.mMinW = r.w();
	ctx.mMinH = r.h();
}

void VDUIConstructor::SetMaximumSize(int xa, int ya) {
	VDUINestingContext& ctx = mContexts.front();
	VDUIRect r = { 0, 0, xa, ya };

	mpBaseControl->AsBase()->MapUnitsToPixels(r);

	ctx.mMaxW = r.w();
	ctx.mMaxH = r.h();
}

///////////////////////////////////////////////////////////////////////////

void VDUIConstructor::AddLabel(unsigned id, tCSW label, int maxlen) {
	_AddControl(mpContext->CreateLabel(maxlen), id, label);
}

void VDUIConstructor::AddEdit(unsigned id, tCSW label, int maxlen) {
	_AddControl(mpContext->CreateEdit(maxlen), id, label);
}

void VDUIConstructor::AddEditInt(unsigned id, int initv, int minv, int maxv) {
	IVDUIControl *pControl = mpContext->CreateEditInt(minv, maxv);

	if (_AddControl(pControl, id, NULL))
		pControl->SetStatei(initv);
}

void VDUIConstructor::AddButton(unsigned id, tCSW label) {
	_AddControl(mpContext->CreateButton(), id, label);
}

void VDUIConstructor::AddCheckbox(unsigned id, tCSW label) {
	_AddControl(mpContext->CreateCheckbox(), id, label);
}

void VDUIConstructor::AddListbox(unsigned id, int minrows) {
	IVDUIControl *pControl = mpContext->CreateListbox(minrows);

	_AddControl(pControl, id, NULL);
}

void VDUIConstructor::AddCombobox(unsigned id, int minrows) {
	IVDUIControl *pControl = mpContext->CreateCombobox(minrows);

	_AddControl(pControl, id, NULL);
}

void VDUIConstructor::AddListView(unsigned id, int minrows) {
	IVDUIControl *pControl = mpContext->CreateListView(minrows);

	_AddControl(pControl, id, NULL);
}

void VDUIConstructor::AddTrackbar(unsigned id, int minv, int maxv) {
	IVDUIControl *pControl = mpContext->CreateTrackbar(minv, maxv);

	_AddControl(pControl, id, NULL);
}

void VDUIConstructor::AddFileControl(unsigned id, tCSW label, int maxlen) {
	_AddControl(mpContext->CreateFileControl(maxlen), id, label);
}

// Grouped controls

void VDUIConstructor::BeginOptionSet(unsigned id) {
	mpFirstOption = NULL;
	mNextOptionID = id;
}

void VDUIConstructor::AddOption(tCSW label) {
	IVDUIControl *pControl = mpContext->CreateOption(mpFirstOption);

	if (_AddControl(pControl, mNextOptionID, label)) {
		++mNextOptionID;

		if (!mpFirstOption)
			mpFirstOption = pControl;
	}	
}

void VDUIConstructor::EndOptionSet() {
	mpFirstOption = NULL;
}

// Layout sets

void VDUIConstructor::BeginHorizSet(unsigned id) {
	_PushContext(mpContext->CreateHorizontalSet(), id, NULL);
}

void VDUIConstructor::BeginVertSet(unsigned id) {
	_PushContext(mpContext->CreateVerticalSet(), id, NULL);
}

void VDUIConstructor::BeginGroupSet(unsigned id, tCSW label) {
	IVDUIControl *pControl = mpContext->CreateGroup();

	if (_PushContext(pControl, id, label))
		_ReplaceContext(mpContext->CreateVerticalSet(), id, NULL);
}

void VDUIConstructor::EndSet() {
	_PopContext();
}

// Grids

void VDUIConstructor::BeginGrid(unsigned id, int cols, int rows, int xpad, int ypad, int default_affinity) {
	IVDUIControl *pControl;

	if (_PushContext(pControl = mpContext->CreateGrid(cols, rows), id, NULL)) {
		VDUINestingContext& ctx = mContexts.front();
		IVDUIGrid *pGrid = pControl->AsUIGrid();

		if (default_affinity >= 0)
			pGrid->WipeAffinities(default_affinity);

		pGrid->SetSpacing(xpad, ypad);

		ctx.mGridW = cols;
		ctx.mGridH = rows;
		ctx.mpGridMap = new bool[cols*rows];

		for(int i=0; i<cols*rows; ++i)
			ctx.mpGridMap[i] = false;
	}
}

void VDUIConstructor::SetGridDirection(nsVDUI::eGridDirection d) {
	VDUINestingContext& ctx = mContexts.front();

	if (!ctx.mpAsGrid) {
		mbErrorState = true;
		return;
	}

	ctx.mGridDirection = d;
}

void VDUIConstructor::SpanNext(int w, int h) {
	mFlowSpanW = w;
	mFlowSpanH = h;
}

void VDUIConstructor::SetGridPos(int x, int y) {
	VDUINestingContext& ctx = mContexts.front();

	if (!ctx.mpAsGrid || (x|y)<0 || x>=ctx.mGridW || y>=ctx.mGridH
				|| ctx.mpGridMap[y*ctx.mGridW+x]) {
		mbErrorState = true;
		return;
	}

	ctx.mGridPosX = x;
	ctx.mGridPosY = y;
}

void VDUIConstructor::SkipGrid(int x, int y) {
	VDUINestingContext& ctx = mContexts.front();

	if (!ctx.mpAsGrid || (x|y)<0) {
		mbErrorState = true;
		return;
	}

	if (ctx.mGridDirection == nsVDUI::kHorizontalDirection) {
		if (y) {
			ctx.mGridPosY += y;
			ctx.mGridPosX = 0;

		}

		ctx.mGridPosX += x;
		if (ctx.mGridPosX >= ctx.mGridW) {
			ctx.mGridPosX = 0;
			++ctx.mGridPosY;
		}
	} else {
		if (x) {
			ctx.mGridPosX += x;
			ctx.mGridPosY = 0;

		}

		ctx.mGridPosY += y;
		if (ctx.mGridPosY >= ctx.mGridH) {
			ctx.mGridPosY = 0;
			++ctx.mGridPosX;
		}
	}
}

void VDUIConstructor::SetGridColumnAffinity(int x, int affinity) {
	VDUINestingContext& ctx = mContexts.front();

	if (!ctx.mpAsGrid || x<0 || x>=ctx.mGridW) {
		mbErrorState = true;
		return;
	}

	ctx.mpAsGrid->SetColumnAffinity(x, affinity);
}

void VDUIConstructor::SetGridRowAffinity(int y, int affinity) {
	VDUINestingContext& ctx = mContexts.front();

	if (!ctx.mpAsGrid || y<0 || y>=ctx.mGridH) {
		mbErrorState = true;
		return;
	}

	ctx.mpAsGrid->SetRowAffinity(y, affinity);
}

void VDUIConstructor::EndGrid() {
	VDUINestingContext& ctx = mContexts.front();

	if (!ctx.mpAsGrid) {
		mbErrorState = true;
		return;
	}

	_PopContext();
}
