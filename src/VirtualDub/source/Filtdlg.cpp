//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#include "stdafx.h"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include "VideoSource.h"
#include <vd2/system/error.h>
#include <vd2/system/list.h>
#include <vd2/Dita/services.h>
#include <vd2/Kasumi/pixmapops.h>

#include "plugins.h"
#include "resource.h"
#include "oshelper.h"
#include "PositionControl.h"
#include "ClippingControl.h"
#include "gui.h"

#include "filtdlg.h"
#include "filters.h"

extern HINSTANCE g_hInst;
extern const char g_szError[];
extern FilterFunctions g_filterFuncs;

extern vdrefptr<VideoSource> inputVideoAVI;

enum {
	kFileDialog_LoadPlugin		= 'plug',
};

//////////////////////////////

bool VDShowFilterClippingDialog(VDGUIHandle hParent, FilterInstance *pFiltInst, List *pFilterList);
void FilterLoadFilter(HWND hWnd);

///////////////////////////////////////////////////////////////////////////
//
//	add filter dialog
//
///////////////////////////////////////////////////////////////////////////


class VDDialogFilterListW32 : public VDDialogBaseW32 {
public:
	inline VDDialogFilterListW32() : VDDialogBaseW32(IDD_FILTER_LIST) {}

	FilterDefinitionInstance *Activate(VDGUIHandle hParent);

protected:
	INT_PTR DlgProc(UINT message, WPARAM wParam, LPARAM lParam);
	void ReinitDialog();

	HWND						mhwndList;
	FilterDefinitionInstance	*mpFilterDefInst;
	std::list<FilterBlurb>		mFilterList;
};

FilterDefinitionInstance *VDDialogFilterListW32::Activate(VDGUIHandle hParent) {
	return ActivateDialog(hParent) ? mpFilterDefInst : NULL;
}

void VDDialogFilterListW32::ReinitDialog() {
	static INT tabs[]={ 175 };

	mhwndList = GetDlgItem(mhdlg, IDC_FILTER_LIST);

	mFilterList.clear();
	FilterEnumerateFilters(mFilterList);

	int index;

	SendMessage(mhwndList, LB_SETTABSTOPS, 1, (LPARAM)tabs);
	SendMessage(mhwndList, LB_RESETCONTENT, 0, 0);

	for(std::list<FilterBlurb>::const_iterator it(mFilterList.begin()), itEnd(mFilterList.end()); it!=itEnd; ++it) {
		const FilterBlurb& fb = *it;
		char buf[256];

		wsprintf(buf,"%s\t%s", fb.name.c_str(), fb.author.c_str());
		index = SendMessage(mhwndList, LB_ADDSTRING, 0, (LPARAM)buf);
		SendMessage(mhwndList, LB_SETITEMDATA, (WPARAM)index, (LPARAM)&fb);
	}
}

INT_PTR VDDialogFilterListW32::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_INITDIALOG:
			ReinitDialog();
            return TRUE;

        case WM_COMMAND:
			switch(HIWORD(wParam)) {
			case LBN_SELCANCEL:
				SendMessage(GetDlgItem(mhdlg, IDC_FILTER_INFO), WM_SETTEXT, 0, (LPARAM)"");
				break;
			case LBN_SELCHANGE:
				{
					int index;

					if (LB_ERR != (index = SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0))) {
						const FilterBlurb& fb = *(FilterBlurb *)SendMessage((HWND)lParam, LB_GETITEMDATA, (WPARAM)index, 0);

						SendMessage(GetDlgItem(mhdlg, IDC_FILTER_INFO), WM_SETTEXT, 0, (LPARAM)fb.description.c_str());
					}
				}
				break;
			case LBN_DBLCLK:
				SendMessage(mhdlg, WM_COMMAND, MAKELONG(IDOK, BN_CLICKED), (LPARAM)GetDlgItem(mhdlg, IDOK));
				break;
			default:
				switch(LOWORD(wParam)) {
				case IDOK:
					{
						int index;

						if (LB_ERR != (index = SendMessage(mhwndList, LB_GETCURSEL, 0, 0))) {
							const FilterBlurb& fb = *(FilterBlurb *)SendMessage(mhwndList, LB_GETITEMDATA, (WPARAM)index, 0);

							mpFilterDefInst = fb.key;

							End(true);
						}
					}
					return TRUE;
				case IDCANCEL:
					End(false);
					return TRUE;
				case IDC_LOAD:
					FilterLoadFilter(mhdlg);
					ReinitDialog();
					return TRUE;
				}
			}
            break;
    }
    return FALSE;
}

///////////////////////////////////////////////////////////////////////////
//
//	Filter list dialog
//
///////////////////////////////////////////////////////////////////////////

void MakeFilterList(List& list, HWND hWndList) {
	int count;
	int ind;
	FilterInstance *fa;

	if (LB_ERR == (count = SendMessage(hWndList, LB_GETCOUNT, 0, 0))) return;

	// have to do this since the filter list is intrusive
	filters.DeinitFilters();
	filters.DeallocateBuffers();

	for(ind=count-1; ind>=0; ind--) {
		fa = (FilterInstance *)SendMessage(hWndList, LB_GETITEMDATA, (WPARAM)ind, 0);

		list.AddTail(fa);
	}
}

static void EnableConfigureBox(HWND hdlg, int index = -1) {
	HWND hwndList = GetDlgItem(hdlg, IDC_FILTER_LIST);

	if (index < 0)
		index = SendMessage(hwndList, LB_GETCURSEL, 0, 0);

	if (index != LB_ERR) {
		FilterInstance *fa = (FilterInstance *)SendMessage(hwndList, LB_GETITEMDATA, (WPARAM)index, 0);

		EnableWindow(GetDlgItem(hdlg, IDC_CONFIGURE), !!fa->filter->configProc);
		EnableWindow(GetDlgItem(hdlg, IDC_CLIPPING), TRUE);
		EnableWindow(GetDlgItem(hdlg, IDC_BLENDING), TRUE);
	} else {
		EnableWindow(GetDlgItem(hdlg, IDC_CONFIGURE), FALSE);
		EnableWindow(GetDlgItem(hdlg, IDC_CLIPPING), FALSE);
		EnableWindow(GetDlgItem(hdlg, IDC_BLENDING), FALSE);
	}
}

static void RedoFilters(HWND hWndList) {
	List listFA;
	int ind, ind2, l;
	FilterInstance *fa;
	int sel;

	sel = SendMessage(hWndList, LB_GETCURSEL, 0, 0);

	MakeFilterList(listFA, hWndList);

	try {
		if (inputVideoAVI) {
			BITMAPINFOHEADER *bmih = inputVideoAVI->getImageFormat();
			filters.prepareLinearChain(&listFA, (Pixel *)(bmih+1), bmih->biWidth, abs(bmih->biHeight), 24);
		} else {
			filters.prepareLinearChain(&listFA, NULL, 320, 240, 24);
		}
	} catch(const MyError&) {
		return;
	}

	ind = 0;
	fa = (FilterInstance *)listFA.tail.next;
	while(fa->next) {
		char buf[2048];
		l = wsprintf(buf, "%dx%d\t%dx%d\t%s%s"
				,fa->src.w
				,fa->src.h
				,fa->dst.w
				,fa->dst.h
				,fa->GetAlphaParameterCurve() ? "[B] " : ""
				,fa->filter->name);

		if (fa->filter->stringProc2)
			fa->filter->stringProc2(fa, &g_filterFuncs, buf+l, (sizeof buf) - l);
		else if (fa->filter->stringProc)
			fa->filter->stringProc(fa, &g_filterFuncs, buf+l);

		if (LB_ERR == (ind2 = SendMessage(hWndList, LB_INSERTSTRING, (WPARAM)ind, (LPARAM)buf)))
			return;

		SendMessage(hWndList, LB_SETITEMDATA, (WPARAM)ind2, (LPARAM)fa);
		SendMessage(hWndList, LB_DELETESTRING, (WPARAM)ind+1, 0);

		fa = (FilterInstance *)fa->next;
		++ind;
	}

   if (sel != LB_ERR)
      SendMessage(hWndList, LB_SETCURSEL, sel, 0);
}

INT_PTR CALLBACK FilterDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static const char szPending[]="(pending)";

    switch (message)
    {
        case WM_INITDIALOG:
			{
				HWND hWndList = GetDlgItem(hDlg, IDC_FILTER_LIST);
				FilterInstance *fa_list, *fa;
				int index;

				fa_list = (FilterInstance *)g_listFA.tail.next;

				while(fa_list->next) {
					try {
						fa = fa_list->Clone();

						if (LB_ERR != (index = SendMessage(hWndList, LB_ADDSTRING, 0, (LPARAM)szPending)))
							SendMessage(hWndList, LB_SETITEMDATA, (WPARAM)index, (LPARAM)fa);
						else
							delete fa;

					} catch(const MyError&) {
						// bleah!  should really do something...
					}

					fa_list = (FilterInstance *)fa_list->next;
				}

				RedoFilters(hWndList);

				SetFocus(hWndList);
			}
            return FALSE;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_FILTER_LIST:
				switch(HIWORD(wParam)) {
				case LBN_DBLCLK:
					SendMessage(hDlg, WM_COMMAND, MAKELONG(IDC_CONFIGURE, BN_CLICKED), (LPARAM)GetDlgItem(hDlg, IDC_CONFIGURE));
					break;
				case LBN_SELCHANGE:
					EnableConfigureBox(hDlg);
					break;
				}
				break;

			case IDC_ADD:
				if (FilterDefinitionInstance *fdi = VDDialogFilterListW32().Activate((VDGUIHandle)hDlg)) {
					int index;
					HWND hWndList = GetDlgItem(hDlg, IDC_FILTER_LIST);

					if (LB_ERR != (index = SendMessage(hWndList, LB_ADDSTRING, 0, (LPARAM)szPending))) {
						try {
							FilterInstance *fa = new FilterInstance(fdi);

							fa->x1 = fa->y1 = fa->x2 = fa->y2 = 0;

							SendMessage(hWndList, LB_SETITEMDATA, (WPARAM)index, (LPARAM)fa);

							RedoFilters(hWndList);

							if (fa->filter->configProc) {
								List list;
								bool fRemove;

								MakeFilterList(list, hWndList);

								{
									FilterPreview fp(inputVideoAVI ? &list : NULL, fa);

									fa->ifp = &fp;
									fa->ifp2 = &fp;

									fRemove = 0!=fa->filter->configProc(fa, &g_filterFuncs, hDlg);
								}

								if (fRemove) {
									delete fa;
									SendMessage(hWndList, LB_DELETESTRING, (WPARAM)index, 0);
									break;
								}
							}

							RedoFilters(hWndList);

							SendMessage(hWndList, LB_SETCURSEL, (WPARAM)index, 0);
							EnableConfigureBox(hDlg, index);
						} catch(const MyError& e) {
							e.post(hDlg, g_szError);
						}
					}
				}
				break;

			case IDC_DELETE:
				{
					HWND hWndList = GetDlgItem(hDlg, IDC_FILTER_LIST);
					int index;

					if (LB_ERR != (index = SendMessage(hWndList, LB_GETCURSEL, 0, 0))) {
						FilterInstance *fa = (FilterInstance *)SendMessage(hWndList, LB_GETITEMDATA, (WPARAM)index, 0);

						delete fa;

						SendMessage(hWndList, LB_DELETESTRING, (WPARAM)index, 0);
					}
				}
				break;

			case IDC_CONFIGURE:
				{
					HWND hWndList = GetDlgItem(hDlg, IDC_FILTER_LIST);
					int index;

					if (LB_ERR != (index = SendMessage(hWndList, LB_GETCURSEL, 0, 0))) {
						FilterInstance *fa = (FilterInstance *)SendMessage(hWndList, LB_GETITEMDATA, (WPARAM)index, 0);

						if (fa->filter->configProc) {
							List list;

							RedoFilters(hWndList);
							MakeFilterList(list, hWndList);

							{
								FilterPreview fp(inputVideoAVI ? &list : NULL, fa);

								fa->ifp = &fp;
								fa->ifp2 = &fp;
								fa->filter->configProc(fa, &g_filterFuncs, hDlg);
							}
							RedoFilters(hWndList);
							SendMessage(hWndList, LB_SETCURSEL, (WPARAM)index, 0);

						}
					}
				}
				break;

			case IDC_CLIPPING:
				{
					HWND hWndList = GetDlgItem(hDlg, IDC_FILTER_LIST);
					int index;

					RedoFilters(hWndList);

					if (LB_ERR != (index = SendMessage(hWndList, LB_GETCURSEL, 0, 0))) {
						FilterInstance *fa = (FilterInstance *)SendMessage(hWndList, LB_GETITEMDATA, (WPARAM)index, 0);

						List filterList;
						MakeFilterList(filterList, hWndList);

						if (VDShowFilterClippingDialog((VDGUIHandle)hDlg, fa, &filterList)) {
							RedoFilters(hWndList);
							SendMessage(hWndList, LB_SETCURSEL, (WPARAM)index, 0);
						}
					}
				}
				break;

			case IDC_BLENDING:
				{
					HWND hWndList = GetDlgItem(hDlg, IDC_FILTER_LIST);
					int index;

					if (LB_ERR != (index = SendMessage(hWndList, LB_GETCURSEL, 0, 0))) {
						FilterInstance *fa = (FilterInstance *)SendMessage(hWndList, LB_GETITEMDATA, (WPARAM)index, 0);

						if (fa->GetAlphaParameterCurve()) {
							fa->SetAlphaParameterCurve(NULL);
						} else {
							VDParameterCurve *curve = new_nothrow VDParameterCurve();
							if (curve) {
								curve->SetYRange(0.0f, 1.0f);
								fa->SetAlphaParameterCurve(curve);
							}
						}

						RedoFilters(hWndList);
					}
				}
				break;

			case IDC_MOVEUP:
				{
					HWND hWndList = GetDlgItem(hDlg, IDC_FILTER_LIST);
					int index;
					LONG lpData;

					if (LB_ERR != (index = SendMessage(hWndList, LB_GETCURSEL, 0, 0))) {
						if (index == 0) break;

						lpData = SendMessage(hWndList, LB_GETITEMDATA, (WPARAM)index, 0);
						SendMessage(hWndList, LB_INSERTSTRING, (WPARAM)index-1, (LPARAM)szPending);
						SendMessage(hWndList, LB_SETITEMDATA, (WPARAM)index-1, (LPARAM)lpData);
						SendMessage(hWndList, LB_DELETESTRING, (WPARAM)index+1, 0);
						RedoFilters(hWndList);
						SendMessage(hWndList, LB_SETCURSEL, (WPARAM)index-1, 0);
					}
				}
				break;

			case IDC_MOVEDOWN:
				{
					HWND hWndList = GetDlgItem(hDlg, IDC_FILTER_LIST);
					int index, count;
					LONG lpData;

					if (LB_ERR != (index = SendMessage(hWndList, LB_GETCURSEL, 0, 0))) {
						if (LB_ERR != (count = SendMessage(hWndList, LB_GETCOUNT, 0, 0))) {
							if (index == count-1) break;

							lpData = SendMessage(hWndList, LB_GETITEMDATA, (WPARAM)index, 0);
							SendMessage(hWndList, LB_INSERTSTRING, (WPARAM)index+2, (LPARAM)szPending);
							SendMessage(hWndList, LB_SETITEMDATA, (WPARAM)index+2, (LPARAM)lpData);
							SendMessage(hWndList, LB_DELETESTRING, (WPARAM)index, 0);
							RedoFilters(hWndList);
							SendMessage(hWndList, LB_SETCURSEL, (WPARAM)index+1, 0);
						}
					}
				}
				break;

			case IDOK:
				// We must force filters to stop before we muck with the global list... in case
				// the pane refresh restarted them.
				filters.DeinitFilters();
				filters.DeallocateBuffers();
				{
					HWND hwndList = GetDlgItem(hDlg, IDC_FILTER_LIST);
					FilterInstance *fa, *fa2;

					fa = (FilterInstance *)g_listFA.tail.next;

					while(fa2 = (FilterInstance *)fa->next) {
						if (!fa->filter->copyProc)
							fa->ForceNoDeinit();

						delete fa;

						fa = fa2;
					}

					g_listFA.Init();

					MakeFilterList(g_listFA, hwndList);
				}
				EndDialog(hDlg, TRUE);
				return TRUE;
			case IDCANCEL:
				// We must force filters to stop before we muck with the global list... in case
				// the pane refresh restarted them.
				filters.DeinitFilters();
				filters.DeallocateBuffers();
				{
					HWND hWndList = GetDlgItem(hDlg, IDC_FILTER_LIST);
					List list;
					FilterInstance *fa, *fa2;

					MakeFilterList(list, hWndList);

					fa = (FilterInstance *)list.tail.next;
					while(fa2 = (FilterInstance *)fa->next) {
						if (!fa->filter->copyProc)
							fa->ForceNoDeinit();
						delete fa;
						fa = fa2;
					}
				}
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
            break;
    }
    return FALSE;
}


///////////////////////////////////////////////////////////////////////////
//
//	filter crop dialog
//
///////////////////////////////////////////////////////////////////////////

class VDFilterClippingDialog : public VDDialogBaseW32 {
public:
	VDFilterClippingDialog(FilterInstance *pFiltInst, List *pFilterList)
		: VDDialogBaseW32(IDD_FILTER_CLIPPING)
		, mpFilterList(pFilterList)
		, mpFilterInst(pFiltInst)
	{
	}

protected:
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void UpdateFrame(VDPosition pos);

	List			*mpFilterList;
	FilterInstance	*mpFilterInst;
	FilterStateInfo	mfsi;
	FilterSystem	mFilterSys;
	IVDClippingControl	*mpClipCtrl;
	IVDPositionControl	*mpPosCtrl;
};

INT_PTR VDFilterClippingDialog::DlgProc(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_INITDIALOG:
			{
				LONG hspace;
				RECT rw, rc, rcok, rccancel;
				HWND hWnd, hWndCancel;

				// try to init filters
				if (mpFilterList && inputVideoAVI) {
					const BITMAPINFOHEADER *pbih = inputVideoAVI->getImageFormat();
					const BITMAPINFOHEADER *pbih2 = inputVideoAVI->getDecompressedFormat();

					try {
						// halt the main filter system
						filters.DeinitFilters();
						filters.DeallocateBuffers();

						// start private filter system
						mFilterSys.initLinearChain(
								mpFilterList,
								(Pixel *)((const char *)pbih + pbih->biSize),
								pbih2->biWidth,
								abs(pbih2->biHeight),
								0);

						mfsi.lCurrentFrame			= 0;
						mfsi.lCurrentSourceFrame	= 0;
						mfsi.lMicrosecsPerFrame		= (long)inputVideoAVI->getRate().scale64ir(1000000);
						mfsi.lMicrosecsPerSrcFrame	= mfsi.lMicrosecsPerFrame;
						mfsi.lSourceFrameMS			= 0;
						mfsi.lDestFrameMS			= 0;
						mfsi.flags = FilterStateInfo::kStatePreview;

						mFilterSys.ReadyFilters(mfsi);
					} catch(const MyError&) {
						// eat the error
					}
				}

				HWND hwndClipping = GetDlgItem(mhdlg, IDC_BORDERS);
				mpClipCtrl = VDGetIClippingControl((VDGUIHandle)hwndClipping);

				mpClipCtrl->SetBitmapSize(mpFilterInst->origw, mpFilterInst->origh);
				mpClipCtrl->SetClipBounds(vdrect32(mpFilterInst->x1, mpFilterInst->y1, mpFilterInst->x2, mpFilterInst->y2));

				mpPosCtrl = VDGetIPositionControlFromClippingControl((VDGUIHandle)hwndClipping);
				guiPositionInitFromStream(mpPosCtrl);

				GetWindowRect(mhdlg, &rw);
				GetWindowRect(hwndClipping, &rc);
				const int origH = (rw.bottom - rw.top);
				int padW = (rw.right - rw.left) - (rc.right - rc.left);
				int padH = origH - (rc.bottom - rc.top);

				mpClipCtrl->AutoSize(padW, padH);

				GetWindowRect(hwndClipping, &rc);
				MapWindowPoints(NULL, mhdlg, (LPPOINT)&rc, 2);

				const int newH = (rc.bottom - rc.top) + padH;
				const int deltaH = newH - origH;
				SetWindowPos(mhdlg, NULL, 0, 0, (rc.right - rc.left) + padW, newH, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOMOVE);
				SendMessage(mhdlg, DM_REPOSITION, 0, 0);

				hWndCancel = GetDlgItem(mhdlg, IDCANCEL);
				hWnd = GetDlgItem(mhdlg, IDOK);
				GetWindowRect(hWnd, &rcok);
				GetWindowRect(hWndCancel, &rccancel);
				hspace = rccancel.left - rcok.right;
				ScreenToClient(mhdlg, (LPPOINT)&rcok.left);
				ScreenToClient(mhdlg, (LPPOINT)&rcok.right);
				ScreenToClient(mhdlg, (LPPOINT)&rccancel.left);
				ScreenToClient(mhdlg, (LPPOINT)&rccancel.right);
				SetWindowPos(hWndCancel, NULL, rc.right - (rccancel.right-rccancel.left), rccancel.top + deltaH, 0,0,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
				SetWindowPos(hWnd, NULL, rc.right - (rccancel.right-rccancel.left) - (rcok.right-rcok.left) - hspace, rcok.top + deltaH, 0,0,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);

				// render first frame
				UpdateFrame(mpPosCtrl->GetPosition());
			}

            return (TRUE);

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					vdrect32 r;
					mpClipCtrl->GetClipBounds(r);
					mpFilterInst->x1 = r.left;
					mpFilterInst->y1 = r.top;
					mpFilterInst->x2 = r.right;
					mpFilterInst->y2 = r.bottom;
				}
				End(TRUE);
				return TRUE;
			case IDCANCEL:
				End(FALSE);
				return TRUE;
			case IDC_BORDERS:
				UpdateFrame(guiPositionHandleCommand(wParam, mpPosCtrl));
				return TRUE;
			}
            break;

		case WM_NOTIFY:
			if (GetWindowLong(((NMHDR *)lParam)->hwndFrom, GWL_ID) == IDC_BORDERS) {
				VDPosition pos = guiPositionHandleNotify(lParam, mpPosCtrl);

				if (pos >= 0)
					UpdateFrame(pos);
			}
			break;

    }
    return FALSE;
}

void VDFilterClippingDialog::UpdateFrame(VDPosition pos) {
	if (mFilterSys.isRunning()) {
		bool success = false;

		if (pos >= inputVideoAVI->getStart() && pos < inputVideoAVI->getEnd()) {
			mfsi.lCurrentSourceFrame	= (long)pos;
			mfsi.lCurrentFrame			= (long)pos;
			mfsi.lSourceFrameMS			= VDRoundToLong(1000000.0 / inputVideoAVI->getRate().asDouble() * pos);
			mfsi.lDestFrameMS			= mfsi.lSourceFrameMS;

			try {
				if (inputVideoAVI->getFrame(pos)) {
					VDPixmapBlt(VDAsPixmap(*mFilterSys.InputBitmap()), inputVideoAVI->getTargetFormat());
					mFilterSys.RunFilters(mfsi, mpFilterInst);

					VDPixmap output;
					if (mpFilterInst->prev->prev)
						output = VDAsPixmap(static_cast<FilterInstance *>(mpFilterInst->prev)->realDst);
					else
						output = VDAsPixmap(*mFilterSys.InputBitmap());

					mpClipCtrl->BlitFrame(&output);
					success = true;
				}
			} catch(const MyError&) {
				// eat the error
			}
		}

		if (!success)
			mpClipCtrl->BlitFrame(NULL);
	} else
		guiPositionBlit(GetDlgItem(mhdlg, IDC_BORDERS), pos, mpFilterInst->origw, mpFilterInst->origh);
}

bool VDShowFilterClippingDialog(VDGUIHandle hParent, FilterInstance *pFiltInst, List *pFilterList) {
	VDFilterClippingDialog dlg(pFiltInst, pFilterList);

	return 0 != dlg.ActivateDialog(hParent);
}

///////////////////////////////////////////////////////////////////////

void FilterLoadFilter(HWND hWnd) {
	const VDStringW filename(VDGetLoadFileName(kFileDialog_LoadPlugin, (VDGUIHandle)hWnd, L"Load external filter", L"VirtualDub filter (*.vdf)\0*.vdf\0Windows Dynamic-Link Library (*.dll)\0*.dll\0All files (*.*)\0*.*\0", NULL, NULL, NULL));

	if (!filename.empty()) {
		try {
			VDAddPluginModule(filename.c_str());
		} catch(const MyError& e) {
			e.post(hWnd, g_szError);
		}
	}
}


