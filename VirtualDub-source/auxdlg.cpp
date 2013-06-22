#include <windows.h>

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
