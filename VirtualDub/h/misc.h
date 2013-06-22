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

#ifndef f_VIRTUALDUB_MISC_H
#define f_VIRTUALDUB_MISC_H

#include <mmsystem.h>

#include <vd2/system/VDString.h>
#include <vd2/system/text.h>
#include <vd2/system/vdstl.h>

long MulDivTrunc(long a, long b, long c);
int NearestLongValue(long v, const long *array, int array_size);
unsigned __stdcall MulDivUnsigned(unsigned a, unsigned b, unsigned c);

// only works properly when d1,d2>0!!

long inline int64divto32(__int64 d1, __int64 d2) {
	return d2?(long)((d1+d2/2)/d2):0;
}

__int64 inline int64divround(__int64 d1, __int64 d2) {
	return d2?((d1+d2/2)/d2):0;
}

__int64 inline int64divroundup(__int64 d1, __int64 d2) {
	return d2?((d1+d2-1)/d2):0;
}

bool isEqualFOURCC(FOURCC fccA, FOURCC fccB);
bool isValidFOURCC(FOURCC fcc);
FOURCC toupperFOURCC(FOURCC fcc);

bool IsMMXState();
void ClearMMXState();

void VDClearEvilCPUStates();

void VDPreCheckExternalCodeCall(const char *file, int line);
void VDPostCheckExternalCodeCall(const wchar_t *mpContext, const char *mpFile, int mLine);

struct VDSilentExternalCodeBracket {
	VDSilentExternalCodeBracket() {
		VDClearEvilCPUStates();
	}

	~VDSilentExternalCodeBracket() {
		VDClearEvilCPUStates();
	}
};

struct VDExternalCodeBracket {
	VDExternalCodeBracket(const wchar_t *pContext, const char *file, const int line)
		: mpContext(pContext)
		, mpFile(file)
		, mLine(line)
	{
		VDPreCheckExternalCodeCall(file, line);
	}

	~VDExternalCodeBracket() {
		VDPostCheckExternalCodeCall(mpContext, mpFile, mLine);
	}

	const wchar_t *mpContext;
	const char *mpFile;
	const int mLine;
};

char *strCify(const char *s);
VDStringA VDEncodeScriptString(const VDStringW& sw);

///////////////////////////////////////////////////////////////////////////
//
//	pixmap <-> bitmap format converters
//
//	pixmap format:	describes channel layout, color space, subsampling
//	variant:		describes variations in layout and FOURCC that may
//					correspond to the same pixmap format
//	bitmap format:	Win32 BITMAPINFOHEADER
//
struct VDPixmapLayout;

int VDGetPixmapToBitmapVariants(int format);
int VDBitmapFormatToPixmapFormat(const BITMAPINFOHEADER& hdr);
int VDBitmapFormatToPixmapFormat(const BITMAPINFOHEADER& hdr, int& variant);
bool VDMakeBitmapFormatFromPixmapFormat(vdstructex<BITMAPINFOHEADER>& dst, const vdstructex<BITMAPINFOHEADER>& src, int format, int variant);
bool VDMakeBitmapFormatFromPixmapFormat(vdstructex<BITMAPINFOHEADER>& dst, const vdstructex<BITMAPINFOHEADER>& src, int format, int variant, uint32 w, uint32 h);
uint32 VDMakeBitmapCompatiblePixmapLayout(VDPixmapLayout& layout, uint32 w, uint32 h, int format, int variant);


#endif
