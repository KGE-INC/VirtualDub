//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#ifndef f_VD2_SYSTEM_FILE_H
#define f_VD2_SYSTEM_FILE_H

#include <limits.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/vdalloc.h>

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
	vdautoptr2<char>	mpFilename;

private:
	VDFile(const VDFile&);
	const VDFile& operator=(const VDFile& f);

public:
	VDFile() : mhFile(NULL) {}
	VDFile(const char *pszFileName, uint32 flags = nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting);
	VDFile(const wchar_t *pwszFileName, uint32 flags = nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting);
	VDFile(VDFileHandle h);
	~VDFile();

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

	// unbuffered I/O requires aligned buffers ("unbuffers")
	static void *AllocUnbuffer(size_t nBytes);
	static void FreeUnbuffer(void *p);

protected:
	bool	open_internal(const char *pszFilename, const wchar_t *pwszFilename, uint32 flags);
};

class IVDStream {
public:
	virtual sint64	Pos() = 0;
	virtual void	Read(void *buffer, sint32 bytes) = 0;
	virtual sint32	ReadData(void *buffer, sint32 bytes) = 0;
};

class IVDRandomAccessStream : public IVDStream {
public:
	virtual sint64	Length() = 0;
	virtual void	Seek(sint64 offset) = 0;
};

class VDFileStream : public VDFile, public IVDRandomAccessStream {
private:
	VDFileStream(const VDFile&);
	const VDFileStream& operator=(const VDFileStream& f);

public:
	VDFileStream() {}
	VDFileStream(const char *pszFileName, uint32 flags = nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting)
		: VDFile(pszFileName, flags) {}
	VDFileStream(const wchar_t *pwszFileName, uint32 flags = nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting)
		: VDFile(pwszFileName, flags) {}
	VDFileStream(VDFileHandle h) : VDFile(h) {}
	~VDFileStream();

	sint64	Pos();
	void	Read(void *buffer, sint32 bytes);
	sint32	ReadData(void *buffer, sint32 bytes);
	sint64	Length();
	void	Seek(sint64 offset);
};

class VDMemoryStream : public IVDRandomAccessStream {
public:
	VDMemoryStream(const void *pSrc, uint32 len);

	sint64	Pos();
	void	Read(void *buffer, sint32 bytes);
	sint32	ReadData(void *buffer, sint32 bytes);
	sint64	Length();
	void	Seek(sint64 offset);

protected:
	const char		*mpSrc;
	const uint32	mLength;
	uint32			mPos;
};

///////////////////////////////////////////////////////////////////////////

template<class T>
class VDFileUnbufferAllocator {
public:
	typedef	size_t		size_type;
	typedef	ptrdiff_t	difference_type;
	typedef	T*			pointer;
	typedef	const T*	const_pointer;
	typedef	T&			reference;
	typedef	const T&	const_reference;
	typedef	T			value_type;

	template<class U> struct rebind { typedef VDFileUnbufferAllocator<U> other; };

	pointer			address(reference x) const			{ return &x; }
	const_pointer	address(const_reference x) const	{ return &x; }

	pointer			allocate(size_type n, void *p = 0)	{ return (pointer)VDFile::AllocUnbuffer(n * sizeof(T)); }
	void			deallocate(pointer p, size_type n)	{ VDFile::FreeUnbuffer(p); }
	size_type		max_size() const throw()			{ return MAX_INT; }

	void			construct(pointer p, const T& val)	{ new((void *)p) T(val); }
	void			destroy(pointer p)					{ ((T*)p)->~T(); }

#if defined(_MSC_VER) && _MSC_VER < 1300
	char *			_Charalloc(size_type n)				{ return (char *)allocate(n); }
#endif
};

#endif
