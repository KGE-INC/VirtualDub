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

static void __declspec(naked) __stdcall VDInterlockedOr(volatile DWORD& var, DWORD flag) {
	__asm {
		mov eax, [esp+8]
		mov edx, [esp+4]
		lock or dword ptr [edx], eax
		ret 8
	}
}

static void __declspec(naked) __stdcall VDInterlockedAnd(volatile DWORD& var, DWORD flag) {
	__asm {
		mov eax, [esp+8]
		mov edx, [esp+4]
		lock and dword ptr [edx], eax
		ret 8
	}
}



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



void AsyncBlitter::enablePulsing(BOOL p) {
	dwPulseFrame = 0;
	dwDrawFrame = 0;
	fPulsed = p;
}

void AsyncBlitter::pulse() {
	++dwPulseFrame;
	mEventDraw.signal();
}

void AsyncBlitter::setPulseClock(DWORD clk) {
	if (clk <= dwPulseFrame)
		return;

	dwPulseFrame = clk;
	mEventDraw.signal();
}

void AsyncBlitter::lock(DWORD id) {
	if (!requests || fAbort) return;

	if (dwLockedBuffers & id) {
		while((dwLockedBuffers & id) && !fAbort) {
			LOCK_SET(LOCK_LOCK);
			mEventDrawReturn.wait();
			LOCK_CLEAR(LOCK_LOCK);
		}
	}
	VDInterlockedOr(dwLockedBuffers, id);
}

void AsyncBlitter::unlock(DWORD id) {
	if (!requests) return;

	VDInterlockedAnd(dwLockedBuffers, ~id);
}

void AsyncBlitter::setPulseCallback(BOOL (*pc)(void *, DWORD), void *pcd) {
	pulseCallback = pc;
	pulseCallbackData = pcd;
}

BOOL AsyncBlitter::waitPulse(DWORD framenum) {
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
				while(!fAbort && !fFlush && (signed long)(dwPulseFrame-framenum) < 0) {
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

void AsyncBlitter::postDirectDrawCopy(DWORD id, void *data, BITMAPINFOHEADER *pbih, IDDrawSurface *pDest) {
	int i;

	if (fAbort) {
		unlock(id);
		return;
	}

	if (!requests) {
		VBitmap vbm;

		if (pDest->Lock(&vbm)) {
			int h = vbm.h;
			int w = vbm.w;
			unsigned char *dst = (unsigned char *)vbm.data;
			unsigned char *src = (unsigned char *)data;
			long lBytes;

			lBytes = (w*pbih->biBitCount)/8;

			do {
				memcpy(dst, src, lBytes);

				src += lBytes;
				dst += vbm.pitch;
			} while(--h);

			pDest->Unlock();
		}
		unlock(id);
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

	requests[i].type		= AsyncBlitRequest::REQTYPE_DIRECTDRAWCOPY;

	requests[i].ddcopy.data	= data;
	requests[i].ddcopy.pbih = pbih;
	requests[i].ddcopy.pDest = pDest;

	requests[i].framenum	= dwDrawFrame;
	requests[i].bufferID	= id;		// must be last!!!!

	mEventDraw.signal();
}

void AsyncBlitter::postDirectDrawCopyLaced(DWORD id, void *data, BITMAPINFOHEADER *pbih, IDDrawSurface *pDest, bool bFieldBDominant) {
	int i;

	if (fAbort) {
		unlock(id);
		return;
	}

	if (!requests) {
		VBitmap vbm;

		if (pDest->Lock(&vbm)) {
			int h = vbm.h;
			int w = vbm.w;
			unsigned char *dst = (unsigned char *)vbm.data;
			unsigned char *src = (unsigned char *)data;
			long lBytes;

			lBytes = (w*pbih->biBitCount)/8;

			do {
				memcpy(dst, src, lBytes);

				src += lBytes;
				dst += vbm.pitch;
			} while(--h);

			pDest->Unlock();
		}
		unlock(id);
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

	requests[i].type		= AsyncBlitRequest::REQTYPE_DIRECTDRAWCOPYLACED;

	requests[i].ddcopy.data	= data;
	requests[i].ddcopy.pbih = pbih;
	requests[i].ddcopy.pDest = pDest;
	requests[i].ddcopy.bFirst = true;
	requests[i].ddcopy.bFieldBDominant = bFieldBDominant;

	requests[i].framenum	= dwDrawFrame;
	requests[i].bufferID	= id;		// must be last!!!!

	mEventDraw.signal();
}

void AsyncBlitter::postDirectDrawBlit(DWORD id, IDirectDrawSurface *pDst, IDDrawSurface *pSrc, int xDst, int yDst, int dxDst, int dyDst) {
	int i;

	if (fAbort) {
		unlock(id);
		return;
	}

	if (!requests) return;

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

	requests[i].type		= AsyncBlitRequest::REQTYPE_DIRECTDRAWBLIT;

	requests[i].ddblit.pSrc		= pSrc;
	requests[i].ddblit.pDest	= pDst;
	requests[i].ddblit.r.left	= xDst;
	requests[i].ddblit.r.top	= yDst;
	requests[i].ddblit.r.right	= xDst + dxDst;
	requests[i].ddblit.r.bottom	= yDst + dyDst;

	requests[i].framenum	= dwDrawFrame;
	requests[i].bufferID	= id;		// must be last!!!!

	mEventDraw.signal();
}

void AsyncBlitter::postDirectDrawBlitLaced(DWORD id, IDirectDrawSurface *pDst, IDDrawSurface *pSrc, int xDst, int yDst, int dxDst, int dyDst, bool bFieldBDominant) {
	int i;

	if (fAbort) {
		unlock(id);
		return;
	}

	if (!requests) return;

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

	requests[i].type		= AsyncBlitRequest::REQTYPE_DIRECTDRAWBLITLACED;

	requests[i].ddblit.pSrc		= pSrc;
	requests[i].ddblit.pDest	= pDst;
	requests[i].ddblit.r.left	= xDst;
	requests[i].ddblit.r.top	= yDst;
	requests[i].ddblit.r.right	= xDst + dxDst;
	requests[i].ddblit.r.bottom	= yDst + dyDst;
	requests[i].ddblit.bFieldBDominant	= bFieldBDominant;
	requests[i].ddblit.bFirst	= true;

	requests[i].framenum	= dwDrawFrame;
	requests[i].bufferID	= id;		// must be last!!!!

	mEventDraw.signal();
}

void AsyncBlitter::post(DWORD id, HDRAWDIB hdd, HDC hdc, int xDst, int yDst, int dxDst, int dyDst, LPBITMAPINFOHEADER lpbi,
			  LPVOID lpBits, int xSrc, int ySrc, int dxSrc, int dySrc, UINT wFlags) {

	int i;

	if (fAbort) {
		unlock(id);
		return;
	}

	if (!requests) {
		if (!waitPulse(dwDrawFrame))
			DrawDibDraw(hdd, hdc, xDst, yDst, dxDst, dyDst, lpbi, lpBits, xSrc, ySrc, dxSrc, dySrc, wFlags);

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

	requests[i].type				= AsyncBlitRequest::REQTYPE_DRAWDIB;
	requests[i].drawdib.hdd			= hdd;
	requests[i].drawdib.hdc			= hdc;
	requests[i].drawdib.xDst		= xDst;
	requests[i].drawdib.yDst		= yDst;
	requests[i].drawdib.dxDst		= dxDst;
	requests[i].drawdib.dyDst		= dyDst;
	requests[i].drawdib.lpbi		= lpbi;
	requests[i].drawdib.lpBits		= lpBits;
	requests[i].drawdib.xSrc		= xSrc;
	requests[i].drawdib.ySrc		= ySrc;
	requests[i].drawdib.dxSrc		= dxSrc;
	requests[i].drawdib.dySrc		= dySrc;
	requests[i].drawdib.wFlags		= wFlags;

	requests[i].framenum	= dwDrawFrame;
	requests[i].bufferID	= id;		// must be last!!!!

	mEventDraw.signal();
}

void AsyncBlitter::postStretchBlt(DWORD id, HDC hdcDst, int xDst, int yDst, int dxDst, int dyDst,
			HDC hdcSrc, int xSrc, int ySrc, int dxSrc, int dySrc) {

	int i;

	if (fAbort) {
		unlock(id);
		return;
	}

	if (!requests) {
		if (!waitPulse(dwDrawFrame))
			if (dxDst == dxSrc && dyDst == dySrc)
				BitBlt(hdcDst, xDst, yDst, dxDst, dyDst, hdcSrc, xSrc, ySrc, SRCCOPY);
			else
				StretchBlt(hdcDst, xDst, yDst, dxDst, dyDst, hdcSrc, xSrc, ySrc, dxSrc, dySrc, SRCCOPY);

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

	requests[i].type				= AsyncBlitRequest::REQTYPE_STRETCHBLT;
	requests[i].stretchblt.hdcDst	= hdcDst;
	requests[i].stretchblt.xDst		= xDst;
	requests[i].stretchblt.yDst		= yDst;
	requests[i].stretchblt.dxDst	= dxDst;
	requests[i].stretchblt.dyDst	= dyDst;
	requests[i].stretchblt.hdcSrc	= hdcSrc;
	requests[i].stretchblt.xSrc		= xSrc;
	requests[i].stretchblt.ySrc		= ySrc;
	requests[i].stretchblt.dxSrc	= dxSrc;
	requests[i].stretchblt.dySrc	= dySrc;

	requests[i].framenum	= dwDrawFrame;
	requests[i].bufferID	= id;		// must be last!!!!

	mEventDraw.signal();
}

void AsyncBlitter::postBitBltLaced(DWORD id, HDC hdcDst, int xDst, int yDst, int dxDst, int dyDst,
			HDC hdcSrc, int xSrc, int ySrc, bool bFieldBDominant) {

	int i;

	if (fAbort) {
		unlock(id);
		return;
	}

	if (!requests) {
		if (!waitPulse(dwDrawFrame))
			BitBlt(hdcDst, xDst, yDst, dxDst, dyDst, hdcSrc, xSrc, ySrc, SRCCOPY);

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

	requests[i].type				= AsyncBlitRequest::REQTYPE_BITBLTLACED;
	requests[i].stretchblt.hdcDst	= hdcDst;
	requests[i].stretchblt.xDst		= xDst;
	requests[i].stretchblt.yDst		= yDst;
	requests[i].stretchblt.dxDst	= dxDst;
	requests[i].stretchblt.dyDst	= dyDst;
	requests[i].stretchblt.hdcSrc	= hdcSrc;
	requests[i].stretchblt.xSrc		= xSrc;
	requests[i].stretchblt.ySrc		= ySrc;
	requests[i].stretchblt.dxSrc	= bFieldBDominant ? 1 : 0;
	requests[i].stretchblt.dySrc	= 0;

	requests[i].framenum	= dwDrawFrame;
	requests[i].bufferID	= id;		// must be last!!!!

	mEventDraw.signal();
}

void AsyncBlitter::postStretchDIBits(DWORD id, HDC hdc, int xDst, int yDst, int dxDst, int dyDst, int xSrc, int ySrc, int dxSrc, int dySrc, const void *pBits, const BITMAPINFO *pBitsInfo, UINT iUsage, DWORD dwRop) {
	int i;

	if (fAbort) {
		unlock(id);
		return;
	}

	if (!requests) {
		if (!waitPulse(dwDrawFrame))
			StretchDIBits(hdc, xDst, yDst, dxDst, dyDst, xSrc, ySrc, dxSrc, dySrc, pBits, pBitsInfo, iUsage, dwRop);

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

	requests[i].type					= AsyncBlitRequest::REQTYPE_STRETCHDIBITS;
	requests[i].stretchdibits.hdc		= hdc;
	requests[i].stretchdibits.xDst		= xDst;
	requests[i].stretchdibits.yDst		= yDst;
	requests[i].stretchdibits.dxDst		= dxDst;
	requests[i].stretchdibits.dyDst		= dyDst;
	requests[i].stretchdibits.xSrc		= xSrc;
	requests[i].stretchdibits.ySrc		= ySrc;
	requests[i].stretchdibits.dxSrc		= dxSrc;
	requests[i].stretchdibits.dySrc		= dySrc;
	requests[i].stretchdibits.pBits		= pBits;
	requests[i].stretchdibits.pBitsInfo	= pBitsInfo;
	requests[i].stretchdibits.iUsage	= iUsage;
	requests[i].stretchdibits.dwRop		= dwRop;

	requests[i].framenum	= dwDrawFrame;
	requests[i].bufferID	= id;		// must be last!!!!

	mEventDraw.signal();
}

void AsyncBlitter::postICDraw(DWORD id, HIC hic, DWORD dwFlags, LPVOID pFormat, LPVOID pData, DWORD cbData, LONG lTime) {
	int i;

	if (fAbort) {
		unlock(id);
		return;
	}

	if (!requests) {
		ICDraw(hic, dwFlags, pFormat, pData, cbData, lTime);
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

	requests[i].type			= AsyncBlitRequest::REQTYPE_ICDRAW;

	requests[i].icdraw.hic		= hic;
	requests[i].icdraw.dwFlags	= dwFlags;
	requests[i].icdraw.pFormat	= pFormat;
	requests[i].icdraw.pData	= pData;
	requests[i].icdraw.cbData	= cbData;
	requests[i].icdraw.lTime	= lTime;

	requests[i].framenum	= dwDrawFrame;
	requests[i].bufferID	= id;		// must be last!!!!

	mEventDraw.signal();
}

void AsyncBlitter::postAFC(DWORD id, void (*pFunc)(void *), void *pData) {
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

void AsyncBlitter::release(DWORD id) {
	if (!requests) return;

	VDInterlockedAnd(dwLockedBuffers, ~id);
	mEventDrawReturn.signal();
}

void AsyncBlitter::abort() {
	fAbort = TRUE;
}

void AsyncBlitter::flush() {
	if (!requests)
		return;

	fFlush = true;
	mEventDraw.signal();

	while(fFlush)
		mEventAbort.wait();
}

static void __declspec(naked) MMXcopy(void *dst, void *src, int cnt) {
	__asm {
		mov	ecx,[esp+4]
		mov	edx,[esp+8]
		mov eax,[esp+12]
copyloop:
		movq	mm0,[edx]
		movq	[ecx],mm0
		add		edx,8
		add		ecx,8
		dec		eax
		jne		copyloop
		ret
	};
}

static void __declspec(naked) move_to_vidmem(void *dst, void *src, long dstpitch, long srcpitch, int w, int h) {
	__asm {
		push	ebp
		push	edi
		push	esi
		push	ebx

		mov		ebx,[esp+8+16]
		mov		edx,[esp+4+16]
		mov		eax,[esp+20+16]
		mov		ebp,[esp+24+16]
		mov		esi,eax
		and		eax,0ffffffe0h
		neg		eax
		mov		[esp+20+16],eax
		and		esi,31
		mov		[esp+4+16],esi
		sub		ebx,eax
		sub		edx,eax

yloop:
		mov		eax,[esp+20+16]
xloop:
		fild	qword ptr [ebx+eax]		;0
		fild	qword ptr [ebx+eax+8]	;1 0
		fild	qword ptr [ebx+eax+16]	;2 1 0
		fild	qword ptr [ebx+eax+24]	;3 2 1 0
		fxch	st(3)					;0 2 1 3
		fistp	qword ptr [edx+eax]		;2 1 3
		fistp	qword ptr [edx+eax+16]	;1 3
		fistp	qword ptr [edx+eax+8]	;3
		fistp	qword ptr [edx+eax+24]
		add		eax,32
		jnz		xloop

		mov		ecx,[esp+4+16]
		mov		esi,ebx
		mov		edi,edx
		rep		movsb

		add		ebx,[esp+16+16]
		add		edx,[esp+12+16]

		dec		ebp
		jne		yloop

		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		ret
	}
}

bool AsyncBlitter::DoRequest(AsyncBlitRequest *req) {
	// DrawDibDraw(), StretchDIBits(), and SetDIBitsToDevice() are about the same...

	switch(req->type) {
	case AsyncBlitRequest::REQTYPE_DRAWDIB:

		DrawDibDraw(req->drawdib.hdd, req->drawdib.hdc, req->drawdib.xDst, req->drawdib.yDst, req->drawdib.dxDst, req->drawdib.dyDst,
			req->drawdib.lpbi, req->drawdib.lpBits, req->drawdib.xSrc, req->drawdib.ySrc, req->drawdib.dxSrc, req->drawdib.dySrc, req->drawdib.wFlags);

		GdiFlush();
		break;

	case AsyncBlitRequest::REQTYPE_BITBLTLACED:
		{
			// It's actually four times faster to do 240 bitblts than to blit through
			// an interlaced clip region.

			for(int y=req->stretchblt.dxSrc; y<req->stretchblt.dyDst; y+=2) {
				BitBlt(req->stretchblt.hdcDst, req->stretchblt.xDst, req->stretchblt.yDst+y, req->stretchblt.dxDst, 1,
						req->stretchblt.hdcSrc, req->stretchblt.xSrc, req->stretchblt.ySrc+y, SRCCOPY);
			}

			GdiFlush();

			req->stretchblt.dxSrc ^= 1;
			return ++req->stretchblt.dySrc<2;
		}
		break;

	case AsyncBlitRequest::REQTYPE_STRETCHBLT:

		// BitBlt/StretchBlt request

		if (req->stretchblt.dxDst == req->stretchblt.dxSrc && req->stretchblt.dyDst == req->stretchblt.dySrc)
			BitBlt(req->stretchblt.hdcDst, req->stretchblt.xDst, req->stretchblt.yDst, req->stretchblt.dxDst, req->stretchblt.dyDst,
					req->stretchblt.hdcSrc, req->stretchblt.xSrc, req->stretchblt.ySrc, SRCCOPY);
		else
			StretchBlt(req->stretchblt.hdcDst, req->stretchblt.xDst, req->stretchblt.yDst, req->stretchblt.dxDst, req->stretchblt.dyDst,
					req->stretchblt.hdcSrc, req->stretchblt.xSrc, req->stretchblt.ySrc, req->stretchblt.dxSrc, req->stretchblt.dySrc, SRCCOPY);

		GdiFlush();

		break;

	case AsyncBlitRequest::REQTYPE_STRETCHDIBITS:
		StretchDIBits(
			req->stretchdibits.hdc,
			req->stretchdibits.xDst,
			req->stretchdibits.yDst,
			req->stretchdibits.dxDst,
			req->stretchdibits.dyDst,
			req->stretchdibits.xSrc,
			req->stretchdibits.ySrc,
			req->stretchdibits.dxSrc,
			req->stretchdibits.dySrc,
			req->stretchdibits.pBits,
			req->stretchdibits.pBitsInfo,
			req->stretchdibits.iUsage,
			req->stretchdibits.dwRop);
		GdiFlush();
		break;

	case AsyncBlitRequest::REQTYPE_DIRECTDRAWCOPY:

		{
			VBitmap vbm;

			if (req->ddcopy.pDest->Lock(&vbm)) {
				int h = vbm.h;
				int w = vbm.w;
				unsigned char *dst = (unsigned char *)vbm.data;
				unsigned char *src = (unsigned char *)req->ddcopy.data;
				long lBytes;

//								dst += vbm.pitch*(h-1);

				lBytes = (w*req->ddcopy.pbih->biBitCount)/8;

//				if (!MMX_enabled)
//					move_to_vidmem(dst, src, vbm.pitch, lBytes, lBytes, h);
//				else
				if (MMX_enabled && !(((long)dst | (long)src)&7) && !(lBytes&7)) {
					do {
						MMXcopy(dst, src, lBytes/8);

						src += lBytes;
						dst += vbm.pitch;
					} while(--h);
					__asm emms
				} else
					do {
						memcpy(dst, src, lBytes);

						src += lBytes;
						dst += vbm.pitch;
					} while(--h);

				req->ddcopy.pDest->Unlock();
			}

		}

		break;

	case AsyncBlitRequest::REQTYPE_DIRECTDRAWCOPYLACED:

		{
			VBitmap vbm;

			if (req->ddcopy.pDest->Lock(&vbm)) {
				int h = vbm.h;
				int w = vbm.w;
				unsigned char *dst = (unsigned char *)vbm.data;
				unsigned char *src = (unsigned char *)req->ddcopy.data;
				long lBytes;

				lBytes = (w*req->ddcopy.pbih->biBitCount)/8;

				if (!(req->ddcopy.bFirst ^ req->ddcopy.bFieldBDominant)) {
					src += lBytes;
					dst += vbm.pitch;
				}

				h >>= 1;

				if (MMX_enabled && !(((long)dst | (long)src)&7) && !(lBytes&7)) {
					do {
						MMXcopy(dst, src, lBytes/8);

						src += lBytes*2;
						dst += vbm.pitch*2;
					} while(--h);
					__asm emms
				} else
					do {
						memcpy(dst, src, lBytes);

						src += lBytes*2;
						dst += vbm.pitch*2;
					} while(--h);

				req->ddcopy.pDest->Unlock();
			}

			return !(req->ddcopy.bFirst = !req->ddcopy.bFirst);
		}

		break;

	case AsyncBlitRequest::REQTYPE_DIRECTDRAWBLIT:
		req->ddblit.pDest->Blt(&req->ddblit.r, req->ddblit.pSrc->getSurface(), NULL, DDBLT_WAIT, NULL);

		break;

	case AsyncBlitRequest::REQTYPE_DIRECTDRAWBLITLACED:
		{
			int y = req->ddblit.bFieldBDominant?1:0;
			int h = req->ddblit.r.bottom - req->ddblit.r.top;
			RECT rSrc;
			int xo = req->ddblit.r.left;
			int yo = req->ddblit.r.top;
			IDirectDrawSurface *pDDS = req->ddblit.pSrc->getSurface();
			IDirectDrawSurface3 *pDDS3;
			bool bPageLocked = false;

			rSrc.left = 0;
			rSrc.right = req->ddblit.r.right - req->ddblit.r.left;

			if (SUCCEEDED(pDDS->QueryInterface(IID_IDirectDrawSurface3, (void **)&pDDS3))) {
				if (SUCCEEDED(pDDS3->PageLock(0)))
					bPageLocked = true;
			} else
				pDDS3 = NULL;

			for(; y<h; y+=2) {
				rSrc.top	= y;
				rSrc.bottom	= y+1;

				req->ddblit.pDest->BltFast(xo, yo+y, pDDS, &rSrc,
					DDBLTFAST_WAIT|DDBLTFAST_NOCOLORKEY);
			}

			if (bPageLocked) {
				pDDS3->PageUnlock(0);
			}

			if (pDDS3)
				pDDS3->Release();

			req->ddblit.bFieldBDominant = !req->ddblit.bFieldBDominant;
		}
		return !(req->ddblit.bFirst = !req->ddblit.bFirst);

	case AsyncBlitRequest::REQTYPE_ICDRAW:
		ICDraw(req->icdraw.hic, req->icdraw.dwFlags, req->icdraw.pFormat, req->icdraw.pData, req->icdraw.cbData, req->icdraw.lTime);
		break;

	case AsyncBlitRequest::REQTYPE_AFC:
		req->afc.pFunc(req->afc.pData);
		break;
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

		fFlush = false;
		mEventAbort.signal();

		return true;
	}

	req = requests;

	for(i=0; i<max_requests && !fAbort; ++i,++req) {
		if (req->bufferID) {
			fRequestServiced = true;

			if (req->type == AsyncBlitRequest::REQTYPE_AFC)
				DoRequest(req);
			else if (!fWait || !waitPulse(req->framenum)) {
				if (dwPulseFrame < req->framenum) {
					continue;
				}

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

	if (mpRTProfiler)
		mpRTProfiler->FreeChannel(mProfileChannel);

	dwLockedBuffers = 0;
	mEventDraw.signal();
	mEventAbort.signal();

	_RPT0(0,"AsyncBlitter: thread exit.\n");
}
