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

#ifndef f_FILE64_H
#define f_FILE64_H

#include <vd2/system/vdtypes.h>

#ifdef WIN32
	typedef void *VDFileHandle;				// this needs to match wtypes.h definition for HANDLE
#else
	#error No operating system target declared??
#endif

namespace nsVDFile {
	enum eSeekMode {
		kSeekStart=0, kSeekCur, kSeekEnd
	};

	enum eFlags {
		kRead			= 0x00000001,
		kWrite			= 0x00000002,
		kReadWrite		= kRead | kWrite,

		kDenyNone		= 0x00000000,
		kDenyRead		= 0x00000010,
		kDenyWrite		= 0x00000020,
		kDenyAll		= kDenyRead | kDenyWrite,

		kOpenExisting		= 0x00000100,
		kOpenAlways			= 0x00000200,
		kCreateAlways		= 0x00000300,
		kCreateNew			= 0x00000400,
		kTruncateExisting	= 0x00000500,		// not particularly useful, really
		kCreationMask		= 0x0000FF00,

		kSequential			= 0x00010000,
		kRandomAccess		= 0x00020000,
		kUnbuffered			= 0x00040000,		// much faster on Win32 thanks to the crappy cache, but possibly bad in Unix?
		kWriteThrough		= 0x00080000,

		kAllFileFlags		= 0xFFFFFFFF
	};
};

class VDFile {
protected:
	VDFileHandle	mhFile;
	sint64			mFilePosition;

private:
	VDFile(const VDFile&);
	const VDFile& operator=(const VDFile& f);

public:
	VDFile() : mhFile(NULL) {}
	VDFile(const char *pszFileName, uint32 flags = nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting);
	VDFile(const wchar_t *pwszFileName, uint32 flags = nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting);
	VDFile(VDFileHandle h);
	~VDFile() throw();

	// The "NT" functions are non-throwing and return success/failure; the regular functions throw exceptions
	// when something bad happens.

	bool	open(const char *pszFileName, uint32 flags = nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting);	// false if failed due to not found or already exists
	bool	open(const wchar_t *pwszFileName, uint32 flags = nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting);	// false if failed due to not found or already exists
	bool	closeNT();
	void	close();
	bool	truncateNT();
	void	truncate();

	sint64	size();
	void	read(void *buffer, long length);
	long	readData(void *buffer, long length);
	void	write(const void *buffer, long length);
	long	writeData(const void *buffer, long length);
	bool	seekNT(sint64 newPos, nsVDFile::eSeekMode mode = nsVDFile::kSeekStart);
	void	seek(sint64 newPos, nsVDFile::eSeekMode mode = nsVDFile::kSeekStart);
	bool	skipNT(sint64 delta);
	void	skip(sint64 delta);
	sint64	tell();

	bool	isOpen();
	VDFileHandle	getRawHandle();

protected:
	bool	open_internal(const char *pszFilename, const wchar_t *pwszFilename, uint32 flags);
};


class File64 {		// DEPRECATED
public:
	VDFileHandle hFile, hFileUnbuffered;
	__int64 i64FilePosition;

	File64();
	File64(VDFileHandle _hFile, VDFileHandle _hFileUnbuffered);

	void _openFile(const char *pszFile);
	void _closeFile();

	long _readFile(void *data, long len);
	void _readFile2(void *data, long len);
	bool _readChunkHeader(unsigned long& pfcc, unsigned long& pdwLen);
	void _seekFile(__int64 i64NewPos);
	bool _seekFile2(__int64 i64NewPos);
	void _skipFile(__int64 bytes);
	bool _skipFile2(__int64 bytes);
	long _readFileUnbuffered(void *data, long len);
	void _seekFileUnbuffered(__int64 i64NewPos);
	__int64 _posFile();
	__int64 _sizeFile();
};

#endif
