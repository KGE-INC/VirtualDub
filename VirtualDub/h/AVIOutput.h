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

#ifndef f_AVIOUTPUT_H
#define f_AVIOUTPUT_H

#include <windows.h>
#include <vfw.h>
#include <vector>
#include <vd2/system/file.h>

#include "Fixes.h"

class AVIIndex;
class AudioSource;
class VideoSource;
class FastWriteStream;
class AVIIndexEntry2;
typedef struct _avisuperindex_chunk AVISUPERINDEX;
struct _avisuperindex_entry;

class IVDMediaOutputStream {
public:
	virtual ~IVDMediaOutputStream() {}		// shouldn't be here but need to get rid of common delete in root destructor

	virtual void *	getFormat() = 0;
	virtual int		getFormatLen() = 0;
	virtual void	setFormat(const void *pFormat, int len) = 0;

	virtual const AVIStreamHeader_fixed& getStreamInfo() = 0;
	virtual void	setStreamInfo(const AVIStreamHeader_fixed& hdr) = 0;
	virtual void	updateStreamInfo(const AVIStreamHeader_fixed& hdr) = 0;

	virtual void	write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) = 0;
	virtual void	flush() = 0;
};

class AVIOutputStream : public IVDMediaOutputStream {
private:
	std::vector<char>	mFormat;

protected:
	class AVIOutput		*output;
	AVIStreamHeader_fixed		streamInfo;

public:
	AVIOutputStream(class AVIOutput *output);
	virtual ~AVIOutputStream();

	virtual void setFormat(const void *pFormat, int len) {
		mFormat.resize(len);
		memcpy(&mFormat[0], pFormat, len);
	}

	virtual void *getFormat() { return &mFormat[0]; }
	virtual int getFormatLen() { return mFormat.size(); }

	virtual const AVIStreamHeader_fixed& getStreamInfo() {
		return streamInfo;
	}

	virtual void setStreamInfo(const AVIStreamHeader_fixed& hdr) {
		streamInfo = hdr;
		streamInfo.dwLength = 0;
		streamInfo.dwSuggestedBufferSize = 0;
	}

	virtual void updateStreamInfo(const AVIStreamHeader_fixed& hdr) {
		streamInfo = hdr;
	}

	virtual void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) = 0;
	virtual void flush() {}
};

class AVIOutput {
protected:
	IVDMediaOutputStream	*audioOut;
	IVDMediaOutputStream	*videoOut;

public:
	AVIOutput();
	virtual ~AVIOutput();

	virtual bool init(const wchar_t *szFile)=0;
	virtual void finalize()=0;

	virtual IVDMediaOutputStream *createAudioStream() = 0;
	virtual IVDMediaOutputStream *createVideoStream() = 0;
	virtual IVDMediaOutputStream *getAudioOutput() { return audioOut; }
	virtual IVDMediaOutputStream *getVideoOutput() { return videoOut; }
};

class AVIOutputFile : public AVIOutput {
private:
	enum {
		kDefaultSuperIndexEntries		= 256,			// Maximum number of OpenDML first-level entries -- the AVI file can never grow past 4GB x this value.
		kDefaultSubIndexEntries			= 8192			// Maximum number of OpenDML second-level entries -- the AVI file can never contain more than the product of these values in sample chunks.
	};

	FastWriteStream *fastIO;
	VDFile		mFile;
	sint64		i64FilePosition;
	sint64		i64XBufferLevel;

	struct AVIBlock {
		sint64		riff_pos;		// position of 'RIFF' chunk
		sint64		movi_pos;		// position of 'movi' chunk
		uint32		riff_len;		// length of 'RIFF' chunk
		uint32		movi_len;		// length of 'movi' chunk
	};

	std::vector<AVIBlock>	mBlocks;
	int			xblock;

	long		strl_pos;
	long		misc_pos;
	long		main_hdr_pos;
	long		audio_hdr_pos;
	long		audio_format_pos;
	long		video_hdr_pos;
	long		audio_indx_pos;
	long		video_indx_pos;
	long		dmlh_pos;
	long		seghint_pos;

	int			chunkFlags;

	uint32		mSuperIndexLimit;
	uint32		mSubIndexLimit;

	AVIIndex	*index, *index_audio, *index_video;
	std::vector<char>	mHeaderBlock;

	MainAVIHeader		avihdr;

	sint32		mBufferSize;
	sint32		mChunkSize;
	long		lAVILimit;
	int			iPadOffset;

	bool		fCaching;
	bool		fExtendedAVI;
	bool		fCaptureMode;
	bool		fInitComplete;
	bool		mbInterleaved;

	char *		pSegmentHint;
	int			cbSegmentHint;

	sint64		i64EndOfFile;
	sint64		i64FarthestWritePoint;
	long		lLargestIndexDelta[2];
	sint64		i64FirstIndexedChunk[2];
	sint64		i64LastIndexedChunk[2];
	bool		fLimitTo4Gb;
	long		lIndexedChunkCount[2];
	long		lIndexSize;
	bool		fPreemptiveExtendFailed;

	bool		_init(const wchar_t *szFile, bool fThreaded);

	sint64		_writeHdr(const void *data, long len);
	sint64		_beginList(FOURCC ckid);
	sint64		_writeHdrChunk(FOURCC ckid, const void *data, long len);
	void		_closeList(sint64 pos);
	void		_flushHdr();
	sint64		_getPosition();
	void		_seekHdr(sint64 i64NewPos);
	bool		_extendFile(sint64 i64NewPoint);
	sint64		_writeDirect(const void *data, long len);

	void		_write(const void *data, int len);
	void		_closeXblock();
	void		_openXblock();
	void		_writeLegacyIndex(bool use_fastIO);

	void		_createNewIndices(AVIIndex *index, AVISUPERINDEX *asi, _avisuperindex_entry *asie, bool is_audio);
	void		_writeNewIndex(struct _avisuperindex_entry *asie, AVIIndexEntry2 *avie2, int size, FOURCC fcc, DWORD dwChunkId, DWORD dwSampleSize);
public:
	AVIOutputFile();
	virtual ~AVIOutputFile();

	void disable_os_caching();
	void disable_extended_avi();
	void set_1Gb_limit();
	void set_capture_mode(bool b);
	void setSegmentHintBlock(bool fIsFinal, const char *pszNextPath, int cbBlock);
	void setInterleaved(bool bInterleaved) { mbInterleaved = bInterleaved; }
	void setBuffering(sint32 nBufferSize, sint32 nChunkSize);
	void setIndexingLimits(sint32 nMaxSuperIndexEntries, sint32 nMaxSubIndexEntries) {
		mSuperIndexLimit = nMaxSuperIndexEntries;
		mSubIndexLimit = nMaxSubIndexEntries;
	}

	IVDMediaOutputStream *createAudioStream();
	IVDMediaOutputStream *createVideoStream();

	bool init(const wchar_t *szFile);
	FastWriteStream *initCapture(const wchar_t *szFile);

	void finalize();

	void writeIndexedChunk(FOURCC ckid, uint32 flags, const void *pBuffer, uint32 cbBuffer);

	LONG bufferStatus(LONG *lplBufferSize);
};

#endif
