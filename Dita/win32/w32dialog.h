#ifndef f_VD2_DITA_W32DIALOG_H
#define f_VD2_DITA_W32DIALOG_H

#include <windows.h>

#include <map>

#include "control.h"
#include "w32peer.h"

class VDUIControlDialogW32 : public VDUIControlBaseW32, public IVDUIBase {
private:
	typedef std::map<IVDUIControl *, nsVDUI::eLinkType> tLinkList;
	typedef std::map<IVDUIControl *, tLinkList> tLinkMap;
	typedef std::multimap<unsigned, IVDUIControl *> tControlMap;

	tLinkMap		mLinkMap;
	tControlMap		mControlMap;
	IVDUIControl	*mpLayoutControl;
	RECT			mrcInsets;
	IVDUICallback	*mpCB;
	UINT			mNextID;
	int				mRetVal;
	int 			mInMessage;
	bool			mbDeferredDestroy;
	bool			mbChild;

	void _ProcessLinksb(IVDUIControl *pControl, tLinkList& linklist, bool bProcessStaticsOnly);
	void _ProcessLinksi(IVDUIControl *pControl, tLinkList& linklist, bool bProcessStaticsOnly);
	void _ProcessLinks(IVDUIControl *pControl, tLinkList& linklist, bool bProcessStaticsOnly, bool bProcessOneShots);

	static BOOL CALLBACK StaticDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

public:
	VDUIControlDialogW32(bool bChild);
	~VDUIControlDialogW32();

	IVDUIControl *AsControl();
	IVDUIBase *AsBase();

	bool Create(IVDUIControl *);
	void Destroy();

	void SetCallback(IVDUICallback *pCB);

	int BeginModal(VDGUIHandle handle, IVDUIContext *);
	void EndModal(int rv);

	void MapUnitsToPixels(VDUIRect& r);
	void MapScreenToClient(VDUIPoint& pt);
	void MapScreenToClient(VDUIRect& pt);
	void MapClientToScreen(VDUIPoint& pt);
	void MapClientToScreen(VDUIRect& pt);

	IVDUIControl *GetControl(unsigned);
	bool Add(IVDUIControl *);
	void AddNonlocal(IVDUIControl *);
	void Remove(IVDUIControl *);
	void SetLayoutControl(IVDUIControl *);
	void Relayout();

	unsigned CreateUniqueID();

	void ProcessLinksID(unsigned from_id);
	void ProcessLinksPtr(IVDUIControl *);
	void ProcessLinksToStatics();
	void ProcessAllLinks();

	void Link(unsigned dst_id, nsVDUI::eLinkType type, unsigned src_id);
	void Link(IVDUIControl *pControlB, nsVDUI::eLinkType type, IVDUIControl *pControlA);
	void Unlink(unsigned dst_id, nsVDUI::eLinkType type, unsigned src_id);
	void Unlink(unsigned dst_id);
	void Unlink(IVDUIControl *);

	// VDUIControlDialogW32

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);
	void PostLayoutBase(const VDUIRect& target);
};

#endif
