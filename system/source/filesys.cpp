#include <wchar.h>
#include <string.h>

#include <vd2/system/VDString.h>
#include <vd2/system/filesys.h>

///////////////////////////////////////////////////////////////////////////

template<class T, class U>
static inline T splitimpL(const T& string, const U *s, int offset) {
	const U *p = string.c_str();
	return T(p, (s+offset) - p);
}

template<class T, class U>
static inline T splitimpR(const T& string, const U *s, int offset) {
	const U *p = string.c_str();
	return T(s+offset);
}

template<class T, class U>
static inline T splitimp2L(const T& string, const U *s, int offset) {
	const U *p = string.c_str();
	return T(p, (s+offset) - p);
}

template<class T, class U>
static inline T splitimp2R(const T& string, const U *s, int offset) {
	const U *p = string.c_str();
	return T(s+offset);
}

///////////////////////////////////////////////////////////////////////////

const char *VDFileSplitPath(const char *s) {
	const char *lastsep = s;

	while(*s++)
		if (s[-1] == ':' || s[-1] == '\\' || s[-1] == '/')
			lastsep = s;

	return lastsep;
}

const wchar_t *VDFileSplitPath(const wchar_t *s) {
	const wchar_t *lastsep = s;

	while(*s++)
		if (s[-1] == L':' || s[-1] == L'\\' || s[-1] == L'/')
			lastsep = s;

	return lastsep;
}

VDString  VDFileSplitPathLeft (const VDString&  s) { return splitimpL(s, VDFileSplitPath(s.c_str()), -1); }
VDStringW VDFileSplitPathLeft (const VDStringW& s) { return splitimpL(s, VDFileSplitPath(s.c_str()), -1); }
VDString  VDFileSplitPathRight(const VDString&  s) { return splitimpR(s, VDFileSplitPath(s.c_str()), 0); }
VDStringW VDFileSplitPathRight(const VDStringW& s) { return splitimpR(s, VDFileSplitPath(s.c_str()), 0); }

const char *VDFileSplitRoot(const char *s) {
	const char *const t = s;

	while(*s && *s != ':' && *s != '/' && *s != '\\')
		++s;

	return *s ? *s == ':' && (s[1]=='/' || s[1]=='\\') ? s+2 : s+1 : t;
}

const wchar_t *VDFileSplitRoot(const wchar_t *s) {
	const wchar_t *const t = s;

	while(*s && *s != L':' && *s != L'/' && *s != L'\\')
		++s;

	return *s ? *s == L':' && (s[1]==L'/' || s[1]==L'\\') ? s+2 : s+1 : t;
}

VDString  VDFileSplitRoot(const VDString&  s) { return splitimp2L(s, VDFileSplitRoot(s.c_str()), 0); }
VDStringW VDFileSplitRoot(const VDStringW& s) { return splitimp2L(s, VDFileSplitRoot(s.c_str()), 0); }

const char *VDFileSplitExt(const char *s) {
	const char *t = s;

	while(*t)
		++t;

	const char *const end = t;

	while(t>s) {
		--t;

		if (*t == '.')
			return t;

		if (*t == ':' || *t == '\\' || *t == '/')
			break;
	}

	return NULL;
}

const wchar_t *VDFileSplitExt(const wchar_t *s) {
	const wchar_t *t = s;

	while(*t)
		++t;

	const wchar_t *const end = t;

	while(t>s) {
		--t;

		if (*t == L'.')
			return t;

		if (*t == L':' || *t == L'\\' || *t == L'/')
			break;
	}

	return end;
}

VDString  VDFileSplitExtLeft (const VDString&  s) { return splitimp2L(s, VDFileSplitExt(s.c_str()), 0); }
VDStringW VDFileSplitExtLeft (const VDStringW& s) { return splitimp2L(s, VDFileSplitExt(s.c_str()), 0); }
VDString  VDFileSplitExtRight(const VDString&  s) { return splitimp2R(s, VDFileSplitExt(s.c_str()), 0); }
VDStringW VDFileSplitExtRight(const VDStringW& s) { return splitimp2R(s, VDFileSplitExt(s.c_str()), 0); }

/////////////////////////////////////////////////////////////////////////////

#include <windows.h>

sint64 VDGetDiskFreeSpace(const VDStringW& path) {
	typedef BOOL (WINAPI *tpGetDiskFreeSpaceExA)(LPCSTR lpDirectoryName, PULARGE_INTEGER lpFreeBytesAvailable, PULARGE_INTEGER lpTotalNumberOfBytes, PULARGE_INTEGER lpTotalNumberOfFreeBytes);
	typedef BOOL (WINAPI *tpGetDiskFreeSpaceExW)(LPCWSTR lpDirectoryName, PULARGE_INTEGER lpFreeBytesAvailable, PULARGE_INTEGER lpTotalNumberOfBytes, PULARGE_INTEGER lpTotalNumberOfFreeBytes);

	static bool sbChecked = false;
	static tpGetDiskFreeSpaceExA spGetDiskFreeSpaceExA;
	static tpGetDiskFreeSpaceExW spGetDiskFreeSpaceExW;

	if (sbChecked) {
		HMODULE hmodKernel = GetModuleHandle("kernel32.dll");
		spGetDiskFreeSpaceExA = (tpGetDiskFreeSpaceExA)GetProcAddress(hmodKernel, "GetDiskFreeSpaceExA");
		spGetDiskFreeSpaceExW = (tpGetDiskFreeSpaceExW)GetProcAddress(hmodKernel, "GetDiskFreeSpaceExW");

		sbChecked = true;
	}

	if (spGetDiskFreeSpaceExA) {
		BOOL success;
		uint64 freeClient, totalBytes, totalFreeBytes;
		VDStringW directoryName(path);

		if (!path.empty()) {
			wchar_t c = path[path.length()-1];

			if (c != L'/' && c != L'\\')
				directoryName += L'\\';
		}

		if ((LONG)GetVersion() < 0)
			success = spGetDiskFreeSpaceExA(VDTextWToA(directoryName).c_str(), (PULARGE_INTEGER)&freeClient, (PULARGE_INTEGER)&totalBytes, (PULARGE_INTEGER)&totalFreeBytes);
		else
			success = spGetDiskFreeSpaceExW(directoryName.c_str(), (PULARGE_INTEGER)&freeClient, (PULARGE_INTEGER)&totalBytes, (PULARGE_INTEGER)&totalFreeBytes);

		return success ? (sint64)freeClient : -1;
	} else {
		DWORD sectorsPerCluster, bytesPerSector, freeClusters, totalClusters;
		BOOL success;

		VDStringW rootPath(VDFileSplitRoot(path));

		if ((LONG)GetVersion() < 0)
			success = GetDiskFreeSpaceA(rootPath.empty() ? NULL : VDTextWToA(rootPath).c_str(), &sectorsPerCluster, &bytesPerSector, &freeClusters, &totalClusters);
		else
			success = GetDiskFreeSpaceW(rootPath.empty() ? NULL : rootPath.c_str(), &sectorsPerCluster, &bytesPerSector, &freeClusters, &totalClusters);

		return success ? (sint64)((uint64)(sectorsPerCluster * bytesPerSector) * freeClusters) : -1;
	}
}

VDStringW VDGetFullPath(const VDStringW& partialPath) {
	typedef BOOL (WINAPI *tpGetFullPathNameW)(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR *lpFilePart);

	static tpGetFullPathNameW spGetFullPathNameW = (tpGetFullPathNameW)GetProcAddress(GetModuleHandle("kernel32.dll"), "GetFullPathNameW");

	union {
		char		a[MAX_PATH];
		wchar_t		w[MAX_PATH];
	} tmpBuf;

	if (spGetFullPathNameW && !(GetVersion() & 0x80000000)) {
		LPWSTR p;

		tmpBuf.w[0] = 0;
		spGetFullPathNameW(partialPath.c_str(), MAX_PATH, tmpBuf.w, &p);

		VDStringW pathw(tmpBuf.w);
		return pathw;

	} else {
		LPSTR p;

		tmpBuf.a[0] = 0;
		GetFullPathNameA(VDTextWToA(partialPath).c_str(), MAX_PATH, tmpBuf.a, &p);

		VDStringW patha(VDTextAToW(tmpBuf.a));
		return patha;
	}
}

