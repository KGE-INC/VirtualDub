#ifndef f_VD2_DITA_W32LIST_H
#define f_VD2_DITA_W32LIST_H

#include <list>
#include "control.h"
#include "ctl_set.h"
#include "w32peer.h"

class VDUIControlListBaseW32 : public VDUIControlBaseW32, public IVDUIList, public IVDUIControlNativeCallbackW32 {
public:
	IVDUIList *AsUIList();
	IVDUIControlNativeCallback *AsNativeCallback();

	virtual void AddColumn(const wchar_t *pText, int width_units);
	virtual void UpdateItem(int item);
	virtual void SetSource(IVDUIListCallback *);
	virtual void Sort();

	virtual void Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw();
	virtual void Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw();
	virtual void Dispatch_WM_HVSCROLL(IVDUICallback *, UINT code) throw();
};

class VDUIControlListboxW32 : public VDUIControlListBaseW32 {
private:
	int mnItems, minlen, minrows;

public:
	VDUIControlListboxW32(int minlines = 2);
	~VDUIControlListboxW32();

	bool Create(IVDUIControl *pControl);

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);

	nsVDUI::eLinkMethod GetLinkMethod();

	int GetStatei();
	void SetStatei(int i);

	int GetItemCount();
	int AddItem(const wchar_t *pEntry, bool bIncludeInAutoSize, void *pCookie, int nInsertBefore);
	void DeleteItem(int item);
	void DeleteAllItems();
	void *GetItemCookie(int item);
	void SetItemCookie(int item, void *pCookie);
	void SetItemText(int item, const wchar_t *pText);

	void Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw();
};

class VDUIControlComboboxW32 : public VDUIControlListBaseW32 {
private:
	int mnItems, minlen, minrows;

public:
	VDUIControlComboboxW32(int minlines = 2);
	~VDUIControlComboboxW32();

	bool Create(IVDUIControl *pControl);

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);
	void PostLayoutBase(const VDUIRect& target) throw();

	nsVDUI::eLinkMethod GetLinkMethod();

	int GetStatei();
	void SetStatei(int i);

	int GetItemCount();
	int AddItem(const wchar_t *pEntry, bool bIncludeInAutoSize, void *pCookie, int nInsertBefore);
	void DeleteItem(int item);
	void DeleteAllItems();
	void *GetItemCookie(int item);
	void SetItemCookie(int item, void *pCookie);
	void SetItemText(int item, const wchar_t *pText);

	void Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw();
};

class VDUIControlListViewW32 : public VDUIControlListBaseW32 {
private:
	int mnItems, mnColumns, minlen, minrows;
	int mnItemHeight, mnHeaderHeight;
	int mnSelected;
	int mnExtensibleColumn;
	bool mbUnicode;

	IVDUIListCallback *mpListCallback;

	struct {
		union {
			char *psza;
			wchar_t *pszw;
		};

		int capacity;
	} mBuffer[2];
	int mNextBuffer;

public:
	VDUIControlListViewW32(int minlines = 2);
	~VDUIControlListViewW32();

	bool Create(IVDUIControl *pControl);

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);
	void PostLayoutBase(const VDUIRect& target);

	nsVDUI::eLinkMethod GetLinkMethod();

	int GetStatei();
	void SetStatei(int i);

	int GetItemCount();
	int AddItem(const wchar_t *pEntry, bool bIncludeInAutoSize, void *pCookie, int nInsertBefore);
	void DeleteItem(int item);
	void DeleteAllItems();
	void *GetItemCookie(int item);
	void SetItemCookie(int item, void *pCookie);
	void SetItemText(int item, const wchar_t *pText);

	void AddColumn(const wchar_t *pText, int width_units);
	void UpdateItem(int item);
	void SetSource(IVDUIListCallback *);

	static int CALLBACK StaticSorter(LPARAM, LPARAM, LPARAM);

	void Sort();

	void Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw();
};

#endif
