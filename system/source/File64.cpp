//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2000 Avery Lee
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

#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/VDString.h>
#include <vd2/system/File64.h>

// hack...

//extern CRITICAL_SECTION g_diskcs;

namespace {
	bool IsWindowsNT() {
		return (LONG)GetVersion()>=0;
	}
};

///////////////////////////////////////////////////////////////////////////////
//
//	VDFile
//
///////////////////////////////////////////////////////////////////////////////

using namespace nsVDFile;

VDFile::VDFile(const char *pszFileName, uint32 flags)
: mhFile(NULL)
{
	if (!open_internal(pszFileName, NULL, flags)) {
		const DWORD err = GetLastError();
		throw MyWin32Error("Cannot open file \"%s\":\n%%s", err, VDFileSplitPathRight(VDString(pszFileName)).c_str());
	}
}

VDFile::VDFile(const wchar_t *pwszFileName, uint32 flags)
: mhFile(NULL)
{
	if (!open_internal(NULL, pwszFileName, flags)) {
		const DWORD err = GetLastError();
		throw MyWin32Error("Cannot open file \"%s\":\n%%s", err, VDFileSplitPathRight(VDTextWToA(VDStringW(pwszFileName))).c_str());
	}
}

VDFile::VDFile(HANDLE h)
	: mhFile(h)
{
	LONG lo, hi = 0;

	lo = SetFilePointer(h, 0, &hi, FILE_CURRENT);

	mFilePosition = (uint32)lo + ((uint64)(uint32)hi << 32);
}

VDFile::~VDFile() {
	closeNT();
}

bool VDFile::open(const char *pszFilename, uint32 flags) {
	return open_internal(pszFilename, NULL, flags);
}

bool VDFile::open(const wchar_t *pwszFilename, uint32 flags) {
	return open_internal(NULL, pwszFilename, flags);
}

bool VDFile::open_internal(const char *pszFilename, const wchar_t *pwszFilename, uint32 flags) {
	close();

	mpFilename = strdup(VDFileSplitPath(pszFilename?pszFilename:VDTextWToA(VDStringW(pwszFilename)).c_str()));
	if (!mpFilename)
		throw MyMemoryError();

	// At least one of the read/write flags must be set.
	VDASSERT(flags & (kRead | kWrite));

	DWORD dwDesiredAccess = 0;

	if (flags & kRead)  dwDesiredAccess  = GENERIC_READ;
	if (flags & kWrite) dwDesiredAccess |= GENERIC_WRITE;

	// Win32 docs are screwed here -- FILE_SHARE_xxx is the inverse of a deny flag.

	DWORD dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
	if (flags & kDenyRead)	dwShareMode = FILE_SHARE_WRITE;
	if (flags & kDenyWrite) dwShareMode &= ~FILE_SHARE_WRITE;

	// One of the creation flags must be set.
	VDASSERT(flags & kCreationMask);

	DWORD dwCreationDisposition;

	uint32 creationType = flags & kCreationMask;

	switch(creationType) {
	case kOpenExisting:		dwCreationDisposition = OPEN_EXISTING; break;
	case kOpenAlways:		dwCreationDisposition = OPEN_ALWAYS; break;
	case kCreateAlways:		dwCreationDisposition = CREATE_ALWAYS; break;
	case kCreateNew:		dwCreationDisposition = CREATE_NEW; break;
	case kTruncateExisting:	dwCreationDisposition = TRUNCATE_EXISTING; break;
	default:
		VDNEVERHERE;
		return false;
	}

	VDASSERT((flags & (kSequential | kRandomAccess)) != (kSequential | kRandomAccess));

	DWORD dwAttributes = FILE_ATTRIBUTE_NORMAL;

	if (flags & kSequential)	dwAttributes |= FILE_FLAG_SEQUENTIAL_SCAN;
	if (flags & kRandomAccess)	dwAttributes |= FILE_FLAG_RANDOM_ACCESS;
	if (flags & kWriteThrough)	dwAttributes |= FILE_FLAG_WRITE_THROUGH;
	if (flags & kUnbuffered)	dwAttributes |= FILE_FLAG_NO_BUFFERING;

	if (pwszFilename && !IsWindowsNT())
		pszFilename = VDFastTextWToA(pwszFilename);

	if (pszFilename)
		mhFile = CreateFileA(pszFilename, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, dwAttributes, NULL);
	else
		mhFile = CreateFileW(pwszFilename, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, dwAttributes, NULL);

	DWORD err = GetLastError();

	VDFastTextFree();

	// INVALID_HANDLE_VALUE isn't NULL.  *sigh*

	if (mhFile == INVALID_HANDLE_VALUE) {
		mhFile = NULL;

		if (err == ERROR_FILE_NOT_FOUND)
			return false;

		if (err == ERROR_PATH_NOT_FOUND && creationType == kOpenExisting)
			return false;

		throw MyWin32Error("Cannot open file \"%s\":\n%%s", err, mpFilename.get());
	}

	mFilePosition = 0;

	return true;
}

bool VDFile::closeNT() {
	if (mhFile) {
		HANDLE h = mhFile;
		mhFile = NULL;
		if (!CloseHandle(h))
			return false;
	}
	return true;
}

void VDFile::close() {
	if (!closeNT())
		throw MyWin32Error("Cannot complete file \"%s\": %%s", GetLastError(), mpFilename.get());
}

bool VDFile::truncateNT() {
	return 0 != SetEndOfFile(mhFile);
}

void VDFile::truncate() {
	if (!truncateNT())
		throw MyWin32Error("Cannot truncate file \"%s\": %%s", GetLastError(), mpFilename.get());
}

long VDFile::readData(void *buffer, long length) {
	DWORD dwActual;

	if (!ReadFile(mhFile, buffer, (DWORD)length, &dwActual, NULL))
		throw MyWin32Error("Cannot read from file \"%s\": %%s", GetLastError(), mpFilename.get());

	mFilePosition += dwActual;

	return dwActual;
}

void VDFile::read(void *buffer, long length) {
	if (length != readData(buffer, length))
		throw MyWin32Error("Cannot read from file \"%s\": %%s", GetLastError(), mpFilename.get());
}

long VDFile::writeData(const void *buffer, long length) {
	DWORD dwActual;

	if (!WriteFile(mhFile, buffer, (DWORD)length, &dwActual, NULL) || dwActual != (DWORD)length)
		throw MyWin32Error("Cannot write to file \"%s\": %%s", GetLastError(), mpFilename.get());

	mFilePosition += dwActual;

	return dwActual;
}

void VDFile::write(const void *buffer, long length) {
	if (length != writeData(buffer, length))
		throw MyError("Cannot read from file \"%s\": %%s", GetLastError(), mpFilename.get());
}

bool VDFile::seekNT(sint64 newPos, eSeekMode mode) {
	DWORD dwMode;

	switch(mode) {
	case kSeekStart:
		dwMode = FILE_BEGIN;
		break;
	case kSeekCur:
		dwMode = FILE_CURRENT;
		break;
	case kSeekEnd:
		dwMode = FILE_END;
		break;
	default:
		VDNEVERHERE;
		return false;
	}

	union {
		sint64 pos;
		LONG l[2];
	} u = { newPos };

	u.l[0] = SetFilePointer(mhFile, u.l[0], &u.l[1], dwMode);

	if (u.l[0] == -1 && GetLastError() != NO_ERROR)
		return false;

	mFilePosition = u.pos;
	return true;
}

void VDFile::seek(sint64 newPos, eSeekMode mode) {
	if (!seekNT(newPos, mode))
		throw MyWin32Error("Cannot seek within file \"%s\": %%s", GetLastError(), mpFilename.get());
}

bool VDFile::skipNT(sint64 delta) {
	if (!delta)
		return true;

	char buf[1024];

	if (delta <= sizeof buf) {
		return (long)delta == readData(buf, (long)delta);
	} else
		return seekNT(delta, kSeekCur);
}

void VDFile::skip(sint64 delta) {
	if (!delta)
		return;

	char buf[1024];

	if (delta <= sizeof buf) {
		if ((long)delta != readData(buf, (long)delta))
			throw MyWin32Error("Cannot seek within file \"%s\": %%s", GetLastError(), mpFilename.get());
	} else
		seek(delta, kSeekCur);
}

sint64 VDFile::size() {
	union {
		uint64 siz;
		DWORD l[2];
	} u;

	u.l[0] = GetFileSize(mhFile, &u.l[1]);

	DWORD err;

	if (u.l[0] == (DWORD)-1L && (err = GetLastError()) != NO_ERROR)
		throw MyWin32Error("Cannot retrieve size of file \"%s\": %%s", GetLastError(), mpFilename.get());

	return (sint64)u.siz;
}

sint64 VDFile::tell() {
	return mFilePosition;
}

bool VDFile::isOpen() {
	return mhFile != 0;
}

VDFileHandle VDFile::getRawHandle() {
	return mhFile;
}

///////////////////////////////////////////////////////////////////////////////

VDFileStream::~VDFileStream() {
}

sint64 VDFileStream::Pos() {
	return tell();
}

void VDFileStream::Read(void *buffer, sint32 bytes) {
	read(buffer, bytes);
}

sint32 VDFileStream::ReadData(void *buffer, sint32 bytes) {
	return readData(buffer, bytes);
}

sint64 VDFileStream::Length() {
	return size();
}

void VDFileStream::Seek(sint64 offset) {
	seek(offset);
}

///////////////////////////////////////////////////////////////////////////////

VDMemoryStream::VDMemoryStream(const void *pSrc, uint32 len) 
	: mpSrc((const char *)pSrc)
	, mPos(0)
	, mLength(len)
{
}

sint64 VDMemoryStream::Pos() {
	return mPos;
}

void VDMemoryStream::Read(void *buffer, sint32 bytes) {
	if (bytes != ReadData(buffer, bytes))
		throw MyError("Attempt to read beyond stream.");
}

sint32 VDMemoryStream::ReadData(void *buffer, sint32 bytes) {
	if (bytes <= 0)
		return 0;

	if (bytes + mPos > mLength)
		bytes = mLength - mPos;

	if (bytes > 0) {
		memcpy(buffer, mpSrc+mPos, bytes);
		mPos += bytes;
	}

	return bytes;
}

sint64 VDMemoryStream::Length() {
	return mLength;
}

void VDMemoryStream::Seek(sint64 offset) {
	if (offset < 0 || offset > mLength)
		throw MyError("Invalid seek position");

	mPos = offset;
}

///////////////////////////////////////////////////////////////////////////////

File64::File64() : hFile(0), hFileUnbuffered(0) {
}

File64::File64(HANDLE _hFile, HANDLE _hFileUnbuffered)
: hFile(_hFile), hFileUnbuffered(_hFileUnbuffered)
{
	i64FilePosition = 0;
}

void File64::_openFile(const char *pszFile) {
	_closeFile();

	hFile = CreateFile(pszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (INVALID_HANDLE_VALUE == hFile) {
		hFile = NULL;
		throw MyWin32Error("Cannot open file: %%s.", GetLastError());
	}
}

void File64::_closeFile() {
	if (hFile) {
		CloseHandle(hFile);
		hFile = NULL;
	}

	if (hFileUnbuffered) {
		CloseHandle(hFileUnbuffered);
		hFileUnbuffered = NULL;
	}
}

long File64::_readFile(void *data, long len) {
	DWORD dwActual;

	if (!ReadFile(hFile, data, len, &dwActual, NULL))
		return -1;

	i64FilePosition += dwActual;

	return (long)dwActual;
}

void File64::_readFile2(void *data, long len) {
	long lActual = _readFile(data, len);

	if (lActual < 0)
		throw MyWin32Error("Failure reading file: %%s.",GetLastError());

	if (lActual != len)
		throw MyError("Failure reading file: Unexpected end of file");
}

bool File64::_readChunkHeader(FOURCC& pfcc, DWORD& pdwLen) {
	DWORD dw[2];
	long actual;

	actual = _readFile(dw, 8);

	if (actual != 8)
		return false;

	pfcc = dw[0];
	pdwLen = dw[1];

	return true;
}

void File64::_seekFile(__int64 i64NewPos) {
	LONG lHi = (LONG)(i64NewPos>>32);
	DWORD dwError;

	if (0xFFFFFFFF == SetFilePointer(hFile, (LONG)i64NewPos, &lHi, FILE_BEGIN))
		if ((dwError = GetLastError()) != NO_ERROR)
			throw MyWin32Error("File64: %%s", dwError);

	i64FilePosition = i64NewPos;
}

bool File64::_seekFile2(__int64 i64NewPos) {
	LONG lHi = (LONG)(i64NewPos>>32);
	DWORD dwError;

//	_RPT1(0,"Seeking to %I64d\n", i64NewPos);

	if (0xFFFFFFFF == SetFilePointer(hFile, (LONG)i64NewPos, &lHi, FILE_BEGIN))
		if ((dwError = GetLastError()) != NO_ERROR)
			return false;

	i64FilePosition = i64NewPos;

	return true;
}

void File64::_skipFile(__int64 bytes) {
	LONG lHi = (LONG)(bytes>>32);
	DWORD dwError;
	LONG lNewLow;

	if (0xFFFFFFFF == (lNewLow = SetFilePointer(hFile, (LONG)bytes, &lHi, FILE_CURRENT)))
		if ((dwError = GetLastError()) != NO_ERROR)
			throw MyWin32Error("File64: %%s", dwError);

	i64FilePosition = (unsigned long)lNewLow | (((__int64)(unsigned long)lHi)<<32);
}

bool File64::_skipFile2(__int64 bytes) {
	LONG lHi = (LONG)(bytes>>32);
	DWORD dwError;
	LONG lNewLow;

	if (0xFFFFFFFF == (lNewLow = SetFilePointer(hFile, (LONG)bytes, &lHi, FILE_CURRENT)))
		if ((dwError = GetLastError()) != NO_ERROR)
			return false;

	i64FilePosition = (unsigned long)lNewLow | (((__int64)(unsigned long)lHi)<<32);

	return true;
}

long File64::_readFileUnbuffered(void *data, long len) {
	DWORD dwActual;

//	EnterCriticalSection(&g_diskcs);
	if (!ReadFile(hFileUnbuffered, data, len, &dwActual, NULL)) {
//		LeaveCriticalSection(&g_diskcs);
		return -1;
	}
//	LeaveCriticalSection(&g_diskcs);

	return (long)dwActual;
}

void File64::_seekFileUnbuffered(__int64 i64NewPos) {
	LONG lHi = (LONG)(i64NewPos>>32);
	DWORD dwError;

	if (0xFFFFFFFF == SetFilePointer(hFileUnbuffered, (LONG)i64NewPos, &lHi, FILE_BEGIN))
		if ((dwError = GetLastError()) != NO_ERROR)
			throw MyWin32Error("File64: %%s", dwError);
}

__int64 File64::_posFile() {
	return i64FilePosition;
}

__int64 File64::_sizeFile() {
	DWORD dwLow, dwHigh;
	DWORD dwError;

	dwLow = GetFileSize(hFile, &dwHigh);

	if (dwLow == 0xFFFFFFFF && (dwError = GetLastError()) != NO_ERROR)
		throw MyWin32Error("Cannot determine file size: %%s", dwError);

	return ((__int64)dwHigh << 32) | (unsigned long)dwLow;
}
