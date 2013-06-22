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

#include <ctype.h>

#include <wtypes.h>
#include <mmsystem.h>

#include "misc.h"
#include <vd2/system/cpuaccel.h>
#include <vd2/system/filesys.h>
#include <vd2/system/log.h>
#include <vd2/system/registry.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>

#ifdef _M_IX86
	long __declspec(naked) MulDivTrunc(long a, long b, long c) {
		__asm {
			mov eax,[esp+4]
			imul dword ptr [esp+8]
			idiv dword ptr [esp+12]
			ret
		}
	}

	unsigned __declspec(naked) __stdcall MulDivUnsigned(unsigned a, unsigned b, unsigned c) {
		__asm {
			mov		eax,[esp+4]
			mov		ecx,[esp+12]
			mul		dword ptr [esp+8]
			shr		ecx,1
			add		eax,ecx
			adc		edx,0
			div		dword ptr [esp+12]
			ret		12
		}
	}
#else
	long MulDivTrunc(long a, long b, long c) {
		return (long)(((sint64)a * b) / c);
	}

	unsigned __stdcall MulDivUnsigned(unsigned a, unsigned b, unsigned c) {
		return (unsigned)(((uint64)a * b + 0x80000000) / c);
	}
#endif

int NearestLongValue(long v, const long *array, int array_size) {
	int i;

	for(i=1; i<array_size; i++)
		if (v*2 < array[i-1]+array[i])
			break;

	return i-1;
}

bool isEqualFOURCC(FOURCC fccA, FOURCC fccB) {
	int i;

	for(i=0; i<4; i++) {
		if (tolower((unsigned char)fccA) != tolower((unsigned char)fccB))
			return false;

		fccA>>=8;
		fccB>>=8;
	}

	return true;
}

bool isValidFOURCC(FOURCC fcc) {
	return isprint((unsigned char)(fcc>>24))
		&& isprint((unsigned char)(fcc>>16))
		&& isprint((unsigned char)(fcc>> 8))
		&& isprint((unsigned char)(fcc    ));
}

FOURCC toupperFOURCC(FOURCC fcc) {
	return(toupper((unsigned char)(fcc>>24)) << 24)
		| (toupper((unsigned char)(fcc>>16)) << 16)
		| (toupper((unsigned char)(fcc>> 8)) <<  8)
		| (toupper((unsigned char)(fcc    ))      );
}

#if defined(WIN32) && defined(_M_IX86)

	bool IsMMXState() {
		char	buf[28];
		unsigned short tagword;

		__asm fnstenv buf		// this resets the FPU control word somehow!?

		tagword = *(unsigned short *)(buf + 8);

		return (tagword != 0xffff);
	}

	bool IsFPUStateOK(unsigned& ctlword) {
		ctlword = 0;

		__asm mov eax, ctlword
		__asm fnstcw [eax]

		ctlword &= 0x0f3f;

		return ctlword == 0x023f;
	}

	void ClearMMXState() {
		if (MMX_enabled)
			__asm emms
		else {
			__asm {
				ffree st(0)
				ffree st(1)
				ffree st(2)
				ffree st(3)
				ffree st(4)
				ffree st(5)
				ffree st(6)
				ffree st(7)
			}
		}
	}

	void ResetFPUState() {
		static const unsigned ctlword = 0x027f;

		__asm fnclex
		__asm fldcw ctlword
	}

	void VDClearEvilCPUStates() {
		ResetFPUState();
		ClearMMXState();
	}

	void VDPreCheckExternalCodeCall(const char *file, int line) {
		unsigned fpucw;
		bool bFPUStateBad = !IsFPUStateOK(fpucw);
		bool bMMXStateBad = IsMMXState();

		if (bMMXStateBad || bFPUStateBad) {
			ClearMMXState();
			ResetFPUState();
		}

		if (bMMXStateBad) {
			VDLog(kVDLogError, VDswprintf(L"Internal error: MMX state was active before entry to external code at (%hs:%d). "
										L"This indicates an uncaught bug either in an external driver or in VirtualDub itself "
										L"that could cause application instability.  Please report this problem to the author!",
										2,
										&file,
										&line
										));
		}
		if (bFPUStateBad) {
#if 0		// Far too many drivers get this wrong... #@&*($@&#(*$
			VDLog(kVDLogError, VDswprintf(L"Internal error: Floating-point state was bad before entry to external code at (%hs:%d). "
										L"This indicates an uncaught bug either in an external driver or in VirtualDub itself "
										L"that could cause application instability.  Please report this problem to the author!\n"
										L"(FPU tag word = %04x)",
										3,
										&file,
										&line,
										&fpucw
										));
#endif
		}
	}

	void VDPostCheckExternalCodeCall(const wchar_t *mpContext, const char *mpFile, int mLine) {
		unsigned fpucw;
		bool bFPUStateBad = !IsFPUStateOK(fpucw);
		bool bMMXStateBad = IsMMXState();
		bool bBadState = bMMXStateBad || bFPUStateBad;

		static bool sbDisableFurtherWarnings = false;

		if (bBadState) {
			ClearMMXState();
			ResetFPUState();
		}

		if (sbDisableFurtherWarnings)
			return;

		if (bMMXStateBad) {
			VDLog(kVDLogWarning, VDswprintf(L"%ls returned to VirtualDub with the CPU's MMX unit still active. "
											L"This indicates a bug in that module which could cause application instability. "
											L"Please check with the module vendor for an updated version which addresses this problem. "
											L"(Trap location: %hs:%d)",
											3,
											&mpContext,
											&mpFile,
											&mLine));
			sbDisableFurtherWarnings = true;
		}
		if (bFPUStateBad) {
#if 0		// Far too many drivers get this wrong... #@&*($@&#(*$
			VDLog(kVDLogWarning, VDswprintf(L"%ls returned to VirtualDub with the floating-point unit in an abnormal state. "
											L"This indicates a bug in that module which could cause application instability. "
											L"Please check with the module vendor for an updated version which addresses this problem. "
											L"(Trap location: %hs:%d, FPUCW = %04x)",
											4,
											&mpContext,
											&mpFile,
											&mLine,
											&fpucw));
#endif
			sbDisableFurtherWarnings = true;
		}
	}

#else

	bool IsMMXState() {
		return false;
	}

	void ClearMMXState() {
	}

	void VDClearEvilCPUStates() {
	}

	void VDPreCheckExternalCodeCall(const char *file, int line) {
	}

	void VDPostCheckExternalCodeCall(const wchar_t *mpContext, const char *mpFile, int mLine) {
	}

#endif

char *strCify(const char *s) {
	static char buf[2048];
	char c,*t = buf;

	while(c=*s++) {
		if (!isprint((unsigned char)c))
			t += sprintf(t, "\\x%02x", (int)c & 0xff);
		else {
			if (c=='"' || c=='\\')
				*t++ = '\\';
			*t++ = c;
		}
	}
	*t=0;

	return buf;
}

VDStringA VDEncodeScriptString(const VDStringW& sw) {
	const VDStringA sa(VDTextWToU8(sw));
	VDStringA out;

	// this is not very fast, but it's only used during script serialization
	for(VDStringA::const_iterator it(sa.begin()), itEnd(sa.end()); it != itEnd; ++it) {
		char c = *it;
		char buf[16];

		if (!isprint((unsigned char)c)) {
			sprintf(buf, "\\x%02x", (int)c & 0xff);
			out.append(buf);
		} else {
			if (c == '"' || c=='\\')
				out += '\\';
			out += c;
		}
	}

	return out;
}

int VDBitmapFormatToPixmapFormat(const BITMAPINFOHEADER& hdr) {
	int variant;

	return VDBitmapFormatToPixmapFormat(hdr, variant);
}

int VDBitmapFormatToPixmapFormat(const BITMAPINFOHEADER& hdr, int& variant) {
	using namespace nsVDPixmap;

	variant = 1;

	switch(hdr.biCompression) {
	case BI_RGB:
		if (hdr.biPlanes == 1) {
			if (hdr.biBitCount == 16)
				return kPixFormat_XRGB1555;
			else if (hdr.biBitCount == 24)
				return kPixFormat_RGB888;
			else if (hdr.biBitCount == 32)
				return kPixFormat_XRGB8888;
		}
		break;
	case BI_BITFIELDS:
		{
			const BITMAPV4HEADER& v4hdr = (const BITMAPV4HEADER&)hdr;
			const int bits = v4hdr.bV4BitCount;
			const uint32 r = v4hdr.bV4RedMask;
			const uint32 g = v4hdr.bV4GreenMask;
			const uint32 b = v4hdr.bV4BlueMask;

			if (bits == 16 && r == 0x7c00 && g == 0x03e0 && b == 0x001f)
				return kPixFormat_XRGB1555;
			else if (bits == 16 && r == 0xf800 && g == 0x07c0 && b == 0x001f)
				return kPixFormat_RGB565;
			else if (bits == 24 && r == 0xff0000 && g == 0x00ff00 && b == 0x0000ff)
				return kPixFormat_RGB888;
			else if (bits == 32 && r == 0xff0000 && g == 0x00ff00 && b == 0x0000ff)
				return kPixFormat_XRGB8888;
		}
		break;
	case 'YVYU':
		return kPixFormat_YUV422_UYVY;
	case '2YUY':
		return kPixFormat_YUV422_YUYV;
	case '21VY':
		return kPixFormat_YUV420_Planar;
	case '024I':
		variant = 2;
		return kPixFormat_YUV420_Planar;
	case 'VUYI':
		variant = 3;
		return kPixFormat_YUV420_Planar;
	case '  8Y':
		return kPixFormat_Y8;
	}
	return 0;
}

int VDGetPixmapToBitmapVariants(int format) {
	if (format == nsVDPixmap::kPixFormat_YUV420_Planar)
		return 3;

	return 1;
}

bool VDMakeBitmapFormatFromPixmapFormat(vdstructex<BITMAPINFOHEADER>& dst, const vdstructex<BITMAPINFOHEADER>& src, int format, int variant) {
	return VDMakeBitmapFormatFromPixmapFormat(dst, src, format, variant, src->biWidth, src->biHeight);
}

bool VDMakeBitmapFormatFromPixmapFormat(vdstructex<BITMAPINFOHEADER>& dst, const vdstructex<BITMAPINFOHEADER>& src, int format, int variant, uint32 w, uint32 h) {
	using namespace nsVDPixmap;

	dst = src;
	dst->biSize				= sizeof(BITMAPINFOHEADER);
	dst->biWidth			= w;
	dst->biHeight			= h;
	dst->biPlanes			= 1;
	dst->biXPelsPerMeter	= src->biXPelsPerMeter;
	dst->biYPelsPerMeter	= src->biYPelsPerMeter;

	if (format == kPixFormat_Pal8) {
		dst->biBitCount		= 8;
		dst->biCompression	= BI_RGB;
		dst->biSizeImage	= ((w+3)&~3)*h;
		return true;
	}

	dst.resize(sizeof(BITMAPINFOHEADER));

	dst->biClrUsed			= 0;
	dst->biClrImportant		= 0;

	switch(format) {
	case kPixFormat_XRGB1555:
		dst->biCompression	= BI_RGB;
		dst->biBitCount		= 16;
		dst->biSizeImage	= ((w*2+3)&~3) * h;
		break;
	case kPixFormat_RGB565:
		dst->biCompression	= BI_BITFIELDS;
		dst->biBitCount		= 16;
		dst->biSizeImage	= ((w*2+3)&~3) * h;
		dst.resize(sizeof(BITMAPINFOHEADER) + 3*sizeof(DWORD));
		{
			DWORD *fields = (DWORD *)(dst.data() + 1);
			fields[0] = 0xf800;
			fields[1] = 0x07c0;
			fields[2] = 0x001f;
		}
		break;
	case kPixFormat_RGB888:
		dst->biCompression	= BI_RGB;
		dst->biBitCount		= 24;
		dst->biSizeImage	= ((w*3+3)&~3) * h;
		break;
	case kPixFormat_XRGB8888:
		dst->biCompression	= BI_RGB;
		dst->biBitCount		= 32;
		dst->biSizeImage	= w*4 * h;
		break;
	case kPixFormat_YUV422_UYVY:
		dst->biCompression	= 'YVYU';
		dst->biBitCount		= 16;
		dst->biSizeImage	= ((w+1)&~1)*2*h;
		break;
	case kPixFormat_YUV422_YUYV:
		dst->biCompression	= '2YUY';
		dst->biBitCount		= 16;
		dst->biSizeImage	= ((w+1)&~1)*2*h;
		break;
	case kPixFormat_YUV422_Planar:
		dst->biCompression	= '61VY';
		dst->biBitCount		= 16;
		dst->biSizeImage	= ((w+1)>>1) * h * 4;
		break;
	case kPixFormat_YUV420_Planar:
		switch(variant) {
		case 3:
			dst->biCompression	= 'VUYI';
			break;
		case 2:
			dst->biCompression	= '024I';
			break;
		case 1:
		default:
			dst->biCompression	= '21VY';
			break;
		}
		dst->biBitCount		= 12;
		dst->biSizeImage	= w*h + (w>>1)*(h>>1)*2;
		break;
	case kPixFormat_YUV410_Planar:
		dst->biCompression	= '9UVY';
		dst->biBitCount		= 9;
		dst->biSizeImage	= ((w+2)>>2) * ((h+2)>>2) * 18;
		break;
	case kPixFormat_Y8:
		dst->biCompression	= '  8Y';
		dst->biBitCount		= 8;
		dst->biSizeImage	= ((w+3) & ~3) * h;
		break;
	default:
		return false;
	};

	return true;
}

uint32 VDMakeBitmapCompatiblePixmapLayout(VDPixmapLayout& layout, uint32 w, uint32 h, int format, int variant) {
	using namespace nsVDPixmap;

	uint32 linspace = VDPixmapCreateLinearLayout(layout, format, w, h, VDPixmapGetInfo(format).auxbufs > 1 ? 1 : 4);

	switch(format) {
	case kPixFormat_Pal8:
	case kPixFormat_XRGB1555:
	case kPixFormat_RGB888:
	case kPixFormat_RGB565:
	case kPixFormat_XRGB8888:
		layout.data += layout.pitch * (h-1);
		layout.pitch = -layout.pitch;
		break;
	case kPixFormat_YUV420_Planar:
		if (variant < 2) {				// need to swap UV planes for YV12 (1)
			std::swap(layout.data2, layout.data3);
			std::swap(layout.pitch2, layout.pitch3);
		}
		break;
	}

	return linspace;
}


HMODULE VDLoadVTuneDLLW32() {
	VDRegistryKey key("SOFTWARE\\Intel Corporation\\VTune(TM) Performance Environment\\6.0", true);

	if (key.isReady()) {
		VDStringW path;
		if (key.getString("SharedBaseInstallDir", path)) {
			const VDStringW path2(VDMakePath(path, VDStringW(L"Analyzer\\Bin\\VTuneAPI.dll")));

			return LoadLibraryW(path2.c_str());
		}
	}

	return NULL;
}

extern bool g_bEnableVTuneProfiling;
void VDEnableSampling(bool bEnable) {
	if (g_bEnableVTuneProfiling) {
		static HMODULE hmodVTuneAPI = VDLoadVTuneDLLW32();
		if (!hmodVTuneAPI)
			return;

		static void (__cdecl *pVTunePauseSampling)() = (void(__cdecl*)())GetProcAddress(hmodVTuneAPI, "VTPauseSampling");
		static void (__cdecl *pVTuneResumeSampling)() = (void(__cdecl*)())GetProcAddress(hmodVTuneAPI, "VTResumeSampling");

		(bEnable ? pVTuneResumeSampling : pVTunePauseSampling)();
	}
}
