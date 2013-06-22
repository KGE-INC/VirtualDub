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
#include <shellapi.h>
#include <vd2/system/VDString.h>
#include <vd2/system/filesys.h>
#include <vd2/system/zip.h>
#include <vd2/system/Error.h>
#include "oshelper.h"

extern const char g_szError[];

void Draw3DRect(HDC hDC, LONG x, LONG y, LONG dx, LONG dy, BOOL inverted) {
	HPEN hPenOld;

	hPenOld = (HPEN)SelectObject(hDC, GetStockObject(inverted ? WHITE_PEN : BLACK_PEN));
	MoveToEx(hDC, x, y+dy-1, NULL);
	LineTo(hDC, x+dx-1, y+dy-1);
	LineTo(hDC, x+dx-1, y);
	DeleteObject(SelectObject(hDC, GetStockObject(inverted ? BLACK_PEN : WHITE_PEN)));
	MoveToEx(hDC, x, y+dy-1, NULL);
	LineTo(hDC, x, y);
	LineTo(hDC, x+dx-1, y);
	DeleteObject(SelectObject(hDC, hPenOld));
}

// We follow MAME32's lead and put our keys in:
//
//	HKEY_CURRENT_USER\Software\Freeware\VirtualDub\

HKEY OpenConfigKey(const char *szKeyName) {
	char temp[MAX_PATH]="Software\\Freeware\\VirtualDub";
	HKEY hkey;

	if (szKeyName) {
		strcat(temp, "\\");
		strcat(temp, szKeyName);
	}

	return RegOpenKeyEx(HKEY_CURRENT_USER, temp, 0, KEY_ALL_ACCESS, &hkey)==ERROR_SUCCESS
			? hkey
			: NULL;
}

HKEY CreateConfigKey(const char *szKeyName) {
	char temp[MAX_PATH]="Software\\Freeware\\VirtualDub";
	HKEY hkey;
	DWORD dwDisposition;

	if (szKeyName) {
		strcat(temp, "\\");
		strcat(temp, szKeyName);
	}

	return RegCreateKeyEx(HKEY_CURRENT_USER, temp, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, &dwDisposition)==ERROR_SUCCESS
			? hkey
			: NULL;
}

BOOL DeleteConfigValue(const char *szKeyName, const char *szValueName) {
	HKEY hkey;
	BOOL success;

	if (!(hkey = OpenConfigKey(szKeyName)))
		return FALSE;

	success = (RegDeleteValue(hkey, szValueName) == ERROR_SUCCESS);

	RegCloseKey(hkey);

	return success;
}

BOOL QueryConfigString(const char *szKeyName, const char *szValueName, char *lpBuffer, int cbBuffer) {
	HKEY hkey;
	BOOL success;
	DWORD type;

	if (!(hkey = OpenConfigKey(szKeyName)))
		return FALSE;

	success = (ERROR_SUCCESS == RegQueryValueEx(hkey, szValueName, 0, &type, (LPBYTE)lpBuffer, (LPDWORD)&cbBuffer));

	RegCloseKey(hkey);

	return success;
}

DWORD QueryConfigBinary(const char *szKeyName, const char *szValueName, char *lpBuffer, int cbBuffer) {
	HKEY hkey;
	BOOL success;
	DWORD type;
	DWORD size = cbBuffer;

	if (!(hkey = OpenConfigKey(szKeyName)))
		return 0;

	success = (ERROR_SUCCESS == RegQueryValueEx(hkey, szValueName, 0, &type, (LPBYTE)lpBuffer, (LPDWORD)&size));

	RegCloseKey(hkey);

	return success ? size : 0;
}

BOOL QueryConfigDword(const char *szKeyName, const char *szValueName, DWORD *lpdwData) {
	HKEY hkey;
	BOOL success;
	DWORD type;
	DWORD size = sizeof(DWORD);

	if (!(hkey = OpenConfigKey(szKeyName)))
		return 0;

	success = (ERROR_SUCCESS == RegQueryValueEx(hkey, szValueName, 0, &type, (LPBYTE)lpdwData, (LPDWORD)&size));

	RegCloseKey(hkey);

	return success;
}

BOOL SetConfigString(const char *szKeyName, const char *szValueName, const char *lpBuffer) {
	HKEY hkey;
	BOOL success;

	if (!(hkey = CreateConfigKey(szKeyName)))
		return FALSE;

	success = (ERROR_SUCCESS == RegSetValueEx(hkey, szValueName, 0, REG_SZ, (LPBYTE)lpBuffer, strlen(lpBuffer)+1));

	RegCloseKey(hkey);

	return success;
}

BOOL SetConfigBinary(const char *szKeyName, const char *szValueName, const char *lpBuffer, int cbBuffer) {
	HKEY hkey;
	BOOL success;

	if (!(hkey = CreateConfigKey(szKeyName)))
		return FALSE;

	success = (ERROR_SUCCESS == RegSetValueEx(hkey, szValueName, 0, REG_BINARY, (LPBYTE)lpBuffer, cbBuffer));

	RegCloseKey(hkey);

	return success;
}

BOOL SetConfigDword(const char *szKeyName, const char *szValueName, DWORD dwData) {
	HKEY hkey;
	BOOL success;

	if (!(hkey = CreateConfigKey(szKeyName)))
		return FALSE;

	success = (ERROR_SUCCESS == RegSetValueEx(hkey, szValueName, 0, REG_DWORD, (LPBYTE)&dwData, sizeof(DWORD)));

	RegCloseKey(hkey);

	return success;
}

///////////////////////////////////////////////////////////////////////////
//
//	help support
//
///////////////////////////////////////////////////////////////////////////

static bool VDIsWindowsNT() {
	static bool bIsNT = (LONG)GetVersion() >= 0;

	return bIsNT;
}

VDStringW VDGetProgramPath() {
	union {
		wchar_t w[MAX_PATH];
		char a[MAX_PATH];
	} buf;

	VDStringW wstr;

	if (VDIsWindowsNT()) {
		wcscpy(buf.w, L".");
		if (GetModuleFileNameW(NULL, buf.w, MAX_PATH))
			*VDFileSplitPath(buf.w) = 0;
		wstr = buf.w;
	} else {
		strcpy(buf.a, ".");
		if (GetModuleFileNameA(NULL, buf.a, MAX_PATH))
			*VDFileSplitPath(buf.a) = 0;
		wstr = VDTextAToW(buf.a, -1);
	}

	VDStringW wstr2(VDGetFullPath(wstr));

	return wstr2;
}

VDStringW VDGetHelpPath() {
	return VDMakePath(VDGetProgramPath(), VDStringW(L"help"));
}

void VDUnpackHelp(const VDStringW& path) {
	std::vector<char> innerzipdata;
	uint32 chkvalue;		// value used to identify whether help directory is up to date

	// Decompress the first file in the outer zip to make the inner zip archive.
	{
		VDFileStream			helpzipfile(VDMakePath(VDGetProgramPath(), VDStringW(L"VirtualDub.vdhelp")).c_str());

		chkvalue = helpzipfile.Length();

		VDZipArchive			helpzip;
		helpzip.Init(&helpzipfile);
		const VDZipArchive::FileInfo& fi = helpzip.GetFileInfo(0);
		VDZipStream				zipstream(helpzip.OpenRawStream(0), fi.mCompressedSize, !fi.mbPacked);

		innerzipdata.resize(fi.mUncompressedSize);

		zipstream.Read(&innerzipdata.front(), innerzipdata.size());
	}

	// Decompress the inner zip archive.

	VDMemoryStream	innerzipstream(&innerzipdata[0], innerzipdata.size());
	VDZipArchive	innerzip;
	
	innerzip.Init(&innerzipstream);

	if (!VDDoesPathExist(path))
		VDCreateDirectory(path);

	const sint32 entries = innerzip.GetFileCount();

	for(sint32 idx=0; idx<entries; ++idx) {
		const VDZipArchive::FileInfo& fi = innerzip.GetFileInfo(idx);

		// skip directories -- Zip paths always use forward slashes.
		if (!fi.mFileName.empty() && fi.mFileName[fi.mFileName.size()-1] == '/')
			continue;

		// create partial paths as necessary
		const VDStringW name(VDTextAToW(fi.mFileName));
		const wchar_t *base = name.c_str(), *s = base;

		while(const wchar_t *t = wcschr(s, L'/')) {
			VDStringW dirPath(VDMakePath(path, VDStringW(base, t - base)));

			if (!VDDoesPathExist(dirPath))
				VDCreateDirectory(dirPath);

			s = t+1;
		}

		VDStringW filePath(VDMakePath(path, name));

		VDDEBUG("file: %-40S %10d <- %10d\n", filePath.c_str(), fi.mCompressedSize, fi.mUncompressedSize);

		// unpack the file

		VDZipStream unpackstream(innerzip.OpenRawStream(idx), fi.mCompressedSize, !fi.mbPacked);

		std::vector<char> fileData(fi.mUncompressedSize);
		unpackstream.Read(&fileData.front(), fileData.size());

		VDFile outfile(filePath.c_str(), nsVDFile::kWrite | nsVDFile::kCreateAlways);
		outfile.write(&fileData[0], fileData.size());
	}

	// write help identifier
	VDFile chkfile(VDMakePath(path, VDStringW(L"helpfile.id")).c_str(), nsVDFile::kWrite | nsVDFile::kCreateAlways);

	chkfile.write(&chkvalue, 4);
	chkfile.close();
}

bool VDIsHelpUpToDate(const VDStringW& helpPath) {
	if (!VDDoesPathExist(helpPath))
		return false;

	bool bUpToDate = false;

	try {
		VDFile helpzipfile(VDMakePath(VDGetProgramPath(), VDStringW(L"VirtualDub.vdhelp")).c_str());
		VDFile chkfile(VDMakePath(helpPath, VDStringW(L"helpfile.id")).c_str(), nsVDFile::kRead | nsVDFile::kOpenExisting);
		uint32 chk;

		chkfile.read(&chk, 4);

		if (helpzipfile.size() == chk)
			bUpToDate = true;
	} catch(const MyError&) {
		// eat errors
	}

	return bUpToDate;
}

void VDShowHelp(HWND hwnd, const wchar_t *filename) {
	try {
		VDStringW helpPath(VDGetHelpPath());

		if (!VDIsHelpUpToDate(helpPath)) {
			VDUnpackHelp(helpPath);
		}

		LaunchURL(VDTextWToA(VDMakePath(helpPath, VDStringW(filename?filename:L"index.html"))).c_str());
//	WinHelp(hwnd, g_szHelpPath, HELP_FINDER, 0);
	} catch(const MyError& e) {
		e.post(hwnd, g_szError);
	}
}

char *MergePath(char *path, const char *fn) {
	char *slash=NULL, *colon=NULL;
	char *s = path;

	if (!*s) {
		strcpy(path, fn);
		return path;
	}

	while(*s)
		++s;

	if (s[-1]!='\\' && s[-1]!=':')
		*s++ = '\\';

	strcpy(s, fn);

	return path;
}

char *SplitPathRoot(char *dst, const char *path) {

	if (!path)
		return NULL;

	// C:

	if (isalpha(path[0]) && path[1]==':') {
		dst[0] = path[0];
		dst[1] = ':';
		dst[2] = '\\';
		dst[3] = 0;

		return dst;
	}

	// UNC path?

	if (path[0] == '\\' && path[1] == '\\') {
		const char *s = path+2;
		char *t = dst;

		*t++ = '\\';
		*t++ = '\\';

		while(*s && *s != '\\')
			*t++ = *s++;

		if (*s)
			*t++ = *s++;

		while(*s && *s != '\\')
			*t++ = *s++;

		*t++ = '\\';
		*t = 0;

		return dst;
	}

	return NULL;
}

bool IsFilenameOnFATVolume(const char *pszFilename) {
	char szFileRoot[MAX_PATH];
	DWORD dwMaxComponentLength;
	DWORD dwFSFlags;
	char szFilesystem[MAX_PATH];

	if (!GetVolumeInformationA(SplitPathRoot(szFileRoot, pszFilename),
			NULL, 0,		// Volume name buffer
			NULL,			// Serial number buffer
			&dwMaxComponentLength,
			&dwFSFlags,
			szFilesystem,
			sizeof szFilesystem))
		return false;

	return !strnicmp(szFilesystem, "FAT", 3);
}

bool IsFilenameOnFATVolume(const wchar_t *pszFilename) {
	if (GetVersion() & 0x80000000)
		return IsFilenameOnFATVolume(VDTextWToA(pszFilename).c_str());

	wchar_t szFileRoot[MAX_PATH];
	DWORD dwMaxComponentLength;
	DWORD dwFSFlags;
	wchar_t szFilesystem[MAX_PATH];

	wcscpy(szFileRoot, pszFilename);
	*const_cast<wchar_t *>(VDFileSplitRoot(szFileRoot)) = 0;

	if (!GetVolumeInformationW(*szFileRoot ? szFileRoot : NULL,
			NULL, 0,		// Volume name buffer
			NULL,			// Serial number buffer
			&dwMaxComponentLength,
			&dwFSFlags,
			szFilesystem,
			MAX_PATH))
		return false;

	return !wcsnicmp(szFilesystem, L"FAT", 3);

}

HWND APIENTRY VDGetAncestorW95(HWND hwnd, UINT gaFlags) {
	switch(gaFlags) {
	case GA_PARENT:
		return GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD ? GetParent(hwnd) : NULL;
	case GA_ROOT:
		while(GetWindowLong(hwnd, GWL_STYLE) & WS_CHILD)
			hwnd = GetParent(hwnd);
		return hwnd;
	case GA_ROOTOWNER:
		while(HWND hwndParent = GetParent(hwnd))
			hwnd = hwndParent;
		return hwnd;
	default:
		VDNEVERHERE;
		return NULL;
	}
}

namespace {
	typedef HWND (APIENTRY *tpGetAncestor)(HWND, UINT);
}

HWND VDGetAncestorW32(HWND hwnd, UINT gaFlags) {
	struct local {
		static tpGetAncestor FindGetAncestor() {
			tpGetAncestor ga = (tpGetAncestor)GetProcAddress(GetModuleHandle("user32"), "GetAncestor");

			if (!ga)
				ga = VDGetAncestorW95;

			return ga;
		}
	};

	static tpGetAncestor spGetAncestor = local::FindGetAncestor();

	return spGetAncestor(hwnd, gaFlags);
}

///////////////////////////////////////////////////////////////////////////

void LaunchURL(const char *pURL) {
	ShellExecute(NULL, "open", pURL, NULL, NULL, SW_SHOWNORMAL);
}

///////////////////////////////////////////////////////////////////////////

bool EnableCPUTracking() {
	HKEY hOpen;
	DWORD cbData;
	DWORD dwType;
	LPBYTE pByte;
	DWORD rc;

	bool fSuccess = true;

    if ( (rc = RegOpenKeyEx(HKEY_DYN_DATA,"PerfStats\\StartStat", 0,
					KEY_READ, &hOpen)) == ERROR_SUCCESS) {

		// query to get data size
		if ( (rc = RegQueryValueEx(hOpen,"KERNEL\\CPUUsage",NULL,&dwType,
				NULL, &cbData )) == ERROR_SUCCESS) {

			pByte = (LPBYTE)allocmem(cbData);

			rc = RegQueryValueEx(hOpen,"KERNEL\\CPUUsage",NULL,&dwType, pByte,
                              &cbData );

			freemem(pByte);
		} else
			fSuccess = false;

		RegCloseKey(hOpen);
	} else
		fSuccess = false;

	return fSuccess;
}

bool DisableCPUTracking() {
	HKEY hOpen;
	DWORD cbData;
	DWORD dwType;
	LPBYTE pByte;
	DWORD rc;

	bool fSuccess = true;

    if ( (rc = RegOpenKeyEx(HKEY_DYN_DATA,"PerfStats\\StopStat", 0,
					KEY_READ, &hOpen)) == ERROR_SUCCESS) {

		// query to get data size
		if ( (rc = RegQueryValueEx(hOpen,"KERNEL\\CPUUsage",NULL,&dwType,
				NULL, &cbData )) == ERROR_SUCCESS) {

			pByte = (LPBYTE)allocmem(cbData);

			rc = RegQueryValueEx(hOpen,"KERNEL\\CPUUsage",NULL,&dwType, pByte,
                              &cbData );

			freemem(pByte);
		} else
			fSuccess = false;

		RegCloseKey(hOpen);
	} else
		fSuccess = false;

	return fSuccess;
}

CPUUsageReader::CPUUsageReader() {
	FILETIME ftCreate, ftExit;

	hkeyKernelCPU = NULL;
	fNTMethod = false;

	if (GetProcessTimes(GetCurrentProcess(), &ftCreate, &ftExit, (FILETIME *)&kt_last, (FILETIME *)&ut_last)) {

		// Using Windows NT/2000 method
		GetSystemTimeAsFileTime((FILETIME *)&st_last);

		fNTMethod = true;

	} else {

		// Using Windows 95/98 method

		HKEY hkey;

		if (EnableCPUTracking()) {

			if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_DYN_DATA, "PerfStats\\StatData", 0, KEY_READ, &hkey)) {
				hkeyKernelCPU = hkey;
			} else
				DisableCPUTracking();
		}
	}
}

CPUUsageReader::~CPUUsageReader() {
	if (hkeyKernelCPU) {
		RegCloseKey(hkeyKernelCPU);
		DisableCPUTracking();
	}
}

int CPUUsageReader::read() {

	if (hkeyKernelCPU) {
		DWORD type;
		DWORD dwUsage;
		DWORD size = sizeof dwUsage;

		if (ERROR_SUCCESS == RegQueryValueEx(hkeyKernelCPU, "KERNEL\\CPUUsage", 0, &type, (LPBYTE)&dwUsage, (LPDWORD)&size))
			return (int)dwUsage;
		
		return -1;
	} else if (fNTMethod) {
		FILETIME ftCreate, ftExit;
		unsigned __int64 kt, st, ut;
		int cpu;

		GetProcessTimes(GetCurrentProcess(), &ftCreate, &ftExit, (FILETIME *)&kt, (FILETIME *)&ut);
		GetSystemTimeAsFileTime((FILETIME *)&st);

		if (st == st_last)
			return 100;
		else
			cpu = (int)((100 * (kt + ut - kt_last - ut_last) + (st - st_last)/2) / (st - st_last));

		kt_last = kt;
		ut_last = ut;
		st_last = st;

		return cpu;
	}

	return -1;
}
