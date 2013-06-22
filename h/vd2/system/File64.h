#ifndef f_FILE64_H
#define f_FILE64_H

// DEPRECATED -- please switch to <vd2/system/file.h>

#include <vd2/system/file.h>

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
