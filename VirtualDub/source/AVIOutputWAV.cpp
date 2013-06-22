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

//////////////////////////////////////////////////////////////////////
//
// AVIAudioOutputStreamWAV
//
//////////////////////////////////////////////////////////////////////

class AVIAudioOutputStreamWAV : public AVIOutputStream {
public:
	AVIAudioOutputStreamWAV(AVIOutputWAV *pParent);

	virtual void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples);

protected:
	AVIOutputWAV *const mpParent;
};

AVIAudioOutputStreamWAV::AVIAudioOutputStreamWAV(AVIOutputWAV *pParent) : mpParent(pParent) {
}

void AVIAudioOutputStreamWAV::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) {
	mpParent->write(pBuffer, cbBuffer);
}

//////////////////////////////////////////////////////////////////////
//
// AVIOutputWAV
//
//////////////////////////////////////////////////////////////////////

AVIOutputWAV::AVIOutputWAV() {
	fHeaderOpen			= false;
	mBytesWritten		= 0;
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

	mpFileAsync = VDCreateFileAsync();
	mpFileAsync->Open(pwszFile, 2, mBufferSize >> 1);

	dwHeader[0]	= FOURCC_RIFF;
	dwHeader[1] = 0x7F000000;
	dwHeader[2] = mmioFOURCC('W', 'A', 'V', 'E');
	dwHeader[3] = mmioFOURCC('f', 'm', 't', ' ');
	dwHeader[4] = audioOut->getFormatLen();

	write(dwHeader, 20);
	write(audioOut->getFormat(), dwHeader[4]);

	if (dwHeader[4] & 1)
		write("", 1);

	// The 'fact' chunk is required for compressed WAVs and indicates the
	// number of uncompressed samples in the audio.  It is in fact rather
	// useless as it is usually computed from the ratios in the wave header
	// and the size of the compressed data -- check the output of Sound
	// Recorder -- but we must write it out anyway.

	if (((const WAVEFORMATEX *)audioOut->getFormat())->wFormatTag != WAVE_FORMAT_PCM) {
		dwHeader[0] = mmioFOURCC('f', 'a', 'c', 't');
		dwHeader[1] = 4;
		dwHeader[2] = 0;		// This will be filled in later.
		write(dwHeader, 12);
	}

	dwHeader[0] = mmioFOURCC('d', 'a', 't', 'a');
	dwHeader[1] = 0x7E000000;

	write(dwHeader, 8);

	mBytesWritten = 0;

	fHeaderOpen = true;

	return TRUE;
}

void AVIOutputWAV::finalize() {
	long len = audioOut->getFormatLen();

	if (!mpFileAsync->IsOpen())
		return;

	mpFileAsync->FastWriteEnd();

	if (fHeaderOpen) {
		const WAVEFORMATEX& wfex = *(const WAVEFORMATEX *)audioOut->getFormat();
		len = (len+1)&-2;

		DWORD dwTemp;
		if (wfex.wFormatTag == WAVE_FORMAT_PCM) {
			dwTemp = mBytesWritten + len + 20;

			mpFileAsync->Write(4, &dwTemp, 4);
			mpFileAsync->Write(24 + len, &mBytesWritten, 4);
		} else {
			dwTemp = mBytesWritten + len + 32;

			mpFileAsync->Write(4, &dwTemp, 4);

			DWORD dwHeaderRewrite[3];

			dwHeaderRewrite[0] = (DWORD)((sint64)mBytesWritten * wfex.nSamplesPerSec / wfex.nAvgBytesPerSec);
			dwHeaderRewrite[1] = mmioFOURCC('d', 'a', 't', 'a');
			dwHeaderRewrite[2] = mBytesWritten;

			mpFileAsync->Write(28 + len, dwHeaderRewrite, 12);

			len += 12;		// offset the end of file by length of 'fact' chunk
		}

		fHeaderOpen = false;
	}

	mpFileAsync->Truncate(mBytesWritten + len + 28);
	mpFileAsync->Close();
}

void AVIOutputWAV::write(const void *pBuffer, uint32 cbBuffer) {
	mpFileAsync->FastWrite(pBuffer, cbBuffer);
	mBytesWritten += cbBuffer;
}
