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
static inline T splitimpL(const T& string, const U *s) {
	const U *p = string.c_str();
	return T(p, s - p);
}

template<class T, class U>
static inline T splitimpR(const T& string, const U *s) {
	const U *p = string.c_str();
	return T(s);
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

VDString  VDFileSplitPathLeft (const VDString&  s) { return splitimpL(s, VDFileSplitPath(s.c_str())); }
VDStringW VDFileSplitPathLeft (const VDStringW& s) { return splitimpL(s, VDFileSplitPath(s.c_str())); }
VDString  VDFileSplitPathRight(const VDString&  s) { return splitimpR(s, VDFileSplitPath(s.c_str())); }
VDStringW VDFileSplitPathRight(const VDStringW& s) { return splitimpR(s, VDFileSplitPath(s.c_str())); }

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

VDString  VDFileSplitRoot(const VDString&  s) { return splitimpL(s, VDFileSplitRoot(s.c_str())); }
VDStringW VDFileSplitRoot(const VDStringW& s) { return splitimpL(s, VDFileSplitRoot(s.c_str())); }

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

VDString  VDFileSplitExtLeft (const VDString&  s) { return splitimpL(s, VDFileSplitExt(s.c_str())); }
VDStringW VDFileSplitExtLeft (const VDStringW& s) { return splitimpL(s, VDFileSplitExt(s.c_str())); }
VDString  VDFileSplitExtRight(const VDString&  s) { return splitimpR(s, VDFileSplitExt(s.c_str())); }
VDStringW VDFileSplitExtRight(const VDStringW& s) { return splitimpR(s, VDFileSplitExt(s.c_str())); }

/////////////////////////////////////////////////////////////////////////////

#include <windows.h>

sint64 VDGetDiskFreeSpace(const wchar_t *path) {
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

		if (!directoryName.empty()) {
			wchar_t c = directoryName[directoryName.length()-1];

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

bool VDDoesPathExist(const wchar_t *fileName) {
	bool bExists;

	if (!(GetVersion() & 0x80000000)) {
		bExists = ((DWORD)-1 != GetFileAttributesW(fileName));
	} else {
		bExists = ((DWORD)-1 != GetFileAttributesA(VDFastTextWToA(fileName)));
		VDFastTextFree();
	}

	return bExists;
}

void VDCreateDirectory(const wchar_t *path) {
	// can't create dir with trailing slash
	VDStringW::size_type l(wcslen(path));

	if (l) {
		const wchar_t c = path[l-1];

		if (c == L'/' || c == L'\\') {
			VDCreateDirectory(VDStringW(path, l-1).c_str());
			return;
		}
	}

	BOOL succeeded;

	if (!(GetVersion() & 0x80000000)) {
		succeeded = CreateDirectoryW(path, NULL);
	} else {
		succeeded = CreateDirectoryA(VDFastTextWToA(path), NULL);
		VDFastTextFree();
	}

	if (!succeeded)
		throw MyWin32Error("Cannot create directory: %%s", GetLastError());
}

VDStringW VDGetFullPath(const wchar_t *partialPath) {
	typedef BOOL (WINAPI *tpGetFullPathNameW)(LPCWSTR lpFileName, DWORD nBufferLength, LPWSTR lpBuffer, LPWSTR *lpFilePart);

	static tpGetFullPathNameW spGetFullPathNameW = (tpGetFullPathNameW)GetProcAddress(GetModuleHandle("kernel32.dll"), "GetFullPathNameW");

	union {
		char		a[MAX_PATH];
		wchar_t		w[MAX_PATH];
	} tmpBuf;

	if (spGetFullPathNameW && !(GetVersion() & 0x80000000)) {
		LPWSTR p;

		tmpBuf.w[0] = 0;
		spGetFullPathNameW(partialPath, MAX_PATH, tmpBuf.w, &p);

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

VDStringW VDMakePath(const wchar_t *base, const wchar_t *file) {
	if (!*base)
		return VDStringW(file);

	VDStringW result(base);

	const wchar_t c = result[result.size() - 1];

	if (c != L'/' && c != L'\\' && c != L':')
		result += L'\\';

	result.append(file);

	return result;
}

void VDFileFixDirPath(VDStringW& path) {
	if (!path.empty()) {
		wchar_t c = path[path.size()-1];

		if (c != L'/' && c != L'\\' && c != L':')
			path += L'\\';
	}
}

///////////////////////////////////////////////////////////////////////////

VDDirectoryIterator::VDDirectoryIterator(const wchar_t *path)
	: mSearchPath(path)
	, mpHandle(NULL)
	, mbSearchComplete(false)
{
	mBasePath = VDFileSplitPathLeft(mSearchPath);
	VDFileFixDirPath(mBasePath);
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

///////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG

struct VDSystemFilesysTestObject {
	VDSystemFilesysTestObject() {
#define TEST(fn, x, y1, y2) VDASSERT(!strcmp(fn(x), y2)); VDASSERT(!wcscmp(fn(L##x), L##y2)); VDASSERT(fn##Left(VDStringA(x))==y1); VDASSERT(fn##Right(VDStringA(x))==y2); VDASSERT(fn##Left(VDStringW(L##x))==L##y1); VDASSERT(fn##Right(VDStringW(L##x))==L##y2)
		TEST(VDFileSplitPath, "", "", "");
		TEST(VDFileSplitPath, "x", "", "x");
		TEST(VDFileSplitPath, "x\\y", "x\\", "y");
		TEST(VDFileSplitPath, "x\\y\\z", "x\\y\\", "z");
		TEST(VDFileSplitPath, "x\\", "x\\", "");
		TEST(VDFileSplitPath, "x\\y\\z\\", "x\\y\\z\\", "");
		TEST(VDFileSplitPath, "c:", "c:", "");
		TEST(VDFileSplitPath, "c:x", "c:", "x");
		TEST(VDFileSplitPath, "c:\\", "c:\\", "");
		TEST(VDFileSplitPath, "c:\\x", "c:\\", "x");
		TEST(VDFileSplitPath, "c:\\x\\", "c:\\x\\", "");
		TEST(VDFileSplitPath, "c:\\x\\", "c:\\x\\", "");
		TEST(VDFileSplitPath, "c:x\\y", "c:x\\", "y");
		TEST(VDFileSplitPath, "\\\\server\\share\\", "\\\\server\\share\\", "");
		TEST(VDFileSplitPath, "\\\\server\\share\\x", "\\\\server\\share\\", "x");
#undef TEST
#define TEST(fn, x, y1, y2) VDASSERT(!strcmp(fn(x), y2)); VDASSERT(!wcscmp(fn(L##x), L##y2)); VDASSERT(fn(VDStringA(x))==y1); VDASSERT(fn(VDStringW(L##x))==L##y1)
		TEST(VDFileSplitRoot, "", "", "");
		TEST(VDFileSplitRoot, "c:", "c:", "");
		TEST(VDFileSplitRoot, "c:x", "c:", "x");
		TEST(VDFileSplitRoot, "c:x\\", "c:", "x\\");
		TEST(VDFileSplitRoot, "c:x\\y", "c:", "x\\y");
		TEST(VDFileSplitRoot, "c:\\", "c:\\", "");
		TEST(VDFileSplitRoot, "c:\\x", "c:\\", "x");
		TEST(VDFileSplitRoot, "c:\\x\\", "c:\\", "x\\");
		TEST(VDFileSplitRoot, "\\", "\\", "");
		TEST(VDFileSplitRoot, "\\x", "\\", "x");
		TEST(VDFileSplitRoot, "\\x\\", "\\", "x\\");
		TEST(VDFileSplitRoot, "\\x\\y", "\\", "x\\y");
#undef TEST
	}
} g_VDSystemFilesysTestObject;

#endif
