//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2000 Avery Lee
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

#include <stdio.h>
#include <stdarg.h>
#include <crtdbg.h>

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include "ClippingControl.h"
#include "PositionControl.h"
#include "VideoSource.h"
#include "Error.h"
#include "List.h"

#include "gui.h"
#include "resource.h"

#define MAX_STATUS_PARTS (8)

static COLORREF g_crCustomColors[16];

extern HINSTANCE g_hInst;
extern HWND g_hWnd;
extern HWND g_hwndJobs;

static HWND g_hwndDebugWindow=NULL;

static List2<ModelessDlgNode> g_listModelessDlgs;

int g_debugVal, g_debugVal2;

////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK DebugDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:
		SetDlgItemInt(hdlg, IDC_EDIT, g_debugVal, FALSE);
		SetDlgItemInt(hdlg, IDC_EDIT2, g_debugVal2, FALSE);
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDC_EDIT:
		case IDC_SPIN:
			{
				BOOL f;
				int v;

				v = GetDlgItemInt(hdlg, IDC_EDIT, &f, FALSE);

				if (f)
					g_debugVal = v;
			}
			break;
		case IDC_EDIT2:
		case IDC_SPIN2:
			{
				BOOL f;
				int v;

				v = GetDlgItemInt(hdlg, IDC_EDIT2, &f, FALSE);

				if (f)
					g_debugVal2 = v;
			}
			break;
		}
		return TRUE;

	case WM_DESTROY:
		g_hwndDebugWindow = NULL;
		return TRUE;
	}
	return FALSE;
}

void guiOpenDebug() {
	if (!g_hwndDebugWindow)
		g_hwndDebugWindow = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_DEBUGVAL), NULL, DebugDlgProc);
}

////////////////////////////////////////////////////////////////////////////

void guiDlgMessageLoop(HWND hDlg) {
	MSG msg;

	while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (!IsWindow(hDlg) || !IsDialogMessage(hDlg, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

bool guiCheckDialogs(LPMSG pMsg) {
	ModelessDlgNode *pmdn, *pmdn_next;

	if (g_hwndJobs && IsDialogMessage(g_hwndJobs, pMsg))
		return true;

	if (g_hwndDebugWindow && IsDialogMessage(g_hwndDebugWindow, pMsg))
		return true;

	pmdn = g_listModelessDlgs.AtHead();

	while(pmdn_next = pmdn->NextFromHead()) {
		if (IsDialogMessage(pmdn->hdlg, pMsg))
			return true;

		pmdn = pmdn_next;
	}

	return false;
}

void guiAddModelessDialog(ModelessDlgNode *pmdn) {
	if (pmdn->hdlg)
		g_listModelessDlgs.AddTail(pmdn);
}

void guiRedoWindows(HWND hWnd) {
	HWND hWndStatus = GetDlgItem(hWnd, IDC_STATUS_WINDOW);
	HWND hWndPosition = GetDlgItem(hWnd, IDC_POSITION);
	RECT rClient, rStatus, rPosition;
	INT aWidth[MAX_STATUS_PARTS];
	int nParts;

	GetClientRect(hWnd, &rClient);
	GetWindowRect(hWndStatus, &rStatus);
	GetWindowRect(hWndPosition, &rPosition);

	SetWindowPos(hWndPosition,
				NULL,
				rClient.left,
				rClient.bottom - (rStatus.bottom-rStatus.top) - (rPosition.bottom-rPosition.top),
				rClient.right-rClient.left,
				rPosition.bottom-rPosition.top,
				SWP_NOACTIVATE|SWP_NOZORDER);

	SetWindowPos(hWndStatus,
				NULL,
				rClient.left,
				rClient.bottom - (rStatus.bottom-rStatus.top),
				rClient.right-rClient.left,
				rStatus.bottom-rStatus.top,
				SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOCOPYBITS);

	if ((nParts = SendMessage(hWndStatus, SB_GETPARTS, 0, 0))>1) {
		int i;
		INT xCoord = (rStatus.right-rStatus.left) - (rStatus.bottom-rStatus.top);

		aWidth[nParts-2] = xCoord;

		for(i=nParts-3; i>=0; i--) {
			xCoord -= 60;
			aWidth[i] = xCoord;
		}
		aWidth[nParts-1] = -1;

		SendMessage(hWndStatus, SB_SETPARTS, nParts, (LPARAM)aWidth);
	}

}

void guiSetStatus(char *format, int nPart, ...) {
	char buf[256];
	va_list val;

	va_start(val, nPart);
	vsprintf(buf, format, val);
	va_end(val);

	SendMessage(GetDlgItem(g_hWnd, IDC_STATUS_WINDOW), SB_SETTEXT, nPart, (LPARAM)buf);
}

void guiSetTitle(HWND hWnd, UINT uID, ...) {
	char buf1[256],buf2[256];
	va_list val;

	LoadString(g_hInst, uID, buf1, sizeof buf1);

	va_start(val, uID);
	vsprintf(buf2, buf1, val);
	va_end(val);

	SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)buf2);
}

void guiMenuHelp(HWND hwnd, WPARAM wParam, WPARAM part, UINT *iTranslator) {
	HWND hwndStatus = GetDlgItem(hwnd, IDC_STATUS_WINDOW);
	char msgbuf[256];

	if (!(HIWORD(wParam) & MF_POPUP) && !(HIWORD(wParam) & MF_SYSMENU)) {
		UINT *idPtr = iTranslator;

		while(idPtr[0]) {
			if (idPtr[0] == LOWORD(wParam)) {
				if (LoadString(g_hInst, idPtr[1], msgbuf, sizeof msgbuf)) {
					SendMessage(hwndStatus, SB_SETTEXT, part, (LPARAM)msgbuf);
					return;
				}
			}
			idPtr += 2;
		}
	}

	SendMessage(hwndStatus, SB_SETTEXT, part, (LPARAM)"");
}

void guiOffsetDlgItem(HWND hdlg, UINT id, LONG xDelta, LONG yDelta) {
	HWND hwndItem;
	RECT r;

	hwndItem = GetDlgItem(hdlg, id);
	GetWindowRect(hwndItem, &r);
	ScreenToClient(hdlg, (LPPOINT)&r);
	SetWindowPos(hwndItem, NULL, r.left + xDelta, r.top + yDelta, 0, 0, SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
}

void guiResizeDlgItem(HWND hdlg, UINT id, LONG x, LONG y, LONG dx, LONG dy) {
	HWND hwndItem;
	DWORD dwFlags = SWP_NOACTIVATE|SWP_NOZORDER;
	RECT r;

	if (!(x|y))
		dwFlags |= SWP_NOMOVE;

	if (!(dx|dy))
		dwFlags |= SWP_NOSIZE;

	hwndItem = GetDlgItem(hdlg, id);
	GetWindowRect(hwndItem, &r);
	ScreenToClient(hdlg, (LPPOINT)&r);
	ScreenToClient(hdlg, (LPPOINT)&r + 1);
	SetWindowPos(hwndItem, NULL,
				r.left + x,
				r.top + y,
				r.right - r.left + dx,
				r.bottom - r.top + dy,
				dwFlags);
}

void guiSubclassWindow(HWND hwnd, WNDPROC newproc) {
	SetWindowLong(hwnd, GWL_USERDATA, GetWindowLong(hwnd, GWL_WNDPROC));
	SetWindowLong(hwnd, GWL_WNDPROC, (LPARAM)newproc);
}

///////////////////////////////////////

extern VideoSource *inputVideoAVI;

void guiPositionInitFromStream(HWND hWndPosition) {
	if (!inputVideoAVI) return;

	SendMessage(hWndPosition, PCM_SETRANGEMIN, (BOOL)FALSE, inputVideoAVI->lSampleFirst);
	SendMessage(hWndPosition, PCM_SETRANGEMAX, (BOOL)TRUE , inputVideoAVI->lSampleLast);
	SendMessage(hWndPosition, PCM_SETFRAMERATE, 0, MulDiv(1000000, inputVideoAVI->streamInfo.dwScale, inputVideoAVI->streamInfo.dwRate));
}

LONG guiPositionHandleCommand(WPARAM wParam, LPARAM lParam) {
	if (!inputVideoAVI) return -1;

	switch(HIWORD(wParam)) {
		case PCN_START:
			SendMessage((HWND)lParam, PCM_SETPOS, (WPARAM)TRUE, inputVideoAVI->lSampleFirst);
			return inputVideoAVI->lSampleFirst;
		case PCN_BACKWARD:
			{
				LONG lSample = SendMessage((HWND)lParam, PCM_GETPOS, 0, 0);

				if (lSample > inputVideoAVI->lSampleFirst) {
					SendMessage((HWND)lParam, PCM_SETPOS, (WPARAM)TRUE, lSample-1);
					return lSample-1;
				}
			}
			break;
		case PCN_FORWARD:
			{
				LONG lSample = SendMessage((HWND)lParam, PCM_GETPOS, 0, 0);

				if (lSample < inputVideoAVI->lSampleLast) {
					SendMessage((HWND)lParam, PCM_SETPOS, (WPARAM)TRUE, lSample+1);
					return lSample+1;
				}
			}
			break;
		case PCN_END:
			SendMessage((HWND)lParam, PCM_SETPOS, (WPARAM)TRUE, inputVideoAVI->lSampleLast);
			return inputVideoAVI->lSampleLast;
			break;

		case PCN_KEYPREV:
			{
				LONG lSample = inputVideoAVI->prevKey(SendMessage((HWND)lParam, PCM_GETPOS, 0, 0));

				if (lSample < 0) lSample = inputVideoAVI->lSampleFirst;

				SendMessage((HWND)lParam, PCM_SETPOS, (WPARAM)TRUE, lSample);
				return lSample;
			}
			break;
		case PCN_KEYNEXT:
			{
				LONG lSample = inputVideoAVI->nextKey(SendMessage((HWND)lParam, PCM_GETPOS, 0, 0));

				if (lSample < 0) lSample = inputVideoAVI->lSampleLast;

				SendMessage((HWND)lParam, PCM_SETPOS, (WPARAM)TRUE, lSample);
				return lSample;
			}
			break;
	}

	return -1;
}

LONG guiPositionHandleNotify(WPARAM wParam, LPARAM lParam) {
	LPNMHDR nmh = (LPNMHDR)lParam;
	LONG pos;

	switch(nmh->code) {
	case PCN_THUMBTRACK:
	case PCN_THUMBPOSITION:
	case PCN_PAGELEFT:
	case PCN_PAGERIGHT:
	case CCN_REFRESHFRAME:
		pos = SendMessage(nmh->hwndFrom, PCM_GETPOS, 0, 0);

		if (inputVideoAVI) return pos;

		break;
	}

	return -1;
}

void guiPositionBlit(HWND hWndClipping, LONG lFrame) {
	if (lFrame<0) return;
	try {
		BITMAPINFOHEADER *dcf;

		dcf = inputVideoAVI->getDecompressedFormat();

		if (lFrame < inputVideoAVI->lSampleFirst || lFrame >= inputVideoAVI->lSampleLast)
			SendMessage(hWndClipping, CCM_BLITFRAME, (WPARAM)NULL, (LPARAM)NULL);
		else
			SendMessage(hWndClipping, CCM_BLITFRAME, (WPARAM)dcf, (LPARAM)inputVideoAVI->getFrame(lFrame));

	} catch(MyError e) {
		_RPT0(0,"Exception!!!\n");
	}
}

bool guiChooseColor(HWND hwnd, COLORREF& rgbOld) {
	CHOOSECOLOR cc;                 // common dialog box structure 

	// Initialize CHOOSECOLOR
	memset(&cc, 0, sizeof(CHOOSECOLOR));
	cc.lStructSize	= sizeof(CHOOSECOLOR);
	cc.hwndOwner	= hwnd;
	cc.lpCustColors	= (LPDWORD)g_crCustomColors;
	cc.rgbResult	= rgbOld;
	cc.Flags		= CC_FULLOPEN | CC_RGBINIT;

	if (ChooseColor(&cc)==TRUE) {;
		rgbOld = cc.rgbResult;
		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////

struct reposInitData {
	struct ReposItem *lpri;
	POINT *lppt;
	HWND hwndParent;
	RECT rParent;
	HDWP hdwp;
};

static BOOL CALLBACK ReposInitFunc(HWND hwnd, LPARAM lParam) {
	const struct reposInitData *rid = (struct reposInitData *)lParam;
	const struct ReposItem *lpri = rid->lpri;
	POINT *lppt = rid->lppt;
	UINT uiID = GetWindowLong(hwnd, GWL_ID);
	RECT rc;

	while(lpri->uiCtlID) {
		if (lpri->uiCtlID == uiID) {
			GetWindowRect(hwnd, &rc);
			ScreenToClient(rid->hwndParent, (POINT *)&rc + 0);
			ScreenToClient(rid->hwndParent, (POINT *)&rc + 1);

			if (lpri->fReposOpts & REPOS_MOVERIGHT)
				lppt->x = rid->rParent.right - rc.left;

			if (lpri->fReposOpts & REPOS_MOVEDOWN)
				lppt->y = rid->rParent.bottom - rc.top;

			if (lpri->fReposOpts & REPOS_SIZERIGHT)
				lppt->x = rid->rParent.right - (rc.right-rc.left);

			if (lpri->fReposOpts & REPOS_SIZEDOWN)
				lppt->y = rid->rParent.bottom - (rc.bottom-rc.top);

			return TRUE;
		}

		++lpri, ++lppt;
	}

	return TRUE;
}

void guiReposInit(HWND hwnd, struct ReposItem *lpri, POINT *lppt) {
	struct reposInitData rid;

	rid.lpri = lpri;
	rid.lppt = lppt;
	rid.hwndParent = hwnd;
	GetClientRect(hwnd, &rid.rParent);

	EnumChildWindows(hwnd, ReposInitFunc, (LPARAM)&rid);
}

static BOOL CALLBACK ReposResizeFunc(HWND hwnd, LPARAM lParam) {
	const struct reposInitData *rid = (struct reposInitData *)lParam;
	const struct ReposItem *lpri = rid->lpri;
	POINT *lppt = rid->lppt;
	UINT uiID = GetWindowLong(hwnd, GWL_ID);
	RECT rc;

	while(lpri->uiCtlID) {
		if (lpri->uiCtlID == uiID) {
			UINT uiFlags;

			GetWindowRect(hwnd, &rc);
			ScreenToClient(rid->hwndParent, (POINT *)&rc + 0);
			ScreenToClient(rid->hwndParent, (POINT *)&rc + 1);

			if (lpri->fReposOpts & REPOS_MOVERIGHT) {
				rc.right -= rc.left;
				rc.left = rid->rParent.right - lppt->x;
				rc.right += rc.left;
			}

			if (lpri->fReposOpts & REPOS_MOVEDOWN) {
				rc.bottom -= rc.top;
				rc.top = rid->rParent.bottom - lppt->y;
				rc.bottom += rc.top;
			}

			if (lpri->fReposOpts & REPOS_SIZERIGHT)
				rc.right = rc.left + rid->rParent.right - lppt->x;

			if (lpri->fReposOpts & REPOS_SIZEDOWN)
				rc.bottom = rc.top + rid->rParent.bottom - lppt->y;

			uiFlags = (lpri->fReposOpts & (REPOS_MOVERIGHT | REPOS_MOVEDOWN) ? 0 : SWP_NOMOVE)
					 |(lpri->fReposOpts & (REPOS_SIZERIGHT | REPOS_SIZEDOWN) ? 0 : SWP_NOSIZE)
					 |SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_NOZORDER;

			if (rid->hdwp)
				DeferWindowPos(rid->hdwp, hwnd, NULL, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, uiFlags);
			else
				SetWindowPos(hwnd, NULL, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, uiFlags);

			return TRUE;
		}

		++lpri, ++lppt;
	}

	return TRUE;
}

void guiReposResize(HWND hwnd, struct ReposItem *lpri, POINT *lppt) {
	struct reposInitData rid;
	int iWindows = 0;

	rid.lpri = lpri;
	rid.lppt = lppt;
	rid.hwndParent = hwnd;
	GetClientRect(hwnd, &rid.rParent);

	while(lpri++->uiCtlID)
		++iWindows;

	rid.hdwp = BeginDeferWindowPos(iWindows);

	EnumChildWindows(hwnd, ReposResizeFunc, (LPARAM)&rid);

	EndDeferWindowPos(rid.hdwp);
}

void guiDeferWindowPos(HDWP hdwp, HWND hwnd, HWND hwndInsertAfter, int x, int y, int dx, int dy, UINT flags) {
	if (hdwp)
		DeferWindowPos(hdwp, hwnd, hwndInsertAfter, x, y, dx, dy, flags);
	else
		SetWindowPos(hwnd, hwndInsertAfter, x, y, dx, dy, flags);
}

void guiEndDeferWindowPos(HDWP hdwp) {
	if (hdwp)
		EndDeferWindowPos(hdwp);
}

int guiMessageBoxF(HWND hwnd, LPCTSTR lpCaption, UINT uType, const char *format, ...) {
	char buf[1024];
	va_list val;

	va_start(val,format);
	vsprintf(buf, format, val);
	va_end(val);

	return MessageBox(hwnd, buf, lpCaption, uType);
}

///////////////////////////////////////////////////////////////////////////

void ticks_to_str(char *dst, DWORD ticks) {
	int sec, min, hr, day;

	ticks /= 1000;
	sec	= ticks %  60; ticks /=  60;
	min	= ticks %  60; ticks /=  60;
	hr	= ticks %  24; ticks /=  24;
	day	= ticks;

	if (day)
		wsprintf(dst,"%d:%02d:%02d:%02d",day,hr,min,sec);
	else if (hr)
		wsprintf(dst,"%d:%02d:%02d",hr,min,sec);
	else
		wsprintf(dst,"%d:%02d",min,sec);
}

void size_to_str(char *dst, __int64 i64Bytes) {
	if (i64Bytes < 65536) {
		sprintf(dst, "%lu bytes", (unsigned long)i64Bytes);
	} else if (i64Bytes < (1L<<24)) {
		sprintf(dst, "%luK", (unsigned long)((i64Bytes+512) / 1024));
	} else if ((unsigned long)i64Bytes == i64Bytes) {
		i64Bytes += 52429;

		sprintf(dst, "%lu.%cMb", (unsigned long)(i64Bytes >> 20), (char)('0' + (( ((unsigned long)i64Bytes & 1048575) * 10)>>20)));
	} else {
		i64Bytes += (long)((1L<<30) / 200);

		sprintf(dst, "%I64u.%02dGb",
				i64Bytes>>30,
				(int)(
					(
						(
							(i64Bytes >> 10) & 1048575
						)*100
					) >> 20
				)
		);
	}
}