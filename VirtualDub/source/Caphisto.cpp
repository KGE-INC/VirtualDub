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
#include <vfw.h>

#include "resource.h"
#include "oshelper.h"
#include "vbitmap.h"
#include "helpfile.h"
#include "Histogram.h"
#include <vd2/system/error.h>

#include "caphisto.h"

extern LRESULT CALLBACK CaptureStatusCallback(HWND hWnd, int nID, LPCSTR lpsz);

///////////////////////////////////////////////////////////////////////////
//
//	General capture histogram
//
///////////////////////////////////////////////////////////////////////////

CaptureHistogram::CaptureHistogram(HWND hwndCapture, HDC hdcExample, int max_height)
	:CaptureFrameSource(hwndCapture)
	,histo(hdcExample, max_height)
	{
}

CaptureHistogram::~CaptureHistogram() {
}

void CaptureHistogram::Process(VIDEOHDR *pvhdr) {
	const VBitmap& vbAnalyze = *Decompress(pvhdr);

	histo.Zero();

	if (vbAnalyze.depth == 32)
		histo.Process(&vbAnalyze);
	else if (vbAnalyze.depth == 24)
		histo.Process24(&vbAnalyze);
	else if (vbAnalyze.depth == 16)
		histo.Process16(&vbAnalyze);
}

void CaptureHistogram::Draw(HDC hdc, RECT& r) {
	histo.Draw(hdc, &r);
}
