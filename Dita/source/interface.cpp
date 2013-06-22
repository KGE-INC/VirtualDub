#pragma warning(disable: 4786)

#include <list>
#include <map>

#include <windows.h>
#include <commctrl.h>

#include <vd2/system/VDRefCount.h>
#include <vd2/system/VDAtomic.h>
#include <vd2/system/text.h>
#include <vd2/Dita/interface.h>
#include <vd2/Dita/services.h>
#include <vd2/Dita/resources.h>

#include "ctl_set.h"
#include "ctl_grid.h"
#include "ctl_file.h"

#include "w32peer.h"
#include "w32dialog.h"
#include "w32button.h"
#include "w32group.h"
#include "w32list.h"
#include "w32label.h"
#include "w32edit.h"
#include "w32trackbar.h"

extern void VDExecuteDialogResource(const unsigned char *pBytecode, IVDUIBase *pBase, IVDUIConstructor *pConstructor);

///////////////////////////////////////////////////////////////////////////

class VDUIContext : public IVDUIContext {
private:
	VDAtomicInt mRefCount;

public:
	VDUIContext();

	int AddRef() { return mRefCount.inc(); }
	int Release() { int rv = mRefCount.dec(); if (!rv) delete this; return rv; }

	int DoModalDialog(VDGUIHandle handle, IVDUICallback *pCB) {
		VDUIControlDialogW32 dialog(false);
		
		dialog.SetCallback(pCB);
		return dialog.BeginModal(handle, this);
	}

	IVDUIControl *CreateBase()							{ return new VDUIControlDialogW32(false); }
	IVDUIControl *CreateModelessDialog()				{ return new VDUIControlDialogW32(false); }
	IVDUIControl *CreateChildDialog()					{ return new VDUIControlDialogW32(true); }
	IVDUIControl *CreateLabel(int maxlen)				{ return new VDUIControlLabelW32(maxlen); }
	IVDUIControl *CreateEdit(int maxlen)				{ return new VDUIControlEditW32(maxlen); }
	IVDUIControl *CreateEditInt(int minv, int maxv)		{ return new VDUIControlEditIntW32(minv, maxv); }
	IVDUIControl *CreateButton()						{ return new VDUIControlButtonW32; }
	IVDUIControl *CreateCheckbox()						{ return new VDUIControlCheckboxW32; }
	IVDUIControl *CreateOption(IVDUIControl	*pParent)	{ return new VDUIControlOptionW32(pParent); }
	IVDUIControl *CreateListbox(int rows)				{ return new VDUIControlListboxW32(rows); }
	IVDUIControl *CreateCombobox(int rows)				{ return new VDUIControlComboboxW32(rows); }
	IVDUIControl *CreateListView(int rows)				{ return new VDUIControlListViewW32(rows); }
	IVDUIControl *CreateTrackbar(int minv, int maxv)	{ return new VDUIControlTrackbarW32(minv, maxv); }
	IVDUIControl *CreateFileControl(int maxlen)			{ return new VDUIControlFile(maxlen); }
	IVDUIControl *CreateGroup()							{ return new VDUIControlGroupW32; }
	IVDUIControl *CreateHorizontalSet()					{ return new VDUIControlHorizontalSet; }
	IVDUIControl *CreateVerticalSet()					{ return new VDUIControlVerticalSet; }
	IVDUIControl *CreateGrid(int w, int h)				{ return new VDUIGrid(w, h); }

	IVDUIConstructor *CreateConstructor(IVDUIControl *pBase) {
		IVDUIConstructor *pConstructor = VDCreateUIConstructor();

		if (pConstructor) {
			pConstructor->Init(this, pBase);
		}

		return pConstructor;
	}

	virtual bool ExecuteDialogResource(IVDUIBase *pBase, int moduleID, int dialogID) {
		IVDUIConstructor *pcon = VDGetUIContext()->CreateConstructor(pBase->AsControl());

		if (pcon) {
			const unsigned char *data = VDLoadDialog(moduleID, dialogID);

			if (data) {
				VDExecuteDialogResource(data, pBase, pcon);
				pBase->ProcessAllLinks();
				pcon->Release();
				return true;
			}
		}

		pcon->Release();

		return false;
	}

	virtual bool ExecuteTemplateResource(IVDUIBase *pBase, int moduleID, int dialogID) {
		IVDUIConstructor *pcon = VDGetUIContext()->CreateConstructor(pBase->AsControl());

		if (pcon) {
			const unsigned char *data = VDLoadTemplate(moduleID, dialogID);

			if (data) {
				VDExecuteDialogResource(data, pBase, pcon);
				pBase->ProcessAllLinks();
				pcon->Release();
				return true;
			}
		}

		pcon->Release();

		return false;
	}
};

///////////////////////////////////////////////////////////////////////////

IVDUIContext *VDCreateUIContext() {
	return new VDUIContext;
}

VDUIContext::VDUIContext() : mRefCount(0) {
}

///////////////////////////////////////////////////////////////////////////

VDAppDialogBaseImpl::VDAppDialogBaseImpl(int id, bool bTemplate)
	: mDialogID(id), mbTemplate(bTemplate)
{
}

VDAppDialogBaseImpl::~VDAppDialogBaseImpl() {
}

bool VDAppDialogBaseImpl::UIConstructModal(IVDUIContext *pContext, IVDUIBase *pBase) {
	mpBase = pBase;
	pBase->AsControl()->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);
	if ((pContext->*(mbTemplate ? IVDUIContext::ExecuteTemplateResource : IVDUIContext::ExecuteDialogResource))(pBase, 0, mDialogID)) {
		Init();
		pBase->ProcessAllLinks();
		return true;
	}
	return false;
}

void VDAppDialogBaseImpl::UIEvent(IVDUIControl *pBase, unsigned id, IVDUICallback::eEventType ev, int item) {
	if (ev == kEventClose) {
		if (!item || Writeback())
			pBase->AsBase()->EndModal(item);
	}

	Event(pBase, id, ev, item);
}

void VDAppDialogBaseImpl::Init() {}
void VDAppDialogBaseImpl::Event(IVDUIControl *pBase, unsigned id, IVDUICallback::eEventType ev, int item) {}
void VDAppDialogBaseImpl::Cancel() {}
bool VDAppDialogBaseImpl::Writeback() { return true; }

bool VDAppDialogBaseImpl::GetB(unsigned id) const { return mpBase->GetControl(id)->GetStateb(); }
int VDAppDialogBaseImpl::GetI(unsigned id) const { return mpBase->GetControl(id)->GetStatei(); }
VDStringW VDAppDialogBaseImpl::GetW(unsigned id) const {
	IVDUIControl *pControl = mpBase->GetControl(id);
	VDStringW s;
	int l = pControl->GetTextLengthw();

	pControl->GetTextw(s.alloc(l), l+1);

	return s;
}
void VDAppDialogBaseImpl::SetB(unsigned id, bool b) const { mpBase->GetControl(id)->SetStateb(b); }
void VDAppDialogBaseImpl::SetI(unsigned id, int v) const { mpBase->GetControl(id)->SetStatei(v); }
void VDAppDialogBaseImpl::SetW(unsigned id, const wchar_t *s) const { mpBase->GetControl(id)->SetTextw(s); }
void VDAppDialogBaseImpl::SetW(unsigned id, unsigned strid) const { mpBase->GetControl(id)->SetTextw(VDLoadString(0, mDialogID, strid)); }
void VDAppDialogBaseImpl::SetWF(unsigned id, unsigned strid, int args, ...) const {
	va_list val;

	va_start(val, args);
	SetW(id, VDvswprintf(VDLoadString(0, mDialogID, strid), args, val).c_str());
	va_end(val);
}
void VDAppDialogBaseImpl::Enable(unsigned id, bool b) const {
	mpBase->GetControl(id)->Enable(b);
}

///////////////////////////////////////////////////////////////////////////

#if 0
#include <vd2/system/VDString.h>
#include <deque>

struct Ent {
	VDStringW path;
	int pri;
	int lim;

	Ent() : pri(0), lim(0) {}
};

class MyDialog : private IVDUICallback {
public:
	Ent& entry;

	MyDialog(Ent& e) : entry(e) {}

	void Go(VDGUIHandle parent) {
		vdrefptr<IVDUIContext> pctx(CreateVDUIContext());

		pctx->DoModalDialog(parent, this);		
	}

	bool UIConstructModal(IVDUIContext *pctx, IVDUIBase *pBase) {
		IVDUIConstructor *pcon = pctx->CreateConstructor(pBase->AsControl());

		if (pcon) {
			pBase->AsControl()->SetTextw(L"Edit storage entry");
			pBase->AsControl()->SetDesiredAspectRatio(1.4f);
			pBase->AsControl()->SetAlignment(nsVDUI::kFill, nsVDUI::kCenter);

			pcon->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);
			pcon->BeginGrid(0, 2, 3);
				pcon->SetAlignment(nsVDUI::kFill, nsVDUI::kCenter);
				pcon->SetGridColumnAffinity(0, 0);
				pcon->AddLabel(0, L"Path");
				pcon->SetMinimumSize(200,0);
				pcon->AddFileControl(200, entry.path.c_str(), -1);
				pcon->SetMinimumSize(0,0);

				pcon->SetAlignment(nsVDUI::kLeft, nsVDUI::kCenter);
				pcon->AddLabel(0, L"Priority");
				pcon->SetMinimumSize(50, 0);
				pcon->AddEditInt(201, entry.pri, -128, 127);
				pcon->SetMinimumSize(0, 0);
				pcon->AddLabel(0, L"Minimum disk space");
				pcon->SetMinimumSize(50, 0);
				pcon->AddEditInt(202, entry.lim, 0, 0xFFFFFF);
			pcon->EndGrid();

			pcon->SetAlignment(nsVDUI::kRight, nsVDUI::kBottom);
			pcon->BeginHorizSet(0);
				pcon->SetMinimumSize(50, 14);
				pcon->AddButton(100, L"OK");
				pcon->AddButton(101, L"Cancel");
			pcon->EndSet();

			if (pcon->GetErrorState()) {
				VDASSERT(false);
				return false;
			}

			pBase->Link(100, nsVDUI::kLinkEndDialog, 100);
			pBase->Link(101, nsVDUI::kLinkEndDialog, 101);
		}
		return true;
	}

	void UIEvent(IVDUIControl *pControl, unsigned id, eEventType type, int item) {
		if (type == kEventClose) {
			if (item) {
				IVDUIBase *pBase = pControl->AsBase();
				IVDUIControl *pcPath = pBase->GetControl(200);
				IVDUIControl *pcPri = pBase->GetControl(201);
				IVDUIControl *pcLim = pBase->GetControl(202);

				int len = pcPath->GetTextLengthw();
				wchar_t *pszw = entry.path.alloc(len);

				if (pszw)
					pcPath->GetTextw(pszw, len+1);

				entry.pri = pcPri->GetStatei();
				entry.lim = pcLim->GetStatei();
			}
		}
	}

};

class MyIFace : public IVDUICallback, private IVDUIListCallback {
public:
	typedef std::list<Ent> tEntryList;
	
	tEntryList mEntList;

	bool UIConstructModal(IVDUIContext *pctx, IVDUIBase *pBase) {
#if 1
		IVDUIConstructor *pcon = pctx->CreateConstructor(pBase->AsControl());

		pBase->AsControl()->SetTextw(L"Configure storage");
		pBase->AsControl()->SetDesiredAspectRatio(1.4f);
		pBase->AsControl()->SetAlignment((nsVDUI::eAlign)(nsVDUI::kFill | nsVDUI::kExpandFlag), nsVDUI::kFill);

		if (pcon) {

			pcon->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);

			pcon->AddListView(100, 8);

			pcon->SetAlignment(nsVDUI::kFill, nsVDUI::kTop);

			pcon->BeginHorizSet(0);
				pcon->SetAlignment(0, nsVDUI::kCenter);

				pcon->BeginHorizSet(0);
					pcon->AddButton(200, L"Add");
					pcon->AddButton(201, L"Edit");
					pcon->AddButton(202, L"Remove");
				pcon->EndSet();

				pcon->AddCheckbox(0, L"Redirect first file");
			pcon->EndSet();


			pcon->BeginGroupSet(0, L"File size control");
				pcon->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);
				pcon->BeginGrid(0,2,2);
					pcon->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);
					pcon->SetGridColumnAffinity(0, 0);
					pcon->SetGridColumnAffinity(1, 1);
					pcon->AddLabel(0, L"Minimum file size (MB) ");
					pcon->AddEditInt(0, 50, 50, 2048);
					pcon->AddLabel(0, L"Maximum file size (MB) ");
					pcon->AddEditInt(0, 2048, 50, 2048);
				pcon->EndGrid();
			pcon->EndSet();

			pcon->SetAlignment(nsVDUI::kRight, 0);

			pcon->BeginHorizSet(0);
				pcon->SetMinimumSize(50, 14);
				pcon->AddButton(300, L"OK");
				pcon->AddButton(301, L"Cancel");
			pcon->EndSet();

			if (pcon->GetErrorState()) {
				VDASSERT(false);
				return false;
			}

			IVDUIList *plv = pBase->GetControl(100)->AsUIList();

			plv->AddColumn(L"Path", -1);
			plv->AddColumn(L"Pri", 40);
			plv->AddColumn(L"Limit", 40);
			plv->SetSource(this);

			pBase->Link(201, nsVDUI::kLinkIntSelectedEnable, 100);
			pBase->Link(202, nsVDUI::kLinkIntSelectedEnable, 100);
			pBase->Link(300, nsVDUI::kLinkEndDialog, 300);
			pBase->Link(301, nsVDUI::kLinkEndDialogFalse, 301);

			pBase->ProcessAllLinks();
		}

		return true;
#elif 1
		IVDUIControl *pControl, *pControl0, *pSet, *pSet2;

		pBase->AsControl()->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);

		pSet = pctx->CreateHorizontalSet();

		pBase->Add(pSet);
		pBase->SetLayoutControl(pSet);

		pSet2 = pctx->CreateVerticalSet();
		pSet->AsUISet()->Add(pSet2);
		
		pControl = pctx->CreateCombobox(4);
		pControl->SetID(1000);
		pSet2->AsUISet()->Add(pControl);
		pControl->SetAlignment(nsVDUI::kFill, nsVDUI::kTop);
		pControl->AsUIList()->AddItem(L"\x3046\x3055\x304E");
		pControl->AsUIList()->AddItem(L"\x3042\x307F");
		pControl->AsUIList()->AddItem(L"\x308C\x3044");
		pControl->AsUIList()->AddItem(L"\x307E\x3053\x3068");
		pControl->AsUIList()->AddItem(L"\x307F\x306A\x3053");
		pControl->AsUIList()->AddItem(L"\x307B\x305F\x308B");
		pControl->AsUIList()->AddItem(L"\x307F\x3061\x308B");
		pControl->AsUIList()->AddItem(L"\x306F\x308B\x304B");
		pControl->AsUIList()->AddItem(L"\x305B\x3064\x306A");

//		pControl = pctx->CreateListbox(4);
		pControl = pctx->CreateListView(9);
		pControl->SetID(1001);
		pSet2->AsUISet()->Add(pControl);
		pControl->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);
		pControl->AsUIList()->AddColumn(L"Name", -1);
		pControl->AsUIList()->AddItem(L"\x3046\x3055\x304E");
		pControl->AsUIList()->AddItem(L"\x3042\x307F");
		pControl->AsUIList()->AddItem(L"\x308C\x3044");
		pControl->AsUIList()->AddItem(L"\x307E\x3053\x3068");
		pControl->AsUIList()->AddItem(L"\x307F\x306A\x3053");
		pControl->AsUIList()->AddItem(L"\x307B\x305F\x308B");
		pControl->AsUIList()->AddItem(L"\x307F\x3061\x308B");
		pControl->AsUIList()->AddItem(L"\x306F\x308B\x304B");
		pControl->AsUIList()->AddItem(L"\x305B\x3064\x306A");

		pControl = pctx->CreateGroup();
		pSet->AsUISet()->Add(pControl);
		pControl->SetTextw(L"Buttons");
		pSet2 = pControl;

		pControl = pctx->CreateVerticalSet();
		pSet2->AsUISet()->Add(pControl);
		pSet2 = pControl;

		pControl0 = pControl = pctx->CreateOption(NULL);
		pControl->SetID(1002);
		pSet2->AsUISet()->Add(pControl);
		pControl->SetTextw(L"Option 1");

		pControl = pctx->CreateOption(pControl0);
		pSet2->AsUISet()->Add(pControl);
		pControl->SetTextw(L"Option 2");

		pControl = pctx->CreateOption(pControl0);
		pSet2->AsUISet()->Add(pControl);
		pControl->SetTextw(L"Option 3");

		pControl = pctx->CreateOption(pControl0);
		pSet2->AsUISet()->Add(pControl);
		pControl->SetTextw(L"Option 4");

		pControl = pctx->CreateOption(pControl0);
		pSet2->AsUISet()->Add(pControl);
		pControl->SetTextw(L"Option 5");

		pControl = pctx->CreateOption(pControl0);
		pSet2->AsUISet()->Add(pControl);
		pControl->SetTextw(L"Option 6");


		pControl = pctx->CreateEditInt(0, 10);
		pControl->SetID(1003);
		pControl->SetMinimumSize(VDUISize(30,0));
		pSet2->AsUISet()->Add(pControl);


		pControl = pctx->CreateButton();
		pControl->SetID(2000);
		pSet2->AsUISet()->Add(pControl);
		pControl->SetTextw(L"Reset");

		pBase->Link(1000, nsVDUI::kLinkInt, 1001);
		pBase->Link(1000, nsVDUI::kLinkInt, 1002);
		pBase->Link(1001, nsVDUI::kLinkInt, 1000);
		pBase->Link(1001, nsVDUI::kLinkInt, 1002);
		pBase->Link(1002, nsVDUI::kLinkInt, 1000);
		pBase->Link(1002, nsVDUI::kLinkInt, 1001);
		pBase->Link(1003, nsVDUI::kLinkInt, 1000);
		pBase->Link(1003, nsVDUI::kLinkInt, 1001);

		pBase->Link(1000, nsVDUI::kLinkBoolToInt, 2000);
		pBase->Link(1001, nsVDUI::kLinkBoolToInt, 2000);
		pBase->Link(1002, nsVDUI::kLinkBoolToInt, 2000);
#endif
	}

	void go() {
		vdrefptr<IVDUIContext> pctx(CreateVDUIContext());

		pctx->DoModalDialog(NULL, this);
	}
	
	void UIEvent(IVDUIControl *pControl, unsigned id, eEventType type, int item) {
		switch(id) {
		case 100:
			if (type == kEventDoubleClick) {
				int iSel = pControl->GetStatei();

				if (iSel>=0) {
					Ent *pEnt = (Ent *)pControl->AsUIList()->GetItemCookie(iSel);

					MyDialog(*pEnt).Go(pControl->GetBase()->AsControl()->GetRawHandle());
					pControl->AsUIList()->UpdateItem(iSel);
					pControl->AsUIList()->Sort();
				}
			}
			break;
		case 200:
			if (type == kEventSelect) {
				IVDUIList *pList = pControl->GetBase()->GetControl(100)->AsUIList();
				mEntList.push_back(Ent());

				if (pList->AddItem(NULL, true, &mEntList.back()) < 0)
					mEntList.pop_back();
				else {
					MyDialog(mEntList.back()).Go(pControl->GetBase()->AsControl()->GetRawHandle());
					pList->Sort();
				}
			}
			break;
		case 201:
			if (type == kEventSelect) {
				IVDUIBase *pBase = pControl->GetBase();
				IVDUIControl *pList = pBase->GetControl(100);

				int iSel = pList->GetStatei();

				if (iSel >= 0) {
					Ent *pEnt = (Ent *)pList->AsUIList()->GetItemCookie(iSel);

					MyDialog(*pEnt).Go(pControl->GetBase()->AsControl()->GetRawHandle());
					pList->AsUIList()->UpdateItem(iSel);
					pList->AsUIList()->Sort();
				}
			}
			break;
		}
	}

	wchar_t buf[32];

	const wchar_t *UIListTextRequest(IVDUIControl *pList, int item, int subitem, bool& bPersistent) {
		const Ent& entry = *(Ent *)pList->AsUIList()->GetItemCookie(item);

		switch(subitem) {
		case 0:
			return entry.path.c_str();
		case 1:
			_snwprintf(buf, 32, L"%+d", entry.pri);
			return buf;
		case 2:
			_snwprintf(buf, 32, L"%d MB", entry.lim);
			return buf;
		default:
			VDASSERT(false);
			return L"";
		}
	}

	int UIListSortRequest(IVDUIControl *pList, void *p1, void *p2) {
		const Ent& e1 = *(Ent *)p1;
		const Ent& e2 = *(Ent *)p2;

		return e1.pri != e2.pri ? e2.pri - e1.pri : e1.path.compare(e2.path);
	}
};
#else

#include <vd2/Dita/resources.h>

class MyIFace : public IVDUICallback {
public:

	bool UIConstructModal(IVDUIContext *pctx, IVDUIBase *pBase) {
		IVDUIConstructor *pcon = pctx->CreateConstructor(pBase->AsControl());

		pBase->AsControl()->SetTextw(L"Configure storage");
		pBase->AsControl()->SetDesiredAspectRatio(1.4f);
		pBase->AsControl()->SetAlignment((nsVDUI::eAlign)(nsVDUI::kFill), nsVDUI::kFill);

		if (pcon) {
			VDExecuteDialogResource(VDLoadDialog(0, 100), pBase, pcon);

			pBase->ProcessAllLinks();
		}

		return true;
	}

	void go() {
		vdrefptr<IVDUIContext> pctx(VDCreateUIContext());

		pctx->DoModalDialog(NULL, this);
	}
	
	void UIEvent(IVDUIControl *pControl, unsigned id, eEventType type, int item) {
	}
};

#endif

void VDInterfaceTest() {
	MyIFace().go();
}
