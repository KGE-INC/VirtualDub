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

#define f_HEXVIEWER_CPP

#define _WIN32_WINNT 0x0500

#include <stdio.h>
#include <crtdbg.h>

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include "resource.h"
#include "oshelper.h"
#include "gui.h"
#include "error.h"

#include "HexViewer.h"
#include "ProgressDialog.h"

extern HINSTANCE g_hInst;

static LRESULT APIENTRY HexViewerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

////////////////////////////

extern const char szHexViewerClassName[]="birdyHexViewer";

////////////////////////////

class HexViewer {
public:
	const HWND	hwnd;
	HANDLE	hFile;
	__int64	i64TopOffset;
	__int64 i64FileSize;
	int		nLineHeight;
	int		nLineLimit;
	int		nCurrentVisLines;
	int		iMouseWheelDelta;

	HexViewer(HWND);
	~HexViewer();

	void Init();
	void Open();
	void Open(const char *pszFile);
	void Close();

	void ScrollTopTo(long lLine);

	LRESULT Handle_WM_COMMAND(WPARAM wParam, LPARAM lParam);
	LRESULT Handle_WM_PAINT(WPARAM wParam, LPARAM lParam);
	LRESULT Handle_WM_VSCROLL(WPARAM wParam, LPARAM lParam);
	LRESULT Handle_WM_SIZE(WPARAM wParam, LPARAM lParam);
	LRESULT Handle_WM_MOUSEWHEEL(WPARAM wParam, LPARAM lParam);
	LRESULT Handle_WM_KEYDOWN(WPARAM wParam, LPARAM lParam);

	static BOOL CALLBACK AskForValuesDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
	bool AskForValues(const char *title, const char *name1, const char *name2, __int64& default1, __int64& default2, int (HexViewer::*verifier)(HWND hdlg, __int64 v1, __int64 v2));
	int JumpVerifier(HWND hdlg, __int64 v1, __int64 v2);
	int ExtractVerifier(HWND hdlg, __int64 v1, __int64 v2);
	int TruncateVerifier(HWND hdlg, __int64 v1, __int64 v2);

	void Extract();
};

////////////////////////////

HexViewer::HexViewer(HWND _hwnd) : hwnd(_hwnd) {
	hFile = INVALID_HANDLE_VALUE;

	i64TopOffset = i64FileSize = 0;
	iMouseWheelDelta = 0;
}

HexViewer::~HexViewer() {
	if (hFile != INVALID_HANDLE_VALUE)
		CloseHandle(hFile);
}

void HexViewer::Init() {
	HDC hdc;

	nLineHeight = 16;
	if (hdc = GetDC(hwnd)) {
		TEXTMETRIC tm;
		HGDIOBJ hfOld;

		hfOld = SelectObject(hdc, GetStockObject(ANSI_FIXED_FONT));

		GetTextMetrics(hdc, &tm);
		nLineHeight = tm.tmHeight;

		DeleteObject(SelectObject(hdc, hfOld));

		ReleaseDC(hwnd, hdc);
	}
}

void HexViewer::Open() {
	char szName[MAX_PATH];
	OPENFILENAME ofn;

	szName[0] = 0;

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= hwnd;
	ofn.lpstrFilter			= "All files (*.*)\0*.*\0";
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= szName;
	ofn.nMaxFile			= sizeof szName;
	ofn.lpstrFileTitle		= NULL;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= NULL;
	ofn.Flags				= OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;
	ofn.lpstrDefExt			= NULL;

	if (GetOpenFileName(&ofn))
		Open(szName);
}

void HexViewer::Open(const char *pszFile) {
	Close();

	hFile = CreateFile(pszFile, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		MyWin32Error("Cannot open file: %%s", GetLastError()).post(hwnd, "Hex viewer error");
		return;
	}

	char buf[512];

	wsprintf(buf, "VirtualDub Hex Viewer - [%s]", pszFile);
	SetWindowText(hwnd, buf);

	DWORD dwLow, dwHigh;
	
	dwLow = GetFileSize(hFile, &dwHigh);

	i64FileSize = dwLow | ((__int64)dwHigh << 32);

	SetScrollRange(hwnd, SB_VERT, 0, i64FileSize>>4, TRUE);

	i64TopOffset	= 0;
	nLineLimit		= ((i64FileSize+15)>>4);
	InvalidateRect(hwnd, NULL, TRUE);
	UpdateWindow(hwnd);
}

void HexViewer::Close() {
	if (hFile == INVALID_HANDLE_VALUE)
		return;

	CloseHandle(hFile);
}

void HexViewer::ScrollTopTo(long lLine) {
	HDC hdc;
	RECT rRedraw;

	if (lLine < 0)
		lLine = 0;

	if (lLine > nLineLimit)
		lLine = nLineLimit;

	long delta = lLine - (long)(i64TopOffset>>4);

	if (!delta)
		return;

	iMouseWheelDelta = 0;

	SetScrollPos(hwnd, SB_VERT, lLine, TRUE);
	i64TopOffset = (__int64)lLine<<4;

	if (abs(delta) > nCurrentVisLines) {
		InvalidateRect(hwnd, NULL, TRUE);
		return;
	}

	if (hdc = GetDC(hwnd)) {
		ScrollDC(hdc, 0, -delta*nLineHeight, NULL, NULL, NULL, &rRedraw);
		InvalidateRect(hwnd, &rRedraw, TRUE);
		UpdateWindow(hwnd);

		ReleaseDC(hwnd, hdc);
	}
}

LRESULT HexViewer::Handle_WM_COMMAND(WPARAM wParam, LPARAM lParam) {
	switch(LOWORD(wParam)) {
	case ID_FILE_EXIT:
		CloseWindow(hwnd);
		break;
	case ID_FILE_CLOSE:
		Close();
		break;
	case ID_FILE_OPEN:
		Open();
		break;
	case ID_EDIT_JUMP:
		{
			__int64 v1 = i64TopOffset, v2;

			if (AskForValues("Jump to address", "Address:", NULL, v1, v2, JumpVerifier))
				ScrollTopTo(v1>>4);
		}
		break;
	case ID_EDIT_TRUNCATE:
		{
			__int64 v1 = i64TopOffset, v2;

			if (AskForValues("Truncate file", "Address:", NULL, v1, v2, TruncateVerifier)) {
				if ((0xFFFFFFFF==SetFilePointer(hFile, (LONG)v1, (LONG *)&v1 + 1, FILE_BEGIN) && GetLastError()!=NO_ERROR)
						|| !SetEndOfFile(hFile))
					MyWin32Error("Cannot truncate file: %%s", GetLastError()).post(hwnd, "Error");

				i64FileSize = v1;

				ScrollTopTo(i64TopOffset>>4);
			}
		}
		break;

	case ID_EDIT_EXTRACT:
		Extract();
		break;

	case ID_HELP_WHY:
		MessageBox(hwnd,
			"I need a quick way for people to send me parts of files that don't load properly "
			"in VirtualDub, and this is a handy way to do it. Well, that, and it's annoying to "
			"check 3Gb AVI files if your hex editor tries to load the file into memory.",
			"Why is there a hex viewer in VirtualDub?",
			MB_OK);
		break;

	}

	return 0;
}

static const char hexdig[]="0123456789ABCDEF";

LRESULT HexViewer::Handle_WM_PAINT(WPARAM wParam, LPARAM lParam) {
	HDC hdc;
	PAINTSTRUCT ps;
	char buf[128];
	__int64 i64Offset;
	int y;
	RECT r;
	int i;

	hdc = BeginPaint(hwnd, &ps);

	i = GetClipBox(hdc, &r);

	if (i != ERROR && i != NULLREGION) {
		char data[16];
		DWORD dwActual;
		HGDIOBJ hfOld;

		hfOld = SelectObject(hdc, GetStockObject(ANSI_FIXED_FONT));

		SetTextAlign(hdc, TA_TOP | TA_LEFT);

		y = r.top - r.top % nLineHeight;
		i64Offset = i64TopOffset + (y/nLineHeight)*16;

		SetFilePointer(hFile, (LONG)i64Offset, (LONG *)&i64Offset + 1, FILE_BEGIN);

		while(y < r.bottom+nLineHeight-1 && i64Offset < i64FileSize) {
			if (ReadFile(hFile, data, 16, &dwActual, NULL) && dwActual>0) {
				char *s;
				int i;

				s = buf + sprintf(buf, "%12I64X: ", i64Offset);

				for(i=0; i<dwActual; i++) {
					*s++ = hexdig[(unsigned char)data[i] >> 4];
					*s++ = hexdig[data[i] & 15];
					*s++ = ' ';
				}

				while(i<16) {
					*s++ = ' ';
					*s++ = ' ';
					*s++ = ' ';
					++i;
				}

				s[-8*3-1] = '-';
				*s++ = ' ';

				for(i=0; i<dwActual; i++) {
					if (data[i]>=0x20 && data[i]<0x7f)
						*s++ = data[i];
					else
						*s++ = '.';
				}

				TextOut(hdc, 0, y, buf, s - buf);
			}

			y += nLineHeight;
			i64Offset += 16;
		}

		DeleteObject(SelectObject(hdc, hfOld));
	}
	EndPaint(hwnd, &ps);

	return 0;
}

LRESULT HexViewer::Handle_WM_VSCROLL(WPARAM wParam, LPARAM lParam) {
	SCROLLINFO si;

	switch(LOWORD(wParam)) {
	case SB_BOTTOM:		ScrollTopTo(nLineLimit); break;
	case SB_TOP:		ScrollTopTo(0); break;
	case SB_LINEUP:		ScrollTopTo((long)(i64TopOffset>>4) - 1); break;
	case SB_LINEDOWN:	ScrollTopTo((long)(i64TopOffset>>4) + 1); break;
	case SB_PAGEUP:		ScrollTopTo((long)(i64TopOffset>>4) - (nCurrentVisLines - 1)); break;
	case SB_PAGEDOWN:	ScrollTopTo((long)(i64TopOffset>>4) + (nCurrentVisLines - 1)); break;
	case SB_THUMBTRACK:
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_TRACKPOS;
		GetScrollInfo(hwnd, SB_VERT, &si);
		ScrollTopTo(si.nTrackPos);
		break;
	}

	return 0;
}

LRESULT HexViewer::Handle_WM_SIZE(WPARAM wParam, LPARAM lParam) {
	RECT r;

	GetClientRect(hwnd, &r);

	nCurrentVisLines = (r.bottom - r.top + nLineHeight - 1) / nLineHeight;

	return 0;
}

LRESULT HexViewer::Handle_WM_MOUSEWHEEL(WPARAM wParam, LPARAM lParam) {
	int iNewDelta, nScroll;
	
	iNewDelta = iMouseWheelDelta - (signed short)HIWORD(wParam);
	nScroll = iNewDelta / WHEEL_DELTA;

	if (nScroll) {
		ScrollTopTo((long)(i64TopOffset>>4) + nScroll);
		iNewDelta -= WHEEL_DELTA * nScroll;
	}

	iMouseWheelDelta = iNewDelta;

	return 0;
}

LRESULT HexViewer::Handle_WM_KEYDOWN(WPARAM wParam, LPARAM lParam) {
	switch(wParam) {
	case VK_UP:			ScrollTopTo((long)(i64TopOffset>>4) - 1); break;
	case VK_DOWN:		ScrollTopTo((long)(i64TopOffset>>4) + 1); break;
	case VK_PRIOR:		ScrollTopTo((long)(i64TopOffset>>4) - (nCurrentVisLines-1)); break;
	case VK_NEXT:		ScrollTopTo((long)(i64TopOffset>>4) + (nCurrentVisLines-1)); break;
	case VK_HOME:		ScrollTopTo(0); break;
	case VK_END:		ScrollTopTo((long)((i64FileSize+15)>>4) - nCurrentVisLines); break;
	}

	return 0;
}

////////////////////////////

struct HexViewerAskData {
	HexViewer *thisPtr;
	const char *title, *name1, *name2;
	__int64 v1, v2;
	int (HexViewer::*verifier)(HWND, __int64 v1, __int64 v2);
};

BOOL CALLBACK HexViewer::AskForValuesDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	HexViewerAskData *pData = (HexViewerAskData *)GetWindowLong(hdlg, DWL_USER);
	char buf[32];

	switch(msg) {
	case WM_INITDIALOG:
		pData = (HexViewerAskData *)lParam;
		SetWindowLong(hdlg, DWL_USER, lParam);

		SetWindowText(hdlg, pData->title);
		sprintf(buf, "%I64X", pData->v1);
		SetDlgItemText(hdlg, IDC_EDIT_ADDRESS1, buf);
		SendDlgItemMessage(hdlg, IDC_EDIT_ADDRESS1, EM_LIMITTEXT, 16, 0);
		SetDlgItemText(hdlg, IDC_STATIC_ADDRESS1, pData->name1);
		if (pData->name2) {
			sprintf(buf, "%I64X", pData->v2);
			SetDlgItemText(hdlg, IDC_EDIT_ADDRESS2, buf);
			SendDlgItemMessage(hdlg, IDC_EDIT_ADDRESS1, EM_LIMITTEXT, 16, 0);
			SetDlgItemText(hdlg, IDC_STATIC_ADDRESS2, pData->name2);
		} else {
			ShowWindow(GetDlgItem(hdlg, IDC_EDIT_ADDRESS2), SW_HIDE);
			ShowWindow(GetDlgItem(hdlg, IDC_STATIC_ADDRESS2), SW_HIDE);
		}
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDCANCEL:
			EndDialog(hdlg, 0);
			break;
		case IDOK:
			{
				__int64 v1=0, v2=0;
				const char *s, *t;
				char c;
				int i;

				GetDlgItemText(hdlg, IDC_EDIT_ADDRESS1, buf, sizeof buf);

				s = buf;
				while(c=*s++) {
					if (!(t=strchr(hexdig, toupper(c)))) {
						SetFocus(GetDlgItem(hdlg, IDC_EDIT_ADDRESS1));
						MessageBeep(MB_ICONEXCLAMATION);
						return TRUE;
					}

					v1 = (v1<<4) | (t-hexdig);
				}

				if (pData->name2) {
					GetDlgItemText(hdlg, IDC_EDIT_ADDRESS2, buf, sizeof buf);

					s = buf;
					while(c=*s++) {
						if (!(t=strchr(hexdig, toupper(c)))) {
							SetFocus(GetDlgItem(hdlg, IDC_EDIT_ADDRESS2));
							MessageBeep(MB_ICONEXCLAMATION);
							return TRUE;
						}

						v2 = (v2<<4) | (t-hexdig);
					}
				}

				if (i = (pData->thisPtr->*(pData->verifier))(hdlg, v1, v2)) {
					if (i>=0) {
						SetFocus(GetDlgItem(hdlg, i==1?IDC_EDIT_ADDRESS1:IDC_EDIT_ADDRESS2));
						MessageBeep(MB_ICONEXCLAMATION);
					}
					return TRUE;
				}

				pData->v1 = v1;
				pData->v2 = v2;
			}
			EndDialog(hdlg, 1);
			break;
		}
		return TRUE;
	}
	return FALSE;
}

bool HexViewer::AskForValues(const char *title, const char *name1, const char *name2, __int64& v1, __int64& v2, int (HexViewer::*verifier)(HWND hdlg, __int64 v1, __int64 v2)) {
	HexViewerAskData hvad;

	hvad.thisPtr = this;
	hvad.title = title;
	hvad.name1 = name1;
	hvad.name2 = name2;
	hvad.v1 = v1;
	hvad.v2 = v2;
	hvad.verifier = verifier;

	if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_HEXVIEWER), hwnd, AskForValuesDlgProc, (LPARAM)&hvad)) {
		v1 = hvad.v1;
		v2 = hvad.v2;
		return true;
	}
	return false;
}

int HexViewer::JumpVerifier(HWND hdlg, __int64 v1, __int64 v2) {
	if (v1>i64FileSize)
		return 1;

	return 0;
}

int HexViewer::ExtractVerifier(HWND hdlg, __int64 v1, __int64 v2) {
	if (v1 > i64FileSize)
		return 1;

	if (v1+v2 > i64FileSize)
		return 2;

	return 0;
}

int HexViewer::TruncateVerifier(HWND hdlg, __int64 v1, __int64 v2) {
	int r;

	if (v1 < i64FileSize)
		r = MessageBox(hdlg, "You will lose all data past the specified address. Are you sure you want to truncate the file?", "Warning", MB_ICONEXCLAMATION|MB_YESNO);
	else if (v1 > i64FileSize)
		r = MessageBox(hdlg, "You have specified an address past the end of the file. Extend file to specified address?", "Warning", MB_ICONEXCLAMATION|MB_YESNO);

	return r==IDYES ? 0 : -1;
}

void HexViewer::Extract() {
	__int64 v1 = i64TopOffset, v2=0x1000;

	if (AskForValues("Extract file segment", "Address:", "Length:", v1, v2, ExtractVerifier)) {
		char szName[MAX_PATH];
		OPENFILENAME ofn;

		szName[0] = 0;

		ofn.lStructSize			= sizeof(OPENFILENAME);
		ofn.hwndOwner			= hwnd;
		ofn.lpstrFilter			= "All files (*.*)\0*.*\0";
		ofn.lpstrCustomFilter	= NULL;
		ofn.nFilterIndex		= 1;
		ofn.lpstrFile			= szName;
		ofn.nMaxFile			= sizeof szName;
		ofn.lpstrFileTitle		= NULL;
		ofn.lpstrInitialDir		= NULL;
		ofn.lpstrTitle			= NULL;
		ofn.Flags				= OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
		ofn.lpstrDefExt			= NULL;

		if (GetSaveFileName(&ofn)) {
			HANDLE hFile2 = INVALID_HANDLE_VALUE;
			char *pBuf = NULL;

			try {
				__int64 fpos = 0;

				if (0xFFFFFFFF==SetFilePointer(hFile, (LONG)v1, (LONG *)&v1 + 1, FILE_BEGIN) && GetLastError()!=NO_ERROR)
					throw GetLastError();

				pBuf = new char[65536];

				if (!pBuf)
					throw MyMemoryError();

				hFile2 = CreateFile(szName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN, NULL);

				if (hFile2 == INVALID_HANDLE_VALUE)
					throw GetLastError();

				if ((0xFFFFFFFF==SetFilePointer(hFile2, (LONG)v2, (LONG *)&v2 + 1, FILE_BEGIN) && GetLastError()!=NO_ERROR)
					|| !SetEndOfFile(hFile2))
					throw GetLastError();

				if (0xFFFFFFFF==SetFilePointer(hFile2, 0, NULL, FILE_BEGIN) && GetLastError()!=NO_ERROR)
					throw GetLastError();

				ProgressDialog pd(hwnd, "Extract segment", "Copying data range", (long)(v2>>10), TRUE);
				pd.setValueFormat("%dK of %dK");

				while(v2 > 0) {
					DWORD dwToCopy = v2, dwActual;

					if (dwToCopy > 65536)
						dwToCopy = 65536;

					pd.check();

					if (!ReadFile(hFile, pBuf, dwToCopy, &dwActual, NULL) || dwActual < dwToCopy)
						throw GetLastError();

					if (!WriteFile(hFile2, pBuf, dwToCopy, &dwActual, NULL) || dwActual < dwToCopy)
						throw GetLastError();

					v2 -= dwActual;

					pd.advance((long)((fpos += dwActual)>>10));
				}

				if (!CloseHandle(hFile2))
					throw GetLastError();

				hFile2 = INVALID_HANDLE_VALUE;
			} catch(DWORD dw) {
				MyWin32Error("Cannot create extract file: %%s", dw).post(hwnd, "Error");
			} catch(MyUserAbortError e) {
				SetEndOfFile(hFile2);
			}

			if (hFile2 != INVALID_HANDLE_VALUE)
				CloseHandle(hFile2);

			delete[] pBuf;
		}
	}
}

////////////////////////////

ATOM RegisterHexViewer() {
	WNDCLASS wc;

	wc.style		= 0;
	wc.lpfnWndProc	= HexViewerWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(HexViewer *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
    wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName	= MAKEINTRESOURCE(IDR_HEXVIEWER_MENU);
	wc.lpszClassName= HEXVIEWERCLASS;

	return RegisterClass(&wc);

}

static LRESULT APIENTRY HexViewerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	HexViewer *pcd = (HexViewer *)GetWindowLong(hwnd, 0);

	switch(msg) {

	case WM_NCCREATE:
		if (!(pcd = new HexViewer(hwnd)))
			return FALSE;

		SetWindowLong(hwnd, 0, (LONG)pcd);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_CREATE:
		pcd->Init();
		return 0;

	case WM_SIZE:
		return pcd->Handle_WM_SIZE(wParam, lParam);

	case WM_DESTROY:
		delete pcd;
		SetWindowLong(hwnd, 0, 0);
		break;

	case WM_COMMAND:
		return pcd->Handle_WM_COMMAND(wParam, lParam);

	case WM_PAINT:
		return pcd->Handle_WM_PAINT(wParam, lParam);

	case WM_MOUSEWHEEL:
		return pcd->Handle_WM_MOUSEWHEEL(wParam, lParam);

	case WM_KEYDOWN:
		return pcd->Handle_WM_KEYDOWN(wParam, lParam);

	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;

	case WM_VSCROLL:
		return pcd->Handle_WM_VSCROLL(wParam, lParam);

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return FALSE;
}


void HexView(HWND hwndParent) {
	CreateWindowEx(
		WS_EX_CLIENTEDGE,
		HEXVIEWERCLASS,
		"VirtualDub Hex Viewer",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_VSCROLL,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		hwndParent,
		NULL,
		g_hInst,
		NULL);
}
