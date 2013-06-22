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

#ifndef f_DDRAWSUP_H
#define f_DDRAWSUP_H

#include <windows.h>
#include <objbase.h>

DECLARE_INTERFACE(IDirectDraw2);
DECLARE_INTERFACE(IDirectDrawClipper);
DECLARE_INTERFACE(IDirectDrawSurface);

class VBitmap;

HRESULT InitCOM();

bool DDrawDetect();
BOOL DDrawInitialize(HWND);
void DDrawDeinitialize();
IDirectDraw2 *DDrawObtainInterface();
IDirectDrawSurface *DDrawObtainPrimary();

class __declspec(novtable) IDDrawSurface {
public:
	virtual ~IDDrawSurface() {}
	virtual bool Lock(VBitmap *pvbm)=0;
	virtual bool LockInverted(VBitmap *pvbm)=0;
	virtual void Unlock()=0;
	virtual void SetColorKey(COLORREF rgb)=0;
	virtual void MoveOverlay(long x, long y)=0;
	virtual void SetOverlayPos(RECT *pr)=0;

	virtual IDirectDrawSurface *getSurface()=0;
};

IDDrawSurface *CreateDDrawSurface(IDirectDrawSurface *lpdds);

#endif
