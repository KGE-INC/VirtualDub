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

class IDDrawSurface;

class AsyncBlitRequestDrawDib {
public:
	HDRAWDIB hdd;
	HDC		hdc;
	int		xDst, yDst;
	int		dxDst, dyDst;
	LPBITMAPINFOHEADER lpbi;
	LPVOID	lpBits;
	int		xSrc;
	int		ySrc;
	int		dxSrc;
	int		dySrc;
	UINT	wFlags;
};

class AsyncBlitRequestStretchBlt {
public:
	HDC		hdcDst;
	int		xDst, yDst;
	int		dxDst, dyDst;
	HDC		hdcSrc;
	int		xSrc, ySrc;
	int		dxSrc, dySrc;
};

class AsyncBlitRequestStretchDIBits {
public:
	HDC		hdc;
	int		xDst, yDst, dxDst, dyDst;
	int		xSrc, ySrc, dxSrc, dySrc;
	const void	*pBits;
	const BITMAPINFO *pBitsInfo;
	UINT	iUsage;
	DWORD	dwRop;
};

class AsyncBlitRequestDirectDrawCopy {
public:
	void	*data;
	BITMAPINFOHEADER *pbih;
	IDDrawSurface *pDest;
	bool	bFirst;
	bool	bFieldBDominant;
};

class AsyncBlitRequestDirectDrawBlit {
public:
	IDDrawSurface *pSrc;
	IDirectDrawSurface *pDest;
	RECT r;
	bool bFieldBDominant;
	bool bFirst;
};

class AsyncBlitRequestICDraw {
public:
	HIC hic;
	DWORD dwFlags;
	LPVOID pFormat;
	LPVOID pData;
	DWORD cbData;
	LONG lTime;
};

class AsyncBlitRequestAFC {
public:
	void (*pFunc)(void *);
	void *pData;
};

class AsyncBlitRequest {
public:
	enum {
		REQTYPE_DRAWDIB,
		REQTYPE_STRETCHBLT,
		REQTYPE_DIRECTDRAWCOPY,
		REQTYPE_DIRECTDRAWBLIT,
		REQTYPE_ICDRAW,
		REQTYPE_STRETCHDIBITS,
		REQTYPE_AFC,
		REQTYPE_BITBLTLACED,
		REQTYPE_DIRECTDRAWCOPYLACED,
		REQTYPE_DIRECTDRAWBLITLACED,
	} type;
	DWORD	bufferID;
	DWORD	framenum;

	union {
		AsyncBlitRequestDrawDib	drawdib;
		AsyncBlitRequestStretchBlt stretchblt;
		AsyncBlitRequestDirectDrawCopy	ddcopy;
		AsyncBlitRequestDirectDrawBlit	ddblit;
		AsyncBlitRequestICDraw	icdraw;
		AsyncBlitRequestStretchDIBits stretchdibits;
		AsyncBlitRequestAFC afc;
	};
};

class AsyncBlitter {
private:
	AsyncBlitRequest *requests;
	int max_requests;

	volatile HANDLE	hThreadDraw;
	HANDLE			hEventDraw;
	HANDLE			hEventDrawReturn;
	HANDLE			hEventAbort;
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
	static void drawThread(void *);
	bool DoRequest(AsyncBlitRequest *req);
	void drawThread2();
	HRGN CreateLacedRegion(int w, int h, int yo);

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
	void post(DWORD, HDRAWDIB, HDC, int, int, int, int, LPBITMAPINFOHEADER, LPVOID, int, int, int, int, UINT);
	void postBitBltLaced(DWORD id, HDC hdcDest, int xDst, int yDst, int dxDst, int dyDst, HDC hdcSrc, int xSrc, int ySrc, bool bFieldBDominant);
	void postStretchBlt(DWORD id, HDC hdcDest, int xDst, int yDst, int dxDst, int dyDst, HDC hdcSrc, int xSrc, int ySrc, int dxSrc, int dySrc);
	void postStretchDIBits(DWORD id, HDC hdc, int xDst, int yDst, int dxDst, int dyDst, int xSrc, int ySrc, int dxSrc, int dySrc, const void *pBits, const BITMAPINFO *pBitsInfo, UINT iUsage, DWORD dwRop);
	void postDirectDrawCopy(DWORD id, void *data, BITMAPINFOHEADER *pbih, IDDrawSurface *pDest);
	void postDirectDrawCopyLaced(DWORD id, void *data, BITMAPINFOHEADER *pbih, IDDrawSurface *pDest, bool bFieldBDominant);
	void postDirectDrawBlit(DWORD id, IDirectDrawSurface *pDst, IDDrawSurface *pSrc, int xDst, int yDst, int dxDst, int dyDst);
	void postDirectDrawBlitLaced(DWORD id, IDirectDrawSurface *pDst, IDDrawSurface *pSrc, int xDst, int yDst, int dxDst, int dyDst, bool bFieldBDominant);
	void postICDraw(DWORD id, HIC hic, DWORD dwFlags, LPVOID pFormat, LPVOID pData, DWORD cbData, LONG lTime);
	void postAFC(DWORD id, void (*pFunc)(void *), void *pData);
	void abort();
	void flush();

	bool ServiceRequests(bool fWait);
};

#endif
