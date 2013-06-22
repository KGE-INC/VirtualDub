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

#include <ddraw.h>

#include "AsyncBlitter.h"
#include <vd2/system/error.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/profile.h>
#include <vd2/system/tls.h>

#include "VBitmap.h"
#include "ddrawsup.h"

#define USE_DRAWDIBDRAW
//#define USE_STRETCHDIBITS
//#define USE_SETDIBITSTODEVICE


#ifdef _DEBUG
#define LOCK_SET(x)		(lock_state |= (x))
#define LOCK_CLEAR(x)	(lock_state &= ~(x))
#define LOCK_RESET		(lock_state = LOCK_NONE)
#else
#define LOCK_SET(x)
#define LOCK_CLEAR(x)
#define LOCK_RESET
#endif



AsyncBlitter::AsyncBlitter() : VDThread("AsyncBlitter") {
	max_requests		= 0;
	requests			= NULL;
	dwLockedBuffers		= 0;
	fAbort				= FALSE;
	fFlush				= false;
	fPulsed				= FALSE;
	pulseCallback		= NULL;
	dwPulseFrame		= 0;
	dwDrawFrame			= 0;

	LOCK_RESET;
}

AsyncBlitter::AsyncBlitter(int maxreq) : VDThread("AsyncBlitter") {

	max_requests		= maxreq;
	requests			= new AsyncBlitRequest[max_requests];
	memset(requests,0,sizeof(AsyncBlitRequest)*max_requests);
	dwLockedBuffers		= 0;
	fAbort				= FALSE;
	fFlush				= false;
	fPulsed				= FALSE;
	pulseCallback		= NULL;
	dwPulseFrame		= 0;
	dwDrawFrame			= 0;

	LOCK_RESET;

	if (!ThreadStart())
		throw MyError("Couldn't create draw thread!");

	SetThreadPriority(getThreadHandle(), THREAD_PRIORITY_HIGHEST);
}

AsyncBlitter::~AsyncBlitter() {
	if (isThreadAttached()) {
		fAbort = TRUE;
		mEventDraw.signal();

		ThreadWait();
	}

	delete[] requests;
}



///////////////////////////////////////////



void AsyncBlitter::enablePulsing(bool p) {
	dwPulseFrame = 0;
	dwDrawFrame = 0;
	fPulsed = p;
}

void AsyncBlitter::pulse() {
	++dwPulseFrame;
	mEventDraw.signal();
}

void AsyncBlitter::setPulseClock(uint32 clk) {
	if (clk <= dwPulseFrame)
		return;

	dwPulseFrame = clk;
	mEventDraw.signal();
}

bool AsyncBlitter::lock(uint32 id, sint32 timeout) {
	if (!requests)
		return true;
	
	if (fAbort)
		return false;

	if (dwLockedBuffers & id) {
		while((dwLockedBuffers & id) && !fAbort) {
			LOCK_SET(LOCK_LOCK);
			DWORD result = WaitForSingleObject(mEventDrawReturn.getHandle(), timeout < 0 ? INFINITE : timeout);
			LOCK_CLEAR(LOCK_LOCK);
			if (WAIT_TIMEOUT == result)
				return false;
		}
	}
	dwLockedBuffers |= id;

	return true;
}

void AsyncBlitter::unlock(uint32 id) {
	if (!requests) return;

	dwLockedBuffers &= ~id;
}

void AsyncBlitter::setPulseCallback(BOOL (*pc)(void *, DWORD), void *pcd) {
	pulseCallback = pc;
	pulseCallbackData = pcd;
}

bool AsyncBlitter::waitPulse(uint32 framenum) {
	if (fPulsed) {
		int pcret = PCR_OKAY;

		do {
			if (pulseCallback) {
				pcret = pulseCallback(pulseCallbackData, framenum);
				if (pcret == PCR_WAIT) {
					LOCK_SET(LOCK_PULSE);
					mEventDraw.wait();
					LOCK_CLEAR(LOCK_PULSE);
				}
			} else
				while(!fAbort && !fFlush) {
					sint32 diff = (sint32)(dwPulseFrame-framenum);

					if (diff >= 0)
						break;

					LOCK_SET(LOCK_PULSE);
					mEventDraw.wait();
					LOCK_CLEAR(LOCK_PULSE);
				}
		} while(pcret == PCR_WAIT && !fAbort && !fFlush);

		if (pcret == PCR_NOBLIT) return FALSE;
	}

	return fAbort;
}

void AsyncBlitter::nextFrame(long adv) {
	dwDrawFrame += adv;
}

long AsyncBlitter::getFrameDelta() {
	return dwPulseFrame - dwDrawFrame;
}

void AsyncBlitter::sendAFC(uint32 id, void (*pFunc)(void *), void *pData) {
	int i;

	if (fAbort) {
		return;
	}

	lock(id);

	if (!requests) {
		pFunc(pData);
		return;
	}

	for(;;) {
		for(i=0; i<max_requests; i++)
			if (!requests[i].bufferID) break;

		if (i < max_requests) break;

		LOCK_SET(LOCK_POST);
		mEventDrawReturn.wait();
		LOCK_CLEAR(LOCK_POST);

		if (fAbort) {
			unlock(id);
			return;
		}
	}

	requests[i].type			= AsyncBlitRequest::REQTYPE_AFC;

	requests[i].afc.pFunc		= pFunc;
	requests[i].afc.pData		= pData;

	requests[i].bufferID	= id;		// must be last!!!!

	mEventDraw.signal();

	// wait for request to complete

	lock(id);
	unlock(id);
}

void AsyncBlitter::postAPC(uint32 id, bool (*pFunc)(int, void *, void *), void *pData1, void *pData2) {
	int i;

	if (fAbort) {
		return;
	}

	if (!requests) {
		for(int pass = 0; pFunc(pass, pData1, pData2); ++pass)
			;

		return;
	}

	for(;;) {
		for(i=0; i<max_requests; i++)
			if (!requests[i].bufferID) break;

		if (i < max_requests) break;

		LOCK_SET(LOCK_POST);
		mEventDrawReturn.wait();
		LOCK_CLEAR(LOCK_POST);

		if (fAbort) {
			unlock(id);
			return;
		}
	}

	requests[i].type			= AsyncBlitRequest::REQTYPE_APC;

	requests[i].apc.pFunc		= pFunc;
	requests[i].apc.pass		= 0;
	requests[i].apc.pData1		= pData1;
	requests[i].apc.pData2		= pData2;

	requests[i].framenum	= dwDrawFrame;
	requests[i].bufferID	= id;		// must be last!!!!

	mEventDraw.signal();

//	VDDEBUG2("AsyncBlitter: posted %d (time=%d)\n", dwDrawFrame, dwPulseFrame);
}

void AsyncBlitter::release(uint32 id) {
	if (!requests) return;

	dwLockedBuffers &= ~id;
	mEventDrawReturn.signal();
}

void AsyncBlitter::abort() {
	fAbort = TRUE;
}

void AsyncBlitter::beginFlush() {
	if (!requests)
		return;

	fFlush = true;
	mEventDraw.signal();
}

bool AsyncBlitter::DoRequest(AsyncBlitRequest *req) {
	// DrawDibDraw(), StretchDIBits(), and SetDIBitsToDevice() are about the same...

	switch(req->type) {
	case AsyncBlitRequest::REQTYPE_AFC:
		req->afc.pFunc(req->afc.pData);
		break;

	case AsyncBlitRequest::REQTYPE_APC:
		return req->apc.pFunc(req->apc.pass++, req->apc.pData1, req->apc.pData2);
	}

	return false;
}

bool AsyncBlitter::ServiceRequests(bool fWait) {
	AsyncBlitRequest *req;
	bool fRequestServiced = false;
	int i;

	if (fFlush) {
		req = requests;

		for(i=0; i<max_requests; ++i,++req) {
			if (req->bufferID) {
				release(req->bufferID);
				req->bufferID = 0;
			}
		}

		mEventAbort.signal();

		return false;
	}

	req = requests;

	for(i=0; i<max_requests && !fAbort; ++i,++req) {
		if (req->bufferID) {
			if (!fFlush) {
				if (req->type == AsyncBlitRequest::REQTYPE_AFC) {
					DoRequest(req);
					fRequestServiced = true;
				} else if (!fWait || !waitPulse(req->framenum)) {
					if (dwPulseFrame < req->framenum) {
						continue;
					}

					fRequestServiced = true;
					if (mpRTProfiler)
						mpRTProfiler->BeginEvent(mProfileChannel, 0xe0ffe0, "Blit");
					bool bMore = DoRequest(req);
					if (mpRTProfiler)
						mpRTProfiler->EndEvent(mProfileChannel);

					if (bMore) {
						++req->framenum;
						continue;
					}
				}
			}

			fRequestServiced = true;
			release(req->bufferID);
			req->bufferID = 0;
		}
	}

	return fRequestServiced;
}

void AsyncBlitter::ThreadRun() {
	_RPT0(0,"AsyncBlitter: Thread started.\n");

	mpRTProfiler = VDGetRTProfiler();
	if (mpRTProfiler)
		mProfileChannel = mpRTProfiler->AllocChannel("Blitter");

	while(!fAbort) {
		if (!ServiceRequests(true) && !fAbort) {
			LOCK_SET(LOCK_ASYNC_EXIT);
			mEventDraw.wait();
			LOCK_CLEAR(LOCK_ASYNC_EXIT);
		}
	}

	GdiFlush();

	if (mpRTProfiler)
		mpRTProfiler->FreeChannel(mProfileChannel);

	dwLockedBuffers = 0;
	mEventDraw.signal();
	mEventAbort.signal();

	_RPT0(0,"AsyncBlitter: thread exit.\n");
}
