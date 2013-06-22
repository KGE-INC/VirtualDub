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

#include <windows.h>
#include <commctrl.h>
#include <crtdbg.h>
#include <vfw.h>

#include "resource.h"
#include "gui.h"
#include "audio.h"

#include "AudioSource.h"
#include "VideoSource.h"
#include "InputFile.h"

#include "DubStatus.h"

#include "dub.h"

extern HWND g_hWnd;

///////////////////////////////////////////////////////////////////////////

class DubStatus : public IDubStatusHandler {
private:
	enum { TITLE_IDLE, TITLE_MINIMIZED, TITLE_NORMAL };

	int					iLastTitleMode;
	long				lLastTitleProgress;

	HWND				hwndStatus;
	UINT statTimer;
	DWORD dwStartTime;
	LONG lastFrame;
	LONG lFrameDiff1,lFrameDiff2;
	DWORD dwTicks, dwLastTicks1, dwLastTicks2;

	enum { MAX_FRAME_SIZES = 512 };

	DWORD		dwFrameSizes[MAX_FRAME_SIZES];
	long		lFrameFirstIndex, lFrameLastIndex;
	long		lFrameLobound, lFrameHibound;
	RECT				rStatusChild;
	HWND				hwndStatusChild;
	bool				fShowStatusWindow;
	bool				fFrozen;

	// our links...

	DubAudioStreamInfo	*painfo;
	DubVideoStreamInfo	*pvinfo;
	AudioSource			*aSrc;
	VideoSource			*vSrc;
	InputFile			*pInput;
	AudioStream			*audioStreamSource;

	IDubber				*pDubber;
	DubOptions			*opt;
	DubPositionCallback	positionCallback;
	int					iPriority;				// current priority level index of processes

	static BOOL APIENTRY StatusMainDlgProc( HWND hWnd, UINT message, UINT wParam, LONG lParam );
	static BOOL APIENTRY StatusVideoDlgProc( HWND hWnd, UINT message, UINT wParam, LONG lParam );
	static BOOL APIENTRY StatusPerfDlgProc( HWND hWnd, UINT message, UINT wParam, LONG lParam );
	static BOOL APIENTRY StatusDlgProc( HWND hWnd, UINT message, UINT wParam, LONG lParam );
	void StatusTimerProc(HWND hWnd);

	static int iPriorities[][2];

public:
	DubStatus();
	~DubStatus();
	void InitLinks(	DubAudioStreamInfo	*painfo,
		DubVideoStreamInfo	*pvinfo,
		AudioSource			*aSrc,
		VideoSource			*vSrc,
		InputFile			*pInput,
		AudioStream			*audioStreamSource,

		IDubber				*pDubber,
		DubOptions			*opt);
	HWND Display(HWND hwndParent, int iInitialPriority);
	void Destroy();
	bool ToggleStatus();
	void SetPositionCallback(DubPositionCallback dpc);
	void NotifyNewFrame(long f);
	void SetLastPosition(LONG pos);
	void Freeze();
	bool isVisible();
	bool isFrameVisible(bool fOutput);
	bool ToggleFrame(bool fOutput);

};

IDubStatusHandler *CreateDubStatusHandler() {
	return new DubStatus();
}

///////////////////////////////////////////////////////////////////////////

extern char g_szInputAVIFileTitle[];
extern HINSTANCE g_hInst;
extern HWND g_hWnd;

///////////////////////////////////////////////////////////////////////////

static long pickClosestNiceBound(long val, bool higher) {

	static long bounds[]={
		0,
		100,
		200,
		500,
		1024,
		2048,
		5120,
		10240,
		20480,
		51200,
		102400,
		204800,
		512000,
		1048576,
		2097152,
		5242880,
		10485760,
		20971520,
		52428800,
		104857600,
		209715200,
		524288000,
		1073741824,
		2147483647,
	};

	int i;

	// silly value?

	if (val <= 0)
		return 0;

	if (!higher) {
		for(i=0; i<23; i++)
			if (bounds[i] <= val)
				return bounds[i];

		return 0x7FFFFFFF;
	} else {
		for(i=23; i>0; i--)
			if (bounds[i-1] < val)
				break;

		return bounds[i];
	}
}

///////////////////////////////////////////////////////////////////////////

DubStatus::DubStatus() {
	iLastTitleMode		= TITLE_IDLE;
	lLastTitleProgress	= 0;

	memset(dwFrameSizes, 0, sizeof dwFrameSizes);
	lFrameFirstIndex = 0;
	lFrameLastIndex = 0;

	fShowStatusWindow	= true;

	hwndStatus			= NULL;
	positionCallback	= NULL;

	fFrozen = false;
}

DubStatus::~DubStatus() {
	Destroy();
}

void DubStatus::InitLinks(	DubAudioStreamInfo	*painfo,
	DubVideoStreamInfo	*pvinfo,
	AudioSource			*aSrc,
	VideoSource			*vSrc,
	InputFile			*pInput,
	AudioStream			*audioStreamSource,

	IDubber				*pDubber,
	DubOptions			*opt) {

	this->painfo			= painfo;
	this->pvinfo			= pvinfo;
	this->aSrc				= aSrc;
	this->vSrc				= vSrc;
	this->pInput			= pInput;
	this->audioStreamSource	= audioStreamSource;

	this->pDubber			= pDubber;
	this->opt				= opt;

	if (!GetWindowLong(g_hWnd, GWL_USERDATA))
		DestroyWindow(g_hWnd);
}


HWND DubStatus::Display(HWND hwndParent, int iInitialPriority) {
	iPriority = iInitialPriority;

	if (hwndStatus = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_DUBBING), hwndParent, StatusDlgProc, (LPARAM)this)) {
		if (fShowStatusWindow = opt->fShowStatus) {
			SetWindowLong(hwndStatus, GWL_STYLE, GetWindowLong(hwndStatus, GWL_STYLE) & ~WS_POPUP);
			ShowWindow(hwndStatus, SW_SHOW);
		}

		return hwndStatus;
	}
	return NULL;
}

void DubStatus::Destroy() {
	if (hwndStatus) {
		DestroyWindow(hwndStatus);
		hwndStatus = NULL;
	}
}


///////////////////////////////////////////////////////////////////////////

void DubStatus::StatusTimerProc(HWND hWnd) {
	DWORD dwProgress;
	__int64 lProjSize;
	char buf[256];

	LONG	totalVSamples	= pvinfo->end_src - pvinfo->start_src;
//	LONG	totalASamples	= painfo->end_src - painfo->start_src;
	LONG	totalASamples	= audioStreamSource ? audioStreamSource->GetLength() : 0;
	LONG	curVSample		= pvinfo->cur_proc_src - pvinfo->start_src;
	LONG	curASample		= audioStreamSource ? audioStreamSource->GetSampleCount() : 0;
	char	*s;
	bool	bPreloading = false;

	/////////////

	curVSample -= opt->video.frameRateDecimation * pvinfo->nLag;

	if (curVSample<0) {
		curVSample = 0;
		bPreloading = true;
	}

	dwProgress = (curVSample>totalVSamples ? 4096 : MulDiv(curVSample, 4096, totalVSamples))
				+(curASample>totalASamples ? 4096 : MulDiv(curASample, 4096, totalASamples));

	if (!totalASamples || !totalVSamples || pvinfo->fAudioOnly) dwProgress *= 2;

	dwLastTicks2 = dwLastTicks1;
	dwLastTicks1 = dwTicks;
	dwTicks = GetTickCount() - dwStartTime;

	if (bPreloading) {
		SetDlgItemText(hWnd, IDC_CURRENT_VFRAME, "Preloading...");
	} else {
		wsprintf(buf, "%ld/%ld", curVSample, totalVSamples);
		SetDlgItemText(hWnd, IDC_CURRENT_VFRAME, buf);
	}

	wsprintf(buf, "%ld/%ld", curASample, totalASamples);
	SetDlgItemText(hWnd, IDC_CURRENT_ASAMPLE, buf);
 
	size_to_str(buf, pvinfo->total_size);

	if (pvinfo->processed) {
		__int64 divisor = pvinfo->processed*(__int64)pvinfo->usPerFrame;

		s=buf; while(*s) ++s;
		wsprintf(s, " (%3ldK/s)", (long)((((pvinfo->total_size+1023)/1024)*1000000i64 + divisor - 1) / divisor));
	}

	SetDlgItemText(hWnd, IDC_CURRENT_VSIZE, buf);

	size_to_str(buf, painfo->total_size);
	SetDlgItemText(hWnd, IDC_CURRENT_ASIZE, buf);

	lProjSize = 0;
	if (totalVSamples && curVSample) {
		long divisor = min(totalVSamples, curVSample);

		lProjSize += ((__int64)pvinfo->total_size * totalVSamples + divisor/2) / divisor;
	}
	if (totalASamples && curASample) {
		__int64 divisor = (__int64)min(totalASamples, curASample);// * wf->nSamplesPerSec;

		lProjSize += ((__int64)painfo->total_size * 
						(__int64)totalASamples + divisor/2) / divisor;
	}

	if (lProjSize) {
		lProjSize += 2048 + 16;

		__int64 kilobytes = (lProjSize+1023)>>10;

		if (kilobytes < 65536)
			wsprintf(buf, "%ldK", kilobytes);
		else {
			kilobytes = (lProjSize*100) / 1048576;
			wsprintf(buf, "%ld.%02dMb", (LONG)(kilobytes/100), (LONG)(kilobytes%100));
		}
		SetDlgItemText(hWnd, IDC_PROJECTED_FSIZE, buf);
	} else {
		SetDlgItemText(hWnd, IDC_PROJECTED_FSIZE, "unknown");
	}

	ticks_to_str(buf, dwTicks);
	SetDlgItemText(hWnd, IDC_TIME_ELAPSED, buf);

	if (dwProgress > 16) {
		ticks_to_str(buf, MulDiv(dwTicks,8192,dwProgress));
		SetDlgItemText(hWnd, IDC_TIME_REMAINING, buf);
	}

	lFrameDiff2 = lFrameDiff1;
	lFrameDiff1 = pvinfo->processed-lastFrame;
	lastFrame += lFrameDiff1;

	{
		long fps10;

		fps10 = MulDiv(lFrameDiff1 + lFrameDiff2, 10000, dwTicks - dwLastTicks2);

		wsprintf(buf, "%ld.%c fps",fps10/10, (fps10%10) + '0');
		SetDlgItemText(hWnd, IDC_FPS, buf);
	}

	if (GetWindowLong(g_hWnd, GWL_STYLE) & WS_MINIMIZE) {
		long lNewProgress = (dwProgress*25)/2048;

		if (iLastTitleMode != TITLE_MINIMIZED || lLastTitleProgress != lNewProgress) {
			guiSetTitle(g_hWnd, IDS_TITLE_DUBBING_MINIMIZED, lNewProgress, g_szInputAVIFileTitle);

			iLastTitleMode = TITLE_MINIMIZED;
			lLastTitleProgress = lNewProgress;
		}
	} else {
		if (iLastTitleMode != TITLE_NORMAL) {
			iLastTitleMode = TITLE_NORMAL;
			guiSetTitle(g_hWnd, IDS_TITLE_DUBBING, g_szInputAVIFileTitle);
		}
	}
}

///////////////////////////////////

BOOL APIENTRY DubStatus::StatusMainDlgProc( HWND hdlg, UINT message, UINT wParam, LONG lParam) {
	DubStatus *thisPtr = (DubStatus *)GetWindowLong(hdlg, DWL_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			{
				SetWindowLong(hdlg, DWL_USER, lParam);
				thisPtr = (DubStatus *)lParam;
				SetWindowPos(hdlg, HWND_TOP, thisPtr->rStatusChild.left, thisPtr->rStatusChild.top, 0, 0, SWP_NOSIZE);

				thisPtr->StatusTimerProc(hdlg);
			}
            return (TRUE);

		case WM_TIMER:
			thisPtr->StatusTimerProc(hdlg);
			return TRUE;

    }
    return FALSE;
}



BOOL APIENTRY DubStatus::StatusVideoDlgProc( HWND hdlg, UINT message, UINT wParam, LONG lParam) {
	DubStatus *thisPtr = (DubStatus *)GetWindowLong(hdlg, DWL_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			{
				SetWindowLong(hdlg, DWL_USER, lParam);
				thisPtr = (DubStatus *)lParam;
				SetWindowPos(hdlg, HWND_TOP, thisPtr->rStatusChild.left, thisPtr->rStatusChild.top, 0, 0, SWP_NOSIZE);

				thisPtr->lFrameLobound = 0;
				thisPtr->lFrameHibound = 10240;
			}
            return (TRUE);

		case WM_TIMER:
			{
				HDC hdc;
				RECT r;
				RECT rUpdate;
				int dx;

				dx = thisPtr->lFrameLastIndex - thisPtr->lFrameFirstIndex;

				if (dx > 0) {
					long lo, hi;
					int i;

					r.left = r.top = 7;
					r.right = 7 + 133;
					r.bottom = 7 + 72;

					MapDialogRect(hdlg, &r);

					// scan the array and recompute bounds

					lo = 0x7FFFFFFF;
					hi = 0;

					for(i=r.left - r.right; i<0; i++) {
						if (thisPtr->lFrameFirstIndex + i >= 0) {
							long size = thisPtr->dwFrameSizes[(thisPtr->lFrameFirstIndex+i) & (MAX_FRAME_SIZES-1)] & 0x7FFFFFFF;

							if (size < lo)
								lo = size;

							if (size > hi)
								hi = size;
						}
					}

					// compute "nice" bounds

					if (lo == 0x7FFFFFFF)
						lo = 0;

					lo = pickClosestNiceBound(lo, false);
					hi = pickClosestNiceBound(hi, true);

					if (lo == hi)
						hi = pickClosestNiceBound(hi+1, true);

					// if the bounds are different, force a full redraw, else scroll

					thisPtr->lFrameFirstIndex += dx;
					if (lo != thisPtr->lFrameLobound || hi != thisPtr->lFrameHibound) {
						char buf[64];

						thisPtr->lFrameLobound = lo;
						thisPtr->lFrameHibound = hi;

						if (lo >= 0x40000000)
							wsprintf(buf, "%dGb", lo>>30);
						else if (lo >= 0x100000)
							wsprintf(buf, "%dMb", lo>>20);
						else if (lo >= 0x400)
							wsprintf(buf, "%dK", lo>>10);
						else
							wsprintf(buf, "%d", lo);
						SetDlgItemText(hdlg, IDC_STATIC_LOBOUND, buf);

						if (hi >= 0x40000000)
							wsprintf(buf, "%dGb", hi>>30);
						else if (hi >= 0x100000)
							wsprintf(buf, "%dMb", hi>>20);
						else if (hi >= 0x400)
							wsprintf(buf, "%dK", hi>>10);
						else
							wsprintf(buf, "%d", hi);
						SetDlgItemText(hdlg, IDC_STATIC_HIBOUND, buf);

						InvalidateRect(hdlg, &r, FALSE);
					} else if (hdc = GetDC(hdlg)) {

						ScrollDC(hdc, -dx, 0, &r, &r, NULL, &rUpdate);

						rUpdate.left = r.right - dx;
						rUpdate.right = r.right;
						rUpdate.top = r.top;
						rUpdate.bottom = r.bottom;

						InvalidateRect(hdlg, &rUpdate, FALSE);

						ReleaseDC(hdlg, hdc);
					}
				}

			}
			return TRUE;

		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc;
				RECT r, r2;
				RECT rDest;
				HBRUSH hbrRed, hbrBlue;
				int x, width, height;
				long range = thisPtr->lFrameHibound - thisPtr->lFrameLobound;

				if (!range) ++range;

				r.left = r.top = 7;
				r.right = 7+133;
				r.bottom = 72;

				MapDialogRect(hdlg, &r);

				width = r.right - r.left;
				height = r.bottom - r.top;

				hdc = BeginPaint(hdlg, &ps);

				IntersectRect(&rDest, &r, &ps.rcPaint);

				FillRect(hdc, &rDest, (HBRUSH)GetStockObject(BLACK_BRUSH));

				hbrRed = CreateSolidBrush(RGB(255,0,0));
				hbrBlue = CreateSolidBrush(RGB(0,0,255));

				for(x=rDest.left; x<rDest.right; x++) {
					DWORD dwSize;
					int y;

					if (thisPtr->lFrameFirstIndex+x-r.right >= 0) {
						dwSize = thisPtr->dwFrameSizes[(thisPtr->lFrameFirstIndex+x-r.right) & (MAX_FRAME_SIZES-1)];

						y = (((dwSize & 0x7FFFFFFF) - thisPtr->lFrameLobound)*height + range - 1)/range;
						if (y > height)
							y = height;

						if (y>0) {

							r2.left = x;
							r2.right = x+1;
							r2.top = r.bottom - y;
							r2.bottom = r.bottom;

							FillRect(hdc, &r2, dwSize & 0x80000000 ? hbrBlue : hbrRed);
						}
					}
				}

				DeleteObject(hbrBlue);
				DeleteObject(hbrRed);

				EndPaint(hdlg, &ps);
			}
			return TRUE;

    }
    return FALSE;
}

BOOL APIENTRY DubStatus::StatusPerfDlgProc( HWND hdlg, UINT message, UINT wParam, LONG lParam) {
	DubStatus *thisPtr = (DubStatus *)GetWindowLong(hdlg, DWL_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			{
				SetWindowLong(hdlg, DWL_USER, lParam);
				thisPtr = (DubStatus *)lParam;
				SetWindowPos(hdlg, HWND_TOP, thisPtr->rStatusChild.left, thisPtr->rStatusChild.top, 0, 0, SWP_NOSIZE);

			}
            return (TRUE);

		case WM_TIMER:
			if (thisPtr->pInput) {
				SetDlgItemText(hdlg, IDC_STATIC_OPTPREVIEW,
					thisPtr->pInput->isOptimizedForRealtime() ? "Yes" : "No");

				SetDlgItemText(hdlg, IDC_STATIC_READMODE,
					thisPtr->pInput->isStreaming() ? "Streaming" : "Discrete");

				if (thisPtr->vSrc)
					SetDlgItemText(hdlg, IDC_STATIC_VIDEOSTREAM,
						thisPtr->vSrc->isStreaming() ? "Yes" : "No");

				if (thisPtr->aSrc)
					SetDlgItemText(hdlg, IDC_STATIC_AUDIOSTREAM,
						thisPtr->aSrc->isStreaming() ? "Yes" : "No");
			}
			return TRUE;

    }
    return FALSE;
}

///////////////////////////////////

const char * const g_szDubPriorities[]={
		"Idle",
		"Lowest",
		"Even lower",
		"Lower",
		"Normal",
		"Higher",
		"Even higher",
		"Highest",
};

BOOL APIENTRY DubStatus::StatusDlgProc( HWND hdlg, UINT message, UINT wParam, LONG lParam) {

	static struct DubStatusTabs {
		LPTSTR	rsrc;
		char	*name;
		DLGPROC	dProc;
	} tabs[]={
		{	MAKEINTRESOURCE(IDD_DUBBING_MAIN),	"Main",		StatusMainDlgProc	},
		{	MAKEINTRESOURCE(IDD_DUBBING_VIDEO),	"Video",	StatusVideoDlgProc	},
		{	MAKEINTRESOURCE(IDD_DUBBING_PERF),	"Perf",		StatusPerfDlgProc	},
	};

	DubStatus *thisPtr = (DubStatus *)GetWindowLong(hdlg, DWL_USER);
	HWND hwndItem;
	RECT r, r2;
	int i;

#define MYWM_NULL (WM_APP + 0)

    switch (message)
    {
        case WM_INITDIALOG:
			{
				long xoffset, yoffset;

				SetWindowLong(hdlg, DWL_USER, lParam);
				thisPtr = (DubStatus *)lParam;

				thisPtr->hwndStatus = hdlg;

				// Initialize tab window

				hwndItem = GetDlgItem(hdlg, IDC_TABS);

				for(i=0; i<(sizeof tabs/sizeof tabs[0]); i++) {
					TC_ITEM ti;

					ti.mask		= TCIF_TEXT;
					ti.pszText	= tabs[i].name;

					TabCtrl_InsertItem(hwndItem, i, &ti);
				}

				// Compute size of tab control needed to hold this child dialog

				r.left = r.top = 0;
				r.right = 172;
				r.bottom = 102;
				MapDialogRect(hdlg, &r);

				TabCtrl_AdjustRect(hwndItem, TRUE, &r);

				// Resize tab control and compute offsets for other controls

				GetWindowRect(hwndItem, &r2);
				ScreenToClient(hdlg, (LPPOINT)&r2 + 0);
				ScreenToClient(hdlg, (LPPOINT)&r2 + 1);

				OffsetRect(&r, r2.left - r.left, r2.top - r.top);

				SetWindowPos(hwndItem, NULL, r.left, r.top, r.right-r.left, r.bottom-r.top, SWP_NOZORDER);
				thisPtr->rStatusChild = r;

				TabCtrl_AdjustRect(hwndItem, FALSE, &thisPtr->rStatusChild);

				xoffset = (r.right-r.left) - (r2.right-r2.left);
				yoffset = (r.bottom-r.top) - (r2.bottom-r2.top);

				guiResizeDlgItem(hdlg, IDC_PROGRESS, 0, yoffset, xoffset, 0);
				guiResizeDlgItem(hdlg, IDC_PRIORITY, 0, yoffset, xoffset, 0);
				guiOffsetDlgItem(hdlg, IDC_ABORT, xoffset, yoffset);
				guiOffsetDlgItem(hdlg, IDC_STATIC_PROGRESS, 0, yoffset);
				guiOffsetDlgItem(hdlg, IDC_STATIC_PRIORITY, 0, yoffset);
				guiOffsetDlgItem(hdlg, IDC_DRAW_INPUT, 0, yoffset);
				guiOffsetDlgItem(hdlg, IDC_DRAW_OUTPUT, 0, yoffset);

				// resize us

				GetWindowRect(hdlg, &r);
				SetWindowPos(hdlg, NULL, 0, 0, r.right-r.left + xoffset, r.bottom-r.top + yoffset, SWP_NOMOVE | SWP_NOZORDER);

				// open up child dialog

				thisPtr->hwndStatusChild = CreateDialogParam(g_hInst, tabs[0].rsrc, hdlg, tabs[0].dProc, (LPARAM)thisPtr);

				// setup timer, progress bar

				thisPtr->statTimer = SetTimer(hdlg, 1, 500, NULL);
				SendMessage(GetDlgItem(hdlg, IDC_PROGRESS), PBM_SETRANGE, 0, MAKELPARAM(0, 8192));
				thisPtr->dwStartTime	= GetTickCount();
				thisPtr->lastFrame		= 0;
				thisPtr->lFrameDiff1	= 0;
				thisPtr->dwTicks		= 0;
				thisPtr->dwLastTicks1	= 0;

				CheckDlgButton(hdlg, IDC_DRAW_INPUT, thisPtr->opt->video.fShowInputFrame);
				CheckDlgButton(hdlg, IDC_DRAW_OUTPUT, thisPtr->opt->video.fShowOutputFrame);

				hwndItem = GetDlgItem(hdlg, IDC_PRIORITY);
				SendMessage(hwndItem, CB_RESETCONTENT,0,0);
				for(i=0; i<8; i++)
					SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)g_szDubPriorities[i]);

				SendMessage(hwndItem, CB_SETCURSEL, thisPtr->iPriority-1, 0);

				guiSetTitle(hdlg, IDS_TITLE_STATUS,  g_szInputAVIFileTitle);

			}
            return (TRUE);

		case WM_DESTROY:
			thisPtr->hwndStatus = NULL;
			if (thisPtr->statTimer)
				KillTimer(hdlg, thisPtr->statTimer);
			PostMessage(GetParent(hdlg), MYWM_NULL, 0, 0);
			return TRUE;

		case WM_TIMER:
			if (thisPtr->fFrozen)
				return TRUE;

			thisPtr->SetLastPosition(thisPtr->pvinfo->cur_proc_src);

			if (thisPtr->hwndStatusChild)
				SendMessage(thisPtr->hwndStatusChild, WM_TIMER, 0, 0);

			{
				DWORD dwProgress;

				LONG	totalVSamples	= thisPtr->pvinfo->end_src - thisPtr->pvinfo->start_src;
				LONG	totalASamples	= thisPtr->audioStreamSource ? thisPtr->audioStreamSource->GetLength() : 0;
//				LONG	totalASamples	= thisPtr->painfo->end_src - thisPtr->painfo->start_src;
				LONG	curVSample		= thisPtr->pvinfo->cur_proc_src - thisPtr->pvinfo->start_src;
				LONG	curASample		= thisPtr->audioStreamSource ? thisPtr->audioStreamSource->GetSampleCount() : 0;

				/////////////

				dwProgress = (curVSample>totalVSamples ? 4096 : MulDiv(curVSample, 4096, totalVSamples))
							+(curASample>totalASamples ? 4096 : MulDiv(curASample, 4096, totalASamples));

				if (!totalASamples || !totalVSamples || thisPtr->pvinfo->fAudioOnly) dwProgress *= 2;

				SendMessage(GetDlgItem(hdlg, IDC_PROGRESS), PBM_SETPOS,	(WPARAM)dwProgress, 0);
			}

			thisPtr->pDubber->UpdateFrames();

			return TRUE;

		case WM_NOTIFY: {
			NMHDR *nm = (LPNMHDR)lParam;

			switch(nm->code) {
			case TCN_SELCHANGE:
				{
					int iTab = TabCtrl_GetCurSel(nm->hwndFrom);

					if (iTab>=0) {
						if (thisPtr->hwndStatusChild)
							DestroyWindow(thisPtr->hwndStatusChild);

						thisPtr->hwndStatusChild = CreateDialogParam(g_hInst, tabs[iTab].rsrc, hdlg, tabs[iTab].dProc, (LPARAM)thisPtr);
					}
				}
				return TRUE;
			}
			}break;

        case WM_COMMAND:                      
			switch(LOWORD(wParam)) {
			case IDC_DRAW_INPUT:
				thisPtr->opt->video.fShowInputFrame = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)==BST_CHECKED;
				break;

			case IDC_DRAW_OUTPUT:
				thisPtr->opt->video.fShowOutputFrame = SendMessage((HWND)lParam, BM_GETCHECK, 0, 0)==BST_CHECKED;
				break;

			case IDC_PRIORITY:
				if (HIWORD(wParam) == CBN_SELCHANGE) {
					LRESULT index;

					if (CB_ERR != (index = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0))) {
						thisPtr->pDubber->SetPriority(index);
					}
				}
				break;

			case IDC_ABORT:
				SendMessage(hdlg, WM_SETTEXT, 0, (LPARAM)"Aborting...");
				EnableWindow((HWND)lParam, FALSE);
				thisPtr->pDubber->Abort();
				thisPtr->hwndStatus = NULL;
				DestroyWindow(hdlg);
				break;

			case IDCANCEL:
				_RPT0(0,"Received cancel\n");
				thisPtr->ToggleStatus();
				break;
            }
            break;
    }
    return FALSE;
}


bool DubStatus::ToggleStatus() {

	fShowStatusWindow = !fShowStatusWindow;

	if (hwndStatus) {
		if (fShowStatusWindow) {
			SetWindowLong(hwndStatus, GWL_STYLE, GetWindowLong(hwndStatus, GWL_STYLE) & ~WS_POPUP);
			ShowWindow(hwndStatus, SW_SHOW);
		} else {
			SetWindowLong(hwndStatus, GWL_STYLE, GetWindowLong(hwndStatus, GWL_STYLE) | WS_POPUP);
			ShowWindow(hwndStatus, SW_HIDE);
		}
	}

	return fShowStatusWindow;
}

void DubStatus::SetPositionCallback(DubPositionCallback dpc) {
	positionCallback = dpc;
}

void DubStatus::NotifyNewFrame(long f) {
	dwFrameSizes[(lFrameLastIndex++)&(MAX_FRAME_SIZES-1)] = (DWORD)f;
}

void DubStatus::SetLastPosition(LONG pos) {
		if (positionCallback)
			positionCallback(
					pvinfo->start_src,
					pos < pvinfo->start_src
							? pvinfo->start_src
							: pos > pvinfo->end_src
									? pvinfo->end_src
									: pos,
					pvinfo->end_src);
}

void DubStatus::Freeze() {
	fFrozen = true;
}

bool DubStatus::isVisible() {
	return fShowStatusWindow;
}

bool DubStatus::isFrameVisible(bool fOutput) {
	return !!(fOutput ? opt->video.fShowOutputFrame : opt->video.fShowInputFrame);
}

bool DubStatus::ToggleFrame(bool fFrameOutput) {
	if (fFrameOutput) {
		if (hwndStatus)
			PostMessage(GetDlgItem(hwndStatus, IDC_DRAW_OUTPUT), BM_SETCHECK, !opt->video.fShowOutputFrame ? BST_CHECKED : BST_UNCHECKED, 0);
		return opt->video.fShowOutputFrame = !opt->video.fShowOutputFrame;
	} else {
		if (hwndStatus)
			PostMessage(GetDlgItem(hwndStatus, IDC_DRAW_INPUT), BM_SETCHECK, !opt->video.fShowInputFrame ? BST_CHECKED : BST_UNCHECKED, 0);
		return opt->video.fShowInputFrame = !opt->video.fShowInputFrame;
	}
}
