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

#define f_POSITIONCONTROL_CPP

#include <crtdbg.h>

#include <windows.h>
#include <commctrl.h>

#include "resource.h"
#include "oshelper.h"

#include "PositionControl.h"

extern HINSTANCE g_hInst;

static LRESULT APIENTRY PositionControlWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

////////////////////////////

extern const char szPositionControlName[]="birdyPositionControl";

static HICON hIcons[13];

static const UINT uIconIDs[13]={
	IDI_POS_STOP,
	IDI_POS_PLAY,
	IDI_POS_PLAYPREVIEW,
	IDI_POS_START,
	IDI_POS_BACKWARD,
	IDI_POS_FORWARD,
	IDI_POS_END,
	IDI_POS_PREV_KEY,
	IDI_POS_NEXT_KEY,
	IDI_POS_SCENEREV,
	IDI_POS_SCENEFWD,
	IDI_POS_MARKIN,
	IDI_POS_MARKOUT,
};

////////////////////////////

typedef struct PositionControlData {
	HFONT				hFont;
	int					nFrameCtlHeight;
	LONG				usPerFrame;
	PosCtlFTCallback	pFTCallback;
	void				*pvFTCData;

	bool fHasPlaybackControls;
	bool fHasMarkControls;
	bool fHasSceneControls;
	bool fTracking;
	bool fNoAutoFrame;
} PositionControlData;

////////////////////////////

ATOM RegisterPositionControl() {
	WNDCLASS wc;

	for(int i=0; i<(sizeof hIcons/sizeof hIcons[0]); i++)

		if (!(hIcons[i] = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(uIconIDs[i]),IMAGE_ICON ,0,0,0))) {

			_RPT1(0,"PositionControl: load failure on icon #%d\n",i+1);
			return NULL;
		}


	wc.style		= 0;
	wc.lpfnWndProc	= PositionControlWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(PositionControlData *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= NULL;
	wc.hbrBackground= (HBRUSH)(COLOR_3DFACE+1);	//GetStockObject(LTGRAY_BRUSH);
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= POSITIONCONTROLCLASS;

	return RegisterClass(&wc);
}

#undef IDC_START
enum {
	IDC_TRACKBAR	= 500,
	IDC_FRAME		= 501,
	IDC_STOP		= 502,
	IDC_PLAY		= 503,
	IDC_PLAYPREVIEW	= 504,
	IDC_START		= 505,
	IDC_BACKWARD	= 506,
	IDC_FORWARD		= 507,
	IDC_END			= 508,
	IDC_KEYPREV		= 509,
	IDC_KEYNEXT		= 510,
	IDC_SCENEREV	= 511,
	IDC_SCENEFWD	= 512,
	IDC_MARKIN		= 513,
	IDC_MARKOUT		= 514,
};

static const struct {
	UINT id;
	const char *tip;
} g_posctltips[]={
	{ IDC_TRACKBAR, "[Trackbar]\r\n\r\nDrag this to seek to any frame in the movie. Hold down SHIFT to snap to keyframes/I-frames." },
	{ IDC_FRAME, "[Frame indicator]\r\n\r\nDisplays the current frame number, timestamp, and frame type.\r\n\r\n"
					"[ ] AVI delta frame\r\n"
					"[D] AVI dropped frame\r\n"
					"[K] AVI key frame\r\n"
					"[I] MPEG-1 intra frame\r\n"
					"[P] MPEG-1 forward predicted frame\r\n"
					"[B] MPEG-1 bidirectionally predicted frame" },
	{ IDC_STOP, "[Stop] Stops playback or the current dub operation." },
	{ IDC_PLAY, "[Input playback] Starts playback of the input file." },
	{ IDC_PLAYPREVIEW, "[Output playback] Starts preview of processed output." },
	{ IDC_START, "[Start] Move to the first frame." },
	{ IDC_BACKWARD, "[Backward] Back up by one frame." },
	{ IDC_FORWARD, "[Forward] Advance by one frame." },
	{ IDC_END, "[End] Move to the last frame." },
	{ IDC_KEYPREV, "[Key previous] Move to the previous key frame or I-frame." },
	{ IDC_KEYNEXT, "[Key next] Move to the next key frame or I-frame." },
	{ IDC_SCENEREV, "[Scene reverse] Scan backward for the last scene change." },
	{ IDC_SCENEFWD, "[Scene forward] Scan forward for the next scene change." },
	{ IDC_MARKIN, "[Mark in] Specify the start for processing or of a selection to delete." },
	{ IDC_MARKOUT, "[Mark out] Specify the end for processing or of a selection to delete." },
};

static BOOL CALLBACK PositionControlInitChildrenProc(HWND hWnd, LPARAM lParam) {
	UINT id;

	switch(id = GetWindowLong(hWnd, GWL_ID)) {
	case IDC_STOP:
	case IDC_PLAY:
	case IDC_PLAYPREVIEW:
	case IDC_START:
	case IDC_BACKWARD:
	case IDC_FORWARD:
	case IDC_END:
	case IDC_KEYPREV:
	case IDC_KEYNEXT:
	case IDC_SCENEREV:
	case IDC_SCENEFWD:
	case IDC_MARKIN:
	case IDC_MARKOUT:
		SendMessage(hWnd, BM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcons[id - IDC_STOP]);
		break;

	case IDC_FRAME:
		SendMessage(hWnd, WM_SETFONT, (WPARAM)lParam, (LPARAM)MAKELONG(FALSE, 0));
		break;

	}

	return TRUE;
}

static void PositionControlReposChildren(HWND hWnd, PositionControlData *pcd) {
	RECT wndr;
	UINT id;
	int x, y;
	HWND hwndButton;

	GetClientRect(hWnd, &wndr);

	y = wndr.bottom - 24;
	x = 0;

	SetWindowPos(GetDlgItem(hWnd, IDC_TRACKBAR), NULL, 0, 0, wndr.right - wndr.left, y-wndr.top, SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER);

	if (pcd->fHasPlaybackControls) {
		for(id = IDC_STOP; id < IDC_START; id++) {
			SetWindowPos(GetDlgItem(hWnd, id), NULL, x, y, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);

			x += 24;
		}
		x+=8;
	}

	for(id = IDC_START; id < IDC_MARKIN; id++) {
		if (hwndButton = GetDlgItem(hWnd,id))
			SetWindowPos(hwndButton, NULL, x, y, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);

		x += 24;
	}
	x+=8;

	if (pcd->fHasMarkControls) {
		for(id = IDC_MARKIN; id <= IDC_MARKOUT; id++) {
			SetWindowPos(GetDlgItem(hWnd, id), NULL, x, y, 0, 0, SWP_NOACTIVATE|SWP_NOSIZE|SWP_NOZORDER);
			x += 24;
		}

		x+=8;
	}

	SetWindowPos(GetDlgItem(hWnd, IDC_FRAME), NULL, x, y+12-(pcd->nFrameCtlHeight>>1), min(wndr.right - x, 320), pcd->nFrameCtlHeight, SWP_NOACTIVATE|SWP_NOZORDER);

}

static void PositionControlUpdateString(HWND hWnd, PositionControlData *pcd, LONG lPos=-1) {
	char buf[128];
	int l;

	if (lPos < 0)
		lPos = SendMessage(GetDlgItem(hWnd, IDC_TRACKBAR), TBM_GETPOS, 0, 0);

	if (pcd->usPerFrame) {
//		l = wsprintf(buf, " Frame %ld (%ld ms)", lPos, MulDiv(lPos, pcd->usPerFrame, 1000));
		int ms, sec, min;
		long ticks = MulDiv(lPos, pcd->usPerFrame, 1000);

		ms  = ticks %1000; ticks /= 1000;
		sec	= ticks %  60; ticks /=  60;
		min	= ticks %  60; ticks /=  60;

		l = wsprintf(buf, " Frame %ld (%d:%02d:%02d.%03d)", lPos, ticks, min, sec, ms);
	} else
		l = wsprintf(buf, " Frame %ld", lPos);

	if (pcd->pFTCallback && l>0) {
		char c;

		if (c = pcd->pFTCallback(hWnd, pcd->pvFTCData, lPos)) {
			buf[l+0] = ' ';
			buf[l+1] = '[';
			buf[l+2] = c;
			buf[l+3] = ']';
			buf[l+4] = 0;
		}
	}

	SetDlgItemText(hWnd, IDC_FRAME, buf);
}

static LRESULT APIENTRY PositionControlWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	PositionControlData *pcd = (PositionControlData *)GetWindowLong(hWnd, 0);

	switch(msg) {

	case WM_NCCREATE:
		if (!(pcd = new PositionControlData))
			return FALSE;
		memset(pcd,0,sizeof(PositionControlData));

		SetWindowLong(hWnd, 0, (LONG)pcd);
		return TRUE;

	case WM_CREATE:
		{
			DWORD dwStyles;
			TOOLINFO ti;
			HWND hwndTT;

			dwStyles = GetWindowLong(hWnd, GWL_STYLE);
			pcd->fHasPlaybackControls	= !!(dwStyles & PCS_PLAYBACK);
			pcd->fHasMarkControls		= !!(dwStyles & PCS_MARK);
			pcd->fHasSceneControls		= !!(dwStyles & PCS_SCENE);

			pcd->hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
			pcd->nFrameCtlHeight = 18;
			
			if (pcd->hFont) {
				if (HDC hdc = GetDC(hWnd)) {
					TEXTMETRIC tm;
					int pad = 2*GetSystemMetrics(SM_CYEDGE);
					int availHeight = 24 - pad;
					HGDIOBJ hgoFont = SelectObject(hdc, pcd->hFont);

					if (GetTextMetrics(hdc, &tm)) {
						LOGFONT lf;

						pcd->nFrameCtlHeight = tm.tmHeight + tm.tmInternalLeading + pad;

						if (tm.tmHeight > availHeight && GetObject(pcd->hFont, sizeof lf, &lf)) {
							lf.lfHeight = availHeight;
							pcd->nFrameCtlHeight = 24;

							HFONT hFont = CreateFontIndirect(&lf);
							if (hFont)
								pcd->hFont = hFont;		// the old font was a stock object, so it doesn't need to be deleted
						}
					}

					pcd->nFrameCtlHeight = (pcd->nFrameCtlHeight+1) & ~1;

					SelectObject(hdc, hgoFont);
					ReleaseDC(hWnd, hdc);
				}
			}

			CreateWindowEx(0,TRACKBAR_CLASS,NULL,WS_CHILD|WS_VISIBLE|TBS_AUTOTICKS|TBS_ENABLESELRANGE,0,0,0,0,hWnd, (HMENU)IDC_TRACKBAR, g_hInst, NULL);

			SendDlgItemMessage(hWnd, IDC_TRACKBAR, TBM_SETPAGESIZE, 0, 50);

			CreateWindowEx(WS_EX_STATICEDGE,"EDIT",NULL,WS_CHILD|WS_VISIBLE|ES_READONLY,0,0,0,24,hWnd,(HMENU)IDC_FRAME,g_hInst,NULL);

			if (pcd->fHasPlaybackControls) {
				CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_ICON			,0,0,24,24,hWnd, (HMENU)IDC_STOP		, g_hInst, NULL);
				CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_ICON			,0,0,24,24,hWnd, (HMENU)IDC_PLAY		, g_hInst, NULL);
				CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_ICON			,0,0,24,24,hWnd, (HMENU)IDC_PLAYPREVIEW	, g_hInst, NULL);
			}
			CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_ICON			,0,0,24,24,hWnd, (HMENU)IDC_START		, g_hInst, NULL);
			CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_ICON			,0,0,24,24,hWnd, (HMENU)IDC_BACKWARD	, g_hInst, NULL);
			CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_ICON			,0,0,24,24,hWnd, (HMENU)IDC_FORWARD	, g_hInst, NULL);
			CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_ICON			,0,0,24,24,hWnd, (HMENU)IDC_END		, g_hInst, NULL);
			CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_ICON			,0,0,24,24,hWnd, (HMENU)IDC_KEYPREV	, g_hInst, NULL);
			CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_ICON			,0,0,24,24,hWnd, (HMENU)IDC_KEYNEXT	, g_hInst, NULL);

			if (pcd->fHasSceneControls) {
				CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE | BS_ICON	,0,0,24,24,hWnd, (HMENU)IDC_SCENEREV, g_hInst, NULL);
				CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE | BS_ICON	,0,0,24,24,hWnd, (HMENU)IDC_SCENEFWD, g_hInst, NULL);
			}
			if (pcd->fHasMarkControls) {
				CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_ICON			,0,0,24,24,hWnd, (HMENU)IDC_MARKIN	, g_hInst, NULL);
				CreateWindowEx(0				,"BUTTON"		,NULL,WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_ICON			,0,0,24,24,hWnd, (HMENU)IDC_MARKOUT	, g_hInst, NULL);
			}

			if (pcd->hFont)
				EnumChildWindows(hWnd, (WNDENUMPROC)PositionControlInitChildrenProc, (LPARAM)pcd->hFont);

			// Create tooltip control.

			hwndTT = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL, WS_POPUP|TTS_NOPREFIX|TTS_ALWAYSTIP,
					CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
					hWnd, NULL, g_hInst, NULL);

			if (hwndTT) {

				SetWindowPos(hwndTT, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
				SendMessage(hwndTT, TTM_SETDELAYTIME, TTDT_AUTOMATIC, MAKELONG(2000, 0));
				SendMessage(hwndTT, TTM_SETDELAYTIME, TTDT_RESHOW, MAKELONG(2000, 0));

				ti.cbSize		= sizeof(TOOLINFO);
				ti.uFlags		= TTF_SUBCLASS | TTF_IDISHWND;
				ti.hwnd			= hWnd;
				ti.lpszText		= LPSTR_TEXTCALLBACK;

				for(int i=0; i<sizeof g_posctltips/sizeof g_posctltips[0]; ++i) {
					ti.uId			= (WPARAM)GetDlgItem(hWnd, g_posctltips[i].id);

					if (ti.uId)
						SendMessage(hwndTT, TTM_ADDTOOL, 0, (LPARAM)&ti);
				}
			}
		}

	case WM_SIZE:
		PositionControlReposChildren(hWnd, pcd);
		break;

	case WM_NCDESTROY:
		if (pcd->hFont)
			DeleteObject(pcd->hFont);
		delete pcd;
		SetWindowLong(hWnd, 0, 0);
		break;

	case WM_HSCROLL:
		{
			NMHDR nm;

			nm.hwndFrom = hWnd;
			nm.idFrom	= GetWindowLong(hWnd, GWL_ID);

			switch(LOWORD(wParam)) {
			case TB_PAGEUP:			nm.code = PCN_PAGELEFT;			break;
			case TB_PAGEDOWN:		nm.code	= PCN_PAGERIGHT;		break;
			case TB_THUMBPOSITION:
				if (pcd->fTracking) {
					pcd->fTracking = false;
					nm.code = PCN_ENDTRACK;
					SendMessage(GetParent(hWnd), WM_NOTIFY, nm.idFrom, (LPARAM)&nm);
				}
				nm.code = PCN_THUMBPOSITION;
				break;
			case TB_THUMBTRACK:
				if (!pcd->fTracking) {
					pcd->fTracking = true;
					nm.code = PCN_BEGINTRACK;
					SendMessage(GetParent(hWnd), WM_NOTIFY, nm.idFrom, (LPARAM)&nm);
				}
				nm.code = PCN_THUMBTRACK;
				break;
			default:				nm.code = PCN_THUMBPOSITION;	break;
			}

			if (!pcd->fNoAutoFrame)
				PositionControlUpdateString(hWnd, pcd);
			SendMessage(GetParent(hWnd), WM_NOTIFY, nm.idFrom, (LPARAM)&nm);
		}
		break;

	case WM_NOTIFY:
		if (TTN_GETDISPINFO == ((LPNMHDR)lParam)->code) {
			NMTTDISPINFO *lphdr = (NMTTDISPINFO *)lParam;
			UINT id = (lphdr->uFlags & TTF_IDISHWND) ? GetWindowLong((HWND)lphdr->hdr.idFrom, GWL_ID) : lphdr->hdr.idFrom;

			*lphdr->lpszText = 0;

			SendMessage(lphdr->hdr.hwndFrom, TTM_SETMAXTIPWIDTH, 0, 5000);

			for(int i=0; i<sizeof g_posctltips/sizeof g_posctltips[0]; ++i) {
				if (id == g_posctltips[i].id)
					lphdr->lpszText = const_cast<char *>(g_posctltips[i].tip);
			}

			return TRUE;
		}
		break;

	case WM_COMMAND: {
		UINT cmd;

		switch(LOWORD(wParam)) {
		case IDC_STOP:			cmd = PCN_STOP;			break;
		case IDC_PLAY:			cmd = PCN_PLAY;			break;
		case IDC_PLAYPREVIEW:	cmd = PCN_PLAYPREVIEW;	break;
		case IDC_MARKIN:		cmd = PCN_MARKIN;		break;
		case IDC_MARKOUT:		cmd = PCN_MARKOUT;		break;
		case IDC_START:			cmd = PCN_START;		break;
		case IDC_BACKWARD:		cmd = PCN_BACKWARD;		break;
		case IDC_FORWARD:		cmd = PCN_FORWARD;		break;
		case IDC_END:			cmd = PCN_END;			break;
		case IDC_KEYPREV:		cmd = PCN_KEYPREV;		break;
		case IDC_KEYNEXT:		cmd = PCN_KEYNEXT;		break;

		case IDC_SCENEREV:
			cmd = PCN_SCENEREV;
//			SendMessage((HWND)lParam, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
			if (BST_UNCHECKED!=SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
				if (IsDlgButtonChecked(hWnd, IDC_SCENEFWD))
					CheckDlgButton(hWnd, IDC_SCENEFWD, BST_UNCHECKED);
			} else
				cmd = PCN_SCENESTOP;
			break;
		case IDC_SCENEFWD:
			cmd = PCN_SCENEFWD;
//			SendMessage((HWND)lParam, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
			if (BST_UNCHECKED!=SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)) {
				if (IsDlgButtonChecked(hWnd, IDC_SCENEREV))
					CheckDlgButton(hWnd, IDC_SCENEREV, BST_UNCHECKED);
			} else
				cmd = PCN_SCENESTOP;
			break;
		default:
			return 0;
		}
		SendMessage(GetParent(hWnd), WM_COMMAND, MAKELONG(GetWindowLong(hWnd, GWL_ID), cmd), (LPARAM)hWnd);
		}
		break;

	case PCM_SETRANGEMIN:
		SendMessage(GetDlgItem(hWnd, IDC_TRACKBAR), TBM_SETRANGEMIN, wParam, lParam);
		PositionControlUpdateString(hWnd, pcd);
		break;

	case PCM_SETRANGEMAX:
		SendMessage(GetDlgItem(hWnd, IDC_TRACKBAR), TBM_SETRANGEMAX, wParam, lParam);
		PositionControlUpdateString(hWnd, pcd);
		break;

	case PCM_GETPOS:
		return SendMessage(GetDlgItem(hWnd, IDC_TRACKBAR), TBM_GETPOS, 0, 0);

	case PCM_SETPOS:
		SendMessage(GetDlgItem(hWnd, IDC_TRACKBAR), TBM_SETPOS, (WPARAM)TRUE, lParam);
		PositionControlUpdateString(hWnd, pcd);
		break;

	case PCM_GETSELSTART:
		return SendMessage(GetDlgItem(hWnd, IDC_TRACKBAR), TBM_GETSELSTART, 0, 0);

	case PCM_GETSELEND:
		return SendMessage(GetDlgItem(hWnd, IDC_TRACKBAR), TBM_GETSELEND, 0, 0);

	case PCM_SETSELSTART:
		SendMessage(GetDlgItem(hWnd, IDC_TRACKBAR), TBM_SETSELSTART, wParam, lParam);
		break;

	case PCM_SETSELEND:
		SendMessage(GetDlgItem(hWnd, IDC_TRACKBAR), TBM_SETSELEND, wParam, lParam);
		break;

	case PCM_CLEARSEL:
		SendMessage(GetDlgItem(hWnd, IDC_TRACKBAR), TBM_CLEARSEL, (BOOL)wParam, 0);
		break;

	case PCM_SETFRAMERATE:
		pcd->usPerFrame = lParam;
		PositionControlUpdateString(hWnd, pcd);
		break;

	case PCM_RESETSHUTTLE:
		CheckDlgButton(hWnd, IDC_SCENEREV, BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_SCENEFWD, BST_UNCHECKED);
		break;

	case PCM_SETFRAMETYPECB:
		pcd->pFTCallback = (PosCtlFTCallback)wParam;
		pcd->pvFTCData = (void *)lParam;
		break;

	case PCM_CTLAUTOFRAME:
		pcd->fNoAutoFrame = !lParam;
		break;

	case PCM_SETDISPFRAME:
		PositionControlUpdateString(hWnd, pcd, lParam);
		break;

	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return FALSE;
}
