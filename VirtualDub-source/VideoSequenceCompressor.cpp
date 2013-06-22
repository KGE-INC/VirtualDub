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

#include <crtdbg.h>
#include <windows.h>
#include <vfw.h>

#include "VideoSequenceCompressor.h"
#include "Error.h"

//////////////////////////////////////////////////////////////////////////////
//
//	IMITATING WIN2K AVISAVEV() BEHAVIOR IN 0x7FFFFFFF EASY STEPS
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
	finish();

	delete pConfigData;
	delete pOutputBuffer;
	delete pPrevBuffer;
}

void VideoSequenceCompressor::init(HIC hic, BITMAPINFO *pbiInput, BITMAPINFO *pbiOutput, long lQ, long lKeyRate) {
	ICINFO	info;
	LRESULT	res;

	this->hic		= hic;
	this->pbiInput	= pbiInput;
	this->pbiOutput	= pbiOutput;
	this->lKeyRate	= lKeyRate;

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

	// Start compression process

	res = ICCompressBegin(hic, pbiInput, pbiOutput);

	if (res != ICERR_OK)
		throw MyICError("Unable to start video compression", res);

	// Start decompression process if necessary

	if (pPrevBuffer) {
		res = ICDecompressBegin(hic, pbiOutput, pbiInput);

		if (res != ICERR_OK) {
			ICCompressEnd(hic);
			throw MyICError("Unable to start video compression", res);
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

	++lFrameNum;

	*plSize = pbiOutput->bmiHeader.biSizeImage;

	// If we're using a compressor with a stupid algorithm (Microsoft Video 1),
	// we have to decompress the frame again to compress the next one....

	if (res==ICERR_OK && pPrevBuffer && (!lKeyRate || lKeyRateCounter>1)) {
		res = ICDecompress(hic, dwFlags & AVIIF_KEYFRAME ? 0 : ICDECOMPRESS_NOTKEYFRAME
				,(LPBITMAPINFOHEADER)pbiOutput
				,pOutputBuffer
				,(LPBITMAPINFOHEADER)pbiInput
				,pPrevBuffer);
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
