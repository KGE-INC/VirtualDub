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
#include <vd2/system/error.h>
#include <vd2/Riza/audioout.h>

#include "AVIOutput.h"
#include "AVIOutputPreview.h"

///////////////////////////////////////////////////////////////////////////

AVIAudioPreviewOutputStream::AVIAudioPreviewOutputStream()
	: mpAudioOut(VDCreateAudioOutputWaveOutW32())
{
	initialized = started = FALSE;
	fInitialized = false;
}

AVIAudioPreviewOutputStream::~AVIAudioPreviewOutputStream() {
}

bool AVIAudioPreviewOutputStream::init() {
	fInitialized = true;

	return true;
}

void AVIAudioPreviewOutputStream::initAudio() {
	const WAVEFORMATEX *pwfex = (const WAVEFORMATEX *)getFormat();
	int blocks;
	int blocksin512;

	// Figure out what a 'good' buffer size is.
	// About a 5th of a second sounds good.

	blocks = (pwfex->nAvgBytesPerSec/5 + pwfex->nBlockAlign/2) / pwfex->nBlockAlign;

	// How many blocks for 512 bytes?  We don't want buffers smaller than that.

	blocksin512 = (512 + pwfex->nBlockAlign - 1) / pwfex->nBlockAlign;

	// Use the smaller value and allocate.

	if (!mpAudioOut->Init(std::max<int>(blocks, blocksin512)*pwfex->nBlockAlign, 10, pwfex))
		mpAudioOut->GoSilent();
}

void AVIAudioPreviewOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples) {
	fInitialized = true;

	if (!initialized) {
		initAudio();
		initialized = true;
	}

	mpAudioOut->Write(pBuffer, cbBuffer);
}

void AVIAudioPreviewOutputStream::partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {
	fInitialized = true;

	if (!initialized) {
		initAudio();
		initialized = true;
	}
}

void AVIAudioPreviewOutputStream::partialWrite(const void *pBuffer, uint32 cbBuffer) {
	mpAudioOut->Write(pBuffer, cbBuffer);
}

void AVIAudioPreviewOutputStream::partialWriteEnd() {
}

bool AVIAudioPreviewOutputStream::isSilent() {
	return mpAudioOut == NULL || mpAudioOut->IsSilent();
}

void AVIAudioPreviewOutputStream::start() {
	if (started || !fInitialized) return;

	if (!initialized) {
		initAudio();
		initialized = TRUE;
	}

	if (!mpAudioOut->Start()) {
		delete mpAudioOut;
		mpAudioOut = NULL;
	}

	started = TRUE;
}

void AVIAudioPreviewOutputStream::stop() {
	if (started && mpAudioOut)
		mpAudioOut->Stop();

	started = FALSE;

}

void AVIAudioPreviewOutputStream::flush() {
	_RPT0(0,"AVIAudioPreviewOutputStream: flushing...\n");
	if (mpAudioOut && started)
		mpAudioOut->Flush();
}

void AVIAudioPreviewOutputStream::finalize() {
	_RPT0(0,"AVIAudioPreviewOutputStream: finalizing...\n");
	if (mpAudioOut && started)
		mpAudioOut->Finalize();
}

long AVIAudioPreviewOutputStream::getPosition() {
	return mpAudioOut ? mpAudioOut->GetPosition() : -1;
}

long AVIAudioPreviewOutputStream::getAvailable() {
	return mpAudioOut ? mpAudioOut->GetAvailSpace() : -1;
}

bool AVIAudioPreviewOutputStream::isFrozen() {
	return mpAudioOut ? mpAudioOut->IsFrozen() : true;
}

///////////////////////////////////////////////////////////////////////////

class AVIVideoPreviewOutputStream : public AVIOutputStream {
public:
	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples) {}
	void partialWriteBegin(uint32 flags, uint32 bytes, uint32 samples) {}
	void partialWrite(const void *pBuffer, uint32 cbBuffer) {}
	void partialWriteEnd() {}
};

/////////////////////////////

AVIOutputPreview::AVIOutputPreview() {
}

AVIOutputPreview::~AVIOutputPreview() {
}

IVDMediaOutputStream *AVIOutputPreview::createVideoStream() {
	VDASSERT(!videoOut);
	if (!(videoOut = new_nothrow AVIVideoPreviewOutputStream()))
		throw MyMemoryError();
	return videoOut;
}

IVDMediaOutputStream *AVIOutputPreview::createAudioStream() {
	VDASSERT(!audioOut);
	if (!(audioOut = new_nothrow AVIAudioPreviewOutputStream()))
		throw MyMemoryError();
	return audioOut;
}

bool AVIOutputPreview::init(const wchar_t *szFile) {
	return true;
}

void AVIOutputPreview::finalize() {
	_RPT0(0,"AVIOutputPreview: Finalizing...\n");

	if (audioOut)
		static_cast<AVIAudioPreviewOutputStream *>(audioOut)->finalize();
}
