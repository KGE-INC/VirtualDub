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

#define f_PREFS_CPP

#include <windows.h>
#include <commctrl.h>

#include "resource.h"
#include "helpfile.h"

#include "gui.h"
#include "oshelper.h"
#include "dub.h"
#include "dubstatus.h"
#include "prefs.h"

extern HINSTANCE g_hInst;

Preferences g_prefs={
	{ 0, PreferencesMain::DEPTH_FASTEST, 0, TRUE, 0 },
	{ 50*16, 4*16 },
};

static char g_szMainPrefs[]="Main prefs";

////////////////////////////////////////////////////////////////

static DWORD dwPrefsHelpLookup[]={
	IDC_OUTPUT_DEPTH,			IDH_PREFS_MAIN_OUTPUTCOLORDEPTH,
	IDC_PREVIEW_PRIORITY,		IDH_PREFS_MAIN_PROCESSPRIORITY,
	IDC_DUB_PRIORITY,			IDH_PREFS_MAIN_PROCESSPRIORITY,
	IDC_TACK_EXTENSION,			IDH_PREFS_MAIN_ADDEXTENSION,		
	IDC_ENABLE_16DITHERING,		IDH_PREFS_DISPLAY_16BITDITHER,
	IDC_INTERFRAME_SLIDER,		IDH_PREFS_SCENE_INTERFRAME,
	IDC_INTRAFRAME_SLIDER,		IDH_PREFS_SCENE_INTRAFRAME,
	IDC_PERFOPT_DEFAULT,		IDH_PREFS_CPU_OPTIMIZATIONS,
	IDC_PERFOPT_FORCE,			IDH_PREFS_CPU_OPTIMIZATIONS,
	IDC_PERFOPT_FPU,			IDH_PREFS_CPU_OPTIMIZATIONS,
	IDC_PERFOPT_MMX,			IDH_PREFS_CPU_OPTIMIZATIONS,
	IDC_RESTRICT_AVI_1GB,		IDH_PREFS_AVI_RESTRICT_1GB,
	IDC_AUTOCORRECT_L3,			IDH_PREFS_AVI_AUTOCORRECT_L3,
	0
};

typedef struct PrefsDlgData {
	Preferences prefs;

	HWND hwndDisplay;
	RECT rcTab;
} PrefsDlgData;

static void PreferencesChildPosition(HWND hWnd, PrefsDlgData *pdd) {
	SetWindowPos(hWnd, HWND_TOP, pdd->rcTab.left, pdd->rcTab.top, 0, 0, SWP_NOSIZE);
}

static BOOL APIENTRY PreferencesMainDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	static char *szDepths[]={
		"Fastest (16-bit)",
		"Use output setting",
		"Match display depth",
		"16-bit (HiColor)",
		"24-bit (TrueColor)",
	};

	PrefsDlgData *pdd = (PrefsDlgData *)GetWindowLong(hDlg, DWL_USER);
	HWND hwndItem;
	int i;

	switch(message) {
		case WM_INITDIALOG:
			PreferencesChildPosition(hDlg, (PrefsDlgData *)lParam);
			SetWindowLong(hDlg, DWL_USER, lParam);
			pdd = (PrefsDlgData *)lParam;

			//////////////

			hwndItem = GetDlgItem(hDlg, IDC_OUTPUT_DEPTH);
			for(i=0; i<5; i++)
				SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)szDepths[i]);

			SendMessage(hwndItem, CB_SETCURSEL, pdd->prefs.main.iPreviewDepth, 0);


			hwndItem = GetDlgItem(hDlg, IDC_PREVIEW_PRIORITY);
			SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)"Default");
			for(i=0; i<8; i++)
				SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)g_szDubPriorities[i]);

			SendMessage(hwndItem, CB_SETCURSEL, pdd->prefs.main.iPreviewPriority, 0);

			hwndItem = GetDlgItem(hDlg, IDC_DUB_PRIORITY);
			SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)"Default");
			for(i=0; i<8; i++)
				SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)g_szDubPriorities[i]);

			SendMessage(hwndItem, CB_SETCURSEL, pdd->prefs.main.iDubPriority, 0);

			//////////////

			CheckDlgButton(hDlg, IDC_TACK_EXTENSION, pdd->prefs.main.fAttachExtension);
			return TRUE;

		case WM_DESTROY:
			pdd->prefs.main.iPreviewDepth = (char)SendDlgItemMessage(hDlg, IDC_OUTPUT_DEPTH, CB_GETCURSEL, 0, 0);
			pdd->prefs.main.iPreviewPriority = (char)SendDlgItemMessage(hDlg, IDC_PREVIEW_PRIORITY, CB_GETCURSEL, 0, 0);
			pdd->prefs.main.iDubPriority = (char)SendDlgItemMessage(hDlg, IDC_DUB_PRIORITY, CB_GETCURSEL, 0, 0);
			pdd->prefs.main.fAttachExtension = IsDlgButtonChecked(hDlg, IDC_TACK_EXTENSION);
			return TRUE;
	}

	return FALSE;
}

static BOOL APIENTRY PreferencesDisplayDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	PrefsDlgData *pdd = (PrefsDlgData *)GetWindowLong(hDlg, DWL_USER);

	switch(message) {
		case WM_INITDIALOG:
			PreferencesChildPosition(hDlg, (PrefsDlgData *)lParam);
			SetWindowLong(hDlg, DWL_USER, lParam);
			pdd = (PrefsDlgData *)lParam;

			//////////////

			CheckDlgButton(hDlg, IDC_ENABLE_16DITHERING, !!(pdd->prefs.fDisplay & Preferences::DISPF_DITHER16));

			return TRUE;

		case WM_DESTROY:
			pdd->prefs.fDisplay = IsDlgButtonChecked(hDlg, IDC_ENABLE_16DITHERING) ? Preferences::DISPF_DITHER16 : 0;
			return TRUE;
	}

	return FALSE;
}

static BOOL APIENTRY PreferencesSceneDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	PrefsDlgData *pdd = (PrefsDlgData *)GetWindowLong(hDlg, DWL_USER);
	HWND hwndItem;
	long pos;

	switch(message) {
		case WM_INITDIALOG:
			PreferencesChildPosition(hDlg, (PrefsDlgData *)lParam);
			SetWindowLong(hDlg, DWL_USER, lParam);
			pdd = (PrefsDlgData *)lParam;

			//////////////

			hwndItem = GetDlgItem(hDlg, IDC_INTERFRAME_SLIDER);
			SendMessage(hwndItem, TBM_SETRANGE, FALSE, MAKELONG(0,255));
			SendMessage(hwndItem, TBM_SETPOS, TRUE,
					pdd->prefs.scene.iCutThreshold
						? 256 - ((pdd->prefs.scene.iCutThreshold+8)>>4)
						: 0
					);
			SendMessage(hDlg, WM_HSCROLL, 0, (LPARAM)hwndItem);

			hwndItem = GetDlgItem(hDlg, IDC_INTRAFRAME_SLIDER);
			SendMessage(hwndItem, TBM_SETRANGE, FALSE, MAKELONG(0,256));
			SendMessage(hwndItem, TBM_SETPOS, TRUE, pdd->prefs.scene.iFadeThreshold);
			SendMessage(hDlg, WM_HSCROLL, 0, (LPARAM)hwndItem);

			return TRUE;

		case WM_HSCROLL:
			pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
			switch(GetWindowLong((HWND)lParam, GWL_ID)) {
			case IDC_INTERFRAME_SLIDER:
				if (!pos)
					SetDlgItemText(hDlg, IDC_INTERFRAME_VALUE, "Off");
				else
					SetDlgItemInt(hDlg, IDC_INTERFRAME_VALUE, pos, FALSE);
				return TRUE;
			case IDC_INTRAFRAME_SLIDER:
				if (!pos)
					SetDlgItemText(hDlg, IDC_INTRAFRAME_VALUE, "Off");
				else
					SetDlgItemInt(hDlg, IDC_INTRAFRAME_VALUE, pos, FALSE);

				return TRUE;
			}
			break;

		case WM_DESTROY:
			{
				int x = SendDlgItemMessage(hDlg, IDC_INTERFRAME_SLIDER, TBM_GETPOS, 0, 0);

				pdd->prefs.scene.iCutThreshold = x?(256-x)<<4:0;
				pdd->prefs.scene.iFadeThreshold = SendDlgItemMessage(hDlg, IDC_INTRAFRAME_SLIDER, TBM_GETPOS, 0, 0);
			}
			return TRUE;
	}

	return FALSE;
}

static BOOL APIENTRY PreferencesCPUDlgProc(HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	PrefsDlgData *pdd = (PrefsDlgData *)GetWindowLong(hDlg, DWL_USER);
	BOOL fTemp;

	switch(message) {
		case WM_INITDIALOG:
			PreferencesChildPosition(hDlg, (PrefsDlgData *)lParam);
			SetWindowLong(hDlg, DWL_USER, lParam);
			pdd = (PrefsDlgData *)lParam;

			//////////////

			CheckDlgButton(hDlg, IDC_PERFOPT_FPU, !!(pdd->prefs.main.fOptimizations & PreferencesMain::OPTF_FPU));
			CheckDlgButton(hDlg, IDC_PERFOPT_MMX, !!(pdd->prefs.main.fOptimizations & PreferencesMain::OPTF_MMX));
			CheckDlgButton(hDlg, IDC_PERFOPT_SSE, !!(pdd->prefs.main.fOptimizations & PreferencesMain::OPTF_SSE));
			CheckDlgButton(hDlg, IDC_PERFOPT_SSE2, !!(pdd->prefs.main.fOptimizations & PreferencesMain::OPTF_SSE2));
			CheckDlgButton(hDlg, IDC_PERFOPT_SSEPARTIAL, !!(pdd->prefs.main.fOptimizations & PreferencesMain::OPTF_INTEGER_SSE));
			CheckDlgButton(hDlg, IDC_PERFOPT_3DNOW, !!(pdd->prefs.main.fOptimizations & PreferencesMain::OPTF_3DNOW));
			CheckDlgButton(hDlg, IDC_PERFOPT_3DNOW2, !!(pdd->prefs.main.fOptimizations & PreferencesMain::OPTF_3DNOW_EXT));

			if (pdd->prefs.main.fOptimizations & PreferencesMain::OPTF_FORCE) {
				CheckDlgButton(hDlg, IDC_PERFOPT_FORCE, TRUE);
			} else {
				CheckDlgButton(hDlg, IDC_PERFOPT_DEFAULT, TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_PERFOPT_FPU), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_PERFOPT_MMX), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_PERFOPT_SSE), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_PERFOPT_SSE2), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_PERFOPT_SSEPARTIAL), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_PERFOPT_3DNOW), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_PERFOPT_3DNOW2), FALSE);
			}

			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_PERFOPT_DEFAULT:
			case IDC_PERFOPT_FORCE:
				fTemp = !!(SendMessage(GetDlgItem(hDlg, IDC_PERFOPT_FORCE), BM_GETSTATE, 0, 0)&3);
				EnableWindow(GetDlgItem(hDlg, IDC_PERFOPT_FPU), fTemp);
				EnableWindow(GetDlgItem(hDlg, IDC_PERFOPT_MMX), fTemp);
				EnableWindow(GetDlgItem(hDlg, IDC_PERFOPT_SSE), fTemp);
				EnableWindow(GetDlgItem(hDlg, IDC_PERFOPT_SSE2), fTemp);
				EnableWindow(GetDlgItem(hDlg, IDC_PERFOPT_SSEPARTIAL), fTemp);
				EnableWindow(GetDlgItem(hDlg, IDC_PERFOPT_3DNOW), fTemp);
				EnableWindow(GetDlgItem(hDlg, IDC_PERFOPT_3DNOW2), fTemp);
			}
			return TRUE;

		case WM_DESTROY:
			pdd->prefs.main.fOptimizations	= (IsDlgButtonChecked(hDlg, IDC_PERFOPT_FORCE) ? PreferencesMain::OPTF_FORCE : 0)
											| (IsDlgButtonChecked(hDlg, IDC_PERFOPT_FPU) ? PreferencesMain::OPTF_FPU : 0)
											| (IsDlgButtonChecked(hDlg, IDC_PERFOPT_MMX) ? PreferencesMain::OPTF_MMX : 0)
											| (IsDlgButtonChecked(hDlg, IDC_PERFOPT_3DNOW) ? PreferencesMain::OPTF_3DNOW : 0)
											| (IsDlgButtonChecked(hDlg, IDC_PERFOPT_3DNOW2) ? PreferencesMain::OPTF_3DNOW_EXT : 0)
											| (IsDlgButtonChecked(hDlg, IDC_PERFOPT_SSEPARTIAL) ? PreferencesMain::OPTF_INTEGER_SSE : 0)
											| (IsDlgButtonChecked(hDlg, IDC_PERFOPT_SSE) ? PreferencesMain::OPTF_SSE : 0)
											| (IsDlgButtonChecked(hDlg, IDC_PERFOPT_SSE2) ? PreferencesMain::OPTF_SSE2 : 0);
			return TRUE;
	}

	return FALSE;
}

static BOOL APIENTRY PreferencesAVIDlgProc(HWND hdlg, UINT message, UINT wParam, LONG lParam) {
	PrefsDlgData *pdd = (PrefsDlgData *)GetWindowLong(hdlg, DWL_USER);

	switch(message) {
		case WM_INITDIALOG:
			PreferencesChildPosition(hdlg, (PrefsDlgData *)lParam);
			SetWindowLong(hdlg, DWL_USER, lParam);
			pdd = (PrefsDlgData *)lParam;

			//////////////

			CheckDlgButton(hdlg, IDC_RESTRICT_AVI_1GB, !!pdd->prefs.fAVIRestrict1Gb);
			CheckDlgButton(hdlg, IDC_AUTOCORRECT_L3, !!pdd->prefs.fNoCorrectLayer3);
			return TRUE;

		case WM_DESTROY:
			pdd->prefs.fAVIRestrict1Gb = IsDlgButtonChecked(hdlg, IDC_RESTRICT_AVI_1GB);
			pdd->prefs.fNoCorrectLayer3 = IsDlgButtonChecked(hdlg, IDC_AUTOCORRECT_L3);
			return TRUE;
	}

	return FALSE;
}

////////////////////////////////////////////////////////////////

static struct prefsTabs {
	LPTSTR	rsrc;
	char	*name;
	DLGPROC	dProc;
} tabs[]={
	{	MAKEINTRESOURCE(IDD_PREFS_MAIN),	"Main",		PreferencesMainDlgProc	},
	{	MAKEINTRESOURCE(IDD_PREFS_DISPLAY),	"Display",	PreferencesDisplayDlgProc },
	{	MAKEINTRESOURCE(IDD_PREFS_SCENE),	"Scene",	PreferencesSceneDlgProc	},
	{	MAKEINTRESOURCE(IDD_PREFS_CPU),		"CPU",		PreferencesCPUDlgProc	},
	{	MAKEINTRESOURCE(IDD_PREFS_AVI),		"AVI",		PreferencesAVIDlgProc	},
};

BOOL APIENTRY PreferencesDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {

	PrefsDlgData *pdd = (PrefsDlgData *)GetWindowLong(hDlg, DWL_USER);

	//////////

	switch(message) {
		case WM_INITDIALOG:
			{
				RECT r, r2;
				LONG du = GetDialogBaseUnits();
				LONG duX = LOWORD(du);
				LONG duY = HIWORD(du);
				HWND hWndTab = GetDlgItem(hDlg, IDC_TAB);
				LONG xDelta, yDelta;
				POINT p;
				int i;

				if (!(pdd = new PrefsDlgData)) return FALSE;
				memset(pdd, 0, sizeof pdd);
				SetWindowLong(hDlg, DWL_USER, (LPARAM)pdd);

				pdd->prefs = g_prefs;

				for(i=0; i<(sizeof tabs/sizeof tabs[0]); i++) {
					TC_ITEM ti;

					ti.mask		= TCIF_TEXT;
					ti.pszText	= tabs[i].name;

					TabCtrl_InsertItem(hWndTab, i, &ti);
				}

				r.left = r.top = 0;
				r.right = 250;
				r.bottom = 150;
				MapDialogRect(hDlg, &r);

				GetWindowRect(hWndTab, &r2);
				p.x = r2.left;
				p.y = r2.top;
				ScreenToClient(hDlg, &p);
				pdd->rcTab = r;

				TabCtrl_AdjustRect(hWndTab, TRUE, &r);
				OffsetRect(&pdd->rcTab, p.x-r.left, p.y-r.top);

				xDelta = (r.right-r.left) - (r2.right-r2.left);
				yDelta = (r.bottom-r.top) - (r2.bottom-r2.top);

				SetWindowPos(hWndTab, NULL, 0, 0, r.right-r.left, r.bottom-r.top, SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);

				guiOffsetDlgItem(hDlg, IDC_SAVE, xDelta, yDelta);
				guiOffsetDlgItem(hDlg, IDOK, xDelta, yDelta);
				guiOffsetDlgItem(hDlg, IDCANCEL, xDelta, yDelta);

				GetWindowRect(hDlg, &r);
				SetWindowPos(hDlg, NULL, 0, 0, r.right-r.left + xDelta, r.bottom-r.top + yDelta, SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOZORDER);

				pdd->hwndDisplay = CreateDialogParam(g_hInst, tabs[0].rsrc, hDlg, tabs[0].dProc, (LPARAM)pdd);
			}
			return TRUE;

		case WM_DESTROY:
			if (pdd) {
				delete pdd;
				SetWindowLong(hDlg, DWL_USER, 0);
			}
			return TRUE;

		case WM_NOTIFY: {
			NMHDR *nm = (LPNMHDR)lParam;

			switch(nm->code) {
			case TCN_SELCHANGE:
				{
					int iTab = TabCtrl_GetCurSel(nm->hwndFrom);

					if (iTab>=0) {
						if (pdd->hwndDisplay) DestroyWindow(pdd->hwndDisplay);
						pdd->hwndDisplay = CreateDialogParam(g_hInst, tabs[iTab].rsrc, hDlg, tabs[iTab].dProc, (LPARAM)pdd);
					}
				}
				return TRUE;
			}
			}break;

	    case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_SAVE:
			case IDOK:
				if (pdd->hwndDisplay) {
					DestroyWindow(pdd->hwndDisplay);
					pdd->hwndDisplay = NULL;
				}
				g_prefs = pdd->prefs;

				if (LOWORD(wParam) == IDC_SAVE) {
					SetConfigBinary("", g_szMainPrefs, (char *)&g_prefs, sizeof g_prefs);
				}

				EndDialog(hDlg, TRUE);
				return TRUE;
			case IDCANCEL:
				if (pdd->hwndDisplay) {
					DestroyWindow(pdd->hwndDisplay);
					pdd->hwndDisplay = NULL;
				}
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
            break;

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					VDShowHelp(hDlg, L"d-preferences.html");
			}
			return TRUE;
	}

	return FALSE;
}

void LoadPreferences() {
	DWORD dwSize;
	Preferences prefs_t;

	dwSize = QueryConfigBinary("", g_szMainPrefs, (char *)&prefs_t, sizeof prefs_t);

	if (dwSize) {
		if (dwSize > sizeof g_prefs) dwSize = sizeof g_prefs;

		memcpy(&g_prefs, &prefs_t, dwSize);
	}
}
