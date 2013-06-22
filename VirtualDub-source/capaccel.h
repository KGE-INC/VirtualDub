//	VirtualDub 2.0 (Nina) - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee, All Rights Reserved.
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

#ifndef f_VD2_RYDIA_ACCELDISPLAY_H
#define f_VD2_RYDIA_ACCELDISPLAY_H

#include <ddraw.h>

bool RydiaInitAVICapHotPatch();
void RydiaEnableAVICapPreview(bool b);
void RydiaEnableAVICapInvalidate(bool b);

class RydiaDirectDrawContext {
private:
	IDirectDraw2 *mpdd;
	IDirectDrawSurface3 *mpddsOverlay;
	IDirectDrawSurface3 *mpddsPrimary;

	int nOverlayBPP;
	int nAlignX;
	int nAlignW;
	int mnColorKey;

	bool mbSupportsColorKey;
	bool mbSupportsArithStretchY;
	bool mbCOMInitialized;

public:
	RydiaDirectDrawContext();
	~RydiaDirectDrawContext();

	int getColorKey() { return mnColorKey >= 0 ? 0xFFFF00 : -1; }

	bool Init();
	bool Shutdown();
	bool isReady();
	bool CreateOverlay(int w, int h, int bitcount, DWORD fcc);
	void DestroyOverlay();
	bool PositionOverlay(int x, int y, int w, int h);
	bool LockAndLoad(const void *src0, int yoffset, int yjump);
};

#endif
