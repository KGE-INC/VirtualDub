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
#include <vd2/system/strutil.h>
#include <vd2/system/fraction.h>

#include "filter.h"
#include "af_base.h"

class VDAudioFilterSplit : public VDAudioFilterBase {
public:
	VDAudioFilterSplit();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();

	uint32 Read(unsigned pin, void *dst, uint32 samples);

	sint64 Seek(sint64);

	VDRingBuffer<char> mOutputBuffer[2];
};

VDAudioFilterSplit::VDAudioFilterSplit()
{
}

void __cdecl VDAudioFilterSplit::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterSplit;
}

uint32 VDAudioFilterSplit::Prepare() {
	const VDWaveFormat& format0 = *mpContext->mpInputs[0]->mpFormat;

	VDWaveFormat *pwf0, *pwf1;

	if (!(mpContext->mpOutputs[0]->mpFormat = pwf0 = mpContext->mpAudioCallbacks->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat)))
		mpContext->mpServices->ExceptOutOfMemory();
	if (!(mpContext->mpOutputs[1]->mpFormat = pwf1 = mpContext->mpAudioCallbacks->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat)))
		mpContext->mpServices->ExceptOutOfMemory();
	return 0;
}

void VDAudioFilterSplit::Start() {
	const VDAudioFilterPin& pin0 = *mpContext->mpOutputs[0];
	const VDAudioFilterPin& pin1 = *mpContext->mpOutputs[1];
	const VDWaveFormat& format0 = *pin0.mpFormat;
	const VDWaveFormat& format1 = *pin1.mpFormat;

	mOutputBuffer[0].Init(format0.mBlockSize * pin0.mBufferSize);
	mOutputBuffer[1].Init(format1.mBlockSize * pin1.mBufferSize);
}

uint32 VDAudioFilterSplit::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	int samples, actual = 0;

	void *dst1 = (short *)mOutputBuffer[0].LockWrite(mOutputBuffer[0].getSize(), samples);
	void *dst2 = (short *)mOutputBuffer[1].LockWrite(samples, samples);

	samples /= mpContext->mpOutputs[0]->mpFormat->mBlockSize;

	actual = mpContext->mpInputs[0]->Read(dst1, samples, false, kVFARead_Native);

	memcpy(dst2, dst1, actual * mpContext->mpOutputs[0]->mpFormat->mBlockSize);

	mOutputBuffer[0].UnlockWrite(actual * mpContext->mpOutputs[0]->mpFormat->mBlockSize);
	mOutputBuffer[1].UnlockWrite(actual * mpContext->mpOutputs[1]->mpFormat->mBlockSize);

	mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer[0].getLevel() / mpContext->mpOutputs[0]->mpFormat->mBlockSize;
	mpContext->mpOutputs[1]->mCurrentLevel = mOutputBuffer[1].getLevel() / mpContext->mpOutputs[1]->mpFormat->mBlockSize;

	return !actual && pin.mbEnded ? kVFARun_Finished : 0;
}

uint32 VDAudioFilterSplit::Read(unsigned pinno, void *dst, uint32 samples) {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[pinno];
	const VDWaveFormat& format = *pin.mpFormat;

	samples = std::min<uint32>(samples, mOutputBuffer[pinno].getLevel() / format.mBlockSize);

	if (dst) {
		mOutputBuffer[pinno].Read((char *)dst, samples * format.mBlockSize);
		mpContext->mpOutputs[pinno]->mCurrentLevel = mOutputBuffer[pinno].getLevel() / mpContext->mpOutputs[pinno]->mpFormat->mBlockSize;
	}

	return samples;
}

sint64 VDAudioFilterSplit::Seek(sint64 us) {
	mOutputBuffer[0].Flush();
	mpContext->mpOutputs[0]->mCurrentLevel = 0;
	mOutputBuffer[1].Flush();
	mpContext->mpOutputs[1]->mCurrentLevel = 0;
	return us;
}

extern const struct VDAudioFilterDefinition afilterDef_split = {
	sizeof(VDAudioFilterDefinition),
	kVFAF_Zero,

	sizeof(VDAudioFilterSplit),	1,	2,

	NULL,

	VDAudioFilterSplit::InitProc,
	&VDAudioFilterBase::sVtbl,
};

extern const struct VDPluginInfo apluginDef_split = {
	sizeof(VDPluginInfo),
	L"split",
	NULL,
	L"Splits an audio stream into two identical outputs.",
	0,
	kVDPluginType_Audio,
	0,

	kVDPlugin_APIVersion,
	kVDPlugin_APIVersion,
	kVDPlugin_AudioAPIVersion,
	kVDPlugin_AudioAPIVersion,

	&afilterDef_split
};
