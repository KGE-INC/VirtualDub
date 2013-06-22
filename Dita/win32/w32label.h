#ifndef f_VD2_DITA_W32LABEL_H
#define f_VD2_DITA_W32LABEL_H

#include <windows.h>

#include "control.h"
#include "w32peer.h"

class VDUIControlLabelW32 : public VDUIControlBaseW32 {
	int mMaxLen;
public:
	VDUIControlLabelW32(int maxlen);
	~VDUIControlLabelW32();

	bool Create(IVDUIControl *pControl);
	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);
};

#endif
