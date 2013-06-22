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
#include "af_base.h"
#include "gui.h"
#include "resource.h"
#include <vd2/system/fraction.h>
#include <vd2/system/VDRingBuffer.h>

VDAFBASE_BEGIN_CONFIG(NewRate);
VDAFBASE_CONFIG_ENTRY(NewRate, 0, U32, newfreq, L"New frequency (Hz)",	L"Target frequency in Hertz." );
VDAFBASE_END_CONFIG(NewRate, 0);

typedef VDAudioFilterData_NewRate VDAudioFilterNewRateConfig;

class VDDialogAudioFilterNewRateConfig : public VDDialogBaseW32 {
public:
	VDDialogAudioFilterNewRateConfig(VDAudioFilterNewRateConfig& config) : VDDialogBaseW32(IDD_AF_NEWRATE), mConfig(config) {}

	bool Activate(VDGUIHandle hParent) {
		return 0 != ActivateDialog(hParent);
	}

	BOOL DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
		char buf[256];

		switch(msg) {
		case WM_INITDIALOG:
			sprintf(buf, "%lu", mConfig.newfreq);
			SetDlgItemText(mhdlg, IDC_FREQ, buf);
			return TRUE;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					BOOL valid;

					mConfig.newfreq = GetDlgItemInt(mhdlg, IDC_FREQ, &valid, FALSE);
					if (!valid) {
						MessageBeep(MB_ICONEXCLAMATION);
						SetFocus(GetDlgItem(mhdlg, IDC_FREQ));
						return TRUE;
					}
					End(TRUE);
				}
				return TRUE;
			case IDCANCEL:
				End(FALSE);
				return TRUE;
			}
		}

		return FALSE;
	}

	VDAudioFilterNewRateConfig& mConfig;
};

class VDAudioFilterNewRate : public VDAudioFilterBase {
public:
	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	VDAudioFilterNewRate() {
		mConfig.newfreq = 44100;
	}
	
	bool Config(HWND hwnd) {
		VDAudioFilterNewRateConfig	config(mConfig);

		if (!hwnd)
			return true;

		if (VDDialogAudioFilterNewRateConfig(config).Activate((VDGUIHandle)hwnd)) {
			mConfig = config;
			return true;
		}
		return false;
	}

	uint32 Prepare();
	void Start();
	uint32 Run();
	uint32 Read(unsigned inpin, void *dst, uint32 samples);
	sint64 Seek(sint64 us);

	void *GetConfigPtr() { return &mConfig; }

	VDAudioFilterNewRateConfig	mConfig;
	VDRingBuffer<char>			mOutputBuffer;
	VDFraction					mRatio;
};




uint32 VDAudioFilterNewRate::Prepare() {
	const VDWaveFormat& inFormat = *mpContext->mpInputs[0]->mpFormat;

	if (inFormat.mTag != VDWaveFormat::kTagPCM)
		return kVFAPrepare_BadFormat;

	VDWaveFormat *pwf = mpContext->mpServices->CopyWaveFormat(&inFormat);

	if (!pwf)
		mpContext->mpServices->ExceptOutOfMemory();

	mpContext->mpOutputs[0]->mpFormat = pwf;

	mRatio = VDFraction(mConfig.newfreq, pwf->mSamplingRate);

	pwf->mSamplingRate	= mConfig.newfreq;
	pwf->mDataRate		= pwf->mSamplingRate * pwf->mBlockSize;

	return 0;
}

void VDAudioFilterNewRate::Start() {
	const VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	mOutputBuffer.Init(format.mBlockSize * pin.mBufferSize);
}

uint32 VDAudioFilterNewRate::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDWaveFormat& format = *pin.mpFormat;
	const unsigned blocksize = format.mBlockSize;
	bool bInputRead = false;

	int samples = mpContext->mInputSamples;

	// compute output samples
	int elems = samples * blocksize;
	char *dst;
	
	samples = 0;
	if (elems > 0) {
		dst = mOutputBuffer.LockWrite(elems, elems);
		samples = elems / blocksize;
	}

	if (!samples) {
		if (pin.mbEnded && !elems)
			return kVFARun_Finished;

		return 0;
	}

	// read buffer

	unsigned count = blocksize * samples;

	int actual_samples = mpContext->mpInputs[0]->Read(dst, samples, false, kVFARead_Native);
	VDASSERT(actual_samples == samples);

	mOutputBuffer.UnlockWrite(samples * blocksize);

	mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel() / blocksize;

	return 0;
}

uint32 VDAudioFilterNewRate::Read(unsigned inpin, void *dst, uint32 samples) {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	samples = std::min<uint32>(samples, mOutputBuffer.getLevel() / format.mBlockSize);

	if (dst) {
		mOutputBuffer.Read((char *)dst, samples * format.mBlockSize);
		mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel() / format.mBlockSize;
	}

	return samples;
}

sint64 VDAudioFilterNewRate::Seek(sint64 us) {
	mOutputBuffer.Flush();
	mpContext->mpOutputs[0]->mCurrentLevel = 0;
	return mRatio.scale64r(us);
}



void __cdecl VDAudioFilterNewRate::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterNewRate;
}

extern const struct VDAudioFilterDefinition afilterDef_newrate = {
	sizeof(VDAudioFilterDefinition),
	L"new rate",
	NULL,
	L"Changes the sampling rate on an audio stream without actually changing the audio data.",
	0,
	kVFAF_HasConfig,

	sizeof(VDAudioFilterNewRate),	1,	1,

	&VDAudioFilterData_NewRate::members.info,

	VDAudioFilterNewRate::InitProc,
	&VDAudioFilterBase::sVtbl,
};
