//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2004 Avery Lee
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

#include <vd2/system/vdtypes.h>
#include <vd2/system/strutil.h>
#include <vd2/system/Error.h>
#include <vd2/Riza/w32audiocodec.h>

namespace {
	// Need to take care of this at some point.
	void SafeCopyWaveFormat(vdstructex<WAVEFORMATEX>& dst, const WAVEFORMATEX *src) {
		if (src->wFormatTag == WAVE_FORMAT_PCM) {
			dst.resize(sizeof(WAVEFORMATEX));
			dst->cbSize = 0;
			memcpy(dst.data(), src, sizeof(PCMWAVEFORMAT));
		} else
			dst.assign(src, sizeof(WAVEFORMATEX) + src->cbSize);
	}
}

VDAudioCodecW32::VDAudioCodecW32()
	: mhStream(NULL)
	, mOutputReadPt(0)
{
	mDriverName[0] = 0;
	mDriverFilename[0] = 0;
}

VDAudioCodecW32::~VDAudioCodecW32() {
	Shutdown();
}

void VDAudioCodecW32::Init(const WAVEFORMATEX *pSrcFormat, const WAVEFORMATEX *pDstFormat) {
	Shutdown();

	SafeCopyWaveFormat(mSrcFormat, pSrcFormat);

	if (pDstFormat) {
		SafeCopyWaveFormat(mDstFormat, pDstFormat);
	} else {
		DWORD dwDstFormatSize;

		VDVERIFY(!acmMetrics(NULL, ACM_METRIC_MAX_SIZE_FORMAT, (LPVOID)&dwDstFormatSize));

		mDstFormat.resize(dwDstFormatSize);
		mDstFormat->wFormatTag	= WAVE_FORMAT_PCM;

		if (acmFormatSuggest(NULL, (WAVEFORMATEX *)pSrcFormat, mDstFormat.data(), dwDstFormatSize, ACM_FORMATSUGGESTF_WFORMATTAG)) {
			Shutdown();
			throw MyError(
					"No audio decompressor could be found to decompress the source audio format.\n"
					"(source format tag: %04x)"
					, (uint16)pSrcFormat->wFormatTag
				);
		}

		// sanitize the destination format a bit

		if (mDstFormat->wBitsPerSample!=8 && mDstFormat->wBitsPerSample!=16)
			mDstFormat->wBitsPerSample=16;

		if (mDstFormat->nChannels!=1 && mDstFormat->nChannels!=2)
			mDstFormat->nChannels = 2;

		mDstFormat->nBlockAlign		= (uint16)((mDstFormat->wBitsPerSample>>3) * mDstFormat->nChannels);
		mDstFormat->nAvgBytesPerSec	= mDstFormat->nBlockAlign * mDstFormat->nSamplesPerSec;
		mDstFormat->cbSize				= 0;
	}

	// open conversion stream

	MMRESULT res;

	memset(&mBufferHdr, 0, sizeof mBufferHdr);	// Do this so we can detect whether the buffer is prepared or not.

	res = acmStreamOpen(&mhStream, NULL, (WAVEFORMATEX *)pSrcFormat, mDstFormat.data(), NULL, 0, 0, ACM_STREAMOPENF_NONREALTIME);

	if (res) {
		Shutdown();

		if (res == ACMERR_NOTPOSSIBLE) {
			throw MyError(
						"Error initializing audio stream decompression:\n"
						"The requested conversion is not possible.\n"
						"\n"
						"Check to make sure you have the required codec%s."
						,
						(pSrcFormat->wFormatTag&~1)==0x160 ? " (Microsoft Audio Codec)" : ""
					);
		} else
			throw MyError("Error initializing audio stream decompression.");
	}

	DWORD dwSrcBufferSize = mSrcFormat->nAvgBytesPerSec / 5;
	DWORD dwDstBufferSize = mDstFormat->nAvgBytesPerSec / 5;

	if (!dwSrcBufferSize)
		dwSrcBufferSize = 1;

	dwSrcBufferSize += mSrcFormat->nBlockAlign - 1;
	dwSrcBufferSize -= dwSrcBufferSize % mSrcFormat->nBlockAlign;

	if (!dwDstBufferSize)
		dwDstBufferSize = 1;

	dwDstBufferSize += mDstFormat->nBlockAlign - 1;
	dwDstBufferSize -= dwDstBufferSize % mDstFormat->nBlockAlign;

	if (acmStreamSize(mhStream, dwSrcBufferSize, &dwDstBufferSize, ACM_STREAMSIZEF_SOURCE)) {
		memset(&mBufferHdr, 0, sizeof mBufferHdr);
		throw MyError("Error initializing audio stream output size.");
	}

	mInputBuffer.resize(dwSrcBufferSize);
	mOutputBuffer.resize(dwDstBufferSize);

	mBufferHdr.cbStruct		= sizeof(ACMSTREAMHEADER);
	mBufferHdr.pbSrc		= (LPBYTE)&mInputBuffer.front();
	mBufferHdr.cbSrcLength	= mInputBuffer.size();
	mBufferHdr.pbDst		= (LPBYTE)&mOutputBuffer.front();
	mBufferHdr.cbDstLength	= mOutputBuffer.size();

	if (acmStreamPrepareHeader(mhStream, &mBufferHdr, 0)) {
		memset(&mBufferHdr, 0, sizeof mBufferHdr);
		throw MyError("Error preparing audio decompression buffers.");
	}

	Restart();

	// try to get driver name for debugging purposes (OK to fail)
	mDriverName[0] = mDriverFilename[0] = 0;

	HACMDRIVERID hDriverID;
	if (!acmDriverID((HACMOBJ)mhStream, &hDriverID, 0)) {
		ACMDRIVERDETAILS add = { sizeof(ACMDRIVERDETAILS) };
		if (!acmDriverDetails(hDriverID, &add, 0)) {
			strncpyz(mDriverName, add.szLongName, sizeof mDriverName);
			strncpyz(mDriverFilename, add.szShortName, sizeof mDriverFilename);
		}
	}
}

void VDAudioCodecW32::Shutdown() {
	mDstFormat.clear();

	if (mhStream) {
		if (mBufferHdr.fdwStatus & ACMSTREAMHEADER_STATUSF_PREPARED) {
			mBufferHdr.cbSrcLength = mInputBuffer.size();
			mBufferHdr.cbDstLength = mOutputBuffer.size();
			acmStreamUnprepareHeader(mhStream, &mBufferHdr, 0);
		}
		acmStreamClose(mhStream, 0);
		mhStream = NULL;
	}

	mDriverName[0] = 0;
	mDriverFilename[0] = 0;
}

void *VDAudioCodecW32::LockInputBuffer(unsigned& bytes) {
	unsigned space = mInputBuffer.size() - mBufferHdr.cbSrcLength;

	bytes = space;
	return &mInputBuffer[mBufferHdr.cbSrcLength];
}

void VDAudioCodecW32::UnlockInputBuffer(unsigned bytes) {
	mBufferHdr.cbSrcLength += bytes;
}

void VDAudioCodecW32::Restart() {
	mBufferHdr.cbSrcLength = 0;
	mBufferHdr.cbDstLengthUsed = 0;
	mbFirst	= true;
	mbFlushing = false;
	mbEnded = false;
	mOutputReadPt = 0;
}

bool VDAudioCodecW32::Convert(bool flush, bool requireOutput) {
	if (mOutputReadPt < mBufferHdr.cbDstLengthUsed)
		return true;

	if (mbEnded)
		return false;

	mBufferHdr.cbSrcLengthUsed = 0;
	mBufferHdr.cbDstLengthUsed = 0;

	const bool isCompression = mDstFormat->wFormatTag == WAVE_FORMAT_PCM;

	if (mBufferHdr.cbSrcLength || flush) {
		vdprotected2(isCompression ? "decompressing audio" : "compressing audio", const char *, mDriverName, const char *, mDriverFilename) {
			DWORD flags = ACM_STREAMCONVERTF_BLOCKALIGN;

			if (mbFlushing)
				flags = ACM_STREAMCONVERTF_END;

			if (mbFirst)
				flags |= ACM_STREAMCONVERTF_START;

			if (MMRESULT res = acmStreamConvert(mhStream, &mBufferHdr, flags))
				throw MyError(
					isCompression
						? "ACM reported error on audio compress (%lx)"
						: "ACM reported error on audio decompress (%lx)"
					, res);

			mbFirst = false;
		}

		// If the codec didn't do anything....
		if (!mBufferHdr.cbSrcLengthUsed && !mBufferHdr.cbDstLengthUsed) {
			if (flush) {
				if (mbFlushing)
					mbEnded = true;
				else
					mbFlushing = true;
			} else if (requireOutput) {
				// Check for a jam condition to try to trap that damned 9995 frame
				// hang problem.
				const WAVEFORMATEX& wfsrc = *mSrcFormat;
				const WAVEFORMATEX& wfdst = *mDstFormat;

				throw MyError("The operation cannot continue as the target audio codec has jammed and is not compressing data.\n"
								"Codec state for driver \"%.64s\":\n"
								"    source buffer size: %d bytes\n"
								"    destination buffer size: %d bytes\n"
								"    source format: tag %04x, %dHz/%dch/%d-bit, %d bytes/sec\n"
								"    destination format: tag %04x, %dHz/%dch/%d-bit, %d bytes/sec\n"
								, mDriverName
								, mBufferHdr.cbSrcLength
								, mBufferHdr.cbDstLength
								, wfsrc.wFormatTag, wfsrc.nSamplesPerSec, wfsrc.nChannels, wfsrc.wBitsPerSample, wfsrc.nAvgBytesPerSec
								, wfdst.wFormatTag, wfdst.nSamplesPerSec, wfdst.nChannels, wfdst.wBitsPerSample, wfdst.nAvgBytesPerSec);
			}
		}
	}

	mOutputReadPt = 0;

	// if ACM didn't use all the source data, copy the remainder down
	if (mBufferHdr.cbSrcLengthUsed < mBufferHdr.cbSrcLength) {
		long left = mBufferHdr.cbSrcLength - mBufferHdr.cbSrcLengthUsed;

		memmove(&mInputBuffer.front(), &mInputBuffer[mBufferHdr.cbSrcLengthUsed], left);

		mBufferHdr.cbSrcLength = left;
	} else
		mBufferHdr.cbSrcLength = 0;

	return mBufferHdr.cbSrcLengthUsed || mBufferHdr.cbDstLengthUsed;
}

const void *VDAudioCodecW32::LockOutputBuffer(unsigned& bytes) {
	bytes = mBufferHdr.cbDstLengthUsed - mOutputReadPt;
	return mOutputBuffer.data() + mOutputReadPt;
}

void VDAudioCodecW32::UnlockOutputBuffer(unsigned bytes) {
	mOutputReadPt += bytes;
	VDASSERT(mOutputReadPt <= mBufferHdr.cbDstLengthUsed);
}

unsigned VDAudioCodecW32::GetOutputLevel() {
	return mBufferHdr.cbDstLengthUsed - mOutputReadPt;
}

unsigned VDAudioCodecW32::CopyOutput(void *dst, unsigned bytes) {
	bytes = std::min<unsigned>(bytes, mBufferHdr.cbDstLengthUsed - mOutputReadPt);

	if (dst)
		memcpy(dst, &mOutputBuffer[mOutputReadPt], bytes);

	mOutputReadPt += bytes;
	return bytes;
}
