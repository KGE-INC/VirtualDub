//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2004 Avery Lee
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

#ifndef f_VD2_RIZA_VIDEOCODEC_H
#define f_VD2_RIZA_VIDEOCODEC_H

#include <vd2/system/vdtypes.h>

class VDINTERFACE IVDVideoDecompressor {
public:
	virtual ~IVDVideoDecompressor() {}
	virtual bool QueryTargetFormat(int format) = 0;
	virtual bool QueryTargetFormat(const void *format) = 0;
	virtual bool SetTargetFormat(int format) = 0;
	virtual bool SetTargetFormat(const void *format) = 0;
	virtual int GetTargetFormat() = 0;
	virtual int GetTargetFormatVariant() = 0;
	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual void DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll) = 0;
	virtual const void *GetRawCodecHandlePtr() = 0;		// (HIC *) on Win32
	virtual const wchar_t *GetName() = 0;
};

IVDVideoDecompressor *VDCreateVideoDecompressorVCM(const void *srcFormat, const void *pHIC);
IVDVideoDecompressor *VDCreateVideoDecompressorDV(int w, int h);

IVDVideoDecompressor *VDFindVideoDecompressor(uint32 preferredCodec, const void *srcFormat);

class VDINTERFACE IVDVideoCodecBugTrap {
public:
	virtual void OnCodecRenamingDetected(const wchar_t *pName) = 0;	// Called when codec modifies input format.
	virtual void OnAcceptedBS(const wchar_t *pName) = 0;			// Called when codec accepts BS FOURCC.
};

void VDSetVideoCodecBugTrap(IVDVideoCodecBugTrap *);

#endif
