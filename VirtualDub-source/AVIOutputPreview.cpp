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

#include "VirtualDub.h"

#include <crtdbg.h>

#include "AVIOutput.h"
#include "AVIOutputPreview.h"

AVIAudioPreviewOutputStream::AVIAudioPreviewOutputStream(AVIOutput *out) : AVIAudioOutputStream(out) {
	initialized = started = FALSE;
	myAudioOut = NULL;
	fInitialized = false;
}

AVIAudioPreviewOutputStream::~AVIAudioPreviewOutputStream() {
	delete myAudioOut;
	myAudioOut = NULL;
}

BOOL AVIAudioPreviewOutputStream::init() {
	fInitialized = true;

	return TRUE;
}

bool AVIAudioPreviewOutputStream::initAudio() {
	const WAVEFORMATEX *pwfex = getWaveFormat();
	int blocks;
	int blocksin512;

	// Figure out what a 'good' buffer size is.
	// About a 5th of a second sounds good.

	blocks = (pwfex->nAvgBytesPerSec/5 + pwfex->nBlockAlign/2) / pwfex->nBlockAlign;

	// How many blocks for 512 bytes?  We don't want buffers smaller than that.

	blocksin512 = (512 + pwfex->nBlockAlign - 1) / pwfex->nBlockAlign;

	// Use the smaller value and allocate.

	myAudioOut = new AVIAudioOutput(max(blocks, blocksin512)*pwfex->nBlockAlign, 10);

	return !!myAudioOut;
}

BOOL AVIAudioPreviewOutputStream::write(LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer, LONG lSamples) {
	fInitialized = true;
	if (!myAudioOut && !initAudio()) {
		return FALSE;
	}
	if (!initialized) {
		if (!myAudioOut->init(getWaveFormat())) {
#if 0
			delete myAudioOut;
			myAudioOut = NULL;
			return FALSE;
#else
			myAudioOut->go_silent();
#endif
		}
		initialized = TRUE;
	}

	myAudioOut->write(lpBuffer, cbBuffer, INFINITE);

	return TRUE;
}

BOOL AVIAudioPreviewOutputStream::isSilent() {
	return myAudioOut == NULL || myAudioOut->isSilent();
}

void AVIAudioPreviewOutputStream::start() {
	if (started || !fInitialized) return;

	if (!getWaveFormat()) return;

	if (!myAudioOut && !initAudio()) {
		return;
	}

	if (!initialized) {
		if (!myAudioOut->init(getWaveFormat())) {
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

BOOL AVIAudioPreviewOutputStream::flush() {
	_RPT0(0,"AVIAudioPreviewOutputStream: flushing...\n");
	if (myAudioOut && started) myAudioOut->flush();

	return TRUE;
}

BOOL AVIAudioPreviewOutputStream::finalize() {
	_RPT0(0,"AVIAudioPreviewOutputStream: finalizing...\n");
	if (myAudioOut && started) return myAudioOut->finalize(INFINITE);

	return TRUE;
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

BOOL AVIVideoPreviewOutputStream::write(LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer, LONG lSamples) {
	return TRUE;
}

/////////////////////////////

AVIOutputPreview::AVIOutputPreview() {
}

AVIOutputPreview::~AVIOutputPreview() {
}

BOOL AVIOutputPreview::initOutputStreams() {
	if (!(audioOut = new AVIAudioPreviewOutputStream(this))) return FALSE;
	if (!(videoOut = new AVIVideoPreviewOutputStream(this))) return FALSE;

	return TRUE;
}

BOOL AVIOutputPreview::init(const char *szFile, LONG xSize, LONG ySize, BOOL videoIn, BOOL audioIn, LONG bufferSize, BOOL is_interleaved) {
	return TRUE;
}

BOOL AVIOutputPreview::finalize() {
	_RPT0(0,"AVIOutputPreview: Finalizing...\n");

	if (audioOut && !audioOut->finalize())
		return FALSE;

	return TRUE;
}

BOOL AVIOutputPreview::isPreview() { return TRUE; }

void AVIOutputPreview::writeIndexedChunk(FOURCC ckid, LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer) {
}
