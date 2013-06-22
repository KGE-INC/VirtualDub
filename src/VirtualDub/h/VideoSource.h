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

#ifndef f_VIDEOSOURCE_H
#define f_VIDEOSOURCE_H

#include <windows.h>
#include <vfw.h>
#include <vd2/system/VDString.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/vdstl.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Riza/videocodec.h>

#include "DubSource.h"

class AVIStripeSystem;
class AVIStripeIndexLookup;
class IMJPEGDecoder;
class IAVIReadHandler;
class IAVIReadStream;
class IVDStreamSource;

class IVDVideoSource : public IVDRefCount {
public:
	virtual IVDStreamSource *asStream() = 0;

	virtual BITMAPINFOHEADER *getImageFormat() = 0;

	virtual const void *getFrameBuffer() = 0;

	virtual const VDPixmap& getTargetFormat() = 0;
	virtual bool		setTargetFormat(int format) = 0;
	virtual bool		setDecompressedFormat(int depth) = 0;
	virtual bool		setDecompressedFormat(const BITMAPINFOHEADER *pbih) = 0;

	virtual BITMAPINFOHEADER *getDecompressedFormat() = 0;

	virtual void		streamSetDesiredFrame(VDPosition frame_num) = 0;
	virtual VDPosition	streamGetNextRequiredFrame(bool& is_preroll) = 0;
	virtual int			streamGetRequiredCount(uint32 *totalsize) = 0;
	virtual const void *streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition sample_num, VDPosition target_sample) = 0;
	virtual uint32		streamGetDecodePadding() = 0;
	virtual void		streamFillDecodePadding(void *buffer, uint32 data_len) = 0;

	virtual void		streamBegin(bool fRealTime, bool bForceReset) = 0;
	virtual void		streamRestart() = 0;

	virtual void		invalidateFrameBuffer() = 0;
	virtual	bool		isFrameBufferValid() = 0;

	virtual const void *getFrame(VDPosition frameNum) = 0;

	virtual char		getFrameTypeChar(VDPosition lFrameNum) = 0;

	enum eDropType {
		kDroppable		= 0,
		kDependant,
		kIndependent,
	};

	virtual eDropType	getDropType(VDPosition lFrameNum) = 0;

	virtual bool isKey(VDPosition lSample) = 0;
	virtual VDPosition nearestKey(VDPosition lSample) = 0;
	virtual VDPosition prevKey(VDPosition lSample) = 0;
	virtual VDPosition nextKey(VDPosition lSample) = 0;

	virtual bool		isKeyframeOnly() = 0;
	virtual bool		isType1() = 0;

	virtual VDPosition	streamToDisplayOrder(VDPosition sample_num) = 0;
	virtual VDPosition	displayToStreamOrder(VDPosition display_num) = 0;
	virtual VDPosition	getRealDisplayFrame(VDPosition display_num) = 0;

	virtual bool		isDecodable(VDPosition sample_num) = 0;

	virtual sint64		getSampleBytePosition(VDPosition sample_num) = 0;
};

class VideoSource : public DubSource, public IVDVideoSource {
protected:
	HANDLE		hBufferObject;
	LONG		lBufferOffset;
	void		*lpvBuffer;
	uint32		mFrameBufferSize;

	vdstructex<BITMAPINFOHEADER> mpTargetFormatHeader;
	VDPixmap	mTargetFormat;
	int			mTargetFormatVariant;
	VDPosition	stream_desired_frame;
	VDPosition	stream_current_frame;

	uint32		mPalette[256];

	void *AllocFrameBuffer(long size);
	void FreeFrameBuffer();

	bool setTargetFormatVariant(int format, int variant);
	virtual bool _isKey(VDPosition lSample);

	VideoSource();

public:
	enum {
		IFMODE_NORMAL		=0,
		IFMODE_SWAP			=1,
		IFMODE_SPLIT1		=2,
		IFMODE_SPLIT2		=3,
		IFMODE_DISCARD1		=4,
		IFMODE_DISCARD2		=5,
	};

	virtual ~VideoSource();

	IVDStreamSource *asStream() { return this; }

	int AddRef() { return DubSource::AddRef(); }
	int Release() { return DubSource::Release(); }

	BITMAPINFOHEADER *getImageFormat() {
		return (BITMAPINFOHEADER *)getFormat();
	}

	virtual const void *getFrameBuffer() {
		return lpvBuffer;
	}

	virtual const VDPixmap& getTargetFormat() { return mTargetFormat; }
	virtual bool setTargetFormat(int format);
	virtual bool setDecompressedFormat(int depth);
	virtual bool setDecompressedFormat(const BITMAPINFOHEADER *pbih);

	BITMAPINFOHEADER *getDecompressedFormat() {
		return mpTargetFormatHeader.data();
	}

	virtual void streamSetDesiredFrame(VDPosition frame_num);
	virtual VDPosition streamGetNextRequiredFrame(bool& is_preroll);
	virtual int	streamGetRequiredCount(uint32 *totalsize);
	virtual uint32 streamGetDecodePadding() { return 0; }
	virtual void streamFillDecodePadding(void *inputBuffer, uint32 data_len) {}

	virtual void streamBegin(bool fRealTime, bool bForceReset);
	virtual void streamRestart();

	virtual void invalidateFrameBuffer();
	virtual	bool isFrameBufferValid() = NULL;

	virtual const void *getFrame(VDPosition frameNum) = NULL;

	virtual bool isKey(VDPosition lSample);
	virtual VDPosition nearestKey(VDPosition lSample);
	virtual VDPosition prevKey(VDPosition lSample);
	virtual VDPosition nextKey(VDPosition lSample);

	virtual bool isKeyframeOnly();
	virtual bool isType1();

	virtual VDPosition	streamToDisplayOrder(VDPosition sample_num) { return sample_num; }
	virtual VDPosition	displayToStreamOrder(VDPosition display_num) { return display_num; }
	virtual VDPosition	getRealDisplayFrame(VDPosition display_num) { return display_num; }

	virtual sint64		getSampleBytePosition(VDPosition sample_num) { return -1; }

	virtual bool IsVBR() const { return true; }
};

class VideoSourceAVI : public VideoSource {
private:
	IAVIReadHandler *pAVIFile;
	IAVIReadStream *pAVIStream;
	VDPosition		lLastFrame;
	BITMAPINFOHEADER *bmihTemp;

	VDPixmapLayout	mSourceLayout;
	int				mSourceVariant;
	uint32			mSourceFrameSize;

	AVIStripeSystem			*stripesys;
	IAVIReadHandler			**stripe_files;
	IAVIReadStream			**stripe_streams;
	AVIStripeIndexLookup	*stripe_index;
	int						stripe_count;

	HBITMAP		hbmLame;
	bool		fUseGDI;
	bool		fAllKeyFrames;
	bool		bIsType1;
	bool		bDirectDecompress;
	bool		bInvertFrames;

	IAVIReadStream *format_stream;

	char		*key_flags;
	bool		use_internal;
	int			mjpeg_mode;
	void		*mjpeg_reorder_buffer;
	int			mjpeg_reorder_buffer_size;
	long		*mjpeg_splits;
	VDPosition	mjpeg_last;
	long		mjpeg_last_size;
	FOURCC		fccForceVideo;
	FOURCC		fccForceVideoHandler;

	ErrorMode	mErrorMode;
	bool		mbMMXBrokenCodecDetected;
	bool		mbConcealingErrors;
	bool		mbDecodeStarted;
	bool		mbDecodeRealTime;

	vdautoptr<IVDVideoDecompressor>	mpDecompressor;

	VDStringW	mDriverName;
	char		szCodecName[128];

	void _construct();
	void _destruct();

	bool AttemptCodecNegotiation(BITMAPINFOHEADER *);

	void DecompressFrame(const void *src);

	~VideoSourceAVI();

public:
	VideoSourceAVI(IAVIReadHandler *pAVI, AVIStripeSystem *stripesys=NULL, IAVIReadHandler **stripe_files=NULL, bool use_internal=false, int mjpeg_mode=0, FOURCC fccForceVideo=0, FOURCC fccForceVideoHandler=0);

	void Reinit();
	void redoKeyFlags();

	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);
	bool _isKey(VDPosition samp);
	VDPosition nearestKey(VDPosition lSample);
	VDPosition prevKey(VDPosition lSample);
	VDPosition nextKey(VDPosition lSample);

	bool setTargetFormat(int format);
	bool setDecompressedFormat(int depth) { return VideoSource::setDecompressedFormat(depth); }
	bool setDecompressedFormat(const BITMAPINFOHEADER *pbih);
	void invalidateFrameBuffer();
	bool isFrameBufferValid();
	bool isStreaming();

	void streamBegin(bool fRealTime, bool bForceReset);
	const void *streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition sample_num, VDPosition target_sample);
	void streamEnd();

	// I really hate doing this, but an awful lot of codecs are sloppy about their
	// Huffman or VLC decoding and read a few bytes beyond the end of the stream.
	uint32 streamGetDecodePadding() { return 16; }

	// This is to work around an XviD decode bug. From squid_80:
	// "When decompressing a b-frame, Xvid reads past the end of the input buffer looking for a resync
	//  marker. This is the nasty bit - if it sees what it thinks is a resync marker it toddles off the
	//  end of the input buffer merrily decoding garbage. Unfortunately it doesn't stay merry for long.
	//  Best case = artifacts in the decompressed frame, worst case = heap corruption which lets the
	//  encode continue but with a borked result, normal case = plain old access violation."
	void streamFillDecodePadding(void *inputBuffer, uint32 data_len);

	const void *getFrame(VDPosition frameNum);

	HIC	getDecompressorHandle() const {
		if (!mpDecompressor)
			return NULL;

		const HIC *pHIC = (const HIC *)mpDecompressor->GetRawCodecHandlePtr();

		return pHIC ? *pHIC : NULL;
	}

	const wchar_t *getDecompressorName() const {
		return mpDecompressor ? mpDecompressor->GetName() : NULL;
	}

	char getFrameTypeChar(VDPosition lFrameNum);
	eDropType getDropType(VDPosition lFrameNum);
	bool isKeyframeOnly();
	bool isType1();
	bool isDecodable(VDPosition sample_num);

	ErrorMode getDecodeErrorMode() { return mErrorMode; }
	void setDecodeErrorMode(ErrorMode mode);
	bool isDecodeErrorModeSupported(ErrorMode mode);

	VDPosition	getRealDisplayFrame(VDPosition display_num);
	sint64 getSampleBytePosition(VDPosition pos);
};

#endif
