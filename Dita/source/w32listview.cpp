//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <stdafx.h>
#include <vd2/system/w32assist.h>
#include <windows.h>
#include <commctrl.h>
#include <vector>

#include <vd2/Dita/w32control.h>

#ifdef _MSC_VER
	#pragma comment(lib, "comctl32")
#endif

class VDUIListViewW32 : public VDUIControlW32, public IVDUIListView, public IVDUIList {
public:
	VDUIListViewW32();

	void *AsInterface(uint32 id);

	bool Create(IVDUIParameters *);
	void PreLayoutBaseW32(const VDUILayoutSpecs&);

protected:
	int GetItemCount();
	void AddItem(const wchar_t *text, uintptr data);
	void AddColumn(const wchar_t *name, int width, int affinity);
	bool IsItemChecked(int item);
	void OnNotifyCallback(const NMHDR *);
	void OnResize();

	struct Column {
		int mWidth;
		int mAffinity;
	};

	int mSelected;
	int		mTotalAffinity;
	int		mTotalWidth;
	bool	mbCheckable;
	std::vector<Column>	mColumns;
};

extern IVDUIWindow *VDCreateUIListView() { return new VDUIListViewW32; }

VDUIListViewW32::VDUIListViewW32()
	: mSelected(-1)
	, mTotalAffinity(0)
	, mTotalWidth(0)
{
	InitCommonControls();
}

void *VDUIListViewW32::AsInterface(uint32 id) {
	if (id == IVDUIListView::kTypeID) return static_cast<IVDUIListView *>(this);
	if (id == IVDUIList::kTypeID) return static_cast<IVDUIList *>(this);

	return VDUIControlW32::AsInterface(id);
}

bool VDUIListViewW32::Create(IVDUIParameters *pParameters) {
	mbCheckable = pParameters->GetB(nsVDUI::kUIParam_Checkable, false);

	DWORD dwFlags = LVS_REPORT;

	if (pParameters->GetB(nsVDUI::kUIParam_NoHeader, false))
		dwFlags |= LVS_NOCOLUMNHEADER;

	if (!CreateW32(pParameters, WC_LISTVIEW, dwFlags))
		return false;

	if (mbCheckable) {
		const int cx = GetSystemMetrics(SM_CXMENUCHECK);
		const int cy = GetSystemMetrics(SM_CYMENUCHECK);

		if (HBITMAP hbm = CreateBitmap(cx, cy, 1, 1, NULL)) {
			if (HDC hdc = CreateCompatibleDC(NULL)) {
				if (HGDIOBJ hbmOld = SelectObject(hdc, hbm)) {
					bool success = false;

					RECT r = { 0, 0, cx, cy };

					SetBkColor(hdc, PALETTEINDEX(0));
					ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &r, "", 0, NULL);
					DrawFrameControl(hdc, &r, DFC_BUTTON, DFCS_BUTTONCHECK|DFCS_CHECKED);

					SelectObject(hdc, hbmOld);

					if (HIMAGELIST himl = ImageList_Create(cx, cy, ILC_COLOR, 1, 1)) {
						if (ImageList_Add(himl, hbm, NULL) >= 0)
							ListView_SetImageList(mhwnd, himl, LVSIL_STATE);
						else
							ImageList_Destroy(himl);
					}
				}

				DeleteDC(hdc);
			}

			DeleteObject(hbm);
		}
	}

	return true;
}

void VDUIListViewW32::PreLayoutBaseW32(const VDUILayoutSpecs& parentConstraints) {
}

int VDUIListViewW32::GetItemCount() {
	return (int)ListView_GetItemCount(mhwnd);
}

void VDUIListViewW32::AddItem(const wchar_t *text, uintptr data) {
	DWORD dwMask = LVIF_PARAM | LVIF_TEXT;

	if (mbCheckable)
		dwMask |= LVIF_STATE;

	if (VDIsWindowsNT()) {
		LVITEMW lviw={0};

		lviw.mask		= dwMask;
		lviw.iItem		= 0;
		lviw.iSubItem	= 0;
		lviw.state		= 0x1000;
		lviw.stateMask	= (UINT)-1;
		lviw.pszText	= (LPWSTR)text;
		lviw.lParam		= (LPARAM)data;

		SendMessageW(mhwnd, LVM_INSERTITEMW, 0, (LPARAM)&lviw);
	} else {
		LVITEMA lvia={0};

		VDStringA textA(VDTextWToA(text));

		lvia.mask		= dwMask;
		lvia.iItem		= 0;
		lvia.iSubItem	= 0;
		lvia.state		= 0x1000;
		lvia.stateMask	= (UINT)-1;
		lvia.pszText	= (LPSTR)textA.c_str();
		lvia.lParam		= (LPARAM)data;

		SendMessageA(mhwnd, LVM_INSERTITEMA, 0, (LPARAM)&lvia);
	}
}

void VDUIListViewW32::AddColumn(const wchar_t *name, int width, int affinity) {
	VDASSERT(affinity >= 0);
	VDASSERT(width >= 0);

	if (VDIsWindowsNT()) {
		LVCOLUMNW lvcw={0};

		lvcw.mask		= LVCF_TEXT | LVCF_WIDTH;
		lvcw.pszText	= (LPWSTR)name;
		lvcw.cx			= width;

		SendMessageW(mhwnd, LVM_INSERTCOLUMNW, mColumns.size(), (LPARAM)&lvcw);
	} else {
		LVCOLUMNA lvca={0};
		VDStringA nameA(VDTextWToA(name));

		lvca.mask		= LVCF_TEXT | LVCF_WIDTH;
		lvca.pszText	= (LPSTR)nameA.c_str();
		lvca.cx			= width;

		SendMessageA(mhwnd, LVM_INSERTCOLUMNA, mColumns.size(), (LPARAM)&lvca);
	}

	mColumns.push_back(Column());
	Column& col = mColumns.back();

	col.mWidth		= width;
	col.mAffinity	= affinity;

	mTotalWidth		+= width;
	mTotalAffinity	+= affinity;

	OnResize();
}

bool VDUIListViewW32::IsItemChecked(int item) {
	UINT oldState = ListView_GetItemState(mhwnd, item, -1);

	return 0 != (oldState & 0x1000);
}

void VDUIListViewW32::OnNotifyCallback(const NMHDR *pHdr) {
   	if (pHdr->code == LVN_ITEMCHANGED) {
   		const NMLISTVIEW *plvn = (const NMLISTVIEW *)pHdr;
   
   		if ((plvn->uOldState|plvn->uNewState) & LVIS_SELECTED) {
   			int iSel = (int)SendMessage(mhwnd, LVM_GETNEXTITEM, -1, LVNI_ALL|LVNI_SELECTED);
   
   			if (iSel != mSelected) {
   				mSelected = iSel;
   
				mpBase->ProcessValueChange(this, mID);
   				mpBase->DispatchEvent(this, mID, IVDUICallback::kEventSelect, mSelected);
   			}
   		}
	} else if ((pHdr->code == NM_CLICK || pHdr->code == NM_DBLCLK) && mbCheckable) {
		DWORD pos = GetMessagePos();

		LVHITTESTINFO lvhi = {0};

		lvhi.pt.x = (SHORT)LOWORD(pos);
		lvhi.pt.y = (SHORT)HIWORD(pos);

		ScreenToClient(mhwnd, &lvhi.pt);

		int idx = ListView_HitTest(mhwnd, &lvhi);

		if (idx >= 0) {
			UINT oldState = ListView_GetItemState(mhwnd, idx, -1);

			ListView_SetItemState(mhwnd, idx, oldState ^ INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
			ListView_RedrawItems(mhwnd, idx, idx);
		}
	}
}

void VDUIListViewW32::OnResize() {
	const int ncols = mColumns.size();
	const Column *pcol = &mColumns[0];
	RECT r;

	GetClientRect(mhwnd, &r);

	int spaceLeft = r.right - mTotalWidth;
	int affinityLeft = mTotalAffinity;

	for(int i=0; i<ncols; ++i) {
		const Column& col = *pcol++;
		int width = col.mWidth;

		if (affinityLeft && col.mAffinity) {
			int extra = (spaceLeft * col.mAffinity + affinityLeft - 1) / affinityLeft;

			affinityLeft -= col.mAffinity;
			spaceLeft -= extra;
			width += extra;
		}	

		SendMessageA(mhwnd, LVM_SETCOLUMNWIDTH, i, MAKELPARAM((int)width, 0));
	}
}
