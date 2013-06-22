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

class VDAudioFilterMix : public VDAudioFilterBase {
public:
	VDAudioFilterMix();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();

	uint32 Read(unsigned pin, void *dst, uint32 samples);

	sint64 Seek(sint64);

	VDRingBuffer<sint16> mOutputBuffer;
};

VDAudioFilterMix::VDAudioFilterMix()
{
}

void __cdecl VDAudioFilterMix::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterMix;
}

uint32 VDAudioFilterMix::Prepare() {
	const VDWaveFormat& format0 = *mpContext->mpInputs[0]->mpFormat;
	const VDWaveFormat& format1 = *mpContext->mpInputs[1]->mpFormat;

	if (   format0.mTag != VDWaveFormat::kTagPCM
		|| format1.mTag != VDWaveFormat::kTagPCM
		|| format0.mSamplingRate != format1.mSamplingRate
		|| format0.mSampleBits != format1.mSampleBits
		|| format0.mChannels != format1.mChannels
		|| (format0.mSampleBits != 8 && format0.mSampleBits != 16)
		)
		return kVFAPrepare_BadFormat;

	VDWaveFormat *pwf0;

	if (!(mpContext->mpOutputs[0]->mpFormat = pwf0 = mpContext->mpAudioCallbacks->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat)))
		mpContext->mpServices->ExceptOutOfMemory();

	pwf0->mChannels		= format0.mChannels;
	pwf0->mSampleBits	= 16;
	pwf0->mBlockSize	= 2 * pwf0->mChannels;
	pwf0->mDataRate		= pwf0->mBlockSize * pwf0->mSamplingRate;

	return 0;
}

void VDAudioFilterMix::Start() {
	const VDAudioFilterPin& pin0 = *mpContext->mpOutputs[0];
	const VDWaveFormat& format0 = *pin0.mpFormat;

	mOutputBuffer.Init(format0.mChannels * pin0.mBufferSize);
}

uint32 VDAudioFilterMix::Run() {
	VDAudioFilterPin& pin1 = *mpContext->mpInputs[0];
	VDAudioFilterPin& pin2 = *mpContext->mpInputs[1];
	const VDWaveFormat& format1 = *pin1.mpFormat;
	const VDWaveFormat& format2 = *pin2.mpFormat;

	int samples, actual = 0;

	sint16 *dst = mOutputBuffer.LockWrite(mOutputBuffer.getSize(), samples);

	samples /= format1.mChannels;

	if (samples > mpContext->mInputSamples)
		samples = mpContext->mInputSamples;

	if (!samples && pin1.mbEnded && pin2.mbEnded)
		return true;

	while(samples > 0) {
		sint16 buf[4096];
		int tc = std::min<int>(samples, 4096 / format1.mChannels);

		int tca0 = mpContext->mpInputs[0]->Read(dst, tc, true, kVFARead_PCM16);
		int tca1 = mpContext->mpInputs[1]->Read(buf, tc, true, kVFARead_PCM16);

		VDASSERT(tc == tca0 && tc == tca1);

		int elements = tc * format1.mChannels;

		for(unsigned i=0; i<elements; ++i) {
			const sint32 t = buf[i];
			sint32 t0 = dst[i] + t + 0x8000;

			if ((uint32)t0 >= 0x10000)
				t0 = ~t0 >> 31;

			dst[i] = (sint16)(t0 - 0x8000);
		}

		dst += elements;

		actual += tc;
		samples -= tc;
	}

	mOutputBuffer.UnlockWrite(actual * format1.mChannels);

	mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel() / format1.mChannels;

	return 0;
}

uint32 VDAudioFilterMix::Read(unsigned pinno, void *dst, uint32 samples) {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[pinno];
	const VDWaveFormat& format = *pin.mpFormat;

	samples = std::min<uint32>(samples, mOutputBuffer.getLevel() / format.mChannels);

	if (dst) {
		mOutputBuffer.Read((sint16 *)dst, samples*format.mChannels);
		mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel() / format.mChannels;
	}

	return samples;
}

sint64 VDAudioFilterMix::Seek(sint64 us) {
	mOutputBuffer.Flush();
	mpContext->mpOutputs[0]->mCurrentLevel = 0;
	return us;
}

extern const struct VDAudioFilterDefinition afilterDef_mix = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterMix),	2, 1,

	NULL,

	VDAudioFilterMix::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const struct VDPluginInfo apluginDef_mix = {
	sizeof(VDPluginInfo),
	L"mix",
	NULL,
	L"Mixes two streams together linearly.",
	0,
	kVDPluginType_Audio,
	0,

	kVDPlugin_APIVersion,
	kVDPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_mix
};
