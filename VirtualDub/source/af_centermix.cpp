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

#include <vd2/system/VDRingBuffer.h>
#include <vd2/system/Error.h>

#include "filter.h"
#include "af_base.h"
#include "audioutil.h"

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

	VDWaveFormat *pwf0;

	if (!(mpContext->mpOutputs[0]->mpFormat = pwf0 = mpContext->mpAudioCallbacks->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat)))
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
		sint16 buf[4096];
		int tc = std::min<int>(samples, 2048);		// 4096 / 2 channels

		int tca0 = mpContext->mpInputs[0]->Read(dst, tc, true, kVFARead_PCM16);
		int tca1 = mpContext->mpInputs[1]->Read(buf, tc, true, kVFARead_PCM16);

		VDASSERT(tc == tca0 && tc == tca1);

		for(unsigned i=0; i<tc; ++i) {
			const sint32 t = buf[i];
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
	kVFAF_Zero,

	sizeof(VDAudioFilterCenterMix),	2, 1,

	NULL,

	VDAudioFilterCenterMix::InitProc,
	&VDAudioFilterBase::sVtbl,
};


extern const struct VDPluginInfo apluginDef_centermix = {
	sizeof(VDPluginInfo),
	L"center mix",
	NULL,
	L"Mixes a stereo stream with a mono stream.",
	0,
	kVDPluginType_Audio,
	0,

	kVDPlugin_APIVersion,
	kVDPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_centermix
};
