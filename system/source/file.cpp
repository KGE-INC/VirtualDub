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

#include <windows.h>

#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/VDString.h>
#include <vd2/system/file.h>

namespace {
	bool IsWindowsNT() {
		static bool sbIsNT = (LONG)GetVersion()>=0;
		return sbIsNT;
	}

	bool IsHardDrivePath(const wchar_t *path) {
		wchar_t tmppath1[MAX_PATH], tmppath2[MAX_PATH];
		wchar_t *fn;

		if (GetFullPathNameW(path, sizeof tmppath1 / sizeof tmppath1[0], tmppath1, &fn)) {
			typedef BOOL (__stdcall * tpGetVolumePathNameW)(LPCWSTR lpszFileName, LPWSTR lpszVolumePathName, DWORD cchBufferLength);
			static const tpGetVolumePathNameW pGetVolumePathNameW = (tpGetVolumePathNameW)GetProcAddress(GetModuleHandle("kernel32"), "GetVolumePathNameW");

			*fn = 0;

			bool success = false;

			if (pGetVolumePathNameW)		// requires Windows 2000 or above
				success = 0!=pGetVolumePathNameW(tmppath1, tmppath2, sizeof tmppath2 / sizeof tmppath2[0]);

			if (!success) {
				wcscpy(tmppath2, tmppath1);
				*VDFileSplitRoot(tmppath2) = NULL;
			}

			UINT type = GetDriveTypeW(tmppath2);

			return type == DRIVE_FIXED || type == DRIVE_UNKNOWN || type == DRIVE_REMOVABLE;
		}

		return true;
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

	if (IsWindowsNT()) {
		if (pszFilename) {
			pwszFilename = VDFastTextAToW(pszFilename);
			pszFilename = NULL;
		}
	} else {
		if (pwszFilename) {
			pszFilename = VDFastTextWToA(pwszFilename);
			pwszFilename = NULL;
		}
	}

	if (pszFilename)
		mhFile = CreateFileA(pszFilename, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, dwAttributes, NULL);
	else {
		if (!IsHardDrivePath(pwszFilename))
			flags &= ~FILE_FLAG_NO_BUFFERING;

		mhFile = CreateFileW(pwszFilename, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, dwAttributes, NULL);
	}

	DWORD err = GetLastError();

	// If we failed and FILE_FLAG_NO_BUFFERING was set, strip it and try again.
	// VPC and Novell shares sometimes do this....
	if (mhFile == INVALID_HANDLE_VALUE && err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND) {
		if (dwAttributes & FILE_FLAG_NO_BUFFERING) {
			dwAttributes &= ~FILE_FLAG_NO_BUFFERING;
			dwAttributes |= FILE_FLAG_WRITE_THROUGH;

			if (pszFilename)
				mhFile = CreateFileA(pszFilename, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, dwAttributes, NULL);
			else
				mhFile = CreateFileW(pwszFilename, dwDesiredAccess, dwShareMode, NULL, dwCreationDisposition, dwAttributes, NULL);

			err = GetLastError();
		}
	}

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

bool VDFile::extendValidNT(sint64 pos) {
	if (GetVersion() & 0x80000000)
		return true;				// No need, Windows 95/98/ME do this automatically anyway.

	// The SetFileValidData() API is only available on XP and Server 2003.

	typedef BOOL (APIENTRY *tpSetFileValidData)(HANDLE hFile, LONGLONG ValidDataLength);		// Windows XP, Server 2003
	static tpSetFileValidData pSetFileValidData = (tpSetFileValidData)GetProcAddress(GetModuleHandle("kernel32"), "SetFileValidData");

	if (!pSetFileValidData) {
		SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
		return false;
	}

	// SetFileValidData() requires the SE_MANAGE_VOLUME_NAME privilege, so we must temporarily
	// enable it on our thread token.
	bool bSuccessful = false;
	DWORD err;

	HANDLE h;
	if (OpenThreadToken(GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES|TOKEN_QUERY, FALSE, &h)) {
		LUID luid;

		if (LookupPrivilegeValue(NULL, "SeShutdownPrivilege", &luid)) {
			TOKEN_PRIVILEGES tp;
			tp.PrivilegeCount = 1;
			tp.Privileges[0].Luid = luid;
			tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

			if (AdjustTokenPrivileges(h, FALSE, &tp, 0, NULL, NULL)) {
				if (pSetFileValidData(mhFile, pos))
					bSuccessful = true;
				else
					err = GetLastError();

				tp.Privileges[0].Attributes = 0;
				AdjustTokenPrivileges(h, FALSE, &tp, 0, NULL, NULL);
			}
		}

		CloseHandle(h);
	}

	if (!bSuccessful)
		SetLastError(err);

	return bSuccessful;
}

void VDFile::extendValid(sint64 pos) {
	if (!extendValidNT(pos))
		throw MyWin32Error("Cannot extend file \"%s\": %%s", GetLastError(), mpFilename.get());
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
	bool success = false;

	if (!WriteFile(mhFile, buffer, (DWORD)length, &dwActual, NULL) || dwActual != (DWORD)length)
		goto found_error;

	mFilePosition += dwActual;

	return dwActual;

found_error:
	throw MyWin32Error("Cannot write to file \"%s\": %%s", GetLastError(), mpFilename.get());
}

void VDFile::write(const void *buffer, long length) {
	if (length != writeData(buffer, length))
		throw MyWin32Error("Cannot write to file \"%s\": %%s", GetLastError(), mpFilename.get());
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

void *VDFile::AllocUnbuffer(size_t nBytes) {
	return VirtualAlloc(NULL, nBytes, MEM_COMMIT, PAGE_READWRITE);
}

void VDFile::FreeUnbuffer(void *p) {
	VirtualFree(p, 0, MEM_RELEASE);
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

VDTextStream::VDTextStream(IVDStream *pSrc)
	: mpSrc(pSrc)
	, mBufferPos(0)
	, mBufferLimit(0)
	, mState(kFetchLine)
	, mFileBuffer(kFileBufferSize)
{
}

VDTextStream::~VDTextStream() {
}

const char *VDTextStream::GetNextLine() {
	if (!mpSrc)
		return NULL;

	mLineBuffer.clear();

	for(;;) {
		if (mBufferPos >= mBufferLimit) {
			mBufferPos = 0;
			mBufferLimit = mpSrc->ReadData(mFileBuffer.data(), mFileBuffer.size());

			if (!mBufferLimit) {
				mpSrc = NULL;

				if (mLineBuffer.empty())
					return NULL;

				mLineBuffer.push_back(0);

				return &mLineBuffer[0];
			}
		}

		switch(mState) {

			case kEatNextIfCR:
				mState = kFetchLine;
				if (mFileBuffer[mBufferPos] == '\r')
					++mBufferPos;
				continue;

			case kEatNextIfLF:
				mState = kFetchLine;
				if (mFileBuffer[mBufferPos] == '\n')
					++mBufferPos;
				continue;

			case kFetchLine:
				uint32 base = mBufferPos;

				do {
					const char c = mFileBuffer[mBufferPos++];

					if (c == '\r') {
						mState = kEatNextIfLF;
						mLineBuffer.insert(mLineBuffer.end(), mFileBuffer.begin() + base, mFileBuffer.begin() + (mBufferPos-1));
						mLineBuffer.push_back(0);
						return &mLineBuffer[0];
					}
					if (c == '\n') {
						mState = kEatNextIfCR;
						mLineBuffer.insert(mLineBuffer.end(), mFileBuffer.begin() + base, mFileBuffer.begin() + (mBufferPos-1));
						mLineBuffer.push_back(0);
						return &mLineBuffer[0];
					}
				} while(mBufferPos < mBufferLimit);
				mLineBuffer.insert(mLineBuffer.end(), mFileBuffer.begin() + base, mFileBuffer.end());
				break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

VDTextInputFile::VDTextInputFile(const wchar_t *filename, uint32 flags)
	: mFileStream(filename, flags | nsVDFile::kRead)
	, mTextStream(&mFileStream)
{
}

VDTextInputFile::~VDTextInputFile() {
}
