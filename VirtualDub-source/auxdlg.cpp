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

BOOL APIENTRY AboutDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
			{
				char buf[128];

				wsprintf(buf, "Build %d/"
#ifdef _DEBUG
					"debug"
#else
					"release"
#endif
					" (%s)", version_num, version_time);

				SetDlgItemText(hDlg, IDC_FINALS_SUCK, buf);

				HRSRC hrsrc;

				if (hrsrc = FindResource(NULL, MAKEINTRESOURCE(IDR_CREDITS), "STUFF")) {
					HGLOBAL hGlobal;
					if (hGlobal = LoadResource(NULL, hrsrc)) {
						const char *pData, *pLimit;

						if (pData = (const char *)LockResource(hGlobal)) {
							HWND hwndItem = GetDlgItem(hDlg, IDC_CREDITS);
							const INT tab = 80;

							pLimit = pData + SizeofResource(NULL, hrsrc);

							SendMessage(hwndItem, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(TRUE, 0));
							SendMessage(hwndItem, LB_SETTABSTOPS, 1, (LPARAM)&tab);

							while(pData < pLimit) {
								char *t = buf;

								while(pData < pLimit && *pData!='\r' && *pData!='\n')
									*t++ = *pData++;

								while(pData < pLimit && (*pData=='\r' || *pData=='\n'))
									++pData;

								*t = 0;

								if (t > buf)
									SendMessage(GetDlgItem(hDlg, IDC_CREDITS), LB_ADDSTRING, 0, (LPARAM)buf);
							}

							FreeResource(hGlobal);
						}
						FreeResource(hGlobal);
					}
				}

				IAMPDecoder *iad = CreateAMPDecoder();

				if (iad) {
					wsprintf(buf, "MPEG audio decoder: %s", iad->GetAmpVersionString());
					delete iad;
					SetDlgItemText(hDlg, IDC_MP3_DECODER, buf);
				}
			}
            return (TRUE);

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

BOOL APIENTRY DivXWarningDlgProc( HWND hdlg, UINT message, UINT wParam, LONG lParam)
{
    switch (message)
    {
		case WM_INITDIALOG:
			SendDlgItemMessage(hdlg, IDC_WARNING, WM_SETTEXT, 0, (LPARAM)g_szDivXWarning);
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

void DetectDivX() {
	DWORD dwSeenIt;

	if (!QueryConfigDword(NULL, "SeenDivXWarning", &dwSeenIt) || !dwSeenIt) {
		HIC hic;

		if (hic = ICOpen('CDIV', '3VID', ICMODE_QUERY)) {
			ICClose(hic);

			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DIVX_WARNING), NULL, DivXWarningDlgProc);

			SetConfigDword(NULL, "SeenDivXWarning", 1);
		}
	}
}
