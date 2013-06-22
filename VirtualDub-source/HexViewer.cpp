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

#define f_HEXVIEWER_CPP

#define _WIN32_WINNT 0x0500

#include <stdio.h>
#include <ctype.h>
#include <crtdbg.h>

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include "resource.h"
#include "oshelper.h"
#include "gui.h"
#include "error.h"
#include "list.h"

#include "HexViewer.h"
#include "ProgressDialog.h"

extern HINSTANCE g_hInst;

////////////////////////////

extern const char szHexViewerClassName[]="birdyHexEditor";
static const char g_szHexWarning[]="Hex editor warning";

////////////////////////////

class HVModifiedLine : public ListNode2<HVModifiedLine> {
public:
	char			data[16];
	__int64			address;
	int				mod_flags;
private:

public:
	HVModifiedLine(__int64 addr);
};

HVModifiedLine::HVModifiedLine(__int64 addr)
	: address(addr)
	, mod_flags(0)
{}



////////////////////////////

class HexViewer {
private:
	const HWND	hwnd;
	HWND	hwndFind;
	HWND	hwndTree;
	HANDLE	hFile;
	HFONT	hfont;
	__int64	i64TopOffset;
	__int64 i64FileSize;
	__int64	i64Position;
	int		nCharWidth;
	int		nLineHeight;
	int		nLineLimit;
	int		nCurrentVisLines;
	int		nCurrentWholeLines;
	int		iMouseWheelDelta;

	char	rowcache[16];
	__int64	i64RowCacheAddr;

	List2<HVModifiedLine>	listMods;

	ModelessDlgNode	mdnFind;
	char	*pszFindString;
	int		nFindLength;
	bool	bFindCaseInsensitive;
	bool	bFindHex;
	bool	bFindReverse;

	bool	bCharMode;
	bool	bOddHex;
	bool	bEnableWrite;
	bool	bCaretHidden;

public:
	HexViewer(HWND);
	~HexViewer();

	static LRESULT APIENTRY HexViewerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) throw();
	static BOOL APIENTRY FindDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) throw();
	static BOOL APIENTRY TreeDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) throw();

private:
	void Init() throw();
	void Open() throw();
	void Open(const char *pszFile, bool bRW) throw();
	void Close() throw();
	void Commit() throw();

	const char *FillRowCache(__int64 line) throw();
	void InvalidateLine(__int64 line) throw();
	void ScrollTopTo(long lLine) throw();
	void ScrollVisible(__int64 nVisPos) throw();
	void MoveCaret() throw();
	void MoveCaretToByte(__int64 pos) throw();

	LRESULT Handle_WM_COMMAND(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_PAINT(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_VSCROLL(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_SIZE(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_MOUSEWHEEL(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_KEYDOWN(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_LBUTTONDOWN(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_CHAR(WPARAM wParam, LPARAM lParam) throw();

	static BOOL CALLBACK AskForValuesDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) throw();
	bool AskForValues(const char *title, const char *name1, const char *name2, __int64& default1, __int64& default2, int (HexViewer::*verifier)(HWND hdlg, __int64 v1, __int64 v2) throw()) throw();
	int JumpVerifier(HWND hdlg, __int64 v1, __int64 v2) throw();
	int ExtractVerifier(HWND hdlg, __int64 v1, __int64 v2) throw();
	int TruncateVerifier(HWND hdlg, __int64 v1, __int64 v2) throw();

	void Extract() throw();
	void Find() throw();
	void _RIFFScan(struct RIFFScanInfo &rsi, HWND hwndTV, HTREEITEM hti, __int64 pos, __int64 sizeleft);
	void RIFFTree(HWND hwndTV) throw();

	HVModifiedLine *FindModLine(__int64 addr) throw() {
		HVModifiedLine *pLine, *pLineNext;

		pLine = listMods.AtHead();

		while(pLineNext = pLine->NextFromHead()) {
			if (addr == pLine->address)
				return pLine;

			pLine = pLineNext;
		}

		return NULL;
	}

	void Hide() throw() {
		if (!bCaretHidden) {
			bCaretHidden = true;
			HideCaret(hwnd);
		}
	}

	void Show() throw() {
		if (bCaretHidden) {
			bCaretHidden = false;
			ShowCaret(hwnd);
		}
	}
};

////////////////////////////

HexViewer::HexViewer(HWND _hwnd) : hwnd(_hwnd), hwndFind(0), hwndTree(0), pszFindString(NULL), bFindReverse(false), hfont(0) {
	hFile = INVALID_HANDLE_VALUE;

	i64TopOffset = i64FileSize = 0;
	iMouseWheelDelta = 0;
	i64Position = 0;
	bCharMode = false;
	bOddHex = false;
	i64RowCacheAddr = -1;
}

HexViewer::~HexViewer() {
	delete[] pszFindString;
	pszFindString = NULL;

	Close();

	if (hwndTree)
		DestroyWindow(hwndTree);

	if (hwndFind)
		DestroyWindow(hwndFind);

	if (hfont)
		DeleteObject(hfont);
}

void HexViewer::Init() {
	HDC hdc;

	nLineHeight = 16;
	if (hdc = GetDC(hwnd)) {
		TEXTMETRIC tm;
		HGDIOBJ hfOld;

		hfOld = SelectObject(hdc, GetStockObject(ANSI_FIXED_FONT));

		GetTextMetrics(hdc, &tm);

		hfont = CreateFont(tm.tmHeight, 0, 0, 0, 0, FALSE, FALSE, FALSE, ANSI_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH|FF_DONTCARE, "Lucida Console");

		SelectObject(hdc, hfont ? hfont : GetStockObject(ANSI_FIXED_FONT));

		GetTextMetrics(hdc, &tm);
		nCharWidth	= tm.tmAveCharWidth;
		nLineHeight = tm.tmHeight;

		SelectObject(hdc, hfOld);

		ReleaseDC(hwnd, hdc);
	}
}

void HexViewer::Open() {
	char szName[MAX_PATH];
	OPENFILENAME ofn;

	szName[0] = 0;

	memset(&ofn, 0, sizeof ofn);

	ofn.lStructSize			= 0x4c;	//sizeof(OPENFILENAME); stupid beta include files
	ofn.hwndOwner			= hwnd;
	ofn.lpstrFilter			= "All files (*.*)\0*.*\0";
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= szName;
	ofn.nMaxFile			= sizeof szName;
	ofn.lpstrFileTitle		= NULL;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= NULL;
	ofn.Flags				= OFN_EXPLORER | OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_READONLY;
	ofn.lpstrDefExt			= NULL;

	if (GetOpenFileName(&ofn))
		Open(szName, !(ofn.Flags & OFN_READONLY));
}

void HexViewer::Open(const char *pszFile, bool bRW) {
	Close();

	bEnableWrite = bRW;

	if (bRW)
		hFile = CreateFile(pszFile, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	else
		hFile = CreateFile(pszFile, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		MyWin32Error("Cannot open file: %%s", GetLastError()).post(hwnd, "Hex editor error");
		return;
	}

	char buf[512];

	wsprintf(buf, "VirtualDub Hex Editor - [%s]%s", pszFile, bRW ? "" : " (read only)");
	SetWindowText(hwnd, buf);

	DWORD dwLow, dwHigh;
	
	dwLow = GetFileSize(hFile, &dwHigh);

	i64FileSize = dwLow | ((__int64)dwHigh << 32);

	SetScrollRange(hwnd, SB_VERT, 0, (int)(i64FileSize>>4), TRUE);

	i64RowCacheAddr	= -1;
	i64TopOffset	= 0;
	nLineLimit		= (int)((i64FileSize+15)>>4);
	InvalidateRect(hwnd, NULL, TRUE);
	UpdateWindow(hwnd);
}

void HexViewer::Close() {
	HVModifiedLine *pLine;

	while(pLine = listMods.RemoveHead())
		delete pLine;

	if (hFile == INVALID_HANDLE_VALUE)
		return;

	CloseHandle(hFile);
	hFile = INVALID_HANDLE_VALUE;

	InvalidateRect(hwnd, NULL, TRUE);
	UpdateWindow(hwnd);

	if (hwndTree)
		DestroyWindow(hwndTree);
}

void HexViewer::Commit() {
	HVModifiedLine *pLine;

	while(pLine = listMods.RemoveHead()) {
		DWORD dwBytes = 16, dwActual;

		if (((unsigned __int64)pLine->address>>32) == ((unsigned __int64)i64FileSize >> 32))
			if (!(((long)pLine->address ^ (long)i64FileSize) & 0xfffffff0))
				dwBytes = (long)i64FileSize - (long)pLine->address;

		SetFilePointer(hFile, (LONG)pLine->address, (LONG *)&pLine->address + 1, FILE_BEGIN);
		WriteFile(hFile, pLine->data, dwBytes, &dwActual, NULL);
		delete pLine;
	}

	InvalidateRect(hwnd, NULL, TRUE);
}

const char *HexViewer::FillRowCache(__int64 i64Offset) throw() {
	if (i64Offset == i64RowCacheAddr)
		return rowcache;

	DWORD dwActual;

	i64RowCacheAddr = i64Offset;

	SetFilePointer(hFile, (LONG)i64Offset, (LONG *)&i64Offset + 1, FILE_BEGIN);
	ReadFile(hFile, rowcache, 16, &dwActual, NULL);

	return rowcache;
}

void HexViewer::InvalidateLine(__int64 line) throw() {
	int visidx = (int)(line - i64TopOffset) >> 4;
	RECT r;

	if (visidx < 0 || visidx >= nCurrentVisLines)
		return;

	GetClientRect(hwnd, &r);
	r.top		= nLineHeight * visidx;
	r.bottom	= r.top + nLineHeight;

	InvalidateRect(hwnd, &r, TRUE);
}

void HexViewer::ScrollTopTo(long lLine) throw() {
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

	Hide();
	if (abs(delta) > nCurrentVisLines) {
		InvalidateRect(hwnd, NULL, TRUE);
	} else {
	   if (hdc = GetDC(hwnd)) {
		   ScrollDC(hdc, 0, -delta*nLineHeight, NULL, NULL, NULL, &rRedraw);
		   ReleaseDC(hwnd, hdc);
		   InvalidateRect(hwnd, &rRedraw, TRUE);
		   UpdateWindow(hwnd);
	   }
	}
	MoveCaret();
}

void HexViewer::ScrollVisible(__int64 nVisPos) throw() {
	__int64 nTopLine	= i64TopOffset>>4;
	__int64 nCaretLine	= i64Position>>4;

	if (nCaretLine < nTopLine)
		ScrollTopTo((long)nCaretLine);
	else if (nCaretLine >= nTopLine + nCurrentWholeLines)
		ScrollTopTo((long)(nCaretLine - nCurrentWholeLines + 1));
}

void HexViewer::MoveCaret() throw() {
	__int64 nTopLine	= i64TopOffset>>4;
	__int64 nCaretLine	= i64Position>>4;

	if (nCaretLine < nTopLine || nCaretLine >= nTopLine + nCurrentVisLines) {
		Hide();
		return;
	}

	int nLine, nByteOffset, x, y;

	nLine			= (int)(nCaretLine - nTopLine);
	nByteOffset		= (int)i64Position & 15;

	y = nLine * nLineHeight;

	if (bCharMode) {
		x = 14 + 3*16 + 1 + nByteOffset;
	} else {
		x = 14 + 3*nByteOffset;

		if (bOddHex)
			++x;
	}

	SetCaretPos(x*nCharWidth, y);
	Show();
}

void HexViewer::MoveCaretToByte(__int64 pos) throw() {
	if (pos < 0) {
		bOddHex = false;
		pos = 0;
	} else if (pos >= i64FileSize) {
		pos = i64FileSize - 1;
		bOddHex = !bCharMode;
	}
	i64Position = pos;
	ScrollVisible(pos);
	MoveCaret();
}

LRESULT HexViewer::Handle_WM_COMMAND(WPARAM wParam, LPARAM lParam) throw() {
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
	case ID_FILE_SAVE:
		Commit();
		break;
	case ID_FILE_REVERT:
		if (IDOK==MessageBox(hwnd, "Discard all changes?", g_szHexWarning, MB_OKCANCEL)) {
			HVModifiedLine *pLine;

			while(pLine = listMods.RemoveHead())
				delete pLine;
         
			InvalidateRect(hwnd, NULL, TRUE);
		}
		break;
	case ID_EDIT_JUMP:
		{
			__int64 v1 = i64Position, v2;

			if (AskForValues("Jump to address", "Address:", NULL, v1, v2, JumpVerifier))
				MoveCaretToByte(v1);
		}
		break;
	case ID_EDIT_TRUNCATE:
		{
			__int64 v1 = i64Position, v2;

			if (AskForValues("Truncate file", "Address:", NULL, v1, v2, TruncateVerifier)) {
				if ((0xFFFFFFFF==SetFilePointer(hFile, (LONG)v1, (LONG *)&v1 + 1, FILE_BEGIN) && GetLastError()!=NO_ERROR)
						|| !SetEndOfFile(hFile))
					MyWin32Error("Cannot truncate file: %%s", GetLastError()).post(hwnd, "Error");

				i64FileSize = v1;

				ScrollTopTo((long)(i64TopOffset>>4));
			}
		}
		break;
	case ID_EDIT_EXTRACT:
		Extract();
		break;

	case ID_EDIT_FINDNEXT:
		if (hwndFind) {
			SendMessage(hwndFind, WM_COMMAND, IDC_FIND, 0);
			break;
		} else if (pszFindString) {
			Find();
			break;
		}
	case ID_EDIT_FIND:
		if (hwndFind)
			SetForegroundWindow(hwndFind);
		else
			CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_HEXVIEWER_FIND), hwnd, FindDlgProc, (LPARAM)this);
		break;

	case ID_EDIT_RIFFTREE:
		if (hwndTree)
			DestroyWindow(hwndTree);
		else
			CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_HEXVIEWER_RIFFLIST), hwnd, TreeDlgProc, (LPARAM)this);
		break;

	case ID_HELP_WHY:
		MessageBox(hwnd,
			"I need a quick way for people to send me parts of files that don't load properly "
			"in VirtualDub, and this is a handy way to do it. Well, that, and it's annoying to "
			"check 3Gb AVI files if your hex editor tries to load the file into memory.",
			"Why is there a hex editor in VirtualDub?",
			MB_OK);
		break;

	case ID_HELP_KEYS:
		MessageBox(hwnd,
			"arrow keys/PgUp/PgDn: navigation\n"
			"TAB: switch between ASCII/Hex\n"
			"Backspace: undo",
			"Keyboard commands",
			MB_OK);
		break;

	}

	return 0;
}

static const char hexdig[]="0123456789ABCDEF";

LRESULT HexViewer::Handle_WM_PAINT(WPARAM wParam, LPARAM lParam) throw() {
	HDC hdc;
	PAINTSTRUCT ps;
	char buf[128];
	__int64 i64Offset;
	int y;
	RECT r;
	int i;

	hdc = BeginPaint(hwnd, &ps);

	GetClientRect(hwnd, &r);
	r.left = nCharWidth*79;
	FillRect(hdc, &r, (HBRUSH)(COLOR_WINDOW+1));

	i = GetClipBox(hdc, &r);

	if (i != ERROR && i != NULLREGION) {
		y = r.top - r.top % nLineHeight;

		if (hFile != INVALID_HANDLE_VALUE) {
			HGDIOBJ hfOld;

			i64Offset = i64TopOffset + (y/nLineHeight)*16;

			hfOld = SelectObject(hdc, hfont ? hfont : GetStockObject(ANSI_FIXED_FONT));

			SetTextAlign(hdc, TA_TOP | TA_LEFT);
			SetBkMode(hdc, OPAQUE);

			while(y < r.bottom+nLineHeight-1 && i64Offset < i64FileSize) {
				HVModifiedLine *pModLine = FindModLine(i64Offset);
				const char *data;
				char *s;
				int i, len;

				if (pModLine)
					data = pModLine->data;
				else {
					data = FillRowCache(i64Offset);
				}

				len = 16;

				if ((i64Offset & -16i64) == (i64FileSize & -16i64))
					len = (int)i64FileSize & 15;

				s = buf + sprintf(buf, "%12I64X: ", i64Offset);

				for(i=0; i<len; i++) {
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

				for(i=0; i<len; i++) {
					if (data[i]>=0x20 && data[i]<0x7f)
						*s++ = data[i];
					else
						*s++ = '.';
				}

				TextOut(hdc, 0, y, buf, s - buf);

				// Draw modified constants in blue.

				if (pModLine) {
					COLORREF crOldTextColor = SetTextColor(hdc, 0xFF0000);

					for(i=0; i<16; i++)
						if (!(pModLine->mod_flags & (1<<i))) {
							buf[14 + i*3] = ' ';
							buf[15 + i*3] = ' ';
							buf[63+i] =  ' ';
						}

					buf[37] = ' ';

					SetBkMode(hdc, TRANSPARENT);

					TextOut(hdc, nCharWidth*14, y, buf+14, 16*3-1);
					TextOut(hdc, nCharWidth*63, y, buf+63, 16);

					SetBkMode(hdc, OPAQUE);

					SetTextColor(hdc, crOldTextColor);
				}

				y += nLineHeight;
				i64Offset += 16;
			}

			SelectObject(hdc, hfOld);
		}

		if (y < r.bottom+nLineHeight-1) {
			GetClientRect(hwnd, &r);
			r.top = y;
			FillRect(hdc, &r, (HBRUSH)(COLOR_WINDOW+1));
		}
	}
	EndPaint(hwnd, &ps);

	return 0;
}

LRESULT HexViewer::Handle_WM_VSCROLL(WPARAM wParam, LPARAM lParam) throw() {
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

LRESULT HexViewer::Handle_WM_SIZE(WPARAM wParam, LPARAM lParam) throw() {
	RECT r;

	GetClientRect(hwnd, &r);

	nCurrentWholeLines	= (r.bottom - r.top) / nLineHeight; 
	nCurrentVisLines	= (r.bottom - r.top + nLineHeight - 1) / nLineHeight;

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

LRESULT HexViewer::Handle_WM_KEYDOWN(WPARAM wParam, LPARAM lParam) throw() {
	switch(wParam) {
	case VK_UP:
		MoveCaretToByte(i64Position-16);
		break;
	case VK_DOWN:
		MoveCaretToByte(i64Position+16);
		break;
	case VK_LEFT:
		if (bCharMode || (bOddHex = !bOddHex))
			MoveCaretToByte(i64Position-1);
		else
			MoveCaretToByte(i64Position);
		break;
	case VK_RIGHT:
		if (bCharMode || !(bOddHex = !bOddHex))
			MoveCaretToByte(i64Position+1);
		else
			MoveCaretToByte(i64Position);
		break;
	case VK_PRIOR:
		MoveCaretToByte(i64Position - (nCurrentVisLines-1)*16);
		break;
	case VK_NEXT:
		MoveCaretToByte(i64Position + (nCurrentVisLines-1)*16);
		break;
	case VK_HOME:
		bOddHex = false;
		if ((signed short)GetKeyState(VK_CONTROL)<0)
			MoveCaretToByte(0);
		else
			MoveCaretToByte(i64Position & -16i64);
		break;
	case VK_END:
		bOddHex = true;
		if ((signed short)GetKeyState(VK_CONTROL)<0)
			MoveCaretToByte(i64FileSize - 1);
		else
			MoveCaretToByte(i64Position | 15);
		break;
	case VK_TAB:
		bCharMode = !bCharMode;
		bOddHex = false;
		MoveCaretToByte(i64Position);
		break;
	case VK_F3:
		if (hFile != INVALID_HANDLE_VALUE)
			Handle_WM_COMMAND(ID_EDIT_FINDNEXT, 0);
		break;
	case 'F':
		if (hFile != INVALID_HANDLE_VALUE)
			if (GetKeyState(VK_CONTROL)<0)
				Handle_WM_COMMAND(ID_EDIT_FIND, 0);
		break;
	case 'G':
		if (hFile != INVALID_HANDLE_VALUE)
			if (GetKeyState(VK_CONTROL)<0)
				Handle_WM_COMMAND(ID_EDIT_JUMP, 0);
		break;
	case 'R':
		if (hFile != INVALID_HANDLE_VALUE)
			if (GetKeyState(VK_CONTROL)<0)
				Handle_WM_COMMAND(ID_EDIT_RIFFTREE, 0);
	}

	return 0;
}

LRESULT HexViewer::Handle_WM_LBUTTONDOWN(WPARAM wParam, LPARAM lParam) throw() {
	int x, y;

	x = LOWORD(lParam) / nCharWidth;
	y = HIWORD(lParam) / nLineHeight;

   if (x < 14)
      x = 14;
   else if (x >= 63+16)
      x = 63+15;

	if (x >= 14 && x < 61) {
		x -= 13;

		bCharMode = false;
		bOddHex = false;

		if (x%3 == 2)
			bOddHex = true;

		x = x/3;
	} else if (x >= 63 && x < 63+16) {
		bCharMode = true;
		bOddHex = false;

		x -= 63;
	} else
		return 0;

	MoveCaretToByte(i64TopOffset + y*16 + x);

	return 0;
}

LRESULT HexViewer::Handle_WM_CHAR(WPARAM wParam, LPARAM lParam) throw() {
	int key = wParam;

	if (!bEnableWrite)
		return 0;

	if (key == '\b') {
		HVModifiedLine *pLine;
		__int64 i64Offset;

		if (bCharMode || !bOddHex)
			MoveCaretToByte(i64Position-1);
		else
			MoveCaretToByte(i64Position);

		bOddHex = false;

		i64Offset = i64Position & -16i64;

		if (pLine = FindModLine(i64Offset)) {
			// Revert byte.

			int offset = (int)i64Position & 15;

			pLine->data[offset] = FillRowCache(i64Offset)[offset];
			pLine->mod_flags &= ~(1<<offset);

			if (!pLine->mod_flags) {
				pLine->Remove();
				delete pLine;
			}

			InvalidateLine(i64Offset);
		}

		return 0;
	}

	if (!isprint(key) || (!bCharMode && !isxdigit(key)))
		return 0;

	// Fetch the mod line.

	__int64 i64Offset = i64Position & -16i64;
	HVModifiedLine *pLine = FindModLine(i64Offset);

	if (!pLine) {
		// Line not resident -- fetch from disk and create.

		DWORD dwActual;

		pLine = new HVModifiedLine(i64Offset);

		SetFilePointer(hFile, (LONG)i64Offset, (LONG *)&i64Offset + 1, FILE_BEGIN);

		ReadFile(hFile, pLine->data, 16, &dwActual, NULL);

		listMods.AddTail(pLine);
	}

	// Modify the appropriate byte and redraw the line.

	int offset = (int)i64Position & 15;

	if (bCharMode)
		pLine->data[offset] = key;
	else {
		int v = toupper(key) - '0';
		if (v > 9)
			v -= 7;

		if (bOddHex)
			pLine->data[offset] = (pLine->data[offset]&0xf0) + v;
		else
			pLine->data[offset] = (pLine->data[offset]&0x0f) + (v<<4);
	}
	pLine->mod_flags |= 1<<offset;

	// invalidate row cache

	i64RowCacheAddr = -1;

	InvalidateLine(i64Position);

	// Send a RIGHT keypress to advance.

	SendMessage(hwnd, WM_KEYDOWN, VK_RIGHT, 0);

	return 0;
}

////////////////////////////

struct HexViewerAskData {
	HexViewer *thisPtr;
	const char *title, *name1, *name2;
	__int64 v1, v2;
	int (HexViewer::*verifier)(HWND, __int64 v1, __int64 v2);
};

BOOL CALLBACK HexViewer::AskForValuesDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) throw() {
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

		SendDlgItemMessage(hdlg, IDC_EDIT_ADDRESS1, EM_SETSEL, 0, -1);
		SetFocus(GetDlgItem(hdlg, IDC_EDIT_ADDRESS1));

		return FALSE;

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

bool HexViewer::AskForValues(const char *title, const char *name1, const char *name2, __int64& v1, __int64& v2, int (HexViewer::*verifier)(HWND hdlg, __int64 v1, __int64 v2)) throw() {
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

int HexViewer::JumpVerifier(HWND hdlg, __int64 v1, __int64 v2) throw() {
	if (v1>i64FileSize)
		return 1;

	return 0;
}

int HexViewer::ExtractVerifier(HWND hdlg, __int64 v1, __int64 v2) throw() {
	if (v1 > i64FileSize)
		return 1;

	if (v1+v2 > i64FileSize)
		return 2;

	return 0;
}

int HexViewer::TruncateVerifier(HWND hdlg, __int64 v1, __int64 v2) throw() {
	int r;

	if (v1 < i64FileSize)
		r = MessageBox(hdlg, "You will lose all data past the specified address. Are you sure you want to truncate the file?", "Warning", MB_ICONEXCLAMATION|MB_YESNO);
	else if (v1 > i64FileSize)
		r = MessageBox(hdlg, "You have specified an address past the end of the file. Extend file to specified address?", "Warning", MB_ICONEXCLAMATION|MB_YESNO);

	return r==IDYES ? 0 : -1;
}

void HexViewer::Extract() throw() {
	__int64 v1 = i64TopOffset, v2=0x1000;

	if (AskForValues("Extract file segment", "Address:", "Length:", v1, v2, ExtractVerifier)) {
		char szName[MAX_PATH];
		OPENFILENAME ofn;

		szName[0] = 0;

		ofn.lStructSize			= 0x4c; //sizeof(OPENFILENAME);
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
					DWORD dwToCopy = (DWORD)v2, dwActual;

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

void HexViewer::Find() throw() {
	if (!nFindLength || !pszFindString) {
		SendMessage(hwnd, WM_COMMAND, ID_EDIT_FIND, 0);
		return;
	}

	int *next = new int[nFindLength+1];
	char *searchbuffer = new char[65536];
	char *revstring = new char[nFindLength];
	char *findstring = pszFindString;
	int i,j;

	if (!next || !searchbuffer) {
		delete[] next;
		delete[] searchbuffer;
		delete[] revstring;
		return;
	}

	if (bFindReverse) {
		for(i=0; i<nFindLength; ++i)
			revstring[i] = pszFindString[nFindLength-i-1];

		findstring = revstring;
	}

	// Initialize next list (Knuth-Morris-Pratt algorithm):

	next[0] = -1;
	i = 0;
	j = -1;

	do {
		if (j==-1 || findstring[i] == findstring[j]) {
			++i;
			++j;
			next[i] = (findstring[i] == findstring[j]) ? next[j] : j;
		} else
			j = next[j];
	} while(i < nFindLength);

	// Begin paging in sectors from disk.

	int limit=0;
	int size = 512;
	__int64 pos = i64Position;
	__int64 posbase;
	bool bLastPartial = false;

	ProgressDialog pd(GetForegroundWindow(), "Find",
		bFindReverse?"Reverse searching for string":"Forward searching for string", (long)(((bFindReverse ? i64Position : i64FileSize - i64Position)+1048575)>>20), TRUE);
	pd.setValueFormat("%dMB of %dMB");

	i = 0;
	j = -1;	// this causes the first char to be skipped

	try {
		if (bFindReverse) {
			List2<HVModifiedLine>::rvit itML = listMods.end();

			while(pos >= 0) {
				{
					DWORD dwActual;

					i = pos & 511;

					pos &= ~511i64;

					SetFilePointer(hFile, (LONG)pos, (LONG *)&pos + 1, FILE_BEGIN);
					if (!ReadFile(hFile, searchbuffer, size, &dwActual, NULL))
						break;

					// we're overloading the bLastPartial variable as a 'first' flag....

					if (!bLastPartial && !dwActual)
						goto xit;

					bLastPartial = true;

					limit = (int)dwActual;

					if (pos + limit > i64Position)
						limit = i64Position - pos;

					if (!i) 
						i = limit;

					while(itML && itML->address >= pos+limit)
						--itML;

					while(itML && itML->address >= pos) {
						memcpy(searchbuffer + (long)(itML->address - pos), itML->data, 16);
						--itML;
					}

					posbase = pos;
					pos -= size;

					if (size < 65536 && !((long)pos & (size*2-1)))
						size += size;
				}

				if (bFindCaseInsensitive)
					while(i >= 0) {
						if (j == -1 || toupper((unsigned char)searchbuffer[i]) == toupper((unsigned char)findstring[j])) {
							--i;
							++j;

							if (j >= nFindLength) {
								MoveCaretToByte(posbase+i+1);
								goto xit;
							}
						} else
							j = next[j];
					}
				else
					while(i >= 0) {
						if (j == -1 || searchbuffer[i] == findstring[j]) {
							--i;
							++j;

							if (j >= nFindLength) {
								MoveCaretToByte(posbase+i+1);
								goto xit;
							}
						} else
							j = next[j];
					}

				pd.advance((long)((i64Position - pos)>>20));
				pd.check();
			}
		} else {
			List2<HVModifiedLine>::fwit itML = listMods.begin();

			for(;;) {
				{
					DWORD dwActual;

					if (bLastPartial)
						break;

					i = pos & 511;

					pos &= ~511i64;

					SetFilePointer(hFile, (LONG)pos, (LONG *)&pos + 1, FILE_BEGIN);
					if (!ReadFile(hFile, searchbuffer, size, &dwActual, NULL))
						break;

					limit = (int)dwActual;

					while(itML && itML->address < pos)
						++itML;

					while(itML && itML->address < pos+limit) {
						memcpy(searchbuffer + (long)(itML->address - pos), itML->data, 16);
						++itML;
					}

					if (dwActual < size)
						bLastPartial = true;

					posbase = pos;
					pos += limit;

					if (size < 65536 && !((long)pos & (size*2-1)))
						size += size;
				}

				if (bFindCaseInsensitive)
					while(i < limit) {
						if (j == -1 || toupper((unsigned char)searchbuffer[i]) == toupper((unsigned char)findstring[j])) {
							++i;
							++j;

							if (j >= nFindLength) {
								MoveCaretToByte(pos-limit+i-nFindLength);
								goto xit;
							}
						} else
							j = next[j];
					}
				else
					while(i < limit) {
						if (j == -1 || searchbuffer[i] == findstring[j]) {
							++i;
							++j;

							if (j >= nFindLength) {
								MoveCaretToByte(posbase+i-nFindLength);
								goto xit;
							}
						} else
							j = next[j];
					}

				pd.advance((long)((pos - i64Position)>>20));
				pd.check();
			}
		}

		pd.close();

		MessageBox(GetForegroundWindow(), "Search string not found", "Find", MB_OK);
xit:
		;
	} catch(MyUserAbortError) {
	}

	delete[] next;
	delete[] searchbuffer;
	delete[] revstring;
}

////////////////////////////

LRESULT APIENTRY HexViewer::HexViewerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) throw() {
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

	case WM_SETFOCUS:
		CreateCaret(hwnd, NULL, pcd->nCharWidth, pcd->nLineHeight);
		pcd->bCaretHidden = true;
		pcd->MoveCaret();
		return 0;

	case WM_KILLFOCUS:
		DestroyCaret();
		return 0;

	case WM_LBUTTONDOWN:
		return pcd->Handle_WM_LBUTTONDOWN(wParam, lParam);

	case WM_CHAR:
		return pcd->Handle_WM_CHAR(wParam, lParam);

	case WM_INITMENU:
		{
			DWORD dwEnableFlags = (pcd->hFile != INVALID_HANDLE_VALUE && pcd->bEnableWrite ? (MF_BYCOMMAND|MF_ENABLED) : (MF_BYCOMMAND|MF_GRAYED));
			HMENU hMenu = (HMENU)wParam;

			EnableMenuItem(hMenu,ID_FILE_SAVE, dwEnableFlags);
			EnableMenuItem(hMenu,ID_FILE_REVERT, dwEnableFlags);
			EnableMenuItem(hMenu,ID_EDIT_TRUNCATE, dwEnableFlags);

			dwEnableFlags = (pcd->hFile != INVALID_HANDLE_VALUE ? (MF_BYCOMMAND|MF_ENABLED) : (MF_BYCOMMAND|MF_GRAYED));

			EnableMenuItem(hMenu,ID_EDIT_JUMP, dwEnableFlags);
			EnableMenuItem(hMenu,ID_EDIT_EXTRACT, dwEnableFlags);
			EnableMenuItem(hMenu,ID_EDIT_RIFFTREE, dwEnableFlags);
			EnableMenuItem(hMenu,ID_EDIT_FIND, dwEnableFlags);
			EnableMenuItem(hMenu,ID_EDIT_FINDNEXT, dwEnableFlags);
			EnableMenuItem(hMenu,ID_FILE_CLOSE, dwEnableFlags);

			CheckMenuItem(hMenu, ID_EDIT_RIFFTREE, pcd->hwndTree ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		}
		return 0;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

BOOL APIENTRY HexViewer::FindDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) throw() {
	HexViewer *pcd = (HexViewer *)GetWindowLong(hwnd, DWL_USER);

	switch(msg) {
	case WM_INITDIALOG:
		SetWindowLong(hwnd, DWL_USER, lParam);
		pcd = (HexViewer *)lParam;
		pcd->hwndFind = hwnd;
		pcd->mdnFind.hdlg = hwnd;
		guiAddModelessDialog(&pcd->mdnFind);

		if (pcd->pszFindString) {
			if (pcd->bFindHex) {
				char *text = new char[pcd->nFindLength*3];

				if (text) {
					for(int i=0; i<pcd->nFindLength; ++i) {
						int c = (unsigned char)pcd->pszFindString[i];

						text[i*3+0] = hexdig[c>>4];
						text[i*3+1] = hexdig[c&15];
						text[i*3+2] = ' ';
					}
					text[i*3-1] = 0;

					SetDlgItemText(hwnd, IDC_STRING, text);
				}
				CheckDlgButton(hwnd, IDC_HEX, BST_CHECKED);
			} else {
				SetDlgItemText(hwnd, IDC_STRING, pcd->pszFindString);
			}

			if (pcd->bFindCaseInsensitive)
				CheckDlgButton(hwnd, IDC_CASELESS, BST_CHECKED);

		}

		if (pcd->bFindReverse)
			CheckDlgButton(hwnd, IDC_UP, BST_CHECKED);
		else
			CheckDlgButton(hwnd, IDC_DOWN, BST_CHECKED);

		SetFocus(GetDlgItem(hwnd, IDC_STRING));
		return FALSE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDCANCEL:
			DestroyWindow(hwnd);
			break;

		case IDC_FIND:
			{
				HWND hwndEdit = GetDlgItem(hwnd, IDC_STRING);
				int l = GetWindowTextLength(hwndEdit);

				pcd->bFindHex = !!IsDlgButtonChecked(hwnd, IDC_HEX);
				pcd->bFindCaseInsensitive = !!IsDlgButtonChecked(hwnd, IDC_CASELESS);
				pcd->bFindReverse = !!IsDlgButtonChecked(hwnd, IDC_UP);

				if (l) {
					char *text = new char[l+1];

					if (GetWindowText(hwndEdit, text, l+1)) {
						if (IsDlgButtonChecked(hwnd, IDC_HEX)) {
							char *s = text, *s2;
							char *t = text;
							int c;

							for(;;) {
								while(*s && isspace(*s))
									++s;

								if (!*s)
									break;

								s2 = s;

								if (isxdigit(*s2)) {
									c = strchr(hexdig, toupper((int)(unsigned char)*s2++))-hexdig;
									if (isxdigit(*s2))
										c = c*16 + (strchr(hexdig, toupper((int)(unsigned char)*s2++))-hexdig);

									*t++ = (char)c;
								}

								if (s == s2) {
									SendMessage(hwndEdit, EM_SETSEL, s-text, s-text);
									SetFocus(hwndEdit);
									MessageBeep(MB_ICONEXCLAMATION);

									delete[] text;

									return 0;
								}

								s = s2;
							}

							l = t - text;
						} else {
							l = strlen(text);
						}
					}

					delete[] pcd->pszFindString;
					pcd->pszFindString = text;
					pcd->nFindLength = l;

					pcd->Find();
				}
			}
		}
		return TRUE;

	case WM_DESTROY:
		pcd->hwndFind = NULL;
		pcd->mdnFind.Remove();
		return TRUE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////

static inline bool isValidFOURCC(unsigned long l) {
	return isprint(l>>24) && isprint((l>>16)&0xff) && isprint((l>>8)&0xff) && isprint(l&0xff);
}

static const struct RIFFChunkInfo {
	unsigned long id;
	const char *desc;
} g_RIFFChunks[]={
	' IVA', "Audio/video interleave file",
	'XIVA', "AVI2 extension block",
	'EVAW', "Sound waveform",
	0,
}, g_LISTChunks[]={
	'lrdh', "AVI file header block",
	'lrts', "AVI stream header block",
	'lmdo', "AVI2 extended header block",
	'ivom', "AVI data block",
	0,
}, g_JustChunks[]={
	'KNUJ', "padding",
	'hiva', "AVI file header",
	'hrts', "AVI stream header",
	'frts', "AVI stream format",
	'drts', "AVI stream codec data",
	'xdni', "AVI2 hierarchical indexing data",
	'hlmd', "AVI2 extended header",
	'1xdi', "AVI legacy index",
	'mges', "VirtualDub next segment pointer",
	0
};

static const char *LookupRIFFChunk(const RIFFChunkInfo *tbl, unsigned long ckid) {
	while(tbl->id) {
		if (tbl->id == ckid)
			return tbl->desc;
		++tbl;
	}

	return "unknown";
}

struct RIFFScanInfo {
	ProgressDialog& pd;
	int count[100];
	__int64 size[100];

	RIFFScanInfo(ProgressDialog &_pd) : pd(_pd) {}
};

void HexViewer::_RIFFScan(RIFFScanInfo &rsi, HWND hwndTV, HTREEITEM hti, __int64 pos, __int64 sizeleft) {
	char buf[128];

	while(sizeleft > 8) {
		const char *cl;
		struct {
			unsigned long ckid, size, listid;
		} chunk;
		bool bExpand = false;

		rsi.pd.advance((long)(pos >> 10));
		rsi.pd.check();

		// Try to read 12 bytes at the current position.

		int off = (unsigned long)pos & 15;

		cl = FillRowCache(pos - off);

		memcpy(&chunk, cl+off, off<4 ? 12 : 16-off);

		if (off > 4) {
			cl = FillRowCache(pos+16 - off);
			memcpy((char *)&chunk + (16-off), cl, off-4);
		}

		// quick validation tests

		if (chunk.ckid == 'TSIL' || chunk.ckid == 'FFIR') {		// RIFF or LIST
			char *dst = buf+sprintf(buf, "%08I64X [%-4.4s:%-4.4s:%8ld]: ", pos, &chunk.ckid, &chunk.listid, chunk.size);

			if (sizeleft < 12 || chunk.size < 4 || chunk.size > sizeleft-8 || !isValidFOURCC(chunk.listid)) {
				strcpy(dst, "invalid LIST/RIFF chunk");
				sizeleft = 0;
			} else {
				strcpy(dst, LookupRIFFChunk(chunk.ckid=='TSIL' ? g_LISTChunks : g_RIFFChunks, chunk.listid));
				bExpand = true;
			}
		} else {
			char *dst = buf+sprintf(buf, "%08I64X [%-4.4s:%8ld]: ", pos, &chunk.ckid, chunk.size);

			if (!isValidFOURCC(chunk.ckid) || chunk.size > sizeleft-8) {
				strcpy(dst, "invalid chunk");
				sizeleft = 0;
			} else if (isdigit(chunk.ckid&0xff) && isdigit((chunk.ckid>>8)&0xff)) {
				int stream = 10*(chunk.ckid&15) + ((chunk.ckid&0x0f00)>>8);

				sprintf(dst, "stream %d: byte pos %8I64d, chunk %ld", stream, rsi.size[stream], rsi.count[stream]);
				rsi.size[stream] += chunk.size;
				rsi.count[stream] ++;
			} else
				strcpy(dst, LookupRIFFChunk(g_JustChunks, chunk.ckid));
		}

		TVINSERTSTRUCT tvis;

		tvis.hParent		= hti;
		tvis.hInsertAfter	= TVI_LAST;
		tvis.item.mask		= TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
		tvis.item.lParam	= 1;
		tvis.item.pszText	= buf;
		tvis.item.state		= TVIS_EXPANDED;
		tvis.item.stateMask	= TVIS_EXPANDED;

		HTREEITEM htiNew = TreeView_InsertItem(hwndTV, &tvis);

		if (bExpand)
			_RIFFScan(rsi, hwndTV, htiNew, pos+12, chunk.size-4);

		chunk.size = (chunk.size+1)&~1;

		pos += chunk.size + 8;
		sizeleft -= chunk.size + 8;
	}
}

void HexViewer::RIFFTree(HWND hwndTV) throw() {
	ProgressDialog pd(hwndTree, "Constructing RIFF tree", "Scanning file", (long)((i64FileSize+1023)>>10), true);
	RIFFScanInfo rsi(pd);

	pd.setValueFormat("%dK of %dK");

	memset(&rsi.count, 0, sizeof rsi.count);
	memset(&rsi.size, 0, sizeof rsi.size);

	try {
		_RIFFScan(rsi, hwndTV, TVI_ROOT, 0, i64FileSize);
	} catch(MyUserAbortError) {
	}

	SendMessage(hwndTV, WM_SETFONT, (WPARAM)hfont, TRUE);
}

BOOL APIENTRY HexViewer::TreeDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) throw() {
	HexViewer *pcd = (HexViewer *)GetWindowLong(hdlg, DWL_USER);

	switch(msg) {
	case WM_INITDIALOG:
		SetWindowLong(hdlg, DWL_USER, lParam);
		pcd = (HexViewer *)lParam;
		pcd->hwndTree = hdlg;
		pcd->RIFFTree(GetDlgItem(hdlg, IDC_TREE));
		return TRUE;

	case WM_SIZE:
		SetWindowPos(GetDlgItem(hdlg, IDC_TREE), NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOZORDER|SWP_NOACTIVATE);
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
			DestroyWindow(hdlg);
			return TRUE;
		}
		break;

	case WM_DESTROY:
		pcd->hwndTree = NULL;
		break;

	case WM_NOTIFY:
		if (GetWindowLong(((NMHDR *)lParam)->hwndFrom, GWL_ID) == IDC_TREE) {
			const NMHDR *pnmh = (NMHDR *)lParam;

			if (pnmh->code == NM_DBLCLK || (pnmh->code == TVN_KEYDOWN && ((LPNMTVKEYDOWN)lParam)->wVKey == VK_RETURN)) {
				HTREEITEM hti = TreeView_GetSelection(pnmh->hwndFrom);

				if (hti) {
					char buf[128];
					TVITEM tvi;
					__int64 seekpos;

					tvi.mask		= TVIF_TEXT | TVIF_PARAM;
					tvi.hItem		= hti;
					tvi.pszText		= buf;
					tvi.cchTextMax	= sizeof buf;

					TreeView_GetItem(pnmh->hwndFrom, &tvi);

					if (tvi.lParam && 1==sscanf(buf, "%I64x", &seekpos)) {
						pcd->MoveCaretToByte(seekpos);
						PostMessage(hdlg, WM_APP, 0, 0);
					}
				}

				SetWindowLong(hdlg, DWL_MSGRESULT, 1);
			}
			return TRUE;
		}
		break;

	case WM_APP:
		SetForegroundWindow(pcd->hwnd);
		SetFocus(pcd->hwnd);
		return TRUE;
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////

ATOM RegisterHexViewer() {
	WNDCLASS wc;

	wc.style		= 0;
	wc.lpfnWndProc	= HexViewer::HexViewerWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(HexViewer *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
    wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= NULL; //(HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName	= MAKEINTRESOURCE(IDR_HEXVIEWER_MENU);
	wc.lpszClassName= HEXVIEWERCLASS;

	return RegisterClass(&wc);

}
void HexView(HWND hwndParent) {
	CreateWindowEx(
		WS_EX_CLIENTEDGE,
		HEXVIEWERCLASS,
		"VirtualDub Hex Editor",
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
