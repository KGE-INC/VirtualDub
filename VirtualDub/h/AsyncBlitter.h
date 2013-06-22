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

#ifndef f_ASYNCBLITTER_H
#define f_ASYNCBLITTER_H

#include <windows.h>
#include <vfw.h>
#include <ddraw.h>
#include <vd2/system/thread.h>

class IDDrawSurface;
class VDRTProfiler;

class AsyncBlitRequestAFC {
public:
	void (*pFunc)(void *);
	void *pData;
};

class AsyncBlitRequestAPC {
public:
	bool (*pFunc)(int, void *, void *);
	int pass;
	void *pData1, *pData2;
};

class AsyncBlitRequest {
public:
	enum {
		REQTYPE_AFC,
		REQTYPE_APC,
	} type;
	DWORD	bufferID;
	DWORD	framenum;

	union {
		AsyncBlitRequestAFC afc;
		AsyncBlitRequestAPC apc;
	};
};

class AsyncBlitter : public VDThread {
private:
	AsyncBlitRequest *requests;
	int max_requests;

	VDRTProfiler	*mpRTProfiler;
	int				mProfileChannel;
	VDSignal		mEventDraw;
	VDSignal		mEventDrawReturn;
	VDSignal		mEventAbort;
	volatile DWORD	dwLockedBuffers;
	DWORD			dwPulseFrame;
	DWORD			dwDrawFrame;
	volatile BOOL	fAbort;
	BOOL			fPulsed;
	volatile bool	fFlush;

	int		(*pulseCallback)(void *, DWORD);
	void	*pulseCallbackData;

	void release(DWORD);
	BOOL waitPulse(DWORD);
	bool DoRequest(AsyncBlitRequest *req);
	void ThreadRun();

public:
	AsyncBlitter();
	AsyncBlitter(int max_requests);
	~AsyncBlitter();

	enum {
		PCR_OKAY,
		PCR_NOBLIT,
		PCR_WAIT,
	};

#ifdef _DEBUG
	enum {
		LOCK_NONE		= 0,
		LOCK_DESTROY	= 0x00000001L,
		LOCK_LOCK		= 0x00000002L,
		LOCK_PULSE		= 0x00000004L,
		LOCK_POST		= 0x00000008L,
		LOCK_ASYNC_EXIT	= 0x00000010L,
	};
	
	volatile long lock_state;
#endif

	void enablePulsing(BOOL);
	void setPulseCallback(int (*pc)(void *, DWORD), void *pcd);
	void pulse();
	void setPulseClock(DWORD clk);
	void lock(DWORD);
	void unlock(DWORD);
	void nextFrame(long adv=1);
	long getFrameDelta();
	void sendAFC(DWORD id, void (*pFunc)(void *), void *pData);
	void postAPC(DWORD id, bool (*pFunc)(int, void *, void *), void *pData1, void *pData2);
	void abort();
	void beginFlush();

	bool ServiceRequests(bool fWait);

	VDSignal *getFlushCompleteSignal() {
		return requests ? &mEventAbort : NULL;
	}
};

#endif
