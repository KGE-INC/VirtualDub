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
#include <mmsystem.h>

#include "AVIAudioOutput.h"

AVIAudioOutputBuffer::AVIAudioOutputBuffer(long bsize) {
	memset(&hdr, 0, sizeof hdr);
	hdr.lpData			= new char[bsize];
	hdr.dwBufferLength	= bsize;
	next = prev = NULL;
	dwBytesInBuffer = 0;

}

AVIAudioOutputBuffer::~AVIAudioOutputBuffer() {
	delete hdr.lpData;
}

BOOL AVIAudioOutputBuffer::init(HWAVEOUT hWaveOut) {
	return MMSYSERR_NOERROR == waveOutPrepareHeader(hWaveOut,&hdr,sizeof(WAVEHDR));
}

BOOL AVIAudioOutputBuffer::post(HWAVEOUT hWaveOut) {
	MMRESULT res;

	res = waveOutWrite(hWaveOut,&hdr,sizeof(WAVEHDR));

	return MMSYSERR_NOERROR == res;
}

void AVIAudioOutputBuffer::deinit(HWAVEOUT hWaveOut) {
	waveOutUnprepareHeader(hWaveOut, &hdr, sizeof(WAVEHDR));
}

AVIAudioOutput::AVIAudioOutput(long bufsize, int maxbufs) {
	this->maxbufs		= maxbufs;
	this->bufsize		= bufsize;

	numbufs = 0;
	pending = pending_tail = active = NULL;
	curState = STATE_NONE;

	fill_byte = (char)0x00;
	iBuffersActive = 0;

	hEventBuffersFree	= CreateEvent(NULL,FALSE,FALSE,NULL);

	InitializeCriticalSection(&mcsWaveDevice);
}

AVIAudioOutput::~AVIAudioOutput() {
	AVIAudioOutputBuffer *nb;

	DeleteCriticalSection(&mcsWaveDevice);

	if (curState == STATE_SILENT) return;

	stop();

	while(nb = pending) {
		pending = pending->next;
		nb->deinit(hWaveOut);
		delete nb;
	}

	while(nb = active) {
		active = active->next;
		nb->deinit(hWaveOut);
		delete nb;
	}

	if (curState >= STATE_OPENED)
		waveOutClose(hWaveOut);

	if (hEventBuffersFree)
		CloseHandle(hEventBuffersFree);
}

BOOL AVIAudioOutput::init(const WAVEFORMATEX *wf) {
	MMRESULT res;
	AVIAudioOutputBuffer *aaob;

	if (wf->wFormatTag == WAVE_FORMAT_PCM)
		if (wf->wBitsPerSample == 8)
			fill_byte = (char)0x80;

	if (MMSYSERR_NOERROR != (res = waveOutOpen(&hWaveOut, WAVE_MAPPER, wf, (DWORD)hEventBuffersFree, 0, CALLBACK_EVENT)))
		return FALSE;

	curState = STATE_OPENED;
	nSamplesPerSec = wf->nSamplesPerSec;
	nAvgBytesPerSec = wf->nAvgBytesPerSec;

	// Hmmm... we can't allocate buffers while the wave device
	// is active...

	while(numbufs < maxbufs) {
		aaob = new AVIAudioOutputBuffer(bufsize);
		if (!aaob) break;

		if (!aaob->init(hWaveOut)) {
			_RPT0(0,"Init failed\n");
			delete aaob;
			return FALSE;
		}

#if 0
		{
			char buf[128];
			wsprintf(buf, "Allocated audio buffer of %ld bytes\n", aaob->hdr.dwBufferLength);
			OutputDebugString(buf);
		}
#endif

		aaob->prev = NULL;
		aaob->next = pending;
		if (pending) pending->prev = aaob;
		else pending_tail = aaob;
		pending = aaob;

		++numbufs;
	}

	lAvailSpace = numbufs * bufsize;

	waveOutPause(hWaveOut);

	return TRUE;
}

void AVIAudioOutput::go_silent() {
	curState = STATE_SILENT;
}

BOOL AVIAudioOutput::isSilent() {
	return curState == STATE_SILENT;
}

BOOL AVIAudioOutput::start() {
	if (curState == STATE_SILENT) return TRUE;

	if (curState < STATE_OPENED) return FALSE;

	if (MMSYSERR_NOERROR != waveOutRestart(hWaveOut))
		return FALSE;

	curState = STATE_PLAYING;

	return TRUE;
}

BOOL AVIAudioOutput::stop() {
	if (curState == STATE_SILENT) return TRUE;

	if (curState >= STATE_OPENED) {
		if (MMSYSERR_NOERROR != waveOutReset(hWaveOut))
			return FALSE;

		curState = STATE_OPENED;
	}

	return TRUE;
}

BOOL AVIAudioOutput::checkBuffers() {
	AVIAudioOutputBuffer *aaob,*aaob2;
	int found = 0;

	if (curState == STATE_SILENT) return TRUE;

	aaob = active;
	while(aaob) {
		if (aaob->hdr.dwFlags & WHDR_DONE) {
			aaob2=aaob->next;
			if (aaob->prev) aaob->prev->next = aaob->next; else active=aaob->next;
			if (aaob->next) aaob->next->prev = aaob->prev;
			aaob->hdr.dwFlags &= ~WHDR_DONE;
			aaob->dwBytesInBuffer = 0;

			aaob->prev = NULL;
			aaob->next = pending;
			if (pending) pending->prev = aaob;
			else pending_tail = aaob;
			pending = aaob;

			--iBuffersActive;
			lAvailSpace += aaob->hdr.dwBufferLength;

			aaob=aaob2;
			++found;
		} else
			aaob=aaob->next;
	}

//	_RPT1(0,"%d buffers returned\n",found);

	return found>0;
}

BOOL AVIAudioOutput::waitBuffers(DWORD timeout) {
	if (curState == STATE_SILENT) return TRUE;

	if (hEventBuffersFree && timeout) {
		for(;;) {
			if (WAIT_OBJECT_0 != WaitForSingleObject(hEventBuffersFree, timeout))
				return FALSE;

			if (checkBuffers())
				return TRUE;
		}
	}

	return checkBuffers();
}

long AVIAudioOutput::avail() {
	checkBuffers();
	return lAvailSpace;
}

BOOL AVIAudioOutput::write(const void *data, long len, DWORD timeout) {
	AVIAudioOutputBuffer *aaob;
	long tc;

	if (curState == STATE_SILENT) return TRUE;

	checkBuffers();
//	_RPT1(0,"writing %ld bytes\n",len);

	while(len) {
		if (!pending) {

			if (!waitBuffers(0) && timeout) {
				if (!waitBuffers(timeout)) {
					return FALSE;
				}
				continue;
			}
			break;
		}

		aaob = pending_tail;

		tc = aaob->hdr.dwBufferLength - aaob->dwBytesInBuffer;
		if (tc > len) tc=len;

		if (tc)
			memcpy((char *)aaob->hdr.lpData + aaob->dwBytesInBuffer, data, tc);

		lAvailSpace -= tc;

		// if the buffer is full, ship it out

		if ((aaob->dwBytesInBuffer += tc) >= aaob->hdr.dwBufferLength) {
#if 0
			{
				char buf[128];
				wsprintf(buf, "Posting audio buffer of %ld/%ld bytes (%ld already active)\n", aaob->dwBytesInBuffer, aaob->hdr.dwBufferLength, iBuffersActive);
				OutputDebugString(buf);
			}
#endif
			if (!postBuffer(aaob)) return FALSE;
		}

		len -= tc;
		data = (char *)data + tc;
	}

	return TRUE;
}

void AVIAudioOutput::flush() {
	if (pending_tail && pending_tail->dwBytesInBuffer)
		postBuffer(pending_tail);
}

BOOL AVIAudioOutput::finalize(DWORD timeout) {
	if (curState == STATE_SILENT) return TRUE;

	_RPT0(0,"AVIAudioOutput: finalizing output\n");

	flush();

	while(checkBuffers(), active)
		if (hEventBuffersFree != INVALID_HANDLE_VALUE)
			if (WAIT_OBJECT_0 != WaitForSingleObject(hEventBuffersFree, timeout))
				return FALSE;

	_RPT0(0,"AVIAudioOutput: finalized.\n");

	return TRUE;
}

BOOL AVIAudioOutput::postBuffer(AVIAudioOutputBuffer *aaob) {
	// delink from pending list

	if (aaob->prev) aaob->prev->next = aaob->next; else pending=aaob->next;
	if (aaob->next) aaob->next->prev = aaob->prev; else pending_tail=aaob->prev;

    // clear rest of data

	if (aaob->dwBytesInBuffer < aaob->hdr.dwBufferLength)
		memset((char *)aaob->hdr.lpData + aaob->dwBytesInBuffer, fill_byte, aaob->hdr.dwBufferLength-aaob->dwBytesInBuffer);

	// post to wave device

	aaob->hdr.dwFlags &= ~WHDR_DONE;

	EnterCriticalSection(&mcsWaveDevice);
	bool bResult = 0 != aaob->post(hWaveOut);
	LeaveCriticalSection(&mcsWaveDevice);

	if (!bResult) {
		_RPT0(0,"post failed!\n");
		return FALSE;
	}

	// link to active list

	aaob->prev = NULL;
	aaob->next = active;
	if (active) active->prev = aaob;
	active = aaob;

	++iBuffersActive;

	return TRUE;
}

long AVIAudioOutput::position() {
	MMTIME mmtime;

	if (curState != STATE_PLAYING) return -1;

	mmtime.wType = TIME_SAMPLES;

	EnterCriticalSection(&mcsWaveDevice);
	MMRESULT res = waveOutGetPosition(hWaveOut, &mmtime, sizeof mmtime);
	LeaveCriticalSection(&mcsWaveDevice);

	if (MMSYSERR_NOERROR != res)
		return -1;

	switch(mmtime.wType) {
	case TIME_BYTES:
		return MulDiv(mmtime.u.cb, 1000, nAvgBytesPerSec);
	case TIME_MS:
		return mmtime.u.ms;
	case TIME_SAMPLES:
		return MulDiv(mmtime.u.sample, 1000, nSamplesPerSec);
	}

	return -1;
}

bool AVIAudioOutput::isFrozen() {
	AVIAudioOutputBuffer *aaob;

	if (curState != STATE_PLAYING) return true;

	aaob = active;
	while(aaob) {
		if (!(aaob->hdr.dwFlags & WHDR_DONE))
			return false;

		aaob=aaob->next;
	}

	return true;
}
