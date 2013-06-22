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

#define f_CLIPPINGCONTROL_CPP

#include <windows.h>
#include <commctrl.h>
#include <vfw.h>

#include "oshelper.h"

#include "ClippingControl.h"
#include "PositionControl.h"

extern HINSTANCE g_hInst;

static LRESULT APIENTRY ClippingControlWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern const char szClippingControlName[]="birdyClippingControl";

typedef struct ClippingControlData {
	HFONT hFont;
	HDRAWDIB hddFrame;

	LONG lWidth, lHeight;
	LONG xRect, yRect;

	LONG x1,y1,x2,y2;

	BOOL fInhibitRefresh;
} ClippingControlData;

ATOM RegisterClippingControl() {
	WNDCLASS wc;

	wc.style		= 0;
	wc.lpfnWndProc	= ClippingControlWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(ClippingControlData *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= NULL;
	wc.hbrBackground= (HBRUSH)(COLOR_3DFACE+1);		//GetStockObject(LTGRAY_BRUSH);
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= CLIPPINGCONTROLCLASS;

	return RegisterClass(&wc);
}

enum {
	IDC_X1_STATIC	= 500,
	IDC_X1_EDIT		= 501,
	IDC_X1_SPIN		= 502,
	IDC_Y1_STATIC	= 503,
	IDC_Y1_EDIT		= 504,
	IDC_Y1_SPIN		= 505,
	IDC_X2_STATIC	= 506,
	IDC_X2_EDIT		= 507,
	IDC_X2_SPIN		= 508,
	IDC_Y2_STATIC	= 509,
	IDC_Y2_EDIT		= 510,
	IDC_Y2_SPIN		= 511,
	IDC_POSITION	= 512,
};

static BOOL CALLBACK ClippingControlInitChildrenProc(HWND hWnd, LPARAM lParam) {
	SendMessage(hWnd, WM_SETFONT, (WPARAM)lParam, (LPARAM)MAKELONG(FALSE, 0));

	switch(GetWindowLong(hWnd, GWL_ID)) {
	case IDC_X1_STATIC:		SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)"X1 offset");
							break;
	case IDC_Y1_STATIC:		SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)"Y1 offset");
							break;
	case IDC_X2_STATIC:		SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)"X2 offset");
							break;
	case IDC_Y2_STATIC:		SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)"Y2 offset");
							break;
	}

	return TRUE;
}

static BOOL CALLBACK ClippingControlShowChildrenProc(HWND hWnd, LPARAM lParam) {
	ShowWindow(hWnd, SW_SHOW);

	return TRUE;
}

static void ClippingControlDrawRect(HWND hWnd, HDC hDC, ClippingControlData *ccd) {
	HPEN hPenOld;
	RECT r;
	HDC hDCReleaseMe = NULL;

	if (!hDC) hDC = hDCReleaseMe = GetDC(hWnd);

	hPenOld = (HPEN)SelectObject(hDC, GetStockObject(BLACK_PEN));
	Draw3DRect(hDC, ccd->xRect+0, ccd->yRect+0, ccd->lWidth+8, ccd->lHeight+8, FALSE);
	Draw3DRect(hDC, ccd->xRect+3, ccd->yRect+3, ccd->lWidth+2, ccd->lHeight+2, TRUE);

	r.left		= ccd->xRect + 4;
	r.right		= ccd->xRect + 4 + ccd->lWidth;
	if (ccd->y1) {
		r.top		= ccd->yRect + 4;
		r.bottom	= ccd->yRect + 4 + ccd->y1;
		FillRect(hDC, &r, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
	}
	if (ccd->y2) {
		r.top		= ccd->yRect + 4 + ccd->lHeight - ccd->y2;
		r.bottom	= ccd->yRect + 4 + ccd->lHeight;
		FillRect(hDC, &r, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
	}

	r.top		= ccd->yRect + 4 + ccd->y1;
	r.bottom	= ccd->yRect + 4 + ccd->lHeight - ccd->y2;
	if (ccd->x1) {
		r.left		= ccd->xRect + 4;
		r.right		= ccd->xRect + 4 + ccd->x1;
		FillRect(hDC, &r, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
	}
	if (ccd->x2) {
		r.left		= ccd->xRect + 4 + ccd->lWidth - ccd->x2;
		r.right		= ccd->xRect + 4 + ccd->lWidth;
		FillRect(hDC, &r, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
	}

	if (ccd->hddFrame) {
		NMHDR nm;

		nm.hwndFrom = hWnd;
		nm.idFrom = GetWindowLong(hWnd, GWL_ID);
		nm.code = CCN_REFRESHFRAME;

		SendMessage(GetParent(hWnd), WM_NOTIFY, (WPARAM)nm.idFrom, (LPARAM)&nm);
	}

	if (ccd->x1) {
		MoveToEx(hDC, ccd->xRect+3+ccd->x1, ccd->yRect+4, NULL);
		LineTo	(hDC, ccd->xRect+3+ccd->x1, ccd->yRect+3+ccd->lHeight);
	}
	if (ccd->x2) {
		MoveToEx(hDC, ccd->xRect+4+ccd->lWidth-ccd->x2, ccd->yRect+4, NULL);
		LineTo	(hDC, ccd->xRect+4+ccd->lWidth-ccd->x2, ccd->yRect+3+ccd->lHeight);
	}
	if (ccd->y1) {
		MoveToEx(hDC, ccd->xRect+4, ccd->yRect+3+ccd->y1, NULL);
		LineTo	(hDC, ccd->xRect+3+ccd->lWidth, ccd->yRect+3+ccd->y1);
	}
	if (ccd->y2) {
		MoveToEx(hDC, ccd->xRect+4, ccd->yRect+4+ccd->lHeight-ccd->y2, NULL);
		LineTo	(hDC, ccd->xRect+3+ccd->lWidth, ccd->yRect+4+ccd->lHeight-ccd->y2);
	}

	DeleteObject(SelectObject(hDC, hPenOld));

	if (hDCReleaseMe) ReleaseDC(hWnd, hDCReleaseMe);
}

BOOL ClippingControlVerifyParams(HWND hDlg, ClippingControlData *ccd) {
	ccd->x1 = GetDlgItemInt(hDlg, IDC_X1_EDIT, NULL, TRUE);
	ccd->x2 = GetDlgItemInt(hDlg, IDC_X2_EDIT, NULL, TRUE);
	ccd->y1 = GetDlgItemInt(hDlg, IDC_Y1_EDIT, NULL, TRUE);
	ccd->y2 = GetDlgItemInt(hDlg, IDC_Y2_EDIT, NULL, TRUE);

	if (ccd->x1<0) {
		SetFocus(GetDlgItem(hDlg, IDC_X1_EDIT));
		MessageBeep(MB_ICONQUESTION);
		return FALSE;
	}
	if (ccd->y1<0) {
		SetFocus(GetDlgItem(hDlg, IDC_Y1_EDIT));
		MessageBeep(MB_ICONQUESTION);
		return FALSE;
	}
	if (ccd->x2<0) {
		SetFocus(GetDlgItem(hDlg, IDC_X2_EDIT));
		MessageBeep(MB_ICONQUESTION);
		return FALSE;
	}
	if (ccd->y2<0) {
		SetFocus(GetDlgItem(hDlg, IDC_Y2_EDIT));
		MessageBeep(MB_ICONQUESTION);
		return FALSE;
	}

	return TRUE;
}

static LRESULT APIENTRY ClippingControlWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	ClippingControlData *ccd = (ClippingControlData *)GetWindowLong(hWnd, 0);

	if ((msg >= CCM__FIRST && msg < CCM__LAST) || msg == WM_SETTEXT) {
		HWND hWndPosition = GetDlgItem(hWnd, IDC_POSITION);

		if (hWndPosition)
			return SendMessage(hWndPosition, msg, wParam, lParam);
		else
			return 0;
	}

	switch(msg) {

	case WM_NCCREATE:
		{
			HDRAWDIB hddFrame=NULL;

			if ((GetWindowLong(hWnd, GWL_STYLE) & CCS_FRAME) && !(hddFrame = DrawDibOpen()))
				return FALSE;

			if (!(ccd = new ClippingControlData))
				return FALSE;
			memset(ccd,0,sizeof(ClippingControlData));

			ccd->hddFrame = hddFrame;

			SetWindowLong(hWnd, 0, (LONG)ccd);
		} return TRUE;

	case WM_CREATE:
		{
			LONG du, duX, duY;

			ccd->hFont = CreateFont(8, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "MS Sans Serif");

			du = GetDialogBaseUnits();
			duX = LOWORD(du);
			duY = HIWORD(du);

			ccd->xRect = (53*duX)/4;
			ccd->yRect = (14*duY)/8;

			CreateWindowEx(0				,"STATIC"		,NULL,WS_CHILD | SS_LEFT							,( 0*duX)/4, ( 2*duY)/8, (22*duX)/4, ( 8*duY)/8, hWnd, (HMENU)IDC_X1_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,NULL,WS_CHILD | ES_LEFT | ES_NUMBER				,(23*duX)/4, ( 0*duY)/8, (24*duX)/4, (10*duY)/8, hWnd, (HMENU)IDC_X1_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,NULL,WS_CHILD | UDS_AUTOBUDDY | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT, 0, 0,	 ( 2*duX)/4, (10*duY)/8, hWnd, (HMENU)IDC_X1_SPIN	, g_hInst, NULL);
			CreateWindowEx(0				,"STATIC"		,NULL,WS_CHILD | SS_LEFT							,( 0*duX)/4, (16*duY)/8, (22*duX)/4, ( 8*duY)/8, hWnd, (HMENU)IDC_Y1_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,NULL,WS_CHILD | ES_LEFT | ES_NUMBER				,(23*duX)/4, (14*duY)/8, (24*duX)/4, (10*duY)/8, hWnd, (HMENU)IDC_Y1_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,NULL,WS_CHILD | UDS_AUTOBUDDY | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT, 0, 0,	 ( 2*duX)/4, (10*duY)/8, hWnd, (HMENU)IDC_Y1_SPIN	, g_hInst, NULL);
			CreateWindowEx(0				,"STATIC"		,NULL,WS_CHILD | SS_LEFT							,0         , 0         , (22*duX)/4, ( 8*duY)/8, hWnd, (HMENU)IDC_X2_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,NULL,WS_CHILD | ES_LEFT | ES_NUMBER				,0         , 0         , (24*duX)/4, (10*duY)/8, hWnd, (HMENU)IDC_X2_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,NULL,WS_CHILD | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT, 0, 0,	 ( 2*duX)/4, (10*duY)/8, hWnd, (HMENU)IDC_X2_SPIN	, g_hInst, NULL);
			CreateWindowEx(0				,"STATIC"		,NULL,WS_CHILD | SS_LEFT							,0         , 0         , (22*duX)/4, ( 8*duY)/8, hWnd, (HMENU)IDC_Y2_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,NULL,WS_CHILD | ES_LEFT | ES_NUMBER				,0         , 0         , (24*duX)/4, (10*duY)/8, hWnd, (HMENU)IDC_Y2_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,NULL,WS_CHILD | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT, 0, 0,	 ( 2*duX)/4, (10*duY)/8, hWnd, (HMENU)IDC_Y2_SPIN	, g_hInst, NULL);

			if (GetWindowLong(hWnd, GWL_STYLE) & CCS_POSITION)
				CreateWindowEx(0,POSITIONCONTROLCLASS,NULL,WS_CHILD									,0,0,0,64,hWnd,(HMENU)IDC_POSITION,g_hInst,NULL);

			EnumChildWindows(hWnd, (WNDENUMPROC)ClippingControlInitChildrenProc, (LPARAM)ccd->hFont);
		}
		break;

	case WM_DESTROY:
		if (ccd->hddFrame) DrawDibClose(ccd->hddFrame);
		delete ccd;
		SetWindowLong(hWnd, 0, 0);
		break;

	case WM_PAINT:
		if (ccd->lWidth && ccd->lHeight) {
			PAINTSTRUCT ps;
			HDC hDC;

			hDC = BeginPaint(hWnd, &ps);
			ClippingControlDrawRect(hWnd, hDC, ccd);
			EndPaint(hWnd, &ps);
		}
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDC_X1_EDIT:
		case IDC_X2_EDIT:
		case IDC_Y1_EDIT:
		case IDC_Y2_EDIT:
			if (!ccd->fInhibitRefresh)
				if (ClippingControlVerifyParams(hWnd,ccd))
					ClippingControlDrawRect(hWnd,NULL,ccd);
			break;
		case IDC_POSITION:
			SendMessage(GetParent(hWnd), WM_COMMAND, MAKELONG(GetWindowLong(hWnd, GWL_ID), HIWORD(wParam)), (LPARAM)hWnd);
			break;
		}
		break;

	case WM_NOTIFY:
		{
			NMHDR nm = *(NMHDR *)lParam;

			nm.idFrom = GetWindowLong(hWnd, GWL_ID);
			nm.hwndFrom = hWnd;

			SendMessage(GetParent(hWnd), WM_NOTIFY, nm.idFrom, (LPARAM)&nm);
		}
		break;

	case CCM_SETBITMAPSIZE:
		{
			LONG du, duX, duY;
			HWND hWndItem, hWndPosition;

			du = GetDialogBaseUnits();
			duX = LOWORD(du);
			duY = HIWORD(du);

			ccd->lWidth = LOWORD(lParam);
			ccd->lHeight = HIWORD(lParam);

			SetWindowPos(GetDlgItem(hWnd, IDC_X2_STATIC), NULL, ccd->xRect + ccd->lWidth + 8 - (48*duX)/4, (2*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
			SetWindowPos(GetDlgItem(hWnd, IDC_X2_EDIT  ), NULL, ccd->xRect + ccd->lWidth + 8 - (24*duX)/4, (0*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
			SetWindowPos(GetDlgItem(hWnd, IDC_Y2_STATIC), NULL, ( 0*duX)/4, ccd->yRect + 8 + ccd->lHeight - ( 9*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
			SetWindowPos(GetDlgItem(hWnd, IDC_Y2_EDIT  ), NULL, (24*duX)/4, ccd->yRect + 8 + ccd->lHeight - (10*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);

			if (hWndPosition = GetDlgItem(hWnd, IDC_POSITION))
				SetWindowPos(hWndPosition, NULL, 0, ccd->yRect + ccd->lHeight + 8, ccd->xRect + ccd->lWidth + 8, 64, SWP_NOZORDER | SWP_NOACTIVATE);

			SendMessage(GetDlgItem(hWnd, IDC_X1_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(ccd->lWidth,0));
			SendMessage(GetDlgItem(hWnd, IDC_Y1_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(0,ccd->lHeight));

			hWndItem = GetDlgItem(hWnd, IDC_X2_SPIN);
			SendMessage(hWndItem, UDM_SETBUDDY, (WPARAM)GetDlgItem(hWnd, IDC_X2_EDIT), 0);
			SendMessage(hWndItem, UDM_SETRANGE, 0, (LPARAM)MAKELONG(ccd->lWidth,0));

			hWndItem = GetDlgItem(hWnd, IDC_Y2_SPIN);
			SendMessage(hWndItem, UDM_SETBUDDY, (WPARAM)GetDlgItem(hWnd, IDC_Y2_EDIT), 0);
			SendMessage(hWndItem, UDM_SETRANGE, 0, (LPARAM)MAKELONG(ccd->lHeight,0));

			EnumChildWindows(hWnd, (WNDENUMPROC)ClippingControlShowChildrenProc, (LPARAM)ccd->hFont);
//			RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
			SetWindowPos(hWnd, NULL, 0, 0, ccd->xRect + ccd->lWidth + 8, (hWndPosition?64:0) + ccd->yRect + ccd->lHeight + 8, SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOCOPYBITS);
		}
		break;

	case CCM_SETCLIPBOUNDS:
		{
			ClippingControlBounds *ccb = (ClippingControlBounds *)lParam;

			ccd->x1 = ccb->x1;
			ccd->y1 = ccb->y1;
			ccd->x2 = ccb->x2;
			ccd->y2 = ccb->y2;

			ccd->fInhibitRefresh = TRUE;
			SetDlgItemInt(hWnd, IDC_X1_EDIT, ccd->x1, FALSE);
			SetDlgItemInt(hWnd, IDC_Y1_EDIT, ccd->y1, FALSE);
			SetDlgItemInt(hWnd, IDC_X2_EDIT, ccd->x2, FALSE);
			SetDlgItemInt(hWnd, IDC_Y2_EDIT, ccd->y2, FALSE);
			ClippingControlDrawRect(hWnd, NULL,ccd);
			ccd->fInhibitRefresh = FALSE;
		}
		break;

	case CCM_GETCLIPBOUNDS:
		{
			ClippingControlBounds *ccb = (ClippingControlBounds *)lParam;

			ccb->x1 = ccd->x1;
			ccb->y1 = ccd->y1;
			ccb->x2 = ccd->x2;
			ccb->y2 = ccd->y2;
		}
		break;

	case CCM_BLITFRAME:
		{
			HDC hDC;

			if (hDC = GetDC(hWnd)) {

				if (!wParam) {
					RECT r;

					r.left		= ccd->xRect + 4;
					r.top		= ccd->yRect + 4;
					r.right		= ccd->xRect + 4 + ccd->lWidth;
					r.bottom	= ccd->yRect + 4 + ccd->lHeight;
					FillRect(hDC, &r, (HBRUSH)GetClassLong(hWnd,GCL_HBRBACKGROUND));
				} else {
					BITMAPINFOHEADER *dcf = (BITMAPINFOHEADER *)wParam;
					LONG dx = ccd->lWidth  - ccd->x1 - ccd->x2;
					LONG dy = ccd->lHeight - ccd->y1 - ccd->y2;

#if 0
					static char buf[256];

wsprintf(buf, "DrawDibDraw(%08lx, %08lx, %ld, %ld, %ld, %ld, %08lx, %08lx, %ld, %ld, %ld, %ld, %ld)\n",
								ccd->hddFrame,
								hDC,
								ccd->xRect + 4 + ccd->x1, ccd->yRect + 4 + ccd->y1,
								dx,dy,
								dcf,
								(void *)lParam,
								ccd->x1, ccd->y1, 
								dx,dy,
								0);
OutputDebugString(buf);
#endif
					if (dx>0 && dy>0) {
						DrawDibDraw(
								ccd->hddFrame,
								hDC,
								ccd->xRect + 4 + ccd->x1, ccd->yRect + 4 + ccd->y1,
								dx,dy,
								dcf,
								(void *)lParam,
								ccd->x1, ccd->y1, 
								dx,dy,
								0);
					}
				}

				ReleaseDC(hWnd, hDC);
			}
		}
		break;

	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return FALSE;
}
