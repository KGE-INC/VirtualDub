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

#ifndef f_VIRTUALDUB_FILTERSYSTEM_H
#define f_VIRTUALDUB_FILTERSYSTEM_H

#include <windows.h>

#include <vd2/system/list.h>
#include "vbitmap.h"
#include "filter.h"

class FilterActivation;
class FilterInstance;
class FilterStateInfo;
class FilterSystemBitmap;

class FilterSystem {
private:
	DWORD dwFlags;
	int iBitmapCount;

	enum {
		FILTERS_INITIALIZED = 0x00000001L,
		FILTERS_ERROR		= 0x00000002L,
	};

	FilterStateInfo	mfsi;
	FilterSystemBitmap *bitmap;
	FilterSystemBitmap *bmLast;
	List *listFilters;
	int nFrameLag;
	int mFrameDelayLeft;
	bool mbFirstFrame;

	HBITMAP hbmSrc;
	HDC		hdcSrc;
	HGDIOBJ	hgoSrc;

	HANDLE hFileShared;
	unsigned char *lpBuffer;
	long lAdditionalBytes;
	bool fSharedWindow;

	void AllocateVBitmaps(int count);
	void AllocateBuffers(LONG lTotalBufferNeeded);

public:
	FilterSystem();
	~FilterSystem();
	void prepareLinearChain(List *listFA, Pixel *src_pal, PixDim src_width, PixDim src_height, int dest_depth);
	void initLinearChain(List *listFA, Pixel *src_pal, PixDim src_width, PixDim src_height, int dest_depth);
	int ReadyFilters(const FilterStateInfo&);
	void RestartFilters();
	bool RunFilters(const FilterStateInfo&, FilterInstance *pfiStopPoint = NULL);
	void DeinitFilters();
	void DeallocateBuffers();
	VBitmap *InputBitmap();
	VBitmap *OutputBitmap();
	VBitmap *LastBitmap();
	bool isRunning();
	bool isEmpty() const { return listFilters->IsEmpty(); }

	int getFrameLag();

	bool getOutputMappingParams(HANDLE&, LONG&);

	bool IsFiltered(VDPosition frame) const;
};

#endif
