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

#ifndef f_GUI_H
#define f_GUI_H

#include <windows.h>
#include "list.h"

#define IDC_CAPTURE_WINDOW		(500)
#define IDC_STATUS_WINDOW		(501)
#define IDC_POSITION			(502)

class ModelessDlgNode : public ListNode2<ModelessDlgNode> {
public:
	HWND hdlg;

	ModelessDlgNode() {}
	ModelessDlgNode(HWND _hdlg) : hdlg(_hdlg) {}
};

void guiOpenDebug();

void guiDlgMessageLoop(HWND hDlg);
bool guiCheckDialogs(LPMSG pMsg);
void guiAddModelessDialog(ModelessDlgNode *pmdn);

void guiRedoWindows(HWND hWnd);
void guiSetStatus(char *format, int nPart, ...);
void guiSetTitle(HWND hWnd, UINT uID, ...);
void guiMenuHelp(HWND hwnd, WPARAM wParam, WPARAM part, UINT *iTranslator);
void guiOffsetDlgItem(HWND hdlg, UINT id, LONG xDelta, LONG yDelta);
void guiResizeDlgItem(HWND hdlg, UINT id, LONG x, LONG y, LONG dx, LONG dy);
void guiSubclassWindow(HWND hwnd, WNDPROC newproc);

void guiPositionInitFromStream(HWND hWndPosition);
LONG guiPositionHandleCommand(WPARAM wParam, LPARAM lParam);
LONG guiPositionHandleNotify(WPARAM wParam, LPARAM lParam);
void guiPositionBlit(HWND hWndClipping, LONG lFrame, int w=0, int h=0);

bool guiChooseColor(HWND hwnd, COLORREF& rgbOld);


enum {
	REPOS_NOMOVE		= 0,
	REPOS_MOVERIGHT		= 1,
	REPOS_MOVEDOWN		= 2,
	REPOS_SIZERIGHT		= 4,
	REPOS_SIZEDOWN		= 8
};

struct ReposItem {
	UINT uiCtlID;
	int fReposOpts;
};

void guiReposInit(HWND hwnd, struct ReposItem *lpri, POINT *lppt);
void guiReposResize(HWND hwnd, struct ReposItem *lpri, POINT *lppt);

void guiDeferWindowPos(HDWP hdwp, HWND hwnd, HWND hwndInsertAfter, int x, int y, int dx, int dy, UINT flags);
void guiEndDeferWindowPos(HDWP hdwp);
int guiMessageBoxF(HWND hwnd, LPCTSTR lpCaption, UINT uType, const char *format, ...);

void ticks_to_str(char *dst, DWORD ticks);
void size_to_str(char *dst, __int64 i64Bytes);

int guiListboxInsertSortedString(HWND, const char *);

#endif
