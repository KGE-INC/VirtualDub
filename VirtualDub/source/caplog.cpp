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
#include <commctrl.h>
#include <commdlg.h>

#include "resource.h"
#include <vd2/system/error.h>

#include "caplog.h"

extern HINSTANCE g_hInst;

CaptureLog::CaptureLog() {
	nEvents = nEventsBlock = 0;
	pceb = NULL;
}

CaptureLog::~CaptureLog() {
	Dispose();
}

void CaptureLog::Dispose() {
	CapEventBlock *pceb2;

	while(pceb2 = listBlocks.RemoveHead())
		delete pceb2;

	nEvents = nEventsBlock = 0;
	pceb = NULL;
}

CapEvent *CaptureLog::GetNewRequest() {
	if (!pceb || nEventsBlock >= 256) {
		pceb = new CapEventBlock();

		if (!pceb)
			return NULL;

		listBlocks.AddTail(pceb);

		nEventsBlock = 0;
	}

	++nEvents;

	return &pceb->ev[nEventsBlock++];
}

bool CaptureLog::LogVideo(DWORD dwTimeStampReceived, DWORD dwBytes, DWORD dwTimeStampRecorded) {
	CapEvent *pce = GetNewRequest();

	if (!pce)
		return false;

	pce->type = CapEvent::VIDEO;
	pce->dwTimeStampReceived = dwTimeStampReceived;
	pce->dwBytes = dwBytes;
	pce->video.dwTimeStampRecorded = dwTimeStampRecorded;

	return true;
}

bool CaptureLog::LogAudio(DWORD dwTimeStampReceived, DWORD dwBytes, LONG lVideoDelta) {
	CapEvent *pce = GetNewRequest();

	if (!pce)
		return false;

	pce->type = CapEvent::AUDIO;
	pce->dwTimeStampReceived = dwTimeStampReceived;
	pce->dwBytes = dwBytes;
	pce->audio.lVideoDelta = lVideoDelta;

	return true;
}

///////////////////////////////////////////////////////////////////////////

void CaptureLog::GetDispInfo(NMLVDISPINFO *nldi) {
	static const char *szTypes[]={ "Video", "Audio" };
	CapEvent *pev = (CapEvent *)nldi->item.lParam;

	nldi->item.mask			= LVIF_TEXT;
	nldi->item.pszText[0]	= 0;

	switch(nldi->item.iSubItem) {
	case 0:
		nldi->item.pszText = (char *)szTypes[pev->type];
		break;
	case 1:
		_snprintf(nldi->item.pszText, nldi->item.cchTextMax, "%d.%03d", pev->dwTimeStampReceived/1000, pev->dwTimeStampReceived%1000);
		break;
	case 2:
		if (pev->type == CapEvent::AUDIO)
			nldi->item.pszText[0] = 0;
		else
			_snprintf(nldi->item.pszText, nldi->item.cchTextMax, "%d.%03d", pev->video.dwTimeStampRecorded/1000, pev->video.dwTimeStampRecorded%1000);
		break;
	case 3:
		_snprintf(nldi->item.pszText, nldi->item.cchTextMax, "%d", pev->dwBytes);
		break;
	}
}

BOOL CaptureLog::DlgProc2(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	static char *szColumnNames[]={ "Type","Received","Recorded","Bytes" };
	static int iColumnWidths[]={ 100,75,75,75 };

	int index;
	HWND hwndItem;

	switch(msg) {
	case WM_INITDIALOG:
		{
			LV_COLUMN lvc;
			CapEventBlock *pceb = listBlocks.AtHead(), *pceb_next;
			int i, cnt;

			hwndItem = GetDlgItem(hdlg, IDC_EVENTS);

			ListView_SetExtendedListViewStyleEx(hwndItem, LVS_EX_FULLROWSELECT , LVS_EX_FULLROWSELECT);

			for (i=0; i<4; i++) {
				lvc.mask = LVCF_FMT | LVCF_SUBITEM | LVCF_TEXT | LVCF_WIDTH;
				lvc.fmt = LVCFMT_LEFT;
				lvc.cx = iColumnWidths[i];
				lvc.pszText = szColumnNames[i];

				ListView_InsertColumn(hwndItem, i, &lvc);
			}

			index = 0;
			cnt = nEvents;

//			FILE *f = fopen("f:\\caplog.txt", "w");

			while(pceb_next = pceb->NextFromHead()) {
				CapEvent *evptr = pceb->ev;
				int mx = cnt>256 ? 256 : cnt;


				cnt -= mx;

				for(i=mx; i; i--) {
					LVITEM li;

					li.mask		= LVIF_TEXT|LVIF_PARAM;
					li.iSubItem	= 0;
					li.iItem	= index++;
					li.pszText	= LPSTR_TEXTCALLBACK;
					li.lParam	= (LPARAM)evptr++;

					ListView_InsertItem(hwndItem, &li);

//					if (evptr->type == CapEvent::VIDEO) {
//						fprintf(f, "%-10d %-10d %-10d\n", evptr->dwTimeStampReceived, evptr->video.dwTimeStampRecorded, evptr->dwBytes);
//					}
				}

				pceb = pceb_next;
			}

//			fclose(f);
		}
		return FALSE;

	case WM_NOTIFY:
		{
			NMHDR *nm = (NMHDR *)lParam;

			if (nm->idFrom == IDC_EVENTS) {
				NMLVDISPINFO *nldi = (NMLVDISPINFO *)nm;

				switch(nm->code) {
				case LVN_GETDISPINFO:
					GetDispInfo(nldi);
					return TRUE;
				}
			}
		}
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_SAVE) {
			OPENFILENAME ofn;
			char szFile[MAX_PATH];

			szFile[0] = 0;

			ofn.lStructSize			= OPENFILENAME_SIZE_VERSION_400;
			ofn.hwndOwner			= hdlg;
			ofn.lpstrFilter			= "Comma separated values (*.csv)\0*.csv\0";
			ofn.lpstrCustomFilter	= NULL;
			ofn.nFilterIndex		= 1;
			ofn.lpstrFile			= szFile;
			ofn.nMaxFile			= sizeof szFile;
			ofn.lpstrFileTitle		= NULL;
			ofn.nMaxFileTitle		= NULL;
			ofn.lpstrInitialDir		= NULL;
			ofn.lpstrTitle			= "Save capture timing data";
			ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
			ofn.lpstrDefExt			= NULL;

			if (GetSaveFileName(&ofn)) {
				FILE *f = fopen(szFile, "w");

				try {
					if (!f)
						throw MyError("Cannot save file: %s", strerror(NULL));

					CapEventBlock *pceb = listBlocks.AtHead(), *pceb_next;
					int i, cnt;

					cnt = nEvents;

					while(pceb_next = pceb->NextFromHead()) {
						CapEvent *evptr = pceb->ev;
						int mx = cnt>256 ? 256 : cnt;

						cnt -= mx;

						for(i=mx; i; i--) {
							if (evptr->type == CapEvent::VIDEO) {
								fprintf(f, "video,%u,%u,%u,-1\n", evptr->dwTimeStampReceived, evptr->video.dwTimeStampRecorded, evptr->dwBytes);
							} else if (evptr->type == CapEvent::AUDIO) {
								fprintf(f, "audio,%u,%u,%u,%d\n", evptr->dwTimeStampReceived, evptr->dwTimeStampReceived, evptr->dwBytes, evptr->audio.lVideoDelta);
							}
							++evptr;
						}

						pceb = pceb_next;
					}

					if (ferror(f) || fclose(f))
						throw MyError("Cannot save file: %s", strerror(NULL));

				} catch(const MyError& e) {
					e.post(hdlg, "Log write error");
				}

				if (f) fclose(f);
			}

			return TRUE;
		} else if (LOWORD(wParam) == IDOK) {
			EndDialog(hdlg, 0);
			return TRUE;
		}

		break;

	case WM_CLOSE:
		EndDialog(hdlg, 0);
		return TRUE;
	}
	return FALSE;
}

BOOL CALLBACK CaptureLog::DlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_INITDIALOG) {
		SetWindowLong(hdlg, DWL_USER, lParam);
		return ((CaptureLog *)lParam)->DlgProc2(hdlg, msg, wParam, lParam);
	}

	CaptureLog *thisPtr = (CaptureLog *)GetWindowLong(hdlg, DWL_USER);

	if (!thisPtr)
		return FALSE;

	return thisPtr->DlgProc2(hdlg, msg, wParam, lParam);
}

void CaptureLog::Display(HWND hwndParent) {
	DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_EVENTLOG), hwndParent, CaptureLog::DlgProc, (LPARAM)this);
}
