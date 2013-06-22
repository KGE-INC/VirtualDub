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
#include <windows.h>
#include <vfw.h>

#include "resource.h"
#include "oshelper.h"
#include "vbitmap.h"
#include "helpfile.h"
#include "Histogram.h"
#include <vd2/system/error.h>

#include "caphisto.h"

extern LRESULT CALLBACK CaptureStatusCallback(HWND hWnd, int nID, LPCSTR lpsz);

///////////////////////////////////////////////////////////////////////////
//
//	General capture histogram
//
///////////////////////////////////////////////////////////////////////////

CaptureHistogram::CaptureHistogram(HWND hwndCapture, HDC hdcExample, int max_height)
	:CaptureFrameSource(hwndCapture)
	,histo(hdcExample, max_height)
	{
}

CaptureHistogram::~CaptureHistogram() {
}

void CaptureHistogram::Process(VIDEOHDR *pvhdr) {
	const VBitmap& vbAnalyze = *Decompress(pvhdr);

	histo.Zero();

	if (vbAnalyze.depth == 32)
		histo.Process(&vbAnalyze);
	else if (vbAnalyze.depth == 24)
		histo.Process24(&vbAnalyze);
	else if (vbAnalyze.depth == 16)
		histo.Process16(&vbAnalyze);
}

void CaptureHistogram::Draw(HDC hdc, RECT& r) {
	histo.Draw(hdc, &r);
}

///////////////////////////////////////////////////////////////////////////
//
//	Active histogram
//
///////////////////////////////////////////////////////////////////////////

// VideoSource.cpp
extern void DIBconvert(void *src, BITMAPINFOHEADER *srcfmt, void *dst, BITMAPINFOHEADER *dstfmt);

typedef struct HistogramDlgData {
	HWND hDlg;
	HWND hwndCapture; //, hwndStatus;
	LONG fsize;
	BITMAPINFOHEADER *bmih, bmihDecomp;
	CAPTUREPARMS cp, cp_back;
	UINT uTimer;
	void *buffer;
	HIC hic;
	HDC hdc;
	RECT rHisto;
	VBitmap vbitmap;
	Histogram *histo;

	BOOL fParmsSet, fCapture, fCaptureStupid, fCompressionOk;
} HistogramDlgData;

LRESULT CALLBACK CaptureHistogramStatusCallbackProc(HWND hWnd, int nID, LPCSTR lpsz) {
	HistogramDlgData *hdd = (HistogramDlgData *)capGetUserData(hWnd);

	switch(nID) {
	case IDS_CAP_BEGIN:
		hdd->fCapture = TRUE;
		break;
	case IDS_CAP_END:
		hdd->fCapture = FALSE;
		PostMessage(hdd->hDlg, WM_COMMAND, 0, 0);
		break;
	}

	return 0;
}

static LRESULT CALLBACK CaptureHistogramVideoCallbackProc(HWND hWnd, LPVIDEOHDR lpVHdr)
{
	HistogramDlgData *hdd = (HistogramDlgData *)capGetUserData(hWnd);
	DWORD err;
	char buf[128];
	CAPSTATUS capStatus;

	capGetStatus(hWnd, (LPARAM)&capStatus, sizeof(CAPSTATUS));

	hdd->bmih->biSizeImage = lpVHdr->dwBytesUsed;

	if (hdd->hic) {

		err = ICDecompress(
				hdd->hic,
				lpVHdr->dwFlags & VHDR_KEYFRAME ? AVIIF_KEYFRAME : 0,
				hdd->bmih,
				lpVHdr->lpData,
				&hdd->bmihDecomp,
				hdd->buffer);

		if (err != ICERR_OK) {
			wsprintf(buf,"decompression error %08lx",err);
			SendMessage(hdd->hDlg, WM_SETTEXT, 0, (LPARAM)buf);
//			RedrawWindow(hdd->hwndStatus, NULL, NULL, RDW_INVALIDATE|RDW_UPDATENOW);
			return 0;
		}

	} else {
		DIBconvert(lpVHdr->lpData, hdd->bmih, hdd->buffer, &hdd->bmihDecomp);
	}

	hdd->histo->Zero();
	hdd->histo->Process24(&hdd->vbitmap);
	hdd->histo->Draw(hdd->hdc, &hdd->rHisto);

//	if (!hdd->hic) {
		wsprintf(buf, "Histogram: %ld(%ld) frames",capStatus.dwCurrentVideoFrame,capStatus.dwCurrentVideoFramesDropped);
		SendMessage(hdd->hDlg, WM_SETTEXT, 0, (LPARAM)buf);
//	}

//	RedrawWindow(hdd->hwndStatus, NULL, NULL, RDW_INVALIDATE|RDW_UPDATENOW);
	return 0;
}

static void CaptureHistogramDestruct(HWND hwnd, HistogramDlgData *hdd) {
	if (hdd->fParmsSet)
		capCaptureSetSetup(hdd->hwndCapture, &hdd->cp_back, sizeof(CAPTUREPARMS));

	freemem(hdd->buffer);
	if (hdd->fCompressionOk)	ICDecompressEnd(hdd->hic);
	if (hdd->hic)				ICClose(hdd->hic);

	delete hdd->histo;
	freemem(hdd->bmih);
	if (hdd->hdc) ReleaseDC(hwnd, hdd->hdc);
	capSetCallbackOnFrame(hdd->hwndCapture, (LPVOID)NULL);
	capSetCallbackOnVideoStream(hdd->hwndCapture, (LPVOID)NULL);
	capSetCallbackOnStatus(hdd->hwndCapture, (LPVOID)CaptureStatusCallback);

	delete hdd;
}

BOOL APIENTRY CaptureHistogramDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	HistogramDlgData *hdd = (HistogramDlgData *)GetWindowLong(hDlg, DWL_USER);

	switch(message) {
		case WM_INITDIALOG:
			try {
				RECT r, rc;
				DWORD err;

				////

				if (!(hdd = new HistogramDlgData)) throw MyError("Out of memory");
				memset(hdd, 0, sizeof(HistogramDlgData));

				hdd->hDlg = hDlg;
				hdd->hwndCapture = (HWND)lParam;
//				hdd->hwndStatus = GetDlgItem(GetParent(hdd->hwndCapture), IDC_STATUS_WINDOW);

				if (!(hdd->fsize = capGetVideoFormatSize((HWND)lParam))
					|| !(hdd->bmih = (BITMAPINFOHEADER *)allocmem(hdd->fsize))
					|| !capGetVideoFormat(hdd->hwndCapture, hdd->bmih, hdd->fsize)
					)
					throw MyError("Couldn't get video format.");

				memcpy(&hdd->bmihDecomp, hdd->bmih, sizeof(BITMAPINFOHEADER));
				hdd->bmihDecomp.biSize			= sizeof(BITMAPINFOHEADER);
				hdd->bmihDecomp.biPlanes		= 1;
				hdd->bmihDecomp.biBitCount		= 24;
				hdd->bmihDecomp.biCompression	= BI_RGB;
				hdd->bmihDecomp.biSizeImage		= ((hdd->bmihDecomp.biWidth*3+3)&-4) * hdd->bmihDecomp.biHeight;

				// allocate screenbuffer

				if (!(hdd->buffer = allocmem(hdd->bmihDecomp.biSizeImage+4))) throw MyError("Out of memory");

				// initialize VirtualBitmap

				hdd->vbitmap.data	= (Pixel *)hdd->buffer;
				hdd->vbitmap.w		= hdd->bmihDecomp.biWidth;
				hdd->vbitmap.h		= hdd->bmihDecomp.biHeight;
				hdd->vbitmap.pitch	= (hdd->bmihDecomp.biWidth*3+3)&-4;
				hdd->vbitmap.modulo	= hdd->vbitmap.pitch - hdd->vbitmap.w*3;

				SetWindowLong(hDlg, DWL_USER, (DWORD)hdd);

				// resize requester to accommodate histogram

				GetWindowRect(hDlg, &r);
				GetClientRect(hDlg, &rc);
				SetWindowPos(hDlg, NULL, 0, 0, (r.right-r.left)-(rc.right-rc.left)+256, (r.bottom-r.top)-(rc.bottom-rc.top)+128, SWP_NOACTIVATE|SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER);

				hdd->rHisto.left = hdd->rHisto.top = 0;
				hdd->rHisto.right = 256;
				hdd->rHisto.bottom = 128;

				// allocate a Histogram

				if (!(hdd->hdc = GetDC(hDlg)))
					throw MyError("No display context available");

				if (!(hdd->histo = new Histogram(hdd->hdc, 128)))
					throw MyError("No histogram!  *sniff*");

				hdd->histo->SetMode(Histogram::MODE_GRAY);

				// change parms

				if (!capCaptureGetSetup(hdd->hwndCapture, &hdd->cp, sizeof(CAPTUREPARMS)))
					throw MyError("Couldn't get capture parameters");

				hdd->cp_back = hdd->cp;

				hdd->cp.fCaptureAudio			= FALSE;
				hdd->cp.fMakeUserHitOKToCapture	= FALSE;
				hdd->cp.fYield					= TRUE;
				hdd->cp.vKeyAbort				= 0;
				hdd->cp.fAbortLeftMouse			= FALSE;
				hdd->cp.fAbortRightMouse		= FALSE;
				hdd->cp.wTimeLimit				= FALSE;
				hdd->cp.fMCIControl				= FALSE;

				if (!capCaptureSetSetup(hdd->hwndCapture, &hdd->cp, sizeof(CAPTUREPARMS)))
					throw MyError("Couldn't set capture parameters");

				hdd->fParmsSet = TRUE;

				// point capture window to us

				capSetUserData(hdd->hwndCapture, (LPARAM)hdd);

				// send off a timer message to kickstart the op

				capSetCallbackOnStatus(hdd->hwndCapture, (LPVOID)CaptureHistogramStatusCallbackProc);

				if (hdd->bmih->biCompression != BI_RGB) {

					// attempt to find a decompressor

//					hdd->hic = ICLocate('CDIV', NULL, hdd->bmih, NULL/*&hdd->bmihDecomp*/, ICMODE_DECOMPRESS);
					hdd->hic = ICOpen(ICTYPE_VIDEO, hdd->bmih->biCompression, ICMODE_DECOMPRESS);
					if (!hdd->hic)
						hdd->hic = ICLocate(ICTYPE_VIDEO, NULL, hdd->bmih, NULL, ICMODE_DECOMPRESS);

					if (!hdd->hic)
						throw MyError("Couldn't find a decompressor to 24-bit RGB video");

					if (ICERR_OK != (err = ICDecompressBegin(hdd->hic, hdd->bmih, &hdd->bmihDecomp)))
						throw MyICError("Failure init'ing decompression", err);

					hdd->fCompressionOk = TRUE;

					capSetCallbackOnFrame(hdd->hwndCapture, (LPVOID)CaptureHistogramVideoCallbackProc);
					hdd->fCaptureStupid = TRUE;
					SetTimer(hDlg, 1, 0, NULL);

				} else {

					hdd->fCapture = TRUE;
					capSetCallbackOnVideoStream(hdd->hwndCapture, (LPVOID)CaptureHistogramVideoCallbackProc);
					PostMessage(hdd->hwndCapture, WM_CAP_SEQUENCE_NOFILE, 0, 0);
				}
			} catch(const MyError& e) {
				e.post(hDlg,"Histogram error");

				if (hdd) CaptureHistogramDestruct(hDlg, hdd);

				EndDialog(hDlg, FALSE);
			}
			break;

		case WM_LBUTTONUP:
			hdd->histo->SetMode(Histogram::MODE_NEXT);
			return TRUE;

		case WM_SYSCOMMAND:
			if ((wParam & 0xFFF0) == SC_CONTEXTHELP) {
				HelpPopup(hDlg, IDH_CAPTURE_HISTOGRAM);
				return TRUE;
			}
			return FALSE;

		case WM_COMMAND:
			if (hdd) {
				if (hdd->fCapture) {
					capCaptureStop(hdd->hwndCapture);
					return TRUE;
				}
				CaptureHistogramDestruct(hDlg, hdd);
			}
			EndDialog(hDlg, 0);
			return TRUE;

		case WM_TIMER:
			if (hdd->fCaptureStupid)
				capGrabFrame(hdd->hwndCapture);

			return TRUE;
	}

	return FALSE;
}
