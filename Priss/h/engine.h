//	Priss (NekoAmp 2.0) - MPEG-1/2 audio decoding library
//	Copyright (C) 2003 Avery Lee
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

#ifndef f_VD2_PRISS_ENGINE_H
#define f_VD2_PRISS_ENGINE_H

#include <vd2/system/vdtypes.h>
#include <vd2/Priss/decoder.h>

#include "polyphase.h"

class VDMPEGAudioHuffBitReader;

class VDMPEGAudioDecoder : public IAMPDecoder {
public:
	VDMPEGAudioDecoder();
	~VDMPEGAudioDecoder();

	void	Destroy();
	char *	GetAmpVersionString();
	void	Init();
	void	setSource(IAMPBitsource *pSource);
	void	setDestination(short *psDest);
	long	getSampleCount();
	void	getStreamInfo(AMPStreamInfo *pasi);
	char *	getErrorString(int err);
	void	Reset();
	void	ReadHeader();
	void	PrereadFrame();
	bool	DecodeFrame();
	void	ConcealFrame();

protected:
	bool	DecodeLayerI();
	bool	DecodeLayerII();
	void	PrereadLayerIII();
	bool	DecodeLayerIII();

	static void DecodeHuffmanValues(VDMPEGAudioHuffBitReader& bitreader, sint32 *dst, unsigned table, unsigned pairs);

	IAMPBitsource *mpSource;
	AMPStreamInfo	mHeader;
	uint32			mFrameDataSize;
	short			*mpSampleDst;
	uint32			mSamplesDecoded;

	uint8			mBitrateIndex;
	uint8			mSamplingRateIndex;
	uint8			mMode;
	uint8			mModeExtension;

	float			mL2Scalefactors[64];
	sint8			mL2Ungroup3[31][3];

	uint8			mFrameBuffer[1728+4];

	enum { kL3BufferSize = 2048 };
	uint8			mL3Buffer[2048];			// Only needs to be 960, but we make it 1024 for address masking
	uint32			mL3BufferPos;				// Tail for layer III circular buffer operation

	float			mL3OverlapBuffer[2][32][18];	// used for overlapping tails of IMDCT output

	float			mL3Windows[4][36];			// IMDCT windows

	VDMPEGAudioPolyphaseFilter	*mpPolyphaseFilter;

	static const struct L3HuffmanTableDescriptor {
		const uint8 (*table)[2];
		unsigned treelen;
		unsigned bits;
		unsigned linbits;
	} sL3HuffmanTables[];
};

#endif
