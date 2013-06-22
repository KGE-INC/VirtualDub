//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#include <ctype.h>
#include <string.h>

#include <vd2/system/VDString.h>
#include <vd2/system/filesys.h>
#include <vd2/system/Error.h>

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

const char *VDFileSplitFirstDir(const char *s) {
	const char *start = s;

	while(*s++)
		if (s[-1] == ':' || s[-1] == '\\' || s[-1] == '/')
			return s;

	return start;
}

const wchar_t *VDFileSplitFirstDir(const wchar_t *s) {
	const wchar_t *start = s;

	while(*s++)
		if (s[-1] == L':' || s[-1] == L'\\' || s[-1] == L'/')
			return s;

	return start;
}

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

	if (!sbChecked) {
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

		return success ? (sint64)((uint64)sectorsPerCluster * bytesPerSector * freeClusters) : -1;
	}
}

bool VDDoesPathExist(const VDStringW& fileName) {
	bool bExists;

	if (!(GetVersion() & 0x80000000)) {
		bExists = ((DWORD)-1 != GetFileAttributesW(fileName.c_str()));
	} else {
		bExists = ((DWORD)-1 != GetFileAttributesA(VDFastTextWToA(fileName.c_str())));
		VDFastTextFree();
	}

	return bExists;
}

void VDCreateDirectory(const VDStringW& path) {
	// can't create dir with trailing slash
	VDStringW::size_type l(path.size());

	if (l) {
		const wchar_t c = path[l-1];

		if (c == L'/' || c == L'\\') {
			VDCreateDirectory(VDStringW(path.c_str(), l-1));
			return;
		}
	}

	BOOL succeeded;

	if (!(GetVersion() & 0x80000000)) {
		succeeded = CreateDirectoryW(path.c_str(), NULL);
	} else {
		succeeded = CreateDirectoryA(VDFastTextWToA(path.c_str()), NULL);
		VDFastTextFree();
	}

	if (!succeeded)
		throw MyWin32Error("Cannot create directory: %%s", GetLastError());
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

VDStringW VDMakePath(const VDStringW& base, const VDStringW& file) {
	if (base.empty())
		return file;

	VDStringW result(base);

	const wchar_t c = base[base.size() - 1];

	if (c != L'/' && c != L'\\' && c != L'/')
		result += L'\\';

	result.append(file);

	return result;
}
 
///////////////////////////////////////////////////////////////////////////

VDDirectoryIterator::VDDirectoryIterator(const wchar_t *path)
	: mSearchPath(path)
	, mpHandle(NULL)
	, mbSearchComplete(false)
{
	mBasePath = VDFileSplitPathLeft(mSearchPath) + L"\\";
}

VDDirectoryIterator::~VDDirectoryIterator() {
	if (mpHandle)
		FindClose((HANDLE)mpHandle);
}

bool VDDirectoryIterator::Next() {
	if (mbSearchComplete)
		return false;

	union {
		WIN32_FIND_DATAA a;
		WIN32_FIND_DATAW w;
	} wfd;

	if (GetVersion() & 0x80000000) {
		if (mpHandle)
			mbSearchComplete = !FindNextFileA((HANDLE)mpHandle, &wfd.a);
		else {
			mpHandle = FindFirstFileA(VDTextWToA(mSearchPath).c_str(), &wfd.a);
			mbSearchComplete = (INVALID_HANDLE_VALUE == mpHandle);
		}
		if (mbSearchComplete)
			return false;

		mbDirectory = (wfd.a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		mFilename = VDTextAToW(wfd.a.cFileName);
		mFileSize = wfd.a.nFileSizeLow + ((sint64)wfd.w.nFileSizeHigh << 32);
	} else {
		if (mpHandle)
			mbSearchComplete = !FindNextFileW((HANDLE)mpHandle, &wfd.w);
		else {
			mpHandle = FindFirstFileW(mSearchPath.c_str(), &wfd.w);
			mbSearchComplete = (INVALID_HANDLE_VALUE == mpHandle);
		}
		if (mbSearchComplete)
			return false;

		mbDirectory = (wfd.w.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		mFilename = wfd.w.cFileName;
		mFileSize = wfd.w.nFileSizeLow + ((sint64)wfd.w.nFileSizeHigh << 32);
	}

	return true;
}
