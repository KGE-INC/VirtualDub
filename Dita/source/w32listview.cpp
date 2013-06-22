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
	void AddItem(const wchar_t *text);
	void AddColumn(const wchar_t *name, int width, int affinity);
	void OnNotifyCallback(const NMHDR *);
	void OnResize();

	struct Column {
		int mWidth;
		int mAffinity;
	};

	int mSelected;
	std::vector<Column>	mColumns;
};

extern IVDUIWindow *VDCreateUIListView() { return new VDUIListViewW32; }

VDUIListViewW32::VDUIListViewW32()
	: mSelected(-1)
{
}

void *VDUIListViewW32::AsInterface(uint32 id) {
	if (id == IVDUIListView::kTypeID) return static_cast<IVDUIListView *>(this);
	if (id == IVDUIList::kTypeID) return static_cast<IVDUIList *>(this);

	return VDUIControlW32::AsInterface(id);
}

bool VDUIListViewW32::Create(IVDUIParameters *pParameters) {
	return CreateW32(pParameters, WC_LISTVIEW, LVS_REPORT);
}

void VDUIListViewW32::PreLayoutBaseW32(const VDUILayoutSpecs& parentConstraints) {
}

void VDUIListViewW32::AddItem(const wchar_t *text) {
}

void VDUIListViewW32::AddColumn(const wchar_t *name, int width, int affinity) {
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
	}
}

void VDUIListViewW32::OnResize() {
	const int ncols = mColumns.size();
	const Column *pcol = &mColumns[0];

	for(int i=0; i<ncols; ++i) {
		const Column& col = *pcol++;

		SendMessageA(mhwnd, LVM_SETCOLUMNWIDTH, i, MAKELPARAM((int)col.mWidth, 0));
	}
}
