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
#include <vector>
#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/system/fileasync.h>
#include <vd2/system/thread.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/VDString.h>
#include <vd2/system/VDRingBuffer.h>

namespace {
	bool IsWindowsNT() {
		static bool sbIsNT = (LONG)GetVersion()>=0;
		return sbIsNT;
	}
};

///////////////////////////////////////////////////////////////////////////
//
//	VDFileAsync - Windows 9x implementation
//
///////////////////////////////////////////////////////////////////////////

class VDFileAsync9x : public IVDFileAsync, protected VDThread {
public:
	VDFileAsync9x();
	~VDFileAsync9x();

	bool IsOpen() { return mhFileFast != INVALID_HANDLE_VALUE; }

	void Open(const wchar_t *pszFilename, uint32 count, uint32 bufferSize);
	void Close();
	void FastWrite(const void *pData, uint32 bytes);
	void FastWriteEnd();
	void Write(sint64 pos, const void *pData, uint32 bytes);
	bool Extend(sint64 pos);
	void Truncate(sint64 pos);
	void SafeTruncateAndClose(sint64 pos);
	sint64 GetSize();

protected:
	void Seek(sint64 pos);
	bool SeekNT(sint64 pos);
	void ThrowError();
	void ThreadRun();

	HANDLE		mhFileSlow;
	HANDLE		mhFileFast;
	uint32		mBlockSize;
	uint32		mBlockCount;
	uint32		mSectorSize;

	enum {
		kStateNormal,
		kStateFlush,
		kStateAbort
	};
	VDAtomicInt	mState;

	VDSignal	mReadOccurred;
	VDSignal	mWriteOccurred;

	VDRingBuffer<char, VDFileUnbufferAllocator<char> >	mBuffer;

	VDStringA	mFilename;
	VDAtomicPtr<MyError>	mpError;
};

///////////////////////////////////////////////////////////////////////////

VDFileAsync9x::VDFileAsync9x()
	: mhFileSlow(INVALID_HANDLE_VALUE)
	, mhFileFast(INVALID_HANDLE_VALUE)
	, mpError(NULL)
{
}

VDFileAsync9x::~VDFileAsync9x() {
	Close();
}

void VDFileAsync9x::Open(const wchar_t *pszFilename, uint32 count, uint32 bufferSize) {
	try {
		mFilename = VDTextWToA(pszFilename);

		mhFileSlow = CreateFile(mFilename.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (mhFileSlow != INVALID_HANDLE_VALUE)
			mhFileFast = CreateFile(mFilename.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, NULL);
		if (mhFileFast == INVALID_HANDLE_VALUE)
			throw MyWin32Error("Unable to open file \"%s\" for write: %%s", GetLastError(), mFilename.c_str());

		mSectorSize = 4096;		// guess for now... proper way would be GetVolumeMountPoint() followed by GetDiskFreeSpace().

		mBlockSize = bufferSize;
		mBlockCount = count;
		mBuffer.Init(count * bufferSize);

		mState = kStateNormal;
	} catch(const MyError&) {
		Close();
		throw;
	}

	ThreadStart();
}

void VDFileAsync9x::Close() {
	mState = kStateAbort;
	mWriteOccurred.signal();
	ThreadWait();

	if (mhFileSlow != INVALID_HANDLE_VALUE) {
		CloseHandle(mhFileSlow);
		mhFileSlow = INVALID_HANDLE_VALUE;
	}
	if (mhFileFast != INVALID_HANDLE_VALUE) {
		CloseHandle(mhFileFast);
		mhFileFast = INVALID_HANDLE_VALUE;
	}
}

void VDFileAsync9x::FastWrite(const void *pData, uint32 bytes) {
	if (mpError)
		ThrowError();

	while(bytes) {
		int actual;
		void *p = mBuffer.LockWrite(bytes, actual);

		if (!actual) {
			mReadOccurred.wait();
			continue;
		}

		if (pData) {
			memcpy(p, pData, actual);
			pData = (const char *)pData + actual;
		} else {
			memset(p, 0, actual);
		}
		mBuffer.UnlockWrite(actual);
		mWriteOccurred.signal();
		bytes -= actual;
	}
}

void VDFileAsync9x::FastWriteEnd() {
	FastWrite(NULL, mSectorSize - 1);

	mState = kStateFlush;
	mWriteOccurred.signal();
	ThreadWait();

	if (mpError)
		ThrowError();
}

void VDFileAsync9x::Write(sint64 pos, const void *p, uint32 bytes) {
	Seek(pos);

	DWORD dwActual;
	if (!WriteFile(mhFileSlow, p, bytes, &dwActual, NULL) || dwActual != bytes)
		throw MyWin32Error("Write error occurred on file \"%s\": %%s\n", GetLastError(), mFilename.c_str());
}

bool VDFileAsync9x::Extend(sint64 pos) {
	return SeekNT(pos) && SetEndOfFile(mhFileSlow);
}

void VDFileAsync9x::Truncate(sint64 pos) {
	Seek(pos);
	if (!SetEndOfFile(mhFileSlow))
		throw MyWin32Error("I/O error on file \"%s\": %%s", GetLastError(), mFilename.c_str());
}

void VDFileAsync9x::SafeTruncateAndClose(sint64 pos) {
	if (mhFileFast != INVALID_HANDLE_VALUE) {
		FastWrite(NULL, mSectorSize - 1);

		mState = kStateFlush;
		mWriteOccurred.signal();
		ThreadWait();

		Extend(pos);
		Close();
	}
}

sint64 VDFileAsync9x::GetSize() {
	DWORD dwSizeHigh;
	DWORD dwSizeLow = GetFileSize(mhFileSlow, &dwSizeHigh);

	if (dwSizeLow == (DWORD)-1 && GetLastError() != NO_ERROR)
		throw MyWin32Error("I/O error on file \"%s\": %%s", GetLastError(), mFilename.c_str());

	return dwSizeLow + ((sint64)dwSizeHigh << 32);
}

void VDFileAsync9x::Seek(sint64 pos) {
	if (!SeekNT(pos))
		throw MyWin32Error("I/O error on file \"%s\": %%s", GetLastError(), mFilename.c_str());
}

bool VDFileAsync9x::SeekNT(sint64 pos) {
	LONG posHi = (LONG)(pos >> 32);
	DWORD result = SetFilePointer(mhFileSlow, (LONG)pos, &posHi, FILE_BEGIN);

	if (result == INVALID_SET_FILE_POINTER) {
		DWORD dwError = GetLastError();

		if (dwError != NO_ERROR)
			return false;
	}

	return true;
}

void VDFileAsync9x::ThrowError() {
	MyError *e = mpError.xchg(NULL);

	if (e) {
		MyError tmp;
		tmp.TransferFrom(*e);
		delete e;
		throw tmp;
	}
}

void VDFileAsync9x::ThreadRun() {
	for(;;) {
		int state = mState;

		if (state == kStateAbort)
			break;

		int actual;
		const void *p = mBuffer.LockRead(mBlockSize, actual);

		if (actual < mBlockSize) {
			if (state == kStateNormal) {
				mWriteOccurred.wait();
				continue;
			}

			VDASSERT(state == kStateFlush);

			actual &= ~(mSectorSize-1);
			if (!actual)
				break;
		}

		DWORD dwActual;
		if (!WriteFile(mhFileFast, p, actual, &dwActual, NULL) || dwActual != actual) {
			DWORD dwError = GetLastError();
			MyError *e = new MyWin32Error("Write error occurred on file \"%s\": %%s\n", dwError, mFilename.c_str());

			delete mpError.xchg(e);
			break;
		}

		mBuffer.UnlockRead(actual);

		mReadOccurred.signal();
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	VDFileAsync - Windows NT implementation
//
///////////////////////////////////////////////////////////////////////////

struct VDFileAsyncNTBuffer : public OVERLAPPED {
	bool	mbActive;

	VDFileAsyncNTBuffer() : mbActive(false) { hEvent = CreateEvent(NULL, FALSE, FALSE, NULL); }
	~VDFileAsyncNTBuffer() { if (hEvent) CloseHandle(hEvent); }
};

class VDFileAsyncNT : public IVDFileAsync {
public:
	VDFileAsyncNT();
	~VDFileAsyncNT();

	bool IsOpen() { return mhFileFast != INVALID_HANDLE_VALUE; }

	void Open(const wchar_t *pszFilename, uint32 count, uint32 bufferSize);
	void Close();
	void FastWrite(const void *pData, uint32 bytes);
	void FastWriteEnd();
	void Write(sint64 pos, const void *pData, uint32 bytes);
	bool Extend(sint64 pos);
	void Truncate(sint64 pos);
	void SafeTruncateAndClose(sint64 pos);
	sint64 GetSize();

protected:
	void Seek(sint64 pos);
	bool SeekNT(sint64 pos);
	void Flush(bool bMayThrow);

	HANDLE		mhFileSlow;
	HANDLE		mhFileFast;
	uint32		mBlockSize;
	uint32		mBlockCount;
	uint32		mBufferSize;
	uint32		mSectorSize;

	uint32		mReadOffset;
	uint32		mWriteOffset;
	sint64		mFastPointer;

	vdautoarrayptr<VDFileAsyncNTBuffer>	mpBlocks;

	vdblock<char, VDFileUnbufferAllocator<char> >	mBuffer;

	VDStringA	mFilename;
};

VDFileAsyncNT::VDFileAsyncNT()
	: mhFileSlow(INVALID_HANDLE_VALUE)
	, mhFileFast(INVALID_HANDLE_VALUE)
	, mFastPointer(0)
{
}

VDFileAsyncNT::~VDFileAsyncNT() {
	Close();
}

void VDFileAsyncNT::Open(const wchar_t *pszFilename, uint32 count, uint32 bufferSize) {
	try {
		mFilename = VDTextWToA(pszFilename);

		mhFileSlow = CreateFileW(pszFilename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (mhFileSlow != INVALID_HANDLE_VALUE)
			mhFileFast = CreateFileW(pszFilename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
		if (mhFileFast == INVALID_HANDLE_VALUE)
			throw MyWin32Error("Unable to open file \"%s\" for write: %%s", GetLastError(), mFilename.c_str());

		mSectorSize = 4096;		// guess for now... proper way would be GetVolumeMountPoint() followed by GetDiskFreeSpace().

		mBlockSize = bufferSize;
		mBlockCount = count;
		mBufferSize = mBlockSize * mBlockCount;

		mReadOffset = mWriteOffset = 0;

		mpBlocks = new VDFileAsyncNTBuffer[count];
		mBuffer.resize(count * bufferSize);
	} catch(const MyError&) {
		Close();
		throw;
	}
}

void VDFileAsyncNT::Close() {
	if (mhFileSlow != INVALID_HANDLE_VALUE) {
		CloseHandle(mhFileSlow);
		mhFileSlow = INVALID_HANDLE_VALUE;
	}
	if (mhFileFast != INVALID_HANDLE_VALUE) {
		typedef BOOL (WINAPI *tpCancelIo)(HANDLE);
		static const tpCancelIo pCancelIo = (tpCancelIo)GetProcAddress(GetModuleHandle("kernel32"), "CancelIo");
		pCancelIo(mhFileFast);
		CloseHandle(mhFileFast);
		mhFileFast = INVALID_HANDLE_VALUE;
	}

	mpBlocks = NULL;
}

void VDFileAsyncNT::FastWrite(const void *p, uint32 bytes) {
	while(bytes) {
		uint32 offset = mWriteOffset % mBlockSize;

		// Are we attempting to open a block?
		if (!offset) {
			// Check that the block is idle.
			int blockno = mWriteOffset / mBlockSize;
			VDFileAsyncNTBuffer& buf = mpBlocks[blockno];

			if (buf.mbActive) {
				DWORD dwActual;
				if (!GetOverlappedResult(mhFileFast, &buf, &dwActual, TRUE))
					throw MyWin32Error("Write error occurred on file \"%s\": %%s", GetLastError(), mFilename.c_str());
				buf.mbActive = false;
			}
		}

		uint32 toCopy = mBlockSize - offset;
		if (toCopy > bytes)
			toCopy = bytes;
		bytes -= toCopy;

		if (p) {
			memcpy(&mBuffer[mWriteOffset], p, toCopy);
			p = (const char *)p + toCopy;
		} else
			memset(&mBuffer[mWriteOffset], 0, toCopy);

		// Issue a write if a block is complete.
		if (offset + toCopy >= mBlockSize) {
			int blockno = mWriteOffset / mBlockSize;
			VDFileAsyncNTBuffer& buf = mpBlocks[blockno];

			VDASSERT(!buf.mbActive);

			DWORD dwActual;

			buf.Offset = (DWORD)mFastPointer;
			buf.OffsetHigh = (DWORD)((uint64)mFastPointer >> 32);

			if (!WriteFile(mhFileFast, &mBuffer[mWriteOffset - offset], mBlockSize, &dwActual, &buf)) {
				if (GetLastError() != ERROR_IO_PENDING)
					throw MyWin32Error("Write error occurred on file \"%s\": %%s", GetLastError(), mFilename.c_str());
			}

			mFastPointer += mBlockSize;

			buf.mbActive = true;
		}
		mWriteOffset += toCopy;
		if (mWriteOffset >= mBufferSize)
			mWriteOffset = 0;
	}
}

void VDFileAsyncNT::FastWriteEnd() {
	Flush(true);
}

void VDFileAsyncNT::Write(sint64 pos, const void *p, uint32 bytes) {
	Seek(pos);

	DWORD dwActual;
	if (!WriteFile(mhFileSlow, p, bytes, &dwActual, NULL) || dwActual != bytes)
		throw MyWin32Error("Write error occurred on file \"%s\": %%s", GetLastError(), mFilename.c_str());
}

bool VDFileAsyncNT::Extend(sint64 pos) {
	return SeekNT(pos) && SetEndOfFile(mhFileSlow);
}

void VDFileAsyncNT::Truncate(sint64 pos) {
	Seek(pos);
	if (!SetEndOfFile(mhFileSlow))
		throw MyWin32Error("I/O error on file \"%s\": %%s", GetLastError(), mFilename.c_str());
}

void VDFileAsyncNT::SafeTruncateAndClose(sint64 pos) {
	if (mhFileFast != INVALID_HANDLE_VALUE) {
		Flush(false);
		Extend(pos);
		Close();
	}
}

sint64 VDFileAsyncNT::GetSize() {
	DWORD dwSizeHigh;
	DWORD dwSizeLow = GetFileSize(mhFileSlow, &dwSizeHigh);

	if (dwSizeLow == (DWORD)-1 && GetLastError() != NO_ERROR)
		throw MyWin32Error("I/O error on file \"%s\": %%s", GetLastError(), mFilename.c_str());

	return dwSizeLow + ((sint64)dwSizeHigh << 32);
}

void VDFileAsyncNT::Seek(sint64 pos) {
	if (!SeekNT(pos))
		throw MyWin32Error("I/O error on file \"%s\": %%s", GetLastError(), mFilename.c_str());
}

bool VDFileAsyncNT::SeekNT(sint64 pos) {
	LONG posHi = (LONG)(pos >> 32);
	DWORD result = SetFilePointer(mhFileSlow, (LONG)pos, &posHi, FILE_BEGIN);

	if (result == INVALID_SET_FILE_POINTER) {
		DWORD dwError = GetLastError();

		if (dwError != NO_ERROR)
			return false;
	}

	return true;
}

void VDFileAsyncNT::Flush(bool bMayThrow) {
	FastWrite(NULL, mSectorSize - 1);

	// check for partial final block
	uint32 bytes = mWriteOffset % mBlockSize;
	if (bytes) {
		int block = mWriteOffset / mBlockSize;

		VDFileAsyncNTBuffer& buf = mpBlocks[block];
		VDASSERT(!buf.mbActive);

		DWORD dwActual;
		uint32 toWrite = bytes & ~(mSectorSize-1);
		buf.Offset = (DWORD)mFastPointer;
		buf.OffsetHigh = (DWORD)((uint64)mFastPointer >> 32);
		if (!WriteFile(mhFileFast, &mBuffer[mWriteOffset - bytes], toWrite, &dwActual, &buf)) {
			if (GetLastError() != ERROR_IO_PENDING) {
				if (bMayThrow)
					throw MyWin32Error("Write error occurred on file \"%s\": %%s", GetLastError(), mFilename.c_str());
			}
		} else {
			buf.mbActive = true;
			mFastPointer += toWrite;
		}
	}

	for(int i=0; i<mBlockCount; ++i) {
		VDFileAsyncNTBuffer& buf = mpBlocks[i];

		if (buf.mbActive) {
			DWORD dwActual;
			buf.mbActive = false;
			if (!GetOverlappedResult(mhFileFast, &buf, &dwActual, TRUE)) {
				if (bMayThrow)
					throw MyWin32Error("Write error occurred on file \"%s\": %%s", GetLastError(), mFilename.c_str());
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////

IVDFileAsync *VDCreateFileAsync() {
	return IsWindowsNT() ? static_cast<IVDFileAsync *>(new VDFileAsyncNT) : static_cast<IVDFileAsync *>(new VDFileAsync9x);
}
