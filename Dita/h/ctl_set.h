#ifndef f_VD2_DITA_CTLSET_H
#define f_VD2_DITA_CTLSET_H

#include <list>

#include "control.h"

class VDUIControlHorizontalSet : public VDUIControlBase, public IVDUISet {
private:
	typedef std::list<IVDUIControl *> tChildList;

	tChildList mChildren;
	int mnFillCount;
	int mComponentWidth;

public:
	VDUIControlHorizontalSet();
	~VDUIControlHorizontalSet();

	void Destroy();

	IVDUISet *AsUISet();

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);
	void PostLayoutBase(const VDUIRect& target);

	bool Add(IVDUIControl *);
	void Remove(IVDUIControl *);

	void Enable(bool b);
	void Show(bool b);
};

class VDUIControlVerticalSet : public VDUIControlBase, public IVDUISet {
private:
	typedef std::list<IVDUIControl *> tChildList;

	tChildList mChildren;
	int mnFillCount;
	int mComponentHeight;

public:
	VDUIControlVerticalSet();
	~VDUIControlVerticalSet();

	void Destroy();

	IVDUISet *AsUISet();

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);
	void PostLayoutBase(const VDUIRect& target);

	bool Add(IVDUIControl *);
	void Remove(IVDUIControl *);

	void Enable(bool b);
	void Show(bool b);
};

#endif
