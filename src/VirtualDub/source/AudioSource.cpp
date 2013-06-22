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
#include <vd2/system/file.h>
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
		kVDM_VBRAudioDetected,
		kVDM_MP3BitDepthFixed,
		kVDM_TruncatedCompressedFormatFixed
	};
}

// TODO: Merge this with defs from AVIOutputWAV.cpp.
namespace
{
	static const uint8 kGuidRIFF[16]={
		// {66666972-912E-11CF-A5D6-28DB04C10000}
		0x72, 0x69, 0x66, 0x66, 0x2E, 0x91, 0xCF, 0x11, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00
	};

	static const uint8 kGuidLIST[16]={
		// {7473696C-912E-11CF-A5D6-28DB04C10000}
		0x6C, 0x69, 0x73, 0x74, 0x2E, 0x91, 0xCF, 0x11, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00
	};

	static const uint8 kGuidWAVE[16]={
		// {65766177-ACF3-11D3-8CD1-00C04F8EDB8A}
		0x77, 0x61, 0x76, 0x65, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A
	};

	static const uint8 kGuidfmt[16]={
		// {20746D66-ACF3-11D3-8CD1-00C04F8EDB8A}
		0x66, 0x6D, 0x74, 0x20, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A
	};

	static const uint8 kGuidfact[16]={
		// {74636166-ACF3-11D3-8CD1-00C04F8EDB8A}
		0x66, 0x61, 0x63, 0x74, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A
	};

	static const uint8 kGuiddata[16]={
		// {61746164-ACF3-11D3-8CD1-00C04F8EDB8A}
		0x64, 0x61, 0x74, 0x61, 0xF3, 0xAC, 0xD3, 0x11, 0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A
	};
}

//////////////////////////////////////////////////////////////////////////////

class AudioSourceWAV : public AudioSource {
public:
	AudioSourceWAV(const wchar_t *fn, uint32 inputBufferSize);
	~AudioSourceWAV();

	bool init();
	virtual int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lSamplesRead, uint32 *lBytesRead);

private:
	void ParseWAVE();
	void ParseWAVE64();

	sint64			mDataStart;
	sint64			mDataLength;

	VDFileStream	mFile;
	VDBufferedStream	mBufferedFile;
	VDPosition		mCurrentSample;
	uint32			mBytesPerSample;
};

AudioSource *VDCreateAudioSourceWAV(const wchar_t *fn, uint32 inputBufferSize) {
	return new AudioSourceWAV(fn, inputBufferSize);
}

AudioSourceWAV::AudioSourceWAV(const wchar_t *szFile, uint32 inputBufferSize)
	: mBufferedFile(&mFile, inputBufferSize)
{
	mFile.open(szFile);
}

AudioSourceWAV::~AudioSourceWAV() {
}

bool AudioSourceWAV::init() {
	// Read the first 12 bytes of the file. They must always be RIFF <size> WAVE for a WAVE
	// file. We deliberately ignore the length of the RIFF and only use the length of the data
	// chunk.
	uint32 ckinfo[10];

	mBufferedFile.Read(ckinfo, 12);
	if (ckinfo[0] == mmioFOURCC('R', 'I', 'F', 'F') && ckinfo[2] == mmioFOURCC('W', 'A', 'V', 'E')) {
		ParseWAVE();
		goto ok;
	} else if (ckinfo[0] == mmioFOURCC('r', 'i', 'f', 'f')) {
		mBufferedFile.Read(ckinfo+3, 40 - 12);

		if (!memcmp(ckinfo, kGuidRIFF, 16) && !memcmp(ckinfo + 6, kGuidWAVE, 16)) {
			ParseWAVE64();
			goto ok;
		}
	}

	throw MyError("\"%ls\" is not a WAVE file.", mBufferedFile.GetNameForError());

ok:
	mBytesPerSample	= getWaveFormat()->nBlockAlign; //getWaveFormat()->nAvgBytesPerSec / getWaveFormat()->nSamplesPerSec;
	mSampleFirst	= 0;
	mSampleLast		= mDataLength / mBytesPerSample;
	mCurrentSample	= -1;

	streamInfo.fccType					= streamtypeAUDIO;
	streamInfo.fccHandler				= 0;
	streamInfo.dwFlags					= 0;
	streamInfo.wPriority				= 0;
	streamInfo.wLanguage				= 0;
	streamInfo.dwInitialFrames			= 0;
	streamInfo.dwScale					= mBytesPerSample;
	streamInfo.dwRate					= getWaveFormat()->nAvgBytesPerSec;
	streamInfo.dwStart					= 0;
	streamInfo.dwLength					= (DWORD)mSampleLast;
	streamInfo.dwSuggestedBufferSize	= 0;
	streamInfo.dwQuality				= 0xffffffff;
	streamInfo.dwSampleSize				= mBytesPerSample;

	return true;
}

void AudioSourceWAV::ParseWAVE() {
	// iteratively open chunks
	static const uint32 kFoundFormat = 1;
	static const uint32 kFoundData = 2;

	uint32 notFoundYet = kFoundFormat | kFoundData;
	while(notFoundYet != 0) {
		uint32 ckinfo[2];

		// read chunk and chunk id
		if (8 != mBufferedFile.ReadData(ckinfo, 8))
			throw MyError("\"%ls\" is incomplete and could not be opened as a WAVE file.", mBufferedFile.GetNameForError());

		uint32 size = ckinfo[1];
		uint32 sizeToSkip = (size + 1) & ~1;	// RIFF chunks are dword aligned.

		switch(ckinfo[0]) {
			case mmioFOURCC('f', 'm', 't', ' '):
				if (size > 0x100000)
					throw MyError("\"%ls\" contains a format block that is too large (%u bytes).", mBufferedFile.GetNameForError(), size);
				if (!allocFormat(size))
					throw MyMemoryError();
				mBufferedFile.Read((char *)getWaveFormat(), size);
				sizeToSkip -= size;
				notFoundYet &= ~kFoundFormat;
				break;

			case mmioFOURCC('d', 'a', 't', 'a'):
				mDataStart = mBufferedFile.Pos();

				// truncate length if it extends beyond file
				mDataLength = std::min<sint64>(size, mBufferedFile.Length() - mDataStart);
				notFoundYet &= ~kFoundData;
				break;

			case mmioFOURCC('L', 'I', 'S', 'T'):
				if (size < 4)
					throw MyError("\"%ls\" contains a structural error at position %08llx and cannot be loaded.", mBufferedFile.GetNameForError(), mBufferedFile.Pos() - 8);
				sizeToSkip = 4;
				break;
		}

		mBufferedFile.Skip(sizeToSkip);
	}
}

void AudioSourceWAV::ParseWAVE64() {
	// iteratively open chunks
	static const uint32 kFoundFormat = 1;
	static const uint32 kFoundData = 2;

	uint32 notFoundYet = kFoundFormat | kFoundData;
	while(notFoundYet != 0) {
		struct {
			uint8 guid[16];
			uint64 size;
		} ck;

		// read chunk and chunk id
		if (24 != mBufferedFile.ReadData(&ck, 24))
			break;

		// unlike RIFF, WAVE64 includes the chunk header in the chunk size.
		if (ck.size < 24)
			throw MyError("\"%ls\" contains a structural error at position %08llx and cannot be loaded.", mBufferedFile.GetNameForError(), mBufferedFile.Pos() - 8);

		sint64 sizeToSkip = (ck.size + 7 - 24) & ~7;		// WAVE64 chunks are 8-byte aligned.

		if (!memcmp(ck.guid, kGuidfmt, 16)) {
			if (ck.size > 0x100000)
				throw MyError("\"%ls\" contains a format block that is too large (%llu bytes).", mBufferedFile.GetNameForError(), (unsigned long long)ck.size);

			if (!allocFormat((uint32)ck.size - 24))
				throw MyMemoryError();

			mBufferedFile.Read((char *)getWaveFormat(), (uint32)ck.size - 24);
			sizeToSkip -= (uint32)ck.size - 24;
			notFoundYet &= ~kFoundFormat;
		} else if (!memcmp(ck.guid, kGuiddata, 16)) {
			mDataStart = mBufferedFile.Pos();

			// truncate length if it extends beyond file
			mDataLength = std::min<sint64>(ck.size - 24, mBufferedFile.Length() - mDataStart);
		} else if (!memcmp(ck.guid, kGuidLIST, 16)) {
			sizeToSkip = 8;
		}

		mBufferedFile.Skip(sizeToSkip);
	}
}

int AudioSourceWAV::_read(VDPosition lStart, uint32 lCount, void *buffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead) {
	uint32 bytes = lCount * mBytesPerSample;

	if (bytes > cbBuffer) {
		bytes = cbBuffer - cbBuffer % mBytesPerSample;
		lCount = bytes / mBytesPerSample;
	}
	
	if (buffer) {
		if (lStart != mCurrentSample)
			mBufferedFile.Seek(mDataStart + mBytesPerSample*lStart);

		mBufferedFile.Read(buffer, bytes);

		mCurrentSample = lStart + lCount;
	}

	*lSamplesRead = lCount;
	*lBytesRead = bytes;

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

	// Check for invalid (truncated) MP3 format.
	WAVEFORMATEX *pwfex = getWaveFormat();

	if (pwfex->wFormatTag == WAVE_FORMAT_MPEGLAYER3) {
		if (format_len < sizeof(MPEGLAYER3WAVEFORMAT)) {
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

			pwfex = getWaveFormat();
			memcpy(pwfex, &wf, sizeof wf);

			const int bad_len = format_len;
			const int good_len = sizeof(MPEGLAYER3WAVEFORMAT);
			VDLogAppMessage(kVDLogWarning, kVDST_AudioSource, kVDM_TruncatedMP3FormatFixed, 2, &bad_len, &good_len);
		}

		// Check if the wBitsPerSample tag is something other than zero, and reset it
		// if so.
		if (pwfex->wBitsPerSample != 0) {
			pwfex->wBitsPerSample = 0;

			VDLogAppMessage(kVDLogWarning, kVDST_AudioSource, kVDM_MP3BitDepthFixed, 0);
		}
	} else {
		uint32 cbSize = 0;

		if (format_len >= sizeof(WAVEFORMATEX))
			cbSize = pwfex->cbSize;

		uint32 requiredFormatSize = sizeof(WAVEFORMATEX) + cbSize;
		if ((uint32)format_len < requiredFormatSize && pwfex->wFormatTag != WAVE_FORMAT_PCM) {
			vdstructex<WAVEFORMATEX> newFormat(requiredFormatSize);
			memset(newFormat.data(), 0, requiredFormatSize);
			memcpy(newFormat.data(), pwfex, format_len);

			if (!allocFormat(requiredFormatSize))
				return FALSE;

			pwfex = getWaveFormat();
			memcpy(pwfex, &*newFormat, requiredFormatSize);

			const int bad_len = format_len;
			const int good_len = sizeof(WAVEFORMATEX) + cbSize;
			VDLogAppMessage(kVDLogWarning, kVDST_AudioSource, kVDM_TruncatedCompressedFormatFixed, 2, &bad_len, &good_len);
		}
	}

	if (!bQuiet) {
		double mean, stddev, maxdev;

		if (pAVIStream->getVBRInfo(mean, stddev, maxdev)) {
			double meanOut = mean*0.001;
			double stddevOut = stddev*0.001;
			double maxdevOut = maxdev*1000.0;

			VDLogAppMessage(kVDLogWarning, kVDST_AudioSource, kVDM_VBRAudioDetected, 3, &maxdevOut, &meanOut, &stddevOut);
		}
	}

	return TRUE;
}

void AudioSourceAVI::Reinit() {
	pAVIStream->Info(&streamInfo, sizeof streamInfo);
	mSampleFirst = pAVIStream->Start();
	mSampleLast = pAVIStream->End();
}

int AudioSourceAVI::GetPreloadSamples() {
	long bytes, samples;
	if (pAVIStream->Read(mSampleFirst, AVISTREAMREAD_CONVENIENT, NULL, 0, &bytes, &samples))
		return 0;

	sint64 astart = pAVIStream->getSampleBytePosition(mSampleFirst);

	if (astart < 0)
		return (int)samples;

	IAVIReadStream *pVS = pAVIFile->GetStream(streamtypeVIDEO, 0);
	if (!pVS)
		return (int)samples;

	sint64 vstart = pVS->getSampleBytePosition(pVS->Start());
	delete pVS;

	if (vstart >= 0 && vstart < astart)
		return 0;

	return (int)samples;
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

VDPosition AudioSourceAVI::TimeToPositionVBR(VDTime us) const {
	return pAVIStream->TimeToPosition(us);
}

VDTime AudioSourceAVI::PositionToTimeVBR(VDPosition samples) const {
	return pAVIStream->PositionToTime(samples);
}

bool AudioSourceAVI::IsVBR() const {
	double bitrate_mean, bitrate_stddev, maxdev;
	return pAVIStream->getVBRInfo(bitrate_mean, bitrate_stddev, maxdev);
}

///////////////////////////////////////////////////////////////////////////

AudioSourceDV::AudioSourceDV(IAVIReadStream *pStream, bool bAutomated)
	: mpStream(pStream)
	, mLastFrame(-1)
	, mErrorMode(kErrorModeReportAll)
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
		break;
	case 0x08:
		samplingRate = 44100;
		mSamplesPerSet		= isPAL ? 17640 : 14715;
		break;
	case 0x10:
		samplingRate = 32000;
		mSamplesPerSet		= isPAL ? 12800 : 10677;
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

	mGatherTab.resize(1960);
	memset(mGatherTab.data(), 0, mGatherTab.size() * sizeof mGatherTab[0]);

	if (isPAL) {
		for(int i=0; i<1944; ++i) {
			int dif_sequence	= ((i/3)+2*(i%3))%6;
			int dif_block		= 6 + 16*(3*(i%3) + ((i%54)/18));
			int byte_offset		= 8 + bytesPerSample*(i/54);

			mGatherTab[i] = 12000*dif_sequence + 80*dif_block + byte_offset;
		}

		mRightChannelOffset = 12000*6;	// left channel is first 6 DIF sequences
	} else {
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

	VDPosition len(mSampleLast - mSampleFirst);
	streamInfo.dwLength	= (uint32)len == len ? (uint32)len : 0xFFFFFFFF;

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
		uint32 setSize = mSamplesPerSet;
		VDPosition pos = mRawStart + 10 * setpos;
		VDPosition limit = pos + 10;
		if (limit > mRawEnd) {
			limit = mRawEnd;

			setSize = mSamplesPerSet * ((sint32)limit - (sint32)pos) / 10;
		}
		cline.mRawSamples = 0;

		uint8 *dst = (uint8 *)cline.mRawData;

		while(pos < limit) {
			long bytes, samples;

			int err = mpStream->Read(pos++, 1, mTempBuffer.data(), mTempBuffer.size(), &bytes, &samples);
			if (err)
				return NULL;

			if (!bytes) {
zero_fill:
				uint32 n = mSamplesPerSet / 10;

				if (cline.mRawSamples+n >= sizeof cline.mRawData / sizeof cline.mRawData[0]) {
					VDDEBUG("AudioSourceDV: Sample count overflow!\n");
					VDASSERT(false);
					break;
				}

				cline.mRawSamples += n;

				memset(dst, 0, n*4);
				dst += n*4;
			} else {
				if ((uint32)bytes != mTempBuffer.size())
					return NULL;

				const uint8 *pAAUX = &mTempBuffer[80*(1*150 + 6) + 3];

				uint8 vaux_vs_pc3 = mTempBuffer[80*(1*150 + 3) + 6];		// DIF sequence 1, block 3, VAUX pack 0

				bool isPAL = 0 != (vaux_vs_pc3 & 0x20);

				uint32 minimumFrameSize;

				switch(pAAUX[4] & 0x38) {
				case 0x00:
					minimumFrameSize	= isPAL ? 1896 : 1580;
					break;
				case 0x08:
					minimumFrameSize	= isPAL ? 1742 : 1452;
					break;
				case 0x10:
					minimumFrameSize	= isPAL ? 1264 : 1053;
					break;
				default:
					if (mErrorMode != kErrorModeReportAll)
						goto zero_fill;

					return NULL;
				}

				const uint32 n = minimumFrameSize + (pAAUX[1] & 0x3f);

				if (cline.mRawSamples+n >= sizeof cline.mRawData / sizeof cline.mRawData[0]) {
					VDDEBUG("AudioSourceDV: Sample count overflow!\n");
					VDASSERT(false);
					break;
				}

				cline.mRawSamples += n;

				if ((pAAUX[4] & 7) == 1) {	// 1: 12-bit nonlinear
					const uint8 *src0 = (const uint8 *)mTempBuffer.data();
					sint32 *pOffsets = mGatherTab.data();

					sint16 *dst16 = (sint16 *)dst;
					dst += 4*n;

					for(int i=0; i<n; ++i) {
						const ptrdiff_t pos = *pOffsets++;
						const uint8 *srcF = src0 + pos;

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

						dst16[0] = (sint16)((vL << shift[expL]) + addend[expL]);
						dst16[1] = (sint16)((vR << shift[expR]) + addend[expR]);
						dst16 += 2;
					}
				} else {					// 0: 16-bit linear
					const uint8 *src0 = (const uint8 *)mTempBuffer.data();
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
		}

		// resample if required
		if (cline.mRawSamples == setSize) {
			// no resampling required -- straight copy
			memcpy(cline.mResampledData, cline.mRawData, 4*setSize);
		} else {
			const sint16 *src = &cline.mRawData[0][0];
			      sint16 *dst = &cline.mResampledData[0][0];

			// copy first sample
			dst[0] = src[0];
			dst[1] = src[1];
			dst += 2;

			// linearly interpolate middle samples
			uint32 dudx = ((cline.mRawSamples-1) << 16) / (setSize-1);
			uint32 u = dudx + (dudx >> 1) - 0x8000;

			VDASSERT((sint32)u >= 0);

			unsigned n = setSize-2;
			do {
				const sint16 *src2 = src + (u>>16)*2;
				sint32 f = (u>>4)&0xfff;

				u += dudx;

				sint32 a0 = src2[0];
				sint32 b0 = src2[1];
				sint32 da = src2[2] - a0;
				sint32 db = src2[3] - b0;

				dst[0] = (sint16)(a0 + ((da*f + 0x800) >> 12));
				dst[1] = (sint16)(b0 + ((db*f + 0x800) >> 12));
				dst += 2;
			} while(--n);

			VDASSERT(((u - dudx) >> 16) < cline.mRawSamples);

			// copy last sample
			dst[0] = src[cline.mRawSamples*2-2];
			dst[1] = src[cline.mRawSamples*2-1];
		}

		VDDEBUG("AudioSourceDV: Loaded cache line %u for %u raw samples (%u samples expected)\n", (unsigned)setpos*10, cline.mRawSamples, setSize);

		mCacheLinePositions[line] = setpos;
	}

	return &cline;
}

void AudioSourceDV::FlushCache() {
	for(int i=0; i<kCacheLines; ++i)
		mCacheLinePositions[i] = -1;
}
