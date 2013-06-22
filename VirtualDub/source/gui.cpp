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

#include <stdarg.h>

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include "ClippingControl.h"
#include "PositionControl.h"
#include "VideoSource.h"
#include <vd2/system/error.h>
#include <vd2/system/list.h>
#include <vd2/system/w32assist.h>
#include "VBitmap.h"
#include "timeline.h"

#include "gui.h"
#include "resource.h"
#include "misc.h"
#include "oshelper.h"

#define MAX_STATUS_PARTS (8)

static COLORREF g_crCustomColors[16];

extern HINSTANCE g_hInst;
extern HWND g_hWnd;
extern HWND g_hwndJobs;

static HWND g_hwndDebugWindow=NULL;

static List2<ModelessDlgNode> g_listModelessDlgs;

int g_debugVal, g_debugVal2;

extern "C" void ycblit(void *, void *);

////////////////////////////////////////////////////////////////////////////

INT_PTR CALLBACK DebugDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
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
		case IDCANCEL:
			DestroyWindow(hdlg);
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
#pragma vdpragma_TODO("improve this")
#ifndef _M_AMD64
	else if (GetKeyState(VK_CONTROL)<0) {
		char *p = new char[16384+128];
		static const struct {
			BITMAPINFOHEADER bih;
			unsigned long p[8];
		} f={
			{sizeof(BITMAPINFOHEADER),128,128,1,8,BI_RGB,128*128,0,0,0,0},
			{
				0xffffff,
				0xf1f1f1,
				0xdfdfdf,
				0xc9c9c9,
				0xafafaf,
				0x919191,
				0x6d6d6d,
				0x404040,
			}
		};
		ycblit(p,0);

		HDC hdc = GetDC(g_hWnd);
		SetDIBitsToDevice(hdc, 0, 0, 128, 128, 0, 0, 0, 128, p, (const BITMAPINFO *)&f.bih, DIB_RGB_COLORS);
		ReleaseDC(g_hWnd, hdc);

		delete[] p;
	}
#endif
}

////////////////////////////////////////////////////////////////////////////

void guiDlgMessageLoop(HWND hDlg) {
	MSG msg;

	while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (!hDlg || !IsWindow(hDlg) || !IsDialogMessage(hDlg, &msg)) {
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

	HWND hwndAncestor = NULL;
	if (pMsg->hwnd)
		hwndAncestor = VDGetAncestorW32(pMsg->hwnd, GA_ROOT);

	while(pmdn_next = pmdn->NextFromHead()) {
		if (pmdn->mhAccel && hwndAncestor == pmdn->hdlg)
			if (TranslateAccelerator(pmdn->hdlg, pmdn->mhAccel, pMsg))
				return true;

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

HHOOK g_vdModelessDialogHook;
bool g_vdModelessRecursionFlag;

LRESULT CALLBACK VDModelessDialogHookW32(int code, WPARAM wParam, LPARAM lParam) {
	if (code == MSGF_DIALOGBOX && !g_vdModelessRecursionFlag) {
		g_vdModelessRecursionFlag = true;
		bool taken = guiCheckDialogs((LPMSG)lParam);
		g_vdModelessRecursionFlag = false;
		if (taken)
			return TRUE;
	}

	return CallNextHookEx(g_vdModelessDialogHook, code, wParam, lParam);
}

void VDDeinstallModelessDialogHookW32() {
	if (g_vdModelessDialogHook) {
		UnhookWindowsHookEx(g_vdModelessDialogHook);
		g_vdModelessDialogHook = NULL;
	}
}

void VDInstallModelessDialogHookW32() {
	if (!g_vdModelessDialogHook)
		g_vdModelessDialogHook = SetWindowsHookEx(WH_MSGFILTER, VDModelessDialogHookW32, NULL, GetCurrentThreadId());
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

#if 0
	HWND hwndAudio = GetDlgItem(hWnd, 12345);
	SetWindowPos(hwndAudio,
				NULL,
				0,
				rClient.bottom - (rStatus.bottom-rStatus.top) - (rPosition.bottom-rPosition.top) - 256,
				rClient.right,
				256,
				SWP_NOACTIVATE|SWP_NOZORDER);
#endif

	SetWindowPos(hWndPosition,
				NULL,
				0,
				rClient.bottom - (rStatus.bottom-rStatus.top) - (rPosition.bottom-rPosition.top),
				rClient.right,
				rPosition.bottom-rPosition.top,
				SWP_NOACTIVATE|SWP_NOZORDER);

	SetWindowPos(hWndStatus,
				NULL,
				0,
				rClient.bottom - (rStatus.bottom-rStatus.top),
				rClient.right,
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
	char buf[1024];
	va_list val;

	va_start(val, nPart);
	_vsnprintf(buf, sizeof buf, format, val);
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

	SetWindowText(hWnd, buf2);
}

void guiSetTitleW(HWND hWnd, UINT uID, ...) {
	wchar_t buf2[256];
	va_list val;

	VDStringW s(VDLoadStringW32(uID));

	va_start(val, uID);
	vswprintf(buf2, s.c_str(), val);
	va_end(val);

	if (GetVersion() < 0x80000000)
		SetWindowTextW(hWnd, buf2);
	else
		SetWindowText(hWnd, VDTextWToA(buf2).c_str());
}

void guiMenuHelp(HWND hwnd, WPARAM wParam, WPARAM part, const UINT *iTranslator) {
	HWND hwndStatus = GetDlgItem(hwnd, IDC_STATUS_WINDOW);
	char msgbuf[256];

	if (!(HIWORD(wParam) & MF_POPUP) && !(HIWORD(wParam) & MF_SYSMENU)) {
		const UINT *idPtr = iTranslator;

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
	SetWindowLongPtr(hwnd, GWLP_USERDATA, GetWindowLongPtr(hwnd, GWLP_WNDPROC));
	SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LPARAM)newproc);
}

///////////////////////////////////////

extern vdrefptr<VideoSource> inputVideoAVI;

void guiPositionInitFromStream(IVDPositionControl *pc) {
	if (!inputVideoAVI) return;

	const VDFraction videoRate(inputVideoAVI->getRate());

	pc->SetRange(inputVideoAVI->getStart(), inputVideoAVI->getEnd());
	pc->SetFrameRate(videoRate);
}

void VDTranslatePositionCommand(HWND hwnd, WPARAM wParam, LPARAM lParam) {
	switch(HIWORD(wParam)) {
		case PCN_START:
			SendMessage(hwnd, WM_COMMAND, MAKELONG(ID_VIDEO_SEEK_START, 0), NULL);
			break;
		case PCN_BACKWARD:
			SendMessage(hwnd, WM_COMMAND, MAKELONG(ID_VIDEO_SEEK_PREV, 0), NULL);
			break;
		case PCN_FORWARD:
			SendMessage(hwnd, WM_COMMAND, MAKELONG(ID_VIDEO_SEEK_NEXT, 0), NULL);
			break;
		case PCN_END:
			SendMessage(hwnd, WM_COMMAND, MAKELONG(ID_VIDEO_SEEK_END, 0), NULL);
			break;
		case PCN_KEYPREV:
			SendMessage(hwnd, WM_COMMAND, MAKELONG(ID_VIDEO_SEEK_KEYPREV, 0), NULL);
			break;
		case PCN_KEYNEXT:
			SendMessage(hwnd, WM_COMMAND, MAKELONG(ID_VIDEO_SEEK_KEYNEXT, 0), NULL);
			break;
	}
}

bool VDHandleTimelineCommand(IVDPositionControl *pc, VDTimeline *pTimeline, UINT cmd) {
	switch(cmd) {
		case ID_VIDEO_SEEK_START:		pc->SetPosition(pTimeline->GetStart());			return true;
		case ID_VIDEO_SEEK_PREV:		pc->SetPosition(pc->GetPosition() - 1);			return true;
		case ID_VIDEO_SEEK_NEXT:		pc->SetPosition(pc->GetPosition() + 1);			return true;
		case ID_VIDEO_SEEK_END:			pc->SetPosition(pTimeline->GetEnd());			return true;
		case ID_VIDEO_SEEK_PREVONESEC:	pc->SetPosition(pc->GetPosition() - 50);		return true;
		case ID_VIDEO_SEEK_NEXTONESEC:	pc->SetPosition(pc->GetPosition() + 50);		return true;

		case ID_VIDEO_SEEK_KEYPREV:
			{
				VDPosition pos = pTimeline->GetPrevKey(pc->GetPosition());

				if (pos < 0) pos = pTimeline->GetStart();

				pc->SetPosition(pos);
			}
			return true;
		case ID_VIDEO_SEEK_KEYNEXT:
			{
				VDPosition pos = pTimeline->GetNextKey(pc->GetPosition());

				if (pos < 0) pos = pTimeline->GetEnd();

				pc->SetPosition(pos);
			}
			return true;
	}

	return false;
}

VDPosition guiPositionHandleCommand(WPARAM wParam, IVDPositionControl *pc) {
	if (!inputVideoAVI) return -1;

	switch(HIWORD(wParam)) {
		case PCN_START:
			pc->SetPosition(inputVideoAVI->getStart());
			return inputVideoAVI->getStart();
		case PCN_BACKWARD:
			{
				VDPosition pos = pc->GetPosition();

				if (pos > inputVideoAVI->getStart()) {
					pc->SetPosition(pos - 1);
					return pos - 1;
				}
			}
			break;
		case PCN_FORWARD:
			{
				VDPosition pos = pc->GetPosition();

				if (pos < inputVideoAVI->getEnd()) {
					pc->SetPosition(pos + 1);
					return pos + 1;
				}
			}
			break;
		case PCN_END:
			pc->SetPosition(inputVideoAVI->getEnd());
			return inputVideoAVI->getEnd();

		case PCN_KEYPREV:
			{
				VDPosition lSample = inputVideoAVI->prevKey(pc->GetPosition());

				if (lSample < 0) lSample = inputVideoAVI->getStart();

				pc->SetPosition(lSample);
				return lSample;
			}
			break;
		case PCN_KEYNEXT:
			{
				VDPosition lSample = inputVideoAVI->nextKey(pc->GetPosition());

				if (lSample < 0) lSample = inputVideoAVI->getEnd();

				pc->SetPosition(lSample);
				return lSample;
			}
			break;
	}

	return -1;
}

VDPosition guiPositionHandleNotify(LPARAM lParam, IVDPositionControl *pc) {
	LPNMHDR nmh = (LPNMHDR)lParam;

	switch(nmh->code) {
	case PCN_THUMBTRACK:
	case PCN_THUMBPOSITION:
	case PCN_PAGELEFT:
	case PCN_PAGERIGHT:
	case CCN_REFRESHFRAME:
		if (inputVideoAVI)
			return pc->GetPosition();

		break;
	}

	return -1;
}

void guiPositionBlit(HWND hWndClipping, VDPosition lFrame, int w, int h) {
	if (lFrame<0) return;
	try {
		BITMAPINFOHEADER *dcf;

		dcf = inputVideoAVI->getDecompressedFormat();

		if (lFrame < inputVideoAVI->getStart() || lFrame >= inputVideoAVI->getEnd())
			SendMessage(hWndClipping, CCM_BLITFRAME2, 0, (LPARAM)NULL);
		else {
			Pixel32 *tmpmem;
			const void *pFrame = inputVideoAVI->getFrame(lFrame);

			if (w>0 && h>0 && w!=dcf->biWidth && h != dcf->biHeight && (tmpmem = new Pixel32[((w+1)&~1)*h + ((dcf->biWidth+1)&~1)*dcf->biHeight])) {
				VBitmap vbt(tmpmem, w, h, 32);
				VBitmap vbs(tmpmem+((w+1)&~1)*h, dcf->biWidth, dcf->biHeight, 32);
				BITMAPINFOHEADER bih;

				VBitmap srcbm((void *)pFrame, dcf);
				vbs.BitBlt(0, 0, &srcbm, 0, 0, -1, -1);
				vbt.StretchBltBilinearFast(0, 0, w, h, &vbs, 0, 0, vbs.w, vbs.h);

				VDPixmap px(VDAsPixmap(vbt));

				SendMessage(hWndClipping, CCM_BLITFRAME2, 0, (LPARAM)&px);

				delete[] tmpmem;
			} else
				SendMessage(hWndClipping, CCM_BLITFRAME2, 0, (LPARAM)&inputVideoAVI->getTargetFormat());
		}

	} catch(const MyError&) {
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
		sprintf(dst, "%luKB", (unsigned long)((i64Bytes+512) / 1024));
	} else if ((unsigned long)i64Bytes == i64Bytes) {
		i64Bytes += 52429;

		sprintf(dst, "%lu.%cMB", (unsigned long)(i64Bytes >> 20), (char)('0' + (( ((unsigned long)i64Bytes & 1048575) * 10)>>20)));
	} else {
		i64Bytes += (long)((1L<<30) / 200);

		sprintf(dst, "%I64u.%02dGB",
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




int guiListboxInsertSortedString(HWND hwnd, const char *pszStr) {
	int idx, cnt;
	char buf[2048];
	char *activebuf = buf, *activebufalloc = NULL;
	int activebuflen = sizeof buf;

	cnt = SendMessage(hwnd, LB_GETCOUNT, 0, 0);

	if (cnt==LB_ERR)
		return -1;

	for(idx=0; idx<cnt; idx++) {
		int len = SendMessage(hwnd, LB_GETTEXTLEN, idx, 0);

		if (len < 0) {
			freemem(activebufalloc);
			return -1;
		}

		if (++len > activebuflen) {
			activebuf = (char *)reallocmem(activebufalloc, len);

			if (!activebuf) {
				freemem(activebufalloc);
				return -1;
			}

			activebufalloc = activebuf;
			activebuflen = len;
		}

		SendMessage(hwnd, LB_GETTEXT, idx, (LPARAM)activebuf);

		if (stricmp(pszStr, activebuf) < 0)
			break;
	}

	if (idx >= cnt)
		idx = -1;

	return SendMessage(hwnd, LB_INSERTSTRING, idx, (LPARAM)pszStr);
}

////////////////////////////////////////////////////////////////////////////////

VDAutoLogDisplay::VDAutoLogDisplay()
	: mLogger(kVDLogWarning)
{
}

VDAutoLogDisplay::~VDAutoLogDisplay() {
}

void VDAutoLogDisplay::Post(VDGUIHandle hParent) {
	const VDAutoLogger::tEntries& ents = mLogger.GetEntries();

	if (!ents.empty()) {
		DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_WITHERRORS), (HWND)hParent, DlgProc, (LPARAM)&ents);
	}
}

INT_PTR CALLBACK VDAutoLogDisplay::DlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:
		{
			const VDAutoLogger::tEntries& ents = *(VDAutoLogger::tEntries *)lParam;
			IVDLogWindowControl *pLogWin = VDGetILogWindowControl(GetDlgItem(hdlg, IDC_LOG));

			for(VDAutoLogger::tEntries::const_iterator it(ents.begin()), itEnd(ents.end()); it!=itEnd; ++it) {
				const VDAutoLogger::Entry& ent = *it;
				pLogWin->AddEntry(ent.severity, ent.text);
			}
		}
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK: case IDCANCEL:
			EndDialog(hdlg, 0);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////

VDDialogBaseW32::VDDialogBaseW32(UINT dlgid)
: mpszDialogName(MAKEINTRESOURCE(dlgid))
, mhdlg(NULL)
{
}

INT_PTR CALLBACK VDDialogBaseW32::StaticDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDDialogBaseW32 *pThis = (VDDialogBaseW32 *)GetWindowLongPtr(hwnd, DWLP_USER);

	if (msg == WM_INITDIALOG) {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		pThis = (VDDialogBaseW32 *)lParam;
		pThis->mhdlg = hwnd;
	} else if (msg == WM_NCDESTROY) {
		if (pThis)
			pThis->PreNCDestroy();
	}

	return pThis ? pThis->DlgProc(msg, wParam, lParam) : FALSE;
}

LRESULT VDDialogBaseW32::ActivateDialog(VDGUIHandle hParent) {
	return DialogBoxParam(g_hInst, mpszDialogName, (HWND)hParent, StaticDlgProc, (LPARAM)this);
}

LRESULT VDDialogBaseW32::ActivateDialogDual(VDGUIHandle hParent) {
	if (VDIsWindowsNT())
		return DialogBoxParamW(g_hInst, IS_INTRESOURCE(mpszDialogName) ? (LPCWSTR)mpszDialogName : VDTextAToW(mpszDialogName).c_str(), (HWND)hParent, StaticDlgProc, (LPARAM)this);
	else
		return DialogBoxParamA(g_hInst, mpszDialogName, (HWND)hParent, StaticDlgProc, (LPARAM)this);
}

void VDDialogBaseW32::End(LRESULT res) {
	EndDialog(mhdlg, res);
	mhdlg = NULL;
}

bool VDDialogBaseW32::CreateModeless(VDGUIHandle hParent) {
	VDASSERT(!mhdlg);
	return !!CreateDialogParam(g_hInst, mpszDialogName, (HWND)hParent, StaticDlgProc, (LPARAM)this);
}

void VDDialogBaseW32::DestroyModeless() {
	DestroyWindow(mhdlg);
	mhdlg = NULL;
}

