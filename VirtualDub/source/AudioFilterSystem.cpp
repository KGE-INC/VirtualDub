#include "stdafx.h"
#include <vector>
#include <list>
#include <utility>

#include <vd2/system/vdalloc.h>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/Error.h>
#include <vd2/system/VDRingBuffer.h>
#include <vd2/system/fraction.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdaudiofilt.h>

#include "filter.h"
#include "filters.h"
#include "AudioFilterSystem.h"
#include "audioutil.h"
#include "plugins.h"

extern FilterFunctions g_filterFuncs;

namespace {
	uint32 ceildiv64to32u(uint64 x, uint64 y) {
		return (unsigned)((x+y-1)/y);
	}
}

///////////////////////////////////////////////////////////////////////////

VDWaveFormat *VDAllocPCMWaveFormat(unsigned sampling_rate, unsigned channels, unsigned bits, bool bFloat) {
	VDWaveFormat *pwf = (VDWaveFormat *)malloc(sizeof(VDWaveFormat));

	if (pwf) {
		pwf->mTag			= VDWaveFormat::kTagPCM;
		pwf->mChannels		= (uint16)channels;
		pwf->mSamplingRate	= sampling_rate;
		pwf->mDataRate		= sampling_rate * channels * (bits>>3);
		pwf->mBlockSize		= (uint16)((bits>>3) * channels);
		pwf->mSampleBits	= (uint16)bits;
		pwf->mExtraSize		= 0;
	}

	return pwf;
}

VDWaveFormat *VDAllocCustomWaveFormat(unsigned extra_size) {
	VDWaveFormat *pwf = (VDWaveFormat *)malloc(sizeof(VDWaveFormat) + extra_size);

	if (pwf)
		pwf->mExtraSize = (uint16)extra_size;

	return pwf;
}

VDWaveFormat *VDCopyWaveFormat(const VDWaveFormat *pFormat) {
	const unsigned size = sizeof(VDWaveFormat) + pFormat->mExtraSize;

	VDWaveFormat *pwf = (VDWaveFormat *)malloc(size);
	if (pwf)
		memcpy(pwf, pFormat, size);

	return pwf;
}

void VDFreeWaveFormat(const VDWaveFormat *pFormat) {
	free((void *)pFormat);
}

const VDAudioFilterCallbacks g_audioFilterCallbacks = {
	VDAllocPCMWaveFormat,
	VDAllocCustomWaveFormat,
	VDCopyWaveFormat,
	VDFreeWaveFormat
};

///////////////////////////////////////////////////////////////////////////

VDFilterConfigVariant::VDFilterConfigVariant(const VDFilterConfigVariant& x) : mType(x.mType), mData(x.mData) {
	switch(mType) {
	case kTypeAStr:
		SetAStr(x.mData.vsa.s);
		break;
	case kTypeWStr:
		SetWStr(x.mData.vsw.s);
		break;
	case kTypeBlock:
		SetBlock(x.mData.vb.s, x.mData.vb.len);
		break;
	}
}

VDFilterConfigVariant::~VDFilterConfigVariant() {
	Clear();
}

VDFilterConfigVariant& VDFilterConfigVariant::operator=(const VDFilterConfigVariant& x) {
	if (this != &x) {
		switch(mType = x.mType) {
		case kTypeAStr:
		case kTypeWStr:
		case kTypeBlock:
			this->~VDFilterConfigVariant();
			new(this) VDFilterConfigVariant(x);
			break;
		default:
			mData = x.mData;
			break;
		}
	}
	return *this;
}

void VDFilterConfigVariant::Clear() {
	switch(mType) {
	case kTypeAStr:
		delete[] mData.vsa.s;
		break;
	case kTypeWStr:
		delete[] mData.vsw.s;
		break;
	case kTypeBlock:
		delete[] mData.vb.s;
		break;
	}

	mType = kTypeInvalid;
}

void VDFilterConfigVariant::SetAStr(const char *s) {
	Clear();
	mType = kTypeAStr;

	size_t l = strlen(s);
	mData.vsa.s = new char[l+1];
	memcpy(mData.vsa.s, s, sizeof(char) * (l+1));
}

void VDFilterConfigVariant::SetWStr(const wchar_t *s) {
	Clear();
	mType = kTypeWStr;

	size_t l = wcslen(s);
	mData.vsw.s = new wchar_t[l+1];
	memcpy(mData.vsw.s, s, sizeof(wchar_t) * (l+1));
}

void VDFilterConfigVariant::SetBlock(const void *s, unsigned b) {
	Clear();
	mType = kTypeBlock;

	mData.vb.s = (char *)malloc(b);
	mData.vb.len = b;
	memcpy(mData.vb.s, s, b);
}

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterPinImpl : public VDAudioFilterPin {
public:
	VDAudioFilterPinImpl();
	~VDAudioFilterPinImpl();

	void SetFilter(VDAudioFilterInstance *pInst, unsigned pin) { mpFilter = pInst; mPinNumber = pin; }

	VDAudioFilterInstance *Filter() const { return mpFilter; }
	VDAudioFilterPinImpl *Connection() const { return mpPin; }
	VDAudioFilterInstance *ConnectedFilter() const { return mpPin->mpFilter; }
	int Number() const { return mPinNumber; }
	bool IsConnected() const { return mpPin != 0; }
	int GetFormat() const { return mFormat; }
	void SetFormat(int f) { mFormat = f; }

	void Connect(VDAudioFilterInstance *pFilt, unsigned pin);

	void ResetBufferConfiguration();
	void PullBufferConfiguration();
	void PushBufferConfiguration();

	void EqualizeDelay(sint32 nTargetDelay);

	sint32 OutputDelay() const;

	// Change detection
	void MarkForReadCheck() {
		mMarkedLevel = 0;
	}

	bool CheckForRead() const {
		return mMarkedLevel != 0;
	}

protected:
	static uint32 __cdecl ReadData(VDAudioFilterPin *pPin, void *dst, uint32 samples, bool bAllowFill, int dstFormat);

	VDAudioFilterInstance	*mpFilter;
	VDAudioFilterPinImpl	*mpPin;
	unsigned				mPinNumber;
	sint32					mAddedDelay;
	int						mFormat;
	VDAtomicInt				mMarkedLevel;
};

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterInstance : protected VDAudioFilterContext, public VDSchedulerNode, public IVDAudioFilterInstance {
	VDAudioFilterInstance& operator=(const VDAudioFilterInstance&);		// prohibited
public:
	VDAudioFilterInstance(VDPluginDescription *pDef);
	~VDAudioFilterInstance();

	unsigned&	SortKey() { return mSortKey; }

	unsigned		InputPinCount() const { return mpDefinition->mInputPins; }
	unsigned		OutputPinCount() const { return mpDefinition->mOutputPins; }
	VDAudioFilterPinImpl& InputPin(unsigned p) { return mPins[p]; }
	VDAudioFilterPinImpl& OutputPin(unsigned p) { return mPins[p + mpDefinition->mInputPins]; }

	const VDPluginInfo *GetPluginInfo() { return mpPluginInfo; }
	const VDAudioFilterDefinition *GetDefinition() { return mpDefinition; }

	bool IsPrepared() const { return mbPrepared; }

	void Reset();
	uint32 Prepare();
	void Start();
	void Stop();

	bool Configure(VDGUIHandle hParent);
	void SerializeConfig(VDFilterConfig& config);
	void DeserializeConfig(const VDFilterConfig& config);
	void *GetObject();
	sint64 GetPosition();
	sint64 GetLength();

	uint32 ReadData(unsigned pin, void *dst, uint32 samples, bool bAllowFill, int dstFormat);

	void EqualizeDelay();
	sint32 OutputDelay() const { return mTotalDelay; }

	void Seek(sint64 us);

	bool GetOutputPinFormats(std::vector<VDWaveFormat>& formats);

protected:
	bool Service();
	void DumpStatus();

	VDPluginDescription *mpDescription;
	const VDPluginInfo *mpPluginInfo;

	std::vector<char>				mFilterData;
	std::vector<VDAudioFilterPin *> mPinPtrs;
	std::vector<VDAudioFilterPinImpl>	mPins;
	std::vector<VDRingBuffer<char> > mOutputBuffers;

	VDStringA	mDebugName;

	unsigned	mSortKey;
	sint32		mTotalDelay;
	bool		mbPrepared;
	bool		mbEnded;

	sint64		mLastSeekPoint;
	sint64		mSamplesSinceLastSeekPoint;
};

///////////////////////////////////////////////////////////////////////////

VDAudioFilterPinImpl::VDAudioFilterPinImpl() 
	: mpPin(NULL)
{
	mpFormat = NULL;
	mpReadProc = ReadData;
}

VDAudioFilterPinImpl::~VDAudioFilterPinImpl() {
	free((void *)mpFormat);
}

void VDAudioFilterPinImpl::Connect(VDAudioFilterInstance *pFilt, unsigned pin) {
	mpPin = &pFilt->OutputPin(pin);
	mpPin->mpPin = this;
}

void VDAudioFilterPinImpl::ResetBufferConfiguration() {
	mCurrentLevel	= 0;
	mBufferSize		= 0;
	mGranularity	= 1;
	mbVBR			= false;
	mbEnded			= false;
	mAddedDelay		= 0;
	mDelay			= 0;
	if (mpFormat) {
		free((void *)mpFormat);
		mpFormat = NULL;
	}
}

void VDAudioFilterPinImpl::PullBufferConfiguration() {
	free((void *)mpFormat);
	mpFormat = VDCopyWaveFormat(mpPin->mpFormat);
	if (!mpFormat)
		throw MyMemoryError();

	mBufferSize = mpPin->mBufferSize;
}

void VDAudioFilterPinImpl::PushBufferConfiguration() {
	unsigned out_gran = mpPin->mGranularity;
	unsigned in_gran = mGranularity;

	mBufferSize = in_gran;

	unsigned minsize = (mpFormat->mDataRate + mpFormat->mBlockSize*10 - 1) / (mpFormat->mBlockSize*10);

	if (mBufferSize < minsize)
		mBufferSize = minsize;

	mBufferSize += out_gran;
	mBufferSize -= mBufferSize % out_gran;

	uint32 extra = ceildiv64to32u((uint64)mAddedDelay * mpFormat->mDataRate, (uint64)1000000 * mpFormat->mBlockSize);

	extra = (extra + in_gran - 1);
	extra -= extra % in_gran;
	extra = (extra + out_gran - 1);
	extra -= extra % out_gran;

	mpPin->mBufferSize = mBufferSize + extra;
}

uint32 __cdecl VDAudioFilterPinImpl::ReadData(VDAudioFilterPin *pPin0, void *dst, uint32 samples, bool bAllowFill, int format) {
	VDAudioFilterPinImpl *pPin = static_cast<VDAudioFilterPinImpl *>(pPin0);
	VDAudioFilterPinImpl *pOtherPin = pPin->mpPin;
	const int outputPinNum = pOtherPin->mPinNumber;
	VDAudioFilterInstance *pOtherFilter = pOtherPin->mpFilter;

	uint32 actual = pOtherFilter->ReadData(outputPinNum, dst, samples, bAllowFill, format);

	pPin->mCurrentLevel = pOtherPin->mCurrentLevel;

	// This will result in incorrect values when fill occurs.  But in that case the filter's already
	// done, so we don't care.
	pPin->mMarkedLevel += actual;

	return actual;
}

void VDAudioFilterPinImpl::EqualizeDelay(sint32 nTargetDelay) {
	sint32 nSourceDelay = mpPin->mpFilter->OutputDelay();
	sint32 nDelta = nTargetDelay - (nSourceDelay + mDelay);

	VDASSERT(nDelta >= 0);

	mAddedDelay = nDelta;
}

sint32 VDAudioFilterPinImpl::OutputDelay() const {
	return mpPin->mpFilter->OutputDelay() + mDelay;
}

///////////////////////////////////////////////////////////////////////////

VDAudioFilterInstance::VDAudioFilterInstance(VDPluginDescription *pDesc) 
	: mpDescription(pDesc)
	, mpPluginInfo(NULL)
	, mbPrepared(false)
{
	mpPluginInfo = VDLockPlugin(pDesc);
	mpDefinition = reinterpret_cast<const VDAudioFilterDefinition *>(mpPluginInfo->mpTypeSpecificInfo);

	const int inputPins = mpDefinition->mInputPins;
	const int outputPins = mpDefinition->mOutputPins;
	const int totalPins = inputPins + outputPins;

	mFilterData.resize(mpDefinition->mFilterDataSize);
	mPinPtrs.resize(totalPins);
	mPins.resize(totalPins);
	mOutputBuffers.resize(outputPins);
	mDebugName	= VDTextWToA(pDesc->mName);

	for(unsigned i=0; i<totalPins; ++i) {
		mPinPtrs[i] = &mPins[i];
		mPins[i].SetFilter(this, i>=inputPins ? i-inputPins : i);
	}

	mpFilterData		= &mFilterData[0];
	mpInputs			= &mPinPtrs[0];
	mpOutputs			= &mPinPtrs[inputPins];
	mpServices			= &g_pluginCallbacks;
	mpAudioCallbacks	= &g_audioFilterCallbacks;
	mAPIVersion			= kVDPlugin_APIVersion;

	mpDefinition->mpInit(this);
}

VDAudioFilterInstance::~VDAudioFilterInstance() {
	if (mpPluginInfo) {
		mpDefinition->mpVtbl->mpDestroy(this);
		VDUnlockPlugin(mpDescription);
	}
}

void VDAudioFilterInstance::Reset() {
	mbPrepared = false;
}

uint32 VDAudioFilterInstance::Prepare() {
	sint64 max_us = 0;
	if (mpDefinition->mInputPins) {
		for(unsigned i=0; i<mpDefinition->mInputPins; ++i) {
			VDAudioFilterPinImpl& pin = mPins[i];

			pin.mLength = pin.Connection()->mLength;
			max_us = std::max<sint64>(max_us, pin.mLength);

			VDASSERT(InputPin(i).mpFormat);
		}
	}

	for(unsigned j=0; j<mpDefinition->mOutputPins; ++j) {
		VDAudioFilterPinImpl& pin = OutputPin(j);

		pin.mLength = max_us;
	}

	uint32 rv = mpDefinition->mpVtbl->mpPrepare(this);

	if (!rv)
		mbPrepared = true;

	return rv;
}

void VDAudioFilterInstance::Start() {
	mbEnded = false;

	for(unsigned j=0; j<mpDefinition->mOutputPins; ++j) {
		VDAudioFilterPinImpl& pin = OutputPin(j);

		mOutputBuffers[j].Init(pin.mBufferSize * pin.mpFormat->mBlockSize);
	}

	mpDefinition->mpVtbl->mpStart(this);

	mLastSeekPoint = 0;
	mSamplesSinceLastSeekPoint = 0;
}

void VDAudioFilterInstance::Stop() {
	mpDefinition->mpVtbl->mpStop(this);

	for(unsigned j=0; j<mpDefinition->mOutputPins; ++j) {
		mOutputBuffers[j].Shutdown();
	}
}

bool VDAudioFilterInstance::Configure(VDGUIHandle hParent) {
	if (!mpDefinition->mpVtbl->mpConfig)
		return false;

	bool rv;

	vdprotected1("displaying config dialog for audio filter \"%s\"", const char *, mDebugName.c_str()) {
		rv = mpDefinition->mpVtbl->mpConfig(this, (HWND)hParent);
	}

	return rv;
}

void VDAudioFilterInstance::SerializeConfig(VDFilterConfig& config) {
	config.clear();

	const VDFilterConfigEntry *pEnt = mpDefinition->mpConfigInfo;

	if (pEnt) {
		vdprotected1("retrieving config for audio filter \"%s\"", const char *, mDebugName.c_str()) {
			for(; pEnt->next; pEnt = pEnt->next) {
				switch(pEnt->type) {
				case VDFilterConfigEntry::kTypeU32:
					{
						uint32 v;
						mpDefinition->mpVtbl->mpGetParam(this, pEnt->idx, &v, sizeof(uint32));
						config[pEnt->idx].SetU32(v);
					}
					break;
				case VDFilterConfigEntry::kTypeS32:
					{
						uint32 v;
						mpDefinition->mpVtbl->mpGetParam(this, pEnt->idx, &v, sizeof(sint32));
						config[pEnt->idx].SetS32(v);
					}
					break;
				case VDFilterConfigEntry::kTypeU64:
					{
						uint32 v;
						mpDefinition->mpVtbl->mpGetParam(this, pEnt->idx, &v, sizeof(uint64));
						config[pEnt->idx].SetU64(v);
					}
					break;
				case VDFilterConfigEntry::kTypeS64:
					{
						uint32 v;
						mpDefinition->mpVtbl->mpGetParam(this, pEnt->idx, &v, sizeof(sint64));
						config[pEnt->idx].SetS64(v);
					}
					break;
				case VDFilterConfigEntry::kTypeDouble:
					{
						double v;
						mpDefinition->mpVtbl->mpGetParam(this, pEnt->idx, &v, sizeof(double));
						config[pEnt->idx].SetDouble(v);
					}
					break;
				case VDFilterConfigEntry::kTypeAStr:
					{
						uint32 l = mpDefinition->mpVtbl->mpGetParam(this, pEnt->idx, NULL, 0);
						std::vector<char> tmp(l);
						mpDefinition->mpVtbl->mpGetParam(this, pEnt->idx, &tmp.front(), l);
						config[pEnt->idx].SetAStr(&tmp.front());
					}
					break;
				case VDFilterConfigEntry::kTypeWStr:
					{
						uint32 l = mpDefinition->mpVtbl->mpGetParam(this, pEnt->idx, NULL, 0);
						std::vector<char> tmp(l);
						mpDefinition->mpVtbl->mpGetParam(this, pEnt->idx, &tmp.front(), l);
						config[pEnt->idx].SetWStr((const wchar_t *)&tmp.front());
					}
					break;
				case VDFilterConfigEntry::kTypeBlock:
					{
						uint32 l = mpDefinition->mpVtbl->mpGetParam(this, pEnt->idx, NULL, 0);
						std::vector<char> tmp(l);
						mpDefinition->mpVtbl->mpGetParam(this, pEnt->idx, &tmp.front(), l);
						config[pEnt->idx].SetBlock(&tmp.front(), l);
					}
					break;
				}
			}
		}
	}
}

void VDAudioFilterInstance::DeserializeConfig(const VDFilterConfig& config) {
	vdprotected1("restoring config for audio filter \"%s\"", const char *, mDebugName.c_str()) {
		VDFilterConfig::const_iterator it(config.begin()), itEnd(config.end());

		for(; it!=itEnd; ++it) {
			const unsigned idx = (*it).first;
			const VDFilterConfigVariant& var = (*it).second;

			switch(var.GetType()) {
			case VDFilterConfigVariant::kTypeU32:		mpDefinition->mpVtbl->mpSetParam(this, idx, &var.GetU32(), sizeof(uint32)); break;
			case VDFilterConfigVariant::kTypeS32:		mpDefinition->mpVtbl->mpSetParam(this, idx, &var.GetS32(), sizeof(sint32)); break;
			case VDFilterConfigVariant::kTypeU64:		mpDefinition->mpVtbl->mpSetParam(this, idx, &var.GetU64(), sizeof(uint64)); break;
			case VDFilterConfigVariant::kTypeS64:		mpDefinition->mpVtbl->mpSetParam(this, idx, &var.GetS64(), sizeof(sint64)); break;
			case VDFilterConfigVariant::kTypeDouble:	mpDefinition->mpVtbl->mpSetParam(this, idx, &var.GetDouble(), sizeof(double)); break;
			case VDFilterConfigVariant::kTypeAStr:		mpDefinition->mpVtbl->mpSetParam(this, idx, var.GetAStr(), strlen(var.GetAStr())+1); break;
			case VDFilterConfigVariant::kTypeWStr:		mpDefinition->mpVtbl->mpSetParam(this, idx, var.GetWStr(), (wcslen(var.GetWStr())+1)*sizeof(wchar_t)); break;
			case VDFilterConfigVariant::kTypeBlock:		mpDefinition->mpVtbl->mpSetParam(this, idx, var.GetBlockPtr(), var.GetBlockLen()); break;
			default:
				VDASSERT(false);
			}
		}
	}
}

void *VDAudioFilterInstance::GetObject() {
	return mpFilterData;
}

sint64 VDAudioFilterInstance::GetPosition() {
	const VDWaveFormat *pFormat = NULL;

	if (mpDefinition->mOutputPins)
		pFormat = mpOutputs[0]->mpFormat;
	else if (mpDefinition->mInputPins)
		return InputPin(0).ConnectedFilter()->GetPosition();

	sint64 us = mLastSeekPoint;

	if (pFormat)
		us += VDFraction(pFormat->mSamplingRate, 1000000).scale64ir(mSamplesSinceLastSeekPoint);

	return us;
}

sint64 VDAudioFilterInstance::GetLength() {
	sint64 us = 0;

	const VDAudioFilterPin *pPin = NULL;

	if (mpDefinition->mOutputPins)
		pPin = mpOutputs[0];
	else if (mpDefinition->mInputPins)
		pPin = mpInputs[0];

	if (pPin)
		us = pPin->mLength;

	return us;
}

uint32 VDAudioFilterInstance::ReadData(unsigned nPin, void *dst, uint32 samples, bool bAllowFill, int nFormat) {
	VDAudioFilterPinImpl& pin = OutputPin(nPin);
	uint32 actual;

	vdprotected1("reading samples from audio filter \"%s\"", const char *, mDebugName.c_str()) {
		const VDWaveFormat& format = *pin.mpFormat;
		const int srcFormat = pin.GetFormat();

		if (srcFormat == nFormat || nFormat == kVFARead_Native) {
			VDRingBuffer<char>& buffer = mOutputBuffers[nPin];

			actual = std::min<uint32>(samples, buffer.getLevel() / format.mBlockSize);

			if (dst) {
				buffer.Read((char *)dst, actual * format.mBlockSize);
				pin.mCurrentLevel = buffer.getLevel() / format.mBlockSize;
			}
		} else if (pin.GetFormat() != kVFARead_Native) {
			static const uint32 sBlkSize[]={0,1,2,4};

			uint32 total = 0;
			const uint32 ch			= format.mChannels;
			const uint32 sblksize	= format.mBlockSize * ch;
			const uint32 dblksize	= sBlkSize[nFormat] * ch;

			const tpVDConvertPCMVtbl vtbl = VDGetPCMConversionVtable();
			VDRingBuffer<char>& buffer = mOutputBuffers[nPin];

			uint32 left = samples;
			while(left > 0) {
				int actual;

				const void *p = buffer.LockRead(samples * sblksize * ch, actual);

				if (actual < sblksize)
					break;

				actual /= sblksize;

				vtbl[srcFormat-1][nFormat-1](dst, p, ch * actual);

				buffer.UnlockRead(actual * sblksize * ch);

				left -= actual;
				total += actual;
				dst = (char *)dst + dblksize * actual;
			}

			actual = total;
		} else
			throw MyError("Error in audio filter \"%s\": Attempt to read PCM from non-PCM source", mDebugName.c_str());
	}

	if (actual || !mbEnded)
		return actual;

	if (!bAllowFill)
		return 0;

	if (dst) {
		const VDWaveFormat& format = *mpOutputs[nPin]->mpFormat;
		VDASSERT(format.mTag == VDWaveFormat::kTagPCM);

		unsigned fill = 0;
		if (format.mSampleBits == 8)
			fill = 0x80;

		memset(dst, fill, samples * format.mBlockSize);
	}

	return samples;
}

bool VDAudioFilterInstance::Service() {
	// pull input levels from output pins and compute aggregate input
	// availability

	if (mpDefinition->mInputPins) {
		mInputSamples	= 0x7fffffff;
		mInputGranules	= 0x7fffffff;
	} else {
		mInputSamples	= 0;
		mInputGranules	= 0;
	}
	mInputsEnded	= 0;

	if (mpDefinition->mOutputPins) {
		mOutputSamples	= 0x7fffffff;
		mOutputGranules	= 0x7fffffff;
	} else {
		mOutputSamples	= 0;
		mOutputGranules	= 0;
	}

	int i;
	for(i=0; i<mpDefinition->mInputPins; ++i) {
		VDAudioFilterPinImpl& inpin			= InputPin(i);
		VDAudioFilterPinImpl& inpinconn		= *inpin.Connection();

		unsigned	samples		= inpinconn.mCurrentLevel;
		unsigned	granules	= samples / inpin.mGranularity;

		inpin.mCurrentLevel = samples;
		inpin.mbEnded		= inpinconn.mbEnded;

		if (inpin.mbEnded)
			++mInputsEnded;

		if (!inpin.mbEnded || samples) {
			if (mInputSamples > samples)
				mInputSamples = samples;

			if (mInputGranules > granules)
				mInputGranules = granules;
		}

		inpin.MarkForReadCheck();
	}

	bool bForceFinalRun = false;
	if (mInputsEnded >= mpDefinition->mInputPins) {
		mInputSamples = 0;
		mInputGranules = 0;

		if (!mbEnded)
			bForceFinalRun = true;
	}

	// lock output pins
	for(i=0; i<mpDefinition->mOutputPins; ++i) {
		VDAudioFilterPinImpl& outpin = OutputPin(i);
		int avail;

		outpin.mpBuffer = mOutputBuffers[i].LockWriteAll(avail);
		outpin.mAvailSpace = avail / outpin.mpFormat->mBlockSize;
		outpin.mSamplesWritten = 0;

		if (mOutputSamples > outpin.mAvailSpace)
			mOutputSamples = outpin.mAvailSpace;

		unsigned granules = outpin.mAvailSpace / outpin.mGranularity;

		if (mOutputGranules > granules)
			mOutputGranules = granules;
	}

	if (mpDefinition->mInputPins) {
		mCommonSamples = mInputSamples;
		mCommonGranules = mInputGranules;

		if (mpDefinition->mOutputPins) {
			if (mCommonSamples > mOutputSamples)
				mCommonSamples = mOutputSamples;
			if (mCommonGranules > mOutputGranules)
				mCommonGranules = mOutputGranules;
		}
	} else {
		mCommonSamples = mOutputSamples;
		mCommonGranules = mOutputGranules;
	}

	// run the filter

	uint32 res = 0;

	if (bForceFinalRun || mInputGranules || mOutputGranules) {
		vdprotected1("running audio filter \"%s\"", const char *, mDebugName.c_str()) {
			res = mpDefinition->mpVtbl->mpRun(this);
		}
	}

	if (res & kVFARun_Finished) {
		mbEnded = true;

		VDDEBUG("AudioFilterSystem: Filter \"%ls\" has finished.\n", mpPluginInfo->mpName);
	}

	pScheduler->Lock();

	bool bAnyActivity = 0!=(res & kVFARun_InternalWork);

	for(int k=0; k<mpDefinition->mInputPins; ++k) {
		VDAudioFilterPinImpl& inpin = InputPin(k);

		inpin.mbEnded = mbEnded;

		if (inpin.CheckForRead()) {
			pScheduler->RescheduleFast(inpin.ConnectedFilter());
			bAnyActivity = true;
		}
	}

	for(int j=0; j<mpDefinition->mOutputPins; ++j) {
		VDAudioFilterPinImpl& outpin = OutputPin(j);
		VDRingBuffer<char>& buffer = mOutputBuffers[j];

		outpin.mbEnded = mbEnded;

		if (!j)
			mSamplesSinceLastSeekPoint += outpin.mSamplesWritten;

		buffer.UnlockWrite(outpin.mSamplesWritten * outpin.mpFormat->mBlockSize);

		outpin.mCurrentLevel = buffer.getLevel() / outpin.mpFormat->mBlockSize;

		if (mbEnded || outpin.mSamplesWritten) {
			pScheduler->RescheduleFast(outpin.ConnectedFilter());
			bAnyActivity = true;
		}
	}

	pScheduler->Unlock();

	return !mbEnded && bAnyActivity;
}

void VDAudioFilterInstance::EqualizeDelay() {
	sint32 nTargetDelay = 0;

	for(unsigned i=0; i<mpDefinition->mInputPins; ++i)
		nTargetDelay = std::max<sint32>(nTargetDelay, mPins[i].OutputDelay());

	for(unsigned j=0; j<mpDefinition->mInputPins; ++j)
		mPins[j].EqualizeDelay(nTargetDelay);

	mTotalDelay = nTargetDelay;

	VDDEBUG("AudioFilterSystem: Filter \"%-30ls\": delay %d us\n", mpPluginInfo->mpName, mTotalDelay);
}

void VDAudioFilterInstance::Seek(sint64 us) {
	mLastSeekPoint = us;
	mSamplesSinceLastSeekPoint = 0;

	us = mpDefinition->mpVtbl->mpSeek(this, us);

	for(unsigned i=0; i<mpDefinition->mInputPins; ++i) {
		VDAudioFilterPinImpl& inpin = InputPin(i);
		VDAudioFilterPinImpl& inpinpeer = *inpin.Connection();

		inpin.mbEnded = false;

		// propagate seek only if we're connected to master pin (pin 0)
		if (!inpinpeer.Number())
			inpinpeer.Filter()->Seek(us);
	}

	for(unsigned j=0; j<mpDefinition->mOutputPins; ++j) {
		VDAudioFilterPinImpl& outpin = OutputPin(j);

		outpin.mCurrentLevel = 0;
		mOutputBuffers[j].Flush();
	}

	mInputsEnded = 0;
}

bool VDAudioFilterInstance::GetOutputPinFormats(std::vector<VDWaveFormat>& formats) {
	if (!mbPrepared)
		return false;

	formats.resize(mpDefinition->mOutputPins);

	for(unsigned i=0; i<mpDefinition->mOutputPins; ++i)
		formats[i] = *OutputPin(i).mpFormat;

	return true;
}

void VDAudioFilterInstance::DumpStatus() {
	VDDEBUG2("        Filter \"%s\":\n", mDebugName.c_str());

	for(unsigned j=0; j<mpDefinition->mOutputPins; ++j) {
		const VDRingBuffer<char>& buffer = mOutputBuffers[j];
		const VDAudioFilterPinImpl& pin = OutputPin(j);

		VDDEBUG2("            Output pin %d: %d/%d bytes (%s)\n", j, buffer.getLevel(), buffer.getSize(), pin.mbEnded ? "ended" : "active");
	}
}

///////////////////////////////////////////////////////////////////////////

VDAudioFilterSystem::VDAudioFilterSystem() {
}

VDAudioFilterSystem::~VDAudioFilterSystem() {
	Clear();
}

IVDAudioFilterInstance *VDAudioFilterSystem::Create(VDPluginDescription *pDesc) {
	vdautoptr<VDAudioFilterInstance> pFilter(new VDAudioFilterInstance(pDesc));

	mFilters.push_back(pFilter);

	mFilterScheduler.Add(pFilter);

	return pFilter.release();
}

void VDAudioFilterSystem::Destroy(IVDAudioFilterInstance *pInst0) {
	VDAudioFilterInstance *pInst = static_cast<VDAudioFilterInstance *>(pInst0);
	tFilterList::iterator it(std::find(mFilters.begin(), mFilters.end(), pInst));

	if (it != mFilters.end()) {
		mFilterScheduler.Remove(pInst);
		mFilters.erase(it);
	} else {
		VDASSERT(false);
	}

	delete pInst;
}

void VDAudioFilterSystem::Connect(IVDAudioFilterInstance *pFilterIn, unsigned nPinIn, IVDAudioFilterInstance *pFilterOut, unsigned nPinOut) {
	VDAudioFilterInstance *pIn = static_cast<VDAudioFilterInstance *>(pFilterIn);
	VDAudioFilterInstance *pOut = static_cast<VDAudioFilterInstance *>(pFilterOut);
	pIn->InputPin(nPinIn).Connect(pOut, nPinOut);

	VDDEBUG("[AudioFilter] %s[%d] -> %s[%d]\n", VDTextWToA(pFilterIn->GetPluginInfo()->mpName).c_str(), nPinIn, VDTextWToA(pFilterOut->GetPluginInfo()->mpName).c_str(), nPinOut);
}

void VDAudioFilterSystem::LoadFromGraph(const VDAudioFilterGraph& graph, std::vector<IVDAudioFilterInstance *>& filterPtrs) {
	Clear();

	filterPtrs.clear();

	// construct filters

	int connidx = 0;

	for(std::list<VDAudioFilterGraph::FilterEntry>::const_iterator it(graph.mFilters.begin()), itEnd(graph.mFilters.end()); it!=itEnd; ++it) {
		const VDAudioFilterGraph::FilterEntry& f = *it;

		VDPluginDescription *pDesc = VDGetPluginDescription(f.mFilterName.c_str(), kVDPluginType_Audio);
		if (!pDesc)
			throw MyError("Cannot find audio filter \"%s\" specified in filter graph.", VDTextWToA(f.mFilterName).c_str());

		const VDPluginInfo *pInfo = pDesc->mpInfo;
		const VDAudioFilterDefinition *pDef = reinterpret_cast<const VDAudioFilterDefinition *>(pInfo->mpTypeSpecificInfo);

		if (pDef->mInputPins != f.mInputPins || pDef->mOutputPins != f.mOutputPins)
			throw MyError("Audio filter \"%s\" has a different number of pins than specified in filter graph.", VDTextWToA(pDesc->mName).c_str());

		IVDAudioFilterInstance *pInst = Create(pDesc);

		pInst->DeserializeConfig(f.mConfig);

		filterPtrs.push_back(pInst);

		if (connidx + f.mInputPins > graph.mConnections.size())
			throw MyInternalError("Audio filter graph has too few connections\n(%s:%d)", __FILE__, __LINE__);

		for(int i=0; i<f.mInputPins; ++i) {
			const VDAudioFilterGraph::FilterConnection& conn = graph.mConnections[connidx];

			if (conn.filt < 0)
				throw MyError("Audio filter \"%s\" has unconnected input pins.", VDTextWToA(f.mFilterName).c_str());

			if (conn.filt >= filterPtrs.size())
				throw MyInternalError("Audio filter graph has forward branches\n(%s:%d)", __FILE__, __LINE__);

			IVDAudioFilterInstance *pSrcInst = filterPtrs[conn.filt];

			if (conn.pin >= pSrcInst->GetDefinition()->mOutputPins)
				throw MyInternalError("Audio filter graph has invalid backward connection\n(%s:%d)", __FILE__, __LINE__);

			if (conn.pin >= 0)
				Connect(pInst, i, pSrcInst, conn.pin);

			++connidx;
		}
	}
}

void VDAudioFilterSystem::Clear() {
	Stop();

	while(!mFilters.empty()) {
		VDAudioFilterInstance *pInst = mFilters.front();
		mFilterScheduler.Remove(pInst);
		mFilters.pop_front();
		delete pInst;
	}
}

void VDAudioFilterSystem::SortFilter(tFilterList& newList, VDAudioFilterInstance *pFilt) {
	if (pFilt->SortKey())
		return;

	pFilt->SortKey() = 1;

	const int inputs = pFilt->InputPinCount();
	const int outputs = pFilt->OutputPinCount();

	for(int i=0; i<inputs; ++i)
		if (pFilt->InputPin(i).IsConnected())
			SortFilter(newList, pFilt->InputPin(i).Connection()->Filter());

	newList.push_back(pFilt);

	for(int j=0; j<outputs; ++j)
		if (pFilt->OutputPin(j).IsConnected())
			SortFilter(newList, pFilt->OutputPin(j).Connection()->Filter());
}

void VDAudioFilterSystem::Sort() {
	tFilterList::const_iterator it(mFilters.begin()), itEnd(mFilters.end());
	for(; it!=itEnd; ++it)
		(*it)->SortKey() = 0;

	tFilterList newList;

	for(it=mFilters.begin(); it!=itEnd; ++it)
		SortFilter(newList, *it);

	mFilters.swap(newList);
}

// We need to avoid this nasty problem in our filter graph:
//
// in1 ->|            | ------------------------->|            |
//       |  filter 1  |                           |  filter 3  |
// in2 ->|            | -----> | filter 2 | ----->|            |
//
// where filter2 has a longer delay than the buffer between filter 1
// and filter 3.  This causes a deadlock, where filter 1 cannot run
// because its top buffer is full, even if its bottom buffer is empty.
// The solution is buffer equalization, where we add enough extra
// buffering to the short delay path to overcome the longer delay in
// the other paths.  Since we do not allow cycles in the path, we
// can do so simply by equalizing the delays in the paths.

void VDAudioFilterSystem::Prepare() {
	mpClock = NULL;

	vdprotected("preparing audio filter chain") {
		Sort();

		for(tFilterList::const_iterator it(mFilters.begin()), itEnd(mFilters.end()); it!=itEnd; ++it) {
			VDAudioFilterInstance *pInst = *it;
			unsigned i, n;

			if (!mpClock && !pInst->OutputPinCount())
				mpClock = pInst;

			for(i=0, n=pInst->InputPinCount(); i<n; ++i) {
				if (!pInst->InputPin(i).IsConnected())
					throw MyError("Input pin %d of audio filter \"%s\" is unconnected.", i, VDTextWToA(pInst->GetPluginInfo()->mpName).c_str());
				pInst->InputPin(i).ResetBufferConfiguration();
				pInst->InputPin(i).PullBufferConfiguration();

				VDASSERT(pInst->InputPin(i).mpFormat);
			}

			for(i=0, n=pInst->OutputPinCount(); i<n; ++i) {
				if (!pInst->OutputPin(i).IsConnected())
					throw MyError("Output pin %d of audio filter \"%s\" is unconnected.", i, VDTextWToA(pInst->GetPluginInfo()->mpName).c_str());
				pInst->OutputPin(i).ResetBufferConfiguration();
			}

			uint32 rv = pInst->Prepare();

			if (rv == kVFAPrepare_BadFormat)
				throw MyError("Audio filter \"%s\" cannot handle its input. Check that the filter is designed to handle the audio format you are attempting to process.",
					VDTextWToA(pInst->GetPluginInfo()->mpName).c_str());

			pInst->EqualizeDelay();

			for(i=0, n=pInst->InputPinCount(); i<n; ++i) {
				VDAudioFilterPinImpl& pin = pInst->InputPin(i);

				pin.PushBufferConfiguration();
				VDDEBUG("AudioFilterSystem: Filter %s pin %d: size %d\n", VDTextWToA(pInst->GetPluginInfo()->mpName).c_str(), i, pin.mBufferSize);
			}

			for(i=0, n=pInst->OutputPinCount(); i<n; ++i) {
				VDASSERT(pInst->OutputPin(i).mpFormat);

				VDAudioFilterPinImpl& pin = pInst->OutputPin(i);

				const VDWaveFormat& f = *pin.mpFormat;

				if (f.mTag == VDWaveFormat::kTagPCM) {
					VDASSERT(!(f.mSampleBits & 7));
					VDASSERT(f.mChannels > 0);
					VDASSERT(f.mChannels * (f.mSampleBits>>3) == f.mBlockSize);
					VDASSERT(f.mBlockSize * f.mSamplingRate == f.mDataRate);
				}

				if (f.mTag == VDWaveFormat::kTagPCM) {
					switch(f.mSampleBits) {
					case 8:
						pin.SetFormat(kVFARead_PCM8);
						break;
					case 16:
						pin.SetFormat(kVFARead_PCM16);
						break;
					default:
						pin.SetFormat(kVFARead_Native);
						break;
					}

				} else {
					pin.SetFormat(kVFARead_Native);
				}
			}
		}
	}
}

void VDAudioFilterSystem::Start() {
	Prepare();

	vdsynchronized(mcsStateChange) {
		vdprotected("starting audio filter chain") {
			for(tFilterList::const_iterator it(mFilters.begin()), itEnd(mFilters.end()); it!=itEnd; ++it) {
				VDAudioFilterInstance *pInst = *it;

				pInst->Start();
				mStartedFilters.push_back(pInst);
			}
		}
	}
}

bool VDAudioFilterSystem::Run() {
	if (!mFilterScheduler.Run()) {
		mFilterScheduler.DumpStatus();
		return false;
	}

	return true;
}

void VDAudioFilterSystem::Stop() {
	vdsynchronized(mcsStateChange) {
		while(!mStartedFilters.empty()) {
			VDAudioFilterInstance *pInst = mStartedFilters.front();
			mStartedFilters.pop_front();
			pInst->Stop();
		}
	}
}

void VDAudioFilterSystem::Seek(sint64 us) {
	vdsynchronized(mcsStateChange) {
		Suspend();

		for(tFilterList::const_iterator it(mStartedFilters.begin()), itEnd(mStartedFilters.end()); it!=itEnd; ++it) {
			VDAudioFilterInstance *pInst = *it;

			if (pInst->InputPinCount() && !pInst->OutputPinCount())
				pInst->Seek(us);
		}

		Resume();
	}
}

IVDAudioFilterInstance *VDAudioFilterSystem::GetClock() {
	return mpClock;
}

void VDAudioFilterSystem::SetIdleSignal(VDSignal *pSignal) {
	mFilterScheduler.setSignal(pSignal);
}

void VDAudioFilterSystem::Suspend() {
	for(tFilterList::const_iterator it(mFilters.begin()), itEnd(mFilters.end()); it!=itEnd; ++it) {
		VDAudioFilterInstance *pInst = *it;

		mFilterScheduler.Remove(pInst);
	}
}

void VDAudioFilterSystem::Resume() {
	for(tFilterList::const_iterator it(mFilters.begin()), itEnd(mFilters.end()); it!=itEnd; ++it) {
		VDAudioFilterInstance *pInst = *it;

		mFilterScheduler.Add(pInst);
	}
}
