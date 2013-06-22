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

#include <windows.h>
#include <crtdbg.h>

#include "AVIPipe.h"

///////////////////////////////

//#define AVIPIPE_PERFORMANCE_MESSAGES

//static CRITICAL_SECTION csect;

///////////////////////////////

AVIPipe::AVIPipe(int buffers, long roundup_size) {
	hEventRead		= CreateEvent(NULL,FALSE,FALSE,NULL);
	hEventWrite		= CreateEvent(NULL,FALSE,FALSE,NULL);
	pBuffers		= new struct AVIPipeBuffer[buffers];
	num_buffers		= buffers;
	round_size		= roundup_size;
	cur_read		= 1;
	cur_write		= 1;
	finalize_state	= 0;
	total_audio		= 0;

	if (pBuffers)
		memset((void *)pBuffers, 0, sizeof(struct AVIPipeBuffer)*buffers);

	InitializeCriticalSection(&critsec);
}

AVIPipe::~AVIPipe() {
	_RPT0(0,"AVIPipe::~AVIPipe()\n");

	DeleteCriticalSection(&critsec);

	if (pBuffers) {
		for(int i=0; i<num_buffers; i++)
			if (pBuffers[i].data)
				VirtualFree(pBuffers[i].data, 0, MEM_RELEASE);

		delete[] (void *)pBuffers;
	}

	if (hEventRead)		CloseHandle(hEventRead);
	if (hEventWrite)	CloseHandle(hEventWrite);
}

BOOL AVIPipe::isOkay() {
	return pBuffers && hEventRead && hEventWrite;
}

BOOL AVIPipe::isFinalized() {
	if (finalize_state & FINALIZE_TRIGGERED) {
		finalize_state |= FINALIZE_ACKNOWLEDGED;
		SetEvent(hEventRead);

		return TRUE;
	}

	return FALSE;
}

BOOL AVIPipe::isNoMoreAudio() {
	return (finalize_state & FINALIZE_TRIGGERED) && !total_audio;
}

void *AVIPipe::getWriteBuffer(long len, int *handle_ptr, DWORD timeout) {
	int h;

	if (!len) ++len;
	len = ((len+round_size-1) / round_size) * round_size;

	EnterCriticalSection(&critsec);

	for(;;) {

		if (finalize_state & FINALIZE_ABORTED) {
			LeaveCriticalSection(&critsec);
			return NULL;
		}

		// look for a handle without a buffer

		for(h=0; h<num_buffers; h++)
			if (!pBuffers[h].size)
				if (pBuffers[h].data = VirtualAlloc(NULL, len, MEM_COMMIT, PAGE_READWRITE)) {
					pBuffers[h].size = len;
#ifdef AVIPIPE_PERFORMANCE_MESSAGES
					_RPT2(0,"Allocated #%d: %ld bytes\n", h+1, len);
#endif
					break;
				}
		
		if (h<num_buffers) break;

		// look for a handle with a free buffer that's large enough
		
		for(h=0; h<num_buffers; h++)
			if (!pBuffers[h].len && pBuffers[h].size>=len) break;

		if (h<num_buffers) break;

		// look for a handle with a free buffer

		for(h=0; h<num_buffers; h++)
			if (!pBuffers[h].len && pBuffers[h].size) {
				VirtualFree(pBuffers[h].data, 0, MEM_RELEASE);
				pBuffers[h].data = NULL;
				pBuffers[h].size = 0;

				if (pBuffers[h].data = VirtualAlloc(NULL, len, MEM_COMMIT, PAGE_READWRITE)) {
					pBuffers[h].size = len;
#ifdef AVIPIPE_PERFORMANCE_MESSAGES
					_RPT2(0,"Reallocated #%d: %ld bytes\n", h+1, len);
#endif
					break;
				}
			}

		if (h<num_buffers) break;

		LeaveCriticalSection(&critsec);

		if (WAIT_TIMEOUT == WaitForSingleObject(hEventRead, timeout)) {
			return NULL;
		}

		EnterCriticalSection(&critsec);
	}

	LeaveCriticalSection(&critsec);
	*handle_ptr = h;

	return pBuffers[h].data;
}

void AVIPipe::postBuffer(long len, long samples, long dframe, int exdata, int droptype, int h) {

	EnterCriticalSection(&critsec);

	pBuffers[h].len		= len+1;
	pBuffers[h].sample	= samples;
	pBuffers[h].displayframe = dframe;
	pBuffers[h].iExdata	= exdata;
	pBuffers[h].droptype = droptype;
	pBuffers[h].id		= cur_write++;

	if (exdata == -1) ++total_audio;

	LeaveCriticalSection(&critsec);

	SetEvent(hEventWrite);

	//	_RPT2(0,"Posted buffer %ld (ID %08lx)\n",handle,cur_write-1);
}

void AVIPipe::getDropDistances(int& total, int& indep) {
	int h;

	total = 0;
	indep = 0x3FFFFFFF;

	EnterCriticalSection(&critsec);

	for(h=0; h<num_buffers; h++) {
		int ahead = pBuffers[h].id - cur_read;

		if (pBuffers[h].iExdata >= 0) {
			if (pBuffers[h].len>1) {
				if (pBuffers[h].droptype == kIndependent && ahead >= 0 && ahead < indep)
					indep = ahead;
			}

			++total;
		}
	}

	LeaveCriticalSection(&critsec);
}

void *AVIPipe::getReadBuffer(long *len_ptr, long *samples_ptr, long *displayframe_ptr, int *exdata_ptr, int *droptype_ptr, int *handle_ptr, DWORD timeout) {
	int h;

	EnterCriticalSection(&critsec);

	for(;;) {
//		_RPT1(0,"Scouring buffers for ID %08lx\n",cur_read);

		for(h=0; h<num_buffers; h++) {
//			_RPT2(0,"Buffer %ld: ID %08lx\n", h, lpBufferID[h]);
			if (pBuffers[h].id == cur_read) break;
		}

		if (h<num_buffers) break;

		if (finalize_state & SYNCPOINT_TRIGGERED) {
			finalize_state |= SYNCPOINT_ACKNOWLEDGED;
			finalize_state &= ~SYNCPOINT_TRIGGERED;
			SetEvent(hEventRead);
		}

		LeaveCriticalSection(&critsec);

		if (finalize_state & FINALIZE_TRIGGERED) {
			finalize_state |= FINALIZE_ACKNOWLEDGED;

			SetEvent(hEventRead);
			return NULL;
		}

		if (WAIT_TIMEOUT == WaitForSingleObject(hEventWrite, timeout)) {
			return NULL;
		}
		EnterCriticalSection(&critsec);
	}

#ifdef AVIPIPE_PERFORMANCE_MESSAGES
	_RPT1(0,"[#%d] ", h+1);
#endif

	++cur_read;

	LeaveCriticalSection(&critsec);

	*len_ptr			= pBuffers[h].len-1;
	*samples_ptr		= pBuffers[h].sample;
	*displayframe_ptr	= pBuffers[h].displayframe;
	*exdata_ptr			= pBuffers[h].iExdata;
	*droptype_ptr		= pBuffers[h].droptype;
	*handle_ptr			= h;

	return pBuffers[h].data;
}

void AVIPipe::releaseBuffer(int handle) {

	EnterCriticalSection(&critsec);

	if (pBuffers[handle].iExdata==-1)
		--total_audio;
	pBuffers[handle].len = 0;

	LeaveCriticalSection(&critsec);

	SetEvent(hEventRead);
}

void AVIPipe::finalize() {
	finalize_state |= FINALIZE_TRIGGERED;
	SetEvent(hEventWrite);

	_RPT0(0,"AVIPipe: finalizing...\n");

	while(!(finalize_state & FINALIZE_ACKNOWLEDGED)) {
		WaitForSingleObject(hEventRead, INFINITE);
	}

	_RPT0(0,"AVIPipe: finalized.\n");
}

void AVIPipe::abort() {
	finalize_state |= FINALIZE_ABORTED;
	SetEvent(hEventWrite);
	SetEvent(hEventRead);
}

bool AVIPipe::sync() {
	finalize_state |= SYNCPOINT_TRIGGERED;
	while(!(finalize_state & SYNCPOINT_ACKNOWLEDGED)) {
		if (finalize_state & FINALIZE_ABORTED)
			return false;

		SetEvent(hEventWrite);
		WaitForSingleObject(hEventRead, INFINITE);
	}
	finalize_state &= ~SYNCPOINT_ACKNOWLEDGED;

	return true;
}