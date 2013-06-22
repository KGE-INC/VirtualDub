#ifndef f_VD2_DITA_CTLGRID_H
#define f_VD2_DITA_CTLGRID_H

#include <utility>
#include <map>

#include "control.h"

class VDUIGrid : public VDUIControlBase, public IVDUIGrid {
private:
	struct ChildInfo {
		IVDUIControl *pControl;
		int w, h;
	};

	typedef std::pair<int, int> tPlacement;
	typedef std::map<tPlacement, ChildInfo> tChildMap;

	tChildMap	mChildren;

	struct LineSpec {
		int minsize;
		int pos;			// size is stored as difference in positions
		int affinity;
	};

	LineSpec *mpRowSpecs;
	LineSpec *mpColSpecs;
	int mnCols, mnRows;
	int mnXPad, mnYPad;
	int mTotalRowAffinity;
	int mTotalColAffinity;

public:
	VDUIGrid(int cols, int rows);
	~VDUIGrid();

	IVDUIGrid *AsUIGrid();

	bool Create(IVDUIControl *pParent);
	void Destroy();

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);
	void PostLayoutBase(const VDUIRect& target);

	bool Add(IVDUIControl *, int x, int y, int w=1, int h=1);
	void Remove(int x, int y);
	void SetSpacing(int hspacing, int vspacing);
	void WipeAffinities(int affinity);
	void SetColumnAffinity(int col, int affinity);
	void SetRowAffinity(int row, int affinity);

	void Enable(bool b);
	void Show(bool b);
};

#endif
