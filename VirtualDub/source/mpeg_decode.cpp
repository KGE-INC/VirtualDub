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
//
///////////////////////////////////////////////////////////////////////////
//
//                                 WARNING
//
// This code is heavily based off of the Java MPEG video player written by
// Joerg Anders.  Because his code was released under the GNU GPL v2, this
// means VirtualDub must also be released under GNU GPL v2 when MPEG
// support is included.
//
// (Like that's any different.)
//
// This code is really nasty...
//
///////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <process.h>
#include <new>

#include <windows.h>

#include "CMemoryBitInput.h"
#include <vd2/system/error.h>

#include <vd2/system/cpuaccel.h>
#include "mpeg_idct.h"
#include "mpeg_tables.h"
#include "mpeg_decode.h"

//////////////////////////////////////////////////////////////

#ifdef _DEBUG
//#define STATISTICS
#endif

//#define MB_STATS
//#define MB_SPLIT_STATS

//#define TIME_TRIALS

//////////////////////////////////////////////////////////////

extern "C" void __cdecl IDCT_mmx(signed short *dct_coeff, void *dst, long pitch, int intra_flag, int pos);
extern "C" void __cdecl IDCT_isse(signed short *dct_coeff, void *dst, long pitch, int intra_flag, int pos);

//////////////////////////////////////////////////////////////

#define VIDPKT_TYPE_PICTURE_START		(0x00)
#define VIDPKT_TYPE_SLICE_START_MIN		(0x01)
#define	VIDPKT_TYPE_SLICE_START_MAX		(0xaf)

#define MIDVAL (262144)
#define ROUNDVAL (1024)

#ifdef _DEBUG
class BranchPredictor {
private:
	int v[16];
	int ls;
	int taken;
	int mispredict;
	int total;
	const char *s;

public:
	BranchPredictor(const char *_s);
	~BranchPredictor();

	bool predict(bool b);
};

BranchPredictor::BranchPredictor(const char *_s) {
	memset(v, 0, sizeof v);
	ls = 0;
	taken = 0;
	mispredict = 0;
	total = 0;
	s = _s;
}

BranchPredictor::~BranchPredictor() {
	char buf[256];

	sprintf(buf, "Branch predictor \"%s\": %d branches (%d%% taken), %d mispredicts (%d%%)\n"
			,s
			,total
			,MulDiv(taken, 100, total)
			,mispredict
			,MulDiv(mispredict, 100, total));
	_RPT0(0,buf);
}

bool BranchPredictor::predict(bool b) {

	if (b) {
		if (v[ls]<2)
			++mispredict;
		if (v[ls]<3)
			++v[ls];
		++taken;
	} else {
		if (v[ls]>=2)
			++mispredict;
		if (v[ls])
			--v[ls];
	}

	ls = ((ls<<1)|(int)b) & 15;
	++total;

	return b;
}

BranchPredictor g_predict080(">=080 (90%)");
BranchPredictor g_predict600("<600 (40%)");
BranchPredictor g_predictC00("C00-FFF (20%)");
BranchPredictor g_predict800("800-BFF");
BranchPredictor g_predict040("040");
BranchPredictor g_predict020("020");

#define PREDICT(ths, v) (g_predict##ths.predict(v))
#else
#define PREDICT(ths, v) (v)
#endif


//////////////////////////////////////////////////////////////

typedef unsigned char YUVPixel;

static void video_process_picture_start_packet(char *ptr);

struct MPEGRGBDecodeWorkspace {
	long	rgb_pitch;
	long	y_pitch;
	long	uv_pitch;
	long	w;
	long	h;
	long	pad_[3];
	__int64 constants[28];
};


static void YUVToRGB32(YUVPixel *Y_ptr, YUVPixel *U_ptr, YUVPixel *V_ptr, unsigned char *dst, long bpr, long w, long h);
static void YUVToRGB24(YUVPixel *Y_ptr, YUVPixel *U_ptr, YUVPixel *V_ptr, unsigned char *dst, long bpr, long w, long h, MPEGRGBDecodeWorkspace *workspace);
static void YUVToRGB16(YUVPixel *Y_ptr, YUVPixel *U_ptr, YUVPixel *V_ptr, unsigned char *dst, long bpr, long w, long h);
static void YUVToUYVY16(YUVPixel *Y_ptr, YUVPixel *U_ptr, YUVPixel *V_ptr, unsigned char *dst, long bpr, long w, long h);
static void YUVToYUY216(YUVPixel *Y_ptr, YUVPixel *U_ptr, YUVPixel *V_ptr, unsigned char *dst, long bpr, long w, long h);
static void YUVToYUV12(YUVPixel *Y_ptr, YUVPixel *U_ptr, YUVPixel *V_ptr, unsigned char *dst, long bpr, long w, long h);

static void video_copy_forward(int x_pos, int y_pos);
static void video_copy_forward_prediction(int x_pos, int y_pos, bool);
static void video_copy_backward_prediction(int x_pos, int y_pos, bool);
static void video_add_backward_prediction(int x_pos, int y_pos, bool);

///////////////////////////////////////////////////////////////////////////////

static const int zigzag[] = {		// the reverse zigzag scan order
	 0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63,
};

static const int zigzag_MMX[] = {				// the reverse zigzag scan order, interleaved for the MMX/ISSE IDCT
	 0,  2,  8, 16, 10,  4,  6, 12, 
	18, 24, 32, 26, 20, 14,  1,  3, 
	 9, 22, 28, 34, 40, 48, 42, 36, 
	30, 17, 11,  5,  7, 13, 19, 25, 
	38, 44, 50, 56, 58, 52, 46, 33, 
	27, 21, 15, 23, 29, 35, 41, 54, 
	60, 62, 49, 43, 37, 31, 39, 45, 
	51, 57, 59, 53, 47, 55, 61, 63, 
};

static const int intramatrix_default[64] = {		// the default intramatrix
	 8, 16, 19, 22, 26, 27, 29, 34,
	16, 16, 22, 24, 27, 29, 34, 37,
	19, 22, 26, 27, 29, 34, 34, 38,
	22, 22, 26, 27, 29, 34, 37, 40,
	22, 26, 27, 29, 32, 35, 40, 48,
	26, 27, 29, 32, 35, 40, 48, 58, 
	26, 27, 29, 34, 38, 46, 56, 69,
	27, 29, 35, 38, 46, 56, 69, 83};

////////////////////////////////////////////////////////////////////////////////

struct MPEGDecodeEngine {
	CMemoryBitInput		mBits;

	__declspec(align(16)) signed short mDCTCoeff[64];

	void (MPEGDecodeEngine::*mpDecodeBlock)(YUVPixel *dst, long modulo, long DC_val);

	virtual void DecodeBlock(YUVPixel *dst, long modulo, long DC_val) = 0;

	MPEGDecodeEngine() {
		mpDecodeBlock	= DecodeBlock;
	}

	void decode_mblock_Y(YUVPixel *dst);
	void decode_mblock_UV(YUVPixel *dst, int& dc_ref);
	void video_process_picture_slice_I(char *ptr, int type);
	inline int mpeg_get_motion_component();
	void video_process_picture_slice_P(char *ptr, int type);
	void video_process_picture_slice_B(char *ptr, int type);
	void video_process_picture_slice(char *ptr, int type);
};

struct MPEGDecodeEngineScalar : public MPEGDecodeEngine {
	void DecodeBlock(YUVPixel *dst, long modulo, long DC_val);
};

struct MPEGDecodeEngineMMX : public MPEGDecodeEngine {
	void (__cdecl *mpIDCTMMX)(signed short *dct_coeff, void *dst, long pitch, int intra_flag, int pos);

	MPEGDecodeEngineMMX() {
		memset(mDCTCoeff, 0, sizeof mDCTCoeff);

		mpIDCTMMX		= ISSE_enabled ? IDCT_isse : IDCT_mmx;
	}

	void DecodeBlock(YUVPixel *dst, long modulo, long DC_val);
};

struct MPEGDecodeEngineSSE2 : public MPEGDecodeEngineMMX {
	__declspec(align(16)) MPEGRGBDecodeWorkspace	mRGBDecodeWorkspace;

	MPEGDecodeEngineSSE2() {
		static const __int64 decode_constants[28]={
			0x00080008000800080, 0x00080008000800080,
			0x00081008100810081, 0x00081008100810081,
			0x00066006600660066, 0x00066006600660066,
			0x0FFE7FFE7FFE7FFE7, 0x0FFE7FFE7FFE7FFE7,
			0x0FFCCFFCCFFCCFFCC, 0x0FFCCFFCCFFCCFFCC,
			0x000FF00FF00FF00FF, 0x000FF00FF00FF00FF,
			0x00010001000100010, 0x00010001000100010,
			0x0004A004A004A004A, 0x0004A004A004A004A,
			0x000810000FFE70081, 0x0FFE700810000FFE7,
			0x00000FFE700810000, 0x000810000FFE70081,
			0x0FFE700810000FFE7, 0x00000FFE700810000,
			0x000000066FFCC0000, 0x0FFCC00000066FFCC,
			0x00066FFCC00000066, 0x000000066FFCC0000,
			0x0FFCC00000066FFCC, 0x00066FFCC00000066,
		};

		memcpy(mRGBDecodeWorkspace.constants, decode_constants, sizeof mRGBDecodeWorkspace);
	}
};

////////////////////////////////////////////////////////////////////////////////


static char *memblock = NULL;

static int intramatrix0[64];
static int nonintramatrix0[64];

static int intramatrices[32][64];
static int nonintramatrices[32][64];

static struct MPEGBuffer {
	MPEGDecodeEngine	*dec;
	YUVPixel *Y;
	YUVPixel *U;
	YUVPixel *V;
	int frame_num;
} buffers[3];

#define I_FRAME		(0x1)
#define P_FRAME		(0x2)
#define B_FRAME		(0x3)
#define D_FRAME		(0x4)

static int frame_type;

static YUVPixel *Y_back, *Y_forw, *Y_dest;
static YUVPixel *U_back, *U_forw, *U_dest;
static YUVPixel *V_back, *V_forw, *V_dest;

static long pelWidth, pelHeight, mbWidth, mbHeight;

static BOOL reset_flag;

static long y_pitch, uv_pitch;
static long y_modulo, uv_modulo;

extern "C" const unsigned long YUV_Y_table[], YUV_U_table[], YUV_V_table[];
extern "C" const unsigned char YUV_clip_table[];

#ifdef STATISTICS
struct {
	int coded_block_pattern;
} stats;
#endif

#ifdef TIME_TRIALS
struct {
	int counts[4];
	__int64 cycles[4];
	__int64 totalcycles;
	int totalframes;
} timetrials;
#endif

static int vector_limit_x;
static int vector_limit_y;

static enum {
	MPEG_NOT_READY,
		MPEG_READY_SCALAR,
		MPEG_READY_MMX
} mpeg_ready_state;

extern "C" void video_copy_prediction_Y_ISSE(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
extern "C" void video_copy_prediction_C_ISSE(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
extern "C" void video_add_prediction_Y_ISSE(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
extern "C" void video_add_prediction_C_ISSE(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
extern "C" void video_copy_prediction_Y_scalar(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
extern "C" void video_copy_prediction_C_scalar(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
extern "C" void video_add_prediction_Y_scalar(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
extern "C" void video_add_prediction_C_scalar(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
extern "C" void video_copy_prediction_Y_MMX(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
extern "C" void video_copy_prediction_C_MMX(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
extern "C" void video_add_prediction_Y_MMX(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
extern "C" void video_add_prediction_C_MMX(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
extern "C" void video_copy_prediction_Y_SSE2(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
extern "C" void video_add_prediction_Y_SSE2(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);

static void (*video_copy_prediction_Y)(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
static void (*video_copy_prediction_C)(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
static void (*video_add_prediction_Y)(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);
static void (*video_add_prediction_C)(YUVPixel *src, YUVPixel *dst, int vx, int vy, long pitch);

////////////////////////////////////////////////////////////////////////////////

void mpeg_deinitialize() {
//	if (memblock) { freemem(memblock); memblock = NULL; }
	if (memblock) { VirtualFree(memblock, 0, MEM_RELEASE); memblock = NULL; }
}

namespace {
	template<int x, int y> struct const_max2 { enum { value = x>y?x:y }; };
	template<int x, int y, int z> struct const_max3 { enum { value = const_max2<const_max2<x,y>::value,z>::value }; };
};

void mpeg_initialize(int width, int height, char *imatrix, char *nimatrix, BOOL fullpel_only) {
	mpeg_deinitialize();

	try {
		int i;

		pelWidth		= width;
		pelHeight		= height;
		mbWidth			= (width+15)/16;
		mbHeight		= (height+15)/16;

		unsigned mbWidthEven = (mbWidth + 1) & ~1;

		y_pitch			= mbWidthEven * 24;
		uv_pitch		= mbWidthEven * 48;
		y_modulo		= 16*y_pitch - 16*mbWidth;
		uv_modulo		= 8*uv_pitch - 8*mbWidth;

		vector_limit_x	= (mbWidth-1) * 32;
		vector_limit_y	= (mbHeight-1) * 32;

//		if (!(memblock = (char *)allocmem(32 + mbWidth * mbHeight * (3*256*sizeof(YUVPixel) + 6*64*sizeof(YUVPixel)) + (uv_pitch+8)*8)))
//			throw MyMemoryError();

		enum {
			kDecodeStructSize = const_max3<sizeof(MPEGDecodeEngineScalar), sizeof(MPEGDecodeEngineMMX), sizeof(MPEGDecodeEngineSSE2)>::value,
			kDecodeStructPad = (kDecodeStructSize + 127) & ~127
		};

		int buffer_size	= (64 * mbWidthEven * mbHeight)*6 + kDecodeStructPad;

		if (!(memblock = (char *)VirtualAlloc(NULL, 3*buffer_size, MEM_COMMIT, PAGE_READWRITE)))
			throw MyMemoryError();

		// We organize buffers as follows (320x240):
		//
		// repeat 160 {
		//		Y1 [0...319]		320 bytes
		//		U  [0...159]		160 bytes
		//		Y2 [0...319]		320 bytes
		//		V  [0...159]		160 bytes
		// }

		buffers[0].dec	= (MPEGDecodeEngine *)memblock;
		buffers[0].Y	= (YUVPixel *)((char *)buffers[0].dec + kDecodeStructPad);
		buffers[0].U	= (YUVPixel *)((char *)buffers[0].Y + mbWidthEven * 16 * sizeof(YUVPixel));
		buffers[0].V	= (YUVPixel *)((char *)buffers[0].Y + mbWidthEven * 40 * sizeof(YUVPixel));

		buffers[1].dec	= (MPEGDecodeEngine *)((char *)buffers[0].Y + mbWidthEven * mbHeight * 384 * sizeof(YUVPixel));
		buffers[1].Y	= (YUVPixel *)((char *)buffers[1].dec + kDecodeStructPad);
		buffers[1].U	= (YUVPixel *)((char *)buffers[1].Y + mbWidthEven * 16 * sizeof(YUVPixel));
		buffers[1].V	= (YUVPixel *)((char *)buffers[1].Y + mbWidthEven * 40 * sizeof(YUVPixel));

		buffers[2].dec	= (MPEGDecodeEngine *)((char *)buffers[1].Y + mbWidthEven * mbHeight * 384 * sizeof(YUVPixel));
		buffers[2].Y	= (YUVPixel *)((char *)buffers[2].dec + kDecodeStructPad);
		buffers[2].U	= (YUVPixel *)((char *)buffers[2].Y + mbWidthEven * 16 * sizeof(YUVPixel));
		buffers[2].V	= (YUVPixel *)((char *)buffers[2].Y + mbWidthEven * 40 * sizeof(YUVPixel));

		buffers[0].frame_num = buffers[1].frame_num = buffers[2].frame_num = -1;

		if (imatrix)
			for(i=0; i<64; i++) intramatrix0[zigzag[i]] = (unsigned char)imatrix[i];
		else
			memcpy(intramatrix0, intramatrix_default, 64*sizeof(int));

		if (nimatrix)
			for(i=0; i<64; i++) nonintramatrix0[zigzag[i]] = (unsigned char)nimatrix[i];
		else
			for(i=0; i<64; i++) nonintramatrix0[i] = 16;

		IDCT_init();

		mpeg_reset();

	} catch(const MyError&) {
		mpeg_deinitialize();
		throw;
	}
}

void mpeg_reset() {

//	for(int i=0; i<64; i++)
//		_RPT2(0,"%d: %d\n", i, lpos_stats[i]);

	reset_flag = TRUE;
	mpeg_ready_state = MPEG_NOT_READY;
}

void mpeg_convert_frame32(void *output_buffer, int buffer_ID) {
//	_RPT1(0,"MPEG: converting frame buffer %d\b", buffer_ID);


#ifdef _DEBUG
	if (buffer_ID == -1) throw MyError("Invalid source buffer in "__FILE__", line %d",__LINE__);
#endif

//	memset(buffers[buffer_ID].Y, 0x80, y_pitch*mbHeight*16);

	YUVToRGB32(buffers[buffer_ID].Y, buffers[buffer_ID].U, buffers[buffer_ID].V, (unsigned char *)output_buffer,
			(mbWidth*16)*4, (pelWidth+1)>>1, (pelHeight+1)>>1);
}

void mpeg_convert_frame24(void *output_buffer, int buffer_ID) {
//	_RPT1(0,"MPEG: converting frame buffer %d\b", buffer_ID);


#ifdef _DEBUG
	if (buffer_ID == -1) throw MyError("Invalid source buffer in "__FILE__", line %d",__LINE__);
#endif

//	memset(buffers[buffer_ID].Y, 0x80, y_pitch*mbHeight*16);

	new(buffers[buffer_ID].dec) MPEGDecodeEngineSSE2;
	MPEGDecodeEngineSSE2 *pDecoder = (MPEGDecodeEngineSSE2 *)buffers[buffer_ID].dec;

	YUVToRGB24(buffers[buffer_ID].Y, buffers[buffer_ID].U, buffers[buffer_ID].V, (unsigned char *)output_buffer,
			(mbWidth*16)*3, ((pelWidth+7)&-8)>>1, (pelHeight+1)>>1, &pDecoder->mRGBDecodeWorkspace);
}

void mpeg_convert_frame16(void *output_buffer, int buffer_ID) {
//	_RPT1(0,"MPEG: converting frame buffer %d\b", buffer_ID);


#ifdef _DEBUG
	if (buffer_ID == -1) throw MyError("Invalid source buffer in "__FILE__", line %d",__LINE__);
#endif

//	memset(buffers[buffer_ID].Y, 0x80, y_pitch*mbHeight*16);

	YUVToRGB16(buffers[buffer_ID].Y, buffers[buffer_ID].U, buffers[buffer_ID].V, (unsigned char *)output_buffer,
			(mbWidth*16)*2, ((pelWidth+7)&-8)>>1, (pelHeight+1)>>1);
}

void mpeg_convert_frameUYVY16(void *output_buffer, int buffer_ID) {
//	_RPT1(0,"MPEG: converting frame buffer %d\b", buffer_ID);

#ifdef _DEBUG
	if (buffer_ID == -1) throw MyError("Invalid source buffer in "__FILE__", line %d",__LINE__);
#endif

	YUVToUYVY16(buffers[buffer_ID].Y, buffers[buffer_ID].U, buffers[buffer_ID].V, (unsigned char *)output_buffer,
			(mbWidth*16)*2, (pelWidth+1)>>1, (pelHeight+1)>>1);
}

void mpeg_convert_frameYUY216(void *output_buffer, int buffer_ID) {
//	_RPT1(0,"MPEG: converting frame buffer %d\b", buffer_ID);

#ifdef _DEBUG
	if (buffer_ID == -1) throw MyError("Invalid source buffer in "__FILE__", line %d",__LINE__);
#endif

	YUVToYUY216(buffers[buffer_ID].Y, buffers[buffer_ID].U, buffers[buffer_ID].V, (unsigned char *)output_buffer,
			(mbWidth*16)*2, (pelWidth+1)>>1, (pelHeight+1)>>1);
}

static MPEGDecodeEngine *g_pDecoder;

void mpeg_decode_frame(void *input_data, int len, int frame_num) {
	char *ptr = (char *)input_data;
	char *limit = ptr + len - 4;
	int type;

	if (SSE2_enabled) {
		video_copy_prediction_Y = video_copy_prediction_Y_SSE2;
		video_add_prediction_Y = video_add_prediction_Y_SSE2;
		video_copy_prediction_C = video_copy_prediction_C_ISSE;
		video_add_prediction_C = video_add_prediction_C_ISSE;
	} else if (ISSE_enabled) {
		video_copy_prediction_Y = video_copy_prediction_Y_ISSE;
		video_add_prediction_Y = video_add_prediction_Y_ISSE;
		video_copy_prediction_C = video_copy_prediction_C_ISSE;
		video_add_prediction_C = video_add_prediction_C_ISSE;
	} else if (MMX_enabled) {
		video_copy_prediction_Y = video_copy_prediction_Y_MMX;
		video_add_prediction_Y = video_add_prediction_Y_MMX;
		video_copy_prediction_C = video_copy_prediction_C_MMX;
		video_add_prediction_C = video_add_prediction_C_MMX;
	} else {
		video_copy_prediction_Y = video_copy_prediction_Y_scalar;
		video_add_prediction_Y = video_add_prediction_Y_scalar;
		video_copy_prediction_C = video_copy_prediction_C_scalar;
		video_add_prediction_C = video_add_prediction_C_scalar;
	}

	if (MMX_enabled && mpeg_ready_state != MPEG_READY_MMX) {
		int i,j;

		for(j=0; j<32; j++) {
			for(i=0; i<64; i+=8) {
				nonintramatrices[j][i+0] = nonintramatrix0[i+0] * j;
				intramatrices   [j][i+0] = intramatrix0   [i+0] * j;
				nonintramatrices[j][i+2] = nonintramatrix0[i+1] * j;
				intramatrices   [j][i+2] = intramatrix0   [i+1] * j;
				nonintramatrices[j][i+4] = nonintramatrix0[i+2] * j;
				intramatrices   [j][i+4] = intramatrix0   [i+2] * j;
				nonintramatrices[j][i+6] = nonintramatrix0[i+3] * j;
				intramatrices   [j][i+6] = intramatrix0   [i+3] * j;
				nonintramatrices[j][i+1] = nonintramatrix0[i+4] * j;
				intramatrices   [j][i+1] = intramatrix0   [i+4] * j;
				nonintramatrices[j][i+3] = nonintramatrix0[i+5] * j;
				intramatrices   [j][i+3] = intramatrix0   [i+5] * j;
				nonintramatrices[j][i+5] = nonintramatrix0[i+6] * j;
				intramatrices   [j][i+5] = intramatrix0   [i+6] * j;
				nonintramatrices[j][i+7] = nonintramatrix0[i+7] * j;
				intramatrices   [j][i+7] = intramatrix0   [i+7] * j;
			}
		}
		
		mpeg_ready_state = MPEG_READY_MMX;

		
	} else if (!MMX_enabled && mpeg_ready_state != MPEG_READY_SCALAR) {
		int i,j;

		for(j=0; j<32; j++) {
			for(i=0;i<64;i++) {
				nonintramatrices[j][i] = nonintramatrix0[i]*j;
				intramatrices[j][i] = intramatrix0[i]*j;
			}

			IDCT_norm(intramatrices[j]);
			IDCT_norm(nonintramatrices[j]);
		}

		mpeg_ready_state = MPEG_READY_SCALAR;
	}

#ifdef STATISTICS
	memset(&stats, 0, sizeof stats);
#endif

#ifdef TIME_TRIALS
	__int64 time_start, time_end;

	__asm {
		rdtsc
		mov dword ptr time_start+0,eax
		mov dword ptr time_start+4,edx
	};

#endif



	frame_type = -1;

	ptr[len-4] = 0;
	ptr[len-3] = 0;
	ptr[len-2] = 1;
	ptr[len-1] = (char)0xff;

	while(ptr < limit) {
		do {
			if (ptr>limit) goto advance;
			while(*ptr++) if (ptr>limit) goto advance;
		} while(ptr[0] != 0 || ptr[1] != 1);

		type = ptr[2];
		ptr += 3;

//		_RPT1(0,"Packet type %02x\n", type);

		switch(type) {
		case VIDPKT_TYPE_PICTURE_START:
			video_process_picture_start_packet(ptr);
			break;
		default:
			if (type >= VIDPKT_TYPE_SLICE_START_MIN && type <= VIDPKT_TYPE_SLICE_START_MAX)
				g_pDecoder->video_process_picture_slice(ptr, type);
		}
	}

	if (MMX_enabled)
		__asm emms

#ifdef TIME_TRIALS
	__asm {
		rdtsc
		mov dword ptr time_end+0,eax
		mov dword ptr time_end+4,edx
	};

	++timetrials.counts[frame_type-1];
	timetrials.cycles[frame_type-1] += (time_end - time_start);
	timetrials.totalcycles += (time_end - time_start);

	if (!(++timetrials.totalframes & 63)) {
		static char buf[256];

		wsprintf(buf, "%d I-frames (%d, %d%%), %d P-frames (%d, %d%%), %d B-frames (%d, %d%%)\n"
			,timetrials.counts[0]
			,timetrials.counts[0] ? (int)(timetrials.cycles[0] / timetrials.counts[0]) : 0
			,timetrials.counts[0] ? (int)((timetrials.cycles[0]*100)/timetrials.totalcycles) : 0
			,timetrials.counts[1]
			,timetrials.counts[1] ? (int)(timetrials.cycles[1] / timetrials.counts[1]) : 0
			,timetrials.counts[1] ? (int)((timetrials.cycles[1]*100)/timetrials.totalcycles) : 0
			,timetrials.counts[2]
			,timetrials.counts[2] ? (int)(timetrials.cycles[2] / timetrials.counts[2]) : 0
			,timetrials.counts[2] ? (int)((timetrials.cycles[2]*100)/timetrials.totalcycles) : 0);
		OutputDebugString(buf);
	}

#endif


advance:

	switch(frame_type) {
	case I_FRAME:
		mpeg_swap_buffers(MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);

		buffers[MPEG_BUFFER_FORWARD].frame_num = frame_num;

		reset_flag = FALSE;

		break;
	case P_FRAME:
		mpeg_swap_buffers(MPEG_BUFFER_FORWARD, MPEG_BUFFER_BACKWARD);

		buffers[MPEG_BUFFER_FORWARD].frame_num = frame_num;
		reset_flag = FALSE;
		break;

	case B_FRAME:
		buffers[MPEG_BUFFER_BIDIRECTIONAL].frame_num = frame_num;
		break;

#ifdef _DEBUG
	default:
		throw MyError("Invalid frame type");
#endif
	};


#ifdef STATISTICS
	_RPT2(0,"--- Frame #%d statistics (%c-frame)\n", frame_num, " IPBD567"[frame_type]);
	_RPT2(0,"\tCoded block pattern: %7d/%d macroblocks\n", stats.coded_block_pattern, mbWidth*mbHeight);
	_RPT0(0,"\n");
#endif
}

void mpeg_swap_buffers(int buffer1, int buffer2) {
	MPEGBuffer b;

	b = buffers[buffer1];
	buffers[buffer1] = buffers[buffer2];
	buffers[buffer2] = b;
}

int mpeg_lookup_frame(int frame) {
//	_RPT4(0,"Looking for %ld (%ld/%ld/%ld)\n", frame, buffers[0].frame_num, buffers[1].frame_num, buffers[2].frame_num);

	for(int i=0; i<(sizeof buffers/sizeof buffers[0]); i++)
		if (buffers[i].frame_num == frame)
			return i;

	return -1;
}

////////////////////////////////////////////////////////////////////////////////////////

static int *intramatrix;
static int *nonintramatrix;

static int forw_vector_full_pel, forw_vector_bits;
static int back_vector_full_pel, back_vector_bits;

static int forw_vector_mask, back_vector_mask;
static int forw_vector_extend, back_vector_extend;

static int forw_vector_x, forw_vector_y;
static int back_vector_x, back_vector_y;

/////////////////////////////

static void mpeg_set_destination_buffer(int id) {
	g_pDecoder	= buffers[id].dec;
	Y_dest = buffers[id].Y;
	U_dest = buffers[id].U;
	V_dest = buffers[id].V;
}

static void video_process_picture_start_packet(char *ptr) {
	CMemoryBitInput bits(ptr);
	long temp_rf = bits.get(10);

	frame_type = bits.get(3);

//	_RPT2(0,"Processing %c-frame (#%d)\n", " IPBD567"[frame_type], temp_rf);

	switch(frame_type) {
	case I_FRAME:		// I-frames have no prediction
		mpeg_set_destination_buffer(MPEG_BUFFER_BACKWARD);
//		_RPT1(0,"Processing I-frame (#%d)\n", temp_rf);
		break;

	case P_FRAME:		// P-frames predict back to the last I or P
		mpeg_set_destination_buffer(MPEG_BUFFER_BACKWARD);
//		_RPT2(0,"Processing P-frame (#%d) (forward: %ld)\n", temp_rf, buffers[MPEG_BUFFER_FORWARD].frame_num);

		Y_forw = buffers[MPEG_BUFFER_FORWARD].Y;
		U_forw = buffers[MPEG_BUFFER_FORWARD].U;
		V_forw = buffers[MPEG_BUFFER_FORWARD].V;
		break;

	case B_FRAME:		// B-frames predict back to the last I or P and forward to the next P
		mpeg_set_destination_buffer(MPEG_BUFFER_BIDIRECTIONAL);
//		_RPT3(0,"Processing B-frame (#%d) (f: %ld  b: %ld)\n", temp_rf, buffers[MPEG_BUFFER_BACKWARD].frame_num, buffers[MPEG_BUFFER_FORWARD].frame_num);
		Y_back = buffers[MPEG_BUFFER_FORWARD].Y;
		U_back = buffers[MPEG_BUFFER_FORWARD].U;
		V_back = buffers[MPEG_BUFFER_FORWARD].V;
		if (reset_flag) {
			Y_forw = Y_back;
			U_forw = U_back;
			V_forw = V_back;
		} else {
			Y_forw = buffers[MPEG_BUFFER_BACKWARD].Y;
			U_forw = buffers[MPEG_BUFFER_BACKWARD].U;
			V_forw = buffers[MPEG_BUFFER_BACKWARD].V;
		}
		break;

	case D_FRAME:
		throw MyError("D-type frames not supported");

	default:
		throw MyError("Unknown frame type 0x%d", frame_type);
	}

	if (MMX_enabled)
		new(g_pDecoder) MPEGDecodeEngineMMX;
	else
		new(g_pDecoder) MPEGDecodeEngineScalar;

	bits.get(16);	// VBV_delay
	if (frame_type == P_FRAME || frame_type == B_FRAME) {
		forw_vector_full_pel	= bits.get();
		forw_vector_bits		= bits.get(3)-1;
		forw_vector_mask		= (32<<forw_vector_bits)-1;
		forw_vector_extend		= ~((16<<forw_vector_bits)-1);
	}

	if (frame_type == B_FRAME) {
		back_vector_full_pel	= bits.get();
		back_vector_bits		= bits.get(3)-1;
		back_vector_mask		= (32<<back_vector_bits)-1;
		back_vector_extend		= ~((16<<back_vector_bits)-1);
	}
}

////////////////////////////////////////////////////

static int dct_dc_y_past, dct_dc_u_past, dct_dc_v_past;

extern int dct_coeff[64];

#define MBF_NEW_QUANT		(16)
#define MBF_FORWARD			(8)
#define MBF_BACKWARD		(4)
#define MBF_PATTERN			(2)
#define MBF_INTRA			(1)

static int macro_block_flags;

static YUVPixel *dstY, *dstU, *dstV;

//////////////////////

void MPEGDecodeEngineScalar::DecodeBlock(YUVPixel *dst, long modulo, long DC_val) {
#ifdef MB_STATS

#ifdef MB_SPLIT_STATS
#define MB_DECLARE_STAT(x)		static int x##_intra=0, x##_inter=0;
#define MB_STAT_INC(x)			(macro_block_flags & MBF_INTRA ? (++x##_intra) : (++x##_inter))
#else
#define MB_DECLARE_STAT(x)		static int x=0;
#define MB_STAT_INC(x)			(++x)
#endif

MB_DECLARE_STAT(st_level1_idx0);
MB_DECLARE_STAT(st_level1_idx1);
MB_DECLARE_STAT(st_exit);
MB_DECLARE_STAT(st_short);
MB_DECLARE_STAT(st_long);
MB_DECLARE_STAT(st_vshort);
MB_DECLARE_STAT(st_escape);
MB_DECLARE_STAT(st_first_short);
MB_DECLARE_STAT(st_first_long);

#else
#define MB_STAT_INC(x)
#endif

	const int *idx=zigzag;
	int level;
	int pos=0;
	int coeff_count = 0;
	const int *quant_matrix = intramatrix;
	long v;
	int sign = 0;

	dct_coeff[0] = DC_val;

	v = mBits.peek(12);
	if (!(macro_block_flags & MBF_INTRA)) {
		quant_matrix = nonintramatrix;
		sign = 1;

		if (v < 0x800) {
			idx = zigzag-1;
			dct_coeff[0]=0;

			MB_STAT_INC(st_first_long);
		} else {

			if (v & 0x400)
				dct_coeff[0] = (-3*quant_matrix[0] + 128) >> 8;
			else
				dct_coeff[0] = (3*quant_matrix[0] + 128) >> 8;

			mBits.skip(2);
			v = mBits.peek(12);

			MB_STAT_INC(st_first_short);
		}
	}

	// macroblock statistics from nuku.mpg:
	//
	//	2883584 mblocks
	//		7142656 (42%)		very short
	//		3457222 (20%)		level 1, idx_run = 0
	//		1604394 ( 9%)		level 1, idx_run = 1
	//		 937613 ( 5%)		long
	//		 451518 ( 2%)		short
	//		 161300 ( 0%)		escape
	//		1457101 (50%)		first long
	//		 705505 (24%)		first short

	// nuku1.mpg:
	//	47710208 mblocks
	//		120471300 (43%)		very short
	//		 52059388 (18%)		level 1, idx_run = 0
	//		 47710208 (17%)		exits
	//		 27499741 ( 9%)		level 1, idx_run = 1
	//		 18368812 ( 6%)		long
	//		  7911139 ( 2%)		short
	//		  4948686 ( 1%)		escape
	//		 29281756 (61%)		first long
	//		 14165596 (29%)		first short

	for(;;v = mBits.peek(12)) {
		int level_sign = 0;

		if (v >= 0x080) {				// 080-FFF		(90%)
			if (v < 0x600) {			// 080-5FF very short (40%)
				int t = (v>>4);
				int bcnt;

				bcnt = mpeg_dct_coeff_decode0[t*4+2-32];
				idx += mpeg_dct_coeff_decode0[t*4+0-32];
				level = mpeg_dct_coeff_decode0[t*4+1-32];

				_ASSERT(level != 0);

				mBits.skip(bcnt);

				if (v & mpeg_dct_coeff_decode0[t*4+3-32])
					level_sign = -1;

				MB_STAT_INC(st_vshort);

			} else if (v >= 0x0c00) {	// C00-FFF level1-idx0 (20%)
				mBits.skip(3);

				++idx;
				level = 1;

				if (v & 0x200)
					level_sign = -1;

				MB_STAT_INC(st_level1_idx0);

			} else if (v >= 0x800) {	// 800-BFF
				mBits.skip(2);

				MB_STAT_INC(st_exit);

				break;
			} else {					// 600-7FF
				mBits.skip(4);
				idx += 2;

				level = 1;
				if (v & 0x100)
					level_sign = -1;

				MB_STAT_INC(st_level1_idx1);

			}
		} else {
			if (v >= 0x040) {
				mBits.skip(6);
				idx += mBits.get(6)+1;
				level = mBits.get_signed(8);

//				_ASSERT(level != 0);

				if (!(level & 0x7f)) {
					level <<= 1;
					level |= mBits.get(8);
				}

				if (level<0) {
					level = -level;
					level_sign = -1;
				}

				if (!level)
					goto conceal_error;

				MB_STAT_INC(st_escape);

			} else if (v >= 0x020) {
				int t = (v>>2);

				idx += mpeg_dct_coeff_decode1[t*2+0-16]+1;
				level = mpeg_dct_coeff_decode1[t*2+1-16];

				_ASSERT(level != 0);

				MB_STAT_INC(st_short);

				mBits.skip(11);
				if (v & 2)
					level_sign = -1;
			} else {
				int t, bcnt;

				MB_STAT_INC(st_long);

				mBits.skip(7);
				v = mBits.peek(10);
				t = (v>>1)-16;

				if (t<0)
					goto conceal_error;

				bcnt = mpeg_dct_coeff_decode2[t*4+2];
				idx += mpeg_dct_coeff_decode2[t*4+0]+1;
				level = mpeg_dct_coeff_decode2[t*4+1];
				mBits.skip(bcnt);

				_ASSERT(level != 0);

				if (v & (0x400>>bcnt))
					level_sign = -1;
			}
		}

		++coeff_count;

		if (idx >= zigzag+64) {
conceal_error:
			pos = 0;
			memset(dct_coeff+1, 0, 63*sizeof(dct_coeff[0]));
			break;
		}

		pos = *idx;

		// quant_matrix: 0...255
		// level: -256...255

		_ASSERT(level != 0);

		// We need to oddify coefficients down toward zero.
		// can't - already added DCT>FFT matrix!

		level = (((level*2+sign) * quant_matrix[pos] + 128) >> 8);

		// Negate coefficient if necessary.

		dct_coeff[pos] = (level ^ level_sign) - level_sign;

	}

#ifdef MB_STATS
#ifdef MB_SPLIT_STATS
	if (st_exit_intra && !(st_exit_intra & 262143)) {
		static char buf[256];
		int total	= st_exit_intra
					+ st_short_intra
					+ st_long_intra
					+ st_level1_idx0_intra
					+ st_level1_idx1_intra
					+ st_vshort_intra
					+ st_escape_intra;

		sprintf(buf, "[intra] %ld mblocks, %ld vshort (%d%%), %ld sh (%d%%), %ld ln (%d%%), %ld 1-0 (%d%%), %ld 1-1 (%d%%), %ld E (%d%%), %ld f-s (%d%%), %ld f-l (%d%%)\n"
					,st_exit_intra
					,st_vshort_intra		,(int)((st_vshort_intra			*100i64)/total)
					,st_short_intra			,(int)((st_short_intra			*100i64)/total)
					,st_long_intra			,(int)((st_long_intra			*100i64)/total)
					,st_level1_idx0_intra	,(int)((st_level1_idx0_intra	*100i64)/total)
					,st_level1_idx1_intra	,(int)((st_level1_idx1_intra	*100i64)/total)
					,st_escape_intra		,(int)((st_escape_intra			*100i64)/total)
					,st_first_short_intra	,(int)((st_first_short_intra	*100i64)/total)
					,st_first_long_intra	,(int)((st_first_long_intra		*100i64)/total)
					);
		OutputDebugString(buf);
	}
	if (st_exit_inter && !(st_exit_inter & 262143)) {
		static char buf[256];
		int total	= st_exit_inter
					+ st_short_inter
					+ st_long_inter
					+ st_level1_idx0_inter
					+ st_level1_idx1_inter
					+ st_vshort_inter
					+ st_escape_inter;

		sprintf(buf, "[inter] %ld mblocks, %ld vshort (%d%%), %ld sh (%d%%), %ld ln (%d%%), %ld 1-0 (%d%%), %ld 1-1 (%d%%), %ld E (%d%%), %ld f-s (%d%%), %ld f-l (%d%%)\n"
					,st_exit_inter
					,st_vshort_inter		,(int)((st_vshort_inter			*100i64)/total)
					,st_short_inter			,(int)((st_short_inter			*100i64)/total)
					,st_long_inter			,(int)((st_long_inter			*100i64)/total)
					,st_level1_idx0_inter	,(int)((st_level1_idx0_inter	*100i64)/total)
					,st_level1_idx1_inter	,(int)((st_level1_idx1_inter	*100i64)/total)
					,st_escape_inter		,(int)((st_escape_inter			*100i64)/total)
					,st_first_short_inter	,(int)((st_first_short_inter	*100i64)/total)
					,st_first_long_inter	,(int)((st_first_long_inter		*100i64)/total)
					);
		OutputDebugString(buf);
	}
#else
	if (!(st_exit & 262143)) {
		static char buf[256];
		int total = st_exit + st_short + st_long + st_level1_idx0 + st_level1_idx1 + st_vshort + st_escape;

		sprintf(buf, "%ld mblocks, %ld vshort (%d%%), %ld sh (%d%%), %ld ln (%d%%), %ld 1-0 (%d%%), %ld 1-1 (%d%%), %ld E (%d%%), %ld f-s (%d%%), %ld f-l (%d%%)\n"
					,st_exit
					,st_vshort,(int)((st_vshort*100i64)/total)
					,st_short,(int)((st_short*100i64)/total)
					,st_long,(int)((st_long*100i64)/total)
					,st_level1_idx0,(int)((st_level1_idx0*100i64)/total)
					,st_level1_idx1,(int)((st_level1_idx1*100i64)/total)
					,st_escape,(int)((st_escape*100i64)/total)
					,st_first_short, (int)((st_first_short*100i64)/total)
					,st_first_long, (int)((st_first_long*100i64)/total)
					);
		OutputDebugString(buf);
	}
#endif
#endif

	dct_coeff[0] += ROUNDVAL;

	if (!pos || coeff_count<=1) {
		if (macro_block_flags & MBF_INTRA)
			IDCT_fast_put(pos, dst, modulo);
		else
			IDCT_fast_add(pos, dst, modulo);
	} else {
		IDCT(dst, modulo, macro_block_flags & MBF_INTRA);
	}
}

void MPEGDecodeEngineMMX::DecodeBlock(YUVPixel *dst, long modulo, long DC_val) {
	const int *idx=zigzag_MMX;
	int level;
	int pos=0;
	const int *quant_matrix = intramatrix;
	unsigned long v;
	int sign = 0;

	mDCTCoeff[0] = (DC_val + 128) >> 8;

	if (!(macro_block_flags & MBF_INTRA)) {
		quant_matrix = nonintramatrix;
		sign = 1;

		v = mBits.peek();

		if (v < 0x80000000) {
			idx = zigzag_MMX-1;				// use AC coeff path to determine DC compensation

			MB_STAT_INC(st_first_long);
		} else {

			mDCTCoeff[0] = (((3*quant_matrix[0]) >> 4) - 1) | 1;

			if (v & 0x40000000)
				mDCTCoeff[0] = -mDCTCoeff[0];

			mBits.skip(2);

			MB_STAT_INC(st_first_short);
		}
	}
	
	// macroblock statistics from nuku.mpg:
	//
	//	2883584 mblocks
	//		7142656 (42%)		very short
	//		3457222 (20%)		level 1, idx_run = 0
	//		1604394 ( 9%)		level 1, idx_run = 1
	//		 937613 ( 5%)		long
	//		 451518 ( 2%)		short
	//		 161300 ( 0%)		escape
	//		1457101 (50%)		first long
	//		 705505 (24%)		first short

	// nuku1.mpg:
	//	47710208 mblocks
	//		120471300 (43%)		very short
	//		 52059388 (18%)		level 1, idx_run = 0
	//		 47710208 (17%)		exits
	//		 27499741 ( 9%)		level 1, idx_run = 1
	//		 18368812 ( 6%)		long
	//		  7911139 ( 2%)		short
	//		  4948686 ( 1%)		escape
	//		 29281756 (61%)		first long
	//		 14165596 (29%)		first short

	for(;;) {
		int level_sign = 0;

		v = mBits.peek();

		if (PREDICT(080, v >= 0x08000000)) {				// 080-FFF		(90%)
			if (PREDICT(600, v < 0x60000000)) {			// 080-5FF very short (40%)
				int t = (v>>24);
				int bcnt;

				bcnt = mpeg_dct_coeff_decode0[t*4+2-32];
				idx += mpeg_dct_coeff_decode0[t*4+0-32];
				level = mpeg_dct_coeff_decode0[t*4+1-32];

				_ASSERT(level != 0);

				mBits.skip8(bcnt);

				if ((v>>20) & mpeg_dct_coeff_decode0[t*4+3-32])
					level_sign = -1;
//				level_sign = (((signed long)(v>>20) & mpeg_dct_coeff_decode0[t*4+3-32])+0x7FFFFFFF) >> 31;

				MB_STAT_INC(st_vshort);

			} else if (PREDICT(C00, v >= 0xc0000000)) {	// C00-FFF level1-idx0 (20%)
				mBits.skipconst(3);

				++idx;

				level = 1;

//				if (v & 0x200)
//					level_sign = -1;

				level_sign = (((signed long)v&0x20000000)+0x7FFFFFFF) >> 31;

				MB_STAT_INC(st_level1_idx0);

			} else if (PREDICT(800, v >= 0x80000000)) {	// 800-BFF
				mBits.skipconst(2);

				MB_STAT_INC(st_exit);

				break;
			} else {					// 600-7FF
				mBits.skipconst(4);
				idx += 2;

				level = 1;
//				if (v & 0x100)
//					level_sign = -1;

				level_sign = (((signed long)v&0x10000000)+0x7FFFFFFF) >> 31;

				MB_STAT_INC(st_level1_idx1);

			}
		} else {
			if (PREDICT(040, v >= 0x04000000)) {
				mBits.skipconst(6);
				idx += mBits.getconst(6)+1;
				level = mBits.get_signed_const(8);

//				_ASSERT(level != 0);

				if (!(level & 0x7f)) {
					level <<= 1;
					level |= mBits.getconst(8);
				}

				if (level<0) {
					level = -level;
					level_sign = -1;
				}

				if (!level)
					goto conceal_error;

				MB_STAT_INC(st_escape);

			} else if (PREDICT(020, v >= 0x02000000)) {
				int t = (v>>22);

				idx += mpeg_dct_coeff_decode1[t*2+0-16]+1;
				level = mpeg_dct_coeff_decode1[t*2+1-16];

				_ASSERT(level != 0);

				MB_STAT_INC(st_short);

				mBits.skipconst(11);
//				if (v & 2)
//					level_sign = -1;
				level_sign = -((signed long)v&0x00200000)>>31;
			} else {
				int t, bcnt;

				MB_STAT_INC(st_long);

				mBits.skipconst(7);
				v = mBits.peek(10);
				t = (v>>1) - 16;

				if (t<0)
					goto conceal_error;

				bcnt = mpeg_dct_coeff_decode2[t*4+2];
				idx += mpeg_dct_coeff_decode2[t*4+0]+1;
				level = mpeg_dct_coeff_decode2[t*4+1];
				mBits.skip(bcnt);

//				if (v & (0x400>>bcnt))
//					level_sign = -1;
				level_sign = -((signed long)v&(0x400>>bcnt))>>31;
			}
		}

		if (idx >= zigzag_MMX+64) {
			// Error concealment: clear all but the first DC coefficent.

conceal_error:
			pos = 0;
			memset(mDCTCoeff+1, 0, 63*sizeof(mDCTCoeff[0]));
			break;
		}

		pos = *idx;

		// quant_matrix: 0...255
		// level: -256...255

		_ASSERT(level != 0);

		// We need to oddify coefficients down toward zero.

		level = ((((level*2+sign) * quant_matrix[pos]) >> 4) - 1) | 1;

		// Negate coefficient if necessary.

		mDCTCoeff[pos] = (level ^ level_sign) - level_sign;

	}

	if (!pos) {
		if (macro_block_flags & MBF_INTRA)
			IDCT_fast_put_MMX(mDCTCoeff[0]+4, dst, modulo);
		else
			IDCT_fast_add_MMX(mDCTCoeff[0]+4, dst, modulo);

		mDCTCoeff[0] = 0;
	} else
		mpIDCTMMX(mDCTCoeff, dst, modulo, !!(macro_block_flags & MBF_INTRA), pos);
}

void MPEGDecodeEngine::decode_mblock_Y(YUVPixel *dst) {
//	memset(dct_coeff, 0, sizeof dct_coeff);
#ifdef DEBUG
	for(int i=0; i<dct_coeff; i++)
		_ASSERT(dct_coeff[i] == 0);
#endif

	int value = 0;

	if ((macro_block_flags & MBF_INTRA)) {
		int size;

		{
			long v=mBits.peek(7)*2;

			if (v < 64*2) {
				mBits.skip8(2);
				size = (v>>6)+1;
			} else {
				size = mpeg_dct_size_luminance_decode[v - 64*2];
				mBits.skip(mpeg_dct_size_luminance_decode[v+1 - 64*2]);
			}
		}

		if (size) {
			int halfval;

			value = mBits.get(size);
			halfval = 1 << (size-1);

			if (value < halfval)
				value = (value+1) - 2*halfval;

			value <<= 11;
		}

		value = dct_dc_y_past += value;
	}

	(this->*mpDecodeBlock)(dst, y_pitch, value);
}

void MPEGDecodeEngine::decode_mblock_UV(YUVPixel *dst, int& dc_ref) {
//	memset(dct_coeff, 0, sizeof dct_coeff);
#ifdef DEBUG
	for(int i=0; i<dct_coeff; i++)
		_ASSERT(dct_coeff[i] == 0);
#endif

	int value = 0;

	if ((macro_block_flags & MBF_INTRA)) {
		int size;

		{
			long v=mBits.peek(8)*2;

			if (v < 192*2) {
				size = v>>7;
				mBits.skip8(2);
			} else {
				size = mpeg_dct_size_chrominance_decode[v - 192*2];
				mBits.skip(mpeg_dct_size_chrominance_decode[v+1 - 192*2]);
			}
		}

		if (size) {
			int halfval;

			value = mBits.get(size);

			halfval = 1 << (size-1);

			if (value < halfval)
				value = (value+1) - 2*halfval;

			value <<= 11;

		}

		value = dc_ref += value;
	}

	(this->*mpDecodeBlock)(dst, uv_pitch, value);
}

///////////////////////////

void MPEGDecodeEngine::video_process_picture_slice_I(char *ptr, int type) {
	long pos_x, pos_y;

	pos_y = type-1;
	pos_x = -1;

	dstY = Y_dest + 16 *  y_pitch * pos_y - 16;
	dstU = U_dest +  8 * uv_pitch * pos_y - 8;
	dstV = V_dest +  8 * uv_pitch * pos_y - 8;

	do {
		int inc = 0;
		int i;

		// 00000001111 (00F) -> padding
		// 00000001000 (008) -> skip 33 more
		// 1 -> skip 1

		i = mBits.peek(11);
		while(i == 0xf) {
			mBits.skip(11);
			i = mBits.peek(11);
		}
		while(i == 0x8) {
			mBits.skip(11);
			i = mBits.peek(11);
			inc += 33;
			dct_dc_y_past = dct_dc_u_past = dct_dc_v_past = MIDVAL;
		}
		
		if (i&0x400) {
			mBits.skip();
			++inc;
		} else if (i>=96) {
			i>>=4;
			mBits.skip8(mpeg_macro_block_inc_decode2[i*2+1-12]);
			inc += mpeg_macro_block_inc_decode2[i*2+0-12];
		} else {
			mBits.skip(mpeg_macro_block_inc_decode[i*2+1]);
			inc += mpeg_macro_block_inc_decode[i*2+0];
		}

		pos_x += inc;

		while(pos_x >= mbWidth) {
			pos_x-=mbWidth;
			pos_y++;
			if (pos_y >= mbHeight)
				return;
			dstY += y_modulo;
			dstU += uv_modulo;
			dstV += uv_modulo;
		}

		dstY += 16*inc;
		dstU += 8*inc;
		dstV += 8*inc;

		_ASSERT(dstY >= Y_dest);
		_ASSERT(dstY <= Y_dest + y_pitch * 16 * (mbHeight-1) + 16*(mbWidth-1));

		macro_block_flags = MBF_INTRA;

		if (!mBits.get_flag()) {
//			macro_block_flags |= MBF_NEW_QUANT;
			mBits.skip();

			int quant_scale = mBits.get8(5);
			intramatrix = intramatrices[quant_scale];
			nonintramatrix = nonintramatrices[quant_scale];
		}

		decode_mblock_Y(dstY);
		decode_mblock_Y(dstY + 8);
		decode_mblock_Y(dstY + y_pitch*8);
		decode_mblock_Y(dstY + y_pitch*8 + 8);
		decode_mblock_UV(dstU, dct_dc_u_past);
		decode_mblock_UV(dstV, dct_dc_v_past);

	} while(!mBits.next(23,0));
}

inline int MPEGDecodeEngine::mpeg_get_motion_component() {
	long v;

	v = mBits.peek(11);
	if (v & 0x400) {
		mBits.skip();
		return 0;
	} else if (v >= 96) {
		v = (v-96)>>4;
		mBits.skip(mpeg_motion_code_decode2[v*2+1]);
		return mpeg_motion_code_decode2[v*2+0];
	} else {
		mBits.skip(mpeg_motion_code_decode[v*2+1]);
		return mpeg_motion_code_decode[v*2+0];
	}
}

void MPEGDecodeEngine::video_process_picture_slice_P(char *ptr, int type) {
	BOOL is_first_block = TRUE;
	long pos_x, pos_y;

	pos_y = type-1;
	pos_x = -1;

	dstY = Y_dest + 16 *  y_pitch * pos_y - 16;
	dstU = U_dest +  8 * uv_pitch * pos_y - 8;
	dstV = V_dest +  8 * uv_pitch * pos_y - 8;

	do {
		int inc = 0;
		signed char cbp = 0x3f;
		int i;

		// 00000001111 (00F) -> padding
		// 00000001000 (008) -> skip 33 more
		// 1 -> skip 1

		i = mBits.peek(11);
		while(i == 0xf) {
			mBits.skip(11);
			i = mBits.peek(11);
		}
		while(i == 0x8) {
			mBits.skip(11);
			i = mBits.peek(11);
			inc += 33;
		}
		
		if (i&0x400) {
			mBits.skip();
			++inc;
		} else if (i>=96) {
			i >>= 4;
			mBits.skip8(mpeg_macro_block_inc_decode2[i*2+1-12]);
			inc += mpeg_macro_block_inc_decode2[i*2+0-12];
		} else {
			mBits.skip(mpeg_macro_block_inc_decode[i*2+1]);
			inc += mpeg_macro_block_inc_decode[i*2+0];
		}

//		_RPT3(0,"(%d,%d): inc %d\n", (pos_x+1), pos_y + (pos_x==mbWidth-1?1:0), inc);

//		_RPT1(0,"skip: %ld\n", inc);
		if (inc > 1) {
			dct_dc_y_past = dct_dc_u_past = dct_dc_v_past = MIDVAL;

			forw_vector_x = forw_vector_y = 0;
			if (pos_x >= 0)
				for(i=1; i<inc; i++) {
					++pos_x;
					dstY += 16;
					dstU += 8;
					dstV += 8;

					if(pos_x >= mbWidth) {
						pos_x-=mbWidth;
						pos_y++;

						if (pos_y >= mbHeight)
							return;

						dstY += y_modulo;
						dstU += uv_modulo;
						dstV += uv_modulo;
					}

					video_copy_forward(pos_x, pos_y);
				}
			else {
				pos_x += inc-1;
				dstY += 16*(inc-1);
				dstU += 8*(inc-1);
				dstV += 8*(inc-1);
			}
//			} else pos_x += inc-1;
		}

		++pos_x;

		while (pos_x >= mbWidth) {
			pos_x-=mbWidth;
			pos_y++;
			if (pos_y >= mbHeight)
				return;
			dstY += y_modulo;
			dstU += uv_modulo;
			dstV += uv_modulo;
		}

		dstY += 16;
		dstU += 8;
		dstV += 8;

		_ASSERT(dstY >= Y_dest);
		_ASSERT(dstY <= Y_dest + y_pitch * 16 * (mbHeight-1) + 16*(mbWidth-1));

		{
			long v=mBits.peek(6);

			if (v>=32) {
				macro_block_flags = MBF_FORWARD | MBF_PATTERN;
				mBits.skip();
			} else {
				macro_block_flags = mpeg_p_type_mb_type_decode[v*2];
				mBits.skip(mpeg_p_type_mb_type_decode[v*2+1]);
			}
		}

		if (!(macro_block_flags & MBF_INTRA)) {
			dct_dc_y_past = dct_dc_u_past = dct_dc_v_past = MIDVAL;
			cbp = 0;
		}

		if (macro_block_flags & MBF_NEW_QUANT) {
			int quant_scale = mBits.get8(5);
			intramatrix = intramatrices[quant_scale];
			nonintramatrix = nonintramatrices[quant_scale];
		}

		if (macro_block_flags & MBF_FORWARD) {				// motion vector for forward prediction exists
			int motion_x_forw_c, motion_y_forw_c;
			int motion_x_forw_r, motion_y_forw_r;
			int delta;

			motion_x_forw_c = mpeg_get_motion_component();

			// according to this information the motion vector must be decoded

			if ((signed char)forw_vector_bits<=0 || motion_x_forw_c==0)
				delta = motion_x_forw_c;
			else {
				motion_x_forw_r = mBits.get(forw_vector_bits)+1;

				if (motion_x_forw_c<0)
					delta = -(((-motion_x_forw_c-1)<<forw_vector_bits) + motion_x_forw_r);
				else
					delta = ((motion_x_forw_c-1)<<forw_vector_bits) + motion_x_forw_r;
			}
			forw_vector_x = (((forw_vector_x + delta) & forw_vector_mask) + forw_vector_extend) ^ forw_vector_extend;

			motion_y_forw_c = mpeg_get_motion_component();

			if ((signed char)forw_vector_bits<=0 || motion_y_forw_c==0)
				delta = motion_y_forw_c;
			else {
				motion_y_forw_r = mBits.get(forw_vector_bits)+1;

				if (motion_y_forw_c<0)
					delta = -(((-motion_y_forw_c-1)<<forw_vector_bits) + motion_y_forw_r);
				else
					delta = ((motion_y_forw_c-1)<<forw_vector_bits) + motion_y_forw_r;
			}
			forw_vector_y = (((forw_vector_y + delta) & forw_vector_mask) + forw_vector_extend) ^ forw_vector_extend;
				
			// grab the referred area into "pel1"

//_RPT4(0,"(%d,%d) copy motion vector (%+d,%+d)\n", pos_x, pos_y, forw_vector_x, forw_vector_y);

//			video_copy_forward_prediction(dstY, dstU, dstV, pos_x, pos_y);
		} else { // (only) in P_TYPE the motion vector is to be reset. 
			forw_vector_x = forw_vector_y = 0;
//_RPT2(0,"(%d,%d) reset motion vector\n", pos_x, pos_y);
			video_copy_forward(pos_x, pos_y);
		}

		if ((macro_block_flags & MBF_PATTERN)) {
#if 0
			long v = mBits.peek(9)*2;

			cbp = mpeg_block_pattern_decode[v+0];
			mBits.skip(mpeg_block_pattern_decode[v+1]);
#else
			long v = mBits.peek(9);

			if (v >= 128) {
				v>>=4;
				cbp = mpeg_block_pattern_decode0[v*2+0-16];
				mBits.skip(mpeg_block_pattern_decode0[v*2+1-16]);
			} else {
				cbp = mpeg_block_pattern_decode1[v*2+0];
				mBits.skip(mpeg_block_pattern_decode1[v*2+1]);
			}
#endif
		}

		if (macro_block_flags & MBF_FORWARD)
			video_copy_forward_prediction(pos_x, pos_y, false);

		if (cbp & 0x20) decode_mblock_Y(dstY);
		if (cbp & 0x10) decode_mblock_Y(dstY + 8);
		if (cbp & 0x08) decode_mblock_Y(dstY + y_pitch*8);
		if (cbp & 0x04) decode_mblock_Y(dstY + y_pitch*8 + 8);

		if (macro_block_flags & MBF_FORWARD)
			video_copy_forward_prediction(pos_x, pos_y, true);

		if (cbp & 0x02) decode_mblock_UV(dstU, dct_dc_u_past);
		if (cbp & 0x01) decode_mblock_UV(dstV, dct_dc_v_past);

		if (!(macro_block_flags & MBF_INTRA))
			dct_dc_y_past = dct_dc_u_past = dct_dc_v_past = MIDVAL;

		is_first_block=FALSE;

#ifdef STATISTICS
		if (macro_block_flags & MBF_PATTERN)
			stats.coded_block_pattern++;
#endif
	} while(!mBits.next(23,0));
}

void MPEGDecodeEngine::video_process_picture_slice_B(char *ptr, int type) {
	BOOL is_first_block = TRUE;
	long pos_x, pos_y;

	pos_y = type-1;
	pos_x = -1;

	dstY = Y_dest + 16 *  y_pitch * pos_y - 16;
	dstU = U_dest +  8 * uv_pitch * pos_y - 8;
	dstV = V_dest +  8 * uv_pitch * pos_y - 8;

	do {
		int inc = 0;
		signed char cbp = 0x3f;
		int i;

		// 00000001111 (00F) -> padding
		// 00000001000 (008) -> skip 33 more
		// 1 -> skip 1

		i = mBits.peek(11);
		while(i == 0xf) {
			mBits.skip(11);
			i = mBits.peek(11);
		}
		while(i == 0x8) {
			mBits.skip(11);
			i = mBits.peek(11);
			inc += 33;
		}
		
		if (i&0x400) {
			mBits.skip();
			++inc;
		} else if (i>=96) {
			i >>= 4;
			mBits.skip8(mpeg_macro_block_inc_decode2[i*2+1-12]);
			inc += mpeg_macro_block_inc_decode2[i*2+0-12];
		} else {
			mBits.skip(mpeg_macro_block_inc_decode[i*2+1]);
			inc += mpeg_macro_block_inc_decode[i*2+0];
		}

		if (inc > 1) {
			dct_dc_y_past = dct_dc_u_past = dct_dc_v_past = MIDVAL;

			if (!is_first_block) {
				for(i=1; i<inc; i++) {
					++pos_x;
					dstY += 16;
					dstU += 8;
					dstV += 8;

					if(pos_x >= mbWidth) {
						pos_x-=mbWidth;
						pos_y++;
						if (pos_y >= mbHeight)
							return;
						dstY += y_modulo;
						dstU += uv_modulo;
						dstV += uv_modulo;
					}

					if ((macro_block_flags & MBF_FORWARD)) {
						video_copy_forward_prediction(
							pos_x, pos_y, false);

						if ((macro_block_flags & MBF_BACKWARD))
							video_add_backward_prediction(
								pos_x, pos_y, false);
					} else if ((macro_block_flags & MBF_BACKWARD)) {
						video_copy_backward_prediction(
							pos_x, pos_y, false);
					}
					if ((macro_block_flags & MBF_FORWARD)) {
						video_copy_forward_prediction(
							pos_x, pos_y, true);

						if ((macro_block_flags & MBF_BACKWARD))
							video_add_backward_prediction(
								pos_x, pos_y, true);
					} else if ((macro_block_flags & MBF_BACKWARD)) {
						video_copy_backward_prediction(
							pos_x, pos_y, true);
					}
				}
			} else {
				pos_x += inc-1;
				dstY += 16*(inc-1);
				dstU += 8*(inc-1);
				dstV += 8*(inc-1);
			}
		}

		++pos_x;

		while (pos_x >= mbWidth) {
			pos_x -= mbWidth;
			++pos_y;
			if (pos_y >= mbHeight)
				return;

			dstY += y_modulo;
			dstU += uv_modulo;
			dstV += uv_modulo;
		}

		dstY += 16;
		dstU += 8;
		dstV += 8;

		_ASSERT(dstY >= Y_dest);
		_ASSERT(dstY <= Y_dest + y_pitch * 16 * (mbHeight-1) + 16*(mbWidth-1));

		{
			long v=mBits.peek(6);

			if (v>=32) {
				macro_block_flags = MBF_FORWARD | MBF_BACKWARD;
				if (v>=48)
					macro_block_flags = MBF_FORWARD | MBF_BACKWARD | MBF_PATTERN;

				mBits.skip8(2);
			} else {
				macro_block_flags = mpeg_b_type_mb_type_decode[v*2];
				mBits.skip8(mpeg_b_type_mb_type_decode[v*2+1]);
			}
		}


		if (!(macro_block_flags & MBF_INTRA)) {
			dct_dc_y_past = dct_dc_u_past = dct_dc_v_past = MIDVAL;
			cbp = 0;
		}

		if ((macro_block_flags & MBF_NEW_QUANT)) {
			int quant_scale = mBits.get8(5);
			intramatrix = intramatrices[quant_scale];
			nonintramatrix = nonintramatrices[quant_scale];
		}

		if ((macro_block_flags & MBF_FORWARD)) {				// motion vector for forward prediction exists
			int motion_x_forw_c, motion_y_forw_c;
			int motion_x_forw_r, motion_y_forw_r;
			int delta;

			motion_x_forw_c = mpeg_get_motion_component();

			// according to this information the motion vector must be decoded

			if ((signed char)forw_vector_bits<=0 || motion_x_forw_c==0)
				delta = motion_x_forw_c;
			else {
				motion_x_forw_r = mBits.get(forw_vector_bits)+1;

				if (motion_x_forw_c<0)
					delta = -(((-motion_x_forw_c-1)<<forw_vector_bits) + motion_x_forw_r);
				else
					delta = ((motion_x_forw_c-1)<<forw_vector_bits) + motion_x_forw_r;
			}
			forw_vector_x = (((forw_vector_x + delta) & forw_vector_mask) + forw_vector_extend) ^ forw_vector_extend;

			motion_y_forw_c = mpeg_get_motion_component();

			if ((signed char)forw_vector_bits<=0 || motion_y_forw_c==0)
				delta = motion_y_forw_c;
			else {
				motion_y_forw_r = mBits.get(forw_vector_bits)+1;

				if (motion_y_forw_c<0)
					delta = -(((-motion_y_forw_c-1)<<forw_vector_bits) + motion_y_forw_r);
				else
					delta = ((motion_y_forw_c-1)<<forw_vector_bits) + motion_y_forw_r;
			}
			forw_vector_y = (((forw_vector_y + delta) & forw_vector_mask) + forw_vector_extend) ^ forw_vector_extend;

		}

		if ((macro_block_flags & MBF_BACKWARD)) {
			int motion_x_back_c, motion_y_back_c;
			int motion_x_back_r, motion_y_back_r;
			int delta;

			motion_x_back_c = mpeg_get_motion_component();

			// according to this information the motion vector must be decoded

			if ((signed char)back_vector_bits<=0 || motion_x_back_c==0)
				delta = motion_x_back_c;
			else {
				motion_x_back_r = mBits.get(back_vector_bits)+1;

				if (motion_x_back_c<0)
					delta = -(((-motion_x_back_c-1)<<back_vector_bits) + motion_x_back_r);
				else
					delta = ((motion_x_back_c-1)<<back_vector_bits) + motion_x_back_r;
			}
			back_vector_x = (((back_vector_x + delta) & back_vector_mask) + back_vector_extend) ^ back_vector_extend;

			motion_y_back_c = mpeg_get_motion_component();

			if ((signed char)back_vector_bits<=0 || motion_y_back_c==0)
				delta = motion_y_back_c;
			else {
				motion_y_back_r = mBits.get(back_vector_bits)+1;

				if (motion_y_back_c<0)
					delta = -(((-motion_y_back_c-1)<<back_vector_bits) + motion_y_back_r);
				else
					delta = ((motion_y_back_c-1)<<back_vector_bits) + motion_y_back_r;
			}
			back_vector_y = (((back_vector_y + delta) & back_vector_mask) + back_vector_extend) ^ back_vector_extend;
				
		}

		if ((macro_block_flags & MBF_PATTERN)) {
			long v = mBits.peek(9);

			if (v >= 128) {
				v>>=4;
				cbp = mpeg_block_pattern_decode0[v*2+0-16];
				mBits.skip8(mpeg_block_pattern_decode0[v*2+1-16]);
			} else {
				cbp = mpeg_block_pattern_decode1[v*2+0];
				mBits.skip(mpeg_block_pattern_decode1[v*2+1]);
			}
		}

		if (macro_block_flags & MBF_FORWARD) {
			video_copy_forward_prediction(pos_x, pos_y, false);
			if (macro_block_flags & MBF_BACKWARD)
				video_add_backward_prediction(pos_x, pos_y, false);
		} else if (macro_block_flags & MBF_BACKWARD)
			video_copy_backward_prediction(pos_x, pos_y, false);

		if (cbp & 0x20) decode_mblock_Y(dstY);
		if (cbp & 0x10) decode_mblock_Y(dstY + 8);
		if (cbp & 0x08) decode_mblock_Y(dstY + y_pitch*8);
		if (cbp & 0x04) decode_mblock_Y(dstY + y_pitch*8 + 8);

		if (macro_block_flags & MBF_FORWARD) {
			video_copy_forward_prediction(pos_x, pos_y, true);
			if (macro_block_flags & MBF_BACKWARD)
				video_add_backward_prediction(pos_x, pos_y, true);
		} else if (macro_block_flags & MBF_BACKWARD)
			video_copy_backward_prediction(pos_x, pos_y, true);

		if (cbp & 0x02) decode_mblock_UV(dstU, dct_dc_u_past);
		if (cbp & 0x01) decode_mblock_UV(dstV, dct_dc_v_past);

		if (macro_block_flags & MBF_INTRA)
			forw_vector_x = forw_vector_y = back_vector_x = back_vector_y = 0;			
		else
			dct_dc_y_past = dct_dc_u_past = dct_dc_v_past = MIDVAL;

		is_first_block=FALSE;

#ifdef STATISTICS
		if (macro_block_flags & MBF_PATTERN)
			stats.coded_block_pattern++;
#endif

	} while(!mBits.next(23,0));
}

void MPEGDecodeEngine::video_process_picture_slice(char *ptr, int type) {
	int quant_scale;

	mBits = CMemoryBitInput(ptr);

	dct_dc_y_past = dct_dc_u_past = dct_dc_v_past = MIDVAL;

	forw_vector_x = forw_vector_y = 0;
	back_vector_x = back_vector_y = 0;

	quant_scale = mBits.get(5);
	intramatrix = intramatrices[quant_scale];
	nonintramatrix = nonintramatrices[quant_scale];

	memset(dct_coeff, 0, sizeof dct_coeff);

	while(mBits.get())
		mBits.skip(8);

	if (type > mbHeight)
		return;

	switch(frame_type) {
	case I_FRAME:	video_process_picture_slice_I(ptr, type); break;
	case P_FRAME:	video_process_picture_slice_P(ptr, type); break;
	case B_FRAME:	video_process_picture_slice_B(ptr, type); break;
	}
}

//////////////////////////////////////////////////////////////

extern "C" void asm_YUVtoRGB32_row(unsigned long *ARGB1_pointer, unsigned long *ARGB2_pointer, YUVPixel *Y1_pointer, YUVPixel *Y2_pointer, YUVPixel *U_pointer, YUVPixel *V_pointer, long width);
extern "C" void asm_YUVtoRGB32_row_MMX(unsigned long *ARGB1_pointer, unsigned long *ARGB2_pointer, YUVPixel *Y1_pointer, YUVPixel *Y2_pointer, YUVPixel *U_pointer, YUVPixel *V_pointer, long width);
extern "C" void asm_YUVtoRGB32_row_ISSE(unsigned long *ARGB1_pointer, unsigned long *ARGB2_pointer, YUVPixel *Y1_pointer, YUVPixel *Y2_pointer, YUVPixel *U_pointer, YUVPixel *V_pointer, long width);
extern "C" void asm_YUVtoRGB32_row_SSE2(unsigned long *ARGB1_pointer, unsigned long *ARGB2_pointer, YUVPixel *Y1_pointer, YUVPixel *Y2_pointer, YUVPixel *U_pointer, YUVPixel *V_pointer, long width);

extern "C" void asm_YUVtoRGB32hq_row_ISSE(
		unsigned long *ARGB1_pointer,
		unsigned long *ARGB2_pointer,
		YUVPixel *Y1_pointer,
		YUVPixel *Y2_pointer,
		YUVPixel *U_pointer,
		YUVPixel *V_pointer,
		long width,
		long uv_up,
		long uv_down
		);

extern "C" void asm_YUVtoRGB24_row(unsigned long *ARGB1_pointer, unsigned long *ARGB2_pointer, YUVPixel *Y1_pointer, YUVPixel *Y2_pointer, YUVPixel *U_pointer, YUVPixel *V_pointer, long width);
extern "C" void asm_YUVtoRGB24_row_MMX(unsigned long *ARGB1_pointer, unsigned long *ARGB2_pointer, YUVPixel *Y1_pointer, YUVPixel *Y2_pointer, YUVPixel *U_pointer, YUVPixel *V_pointer, long width);
extern "C" void asm_YUVtoRGB24_row_ISSE(unsigned long *ARGB1_pointer, unsigned long *ARGB2_pointer, YUVPixel *Y1_pointer, YUVPixel *Y2_pointer, YUVPixel *U_pointer, YUVPixel *V_pointer, long width);

extern "C" void asm_YUVtoRGB24_SSE2(unsigned long *ARGB1_pointer, unsigned long *ARGB2_pointer, YUVPixel *Y1_pointer, YUVPixel *Y2_pointer, YUVPixel *U_pointer, YUVPixel *V_pointer, long width, MPEGRGBDecodeWorkspace *workspace);

extern "C" void asm_YUVtoRGB16_row(unsigned long *ARGB1_pointer, unsigned long *ARGB2_pointer, YUVPixel *Y1_pointer, YUVPixel *Y2_pointer, YUVPixel *U_pointer, YUVPixel *V_pointer, long width);
extern "C" void asm_YUVtoRGB16_row_MMX(unsigned long *ARGB1_pointer, unsigned long *ARGB2_pointer, YUVPixel *Y1_pointer, YUVPixel *Y2_pointer, YUVPixel *U_pointer, YUVPixel *V_pointer, long width);
extern "C" void asm_YUVtoRGB16_row_ISSE(unsigned long *ARGB1_pointer, unsigned long *ARGB2_pointer, YUVPixel *Y1_pointer, YUVPixel *Y2_pointer, YUVPixel *U_pointer, YUVPixel *V_pointer, long width);

typedef void (__cdecl *tpYUVtoRGB1)(unsigned long *ARGB1_pointer, unsigned long *ARGB2_pointer, YUVPixel *Y1_pointer, YUVPixel *Y2_pointer, YUVPixel *U_pointer, YUVPixel *V_pointer, long width);
typedef void (__cdecl *tpYUVtoRGB2)(unsigned long *ARGB1_pointer, unsigned long *ARGB2_pointer, YUVPixel *Y1_pointer, YUVPixel *Y2_pointer, YUVPixel *U_pointer, YUVPixel *V_pointer, long width);

static void YUVToRGB32(YUVPixel *Y_ptr, YUVPixel *U_ptr, YUVPixel *V_ptr, unsigned char *dst, long bpr, long w, long h) {
	dst = dst + bpr * (2*h - 2);

//#pragma warning("don't ship with this!!!!!!");

	tpYUVtoRGB1 rowFunc = SSE2_enabled ? asm_YUVtoRGB32_row_SSE2 : ISSE_enabled ? asm_YUVtoRGB32_row_ISSE : MMX_enabled ? asm_YUVtoRGB32_row_MMX : asm_YUVtoRGB32_row;

	long h0 = h;

	do {
#if 1
		rowFunc((unsigned long *)(dst + bpr),
				(unsigned long *)dst,
				Y_ptr,
				Y_ptr + y_pitch,
				U_ptr,
				V_ptr,
				w);
#else
		asm_YUVtoRGB32hq_row_ISSE(
				(unsigned long *)(dst + bpr),
				(unsigned long *)dst,
				Y_ptr,
				Y_ptr + y_pitch,
				U_ptr,
				V_ptr,
				w,
				h0==h ? 0 : -uv_pitch,
				h==1 ? 0 : uv_pitch
				);
#endif

		dst = dst - 2*bpr;
		Y_ptr = Y_ptr + 2*y_pitch;
		U_ptr = U_ptr + uv_pitch;
		V_ptr = V_ptr + uv_pitch;
	} while(--h);

	if (MMX_enabled)
		__asm emms

	if (ISSE_enabled)
		__asm sfence
}

static void YUVToRGB24(YUVPixel *Y_ptr, YUVPixel *U_ptr, YUVPixel *V_ptr, unsigned char *dst, long bpr, long w, long h, MPEGRGBDecodeWorkspace *workspace) {
	dst = dst + bpr * (2*h - 2);

	if (SSE2_enabled) {
		workspace->rgb_pitch	= -bpr*2-w*6;
		workspace->y_pitch		= y_pitch*2;
		workspace->uv_pitch		= uv_pitch;
		workspace->w			= w;
		workspace->h			= h;

		asm_YUVtoRGB24_SSE2(
				(unsigned long *)(dst + bpr),
				(unsigned long *)dst,
				Y_ptr,
				Y_ptr + y_pitch,
				U_ptr,
				V_ptr,
				w,
				workspace);

		__asm emms
		__asm sfence

		return;
	}

	tpYUVtoRGB2 rowFunc = ISSE_enabled ? asm_YUVtoRGB24_row_ISSE : MMX_enabled ? asm_YUVtoRGB24_row_MMX : asm_YUVtoRGB24_row;

	do {
		rowFunc((unsigned long *)(dst + bpr),
				(unsigned long *)dst,
				Y_ptr,
				Y_ptr + y_pitch,
				U_ptr,
				V_ptr,
				w);

		dst = dst - 2*bpr;
		Y_ptr = Y_ptr + 2*y_pitch;
		U_ptr = U_ptr + uv_pitch;
		V_ptr = V_ptr + uv_pitch;
	} while(--h);

	if (MMX_enabled)
		__asm emms

	if (ISSE_enabled)
		__asm sfence
}
static void YUVToRGB16(YUVPixel *Y_ptr, YUVPixel *U_ptr, YUVPixel *V_ptr, unsigned char *dst, long bpr, long w, long h) {
	dst = dst + bpr * (2*h - 2);

	tpYUVtoRGB1 rowFunc = ISSE_enabled ? asm_YUVtoRGB16_row_ISSE : MMX_enabled ? asm_YUVtoRGB16_row_MMX : asm_YUVtoRGB16_row;

	do {
		rowFunc((unsigned long *)(dst + bpr),
				(unsigned long *)dst,
				Y_ptr,
				Y_ptr + y_pitch,
				U_ptr,
				V_ptr,
				w);

		dst = dst - 2*bpr;
		Y_ptr = Y_ptr + 2*y_pitch;
		U_ptr = U_ptr + uv_pitch;
		V_ptr = V_ptr + uv_pitch;
	} while(--h);

	if (MMX_enabled)
		__asm emms

	if (ISSE_enabled)
		__asm sfence
}

static void __declspec(naked) YUVtoUYVY16_MMX(
				YUVPixel *Y_ptr,		// [esp+4+16]
				YUVPixel *U_ptr,		// [esp+8+16]
				YUVPixel *V_ptr,		// [esp+12+16]
				unsigned char *dst,		// [esp+16+16]
				long bpr,				// [esp+20+16]
				long w,					// [esp+24+16]
				long h) {				// [esp+28+16]

	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			edx,[esp+24+16]			;load width (mult of 8)

		mov			ebx,[esp+8+16]			;load source U ptr
		mov			ecx,[esp+12+16]			;load source V ptr
		mov			eax,[esp+4+16]			;load source Y ptr
		mov			edi,[esp+16+16]			;load destination ptr
		mov			esi,[esp+20+16]			;load destination pitch
		mov			ebp,[esp+28+16]			;load height

		lea			ebx,[ebx+edx]			;bias pointers
		lea			ecx,[ecx+edx]			;(we count from -n to 0)
		lea			eax,[eax+edx*2]
		lea			edi,[edi+edx*4]

		neg			edx
		mov			[esp+24+16],edx
xyloop:
		movq		mm0,[ebx+edx]			;U0-U7

		movq		mm7,[ecx+edx]			;V0-V7
		movq		mm2,mm0					;U0-U7

		movq		mm4,[eax+edx*2]
		punpcklbw	mm0,mm7					;[V3][U3][V2][U2][V1][U1][V0][U0]

		movq		mm5,[eax+edx*2+8]
		punpckhbw	mm2,mm7					;[V7][U7][V6][U6][V5][U5][V4][U4]

		movq		mm1,mm0
		punpcklbw	mm0,mm4					;[Y3][V1][Y2][U1][Y1][V0][Y0][U0]

		punpckhbw	mm1,mm4					;[Y7][V3][Y6][U3][Y5][V2][Y4][U2]
		movq		mm3,mm2

		movq		[edi+edx*4+ 0],mm0
		punpcklbw	mm2,mm5					;[YB][V5][YA][U5][Y9][V4][Y8][U4]

		movq		[edi+edx*4+ 8],mm1
		punpckhbw	mm3,mm5					;[YF][V7][YE][U7][YD][V6][YC][U6]

		movq		[edi+edx*4+16],mm2

		movq		[edi+edx*4+24],mm3

		add			edx,8
		jnc			xyloop

		mov			edx,[esp+24+16]			;reload width counter

		test		ebp,1					;update U/V row every other row only
		jz			oddline

		add			ebx,uv_pitch			;advance U pointer
		add			ecx,uv_pitch			;advance V pointer

oddline:
		add			eax,y_pitch				;advance Y pointer

		add			edi,esi					;advance dest ptr

		dec			ebp
		jne			xyloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		emms
		ret
	}
}

static void YUVToUYVY16(YUVPixel *Y_ptr, YUVPixel *U_ptr, YUVPixel *V_ptr, unsigned char *dst, long bpr, long w, long h) {
	if (MMX_enabled) {
		YUVtoUYVY16_MMX(Y_ptr, U_ptr, V_ptr, dst, bpr, w, h*2);
		return;
	}

	w>>=3;

	do {
		int wt;

		wt = w;
		do {
			volatile char z = dst[31];
			dst[0] = U_ptr[0];
			dst[1] = Y_ptr[0];
			dst[2] = V_ptr[0];
			dst[3] = Y_ptr[1];
			dst[4] = U_ptr[1];
			dst[5] = Y_ptr[2];
			dst[6] = V_ptr[1];
			dst[7] = Y_ptr[3];
			dst[8] = U_ptr[2];
			dst[9] = Y_ptr[4];
			dst[10] = V_ptr[2];
			dst[11] = Y_ptr[5];
			dst[12] = U_ptr[3];
			dst[13] = Y_ptr[6];
			dst[14] = V_ptr[4];
			dst[15] = Y_ptr[7];
			dst[16] = U_ptr[4];
			dst[17] = Y_ptr[8];
			dst[18] = V_ptr[4];
			dst[19] = Y_ptr[9];
			dst[20] = U_ptr[5];
			dst[21] = Y_ptr[10];
			dst[22] = V_ptr[5];
			dst[23] = Y_ptr[11];
			dst[24] = U_ptr[6];
			dst[25] = Y_ptr[12];
			dst[26] = V_ptr[6];
			dst[27] = Y_ptr[13];
			dst[28] = U_ptr[7];
			dst[29] = Y_ptr[14];
			dst[30] = V_ptr[7];
			dst[31] = Y_ptr[15];
			dst += 32;
			U_ptr += 8;
			V_ptr += 8;
			Y_ptr += 16;
		} while(--wt);

		U_ptr -= w*8;
		V_ptr -= w*8;
		Y_ptr += y_pitch - w*16;
		
		wt = w;
		do {
			volatile char z = dst[31];
			dst[0] = U_ptr[0];
			dst[1] = Y_ptr[0];
			dst[2] = V_ptr[0];
			dst[3] = Y_ptr[1];
			dst[4] = U_ptr[1];
			dst[5] = Y_ptr[2];
			dst[6] = V_ptr[1];
			dst[7] = Y_ptr[3];
			dst[8] = U_ptr[2];
			dst[9] = Y_ptr[4];
			dst[10] = V_ptr[2];
			dst[11] = Y_ptr[5];
			dst[12] = U_ptr[3];
			dst[13] = Y_ptr[6];
			dst[14] = V_ptr[4];
			dst[15] = Y_ptr[7];
			dst[16] = U_ptr[4];
			dst[17] = Y_ptr[8];
			dst[18] = V_ptr[4];
			dst[19] = Y_ptr[9];
			dst[20] = U_ptr[5];
			dst[21] = Y_ptr[10];
			dst[22] = V_ptr[5];
			dst[23] = Y_ptr[11];
			dst[24] = U_ptr[6];
			dst[25] = Y_ptr[12];
			dst[26] = V_ptr[6];
			dst[27] = Y_ptr[13];
			dst[28] = U_ptr[7];
			dst[29] = Y_ptr[14];
			dst[30] = V_ptr[7];
			dst[31] = Y_ptr[15];
			dst += 32;
			U_ptr += 8;
			V_ptr += 8;
			Y_ptr += 16;
		} while(--wt);

		U_ptr += uv_pitch - w*8;
		V_ptr += uv_pitch - w*8;
		Y_ptr += y_pitch - w*16;
	} while(--h);
}

static void __declspec(naked) YUVtoYUY2_MMX(
				YUVPixel *Y_ptr,		// [esp+4+16]
				YUVPixel *U_ptr,		// [esp+8+16]
				YUVPixel *V_ptr,		// [esp+12+16]
				unsigned char *dst,		// [esp+16+16]
				long bpr,				// [esp+20+16]
				long w,					// [esp+24+16]
				long h) {				// [esp+28+16]

	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			edx,[esp+24+16]			;multiply width by 8

		mov			ebx,[esp+8+16]			;load source U ptr
		mov			ecx,[esp+12+16]			;load source V ptr
		mov			eax,[esp+4+16]			;load source Y ptr
		mov			edi,[esp+16+16]			;load destination ptr
		mov			esi,[esp+20+16]			;load destination pitch
		mov			ebp,[esp+28+16]			;load height

		lea			ebx,[ebx+edx]			;bias pointers
		lea			ecx,[ecx+edx]			;(we count from -n to 0)
		lea			eax,[eax+edx*2]
		lea			edi,[edi+edx*4]

		neg			edx
		mov			[esp+24+16],edx
xyloop:
		movq		mm0,[ebx+edx]			;U0-U7

		movq		mm7,[ecx+edx]			;V0-V7
		movq		mm1,mm0					;U0-U7

		movq		mm2,[eax+edx*2]			;Y0-Y7
		punpcklbw	mm0,mm7					;[V3][U3][V2][U2][V1][U1][V0][U0]

		movq		mm4,[eax+edx*2+8]		;Y8-YF
		punpckhbw	mm1,mm7					;[V7][U7][V6][U6][V5][U5][V4][U4]

		movq		mm3,mm2
		punpcklbw	mm2,mm0					;[V1][Y3][U1][Y2][V0][Y1][U0][Y0]

		movq		mm5,mm4
		punpckhbw	mm3,mm0					;[V3][Y7][U3][Y6][V2][Y5][U2][Y4]

		movq		[edi+edx*4+ 0],mm2
		punpcklbw	mm4,mm1					;[V5][YB][U5][YA][V4][Y9][U4][Y8]

		movq		[edi+edx*4+ 8],mm3
		punpckhbw	mm5,mm1					;[V7][YF][U7][YE][V6][YD][U6][YC]

		movq		[edi+edx*4+16],mm4

		movq		[edi+edx*4+24],mm5
		add			edx,8

		jnc			xyloop

		mov			edx,[esp+24+16]			;reload width counter

		test		ebp,1					;update U/V row every other row only
		jz			oddline

		add			ebx,uv_pitch			;advance U pointer
		add			ecx,uv_pitch			;advance V pointer

oddline:
		add			eax,y_pitch				;advance Y pointer

		add			edi,esi					;advance dest ptr

		dec			ebp
		jne			xyloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		emms
		ret
	}
}

static void YUVToYUY216(YUVPixel *Y_ptr, YUVPixel *U_ptr, YUVPixel *V_ptr, unsigned char *dst, long bpr, long w, long h) {

	if (MMX_enabled) {
		YUVtoYUY2_MMX(Y_ptr, U_ptr, V_ptr, dst, bpr, w, h*2);
		return;
	}

	w>>=3;
	do {
		int wt;

		wt = w;
		do {
			volatile char z = dst[31];
			dst[0] = Y_ptr[0];
			dst[1] = U_ptr[0];
			dst[2] = Y_ptr[1];
			dst[3] = V_ptr[0];
			dst[4] = Y_ptr[2];
			dst[5] = U_ptr[1];
			dst[6] = Y_ptr[3];
			dst[7] = V_ptr[1];
			dst[8] = Y_ptr[4];
			dst[9] = U_ptr[2];
			dst[10] = Y_ptr[5];
			dst[11] = V_ptr[2];
			dst[12] = Y_ptr[6];
			dst[13] = U_ptr[3];
			dst[14] = Y_ptr[7];
			dst[15] = V_ptr[3];
			dst[16] = Y_ptr[8];
			dst[17] = U_ptr[4];
			dst[18] = Y_ptr[9];
			dst[19] = V_ptr[4];
			dst[20] = Y_ptr[10];
			dst[21] = U_ptr[5];
			dst[22] = Y_ptr[11];
			dst[23] = V_ptr[5];
			dst[24] = Y_ptr[12];
			dst[25] = U_ptr[6];
			dst[26] = Y_ptr[13];
			dst[27] = V_ptr[6];
			dst[28] = Y_ptr[14];
			dst[29] = U_ptr[7];
			dst[30] = Y_ptr[15];
			dst[31] = V_ptr[7];
			dst += 32;
			U_ptr += 8;
			V_ptr += 8;
			Y_ptr += 16;
		} while(--wt);

		U_ptr -= w*8;
		V_ptr -= w*8;
		Y_ptr += y_pitch - w*16;
		
		wt = w;
		do {
			volatile char z = dst[31];
			dst[0] = Y_ptr[0];
			dst[1] = U_ptr[0];
			dst[2] = Y_ptr[1];
			dst[3] = V_ptr[0];
			dst[4] = Y_ptr[2];
			dst[5] = U_ptr[1];
			dst[6] = Y_ptr[3];
			dst[7] = V_ptr[1];
			dst[8] = Y_ptr[4];
			dst[9] = U_ptr[2];
			dst[10] = Y_ptr[5];
			dst[11] = V_ptr[2];
			dst[12] = Y_ptr[6];
			dst[13] = U_ptr[3];
			dst[14] = Y_ptr[7];
			dst[15] = V_ptr[3];
			dst[16] = Y_ptr[8];
			dst[17] = U_ptr[4];
			dst[18] = Y_ptr[9];
			dst[19] = V_ptr[4];
			dst[20] = Y_ptr[10];
			dst[21] = U_ptr[5];
			dst[22] = Y_ptr[11];
			dst[23] = V_ptr[5];
			dst[24] = Y_ptr[12];
			dst[25] = U_ptr[6];
			dst[26] = Y_ptr[13];
			dst[27] = V_ptr[6];
			dst[28] = Y_ptr[14];
			dst[29] = U_ptr[7];
			dst[30] = Y_ptr[15];
			dst[31] = V_ptr[7];
			dst += 32;
			U_ptr += 8;
			V_ptr += 8;
			Y_ptr += 16;
		} while(--wt);

		U_ptr += uv_pitch - w*8;
		V_ptr += uv_pitch - w*8;
		Y_ptr += y_pitch - w*16;
	} while(--h);
}

//////////////////////////////////////////////////////////////

#pragma warning(push)
#pragma warning(disable: 4799)		// function has no EMMS instruction

void video_copy_forward(int x_pos, int y_pos) {
	YUVPixel *Y_src, *U_src, *V_src;

	Y_src = Y_forw + 16*x_pos +  y_pitch*(16*y_pos);
	U_src = U_forw +  8*x_pos + uv_pitch*( 8*y_pos);
	V_src = V_forw +  8*x_pos + uv_pitch*( 8*y_pos);

	// luminance

	if (MMX_enabled)
		__asm {
			push esi
			push edi
			mov esi,y_pitch

			mov ecx,Y_src
			mov edx,dstY
			mov edi,8

		loop_Y_MMX:
			movq	mm0,[ecx]
			movq	mm1,[ecx+8]
			movq	[edx],mm0
			movq	mm2,[ecx+esi]
			movq	[edx+8],mm1
			movq	mm3,[ecx+esi+8]
			movq	[edx+esi],mm2
			movq	[edx+esi+8],mm3
			lea		ecx,[ecx+esi*2]
			dec		edi
			lea		edx,[edx+esi*2]
			jne		loop_Y_MMX

			mov ecx,U_src
			mov edx,dstU
			mov esi,uv_pitch
			mov edi,4

		loop_U_MMX:
			movq	mm0,[ecx]
			movq	mm1,[ecx+esi]
			movq	[edx],mm0
			movq	[edx+esi],mm1
			lea		ecx,[ecx+esi*2]
			dec		edi
			lea		edx,[edx+esi*2]
			jne		loop_U_MMX

			mov ecx,V_src
			mov edx,dstV
			mov edi,4

		loop_V_MMX:
			movq	mm0,[ecx]
			movq	mm1,[ecx+esi]
			movq	[edx],mm0
			movq	[edx+esi],mm1
			lea		ecx,[ecx+esi*2]
			dec		edi
			lea		edx,[edx+esi*2]
			jne		loop_V_MMX

			pop edi
			pop esi
		}
	else
		__asm {
			push esi
			push edi
			mov esi,y_pitch

			mov ecx,Y_src
			mov edx,dstY
			mov edi,16

		loop_Y:
			mov eax,[ecx]
			mov ebx,[ecx+4]
			mov [edx],eax
			mov [edx+4],ebx

			mov eax,[ecx+8]
			mov ebx,[ecx+12]
			mov [edx+8],eax
			mov [edx+12],ebx

			add ecx,esi
			add edx,esi

			dec edi
			jne loop_Y

			mov ecx,U_src
			mov edx,dstU
			mov esi,uv_pitch
			mov edi,8

		loop_U:
			mov eax,[ecx]
			mov ebx,[ecx+4]
			mov [edx],eax
			mov [edx+4],ebx

			add ecx,esi
			add edx,esi

			dec edi
			jne loop_U

			mov ecx,V_src
			mov edx,dstV
			mov edi,8

		loop_V:
			mov eax,[ecx]
			mov ebx,[ecx+4]
			mov [edx],eax
			mov [edx+4],ebx

			add ecx,esi
			add edx,esi

			dec edi
			jne loop_V

			pop edi
			pop esi
		}
}

#pragma warning(pop)

static void video_copy_forward_prediction(int x_pos, int y_pos, bool fchrom) {
	long vx = forw_vector_x;
	long vy = forw_vector_y;
	long vxY, vyY, vxC, vyC;

	if (forw_vector_full_pel) {
		vx <<= 1;
		vy <<= 1;
	}

	vxY = vx + 32*x_pos;
	vyY = vy + 32*y_pos;
	vxC = vx/2 + 16*x_pos;
	vyC = vy/2 + 16*y_pos;

	if (vxY<0 || vyY<0 || vxY>vector_limit_x || vyY>vector_limit_y)
		vxY = vyY = vxC = vyC = 0;

	if (!fchrom)
		video_copy_prediction_Y(Y_forw, dstY, vxY, vyY, y_pitch);
	else {
		video_copy_prediction_C(U_forw, dstU, vxC, vyC, uv_pitch);
		video_copy_prediction_C(V_forw, dstV, vxC, vyC, uv_pitch);
	}

}

static void video_copy_backward_prediction(int x_pos, int y_pos, bool fchrom) {
	long vx = back_vector_x;
	long vy = back_vector_y;
	long vxY, vyY, vxC, vyC;

	if (back_vector_full_pel) {
		vx <<= 1;
		vy <<= 1;
	}

	vxY = vx + 32*x_pos;
	vyY = vy + 32*y_pos;
	vxC = vx/2 + 16*x_pos;
	vyC = vy/2 + 16*y_pos;

	if (vxY<0 || vyY<0 || vxY>vector_limit_x || vyY>vector_limit_y)
		vxY = vyY = vxC = vyC = 0;

	if (!fchrom)
		video_copy_prediction_Y(Y_back, dstY, vxY, vyY, y_pitch);
	else {
		video_copy_prediction_C(U_back, dstU, vxC, vyC, uv_pitch);
		video_copy_prediction_C(V_back, dstV, vxC, vyC, uv_pitch);
	}
}

static void video_add_backward_prediction(int x_pos, int y_pos, bool fchrom) {
	long vx = back_vector_x;
	long vy = back_vector_y;
	long vxY, vyY, vxC, vyC;

	if (back_vector_full_pel) {
		vx <<= 1;
		vy <<= 1;
	}

	vxY = vx + 32*x_pos;
	vyY = vy + 32*y_pos;
	vxC = vx/2 + 16*x_pos;
	vyC = vy/2 + 16*y_pos;

	if (vxY<0 || vyY<0 || vxY>vector_limit_x || vyY>vector_limit_y)
		vxY = vyY = vxC = vyC = 0;

	if (!fchrom)
		video_add_prediction_Y(Y_back, dstY, vxY, vyY, y_pitch);
	else {
		video_add_prediction_C(U_back, dstU, vxC, vyC, uv_pitch);
		video_add_prediction_C(V_back, dstV, vxC, vyC, uv_pitch);
	}
}

///////////////////////////////////////////////////////////////////////////

#if 0
#ifdef _DEBUG

class MPEGDecoderVerifier {
private:
	__declspec(align(16)) YUVPixel src[32][32], dst[32][32], src2[32][32];
	__declspec(align(16)) int err[2][2][2];

	void rnd();
	int checkpred(int vx, int vy, int w, int h, bool);
public:
	MPEGDecoderVerifier();
} g_VerifyMPEGDecoder;

void MPEGDecoderVerifier::rnd() {
	int i,j;

	for(j=0; j<32; j++) {
		for(i=0; i<32; i++) {
			src[j][i] = (YUVPixel)rand();
		}
	}
}

int MPEGDecoderVerifier::checkpred(int vx, int vy, int w, int h, bool add) {
	YUVPixel p1, p2, p3, p4;
	int r;
	int i, j;
	int e, sum=0;

	for(j=0; j<h; j++)
		for(i=0; i<w; i++) {
			p1 = src[j+((vy+0)>>1)][i+((vx+0)>>1)];
			p2 = src[j+((vy+0)>>1)][i+((vx+1)>>1)];
			p3 = src[j+((vy+1)>>1)][i+((vx+0)>>1)];
			p4 = src[j+((vy+1)>>1)][i+((vx+1)>>1)];

			r = ((int)p1 + (int)p2 + (int)p3 + (int)p4 + 2)/4;

			if (add)
				r = (r + src2[j][i] + 1)/2;

			e = abs(r - (int)dst[j][i]);

			if (e>1)
				throw MyError("Predictor verify error in MPEG decoder with vector %d,%d!", vx, vy);

			sum += e;
		}

	return sum;
}

MPEGDecoderVerifier::MPEGDecoderVerifier() {
	try {
		int i,j;

		for(j=0; j<32; j++) {
			for(i=0; i<32; i++) {
				src[j][i] = (YUVPixel)rand();
				src2[j][i] = (YUVPixel)rand();
			}
		}

		CPUCheckForExtensions();
		CPUEnableExtensions(0);

		do {
			_RPT3(0,"MPEG-1 decoder: testing prediction copy (MMX %s / ISSE %s / SSE2 %s)\n", MMX_enabled ? "on" : "off", ISSE_enabled ? "on" : "off", SSE2_enabled ? "on" : "off");

			video_copy_prediction_Y = video_copy_prediction_Y_scalar;
			video_add_prediction_Y = video_add_prediction_Y_scalar;
			video_copy_prediction_C = video_copy_prediction_C_scalar;
			video_add_prediction_C = video_add_prediction_C_scalar;
			if (MMX_enabled) {
				video_copy_prediction_Y = video_copy_prediction_Y_MMX;
				video_add_prediction_Y = video_add_prediction_Y_MMX;
				video_copy_prediction_C = video_copy_prediction_C_MMX;
				video_add_prediction_C = video_add_prediction_C_MMX;
			}
			if (ISSE_enabled) {
				video_copy_prediction_Y = video_copy_prediction_Y_ISSE;
				video_add_prediction_Y = video_add_prediction_Y_ISSE;
				video_copy_prediction_C = video_copy_prediction_C_ISSE;
				video_add_prediction_C = video_add_prediction_C_ISSE;
			}
			if (SSE2_enabled) {
				video_copy_prediction_Y = video_copy_prediction_Y_SSE2;
				video_add_prediction_Y = video_add_prediction_Y_SSE2;
			}

			memset(err, 0, sizeof err);
			for(j=0; j<16; j++) {
				for(i=0; i<16; i++) {
					video_copy_prediction_Y(&src[0][0], &dst[0][0], i, j, 32);
					err[0][j&1][i&1] += checkpred(i, j, 16, 16, false);
					video_copy_prediction_C(&src[0][0], &dst[0][0], i, j, 32);
					err[1][j&1][i&1] += checkpred(i, j, 8, 8, false);
				}
			}

			if (MMX_enabled)
				__asm emms

			_RPT2(0,"full/full: average error %.4lf, %.4lf\n", (double)err[0][0][0] / (16.0*16.0*256.0), (double)err[1][0][0] / (16.0*16.0*64.0));
			_RPT2(0,"half/full: average error %.4lf, %.4lf\n", (double)err[0][0][1] / (16.0*16.0*256.0), (double)err[1][0][1] / (16.0*16.0*64.0));
			_RPT2(0,"full/half: average error %.4lf, %.4lf\n", (double)err[0][1][0] / (16.0*16.0*256.0), (double)err[1][1][0] / (16.0*16.0*64.0));
			_RPT2(0,"half/half: average error %.4lf, %.4lf\n", (double)err[0][1][1] / (16.0*16.0*256.0), (double)err[1][1][1] / (16.0*16.0*64.0));

			_RPT3(0,"MPEG-1 decoder: testing prediction add (MMX %s / ISSE %s / SSE2 %s)\n", MMX_enabled ? "on" : "off", ISSE_enabled ? "on" : "off", SSE2_enabled ? "on" : "off");

			memset(err, 0, sizeof err);
			for(j=0; j<16; j++) {
				for(i=0; i<16; i++) {
					memcpy(dst, src2, sizeof src2);
					video_add_prediction_Y(&src[0][0], &dst[0][0], i, j, 32);
					err[0][j&1][i&1] += checkpred(i, j, 16, 16, true);
					memcpy(dst, src2, sizeof src2);
					video_add_prediction_C(&src[0][0], &dst[0][0], i, j, 32);
					err[1][j&1][i&1] += checkpred(i, j, 8, 8, true);
				}
			}

			if (MMX_enabled)
				__asm emms

			_RPT2(0,"full/full: average error %.4lf, %.4lf\n", (double)err[0][0][0] / (16.0*16.0*256.0), (double)err[1][0][0] / (16.0*16.0*64.0));
			_RPT2(0,"half/full: average error %.4lf, %.4lf\n", (double)err[0][0][1] / (16.0*16.0*256.0), (double)err[1][0][1] / (16.0*16.0*64.0));
			_RPT2(0,"full/half: average error %.4lf, %.4lf\n", (double)err[0][1][0] / (16.0*16.0*256.0), (double)err[1][1][0] / (16.0*16.0*64.0));
			_RPT2(0,"half/half: average error %.4lf, %.4lf\n", (double)err[0][1][1] / (16.0*16.0*256.0), (double)err[1][1][1] / (16.0*16.0*64.0));

			CPUEnableExtensions(CPUGetAvailableExtensions() & (MMX_enabled ? ISSE_enabled ? SSE2_enabled ? 0 : CPUF_SUPPORTS_MMX|CPUF_SUPPORTS_INTEGER_SSE|CPUF_SUPPORTS_SSE2 : CPUF_SUPPORTS_MMX|CPUF_SUPPORTS_INTEGER_SSE : CPUF_SUPPORTS_MMX));
		} while(MMX_enabled || ISSE_enabled || SSE2_enabled);
	} catch(const MyError& e) {
		e.post(NULL, "MPEG-1 decoder verification error");
	}
}

#endif
#endif
