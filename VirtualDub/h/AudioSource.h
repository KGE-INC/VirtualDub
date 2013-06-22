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

#ifndef f_AUDIOSOURCE_H
#define f_AUDIOSOURCE_H

#include <windows.h>
#include <vfw.h>
#include <vector>
#include <vd2/system/vdstl.h>

#include "DubSource.h"

class IAVIReadHandler;
class IAVIReadStream;

class AudioSource : public DubSource {
public:
	WAVEFORMATEX *getWaveFormat() {
		return (WAVEFORMATEX *)getFormat();
	}
};

class AudioSourceWAV : public AudioSource {
private:
	HMMIO				hmmioFile;
	MMCKINFO			chunkRIFF;
	MMCKINFO			chunkDATA;
	LONG				lCurrentSample;
	LONG				bytesPerSample;

	~AudioSourceWAV();

public:
	AudioSourceWAV(const wchar_t *fn, LONG inputBufferSize);

	bool init();
	virtual int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lSamplesRead, uint32 *lBytesRead);
};

class AudioSourceAVI : public AudioSource {
private:
	IAVIReadHandler *pAVIFile;
	IAVIReadStream *pAVIStream;
	bool bQuiet;

	bool _isKey(VDPosition lSample);

	~AudioSourceAVI();

public:
	AudioSourceAVI(IAVIReadHandler *pAVIFile, bool bAutomated);

	void setRate(const VDFraction& f) { streamInfo.dwRate = f.getHi(); streamInfo.dwScale = f.getLo(); }

	void Reinit();
	bool isStreaming();

	void streamBegin(bool fRealTime);
	void streamEnd();

	bool init();
	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lSamplesRead, uint32 *lBytesRead);
};

class AudioSourceDV : public AudioSource {
public:
	AudioSourceDV(IAVIReadStream *pAVIStream, bool bAutomated);

	void Reinit();
	bool isStreaming();

	void streamBegin(bool fRealTime);
	void streamEnd();

	bool init();
	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lSamplesRead, uint32 *lBytesRead);

protected:
	~AudioSourceDV();

	struct CacheLine;

	const CacheLine *LoadSet(VDPosition set);
	void FlushCache();
	bool _isKey(VDPosition lSample);

	vdblock<uint8> mTempBuffer;

	IAVIReadStream *mpStream;
	bool bQuiet;
	sint32	mSamplesPerSet;
	sint32	mMinimumFrameSize;
	sint32	mRightChannelOffset;
	VDPosition mLastFrame;
	VDPosition	mRawFrames;
	VDPosition	mRawStart;
	VDPosition	mRawEnd;

	vdblock<sint32>	mGatherTab;

	enum { kCacheLines = 4 };

	VDPosition	mCacheLinePositions[kCacheLines];

	struct CacheLine {
		sint16	mRawData[1959*10][2];
		sint16	mResampledData[1920*10][2];
		uint32	mRawSamples;
	} mCache[kCacheLines];

};

#endif
