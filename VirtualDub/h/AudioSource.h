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
	virtual int _read(LONG lStart, LONG lCount, LPVOID lpBuffer, LONG cbBuffer, LONG *lSamplesRead, LONG *lBytesRead);
};

class AudioSourceAVI : public AudioSource {
private:
	IAVIReadHandler *pAVIFile;
	IAVIReadStream *pAVIStream;
	bool bQuiet;

	BOOL _isKey(LONG lSample);

	~AudioSourceAVI();

public:
	AudioSourceAVI(IAVIReadHandler *pAVIFile, bool bAutomated);

	void setRate(const VDFraction& f) { streamInfo.dwRate = f.getHi(); streamInfo.dwScale = f.getLo(); }

	void Reinit();
	bool isStreaming();

	void streamBegin(bool fRealTime);
	void streamEnd();

	bool init();
	int _read(LONG lStart, LONG lCount, LPVOID lpBuffer, LONG cbBuffer, LONG *lSamplesRead, LONG *lBytesRead);
};

#endif
