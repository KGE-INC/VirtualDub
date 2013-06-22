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
#include "af_base.h"
#include "gui.h"
#include "resource.h"

///////////////////////////////////////////////////////////////////////////

VDAFBASE_BEGIN_CONFIG(PitchShift);
VDAFBASE_CONFIG_ENTRY(PitchShift, 0, Double, ratio, L"Pitch ratio", L"Factor by which to multiply pitch of sound.");
VDAFBASE_END_CONFIG(PitchShift, 0);

typedef VDAudioFilterData_PitchShift VDAudioFilterPitchShiftConfig;

class VDDialogAudioFilterPitchShiftConfig : public VDDialogBaseW32 {
public:
	VDDialogAudioFilterPitchShiftConfig(VDAudioFilterPitchShiftConfig& config) : VDDialogBaseW32(IDD_AF_PITCHSHIFT), mConfig(config) {}

	bool Activate(VDGUIHandle hParent) {
		return 0 != ActivateDialog(hParent);
	}

	BOOL DlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
		char buf[256];

		switch(msg) {
		case WM_INITDIALOG:
			sprintf(buf, "%.4f", mConfig.ratio);
			SetDlgItemText(mhdlg, IDC_RATIO, buf);
			return TRUE;
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					double v;

					if (!GetDlgItemText(mhdlg, IDC_RATIO, buf, sizeof buf) || (v=atof(buf))<0.5 || (v>2.0)) {
						MessageBeep(MB_ICONEXCLAMATION);
						SetFocus(GetDlgItem(mhdlg, IDC_RATIO));
						return TRUE;
					}
					mConfig.ratio = v;
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

	VDAudioFilterPitchShiftConfig& mConfig;
};

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterPitchShift : public VDAudioFilterBase {
public:
	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	VDAudioFilterPitchShift();
	~VDAudioFilterPitchShift();

	uint32 Prepare();
	void Start();
	uint32 Run();
	uint32 Read(unsigned inpin, void *dst, uint32 samples);
	sint64 Seek(sint64 us);

	void *GetConfigPtr() { return &mConfig; }

	bool Config(HWND hwnd) {
		VDAudioFilterPitchShiftConfig	config(mConfig);

		if (!hwnd)
			return true;

		if (VDDialogAudioFilterPitchShiftConfig(config).Activate((VDGUIHandle)hwnd)) {
			mConfig = config;
			return true;
		}
		return false;
	}

protected:
	VDAudioFilterPitchShiftConfig	mConfig;
	std::vector<sint16>		mPitchBuffer;
	VDRingBuffer<char>		mOutputBuffer;
	sint32 mSrcSamples;
	uint32 mdudx;
	std::vector<sint32>		mScores;
	std::vector<uint32>		mus;
};

void __cdecl VDAudioFilterPitchShift::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterPitchShift;
}

VDAudioFilterPitchShift::VDAudioFilterPitchShift() {
	mConfig.ratio = 1.0;
}

VDAudioFilterPitchShift::~VDAudioFilterPitchShift() {
}

uint32 VDAudioFilterPitchShift::Prepare() {
	const VDWaveFormat& inFormat = *mpContext->mpInputs[0]->mpFormat;

	if (   inFormat.mTag != VDWaveFormat::kTagPCM
		|| (inFormat.mSampleBits != 8 && inFormat.mSampleBits != 16)
		)
		return kVFAPrepare_BadFormat;

	mpContext->mpInputs[0]->mGranularity	= 1;
	mpContext->mpInputs[0]->mDelay			= 0;
	mpContext->mpOutputs[0]->mGranularity	= 1;

	VDWaveFormat *pwf = mpContext->mpServices->CopyWaveFormat(&inFormat);

	if (!pwf)
		mpContext->mpServices->ExceptOutOfMemory();

	mpContext->mpOutputs[0]->mpFormat = pwf;

	pwf->mSampleBits	= 16;
	pwf->mBlockSize		= 2 * pwf->mChannels;
	pwf->mDataRate		= pwf->mSamplingRate * pwf->mBlockSize;

	return 0;
}

void VDAudioFilterPitchShift::Start() {
	const VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	mPitchBuffer.resize(2048 * format.mChannels);
	mScores.resize(32 * format.mChannels);
	mOutputBuffer.Init(format.mBlockSize * pin.mBufferSize);
	mSrcSamples = 0;
	mus.resize(format.mChannels*3);
	std::fill(mus.begin(), mus.end(), 0);
	mdudx = (uint32)((uint64)0x10000 * mConfig.ratio);
}

uint32 VDAudioFilterPitchShift::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDWaveFormat& format = *pin.mpFormat;
	bool bInputRead = false;

	// foo
	short buf16[4096];
	char buf8[4096];

	int samples = std::min<int>(mpContext->mInputSamples, 4096 / format.mChannels);

	// compute output samples
	int bytes = samples * format.mBlockSize;
	sint16 *dst;
	
	samples = 0;
	if (bytes > 0) {
		dst = (sint16 *)mOutputBuffer.LockWrite(bytes, bytes);
		samples = bytes / format.mBlockSize;
	}

	if (!samples) {
		if (pin.mbEnded && !bytes)
			return kVFARun_Finished;

		return 0;
	}

	// read buffer

	switch(format.mSampleBits) {
	case 8:
		{
			int actual_samples = mpContext->mpInputs[0]->mpReadProc(mpContext->mpInputs[0], buf8, samples, false);
			VDASSERT(actual_samples == samples);

			unsigned count = format.mChannels * samples;

			for(unsigned i=0; i<count; ++i)
				buf16[i] = (sint16)((buf8[i]<<8) - 0x8000);
		}
		break;
	case 16:
		{
			int actual_samples = mpContext->mpInputs[0]->mpReadProc(mpContext->mpInputs[0], buf16, samples, false);
			VDASSERT(actual_samples == samples);
		}
		break;
	}

	// matrixing (2-channel only)

	if (format.mChannels == 2) {
		sint16 *tmp = buf16;

		for(unsigned i=0; i<samples; ++i) {
			const sint32 l = tmp[0];
			const sint32 r = tmp[1];

			tmp[0] = (l+r+1)>>1;
			tmp[1] = (l-r+1)>>1;
			tmp += 2;
		}
	}

	const int delta = mdudx > 0x10000 ? -128 : 128;
	const ptrdiff_t nch = format.mChannels;
	for(unsigned ch=0; ch<nch; ++ch) {
		const sint16 *src = &buf16[ch];
		sint16 *bufp = &mPitchBuffer[2048 * ch];

		static const int offsets[32]={
			0x03f0,			0x0410,
			0x03d0,			0x0430,
			0x03b0,			0x0450,
			0x0390,			0x0470,
			0x0370,			0x0490,
			0x0350,			0x04b0,
			0x0330,			0x04d0,
			0x0310,			0x04f0,
			0x02f0,			0x0510,
			0x02d0,			0x0530,
			0x02b0,			0x0550,
			0x0290,			0x0570,
			0x0270,			0x0590,
			0x0250,			0x05b0,
			0x0230,			0x05d0,
			0x0210,			0x05f0,
		};

		struct sign {
			static sint32 fn(sint32 v) { return (v>>31) - ((-v)>>31); }
		};

		struct sample {
			static sint32 fn(const sint16 *src, uint32 u) {
				sint32 uf = (sint32)(u & 0xffff)>>2;
				uint32 ui = (uint32)((u>>16) & 2047);
				const sint32 v0 = src[ui];
				const sint32 v1 = src[(ui+1)&2047];

				return v0 + (((v1-v0)*uf+0x2000)>>14);
			}
		};

		uint32 u0 = mus[ch*3+0];
		uint32 u1 = mus[ch*3+1];
		uint32 blend = mus[ch*3+2];
		sint32 *scores = &mScores[32 * ch];
		uint32 srcidx = mSrcSamples;
		sint16 *dst2 = dst + ch;

		for(unsigned i=0; i<samples; ++i) {
			const unsigned inpos = srcidx++ & 2047;
			const unsigned outpos0 = (u0>>16)&2047;
			const unsigned outpos1 = (u1>>16)&2047;

			bufp[inpos] = *src;

			const sint32 v0 = sample::fn(bufp, u0);
			const sint32 v1 = sample::fn(bufp, u1);
			const sint32 vo = v0+(((v1-v0)*(sint32)blend) >> 5);

			dst2[i*nch] = vo;
			u0 += mdudx;
			u1 += mdudx;
			src += nch;

			if (blend>0)
				--blend;

			int s1 = sign::fn((int)bufp[(inpos+delta)&2047]);

			for(unsigned k=0; k<32; ++k) {
				const int vp = (int)bufp[(inpos+offsets[k])&2047];

				scores[k] += s1*vp;
			}

			if (!(srcidx&31)) {
				for(unsigned j=0; j<32; ++j)
					scores[j] >>= 1;
			}

			if (!((inpos-outpos0+128) & ~255)) {
				unsigned hiscore = 0;
				for(unsigned j=1; j<32; ++j)
					if (scores[j] > scores[hiscore])
						hiscore = j;

				u1 = u0;
				u0 = (u0 & 0xffff) + ((srcidx + offsets[hiscore])<<16);
				blend = 32;
			}
		}

		mus[ch*3+0] = u0;
		mus[ch*3+1] = u1;
		mus[ch*3+2] = blend;
	}

	// matrixing (2-channel only)

	if (format.mChannels == 2) {
		sint16 *tmp = dst;

		for(unsigned i=0; i<samples; ++i) {
			uint32 l = tmp[0] + tmp[1] + 0x8000;
			uint32 r = tmp[0] - tmp[1] + 0x8000;

			if (l >= 0x10000)
				l = (sint32)~l >> 31;

			if (r >= 0x10000)
				r = (sint32)~r >> 31;

			tmp[0] = l - 0x8000;
			tmp[1] = r - 0x8000;
			tmp += 2;
		}
	}

	mSrcSamples += samples;

	mOutputBuffer.UnlockWrite(samples * format.mBlockSize);

	mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel() / mpContext->mpOutputs[0]->mpFormat->mBlockSize;

	return 0;
}

uint32 VDAudioFilterPitchShift::Read(unsigned inpin, void *dst, uint32 samples) {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	samples = std::min<uint32>(samples, mOutputBuffer.getLevel() / format.mBlockSize);

	if (dst) {
		mOutputBuffer.Read((char *)dst, samples * format.mBlockSize);
		mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel() / mpContext->mpOutputs[0]->mpFormat->mBlockSize;
	}

	return samples;
}

sint64 VDAudioFilterPitchShift::Seek(sint64 us) {
	mOutputBuffer.Flush();
	mpContext->mpOutputs[0]->mCurrentLevel = 0;
	return us;
}

extern const struct VDAudioFilterDefinition afilterDef_pitchshift = {
	sizeof(VDAudioFilterDefinition),
	L"ratty pitch shift",
	NULL,
	L"Scales the pitch of audio by a fixed ratio. This filter uses an awful time-domain based algorithm "
		L"that may result in some clicks.",
	0,

	sizeof(VDAudioFilterPitchShift),	1,	1,

	&VDAudioFilterData_PitchShift::members.info,

	VDAudioFilterPitchShift::InitProc,
	VDAudioFilterPitchShift::DestroyProc,
	VDAudioFilterPitchShift::PrepareProc,
	VDAudioFilterPitchShift::StartProc,
	VDAudioFilterPitchShift::StopProc,
	VDAudioFilterPitchShift::RunProc,
	VDAudioFilterPitchShift::ReadProc,
	VDAudioFilterPitchShift::SeekProc,
	VDAudioFilterPitchShift::SerializeProc,
	VDAudioFilterPitchShift::DeserializeProc,
	VDAudioFilterPitchShift::GetParamProc,
	VDAudioFilterPitchShift::SetParamProc,
	VDAudioFilterPitchShift::ConfigProc,
};
