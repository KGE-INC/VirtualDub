#ifndef f_VD2_DITA_W32BUTTON_H
#define f_VD2_DITA_W32BUTTON_H

#include <windows.h>

#include "control.h"
#include "w32peer.h"

class VDUIControlButtonW32 : public VDUIControlBaseW32, private IVDUIControlNativeCallbackW32 {
public:
	VDUIControlButtonW32();
	~VDUIControlButtonW32();

	bool Create(IVDUIControl *pControl);
	IVDUIControlNativeCallback *AsNativeCallback();
	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);

	nsVDUI::eLinkMethod GetLinkMethod();

	bool GetStateb();

	void Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw();
	void Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw();
	void Dispatch_WM_HVSCROLL(IVDUICallback *, UINT code) throw();
};

class VDUIControlCheckboxW32 : public VDUIControlBaseW32, private IVDUIControlNativeCallbackW32 {
public:
	VDUIControlCheckboxW32();
	~VDUIControlCheckboxW32();

	bool Create(IVDUIControl *pControl);
	IVDUIControlNativeCallback *AsNativeCallback();
	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);

	nsVDUI::eLinkMethod GetLinkMethod();
	nsVDUI::eCompressType GetCompressType();

	bool GetStateb();
	void SetStateb(bool b);

	void Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw();
	void Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw();
	void Dispatch_WM_HVSCROLL(IVDUICallback *, UINT code) throw();
};

class VDUIControlOptionW32 : public VDUIControlBaseW32, private IVDUIControlNativeCallbackW32 {
	VDUIControlOptionW32	*mpParentOption;
	int						mnItems;
	int						mnSelected;
public:
	VDUIControlOptionW32(IVDUIControl *pParentOption);
	~VDUIControlOptionW32();

	bool Create(IVDUIControl *pControl);

	IVDUIControlNativeCallback *AsNativeCallback();

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);

	nsVDUI::eLinkMethod GetLinkMethod();

	nsVDUI::eCompressType GetCompressType();

	int GetStatei();
	void SetStatei(int i);

	void Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw();
	void Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw();
	void Dispatch_WM_HVSCROLL(IVDUICallback *, UINT code) throw();
};

#endif
