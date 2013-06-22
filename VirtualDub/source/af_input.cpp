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
#include <vd2/system/strutil.h>
#include <vd2/system/fraction.h>

#include "filter.h"
#include "AudioSource.h"
#include "af_base.h"

///////////////////////////////////////////////////////////////////////////

class VDAudioDecompressorW32 {
public:
	VDAudioDecompressorW32();
	~VDAudioDecompressorW32();

	void Init(const WAVEFORMATEX *pSrcFormat);
	void Shutdown();

	unsigned	GetInputLevel() const { return mBufferHdr.cbSrcLength; }
	const WAVEFORMATEX *GetOutputFormat() const { return mpDstFormat; }

	void Convert();

	void		*LockInputBuffer(unsigned& bytes);
	void		UnlockInputBuffer(unsigned bytes);
	unsigned	GetOutputLevel();
	unsigned	CopyOutput(void *dst, unsigned bytes);

protected:
	HACMSTREAM		mhStream;
	WAVEFORMATEX	*mpDstFormat;
	ACMSTREAMHEADER mBufferHdr;
	char mDriverName[64];
	char mDriverFilename[64];

	unsigned		mOutputReadPt;
	bool			mbFirst;

	std::vector<char>	mInputBuffer;
	std::vector<char>	mOutputBuffer;

	enum { kInputBufferSize = 16384 };
};

VDAudioDecompressorW32::VDAudioDecompressorW32()
	: mhStream(NULL)
	, mpDstFormat(NULL)
	, mOutputReadPt(0)
{
	mDriverName[0] = 0;
	mDriverFilename[0] = 0;
}

VDAudioDecompressorW32::~VDAudioDecompressorW32() {
	Shutdown();
}

void VDAudioDecompressorW32::Init(const WAVEFORMATEX *pSrcFormat) {
	Shutdown();

	DWORD dwDstFormatSize;

	if (acmMetrics(NULL, ACM_METRIC_MAX_SIZE_FORMAT, (LPVOID)&dwDstFormatSize))
		throw MyError("Couldn't get ACM's max format size");

	mpDstFormat = (WAVEFORMATEX *)allocmem(dwDstFormatSize);
	if (!mpDstFormat)
		throw MyMemoryError();

	mpDstFormat->wFormatTag	= WAVE_FORMAT_PCM;

	if (acmFormatSuggest(NULL, (WAVEFORMATEX *)pSrcFormat, mpDstFormat, dwDstFormatSize, ACM_FORMATSUGGESTF_WFORMATTAG)) {
		Shutdown();
		throw MyError("No audio decompressor could be found to decompress the source audio format.");
	}

	// sanitize the destination format a bit

	if (mpDstFormat->wBitsPerSample!=8 && mpDstFormat->wBitsPerSample!=16)
		mpDstFormat->wBitsPerSample=16;

	if (mpDstFormat->nChannels!=1 && mpDstFormat->nChannels!=2)
		mpDstFormat->nChannels = 2;

	mpDstFormat->nBlockAlign		= (mpDstFormat->wBitsPerSample/8) * mpDstFormat->nChannels;
	mpDstFormat->nAvgBytesPerSec	= mpDstFormat->nBlockAlign * mpDstFormat->nSamplesPerSec;
	mpDstFormat->cbSize				= 0;

	// open conversion stream

	MMRESULT res;

	memset(&mBufferHdr, 0, sizeof mBufferHdr);	// Do this so we can detect whether the buffer is prepared or not.

	res = acmStreamOpen(&mhStream, NULL, (WAVEFORMATEX *)pSrcFormat, mpDstFormat, NULL, 0, 0, ACM_STREAMOPENF_NONREALTIME);

	if (res) {
		Shutdown();

		if (res == ACMERR_NOTPOSSIBLE) {
			throw MyError(
						"Error initializing audio stream decompression:\n"
						"The requested conversion is not possible.\n"
						"\n"
						"Check to make sure you have the required codec%s."
						,
						(pSrcFormat->wFormatTag&~1)==0x160 ? " (Microsoft Audio Codec)" : ""
					);
		} else
			throw MyError("Error initializing audio stream decompression.");
	}

	DWORD dwDstBufferSize;

	if (acmStreamSize(mhStream, kInputBufferSize, &dwDstBufferSize, ACM_STREAMSIZEF_SOURCE)) {
		memset(&mBufferHdr, 0, sizeof mBufferHdr);
		throw MyError("Error initializing audio stream output size.");
	}

	mInputBuffer.resize(kInputBufferSize);
	mOutputBuffer.resize(dwDstBufferSize);

	mBufferHdr.cbStruct		= sizeof(ACMSTREAMHEADER);
	mBufferHdr.pbSrc		= (LPBYTE)&mInputBuffer.front();
	mBufferHdr.cbSrcLength	= mInputBuffer.size();
	mBufferHdr.pbDst		= (LPBYTE)&mOutputBuffer.front();
	mBufferHdr.cbDstLength	= mOutputBuffer.size();

	if (acmStreamPrepareHeader(mhStream, &mBufferHdr, 0)) {
		memset(&mBufferHdr, 0, sizeof mBufferHdr);
		throw MyError("Error preparing audio decompression buffers.");
	}

	mBufferHdr.cbSrcLength = 0;
	mBufferHdr.cbDstLengthUsed = 0;
	mbFirst	= true;

	// try to get driver name for debugging purposes (OK to fail)
	mDriverName[0] = mDriverFilename[0] = 0;

	HACMDRIVERID hDriverID;
	if (!acmDriverID((HACMOBJ)mhStream, &hDriverID, 0)) {
		ACMDRIVERDETAILS add = { sizeof(ACMDRIVERDETAILS) };
		if (!acmDriverDetails(hDriverID, &add, 0)) {
			strncpyz(mDriverName, add.szLongName, sizeof mDriverName);
			strncpyz(mDriverFilename, add.szShortName, sizeof mDriverFilename);
		}
	}
}

void VDAudioDecompressorW32::Shutdown() {
	free(mpDstFormat);
	mpDstFormat = NULL;

	if (mhStream) {
		if (mBufferHdr.fdwStatus & ACMSTREAMHEADER_STATUSF_PREPARED) {
			mBufferHdr.cbSrcLength = mInputBuffer.size();
			mBufferHdr.cbDstLength = mOutputBuffer.size();
			acmStreamUnprepareHeader(mhStream, &mBufferHdr, 0);
		}
		acmStreamClose(mhStream, 0);
		mhStream = NULL;
	}

	mDriverName[0] = 0;
	mDriverFilename[64] = 0;
}

void *VDAudioDecompressorW32::LockInputBuffer(unsigned& bytes) {
	unsigned space = mInputBuffer.size() - mBufferHdr.cbSrcLength;

	bytes = space;
	return &mInputBuffer[mBufferHdr.cbSrcLength];
}

void VDAudioDecompressorW32::UnlockInputBuffer(unsigned bytes) {
	mBufferHdr.cbSrcLength += bytes;
}

void VDAudioDecompressorW32::Convert() {
	VDASSERT(mOutputReadPt >= mBufferHdr.cbDstLengthUsed);		// verify all output data used

	mBufferHdr.cbSrcLengthUsed = 0;
	mBufferHdr.cbDstLengthUsed = 0;

	vdprotected2("decompressing audio", const char *, mDriverName, const char *, mDriverFilename) {
		if (mBufferHdr.cbSrcLength)
			if (MMRESULT res = acmStreamConvert(mhStream, &mBufferHdr, (mbFirst ? ACM_STREAMCONVERTF_START : 0) | ACM_STREAMCONVERTF_BLOCKALIGN))
				throw MyError("ACM reported error on audio decompress (%lx)", res);
	}

	mbFirst = false;
	mOutputReadPt = 0;

	// if ACM didn't use all the source data, copy the remainder down

	if (mBufferHdr.cbSrcLengthUsed < mBufferHdr.cbSrcLength) {
		long left = mBufferHdr.cbSrcLength - mBufferHdr.cbSrcLengthUsed;

		memmove(&mInputBuffer.front(), &mInputBuffer[mBufferHdr.cbSrcLengthUsed], left);

		mBufferHdr.cbSrcLength = left;
	} else
		mBufferHdr.cbSrcLength = 0;
}

unsigned VDAudioDecompressorW32::GetOutputLevel() {
	return mBufferHdr.cbDstLengthUsed - mOutputReadPt;
}

unsigned VDAudioDecompressorW32::CopyOutput(void *dst, unsigned bytes) {
	bytes = std::min<unsigned>(bytes, mBufferHdr.cbDstLengthUsed - mOutputReadPt);

	memcpy(dst, &mOutputBuffer[mOutputReadPt], bytes);

	mOutputReadPt += bytes;
	return bytes;
}

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterInput : public VDAudioFilterBase {
public:
	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();
	uint32 Read(unsigned pin, void *dst, uint32 samples);
	sint64 Seek(sint64);

	VDRingBuffer<char> mOutputBuffer;

	AudioSource *mpSrc;
	unsigned		mPos;
	unsigned		mLimit;
	bool			mbDecompressionActive;

	VDAudioDecompressorW32	mDecompressor;
};

void __cdecl VDAudioFilterInput::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterInput;
}

extern AudioSource *inputAudio;

uint32 VDAudioFilterInput::Prepare() {
	mpSrc = inputAudio;

	if (!mpSrc)
		throw MyError("No audio source is available for the \"input\" audio filter.");

	const WAVEFORMATEX *pwfex = mpSrc->getWaveFormat();

	mDecompressor.Shutdown();
	mbDecompressionActive = false;

	if (pwfex->wFormatTag != WAVE_FORMAT_PCM) {
		mDecompressor.Init(pwfex);
		pwfex = mDecompressor.GetOutputFormat();
		mbDecompressionActive = true;
	}

	VDASSERT(pwfex->wFormatTag == WAVE_FORMAT_PCM);

	VDWaveFormat *pwf = mpContext->mpServices->AllocCustomWaveFormat(0);

	if (!pwf)
		mpContext->mpServices->ExceptOutOfMemory();

	memcpy(pwf, pwfex, sizeof(PCMWAVEFORMAT));

	mpContext->mpOutputs[0]->mGranularity	= 1;
	mpContext->mpOutputs[0]->mpFormat		= pwf;
	mpContext->mpOutputs[0]->mbVBR			= false;

	return 0;
}

void VDAudioFilterInput::Start() {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	mOutputBuffer.Init(format.mBlockSize * pin.mBufferSize);

	mPos		= 0;
	mLimit	= mpSrc->lSampleLast;

	const WAVEFORMATEX& srcFormat = *mpSrc->getWaveFormat();

	pin.mLength = VDFraction(srcFormat.nAvgBytesPerSec, srcFormat.nBlockAlign).scale64ir(mLimit * (sint64)1000000);
}

uint32 VDAudioFilterInput::Run() {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	if (mbDecompressionActive) {
		unsigned bytes = mDecompressor.GetOutputLevel();

		if (bytes > 0) {
			int count = bytes;
			void *dst = mOutputBuffer.LockWrite(count, count);

			count = mDecompressor.CopyOutput(dst, count);
			mOutputBuffer.UnlockWrite(count);

			pin.mCurrentLevel = mOutputBuffer.getLevel() / format.mBlockSize;

			return 0;
		}

		void *dst = mDecompressor.LockInputBuffer(bytes);

		if (bytes && mPos < mLimit) {
			LONG actualbytes, samples;
			int res = mpSrc->read(mPos, bytes, dst, bytes, &actualbytes, &samples);

			VDASSERT(res != AVIERR_BUFFERTOOSMALL);

			if (res)
				throw MyError("Read error on audio sample %u. The source may be corrupted.", (unsigned)mPos);

			mPos += samples;
			mDecompressor.UnlockInputBuffer(actualbytes);

			if (bytes)
				return 0;
		}

		if (mDecompressor.GetInputLevel())
			mDecompressor.Convert();

		return mPos >= mLimit && !mDecompressor.GetOutputLevel() ? kVFARun_Finished : 0;
	} else {
		int count = pin.mBufferSize;

		void *dst = mOutputBuffer.LockWrite(count, count);
		LONG bytes, samples;

//		VDDEBUG("reading %d x %d\n", mPos, count);

		int res = mpSrc->read(mPos, count, dst, count, &bytes, &samples);

		if (res)
			throw MyError("Read error on audio sample %u. The source may be corrupted.", (unsigned)mPos);

		mOutputBuffer.UnlockWrite(bytes);

		pin.mCurrentLevel = mOutputBuffer.getLevel() / format.mBlockSize;

		mPos += samples;

		return mPos >= mLimit ? kVFARun_Finished : 0;
	}
}

uint32 VDAudioFilterInput::Read(unsigned pinid, void *dst, uint32 samples) {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	samples = std::min<uint32>(samples, mOutputBuffer.getLevel() / format.mBlockSize);

	if (dst) {
		mOutputBuffer.Read((char *)dst, samples * format.mBlockSize);
		mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel() / mpContext->mpOutputs[0]->mpFormat->mBlockSize;
	}

	return samples;
}

sint64 VDAudioFilterInput::Seek(sint64 us) {
	mOutputBuffer.Flush();
	mpContext->mpOutputs[0]->mCurrentLevel = 0;
	mPos = mpSrc->msToSamples((long)(us/1000));
	return us;
}

extern const struct VDAudioFilterDefinition afilterDef_input = {
	sizeof(VDAudioFilterDefinition),
	L"input",
	NULL,
	L"Produces audio from current audio source.",
	0,
	kVFAF_Zero,

	sizeof(VDAudioFilterInput),	0,	1,

	NULL,

	VDAudioFilterInput::InitProc,
	VDAudioFilterInput::DestroyProc,
	VDAudioFilterInput::PrepareProc,
	VDAudioFilterInput::StartProc,
	VDAudioFilterInput::StopProc,
	VDAudioFilterInput::RunProc,
	VDAudioFilterInput::ReadProc,
	VDAudioFilterInput::SeekProc,
	VDAudioFilterInput::SerializeProc,
	VDAudioFilterInput::DeserializeProc,
	VDAudioFilterInput::GetParamProc,
	VDAudioFilterInput::SetParamProc,
	VDAudioFilterInput::ConfigProc,
};

///////////////////////////////////////////////////////////////////////////

#include "AVIAudioOutput.h"

class VDAudioFilterPlayback : public VDAudioFilterBase {
public:
	VDAudioFilterPlayback();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();

	uint32 Read(unsigned pin, void *dst, uint32 samples);

	AVIAudioOutput mAudioOut;
};

VDAudioFilterPlayback::VDAudioFilterPlayback()
	: mAudioOut(16384, 2)
{
}

void __cdecl VDAudioFilterPlayback::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterPlayback;
}

uint32 VDAudioFilterPlayback::Prepare() {
	mpContext->mpInputs[0]->mGranularity	= 1;
	mpContext->mpInputs[0]->mDelay		= 0;
	return 0;
}

void VDAudioFilterPlayback::Start() {
	const VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	mAudioOut.init((WAVEFORMATEX *)&format);
	mAudioOut.start();
}

uint32 VDAudioFilterPlayback::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDWaveFormat& format = *pin.mpFormat;
	char buf[16384];

	uint32 samples = mpContext->mpInputs[0]->mpReadProc(mpContext->mpInputs[0], buf, sizeof buf / format.mBlockSize, false);

	if (samples)
		mAudioOut.write(buf, samples*format.mBlockSize, INFINITE);
	else if (pin.mbEnded)
		return kVFARun_Finished;


	return 0;
}

uint32 VDAudioFilterPlayback::Read(unsigned pin, void *dst, uint32 samples) {
	return 0;
}

extern const struct VDAudioFilterDefinition afilterDef_playback = {
	sizeof(VDAudioFilterDefinition),
	L"*playback",
	NULL,
	L"",
	0,
	kVFAF_Zero,

	sizeof(VDAudioFilterPlayback),	1,	0,

	NULL,

	VDAudioFilterPlayback::InitProc,
	VDAudioFilterPlayback::DestroyProc,
	VDAudioFilterPlayback::PrepareProc,
	VDAudioFilterPlayback::StartProc,
	VDAudioFilterPlayback::StopProc,
	VDAudioFilterPlayback::RunProc,
	VDAudioFilterPlayback::ReadProc,
	VDAudioFilterPlayback::SeekProc,
	VDAudioFilterPlayback::SerializeProc,
	VDAudioFilterPlayback::DeserializeProc,
	VDAudioFilterPlayback::GetParamProc,
	VDAudioFilterPlayback::SetParamProc,
	VDAudioFilterPlayback::ConfigProc,
};

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterButterfly : public VDAudioFilterBase {
public:
	VDAudioFilterButterfly();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();

	uint32 Read(unsigned pin, void *dst, uint32 samples);

	sint64 Seek(sint64);

	VDRingBuffer<char> mOutputBuffer;
};

VDAudioFilterButterfly::VDAudioFilterButterfly()
{
}

void __cdecl VDAudioFilterButterfly::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterButterfly;
}

uint32 VDAudioFilterButterfly::Prepare() {
	mpContext->mpInputs[0]->mGranularity	= 1;
	mpContext->mpOutputs[0]->mGranularity = 1;
	if (!(mpContext->mpOutputs[0]->mpFormat = mpContext->mpServices->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat)))
		mpContext->mpServices->ExceptOutOfMemory();
	return 0;
}

void VDAudioFilterButterfly::Start() {
	const VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	mOutputBuffer.Init(format.mBlockSize * pin.mBufferSize);
}

uint32 VDAudioFilterButterfly::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	int samples;

	char *dst = mOutputBuffer.LockWrite(mOutputBuffer.getSize(), samples);

	samples /= format.mBlockSize;

	if (!samples)
		return pin.mbEnded ? kVFARun_Finished : 0;

	samples = mpContext->mpInputs[0]->mpReadProc(mpContext->mpInputs[0], dst, samples, false);

	if (samples) {
		short *p = (short *)dst;

		for(int i=0; i<samples; ++i) {
			const int x = p[0];
			const int y = p[1];

			p[0] = (x+y)>>1;
			p[1] = (x-y)>>1;

			p += 2;
		}

		mOutputBuffer.UnlockWrite(samples * format.mBlockSize);
	}
	mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel() / mpContext->mpOutputs[0]->mpFormat->mBlockSize;

	return 0;
}

uint32 VDAudioFilterButterfly::Read(unsigned pinno, void *dst, uint32 samples) {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	samples = std::min<uint32>(samples, mOutputBuffer.getLevel() / format.mBlockSize);

	if (dst) {
		mOutputBuffer.Read((char *)dst, samples * format.mBlockSize);
		mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel() / mpContext->mpOutputs[0]->mpFormat->mBlockSize;
	}

	return samples;
}

sint64 VDAudioFilterButterfly::Seek(sint64 us) {
	mOutputBuffer.Flush();
	mpContext->mpOutputs[0]->mCurrentLevel = 0;
	return us;
}

extern const struct VDAudioFilterDefinition afilterDef_butterfly = {
	sizeof(VDAudioFilterDefinition),
	L"butterfly",
	NULL,
	L"Computes the half-sum and half-difference between stereo channels. This can be used to "
		L"split stereo into mid/side signals or recombine mid/side into stereo.",
	0,
	kVFAF_Zero,

	sizeof(VDAudioFilterButterfly),	1,	1,

	NULL,

	VDAudioFilterButterfly::InitProc,
	VDAudioFilterButterfly::DestroyProc,
	VDAudioFilterButterfly::PrepareProc,
	VDAudioFilterButterfly::StartProc,
	VDAudioFilterButterfly::StopProc,
	VDAudioFilterButterfly::RunProc,
	VDAudioFilterButterfly::ReadProc,
	VDAudioFilterButterfly::SeekProc,
	VDAudioFilterButterfly::SerializeProc,
	VDAudioFilterButterfly::DeserializeProc,
	VDAudioFilterButterfly::GetParamProc,
	VDAudioFilterButterfly::SetParamProc,
	VDAudioFilterButterfly::ConfigProc,
};

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterStereoSplit : public VDAudioFilterBase {
public:
	VDAudioFilterStereoSplit();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();

	uint32 Read(unsigned pin, void *dst, uint32 samples);

	sint64 Seek(sint64);

	VDRingBuffer<char> mOutputBuffer[2];
};

VDAudioFilterStereoSplit::VDAudioFilterStereoSplit()
{
}

void __cdecl VDAudioFilterStereoSplit::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterStereoSplit;
}

uint32 VDAudioFilterStereoSplit::Prepare() {
	const VDWaveFormat& format0 = *mpContext->mpInputs[0]->mpFormat;

	if (   format0.mTag != VDWaveFormat::kTagPCM
		|| format0.mChannels != 2
		|| (format0.mSampleBits != 8 && format0.mSampleBits != 16)
		)
		return kVFAPrepare_BadFormat;

	mpContext->mpInputs[0]->mGranularity	= 1;
	mpContext->mpInputs[0]->mDelay		= 0;
	mpContext->mpOutputs[0]->mGranularity = 1;
	mpContext->mpOutputs[1]->mGranularity = 1;

	VDWaveFormat *pwf0, *pwf1;

	if (!(mpContext->mpOutputs[0]->mpFormat = pwf0 = mpContext->mpServices->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat)))
		mpContext->mpServices->ExceptOutOfMemory();
	if (!(mpContext->mpOutputs[1]->mpFormat = pwf1 = mpContext->mpServices->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat)))
		mpContext->mpServices->ExceptOutOfMemory();
	pwf0->mChannels = 1;
	pwf1->mChannels = 1;
	pwf0->mBlockSize = pwf0->mSampleBits>>3;
	pwf1->mBlockSize = pwf1->mSampleBits>>3;
	pwf0->mDataRate	= pwf0->mBlockSize * pwf0->mSamplingRate;
	pwf1->mDataRate	= pwf1->mBlockSize * pwf1->mSamplingRate;
	return 0;
}

void VDAudioFilterStereoSplit::Start() {
	const VDAudioFilterPin& pin0 = *mpContext->mpOutputs[0];
	const VDAudioFilterPin& pin1 = *mpContext->mpOutputs[1];
	const VDWaveFormat& format0 = *pin0.mpFormat;
	const VDWaveFormat& format1 = *pin1.mpFormat;

	mOutputBuffer[0].Init(format0.mBlockSize * pin0.mBufferSize);
	mOutputBuffer[1].Init(format1.mBlockSize * pin1.mBufferSize);
}

uint32 VDAudioFilterStereoSplit::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	const VDWaveFormat& format = *pin.mpFormat;

	int samples, actual = 0;

	void *dst1 = (short *)mOutputBuffer[0].LockWrite(mOutputBuffer[0].getSize(), samples);
	void *dst2 = (short *)mOutputBuffer[1].LockWrite(samples, samples);

	samples /= mpContext->mpOutputs[0]->mpFormat->mBlockSize;

	while(samples > 0) {
		union {
			sint16	w[4096];
			uint8	b[8192];
		} buf;
		int tc = mpContext->mpInputs[0]->mpReadProc(mpContext->mpInputs[0], &buf, std::min<int>(samples, sizeof buf / format.mBlockSize), false);

		if (tc<=0)
			break;

		switch(format.mSampleBits) {
		case 8:
			{
				uint8 *dst1w = (uint8 *)dst1;
				uint8 *dst2w = (uint8 *)dst2;

				for(int i=0; i<tc; ++i) {
					*dst1w++ = buf.b[i*2+0];
					*dst2w++ = buf.b[i*2+1];
				}

				dst1 = dst1w;
				dst2 = dst2w;
			}
			break;
		case 16:
			{
				sint16 *dst1w = (sint16 *)dst1;
				sint16 *dst2w = (sint16 *)dst2;

				for(int i=0; i<tc; ++i) {
					*dst1w++ = buf.w[i*2+0];
					*dst2w++ = buf.w[i*2+1];
				}

				dst1 = dst1w;
				dst2 = dst2w;
			}
			break;
		}

		actual += tc;
		samples -= tc;
	}

	mOutputBuffer[0].UnlockWrite(actual * mpContext->mpOutputs[0]->mpFormat->mBlockSize);
	mOutputBuffer[1].UnlockWrite(actual * mpContext->mpOutputs[1]->mpFormat->mBlockSize);

	mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer[0].getLevel() / mpContext->mpOutputs[0]->mpFormat->mBlockSize;
	mpContext->mpOutputs[1]->mCurrentLevel = mOutputBuffer[1].getLevel() / mpContext->mpOutputs[1]->mpFormat->mBlockSize;

	return !actual && pin.mbEnded ? kVFARun_Finished : 0;
}

uint32 VDAudioFilterStereoSplit::Read(unsigned pinno, void *dst, uint32 samples) {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[pinno];
	const VDWaveFormat& format = *pin.mpFormat;

	samples = std::min<uint32>(samples, mOutputBuffer[pinno].getLevel() / format.mBlockSize);

	if (dst) {
		mOutputBuffer[pinno].Read((char *)dst, samples * format.mBlockSize);
		mpContext->mpOutputs[pinno]->mCurrentLevel = mOutputBuffer[pinno].getLevel() / mpContext->mpOutputs[pinno]->mpFormat->mBlockSize;
	}

	return samples;
}

sint64 VDAudioFilterStereoSplit::Seek(sint64 us) {
	mOutputBuffer[0].Flush();
	mpContext->mpOutputs[0]->mCurrentLevel = 0;
	mOutputBuffer[1].Flush();
	mpContext->mpOutputs[1]->mCurrentLevel = 0;
	return us;
}

extern const struct VDAudioFilterDefinition afilterDef_stereosplit = {
	sizeof(VDAudioFilterDefinition),
	L"stereo split",
	NULL,
	L"Splits a stereo stream into two mono streams, one per channel.",
	0,
	kVFAF_Zero,

	sizeof(VDAudioFilterStereoSplit),	1,	2,

	NULL,

	VDAudioFilterStereoSplit::InitProc,
	VDAudioFilterStereoSplit::DestroyProc,
	VDAudioFilterStereoSplit::PrepareProc,
	VDAudioFilterStereoSplit::StartProc,
	VDAudioFilterStereoSplit::StopProc,
	VDAudioFilterStereoSplit::RunProc,
	VDAudioFilterStereoSplit::ReadProc,
	VDAudioFilterStereoSplit::SeekProc,
	VDAudioFilterStereoSplit::SerializeProc,
	VDAudioFilterStereoSplit::DeserializeProc,
	VDAudioFilterStereoSplit::GetParamProc,
	VDAudioFilterStereoSplit::SetParamProc,
	VDAudioFilterStereoSplit::ConfigProc,
};

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterStereoMerge : public VDAudioFilterBase {
public:
	VDAudioFilterStereoMerge();

	static void __cdecl InitProc(const VDAudioFilterContext *pContext);

	uint32 Prepare();
	uint32 Run();
	void Start();

	uint32 Read(unsigned pin, void *dst, uint32 samples);

	sint64 Seek(sint64);

	VDRingBuffer<char> mOutputBuffer;
};

VDAudioFilterStereoMerge::VDAudioFilterStereoMerge()
{
}

void __cdecl VDAudioFilterStereoMerge::InitProc(const VDAudioFilterContext *pContext) {
	new (pContext->mpFilterData) VDAudioFilterStereoMerge;
}

uint32 VDAudioFilterStereoMerge::Prepare() {
	const VDWaveFormat& format0 = *mpContext->mpInputs[0]->mpFormat;
	const VDWaveFormat& format1 = *mpContext->mpInputs[1]->mpFormat;

	if (   format0.mTag != VDWaveFormat::kTagPCM
		|| format0.mChannels != 1
		|| (format0.mSampleBits != 8 && format0.mSampleBits != 16)
		|| format1.mTag != VDWaveFormat::kTagPCM
		|| format1.mChannels != 1
		|| (format1.mSampleBits != 8 && format0.mSampleBits != 16)
		|| format0.mSamplingRate != format1.mSamplingRate
		|| format0.mSampleBits != format1.mSampleBits
		)
		return kVFAPrepare_BadFormat;

	mpContext->mpInputs[0]->mGranularity		= 1;
	mpContext->mpInputs[0]->mDelay			= 0;
	mpContext->mpInputs[1]->mGranularity		= 1;
	mpContext->mpInputs[1]->mDelay			= 0;
	mpContext->mpOutputs[0]->mGranularity	= 1;

	VDWaveFormat *pwf0;

	if (!(mpContext->mpOutputs[0]->mpFormat = pwf0 = mpContext->mpServices->CopyWaveFormat(mpContext->mpInputs[0]->mpFormat)))
		mpContext->mpServices->ExceptOutOfMemory();

	pwf0->mChannels = 2;
	pwf0->mBlockSize = pwf0->mSampleBits>>2;
	pwf0->mDataRate	= pwf0->mBlockSize * pwf0->mSamplingRate;

	return 0;
}

void VDAudioFilterStereoMerge::Start() {
	const VDAudioFilterPin& pin0 = *mpContext->mpOutputs[0];
	const VDWaveFormat& format0 = *pin0.mpFormat;

	mOutputBuffer.Init(format0.mBlockSize * pin0.mBufferSize);
}

uint32 VDAudioFilterStereoMerge::Run() {
	VDAudioFilterPin& pin = *mpContext->mpInputs[0];
	VDAudioFilterPin& pin2 = *mpContext->mpInputs[1];
	const VDWaveFormat& format = *pin.mpFormat;

	int samples, actual = 0;

	void *dst = mOutputBuffer.LockWrite(mOutputBuffer.getSize(), samples);

	samples /= mpContext->mpOutputs[0]->mpFormat->mBlockSize;

	if (samples > mpContext->mInputSamples)
		samples = mpContext->mInputSamples;

	if (!samples && pin.mbEnded && pin2.mbEnded)
		return true;

	while(samples > 0) {
		union {
			sint16	w[4096];
			uint8	b[8192];
		} buf0, buf1;
		int tc = std::min<int>(samples, sizeof buf0 / format.mBlockSize);

		int tca0 = mpContext->mpInputs[0]->mpReadProc(mpContext->mpInputs[0], &buf0, tc, false);
		int tca1 = mpContext->mpInputs[1]->mpReadProc(mpContext->mpInputs[1], &buf1, tc, false);

		VDASSERT(tc == tca0 && tc == tca1);

		switch(format.mSampleBits) {
		case 8:
			{
				uint8 *dstb = (uint8 *)dst;
				for(int i=0; i<tc; ++i) {
					*dstb++ = buf0.b[i];
					*dstb++ = buf1.b[i];
				}
				dst = dstb;
			}
			break;
		case 16:
			{
				sint16 *dstw = (sint16 *)dst;
				for(int i=0; i<tc; ++i) {
					*dstw++ = buf0.w[i];
					*dstw++ = buf1.w[i];
				}
				dst = dstw;
			}
			break;
		}

		actual += tc;
		samples -= tc;
	}

	mOutputBuffer.UnlockWrite(actual * mpContext->mpOutputs[0]->mpFormat->mBlockSize);

	mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel() / mpContext->mpOutputs[0]->mpFormat->mBlockSize;

	return 0;
}

uint32 VDAudioFilterStereoMerge::Read(unsigned pinno, void *dst, uint32 samples) {
	VDAudioFilterPin& pin = *mpContext->mpOutputs[pinno];
	const VDWaveFormat& format = *pin.mpFormat;

	samples = std::min<uint32>(samples, mOutputBuffer.getLevel() / format.mBlockSize);

	if (dst) {
		mOutputBuffer.Read((char *)dst, samples * format.mBlockSize);
		mpContext->mpOutputs[0]->mCurrentLevel = mOutputBuffer.getLevel() / mpContext->mpOutputs[0]->mpFormat->mBlockSize;
	}

	return samples;
}

sint64 VDAudioFilterStereoMerge::Seek(sint64 us) {
	mOutputBuffer.Flush();
	mpContext->mpOutputs[0]->mCurrentLevel = 0;
	return us;
}

extern const struct VDAudioFilterDefinition afilterDef_stereomerge = {
	sizeof(VDAudioFilterDefinition),
	L"stereo merge",
	NULL,
	L"Recombines two mono streams into a single stereo stream.",
	0,
	kVFAF_Zero,

	sizeof(VDAudioFilterStereoMerge),	2, 1,

	NULL,

	VDAudioFilterStereoMerge::InitProc,
	VDAudioFilterStereoMerge::DestroyProc,
	VDAudioFilterStereoMerge::PrepareProc,
	VDAudioFilterStereoMerge::StartProc,
	VDAudioFilterStereoMerge::StopProc,
	VDAudioFilterStereoMerge::RunProc,
	VDAudioFilterStereoMerge::ReadProc,
	VDAudioFilterStereoMerge::SeekProc,
	VDAudioFilterStereoMerge::SerializeProc,
	VDAudioFilterStereoMerge::DeserializeProc,
	VDAudioFilterStereoMerge::GetParamProc,
	VDAudioFilterStereoMerge::SetParamProc,
	VDAudioFilterStereoMerge::ConfigProc,
};

///////////////////////////////////////////////////////////////////////////

#include "AudioFilterSystem.h"

extern const struct VDAudioFilterDefinition afilterDef_lowpass;

void DoAudioFilterTest() {
	VDAudioFilterSystem afs;

	IVDAudioFilterInstance *afi1 = afs.Create(&afilterDef_input);
	IVDAudioFilterInstance *afi2 = afs.Create(&afilterDef_lowpass);
	IVDAudioFilterInstance *afi3 = afs.Create(&afilterDef_playback);

	afs.Connect(afi3, 0, afi2, 0);
	afs.Connect(afi2, 0, afi1, 0);

	afs.Start();
	for(;;)
		afs.Run();
}
