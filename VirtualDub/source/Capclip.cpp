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

#include "resource.h"
#include "oshelper.h"
#include "vbitmap.h"
#include "helpfile.h"
#include <vd2/system/error.h>
#include <vd2/system/thread.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "ClippingControl.h"

#include "capture.h"
#include "capclip.h"
#include "gui.h"

extern HINSTANCE g_hInst;

///////////////////////////////////////////////////////////////////////////

class VDDialogCaptureCropping : public VDDialogBaseW32, public VDCaptureProjectBaseCallback {
public:
	VDDialogCaptureCropping(IVDCaptureProject *pProject) : VDDialogBaseW32(IDD_CAPTURE_CLIPPING), mpProject(pProject) {}

protected:
	INT_PTR DlgProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void OnInit();
	void OnCleanup();

	void UICaptureAnalyzeFrame(const VDPixmap& format);

	IVDCaptureProject		*mpProject;
	nsVDCapture::DisplayMode	mOldDisplayMode;

	IVDCaptureProjectCallback	*mpOldCallback;
	VDCaptureFilterSetup	mFilterSetup;

	VDCriticalSection	mDisplayBufferLock;
	VDPixmapBuffer		mDisplayBuffer;
};

INT_PTR VDDialogCaptureCropping::DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg)
    {
        case WM_INITDIALOG:
			OnInit();
            return TRUE;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					ClippingControlBounds ccb;

					SendMessage(GetDlgItem(mhdlg, IDC_BORDERS), CCM_GETCLIPBOUNDS, 0, (LPARAM)&ccb);
					mFilterSetup.mCropRect.left = ccb.x1;
					mFilterSetup.mCropRect.top = ccb.y1;
					mFilterSetup.mCropRect.right = ccb.x2;
					mFilterSetup.mCropRect.bottom = ccb.y2;

					OnCleanup();
					End(TRUE);
				}
				return TRUE;
			case IDCANCEL:
				OnCleanup();
				End(FALSE);
				return TRUE;
			case IDC_BORDERS:
//				guiPositionBlit((HWND)lParam, guiPositionHandleCommand(wParam, lParam));
				return TRUE;
			}
            break;

		case WM_TIMER:
			break;

		case WM_NOTIFY:
			{
//				const VBitmap *vbm = g_pClippingDecoder->getFrameBuffer();

//				vbm->MakeBitmapHeader(&bih);

//				SendMessage(g_hwndClippingDisplay, CCM_BLITFRAME, (WPARAM)&bih, (LPARAM)vbm->data);
			}
			break;

		case WM_USER+100:
			vdsynchronized(mDisplayBufferLock) {
				IVDClippingControl *pCC = VDGetIClippingControl((VDGUIHandle)GetDlgItem(mhdlg, IDC_BORDERS));
				pCC->BlitFrame(&mDisplayBuffer);
			}
			break;

    }
    return FALSE;
}

void VDDialogCaptureCropping::OnInit() {
	mFilterSetup = mpProject->GetFilterSetup();

	ClippingControlBounds ccb;
	LONG hborder, hspace;
	RECT rw, rc, rcok, rccancel;
	HWND hWnd, hWndCancel;

	hWnd = GetDlgItem(mhdlg, IDC_BORDERS);
	ccb.x1	= mFilterSetup.mCropRect.left;
	ccb.x2	= mFilterSetup.mCropRect.right;
	ccb.y1	= mFilterSetup.mCropRect.top;
	ccb.y2	= mFilterSetup.mCropRect.bottom;

//	capGetStatus(hwndCapture, &cs, sizeof cs);

	vdstructex<BITMAPINFOHEADER> bih;

	if (mpProject->GetVideoFormat(bih))
		SendMessage(hWnd, CCM_SETBITMAPSIZE, 0, MAKELONG(bih->biWidth, bih->biHeight));
	else
		SendMessage(hWnd, CCM_SETBITMAPSIZE, 0, MAKELONG(320,240));

	SendMessage(hWnd, CCM_SETCLIPBOUNDS, 0, (LPARAM)&ccb);

	GetWindowRect(mhdlg, &rw);
	GetWindowRect(hWnd, &rc);
	hborder = rc.left - rw.left;
	ScreenToClient(mhdlg, (LPPOINT)&rc.left);
	ScreenToClient(mhdlg, (LPPOINT)&rc.right);

	SetWindowPos(mhdlg, NULL, 0, 0, (rc.right - rc.left) + hborder*2, (rw.bottom-rw.top)+(rc.bottom-rc.top), SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOMOVE);

	hWndCancel = GetDlgItem(mhdlg, IDCANCEL);
	hWnd = GetDlgItem(mhdlg, IDOK);
	GetWindowRect(hWnd, &rcok);
	GetWindowRect(hWndCancel, &rccancel);
	hspace = rccancel.left - rcok.right;
	ScreenToClient(mhdlg, (LPPOINT)&rcok.left);
	ScreenToClient(mhdlg, (LPPOINT)&rcok.right);
	ScreenToClient(mhdlg, (LPPOINT)&rccancel.left);
	ScreenToClient(mhdlg, (LPPOINT)&rccancel.right);
	SetWindowPos(hWndCancel, NULL, rc.right - (rccancel.right-rccancel.left), rccancel.top + (rc.bottom-rc.top), 0,0,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);
	SetWindowPos(hWnd, NULL, rc.right - (rccancel.right-rccancel.left) - (rcok.right-rcok.left) - hspace, rcok.top + (rc.bottom-rc.top), 0,0,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOSIZE);

	SetTimer(mhdlg, 1, 500, NULL);

	// save old callback and splice ourselves in
	mpOldCallback = mpProject->GetCallback();
	mpProject->SetCallback(this);

	// save old display mode
	mOldDisplayMode = mpProject->GetDisplayMode();
	mpProject->SetDisplayMode(nsVDCapture::kDisplayNone);

	// clear existing crop rect
	VDCaptureFilterSetup filtSetupNoCrop(mFilterSetup);
	filtSetupNoCrop.mCropRect.set(0, 0, 0, 0);
	mpProject->SetFilterSetup(filtSetupNoCrop);

	// jump to analysis mode
	mpProject->SetDisplayMode(nsVDCapture::kDisplayAnalyze);
}

void VDDialogCaptureCropping::OnCleanup() {
	// restore old display mode
	mpProject->SetDisplayMode(nsVDCapture::kDisplayNone);
	mpProject->SetFilterSetup(mFilterSetup);
	mpProject->SetDisplayMode(mOldDisplayMode);

	// restore old callback
	mpProject->SetCallback(mpOldCallback);
}

void VDDialogCaptureCropping::UICaptureAnalyzeFrame(const VDPixmap& format) {
	vdsynchronized(mDisplayBufferLock) {
		mDisplayBuffer.assign(format);
	}

	PostMessage(mhdlg, WM_USER+100, 0, 0);
}

void VDShowCaptureCroppingDialog(VDGUIHandle hParent, IVDCaptureProject *pProject) {
	VDDialogCaptureCropping dlg(pProject);

	dlg.ActivateDialog(hParent);
}
