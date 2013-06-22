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

#include <crtdbg.h>

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include "VideoSource.h"
#include "Error.h"
#include "List.h"

#include "resource.h"
#include "oshelper.h"
#include "ClippingControl.h"
#include "error.h"
#include "gui.h"

#include "filtdlg.h"
#include "filters.h"

extern HINSTANCE g_hInst;
extern char g_msgBuf[];
extern const char g_szError[];
extern FilterFunctions g_filterFuncs;

extern VideoSource *inputVideoAVI;

//////////////////////////////

BOOL APIENTRY FilterClippingDlgProc(HWND hDlg, UINT message, UINT wParam, LONG lParam);
void FilterLoadFilter(HWND hWnd);

//////////////////////////////

static FilterDefinition *g_selectedFilter;

void MakeFilterList(List& list, HWND hWndList) {
	int count;
	int ind;
	FilterInstance *fa;

	if (LB_ERR == (count = SendMessage(hWndList, LB_GETCOUNT, 0, 0))) return;

	_RPT0(0,"MFL start\n");
	for(ind=count-1; ind>=0; ind--) {
		fa = (FilterInstance *)SendMessage(hWndList, LB_GETITEMDATA, (WPARAM)ind, 0);

		_RPT1(0,"adding: %p\n", fa);
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
	} else {
		EnableWindow(GetDlgItem(hdlg, IDC_CONFIGURE), FALSE);
		EnableWindow(GetDlgItem(hdlg, IDC_CLIPPING), FALSE);
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
			filters.prepareLinearChain(&listFA, (Pixel *)(bmih+1), bmih->biWidth, bmih->biHeight, 24, 24);
		} else {
			filters.prepareLinearChain(&listFA, NULL, 320, 240, 24, 24);
		}
	} catch(const MyError&) {
		return;
	}

	ind = 0;
	fa = (FilterInstance *)listFA.tail.next;
	while(fa->next) {
		l = wsprintf(g_msgBuf, "%dx%d\t%dx%d\t%s"
				,fa->src.w
				,fa->src.h
				,fa->dst.w
				,fa->dst.h
				,fa->filter->name);

		if (fa->filter->stringProc) fa->filter->stringProc(fa, &g_filterFuncs, g_msgBuf+l);

		if (LB_ERR == (ind2 = SendMessage(hWndList, LB_INSERTSTRING, (WPARAM)ind, (LPARAM)g_msgBuf)))
			return;

		SendMessage(hWndList, LB_SETITEMDATA, (WPARAM)ind2, (LPARAM)fa);
		SendMessage(hWndList, LB_DELETESTRING, (WPARAM)ind+1, 0);

		fa = (FilterInstance *)fa->next;
		++ind;
	}

   if (sel != LB_ERR)
      SendMessage(hWndList, LB_SETCURSEL, sel, 0);
}

BOOL APIENTRY FilterDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam)
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
				if (DialogBox(g_hInst, MAKEINTRESOURCE(IDD_FILTER_LIST), hDlg, AddFilterDlgProc)) {
					int index;
					HWND hWndList = GetDlgItem(hDlg, IDC_FILTER_LIST);

					if (LB_ERR != (index = SendMessage(hWndList, LB_ADDSTRING, 0, (LPARAM)szPending))) {
						try {
							FilterInstance *fa = new FilterInstance(g_selectedFilter);

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

							MakeFilterList(list, hWndList);

							{
								FilterPreview fp(inputVideoAVI ? &list : NULL, fa);

								fa->ifp = &fp;
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
						const unsigned long x1 = fa->x1, y1=fa->y1, x2=fa->x2, y2=fa->y2;


						if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_FILTER_CLIPPING), hDlg, FilterClippingDlgProc, (LPARAM)fa)) {
							RedoFilters(hWndList);
							SendMessage(hWndList, LB_SETCURSEL, (WPARAM)index, 0);
						} else {
							fa->x1 = x1;
							fa->y1 = y1;
							fa->x2 = x2;
							fa->y2 = y2;
						}
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
				{
					HWND hwndList = GetDlgItem(hDlg, IDC_FILTER_LIST);
					FilterInstance *fa, *fa2;

					fa = (FilterInstance *)g_listFA.tail.next;

					while(fa2 = (FilterInstance *)fa->next) {
						_RPT1(0,"Deleting %p\n", fa);
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
				{
					HWND hWndList = GetDlgItem(hDlg, IDC_FILTER_LIST);
					List list;
					FilterInstance *fa, *fa2;

					MakeFilterList(list, hWndList);

					fa = (FilterInstance *)list.tail.next;
					while(fa2 = (FilterInstance *)fa->next) {
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

void AddFilterDlgInit(HWND hDlg) {
	static INT tabs[]={ 175 };

	HWND hWndList = GetDlgItem(hDlg, IDC_FILTER_LIST);
	FilterDefinition *fd = filter_list;
	int index;

	SendMessage(hWndList, LB_SETTABSTOPS, 1, (LPARAM)tabs);
	SendMessage(hWndList, LB_RESETCONTENT, 0, 0);

	while(fd) {
		wsprintf(g_msgBuf,"%s\t%s",fd->name,fd->maker?fd->maker:"(internal)");
		index = SendMessage(hWndList, LB_ADDSTRING, 0, (LPARAM)g_msgBuf);
		SendMessage(hWndList, LB_SETITEMDATA, (WPARAM)index, (LPARAM)fd);
		fd=fd->next;
	}

	SetFocus(hWndList);
}

BOOL APIENTRY AddFilterDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
			AddFilterDlgInit(hDlg);
            return FALSE;

        case WM_COMMAND:
			switch(HIWORD(wParam)) {
			case LBN_SELCANCEL:
				SendMessage(GetDlgItem(hDlg, IDC_FILTER_INFO), WM_SETTEXT, 0, (LPARAM)"");
				break;
			case LBN_SELCHANGE:
				{
					FilterDefinition *fd;
					int index;

					if (LB_ERR != (index = SendMessage((HWND)lParam, LB_GETCURSEL, 0, 0))) {
						fd = (FilterDefinition *)SendMessage((HWND)lParam, LB_GETITEMDATA, (WPARAM)index, 0);

						SendMessage(GetDlgItem(hDlg, IDC_FILTER_INFO), WM_SETTEXT, 0, (LPARAM)fd->desc);
					}
				}
				break;
			case LBN_DBLCLK:
				SendMessage(hDlg, WM_COMMAND, MAKELONG(IDOK, BN_CLICKED), (LPARAM)GetDlgItem(hDlg, IDOK));
				break;
			default:
				switch(LOWORD(wParam)) {
				case IDOK:
					{
						int index;
						HWND hWndList = GetDlgItem(hDlg, IDC_FILTER_LIST);

						if (LB_ERR != (index = SendMessage(hWndList, LB_GETCURSEL, 0, 0))) {
							g_selectedFilter = (FilterDefinition *)SendMessage(hWndList, LB_GETITEMDATA, (WPARAM)index, 0);

							EndDialog(hDlg, TRUE);
						}
					}
					return TRUE;
				case IDCANCEL:
					EndDialog(hDlg, FALSE);
					return TRUE;
				case IDC_LOAD:
					FilterLoadFilter(hDlg);
					AddFilterDlgInit(hDlg);
					return TRUE;
				}
			}
            break;
    }
    return FALSE;
}


BOOL APIENTRY FilterClippingDlgProc(HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	FilterInstance *fa;

    switch (message)
    {
        case WM_INITDIALOG:
			{
				ClippingControlBounds ccb;
				LONG hborder, hspace;
				RECT rw, rc, rcok, rccancel;
				HWND hWnd, hWndCancel;

				fa = (FilterInstance *)lParam;
				SetWindowLong(hDlg, DWL_USER, (LONG)fa);

				hWnd = GetDlgItem(hDlg, IDC_BORDERS);
				ccb.x1	= fa->x1;
				ccb.x2	= fa->x2;
				ccb.y1	= fa->y1;
				ccb.y2	= fa->y2;

/*				if (inputVideoAVI) {
					BITMAPINFOHEADER *bmi = inputVideoAVI->getImageFormat();
					SendMessage(hWnd, CCM_SETBITMAPSIZE, 0, MAKELONG(bmi->biWidth,bmi->biHeight));
				} else
					SendMessage(hWnd, CCM_SETBITMAPSIZE, 0, MAKELONG(320,240));*/

				SendMessage(hWnd, CCM_SETBITMAPSIZE, 0, MAKELONG(fa->origw,fa->origh));
				SendMessage(hWnd, CCM_SETCLIPBOUNDS, 0, (LPARAM)&ccb);

				guiPositionInitFromStream(hWnd);

				GetWindowRect(hDlg, &rw);
				GetWindowRect(hWnd, &rc);
				hborder = rc.left - rw.left;
				ScreenToClient(hDlg, (LPPOINT)&rc.left);
				ScreenToClient(hDlg, (LPPOINT)&rc.right);

				SetWindowPos(hDlg, NULL, 0, 0, (rc.right - rc.left) + hborder*2, (rw.bottom-rw.top)+(rc.bottom-rc.top), SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOMOVE);

				hWndCancel = GetDlgItem(hDlg, IDCANCEL);
				hWnd = GetDlgItem(hDlg, IDOK);
				GetWindowRect(hWnd, &rcok);
				GetWindowRect(hWndCancel, &rccancel);
				hspace = rccancel.left - rcok.right;
				ScreenToClient(hDlg, (LPPOINT)&rcok.left);
				ScreenToClient(hDlg, (LPPOINT)&rcok.right);
				ScreenToClient(hDlg, (LPPOINT)&rccancel.left);
				ScreenToClient(hDlg, (LPPOINT)&rccancel.right);
				SetWindowPos(hWndCancel, NULL, rc.right - (rccancel.right-rccancel.left), rccancel.top + (rc.bottom-rc.top), 0,0,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
				SetWindowPos(hWnd, NULL, rc.right - (rccancel.right-rccancel.left) - (rcok.right-rcok.left) - hspace, rcok.top + (rc.bottom-rc.top), 0,0,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
			}

            return (TRUE);

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					ClippingControlBounds ccb;

					fa = (FilterInstance *)GetWindowLong(hDlg, DWL_USER);
					SendMessage(GetDlgItem(hDlg, IDC_BORDERS), CCM_GETCLIPBOUNDS, 0, (LPARAM)&ccb);
					fa->x1 = ccb.x1;
					fa->y1 = ccb.y1;
					fa->x2 = ccb.x2;
					fa->y2 = ccb.y2;
					EndDialog(hDlg, TRUE);
				}
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			case IDC_BORDERS:
				fa = (FilterInstance *)GetWindowLong(hDlg, DWL_USER);
				guiPositionBlit((HWND)lParam, guiPositionHandleCommand(wParam, lParam), fa->origw, fa->origh);
				return TRUE;
			}
            break;

		case WM_NOTIFY:
			if (GetWindowLong(((NMHDR *)lParam)->hwndFrom, GWL_ID) == IDC_BORDERS) {
				fa = (FilterInstance *)GetWindowLong(hDlg, DWL_USER);

				if (fa)
					guiPositionBlit(((NMHDR *)lParam)->hwndFrom, guiPositionHandleNotify(wParam, lParam), fa->origw, fa->origh);
			}
			break;

    }
    return FALSE;
}

///////////////////////////////////////////////////////////////////////

void FilterLoadFilter(HWND hWnd) {
	OPENFILENAME ofn;
	char szFile[MAX_PATH];

	szFile[0]=0;

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= hWnd;
	ofn.lpstrFilter			= "VirtualDub filter (*.vdf)\0*.vdf\0Windows Dynamic-Link Library (*.dll)\0*.dll\0All files (*.*)\0*.*\0";
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= szFile;
	ofn.nMaxFile			= sizeof szFile;
	ofn.lpstrFileTitle		= NULL;
	ofn.nMaxFileTitle		= 0;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= "Load external filter";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
	ofn.lpstrDefExt			= NULL;

	if (GetOpenFileName(&ofn)) {
		try {
			FilterLoadModule(szFile);
		} catch(const MyError& e) {
			e.post(hWnd,"Filter load error");
		}
	}
}


