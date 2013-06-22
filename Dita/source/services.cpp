#pragma warning(disable: 4786)

#include <map>
#include <string.h>

#define _WIN32_WINNT 0x0500

#include <windows.h>
#include <commdlg.h>
#include <objbase.h>
#include <shlobj.h>

#include <vd2/system/text.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/VDString.h>
#include <vd2/Dita/services.h>
#include <vd2/Dita/interface.h>

#ifndef OPENFILENAME_SIZE_VERSION_400
#define OPENFILENAME_SIZE_VERSION_400 0x4c
#endif

#ifndef BIF_NEWDIALOGSTYLE
#define BIF_NEWDIALOGSTYLE     0x0040
#endif

///////////////////////////////////////////////////////////////////////////

IVDUIContext *VDGetUIContext() {
	static vdautoptr<IVDUIContext> spctx(VDCreateUIContext());

	return spctx;
}

///////////////////////////////////////////////////////////////////////////

struct FilespecEntry {
	wchar_t szFile[MAX_PATH];
};

typedef std::map<long, FilespecEntry> tFilespecMap;

// Visual C++ 7.0 has a bug with lock initialization in the STL -- the lock
// code uses a normal constructor, and thus usually executes too late for
// static construction.
tFilespecMap *g_pFilespecMap;

///////////////////////////////////////////////////////////////////////////

namespace {
	int FileFilterLength(const wchar_t *pszFilter) {
		const wchar_t *s = pszFilter;

		while(*s) {
			while(*s++);
			while(*s++);
		}

		return s - pszFilter;
	}
}

///////////////////////////////////////////////////////////////////////////

void VDInitFilespecSystem() {
	if (!g_pFilespecMap)
		g_pFilespecMap = new tFilespecMap;
}

const VDStringW VDGetLoadFileNameReadOnly(long nKey, VDGUIHandle ctxParent, const wchar_t *pszTitle, const wchar_t *pszFilters, const wchar_t *pszExt, bool& bReadOnly) {
	VDInitFilespecSystem();

	tFilespecMap::iterator it = g_pFilespecMap->find(nKey);

	if (it == g_pFilespecMap->end()) {
		std::pair<tFilespecMap::iterator, bool> r = g_pFilespecMap->insert(tFilespecMap::value_type(nKey, FilespecEntry()));

		if (!r.second) {
			VDStringW empty;
			return empty;
		}

		it = r.first;

		(*it).second.szFile[0] = 0;
	}

	FilespecEntry& fsent = (*it).second;

	VDASSERTCT(sizeof(OPENFILENAMEA) == sizeof(OPENFILENAMEW));
	union {
		OPENFILENAMEA a;
		OPENFILENAMEW w;
	} ofn={0};

	ofn.w.lStructSize		= OPENFILENAME_SIZE_VERSION_400;
	ofn.w.hwndOwner			= (HWND)ctxParent;
	ofn.w.lpstrCustomFilter	= NULL;
	ofn.w.nFilterIndex		= 0;
	ofn.w.lpstrFileTitle	= NULL;
	ofn.w.lpstrInitialDir	= NULL;
	ofn.w.Flags				= OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST|OFN_ENABLESIZING|OFN_EXPLORER|OFN_OVERWRITEPROMPT;

	if (!bReadOnly)
		ofn.w.Flags |= OFN_HIDEREADONLY;

	VDStringW strFilename;

	if ((sint32)GetVersion() < 0) {		// Windows 95/98
		VDStringA strFilters(VDTextWToA(pszFilters, FileFilterLength(pszFilters)));
		VDStringA strDefExt(VDTextWToA(pszExt, -1));
		VDStringA strTitle(VDTextWToA(pszTitle, -1));

		char szFile[MAX_PATH];

		VDTextWToA(szFile, sizeof szFile, fsent.szFile, -1);

		ofn.a.lpstrFilter		= strFilters.c_str();
		ofn.a.lpstrFile			= szFile;
		ofn.a.nMaxFile			= sizeof(szFile) / sizeof(szFile[0]);
		ofn.a.lpstrTitle		= strTitle.c_str();
		ofn.a.lpstrDefExt		= strDefExt.c_str();

		if (GetOpenFileNameA(&ofn.a)) {
			VDTextAToW(fsent.szFile, sizeof fsent.szFile, szFile, -1);
			strFilename = fsent.szFile;
		}
	} else {
		ofn.w.lpstrFilter		= pszFilters;
		ofn.w.lpstrFile			= fsent.szFile;
		ofn.w.nMaxFile			= sizeof(fsent.szFile) / sizeof(fsent.szFile[0]);
		ofn.w.lpstrTitle		= pszTitle;
		ofn.w.lpstrDefExt		= pszExt;

		if (GetOpenFileNameW(&ofn.w)) {
			strFilename = fsent.szFile;
		}
	}

	bReadOnly = !!(ofn.a.Flags & OFN_READONLY);

	return strFilename;
}

const VDStringW VDGetLoadFileName(long nKey, VDGUIHandle ctxParent, const wchar_t *pszTitle, const wchar_t *pszFilters, const wchar_t *pszExt) {
	bool bReadOnly = false;

	return VDGetLoadFileNameReadOnly(nKey, ctxParent, pszTitle, pszFilters, pszExt, bReadOnly);
}

const VDStringW VDGetSaveFileName(long nKey, VDGUIHandle ctxParent, const wchar_t *pszTitle, const wchar_t *pszFilters, const wchar_t *pszExt) {
	VDInitFilespecSystem();

	tFilespecMap::iterator it = g_pFilespecMap->find(nKey);

	if (it == g_pFilespecMap->end()) {
		std::pair<tFilespecMap::iterator, bool> r = g_pFilespecMap->insert(tFilespecMap::value_type(nKey, FilespecEntry()));

		if (!r.second) {
			VDStringW empty;
			return empty;
		}

		it = r.first;

		(*it).second.szFile[0] = 0;
	}

	FilespecEntry& fsent = (*it).second;
	VDASSERTCT(sizeof(OPENFILENAMEA) == sizeof(OPENFILENAMEW));
	union {
		OPENFILENAMEA a;
		OPENFILENAMEW w;
	} ofn={0};

	ofn.a.lStructSize		= OPENFILENAME_SIZE_VERSION_400;
	ofn.a.hwndOwner			= (HWND)ctxParent;
	ofn.a.lpstrCustomFilter	= NULL;
	ofn.a.nFilterIndex		= 0;
	ofn.a.lpstrFileTitle	= NULL;
	ofn.a.lpstrInitialDir	= NULL;
	ofn.a.Flags				= OFN_PATHMUSTEXIST|OFN_ENABLESIZING|OFN_HIDEREADONLY|OFN_EXPLORER|OFN_OVERWRITEPROMPT;

	VDStringW strFilename;

	if ((sint32)GetVersion() < 0) {
		VDStringA strFilters(VDTextWToA(pszFilters, FileFilterLength(pszFilters)));
		VDStringA strDefExt(VDTextWToA(pszExt, -1));
		VDStringA strTitle(VDTextWToA(pszTitle, -1));

		char szFile[MAX_PATH];

		VDTextWToA(szFile, sizeof szFile, fsent.szFile, -1);

		ofn.a.lpstrFilter		= strFilters.c_str();
		ofn.a.lpstrFile			= szFile;
		ofn.a.nMaxFile			= sizeof(szFile) / sizeof(szFile[0]);
		ofn.a.lpstrTitle		= strTitle.c_str();
		ofn.a.lpstrDefExt		= strDefExt.c_str();

		if (GetSaveFileNameA(&ofn.a)) {
			VDTextAToW(fsent.szFile, sizeof(fsent.szFile)/sizeof(fsent.szFile[0]), szFile, -1);

			strFilename = fsent.szFile;
		}
	} else {
		ofn.w.lpstrFilter		= pszFilters;
		ofn.w.lpstrFile			= fsent.szFile;
		ofn.w.nMaxFile			= sizeof(fsent.szFile) / sizeof(fsent.szFile[0]);
		ofn.w.lpstrTitle		= pszTitle;
		ofn.w.lpstrDefExt		= pszExt;

		if (GetSaveFileNameW(&ofn.w))
			strFilename = fsent.szFile;
	}

	return strFilename;
}

///////////////////////////////////////////////////////////////////////////

struct DirspecEntry {
	wchar_t szFile[MAX_PATH];
};

typedef std::map<long, DirspecEntry> tDirspecMap;
tDirspecMap *g_pDirspecMap;

///////////////////////////////////////////////////////////////////////////

const VDStringW VDGetDirectory(long nKey, VDGUIHandle ctxParent, const wchar_t *pszTitle) {
	if (!g_pDirspecMap)
		g_pDirspecMap = new tDirspecMap;

	tDirspecMap::iterator it = g_pDirspecMap->find(nKey);

	if (it == g_pDirspecMap->end()) {
		std::pair<tDirspecMap::iterator, bool> r = g_pDirspecMap->insert(tDirspecMap::value_type(nKey, DirspecEntry()));

		if (!r.second) {
			VDStringW empty;
			return empty;
		}

		it = r.first;

		(*it).second.szFile[0] = 0;
	}

	DirspecEntry& fsent = (*it).second;
	bool bSuccess = false;

	if (SUCCEEDED(CoInitialize(NULL))) {
		IMalloc *pMalloc;

		if (SUCCEEDED(SHGetMalloc(&pMalloc))) {

			if ((LONG)GetVersion() < 0) {		// Windows 9x
				char *pszBuffer;

				if (pszBuffer = (char *)pMalloc->Alloc(MAX_PATH)) {
					BROWSEINFOA bi;
					ITEMIDLIST *pidlBrowse;

					bi.hwndOwner		= (HWND)ctxParent;
					bi.pidlRoot			= NULL;
					bi.pszDisplayName	= pszBuffer;
					bi.lpszTitle		= VDFastTextWToA(pszTitle);
					bi.ulFlags			= BIF_EDITBOX | /*BIF_NEWDIALOGSTYLE |*/ BIF_RETURNONLYFSDIRS | BIF_VALIDATE;
					bi.lpfn				= NULL;

					if (pidlBrowse = SHBrowseForFolderA(&bi)) {
						if (SHGetPathFromIDListA(pidlBrowse, pszBuffer)) {
							VDTextAToW(fsent.szFile, MAX_PATH, pszBuffer);
							bSuccess = true;
						}

						pMalloc->Free(pidlBrowse);
					}
					pMalloc->Free(pszBuffer);
				}
			} else {
				HMODULE hmod = GetModuleHandle("shell32.dll");		// We know shell32 is loaded because we hard link to SHBrowseForFolderA.
				typedef LPITEMIDLIST (APIENTRY *tpSHBrowseForFolderW)(LPBROWSEINFOW);
				typedef BOOL (APIENTRY *tpSHGetPathFromIDListW)(LPCITEMIDLIST pidl, LPWSTR pszPath);
				tpSHBrowseForFolderW pSHBrowseForFolderW = (tpSHBrowseForFolderW)GetProcAddress(hmod, "SHBrowseForFolderW");
				tpSHGetPathFromIDListW pSHGetPathFromIDListW = (tpSHGetPathFromIDListW)GetProcAddress(hmod, "SHGetPathFromIDListW");

				if (pSHBrowseForFolderW && pSHGetPathFromIDListW) {
					if (wchar_t *pszBuffer = (wchar_t *)pMalloc->Alloc(MAX_PATH * sizeof(wchar_t))) {
						BROWSEINFOW bi;
						ITEMIDLIST *pidlBrowse;

						bi.hwndOwner		= (HWND)ctxParent;
						bi.pidlRoot			= NULL;
						bi.pszDisplayName	= pszBuffer;
						bi.lpszTitle		= pszTitle;
						bi.ulFlags			= BIF_EDITBOX | /*BIF_NEWDIALOGSTYLE |*/ BIF_RETURNONLYFSDIRS | BIF_VALIDATE;
						bi.lpfn				= NULL;

						if (pidlBrowse = pSHBrowseForFolderW(&bi)) {
							if (pSHGetPathFromIDListW(pidlBrowse, pszBuffer)) {
								wcscpy(fsent.szFile, pszBuffer);
								bSuccess = true;
							}

							pMalloc->Free(pidlBrowse);
						}
						pMalloc->Free(pszBuffer);
					}
				}
			}
		}

		CoUninitialize();
	}

	VDStringW strDir;

	if (bSuccess)
		strDir = fsent.szFile;

	return strDir;
}