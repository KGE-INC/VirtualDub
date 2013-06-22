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
#include <vfw.h>

#include "resource.h"
#include "auxdlg.h"
#include "oshelper.h"

#include "IAmpDecoder.h"

extern "C" unsigned long version_num;
extern "C" char version_time[];

extern HINSTANCE g_hInst;

BOOL APIENTRY ShowTextDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	HRSRC hRSRC;

    switch (message)
    {
        case WM_INITDIALOG:
			if (hRSRC = FindResource(NULL, (LPSTR)lParam, "STUFF")) {
				HGLOBAL hGlobal;
				if (hGlobal = LoadResource(NULL, hRSRC)) {
					LPVOID lpData;

					if (lpData = LockResource(hGlobal)) {
						char *s = (char *)lpData;
						char *ttl = new char[256];

						while(*s!='\r') ++s;
						if (ttl) {
							memcpy(ttl, (char *)lpData, s - (char *)lpData);
							ttl[s-(char*)lpData]=0;
							SendMessage(hDlg, WM_SETTEXT, 0, (LPARAM)ttl);
							delete ttl;
						}
						s+=2;
						SendMessage(GetDlgItem(hDlg, IDC_CHANGES), WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), MAKELPARAM(TRUE, 0));
						SendMessage(GetDlgItem(hDlg, IDC_CHANGES), WM_SETTEXT, 0, (LPARAM)s);
						FreeResource(hGlobal);
						return TRUE;
					}
					FreeResource(hGlobal);
				}
			}
            return FALSE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
            {
                EndDialog(hDlg, TRUE);  
                return TRUE;
            }
            break;
    }
    return FALSE;
}

BOOL APIENTRY WelcomeDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
    switch (message)
    {
        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK: case IDCANCEL:
                EndDialog(hDlg, TRUE);  
                return TRUE;
			case IDC_HELP2:
				HelpShowHelp(hDlg);
				return TRUE;
            }
            break;
    }
    return FALSE;
}

void Welcome() {
	DWORD dwSeenIt;

	if (!QueryConfigDword(NULL, "SeenWelcome", &dwSeenIt) || !dwSeenIt) {
		DialogBox(g_hInst, MAKEINTRESOURCE(IDD_WELCOME), NULL, WelcomeDlgProc);

		SetConfigDword(NULL, "SeenWelcome", 1);
	}
}

static const char g_szDivXWarning[]=
	"\"DivX\" codec detected\0"
	"One or more of the \"DivX\" drivers have been detected on your system. These drivers are illegal binary hacks "
	"of legitimate drivers:\r\n"
	"\r\n"
	"* DivX low motion/fast motion: Microsoft MPEG-4 V3 video\r\n"
	"* DivX audio: Microsoft Windows Media Audio\r\n"
	"* \"Radium\" MP3: Fraunhofer-IIS MPEG layer III audio\r\n"
	"\r\n"
	"Hacked drivers are known to cause serious problems, including "
	"crashes and interference with the original drivers. When these drivers are loaded, the author cannot "
	"make any guarantees as to the stability of VirtualDub. Please do not "
	"forward crash dumps involving these drivers, as the author has no control "
	"of the original third-party drivers or the binary hacks applied to them.";

static const char g_szAPWarning[]=
	"\"AngelPotion Definitive\" codec detected\0"
	"The \"AngelPotion Definitive\" codec has been detected on your system. This driver is an illegal binary hack "
	"of the following legitimate drivers:\r\n"
	"\r\n"
	"* Microsoft MPEG-4 V3 video\r\n"
	"\r\n"
	"The AngelPotion codec is a particularly poor hack of the MS MPEG-4 V3 codec and is known "
	"to cause a number of serious conflicts, including but not limited to:\r\n"
	"* Excessive disk usage on temporary drive\r\n"
	"* Incorrectly responding to compressed formats of other codecs, even uncompressed RGB\r\n"
	"* Preventing some applications from loading AVI files at all\r\n"
	"* Inhibiting Windows Media Player automatic codec download\r\n"
	"\r\n"
	"The author cannot guarantee the stability of VirtualDub in any way when AngelPotion is loaded, "
	"even if the codec is not in use. All crash dumps indicating AP is loaded will be promptly discarded. "
	"It is HIGHLY suggested that you uninstall AngelPotion immediately."
	;

BOOL APIENTRY DivXWarningDlgProc( HWND hdlg, UINT message, UINT wParam, LONG lParam)
{
	const char *s;

    switch (message)
    {
		case WM_INITDIALOG:
			s = (const char *)lParam;
			SetWindowText(hdlg, s);
			while(*s++);
			SendDlgItemMessage(hdlg, IDC_WARNING, WM_SETTEXT, 0, (LPARAM)s);
			return TRUE;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK: case IDCANCEL:
                EndDialog(hdlg, TRUE);
                return TRUE;
            }
            break;
    }
    return FALSE;
}

static bool DetectDriver(const char *pszName) {
	char szDriverANSI[256];
	ICINFO info;

	for(int i=0; ICInfo(ICTYPE_VIDEO, i, &info); ++i) {
		if (WideCharToMultiByte(CP_ACP, 0, info.szDriver, -1, szDriverANSI, sizeof szDriverANSI, NULL, NULL)
			&& !stricmp(szDriverANSI, pszName))

			return true;
	}

	return false;
}

void DetectDivX() {
	DWORD dwSeenIt;

	if (!QueryConfigDword(NULL, "SeenDivXWarning", &dwSeenIt) || !dwSeenIt) {
		if (DetectDriver("divxc32.dll") || DetectDriver("divxc32f.dll")) {
			DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_DIVX_WARNING), NULL, DivXWarningDlgProc, (LPARAM)g_szDivXWarning);

			SetConfigDword(NULL, "SeenDivXWarning", 1);
		}
	}
	if (!QueryConfigDword(NULL, "SeenAngelPotionWarning", &dwSeenIt) || !dwSeenIt) {
		if (DetectDriver("APmpg4v1.dll")) {

			DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_DIVX_WARNING), NULL, DivXWarningDlgProc, (LPARAM)g_szAPWarning);

			SetConfigDword(NULL, "SeenAngelPotionWarning", 1);
		}
	}
}
