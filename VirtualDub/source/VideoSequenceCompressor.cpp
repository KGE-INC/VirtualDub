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
#include <windows.h>
#include <vfw.h>

#include "VideoSequenceCompressor.h"
#include <vd2/system/error.h>
#include "crash.h"
#include "misc.h"

// XviD VFW extensions

#define VFW_EXT_RESULT			1

//////////////////////////////////////////////////////////////////////////////
//
//	IMITATING WIN2K AVISAVEV() BEHAVIOR IN 0x7FFFFFFF EASY STEPS
//
//	It seems some codecs are rather touchy about how exactly you call
//	them, and do a variety of odd things if you don't imitiate the
//	standard libraries... compressing at top quality seems to be the most
//	common symptom.
//
//	ICM_COMPRESS_FRAMES_INFO:
//
//		dwFlags			Trashed with address of lKeyRate in tests. Something
//						might be looking for a non-zero value here, so better
//						set it.
//		lpbiOutput		NULL.
//		lOutput			0.
//		lpbiInput		NULL.
//		lInput			0.
//		lStartFrame		0.
//		lFrameCount		Number of frames.
//		lQuality		Set to quality factor, or zero if not supported.
//		lDataRate		Set to data rate in 1024*kilobytes, or zero if not
//						supported.
//		lKeyRate		Set to the desired maximum keyframe interval.  For
//						all keyframes, set to 1.		
//
//	ICM_COMPRESS:
//
//		dwFlags			Equal to ICCOMPRESS_KEYFRAME if a keyframe is
//						required, and zero otherwise.
//		lpckid			Always points to zero.
//		lpdwFlags		Points to AVIIF_KEYFRAME if a keyframe is required,
//						and zero otherwise.
//		lFrameNum		Ascending from zero.
//		dwFrameSize		Always set to 7FFFFFFF (Win9x) or 00FFFFFF (WinNT)
//						for first frame.  Set to zero for subsequent frames
//						if data rate control is not active or not supported,
//						and to the desired frame size in bytes if it is.
//		dwQuality		Set to quality factor from 0-10000 if quality is
//						supported.  Otherwise, it is zero.
//		lpbiPrev		Set to NULL if not required.
//		lpPrev			Set to NULL if not required.
//
//////////////////////////////////////////////////////////////////////////////

VideoSequenceCompressor::VideoSequenceCompressor() {
	pPrevBuffer		= NULL;
	pOutputBuffer	= NULL;
	pConfigData		= NULL;
}

VideoSequenceCompressor::~VideoSequenceCompressor() {

	freemem(pbiInput);
	freemem(pbiOutput);

	finish();

	delete pConfigData;
	delete pOutputBuffer;
	delete pPrevBuffer;
}

void VideoSequenceCompressor::init(HIC hic, BITMAPINFO *pbiInput, BITMAPINFO *pbiOutput, long lQ, long lKeyRate) {
	ICINFO	info;
	LRESULT	res;
	int cbSizeIn, cbSizeOut;

	cbSizeIn = pbiInput->bmiHeader.biSize + pbiInput->bmiHeader.biClrUsed*4;
	cbSizeOut = pbiOutput->bmiHeader.biSize + pbiOutput->bmiHeader.biClrUsed*4;

	this->hic		= hic;
	this->pbiInput	= (BITMAPINFO *)allocmem(cbSizeIn);
	this->pbiOutput	= (BITMAPINFO *)allocmem(cbSizeOut);
	this->lKeyRate	= lKeyRate;

	memcpy(this->pbiInput, pbiInput, cbSizeIn);
	memcpy(this->pbiOutput, pbiOutput, cbSizeOut);

	lKeyRateCounter = 1;

	// Retrieve compressor information.

	res = ICGetInfo(hic, &info, sizeof info);

	if (!res)
		throw MyError("Unable to retrieve video compressor information.");

	// Analyze compressor.

	this->dwFlags = info.dwFlags;

	if (info.dwFlags & VIDCF_TEMPORAL) {
		if (!(info.dwFlags & VIDCF_FASTTEMPORALC)) {
			// Allocate backbuffer

			if (!(pPrevBuffer = new char[pbiInput->bmiHeader.biSizeImage]))
				throw MyMemoryError();
		}
	}

	if (info.dwFlags & VIDCF_QUALITY)
		lQuality = lQ;
	else
		lQuality = 0;

	// Allocate destination buffer

	lMaxPackedSize = ICCompressGetSize(hic, pbiInput, pbiOutput);

	// Work around a bug in Huffyuv.  Ben tried to save some memory
	// and specified a "near-worst-case" bound in the codec instead
	// of the actual worst case bound.  Unfortunately, it's actually
	// not that hard to exceed the codec's estimate with noisy
	// captures -- the most common way is accidentally capturing
	// static from a non-existent channel.
	//
	// According to the 2.1.1 comments, Huffyuv uses worst-case
	// values of 24-bpp for YUY2/UYVY and 40-bpp for RGB, while the
	// actual worst case values are 43 and 51.  We'll compute the
	// 43/51 value, and use the higher of the two.

	if (isEqualFOURCC(info.fccHandler, 'UYFH')) {
		long lRealMaxPackedSize = pbiInput->bmiHeader.biWidth * pbiInput->bmiHeader.biHeight;

		if (pbiInput->bmiHeader.biCompression == BI_RGB)
			lRealMaxPackedSize = (lRealMaxPackedSize * 51) >> 3;
		else
			lRealMaxPackedSize = (lRealMaxPackedSize * 43) >> 3;

		if (lRealMaxPackedSize > lMaxPackedSize)
			lMaxPackedSize = lRealMaxPackedSize;
	}

	if (!(pOutputBuffer = new char[lMaxPackedSize]))
		throw MyMemoryError();

	// Save configuration state.
	//
	// Ordinarily, we wouldn't do this, but there seems to be a bug in
	// the Microsoft MPEG-4 compressor that causes it to reset its
	// configuration data after a compression session.  This occurs
	// in all versions from V1 through V3.
	//
	// Stupid fscking Matrox driver returns -1!!!

	cbConfigData = ICGetStateSize(hic);

	if (cbConfigData > 0) {
		if (!(pConfigData = new char[cbConfigData]))
			throw MyMemoryError();

		cbConfigData = ICGetState(hic, pConfigData, cbConfigData);

		// As odd as this may seem, if this isn't done, then the Indeo5
		// compressor won't allow data rate control until the next
		// compression operation!

		if (cbConfigData)
			ICSetState(hic, pConfigData, cbConfigData);
	}

	lMaxFrameSize = 0;
	lSlopSpace = 0;
}

void VideoSequenceCompressor::setDataRate(long lDataRate, long lUsPerFrame, long lFrameCount) {

	if (lDataRate && (dwFlags & VIDCF_CRUNCH))
		lMaxFrameSize = MulDiv(lDataRate, lUsPerFrame, 1000000);
	else
		lMaxFrameSize = 0;

	// Indeo 5 needs this message for data rate clamping.

	// The Morgan codec requires the message otherwise it assumes 100%
	// quality :(

	// The original version (2700) MPEG-4 V1 requires this message, period.
	// V3 (DivX) gives crap if we don't send it.  So special case it.

	ICINFO ici;

	ICGetInfo(hic, &ici, sizeof ici);

	{
		ICCOMPRESSFRAMES icf;

		memset(&icf, 0, sizeof icf);

		icf.dwFlags		= (DWORD)&icf.lKeyRate;
		icf.lStartFrame = 0;
		icf.lFrameCount = lFrameCount;
		icf.lQuality	= lQuality;
		icf.lDataRate	= lDataRate;
		icf.lKeyRate	= lKeyRate;
		icf.dwRate		= 1000000;
		icf.dwScale		= lUsPerFrame;

		ICSendMessage(hic, ICM_COMPRESS_FRAMES_INFO, (WPARAM)&icf, sizeof(ICCOMPRESSFRAMES));
	}
}

void VideoSequenceCompressor::start() {
	LRESULT	res;

	// Query for VFW extensions (XviD)

#if 0
	BITMAPINFOHEADER bih = {0};

	bih.biCompression = 0xFFFFFFFF;

	res = ICCompressQuery(hic, &bih, NULL);

	mVFWExtensionMessageID = 0;

	if ((LONG)res >= 0) {
		mVFWExtensionMessageID = res;
	}
#endif

	// Start compression process

	res = ICCompressBegin(hic, pbiInput, pbiOutput);

	if (res != ICERR_OK)
		throw MyICError(res, "Cannot start video compression:\n\n%%s\n(error code %d)", (int)res);

	// Start decompression process if necessary

	if (pPrevBuffer) {
		res = ICDecompressBegin(hic, pbiOutput, pbiInput);

		if (res != ICERR_OK) {
			ICCompressEnd(hic);
			throw MyICError(res, "Cannot start video compression:\n\n%%s\n(error code %d)", (int)res);
		}
	}

	fCompressionStarted = true;
	lFrameNum = 0;
}

void VideoSequenceCompressor::finish() {
	if (!fCompressionStarted)
		return;

	if (pPrevBuffer)
		ICDecompressEnd(hic);

	ICCompressEnd(hic);

	fCompressionStarted = false;

	// Reset MPEG-4 compressor

	if (cbConfigData && pConfigData)
		ICSetState(hic, pConfigData, cbConfigData);
}

void VideoSequenceCompressor::dropFrame() {
	if (lKeyRate && lKeyRateCounter>1)
		--lKeyRateCounter;

	++lFrameNum;
}

void *VideoSequenceCompressor::packFrame(void *pBits, bool *pfKeyframe, long *plSize) {
	DWORD dwChunkId = 0;
	DWORD dwFlags=0, dwFlagsIn = ICCOMPRESS_KEYFRAME;
	DWORD res;
	DWORD sizeImage;
	long lAllowableFrameSize=0;//xFFFFFF;	// yes, this is illegal according
											// to the docs (see below)

	long lKeyRateCounterSave = lKeyRateCounter;

	// Figure out if we should force a keyframe.  If we don't have any
	// keyframe interval, force only the first frame.  Otherwise, make
	// sure that the key interval is lKeyRate or less.  We count from
	// the last emitted keyframe, since the compressor can opt to
	// make keyframes on its own.

	if (!lKeyRate) {
		if (lFrameNum)
			dwFlagsIn = 0;
	} else {
		if (--lKeyRateCounter)
			dwFlagsIn = 0;
		else
			lKeyRateCounter = lKeyRate;
	}

	// Figure out how much space to give the compressor, if we are using
	// data rate stricting.  If the compressor takes up less than quota
	// on a frame, save the space for later frames.  If the compressor
	// uses too much, reduce the quota for successive frames, but do not
	// reduce below half datarate.

	if (lMaxFrameSize) {
		lAllowableFrameSize = lMaxFrameSize + (lSlopSpace>>2);

		if (lAllowableFrameSize < (lMaxFrameSize>>1))
			lAllowableFrameSize = lMaxFrameSize>>1;
	}

	// A couple of notes:
	//
	//	o  ICSeqCompressFrame() passes 0x7FFFFFFF when data rate control
	//	   is inactive.  Docs say 0.  We pass 0x7FFFFFFF here to avoid
	//	   a bug in the Indeo 5 QC driver, which page faults if
	//	   keyframe interval=0 and max frame size = 0.

	sizeImage = pbiOutput->bmiHeader.biSizeImage;

//	pbiOutput->bmiHeader.biSizeImage = 0;

	// Compress!

	if (dwFlagsIn)
		dwFlags = AVIIF_KEYFRAME;

	if (IsMMXState())
		throw MyInternalError("MMX state left on: %s:%d", __FILE__, __LINE__);

	VDCHECKPOINT;

	vdprotected3("compressing frame %u from %08x to %08x", unsigned, lFrameNum, unsigned, (unsigned)pBits, unsigned, (unsigned)pOutputBuffer) {
		res = ICCompress(hic, dwFlagsIn,
				(LPBITMAPINFOHEADER)pbiOutput, pOutputBuffer,
				(LPBITMAPINFOHEADER)pbiInput, pBits,
				&dwChunkId,
				&dwFlags,
				lFrameNum,
				lFrameNum ? lAllowableFrameSize : 0xFFFFFF,
				lQuality,
				dwFlagsIn & ICCOMPRESS_KEYFRAME ? NULL : (LPBITMAPINFOHEADER)pbiInput,
				dwFlagsIn & ICCOMPRESS_KEYFRAME ? NULL : pPrevBuffer);
	}

	VDCHECKPOINT;

	if (IsMMXState())
		ClearMMXState();


	// Special handling for DivX 5 and XviD codecs:
	//
	// A one-byte frame starting with 0x7f should be discarded
	// (lag for B-frame).

	bool bNoOutputProduced = false;

	if (pbiOutput->bmiHeader.biCompression == '05xd' ||
		pbiOutput->bmiHeader.biCompression == '05XD' ||
		pbiOutput->bmiHeader.biCompression == 'divx' ||
		pbiOutput->bmiHeader.biCompression == 'DIVX'
		) {
		if (pbiOutput->bmiHeader.biSizeImage == 1 && *(char *)pOutputBuffer == 0x7f) {
			bNoOutputProduced = true;
		}
	}

	// Special handling for XviD codec:
	//
	// Query codec for extended status.

#if 0
	if (mVFWExtensionMessageID) {
		struct {
			DWORD input_consumed;
			DWORD output_produced;
		} result;

		if (ICERR_OK == ICSendMessage(hic, mVFWExtensionMessageID, VFW_EXT_RESULT, (LPARAM)&result)) {

			if (!result.output_produced) {
				bNoOutputProduced = true;
			}
		}
	}
#endif

	if (bNoOutputProduced) {
		pbiOutput->bmiHeader.biSizeImage = sizeImage;
		lKeyRateCounter = lKeyRateCounterSave;
		return NULL;
	}


	_RPT2(0,"Compressed frame %d: %d bytes\n", lFrameNum, pbiOutput->bmiHeader.biSizeImage);

	++lFrameNum;

	*plSize = pbiOutput->bmiHeader.biSizeImage;

	// If we're using a compressor with a stupid algorithm (Microsoft Video 1),
	// we have to decompress the frame again to compress the next one....

	if (res==ICERR_OK && pPrevBuffer && (!lKeyRate || lKeyRateCounter>1)) {

		VDCHECKPOINT;

		res = ICDecompress(hic, dwFlags & AVIIF_KEYFRAME ? 0 : ICDECOMPRESS_NOTKEYFRAME
				,(LPBITMAPINFOHEADER)pbiOutput
				,pOutputBuffer
				,(LPBITMAPINFOHEADER)pbiInput
				,pPrevBuffer);

		VDCHECKPOINT;
	}

	pbiOutput->bmiHeader.biSizeImage = sizeImage;

	if (res != ICERR_OK)
		throw MyICError("Video compression", res);

	// Update quota.

	if (lMaxFrameSize) {
		lSlopSpace += lMaxFrameSize - *plSize;

		_RPT3(0,"Compression: allowed %d, actual %d, slop %+d\n", lAllowableFrameSize, *plSize, lSlopSpace);
	}

	// Was it a keyframe?

	if (dwFlags & AVIIF_KEYFRAME) {
		*pfKeyframe = true;
		lKeyRateCounter = lKeyRate;
	} else {
		*pfKeyframe = false;
	}

	return pOutputBuffer;
}
