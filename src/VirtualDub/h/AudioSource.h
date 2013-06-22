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

	virtual bool IsVBR() const { return false; }
};

AudioSource *VDCreateAudioSourceWAV(const wchar_t *fn, uint32 inputBufferSize);

class VDAudioSourceAVISourced : public AudioSource{
public:
	virtual void Reinit() = 0;
	virtual int GetPreloadSamples() = 0;
};

class AudioSourceAVI : public VDAudioSourceAVISourced {
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
	int GetPreloadSamples();
	bool isStreaming();

	void streamBegin(bool fRealTime, bool bForceReset);
	void streamEnd();

	bool init();
	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lSamplesRead, uint32 *lBytesRead);

	virtual VDPosition TimeToPositionVBR(VDTime us) const;
	virtual VDTime PositionToTimeVBR(VDPosition samples) const;
	virtual bool IsVBR() const;
};

class AudioSourceDV : public VDAudioSourceAVISourced {
public:
	AudioSourceDV(IAVIReadStream *pAVIStream, bool bAutomated);

	void Reinit();
	int GetPreloadSamples() { return 0; }
	bool isStreaming();

	void streamBegin(bool fRealTime, bool bForceReset);
	void streamEnd();

	ErrorMode getDecodeErrorMode() { return mErrorMode; }
	void setDecodeErrorMode(ErrorMode mode) { mErrorMode = mode; }
	bool isDecodeErrorModeSupported(ErrorMode mode) { return mode == kErrorModeConceal || mode == kErrorModeReportAll; }

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
	ErrorMode	mErrorMode;
	uint32	mSamplesPerSet;
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
