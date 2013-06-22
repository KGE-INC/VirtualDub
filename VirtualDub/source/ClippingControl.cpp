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

#define f_CLIPPINGCONTROL_CPP

#include "stdafx.h"

#include <windows.h>
#include <commctrl.h>
#include <vfw.h>

#include "oshelper.h"

#include "ClippingControl.h"
#include "PositionControl.h"

extern HINSTANCE g_hInst;

static LRESULT APIENTRY ClippingControlWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern const char szClippingControlName[]="birdyClippingControl";
static const char szClippingControlOverlayName[]="birdyClippingControlOverlay";

/////////////////////////////////////////////////////////////////////////////

class VDClippingControlOverlay {
public:
	VDClippingControlOverlay(HWND hwnd);
	~VDClippingControlOverlay() throw();

	static VDClippingControlOverlay *Create(HWND hwndParent, int x, int y, int cx, int cy, UINT id);

	void SetImageRect(int x, int y, int cx, int cy);
	void SetBounds(int x1, int y1, int x2, int y2);

	HWND GetHwnd() const { return mhwnd; }

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
private:
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void OnPaint();
	void OnMouseMove(int x, int y);
	void OnLButtonDown(int x, int y);
	LRESULT OnNcHitTest(int x, int y);
	void PoleHitTest(int& x, int& y);

	const HWND mhwnd;

	int		mX, mY, mWidth, mHeight;

	double	mXBounds[2], mYBounds[2];

	int	mDragPoleX, mDragPoleY;
};

/////////////////////////////////////////////////////////////////////////////

VDClippingControlOverlay::VDClippingControlOverlay(HWND hwnd)
	: mhwnd(hwnd)
	, mX(0)
	, mY(0)
	, mWidth(0)
	, mHeight(0)
	, mDragPoleX(-1)
	, mDragPoleY(-1)
{
	mXBounds[0] = mYBounds[0] = 0.0;
	mXBounds[1] = mYBounds[1] = 1.0;
}

VDClippingControlOverlay::~VDClippingControlOverlay() throw() {
}

VDClippingControlOverlay *VDClippingControlOverlay::Create(HWND hwndParent, int x, int y, int cx, int cy, UINT id) {
	HWND hwnd = CreateWindowEx(WS_EX_TRANSPARENT, szClippingControlOverlayName, "", WS_VISIBLE|WS_CHILD, x, y, cx, cy, hwndParent, (HMENU)id, g_hInst, NULL);

	if (hwnd)
		return (VDClippingControlOverlay *)GetWindowLong(hwnd, 0);

	return NULL;
}

void VDClippingControlOverlay::SetImageRect(int x, int y, int cx, int cy) {
	mX = x;
	mY = y;
	mWidth = cx;
	mHeight = cy;
}

void VDClippingControlOverlay::SetBounds(int x1, int y1, int x2, int y2) {
	mXBounds[0] = x1 / (double)mWidth;
	mXBounds[1] = 1.0 - x2 / (double)mWidth;
	mYBounds[0] = y1 / (double)mHeight;
	mYBounds[1] = 1.0 - y2 / (double)mHeight;

	InvalidateRect(mhwnd, NULL, FALSE);
}

/////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK VDClippingControlOverlay::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDClippingControlOverlay *pThis = (VDClippingControlOverlay *)GetWindowLong(hwnd, 0);

	switch(msg) {
	case WM_NCCREATE:
		if (!(pThis = new_nothrow VDClippingControlOverlay(hwnd)))
			return FALSE;

		SetWindowLong(hwnd, 0, (DWORD)pThis);
		break;

	case WM_NCDESTROY:
		delete pThis;
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return pThis->WndProc(msg, wParam, lParam);
}

LRESULT VDClippingControlOverlay::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_PAINT:
		OnPaint();
		return 0;
	case WM_NCHITTEST:
		return OnNcHitTest((SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
	case WM_MOUSEMOVE:
		OnMouseMove((SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
		return 0;
	case WM_LBUTTONDOWN:
		OnLButtonDown((SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam));
		return 0;
	case WM_LBUTTONUP:
		mDragPoleX = mDragPoleY = -1;
		ReleaseCapture();
		return 0;
	}
	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDClippingControlOverlay::OnPaint() {
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(mhwnd, &ps);

	if (hdc) {
		HGDIOBJ hgoOld = SelectObject(hdc, GetStockObject(BLACK_PEN));
		int i;

		static const int adjust[2]={-1,0};

		// draw horizontal lines
		for(i=0; i<2; ++i) {
			long y = mY + VDRoundToLong(mYBounds[i] * mHeight);

			if (y > mY && y < mY+mHeight) {
				int y2 = y+adjust[i];
				MoveToEx(hdc, mX, y2, NULL);
				LineTo(hdc, mX + mWidth, y2);
			}
		}

		// draw vertical lines
		for(i=0; i<2; ++i) {
			long x = mX + VDRoundToLong(mXBounds[i] * mWidth);

			if (x > mX && x < mX+mWidth) {
				int x2 = x+adjust[i];
				MoveToEx(hdc, x2, mY, NULL);
				LineTo(hdc, x2, mY + mHeight);
			}
		}

		SelectObject(hdc, hgoOld);
	}

	EndPaint(mhwnd, &ps);
}

void VDClippingControlOverlay::OnMouseMove(int x, int y) {
	if (mDragPoleX>=0 || mDragPoleY>=0) {

		VDDEBUG("(%d,%d)\n", x, y);
		if (mDragPoleX>=0) {
			double v = std::min<double>(std::max<double>(0.0, (x-mX) / (double)mWidth), 1.0);
			int i;

			mXBounds[mDragPoleX] = v;

			for(i=mDragPoleX-1; i>=0 && mXBounds[i] > v; --i)
				mXBounds[i] = v;

			for(i=mDragPoleX+1; i<2 && mXBounds[i] < v; ++i)
				mXBounds[i] = v;
		}

		if (mDragPoleY>=0) {
			double v = std::min<double>(std::max<double>(0.0, (y-mY) / (double)mHeight), 1.0);
			int i;

			mYBounds[mDragPoleY] = v;

			for(i=mDragPoleY-1; i>=0 && mYBounds[i] > v; --i)
				mYBounds[i] = v;

			for(i=mDragPoleY+1; i<2 && mYBounds[i] < v; ++i)
				mYBounds[i] = v;
		}

		RECT r;
		HWND hwndParent = GetParent(mhwnd);
		GetWindowRect(mhwnd, &r);
		MapWindowPoints(NULL, hwndParent, (LPPOINT)&r, 2);
		ClippingControlBounds ccb;

		ccb.x1 = VDRoundToLong(mWidth * mXBounds[0]);
		ccb.x2 = VDRoundToLong(mWidth * (1.0 - mXBounds[1]));
		ccb.y1 = VDRoundToLong(mHeight * mYBounds[0]);
		ccb.y2 = VDRoundToLong(mHeight * (1.0 - mYBounds[1]));

		VDDEBUG("(%d,%d)-(%d,%d) %f\n", ccb.x1, ccb.y1, ccb.x2, ccb.y2, mXBounds[0] * mWidth);

		SendMessage(hwndParent, CCM_SETCLIPBOUNDS, 0, (LPARAM)&ccb);

		InvalidateRect(hwndParent, &r, TRUE);
	} else {
		PoleHitTest(x, y);

		static const LPCTSTR sCursor[3][3]={
			{ IDC_ARROW,  IDC_SIZENS, IDC_SIZENS },
			{ IDC_SIZEWE, IDC_SIZENWSE, IDC_SIZENESW },
			{ IDC_SIZEWE, IDC_SIZENESW, IDC_SIZENWSE },
		};

		SetCursor(LoadCursor(NULL, sCursor[x+1][y+1]));
	}
}

void VDClippingControlOverlay::OnLButtonDown(int x, int y) {
	PoleHitTest(x, y);

	mDragPoleX = x;
	mDragPoleY = y;

	if (x>=0 || y>=0)
		SetCapture(mhwnd);
}

LRESULT VDClippingControlOverlay::OnNcHitTest(int x, int y) {
	if (mDragPoleX >= 0 || mDragPoleY >= 0)
		return HTCLIENT;

	POINT pt = {x, y};
	ScreenToClient(mhwnd, &pt);
	x = pt.x;
	y = pt.y;
	PoleHitTest(x, y);

	return (x&y) < 0 ? HTTRANSPARENT : HTCLIENT;
}

void VDClippingControlOverlay::PoleHitTest(int& x, int& y) {
	double xf = (x - mX) / (double)mWidth;
	double yf = (y - mY) / (double)mHeight;
	int xi, yi;

	xi = -1;
	while(xi < 1 && mXBounds[xi+1] <= xf)
		++xi;

	yi = -1;
	while(yi < 1 && mYBounds[yi+1] <= yf)
		++yi;

	// [xi, xi+1] and [yi, yi+1] now bound the cursor.  If the cursor is right
	// of the midpoint, select the second pole.

	if (xi<1 && (xi<0 || xf > 0.5 * (mXBounds[xi] + mXBounds[xi+1])))
		++xi;

	if (yi<1 && (yi<0 || yf > 0.5 * (mYBounds[yi] + mYBounds[yi+1])))
		++yi;

	if (fabs(mXBounds[xi] - xf) * mWidth > 5)
		xi = -1;

	if (fabs(mYBounds[yi] - yf) * mHeight > 5)
		yi = -1;

	x = xi;
	y = yi;
}

/////////////////////////////////////////////////////////////////////////////
//
//	VDClippingControl
//
/////////////////////////////////////////////////////////////////////////////

class VDClippingControl {
public:

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
private:
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

	VDClippingControl(HWND hwnd);
	~VDClippingControl();

	static BOOL CALLBACK InitChildrenProc(HWND hwnd, LPARAM lParam);
	void DrawRect(HDC hDC);
	bool VerifyParams();
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	long lWidth, lHeight;
	long xRect, yRect;

	long x1,y1,x2,y2;

	const HWND	mhwnd;

	bool fInhibitRefresh;

	HFONT hFont;
	HDRAWDIB hddFrame;
	VDClippingControlOverlay *pOverlay;
};

ATOM RegisterClippingControl() {
	WNDCLASS wc;

	wc.style		= 0;
	wc.lpfnWndProc	= VDClippingControlOverlay::StaticWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(VDClippingControlOverlay *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= NULL;
	wc.hbrBackground= NULL;
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= szClippingControlOverlayName;

	if (!RegisterClass(&wc))
		return 0;

	wc.style		= 0;
	wc.lpfnWndProc	= VDClippingControl::StaticWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(VDClippingControl *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= (HBRUSH)(COLOR_3DFACE+1);		//GetStockObject(LTGRAY_BRUSH);
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= CLIPPINGCONTROLCLASS;

	return RegisterClass(&wc);
}

VDClippingControl::VDClippingControl(HWND hwnd)
	: mhwnd(hwnd)
	, hddFrame(DrawDibOpen())
{
}

VDClippingControl::~VDClippingControl() {
	if (hddFrame)
		DrawDibClose(hddFrame);
}

BOOL CALLBACK VDClippingControl::InitChildrenProc(HWND hwnd, LPARAM lParam) {
	SendMessage(hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), (LPARAM)MAKELONG(FALSE, 0));

	switch(GetWindowLong(hwnd, GWL_ID)) {
	case IDC_X1_STATIC:		SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)"X1 offset");
							break;
	case IDC_Y1_STATIC:		SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)"Y1 offset");
							break;
	case IDC_X2_STATIC:		SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)"X2 offset");
							break;
	case IDC_Y2_STATIC:		SendMessage(hwnd, WM_SETTEXT, 0, (LPARAM)"Y2 offset");
							break;
	}

	return TRUE;
}

static BOOL CALLBACK ClippingControlShowChildrenProc(HWND hWnd, LPARAM lParam) {
	ShowWindow(hWnd, SW_SHOW);

	return TRUE;
}

void VDClippingControl::DrawRect(HDC hDC) {
	RECT r;
	HDC hDCReleaseMe = NULL;

	if (!hDC) hDC = hDCReleaseMe = GetDC(mhwnd);

	Draw3DRect(hDC, xRect+0, yRect+0, lWidth+8, lHeight+8, FALSE);
	Draw3DRect(hDC, xRect+3, yRect+3, lWidth+2, lHeight+2, TRUE);

	r.left		= xRect + 4;
	r.right		= xRect + 4 + lWidth;
	if (y1) {
		r.top		= yRect + 4;
		r.bottom	= yRect + 4 + y1;
		FillRect(hDC, &r, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
	}
	if (y2) {
		r.top		= yRect + 4 + lHeight - y2;
		r.bottom	= yRect + 4 + lHeight;
		FillRect(hDC, &r, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
	}

	r.top		= yRect + 4 + y1;
	r.bottom	= yRect + 4 + lHeight - y2;
	if (x1) {
		r.left		= xRect + 4;
		r.right		= xRect + 4 + x1;
		FillRect(hDC, &r, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
	}
	if (x2) {
		r.left		= xRect + 4 + lWidth - x2;
		r.right		= xRect + 4 + lWidth;
		FillRect(hDC, &r, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
	}

	if (hddFrame) {
		NMHDR nm;

		nm.hwndFrom = mhwnd;
		nm.idFrom = GetWindowLong(mhwnd, GWL_ID);
		nm.code = CCN_REFRESHFRAME;

		SendMessage(GetParent(mhwnd), WM_NOTIFY, (WPARAM)nm.idFrom, (LPARAM)&nm);
	}

	if (hDCReleaseMe)
		ReleaseDC(mhwnd, hDCReleaseMe);
}

bool VDClippingControl::VerifyParams() {
	x1 = GetDlgItemInt(mhwnd, IDC_X1_EDIT, NULL, TRUE);
	x2 = GetDlgItemInt(mhwnd, IDC_X2_EDIT, NULL, TRUE);
	y1 = GetDlgItemInt(mhwnd, IDC_Y1_EDIT, NULL, TRUE);
	y2 = GetDlgItemInt(mhwnd, IDC_Y2_EDIT, NULL, TRUE);

	if (x1<0) {
		SetFocus(GetDlgItem(mhwnd, IDC_X1_EDIT));
		MessageBeep(MB_ICONQUESTION);
		return false;
	}
	if (y1<0) {
		SetFocus(GetDlgItem(mhwnd, IDC_Y1_EDIT));
		MessageBeep(MB_ICONQUESTION);
		return false;
	}
	if (x2<0) {
		SetFocus(GetDlgItem(mhwnd, IDC_X2_EDIT));
		MessageBeep(MB_ICONQUESTION);
		return false;
	}
	if (y2<0) {
		SetFocus(GetDlgItem(mhwnd, IDC_Y2_EDIT));
		MessageBeep(MB_ICONQUESTION);
		return false;
	}

	return true;
}

LRESULT CALLBACK VDClippingControl::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDClippingControl *pThis = (VDClippingControl *)GetWindowLong(hwnd, 0);

	if (msg == WM_NCCREATE) {
		pThis = new VDClippingControl(hwnd);

		if (!pThis)
			return FALSE;

		SetWindowLong(hwnd, 0, (DWORD)pThis);
	} else if (msg == WM_NCDESTROY) {
		delete pThis;
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return pThis->WndProc(msg, wParam, lParam);
}

LRESULT VDClippingControl::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	if ((msg >= CCM__FIRST && msg < CCM__LAST) || msg == WM_SETTEXT) {
		HWND hWndPosition = GetDlgItem(mhwnd, IDC_POSITION);

		if (hWndPosition)
			return SendMessage(hWndPosition, msg, wParam, lParam);
		else
			return 0;
	}

	switch(msg) {

	case WM_CREATE:
		{
			LONG du, duX, duY;

			hFont = CreateFont(8, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "MS Sans Serif");

			du = GetDialogBaseUnits();
			duX = LOWORD(du);
			duY = HIWORD(du);

			xRect = (53*duX)/4;
			yRect = (14*duY)/8;

			fInhibitRefresh = true;
			CreateWindowEx(0				,"STATIC"		,NULL,WS_CHILD | SS_LEFT							,( 0*duX)/4, ( 2*duY)/8, (22*duX)/4, ( 8*duY)/8, mhwnd, (HMENU)IDC_X1_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,NULL,WS_CHILD | ES_LEFT | ES_NUMBER				,(23*duX)/4, ( 0*duY)/8, (24*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_X1_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,NULL,WS_CHILD | UDS_AUTOBUDDY | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT, 0, 0,	 ( 2*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_X1_SPIN	, g_hInst, NULL);
			CreateWindowEx(0				,"STATIC"		,NULL,WS_CHILD | SS_LEFT							,( 0*duX)/4, (16*duY)/8, (22*duX)/4, ( 8*duY)/8, mhwnd, (HMENU)IDC_Y1_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,NULL,WS_CHILD | ES_LEFT | ES_NUMBER				,(23*duX)/4, (14*duY)/8, (24*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_Y1_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,NULL,WS_CHILD | UDS_AUTOBUDDY | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT, 0, 0,	 ( 2*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_Y1_SPIN	, g_hInst, NULL);
			CreateWindowEx(0				,"STATIC"		,NULL,WS_CHILD | SS_LEFT							,0         , 0         , (22*duX)/4, ( 8*duY)/8, mhwnd, (HMENU)IDC_X2_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,NULL,WS_CHILD | ES_LEFT | ES_NUMBER				,0         , 0         , (24*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_X2_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,NULL,WS_CHILD | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT, 0, 0,	 ( 2*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_X2_SPIN	, g_hInst, NULL);
			CreateWindowEx(0				,"STATIC"		,NULL,WS_CHILD | SS_LEFT							,0         , 0         , (22*duX)/4, ( 8*duY)/8, mhwnd, (HMENU)IDC_Y2_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,NULL,WS_CHILD | ES_LEFT | ES_NUMBER				,0         , 0         , (24*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_Y2_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,NULL,WS_CHILD | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT, 0, 0,	 ( 2*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_Y2_SPIN	, g_hInst, NULL);

			if (GetWindowLong(mhwnd, GWL_STYLE) & CCS_POSITION)
				CreateWindowEx(0,POSITIONCONTROLCLASS,NULL,WS_CHILD									,0,0,0,64,mhwnd,(HMENU)IDC_POSITION,g_hInst,NULL);

			EnumChildWindows(mhwnd, InitChildrenProc, 0);

			pOverlay = VDClippingControlOverlay::Create(mhwnd, 0, 0, 0, 0, 0);
			fInhibitRefresh = false;
		}
		break;

	case WM_ERASEBKGND:
		{
			HBRUSH hbrBackground = (HBRUSH)GetClassLong(mhwnd, GCL_HBRBACKGROUND);
			HDC hdc = (HDC)wParam;
			RECT r, r2;

			GetClientRect(mhwnd, &r);

			r2 = r;
			r2.bottom = yRect;
			FillRect(hdc, &r2, hbrBackground);

			r2 = r;
			r2.top = yRect;
			r2.bottom = yRect + lHeight + 8;
			r2.right = xRect;
			FillRect(hdc, &r2, hbrBackground);

			r2.left = xRect + lWidth + 8;
			r2.right = r.right;
			FillRect(hdc, &r2, hbrBackground);

			r2 = r;
			r2.top = yRect + lHeight + 8;
			FillRect(hdc, &r2, hbrBackground);
		}
		return TRUE;

	case WM_PAINT:
		if (lWidth && lHeight) {
			PAINTSTRUCT ps;
			HDC hDC;

			hDC = BeginPaint(mhwnd, &ps);
			DrawRect(hDC);
			EndPaint(mhwnd, &ps);
		}
		return 0;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDC_X1_EDIT:
		case IDC_X2_EDIT:
		case IDC_Y1_EDIT:
		case IDC_Y2_EDIT:
			if (!fInhibitRefresh)
				if (VerifyParams()) {
					RECT r;
					r.left		= xRect + 4;
					r.top		= yRect + 4;
					r.right		= r.left + lWidth;
					r.bottom	= r.top + lHeight;
					InvalidateRect(mhwnd, &r, TRUE);

					pOverlay->SetBounds(x1, y1, x2, y2);
				}
			return 0;
		case IDC_POSITION:
			SendMessage(GetParent(mhwnd), WM_COMMAND, MAKELONG(GetWindowLong(mhwnd, GWL_ID), HIWORD(wParam)), (LPARAM)mhwnd);
			return 0;
		}
		break;

	case WM_NOTIFY:
		{
			NMHDR nm = *(NMHDR *)lParam;

			nm.idFrom = GetWindowLong(mhwnd, GWL_ID);
			nm.hwndFrom = mhwnd;

			SendMessage(GetParent(mhwnd), WM_NOTIFY, nm.idFrom, (LPARAM)&nm);
		}
		return 0;

	case CCM_SETBITMAPSIZE:
		{
			LONG du, duX, duY;
			HWND hWndItem, hWndPosition;

			du = GetDialogBaseUnits();
			duX = LOWORD(du);
			duY = HIWORD(du);

			lWidth = LOWORD(lParam);
			lHeight = HIWORD(lParam);

			SetWindowPos(GetDlgItem(mhwnd, IDC_X2_STATIC), NULL, xRect + lWidth + 8 - (48*duX)/4, (2*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
			SetWindowPos(GetDlgItem(mhwnd, IDC_X2_EDIT  ), NULL, xRect + lWidth + 8 - (24*duX)/4, (0*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
			SetWindowPos(GetDlgItem(mhwnd, IDC_Y2_STATIC), NULL, ( 0*duX)/4, yRect + 8 + lHeight - ( 9*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
			SetWindowPos(GetDlgItem(mhwnd, IDC_Y2_EDIT  ), NULL, (24*duX)/4, yRect + 8 + lHeight - (10*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);

			if (hWndPosition = GetDlgItem(mhwnd, IDC_POSITION))
				SetWindowPos(hWndPosition, NULL, 0, yRect + lHeight + 8, xRect + lWidth + 8, 64, SWP_NOZORDER | SWP_NOACTIVATE);

			SendMessage(GetDlgItem(mhwnd, IDC_X1_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(lWidth,0));
			SendMessage(GetDlgItem(mhwnd, IDC_Y1_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(0,lHeight));

			hWndItem = GetDlgItem(mhwnd, IDC_X2_SPIN);
			SendMessage(hWndItem, UDM_SETBUDDY, (WPARAM)GetDlgItem(mhwnd, IDC_X2_EDIT), 0);
			SendMessage(hWndItem, UDM_SETRANGE, 0, (LPARAM)MAKELONG(lWidth,0));

			hWndItem = GetDlgItem(mhwnd, IDC_Y2_SPIN);
			SendMessage(hWndItem, UDM_SETBUDDY, (WPARAM)GetDlgItem(mhwnd, IDC_Y2_EDIT), 0);
			SendMessage(hWndItem, UDM_SETRANGE, 0, (LPARAM)MAKELONG(lHeight,0));

			EnumChildWindows(mhwnd, (WNDENUMPROC)ClippingControlShowChildrenProc, 0);

			int w = xRect + lWidth + 8;
			int h = (hWndPosition?64:0) + yRect + lHeight + 8;
			SetWindowPos(mhwnd, NULL, 0, 0, w, h, SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOCOPYBITS);

			SetWindowPos(pOverlay->GetHwnd(), NULL, xRect, yRect, lWidth+8, lHeight+8, SWP_NOACTIVATE|SWP_NOCOPYBITS);
			pOverlay->SetImageRect(4, 4, lWidth, lHeight);
		}
		break;

	case CCM_SETCLIPBOUNDS:
		{
			ClippingControlBounds *ccb = (ClippingControlBounds *)lParam;

			x1 = ccb->x1;
			y1 = ccb->y1;
			x2 = ccb->x2;
			y2 = ccb->y2;

			pOverlay->SetBounds(x1, y1, x2, y2);

			fInhibitRefresh = TRUE;
			SetDlgItemInt(mhwnd, IDC_X1_EDIT, x1, FALSE);
			SetDlgItemInt(mhwnd, IDC_Y1_EDIT, y1, FALSE);
			SetDlgItemInt(mhwnd, IDC_X2_EDIT, x2, FALSE);
			SetDlgItemInt(mhwnd, IDC_Y2_EDIT, y2, FALSE);
			DrawRect(NULL);
			fInhibitRefresh = FALSE;
		}
		return 0;

	case CCM_GETCLIPBOUNDS:
		{
			ClippingControlBounds *ccb = (ClippingControlBounds *)lParam;

			ccb->x1 = x1;
			ccb->y1 = y1;
			ccb->x2 = x2;
			ccb->y2 = y2;
		}
		return 0;

	case CCM_BLITFRAME:
		{
			HDC hDC;

			if (hDC = GetDC(mhwnd)) {

				if (!wParam || !hddFrame) {
					RECT r;

					r.left		= xRect + 4;
					r.top		= yRect + 4;
					r.right		= xRect + 4 + lWidth;
					r.bottom	= yRect + 4 + lHeight;
					FillRect(hDC, &r, (HBRUSH)(COLOR_3DFACE+1));
				} else {
					BITMAPINFOHEADER *dcf = (BITMAPINFOHEADER *)wParam;
					LONG dx = lWidth  - x1 - x2;
					LONG dy = lHeight - y1 - y2;

#if 0
					static char buf[256];

wsprintf(buf, "DrawDibDraw(%08lx, %08lx, %ld, %ld, %ld, %ld, %08lx, %08lx, %ld, %ld, %ld, %ld, %ld)\n",
								hddFrame,
								hDC,
								xRect + 4 + x1, yRect + 4 + y1,
								dx,dy,
								dcf,
								(void *)lParam,
								x1, y1, 
								dx,dy,
								0);
OutputDebugString(buf);
#endif
					if (dx>0 && dy>0) {
						DrawDibDraw(
								hddFrame,
								hDC,
								xRect + 4 + x1, yRect + 4 + y1,
								dx,dy,
								dcf,
								(void *)lParam,
								x1, y1, 
								dx,dy,
								0);
					}
				}

				ReleaseDC(mhwnd, hDC);
			}
		}
		break;
	}

	return DefWindowProc(mhwnd, msg, wParam, lParam);
}
