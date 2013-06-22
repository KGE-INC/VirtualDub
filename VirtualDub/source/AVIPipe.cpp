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

#include "VirtualDub.h"

#include <windows.h>

#include "AVIPipe.h"

///////////////////////////////

//#define AVIPIPE_PERFORMANCE_MESSAGES

//static CRITICAL_SECTION csect;

///////////////////////////////

AVIPipe::AVIPipe(int buffers, long roundup_size)
	: mState(0)
	, mReadPt(0)
	, mWritePt(0)
	, mLevel(0)
{
	pBuffers		= new struct AVIPipeBuffer[buffers];
	num_buffers		= buffers;
	round_size		= roundup_size;

	if (pBuffers)
		memset((void *)pBuffers, 0, sizeof(struct AVIPipeBuffer)*buffers);
}

AVIPipe::~AVIPipe() {
	_RPT0(0,"AVIPipe::~AVIPipe()\n");

	if (pBuffers) {
		for(int i=0; i<num_buffers; i++)
			if (pBuffers[i].data)
				VirtualFree(pBuffers[i].data, 0, MEM_RELEASE);

		delete[] (void *)pBuffers;
	}
}

bool AVIPipe::isFinalized() {
	if (mState & kFlagFinalizeTriggered) {
		finalizeAck();
		return true;
	}

	return false;
}

bool AVIPipe::isFinalizeAcked() {
	return 0 != (mState & kFlagFinalizeAcknowledged);
}

bool AVIPipe::full() {
	int h;

	vdsynchronized(mcsQueue) {
		if (mState & kFlagAborted)
			return false;

		// look for a handle without a buffer

		for(h=0; h<num_buffers; h++)
			if (!pBuffers[h].size)
				return false;
		
		// look for a handle with a free buffer
		
		for(h=0; h<num_buffers; h++)
			if (!pBuffers[h].len)
				return false;

		return true;
	}

	return false;
}

void *AVIPipe::getWriteBuffer(long len, int *handle_ptr) {
	int h;

	if (!len) ++len;
	len = ((len+round_size-1) / round_size) * round_size;

	++mcsQueue;

	for(;;) {
		if (mState & kFlagAborted) {
			--mcsQueue;
			return NULL;
		}

		// try the buffer right under us
		if (mLevel < num_buffers) {
			h = mWritePt;
			if (pBuffers[h].size >= len)
				break;
		}

		int nBufferWithoutAllocation = -1;
		int nBufferWithSmallAllocation = -1;

		h = mWritePt;
		for(int cnt = num_buffers - mLevel; cnt>0; --cnt) {
			if (!pBuffers[h].size)
				nBufferWithoutAllocation = h;
			else if (pBuffers[h].size < len)
				nBufferWithSmallAllocation = h;
			else
				goto buffer_found;

			if (++h >= num_buffers)
				h = 0;
		}

		if (nBufferWithoutAllocation >= 0)
			h = nBufferWithoutAllocation;
		else if (nBufferWithSmallAllocation >= 0)
			h = nBufferWithSmallAllocation;
		else {
			--mcsQueue;
			msigRead.wait();
			++mcsQueue;
			continue;
		}

buffer_found:

		if (pBuffers[h].size < len) {
			if (pBuffers[h].data) {
				VirtualFree(pBuffers[h].data, 0, MEM_RELEASE);
				pBuffers[h].data = NULL;
				pBuffers[h].size = 0;
			}

			VDASSERT(!pBuffers[h].data);
			if (pBuffers[h].data = VirtualAlloc(NULL, len, MEM_COMMIT, PAGE_READWRITE)) {
				pBuffers[h].size = len;
#ifdef AVIPIPE_PERFORMANCE_MESSAGES
				_RPT2(0,"Reallocated #%d: %ld bytes\n", h+1, len);
#endif
			}
		}

		if (h != mWritePt) {
			std::swap(pBuffers[h].data, pBuffers[mWritePt].data);
			std::swap(pBuffers[h].size, pBuffers[mWritePt].size);
		}
	}

	--mcsQueue;

	*handle_ptr = h;

	return pBuffers[h].data;
}

void AVIPipe::postBuffer(long len, VDPosition rawFrame, VDPosition displayFrame, VDPosition timelineFrame, int exdata, int droptype, int h, bool bFinal) {

	VDASSERT(h == mWritePt);

	++mcsQueue;
	pBuffers[h].len		= len+1;
	pBuffers[h].rawFrame		= rawFrame;
	pBuffers[h].displayFrame	= displayFrame;
	pBuffers[h].timelineFrame	= timelineFrame;
	pBuffers[h].iExdata			= exdata;
	pBuffers[h].droptype		= droptype;
	pBuffers[h].bFinal			= bFinal;

	if (++mWritePt >= num_buffers)
		mWritePt = 0;
	++mLevel;
	--mcsQueue;

	msigWrite.signal();

	//	_RPT2(0,"Posted buffer %ld (ID %08lx)\n",handle,cur_write-1);
}

void AVIPipe::getDropDistances(int& total, int& indep) {
	total = 0;
	indep = 0x3FFFFFFF;

	++mcsQueue;

	int h = mReadPt;
	for(int cnt = mLevel; cnt>0; --cnt) {
		int ahead = total;

		if (pBuffers[h].len>1) {
			if (pBuffers[h].droptype == kIndependent && ahead >= 0 && ahead < indep)
				indep = ahead;
		}

		++total;
		if (++h >= num_buffers)
			h = 0;
	}

	--mcsQueue;
}

void AVIPipe::getQueueInfo(int& total, int& finals) {
	total = 0;
	finals = 0;
	++mcsQueue;

	int h = mReadPt;
	for(int cnt = mLevel; cnt>0; --cnt) {
		if (pBuffers[h].len>0) {
			++total;
			if (pBuffers[h].bFinal)
				++finals;
		}
		if (++h >= num_buffers)
			h = 0;
	}

	--mcsQueue;
}

void *AVIPipe::getReadBuffer(long& len, VDPosition& rawFrame, VDPosition& displayFrame, VDPosition& timelineFrame, int *exdata_ptr, int *droptype_ptr, int *handle_ptr) {
	int h;

	++mcsQueue;

	for(;;) {
//		_RPT1(0,"Scouring buffers for ID %08lx\n",cur_read);

		if (mLevel) {
			h = mReadPt;
			break;
		}

		if (mState & kFlagSyncTriggered) {
			mState |= kFlagSyncAcknowledged;
			mState &= ~kFlagSyncTriggered;
			msigRead.signal();
		}

		--mcsQueue;

		if (mState & kFlagAborted)
			return NULL;

		if (mState & kFlagFinalizeTriggered) {
			mState |= kFlagFinalizeAcknowledged;

			msigRead.signal();
			return NULL;
		}

		msigWrite.wait();

		++mcsQueue;
	}

#ifdef AVIPIPE_PERFORMANCE_MESSAGES
	_RPT1(0,"[#%d] ", h+1);
#endif

	--mcsQueue;

	len			= pBuffers[h].len-1;
	rawFrame		= pBuffers[h].rawFrame;
	displayFrame	= pBuffers[h].displayFrame;
	timelineFrame	= pBuffers[h].timelineFrame;
	*exdata_ptr			= pBuffers[h].iExdata;
	*droptype_ptr		= pBuffers[h].droptype;
	*handle_ptr			= h;

	return pBuffers[h].data;
}

void AVIPipe::releaseBuffer(int handle) {
	VDASSERT(handle == mReadPt);

	++mcsQueue;
	pBuffers[handle].len = 0;
	--mLevel;
	if (++mReadPt >= num_buffers)
		mReadPt = 0;
	--mcsQueue;

	msigRead.signal();
}

void AVIPipe::finalize() {
	mState |= kFlagFinalizeTriggered;
	msigWrite.signal();
}

void AVIPipe::finalizeAndWait() {
	finalize();

	while(!(mState & kFlagFinalizeAcknowledged)) {
		msigRead.wait();
	}
}

void AVIPipe::finalizeAck() {
	mState |= kFlagFinalizeAcknowledged;
	msigRead.signal();
}

void AVIPipe::abort() {
	vdsynchronized(mcsQueue) {
		mState |= kFlagAborted;
		msigWrite.signal();
		msigRead.signal();
	}
}

bool AVIPipe::sync() {
	mState |= kFlagSyncTriggered;
	while(!(mState & kFlagSyncAcknowledged)) {
		if (mState & kFlagAborted)
			return false;

		msigWrite.signal();
		msigRead.wait();
	}
	mState &= ~kFlagSyncAcknowledged;

	return true;
}