//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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
#include <math.h>
#include <vector>

#include "oshelper.h"
#include "fht.h"

#include "VideoDisplay.h"

#include "AVIOutputWAV.h"

extern HINSTANCE g_hInst;

extern const char g_szVideoDisplayControlName[]="birdyVideoDisplayControl";

/////////////////////////////////////////////////////////////////////////////
//
//	VDVideoDisplayControl
//
/////////////////////////////////////////////////////////////////////////////

class VDVideoDisplayControl {
public:
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	VDVideoDisplayControl(HWND hwnd);
	~VDVideoDisplayControl();

	void SetSpectralPaletteDefault();

	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void OnPaint(HDC hdc);
	void OnSize();

	const HWND	mhwnd;
};

ATOM RegisterVideoDisplayControl() {
	WNDCLASS wc;

	wc.style		= 0;
	wc.lpfnWndProc	= VDVideoDisplayControl::StaticWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(VDVideoDisplayControl *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= (HBRUSH)(COLOR_3DFACE+1);		//GetStockObject(LTGRAY_BRUSH);
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= VIDEODISPLAYCONTROLCLASS;

	return RegisterClass(&wc);
}

VDVideoDisplayControl::VDVideoDisplayControl(HWND hwnd)
	: mhwnd(hwnd)
{
}

VDVideoDisplayControl::~VDVideoDisplayControl() {
}

LRESULT CALLBACK VDVideoDisplayControl::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDVideoDisplayControl *pThis = (VDVideoDisplayControl *)GetWindowLong(hwnd, 0);

	if (msg == WM_NCCREATE) {
		pThis = new VDVideoDisplayControl(hwnd);

		if (!pThis)
			return FALSE;

		SetWindowLong(hwnd, 0, (DWORD)pThis);
	} else if (msg == WM_NCDESTROY) {
		delete pThis;
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return pThis->WndProc(msg, wParam, lParam);
}

LRESULT VDVideoDisplayControl::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {

	case WM_CREATE:
		OnSize();
		break;

	case WM_SIZE:
		OnSize();
		return 0;

	case WM_ERASEBKGND:
		return FALSE;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hDC;

			hDC = BeginPaint(mhwnd, &ps);
			OnPaint(hDC);
			EndPaint(mhwnd, &ps);
		}
		return 0;
	}

	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDVideoDisplayControl::OnSize() {
	RECT r;
	GetClientRect(mhwnd, &r);
}

void VDVideoDisplayControl::OnPaint(HDC hdc) {
}
