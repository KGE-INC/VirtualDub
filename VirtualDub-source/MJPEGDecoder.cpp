//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2000 Avery Lee
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

#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>
#include <math.h>
#include <crtdbg.h>
#include <windows.h>
#include <conio.h>

#include "MJPEGDecoder.h"

//#define DCTLEN_PROFILE
//#define PROFILE



#ifdef DCTLEN_PROFILE
extern "C" {
	long short_coeffs, med_coeffs, long_coeffs;
};
#endif

///////////////////////////////////////////////////////////////////////////
//
//		Externs
//
///////////////////////////////////////////////////////////////////////////

typedef unsigned char byte;
typedef unsigned long dword;

class MJPEGBlockDef {
public:
	const byte *huff_dc;
	const byte *huff_ac;
	const byte (*huff_ac_quick)[2];
	const byte (*huff_ac_quick2)[2];
	const int *quant;
	int *dc_ptr;
	int	ac_last;
};
extern "C" void asm_mb_decode(dword& bitbuf, int& bitcnt, byte *& ptr, int mcu_length, MJPEGBlockDef *pmbd, short **dctarray);

extern "C" void IDCT_mmx(signed short *dct_coeff, void *dst, long pitch, int intra_flag, int ac_last);

///////////////////////////////////////////////////////////////////////////
//
//		Tables
//
///////////////////////////////////////////////////////////////////////////

static const char MJPEG_zigzag[64] = {		// the reverse zigzag scan order
		 0,  1,  8, 16,  9,  2,  3, 10,
		17, 24, 32, 25, 18, 11,  4,  5,
		12, 19, 26, 33, 40, 48, 41, 34,
		27, 20, 13,  6,  7, 14, 21, 28,
		35, 42, 49, 56, 57, 50, 43, 36,
		29, 22, 15, 23, 30, 37, 44, 51,
		58, 59, 52, 45, 38, 31, 39, 46,
		53, 60, 61, 54, 47, 55, 62, 63,
};

// Huffman tables

static const byte huff_dc_0[] = {	// DC table 0
	0x00,0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,	// counts by bit length
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,	// values
};

static const byte huff_dc_1[] = {	// DC table 1
	0x00,0x03,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,0x00,
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,
};

static const byte huff_ac_0_quick[][2]={
#if 0
	0x01,2,	// 0000-3FFF
	0x01,2,
	0x01,2,
	0x01,2,
	0x02,2,	// 4000-7FFF
	0x02,2,
	0x02,2,
	0x02,2,
	0x03,3,	// 8000-9FFF
	0x03,3,
	0x00,4,	// A000-AFFF
#endif

/* 00-0F */ 0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,
/* 10-1F */ 0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,
/* 20-2F */ 0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,
/* 30-3F */ 0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,
/* 40-4F */ 0xFD,4,0xFD,4,0xFD,4,0xFD,4,0xFD,4,0xFD,4,0xFD,4,0xFD,4,
/* 50-5F */ 0xFE,4,0xFE,4,0xFE,4,0xFE,4,0xFE,4,0xFE,4,0xFE,4,0xFE,4,
/* 60-6F */ 0x02,4,0x02,4,0x02,4,0x02,4,0x02,4,0x02,4,0x02,4,0x02,4,
/* 70-7F */	0x03,4,0x03,4,0x03,4,0x03,4,0x03,4,0x03,4,0x03,4,0x03,4,
/* 80-8F */ 0xF9,6,0xF9,6,0xFA,6,0xFA,6,0xFB,6,0xFB,6,0xFC,6,0xFC,6,
/* 90-9F */ 0x04,6,0x04,6,0x05,6,0x05,6,0x06,6,0x06,6,0x07,6,0x07,6,
/* A0-AF */	0x00,4,0x00,4,0x00,4,0x00,4,0x00,4,0x00,4,0x00,4,0x00,4,
};

static byte huff_ac_0_quick2[0x1000 - 0xB00][2];

static const byte huff_ac_0[]={		// AC table 0
//	0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7D,	// 0xe2 values

/*
	0x01,0x02,					// (00-01) 0000-7FFF
	0x03,						// (02)    8000-9FFF
	0x00,0x04,0x11,				// (03-05) A000-CFFF
	0x05,0x12,0x21,				// (06-08) D000-E7FF
	0x31,0x41,					// (09-0A) E800-EFFF
	0x06,0x13,0x51,0x61,		// (0B-0E) F000-F7FF
	0x07,0x22,0x71,				// (0F-11) F800-FAFF
	0x14,0x32,0x81,0x91,0xA1,	// (12-16) FB00-FD7F
	0x08,0x23,0x42,0xB1,0xC1,	// (17-1B) FD80-FEBF
	0x15,0x52,0xD1,0xF0,		// (1C-1F) FEC0-FF3F
	0x24,0x33,0x62,0x72,		// (20-23) FF40-FF7F
*/
	0x82,15,
	0x82,15,

	0x09,16,0x0A,16,0x16,16,0x17,16,0x18,16,0x19,16,0x1A,16,0x25,16,0x26,16,0x27,16,0x28,16,0x29,16,0x2A,16,0x34,16,0x35,16,0x36,16,
	0x37,16,0x38,16,0x39,16,0x3A,16,0x43,16,0x44,16,0x45,16,0x46,16,0x47,16,0x48,16,0x49,16,0x4A,16,0x53,16,0x54,16,0x55,16,0x56,16,
	0x57,16,0x58,16,0x59,16,0x5A,16,0x63,16,0x64,16,0x65,16,0x66,16,0x67,16,0x68,16,0x69,16,0x6A,16,0x73,16,0x74,16,0x75,16,0x76,16,
	0x77,16,0x78,16,0x79,16,0x7A,16,0x83,16,0x84,16,0x85,16,0x86,16,0x87,16,0x88,16,0x89,16,0x8A,16,0x92,16,0x93,16,0x94,16,0x95,16,
	0x96,16,0x97,16,0x98,16,0x99,16,0x9A,16,0xA2,16,0xA3,16,0xA4,16,0xA5,16,0xA6,16,0xA7,16,0xA8,16,0xA9,16,0xAA,16,0xB2,16,0xB3,16,
	0xB4,16,0xB5,16,0xB6,16,0xB7,16,0xB8,16,0xB9,16,0xBA,16,0xC2,16,0xC3,16,0xC4,16,0xC5,16,0xC6,16,0xC7,16,0xC8,16,0xC9,16,0xCA,16,
	0xD2,16,0xD3,16,0xD4,16,0xD5,16,0xD6,16,0xD7,16,0xD8,16,0xD9,16,0xDA,16,0xE1,16,0xE2,16,0xE3,16,0xE4,16,0xE5,16,0xE6,16,0xE7,16,
	0xE8,16,0xE9,16,0xEA,16,0xF1,16,0xF2,16,0xF3,16,0xF4,16,0xF5,16,0xF6,16,0xF7,16,0xF8,16,0xF9,16,0xFA,16,
};

static const byte huff_ac_0_src[]={		// AC table 0
	0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,0x05,0x04,0x04,0x00,0x00,0x01,0x7D,	// 0xe2 values

	0x01,0x02,					// (00-01) 0000-7FFF
	0x03,						// (02)    8000-9FFF
	0x00,0x04,0x11,				// (03-05) A000-CFFF
	0x05,0x12,0x21,				// (06-08) D000-E7FF
	0x31,0x41,					// (09-0A) E800-EFFF
	0x06,0x13,0x51,0x61,		// (0B-0E) F000-F7FF
	0x07,0x22,0x71,				// (0F-11) F800-FAFF
	0x14,0x32,0x81,0x91,0xA1,	// (12-16) FB00-FD7F
	0x08,0x23,0x42,0xB1,0xC1,	// (17-1B) FD80-FEBF
	0x15,0x52,0xD1,0xF0,		// (1C-1F) FEC0-FF3F
	0x24,0x33,0x62,0x72,		// (20-23) FF40-FF7F
};

static const byte huff_ac_1_quick[][2]={
#if 0
	0x00,2,	// 0000-0FFF
	0x00,2,	// 1000-1FFF
	0x00,2,	// 2000-2FFF
	0x00,2,	// 3000-3FFF
	0x01,2,	// 4000-4FFF
	0x01,2,	// 5000-5FFF
	0x01,2,	// 6000-6FFF
	0x01,2,	// 7000-7FFF
	0x02,3,	// 8000-8FFF
	0x02,3,	// 9000-9FFF
	0x03,4,	// A000-AFFF
#endif

/* 00-0F */ 0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,
/* 10-1F */ 0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,
/* 20-2F */ 0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,
/* 30-3F */ 0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,0x00,2,
/* 40-4F */ 0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,
/* 50-5F */ 0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,0xFF,3,
/* 60-6F */ 0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,
/* 70-7F */ 0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,0x01,3,
/* 80-8F */ 0xFD,5,0xFD,5,0xFD,5,0xFD,5,0xFE,5,0xFE,5,0xFE,5,0xFE,5,
/* 90-9F */ 0x02,5,0x02,5,0x02,5,0x02,5,0x03,5,0x03,5,0x03,5,0x03,5,
/* A0-AF */	0xF9,7,0xFA,7,0xFB,7,0xFC,7,0x04,7,0x05,7,0x06,7,0x07,7,
};

static const byte huff_ac_1[]={		// AC table 1
//	0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,0x00,0x01,0x02,0x77,

/*
	0x00,0x01,								// (00-01) 4000 0000-7FFF
	0x02,									// (02)    2000 8000-9FFF
	0x03,0x11,								// (03-04) 1000 A000-BFFF
	0x04,0x05,0x21,0x31,					// (05-08) 0800 C000-DFFF
	0x06,0x12,0x41,0x51,					// (09-0C) 0400 E000-EFFF
	0x07,0x61,0x71,							// (0D-0F) 0200 F000-F5FF
	0x13,0x22,0x32,0x81,					// (10-13) 0100 F600-F9FF
	0x08,0x14,0x42,0x91,0xA1,0xB1,0xC1,		// (14-1B) 0080 FA00-FD7F
	0x09,0x23,0x33,0x52,0xF0,				// (1C-20) 0040 FD80-FEBF
	0x15,0x62,0x72,0xD1,					// (21-24) 0020 FEC0-FF3F
	0x0A,0x16,0x24,0x34,					// (25-28) 0010 FF40-FF80
*/
	0xE1,14,
	0xE1,14,
	0xE1,14,
	0xE1,14,

	0x25,15,0x25,15,
	0xF1,15,0xF1,15,

	0x17,16,0x18,16,0x19,16,0x1A,16,0x26,16,0x27,16,0x28,16,0x29,16,0x2A,16,0x35,16,0x36,16,0x37,16,0x38,16,0x39,16,0x3A,16,0x43,16,
	0x44,16,0x45,16,0x46,16,0x47,16,0x48,16,0x49,16,0x4A,16,0x53,16,0x54,16,0x55,16,0x56,16,0x57,16,0x58,16,0x59,16,0x5A,16,0x63,16,
	0x64,16,0x65,16,0x66,16,0x67,16,0x68,16,0x69,16,0x6A,16,0x73,16,0x74,16,0x75,16,0x76,16,0x77,16,0x78,16,0x79,16,0x7A,16,0x82,16,
	0x83,16,0x84,16,0x85,16,0x86,16,0x87,16,0x88,16,0x89,16,0x8A,16,0x92,16,0x93,16,0x94,16,0x95,16,0x96,16,0x97,16,0x98,16,0x99,16,
	0x9A,16,0xA2,16,0xA3,16,0xA4,16,0xA5,16,0xA6,16,0xA7,16,0xA8,16,0xA9,16,0xAA,16,0xB2,16,0xB3,16,0xB4,16,0xB5,16,0xB6,16,0xB7,16,
	0xB8,16,0xB9,16,0xBA,16,0xC2,16,0xC3,16,0xC4,16,0xC5,16,0xC6,16,0xC7,16,0xC8,16,0xC9,16,0xCA,16,0xD2,16,0xD3,16,0xD4,16,0xD5,16,
	0xD6,16,0xD7,16,0xD8,16,0xD9,16,0xDA,16,0xE2,16,0xE3,16,0xE4,16,0xE5,16,0xE6,16,0xE7,16,0xE8,16,0xE9,16,0xEA,16,0xF2,16,0xF3,16,
	0xF4,16,0xF5,16,0xF6,16,0xF7,16,0xF8,16,0xF9,16,0xFA,16
};

static const byte huff_ac_1_src[]={		// AC table 1
	0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,0x04,0x04,0x00,0x01,0x02,0x77,

	0x00,0x01,								// (00-01) 4000 0000-7FFF
	0x02,									// (02)    2000 8000-9FFF
	0x03,0x11,								// (03-04) 1000 A000-BFFF
	0x04,0x05,0x21,0x31,					// (05-08) 0800 C000-DFFF
	0x06,0x12,0x41,0x51,					// (09-0C) 0400 E000-EFFF
	0x07,0x61,0x71,							// (0D-0F) 0200 F000-F5FF
	0x13,0x22,0x32,0x81,					// (10-13) 0100 F600-F9FF
	0x08,0x14,0x42,0x91,0xA1,0xB1,0xC1,		// (14-1B) 0080 FA00-FD7F
	0x09,0x23,0x33,0x52,0xF0,				// (1C-20) 0040 FD80-FEBF
	0x15,0x62,0x72,0xD1,					// (21-24) 0020 FEC0-FF3F
	0x0A,0x16,0x24,0x34,					// (25-28) 0010 FF40-FF80
};

static byte huff_ac_1_quick2[0x1000 - 0xB00][2];

static const byte *huff_dc[2] = { huff_dc_0, huff_dc_1 };
static const byte *huff_ac[2] = { huff_ac_0, huff_ac_1 };
static const byte *huff_ac_src[2] = { huff_ac_0_src, huff_ac_1_src };
static const byte (*huff_ac_quick[2])[2] = { huff_ac_0_quick, huff_ac_1_quick };
static const byte (*huff_ac_quick2[2])[2] = { huff_ac_0_quick2, huff_ac_1_quick2 };

///////////////////////////////////////////////////////////////////////////
//
//		Class definitions
//
///////////////////////////////////////////////////////////////////////////


class MJPEGDecoder : public IMJPEGDecoder {
public:
	MJPEGDecoder(int w, int h);
	~MJPEGDecoder();

	void decodeFrame16(dword *output, byte *input, int len);
	void decodeFrame32(dword *output, byte *input, int len);

private:
	int quant[4][128];				// quantization matrices
	int width, height, field_height;
	int mcu_width, mcu_height;		// size of frame when blocked into MCUs
	int mcu_length;
	int mcu_count;
	int mcu_size_y;
	int raw_width, raw_height;
	int clip_row, clip_lines;
	void *pixdst;

	int *comp_quant[3];
	int comp_mcu_x[3], comp_mcu_y[3], comp_mcu_length[4];
	int comp_last_dc[3];
	int comp_id[3];
	int comp_start[3];

	MJPEGBlockDef blocks[24];
	short dct_coeff[24][64];
	short *dct_coeff_ptrs[24];

	bool vc_half;					// chrominance 2:1 vertically?
	bool interlaced;
	bool decode16;

	void decodeFrame(dword *output, byte *input, int len);
	byte *decodeQuantTables(byte *psrc);
	byte *decodeFrameInfo(byte *psrc);
	byte *decodeScan(byte *ptr, bool odd_field);
	byte __forceinline huffDecodeDC(dword& bitbuf, int& bitcnt, const byte * const table);
	byte __forceinline huffDecodeAC(dword& bitbuf, int& bitcnt, const byte * const table);
	byte *decodeMCUs(byte *ptr, bool odd_field);
};

enum {
	MARKER_SOF0	= 0xc0,		// start-of-frame, baseline scan
	MARKER_SOI	= 0xd8,		// start of image
	MARKER_EOI	= 0xd9,		// end of image
	MARKER_SOS	= 0xda,		// start of scan
	MARKER_DQT	= 0xdb,		// define quantization tables
	MARKER_APP_FIRST	= 0xe0,
	MARKER_APP_LAST		= 0xef,
	MARKER_COMMENT		= 0xfe,
};

///////////////////////////////////////////////////////////////////////////
//
//		Construction/destruction
//
///////////////////////////////////////////////////////////////////////////

MJPEGDecoder::MJPEGDecoder(int w, int h) {
	this->vc_half			= false;
	this->width				= w;
	this->height			= h;

	for(int tbl=0; tbl<2; tbl++) {
		int base=0;
		byte *ptr = (byte *)huff_ac_quick2[tbl];
		const byte *countptr = huff_ac_src[tbl];
		const byte *codeptr = huff_ac_src[tbl] + 16;

		for(int bits=1; bits<=12; bits++) {
			for(int cnt=0; cnt<*countptr; cnt++) {
				int first, last;

				first = base;
				last = base + (0x1000 >> bits);

				if (first < 0xB00)
					first = 0xB00;

				while(first < last) {
					*ptr++ = *codeptr;
					*ptr++ = bits;
					++first;
				}

				base = last;

				++codeptr;
			}

			++countptr;
		}

		_RPT2(0,"Code length for table %d: %04x\n", tbl, base);
	}
}

MJPEGDecoder::~MJPEGDecoder() {
}

IMJPEGDecoder *CreateMJPEGDecoder(int w, int h) {
	return new MJPEGDecoder(w, h);
}

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////


int __inline getshort(byte *p) {
	return ((int)p[0]<<8) + (int)p[1];
}



void MJPEGDecoder::decodeFrame16(dword *output, byte *ptr, int size) {
	decode16 = true;
	decodeFrame(output, ptr, size);
}

void MJPEGDecoder::decodeFrame32(dword *output, byte *ptr, int size) {
	decode16 = false;
	decodeFrame(output, ptr, size);
}

void MJPEGDecoder::decodeFrame(dword *output, byte *ptr, int size) {
	byte *limit = ptr+size-1;
	byte tag;
	bool odd_field = true;
	int field_count = 0;

	do {
//		_RPT1(0,"Decoding %s field\n", odd_field ? "odd" : "even");

		// scan for SOI tag

		while(ptr < limit)
			if (*ptr++ == 0xff)
				if ((tag = *ptr++) == MARKER_SOI)
					break;
				else if (tag == 0xff)
					while(ptr<limit && *ptr == 0xff)
						++ptr;
				else {
//					_RPT0(0,"Error: markers found before SOI tag\n");
//					return;
					break;		// happens with dmb1
				}

		if (ptr >= limit) {
//			_RPT0(0,"Error: SOI mark not found\n");
			return;
		}

		// parse out chunks

		while(ptr < limit) {
			if (*ptr++ == 0xff)
				switch(tag = *ptr++) {
				case MARKER_EOI:
//					_RPT1(0,"Note: EOI tag found at %p\n", ptr-2);
					goto next_field;
				case MARKER_DQT:
					ptr = decodeQuantTables(ptr);
					break;
				case MARKER_SOF0:
					ptr = decodeFrameInfo(ptr);

					// dmb1 thinks it's interlaced all the time...

					if (raw_height*2 > height)
						interlaced = false;

					break;
				case MARKER_SOS:
					pixdst = output;
					ptr = decodeScan(ptr, odd_field);
//					_RPT1(0,"scan decode finished at %p\n", ptr);
					break;
				case MARKER_APP_FIRST:
					interlaced = (ptr[6] != 0);
					odd_field = (ptr[6] > 1);
					field_height = interlaced ? height/2 : height;
					ptr += getshort(ptr);
					break;
				case 0xff:
					while(ptr<limit && *ptr == 0xff)
						++ptr;
					break;
				case 0:
					break;
				default:
					if ((tag >= MARKER_APP_FIRST && tag <= MARKER_APP_LAST) || tag == MARKER_COMMENT) {

						ptr += getshort(ptr);
						break;
					}
//					_RPT1(0,"Warning: Unknown tag %02x\n", tag);
				}
		}
next_field:
		;
	} while(interlaced && field_count<2);

//	_RPT0(0,"Warning: No EOI tag found\n");
}

byte *MJPEGDecoder::decodeQuantTables(byte *psrc) {
	int *dst;
	int n;

	psrc += 2;	// skip length
	while(*psrc != 0xff) {
		n = psrc[0] & 15;
		if (n>3)
			throw "Error: Illegal quantization table # in DQT chunk";

		dst = quant[n];
		++psrc;

		// We have to swap around the zigzag order so that the order
		// of rows is: 0, 4, 1, 5, 2, 6, 3, 7.

		if (psrc[-1] & 0xf0) {
			// 16-bit quantization tables

			for(n=0; n<64; n++) {
				dst[n*2+0] = getshort(psrc + n*2);
				dst[n*2+1] = ((MJPEG_zigzag[n] & 56) | ((MJPEG_zigzag[n]&3)<<1) | ((MJPEG_zigzag[n]&4)>>2))*2;
			}
			psrc += 128;
		} else {
			// 8-bit quantization tables

			for(n=0; n<64; n++) {
				dst[n*2+0] = psrc[n];
				dst[n*2+1] = ((MJPEG_zigzag[n] & 56) | ((MJPEG_zigzag[n]&3)<<1) | ((MJPEG_zigzag[n]&4)>>2))*2;
			}
			psrc += 64;
		}

//		MJPEG_IDCT_norm(dst);
	}

	return psrc;
}

byte *MJPEGDecoder::decodeFrameInfo(byte *psrc) {
	int i, n;

	if (psrc[2] != 8)
		throw "Can only decode 8-bit images";

	raw_height = getshort(psrc + 3);
	raw_width = getshort(psrc + 5);

	if (psrc[7] != 3)
		throw "Error: picture must be 3 component (YCC)";

	// parse component data

//	if (psrc[8] != 0)
//		throw "Error: first component must be 0";

//	if (psrc[11] != 1)
//		throw "Error: second component must be 1";

//	if (psrc[14] != 2)
//		throw "Error: third component must be 2";

	if (psrc[12] != psrc[15])
		throw "Error: chrominance subsampling factors must be the same";

	mcu_length = 0;
	for(i=0; i<3; i++) {
		n = psrc[10 + 3*i];
		if (n>3)
			throw "Error: component specifies quantization table other than 0-3";

//		_RPT2(0,"Component %d uses quant %d\n", i, n);

		comp_quant[i] = quant[n];
		comp_mcu_x[i] = psrc[9 + 3*i] >> 4;
		comp_mcu_y[i] = psrc[9 + 3*i] & 15;
		comp_mcu_length[i] = comp_mcu_x[i] * comp_mcu_y[i];
		comp_id[i] = psrc[8 + 3*i];
		comp_start[i] = mcu_length;

		mcu_length += comp_mcu_length[i];
	}

	if (mcu_length > 10)
		throw "Error: macroblocks per MCU > 10";

	if (comp_mcu_x[0] != 2 || comp_mcu_x[1] != 1)
		throw "Error: horizontal chrominance subsampling must be 2:1";

	if ((comp_mcu_x[0] != 2 && comp_mcu_x[0] != 1) || comp_mcu_y[1] != 1)
		throw "Error: vertical chrominance subsampling must be 1:1 or 2:1";

	if (comp_mcu_y[0] == 2)
		vc_half = true;

	mcu_width	= (raw_width + 15)/16;

	if (vc_half)
		mcu_height	= (raw_height + 15)/16;
	else
		mcu_height	= (raw_height + 7)/8;

	mcu_count = mcu_width * mcu_height;
	mcu_size_y = comp_mcu_y[0] * 8;

	if (vc_half) {
		if (mcu_height*16 > field_height) {
			mcu_height	= (field_height + 15)/16;
			clip_row = field_height >> 4;
			clip_lines = field_height & 15;
		}
	} else {
		if (mcu_height*8 > field_height) {
			mcu_height	= (field_height + 7)/8;
			clip_row = field_height >> 3;
			clip_lines = field_height & 7;
		}
	}

	return psrc + 8 + 3*3;
}

byte *MJPEGDecoder::decodeScan(byte *ptr, bool odd_field) {
	int mb=0;
	int i,j;

	// Ns (components in scan) must be 3

	if (ptr[2] != 3)
		throw "Error: scan must have 3 interleaved components";

	if (ptr[9] != 0 || ptr[10] != 63)
		throw "Error: DCT coefficients must run from 0-63";

	if (ptr[11] != 0)
		throw "Error: Successive approximation not allowed";

	// decode component order (indices 3, 5, 7)

//	if (ptr[3] != 0)
//		throw "Error: component 0 must be Y";

//	if (ptr[5] != 1)
//		throw "Error: component 0 must be Cr";

//	if (ptr[7] != 2)
//		throw "Error: component 0 must be Cb";


	// select entropy (Huffman) coders (indices 4, 6, 8)

	for(i=0; i<3; i++) {
		for(j=0; j<3; j++)
			if (ptr[3+2*i] == comp_id[j])
				break;

		if (j>=3)
			throw "Error: MJPEG scan has mislabeled component";

		mb = comp_start[j];

		for(j=0; j<comp_mcu_x[i]*comp_mcu_y[i]; j++) {
			blocks[mb].huff_dc	= huff_dc[ptr[4+2*i]>>4];
			blocks[mb].huff_ac	= huff_ac[ptr[4+2*i]&15];
			blocks[mb].huff_ac_quick = huff_ac_quick[ptr[4+2*i]&15];
			blocks[mb].huff_ac_quick2 = huff_ac_quick2[ptr[4+2*i]&15];
			blocks[mb].quant	= comp_quant[i];
			blocks[mb].dc_ptr	= &comp_last_dc[i];
			++mb;
		}

//		comp_last_dc[i] = 128*8;
	}

	for(i=0; i<mcu_length; i++) {
		blocks[i+mcu_length] = blocks[i];
		blocks[i+mcu_length*2] = blocks[i];
		blocks[i+mcu_length*3] = blocks[i];
		dct_coeff_ptrs[i] = &dct_coeff[i][0];
		dct_coeff_ptrs[i+mcu_length] = &dct_coeff[i+mcu_length][0];
		dct_coeff_ptrs[i+mcu_length*2] = &dct_coeff[i+mcu_length*2][0];
		dct_coeff_ptrs[i+mcu_length*3] = &dct_coeff[i+mcu_length*3][0];
	}

	comp_last_dc[0] = 128*8;
	comp_last_dc[1] = 0;
	comp_last_dc[2] = 0;

	ptr += 12;

	return decodeMCUs(ptr, odd_field);
}

// 320x240 -> 20x30 -> 600 MCUs
// 304x228 -> 19x29 -> 551 MCUs 

byte *MJPEGDecoder::decodeMCUs(byte *ptr, bool odd_field) {
	int mcu;
	dword bitbuf = 0;
	int bitcnt = 24;	// 24 - bits in buffer
	dword *pixptr = (dword *)pixdst;
	int mb_x = 0, mb_y = 0;
	long modulo0;
	long modulo1;
	long modulo2;
	long modulo3;
	long lines = 8;
	__int64 mb_cycles = 0;
	__int64 dct_cycles = 0;
	__int64 cvt_cycles = 0;

	pixptr += mcu_width*(decode16 ? 8 : 16) * (height - (vc_half?2:1));

	if (interlaced) {
		if (vc_half)
			pixptr -= mcu_width*(decode16 ? 8 : 16) *(odd_field?1:2);
		else if (!odd_field)
			pixptr -= mcu_width*(decode16 ? 8 : 16);
	}

	if (vc_half) {
		long bpr = mcu_width * 4 * (decode16 ? 8 : 16);

		if (interlaced) {
			modulo0 = 4*bpr + (decode16 ? 32 : 64);
			modulo1 = 2*bpr;
		} else {
			modulo0 = 2*bpr + (decode16 ? 32 : 64);
			modulo1 = bpr;
		}
	} else {
		modulo0 = (decode16 ? 1 : 2) * (mcu_width*(interlaced ? 64 : 32) + 16);
		modulo1 = (decode16 ? 1 : 2) * (mcu_width*(interlaced ? 512 : 256) + 16);
		modulo2 = 128 - 8;
		modulo3 = 0;
	}


	for(mcu=0; mcu<mcu_length*4; mcu++)
		memset(dct_coeff[mcu], 0, 128);

	for(mcu=0; mcu<mcu_count; mcu+=4) {
//	for(mcu=0; mcu<200; mcu++) {

		int mcus = 4;

		if (mcu >= mcu_count-4)
			mcus = mcu_count - mcu;

#ifdef PROFILE
		__asm {
			rdtsc
			sub dword ptr mb_cycles+0,eax
			sbb dword ptr mb_cycles+4,edx
		}
#endif

		asm_mb_decode(bitbuf, bitcnt, ptr, mcu_length*mcus, blocks, dct_coeff_ptrs);

#ifdef PROFILE
		__asm {
			rdtsc
			add dword ptr mb_cycles+0,eax
			adc dword ptr mb_cycles+4,edx
			sub dword ptr dct_cycles+0,eax
			sbb dword ptr dct_cycles+4,edx
		}
#endif

		for(int i=0; i<mcu_length*mcus; i++)
			IDCT_mmx(dct_coeff[i], dct_coeff[i], 16, 2, blocks[i].ac_last);

#ifdef PROFILE
		__asm {
			rdtsc
			add dword ptr dct_cycles+0,eax
			adc dword ptr dct_cycles+4,edx
			sub dword ptr cvt_cycles+0,eax
			sbb dword ptr cvt_cycles+4,edx
		}
#endif
		for(i=0; i<mcus; i++) {
			short *dct_coeffs = (short *)&dct_coeff[mcu_length * i];

			static const __int64 Cr_coeff = 0x0000005AFFD20000i64;
			static const __int64 Cb_coeff = 0x00000000FFEA0071i64;

			static const __int64 C_bias = 0x0000008000000080i64;
			static const __int64 C_bias2 = 0x0080008000800080i64;

			static const __int64 Cr_coeff_R = 0x005A005A005A005Ai64;
			static const __int64 Cr_coeff_G = 0xFFD2FFD2FFD2FFD2i64;

			static const __int64 CrCb_coeff_G = 0xFFD2FFEAFFD2FFEAi64;

			static const __int64 Cb_coeff_B = 0x0071007100710071i64;
			static const __int64 Cb_coeff_G = 0xFFEAFFEAFFEAFFEAi64;
			static const __int64 mask5		= 0xF8F8F8F8F8F8F8F8i64;

			static const __int64 G_const_1	= 0x7C007C007C007C00i64;
			static const __int64 G_const_2	= 0x7C007C007C007C00i64;
			static const __int64 G_const_3	= 0x03e003e003e003e0i64;

			if (vc_half) {
				if (!decode16)
				__asm {
					push		ebp
					push		edi
					push		esi

					mov			eax,dct_coeffs
					mov			edx,dword ptr pixptr

					push		lines
					push		modulo3
					push		modulo2
					push		modulo1
					push		modulo0

					mov			ebx,[esp + 4]

					mov			ecx,eax
					add			eax,512

					mov			ebp,2
	zloop420:
					mov			edi,[esp + 16]
					shr			edi,1
	yloop420:
					mov			esi,4
	xloop420:
					movq		mm0,[ecx]		;Y (0A,1A,2A,3A)
					pxor		mm7,mm7
					movq		mm1,[ecx+16]	;Y (0B,1B,2B,3B)
					movd		mm2,[eax+128]	;Cr
					movd		mm3,[eax]		;Cb
					movq		[ecx],mm7
					movq		[ecx+16],mm7
					movd		[eax],mm7
					movd		[eax+128],mm7
					psllw		mm0,6
					psllw		mm1,6
					punpcklwd	mm2,mm2
					punpcklwd	mm3,mm3
					movq		mm4,mm2
					movq		mm5,mm3
					pmullw		mm4,Cr_coeff_G
					pmullw		mm5,Cb_coeff_G
					pmullw		mm2,Cr_coeff_R
					pmullw		mm3,Cb_coeff_B
					paddw		mm4,mm5

					movq		mm5,mm0
					movq		mm6,mm0
					paddw		mm0,mm2
					paddw		mm5,mm4
					paddw		mm6,mm3
					psraw		mm0,6
					psraw		mm5,6
					psraw		mm6,6
					packuswb	mm0,mm0
					packuswb	mm5,mm5
					packuswb	mm6,mm6
					punpcklbw	mm6,mm0
					punpcklbw	mm5,mm5
					movq		mm0,mm6
					punpcklbw	mm0,mm5
					punpckhbw	mm6,mm5
					movq		[edx+ebx],mm0
					movq		[edx+ebx+8],mm6

					movq		mm5,mm1
					movq		mm7,mm1
					paddw		mm1,mm2
					paddw		mm5,mm4
					paddw		mm7,mm3
					psraw		mm1,6
					psraw		mm5,6
					psraw		mm7,6
					packuswb	mm1,mm1
					packuswb	mm5,mm5
					packuswb	mm7,mm7
					punpcklbw	mm7,mm1
					punpcklbw	mm5,mm5
					movq		mm1,mm7
					punpcklbw	mm1,mm5
					punpckhbw	mm7,mm5
					movq		[edx],mm1
					movq		[edx+8],mm7


					add			eax,4
					add			ecx,8
					add			edx,16

					test		esi,1
					jz			noblockskip32

					add			ecx,7*16
noblockskip32:

					dec			esi
					jne			xloop420

					sub			ecx,14*16
					sub			edx,dword ptr [esp + 0]		/* 2*bpr + 64 */

					dec			edi
					jne			yloop420

					add			ecx,16*8

					dec			ebp
					jne			zloop420

					add			esp,20

					pop			esi
					pop			edi
					pop			ebp
				}
				else
				__asm {
					push		ebp
					push		edi
					push		esi

					mov			eax,dct_coeffs
					mov			edx,dword ptr pixptr

					push		lines
					push		modulo3
					push		modulo2
					push		modulo1
					push		modulo0

					mov			ebx,[esp + 4]

					mov			ecx,eax
					add			eax,512

					mov			ebp,2
	zloop2420:
					mov			edi,[esp + 16]
					shr			edi,1
	yloop2420:
					mov			esi,4
	xloop2420:
					movq		mm0,[ecx]		;Y (0A,1A,2A,3A)
					pxor		mm7,mm7
					movq		mm1,[ecx+16]	;Y (0B,1B,2B,3B)
					movd		mm2,[eax+128]	;Cr
					movd		mm3,[eax]		;Cb
					movq		[ecx],mm7
					movq		[ecx+16],mm7
					movd		[eax],mm7
					movd		[eax+128],mm7
					psllw		mm0,6
					psllw		mm1,6
					punpcklwd	mm2,mm2
					punpcklwd	mm3,mm3
					movq		mm4,mm2
					movq		mm5,mm3
					pmullw		mm4,Cr_coeff_G
					pmullw		mm5,Cb_coeff_G
					pmullw		mm2,Cr_coeff_R
					pmullw		mm3,Cb_coeff_B
					paddw		mm4,mm5

					pxor		mm7,mm7
					movq		mm5,mm0
					movq		mm6,mm0
					paddw		mm0,mm2
					paddw		mm5,mm4
					paddw		mm6,mm3
					psraw		mm0,6
					psraw		mm5,6
					psraw		mm6,6
					packuswb	mm0,mm0
					packuswb	mm6,mm6
					packuswb	mm5,mm5
					pand		mm0,mask5
					pand		mm6,mask5
					pand		mm5,mask5
					psrlq		mm0,1
					psrlq		mm6,3
					punpcklbw	mm6,mm0
					punpcklbw	mm5,mm7
					psllq		mm5,2
					por			mm6,mm5
					movq		[edx+ebx],mm6

					pxor		mm0,mm0
					movq		mm5,mm1
					movq		mm7,mm1
					paddw		mm1,mm2
					paddw		mm5,mm4
					paddw		mm7,mm3
					psraw		mm1,6
					psraw		mm5,6
					psraw		mm7,6
					packuswb	mm1,mm1
					packuswb	mm5,mm5
					packuswb	mm7,mm7
					pand		mm1,mask5
					pand		mm7,mask5
					pand		mm5,mask5
					psrlq		mm1,1
					psrlq		mm7,3
					punpcklbw	mm7,mm1
					punpcklbw	mm5,mm0
					psllq		mm5,2
					por			mm7,mm5
					movq		[edx],mm7


					add			eax,4
					add			ecx,8
					add			edx,8

					test		esi,1
					jz			noblockskip232

					add			ecx,7*16
noblockskip232:

					dec			esi
					jne			xloop2420

					sub			ecx,14*16
					sub			edx,dword ptr [esp + 0]		/* 2*bpr + 64 */

					dec			edi
					jne			yloop2420

					add			ecx,16*8

					dec			ebp
					jne			zloop2420

					add			esp,20

					pop			esi
					pop			edi
					pop			ebp
				}
			} else {
				if (!decode16)
				__asm {
					push		ebp
					push		edi
					push		esi

					mov			eax,dct_coeffs
					mov			edx,dword ptr pixptr

					push		lines
					push		modulo3
					push		modulo2
					push		modulo1

					mov			ebx,modulo0

					mov			ecx,eax
					add			eax,256

					mov			ebp,2
	zloop:
					mov			edi,[esp + 12]
	yloop:
					mov			esi,2
	xloop:
					movd		mm4,[eax+128]	;Cr (0,1)
					pxor		mm1,mm1

					movd		mm6,[eax]		;Cb (0,1)
					punpcklwd	mm4,mm4

					movq		mm0,[ecx]		;Y (0,1,2,3)
					punpcklwd	mm6,mm6

					movd		[eax],mm1
					psllw		mm0,6

					movd		[eax+128],mm1
					movq		mm5,mm4

					movq		[ecx],mm1	
					movq		mm7,mm6
					punpckldq	mm4,mm4		;Cr (0,0,0,0)

					pmullw		mm4,Cr_coeff
					punpckhdq	mm5,mm5		;Cr (1,1,1,1)

					movq		mm2,mm0
					punpcklwd	mm0,mm0			;Y (0,0,1,1)

					pmullw		mm5,Cr_coeff
					punpckhwd	mm2,mm2			;Y (2,2,3,3)

					movq		mm1,mm0
					punpckldq	mm0,mm0			;Y (0,0,0,0)

					movq		mm3,mm2
					punpckhdq	mm1,mm1			;Y (1,1,1,1)

					punpckldq	mm6,mm6		;Cb (0,0,0,0)
					add			ecx,8

					pmullw		mm6,Cb_coeff
					punpckhdq	mm7,mm7		;Cb (1,1,1,1)

					pmullw		mm7,Cb_coeff
					punpckhdq	mm3,mm3			;Y (3,3,3,3)

					punpckldq	mm2,mm2			;Y (2,2,2,2)
					add			eax,4

					paddsw		mm4,mm6
					paddsw		mm0,mm4

					paddsw		mm5,mm7
					psraw		mm0,6

					paddsw		mm1,mm4
					paddsw		mm2,mm5

					psraw		mm1,6
					paddsw		mm3,mm5

					psraw		mm2,6
					packuswb	mm0,mm1

					psraw		mm3,6
					packuswb	mm2,mm3

					movq		[edx],mm0
					add			edx,16

					dec			esi
					movq		[edx-8],mm2
					jne			xloop

					sub			edx,ebx						/* 304*4 + 32 */
					add			eax,8

					dec			edi
					jne			yloop

					sub			eax,dword ptr [esp + 4]		/* 256-16 */
					add			edx,dword ptr [esp + 0]		/* 304*4*8 + 32 */
					add			ecx,dword ptr [esp + 8]

					dec			ebp
					jne			zloop

					add			esp,16

					pop			esi
					pop			edi
					pop			ebp
				}
				else
				__asm {
					push		ebp
					push		edi
					push		esi

					mov			eax,dct_coeffs
					mov			edx,dword ptr pixptr

					push		lines
					push		modulo3
					push		modulo2
					push		modulo1
					mov			ebx,modulo0

					mov			ecx,eax
					add			eax,256

					mov			ebp,2
	zloop2:
	//				pushad
	//				push		ecx
	//				call		MJPEG_IDCT
	//				pop			ecx
	//				popad

					movq		mm6,mask5
					pxor		mm7,mm7
	;				movq		mm2,CrCb_coeff_G
	;				movq		mm1,x4000400040004000
	;				movq		mm7,Cr_coeff_R
					mov			edi,[esp + 12]
	yloop2:
					mov			esi,2
	xloop2:
					movd		mm5,[eax]		;Cb (0,1)

					movd		mm3,[eax+128]	;Cr (0,1)
					movq		mm4,mm5			;Cb [duplicate]

					movq		mm0,[ecx]		;Y (0,1,2,3)
					punpcklwd	mm5,mm5			;Cb [subsampling]

					pmullw		mm5,Cb_coeff_B	;Cb [produce blue impacts]
					punpcklwd	mm4,mm3			;mm4: [Cr1][Cb1][Cr0][Cb0]

					pmaddwd		mm4,CrCb_coeff_G
					punpcklwd	mm3,mm3			;Cr [subsampling]

					pmullw		mm3,Cr_coeff_R	;Cr [produce red impacts]
					psllw		mm0,6

					paddw		mm5,mm0			;B (0,1,2,3)
					add			edx,8

					packssdw	mm4,mm4			;green impacts

					paddw		mm3,mm0			;R (0,1,2,3)
					psraw		mm5,6

					paddw		mm4,mm0			;G (0,1,2,3)
					psraw		mm3,6

					movq		[ecx],mm7
					packuswb	mm3,mm3

					psraw		mm4,4
					pand		mm3,mm6

					paddsw		mm4,G_const_1
					packuswb	mm5,mm5

					pand		mm5,mm6
					psrlq		mm3,1

					psubusw		mm4,G_const_2
					psrlq		mm5,3

					pand		mm4,G_const_3
					punpcklbw	mm5,mm3

					por			mm5,mm4
					add			eax,4

					add			ecx,8
					dec			esi

					movq		[edx-8],mm5
					jne			xloop2

					sub			edx,ebx						/* 304*4 + 32 */
					add			eax,8

					movq		[eax-16],mm7
					movq		[eax+128-16],mm7

					dec			edi
					jne			yloop2

					sub			eax,dword ptr [esp + 4]		/* 256-16 */
					add			edx,dword ptr [esp + 0]		/* 304*4*8 + 32 */
					add			ecx,dword ptr [esp + 8]

					dec			ebp
					jne			zloop2

					add			esp,16

					pop			esi
					pop			edi
					pop			ebp
				}
			}

			pixptr += decode16 ? 8 : 16;
			if (++mb_x >= mcu_width) {
				long bpr = mcu_width * (decode16 ? 8 : 16);
				mb_x = 0;

				if (vc_half) {
					if (interlaced)
						pixptr -= bpr * 33;
					else
						pixptr -= bpr * 17;
				} else {
					if (interlaced)
						pixptr -= bpr * 17;
					else
						pixptr -= bpr * 9;
				}

				if (++mb_y == clip_row) {
					if (decode16)
						modulo1 = mcu_width*8*4*clip_lines*(interlaced?2:1) + 16;
					else
						modulo1 = mcu_width*16*4*clip_lines*(interlaced?2:1) + 32;
					modulo2 = 16*clip_lines - 16;
					modulo3 = 32*(8-clip_lines);
					lines = clip_lines;
				}
			}
		}
#ifdef PROFILE
		__asm {
			rdtsc
			add dword ptr cvt_cycles+0,eax
			adc dword ptr cvt_cycles+4,edx
		}
#endif
	}

#ifdef PROFILE
	{
		char buf[128];
		static __int64 tcycles;
		static __int64 tcyclesDCT;
		static __int64 tcyclesCVT;
		static int tcount;

		tcycles += mb_cycles;
		tcyclesDCT += dct_cycles;
		tcyclesCVT += cvt_cycles;
		tcount += mcu_count;

		if (tcount >= 65536) {
			sprintf(buf, "decode: %4I64d (%3d%% CPU)     IDCT: %4I64d (%3d%% CPU)    CVT: %4I64d (%3d%% CPU)\n",
						tcycles/tcount,
						(long)((tcycles*24*2997+tcount*2250000i64)/(tcount*4500000i64)),
						tcyclesDCT/tcount,
						(long)((tcyclesDCT*24*2997+tcount*2250000i64)/(tcount*4500000i64)),
						tcyclesCVT/tcount,
						(long)((tcyclesCVT*24*2997+tcount*2250000i64)/(tcount*4500000i64))
						);
			OutputDebugString(buf);
			tcount = 0;
			tcycles = 0;
			tcyclesDCT = 0;
			tcyclesCVT = 0;


#ifdef DCTLEN_PROFILE
			sprintf(buf, "%d short coefficients, %d medium, %d long\n", short_coeffs, med_coeffs, long_coeffs);
			OutputDebugString(buf);
			short_coeffs = med_coeffs = long_coeffs = 0;
#endif
		}
	}
#endif

	__asm emms

//	return ptr - ((31-bitcnt)>>3);
	return ptr - 8;
}
