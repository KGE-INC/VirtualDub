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
#include <vector>

#include "VideoSource.h"
#include "VBitmap.h"
#include "AVIStripeSystem.h"
#include "ProgressDialog.h"
#include "MJPEGDecoder.h"
#include "crash.h"

#include <vd2/system/error.h>
#include <vd2/system/text.h>
#include <vd2/system/log.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/Dita/resources.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "misc.h"
#include "oshelper.h"
#include "helpfile.h"
#include "resource.h"

#include <vd2/Meia/MPEGIDCT.h>

#if defined(_M_AMD64)
	#define VDPROT_PTR	"%p"
#else
	#define VDPROT_PTR	"%08x"
#endif

///////////////////////////

extern const char *LookupVideoCodec(FOURCC);

extern HINSTANCE g_hInst;
extern HWND g_hWnd;

///////////////////////////

namespace {
	enum { kVDST_VideoSource = 3 };

	enum {
		kVDM_ResumeFromConceal,
		kVDM_DecodingError,
		kVDM_FrameTooShort,
		kVDM_CodecMMXError,
		kVDM_FixingHugeVideoFormat,
		kVDM_CodecRenamingDetected,
		kVDM_CodecAcceptsBS
	};
}

///////////////////////////

static bool CheckMPEG4Codec(HIC hic, bool isV3) {
	char frame[0x380];
	BITMAPINFOHEADER bih;

	// Form a completely black frame if it's V3.

	bih.biSize			= 40;
	bih.biWidth			= 320;
	bih.biHeight		= 240;
	bih.biPlanes		= 1;
	bih.biBitCount		= 24;
	bih.biCompression	= '24PM';
	bih.biSizeImage		= 0;
	bih.biXPelsPerMeter	= 0;
	bih.biYPelsPerMeter	= 0;
	bih.biClrUsed		= 0;
	bih.biClrImportant	= 0;

	if (isV3) {
		int i;

		frame[0] = (char)0x3f;
		frame[1] = (char)0x71;
		frame[2] = (char)0x1b;
		frame[3] = (char)0x7c;

		for(i=4; i<0x179; i+=5) {
			frame[i+0] = (char)0x2f;
			frame[i+1] = (char)0x0b;
			frame[i+2] = (char)0xc2;
			frame[i+3] = (char)0xf0;
			frame[i+4] = (char)0xbc;
		}

		frame[0x179] = (char)0xf0;
		frame[0x17a] = (char)0xb8;
		frame[0x17b] = (char)0x01;

		bih.biCompression	= '34PM';
		bih.biSizeImage		= 0x17c;
	}

	// Attempt to decompress.

	HANDLE h;

	{
		VDSilentExternalCodeBracket bracket;
		h = ICImageDecompress(hic, 0, (BITMAPINFO *)&bih, frame, NULL);
	}

	if (h) {
		GlobalFree(h);
		return true;
	} else {
		return false;
	}
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDecompressorMJPEG : public IVDVideoDecompressor {
public:
	VDVideoDecompressorMJPEG();
	~VDVideoDecompressorMJPEG();

	void Init(int w, int h);

	bool QueryTargetFormat(int format);
	bool QueryTargetFormat(const void *format);
	bool SetTargetFormat(int format);
	bool SetTargetFormat(const void *format);
	int GetTargetFormat() { return mFormat; }
	int GetTargetFormatVariant() { return 0; }
	void Start();
	void Stop();
	void DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll);
	const void *GetRawCodecHandlePtr();
	const wchar_t *GetName();

protected:
	int		mWidth, mHeight;
	int		mFormat;

	vdautoptr<IMJPEGDecoder>	mpDecoder;
};

VDVideoDecompressorMJPEG::VDVideoDecompressorMJPEG()
	: mFormat(0)
{
}

VDVideoDecompressorMJPEG::~VDVideoDecompressorMJPEG() {
}

void VDVideoDecompressorMJPEG::Init(int w, int h) {
	mWidth = w;
	mHeight = h;

	mpDecoder = CreateMJPEGDecoder(w, h);
}

bool VDVideoDecompressorMJPEG::QueryTargetFormat(int format) {
	return format == nsVDPixmap::kPixFormat_XRGB1555
		|| format == nsVDPixmap::kPixFormat_XRGB8888
		|| format == nsVDPixmap::kPixFormat_YUV422_UYVY
		|| format == nsVDPixmap::kPixFormat_YUV422_YUYV;
}

bool VDVideoDecompressorMJPEG::QueryTargetFormat(const void *format) {
	const BITMAPINFOHEADER& hdr = *(const BITMAPINFOHEADER *)format;

	if (hdr.biWidth != mWidth || hdr.biHeight != mHeight)
		return false;

	int pxformat = VDBitmapFormatToPixmapFormat(hdr);

	return QueryTargetFormat(pxformat);
}

bool VDVideoDecompressorMJPEG::SetTargetFormat(int format) {
	if (!format)
		format = nsVDPixmap::kPixFormat_XRGB8888;

	if (QueryTargetFormat(format)) {
		mFormat = format;
		return true;
	}

	return false;
}

bool VDVideoDecompressorMJPEG::SetTargetFormat(const void *format) {
	const BITMAPINFOHEADER& hdr = *(const BITMAPINFOHEADER *)format;

	if (hdr.biWidth != mWidth || hdr.biHeight != mHeight)
		return false;

	int pxformat = VDBitmapFormatToPixmapFormat(hdr);

	if (QueryTargetFormat(pxformat)) {
		mFormat = pxformat;
		return true;
	}

	return false;
}

void VDVideoDecompressorMJPEG::Start() {
	if (!mFormat)
		throw MyError("Cannot find compatible target format for video decompression.");
}

void VDVideoDecompressorMJPEG::Stop() {
}

void VDVideoDecompressorMJPEG::DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll) {
	switch(mFormat) {
	case nsVDPixmap::kPixFormat_XRGB1555:
		mpDecoder->decodeFrameRGB15((unsigned long *)dst, (unsigned char *)src, srcSize);
		break;
	case nsVDPixmap::kPixFormat_XRGB8888:
		mpDecoder->decodeFrameRGB32((unsigned long *)dst, (unsigned char *)src, srcSize);
		break;
	case nsVDPixmap::kPixFormat_YUV422_UYVY:
		mpDecoder->decodeFrameUYVY((unsigned long *)dst, (unsigned char *)src, srcSize);
		break;
	case nsVDPixmap::kPixFormat_YUV422_YUYV:
		mpDecoder->decodeFrameYUY2((unsigned long *)dst, (unsigned char *)src, srcSize);
		break;
	default:
		throw MyError("Cannot find compatible target format for video decompression.");
	}
}

const void *VDVideoDecompressorMJPEG::GetRawCodecHandlePtr() {
	return NULL;
}

const wchar_t *VDVideoDecompressorMJPEG::GetName() {
	return L"Internal Motion JPEG decoder";
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDecompressorDV : public IVDVideoDecompressor {
public:
	VDVideoDecompressorDV();
	~VDVideoDecompressorDV();

	void Init(int w, int h);

	bool QueryTargetFormat(int format);
	bool QueryTargetFormat(const void *format);
	bool SetTargetFormat(int format);
	bool SetTargetFormat(const void *format);
	int GetTargetFormat() { return mFormat; }
	int GetTargetFormatVariant() { return 0; }
	void Start();
	void Stop();
	void DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll);
	const void *GetRawCodecHandlePtr();
	const wchar_t *GetName();

protected:
	int		mWidth, mHeight;
	int		mFormat;

	uint8	mYPlane[576][736];

	union {
		struct {
			uint8	mCrPlane[480][184];
			uint8	mCbPlane[480][184];
		} m411;
		struct {
			uint8	mCrPlane[288][368];
			uint8	mCbPlane[288][368];
		} m420;
	};
};

// 10 DIF sequences (NTSC) or 12 DIF sequences (PAL)
// Each DIF sequence contains:
//		1 DIF block		header
//		2 DIF blocks	subcode
//		3 DIF blocks	VAUX
//		9 DIF blocks	audio
//		135 DIF blocks	video
//
// Each DIF block has a 3 byte header and 77 bytes of payload.

VDVideoDecompressorDV::VDVideoDecompressorDV()
	: mFormat(0)
{
}

VDVideoDecompressorDV::~VDVideoDecompressorDV() {
}

void VDVideoDecompressorDV::Init(int w, int h) {
	mWidth = w;
	mHeight = h;
}

bool VDVideoDecompressorDV::QueryTargetFormat(int format) {
	return format > 0;
}

bool VDVideoDecompressorDV::QueryTargetFormat(const void *format) {
	const BITMAPINFOHEADER& hdr = *(const BITMAPINFOHEADER *)format;

	if (hdr.biWidth != mWidth || hdr.biHeight != mHeight)
		return false;

	int pxformat = VDBitmapFormatToPixmapFormat(hdr);

	return QueryTargetFormat(pxformat);
}

bool VDVideoDecompressorDV::SetTargetFormat(int format) {
	if (!format)
		format = nsVDPixmap::kPixFormat_YUV422_YUYV;

	if (QueryTargetFormat(format)) {
		mFormat = format;
		return true;
	}

	return false;
}

bool VDVideoDecompressorDV::SetTargetFormat(const void *format) {
	const BITMAPINFOHEADER& hdr = *(const BITMAPINFOHEADER *)format;

	if (hdr.biWidth != mWidth || hdr.biHeight != mHeight)
		return false;

	int pxformat = VDBitmapFormatToPixmapFormat(hdr);

	if (QueryTargetFormat(pxformat)) {
		mFormat = pxformat;
		return true;
	}

	return false;
}

void VDVideoDecompressorDV::Start() {
	if (!mFormat)
		throw MyError("Cannot find compatible target format for video decompression.");
}

void VDVideoDecompressorDV::Stop() {
}

namespace {

#define	TIMES_2x(v1,v2,v3) v1,v2,v3,v1,v2,v3
#define	TIMES_4x(v1,v2,v3) v1,v2,v3,v1,v2,v3,v1,v2,v3,v1,v2,v3
#define	TIMES_8x(v1,v2,v3) v1,v2,v3,v1,v2,v3,v1,v2,v3,v1,v2,v3,v1,v2,v3,v1,v2,v3,v1,v2,v3,v1,v2,v3

#define VLC_3(run,amp) TIMES_8x({run+1,3,(amp<<7)}),TIMES_8x({run+1,3,-(amp<<7)})
#define VLC_4(run,amp) TIMES_4x({run+1,4,(amp<<7)}),TIMES_4x({run+1,4,-(amp<<7)})
#define REP_4(run,amp) TIMES_4x({run+1,4,(amp<<7)})
#define VLC_5(run,amp) TIMES_2x({run+1,5,(amp<<7)}),TIMES_2x({run+1,5,-(amp<<7)})
#define VLC_6(run,amp) {run+1,6,(amp<<7)},{run+1,6,-(amp<<7)}

#define VLC_7(run,amp) TIMES_2x({run+1,7,(amp<<7)}),TIMES_2x({run+1,7,-(amp<<7)})
#define VLC_8(run,amp) {run+1,8,(amp<<7)},{run+1,8,-(amp<<7)}

#define VLC_9(run,amp) {run+1,9,(amp<<7)},{run+1,9,-(amp<<7)}

#define VLC_10(run,amp) {run+1,10,(amp<<7)},{run+1,10,-(amp<<7)}

#define VLC_11(run,amp) TIMES_4x({run+1,11,(amp<<7)}),TIMES_4x({run+1,11,-(amp<<7)})
#define REP_11(run,amp) TIMES_4x({run+1,11,(amp<<7)})
#define VLC_12(run,amp) TIMES_2x({run+1,12,(amp<<7)}),TIMES_2x({run+1,12,-(amp<<7)})
#define REP_12(run,amp) TIMES_2x({run+1,12,(amp<<7)})
#define VLC_13(run,amp) {run+1,13,(amp<<7)},{run+1,13,-(amp<<7)}

#define REP_13(run,amp) {run+1,13,(amp<<7)}

#define REP_16(run,amp) {run+1,16,(amp<<7)}

	struct VLCEntry {
		uint8	run;
		uint8	len;
		sint16	coeff;
	};

	static const VLCEntry kDVACDecode1[48]={
						// xxxxxx
		VLC_3(0,1),		// 00s
		VLC_4(0,2),		// 010s
		REP_4(64,0),	// 0110
		VLC_5(1,1),		// 0111s
		VLC_5(0,3),		// 1000s
		VLC_5(0,4),		// 1001s
		VLC_6(2,1),		// 10100s
		VLC_6(1,2),		// 10101s
		VLC_6(0,5),		// 10110s
		VLC_6(0,6),		// 10111s
	};

	static const VLCEntry kDVACDecode2[32]={
						// 11xxxxxx
		VLC_7(3,1),		// 110000s
		VLC_7(4,1),		// 110001s
		VLC_7(0,7),		// 110010s
		VLC_7(0,8),		// 110011s
		VLC_8(5,1),		// 1101000s
		VLC_8(6,1),		// 1101001s
		VLC_8(2,2),		// 1101010s
		VLC_8(1,3),		// 1101011s
		VLC_8(1,4),		// 1101100s
		VLC_8(0,9),		// 1101101s
		VLC_8(0,10),	// 1101110s
		VLC_8(0,11),	// 1101111s
	};

	static const VLCEntry kDVACDecode3[32]={
						// 111xxxxxx
		VLC_9(7,1),		// 11100000s
		VLC_9(8,1),		// 11100001s
		VLC_9(9,1),		// 11100010s
		VLC_9(10,1),	// 11100011s
		VLC_9(3,2),		// 11100100s
		VLC_9(4,2),		// 11100101s
		VLC_9(2,3),		// 11100110s
		VLC_9(1,5),		// 11100111s
		VLC_9(1,6),		// 11101000s
		VLC_9(1,7),		// 11101001s
		VLC_9(0,12),	// 11101010s
		VLC_9(0,13),	// 11101011s
		VLC_9(0,14),	// 11101100s
		VLC_9(0,15),	// 11101101s
		VLC_9(0,16),	// 11101110s
		VLC_9(0,17),	// 11101111s
	};

	static const VLCEntry kDVACDecode4[32]={
						// 1111xxxxxx
		VLC_10(11,1),	// 111100000s
		VLC_10(12,1),	// 111100001s
		VLC_10(13,1),	// 111100010s
		VLC_10(14,1),	// 111100011s
		VLC_10(5,2),	// 111100100s
		VLC_10(6,2),	// 111100101s
		VLC_10(3,3),	// 111100110s
		VLC_10(4,3),	// 111100111s
		VLC_10(2,4),	// 111101000s
		VLC_10(2,5),	// 111101001s
		VLC_10(1,8),	// 111101010s
		VLC_10(0,18),	// 111101011s
		VLC_10(0,19),	// 111101100s
		VLC_10(0,20),	// 111101101s
		VLC_10(0,21),	// 111101110s
		VLC_10(0,22),	// 111101111s
	};

	static const VLCEntry kDVACDecode5[192]={
						// 111110xxxxxxx
		VLC_11(5,3),	// 1111100000s
		VLC_11(3,4),	// 1111100001s
		VLC_11(3,5),	// 1111100010s
		VLC_11(2,6),	// 1111100011s
		VLC_11(1,9),	// 1111100100s
		VLC_11(1,10),	// 1111100101s
		VLC_11(1,11),	// 1111100110s
		REP_11(0,0),	// 11111001110
		REP_11(1,0),	// 11111001111
		VLC_12(6,3),	// 11111010000s
		VLC_12(4,4),	// 11111010001s
		VLC_12(3,6),	// 11111010010s
		VLC_12(1,12),	// 11111010011s
		VLC_12(1,13),	// 11111010100s
		VLC_12(1,14),	// 11111010101s
		REP_12(2,0),	// 111110101100
		REP_12(3,0),	// 111110101101
		REP_12(4,0),	// 111110101110
		REP_12(5,0),	// 111110101111
		VLC_13(7,2),	// 111110110000s
		VLC_13(8,2),	// 111110110001s
		VLC_13(9,2),	// 111110110010s
		VLC_13(10,2),	// 111110110011s
		VLC_13(7,3),	// 111110110100s
		VLC_13(8,3),	// 111110110101s
		VLC_13(4,5),	// 111110110110s
		VLC_13(3,7),	// 111110110111s
		VLC_13(2,7),	// 111110111000s
		VLC_13(2,8),	// 111110111001s
		VLC_13(2,9),	// 111110111010s
		VLC_13(2,10),	// 111110111011s
		VLC_13(2,11),	// 111110111100s
		VLC_13(1,15),	// 111110111101s
		VLC_13(1,16),	// 111110111110s
		VLC_13(1,17),	// 111110111111s
		REP_13(0,0),	// 1111110000000
		REP_13(1,0),	// 1111110000001
		REP_13(2,0),	// 1111110000010
		REP_13(3,0),	// 1111110000011
		REP_13(4,0),	// 1111110000100
		REP_13(5,0),	// 1111110000101
		REP_13(6,0),	// 1111110000110
		REP_13(7,0),	// 1111110000111
		REP_13(8,0),	// 1111110001000
		REP_13(9,0),	// 1111110001001
		REP_13(10,0),	// 1111110001010
		REP_13(11,0),	// 1111110001011
		REP_13(12,0),	// 1111110001100
		REP_13(13,0),	// 1111110001101
		REP_13(14,0),	// 1111110001110
		REP_13(15,0),	// 1111110001111
		REP_13(16,0),	// 1111110010000
		REP_13(17,0),	// 1111110010001
		REP_13(18,0),	// 1111110010010
		REP_13(19,0),	// 1111110010011
		REP_13(20,0),	// 1111110010100
		REP_13(21,0),	// 1111110010101
		REP_13(22,0),	// 1111110010110
		REP_13(23,0),	// 1111110010111
		REP_13(24,0),	// 1111110011000
		REP_13(25,0),	// 1111110011001
		REP_13(26,0),	// 1111110011010
		REP_13(27,0),	// 1111110011011
		REP_13(28,0),	// 1111110011100
		REP_13(29,0),	// 1111110011101
		REP_13(30,0),	// 1111110011110
		REP_13(31,0),	// 1111110011111
		REP_13(32,0),	// 1111110100000
		REP_13(33,0),	// 1111110100001
		REP_13(34,0),	// 1111110100010
		REP_13(35,0),	// 1111110100011
		REP_13(36,0),	// 1111110100100
		REP_13(37,0),	// 1111110100101
		REP_13(38,0),	// 1111110100110
		REP_13(39,0),	// 1111110100111
		REP_13(40,0),	// 1111110101000
		REP_13(41,0),	// 1111110101001
		REP_13(42,0),	// 1111110101010
		REP_13(43,0),	// 1111110101011
		REP_13(44,0),	// 1111110101100
		REP_13(45,0),	// 1111110101101
		REP_13(46,0),	// 1111110101110
		REP_13(47,0),	// 1111110101111
		REP_13(48,0),	// 1111110110000
		REP_13(49,0),	// 1111110110001
		REP_13(50,0),	// 1111110110010
		REP_13(51,0),	// 1111110110011
		REP_13(52,0),	// 1111110110100
		REP_13(53,0),	// 1111110110101
		REP_13(54,0),	// 1111110110110
		REP_13(55,0),	// 1111110110111
		REP_13(56,0),	// 1111110111000
		REP_13(57,0),	// 1111110111001
		REP_13(58,0),	// 1111110111010
		REP_13(59,0),	// 1111110111011
		REP_13(60,0),	// 1111110111100
		REP_13(61,0),	// 1111110111101
		REP_13(62,0),	// 1111110111110
		REP_13(63,0),	// 1111110111111
	};

	static const VLCEntry kDVACDecode8[512]={
#define R2(y) {1,16,((y)<<7)},{1,16,-((y)<<7)}
#define R(x) R2(x+0),R2(x+1),R2(x+2),R2(x+3),R2(x+4),R2(x+5),R2(x+6),R2(x+7),R2(x+8),R2(x+9),R2(x+10),R2(x+11),R2(x+12),R2(x+13),R2(x+14),R2(x+15)
		R(0x00),
		R(0x10),
		R(0x20),
		R(0x30),
		R(0x40),
		R(0x50),
		R(0x60),
		R(0x70),
		R(0x80),
		R(0x90),
		R(0xA0),
		R(0xB0),
		R(0xC0),
		R(0xD0),
		R(0xE0),
		R(0xF0),
#undef R
#undef R2
	};

	const VLCEntry *DVDecodeAC(uint32 bitheap) {
		if (bitheap < 0xC0000000)
			return &kDVACDecode1[bitheap >> 26];

		if (bitheap < 0xE0000000)
			return &kDVACDecode2[(bitheap >> 24) - (0xC0000000 >> 24)];

		if (bitheap < 0xF0000000)
			return &kDVACDecode3[(bitheap >> 23) - (0xE0000000 >> 23)];

		if (bitheap < 0xF8000000)
			return &kDVACDecode4[(bitheap >> 22) - (0xF0000000 >> 22)];

		if (bitheap < 0xFE000000)
			return &kDVACDecode5[(bitheap >> 19) - (0xF8000000 >> 19)];

		return &kDVACDecode8[(bitheap >> 16) - (0xFE000000 >> 16)];
	}

	static const int zigzag_std[64]={
		 0,  1,  8, 16,  9,  2,  3, 10,
		17, 24, 32, 25, 18, 11,  4,  5,
		12, 19, 26, 33, 40, 48, 41, 34,
		27, 20, 13,  6,  7, 14, 21, 28,
		35, 42, 49, 56, 57, 50, 43, 36,
		29, 22, 15, 23, 30, 37, 44, 51,
		58, 59, 52, 45, 38, 31, 39, 46,
		53, 60, 61, 54, 47, 55, 62, 63,
	};

	static const int zigzag_alt[64]={
		 0,  8,  1,  9, 16, 24,  2, 10,
		17, 25, 32, 40, 48, 56, 33, 41,
		18, 26,  3, 11,  4, 12, 19, 27,
		34, 42, 49, 57, 50, 58, 35, 43,
		20, 28,  5, 13,  6, 14, 21, 29,
		36, 44, 51, 59, 52, 60, 37, 45,
		22, 30,  7, 15, 23, 31, 38, 46,
		53, 61, 54, 62, 39, 47, 55, 63,
	};

	static const int range[64]={
		0,
		0,0,0,0,0,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
	};

	static const char shifttable[][4]={
		{0,0,0,0},
		{0,0,0,1},
		{0,0,1,1},
		{0,1,1,2},
		{1,1,2,2},
		{1,2,2,3},
		{2,2,3,3},
		{2,3,3,4},
		{3,3,4,4},
		{3,4,4,5},
		{4,4,5,5},
		{1,1,1,1},
		{1,1,1,2},
	};

	static const int quanttable[4][16]={
		{            5,5,4,4,3,3,2,2,1,0,0,0,0,0,0,0},
		{      7,6,6,5,5,4,4,3,3,2,2,1,0,0,0,0},
		{8,8,7,7,6,6,5,5,4,4,3,3,2,2,1,0},
		{ 10,9,9,8,8,7,7,6,6,5,5,4,4,12,11,11},
	};

#if 0
	static const double CS1 = 0.98078528040323044912618223613424;
	static const double CS2 = 0.92387953251128675612818318939679;
	static const double CS3 = 0.83146961230254523707878837761791;
	static const double CS4 = 0.70710678118654752440084436210485;
	static const double CS5 = 0.55557023301960222474283081394853;
	static const double CS6 = 0.3826834323650897717284599840304;
	static const double CS7 = 0.19509032201612826784828486847702;

	static const double w[8] = {
		1.0,
		(4.0*CS7*CS2)/CS4,
		(2.0*CS6)/CS4,
		2.0*CS5,
		8.0/7.0,
		CS3/CS4,
		CS2/CS4,
		CS1/CS4
	};
#else
	// Weights 2/(w(i)*w(j)) according to SMPTE 314M 5.2.2, in zigzag order and 12-bit fixed point.
	static const int weights_std[64]={
		8192,  8035,  8035,  7568,  7880,  7568,  7373,  7423,
		7423,  7373,  7168,  7231,  6992,  7231,  7168,  6967,
		7030,  6811,  6811,  7030,  6967,  6270,  6833,  6622,
		6635,  6622,  6833,  6270,  5906,  6149,  6436,  6451,
		6451,  6436,  6149,  5906,  5793,  5793,  6270,  6272,
		6270,  5793,  5793,  5457,  5643,  6096,  6096,  5643,
		5457,  5315,  5486,  5925,  5486,  5315,  5168,  5332,
		5332,  5168,  5023,  4799,  5023,  4520,  4520,  4258,
	};

	static const int weights_alt[64]={
		8192, 7568, 8035, 7423, 8192, 7568, 7568, 6992,
		8035, 7423, 8035, 7373, 8035, 7373, 7880, 7231,
		7568, 6992, 7373, 6811, 7168, 6622, 7373, 6811,
		7423, 6811, 7880, 7231, 7423, 6811, 7231, 6635,
		7168, 6622, 6967, 6436, 6270, 5793, 6967, 6436,
		7030, 6451, 7231, 6635, 7030, 6451, 6833, 6270,
		6270, 5793, 5906, 5457, 5906, 5457, 6149, 5643,
		6833, 6270, 6149, 5643, 5793, 5315, 5793, 5315,
	};
#endif

	struct DVBitSource {
		const uint8 *src;
		const uint8 *srclimit;
		int bitpos;

		void Init(const uint8 *_src, const uint8 *_srclimit, int _bitpos) {
			src = _src;
			srclimit = _srclimit;
			bitpos = _bitpos;
		}
	};

	class DVDecoderContext {
	public:
		DVDecoderContext();

		const VDMPEGIDCTSet	*mpIDCT;
	};

	DVDecoderContext::DVDecoderContext() {
		long flags = CPUGetEnabledExtensions();

#ifdef _M_AMD64
		mpIDCT = &g_VDMPEGIDCT_sse2;
#else
		if (flags & CPUF_SUPPORTS_SSE2)
			mpIDCT = &g_VDMPEGIDCT_sse2;
		else if (flags & CPUF_SUPPORTS_INTEGER_SSE)
			mpIDCT = &g_VDMPEGIDCT_isse;
		else if (flags & CPUF_SUPPORTS_MMX)
			mpIDCT = &g_VDMPEGIDCT_mmx;
		else
			mpIDCT = &g_VDMPEGIDCT_scalar;
#endif
	}

	class DVDCTBlockDecoder {
	public:
		void Init(uint8 *dst, ptrdiff_t pitch, DVBitSource& bitsource, int qno, bool split, const int zigzag[2][64]);
		bool Decode(DVBitSource& bitsource, const DVDecoderContext& context);

	protected:
		uint32		mBitHeap;
		int			mBitCount;
		int			mIdx;
		const char	*mpShiftTab;
		const int	*mpZigzag;
		const short	*mpWeights;

		__declspec(align(16)) sint16		mCoeff[64];

		uint8		*mpDst;
		ptrdiff_t	mPitch;
		bool		mb842;
		bool		mbSplit;
	};

	short weights_std_prescaled[13][64];
	short weights_alt_prescaled[13][64];

	void DVDCTBlockDecoder::Init(uint8 *dst, ptrdiff_t pitch, DVBitSource& src, int qno, bool split, const int zigzag[2][64]) {
		mpDst = dst;
		mPitch = pitch;
		mBitHeap = mBitCount = 0;
		mIdx = 0;

		memset(&mCoeff, 0, sizeof mCoeff);

		const uint8 *src0 = src.src;
		++src.src;
		src.bitpos = 4;

		const int dctclass = (src0[1] >> 4) & 3;

		mpShiftTab = shifttable[quanttable[dctclass][qno]];
		mCoeff[0] = (sint16)(((sint8)src0[0]*2 + (src0[1]>>7) + 0x100) << 2);

		mb842 = false;
		mpZigzag = zigzag[0];
		mpWeights = weights_std_prescaled[quanttable[dctclass][qno]];
		if (src0[1] & 0x40) {
			mb842 = true;
			mpZigzag = zigzag[1];
			mpWeights = weights_alt_prescaled[quanttable[dctclass][qno]];
		}

		mbSplit = split;

		if (!weights_std_prescaled[0][0]) {
			for(int i=0; i<13; ++i) {
				for(int j=0; j<64; ++j) {
					weights_std_prescaled[i][j] = (short)(((weights_std[j] >> (6 - shifttable[i][range[j]]))+1)>>1);
					weights_alt_prescaled[i][j] = (short)(((weights_alt[j] >> (6 - shifttable[i][range[j]]))+1)>>1);
				}
			}
		}
	}

#ifdef _MSC_VER
	#pragma auto_inline(off)
#endif
	void WrapPrescaledIDCT(const VDMPEGIDCTSet& idct, uint8 *dst, ptrdiff_t pitch, void *coeff0, int last_pos) {
		const sint16 *coeff = (const sint16 *)coeff0;
		int coeff2[64];

		for(int i=0; i<64; ++i)
			coeff2[i] = (coeff[i] * idct.pPrescaler[i] + 128) >> 8;

		idct.pIntra(dst, pitch, coeff2, last_pos);
	}
#ifdef _MSC_VER
	#pragma auto_inline(on)
#endif

	bool DVDCTBlockDecoder::Decode(DVBitSource& bitsource, const DVDecoderContext& context) {
		if (!mpWeights)
			return true;

		if (bitsource.src >= bitsource.srclimit)
			return false;

		VDASSERT(mBitCount < 24);

		mBitHeap += (((*bitsource.src++) << bitsource.bitpos) & 0xff) << (24-mBitCount);
		mBitCount += 8 - bitsource.bitpos;

		for(;;) {
			if (mBitCount < 16) {
				if(bitsource.src < bitsource.srclimit) {
					mBitHeap += *bitsource.src++ << (24 - mBitCount);
					mBitCount += 8;
				}
				if(bitsource.src < bitsource.srclimit) {
					mBitHeap += *bitsource.src++ << (24 - mBitCount);
					mBitCount += 8;
				}
			}

			const VLCEntry *acdec = DVDecodeAC(mBitHeap);

			int tmpcnt = mBitCount - acdec->len;

			if (tmpcnt < 0)
				return false;

			mBitCount = tmpcnt;
			mBitHeap <<= acdec->len;

			if (acdec->run > 64)
				break;

			int tmpidx = mIdx + acdec->run;

			if (tmpidx >= 64) {
				mIdx = 63;
				break;
			}

			mIdx = tmpidx;

			const int q = mpWeights[mIdx];

			mCoeff[mpZigzag[mIdx]] = (short)((((sint32)acdec->coeff /*<< mpShiftTab[range[mIdx]]*/) * q + 2048) >> 12);
		}

		// release bits
		bitsource.src -= mBitCount >> 3;
		bitsource.bitpos = 0;
		if (mBitCount & 7) {
			--bitsource.src;
			bitsource.bitpos = 8 - (mBitCount & 7);
		}

		mpWeights = NULL;

#pragma vdpragma_TODO("optimize interlaced 8x4x2 IDCT")
		if (mb842)
			g_VDMPEGIDCT_reference.pIntra4x2(mpDst, mPitch, mCoeff, 63);
		else {
			if (context.mpIDCT->pPrescaler) {
#pragma vdpragma_TODO("consider whether we should suck less here on scalar")
				WrapPrescaledIDCT(*context.mpIDCT, mpDst, mPitch, mCoeff, mIdx);
			} else {
				context.mpIDCT->pIntra(mpDst, mPitch, mCoeff, mIdx);
			}
		}

		if (mbSplit) {
			for(int i=0; i<8; ++i) {
				*(uint32 *)(mpDst + mPitch*(8+i)) = *(uint32 *)(mpDst + 4 + mPitch*i);
			}
		}

		return true;
	}
}

void VDVideoDecompressorDV::DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll) {
	if (!mFormat)
		throw MyError("Cannot find compatible target format for video decompression.");

	if ((mHeight == 480 && srcSize != 120000) || (mHeight == 576 && srcSize != 144000))
		throw MyError("DV frame data is wrong size (truncated or corrupted)");

	static const int sNTSCMacroblockOffsets[5][27][2]={
#define P(x,y) {x*32+y*8*sizeof(mYPlane[0]), x*8+y*8*sizeof(m411.mCrPlane[0])}
		{
			P( 9,0),P( 9,1),P( 9,2),P( 9,3),P( 9,4),P( 9,5),
			P(10,5),P(10,4),P(10,3),P(10,2),P(10,1),P(10,0),
			P(11,0),P(11,1),P(11,2),P(11,3),P(11,4),P(11,5),
			P(12,5),P(12,4),P(12,3),P(12,2),P(12,1),P(12,0),
			P(13,0),P(13,1),P(13,2)
		},
		{
								P(4,3),P(4,4),P(4,5),
			P(5,5),P(5,4),P(5,3),P(5,2),P(5,1),P(5,0),
			P(6,0),P(6,1),P(6,2),P(6,3),P(6,4),P(6,5),
			P(7,5),P(7,4),P(7,3),P(7,2),P(7,1),P(7,0),
			P(8,0),P(8,1),P(8,2),P(8,3),P(8,4),P(8,5),
		},
		{
									P(13,3),P(13,4),P(13,5),
			P(14,5),P(14,4),P(14,3),P(14,2),P(14,1),P(14,0),
			P(15,0),P(15,1),P(15,2),P(15,3),P(15,4),P(15,5),
			P(16,5),P(16,4),P(16,3),P(16,2),P(16,1),P(16,0),
			P(17,0),P(17,1),P(17,2),P(17,3),P(17,4),P(17,5),
		},
		{
			P(0,0),P(0,1),P(0,2),P(0,3),P(0,4),P(0,5),
			P(1,5),P(1,4),P(1,3),P(1,2),P(1,1),P(1,0),
			P(2,0),P(2,1),P(2,2),P(2,3),P(2,4),P(2,5),
			P(3,5),P(3,4),P(3,3),P(3,2),P(3,1),P(3,0),
			P(4,0),P(4,1),P(4,2)
		},
		{
			P(18,0),P(18,1),P(18,2),P(18,3),P(18,4),P(18,5),
			P(19,5),P(19,4),P(19,3),P(19,2),P(19,1),P(19,0),
			P(20,0),P(20,1),P(20,2),P(20,3),P(20,4),P(20,5),
			P(21,5),P(21,4),P(21,3),P(21,2),P(21,1),P(21,0),
			P(22,0),P(22,2),P(22,4)
		},
#undef P
	};

	static const int sPALMacroblockOffsets[5][27][2]={
#define P(x,y) {x*16+y*16*sizeof(mYPlane[0]), x*8+y*8*sizeof(m420.mCrPlane[0])}
		{
			P(18,0),P(18,1),P(18,2),P(19,2),P(19,1),P(19,0),
			P(20,0),P(20,1),P(20,2),P(21,2),P(21,1),P(21,0),
			P(22,0),P(22,1),P(22,2),P(23,2),P(23,1),P(23,0),
			P(24,0),P(24,1),P(24,2),P(25,2),P(25,1),P(25,0),
			P(26,0),P(26,1),P(26,2),
		},
		{
			P( 9,0),P( 9,1),P( 9,2),P(10,2),P(10,1),P(10,0),
			P(11,0),P(11,1),P(11,2),P(12,2),P(12,1),P(12,0),
			P(13,0),P(13,1),P(13,2),P(14,2),P(14,1),P(14,0),
			P(15,0),P(15,1),P(15,2),P(16,2),P(16,1),P(16,0),
			P(17,0),P(17,1),P(17,2),
		},
		{
			P(27,0),P(27,1),P(27,2),P(28,2),P(28,1),P(28,0),
			P(29,0),P(29,1),P(29,2),P(30,2),P(30,1),P(30,0),
			P(31,0),P(31,1),P(31,2),P(32,2),P(32,1),P(32,0),
			P(33,0),P(33,1),P(33,2),P(34,2),P(34,1),P(34,0),
			P(35,0),P(35,1),P(35,2),
		},
		{
			P(0,0),P(0,1),P(0,2),P(1,2),P(1,1),P(1,0),
			P(2,0),P(2,1),P(2,2),P(3,2),P(3,1),P(3,0),
			P(4,0),P(4,1),P(4,2),P(5,2),P(5,1),P(5,0),
			P(6,0),P(6,1),P(6,2),P(7,2),P(7,1),P(7,0),
			P(8,0),P(8,1),P(8,2)
		},
		{
			P(36,0),P(36,1),P(36,2),P(37,2),P(37,1),P(37,0),
			P(38,0),P(38,1),P(38,2),P(39,2),P(39,1),P(39,0),
			P(40,0),P(40,1),P(40,2),P(41,2),P(41,1),P(41,0),
			P(42,0),P(42,1),P(42,2),P(43,2),P(43,1),P(43,0),
			P(44,0),P(44,1),P(44,2),
		},
#undef P
	};

	static const int sDCTYBlockOffsets411[2][4]={
		0, 8, 16, 24,
		0, 8, 8 * sizeof mYPlane[0], 8 + 8 * sizeof mYPlane[0],
	};

	static const int sDCTYBlockOffsets420[2][4]={
		0, 8, 8 * sizeof mYPlane[0], 8 + 8 * sizeof mYPlane[0],
		0, 8, 8 * sizeof mYPlane[0], 8 + 8 * sizeof mYPlane[0],
	};

	const int (*const pMacroblockOffsets)[27][2] = (mHeight == 576 ? sPALMacroblockOffsets : sNTSCMacroblockOffsets);
	const int (*const pDCTYBlockOffsets)[4] = (mHeight == 576 ? sDCTYBlockOffsets420 : sDCTYBlockOffsets411);
	const uint8 *pVideoBlock = (const uint8 *)src + 7*80;

	const int kChromaStep = sizeof(m411.mCrPlane[0]) * 48;
	VDASSERTCT(kChromaStep == sizeof(m420.mCrPlane[0]) * 24);

	uint8 *pCr, *pCb;
	ptrdiff_t chroma_pitch;
	int nDIFSequences;

	if (mHeight == 576) {
		memset(m420.mCrPlane, 0x80, sizeof m420.mCrPlane);
		memset(m420.mCbPlane, 0x80, sizeof m420.mCbPlane);

		pCr = m420.mCrPlane[0];
		pCb = m420.mCbPlane[0];
		chroma_pitch = sizeof m420.mCrPlane[0];
		nDIFSequences = 12;
	} else {
		memset(m411.mCrPlane, 0x80, sizeof m411.mCrPlane);
		memset(m411.mCbPlane, 0x80, sizeof m411.mCbPlane);

		pCr = m411.mCrPlane[0];
		pCb = m411.mCbPlane[0];
		chroma_pitch = sizeof m411.mCrPlane[0];
		nDIFSequences = 10;
	}

	int zigzag[2][64];

	DVDecoderContext context;

	if (context.mpIDCT->pAltScan)
		memcpy(zigzag[0], context.mpIDCT->pAltScan, sizeof(int)*64);
	else
		memcpy(zigzag[0], zigzag_std, sizeof(int)*64);

	memcpy(zigzag[1], zigzag_alt, sizeof(int)*64);

	for(int i=0; i<nDIFSequences; ++i) {			// 10/12 DIF sequences
		int audiocounter = 0;

		const int columns[5]={
			(i+2) % nDIFSequences,
			(i+6) % nDIFSequences,
			(i+8) % nDIFSequences,
			(i  ) % nDIFSequences,
			(i+4) % nDIFSequences,
		};

		for(int k=0; k<27; ++k) {
			DVBitSource mSources[30];
			__declspec(align(16)) DVDCTBlockDecoder mDecoders[30];

			int blk = 0;

			for(int j=0; j<5; ++j) {
				const int y_offset = pMacroblockOffsets[j][k][0];
				const int c_offset = pMacroblockOffsets[j][k][1];
				const int super_y = columns[j];

				uint8 *yptr = mYPlane[super_y*48] + y_offset;
				uint8 *crptr = pCr + kChromaStep * super_y;
				uint8 *cbptr = pCb + kChromaStep * super_y;

				int qno = pVideoBlock[3] & 15;

				bool bHalfBlock = nDIFSequences == 10 && j==4 && k>=24;
				
				mSources[blk+0].Init(pVideoBlock +  4, pVideoBlock + 18, 0);
				mSources[blk+1].Init(pVideoBlock + 18, pVideoBlock + 32, 0);
				mSources[blk+2].Init(pVideoBlock + 32, pVideoBlock + 46, 0);
				mSources[blk+3].Init(pVideoBlock + 46, pVideoBlock + 60, 0);
				mSources[blk+4].Init(pVideoBlock + 60, pVideoBlock + 70, 0);
				mSources[blk+5].Init(pVideoBlock + 70, pVideoBlock + 80, 0);
				mDecoders[blk+0].Init(yptr + pDCTYBlockOffsets[bHalfBlock][0], sizeof mYPlane[0], mSources[blk+0], qno, false, zigzag);
				mDecoders[blk+1].Init(yptr + pDCTYBlockOffsets[bHalfBlock][1], sizeof mYPlane[0], mSources[blk+1], qno, false, zigzag);
				mDecoders[blk+2].Init(yptr + pDCTYBlockOffsets[bHalfBlock][2], sizeof mYPlane[0], mSources[blk+2], qno, false, zigzag);
				mDecoders[blk+3].Init(yptr + pDCTYBlockOffsets[bHalfBlock][3], sizeof mYPlane[0], mSources[blk+3], qno, false, zigzag);
				mDecoders[blk+4].Init(crptr + c_offset, chroma_pitch, mSources[blk+4], qno, bHalfBlock, zigzag);
				mDecoders[blk+5].Init(cbptr + c_offset, chroma_pitch, mSources[blk+5], qno, bHalfBlock, zigzag);

				int i;

				for(i=0; i<6; ++i)
					mDecoders[blk+i].Decode(mSources[blk+i], context);

				int source = 0;

				i = 0;
				while(i < 6 && source < 6) {
					if (!mDecoders[blk+i].Decode(mSources[blk+source], context))
						++source;
					else
						++i;
				}

				blk += 6;

				pVideoBlock += 80;
				if (++audiocounter >= 15) {
					audiocounter = 0;
					pVideoBlock += 80;
				}
			}

			int source = 0;
			blk = 0;

			while(blk < 30 && source < 30) {
				if (!mDecoders[blk].Decode(mSources[source], context))
					++source;
				else
					++blk;
			}
		}

		pVideoBlock += 80 * 6;
	}

#ifndef _M_AMD64
	if (MMX_enabled)
		__asm emms
	if (ISSE_enabled)
		__asm sfence
#else
	_mm_sfence();
#endif

	// blit time!

	VDPixmap pxsrc = {0};
	pxsrc.data		= mYPlane;
	pxsrc.pitch		= sizeof mYPlane[0];
	pxsrc.w			= 720;
	if (mHeight == 576) {
		pxsrc.data2		= m420.mCbPlane;
		pxsrc.data3		= m420.mCrPlane;
		pxsrc.pitch2	= sizeof m420.mCbPlane[0];
		pxsrc.pitch3	= sizeof m420.mCrPlane[0];
		pxsrc.h			= 576;
		pxsrc.format	= nsVDPixmap::kPixFormat_YUV420_Planar;
	} else {
		pxsrc.data2		= m411.mCbPlane;
		pxsrc.data3		= m411.mCrPlane;
		pxsrc.pitch2	= sizeof m411.mCbPlane[0];
		pxsrc.pitch3	= sizeof m411.mCrPlane[0];
		pxsrc.h			= 480;
		pxsrc.format	= nsVDPixmap::kPixFormat_YUV411_Planar;
	}

	VDPixmapLayout dstlayout;
	VDMakeBitmapCompatiblePixmapLayout(dstlayout, mWidth, mHeight, mFormat, 0);
	VDPixmap pxdst(VDPixmapFromLayout(dstlayout, dst));

	VDPixmapBlt(pxdst, pxsrc);
}

const void *VDVideoDecompressorDV::GetRawCodecHandlePtr() {
	return NULL;
}

const wchar_t *VDVideoDecompressorDV::GetName() {
	return L"Internal DV decoder";
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDecompressorVCM : public IVDVideoDecompressor {
public:
	VDVideoDecompressorVCM();
	~VDVideoDecompressorVCM();

	void Init(const void *srcFormat, HIC hic);

	bool QueryTargetFormat(int format);
	bool QueryTargetFormat(const void *format);
	bool SetTargetFormat(int format);
	bool SetTargetFormat(const void *format);
	int GetTargetFormat() { return mFormat; }
	int GetTargetFormatVariant() { return mFormatVariant; }
	void Start();
	void Stop();
	void DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll);
	const void *GetRawCodecHandlePtr();
	const wchar_t *GetName();

protected:
	HIC			mhic;
	int			mFormat;
	int			mFormatVariant;
	bool		mbActive;
	bool		mbUseEx;
	VDStringW	mName;
	VDStringW	mDriverName;
	vdstructex<BITMAPINFOHEADER>	mSrcFormat;
	vdstructex<BITMAPINFOHEADER>	mDstFormat;
};

VDVideoDecompressorVCM::VDVideoDecompressorVCM()
	: mhic(NULL)
	, mbActive(false)
	, mFormat(0)
	, mFormatVariant(0)
{
}

VDVideoDecompressorVCM::~VDVideoDecompressorVCM() {
	Stop();

	if (mhic) {
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		ICClose(mhic);
	}
}

void VDVideoDecompressorVCM::Init(const void *srcFormat, HIC hic) {
	VDASSERT(!mhic);

	mhic = hic;

	const BITMAPINFOHEADER *bih = (const BITMAPINFOHEADER *)srcFormat;

	mSrcFormat.assign(bih, VDGetSizeOfBitmapHeaderW32(bih));

	ICINFO info = {sizeof(ICINFO)};
	DWORD rv;

	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		rv = ICGetInfo(mhic, &info, sizeof info);
	}

	if (rv >= sizeof info) {
		mName = info.szDescription;
		const wchar_t *pName = info.szDescription;
		mDriverName = VDswprintf(L"Video codec \"%ls\"", 1, &pName);
	}
}

bool VDVideoDecompressorVCM::QueryTargetFormat(int format) {
	vdstructex<BITMAPINFOHEADER> bmformat;
	const int variants = VDGetPixmapToBitmapVariants(format);

	for(int variant=1; variant<=variants; ++variant) {
		if (VDMakeBitmapFormatFromPixmapFormat(bmformat, mSrcFormat, format, variant) && QueryTargetFormat(bmformat.data()))
			return true;
	}

	return false;
}

bool VDVideoDecompressorVCM::QueryTargetFormat(const void *format) {
	const BITMAPINFO *pSrcFormat = (const BITMAPINFO *)mSrcFormat.data();
	DWORD retval;

	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		retval = ICDecompressQuery(mhic, pSrcFormat, (BITMAPINFO *)format);
	}

	return retval == ICERR_OK;
}

bool VDVideoDecompressorVCM::SetTargetFormat(int format) {
	using namespace nsVDPixmap;

	if (!format)
		return SetTargetFormat(kPixFormat_RGB888)
			|| SetTargetFormat(kPixFormat_XRGB8888)
			|| SetTargetFormat(kPixFormat_XRGB1555);

	vdstructex<BITMAPINFOHEADER> bmformat;
	const int variants = VDGetPixmapToBitmapVariants(format);

	for(int variant=1; variant<=variants; ++variant) {
		if (VDMakeBitmapFormatFromPixmapFormat(bmformat, mSrcFormat, format, variant) && SetTargetFormat(bmformat.data())) {
			mFormat = format;
			mFormatVariant = variant;
			return true;
		}
	}

	return false;
}

bool VDVideoDecompressorVCM::SetTargetFormat(const void *format) {
	BITMAPINFO *pSrcFormat = (BITMAPINFO *)mSrcFormat.data();
	BITMAPINFO *pDstFormat = (BITMAPINFO *)format;
	DWORD retval;

	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		retval = ICDecompressQuery(mhic, pSrcFormat, pDstFormat);
	}

	if (retval == ICERR_OK) {
		if (mbActive)
			Stop();

		mDstFormat.assign((const BITMAPINFOHEADER *)format, VDGetSizeOfBitmapHeaderW32((const BITMAPINFOHEADER *)format));
		mFormat = 0;
		mFormatVariant = 0;
		return true;
	}

	return false;
}

void VDVideoDecompressorVCM::Start() {
	if (mDstFormat.empty())
		throw MyError("Cannot find compatible target format for video decompression.");

	if (!mbActive) {
		BITMAPINFO *pSrcFormat = (BITMAPINFO *)mSrcFormat.data();
		BITMAPINFO *pDstFormat = (BITMAPINFO *)mDstFormat.data();
		DWORD retval;

		mbUseEx = false;
		{
			VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
			retval = ICDecompressBegin(mhic, pSrcFormat, pDstFormat);

			if (retval != ICERR_OK) {
				BITMAPINFOHEADER *bihSrc = (BITMAPINFOHEADER *)pSrcFormat;
				BITMAPINFOHEADER *bihDst = (BITMAPINFOHEADER *)pDstFormat;
				if (ICERR_OK == ICDecompressExBegin(mhic, 0, bihSrc, NULL, 0, 0, bihSrc->biWidth, bihSrc->biHeight, bihDst, NULL, 0, 0, bihDst->biWidth, bihDst->biHeight)) {
					mbUseEx = true;
					retval = ICERR_OK;
				}
			}
		}

		if (retval != ICERR_OK)
			throw MyICError("VideoSourceAVI", retval);

		mbActive = true;
	}
}

void VDVideoDecompressorVCM::Stop() {
	if (mbActive) {
		mbActive = false;

		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		if (mbUseEx)
			ICDecompressExEnd(mhic);
		else
			ICDecompressEnd(mhic);
	}
}

void VDVideoDecompressorVCM::DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll) {
	if (!mbActive)
		Start();

	BITMAPINFOHEADER *pSrcFormat = mSrcFormat.data();
	BITMAPINFOHEADER *pDstFormat = mDstFormat.data();

	DWORD dwFlags = 0;

	if (!keyframe)
		dwFlags |= ICDECOMPRESS_NOTKEYFRAME;

	if (preroll)
		dwFlags |= ICDECOMPRESS_PREROLL;

	DWORD dwOldSize = pSrcFormat->biSizeImage;
	pSrcFormat->biSizeImage = srcSize;
	DWORD retval;
	
	{
		VDExternalCodeBracket bracket(mDriverName.c_str(), __FILE__, __LINE__);
		if (mbUseEx) {
			BITMAPINFOHEADER *bihSrc = (BITMAPINFOHEADER *)pSrcFormat;
			BITMAPINFOHEADER *bihDst = (BITMAPINFOHEADER *)pDstFormat;
			retval = ICDecompressEx(mhic, dwFlags, bihSrc, (LPVOID)src, 0, 0, bihSrc->biWidth, bihSrc->biHeight, bihDst, dst, 0, 0, bihDst->biWidth, bihDst->biHeight);
		} else
			retval = ICDecompress(mhic, dwFlags, pSrcFormat, (LPVOID)src, pDstFormat, dst);
	}

	pSrcFormat->biSizeImage = dwOldSize;

	// We will get ICERR_DONTDRAW if we set preroll.
	if (retval < 0)
		throw MyICError(retval, "%%s (Error code: %d)", (int)retval);
}

const void *VDVideoDecompressorVCM::GetRawCodecHandlePtr() {
	return &mhic;
}

const wchar_t *VDVideoDecompressorVCM::GetName() {
	return mName.c_str();
}

///////////////////////////////////////////////////////////////////////////

VideoSource::VideoSource() {
	lpvBuffer = NULL;
	hBufferObject = NULL;
}

VideoSource::~VideoSource() {
	FreeFrameBuffer();
}

void *VideoSource::AllocFrameBuffer(long size) {
	hBufferObject = CreateFileMapping(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			size,
			NULL);

	if (!hBufferObject) return NULL;

	lBufferOffset = 0;

	lpvBuffer = MapViewOfFile(hBufferObject, FILE_MAP_ALL_ACCESS, 0, lBufferOffset, size);
	mFrameBufferSize = size;

	if (!lpvBuffer) {
		CloseHandle(hBufferObject);
		hBufferObject = NULL;
	}

	return lpvBuffer;
}

void VideoSource::FreeFrameBuffer() {
	if (hBufferObject) {
		if (lpvBuffer)
			UnmapViewOfFile(lpvBuffer);
		CloseHandle(hBufferObject);
	} else
		freemem(lpvBuffer);

	lpvBuffer = NULL;
	hBufferObject = NULL;
}

bool VideoSource::setTargetFormat(int format) {
	return setTargetFormatVariant(format, 0);
}

bool VideoSource::setTargetFormatVariant(int format, int variant) {
	using namespace nsVDPixmap;

	if (!format)
		format = kPixFormat_XRGB8888;

	const BITMAPINFOHEADER *bih = getImageFormat();
	const uint32 w = bih->biWidth;
	const uint32 h = bih->biHeight;
	VDPixmapLayout layout;

	VDMakeBitmapCompatiblePixmapLayout(layout, w, h, format, variant);

	mTargetFormat = VDPixmapFromLayout(layout, lpvBuffer);
	mTargetFormatVariant = variant;

	if(format == nsVDPixmap::kPixFormat_Pal8) {
		mpTargetFormatHeader.assign(getImageFormat(), sizeof(BITMAPINFOHEADER));
		mpTargetFormatHeader->biSize			= sizeof(BITMAPINFOHEADER);
		mpTargetFormatHeader->biPlanes			= 1;
		mpTargetFormatHeader->biBitCount		= 8;
		mpTargetFormatHeader->biCompression		= BI_RGB;
		mpTargetFormatHeader->biSizeImage		= ((w+3)&~3)*h;

		mTargetFormat.palette = mPalette;
	} else {
		const vdstructex<BITMAPINFOHEADER> src(bih, getFormatLen());
		if (!VDMakeBitmapFormatFromPixmapFormat(mpTargetFormatHeader, src, format, variant))
			return false;
	}

	invalidateFrameBuffer();

	return true;
}

bool VideoSource::setDecompressedFormat(int depth) {
	switch(depth) {
	case 8:		return setTargetFormat(nsVDPixmap::kPixFormat_Pal8);
	case 16:	return setTargetFormat(nsVDPixmap::kPixFormat_XRGB1555);
	case 24:	return setTargetFormat(nsVDPixmap::kPixFormat_RGB888);
	case 32:	return setTargetFormat(nsVDPixmap::kPixFormat_XRGB8888);
	}

	return false;
}

bool VideoSource::setDecompressedFormat(const BITMAPINFOHEADER *pbih) {
	const BITMAPINFOHEADER *bih = getImageFormat();

	if (pbih->biWidth == bih->biWidth && pbih->biHeight == bih->biHeight) {
		int variant;

		int format = VDBitmapFormatToPixmapFormat(*pbih, variant);

		if (format && variant <= 1)
			return setTargetFormat(format);
	}

	return false;
}

void VideoSource::streamBegin(bool) {
	stream_current_frame	= -1;
}

void VideoSource::streamSetDesiredFrame(VDPosition frame_num) {
	VDPosition key;

	key = isKey(frame_num) ? frame_num : prevKey(frame_num);
	if (key<0)
		key = mSampleFirst;

	stream_desired_frame	= frame_num;

	if (stream_current_frame<key || stream_current_frame>frame_num)
		stream_current_frame	= key-1;

}

VDPosition VideoSource::streamGetNextRequiredFrame(bool& is_preroll) {
	if (stream_current_frame == stream_desired_frame) {
		is_preroll = false;

		return -1;
	}

	is_preroll = (++stream_current_frame != stream_desired_frame);

	return stream_current_frame;
}

int VideoSource::streamGetRequiredCount(uint32 *totalsize) {

	if (totalsize) {
		VDPosition current = stream_current_frame + 1;
		uint32 size = 0, onesize;
		uint32 samp;

		while(current <= stream_desired_frame) {
			if (AVIERR_OK == read(current, 1, NULL, NULL, &onesize, &samp))
				size += onesize;

			++current;
		}

		*totalsize = size;
	}

	return (int)(stream_desired_frame - stream_current_frame);
}

void VideoSource::invalidateFrameBuffer() {
}

bool VideoSource::isKey(VDPosition lSample) {
	if (lSample<mSampleFirst || lSample>=mSampleLast)
		return true;

	return _isKey(lSample);
}

bool VideoSource::_isKey(VDPosition lSample) {
	return true;
}

VDPosition VideoSource::nearestKey(VDPosition lSample) {
	return lSample;
}

VDPosition VideoSource::prevKey(VDPosition lSample) {
	return lSample <= mSampleFirst ? -1 : lSample-1;
}

VDPosition VideoSource::nextKey(VDPosition lSample) {
	return lSample+1 >= mSampleFirst ? -1 : lSample+1;
}

bool VideoSource::isKeyframeOnly() {
   return false;
}

bool VideoSource::isType1() {
   return false;
}

///////////////////////////

VideoSourceAVI::VideoSourceAVI(IAVIReadHandler *pAVI, AVIStripeSystem *stripesys, IAVIReadHandler **stripe_files, bool use_internal, int mjpeg_mode, FOURCC fccForceVideo, FOURCC fccForceVideoHandler)
	: mErrorMode(kErrorModeReportAll)
	, mbMMXBrokenCodecDetected(false)
	, mbConcealingErrors(false)
	, mbDecodeStarted(false)
{
	pAVIFile	= pAVI;
	pAVIStream	= NULL;
	lpvBuffer	= NULL;
	key_flags	= NULL;
	mjpeg_reorder_buffer = NULL;
	mjpeg_reorder_buffer_size = 0;
	mjpeg_splits = NULL;
	mjpeg_last = -1;
	this->fccForceVideo = fccForceVideo;
	this->fccForceVideoHandler = fccForceVideoHandler;
	hbmLame = NULL;
	fUseGDI = false;
	bDirectDecompress = false;
	bInvertFrames = false;
	lLastFrame = -1;

	// striping...

	stripe_streams	= NULL;
	stripe_index	= NULL;
	this->stripesys = stripesys;
	this->stripe_files = stripe_files;
	this->use_internal = use_internal;
	this->mjpeg_mode	= mjpeg_mode;

	try {
		_construct();
	} catch(...) {
		_destruct();
		throw;
	}
}

void VideoSourceAVI::_destruct() {
	delete stripe_index;

	if (stripe_streams) {
		int i;

		for(i=0; i<stripe_count; i++)
			if (stripe_streams[i])
				delete stripe_streams[i];

		delete stripe_streams;
	}

	mpDecompressor = NULL;

	if (pAVIStream) delete pAVIStream;

	freemem(mjpeg_reorder_buffer);
	mjpeg_reorder_buffer = NULL;
	delete[] mjpeg_splits;
	mjpeg_splits = NULL;

	delete[] key_flags; key_flags = NULL;

	if (hbmLame) {
		DeleteObject(hbmLame);
		hbmLame = NULL;
	}
}

VideoSourceAVI::~VideoSourceAVI() {
	_destruct();
}

void VideoSourceAVI::_construct() {
	LONG format_len;
	BITMAPINFOHEADER *bmih;
	bool is_mjpeg, is_dv, is_dib;

	// Look for a standard vids stream

	bIsType1 = false;
	pAVIStream = pAVIFile->GetStream(streamtypeVIDEO, 0);
	if (!pAVIStream) {
		pAVIStream = pAVIFile->GetStream('svai', 0);

		if (!pAVIStream)
			throw MyError("No video stream found.");

		bIsType1 = true;
	}

	if (pAVIStream->Info(&streamInfo, sizeof streamInfo))
		throw MyError("Error obtaining video stream info.");

	// Force the video type to be 'vids', in case it was type-1 coming in.

	streamInfo.fccType = 'sdiv';

	// ADDITION FOR STRIPED AVI SUPPORT:
	//
	// If this is an index for a stripe system, then the video stream will have
	// 'VDST' as its fccHandler and video compression.  This will probably
	// correspond to the "VDub Frameserver" compressor, but since VirtualDub can
	// connect to its own frameservers more efficiently though the AVIFile
	// interface, it makes sense to open striped files in native mode.
	//
	// For this to work, we must have been sent the striping specs beforehand,
	// or else we won't be able to open the stripes.

	if (streamInfo.fccHandler == 'TSDV') {
		int i;

		if (!stripesys)
			throw MyError("AVI file is striped - must be opened with stripe definition file.");

		// allocate memory for stripe stream table

		stripe_count = stripesys->getStripeCount();

		if (!(stripe_streams = new IAVIReadStream *[stripe_count]))
			throw MyMemoryError();

		for(i=0; i<stripe_count; i++)
			stripe_streams[i] = NULL;

		// attempt to open a video stream for every stripe that has one

		format_stream = NULL;

		for(i=0; i<stripe_count; i++) {
			if (stripesys->getStripeInfo(i)->isVideo()) {
				stripe_streams[i] = stripe_files[i]->GetStream(streamtypeVIDEO, 0);
				if (!stripe_streams[i])
					throw MyError("Striping: cannot open video stream for stripe #%d", i+1);

				if (!format_stream) format_stream = stripe_streams[i];
			}
		}

		if (!format_stream)
			throw MyError("Striping: No video stripes found!");

		// Reread the streamInfo structure from first video stripe,
		// because ours is fake.

		if (format_stream->Info(&streamInfo, sizeof streamInfo))
			throw MyError("Error obtaining video stream info from first video stripe.");

		// Initialize the index.

		if (!(stripe_index = new AVIStripeIndexLookup(pAVIStream)))
			throw MyMemoryError();
		
	} else {
		if (stripesys)
			throw MyError("This is not a striped AVI file.");

		format_stream = pAVIStream;
	}

	// Read video format.  If we're striping, the index stripe has a fake
	// format, so we have to grab the format from a video stripe.  If it's a
	// type-1 DV, we're going to have to fake it.

	if (bIsType1) {
		format_len = sizeof(BITMAPINFOHEADER);

		if (!(bmih = (BITMAPINFOHEADER *)allocFormat(format_len))) throw MyMemoryError();

		bmih->biSize			= sizeof(BITMAPINFOHEADER);
		bmih->biWidth			= 720;

		if (streamInfo.dwRate > streamInfo.dwScale*26i64)
			bmih->biHeight			= 480;
		else
			bmih->biHeight			= 576;

		bmih->biPlanes			= 1;
		bmih->biBitCount		= 24;
		bmih->biCompression		= 'dsvd';
		bmih->biSizeImage		= streamInfo.dwSuggestedBufferSize;
		bmih->biXPelsPerMeter	= 0;
		bmih->biYPelsPerMeter	= 0;
		bmih->biClrUsed			= 0;
		bmih->biClrImportant	= 0;
	} else {
		format_stream->FormatSize(0, &format_len);

		std::vector<char> format(format_len);

		if (format_stream->ReadFormat(0, &format.front(), &format_len))
			throw MyError("Error obtaining video stream format.");

		// check for a very large BITMAPINFOHEADER -- structs as large as 153K
		// can be written out by some ePSXe video plugins

		const BITMAPINFOHEADER *pFormat = (const BITMAPINFOHEADER *)&format.front();

		if (format.size() >= 16384 && format.size() > pFormat->biSize) {
			int badsize = format.size();
			int realsize = pFormat->biSize;

			if (realsize >= sizeof(BITMAPINFOHEADER)) {
				realsize += sizeof(RGBQUAD) * pFormat->biClrUsed;

				if (realsize < format.size()) {
					VDLogAppMessage(kVDLogWarning, kVDST_VideoSource, kVDM_FixingHugeVideoFormat, 2, &badsize, &realsize);

					format.resize(realsize);
					format_len = realsize;
				}
			}
		}

		// copy format to official video stream format

		if (!(bmih = (BITMAPINFOHEADER *)allocFormat(format_len)))
			throw MyMemoryError();

		memcpy(getFormat(), pFormat, format.size());
	}

	mpTargetFormatHeader.resize(format_len);

	// initialize pixmap palette
	if (bmih->biClrUsed > 0) {
		memset(mPalette, 0, sizeof mPalette);
		memcpy(mPalette, (const uint32 *)((const char *)bmih + bmih->biSize), std::min<size_t>(256, bmih->biClrUsed) * 4);
	}


	// Some Dazzle drivers apparently do not set biSizeImage correctly.  Also,
	// zero is a valid value for BI_RGB, but it's annoying!

	if (bmih->biCompression == BI_RGB || bmih->biCompression == BI_BITFIELDS) {
		if (bmih->biPlanes == 1) {
			long nPitch = ((bmih->biWidth * bmih->biBitCount + 31) >> 5) * 4 * bmih->biHeight;

			bmih->biSizeImage = nPitch;
		}

		// Check for an inverted DIB.  If so, silently flip it around.

		if ((long)bmih->biHeight < 0) {
			bmih->biHeight = abs((long)bmih->biHeight);
			bInvertFrames = true;
		}
	}

	// Force the video format if necessary

	if (fccForceVideo)
		getImageFormat()->biCompression = fccForceVideo;

	if (fccForceVideoHandler)
		streamInfo.fccHandler = fccForceVideoHandler;

	is_mjpeg = isEqualFOURCC(bmih->biCompression, 'GPJM')
			|| isEqualFOURCC(fccForceVideo, 'GPJM')
			|| isEqualFOURCC(bmih->biCompression, '1bmd')
			|| isEqualFOURCC(fccForceVideo, '1bmd');

	is_dv = isEqualFOURCC(bmih->biCompression, 'dsvd')
		 || isEqualFOURCC(fccForceVideo, 'dsvd');

	// Check if we can handle the format directly; if so, convert bitmap format to Kasumi format
	bool inverted = false;

	mSourceLayout.data		= 0;
	mSourceLayout.data2		= 0;
	mSourceLayout.data3		= 0;
	mSourceLayout.pitch		= 0;
	mSourceLayout.pitch2	= 0;
	mSourceLayout.pitch3	= 0;
	mSourceLayout.palette	= 0;
	mSourceLayout.format	= 0;
	mSourceLayout.w			= bmih->biWidth;
	mSourceLayout.h			= abs(bmih->biHeight);
	mSourceVariant			= 1;

	switch(bmih->biCompression) {
	case BI_RGB:
		inverted = bmih->biHeight >= 0;
		switch(bmih->biBitCount) {
		case 8:
			mSourceLayout.format = nsVDPixmap::kPixFormat_Pal8;
			break;
		case 16:
			mSourceLayout.format = nsVDPixmap::kPixFormat_XRGB1555;
			break;
		case 24:
			mSourceLayout.format = nsVDPixmap::kPixFormat_RGB888;
			break;
		case 32:
			mSourceLayout.format = nsVDPixmap::kPixFormat_XRGB8888;
			break;
		}
		break;
	case BI_BITFIELDS:
		if (getFormatLen() >= sizeof(BITMAPINFOHEADER) + sizeof(DWORD)*3) {
			BITMAPV4HEADER& hdr4 = *(BITMAPV4HEADER *)bmih;		// Only the rgb masks are guaranteed to be valid
			const DWORD red		= hdr4.bV4RedMask;
			const DWORD grn		= hdr4.bV4GreenMask;
			const DWORD blu		= hdr4.bV4BlueMask;
			const DWORD count	= bmih->biBitCount;

			if (red == 0x00007c00 && grn == 0x000003e0 && blu == 0x0000001f && count == 16)
				mSourceLayout.format = nsVDPixmap::kPixFormat_XRGB1555;
			else if (red == 0x0000f800 && grn == 0x000007e0 && blu == 0x0000001f && count == 16)
				mSourceLayout.format = nsVDPixmap::kPixFormat_RGB565;
			else if (red == 0x00ff0000 && grn == 0x0000ff00 && blu == 0x000000ff && count == 24)
				mSourceLayout.format = nsVDPixmap::kPixFormat_RGB888;
			else if (red == 0x00ff0000 && grn == 0x0000ff00 && blu == 0x000000ff && count == 32)
				mSourceLayout.format = nsVDPixmap::kPixFormat_XRGB8888;
		}
		break;
	default:
		extern bool VDPreferencesIsDirectYCbCrInputEnabled();
		if (VDPreferencesIsDirectYCbCrInputEnabled()) {
			switch(bmih->biCompression) {
			case 'VYUY':		// YUYV
			case '2YUY':		// YUY2
				mSourceLayout.format = nsVDPixmap::kPixFormat_YUV422_YUYV;
				break;
			case 'YVYU':		// UYVY
				mSourceLayout.format = nsVDPixmap::kPixFormat_YUV422_UYVY;
				break;
			case '21VY':		// YV12
				mSourceLayout.format = nsVDPixmap::kPixFormat_YUV420_Planar;
				break;
			case 'VUYI':		// IYUV
				mSourceLayout.format = nsVDPixmap::kPixFormat_YUV420_Planar;
				mSourceVariant = 3;
				break;
			case '024I':		// I420
				mSourceLayout.format = nsVDPixmap::kPixFormat_YUV420_Planar;
				mSourceVariant = 2;
				break;
			}
		}
		break;
	}

	is_dib = (mSourceLayout.format != 0);

	if (mSourceLayout.format)
		VDMakeBitmapCompatiblePixmapLayout(mSourceLayout, mSourceLayout.w, mSourceLayout.h, mSourceLayout.format, mSourceVariant);

	// If this is MJPEG, check to see if we should modify the output format and/or stream info

	mSampleFirst = pAVIStream->Start();
	mSampleLast = pAVIStream->End();

	if (is_mjpeg) {
		BITMAPINFOHEADER *pbih = getImageFormat();

		if (mjpeg_mode && mjpeg_mode != IFMODE_SWAP && pbih->biHeight > 288) {
			pbih->biHeight /= 2;

			if (mjpeg_mode == IFMODE_SPLIT1 || mjpeg_mode == IFMODE_SPLIT2) {
				if (streamInfo.dwRate >= 0x7FFFFFFF)
					streamInfo.dwScale >>= 1;
				else
					streamInfo.dwRate *= 2;

				streamInfo.dwLength *= 2;
				mSampleLast = mSampleLast*2 - mSampleFirst;
			}
		}

		if (mjpeg_mode) {
			if (!(mjpeg_splits = new long[(size_t)(mSampleLast - mSampleFirst)]))
				throw MyMemoryError();

			for(int i=0; i<mSampleLast-mSampleFirst; i++)
				mjpeg_splits[i] = -1;
		}
	} else
		mjpeg_mode = 0;

	// allocate framebuffer

	if (!AllocFrameBuffer(bmih->biWidth * 4 * bmih->biHeight + 4))
		throw MyMemoryError();

	// get a decompressor
	//
	// 'DIB ' is the official value for uncompressed AVIs, but some stupid
	// programs also use (null) and 'RAW '
	//
	// NOTE: Don't handle RLE4/RLE8 here.  RLE is slightly different in AVI files!

	if (bmih->biCompression == BI_BITFIELDS
		|| (bmih->biCompression == BI_RGB && bmih->biBitCount<16 && bmih->biBitCount != 8)) {

		// Ugh.  It's one of them weirdo formats.  Let GDI handle it!

		fUseGDI = true;

	} else if (!is_dib) {
		FOURCC fccOriginalCodec = bmih->biCompression;

		// If it's a hacked MPEG-4 driver, try all of the known hacks. #*$&@#(&$)(#$
		// They're all the same driver, so they're mutually compatible.

		bool bFailed = false;

		if (use_internal) {
			bFailed = true;
		} else {
			vdprotected2("attempting codec negotiation: fccHandler=0x%08x, biCompression=0x%08x", unsigned, streamInfo.fccHandler, unsigned, fccOriginalCodec) {
				VDExternalCodeBracket bracket(L"A video codec", __FILE__, __LINE__);

				switch(bmih->biCompression) {
				case '34PM':		// Microsoft MPEG-4 V3
				case '3VID':		// "DivX Low-Motion" (4.10.0.3917)
				case '4VID':		// "DivX Fast-Motion" (4.10.0.3920)
				case '5VID':		// unknown
				case '14PA':		// "AngelPotion Definitive" (4.0.00.3688)
					if (!AttemptCodecNegotiation(bmih)) {
						bmih->biCompression = '34PM';
						if (!AttemptCodecNegotiation(bmih)) {
							bmih->biCompression = '3VID';
							if (!AttemptCodecNegotiation(bmih)) {
								bmih->biCompression = '4VID';
								if (!AttemptCodecNegotiation(bmih)) {
									bmih->biCompression = '14PA';
				default:
									if (!AttemptCodecNegotiation(bmih))
										bFailed = true;
								}
							}
						}
					}
					break;
				}
			}
		}

		if (bFailed) {
			// Is it MJPEG or DV?  Maybe we can do it ourselves.

			const BITMAPINFOHEADER *bihSrc = getImageFormat();
			const int w = bihSrc->biWidth;
			const int h = bihSrc->biHeight;

			if (is_mjpeg) {
				vdautoptr<VDVideoDecompressorMJPEG> pDecoder(new_nothrow VDVideoDecompressorMJPEG);
				pDecoder->Init(w, h);

				mpDecompressor = pDecoder.release();
			} else if (is_dv && w==720 && (h == 480 || h == 576)) {
				vdautoptr<VDVideoDecompressorDV> pDecoder(new_nothrow VDVideoDecompressorDV);
				pDecoder->Init(w, h);

				mpDecompressor = pDecoder.release();
			} else {
				const char *s = LookupVideoCodec(fccOriginalCodec);

				throw MyError("Couldn't locate decompressor for format '%c%c%c%c' (%s)\n"
								"\n"
								"VirtualDub requires a Video for Windows (VFW) compatible codec to decompress "
								"video. DirectShow codecs, such as those used by Windows Media Player, are not "
								"suitable."
							,(fccOriginalCodec    ) & 0xff
							,(fccOriginalCodec>> 8) & 0xff
							,(fccOriginalCodec>>16) & 0xff
							,(fccOriginalCodec>>24) & 0xff
							,s ? s : "unknown");
			}
		}
	}
}

DWORD VDSafeICDecompressQueryW32(HIC hic, LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	vdstructex<BITMAPINFOHEADER> bihIn, bihOut;
	int cbIn = 0, cbOut = 0;

	if (lpbiIn) {
		cbIn = VDGetSizeOfBitmapHeaderW32(lpbiIn);
		bihIn.assign(lpbiIn, cbIn);
	}

	if (lpbiOut) {
		cbOut = VDGetSizeOfBitmapHeaderW32(lpbiOut);
		bihOut.assign(lpbiIn, cbOut);
	}

	// AngelPotion overwrites its input format with biCompression='MP43' and doesn't
	// restore it, which leads to video codec lookup errors.  So what we do here is
	// make a copy of the format in nice, safe memory and feed that in instead.

	// We used to write protect the format here, but apparently some versions of Windows
	// have certain functions accepting (BITMAPINFOHEADER *) that actually call
	// IsBadWritePtr() to verify the incoming pointer, even though those functions
	// don't actually need to write to the format.  It would be nice if someone learned
	// what 'const' was for.  AngelPotion doesn't crash because it has a try/catch
	// handler wrapped around its code.

	DWORD result = ICDecompressQuery(hic, lpbiIn ? bihIn.data() : NULL, lpbiOut ? bihOut.data() : NULL);

	// check for unwanted modification
	static bool sbBadCodecDetected = false;		// we only need one warning, not a billion....
	if (!sbBadCodecDetected && ((lpbiIn && memcmp(bihIn.data(), lpbiIn, cbIn)) || (lpbiOut && memcmp(bihOut.data(), lpbiOut, cbOut)))) {
		ICINFO info = {sizeof(ICINFO)};
		ICGetInfo(hic, &info, sizeof info);

		const wchar_t *ppName = info.szDescription;
		VDLogAppMessage(kVDLogWarning, kVDST_VideoSource, kVDM_CodecRenamingDetected, 1, &ppName);
		sbBadCodecDetected = true;
	}

	return result;
}

HIC VDSafeICOpenW32(DWORD fccType, DWORD fccHandler, UINT wMode) {
	HIC hic;
	
	vdprotected1("attempting to open video codec with FOURCC '%.4s'", const char *, (const char *)&fccHandler) {
		wchar_t buf[64];
		_swprintf(buf, L"A video codec with FOURCC '%.4S'", (const char *)&fccHandler);
		VDExternalCodeBracket bracket(buf, __FILE__, __LINE__);
		hic = ICOpen(fccType, fccHandler, wMode);
	}

	return hic;
}

HIC VDSafeICLocateDecompressW32(DWORD fccType, DWORD fccHandler, LPBITMAPINFOHEADER lpbiIn, LPBITMAPINFOHEADER lpbiOut) {
	ICINFO info={0};

	for(DWORD id=0; ICInfo(fccType, id, &info); ++id) {
		info.dwSize = sizeof(ICINFO);	// I don't think this is necessary, but just in case....

		HIC hic = VDSafeICOpenW32(fccType, info.fccHandler, ICMODE_DECOMPRESS);

		if (!hic)
			continue;

		DWORD result = VDSafeICDecompressQueryW32(hic, lpbiIn, lpbiOut);

		if (result == ICERR_OK) {
			// Check for a codec that doesn't actually support what it says it does.
			// We ask the codec whether it can do a specific conversion that it can't
			// possibly support.  If it does it, then we call BS and ignore the codec.
			// The Grand Tech Camera Codec and Panasonic DV codecs are known to do this.
			//
			// (general idea from Raymond Chen's blog)

			BITMAPINFOHEADER testSrc = {		// note: can't be static const since IsBadWritePtr() will get called on it
				sizeof(BITMAPINFOHEADER),
				320,
				240,
				1,
				24,
				0x2E532E42,
				320*240*3,
				0,
				0,
				0,
				0
			};

			static bool sbBSReported = false;	// Only report once per session.

			if (!sbBSReported && ICERR_OK == ICDecompressQuery(hic, &testSrc, NULL)) {		// Don't need to wrap this, as it's OK if testSrc gets modified.
				ICINFO info = {sizeof(ICINFO)};

				ICGetInfo(hic, &info, sizeof info);

				const wchar_t *ppName = info.szDescription;
				VDLogAppMessage(kVDLogWarning, kVDST_VideoSource, kVDM_CodecAcceptsBS, 1, &ppName);
				sbBSReported = true;

				// Okay, let's give the codec a chance to redeem itself. Reformat the input format into
				// a plain 24-bit RGB image, and ask it what the compressed format is. If it produces
				// a FOURCC that matches, allow it to handle the format. This should allow at least
				// the codec's primary format to work. Otherwise, drop it on the ground.
				
				if (lpbiIn) {
					BITMAPINFOHEADER unpackedSrc={
						sizeof(BITMAPINFOHEADER),
						lpbiIn ? lpbiIn->biWidth : 320,
						lpbiIn ? lpbiIn->biHeight : 240,
						1,
						24,
						BI_RGB,
						0,
						0,
						0,
						0,
						0
					};

					unpackedSrc.biSizeImage = ((unpackedSrc.biWidth*3+3)&~3)*unpackedSrc.biHeight;

					LONG size = ICCompressGetFormatSize(hic, &unpackedSrc);

					if (size >= sizeof(BITMAPINFOHEADER)) {
						vdstructex<BITMAPINFOHEADER> tmp;

						tmp.resize(size);
						if (ICERR_OK == ICCompressGetFormat(hic, &unpackedSrc, tmp.data()) && isEqualFOURCC(tmp->biCompression, lpbiIn->biCompression))
							return hic;
					}
				}
			} else {
				return hic;
			}
		}

		ICClose(hic);
	}

	return NULL;
}

bool VideoSourceAVI::AttemptCodecNegotiation(BITMAPINFOHEADER *bmih) {
	HIC hicDecomp = NULL;

	// Try the handler specified in the file first.  In some cases, it'll
	// be wrong or missing. (VideoMatrix, among other programs, sets fccHandler=0.

	if (streamInfo.fccHandler)
		hicDecomp = VDSafeICOpenW32(ICTYPE_VIDEO, streamInfo.fccHandler, ICMODE_DECOMPRESS);

	if (!hicDecomp || ICERR_OK!=VDSafeICDecompressQueryW32(hicDecomp, bmih, NULL)) {
		if (hicDecomp)
			ICClose(hicDecomp);

		// Pick a handler based on the biCompression field instead. We should imitate the
		// mappings that ICLocate() does -- namely, BI_RGB and BI_RLE8 map to MRLE, and
		// CRAM maps to MSVC (apparently an outdated name for Microsoft Video 1).

		DWORD fcc = bmih->biCompression;

		if (fcc == BI_RGB || fcc == BI_RLE8)
			fcc = 'ELRM';
		else if (isEqualFOURCC(fcc, 'MARC'))
			fcc = 'CVSM';

		if (fcc >= 0x10000)		// if we couldn't map a numerical value like BI_BITFIELDS, don't open a random codec
			hicDecomp = VDSafeICOpenW32(ICTYPE_VIDEO, fcc, ICMODE_DECOMPRESS);

		if (!hicDecomp || ICERR_OK!=VDSafeICDecompressQueryW32(hicDecomp, bmih, NULL)) {
			if (hicDecomp)
				ICClose(hicDecomp);

			// Okay, search all installed codecs.
			hicDecomp = VDSafeICLocateDecompressW32(ICTYPE_VIDEO, NULL, bmih, NULL);
		}
	}

	if (!hicDecomp)
		return false;

	// check for bad MPEG-4 V2/V3 codec

	if (isEqualFOURCC(bmih->biCompression, '24PM')) {
		if (!CheckMPEG4Codec(hicDecomp, false)) {
			ICClose(hicDecomp);
			return false;
		}
	} else if (isEqualFOURCC(bmih->biCompression, '34PM')) {
		if (!CheckMPEG4Codec(hicDecomp, true)) {
			ICClose(hicDecomp);
			return false;
		}
	}

	// All good!

	mpDecompressor = new VDVideoDecompressorVCM;
	static_cast<VDVideoDecompressorVCM *>(&*mpDecompressor)->Init(getImageFormat(), hicDecomp);
	return true;
}

///////////////////////////////////////////////////////////////////////////

void VideoSourceAVI::Reinit() {
	VDPosition nOldFrames, nNewFrames;

	nOldFrames = mSampleLast - mSampleFirst;
	nNewFrames = pAVIStream->End() - pAVIStream->Start();

	if (mjpeg_mode==IFMODE_SPLIT1 || mjpeg_mode==IFMODE_SPLIT2) {
		nOldFrames >>= 1;
	}

	if (nOldFrames != nNewFrames && mjpeg_mode) {
		// We have to resize the mjpeg_splits array.

		long *pNewSplits = new long[nNewFrames];

		if (!pNewSplits)
			throw MyMemoryError();

		int i;

		memcpy(pNewSplits, mjpeg_splits, sizeof(long)*nOldFrames);

		for(i=nOldFrames; i<nNewFrames; i++)
			pNewSplits[i] = -1;

		delete[] mjpeg_splits;

		mjpeg_splits = pNewSplits;
	}

	if (pAVIStream->Info(&streamInfo, sizeof streamInfo))
		throw MyError("Error obtaining video stream info.");

	mSampleFirst = pAVIStream->Start();

	if (mjpeg_mode==IFMODE_SPLIT1 || mjpeg_mode==IFMODE_SPLIT2) {
		if (streamInfo.dwRate >= 0x7FFFFFFF)
			streamInfo.dwScale >>= 1;
		else
			streamInfo.dwRate *= 2;

		mSampleLast = pAVIStream->End() * 2 - mSampleFirst;
	} else
		mSampleLast = pAVIStream->End();

	streamInfo.dwLength		= (DWORD)std::min<VDPosition>(0xFFFFFFFF, mSampleLast - mSampleFirst);
}

void VideoSourceAVI::redoKeyFlags() {
	VDPosition lSample;
	long lMaxFrame=0;
	uint32 lActualBytes, lActualSamples;
	int err;
	void *lpInputBuffer = NULL;
	BOOL fStreamBegun = FALSE;
	int iBytes;
	long *pFrameSums;

	if (!mpDecompressor)
		return;

	iBytes = (mSampleLast+7-mSampleFirst)>>3;

	if (!(key_flags = new char[iBytes]))
		throw MyMemoryError();

	memset(key_flags, 0, iBytes);

	// Find maximum frame

	lSample = mSampleFirst;
	while(lSample < mSampleLast) {
		err = _read(lSample, 1, NULL, 0, &lActualBytes, &lActualSamples);
		if (err == AVIERR_OK)
//			throw MyAVIError("VideoSource", err);

			if (lActualBytes > lMaxFrame) lMaxFrame = lActualBytes;

		++lSample;
	}

	if (!setDecompressedFormat(24))
		if (!setDecompressedFormat(32))
			if (!setDecompressedFormat(16))
				if (!setDecompressedFormat(8))
					throw MyError("Video decompressor is incapable of decompressing to an RGB format.");

	if (!(lpInputBuffer = new char[((lMaxFrame+7)&-8) + lMaxFrame]))
		throw MyMemoryError();

	if (!(pFrameSums = new long[mSampleLast - mSampleFirst])) {
		delete[] lpInputBuffer;
		throw MyMemoryError();
	}

	try {
		ProgressDialog pd(NULL, "AVI Import Filter", "Rekeying video stream", (mSampleLast - mSampleFirst)*2, true);
		pd.setValueFormat("Frame %ld of %ld");

		streamBegin(true);
		fStreamBegun = TRUE;

		lSample = mSampleFirst;
		while(lSample < mSampleLast) {
			long lWhiteTotal=0;
			long x, y;
			const long lWidth	= (mpTargetFormatHeader->biWidth * mpTargetFormatHeader->biBitCount + 7)/8;
			const long lModulo	= (4-lWidth)&3;
			const long lHeight	= mpTargetFormatHeader->biHeight;
			unsigned char *ptr;

			_RPT1(0,"Rekeying frame %ld\n", lSample);

			err = _read(lSample, 1, lpInputBuffer, lMaxFrame, &lActualBytes, &lActualSamples);
			if (err != AVIERR_OK)
//				throw MyAVIError("VideoSourceAVI", err);
				goto rekey_error;


			streamGetFrame(lpInputBuffer, lActualBytes, FALSE, lSample);

			ptr = (unsigned char *)lpvBuffer;
			y = lHeight;
			do {
				x = lWidth;
				do {
					lWhiteTotal += (long)*ptr++;
					lWhiteTotal ^= 0xAAAAAAAA;
				} while(--x);

				ptr += lModulo;
			} while(--y);


			pFrameSums[lSample - mSampleFirst] = lWhiteTotal;


//			if (lBlackTotal == lWhiteTotal)
//				key_flags[(lSample - mSampleFirst)>>3] |= 1<<((lSample-mSampleFirst)&7);

rekey_error:
			++lSample;

			pd.advance(lSample - mSampleFirst);
			pd.check();
		}

		lSample = mSampleFirst;
		do {
			long lWhiteTotal=0;
			long x, y;
			const long lWidth	= (mpTargetFormatHeader->biWidth * mpTargetFormatHeader->biBitCount + 7)/8;
			const long lModulo	= (4-lWidth)&3;
			const long lHeight	= mpTargetFormatHeader->biHeight;
			unsigned char *ptr;

			_RPT1(0,"Rekeying frame %ld\n", lSample);

			err = _read(lSample, 1, lpInputBuffer, lMaxFrame, &lActualBytes, &lActualSamples);
			if (err != AVIERR_OK)
//				throw MyAVIError("VideoSourceAVI", err);
				goto rekey_error2;

			streamGetFrame(lpInputBuffer, lActualBytes, FALSE, lSample);

			ptr = (unsigned char *)lpvBuffer;
			y = lHeight;
			do {
				x = lWidth;
				do {
					lWhiteTotal += (long)*ptr++;
					lWhiteTotal ^= 0xAAAAAAAA;
				} while(--x);

				ptr += lModulo;
			} while(--y);


			if (lWhiteTotal == pFrameSums[lSample - mSampleFirst])
				key_flags[(lSample - mSampleFirst)>>3] |= 1<<((lSample-mSampleFirst)&7);

rekey_error2:
			if (lSample == mSampleFirst)
				lSample = mSampleLast-1;
			else
				--lSample;

			pd.advance(mSampleLast*2 - (lSample+mSampleFirst));
			pd.check();
		} while(lSample >= mSampleFirst+1);

		streamEnd();
	} catch(...) {
		if (fStreamBegun) streamEnd();
		delete[] lpInputBuffer;
		delete[] pFrameSums;
		throw;
	}

	delete[] lpInputBuffer;
	delete[] pFrameSums;
}

int VideoSourceAVI::_read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead) {
	IAVIReadStream *pSource = pAVIStream;
	bool phase = (lStart - mSampleFirst)&1;

	if (mjpeg_mode == IFMODE_SPLIT1 || mjpeg_mode == IFMODE_SPLIT2)
		lStart = mSampleFirst + (lStart - mSampleFirst)/2;

	// If we're striping, then we have to lookup the correct stripe for this sample.

	if (stripesys) {
		AVIStripeIndexEntry *asie;
		long offset;

		if (!(asie = stripe_index->lookup(lStart)))
			return AVIERR_FILEREAD;

		offset = lStart - asie->lSampleFirst;

		if (lCount > asie->lSampleCount-offset)
			lCount = asie->lSampleCount-offset;

		if (!stripe_streams[asie->lStripe])
			return AVIERR_FILEREAD;

		pSource = stripe_streams[asie->lStripe];
		lStart = asie->lStripeSample + offset;
	}

	// MJPEG modification mode?

	if (mjpeg_mode) {
		int res;
		LONG lBytes, lSamples;
		long lOffset, lLength;

		// Did we just read in this sample!?

		if (lStart == mjpeg_last) {
			lBytes = mjpeg_last_size;
			res = AVIERR_OK;
		} else {

			// Read the sample into memory.  If we don't have a lpBuffer *and* already know
			// where the split is, just get the size.

			if (lpBuffer || mjpeg_splits[lStart - mSampleFirst]<0) {

				mjpeg_last = -1;

				if (mjpeg_reorder_buffer_size)
					res = pSource->Read(lStart, 1, mjpeg_reorder_buffer, mjpeg_reorder_buffer_size, &lBytes, &lSamples);

				if (res == AVIERR_BUFFERTOOSMALL || !mjpeg_reorder_buffer_size) {
					void *new_buffer;
					int new_size;

					res = pSource->Read(lStart, 1, NULL, 0, &lBytes, &lSamples);

					if (res == AVIERR_OK) {

						_ASSERT(lBytes != 0);

						new_size = (lBytes + 4095) & -4096;
//						new_size = lBytes;
						new_buffer = reallocmem(mjpeg_reorder_buffer, new_size);

						if (!new_buffer)
							return AVIERR_MEMORY;

						mjpeg_reorder_buffer = new_buffer;
						mjpeg_reorder_buffer_size = new_size;

						res = pSource->Read(lStart, 1, mjpeg_reorder_buffer, mjpeg_reorder_buffer_size, &lBytes, &lSamples);
					}
				}

				if (res == AVIERR_OK) {
					mjpeg_last = lStart;
					mjpeg_last_size = lBytes;
				}
			} else
				res = pSource->Read(lStart, 1, NULL, 0, &lBytes, &lSamples);
		}


		if (res != AVIERR_OK) {
			if (lBytesRead)
				*lBytesRead = 0;
			if (lSamplesRead)
				*lSamplesRead = 0;
			return res;
		} else if (!lBytes) {
			if (lBytesRead)
				*lBytesRead = 0;
			if (lSamplesRead)
				*lSamplesRead = 1;
			return AVIERR_OK;
		}

		// Attempt to find SOI tag in sample

		lOffset = 0;
		lLength = lBytes;

		{
			int i;

			// Do we already know where the split is?

			if (mjpeg_splits[lStart - mSampleFirst]<0) {
				for(i=2; i<lBytes-2; i++)
					if (((unsigned char *)mjpeg_reorder_buffer)[i] == (unsigned char)0xFF
							&& ((unsigned char *)mjpeg_reorder_buffer)[i+1] == (unsigned char)0xD8)
						break;

				mjpeg_splits[lStart - mSampleFirst] = i;
			} else {
				i = mjpeg_splits[lStart - mSampleFirst];
			}

			if (i<lBytes-2) {
				if (mjpeg_mode != IFMODE_SWAP) {
					switch(mjpeg_mode) {
					case IFMODE_SPLIT2:
						phase = !phase;
						break;
					case IFMODE_DISCARD1:
						phase = false;
						break;
					case IFMODE_DISCARD2:
						phase = true;
						break;
					}

					if (phase) {
						lOffset = i;
						lLength = lBytes - i;
					} else {
						lOffset = 0;
						lLength = i;
					}
				} else
					lOffset = i;
			}
		}

		if (lpBuffer) {
			if (lSamplesRead)
				*lSamplesRead = 1;
			if (lBytesRead)
				*lBytesRead = lLength;

			if (cbBuffer < lLength)
				return AVIERR_BUFFERTOOSMALL;

			if (mjpeg_mode == IFMODE_SWAP) {
				char *pp1 = (char *)lpBuffer;
				char *pp2 = (char *)lpBuffer + (lBytes - lOffset);

				memcpy(pp1, (char *)mjpeg_reorder_buffer+lOffset, lBytes - lOffset);
				if (lOffset)
					memcpy(pp2, mjpeg_reorder_buffer, lOffset);

				// patch phase on both MJPEG headers

				if (((short *)pp1)[1]==(short)0xE0FF)
					pp1[10] = 1;

				if (((short *)pp2)[1]==(short)0xE0FF)
					pp2[10] = 2;
				
			} else {

				memcpy(lpBuffer, (char *)mjpeg_reorder_buffer+lOffset, lLength);

				// patch phase on MJPEG header by looking for APP0 tag (xFFE0)

				// FFD8 FFE0 0010 'AVI1' polarity

				if (((short *)lpBuffer)[1]==(short)0xE0FF)
					((char *)lpBuffer)[10] = 0;
			}

			return AVIERR_OK;
		} else {
			if (lSamplesRead)
				*lSamplesRead = 1;
			if (lBytesRead)
				*lBytesRead = lLength;

			return AVIERR_OK;
		}

	} else {
		LONG samplesRead, bytesRead;

		int rv = pSource->Read(lStart, lCount, lpBuffer, cbBuffer, &bytesRead, &samplesRead);

		if (lSamplesRead)
			*lSamplesRead = samplesRead;
		if (lBytesRead)
			*lBytesRead = bytesRead;

		if (lpBuffer && bInvertFrames && !rv && samplesRead) {
			const BITMAPINFOHEADER& hdr = *getImageFormat();
			const long dpr = ((hdr.biWidth * hdr.biBitCount + 31)>>5);

			if (bytesRead >= dpr * 4 * hdr.biHeight) {		// safety check
				const int h2 = hdr.biHeight >> 1;
				long *p0 = (long *)lpBuffer;
				long *p1 = (long *)lpBuffer + dpr * (hdr.biHeight - 1);

				for(int y=0; y<h2; ++y) {
					for(int x=0; x<dpr; ++x) {
						long t = p0[x];
						p0[x] = p1[x];
						p1[x] = t;
					}
					p0 += dpr;
					p1 -= dpr;
				}
			}
		}

		return rv;
	}
}

sint64 VideoSourceAVI::getSampleBytePosition(VDPosition pos) {
	IAVIReadStream *pSource = pAVIStream;

	if (mjpeg_mode == IFMODE_SPLIT1 || mjpeg_mode == IFMODE_SPLIT2)
		pos = mSampleFirst + (pos - mSampleFirst)/2;

	// If we're striping, then we have to lookup the correct stripe for this sample.

	if (stripesys) {
		AVIStripeIndexEntry *asie;
		VDPosition offset;

		if (!(asie = stripe_index->lookup(pos)))
			return -1;

		offset = pos - asie->lSampleFirst;

		if (!stripe_streams[asie->lStripe])
			return -1;

		pSource = stripe_streams[asie->lStripe];
		pos = asie->lStripeSample + offset;
	}

	return pSource->getSampleBytePosition(pos);
}

bool VideoSourceAVI::_isKey(VDPosition samp) {
	if ((mjpeg_mode & -2) == IFMODE_SPLIT1)
		samp = mSampleFirst + (samp - mSampleFirst)/2;

	if (key_flags) {
		samp -= mSampleFirst;

		return !!(key_flags[samp>>3] & (1<<(samp&7)));
	} else
		return pAVIStream->IsKeyFrame(samp);
}

VDPosition VideoSourceAVI::nearestKey(VDPosition lSample) {
	if (key_flags) {
		if (lSample < mSampleFirst || lSample >= mSampleLast)
			return -1;

		if (_isKey(lSample)) return lSample;

		return prevKey(lSample);
	}

//	if (lNear == -1)
//		throw MyError("VideoSourceAVI: error getting previous key frame");

	if ((mjpeg_mode & -2) == IFMODE_SPLIT1) {
		return pAVIStream->NearestKeyFrame(mSampleFirst + (lSample-mSampleFirst)/2)*2-mSampleFirst;
	} else {
		return pAVIStream->NearestKeyFrame(lSample);
	}
}

VDPosition VideoSourceAVI::prevKey(VDPosition lSample) {
	if ((mjpeg_mode & -2) == IFMODE_SPLIT1) {
		lSample = mSampleFirst + (lSample - mSampleFirst)/2;

		if (key_flags) {
			if (lSample >= mSampleLast) return -1;

			while(--lSample >= mSampleFirst)
				if (_isKey(lSample)) return lSample*2-mSampleFirst;

			return -1;
		} else
			return pAVIStream->PrevKeyFrame(lSample)*2-mSampleFirst;
	} else {
		if (key_flags) {
			if (lSample >= mSampleLast) return -1;

			while(--lSample >= mSampleFirst)
				if (_isKey(lSample)) return lSample;

			return -1;
		} else
			return pAVIStream->PrevKeyFrame(lSample);
	}
}

VDPosition VideoSourceAVI::nextKey(VDPosition lSample) {
	if ((mjpeg_mode & -2) == IFMODE_SPLIT1) {
		lSample = mSampleFirst + (lSample - mSampleFirst)/2;

		if (key_flags) {
			if (lSample < mSampleFirst) return -1;

			while(++lSample < mSampleLast)
				if (_isKey(lSample)) return lSample*2 - mSampleFirst;

			return -1;
		} else
			return pAVIStream->NextKeyFrame(lSample)*2 - mSampleFirst;

	} else {

		if (key_flags) {
			if (lSample < mSampleFirst) return -1;

			while(++lSample < mSampleLast)
				if (_isKey(lSample)) return lSample;

			return -1;
		} else
			return pAVIStream->NextKeyFrame(lSample);

	}
}

bool VideoSourceAVI::setTargetFormat(int format) {
	using namespace nsVDPixmap;

	streamEnd();

	bDirectDecompress = false;

	if (!format && mSourceLayout.format)
		format = mSourceLayout.format;

	if (format && format == mSourceLayout.format) {
		if (VideoSource::setTargetFormatVariant(format, mSourceVariant)) {
			if (format == kPixFormat_Pal8)
				mTargetFormat.palette = mPalette;
			bDirectDecompress = true;
			return true;
		}

		return false;
	}

	if (fUseGDI) {
		void *pv;
		HDC hdc;

		if (!format)
			format = kPixFormat_XRGB8888;

		if (format != kPixFormat_XRGB1555 && format != kPixFormat_RGB888 && format != kPixFormat_XRGB8888)
			return false;

		if (!VideoSource::setTargetFormat(format))
			return false;

		if (hbmLame)
			DeleteObject(hbmLame);

		hbmLame = NULL;

		if (hdc = CreateCompatibleDC(NULL)) {
			hbmLame = CreateDIBSection(hdc, (BITMAPINFO *)mpTargetFormatHeader.data(), DIB_RGB_COLORS, &pv, hBufferObject, 0);
			DeleteDC(hdc);
		}

		return true;
	} else if (mpDecompressor) {
		// attempt direct decompression
		if (format) {
			vdstructex<BITMAPINFOHEADER> desiredFormat;
			vdstructex<BITMAPINFOHEADER> srcFormat(getImageFormat(), getFormatLen());

			const int variants = VDGetPixmapToBitmapVariants(format);

			for(int variant=1; variant<=variants; ++variant) {
				if (VDMakeBitmapFormatFromPixmapFormat(desiredFormat, srcFormat, format, variant)) {
					if (srcFormat->biCompression == desiredFormat->biCompression
						&& srcFormat->biBitCount == desiredFormat->biBitCount
						&& srcFormat->biSizeImage == desiredFormat->biSizeImage
						&& srcFormat->biWidth == desiredFormat->biWidth
						&& srcFormat->biHeight == desiredFormat->biHeight
						&& srcFormat->biPlanes == desiredFormat->biPlanes) {

						mpTargetFormatHeader = srcFormat;

						VDVERIFY(VideoSource::setTargetFormatVariant(format, variant));

						if (format == kPixFormat_Pal8)
							mTargetFormat.palette = mPalette;

						bDirectDecompress = true;

						invalidateFrameBuffer();
						return true;
					}
				}
			}

			// no variants were an exact match
		}

		if (mpDecompressor->SetTargetFormat(format)) {
			VDVERIFY(VideoSource::setTargetFormatVariant(mpDecompressor->GetTargetFormat(), mpDecompressor->GetTargetFormatVariant()));
			return true;
		}
		return false;
	} else {
		if (!VideoSource::setTargetFormat(format))
			return false;

		return true;
	}
}

bool VideoSourceAVI::setDecompressedFormat(const BITMAPINFOHEADER *pbih) {
	streamEnd();

	if (pbih->biCompression == BI_RGB && pbih->biBitCount > 8)
		return setDecompressedFormat(pbih->biBitCount);

	if (pbih->biCompression == getImageFormat()->biCompression) {
		const BITMAPINFOHEADER *pbihSrc = getImageFormat();
		if (pbih->biBitCount == pbihSrc->biBitCount
			&& pbih->biSizeImage == pbihSrc->biSizeImage
			&& pbih->biWidth == pbihSrc->biWidth
			&& pbih->biHeight == pbihSrc->biHeight
			&& pbih->biPlanes == pbihSrc->biPlanes) {

			mpTargetFormatHeader.assign(pbih, sizeof(BITMAPINFOHEADER));
			if (mSourceLayout.format)
				mTargetFormat = VDPixmapFromLayout(mSourceLayout, lpvBuffer);
			else
				mTargetFormat.format = 0;

			mTargetFormat.palette = mPalette;

			bDirectDecompress = true;

			invalidateFrameBuffer();
			return true;
		}
	}

	mTargetFormat.format = 0;

	if (mpDecompressor && mpDecompressor->SetTargetFormat(pbih)) {
		mpTargetFormatHeader.assign(pbih, sizeof(BITMAPINFOHEADER));

		invalidateFrameBuffer();
		bDirectDecompress = false;
		return true;
	}

	return false;
}

////////////////////////////////////////////////

void DIBconvert(void *src0, BITMAPINFOHEADER *srcfmt, void *dst0, BITMAPINFOHEADER *dstfmt) {
	const DWORD srcType = srcfmt->biCompression;
	if (srcType == '21VY') {
		VDPixmap pxs = {0};
		VDPixmap pxd = {0};

		pxs.data		= src0;
		pxs.w			= srcfmt->biWidth;
		pxs.h			= srcfmt->biHeight;
		pxs.format		= nsVDPixmap::kPixFormat_YUV420_Planar;
		pxs.pitch		= srcfmt->biWidth;
		pxs.data3		= (char *)pxs.data + pxs.pitch * pxs.h;
		pxs.pitch3		= (pxs.w + 1) >> 1;
		pxs.data2		= (char *)pxs.data3 + pxs.pitch3 * ((pxs.h+1)>>1);
		pxs.pitch2		= pxs.pitch3;

		pxd.w			= dstfmt->biWidth;
		pxd.h			= dstfmt->biHeight;

		switch(dstfmt->biBitCount) {
		case 16:
			pxd.format		= nsVDPixmap::kPixFormat_XRGB1555;
			break;
		case 24:
			pxd.format		= nsVDPixmap::kPixFormat_RGB888;
			break;
		case 32:
			pxd.format		= nsVDPixmap::kPixFormat_XRGB8888;
			break;
		default:
			return;
		}

		pxd.pitch		= -((dstfmt->biWidth * dstfmt->biBitCount + 31)>>5)*4;
		pxd.data		= (char *)dst0 - pxd.pitch * (dstfmt->biHeight - 1);

		VDPixmapBlt(pxd, pxs);
	} else {
		VBitmap dstbm(dst0, dstfmt);
		VBitmap srcbm(src0, srcfmt);

		if (srcfmt->biCompression == '2YUY')
			dstbm.BitBltFromYUY2(0, 0, &srcbm, 0, 0, -1, -1);
		else if (srcfmt->biCompression == '024I')
			VBitmap(dst0, dstfmt).BitBltFromI420(0, 0, &srcbm, 0, 0, -1, -1);
		else
			VBitmap(dst0, dstfmt).BitBlt(0, 0, &srcbm, 0, 0, -1, -1);
	}
}

void VideoSourceAVI::DecompressFrame(const void *pSrc) {
	if (!VDINLINEASSERT(mTargetFormat.format))
		return;

	VDPixmap src(VDPixmapFromLayout(mSourceLayout, (void *)pSrc));

	VDPixmapBlt(mTargetFormat, src);
}

////////////////////////////////////////////////

void VideoSourceAVI::invalidateFrameBuffer() {
	if (lLastFrame != -1 && mpDecompressor)
		mpDecompressor->Stop();

	lLastFrame = -1;
	mbConcealingErrors = false;
}

bool VideoSourceAVI::isFrameBufferValid() {
	return lLastFrame != -1;
}

char VideoSourceAVI::getFrameTypeChar(VDPosition lFrameNum) {
	if (lFrameNum<mSampleFirst || lFrameNum >= mSampleLast)
		return ' ';

	if (_isKey(lFrameNum))
		return 'K';

	uint32 lBytes, lSamples;
	int err = _read(lFrameNum, 1, NULL, 0, &lBytes, &lSamples);

	if (err != AVIERR_OK)
		return ' ';

	return lBytes ? ' ' : 'D';
}

VideoSource::eDropType VideoSourceAVI::getDropType(VDPosition lFrameNum) {
	if (lFrameNum<mSampleFirst || lFrameNum >= mSampleLast)
		return kDroppable;

	if (_isKey(lFrameNum))
		return kIndependent;

	uint32 lBytes, lSamples;
	int err = _read(lFrameNum, 1, NULL, 0, &lBytes, &lSamples);

	if (err != AVIERR_OK)
		return kDependant;

	return lBytes ? kDependant : kDroppable;
}

bool VideoSourceAVI::isDecodable(VDPosition sample_num) {
	if (sample_num<mSampleFirst || sample_num >= mSampleLast)
		return false;

	return (isKey(sample_num) || sample_num == lLastFrame+1);
}

bool VideoSourceAVI::isStreaming() {
	return pAVIStream->isStreaming();
}

bool VideoSourceAVI::isKeyframeOnly() {
   return pAVIStream->isKeyframeOnly();
}

bool VideoSourceAVI::isType1() {
   return bIsType1;
}

void VideoSourceAVI::streamBegin(bool fRealTime) {
	// must reset prevframe in any case  so that dsc renders don't pick up the last
	// preview frame
	stream_current_frame	= -1;

	if (mbDecodeStarted)
		return;

	pAVIStream->BeginStreaming(mSampleFirst, mSampleLast, fRealTime ? 1000 : 2000);

	if (mpDecompressor && !bDirectDecompress)
		mpDecompressor->Start();

	mbDecodeStarted = true;
}

const void *VideoSourceAVI::streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition frame_num) {
	if (isKey(frame_num)) {
		if (mbConcealingErrors) {
			const unsigned frame = frame_num;
			VDLogAppMessage(kVDLogWarning, kVDST_VideoSource, kVDM_ResumeFromConceal, 1, &frame);
		}
		mbConcealingErrors = false;
	}

	if (!data_len || mbConcealingErrors) return getFrameBuffer();

	if (bDirectDecompress) {
		int to_copy = getImageFormat()->biSizeImage;
		if (data_len < to_copy) {
			if (mErrorMode != kErrorModeReportAll) {
				const unsigned actual = data_len;
				const unsigned expected = to_copy;
				const unsigned frame = frame_num;
				VDLogAppMessage(kVDLogWarning, kVDST_VideoSource, kVDM_FrameTooShort, 3, &frame, &actual, &expected);
				to_copy = data_len;
			} else
				throw MyError("VideoSourceAVI: uncompressed frame %u is short (expected %d bytes, got %d)", frame_num, to_copy, data_len);
		}
		
		memcpy((void *)getFrameBuffer(), inputBuffer, to_copy);
	} else if (fUseGDI) {
		if (!hbmLame)
			throw MyError("Insufficient GDI resources to convert frame.");

		// Windows 95/98 need a DC for this.
		HDC hdc = GetDC(0);
		SetDIBits(hdc, hbmLame, 0, getDecompressedFormat()->biHeight, inputBuffer, (BITMAPINFO *)getFormat(), DIB_RGB_COLORS);
		ReleaseDC(0,hdc);
		GdiFlush();
	} else if (mpDecompressor && !bDirectDecompress) {
		// Asus ASV1 crashes with zero byte frames!!!

		if (data_len) {
			try {
				vdprotected2("using output buffer at "VDPROT_PTR"-"VDPROT_PTR, void *, lpvBuffer, void *, (char *)lpvBuffer + mFrameBufferSize - 1) {
					vdprotected2("using input buffer at "VDPROT_PTR"-"VDPROT_PTR, const void *, inputBuffer, const void *, (const char *)inputBuffer + data_len - 1) {
						vdprotected1("decompressing video frame %I64d", uint64, frame_num) {
							mpDecompressor->DecompressFrame(lpvBuffer, inputBuffer, data_len, _isKey(frame_num), is_preroll);
						}
					}
				}
			} catch(const MyError& e) {
				if (mErrorMode == kErrorModeReportAll)
					throw MyError("Error decompressing video frame %u:\n\n%s", (unsigned)frame_num, e.gets());
				else {
					const unsigned frame = frame_num;
					VDLogAppMessage(kVDLogWarning, kVDST_VideoSource, kVDM_DecodingError, 1, &frame);

					if (mErrorMode == kErrorModeConceal)
						mbConcealingErrors = true;
				}
			}
		}
	} else {
		const BITMAPINFOHEADER *bih = getImageFormat();
		long nBytesRequired = ((bih->biWidth * bih->biBitCount + 31)>>5) * 4 * bih->biHeight;
		void *tmpBuffer = 0;

		if (data_len < nBytesRequired) {
			if (mErrorMode != kErrorModeReportAll) {
				const unsigned frame = frame_num;
				const unsigned actual = data_len;
				const unsigned expected = nBytesRequired;
				VDLogAppMessage(kVDLogWarning, kVDST_VideoSource, kVDM_FrameTooShort, 3, &frame, &actual, &expected);

				tmpBuffer = malloc(nBytesRequired);
				if (!tmpBuffer)
					throw MyMemoryError();

				memcpy(tmpBuffer, inputBuffer, data_len);

				inputBuffer = tmpBuffer;
			} else
				throw MyError("VideoSourceAVI: uncompressed frame %u is short (expected %d bytes, got %d)", (unsigned)frame_num, nBytesRequired, data_len);
		}

		DecompressFrame(inputBuffer);

		free(tmpBuffer);
	}
//		memcpy(getFrameBuffer(), inputBuffer, getDecompressedFormat()->biSizeImage);

	lLastFrame = frame_num;

	return getFrameBuffer();
}

void VideoSourceAVI::streamEnd() {

	if (!mbDecodeStarted)
		return;

	mbDecodeStarted = false;

	// If an error occurs, but no one is there to hear it, was
	// there ever really an error?

	if (mpDecompressor && !bDirectDecompress)
		mpDecompressor->Stop();

	pAVIStream->EndStreaming();

}

const void *VideoSourceAVI::getFrame(VDPosition lFrameDesired) {
	VDPosition lFrameKey, lFrameNum;
	int aviErr;

	// illegal frame number?

	if (lFrameDesired < mSampleFirst || lFrameDesired >= mSampleLast)
		throw MyError("VideoSourceAVI: bad frame # (%d not within [%u, %u])", lFrameDesired, (unsigned)mSampleFirst, (unsigned)(mSampleLast-1));

	// do we already have this frame?

	if (lLastFrame == lFrameDesired)
		return getFrameBuffer();

	// back us off to the last key frame if we need to

	lFrameNum = lFrameKey = nearestKey(lFrameDesired);

	_RPT1(0,"Nearest key frame: %ld\n", lFrameKey);

	if (lLastFrame > lFrameKey && lLastFrame < lFrameDesired)
		lFrameNum = lLastFrame+1;

	if (mpDecompressor)
		mpDecompressor->Start();

	lLastFrame = -1;		// In case we encounter an exception.

	vdblock<char>	dataBuffer;
	do {
		uint32 lBytesRead, lSamplesRead;

		for(;;) {
			if (dataBuffer.empty())
				aviErr = AVIERR_BUFFERTOOSMALL;
			else
				aviErr = read(lFrameNum, 1, dataBuffer.data(), dataBuffer.size(), &lBytesRead, &lSamplesRead);

			if (aviErr == AVIERR_BUFFERTOOSMALL) {
				aviErr = read(lFrameNum, 1, NULL, 0, &lBytesRead, &lSamplesRead);

				if (aviErr)
					throw MyAVIError("VideoSourceAVI", aviErr);

				uint32 newSize = (lBytesRead+65535) & -65535;
				if (!newSize)
					++newSize;

				dataBuffer.resize(newSize);
			} else if (aviErr) {
				throw MyAVIError("VideoSourceAVI", aviErr);
			} else
				break;
		};

		if (!lBytesRead)
			continue;

		streamGetFrame(dataBuffer.data(), lBytesRead, false, lFrameNum);

	} while(++lFrameNum <= lFrameDesired);

	lLastFrame = lFrameDesired;

	return getFrameBuffer();
}

void VideoSourceAVI::setDecodeErrorMode(ErrorMode mode) {
	mErrorMode = mode;
}

bool VideoSourceAVI::isDecodeErrorModeSupported(ErrorMode mode) {
	return true;
}
