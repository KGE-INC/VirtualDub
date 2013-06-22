#ifndef f_VD2_DITA_W32GROUP_H
#define f_VD2_DITA_W32GROUP_H

#include "control.h"
#include "ctl_set.h"
#include "w32peer.h"

class VDUIControlGroupW32 : public VDUIControlBaseW32, public IVDUISet {
private:
	IVDUIControl *mpChildControl;

public:
	VDUIControlGroupW32();
	~VDUIControlGroupW32();

	bool Create(IVDUIControl *pControl);
	void Destroy();

	IVDUISet *AsUISet();

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);
	void PostLayoutBase(const VDUIRect& target);

	bool Add(IVDUIControl *);
	void Remove(IVDUIControl *);
	void Enable(bool b);
	void Show(bool b);

	void Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw();
};

#endif
