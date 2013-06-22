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

#include <vd2/system/error.h>

#include "AVIOutputWAV.h"
#include "FastWriteStream.h"

//////////////////////////////////////////////////////////////////////
//
// AVIAudioOutputStreamWAV
//
//////////////////////////////////////////////////////////////////////

class AVIAudioOutputStreamWAV : public AVIOutputStream {
public:
	AVIAudioOutputStreamWAV(class AVIOutput *out);

	virtual void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples);
};

AVIAudioOutputStreamWAV::AVIAudioOutputStreamWAV(class AVIOutput *out) : AVIOutputStream(out) {
}

void AVIAudioOutputStreamWAV::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) {
	static_cast<AVIOutputWAV *>(output)->write(pBuffer, cbBuffer);
}

//////////////////////////////////////////////////////////////////////
//
// AVIOutputWAV
//
//////////////////////////////////////////////////////////////////////

AVIOutputWAV::AVIOutputWAV() {
	fHeaderOpen			= false;
	mBytesWritten		= 0;
	fastIO				= NULL;
	mBufferSize			= 65536;
}

AVIOutputWAV::~AVIOutputWAV() {
}

IVDMediaOutputStream *AVIOutputWAV::createVideoStream() {
	return NULL;
}

IVDMediaOutputStream *AVIOutputWAV::createAudioStream() {
	VDASSERT(!audioOut);
	if (!(audioOut = new_nothrow AVIAudioOutputStreamWAV(this)))
		throw MyMemoryError();
	return audioOut;
}

bool AVIOutputWAV::init(const wchar_t *pwszFile) {
	DWORD dwHeader[5];

	if (!audioOut) return false;

	mFile.open(pwszFile, nsVDFile::kWrite | nsVDFile::kOpenAlways | nsVDFile::kSequential);

	fastIO = new FastWriteStream(pwszFile, mBufferSize, mBufferSize/2);

	dwHeader[0]	= FOURCC_RIFF;
	dwHeader[1] = 0x7F000000;
	dwHeader[2] = mmioFOURCC('W', 'A', 'V', 'E');
	dwHeader[3] = mmioFOURCC('f', 'm', 't', ' ');
	dwHeader[4] = audioOut->getFormatLen();

	_write(dwHeader, 20);
	_write(audioOut->getFormat(), dwHeader[4]);

	if (dwHeader[4] & 1)
		_write("", 1);

	// The 'fact' chunk is required for compressed WAVs and indicates the
	// number of uncompressed samples in the audio.  It is in fact rather
	// useless as it is usually computed from the ratios in the wave header
	// and the size of the compressed data -- check the output of Sound
	// Recorder -- but we must write it out anyway.

	if (((const WAVEFORMATEX *)audioOut->getFormat())->wFormatTag != WAVE_FORMAT_PCM) {
		dwHeader[0] = mmioFOURCC('f', 'a', 'c', 't');
		dwHeader[1] = 4;
		dwHeader[2] = 0;		// This will be filled in later.
		_write(dwHeader, 12);
	}

	dwHeader[0] = mmioFOURCC('d', 'a', 't', 'a');
	dwHeader[1] = 0x7E000000;

	_write(dwHeader, 8);

	fHeaderOpen = true;

	return TRUE;
}

void AVIOutputWAV::finalize() {
	long len = audioOut->getFormatLen();

	if (!mFile.isOpen())
		return;

	DWORD dwTemp;

	if (fastIO) {
		fastIO->Flush1();
		fastIO->Flush2((HANDLE)mFile.getRawHandle());
		delete fastIO;
		fastIO = NULL;
	}

	if (fHeaderOpen) {
		const WAVEFORMATEX& wfex = *(const WAVEFORMATEX *)audioOut->getFormat();
		len = (len+1)&-2;

		if (wfex.wFormatTag == WAVE_FORMAT_PCM) {
			dwTemp = mBytesWritten + len + 20;
			mFile.seek(4);
			mFile.write(&dwTemp, 4);

			mFile.seek(24 + len);
			mFile.write(&mBytesWritten, 4);
		} else {
			dwTemp = mBytesWritten + len + 32;
			mFile.seek(4);
			mFile.write(&dwTemp, 4);

			DWORD dwHeaderRewrite[3];

			dwHeaderRewrite[0] = (DWORD)((sint64)mBytesWritten * wfex.nSamplesPerSec / wfex.nAvgBytesPerSec);
			dwHeaderRewrite[1] = mmioFOURCC('d', 'a', 't', 'a');
			dwHeaderRewrite[2] = mBytesWritten;

			mFile.seek(28 + len);
			mFile.write(dwHeaderRewrite, 12);

			len += 12;		// offset the end of file by length of 'fact' chunk
		}

		fHeaderOpen = false;
	}

	mFile.seek(mBytesWritten + len + 28);
	mFile.truncate();
	mFile.close();
}

void AVIOutputWAV::write(const void *pBuffer, uint32 cbBuffer) {
	_write(pBuffer, cbBuffer);

	mBytesWritten += cbBuffer;
}

///////////////////////////////////////////////////////////////////////////

void AVIOutputWAV::_write(const void *data, int len) {
	if (fastIO) {
		fastIO->Put(data,len);
	} else
		mFile.write(data,len);
}
