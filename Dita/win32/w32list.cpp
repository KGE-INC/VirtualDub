#include <windows.h>
#include <commctrl.h>

#include <vd2/system/text.h>

#include "w32list.h"

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlListBaseW32
//
///////////////////////////////////////////////////////////////////////////

IVDUIList *VDUIControlListBaseW32::AsUIList() {
	return this;
}

IVDUIControlNativeCallback *VDUIControlListBaseW32::AsNativeCallback() {
	return this;
}

void VDUIControlListBaseW32::AddColumn(const wchar_t *pText, int width_units) {
}

void VDUIControlListBaseW32::UpdateItem(int item) {
}

void VDUIControlListBaseW32::SetSource(IVDUIListCallback *) {
}

void VDUIControlListBaseW32::Sort() {
}

void VDUIControlListBaseW32::Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw() {
}

void VDUIControlListBaseW32::Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw() {
}

void VDUIControlListBaseW32::Dispatch_WM_HVSCROLL(IVDUICallback *, UINT code) throw() {
}

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlListboxW32
//
///////////////////////////////////////////////////////////////////////////

VDUIControlListboxW32::VDUIControlListboxW32(int minlines)
	: minlen(0)
	, minrows(minlines)
	, mnItems(0)
{
}

VDUIControlListboxW32::~VDUIControlListboxW32() {
}

bool VDUIControlListboxW32::Create(IVDUIControl *pControl) {
	return VDUIControlBase::Create(pControl)
		&& _Create(0, "LISTBOX", L"LISTBOX", WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT, WS_EX_CLIENTEDGE);
}

void VDUIControlListboxW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	RECT rcMin;

	rcMin.left = rcMin.top = 0;
	rcMin.right = minlen + 4*GetSystemMetrics(SM_CXEDGE) + GetSystemMetrics(SM_CXVSCROLL);
	rcMin.bottom = 2*GetSystemMetrics(SM_CYEDGE);

	if (mhwnd) {
		rcMin.bottom += SendMessage(mhwnd, LB_GETITEMHEIGHT, 0, 0) * minrows;
		AdjustWindowRectEx(&rcMin, GetWindowLong(mhwnd, GWL_STYLE), FALSE, GetWindowLong(mhwnd, GWL_EXSTYLE));
	}

	mLayoutSpecs.minsize.w = rcMin.left + rcMin.right;
	mLayoutSpecs.minsize.h = rcMin.top + rcMin.bottom;
}

nsVDUI::eLinkMethod VDUIControlListboxW32::GetLinkMethod() {
	return nsVDUI::kLinkMethodInt;
}

int VDUIControlListboxW32::GetStatei() {
	if (!mhwnd)
		return 0;

	return SendMessage(mhwnd, LB_GETCURSEL, 0, 0);
}

void VDUIControlListboxW32::SetStatei(int i) {
	if (!mhwnd)
		return;

	if (i >= mnItems)
		i = mnItems - 1;

	if (i < 0)
		i = 0;

	SendMessage(mhwnd, LB_SETCURSEL, i, 0);
}

int VDUIControlListboxW32::GetItemCount() {
	return mnItems;
}

int VDUIControlListboxW32::AddItem(const wchar_t *text, bool bIncludeInAutoSize, void *pCookie, int nInsertBefore) {
	if (mhwnd) {
		SIZE siz={0,0};
		HDC hdc;
		int idx;

		if (nsVDUI::isWindows9x()) {
			const char *str = VDFastTextWToA(text);

			if (bIncludeInAutoSize)
				if (hdc = GetDC(mhwnd)) {
					HGDIOBJ hgoOldFont;

					hgoOldFont = SelectObject(hdc, (HGDIOBJ)SendMessage(mhwnd, WM_GETFONT, 0, 0));

					GetTextExtentPoint32A(hdc, str, strlen(str), &siz);

					SelectObject(hdc, hgoOldFont);

					ReleaseDC(mhwnd, hdc);
				}

			if (nInsertBefore < 0)
				idx = SendMessageA(mhwnd, LB_ADDSTRING, 0, (LPARAM)str);
			else
				idx = SendMessageA(mhwnd, LB_INSERTSTRING, nInsertBefore, (LPARAM)str);

			VDFastTextFree();
		} else {
			if (bIncludeInAutoSize)
				if (hdc = GetDC(mhwnd)) {
					HGDIOBJ hgoOldFont;

					hgoOldFont = SelectObject(hdc, (HGDIOBJ)SendMessage(mhwnd, WM_GETFONT, 0, 0));

					GetTextExtentPoint32W(hdc, text, wcslen(text), &siz);

					SelectObject(hdc, hgoOldFont);

					ReleaseDC(mhwnd, hdc);
				}

			if (nInsertBefore < 0)
				idx = SendMessageW(mhwnd, LB_ADDSTRING, 0, (LPARAM)text);
			else
				idx = SendMessageW(mhwnd, LB_INSERTSTRING, nInsertBefore, (LPARAM)text);

		}

		if (idx >= 0) {
			if (bIncludeInAutoSize)
				if (siz.cx > minlen)
					minlen = siz.cx;

			SendMessage(mhwnd, LB_SETITEMDATA, idx, (LPARAM)pCookie);
			++mnItems;

			return idx;
		}
	}

	return -1;
}

void VDUIControlListboxW32::DeleteItem(int item) {
	if (mhwnd && item >= 0 && item < mnItems) {
		SendMessage(mhwnd, LB_DELETESTRING, item, 0);
		--mnItems;
	}
}

void VDUIControlListboxW32::DeleteAllItems() {
	if (mhwnd)
		SendMessage(mhwnd, LB_RESETCONTENT, 0, 0);

	mnItems = 0;
	minlen = 0;
}

void *VDUIControlListboxW32::GetItemCookie(int item) {
	if (mhwnd && item >= 0 && item < mnItems)
		return (void *)SendMessage(mhwnd, LB_GETITEMDATA, item, 0);

	return NULL;
}

void VDUIControlListboxW32::SetItemCookie(int item, void *pCookie) {
	if (mhwnd && item >= 0 && item < mnItems)
		SendMessage(mhwnd, CB_SETITEMDATA, item, (DWORD)pCookie);
}

void VDUIControlListboxW32::SetItemText(int item, const wchar_t *pText) {
	if (mhwnd && item >= 0 && item < mnItems) {
		int idx;
		int nSelected = GetStatei();

		if (nsVDUI::isWindows9x()) {
			idx = SendMessageA(mhwnd, LB_INSERTSTRING, item, (LPARAM)VDFastTextWToA(pText));

			VDFastTextFree();
		} else
			idx = SendMessageW(mhwnd, LB_INSERTSTRING, item, (LPARAM)pText);

		if (idx >= 0) {
			SendMessageA(mhwnd, LB_SETITEMDATA, idx, SendMessageA(mhwnd, LB_GETITEMDATA, idx+1, 0));
			SendMessageA(mhwnd, LB_DELETESTRING, idx+1, 0);

			if (item == nSelected)
				SendMessage(mhwnd, LB_SETCURSEL, item, 0);
		}
	}
}

void VDUIControlListboxW32::Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw() {
	if (code == LBN_SELCHANGE) {
		ProcessLinks();
		pCB->UIEvent(this, mID, IVDUICallback::kEventSelect, GetStatei());
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlComboboxW32
//
///////////////////////////////////////////////////////////////////////////

VDUIControlComboboxW32::VDUIControlComboboxW32(int minlines)
	: minlen(0)
	, minrows(minlines)
	, mnItems(0)
{
}

VDUIControlComboboxW32::~VDUIControlComboboxW32() {
}

bool VDUIControlComboboxW32::Create(IVDUIControl *pControl) {
	if (VDUIControlBase::Create(pControl)
		&& _Create(0, "COMBOBOX", L"COMBOBOX", CBS_DROPDOWNLIST|WS_VSCROLL)) {

		SendMessage(mhwnd, CB_SETEXTENDEDUI, TRUE, 0);
		return true;
	}

	return false;
}

void VDUIControlComboboxW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	mLayoutSpecs.minsize.w = minlen + 4*GetSystemMetrics(SM_CXEDGE) + GetSystemMetrics(SM_CXVSCROLL);
	mLayoutSpecs.minsize.h = 0;

	if (mhwnd) {
		RECT rcMin;

		GetWindowRect(mhwnd, &rcMin);

		mLayoutSpecs.minsize.h = rcMin.bottom - rcMin.top;
	}
}

void VDUIControlComboboxW32::PostLayoutBase(const VDUIRect& target) throw() {
	VDUIRect r(target);

	if (mhwnd)
		r.y2 += SendMessage(mhwnd, CB_GETITEMHEIGHT, 0, 0) * minrows + 2*GetSystemMetrics(SM_CYEDGE);

	VDUIControlBaseW32::PostLayoutBase(r);
}

nsVDUI::eLinkMethod VDUIControlComboboxW32::GetLinkMethod() {
	return nsVDUI::kLinkMethodInt;
}

int VDUIControlComboboxW32::GetStatei() {
	if (!mhwnd)
		return 0;

	return SendMessage(mhwnd, CB_GETCURSEL, 0, 0);
}

void VDUIControlComboboxW32::SetStatei(int i) {
	if (!mhwnd)
		return;

	if (i >= mnItems)
		i = mnItems - 1;

	if (i < 0)
		i = 0;

	SendMessage(mhwnd, CB_SETCURSEL, i, 0);
}

int VDUIControlComboboxW32::GetItemCount() {
	return mnItems;
}

int VDUIControlComboboxW32::AddItem(const wchar_t *text, bool bIncludeInAutoSize, void *pCookie, int nInsertBefore) {
	if (mhwnd) {
		SIZE siz={0,0};
		HDC hdc;
		int idx;

		if (nsVDUI::isWindows9x()) {
			const char *str = VDFastTextWToA(text);

			if (bIncludeInAutoSize) {
				if (hdc = GetDC(mhwnd)) {
					HGDIOBJ hgoOldFont;

					hgoOldFont = SelectObject(hdc, (HGDIOBJ)SendMessage(mhwnd, WM_GETFONT, 0, 0));

					GetTextExtentPoint32A(hdc, str, strlen(str), &siz);

					SelectObject(hdc, hgoOldFont);

					ReleaseDC(mhwnd, hdc);
				}
			}

			if (nInsertBefore < 0)
				idx = SendMessageA(mhwnd, CB_ADDSTRING, 0, (LPARAM)str);
			else
				idx = SendMessageA(mhwnd, CB_INSERTSTRING, nInsertBefore, (LPARAM)str);

			VDFastTextFree();
		} else {

			if (bIncludeInAutoSize)
				if (hdc = GetDC(mhwnd)) {
					HGDIOBJ hgoOldFont;

					hgoOldFont = SelectObject(hdc, (HGDIOBJ)SendMessage(mhwnd, WM_GETFONT, 0, 0));

					GetTextExtentPoint32W(hdc, text, wcslen(text), &siz);

					SelectObject(hdc, hgoOldFont);

					ReleaseDC(mhwnd, hdc);
				}

			if (nInsertBefore < 0)
				idx = SendMessageW(mhwnd, CB_ADDSTRING, 0, (LPARAM)text);
			else
				idx = SendMessageW(mhwnd, CB_INSERTSTRING, nInsertBefore, (LPARAM)text);
		}

		if (idx >= 0) {
			if (bIncludeInAutoSize)
				if (siz.cx > minlen)
					minlen = siz.cx;

			SendMessage(mhwnd, CB_SETITEMDATA, idx, (LPARAM)pCookie);

			if (!mnItems++)
				SendMessage(mhwnd, CB_SETCURSEL, 0, 0);

			return idx;
		}
	}

	return -1;
}

void VDUIControlComboboxW32::DeleteItem(int item) {
	if (mhwnd && item >= 0 && item < mnItems) {
		SendMessage(mhwnd, CB_DELETESTRING, item, 0);
		--mnItems;
	}
}

void VDUIControlComboboxW32::DeleteAllItems() {
	if (mhwnd)
		SendMessage(mhwnd, CB_RESETCONTENT, 0, 0);

	minlen = 0;
	mnItems = 0;
}

void *VDUIControlComboboxW32::GetItemCookie(int item) {
	if (mhwnd && item >= 0 && item < mnItems)
		return (void *)SendMessage(mhwnd, CB_GETITEMDATA, item, 0);

	return NULL;
}

void VDUIControlComboboxW32::SetItemCookie(int item, void *pCookie) {
	if (mhwnd && item >= 0 && item < mnItems)
		SendMessage(mhwnd, CB_SETITEMDATA, item, (DWORD)pCookie);
}

// SetItemText() is quite clumsy for combo boxes.  We must insert a new
// string afterward and delete the old one, then restore the selection
// if necessary.

void VDUIControlComboboxW32::SetItemText(int item, const wchar_t *pText) {
	if (mhwnd && item >= 0 && item < mnItems) {
		int idx;
		int nSelected = GetStatei();

		if (nsVDUI::isWindows9x()) {
			idx = SendMessageA(mhwnd, CB_INSERTSTRING, item, (LPARAM)VDFastTextWToA(pText));

			VDFastTextFree();
		} else
			idx = SendMessageW(mhwnd, CB_INSERTSTRING, item, (LPARAM)pText);

		if (idx >= 0) {
			SendMessageA(mhwnd, CB_SETITEMDATA, idx, SendMessageA(mhwnd, CB_GETITEMDATA, idx+1, 0));
			SendMessageA(mhwnd, CB_DELETESTRING, idx+1, 0);

			if (item == nSelected)
				SendMessage(mhwnd, CB_SETCURSEL, item, 0);
		}
	}
}

void VDUIControlComboboxW32::Dispatch_WM_COMMAND(IVDUICallback *pCB, UINT code) throw() {
	if (code == CBN_SELCHANGE) {
		ProcessLinks();
//		pCB->UIAction(pctx, mID, IVDUIDlgContextCallback::kModified);
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlListViewW32
//
///////////////////////////////////////////////////////////////////////////

VDUIControlListViewW32::VDUIControlListViewW32(int minlines)
	: minlen(0)
	, minrows(minlines)
	, mnItems(0)
	, mnColumns(0)
	, mnSelected(-1)
	, mnExtensibleColumn(-1)
	, mpListCallback(NULL)
	, mNextBuffer(0)
{
	for(int i=0; i<2; ++i) {
		mBuffer[i].psza = NULL;
		mBuffer[i].capacity = 0;
	}
}

VDUIControlListViewW32::~VDUIControlListViewW32() {
	for(int i=0; i<2; ++i) {
		free(mBuffer[i].psza);
	}
}

bool VDUIControlListViewW32::Create(IVDUIControl *pControl) {
	if (VDUIControlBase::Create(pControl)
		&& _Create(0, WC_LISTVIEWA, WC_LISTVIEWW, LVS_REPORT|LVS_SHOWSELALWAYS|LVS_SINGLESEL, WS_EX_CLIENTEDGE)) {

		SendMessage(mhwnd, LVM_SETEXTENDEDLISTVIEWSTYLE, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
		SendMessage(mhwnd, LVM_SETUNICODEFORMAT, TRUE, 0);

		mbUnicode = !!SendMessage(mhwnd, LVM_GETUNICODEFORMAT, 0, 0);

		// 4.00 common controls library is lame -- punt

		LVCOLUMNA lvc;

		lvc.mask		= LVCF_FMT | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
		lvc.fmt			= LVCFMT_LEFT;
		lvc.cx			= 100;
		lvc.pszText		= (char *)"";
		lvc.iSubItem	= 0;

		SendMessageA(mhwnd, LVM_INSERTCOLUMNA, 0, (LPARAM)&lvc);

		LVITEMA lvi;

		lvi.mask		= LVIF_TEXT;
		lvi.iItem		= 0;
		lvi.iSubItem	= 0;
		lvi.pszText		= (char *)"";

		SendMessageA(mhwnd, LVM_INSERTITEMA, 0, (LPARAM)&lvi);

		RECT r = { LVIR_BOUNDS };	// also lame

		if (SendMessageA(mhwnd, LVM_GETITEMRECT, 0, (LPARAM)&r)) {
			mnItemHeight = r.bottom - r.top;
			mnHeaderHeight = r.top;
		} else {		// uhhh....
			mnItemHeight = 0;
			mnHeaderHeight = 0;

			VDASSERT(false);
		}


		SendMessageA(mhwnd, LVM_DELETEALLITEMS, 0, 0);
		SendMessageA(mhwnd, LVM_DELETECOLUMN, 0, 0);

		return true;
	}

	return false;
}

void VDUIControlListViewW32::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	RECT rcMin;

	rcMin.left = rcMin.top = 0;
	rcMin.right = minlen + 4*GetSystemMetrics(SM_CXEDGE) + GetSystemMetrics(SM_CXVSCROLL);
	rcMin.bottom = mnHeaderHeight + mnItemHeight * minrows + 4*GetSystemMetrics(SM_CYEDGE);

	if (mhwnd) {
		AdjustWindowRectEx(&rcMin, GetWindowLong(mhwnd, GWL_STYLE), FALSE, GetWindowLong(mhwnd, GWL_EXSTYLE));
	}

	mLayoutSpecs.minsize.w = rcMin.left + rcMin.right;
	mLayoutSpecs.minsize.h = rcMin.top + rcMin.bottom;
}

void VDUIControlListViewW32::PostLayoutBase(const VDUIRect& target) {
	VDUIControlListBaseW32::PostLayoutBase(target);

	// Redo the expansible column if necessary.

	if (mhwnd && mnColumns && mnExtensibleColumn>=0) {
		UINT *colarray = new UINT[mnColumns];

		if (colarray) {
			int usable_width = target.w() - 2*GetSystemMetrics(SM_CXEDGE);
			int to_left=0, to_right=0;
			int i;

			for(i=0; i<mnColumns; ++i)
				colarray[i] = SendMessage(mhwnd, LVM_GETCOLUMNWIDTH, i, 0);

			for(i=0; i<mnExtensibleColumn; ++i)
				to_left += colarray[i];

			for(i=mnExtensibleColumn+1; i<mnColumns; ++i)
				to_right += colarray[i];

			if (usable_width < to_left + to_right)
				SendMessageA(mhwnd, LVM_SETCOLUMNWIDTH, mnExtensibleColumn, MAKELPARAM(0, 0));
			else
				SendMessageA(mhwnd, LVM_SETCOLUMNWIDTH, mnExtensibleColumn, MAKELPARAM(usable_width - (to_left + to_right), 0));

			delete[] colarray;
		}
	}
}

nsVDUI::eLinkMethod VDUIControlListViewW32::GetLinkMethod() {
	return nsVDUI::kLinkMethodInt;
}

int VDUIControlListViewW32::GetStatei() {
//	if (!mhwnd)
//		return 0;

//	return (int)SendMessage(mhwnd, LVM_GETNEXTITEM, -1, LVNI_ALL|LVNI_SELECTED);

	return mnSelected;
}

void VDUIControlListViewW32::SetStatei(int i) {
	if (i >= mnItems)
		i = mnItems - 1;

	if (i < 0)
		i = 0;

	if (mnSelected != i) {
		mnSelected = i;

		if (mhwnd) {
			LVITEM lvi;

			lvi.stateMask = LVIS_SELECTED;
			lvi.state = 0;

			if (mnSelected >= 0)
				SendMessage(mhwnd, LVM_SETITEMSTATE, mnSelected, (LPARAM)&lvi);

			lvi.stateMask = LVIS_SELECTED|LVIS_FOCUSED;
			lvi.state = LVIS_SELECTED|LVIS_FOCUSED;

			SendMessage(mhwnd, LVM_SETITEMSTATE, i, (LPARAM)&lvi);
		}
	}
}

int VDUIControlListViewW32::GetItemCount() {
	return mnItems;
}

int VDUIControlListViewW32::AddItem(const wchar_t *text, bool bIncludeInAutoSize, void *pCookie, int nInsertBefore) {
	if (mhwnd) {
		int width;
		int idx;

		if (!mbUnicode) {
			const char *str = VDFastTextWToA(text);

			if (bIncludeInAutoSize)
				width = SendMessageA(mhwnd, LVM_GETSTRINGWIDTHA, 0, (LPARAM)str);

			LVITEMA lvi;

			lvi.mask		= LVIF_TEXT | LVIF_PARAM;
			lvi.iItem		= nInsertBefore < 0 ? 0xFFFFFFF : nInsertBefore;
			lvi.iSubItem	= 0;
			lvi.pszText		= (char *)(str ? str : LPSTR_TEXTCALLBACKA);
			lvi.lParam		= (LPARAM)pCookie;

			idx = SendMessageA(mhwnd, LVM_INSERTITEMA, 0, (LPARAM)&lvi);
			VDFastTextFree();

		} else {
			if (bIncludeInAutoSize)
				width = SendMessageA(mhwnd, LVM_GETSTRINGWIDTHW, 0, (LPARAM)text);

			LVITEMW lvi;

			lvi.mask		= LVIF_TEXT | LVIF_PARAM;
			lvi.iItem		= nInsertBefore < 0 ? 0xFFFFFFF : nInsertBefore;
			lvi.iSubItem	= 0;
			lvi.pszText		= (wchar_t *)(text ? text : LPSTR_TEXTCALLBACKW);
			lvi.lParam		= (LPARAM)pCookie;

			idx = SendMessageA(mhwnd, LVM_INSERTITEMW, 0, (LPARAM)&lvi);
		}

		if (idx >= 0) {
			if (bIncludeInAutoSize)
				if (width > minlen)
					minlen = width;

			++mnItems;

			return idx;
		}
	}

	return -1;
}

void VDUIControlListViewW32::DeleteItem(int item) {
	if (item >= 0 && item < mnItems) {
		if (mhwnd)
			SendMessage(mhwnd, LVM_DELETEITEM, item, 0);

		--mnItems;
	}
}

void VDUIControlListViewW32::DeleteAllItems() {
	if (mhwnd)
		SendMessage(mhwnd, LVM_DELETEALLITEMS, 0, 0);

	mnItems = 0;
	minlen = 0;
}

void *VDUIControlListViewW32::GetItemCookie(int item) {
	if (mhwnd && item >= 0 && item < mnItems) {
		LVITEM lvi;

		lvi.mask = LVIF_PARAM;
		lvi.iItem = item;
		lvi.iSubItem = 0;
		lvi.lParam = NULL;		// safety

		SendMessageA(mhwnd, LVM_GETITEM, 0, (LPARAM)&lvi);

		return (void *)lvi.lParam;
	}

	return NULL;
}

void VDUIControlListViewW32::SetItemCookie(int item, void *pCookie) {
	if (mhwnd && item >= 0 && item < mnItems) {
		LVITEM lvi;

		lvi.mask = LVIF_PARAM;
		lvi.iItem = item;
		lvi.iSubItem = 0;
		lvi.lParam = NULL;		// safety

		SendMessageA(mhwnd, LVM_SETITEM, 0, (LPARAM)&lvi);
	}
}

void VDUIControlListViewW32::SetItemText(int item, const wchar_t *pText) {
	if (mhwnd && item >= 0 && item < mnItems) {
		if (!mbUnicode) {
			LVITEMA lvi;

			lvi.mask = LVIF_TEXT;
			lvi.iItem = item;
			lvi.iSubItem = 0;
			lvi.pszText = (char *)VDFastTextWToA(pText);

			SendMessageA(mhwnd, LVM_SETITEMA, 0, (LPARAM)&lvi);

			VDFastTextFree();
		} else {
			LVITEMW lvi;

			lvi.mask = LVIF_TEXT;
			lvi.iItem = item;
			lvi.iSubItem = 0;
			lvi.pszText = (wchar_t *)pText;

			SendMessageA(mhwnd, LVM_SETITEMW, 0, (LPARAM)&lvi);
		}
	}
}

void VDUIControlListViewW32::AddColumn(const wchar_t *pText, int width_units) {
	UINT width;

	if (width_units < 0) {
		width = 0;
		mnExtensibleColumn = mnColumns;
	} else {
		VDUIRect r = {0,0,width_units,0};

		GetBase()->MapUnitsToPixels(r);

		width = r.x2;
	}

	if (!mbUnicode) {
		LVCOLUMNA lvc;

		lvc.mask		= LVCF_FMT | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
		lvc.fmt			= LVCFMT_LEFT;
		lvc.cx			= 1;
		lvc.pszText		= (char *)VDFastTextWToA(pText);
		lvc.iSubItem	= mnColumns;

		SendMessageA(mhwnd, LVM_INSERTCOLUMNA, mnColumns, (LPARAM)&lvc);

		VDFastTextFree();
	} else {
		LVCOLUMNW lvc;

		lvc.mask		= LVCF_FMT | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
		lvc.fmt			= LVCFMT_LEFT;
		lvc.cx			= 1;
		lvc.pszText		= (wchar_t *)pText;
		lvc.iSubItem	= mnColumns;

		SendMessageA(mhwnd, LVM_INSERTCOLUMNW, mnColumns, (LPARAM)&lvc);
	}

	SendMessageA(mhwnd, LVM_SETCOLUMNWIDTH, mnColumns, MAKELPARAM(width, 0));

	++mnColumns;
}

void VDUIControlListViewW32::UpdateItem(int item) {
	if (mhwnd)
		SendMessage(mhwnd, LVM_UPDATE, item, 0);
}

void VDUIControlListViewW32::SetSource(IVDUIListCallback *pCB) {
	mpListCallback = pCB;
}

int CALLBACK VDUIControlListViewW32::StaticSorter(LPARAM p1, LPARAM p2, LPARAM thisPtr) {
	VDUIControlListViewW32 *pThis = (VDUIControlListViewW32 *)thisPtr;

	return pThis->mpListCallback->UIListSortRequest(pThis, (void *)p1, (void *)p2);
}

void VDUIControlListViewW32::Sort() {
	if (mhwnd) {
		VDASSERT(mpListCallback);

		SendMessageA(mhwnd, LVM_SORTITEMS, (LPARAM)this, (LPARAM)StaticSorter);
	}
}

void VDUIControlListViewW32::Dispatch_WM_NOTIFY(IVDUICallback *pCB, UINT code, const NMHDR *pHdr) throw() {
	if (code == LVN_ITEMCHANGED) {
		const NMLISTVIEW *plvn = (const NMLISTVIEW *)pHdr;

		if ((plvn->uOldState|plvn->uNewState) & LVIS_SELECTED) {
			int iSel = (int)SendMessage(mhwnd, LVM_GETNEXTITEM, -1, LVNI_ALL|LVNI_SELECTED);

			if (iSel != mnSelected) {
				mnSelected = iSel;

				ProcessLinks();
				pCB->UIEvent(this, mID, IVDUICallback::kEventSelect, mnSelected);
			}
		}
	} else if (code == LVN_GETDISPINFOA) {
		NMLVDISPINFO *pdi = (NMLVDISPINFO *)pHdr;

		if (mpListCallback) {
			bool bPersistent;

			const wchar_t *pszw = mpListCallback->UIListTextRequest(this, pdi->item.iItem, pdi->item.iSubItem, bPersistent);

			// In ANSI mode, we must convert, so the persistence flag means nothing.

			const char *psza = VDFastTextWToA(pszw);
			int len = strlen(psza)+1;

			if (len > mBuffer[mNextBuffer].capacity) {
				int newcap = (len + 15) & ~15;
				char *pNew = (char *)realloc(mBuffer[mNextBuffer].psza, newcap);

				if (!pNew)
					return;

				mBuffer[mNextBuffer].capacity = newcap;
				mBuffer[mNextBuffer].psza = pNew;
			}

			pdi->item.pszText = (char *)mBuffer[mNextBuffer].psza;
			memcpy(pdi->item.pszText, psza, len);

			mNextBuffer ^= 1;
		}
	} else if (code == LVN_GETDISPINFOW) {
		NMLVDISPINFO *pdi = (NMLVDISPINFO *)pHdr;

		if (mpListCallback) {
			bool bPersistent;

			const wchar_t *pszw = mpListCallback->UIListTextRequest(this, pdi->item.iItem, pdi->item.iSubItem, bPersistent);

			if (!pszw)
				pszw = L"";

			if (bPersistent) {
				pdi->item.pszText = (char *)pszw;
			} else {
				int len = (wcslen(pszw)+1) * sizeof(wchar_t);

				if (len > mBuffer[mNextBuffer].capacity) {
					int newcap = (len + 15) & ~15;
					wchar_t *pNew = (wchar_t *)realloc(mBuffer[mNextBuffer].pszw, newcap);

					if (!pNew)
						return;

					mBuffer[mNextBuffer].capacity = newcap;
					mBuffer[mNextBuffer].pszw = pNew;
				}

				pdi->item.pszText = (char *)mBuffer[mNextBuffer].pszw;

				memcpy(pdi->item.pszText, pszw, len);

				mNextBuffer ^= 1;
			}
		}

	} else if (code == NM_DBLCLK) {
		if (mnSelected >= 0)
			pCB->UIEvent(this, mID, IVDUICallback::kEventDoubleClick, mnSelected);
	}
}
