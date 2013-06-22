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
#include "VideoDisplay.h"

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
	HWND hwnd = CreateWindowEx(0, szClippingControlOverlayName, "", WS_VISIBLE|WS_CHILD|WS_CLIPSIBLINGS, x, y, cx, cy, hwndParent, (HMENU)id, g_hInst, NULL);

	if (hwnd)
		return (VDClippingControlOverlay *)GetWindowLongPtr(hwnd, 0);

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
	VDClippingControlOverlay *pThis = (VDClippingControlOverlay *)GetWindowLongPtr(hwnd, 0);

	switch(msg) {
	case WM_NCCREATE:
		if (!(pThis = new_nothrow VDClippingControlOverlay(hwnd)))
			return FALSE;

		SetWindowLongPtr(hwnd, 0, (LONG_PTR)pThis);
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

		RECT r;
		GetClientRect(mhwnd, &r);
		FillRect(hdc, &r, (HBRUSH)(COLOR_3DFACE+1));

		// draw horizontal lines
		static const int adjust[2]={-1,0};
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

		const int w = r.right - r.left;
		const int h = r.bottom - r.top;

		Draw3DRect(hdc, r.left, r.top, w, h, FALSE);
		Draw3DRect(hdc, r.left+3, r.top+3, w-6, h-6, TRUE);

		EndPaint(mhwnd, &ps);
	}
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

class VDClippingControl : public IVDClippingControl, public IVDVideoDisplayCallback {
public:
	void SetBitmapSize(int w, int h);
	void SetClipBounds(const vdrect32& r);
	void GetClipBounds(vdrect32& r);
	void BlitFrame(const VDPixmap *);

	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void DisplayRequestUpdate(IVDVideoDisplay *pDisp);
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
		IDC_VIDEODISPLAY= 513
	};

	VDClippingControl(HWND hwnd);
	~VDClippingControl();

	static BOOL CALLBACK InitChildrenProc(HWND hwnd, LPARAM lParam);
	static BOOL CALLBACK ShowChildrenProc(HWND hWnd, LPARAM lParam);
	void ResetDisplayBounds();
	bool VerifyParams();
	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	long lWidth, lHeight;
	long xRect, yRect;

	long x1,y1,x2,y2;

	const HWND	mhwnd;

	bool fInhibitRefresh;

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
{
}

VDClippingControl::~VDClippingControl() {
}

IVDClippingControl *VDGetIClippingControl(VDGUIHandle h) {
	return static_cast<IVDClippingControl *>((VDClippingControl *)GetWindowLongPtr((HWND)h, 0));
}

IVDPositionControl *VDGetIPositionControlFromClippingControl(VDGUIHandle h) {
	HWND hwndPosition = GetDlgItem((HWND)h, 512 /* IDC_POSITION */);

	return VDGetIPositionControl((VDGUIHandle)hwndPosition);
}

void VDClippingControl::SetBitmapSize(int w, int h) {
	LONG du, duX, duY;
	HWND hwndItem, hWndPosition;

	du = GetDialogBaseUnits();
	duX = LOWORD(du);
	duY = HIWORD(du);

	lWidth = w;
	lHeight = h;

	SetWindowPos(GetDlgItem(mhwnd, IDC_X2_STATIC), NULL, xRect + lWidth + 8 - (48*duX)/4, (2*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
	SetWindowPos(GetDlgItem(mhwnd, IDC_X2_EDIT  ), NULL, xRect + lWidth + 8 - (24*duX)/4, (0*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
	SetWindowPos(GetDlgItem(mhwnd, IDC_Y2_STATIC), NULL, ( 0*duX)/4, yRect + 8 + lHeight - ( 9*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
	SetWindowPos(GetDlgItem(mhwnd, IDC_Y2_EDIT  ), NULL, (24*duX)/4, yRect + 8 + lHeight - (10*duY)/8, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);

	if (hWndPosition = GetDlgItem(mhwnd, IDC_POSITION))
		SetWindowPos(hWndPosition, NULL, 0, yRect + lHeight + 8, xRect + lWidth + 8, 64, SWP_NOZORDER | SWP_NOACTIVATE);

	SendMessage(GetDlgItem(mhwnd, IDC_X1_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(lWidth,0));
	SendMessage(GetDlgItem(mhwnd, IDC_Y1_SPIN), UDM_SETRANGE, 0, (LPARAM)MAKELONG(0,lHeight));

	hwndItem = GetDlgItem(mhwnd, IDC_X2_SPIN);
	SendMessage(hwndItem, UDM_SETBUDDY, (WPARAM)GetDlgItem(mhwnd, IDC_X2_EDIT), 0);
	SendMessage(hwndItem, UDM_SETRANGE, 0, (LPARAM)MAKELONG(lWidth,0));

	hwndItem = GetDlgItem(mhwnd, IDC_Y2_SPIN);
	SendMessage(hwndItem, UDM_SETBUDDY, (WPARAM)GetDlgItem(mhwnd, IDC_Y2_EDIT), 0);
	SendMessage(hwndItem, UDM_SETRANGE, 0, (LPARAM)MAKELONG(lHeight,0));

	EnumChildWindows(mhwnd, ShowChildrenProc, 0);

	int w2 = xRect + lWidth + 8;
	int h2 = (hWndPosition?64:0) + yRect + lHeight + 8;
	SetWindowPos(mhwnd, NULL, 0, 0, w2, h2, SWP_NOMOVE|SWP_NOACTIVATE|SWP_NOCOPYBITS);

	hwndItem = GetDlgItem(mhwnd, IDC_VIDEODISPLAY);
	ShowWindow(hwndItem, SW_HIDE);

	SetWindowPos(pOverlay->GetHwnd(), NULL, xRect, yRect, lWidth+8, lHeight+8, SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOZORDER);
	pOverlay->SetImageRect(4, 4, lWidth, lHeight);

	ResetDisplayBounds();
}

void VDClippingControl::SetClipBounds(const vdrect32& r) {
	x1 = r.left;
	y1 = r.top;
	x2 = r.right;
	y2 = r.bottom;

	pOverlay->SetBounds(x1, y1, x2, y2);

	fInhibitRefresh = TRUE;
	SetDlgItemInt(mhwnd, IDC_X1_EDIT, x1, FALSE);
	SetDlgItemInt(mhwnd, IDC_Y1_EDIT, y1, FALSE);
	SetDlgItemInt(mhwnd, IDC_X2_EDIT, x2, FALSE);
	SetDlgItemInt(mhwnd, IDC_Y2_EDIT, y2, FALSE);
	fInhibitRefresh = FALSE;

	ResetDisplayBounds();
}

void VDClippingControl::GetClipBounds(vdrect32& r) {
	r.left		= x1;
	r.top		= y1;
	r.right		= x2;
	r.bottom	= y2;
}

void VDClippingControl::BlitFrame(const VDPixmap *px) {
	HWND hwndDisplay = GetDlgItem(mhwnd, IDC_VIDEODISPLAY);
	bool success = false;

	if (px) {
		IVDVideoDisplay *pVD = VDGetIVideoDisplay(hwndDisplay);

		success = pVD->SetSource(true, *px);
	}

	ShowWindow(GetDlgItem(mhwnd, IDC_VIDEODISPLAY), success ? SW_SHOWNA : SW_HIDE);
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

BOOL CALLBACK VDClippingControl::ShowChildrenProc(HWND hWnd, LPARAM lParam) {
	ShowWindow(hWnd, SW_SHOW);

	return TRUE;
}

void VDClippingControl::ResetDisplayBounds() {
	HWND hwndDisplay = GetDlgItem(mhwnd, IDC_VIDEODISPLAY);
	IVDVideoDisplay *pVD = VDGetIVideoDisplay(hwndDisplay);

	if (x1+x2 >= lWidth || y1+y2 >= lHeight)
		ShowWindow(mhwnd, SW_HIDE);
	else {
		ShowWindow(mhwnd, SW_SHOWNA);
		vdrect32 r(x1, y1, lWidth-x2, lHeight-y2);
		pVD->SetSourceSubrect(&r);
		SetWindowPos(hwndDisplay, NULL, xRect+4+x1, yRect+4+y1, lWidth-(x1+x2), lHeight-(y1+y2), SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOZORDER);
	}
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
	VDClippingControl *pThis = (VDClippingControl *)GetWindowLongPtr(hwnd, 0);

	if (msg == WM_NCCREATE) {
		pThis = new VDClippingControl(hwnd);

		if (!pThis)
			return FALSE;

		SetWindowLongPtr(hwnd, 0, (LONG_PTR)pThis);
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

			du = GetDialogBaseUnits();
			duX = LOWORD(du);
			duY = HIWORD(du);

			xRect = (53*duX)/4;
			yRect = (14*duY)/8;

			fInhibitRefresh = true;
			CreateWindowEx(0				,"STATIC"		,NULL,WS_CHILD | SS_LEFT							,( 0*duX)/4, ( 2*duY)/8, (22*duX)/4, ( 8*duY)/8, mhwnd, (HMENU)IDC_X1_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,NULL,WS_TABSTOP | WS_CHILD | ES_LEFT | ES_NUMBER				,(23*duX)/4, ( 0*duY)/8, (24*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_X1_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,NULL,WS_CHILD | UDS_AUTOBUDDY | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT, 0, 0,	 ( 2*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_X1_SPIN	, g_hInst, NULL);
			CreateWindowEx(0				,"STATIC"		,NULL,WS_CHILD | SS_LEFT							,( 0*duX)/4, (16*duY)/8, (22*duX)/4, ( 8*duY)/8, mhwnd, (HMENU)IDC_Y1_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,NULL,WS_TABSTOP | WS_CHILD | ES_LEFT | ES_NUMBER				,(23*duX)/4, (14*duY)/8, (24*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_Y1_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,NULL,WS_CHILD | UDS_AUTOBUDDY | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT, 0, 0,	 ( 2*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_Y1_SPIN	, g_hInst, NULL);
			CreateWindowEx(0				,"STATIC"		,NULL,WS_CHILD | SS_LEFT							,0         , 0         , (22*duX)/4, ( 8*duY)/8, mhwnd, (HMENU)IDC_X2_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,NULL,WS_TABSTOP | WS_CHILD | ES_LEFT | ES_NUMBER				,0         , 0         , (24*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_X2_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,NULL,WS_CHILD | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT	,0         , 0         , ( 2*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_X2_SPIN	, g_hInst, NULL);
			CreateWindowEx(0				,"STATIC"		,NULL,WS_CHILD | SS_LEFT							,0         , 0         , (22*duX)/4, ( 8*duY)/8, mhwnd, (HMENU)IDC_Y2_STATIC	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	, "EDIT"		,NULL,WS_TABSTOP | WS_CHILD | ES_LEFT | ES_NUMBER				,0         , 0         , (24*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_Y2_EDIT	, g_hInst, NULL);
			CreateWindowEx(WS_EX_CLIENTEDGE	,UPDOWN_CLASS	,NULL,WS_CHILD | UDS_ALIGNRIGHT	| UDS_SETBUDDYINT	,0         , 0         , ( 2*duX)/4, (10*duY)/8, mhwnd, (HMENU)IDC_Y2_SPIN	, g_hInst, NULL);
			HWND hwndDisplay = CreateWindowEx(0,       VIDEODISPLAYCONTROLCLASS,NULL,WS_CHILD, 0, 0, 0, 0, mhwnd, (HMENU)IDC_VIDEODISPLAY, g_hInst, NULL);

			if (GetWindowLong(mhwnd, GWL_STYLE) & CCS_POSITION)
				CreateWindowEx(0,POSITIONCONTROLCLASS,NULL,WS_CHILD									,0,0,0,64,mhwnd,(HMENU)IDC_POSITION,g_hInst,NULL);

			EnumChildWindows(mhwnd, InitChildrenProc, 0);

			pOverlay = VDClippingControlOverlay::Create(mhwnd, 0, 0, 0, 0, 0);

			IVDVideoDisplay *pVD = VDGetIVideoDisplay(hwndDisplay);

			pVD->SetCallback(this);
			
			SetWindowPos(hwndDisplay, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOCOPYBITS);
			fInhibitRefresh = false;
		}
		break;

	case WM_ERASEBKGND:
		{
			HBRUSH hbrBackground = (HBRUSH)GetClassLongPtr(mhwnd, GCLP_HBRBACKGROUND);
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

	case WM_SETCURSOR:
		if ((HWND)wParam != mhwnd) {
			SetCursor(LoadCursor(NULL, IDC_ARROW));
			return TRUE;
		}
		break;

	case CCM_SETBITMAPSIZE:
		SetBitmapSize(LOWORD(lParam), HIWORD(lParam));
		break;

	case CCM_SETCLIPBOUNDS:
		{
			ClippingControlBounds *ccb = (ClippingControlBounds *)lParam;

			SetClipBounds(vdrect32(ccb->x1, ccb->y1, ccb->x2, ccb->y2));
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

	case CCM_BLITFRAME2:
		BlitFrame((const VDPixmap *)lParam);
		return 0;
	}

	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDClippingControl::DisplayRequestUpdate(IVDVideoDisplay *pDisp) {
	NMHDR nm;

	nm.hwndFrom = mhwnd;
	nm.idFrom = GetWindowLong(mhwnd, GWL_ID);
	nm.code = CCN_REFRESHFRAME;

	SendMessage(GetParent(mhwnd), WM_NOTIFY, (WPARAM)nm.idFrom, (LPARAM)&nm);
}
