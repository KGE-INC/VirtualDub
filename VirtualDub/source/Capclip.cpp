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
#include <vd2/system/error.h>
#include "ClippingControl.h"

#include "capclip.h"

extern HINSTANCE g_hInst;

extern LRESULT CALLBACK CaptureStatusCallback(HWND hWnd, int nID, LPCSTR lpsz);

CaptureFrameSource::CaptureFrameSource(HWND hwndCapture)
	:bmihSrc(NULL)
	,hic(NULL)
	,fDecompressionOk(false)
	,pFrameBuffer(NULL)
	{

	try {
		int fsize;

		// determine native video format of capture device

		if ((fsize = capGetVideoFormatSize(hwndCapture))<=0)
			throw MyError("Couldn't get video format.");

		if (!(bmihSrc = (BITMAPINFOHEADER *)allocmem(fsize)))
			throw MyMemoryError();

		if (!capGetVideoFormat(hwndCapture, bmihSrc, fsize))
			throw MyError("Couldn't get video format.");

		// Construct a format for uncompressed RGB, if we don't already have one.

		if (bmihSrc->biCompression != BI_RGB) {

			// Attempt RGB24 first.

			memcpy(&bmihDecomp, bmihSrc, sizeof(BITMAPINFOHEADER));
			bmihDecomp.biSize			= sizeof(BITMAPINFOHEADER);
			bmihDecomp.biPlanes			= 1;
			bmihDecomp.biBitCount		= 24;
			bmihDecomp.biCompression	= BI_RGB;
			bmihDecomp.biSizeImage		= ((bmihDecomp.biWidth*3+3)&-4) * bmihDecomp.biHeight;

			// Attempt to find a decompressor.

//			hic = ICOpen('CDIV', bmihSrc->biCompression, ICMODE_DECOMPRESS);

			hic = ICLocate(ICTYPE_VIDEO, bmihSrc->biCompression, bmihSrc, NULL, ICMODE_DECOMPRESS);

			if (!hic)
				hic = ICLocate(ICTYPE_VIDEO, NULL, bmihSrc, NULL, ICMODE_DECOMPRESS);

			if (!hic)
				throw MyError("No decompressor is available for the current capture format.");

			// Try some formats.  24-bit first, then 32-bit.

			do {

				// See if RGB24 works.

				if (ICERR_OK == ICDecompressBegin(hic, bmihSrc, &bmihDecomp))
					break;

				// Nope, try RGB32.

				bmihDecomp.biBitCount		= 32;
				bmihDecomp.biCompression	= BI_RGB;
				bmihDecomp.biSizeImage		= bmihDecomp.biWidth*4 * bmihDecomp.biHeight;

				if (ICERR_OK == ICDecompressBegin(hic, bmihSrc, &bmihDecomp))
					break;

				// Ick, try RGB15.

				bmihDecomp.biBitCount		= 16;
				bmihDecomp.biCompression	= BI_RGB;
				bmihDecomp.biSizeImage		= ((bmihDecomp.biWidth+1)&-2)*2 * bmihDecomp.biHeight;

				if (ICERR_OK == ICDecompressBegin(hic, bmihSrc, &bmihDecomp))
					break;

				// Nope, we're screwed.

				throw MyError("Cannot find a way to decompress capture data to RGB format.");

			} while(false);

			// Okay, decompressor is ready to go.  Cool.

			fDecompressionOk = true;

			// Now that we have the format, we need this 'framebuffer' thing.

			pFrameBuffer = VirtualAlloc(NULL, bmihDecomp.biSizeImage + (bmihDecomp.biBitCount==24 ? 1 : 0), MEM_COMMIT, PAGE_READWRITE);

			if (!pFrameBuffer)
				throw MyMemoryError();

			// Create a suitable VBitmap for the framebuffer.

			vbAnalyze = VBitmap(pFrameBuffer, &bmihDecomp);

		} else {	// uncompressed RGB already

			// Create a suitable VBitmap for the incoming stream.

			vbAnalyze = VBitmap(NULL, bmihSrc);

		}

	} catch(const MyError&) {
		_destruct();
		throw;
	}
}

void CaptureFrameSource::_destruct() {
	if (bmihSrc)			{ freemem(bmihSrc); bmihSrc = NULL; }
	if (fDecompressionOk)	{ ICDecompressEnd(hic); fDecompressionOk = false; }
	if (hic)				{ ICClose(hic); hic = NULL; }
	if (pFrameBuffer)		{ VirtualFree(pFrameBuffer, 0, MEM_RELEASE); pFrameBuffer = NULL; }
}

CaptureFrameSource::~CaptureFrameSource() {
	_destruct();
}

bool CaptureFrameSource::CheckFrameSize(int w, int h) {
	return w == bmihSrc->biWidth && bmihSrc->biHeight;
}

const VBitmap *CaptureFrameSource::Decompress(VIDEOHDR *pvhdr) {
	if (hic) {
		DWORD err;

		err = ICDecompress(
				hic,
				pvhdr->dwFlags & VHDR_KEYFRAME ? AVIIF_KEYFRAME : 0,
				bmihSrc,
				pvhdr->lpData,
				&bmihDecomp,
				pFrameBuffer);

		if (err != ICERR_OK)
			throw MyICError("Histogram", err);
	} else
		vbAnalyze.data = (Pixel *)pvhdr->lpData;

	return &vbAnalyze;
}

///////////////////////////////////////////////////////////////////////////

static CaptureFrameSource *g_pClippingDecoder;
static HWND g_hwndClippingDisplay;

LRESULT CALLBACK CaptureClippingFrameProc(HWND hWnd, VIDEOHDR *vhdr) {
	BITMAPINFOHEADER bih;

	try {
		const VBitmap *vbm = g_pClippingDecoder->Decompress(vhdr);

		vbm->MakeBitmapHeader(&bih);

		SendMessage(g_hwndClippingDisplay, CCM_BLITFRAME, (WPARAM)&bih, (LPARAM)vbm->data);
	} catch(const MyError&) {
	}

	return 0;
}

#pragma vdpragma_TODO("The code below is really, really wrong")
RECT g_rCaptureClip;

INT_PTR CALLBACK CaptureClippingDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
        case WM_INITDIALOG:
			{
				ClippingControlBounds ccb;
				LONG hborder, hspace;
				RECT rw, rc, rcok, rccancel;
				HWND hWnd, hWndCancel;

				g_hwndClippingDisplay = hWnd = GetDlgItem(hDlg, IDC_BORDERS);
				ccb.x1	= g_rCaptureClip.left;
				ccb.x2	= g_rCaptureClip.right;
				ccb.y1	= g_rCaptureClip.top;
				ccb.y2	= g_rCaptureClip.bottom;

//				capGetStatus(hwndCapture, &cs, sizeof cs);
//				SendMessage(hWnd, CCM_SETBITMAPSIZE, 0, MAKELONG(cs.uiImageWidth,cs.uiImageHeight));
				SendMessage(hWnd, CCM_SETBITMAPSIZE, 0, MAKELONG(320,240));
				SendMessage(hWnd, CCM_SETCLIPBOUNDS, 0, (LPARAM)&ccb);

				GetWindowRect(hDlg, &rw);
				GetWindowRect(hWnd, &rc);
				hborder = rc.left - rw.left;
				ScreenToClient(hDlg, (LPPOINT)&rc.left);
				ScreenToClient(hDlg, (LPPOINT)&rc.right);

				SetWindowPos(hDlg, NULL, 0, 0, (rc.right - rc.left) + hborder*2, (rw.bottom-rw.top)+(rc.bottom-rc.top), SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOMOVE);

				hWndCancel = GetDlgItem(hDlg, IDCANCEL);
				hWnd = GetDlgItem(hDlg, IDOK);
				GetWindowRect(hWnd, &rcok);
				GetWindowRect(hWndCancel, &rccancel);
				hspace = rccancel.left - rcok.right;
				ScreenToClient(hDlg, (LPPOINT)&rcok.left);
				ScreenToClient(hDlg, (LPPOINT)&rcok.right);
				ScreenToClient(hDlg, (LPPOINT)&rccancel.left);
				ScreenToClient(hDlg, (LPPOINT)&rccancel.right);
				SetWindowPos(hWndCancel, NULL, rc.right - (rccancel.right-rccancel.left), rccancel.top + (rc.bottom-rc.top), 0,0,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
				SetWindowPos(hWnd, NULL, rc.right - (rccancel.right-rccancel.left) - (rcok.right-rcok.left) - hspace, rcok.top + (rc.bottom-rc.top), 0,0,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);

				SetTimer(hDlg, 1, 500, NULL);

//				if (g_pClippingDecoder)
//					capSetCallbackOnFrame(hwndCapture, CaptureClippingFrameProc);
			}

            return (TRUE);

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					ClippingControlBounds ccb;

					SendMessage(GetDlgItem(hDlg, IDC_BORDERS), CCM_GETCLIPBOUNDS, 0, (LPARAM)&ccb);
					g_rCaptureClip.left = ccb.x1;
					g_rCaptureClip.top = ccb.y1;
					g_rCaptureClip.right = ccb.x2;
					g_rCaptureClip.bottom = ccb.y2;
//					capSetCallbackOnFrame(hwndCapture, NULL);
					EndDialog(hDlg, TRUE);
				}
				return TRUE;
			case IDCANCEL:
//				capSetCallbackOnFrame(hwndCapture, NULL);
				EndDialog(hDlg, FALSE);
				return TRUE;
			case IDC_BORDERS:
//				guiPositionBlit((HWND)lParam, guiPositionHandleCommand(wParam, lParam));
				return TRUE;
			}
            break;

		case WM_TIMER:
//			capGrabFrameNoStop(hwndCapture);
			break;

		case WM_NOTIFY:
			{
				const VBitmap *vbm = g_pClippingDecoder->getFrameBuffer();
				BITMAPINFOHEADER bih;

				vbm->MakeBitmapHeader(&bih);

				SendMessage(g_hwndClippingDisplay, CCM_BLITFRAME, (WPARAM)&bih, (LPARAM)vbm->data);
			}
			break;

    }
    return FALSE;
}

void CaptureShowClippingDialog(HWND hwndCapture) {
	g_pClippingDecoder = new CaptureFrameSource(hwndCapture);
	CAPSTATUS cs;

	capGetStatus(hwndCapture, &cs, sizeof cs);
	capOverlay(hwndCapture, FALSE);
	capPreview(hwndCapture, FALSE);

	DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_CLIPPING), GetParent(hwndCapture), CaptureClippingDlgProc, (LPARAM)hwndCapture);

	if (cs.fOverlayWindow)
		capOverlay(hwndCapture, TRUE);

	if (cs.fLiveWindow)
		capPreview(hwndCapture, TRUE);

	delete g_pClippingDecoder;
}
