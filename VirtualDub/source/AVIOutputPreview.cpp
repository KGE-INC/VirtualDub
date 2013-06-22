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

#include <vd2/system/error.h>

#include "AVIOutput.h"
#include "AVIOutputPreview.h"

AVIAudioPreviewOutputStream::AVIAudioPreviewOutputStream(AVIOutput *out) : AVIOutputStream(out) {
	initialized = started = FALSE;
	myAudioOut = NULL;
	fInitialized = false;
}

AVIAudioPreviewOutputStream::~AVIAudioPreviewOutputStream() {
	delete myAudioOut;
	myAudioOut = NULL;
}

bool AVIAudioPreviewOutputStream::init() {
	fInitialized = true;

	return true;
}

bool AVIAudioPreviewOutputStream::initAudio() {
	const WAVEFORMATEX *pwfex = (const WAVEFORMATEX *)getFormat();
	int blocks;
	int blocksin512;

	// Figure out what a 'good' buffer size is.
	// About a 5th of a second sounds good.

	blocks = (pwfex->nAvgBytesPerSec/5 + pwfex->nBlockAlign/2) / pwfex->nBlockAlign;

	// How many blocks for 512 bytes?  We don't want buffers smaller than that.

	blocksin512 = (512 + pwfex->nBlockAlign - 1) / pwfex->nBlockAlign;

	// Use the smaller value and allocate.

	myAudioOut = new AVIAudioOutput(std::max<int>(blocks, blocksin512)*pwfex->nBlockAlign, 10);

	return !!myAudioOut;
}

void AVIAudioPreviewOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples) {
	fInitialized = true;
	if (!myAudioOut && !initAudio())
		return;

	if (!initialized) {
		if (!myAudioOut->init((const WAVEFORMATEX *)getFormat())) {
			myAudioOut->go_silent();
		}
		initialized = true;
	}

	myAudioOut->write(pBuffer, cbBuffer, INFINITE);
}

bool AVIAudioPreviewOutputStream::isSilent() {
	return myAudioOut == NULL || myAudioOut->isSilent();
}

void AVIAudioPreviewOutputStream::start() {
	if (started || !fInitialized) return;

	if (!myAudioOut && !initAudio()) {
		return;
	}

	if (!initialized) {
		if (!myAudioOut->init((const WAVEFORMATEX *)getFormat())) {
			delete myAudioOut;
			myAudioOut = NULL;
			return;
		}
		initialized = TRUE;
	}

	if (!myAudioOut->start()) {
		delete myAudioOut;
		myAudioOut = NULL;
	}

	started = TRUE;
}

void AVIAudioPreviewOutputStream::stop() {
	if (started && myAudioOut)
		myAudioOut->stop();

	started = FALSE;

}

void AVIAudioPreviewOutputStream::flush() {
	_RPT0(0,"AVIAudioPreviewOutputStream: flushing...\n");
	if (myAudioOut && started)
		myAudioOut->flush();
}

void AVIAudioPreviewOutputStream::finalize() {
	_RPT0(0,"AVIAudioPreviewOutputStream: finalizing...\n");
	if (myAudioOut && started)
		myAudioOut->finalize(INFINITE);
}

long AVIAudioPreviewOutputStream::getPosition() {
	return myAudioOut ? myAudioOut->position() : -1;
}

long AVIAudioPreviewOutputStream::getAvailable() {
	return myAudioOut ? myAudioOut->avail() : -1;
}

bool AVIAudioPreviewOutputStream::isFrozen() {
	return myAudioOut ? myAudioOut->isFrozen() : true;
}

/////////////////////////////

void AVIVideoPreviewOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples) {
}

/////////////////////////////

AVIOutputPreview::AVIOutputPreview() {
}

AVIOutputPreview::~AVIOutputPreview() {
}

IVDMediaOutputStream *AVIOutputPreview::createVideoStream() {
	VDASSERT(!videoOut);
	if (!(videoOut = new_nothrow AVIVideoPreviewOutputStream(this)))
		throw MyMemoryError();
	return videoOut;
}

IVDMediaOutputStream *AVIOutputPreview::createAudioStream() {
	VDASSERT(!audioOut);
	if (!(audioOut = new_nothrow AVIAudioPreviewOutputStream(this)))
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
