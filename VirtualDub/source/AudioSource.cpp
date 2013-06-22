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
#include <vd2/system/Error.h>
#include <vd2/Dita/resources.h>

#include "gui.h"
#include "AudioSource.h"
#include "AVIReadHandler.h"

//////////////////////////////////////////////////////////////////////////////

extern HWND g_hWnd;		// TODO: Remove in 1.5.0

//////////////////////////////////////////////////////////////////////////////

namespace {
	enum { kVDST_AudioSource = 6 };

	enum {
		kVDM_TruncatedMP3FormatFixed,
	};
}

//////////////////////////////////////////////////////////////////////////////

AudioSourceWAV::AudioSourceWAV(const wchar_t *szFile, LONG inputBufferSize)
{
	MMIOINFO mmi;
	VDStringA filenameA(VDTextWToA(szFile));

	memset(&mmi,0,sizeof mmi);
	mmi.cchBuffer	= inputBufferSize;
	hmmioFile		= mmioOpen((char *)filenameA.c_str(), &mmi, MMIO_READ | MMIO_ALLOCBUF);

	if (!hmmioFile) {
		const char *pError = "An unknown error has occurred";
		switch(mmi.wErrorRet) {
		case MMIOERR_FILENOTFOUND:		pError = "File not found"; break;
		case MMIOERR_OUTOFMEMORY:		pError = "Out of memory"; break;
		case MMIOERR_ACCESSDENIED:		pError = "Access is denied"; break;
		case MMIOERR_CANNOTOPEN:		// fall through
		case MMIOERR_INVALIDFILE:		pError = "The file could not be opened"; break;
		case MMIOERR_NETWORKERROR:		pError = "A network error occurred while opening the file"; break;
		case MMIOERR_PATHNOTFOUND:		pError = "The file path does not exist"; break;
		case MMIOERR_SHARINGVIOLATION:	pError = "The file is currently in use"; break;
		case MMIOERR_TOOMANYOPENFILES:	pError = "Too many files are already open"; break;
		}
		throw MyError("Cannot open \"%s\": %s.", filenameA.c_str(), pError);
	}
}

AudioSourceWAV::~AudioSourceWAV() {
	mmioClose(hmmioFile, 0);
}

bool AudioSourceWAV::init() {
	chunkRIFF.fccType = mmioFOURCC('W','A','V','E');
	if (MMSYSERR_NOERROR != mmioDescend(hmmioFile, &chunkRIFF, NULL, MMIO_FINDRIFF))
		return FALSE;

	chunkDATA.ckid = mmioFOURCC('f','m','t',' ');
	if (MMSYSERR_NOERROR != mmioDescend(hmmioFile, &chunkDATA, &chunkRIFF, MMIO_FINDCHUNK))
		return FALSE;

	if (!allocFormat(chunkDATA.cksize)) return FALSE;
	if (chunkDATA.cksize != mmioRead(hmmioFile, (char *)getWaveFormat(), chunkDATA.cksize))
		return FALSE;

	if (MMSYSERR_NOERROR != mmioAscend(hmmioFile, &chunkDATA, 0))
		return FALSE;

	chunkDATA.ckid = mmioFOURCC('d','a','t','a');
	if (MMSYSERR_NOERROR != mmioDescend(hmmioFile, &chunkDATA, &chunkRIFF, MMIO_FINDCHUNK))
		return FALSE;

	bytesPerSample	= getWaveFormat()->nBlockAlign; //getWaveFormat()->nAvgBytesPerSec / getWaveFormat()->nSamplesPerSec;
	mSampleFirst	= 0;
	mSampleLast		= chunkDATA.cksize / bytesPerSample;
	lCurrentSample	= -1;

	streamInfo.fccType					= streamtypeAUDIO;
	streamInfo.fccHandler				= 0;
	streamInfo.dwFlags					= 0;
	streamInfo.wPriority				= 0;
	streamInfo.wLanguage				= 0;
	streamInfo.dwInitialFrames			= 0;
	streamInfo.dwScale					= bytesPerSample;
	streamInfo.dwRate					= getWaveFormat()->nAvgBytesPerSec;
	streamInfo.dwStart					= 0;
	streamInfo.dwLength					= chunkDATA.cksize / bytesPerSample;
	streamInfo.dwSuggestedBufferSize	= 0;
	streamInfo.dwQuality				= 0xffffffff;
	streamInfo.dwSampleSize				= bytesPerSample;

	return TRUE;
}

int AudioSourceWAV::_read(VDPosition lStart, uint32 lCount, void *buffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead) {
	LONG lBytes = lCount * bytesPerSample;

	if (lBytes > cbBuffer) {
		lBytes = cbBuffer - cbBuffer % bytesPerSample;
		lCount = lBytes / bytesPerSample;
	}
	
	if (buffer) {
		if (lStart != lCurrentSample)
			if (-1 == mmioSeek(hmmioFile, chunkDATA.dwDataOffset + bytesPerSample*lStart, SEEK_SET))
				return AVIERR_FILEREAD;

		if (lBytes != mmioRead(hmmioFile, (char *)buffer, lBytes))
			return AVIERR_FILEREAD;

		lCurrentSample = lStart + lCount;
	}

	*lSamplesRead = lCount;
	*lBytesRead = lBytes;

	return AVIERR_OK;
}

///////////////////////////

AudioSourceAVI::AudioSourceAVI(IAVIReadHandler *pAVI, bool bAutomated) {
	pAVIFile	= pAVI;
	pAVIStream	= NULL;
	bQuiet = bAutomated;	// ugh, this needs to go... V1.5.0.
}

AudioSourceAVI::~AudioSourceAVI() {
	if (pAVIStream)
		delete pAVIStream;
}

bool AudioSourceAVI::init() {
	LONG format_len;

	pAVIStream = pAVIFile->GetStream(streamtypeAUDIO, 0);
	if (!pAVIStream) return FALSE;

	if (pAVIStream->Info(&streamInfo, sizeof streamInfo))
		return FALSE;

	pAVIStream->FormatSize(0, &format_len);

	if (!allocFormat(format_len)) return FALSE;

	if (pAVIStream->ReadFormat(0, getFormat(), &format_len))
		return FALSE;

	mSampleFirst = pAVIStream->Start();
	mSampleLast = pAVIStream->End();

	// Check for illegal (truncated) MP3 format.
	const WAVEFORMATEX *pwfex = getWaveFormat();

	if (pwfex->wFormatTag == WAVE_FORMAT_MPEGLAYER3 && format_len < sizeof(MPEGLAYER3WAVEFORMAT)) {
		MPEGLAYER3WAVEFORMAT wf;

		wf.wfx				= *pwfex;
		wf.wfx.cbSize		= MPEGLAYER3_WFX_EXTRA_BYTES;

		wf.wID				= MPEGLAYER3_ID_MPEG;

		// Attempt to detect the padding mode and block size for the stream.

		double byterate = wf.wfx.nAvgBytesPerSec;
		double fAverageFrameSize = 1152.0 * byterate / wf.wfx.nSamplesPerSec;

		int estimated_bitrate = (int)floor(0.5 + byterate * (1.0/1000.0)) * 8;
		double fEstimatedFrameSizeISO = 144000.0 * estimated_bitrate / wf.wfx.nSamplesPerSec;

		if (wf.wfx.nSamplesPerSec < 32000) {	// MPEG-2?
			fAverageFrameSize *= 0.5;
			fEstimatedFrameSizeISO *= 0.5;
		}

		double fEstimatedFrameSizePaddingOff = floor(fEstimatedFrameSizeISO);
		double fEstimatedFrameSizePaddingOn = fEstimatedFrameSizePaddingOff + 1.0;
		double fErrorISO = fabs(fEstimatedFrameSizeISO - fAverageFrameSize);
		double fErrorPaddingOn = fabs(fEstimatedFrameSizePaddingOn - fAverageFrameSize);
		double fErrorPaddingOff = fabs(fEstimatedFrameSizePaddingOff - fAverageFrameSize);

		if (fErrorISO <= fErrorPaddingOn)				// iso < on
			if (fErrorISO <= fErrorPaddingOff)			// iso < on, off
				wf.fdwFlags			= MPEGLAYER3_FLAG_PADDING_ISO;
			else										// off < iso < on
				wf.fdwFlags			= MPEGLAYER3_FLAG_PADDING_OFF;
		else											// on < iso
			if (fErrorPaddingOn <= fErrorPaddingOff)	// on < iso, off
				wf.fdwFlags			= MPEGLAYER3_FLAG_PADDING_ON;
			else										// off < on < iso
				wf.fdwFlags			= MPEGLAYER3_FLAG_PADDING_OFF;

		wf.nBlockSize		= (uint16)floor(0.5 + fAverageFrameSize);	// This is just a guess.  The MP3 codec always turns padding off, so I don't know whether this should be rounded up or not.
		wf.nFramesPerBlock	= 1;
		wf.nCodecDelay		= 1393;									// This is the number of samples padded by the compressor.  1393 is the value typically written by the codec.

		if (!allocFormat(sizeof wf))
			return FALSE;

		memcpy(getFormat(), &wf, sizeof wf);

		const int bad_len = format_len;
		const int good_len = sizeof(MPEGLAYER3WAVEFORMAT);
		VDLogAppMessage(kVDLogWarning, kVDST_AudioSource, kVDM_TruncatedMP3FormatFixed, 2, &bad_len, &good_len);
	}

	if (!bQuiet) {
		double mean, stddev, maxdev;

		if (pAVIStream->getVBRInfo(mean, stddev, maxdev)) {
			guiMessageBoxF(g_hWnd, "VBR audio stream detected", MB_OK|MB_TASKMODAL|MB_SETFOREGROUND,
				"VirtualDub has detected an improper VBR audio encoding in the source AVI file and will rewrite the audio header "
				"with standard CBR values during processing for better compatibility. This may introduce up to %.0lf ms of skew "
				"from the video stream. If this is unacceptable, decompress the *entire* audio stream to an uncompressed WAV file "
				"and recompress with a constant bitrate encoder. "
				"(bitrate: %.1lf ± %.1lf kbps)"
				,maxdev*1000.0,mean*0.001, stddev*0.001);
		}
	}

	return TRUE;
}

void AudioSourceAVI::Reinit() {
	pAVIStream->Info(&streamInfo, sizeof streamInfo);
	mSampleFirst = pAVIStream->Start();
	mSampleLast = pAVIStream->End();
}

bool AudioSourceAVI::isStreaming() {
	return pAVIStream->isStreaming();
}

void AudioSourceAVI::streamBegin(bool fRealTime, bool bForceReset) {
	pAVIStream->BeginStreaming(mSampleFirst, mSampleLast, fRealTime ? 1000 : 2000);
}

void AudioSourceAVI::streamEnd() {
	pAVIStream->EndStreaming();

}

bool AudioSourceAVI::_isKey(VDPosition lSample) {
	return pAVIStream->IsKeyFrame(lSample);
}

int AudioSourceAVI::_read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lpBytesRead, uint32 *lpSamplesRead) {
	int err;
	long lBytes, lSamples;

	// There are some video clips roaming around with truncated audio streams
	// (audio streams that state their length as being longer than they
	// really are).  We use a kludge here to get around the problem.

	err = pAVIStream->Read(lStart, lCount, lpBuffer, cbBuffer, &lBytes, &lSamples);

	if (lpBytesRead)
		*lpBytesRead = lBytes;
	if (lpSamplesRead)
		*lpSamplesRead = lSamples;

	if (err != AVIERR_FILEREAD)
		return err;

	// Suspect a truncated stream.
	//
	// AVISTREAMREAD_CONVENIENT will tell us if we're actually encountering a
	// true read error or not.  At least for the AVI handler, it returns
	// AVIERR_ERROR if we've broached the end.  

	*lpBytesRead = *lpSamplesRead = 0;

	while(lCount > 0) {
		err = pAVIStream->Read(lStart, AVISTREAMREAD_CONVENIENT, NULL, 0, &lBytes, &lSamples);

		if (err)
			return 0;

		if (!lSamples) return AVIERR_OK;

		if (lSamples > lCount) lSamples = lCount;

		err = pAVIStream->Read(lStart, lSamples, lpBuffer, cbBuffer, &lBytes, &lSamples);

		if (err)
			return err;

		lpBuffer = (LPVOID)((char *)lpBuffer + lBytes);
		cbBuffer -= lBytes;
		lCount -= lSamples;

		*lpBytesRead += lBytes;
		*lpSamplesRead += lSamples;
	}

	return AVIERR_OK;
}


///////////////////////////////////////////////////////////////////////////

AudioSourceDV::AudioSourceDV(IAVIReadStream *pStream, bool bAutomated)
	: mpStream(pStream)
	, mLastFrame(-1)
{
	bQuiet = bAutomated;	// ugh, this needs to go... V1.5.0.
}

AudioSourceDV::~AudioSourceDV() {
	if (mpStream)
		delete mpStream;
}

bool AudioSourceDV::init() {
	LONG format_len;

	mpStream->FormatSize(0, &format_len);

	if (!allocFormat(sizeof(WAVEFORMATEX)))
		return false;

	WAVEFORMATEX *pwfex = (WAVEFORMATEX *)getFormat();

	// fetch AAUX packet from stream format and determine sampling rate
	long size;
	if (FAILED(mpStream->FormatSize(0, &size)))
		return false;

	if (size < 24)
		return false;

	vdblock<uint8> format(size);
	if (FAILED(mpStream->ReadFormat(0, format.data(), &size)))
		return false;

	uint8 aaux_as_pc4 = format[3];
	uint8 vaux_vs_pc3 = format[18];

	// Sometimes the values in the DVINFO block are wrong, so attempt
	// to extract from the first frame instead.
	const VDPosition streamStart = mpStream->Start();
	const VDPosition streamEnd = mpStream->End();
	
	if (streamEnd > streamStart) {
		long bytes, samples;

		if (!mpStream->Read(streamStart, 1, NULL, 0, &bytes, &samples) && bytes >= 120000) {
			vdblock<uint8> tmp(bytes);

			if (!mpStream->Read(streamStart, 1, tmp.data(), tmp.size(), &bytes, &samples)) {
				aaux_as_pc4 = tmp[80*(3*150 + 6) + 7];		// DIF sequence 3, block 6, AAUX pack 0
				vaux_vs_pc3 = tmp[80*(1*150 + 3) + 6];		// DIF sequence 1, block 3, VAUX pack 0
			}
		}
	}

	bool isPAL = 0 != (vaux_vs_pc3 & 0x20);

	sint32 samplingRate;

	switch(aaux_as_pc4 & 0x38) {
	case 0x00:
		samplingRate = 48000;
		mSamplesPerSet		= isPAL ? 19200 : 16016;
		mMinimumFrameSize	= isPAL ? 1896 : 1580;
		break;
	case 0x08:
		samplingRate = 44100;
		mSamplesPerSet		= isPAL ? 17640 : 14715;
		mMinimumFrameSize	= isPAL ? 1742 : 1452;
		break;
	case 0x10:
		samplingRate = 32000;
		mSamplesPerSet		= isPAL ? 12800 : 10677;
		mMinimumFrameSize	= isPAL ? 1264 : 1053;
		break;
	default:
		return false;
	}

	// check for 12-bit quantization
	unsigned bytesPerSample = (aaux_as_pc4 & 7) ? 3 : 2;

	mTempBuffer.resize(isPAL ? 144000 : 120000);

	pwfex->wFormatTag		= WAVE_FORMAT_PCM;
	pwfex->nChannels		= 2;
	pwfex->nSamplesPerSec	= samplingRate;
	pwfex->nAvgBytesPerSec	= samplingRate*4;
	pwfex->nBlockAlign		= 4;
	pwfex->wBitsPerSample	= 16;
	pwfex->cbSize			= 0;

	if (isPAL) {
		mGatherTab.resize(1944);

		for(int i=0; i<1944; ++i) {
			int dif_sequence	= ((i/3)+2*(i%3))%6;
			int dif_block		= 6 + 16*(3*(i%3) + ((i%54)/18));
			int byte_offset		= 8 + bytesPerSample*(i/54);

			mGatherTab[i] = 12000*dif_sequence + 80*dif_block + byte_offset;
		}

		mRightChannelOffset = 12000*6;	// left channel is first 6 DIF sequences
	} else {
		mGatherTab.resize(1620);

		for(int i=0; i<1620; ++i) {
			int dif_sequence	= ((i/3)+2*(i%3))%5;
			int dif_block		= 6 + 16*(3*(i%3) + ((i%45)/15));
			int byte_offset		= 8 + bytesPerSample*(i/45);

			mGatherTab[i] = 12000*dif_sequence + 80*dif_block + byte_offset;
		}

		mRightChannelOffset	= 12000*5;	// left channel is first 5 DIF sequences
	}

	mpStream->Info(&streamInfo, sizeof streamInfo);

	// wonk most of the stream values since they're not appropriate for audio
	streamInfo.fccType		= streamtypeAUDIO;
	streamInfo.fccHandler	= 0;
	streamInfo.dwStart		= VDRoundToInt((double)streamInfo.dwScale / streamInfo.dwRate * samplingRate);
	streamInfo.dwRate		= pwfex->nAvgBytesPerSec;
	streamInfo.dwScale		= pwfex->nBlockAlign;
	streamInfo.dwInitialFrames			= 0;
	streamInfo.dwSuggestedBufferSize	= 0;
	streamInfo.dwQuality				= (DWORD)-1;
	streamInfo.dwSampleSize				= pwfex->nBlockAlign;
	memset(&streamInfo.rcFrame, 0, sizeof streamInfo.rcFrame);

	Reinit();

	return true;
}

void AudioSourceDV::Reinit() {
	const VDPosition start = mpStream->Start();
	const VDPosition end = mpStream->End();

	mRawStart		= start;
	mRawEnd			= end;
	mRawFrames		= end - start;
	mSampleFirst	= (start * mSamplesPerSet) / 10;
	mSampleLast		= (start+end * mSamplesPerSet) / 10;

	streamInfo.dwLength	= mSampleLast - mSampleFirst;

	FlushCache();
}

bool AudioSourceDV::isStreaming() {
	return mpStream->isStreaming();
}

void AudioSourceDV::streamBegin(bool fRealTime, bool bForceReset) {
	mpStream->BeginStreaming(mSampleFirst, mSampleLast, fRealTime ? 1000 : 2000);
}

void AudioSourceDV::streamEnd() {
	mpStream->EndStreaming();

}

bool AudioSourceDV::_isKey(VDPosition lSample) {
	return true;
}

int AudioSourceDV::_read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lpBytesRead, uint32 *lpSamplesRead) {
	if (lpBuffer && cbBuffer < 4)
		return AVIERR_BUFFERTOOSMALL;

	if (lCount == AVISTREAMREAD_CONVENIENT)
		lCount = mSamplesPerSet;

	VDPosition baseSet = lStart / mSamplesPerSet;
	uint32 offset = (uint32)(lStart % mSamplesPerSet);

	if (lCount > mSamplesPerSet - offset)
		lCount = mSamplesPerSet - offset;

	if (lpBuffer && lCount > (cbBuffer>>2))
		lCount = cbBuffer>>2;

	if (lpBuffer) {
		const CacheLine *pLine = LoadSet(baseSet);
		if (!pLine)
			return AVIERR_FILEREAD;

		memcpy(lpBuffer, (char *)pLine->mResampledData + offset*4, 4*lCount);
	}

	if (lpBytesRead)
		*lpBytesRead = lCount * 4;
	if (lpSamplesRead)
		*lpSamplesRead = lCount;

	return AVIERR_OK;
}

const AudioSourceDV::CacheLine *AudioSourceDV::LoadSet(VDPosition setpos) {
	// For now we will be lazy and just direct map the cache.
	unsigned line = (unsigned)setpos & 3;

	CacheLine& cline = mCache[line];

	if (mCacheLinePositions[line] != setpos) {
		mCacheLinePositions[line] = -1;

		// load up to 10 frames and linearize the raw data
		VDPosition pos = mRawStart + 10 * setpos;
		VDPosition limit = pos + 10;
		if (limit > mRawEnd)
			limit = mRawEnd;
		cline.mRawSamples = 0;

		uint8 *dst = (uint8 *)cline.mRawData;

		while(pos < limit) {
			long bytes, samples;

			int err = mpStream->Read(pos++, 1, mTempBuffer.data(), mTempBuffer.size(), &bytes, &samples);
			if (err)
				return NULL;

			const uint8 *pAAUX = &mTempBuffer[80*(1*150 + 6) + 3];

			const uint32 n = mMinimumFrameSize + (pAAUX[1] & 0x3f);

			if (cline.mRawSamples+n >= sizeof cline.mRawData / sizeof cline.mRawData[0]) {
				VDDEBUG("AudioSourceDV: Sample count overflow!\n");
				VDASSERT(false);
				break;
			}

			cline.mRawSamples += n;

			if ((pAAUX[4] & 7) == 1) {	// 1: 12-bit nonlinear
				const uint8 *src0 = (const uint8 *)&mTempBuffer[0];
				const ptrdiff_t rightOffset = mRightChannelOffset;
				sint32 *pOffsets = mGatherTab.data();

				uint16 *dst16 = (uint16 *)dst;
				dst += 4*n;

				for(int i=0; i<n; ++i) {
					const ptrdiff_t pos = *pOffsets++;
					const uint8 *srcF = src0 + pos;
					const uint8 *srcR = srcF + rightOffset;

					// Convert 12-bit nonlinear (one's complement floating-point) sample to 16-bit linear.
					// This value is a 1.3.8 floating-point value similar to IEEE style, except that the
					// sign is represented via 1's-c rather than sign-magnitude.
					//
					// Thus, 0000..00FF are positive denormals, 8000 is the largest negative value, etc.

					sint32 vL = ((sint32)srcF[0]<<4) + (srcF[2]>>4);		// reconstitute left 12-bit value
					sint32 vR = ((sint32)srcF[1]<<4) + (srcF[2] & 15);		// reconstitute right 12-bit value

					static const sint32 addend[16]={
						-0x0000 << 0,
						-0x0000 << 0,
						-0x0100 << 1,
						-0x0200 << 2,
						-0x0300 << 3,
						-0x0400 << 4,
						-0x0500 << 5,
						-0x0600 << 6,

						-0x09ff << 6,
						-0x0aff << 5,
						-0x0bff << 4,
						-0x0cff << 3,
						-0x0dff << 2,
						-0x0eff << 1,
						-0x0fff << 0,
						-0x0fff << 0,
					};
					static const int	shift [16]={0,0,1,2,3,4,5,6,6,5,4,3,2,1,0,0};

					int expL = vL >> 8;
					int expR = vR >> 8;

					dst16[0] = (vL << shift[expL]) + addend[expL];
					dst16[1] = (vR << shift[expR]) + addend[expR];
					dst16 += 2;
				}
			} else {					// 0: 16-bit linear
				const uint8 *src0 = (const uint8 *)&mTempBuffer[0];
				const ptrdiff_t rightOffset = mRightChannelOffset;
				sint32 *pOffsets = mGatherTab.data();

				for(int i=0; i<n; ++i) {
					const ptrdiff_t pos = *pOffsets++;
					const uint8 *srcL = src0 + pos;
					const uint8 *srcR = srcL + rightOffset;

					// convert big-endian sample to little-endian
					dst[0] = srcL[1];
					dst[1] = srcL[0];
					dst[2] = srcR[1];
					dst[3] = srcR[0];
					dst += 4;
				}
			}
		}

		// resample if required
		if (cline.mRawSamples == mSamplesPerSet) {
			// no resampling required -- straight copy
			memcpy(cline.mResampledData, cline.mRawData, 4*mSamplesPerSet);
		} else {
			const sint16 *src = &cline.mRawData[0][0];
			      sint16 *dst = &cline.mResampledData[0][0];

			// copy first sample
			dst[0] = src[0];
			dst[1] = src[1];
			dst += 2;

			// linearly interpolate middle samples
			uint32 dudx = ((cline.mRawSamples-1) << 16) / (mSamplesPerSet-1);
			uint32 u = dudx + (dudx >> 1) - 0x8000;

			unsigned n = mSamplesPerSet-2;
			do {
				const sint16 *src2 = src + (u>>16)*2;
				sint32 f = (u>>4)&0xfff;

				u += dudx;

				sint32 a0 = src2[0];
				sint32 b0 = src2[1];
				sint32 da = src2[2] - a0;
				sint32 db = src2[3] - b0;

				dst[0] = a0 + ((da*f + 0x800) >> 12);
				dst[1] = b0 + ((db*f + 0x800) >> 12);
				dst += 2;
			} while(--n);

			// copy last sample
			dst[0] = src[cline.mRawSamples*2-2];
			dst[1] = src[cline.mRawSamples*2-1];
		}

		VDDEBUG("AudioSourceDV: Loaded cache line %u for %u raw samples (%u samples expected)\n", (unsigned)setpos*10, cline.mRawSamples, mSamplesPerSet);

		mCacheLinePositions[line] = setpos;
	}

	return &cline;
}

void AudioSourceDV::FlushCache() {
	for(int i=0; i<kCacheLines; ++i)
		mCacheLinePositions[i] = -1;
}
