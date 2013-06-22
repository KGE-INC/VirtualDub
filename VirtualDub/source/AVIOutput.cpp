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

#include "stdafx.h"

#include "AudioSource.h"
#include "VideoSource.h"
#include "AVIIndex.h"
#include "FastWriteStream.h"

#include <vd2/system/error.h>
#include "AVIOutput.h"
#include "oshelper.h"
#include "misc.h"

///////////////////////////////////////////

extern "C" unsigned long version_num;

///////////////////////////////////////////

typedef sint64 QUADWORD;

// The following comes from the OpenDML 1.0 spec for extended AVI files

// bIndexType codes
//
#define AVI_INDEX_OF_INDEXES 0x00	// when each entry in aIndex
									// array points to an index chunk

#define AVI_INDEX_OF_CHUNKS 0x01	// when each entry in aIndex
									// array points to a chunk in the file

#define AVI_INDEX_IS_DATA	0x80	// when each entry is aIndex is
									// really the data

// bIndexSubtype codes for INDEX_OF_CHUNKS

#define AVI_INDEX_2FIELD	0x01	// when fields within frames
									// are also indexed
	struct _avisuperindex_entry {
		QUADWORD qwOffset;		// absolute file offset, offset 0 is
								// unused entry??
		DWORD dwSize;			// size of index chunk at this offset
		DWORD dwDuration;		// time span in stream ticks
	};
	struct _avistdindex_entry {
		DWORD dwOffset;			// qwBaseOffset + this is absolute file offset
		DWORD dwSize;			// bit 31 is set if this is NOT a keyframe
	};

#pragma pack(push)
#pragma pack(2)
#pragma warning(disable: 4200)		// warning C4200: nonstandard extension used : zero-sized array in struct/union

typedef struct _avisuperindex_chunk {
	FOURCC fcc;					// ’ix##’
	DWORD cb;					// size of this structure
	WORD wLongsPerEntry;		// must be 4 (size of each entry in aIndex array)
	BYTE bIndexSubType;			// must be 0 or AVI_INDEX_2FIELD
	BYTE bIndexType;			// must be AVI_INDEX_OF_INDEXES
	DWORD nEntriesInUse;		// number of entries in aIndex array that
								// are used
	DWORD dwChunkId;			// ’##dc’ or ’##db’ or ’##wb’, etc
	DWORD dwReserved[3];		// must be 0
	struct _avisuperindex_entry aIndex[];
} AVISUPERINDEX, * PAVISUPERINDEX;

typedef struct _avistdindex_chunk {
	FOURCC fcc;					// ’ix##’
	DWORD cb;
	WORD wLongsPerEntry;		// must be sizeof(aIndex[0])/sizeof(DWORD)
	BYTE bIndexSubType;			// must be 0
	BYTE bIndexType;			// must be AVI_INDEX_OF_CHUNKS
	DWORD nEntriesInUse;		//
	DWORD dwChunkId;			// ’##dc’ or ’##db’ or ’##wb’ etc..
	QUADWORD qwBaseOffset;		// all dwOffsets in aIndex array are
								// relative to this
	DWORD dwReserved3;			// must be 0
	struct _avistdindex_entry aIndex[];
} AVISTDINDEX, * PAVISTDINDEX;

#pragma pack(pop)

///////////////////////////////////////////

AVIOutputStream::AVIOutputStream(class AVIOutput *output) {
	this->output = output;
}

AVIOutputStream::~AVIOutputStream() {
}

////////////////////////////////////

class AVIAudioOutputStream : public AVIOutputStream {
public:
	AVIAudioOutputStream(class AVIOutput *out);

	virtual void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples);
};

AVIAudioOutputStream::AVIAudioOutputStream(class AVIOutput *out) : AVIOutputStream(out) {
}

void AVIAudioOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) {
	static_cast<AVIOutputFile *>(output)->writeIndexedChunk(mmioFOURCC('0','1','w','b'), flags, pBuffer, cbBuffer);

	// ActiveMovie/WMP requires a non-zero dwSuggestedBufferSize for
	// hierarchial indexing.  So we continually bump it up to the
	// largest chunk size.

	if (streamInfo.dwSuggestedBufferSize < cbBuffer)
		streamInfo.dwSuggestedBufferSize = cbBuffer;

	streamInfo.dwLength += samples;
}

////////////////////////////////////

class AVIVideoOutputStream : public AVIOutputStream {
public:
	FOURCC id;

	AVIVideoOutputStream(class AVIOutput *out);

	void setCompressed(bool x) { id = x ? mmioFOURCC('0','0','d','c') : mmioFOURCC('0','0','d','b'); }

	virtual void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples);
};

AVIVideoOutputStream::AVIVideoOutputStream(class AVIOutput *out) : AVIOutputStream(out) {
}

void AVIVideoOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) {
	static_cast<AVIOutputFile *>(output)->writeIndexedChunk(id, flags, pBuffer, cbBuffer);

	// ActiveMovie/WMP requires a non-zero dwSuggestedBufferSize for
	// hierarchial indexing.  So we continually bump it up to the
	// largest chunk size.

	if (streamInfo.dwSuggestedBufferSize < cbBuffer)
		streamInfo.dwSuggestedBufferSize = cbBuffer;

	streamInfo.dwLength += samples;
}

////////////////////////////////////

AVIOutput::AVIOutput() {
	audioOut			= NULL;
	videoOut			= NULL;
}

AVIOutput::~AVIOutput() {
	delete audioOut;
	delete videoOut;
}

AVIOutputFile::AVIOutputFile() {
	fastIO				= NULL;
	index				= NULL;
	index_audio			= NULL;
	index_video			= NULL;
	fCaching			= TRUE;
	i64FilePosition		= 0;
	i64XBufferLevel		= 0;
	xblock				= 0;
	fExtendedAVI		= true;
	lAVILimit			= 0x7F000000L;
	fCaptureMode		= false;
	iPadOffset			= 0;
	pSegmentHint		= NULL;
	cbSegmentHint		= 0;
	fInitComplete		= false;
	mbInterleaved		= true;
	mBufferSize			= 1048576;				// reasonable default: 1MB buffer, 256K chunks
	mChunkSize			= mBufferSize >> 2;
	mSuperIndexLimit	= kDefaultSuperIndexEntries;
	mSubIndexLimit		= kDefaultSubIndexEntries;

	mHeaderBlock.reserve(16384);
	i64FarthestWritePoint	= 0;
	lLargestIndexDelta[0]	= 0;
	lLargestIndexDelta[1]	= 0;
	i64FirstIndexedChunk[0] = 0;
	i64FirstIndexedChunk[1] = 0;
	i64LastIndexedChunk[0] = 0;
	i64LastIndexedChunk[1] = 0;
	lIndexedChunkCount[0]	= 0;
	lIndexedChunkCount[1]	= 0;
	lIndexSize			= 0;
	fPreemptiveExtendFailed = false;
}

AVIOutputFile::~AVIOutputFile() {
	delete index;
	delete index_audio;
	delete index_video;
	delete pSegmentHint;

	if (mFile.isOpen()) {
		if (mFile.seekNT(i64FarthestWritePoint))
			mFile.truncateNT();

		mFile.closeNT();
	}

	delete fastIO;
}

//////////////////////////////////

IVDMediaOutputStream *AVIOutputFile::createVideoStream() {
	VDASSERT(!videoOut);
	if (!(videoOut = new_nothrow AVIVideoOutputStream(this)))
		throw MyMemoryError();

	return videoOut;
}

IVDMediaOutputStream *AVIOutputFile::createAudioStream() {
	VDASSERT(!audioOut);
	if (!(audioOut = new_nothrow AVIAudioOutputStream(this)))
		throw MyMemoryError();

	return audioOut;
}

void AVIOutputFile::disable_os_caching() {
	fCaching = FALSE;
}

void AVIOutputFile::disable_extended_avi() {
	fExtendedAVI = false;
}

void AVIOutputFile::set_1Gb_limit() {
	lAVILimit = 0x3F000000L;
}

void AVIOutputFile::set_capture_mode(bool b) {
	fCaptureMode = b;
}

void AVIOutputFile::setSegmentHintBlock(bool fIsFinal, const char *pszNextPath, int cbBlock) {
	if (!pSegmentHint)
		if (!(pSegmentHint = new char[cbBlock]))
			throw MyMemoryError();

	cbSegmentHint = cbBlock;

	memset(pSegmentHint, 0, cbBlock);

	pSegmentHint[0] = !fIsFinal;
	if (pszNextPath)
		strcpy(pSegmentHint+1, pszNextPath);
}

void AVIOutputFile::setBuffering(sint32 nBufferSize, sint32 nChunkSize) {
	VDASSERT(!(mBufferSize & (mBufferSize-1)));
	VDASSERT(!(mChunkSize & (mChunkSize-1)));
	VDASSERT(mChunkSize >= 4096);
	VDASSERT(mBufferSize >= mChunkSize);

	mBufferSize = nBufferSize;
	mChunkSize = nChunkSize;
}

// I don't like to bitch about other programs (well, okay, so I do), but
// Windows Media Player deserves special attention here.  The ActiveMovie
// implementation of OpenDML hierarchial indexing >2Gb *SUCKS*.  It can't
// cope with a JUNK chunk at the end of the hdrl chunk (even though
// the Microsoft documentation in AVIRIFF.H shows one), requires that
// all standard indexes be the same size except for the last one, and
// requires buffer size information for streams.  NONE of this is required
// by ActiveMovie when an extended index is absent (easily verified by
// changing the 'indx' chunks to JUNK chunks).  While diagnosing these
// problems I got an interesting array of error messages from WMP,
// including:
//
//	o Downloading codec from activex.microsoft.com
//		(Because of an extended index!?)
//	o "Cannot allocate memory because no size has been set"
//		???
//	o "The file format is invalid."
//		Detail: "The file format is invalid. (Error=8004022F)"
//		Gee, that clears everything up.
//	o My personal favorite: recursion of the above until the screen
//		has 100+ dialogs and WMP crashes with a stack fault.
//
// Basically, supporting WMP (or as I like to call it, WiMP) was an
// absolute 100% pain in the ass.

bool AVIOutputFile::init(const wchar_t *szFile) {
	return _init(szFile, true);
}

FastWriteStream *AVIOutputFile::initCapture(const wchar_t *szFile) {
	return _init(szFile, false)	? fastIO : NULL;
}

bool AVIOutputFile::_init(const wchar_t *pwszFile, bool fThreaded) {
	AVISUPERINDEX asi={0};
	struct _avisuperindex_entry asie_dummy = {0};

	fLimitTo4Gb = IsFilenameOnFATVolume(pwszFile);

	if (!videoOut)
		return false;

	// Allocate indexes

	if (!(index = new_nothrow AVIIndex()))
		throw MyMemoryError();

	if (fExtendedAVI) {
		if (!(index_audio = new_nothrow AVIIndex()))
			throw MyMemoryError();
		if (!(index_video = new_nothrow AVIIndex()))
			throw MyMemoryError();
	}

	// Initialize main AVI header (avih)

	{
		const AVIStreamHeader_fixed& vhdr = videoOut->getStreamInfo();

		const BITMAPINFOHEADER *pVF = (const BITMAPINFOHEADER *)videoOut->getFormat();

		memset(&avihdr, 0, sizeof avihdr);
		avihdr.dwMicroSecPerFrame		= MulDivUnsigned(vhdr.dwScale, 1000000U, vhdr.dwRate);
		avihdr.dwMaxBytesPerSec			= 0;
		avihdr.dwPaddingGranularity		= 0;
		avihdr.dwFlags					= AVIF_HASINDEX | (mbInterleaved ? AVIF_ISINTERLEAVED : 0);
		avihdr.dwTotalFrames			= vhdr.dwLength;
		avihdr.dwInitialFrames			= 0;
		avihdr.dwStreams				= audioOut ? 2 : 1;
		avihdr.dwSuggestedBufferSize	= 0;
		avihdr.dwWidth					= pVF->biWidth;
		avihdr.dwHeight					= pVF->biHeight;

		static_cast<AVIVideoOutputStream *>(videoOut)->setCompressed(pVF->biCompression == BI_RGB);
	}

	// Initialize file

	if (!fCaching) {
		mFile.open(pwszFile, nsVDFile::kWrite | nsVDFile::kDenyNone | nsVDFile::kOpenAlways | nsVDFile::kSequential);

		if (!(fastIO = new FastWriteStream(pwszFile, mBufferSize, mChunkSize, fThreaded)))
			throw MyMemoryError();
	} else {
		mFile.open(pwszFile, nsVDFile::kWrite | nsVDFile::kDenyWrite | nsVDFile::kOpenAlways | nsVDFile::kSequential | nsVDFile::kWriteThrough);
	}

	i64FilePosition = 0;

	////////// Initialize the first 'AVI ' chunk //////////

	sint64 hdrl_pos;
	sint64 odml_pos;

	DWORD dw[64];

	// start RIFF chunk

	dw[0]	= FOURCC_RIFF;
	dw[1]	= 0;
	dw[2]	= formtypeAVI;

	_writeHdr(dw, 12);

	// start header chunk

	hdrl_pos = _beginList(listtypeAVIHEADER);

	// write out main AVI header

	main_hdr_pos = _writeHdrChunk(ckidAVIMAINHDR, &avihdr, sizeof avihdr);

	// start video stream headers

	strl_pos = _beginList(listtypeSTREAMHEADER);

	// write out video stream header and format

	video_hdr_pos	= _writeHdrChunk(ckidSTREAMHEADER, &videoOut->getStreamInfo(), sizeof(AVIStreamHeader_fixed));
	_writeHdrChunk(ckidSTREAMFORMAT, videoOut->getFormat(), videoOut->getFormatLen());

	// write out video superindex (but make it a JUNK chunk for now).

	if (fExtendedAVI) {
		video_indx_pos = _getPosition();
		asi.fcc = ckidAVIPADDING;
		asi.cb = (sizeof asi)-8 + mSuperIndexLimit*sizeof(_avisuperindex_entry);
		_writeHdr(&asi, sizeof asi);

		for(int i=0; i<mSuperIndexLimit; ++i)
			_writeHdr(&asie_dummy, sizeof(_avisuperindex_entry));
	}

	// finish video stream header

	_closeList(strl_pos);

	// if there is audio...

	if (audioOut) {
		// start audio stream headers

		strl_pos = _beginList(listtypeSTREAMHEADER);

		// write out audio stream header and format

		audio_hdr_pos	= _writeHdrChunk(ckidSTREAMHEADER, &audioOut->getStreamInfo(), sizeof(AVIStreamHeader_fixed));
		audio_format_pos = _writeHdrChunk(ckidSTREAMFORMAT, audioOut->getFormat(), audioOut->getFormatLen());

		_RPT1(0,"Audio header is at %08lx\n", audio_hdr_pos);

		// write out audio superindex (but make it a JUNK chunk for now).

		if (fExtendedAVI) {
			audio_indx_pos = _getPosition();
			asi.fcc = ckidAVIPADDING;
			asi.cb = (sizeof asi)-8 + mSuperIndexLimit*sizeof(_avisuperindex_entry);
			_writeHdr(&asi, sizeof asi);
			for(int i=0; i<mSuperIndexLimit; ++i)
				_writeHdr(&asie_dummy, sizeof(_avisuperindex_entry));
		}

		// finish audio stream header

		_closeList(strl_pos);
	}

	// write out dmlh header (indicates real # of frames)

	if (fExtendedAVI) {
		odml_pos = _beginList('lmdo');

		memset(dw, 0, sizeof dw);
		dmlh_pos = _writeHdrChunk('hlmd', dw, 62*4);

		_closeList(odml_pos);
	}

	// write out segment hint block

	if (pSegmentHint)
		seghint_pos = _writeHdrChunk('mges', pSegmentHint, cbSegmentHint);

	_closeList(hdrl_pos);

	// pad out to a multiple of 2048 bytes
	//
	// WARNING: ActiveMovie/WMP can't handle a trailing JUNK chunk in hdrl
	//			if an extended index is in use.  It says the file format
	//			is invalid!
	//
	// WARNING: WMP8 (XP) thinks a file is an MP3 file if it contains two
	//			MP3 frames within its first 8K.  We force the LIST/movi
	//			chunk to start beyond 8K to solve this problem.

	{
		sint32	curpos = (sint32)_getPosition();

		if (curpos < 8192 || (curpos & 2047)) {
			sint32	padpos = std::max<sint32>(8192, (curpos + 8 + 2047) & ~2047);
			sint32	pad = padpos - curpos - 8;

			std::vector<char> s(pad);

			if (pad > 80)
				sprintf(&s[0], "VirtualDub build %d/%s", version_num,
#ifdef _DEBUG
		"debug"
#else
		"release"
#endif
				);

			_writeHdrChunk(ckidAVIPADDING, &s[0], pad);
		}

//		// If we are using a fast path, sync the fast path to the slow path

//		if (fastIO)
//			fastIO->Seek(i64FilePosition);
	}

	if (fastIO)
//		fastIO->FlushStart();
		fastIO->Put(&mHeaderBlock.front(), mHeaderBlock.size());
	else
		_flushHdr();

	// If we're using the fast path, we're aligned to a sector boundary.
	// Write out the 12 header bytes.

	_openXblock();


	i64EndOfFile = mFile.size();

	fInitComplete = true;

	return TRUE;
}

void AVIOutputFile::finalize() {
	VDDEBUG("AVIOutputFile: Beginning finalize.\n");

	AVISUPERINDEX asi_video;
	AVISUPERINDEX asi_audio;
	std::vector<_avisuperindex_entry> asie_video(mSuperIndexLimit);
	std::vector<_avisuperindex_entry> asie_audio(mSuperIndexLimit);
	DWORD dw;
	int i;

	if (!fInitComplete)
		return;

	// fast path: clean it up and resync slow path.

	// create extended indices

	if (fExtendedAVI && xblock != 0) {
		_createNewIndices(index_video, &asi_video, &asie_video[0], false);
		if (audioOut)
			_createNewIndices(index_audio, &asi_audio, &asie_audio[0], true);
	}

	// finish last Xblock

	_closeXblock();

	if (fastIO) {
		char pad[2048+8];

		// pad to next boundary

		*(long *)(pad + 0) = 'KNUJ';
		*(long *)(pad + 4) = (2048 - ((i64FilePosition+8)&2047))&2047;
		memset(pad+8, 0, 2048);
		_write(pad, *(long *)(pad+4) + 8);

		// flush fast path, get disk position

		fastIO->Flush1();
		fastIO->Flush2((HANDLE)mFile.getRawHandle());

		// seek slow path up
		mFile.seek(i64FilePosition);
	}

	// truncate file (ok to fail)
	mFile.truncateNT();

	_seekHdr(main_hdr_pos+8);
	_writeHdr(&avihdr, sizeof avihdr);

	_seekHdr(video_hdr_pos+8);
	_writeHdr(&videoOut->getStreamInfo(), sizeof(AVIStreamHeader_fixed));

	if (audioOut) {
		_seekHdr(audio_hdr_pos+8);
		_writeHdr(&audioOut->getStreamInfo(), sizeof(AVIStreamHeader_fixed));

		// we have to rewrite the audio format, in case someone
		// fixed fields in the format afterward (MPEG-1/L3)

		_seekHdr(audio_format_pos+8);
		_writeHdr(audioOut->getFormat(), audioOut->getFormatLen());
	}

	if (fExtendedAVI) {
		_seekHdr(dmlh_pos+8);
		dw = videoOut->getStreamInfo().dwLength;
		_writeHdr(&dw, 4);

		if (xblock > 1) {

			_seekHdr(video_indx_pos);
			_writeHdr(&asi_video, sizeof asi_video);
			_writeHdr(&asie_video[0], sizeof(_avisuperindex_entry)*mSuperIndexLimit);

			if (audioOut) {
				_seekHdr(audio_indx_pos);
				_writeHdr(&asi_audio, sizeof asi_audio);
				_writeHdr(&asie_audio[0], sizeof(_avisuperindex_entry)*mSuperIndexLimit);
			}
		}
	}

	if (pSegmentHint) {
		_seekHdr(seghint_pos+8);
		_writeHdr(pSegmentHint, cbSegmentHint);
	}

	_flushHdr();

	for(i=0; i<xblock; i++) {
		AVIBlock& blockinfo = mBlocks[i];

		mFile.seek(blockinfo.riff_pos+4);
		mFile.write(&blockinfo.riff_len, 4);

		mFile.seek(blockinfo.movi_pos+4);
		mFile.write(&blockinfo.movi_len, 4);
	}

	mFile.close();

	VDDEBUG("AVIOutputFile: Finalize was successful.\n");
}

long AVIOutputFile::bufferStatus(long *lplBufferSize) {
	if (fastIO) {
		return fastIO->getBufferStatus(lplBufferSize);
	} else {
		return 0;
	}
}

////////////////////////////

sint64 AVIOutputFile::_writeHdr(const void *data, long len) {
	long writepos = (long)i64FilePosition;
	int cursize = mHeaderBlock.size();

	if (writepos < cursize)
		memcpy(&mHeaderBlock[writepos], data, std::min<int>(cursize-writepos, len));

	if (writepos + len > cursize)
		mHeaderBlock.insert(mHeaderBlock.end(), (char *)data + (cursize-writepos), (char *)data + len);

	i64FilePosition += len;

	return i64FilePosition - len;
}

sint64 AVIOutputFile::_beginList(FOURCC ckid) {
	DWORD dw[3];

	dw[0] = FOURCC_LIST;
	dw[1] = 0;
	dw[2] = ckid;

	return _writeHdr(dw, 12);
}

sint64 AVIOutputFile::_writeHdrChunk(FOURCC ckid, const void *data, long len) {
	DWORD dw[2];
	sint64 pos;

	dw[0] = ckid;
	dw[1] = len;

	pos = _writeHdr(dw, 8);

	_writeHdr(data, len);

	if (len & 1) {
		dw[0] = 0;

		_writeHdr(dw, 1);
	}

	return pos;
}

void AVIOutputFile::_closeList(sint64 pos) {
	DWORD dw;
	sint64 i64FPSave = i64FilePosition;
	
	dw = i64FilePosition - (pos+8);

	_seekHdr(pos+4);
	_writeHdr(&dw, 4);
	_seekHdr(i64FPSave);
}

void AVIOutputFile::_flushHdr() {
	mFile.seek(0);
	i64FilePosition = 0;

	mFile.write(&mHeaderBlock.front(), mHeaderBlock.size());
	i64FilePosition = mHeaderBlock.size();

	if (i64FarthestWritePoint < i64FilePosition)
		i64FarthestWritePoint = i64FilePosition;
}

sint64 AVIOutputFile::_getPosition() {
	return i64FilePosition;
}

void AVIOutputFile::_seekHdr(sint64 i64NewPos) {
	i64FilePosition = i64NewPos;
}

bool AVIOutputFile::_extendFile(sint64 i64NewPoint) {
	bool fSuccess;

	// Have we already extended the file past that point?

	if (i64NewPoint < i64EndOfFile)
		return true;

	// Attempt to extend the file.

	sint64 i64Save = i64FilePosition;

	mFile.seek(i64NewPoint);
	fSuccess = mFile.truncateNT();
	mFile.seek(i64Save);

	if (fSuccess) {
		i64EndOfFile = i64NewPoint;
//		_RPT1(0,"Successfully extended file to %I64d bytes\n", i64EndOfFile);
	} else {
//		_RPT1(0,"Failed to extend file to %I64d bytes\n", i64NewPoint);
	}

	return fSuccess;
}

void AVIOutputFile::_write(const void *data, int len) {

	if (!fPreemptiveExtendFailed && i64FilePosition + len + lIndexSize > i64EndOfFile - 8388608) {
		fPreemptiveExtendFailed = !_extendFile((i64FilePosition + len + lIndexSize + 16777215) & -8388608);
	}

	if (fastIO) {
		fastIO->Put(data,len);
		i64FilePosition += len;
		if (i64FarthestWritePoint < i64FilePosition)
			i64FarthestWritePoint = i64FilePosition;
	} else {
		mFile.write(data, len);

		sint64 pos = mFile.tell();

		if (i64FarthestWritePoint < pos)
			i64FarthestWritePoint = pos;
	}
}

void AVIOutputFile::writeIndexedChunk(FOURCC ckid, uint32 flags, const void *pBuffer, uint32 cbBuffer) {
	AVIIndexEntry2 avie;
	long buf[5];
	static char zero = 0;
	long siz;
	bool fOpenNewBlock = false;
	int nStream = 0;

	if ((ckid&0xffff) > (int)'00')
		nStream = 1;

	// Determine if we need to open another RIFF block (xblock).

	siz = cbBuffer + (cbBuffer&1) + 16;

	// The original AVI format can't accommodate RIFF chunks >4Gb due to the
	// use of 32-bit size fields.  Most RIFF parsers don't handle >2Gb because
	// of inappropriate use of signed variables.  And to top it all off,
	// stupid mistakes in the MCI RIFF parser prevent processing beyond the
	// 1Gb mark.
	//
	// To be save, we keep the first RIFF AVI chunk below 1Gb, and subsequent
	// RIFF AVIX chunks below 2Gb.  We have to leave ourselves a little safety
	// margin (16Mb in this case) for index blocks.

	if (fExtendedAVI)
		if (i64XBufferLevel + siz > (xblock ? 0x7F000000 : lAVILimit))
			fOpenNewBlock = true;

	// Check available disk space.
	//
	// Take the largest separation between data blocks,

	sint64 chunkloc;
	int idxblocksize;
	int idxblocks;
	sint64 maxpoint;

	chunkloc = i64FilePosition;
	if (fOpenNewBlock)
		chunkloc += 24;

	if (!i64FirstIndexedChunk[nStream])
		i64FirstIndexedChunk[nStream] = chunkloc;

	if ((long)(chunkloc - i64LastIndexedChunk[nStream]) > lLargestIndexDelta[nStream])
		lLargestIndexDelta[nStream] = (long)(chunkloc - i64LastIndexedChunk[nStream]);

	++lIndexedChunkCount[nStream];

	// compute how much total space we need to close the file

	idxblocks = 0;

	if (lLargestIndexDelta[0]) {
		idxblocksize = (int)(0x100000000i64 / lLargestIndexDelta[0]);
		if (idxblocksize > mSubIndexLimit)
			idxblocksize = mSubIndexLimit;
		idxblocks = (lIndexedChunkCount[0] + idxblocksize - 1) / idxblocksize;
	}
	if (lLargestIndexDelta[1]) {
		idxblocksize = (int)(0x100000000i64 / lLargestIndexDelta[1]);
		if (idxblocksize > mSubIndexLimit)
			idxblocksize = mSubIndexLimit;
		idxblocks += (lIndexedChunkCount[1] + idxblocksize - 1) / idxblocksize;
	}

	lIndexSize = 0;

	if (fExtendedAVI)
		lIndexSize = idxblocks*sizeof(AVISTDINDEX);

	lIndexSize += 8 + 16*(lIndexedChunkCount[0]+lIndexedChunkCount[1]);
	
	// Give ourselves ~4K of headroom...

	maxpoint = (chunkloc + cbBuffer + 1 + 8 + 14 + 2047 + lIndexSize + 4096) & -2048i64;

	if (fLimitTo4Gb && maxpoint >= 0x100000000i64) {
		_RPT1(0,"overflow detected!  maxpoint=%I64d\n", maxpoint);
		_RPT2(0,"lIndexSize = %08lx (%ld index blocks)\n", lIndexSize, idxblocks);
		_RPT2(0,"sample counts = %ld, %ld\n", lIndexedChunkCount[0], lIndexedChunkCount[1]);

		throw MyError("Out of file space: Files cannot exceed 4 gigabytes on a FAT32 partition.");
	}

	if (!_extendFile(maxpoint))
		throw MyError("Not enough disk space to write additional data.");

	i64LastIndexedChunk[nStream] = chunkloc;

	// If we need to open a new Xblock, do so.

	if (fOpenNewBlock) {
		_closeXblock();
		_openXblock();
	}

	// Write the chunk.

	avie.ckid	= ckid;
	avie.pos	= i64FilePosition - (mBlocks[0].movi_pos+8); //chunkMisc.dwDataOffset - 2064;
	avie.size	= cbBuffer;

	if (flags & AVIIF_KEYFRAME)
		avie.size |= 0x80000000L;

	buf[0] = ckid;
	buf[1] = cbBuffer;

	_write(buf, 8);

	i64XBufferLevel += siz;

	if ((unsigned short)ckid == '10') {
		if (fExtendedAVI)
			index_audio->add(&avie);
	} else {
		if (fExtendedAVI)
			index_video->add(&avie);
	}

	if (index)
		index->add(&avie);

	_write(pBuffer, cbBuffer);

	// Align to 8-byte boundary, not 2-byte, in capture mode.

	if (fCaptureMode) {
		char *pp;
		int offset = (cbBuffer + iPadOffset) & 7;

		// offset=0:	no action
		// offset=1/2:	[00] 'JUNK' 6 <6 bytes>
		// offset=3/4:	[00] 'JUNK' 4 <4 bytes>
		// offset=5/6:	[00] 'JUNK' 2 <2 bytes>
		// offset=7:	00

		if (offset) {
			buf[0]	= 0;
			buf[1]	= 'KNUJ';
			buf[2]	= (-offset) & 6;
			buf[3]	= 0;
			buf[4]	= 0;

			pp = (char *)&buf[1];

			if (offset & 1)
				--pp;

			_write(pp, (offset & 1) + (((offset+1)&7) ? 8+buf[2] : 0));
		}

		iPadOffset = 0;

	} else {

		// Standard AVI: use 2 bytes

		if (cbBuffer & 1)
			_write(&zero, 1);
	}

}

void AVIOutputFile::_closeXblock() {
	AVIBlock& blockinfo = mBlocks[xblock];

	blockinfo.movi_len = i64FilePosition - (blockinfo.movi_pos+8);

	if (!xblock) {
		avihdr.dwTotalFrames = videoOut->getStreamInfo().dwLength;
		_writeLegacyIndex(true);
	}

	blockinfo.riff_len = i64FilePosition - (blockinfo.riff_pos+8);

	++xblock;

	i64XBufferLevel = 0;
}

void AVIOutputFile::_openXblock() {
	DWORD dw[8];

	// If we're in capture mode, keep this stuff aligned to 8-byte boundaries!

	mBlocks.push_back(AVIBlock());

	VDASSERT(mBlocks.size() == xblock+1);

	AVIBlock& blockinfo = mBlocks.back();

	if (xblock != 0) {

		blockinfo.riff_pos = i64FilePosition;

		dw[0] = FOURCC_RIFF;
		dw[1] = 0x7F000000;
		dw[2] = 'XIVA';
		dw[3] = FOURCC_LIST;
		dw[4] = 0x7F000000;
		dw[5] = 'ivom';	// movi
		_write(dw,24);

		blockinfo.movi_pos = i64FilePosition - 12;
	} else {
		blockinfo.riff_pos = 0;

		blockinfo.movi_pos = i64FilePosition;

		dw[0] = FOURCC_LIST;
		dw[1] = 0x7FFFFFFF;
		dw[2] = 'ivom';		// movi

		if (fCaptureMode)
			iPadOffset = 4;

		_write(dw, 12);
	}

	// WARNING: For AVIFile to parse the index correctly, it assumes that the
	// first chunk in an index starts right after the movi chunk!

//	dw[0] = ckidAVIPADDING;
//	dw[1] = 4;
//	dw[2] = 0;
//	_write(dw, 12);
}

void AVIOutputFile::_writeLegacyIndex(bool use_fastIO) {
	if (!index)
		return;

	index->makeIndex();

//	if (use_fastIO && fastIO) {
		DWORD dw[2];

		dw[0] = ckidAVINEWINDEX;
		dw[1] = index->indexLen() * sizeof(AVIINDEXENTRY);
		_write(dw, 8);
		_write(index->indexPtr(), index->indexLen() * sizeof(AVIINDEXENTRY));
//	} else {
//		_writeHdrChunk(ckidAVINEWINDEX, index->indexPtr(), index->indexLen() * sizeof(AVIINDEXENTRY));

	delete index;
	index = NULL;
}

void AVIOutputFile::_createNewIndices(AVIIndex *index, AVISUPERINDEX *asi, _avisuperindex_entry *asie, bool is_audio) {
	AVIIndexEntry2 *asie2;
	int size;
	int indexnum=0;
	int blocksize;

	if (!index || !index->size())
		return;

	index->makeIndex2();

	size = index->indexLen();
	asie2 = index->index2Ptr();

	memset(asie, 0, sizeof(_avisuperindex_entry)*mSuperIndexLimit);

	// Now we run into a bit of a problem.  DirectShow's AVI2 parser requires
	// that all index blocks have the same # of entries (except the last),
	// which is a problem since we also have to guarantee that each block
	// has offsets <4Gb.

	// For now, use a O(n^2) algorithm to find the optimal size.

	blocksize = mSubIndexLimit;

	while(blocksize > 1) {
		int i;
		int nextblock = 0;
		sint64 offset;

		for(i=0; i<size; i++) {
			if (i == nextblock) {
				nextblock += blocksize;
				offset = asie2[i].pos;
			}

			if (asie2[i].pos >= offset + 0x100000000i64)
				break;
		}

		if (i >= size)
			break;

		--blocksize;
	}
	
	int blockcount = (size - 1) / blocksize + 1;

	if (blockcount > mSuperIndexLimit)
		throw MyError("AVIOutput: Not enough superindex entries to index AVI file.  (%d slots required, %d slots preallocated)",
			blockcount, mSuperIndexLimit);
	
	// Write out the actual index blocks.
	const DWORD indexID = is_audio ? '10xi' : '00xi';
	const DWORD chunkID = is_audio ? 'bw10' : static_cast<AVIVideoOutputStream *>(videoOut)->id;
	const DWORD dwSampleSize = is_audio ? audioOut->getStreamInfo().dwSampleSize : videoOut->getStreamInfo().dwSampleSize;

	while(size > 0) {
		int tc = std::min<int>(size, blocksize);

		_writeNewIndex(&asie[indexnum++], asie2, tc, indexID, chunkID, dwSampleSize);

		asie2 += tc;
		size -= tc;
	}

	memset(asi, 0, sizeof(AVISUPERINDEX));
	asi->fcc			= 'xdni';
	asi->cb				= sizeof(AVISUPERINDEX)-8 + sizeof(_avisuperindex_entry)*mSuperIndexLimit;
	asi->wLongsPerEntry	= 4;
	asi->bIndexSubType	= 0;
	asi->bIndexType		= AVI_INDEX_OF_INDEXES;
	asi->nEntriesInUse	= indexnum;
	asi->dwChunkId		= chunkID;
}

void AVIOutputFile::_writeNewIndex(struct _avisuperindex_entry *asie, AVIIndexEntry2 *avie2, int size, FOURCC fcc, DWORD dwChunkId, DWORD dwSampleSize) {
	AVISTDINDEX asi;
	AVIIndexEntry3 asie3[64];
	sint64 offset = avie2->pos;

	VDASSERT(size>0);
	VDASSERT(avie2[size-1].pos - avie2[0].pos < VD64(0x100000000));

	// Check to see if we need to open a new AVIX block
	if (i64XBufferLevel + sizeof(AVISTDINDEX) + size*sizeof(_avistdindex_entry) > (xblock ? 0x7F000000 : lAVILimit)) {
		_closeXblock();
		_openXblock();
	}

	// setup superindex entry

	asie->qwOffset	= i64FilePosition;
	asie->dwSize	= sizeof(AVISTDINDEX) + size*sizeof(_avistdindex_entry);

	if (dwSampleSize) {
		sint64 total_bytes = 0;

		for(int i=0; i<size; i++)
			total_bytes += avie2[i].size & 0x7FFFFFFF;

		asie->dwDuration = (DWORD)(total_bytes / dwSampleSize);
	} else
		asie->dwDuration = size;

	asi.fcc				= ((dwChunkId & 0xFFFF)<<16) | 'xi';
	asi.cb				= asie->dwSize - 8;
	asi.wLongsPerEntry	= 2;
	asi.bIndexSubType	= 0;
	asi.bIndexType		= AVI_INDEX_OF_CHUNKS;
	asi.nEntriesInUse	= size;
	asi.dwChunkId		= dwChunkId;
	asi.qwBaseOffset	= offset;
	asi.dwReserved3		= 0;

	_write(&asi, sizeof asi);

	// stored entries are relative to the start of the movi chunk and to the start of the data chunk
	// instead of absolute to the data payload, so we have to correct
	sint64 delta = mBlocks[0].movi_pos + 16 - offset;

	while(size > 0) {
		int tc = size;
		if (tc>64) tc=64;

		for(int i=0; i<tc; i++) {
			asie3[i].dwOffset	= (DWORD)(avie2->pos + delta);
			asie3[i].dwSizeKeyframe		= avie2->size;
			++avie2;
		}

		_write(asie3, tc*sizeof(AVIIndexEntry3));

		size -= tc;
	}
}
