//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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
#include <math.h>
#include <vector>
#include <vd2/system/VDRingBuffer.h>
#include "af_base.h"
#include "fht.h"

class VDAudioFilterCenterCut : public VDAudioFilterBase {
public:
	VDAudioFilterCenterCut();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();

	uint32 Read(unsigned pin, void *dst, uint32 samples);

	sint64 Seek(sint64);

protected:
	enum {
		kWindowSize		= 4096,
		kHalfWindow		= 2048,
		kQuarterWindow	= 1024
	};

	uint32		mInputSamplesNeeded;
	uint32		mInputPos;

	VDRingBuffer<char>	mOutputBuffer[2];

	unsigned	mBitRev[kWindowSize];
	float		mWindow[kWindowSize];
	float		mSineTab[kWindowSize];
	float		mInput[kWindowSize][2];
	float		mOverlapC[4][kQuarterWindow];
	float		mTempLBuffer[kWindowSize];
	float		mTempRBuffer[kWindowSize];
	float		mTempCBuffer[kWindowSize];
};

VDAudioFilterCenterCut::VDAudioFilterCenterCut()
{
}

void __cdecl VDAudioFilterCenterCut::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterCenterCut;
}

uint32 VDAudioFilterCenterCut::Prepare() {
	const VDWaveFormat& format0 = *mpContext->mpInputs[0]->mpFormat;

	if (   format0.mTag != VDWaveFormat::kTagPCM
		|| format0.mChannels != 2
		|| (format0.mSampleBits != 8 && format0.mSampleBits != 16)
		)
		return kVFAPrepare_BadFormat;

	mpContext->mpInputs[0]->mGranularity	= kQuarterWindow;
	mpContext->mpInputs[0]->mDelay			= (sint32)ceil((kQuarterWindow*3000000.0)/format0.mSamplingRate);
	mpContext->mpOutputs[0]->mGranularity	= kQuarterWindow;
	mpContext->mpOutputs[1]->mGranularity	= kQuarterWindow;

	VDWaveFormat *pwf0, *pwf1;

	if (!(mpContext->mpOutputs[0]->mpFormat = pwf0 = mpContext->mpServices->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat)))
		mpContext->mpServices->ExceptOutOfMemory();
	if (!(mpContext->mpOutputs[1]->mpFormat = pwf1 = mpContext->mpServices->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat)))
		mpContext->mpServices->ExceptOutOfMemory();

	pwf0->mChannels		= 2;
	pwf0->mSampleBits	= 16;
	pwf0->mBlockSize	= 4;
	pwf0->mDataRate		= pwf1->mSamplingRate * 4;

	pwf1->mChannels		= 1;
	pwf1->mSampleBits	= 16;
	pwf1->mBlockSize	= 2;
	pwf1->mDataRate		= pwf1->mSamplingRate * 2;
	return 0;
}

void VDAudioFilterCenterCut::Start() {
	const VDAudioFilterPin& pin0 = *mpContext->mpOutputs[0];
	const VDAudioFilterPin& pin1 = *mpContext->mpOutputs[1];
	const VDWaveFormat& format0 = *pin0.mpFormat;
	const VDWaveFormat& format1 = *pin1.mpFormat;

	mOutputBuffer[0].Init(format0.mBlockSize * pin0.mBufferSize);
	mOutputBuffer[1].Init(format1.mBlockSize * pin1.mBufferSize);

	VDCreateBitRevTable(mBitRev, kWindowSize);
	VDCreateHalfSineTable(mSineTab, kWindowSize);

	mInputSamplesNeeded = kWindowSize;
	mInputPos = 0;

	memset(mOverlapC, 0, sizeof mOverlapC);

	float tmp[kWindowSize];
	VDCreateRaisedCosineWindow(tmp, kWindowSize, 1.0);
	for(unsigned i=0; i<kWindowSize; ++i) {
		// The correct Hartley<->FFT conversion is:
		//
		//	Fr(i) = 0.5(Hr(i) + Hi(i))
		//	Fi(i) = 0.5(Hr(i) - Hi(i))
		//
		// We omit the 0.5 in both the forward and reverse directions,
		// so we have a 0.25 to put here.  On the other hand, we are
		// using a raised cosine window with 1/4 step instead of a 1/2
		// step, which gives us a 2.0 factor.  So we only need 0.5.

		mWindow[i] = tmp[mBitRev[i]] * 0.25f;
	}
}

uint32 VDAudioFilterCenterCut::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	int samples, actual = 0;
	unsigned i;

	sint16 *dst1 = (sint16 *)mOutputBuffer[0].LockWrite(kQuarterWindow*4, samples);
	sint16 *dst2 = (sint16 *)mOutputBuffer[1].LockWrite(samples>>1, samples);

	samples >>= 1;

	if (samples < kQuarterWindow)
		return 0;

	// fill up the input window

	while(mInputSamplesNeeded > 0) {
		sint16 buf[4096];
		int tc = mpContext->mpInputs[0]->Read(&buf, std::min<int>(mInputSamplesNeeded, 4096 / format.mChannels), false, kVFARead_PCM16);

		if (tc<=0)
			return !actual && pin.mbEnded ? kVFARun_Finished : 0;

		for(i=0; i<tc; ++i) {
			mInput[mInputPos][0] = buf[i*2+0];
			mInput[mInputPos][1] = buf[i*2+1];
			mInputPos = (mInputPos + 1) & (kWindowSize-1);
		}

		actual += tc;
		mInputSamplesNeeded -= tc;
	}

	// copy to temporary buffer and FHT

	for(i=0; i<kWindowSize; ++i) {
		const unsigned j = mBitRev[i];
		const unsigned k = (j + mInputPos) & (kWindowSize-1);
		const float w = mWindow[i];

		mTempLBuffer[i] = mInput[k][0] * w;
		mTempRBuffer[i] = mInput[k][1] * w;
	}

	VDComputeFHT(mTempLBuffer, kWindowSize, mSineTab);
	VDComputeFHT(mTempRBuffer, kWindowSize, mSineTab);

	// perform stereo separation

	mTempCBuffer[0] = 0;
	mTempCBuffer[1] = 0;
	for(i=1; i<kHalfWindow; i++) {
		float lR = mTempLBuffer[i] + mTempLBuffer[kWindowSize-i];
		float lI = mTempLBuffer[i] - mTempLBuffer[kWindowSize-i];
		float rR = mTempRBuffer[i] + mTempRBuffer[kWindowSize-i];
		float rI = mTempRBuffer[i] - mTempRBuffer[kWindowSize-i];

		float alpha, dot, clR, clI, crR, crI, cR, cI;
		float ldotl, rdotr;
		float A, B, C, D;

		dot = lR*rR+lI*rI;
		ldotl = lR*lR+lI*lI;
		rdotr = rR*rR+rI*rI;
		alpha = dot/(ldotl+0.0000001);

		clR = lR * alpha;
		clI = lI * alpha;

		alpha = dot/(rdotr+0.0000001);

		crR = rR * alpha;
		crI = rI * alpha;

		cR = clR + crR;
		cI = clI + crI;

		A = cR*cR + cI*cI;
		B = -cR*(lR+rR)-cI*(lI+rI);
		C = dot;
		D = B*B-4*A*C;

		if (D>=0.0F && A>0.00000001) {
			alpha = (-B-sqrt(D))/(2*A);

			cR*=alpha;
			cI*=alpha;
		} else
			cR = cI = 0.0f;

		unsigned d1 = mBitRev[i];
		unsigned d2 = mBitRev[kWindowSize-i];

		mTempCBuffer[d1]	= cR + cI;
		mTempCBuffer[d2]	= cR - cI;
	}

	// reconstitute left/right/center channels

	VDComputeFHT(mTempCBuffer, kWindowSize, mSineTab);

	// writeout

	enum {
		M0 = 0,
		M1 = kQuarterWindow,
		M2 = kQuarterWindow*2,
		M3 = kQuarterWindow*3
	};

	struct local {
		static sint16 float_to_sint16_clip(float f) {
			f += (12582912.0f + 32768.0f);			// 2^23 + 2^22 + 32K
			int v = reinterpret_cast<int&>(f) - 0x4B400000;

			if ((unsigned)v >= 0x10000)
				v = ~v >> 31;

			return (sint16)v - 0x8000;
		}
	};

	for(i=0; i<kQuarterWindow; ++i) {
		float c = mOverlapC [0][i]    + mTempCBuffer[i+M0];
		float l = mInput[mInputPos+i][0] - c;
		float r = mInput[mInputPos+i][1] - c;

		dst1[0] = local::float_to_sint16_clip(l);
		dst1[1] = local::float_to_sint16_clip(r);
		dst1 += 2;

		*dst2++ = local::float_to_sint16_clip(c);

		mOverlapC [0][i]    = mOverlapC [1][i]    + mTempCBuffer[i+M1];
		mOverlapC [1][i]    = mOverlapC [2][i]    + mTempCBuffer[i+M2];
		mOverlapC [2][i]    = mTempCBuffer[i+M3];
	}

	mInputSamplesNeeded = kQuarterWindow;

	mOutputBuffer[0].UnlockWrite(kQuarterWindow * mpContext->mpOutputs[0]->mpFormat->mBlockSize);
	mOutputBuffer[1].UnlockWrite(kQuarterWindow * mpContext->mpOutputs[1]->mpFormat->mBlockSize);

	mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer[0].getLevel() / mpContext->mpOutputs[0]->mpFormat->mBlockSize;
	mpContext->mpOutputs[1]->mCurrentLevel = mOutputBuffer[1].getLevel() / mpContext->mpOutputs[1]->mpFormat->mBlockSize;

	return 0;
}

uint32 VDAudioFilterCenterCut::Read(unsigned pinno, void *dst, uint32 samples) {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[pinno];
	const VDWaveFormat& format = *pin.mpFormat;

	samples = std::min<uint32>(samples, mOutputBuffer[pinno].getLevel() / format.mBlockSize);

	if (dst) {
		mOutputBuffer[pinno].Read((char *)dst, samples * format.mBlockSize);
		mpContext->mpOutputs[pinno]->mCurrentLevel = mOutputBuffer[pinno].getLevel() / mpContext->mpOutputs[pinno]->mpFormat->mBlockSize;
	}

	return samples;
}

sint64 VDAudioFilterCenterCut::Seek(sint64 us) {
	mOutputBuffer[0].Flush();
	mpContext->mpOutputs[0]->mCurrentLevel = 0;
	mOutputBuffer[1].Flush();
	mpContext->mpOutputs[1]->mCurrentLevel = 0;
	return us;
}

extern const struct VDAudioFilterDefinition afilterDef_centercut = {
	sizeof(VDAudioFilterDefinition),
	L"center cut",
	NULL,
	L"Splits a stereo stream into stereo-side and mono-center outputs using phase analysis.",
	0,
	kVFAF_Zero,

	sizeof(VDAudioFilterCenterCut),	1,	2,

	NULL,

	VDAudioFilterCenterCut::InitProc,
	&VDAudioFilterBase::sVtbl,
};
