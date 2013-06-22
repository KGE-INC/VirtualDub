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

#include <mmsystem.h>

#include <vd2/system/VDRingBuffer.h>
#include <vd2/system/Error.h>

#include "filter.h"
#include "af_base.h"

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterCenterMix : public VDAudioFilterBase {
public:
	VDAudioFilterCenterMix();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();

	uint32 Read(unsigned pin, void *dst, uint32 samples);

	sint64 Seek(sint64);

	VDRingBuffer<sint16> mOutputBuffer;
};

VDAudioFilterCenterMix::VDAudioFilterCenterMix()
{
}

void __cdecl VDAudioFilterCenterMix::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterCenterMix;
}

uint32 VDAudioFilterCenterMix::Prepare() {
	const VDWaveFormat& format0 = *mpContext->mpInputs[0]->mpFormat;
	const VDWaveFormat& format1 = *mpContext->mpInputs[1]->mpFormat;

	if (   format0.mTag != VDWaveFormat::kTagPCM
		|| format0.mChannels != 2
		|| (format0.mSampleBits != 8 && format0.mSampleBits != 16)
		|| format1.mTag != VDWaveFormat::kTagPCM
		|| format1.mChannels != 1
		|| (format1.mSampleBits != 8 && format0.mSampleBits != 16)
		|| format0.mSamplingRate != format1.mSamplingRate
		|| format0.mSampleBits != format1.mSampleBits
		)
		return kVFAPrepare_BadFormat;

	mpContext->mpInputs[0]->mGranularity	= 1;
	mpContext->mpInputs[0]->mDelay			= 0;
	mpContext->mpInputs[1]->mGranularity	= 1;
	mpContext->mpInputs[1]->mDelay			= 0;
	mpContext->mpOutputs[0]->mGranularity	= 1;

	VDWaveFormat *pwf0;

	if (!(mpContext->mpOutputs[0]->mpFormat = pwf0 = mpContext->mpServices->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat)))
		mpContext->mpServices->ExceptOutOfMemory();

	pwf0->mChannels		= 2;
	pwf0->mSampleBits	= 16;
	pwf0->mBlockSize	= 4;
	pwf0->mDataRate		= 4 * pwf0->mSamplingRate;

	return 0;
}

void VDAudioFilterCenterMix::Start() {
	const VDAudioFilterPin& pin0 = *mpContext->mpOutputs[0];
	const VDWaveFormat& format0 = *pin0.mpFormat;

	mOutputBuffer.Init(2 * pin0.mBufferSize);
}

uint32 VDAudioFilterCenterMix::Run() {
	VDAudioFilterPin& pin1 = *mpContext->mpInputs[0];
	VDAudioFilterPin& pin2 = *mpContext->mpInputs[1];
	const VDWaveFormat& format1 = *pin1.mpFormat;
	const VDWaveFormat& format2 = *pin2.mpFormat;

	int samples, actual = 0;

	sint16 *dst = mOutputBuffer.LockWrite(mOutputBuffer.getSize(), samples);

	samples >>= 1;

	if (samples > mpContext->mInputSamples)
		samples = mpContext->mInputSamples;

	if (!samples && pin1.mbEnded && pin2.mbEnded)
		return true;

	while(samples > 0) {
		union {
			sint16	w[4096];
			uint8	b[4096];
		} buf0, buf1;
		int tc = std::min<int>(samples, 2048);

		int tca0 = mpContext->mpInputs[0]->mpReadProc(mpContext->mpInputs[0], &buf0, tc, true);
		int tca1 = mpContext->mpInputs[1]->mpReadProc(mpContext->mpInputs[1], &buf1, tc, true);

		VDASSERT(tc == tca0 && tc == tca1);

		if (format1.mSampleBits==16)
			memcpy(dst, buf0.w, tc*4);
		else {
			for(unsigned i=0; i<tc*2; ++i) {
				dst[i] = (sint16)(sint8)(buf0.b[i]-0x80) << 8;
			}
		}

		if (format2.mSampleBits==16) {
			for(unsigned i=0; i<tc; ++i) {
				const sint32 t = buf1.w[i];
				sint32 t0 = dst[0] + t + 0x8000;
				sint32 t1 = dst[1] + t + 0x8000;

				if ((uint32)t0 >= 0x10000)
					t0 = ~t0 >> 31;

				if ((uint32)t1 >= 0x10000)
					t1 = ~t1 >> 31;

				dst[0] = (sint16)(t0 - 0x8000);
				dst[1] = (sint16)(t1 - 0x8000);
				dst += 2;
			}
		} else {
			for(unsigned i=0; i<tc; ++i) {
				const sint32 t = (sint32)buf1.b[i] << 8;
				sint32 t0 = dst[0] + t;
				sint32 t1 = dst[1] + t;

				if ((uint32)t0 >= 0x10000)
					t0 = ~t0 >> 31;

				if ((uint32)t1 >= 0x10000)
					t1 = ~t1 >> 31;

				dst[0] = (sint16)(t0 - 0x8000);
				dst[1] = (sint16)(t1 - 0x8000);
				dst += 2;
			}
		}

		actual += tc;
		samples -= tc;
	}

	mOutputBuffer.UnlockWrite(actual * 2);

	mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel() >> 1;

	return 0;
}

uint32 VDAudioFilterCenterMix::Read(unsigned pinno, void *dst, uint32 samples) {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[pinno];
	const VDWaveFormat& format = *pin.mpFormat;

	samples = std::min<uint32>(samples, mOutputBuffer.getLevel()>>1);

	if (dst) {
		mOutputBuffer.Read((sint16 *)dst, samples*2);
		mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel() >> 1;
	}

	return samples;
}

sint64 VDAudioFilterCenterMix::Seek(sint64 us) {
	mOutputBuffer.Flush();
	mpContext->mpOutputs[0]->mCurrentLevel = 0;
	return us;
}

extern const struct VDAudioFilterDefinition afilterDef_centermix = {
	sizeof(VDAudioFilterDefinition),
	L"center mix",
	NULL,
	L"Mixes a stereo stream with a mono stream.",
	0,

	sizeof(VDAudioFilterCenterMix),	2, 1,

	NULL,

	VDAudioFilterCenterMix::InitProc,
	VDAudioFilterCenterMix::DestroyProc,
	VDAudioFilterCenterMix::PrepareProc,
	VDAudioFilterCenterMix::StartProc,
	VDAudioFilterCenterMix::StopProc,
	VDAudioFilterCenterMix::RunProc,
	VDAudioFilterCenterMix::ReadProc,
	VDAudioFilterCenterMix::SeekProc,
	VDAudioFilterCenterMix::SerializeProc,
	VDAudioFilterCenterMix::DeserializeProc,
	VDAudioFilterCenterMix::GetParamProc,
	VDAudioFilterCenterMix::SetParamProc,
	VDAudioFilterCenterMix::ConfigProc,
};

