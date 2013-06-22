#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <malloc.h>
#include <ctype.h>
#include <crtdbg.h>

#include <windows.h>
#include <commctrl.h>

#include "resource.h"
#include "registry.h"
#include "help.h"
#include "helpfile.h"

HWND g_hwnd;
HINSTANCE g_hInst; // current instance
char szAppName[] = "VirtualDub Setup Class"; // The name of this application
char szTitle[]   = ""; // The title bar text
char g_szWinPath[MAX_PATH];
char g_szProgPath[MAX_PATH];
char g_szTempPath[MAX_PATH];

///////////////////

BOOL Init(HINSTANCE, int);
LRESULT APIENTRY WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

BOOL APIENTRY InstallAVIFileDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL APIENTRY UninstallAVIFileDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL APIENTRY RemoveSettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL APIENTRY DiskTestDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL APIENTRY AboutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

///////////

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPSTR lpCmdLine, int nCmdShow )
{
	MSG msg;
	char *lpszFilePart;

	///////////

	if (!GetWindowsDirectory(g_szWinPath, sizeof g_szWinPath)) return FALSE;
	if (!GetModuleFileName(NULL, g_szTempPath, sizeof g_szTempPath))
		return FALSE;
	if (!GetFullPathName(g_szTempPath, sizeof g_szProgPath, g_szProgPath, &lpszFilePart))
		return FALSE;
	*lpszFilePart=0;

	HelpSetPath();

	if (!Init(hInstance, nCmdShow)) return FALSE;

	// Main message loop.

	while (GetMessage(&msg, NULL, 0, 0)) {
//		if (!IsDialogMessage(g_hwnd, &msg))
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

////////////////

BOOL Init(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASS  wc;

    // Register the window class for my window.                                                           */
    wc.style = 0;                       // Class style.
    wc.lpfnWndProc = (WNDPROC)WndProc; // Window procedure for this class.
    wc.cbClsExtra = 0;                  // No per-class extra data.
    wc.cbWndExtra = DLGWINDOWEXTRA;                  // No per-window extra data.
    wc.hInstance = hInstance;           // Application that owns the class.
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_3DFACE+1); 
    wc.lpszMenuName =  NULL;   // Name of menu resource in .RC file. 
    wc.lpszClassName = szAppName; // Name used in call to CreateWindow.

    if (!RegisterClass(&wc)) return FALSE;

	g_hInst = hInstance; // Store instance handle in our global variable

	g_hwnd = CreateDialog(hInstance,MAKEINTRESOURCE(IDD_MAINWINDOW),NULL,(DLGPROC)NULL);

	if (!g_hwnd) {
		return (FALSE);
	}

	ShowWindow(g_hwnd, nCmdShow);
	UpdateWindow(g_hwnd);

	return (TRUE);
}

void PrintfWindowText(HWND hWnd, char *format, ...) {
	char buf[256];
	va_list val;

	va_start(val, format);
	vsprintf(buf, format, val);
	va_end(val);
	SetWindowText(hWnd, buf);
}

DWORD dwMainWindowHelpLookup[]={
	IDC_BENCHMARK,		IDH_SETUP_BENCHMARK,
	IDC_INSTALL,		IDH_SETUP_INSTALL,
	IDC_UNINSTALL,		IDH_SETUP_UNINSTALL,
	IDC_REMOVE,			IDH_SETUP_REMOVE,
	0
};

LRESULT APIENTRY WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	switch (message) { 
		case WM_DESTROY:
			PostQuitMessage(0);
			break;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_EXECUTE:
				if ((int)ShellExecute(hWnd, "open", "VirtualDub.exe", NULL, NULL, SW_SHOWNORMAL) <= 32)
					MessageBox(hWnd, "Couldn't launch VirtualDub.exe.", "Oops", MB_OK);
				break;
			case IDC_BENCHMARK:
				DialogBox(g_hInst, MAKEINTRESOURCE(IDD_DISKTEST_SETUP), hWnd, (DLGPROC)DiskTestDlgProc);
				break;
			case IDC_INSTALL:
				DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ATTEMPT), hWnd, (DLGPROC)InstallAVIFileDlgProc);
				break;
			case IDC_UNINSTALL:
				DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ATTEMPT), hWnd, (DLGPROC)UninstallAVIFileDlgProc);
				break;
			case IDC_REMOVE:
				DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ATTEMPT), hWnd, (DLGPROC)RemoveSettingsDlgProc);
				break;
			case IDC_ABOUT:
				DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUT), hWnd, (DLGPROC)AboutDlgProc);
				break;
			case IDCANCEL:
				DestroyWindow(hWnd);
				break;
			}
            break;

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					HelpPopupByID(hWnd, lphi->iCtrlId, dwMainWindowHelpLookup);
			}
			return TRUE;

		default:
			return DefWindowProc(hWnd,message,wParam,lParam);
	}
	return (0);
}

//////////////////////////////////////////////////////////////////////

void ListboxAddf(HWND hwndListbox, char *format, ...) {
	char buf[256];
	va_list val;

	va_start(val, format);
	_vsnprintf(buf, sizeof buf, format, val);
	va_end(val);
	buf[(sizeof buf) - 1] = 0;

	SendMessage(hwndListbox, LB_ADDSTRING, 0, (LPARAM)buf);
}

BOOL InstallFile(char *szSource, char *szDestFormat, ...) {
	char szDest[MAX_PATH];
	char szDestPath[MAX_PATH];
	char szDestFile[MAX_PATH];
	char *lpszDestFile;
	char szCurInst[MAX_PATH];
	char szTempFile[MAX_PATH];
	DWORD dwFlags = VIFF_DONTDELETEOLD;
	DWORD dwRet;
	UINT uTmpLen;
	char szFailure[256];
	va_list val;

	va_start(val, szDestFormat);
	vsprintf(szDest, szDestFormat, val);
	va_end(val);

	if (!GetFullPathName(szDest, sizeof szDestPath, szDestPath, &lpszDestFile))
		return FALSE;

	strcpy(szDestFile, lpszDestFile);
	*lpszDestFile=0;

	do {
		szTempFile[0]=0;
		szCurInst[0]=0;
		uTmpLen = sizeof szTempFile;
		dwRet = VerInstallFile(dwFlags, szSource, szDestFile, g_szProgPath, szDestPath, szCurInst, szTempFile, &uTmpLen);

		if (dwRet & VIF_TEMPFILE) {
			DeleteFile(szTempFile);
			dwRet &= ~VIF_TEMPFILE;
		}

		szFailure[0]=0;

		if (dwRet & (VIF_MISMATCH | VIF_DIFFTYPE))
			sprintf(szFailure,	"The old %s doesn't look like a VirtualDub file.\n"
								"If it belongs to another application, installing the new file may cause "
								"the other app to stop functioning.\n"
								"Install the new file only if you are sure or have a backup."
								,szDestFile);
		else if (dwRet & VIF_SRCOLD)
			sprintf(szFailure,	"%s is older than the %s being installed over.\n"
								"You should install the older %s if you do not use other versions "
								"of VirtualDub, since the newer file may be incompatible."
								,szSource,szDestFile);
		else if (dwRet & VIF_WRITEPROT)
			sprintf(szFailure,	"The %s being installed over has been marked read-only.\n"
								"Override read-only attribute and install anyway?"
								,szDestFile);
		else if (dwRet & VIF_FILEINUSE)
			sprintf(szFailure,	"%s is in use.  It cannot be installed over right now.\n"
								"If you have any copies of VirtualDub or any programs that "
								"may be using VirtualDub's AVIFile handler, please close them "
								"and then click OK to retry the operation."
								,szDestFile);
		else if (dwRet & VIF_OUTOFSPACE)
			sprintf(szFailure,	"Doh! We're out of space trying to write:\n\t%s\n\nCan you clear some up?"
								,szDest);
		else if (dwRet & VIF_ACCESSVIOLATION)
			sprintf(szFailure,	"Access violation.  Check with your administrator to see if you have "
								"the appropriate permissions to write to \"%s\"."
								,szDest);
		else if (dwRet & VIF_SHARINGVIOLATION)
			sprintf(szFailure,	"Share violation; something else probably has %s open.  Try closing applications that "
								"have the file open, and check permissions on network drives."
								,szDestFile);
		else if (dwRet & VIF_CANNOTCREATE)
			sprintf(szFailure,	"Couldn't create temporary file %s.\nTry again?", szTempFile);
		else if (dwRet & VIF_CANNOTDELETE)
			sprintf(szFailure,	"Couldn't delete temporary file %s.\nTry installing again?", szTempFile);
		else if (dwRet & VIF_CANNOTDELETECUR)
			sprintf(szFailure,	"Couldn't delete existing file \"%s\".\nTry installing again?", szDest);
		else if (dwRet & VIF_CANNOTRENAME)
			sprintf(szFailure,	"Deleted old file %s, but couldn't move %s into its place.\n"
								"You should retry this operation.", szDestFile, szSource);
		else if (dwRet & VIF_CANNOTREADSRC)
			sprintf(szFailure,	"Couldn't read source file \"%s\".  Should I try again?"
								,szSource);
		else if (dwRet & VIF_CANNOTREADDST)
			sprintf(szFailure,	"Couldn't read destination file \"%s\".  I can try installing over it "
								"anyway, though."
								,szDest);
		else if (dwRet & VIF_OUTOFMEMORY)
			sprintf(szFailure,	"Ran out of memory!  Try freeing some up.");
		else if (dwRet)
			sprintf(szFailure,	"Unidentified error copying:\n\t%s\n\t\tto\n\t%s\n\nTry forcing install?"
								,szSource
								,szDest);

		if (szFailure[0])
			if (IDNO==MessageBox(NULL, szFailure, "Install error", MB_YESNO | MB_APPLMODAL))
				return FALSE;

		dwFlags |= VIFF_FORCEINSTALL;
	} while(dwRet);

	return TRUE;
}

BOOL InstallRegStr(HKEY hkBase, char *szKeyName, char *szName, char *szValue) {
	char buf[256];

	if (!SetRegString(hkBase, szKeyName, szName, szValue)) {
		sprintf(buf,"Couldn't set registry key %s\\%s",szKeyName,szName?szName:"(default)");
		MessageBox(NULL, buf, "Install error", MB_OK);
		return FALSE;
	}

	return TRUE;
}

BOOL InstallDeleteFile(char *szFileFormat, ...) {
	char szFile[256];
	va_list val;

	va_start(val, szFileFormat);
	vsprintf(szFile, szFileFormat, val);
	va_end(val);

	if (!DeleteFile(szFile))
		if (GetLastError() != ERROR_FILE_NOT_FOUND)
			return FALSE;

	return TRUE;
}


///////////////////////////////////////

BOOL APIENTRY InstallAVIFileDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_INITDIALOG:
			{
				HWND hwndListbox = GetDlgItem(hDlg, IDC_ACTIONLIST);

				SetWindowText(hDlg, "Install AVIFile frameclient");

				ListboxAddf(hwndListbox, "Copy VDREMOTE.DLL to %s\\SYSTEM\\VDREMOTE.DLL", g_szWinPath);
				ListboxAddf(hwndListbox, "Copy VDSRVLNK.DLL to %s\\SYSTEM\\VDSRVLNK.DLL", g_szWinPath);
				ListboxAddf(hwndListbox, "Add VDRemote class and AVIFile entries to Registry");
			}
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				if (	InstallFile("vdremote.dll","%s\\system\\vdremote.dll",g_szWinPath)
					&&	InstallFile("vdsvrlnk.dll","%s\\system\\vdsvrlnk.dll",g_szWinPath)
					&&	InstallRegStr(HKEY_CLASSES_ROOT,"CLSID\\{894288e0-0948-11d2-8109-004845000eb5}",NULL,"VirtualDub link handler")
					&&	InstallRegStr(HKEY_CLASSES_ROOT,"CLSID\\{894288e0-0948-11d2-8109-004845000eb5}\\InprocServer32",NULL,"vdremote.dll")
					&&	InstallRegStr(HKEY_CLASSES_ROOT,"CLSID\\{894288e0-0948-11d2-8109-004845000eb5}\\InprocServer32","ThreadingModel","Apartment")
					&&	InstallRegStr(HKEY_CLASSES_ROOT,"CLSID\\{894288e0-0948-11d2-8109-004845000eb5}\\InprocServer32\\AVIFile",NULL,"1")
					&&	InstallRegStr(HKEY_CLASSES_ROOT,"AVIFile\\Extensions\\VDR",NULL,"{894288e0-0948-11d2-8109-004845000eb5}")
					&&	InstallRegStr(HKEY_CLASSES_ROOT,"AVIFile\\RIFFHandlers\\VDRM",NULL,"{894288e0-0948-11d2-8109-004845000eb5}")
					)

					MessageBox(hDlg, "AVIFile frameclient install successful.", "VirtualDub Setup", MB_OK);
				else
					MessageBox(hDlg, "AVIFile frameclient install failed.", "VirtualDub Setup", MB_OK);

				EndDialog(hDlg, TRUE);
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
	}
	return FALSE;
}

BOOL APIENTRY UninstallAVIFileDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	BOOL fSuccess;

	switch(msg) {
		case WM_INITDIALOG:
			{
				HWND hwndListbox = GetDlgItem(hDlg, IDC_ACTIONLIST);

				SetWindowText(hDlg, "Uninstall AVIFile frameclient");

				ListboxAddf(hwndListbox, "Delete %s\\SYSTEM\\VDREMOTE.DLL", g_szWinPath);
				ListboxAddf(hwndListbox, "Delete %s\\SYSTEM\\VDSRVLNK.DLL", g_szWinPath);
				ListboxAddf(hwndListbox, "Remove VDRemote class and AVIFile entries from Registry");
			}
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				fSuccess =  InstallDeleteFile("%s\\system\\vdremote.dll",g_szWinPath);
				fSuccess &= InstallDeleteFile("%s\\system\\vdsvrlnk.dll",g_szWinPath);

				if (	ERROR_SUCCESS != RegDeleteKey(HKEY_CLASSES_ROOT,"Clsid\\{894288e0-0948-11d2-8109-004845000eb5}\\InprocServer32\\AVIFile")
					||	ERROR_SUCCESS != RegDeleteKey(HKEY_CLASSES_ROOT,"Clsid\\{894288e0-0948-11d2-8109-004845000eb5}\\InprocServer32")
					||	ERROR_SUCCESS != RegDeleteKey(HKEY_CLASSES_ROOT,"Clsid\\{894288e0-0948-11d2-8109-004845000eb5}")
					||	ERROR_SUCCESS != RegDeleteKey(HKEY_CLASSES_ROOT,"AVIFile\\Extensions\\VDR")
					||	ERROR_SUCCESS != RegDeleteKey(HKEY_CLASSES_ROOT,"AVIFile\\RIFFHandlers\\VDRM"))

					MessageBox(hDlg, "Registry entries were in use.  Deinstall not successful.\n"
									"\n"
									"A partial installation now exists on your system.  Reinstall the AVIFile "
									"handler to restore frameclient functionality, or close applications that may "
									"be occupying the Registry entries and retry the deinstall."
									,"VirtualDub Setup",MB_OK);

				else if (!fSuccess)
					MessageBox(hDlg, "DLL files were in use.  Deinstall not successful.\n"
									"\n"
									"A partial installation now exists on your system.  Reinstall the AVIFile "
									"handler to restore frameclient functionality, or close applications that may "
									"be occupying the shared DLLs and retry the deinstall."
									,"VirtualDub Setup",MB_OK);
				else
					MessageBox(hDlg, "AVIFile frameclient deinstall successful.", "VirtualDub Setup", MB_OK);

				EndDialog(hDlg, TRUE);
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
	}
	return FALSE;
}

///////////////////////////////////////////////

void RemoveVirtualDubKeys(HWND hwndListbox) {
	FILETIME ftModified;
	int i;
	LONG err;
	char szKeyName[MAX_PATH];
	char szErrorText[128];
	DWORD dwKeyNameLen;

	SendMessage(hwndListbox, LB_RESETCONTENT, 0, 0);

	i=0;
	for(;;) {
		dwKeyNameLen = sizeof szKeyName;
		err = RegEnumKeyEx(HKEY_USERS, i++, szKeyName, &dwKeyNameLen, 0, NULL, 0, &ftModified);

		if (err == ERROR_NO_MORE_ITEMS)
			break;
		else if (err == ERROR_SUCCESS) {
			HKEY hkeyUser;
			char *bp = szKeyName + strlen(szKeyName);
			
			err = RegOpenKeyEx(HKEY_USERS, szKeyName, 0, KEY_ALL_ACCESS, &hkeyUser);
			if (err != ERROR_SUCCESS) {
				FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, szErrorText, sizeof szErrorText, NULL);
				ListboxAddf(hwndListbox, "HKEY_USERS\\%s: %s", szKeyName, szErrorText);
				continue;
			}

			strcpy(bp, "\\Software\\Freeware\\VirtualDub\\Capture");

			err = RegDeleteKey(HKEY_USERS,szKeyName);
			if (err != ERROR_SUCCESS && err != ERROR_FILE_NOT_FOUND)
				FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, szErrorText, sizeof szErrorText, NULL);
			else
				strcpy(szErrorText, "Deleted");
			ListboxAddf(hwndListbox, "HKEY_USERS\\%s: %s", szKeyName, szErrorText);

			strcpy(bp, "\\Software\\Freeware\\VirtualDub\\MRUList");

			err = RegDeleteKey(HKEY_USERS,szKeyName);
			if (err != ERROR_SUCCESS && err != ERROR_FILE_NOT_FOUND)
				FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, szErrorText, sizeof szErrorText, NULL);
			else
				strcpy(szErrorText, "Deleted");
			ListboxAddf(hwndListbox, "HKEY_USERS\\%s: %s", szKeyName, szErrorText);

			bp[29]=0;

			err = RegDeleteKey(HKEY_USERS,szKeyName);
			if (err != ERROR_SUCCESS && err != ERROR_FILE_NOT_FOUND)
				FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, szErrorText, sizeof szErrorText, NULL);
			else
				strcpy(szErrorText, "Deleted");
			ListboxAddf(hwndListbox, "HKEY_USERS\\%s: %s", szKeyName, szErrorText);

			RegCloseKey(hkeyUser);
		}
	}
}

BOOL APIENTRY RemoveSettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_INITDIALOG:
			{
				HWND hwndListbox = GetDlgItem(hDlg, IDC_ACTIONLIST);

				SetWindowText(hDlg, "Remove VirtualDub preference data");

				ListboxAddf(hwndListbox, "Remove HKEY_USERS\\*\\Software\\Freeware\\VirtualDub\\*");
			}
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				SetWindowText(GetDlgItem(hDlg, IDC_ACTION), "Results:");

				RemoveVirtualDubKeys(GetDlgItem(hDlg, IDC_ACTIONLIST));

				SetWindowText(GetDlgItem(hDlg, IDOK), "Retry");
				SetWindowText(GetDlgItem(hDlg, IDCANCEL), "Done");
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////
//
// disk test
//
///////////////////////////////////////////////////////////////////////////

class DiskTestParameters {
public:
	LONG lFrameSize;
	LONG lFrameCount;
	LONG lFrameBuffers;
	LONG lFrameRate;
	LONG lDiskBuffer;
	LONG lMicroSecPerFrame;
	DWORD dwBufferMode;
};

BOOL DiskTestCreateStruc(HWND hDlg, DiskTestParameters *dtp) {
	BOOL fSuccess;
	char buf[128], c;
	double d;
	long lWidth, lHeight, lDepth;

	SendMessage(GetDlgItem(hDlg, IDC_FRAME_SIZE), WM_GETTEXT, sizeof buf, (LPARAM)buf);

	////

	if (3==sscanf(buf, "%ldx%ldx%ld", &lWidth, &lHeight, &lDepth))
		dtp->lFrameSize = (lWidth*lDepth+31)/32 * 4 * lHeight;

	else if (2==sscanf(buf, "%ldx%ld", &lWidth, &lHeight))
		dtp->lFrameSize = (lWidth+(lWidth&1))*lHeight*2;

	else if (2==sscanf(buf, "%ld%c", &lWidth, &c) && c=='k' || c=='K')
		dtp->lFrameSize = lWidth*1024;		// kilobytes

	else if (1==sscanf(buf, "%ld", &lWidth))
		dtp->lFrameSize = lWidth;		// well, bytes actually...

	////

	if (dtp->lFrameSize < 16 || dtp->lFrameSize > 1048576) {
		return FALSE;
	}

	dtp->lFrameCount		= GetDlgItemInt(hDlg, IDC_FRAME_COUNT, &fSuccess, FALSE);
	if (dtp->lFrameCount < 1 || dtp->lFrameCount > 89478400) {
		return FALSE;
	}

	dtp->lFrameBuffers	= GetDlgItemInt(hDlg, IDC_FRAME_BUFFERS, &fSuccess, FALSE);
	if (dtp->lFrameBuffers < 16 || dtp->lFrameBuffers > 4096) {
		return FALSE;
	}

	dtp->lDiskBuffer		= GetDlgItemInt(hDlg, IDC_DISK_BUFFER, &fSuccess, FALSE);
	if (dtp->lDiskBuffer < 64) {
		return FALSE;
	}

	SendMessage(GetDlgItem(hDlg, IDC_FRAME_RATE), WM_GETTEXT, sizeof buf, (LPARAM)buf);
	if (1!=sscanf(buf, "%lf", &d) || d<0.01 || d>200.0) {
		return FALSE;
	}

	dtp->lMicroSecPerFrame = (long)((1000000.0 / d) + 0.5);

	dtp->dwBufferMode = 0;
	if (IsDlgButtonChecked(hDlg, IDC_DISABLE_BUFFERING))
		dtp->dwBufferMode = FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING;

	return TRUE;
}

/////////////////

LONG buffers;
LONG buffers_in_use;
long drop_count;
long burst_drop_count;
long max_burst_drop_count;
BOOL last_was_dropped;
volatile long captured_frames;

void CALLBACK DiskTestTimerProc(UINT uID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
	if (buffers_in_use >= buffers) {
		++drop_count;

		if (!last_was_dropped) {
			burst_drop_count = 0;
		}

		++burst_drop_count;
		last_was_dropped = TRUE;
	} else {
		if (burst_drop_count > max_burst_drop_count)
			max_burst_drop_count = burst_drop_count;

		InterlockedIncrement(&buffers_in_use);

		last_was_dropped = FALSE;
	}

	++captured_frames;
}

BOOL APIENTRY DiskTestStatusDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDCANCEL:
			PostQuitMessage(0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void DiskTestDoIt(HWND hDlg) {
	DiskTestParameters dtp;
	char szFileName[MAX_PATH];
	char szErrorBuf[MAX_PATH];
	HMMIO hmmio		= NULL;
	UINT timerID	= 0;
	HANDLE hFile	= INVALID_HANDLE_VALUE;
	HWND hwndStatus = NULL;
	char *framebuf	= NULL;
	char *buffer	= NULL;
	int drive;

	if (CB_ERR == (drive = SendDlgItemMessage(hDlg, IDC_DRIVE, CB_GETCURSEL, 0, 0)))
		return;

	drive = SendDlgItemMessage(hDlg, IDC_DRIVE, CB_GETITEMDATA, drive, 0);
	sprintf(szFileName,"%c:\\DISKTEST.BIN", drive+'A');

	if (!DiskTestCreateStruc(hDlg, &dtp)) return;

	dtp.lFrameSize += 8;

	try {
		DWORD max_frames;
		MMIOINFO mmioinfo;
		BOOL updated = FALSE;
		long max_buffer_point = 0;
		LONG lTotalBytes=0;
		HWND hwndFrameCount, hwndDropCount, hwndBurstCount, hwndMaxBuffers;
		LONG perthousand;
		LONG tsec;
		MSG msg;
		DWORD dwOpenMode;

		if (!(hwndStatus = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_DISKTEST), hDlg, (DLGPROC)DiskTestStatusDlgProc)))
			throw "Umm, no status dialog.  Doh!";

		EnableWindow(hDlg, FALSE);

		hwndFrameCount	= GetDlgItem(hwndStatus, IDC_FRAMES);
		hwndDropCount	= GetDlgItem(hwndStatus, IDC_DROPPED);
		hwndBurstCount	= GetDlgItem(hwndStatus, IDC_BURST_DROP);
		hwndMaxBuffers	= GetDlgItem(hwndStatus, IDC_MAX_BUFFERS);

		if (!(framebuf = (char *)malloc(dtp.lFrameSize)))
			throw "Couldn't allocate frame buffer.";

		if (!(buffer = (char *)VirtualAlloc(NULL, dtp.lDiskBuffer*1024, MEM_COMMIT, PAGE_READWRITE)))
			throw "Couldn't allocate disk buffer";

		SetPriorityClass((HANDLE)GetCurrentProcess(), HIGH_PRIORITY_CLASS);

		///////////

		buffers = dtp.lFrameBuffers;
		buffers_in_use = 0;
		drop_count = 0;
		burst_drop_count = max_burst_drop_count = 0;
		last_was_dropped = FALSE;
		captured_frames = 0;
		max_frames = dtp.lFrameCount;

		////////////

		dwOpenMode = CREATE_NEW;

		for(;;) {
			char msgbuf[MAX_PATH + 64];

			hFile = CreateFile(
					szFileName,
					GENERIC_WRITE,
					0,
					NULL,
					dwOpenMode,
					FILE_ATTRIBUTE_NORMAL | dtp.dwBufferMode,
					NULL
					);

			if (hFile == INVALID_HANDLE_VALUE) {
				if (GetLastError() == ERROR_FILE_EXISTS) {
					sprintf(msgbuf, "File %s already exists.  Overwrite?", szFileName);
					if (IDYES != MessageBox(hDlg, msgbuf, "Disk test warning", MB_YESNO))
						throw NULL;

					dwOpenMode = OPEN_ALWAYS;
				} else {
					strcpy(szErrorBuf, "Error opening file: '");
					FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, szErrorBuf+22, (sizeof szErrorBuf)-22, NULL);
					strcat(szErrorBuf, "'");

					throw szErrorBuf;
				}
			} else
				break;

		}

		memset(&mmioinfo, 0, sizeof mmioinfo);
		mmioinfo.fccIOProc	= FOURCC_DOS;
		mmioinfo.cchBuffer	= dtp.lDiskBuffer*1024;
		mmioinfo.pchBuffer	= buffer;
		mmioinfo.adwInfo[0] = (DWORD)hFile;

		if (!(hmmio = mmioOpen(NULL, &mmioinfo, MMIO_WRITE /*| MMIO_ALLOCBUF*/)))
			throw "Couldn't open test file.";

		if (!(timerID = timeSetEvent(
						(dtp.lMicroSecPerFrame+500)/1000,
						(dtp.lMicroSecPerFrame+500)/1000,
						DiskTestTimerProc,
						NULL,
						TIME_PERIODIC)))
			throw "Unable to initialize timer.";

		while(captured_frames < max_frames) {
			if (buffers_in_use) {
				updated = FALSE;

				if (buffers_in_use > max_buffer_point) max_buffer_point = buffers_in_use;

				if (dtp.lFrameSize != mmioWrite(hmmio, framebuf, dtp.lFrameSize))
					throw "I/O error";

				lTotalBytes += dtp.lFrameSize;

				InterlockedDecrement(&buffers_in_use);
			} else {
				if (!updated) {
					updated = TRUE;
					PrintfWindowText(hwndFrameCount, "%d/%d ", captured_frames, dtp.lFrameCount);
					PrintfWindowText(hwndDropCount, "%d ", drop_count);
					PrintfWindowText(hwndBurstCount, "%d ", max_burst_drop_count);
					PrintfWindowText(hwndMaxBuffers, "%d/%d ", max_buffer_point, dtp.lFrameBuffers);
				}

				while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
					if (msg.message == WM_QUIT)
						throw (char *)NULL;

					if (!IsDialogMessage(hwndStatus, &msg) && !IsDialogMessage(hDlg, &msg)) {
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
				}
			}
		}

		if (last_was_dropped)
			if (burst_drop_count > max_burst_drop_count)
				max_burst_drop_count = burst_drop_count;

		// round out write to next 2048 bytes

		lTotalBytes = -lTotalBytes & 2047;

		if (lTotalBytes)
			if (lTotalBytes != mmioWrite(hmmio, framebuf, lTotalBytes))
				throw "I/O error";

		////////////

		if (!drop_count)
			SetWindowText(hwndStatus, "Test passed - no frames dropped.");
		else
			SetWindowText(hwndStatus, "Test failed - frames were dropped.");

		SetDlgItemText(hwndStatus, IDCANCEL, "Ok");

		perthousand = (drop_count * 1000 + dtp.lFrameCount - 1) / dtp.lFrameCount;
		tsec = (LONG)(((__int64)captured_frames * dtp.lMicroSecPerFrame + (__int64)99999) / 100000);

		PrintfWindowText(hwndFrameCount, "%d (%d.%d s)", captured_frames, tsec/10, tsec%10);
		PrintfWindowText(hwndDropCount, "%d (%d.%d%%)", drop_count, perthousand/10, perthousand%10);
		PrintfWindowText(hwndBurstCount, "%d (%d ms)", max_burst_drop_count,
				MulDiv(max_burst_drop_count, dtp.lMicroSecPerFrame, 1000));
		PrintfWindowText(hwndMaxBuffers, "%d/%d ", max_buffer_point, dtp.lFrameBuffers);

		while (GetMessage(&msg, NULL, 0, 0)) {
			if (!IsDialogMessage(hwndStatus, &msg) && !IsDialogMessage(hDlg, &msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

	} catch(char *s) {
		if (s)
			MessageBox(hDlg, s, "Disk Test Error", MB_OK | MB_ICONEXCLAMATION);
	}

	SetPriorityClass((HANDLE)GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
	if (timerID) timeKillEvent(timerID); timerID = 0;
	if (hmmio) mmioClose(hmmio,MMIO_FHOPEN);
	if (hFile != INVALID_HANDLE_VALUE) {
		CloseHandle(hFile);
		DeleteFile(szFileName);
	}
	if (buffer) VirtualFree(buffer, 0, MEM_RELEASE);
	if (framebuf) free(framebuf);
	if (hwndStatus) {
		EnableWindow(hDlg, TRUE);
		SetFocus(hDlg);
		DestroyWindow(hwndStatus);
	}
}

void DiskTestDoIt2(HWND hDlg) {
	char szFileName[MAX_PATH];
	char szErrorBuf[MAX_PATH];
	UINT timerID	= 0;
	HANDLE hFile	= INVALID_HANDLE_VALUE;
	HWND hwndStatus = NULL;
	char *buffer	= NULL;
	int drive;
	long lDiskBuffer, lMaxBytes;
	BOOL fSuccess;
	DWORD dwBufferMode;

	if (CB_ERR == (drive = SendDlgItemMessage(hDlg, IDC_DRIVE, CB_GETCURSEL, 0, 0)))
		return;

	drive = SendDlgItemMessage(hDlg, IDC_DRIVE, CB_GETITEMDATA, drive, 0);
	sprintf(szFileName,"%c:\\DISKTEST.BIN", drive+'A');

	lDiskBuffer		= GetDlgItemInt(hDlg, IDC_DISK_BUFFER, &fSuccess, FALSE);
	if (lDiskBuffer < 64)
		lDiskBuffer = 64;

	lDiskBuffer <<= 10;

	lMaxBytes		= GetDlgItemInt(hDlg, IDC_TOTAL_SIZE, &fSuccess, FALSE);
	lMaxBytes <<= 20;

	dwBufferMode = 0;
	if (IsDlgButtonChecked(hDlg, IDC_DISABLE_BUFFERING))
		dwBufferMode = FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING;

	try {
		LONG lTotalBytes=0;
		MSG msg;
		DWORD dwOpenMode;
		DWORD dwReadTimeStart, dwReadTimeEnd, dwWriteTimeStart, dwWriteTimeEnd;

		if (!(hwndStatus = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_DISKTEST2), hDlg, (DLGPROC)DiskTestStatusDlgProc)))
			throw "Umm, no status dialog.  Doh!";

		EnableWindow(hDlg, FALSE);

		if (!(buffer = (char *)VirtualAlloc(NULL, lDiskBuffer, MEM_COMMIT, PAGE_READWRITE)))
			throw "Couldn't allocate disk buffer";

		SetPriorityClass((HANDLE)GetCurrentProcess(), HIGH_PRIORITY_CLASS);

		////////////

		dwOpenMode = CREATE_NEW;

		for(;;) {
			char msgbuf[MAX_PATH + 64];

			hFile = CreateFile(
					szFileName,
					GENERIC_READ | GENERIC_WRITE,
					0,
					NULL,
					dwOpenMode,
					FILE_ATTRIBUTE_NORMAL | dwBufferMode,
					NULL
					);

			if (hFile == INVALID_HANDLE_VALUE) {
				if (GetLastError() == ERROR_FILE_EXISTS) {
					sprintf(msgbuf, "File %s already exists.  Overwrite?", szFileName);
					if (IDYES != MessageBox(hDlg, msgbuf, "Disk test warning", MB_YESNO))
						throw NULL;

					dwOpenMode = OPEN_ALWAYS;
				} else {
					strcpy(szErrorBuf, "Error opening file: '");
					FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), 0, szErrorBuf+22, (sizeof szErrorBuf)-22, NULL);
					strcat(szErrorBuf, "'");

					throw szErrorBuf;
				}
			} else
				break;

		}

		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				throw (char *)NULL;

			if (!IsDialogMessage(hwndStatus, &msg) && !IsDialogMessage(hDlg, &msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		SetWindowText(hwndStatus, "Prewriting file...");
		while(lTotalBytes < lMaxBytes) {
			DWORD dwBytes = lMaxBytes - lTotalBytes;
			DWORD dwActual;

			if (dwBytes > lDiskBuffer)
				dwBytes = lDiskBuffer;

			if (!WriteFile(hFile, buffer, dwBytes, &dwActual, NULL) || dwActual != dwBytes)
				throw "I/O error";

			lTotalBytes += dwBytes;
		}
		FlushFileBuffers(hFile);

		SetWindowText(hwndStatus, "Writing to file...");
		SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		FlushFileBuffers(hFile);

		dwWriteTimeStart = GetTickCount();
		lTotalBytes=0;
		while(lTotalBytes < lMaxBytes) {
			DWORD dwBytes = lMaxBytes - lTotalBytes;
			DWORD dwActual;

			if (dwBytes > lDiskBuffer)
				dwBytes = lDiskBuffer;

			if (!WriteFile(hFile, buffer, dwBytes, &dwActual, NULL) || dwActual != dwBytes)
				throw "I/O error";

			lTotalBytes += dwBytes;
		}
		FlushFileBuffers(hFile);
		dwWriteTimeEnd = GetTickCount();

		SetWindowText(hwndStatus, "Reading from file...");

		SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
		FlushFileBuffers(hFile);

		dwReadTimeStart = GetTickCount();
		lTotalBytes=0;
		while(lTotalBytes < lMaxBytes) {
			DWORD dwBytes = lMaxBytes - lTotalBytes, dwActual;

			if (dwBytes > lDiskBuffer)
				dwBytes = lDiskBuffer;

			if (!ReadFile(hFile, buffer, dwBytes, &dwActual, NULL) || dwActual != dwBytes)
				throw "I/O error";

			lTotalBytes += dwBytes;
		}
		FlushFileBuffers(hFile);
		dwReadTimeEnd = GetTickCount();

		////////////
		HWND hwndWrite, hwndRead;

		SetWindowText(hwndStatus, "Test complete.");

		SetDlgItemText(hwndStatus, IDCANCEL, "Ok");

		hwndRead = GetDlgItem(hwndStatus, IDC_READ_SPEED);
		hwndWrite = GetDlgItem(hwndStatus, IDC_WRITE_SPEED);

		if (dwReadTimeStart == dwReadTimeEnd)
			SetWindowText(hwndRead, "(too fast to measure)");
		else
			PrintfWindowText(hwndRead , "%ldKB/s ", MulDiv(lMaxBytes>>10, 1000, dwReadTimeEnd-dwReadTimeStart));

		if (dwWriteTimeStart == dwWriteTimeEnd)
			SetWindowText(hwndRead, "(too fast to measure)");
		else
			PrintfWindowText(hwndWrite, "%ldKB/s ", MulDiv(lMaxBytes>>10, 1000, dwWriteTimeEnd-dwWriteTimeStart));

		while (GetMessage(&msg, NULL, 0, 0)) {
			if (!IsDialogMessage(hwndStatus, &msg) && !IsDialogMessage(hDlg, &msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

	} catch(char *s) {
		if (s)
			MessageBox(hDlg, s, "Disk Test Error", MB_OK | MB_ICONEXCLAMATION);
	}

	SetPriorityClass((HANDLE)GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
	if (hFile != INVALID_HANDLE_VALUE) {
		CloseHandle(hFile);
		DeleteFile(szFileName);
	}
	if (buffer) VirtualFree(buffer, 0, MEM_RELEASE);
	if (hwndStatus) {
		EnableWindow(hDlg, TRUE);
		SetFocus(hDlg);
		DestroyWindow(hwndStatus);
	}
}

///////////////////////////////////////////////////

DWORD dwDiskTestInitDlgHelpLookup[]={
	IDC_FRAME_SIZE,				IDH_BENCHMARK_FRAME_SIZE,
	IDC_FRAME_COUNT,			IDH_BENCHMARK_FRAME_COUNT,
	IDC_FRAME_BUFFERS,			IDH_BENCHMARK_FRAME_BUFFERS,
	IDC_FRAME_RATE,				IDH_BENCHMARK_FRAME_RATE,
	IDC_DISK_BUFFER,			IDH_BENCHMARK_DISK_BUFFER,
	IDC_DATA_RATE,				IDH_BENCHMARK_DATA_RATE,
	IDC_BUFFERING_NONE,			IDH_BENCHMARK_BUFFERING,
	IDC_BUFFERING_COMBINEONLY,	IDH_BENCHMARK_BUFFERING,
	IDC_BUFFERING_FULL,			IDH_BENCHMARK_BUFFERING,
	0,
};

void DiskTestUpdateFields(HWND hDlg) {
	DiskTestParameters dtp;

	if (!DiskTestCreateStruc(hDlg, &dtp))
		SetDlgItemText(hDlg, IDC_DATA_RATE, "<unknown>");
	else
		SetDlgItemInt(hDlg, IDC_DATA_RATE,(LONG)(
			(((__int64)dtp.lFrameSize * 1000000 + dtp.lMicroSecPerFrame/2) / dtp.lMicroSecPerFrame + 1023) / 1024),
			FALSE);
}

void DiskTestInitDlg(HWND hDlg) {
	DWORD dwDriveMask = GetLogicalDrives();
	HWND hwndCombo = GetDlgItem(hDlg, IDC_DRIVE);
	char buf[256], szVolName[64];
	int i;
	BOOL fHaveSelection = FALSE;

	for(i=0; i<26; i++) {
		if (dwDriveMask & (1L<<i)) {
			UINT uiDriveType;
			char *lpszDesc;
			int index;

			sprintf(buf, "%c:\\", i+'A');
			uiDriveType = GetDriveType(buf);

			switch(uiDriveType) {
			case 1:					continue;	// no root directory!?
			case DRIVE_CDROM:		continue;	// How do you capture to a CD-ROM?
			case DRIVE_REMOVABLE:	lpszDesc = "Removable disk";	break;
			case DRIVE_FIXED:		lpszDesc = "Hard disk";			break;
			case DRIVE_REMOTE:		lpszDesc = "Network drive";		break;
			case DRIVE_RAMDISK:		lpszDesc = "Ramdisk";			break;
			default:				lpszDesc = "";					break;
			}

			if (uiDriveType==DRIVE_FIXED && GetVolumeInformation(buf, szVolName, sizeof szVolName, NULL, NULL, NULL, NULL, 0))
				sprintf(buf, "%c: %s [%s]", i+'A', lpszDesc, szVolName);
			else
				sprintf(buf, "%c: %s", i+'A', lpszDesc);

			index = SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)buf);

			if (index != CB_ERR) {
				SendMessage(hwndCombo, CB_SETITEMDATA, (WPARAM)index, i);
				if (!fHaveSelection && (i>=2 || !(dwDriveMask>>(i+1)))) {
					fHaveSelection = TRUE;

					SendMessage(hwndCombo, CB_SETCURSEL, (WPARAM)index, 0);
				}
			}
		}
	}

	SetDlgItemText(hDlg, IDC_FRAME_SIZE, "320x240x16");
	SetDlgItemText(hDlg, IDC_FRAME_COUNT, "200");
	SetDlgItemText(hDlg, IDC_FRAME_BUFFERS, "50");
	SetDlgItemText(hDlg, IDC_FRAME_RATE, "15.000");
	SetDlgItemText(hDlg, IDC_DISK_BUFFER, "1024");
	SetDlgItemText(hDlg, IDC_TOTAL_SIZE, "50");
	DiskTestUpdateFields(hDlg);

	CheckDlgButton(hDlg, IDC_BUFFERING_NONE, BST_CHECKED);
}

BOOL APIENTRY DiskTestDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_INITDIALOG:
			DiskTestInitDlg(hDlg);
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_FRAME_SIZE:
			case IDC_FRAME_COUNT:
			case IDC_FRAME_BUFFERS:
			case IDC_FRAME_RATE:
			case IDC_DISK_BUFFER:
				if (HIWORD(wParam)==EN_UPDATE)
					DiskTestUpdateFields(hDlg);
				break;
			case IDOK:
				DiskTestDoIt(hDlg);
				return TRUE;
			case IDC_BENCHDISK:
				DiskTestDoIt2(hDlg);
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
			break;

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					HelpPopupByID(hDlg, lphi->iCtrlId, dwDiskTestInitDlgHelpLookup);
			}
			return TRUE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////
//
//	About...
//
///////////////////////////////////////////////////////////////////////////

BOOL APIENTRY AboutDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
		case WM_INITDIALOG:
			SetDlgItemText(hDlg, IDC_FINALS_SUCK,
#ifdef _DEBUG
				"Debug build"
#else
				"Release build"
#endif
				" ("__DATE__" "__TIME__")");

			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
	}
	return FALSE;
}
