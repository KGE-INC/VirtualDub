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

#include <windows.h>
#include <commctrl.h>

#include "resource.h"

#include "ProgressDialog.h"

#include "Error.h"

extern HINSTANCE g_hInst;

ProgressDialog::ProgressDialog(HWND hwndParent, const char *szTitle, const char *szCaption, long maxval, bool fAbortEnabled) {
	lpszTitle		= szTitle;
	lpszCaption		= szCaption;
	lpszValueFormat	= NULL;
	this->maxval	= maxval;
	this->curval	= 0;
	this->newval	= 0;
	this->fAbortEnabled = fAbortEnabled;

	fAbort = false;

	hwndProgressBar = NULL;
	hwndValue		= NULL;
	hwndDialog		= NULL;

	CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_PROGRESS), hwndParent, ProgressDlgProc, (LPARAM)this);
}

ProgressDialog::~ProgressDialog() {
	close();
}

void ProgressDialog::setValueFormat(const char *sz) {
	lpszValueFormat = sz;
}

#if 0
void ProgressDialog::advance(long newval) {
	if (!hwndProgressBar) return;

	if (newval > curval) {
		curval = newval;
		if (curval > maxval) curval = maxval;

		SendMessage(hwndProgressBar, PBM_SETPOS, (WPARAM)MulDiv(curval, 16384, maxval), 0);
	}
}
#endif

void ProgressDialog::check() {
	MSG msg;

	while(PeekMessage(&msg, hwndDialog, 0, 0, PM_REMOVE)) {
		if (!IsWindow(hwndDialog) || !IsDialogMessage(hwndDialog, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (fAbort)
		throw MyUserAbortError();
}

void ProgressDialog::close() {
	if (hwndDialog)
		DestroyWindow(hwndDialog);
}

BOOL CALLBACK ProgressDialog::ProgressDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	ProgressDialog *thisPtr = (ProgressDialog *)GetWindowLong(hDlg, DWL_USER);
	int newval2;

	switch(msg) {
		case WM_INITDIALOG:
			SetWindowLong(hDlg, DWL_USER, lParam);

			thisPtr = (ProgressDialog *)lParam;
			thisPtr->hwndProgressBar = GetDlgItem(hDlg, IDC_PROGRESS);
			thisPtr->hwndValue = GetDlgItem(hDlg, IDC_CURRENT_VALUE);
			SendMessage(GetDlgItem(hDlg, IDC_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, 16384));

			if (!thisPtr->fAbortEnabled)
				EnableWindow(GetDlgItem(hDlg, IDCANCEL), FALSE);

			SetWindowText(hDlg, thisPtr->lpszTitle);
			SetDlgItemText(hDlg, IDC_STATIC_MESSAGE, thisPtr->lpszCaption);

			thisPtr->hwndDialog = hDlg;

			SetTimer(hDlg, 1, 500, NULL);

			break;

		case WM_TIMER:
			newval2 = MulDiv(thisPtr->newval, 16384, thisPtr->maxval);

			if (newval2 > thisPtr->curval) {
				if (newval2 > 16384) newval2 = 16384;
				thisPtr->curval = newval2;

				SendMessage(thisPtr->hwndProgressBar, PBM_SETPOS, (WPARAM)newval2, 0);
			}

			if (thisPtr->lpszValueFormat) {
				char szTemp[128];

				wsprintf(szTemp, thisPtr->lpszValueFormat, thisPtr->newval, thisPtr->maxval);
				SendMessage(thisPtr->hwndValue, WM_SETTEXT, 0, (LPARAM)szTemp);
			}
			return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDCANCEL)
				thisPtr->fAbort = true;
			return TRUE;
	}

	return FALSE;
}
