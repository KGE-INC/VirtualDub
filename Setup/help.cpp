#include <windows.h>

///////////////////////////////////////////////////////////////////////////
//
//	help support
//
///////////////////////////////////////////////////////////////////////////

static char g_szHelpPath[MAX_PATH]="VirtualD.hlp";

void HelpSetPath() {
	char szPath[MAX_PATH];
	char *lpFilePart;

	if (GetModuleFileName(NULL, szPath, sizeof szPath))
		if (GetFullPathName(szPath, sizeof g_szHelpPath, g_szHelpPath, &lpFilePart))
			strcpy(lpFilePart, "VirtualD.hlp");
}

void HelpShowHelp(HWND hwnd) {
	WinHelp(hwnd, g_szHelpPath, HELP_FINDER, 0);
}

void HelpPopup(HWND hwnd, DWORD helpID) {
	WinHelp(hwnd, g_szHelpPath, HELP_CONTEXTPOPUP, helpID);
}

void HelpPopupByID(HWND hwnd, DWORD ctrlID, DWORD *lookup) {
	while(lookup[0]) {
		if (lookup[0] == ctrlID)
			HelpPopup(hwnd, lookup[1]);

		lookup+=2;
	}
}

