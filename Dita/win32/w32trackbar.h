#ifndef f_VD2_DITA_W32TRACKBAR_H
#define f_VD2_DITA_W32TRACKBAR_H

#include "control.h"
#include "w32peer.h"

class VDUIControlTrackbarW32 : public VDUIControlBaseW32, public IVDUIControlNativeCallbackW32, public IVDUISlider {
private:
	int mnItems, minval, maxval;

public:
	VDUIControlTrackbarW32(int minv, int maxv);
	~VDUIControlTrackbarW32();

	bool Create(IVDUIControl *pControl);
	IVDUIControlNativeCallback *AsNativeCallback();
	IVDUISlider *AsUISlider();

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);

	nsVDUI::eLinkMethod GetLinkMethod();

	int GetStatei();
	void SetStatei(int i);

	void SetRange(int begin, int end);

	void Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw();
	void Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw();
	void Dispatch_WM_HVSCROLL(IVDUICallback *, UINT code) throw();
};

#endif
