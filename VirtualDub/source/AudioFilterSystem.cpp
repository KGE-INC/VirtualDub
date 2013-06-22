#include "stdafx.h"
#include <vector>
#include <list>
#include <utility>

#include <vd2/system/vdalloc.h>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/Error.h>

#include "filter.h"
#include "filters.h"
#include "AudioFilterSystem.h"

extern FilterFunctions g_filterFuncs;

namespace {
	uint32 ceildiv64to32u(uint64 x, uint64 y) {
		return (unsigned)((x+y-1)/y);
	}
}

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
	bool IsConnected() const { return mpPin != 0; }

	void Connect(VDAudioFilterInstance *pFilt, unsigned pin);

	void ResetBufferConfiguration();
	void PullBufferConfiguration();
	void PushBufferConfiguration();

	void EqualizeDelay(sint32 nTargetDelay);

	sint32 OutputDelay() const;

protected:
	static uint32 __cdecl ReadData(VDAudioFilterPin *pPin, void *dst, uint32 samples, bool bAllowFill);

	VDAudioFilterInstance	*mpFilter;
	VDAudioFilterPinImpl	*mpPin;
	unsigned				mPinNumber;
	sint32					mAddedDelay;
};

///////////////////////////////////////////////////////////////////////////

class VDAudioFilterInstance : protected VDAudioFilterContext, public VDSchedulerNode, public IVDAudioFilterInstance {
	VDAudioFilterInstance& operator=(const VDAudioFilterInstance&);		// prohibited
public:
	VDAudioFilterInstance(const VDAudioFilterDefinition *pDef);
	~VDAudioFilterInstance();

	unsigned&	SortKey() { return mSortKey; }

	unsigned		InputPinCount() const { return mpDefinition->mInputPins; }
	unsigned		OutputPinCount() const { return mpDefinition->mOutputPins; }
	VDAudioFilterPinImpl& InputPin(unsigned p) { return mPins[p]; }
	VDAudioFilterPinImpl& OutputPin(unsigned p) { return mPins[p + mpDefinition->mInputPins]; }

	const VDAudioFilterDefinition *GetDefinition() { return mpDefinition; }

	uint32 Prepare();
	void Start();
	void Stop();

	bool Configure(VDGUIHandle hParent);
	void SerializeConfig(VDFilterConfig& config);
	void DeserializeConfig(const VDFilterConfig& config);
	void *GetObject();

	uint32 ReadData(unsigned pin, void *dst, uint32 samples, bool bAllowFill);

	void EqualizeDelay();
	sint32 OutputDelay() const { return mTotalDelay; }

	void Seek(sint64 us);

protected:
	bool Service();

	std::vector<char>				mFilterData;
	std::vector<VDAudioFilterPin *> mPinPtrs;
	std::vector<VDAudioFilterPinImpl>	mPins;

	VDStringA	mDebugName;

	unsigned	mSortKey;
	sint32		mTotalDelay;
	bool		mbEnded;
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
	mGranularity	= 0;
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

uint32 __cdecl VDAudioFilterPinImpl::ReadData(VDAudioFilterPin *pPin0, void *dst, uint32 samples, bool bAllowFill) {
	VDAudioFilterPinImpl *pPin = static_cast<VDAudioFilterPinImpl *>(pPin0);
	uint32 actual = pPin->mpPin->mpFilter->ReadData(pPin->mpPin->mPinNumber, dst, samples, bAllowFill);

	pPin->mCurrentLevel = pPin->mpPin->mCurrentLevel;
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

VDAudioFilterInstance::VDAudioFilterInstance(const VDAudioFilterDefinition *pDef) 
	: mFilterData(pDef->mFilterDataSize)
	, mPinPtrs(pDef->mInputPins + pDef->mOutputPins)
	, mPins(pDef->mInputPins + pDef->mOutputPins)
	, mDebugName(VDTextWToA(pDef->pszName))
{
	for(unsigned i=0; i<mPins.size(); ++i) {
		mPinPtrs[i] = &mPins[i];
		mPins[i].SetFilter(this, i>=pDef->mInputPins ? i-pDef->mInputPins : i);
	}

	mpFilterData	= &mFilterData[0];
	mpInputs		= &mPinPtrs[0];
	mpOutputs		= &mPinPtrs[pDef->mInputPins];
	mpServices		= &g_filterFuncs;
	mpDefinition	= pDef;
	mAPIVersion		= VIRTUALDUB_FILTERDEF_VERSION;

	mpDefinition->mpInit(this);
}

VDAudioFilterInstance::~VDAudioFilterInstance() {
	mpDefinition->mpDestroy(this);
}

uint32 VDAudioFilterInstance::Prepare() {
	return mpDefinition->mpPrepare(this);
}

void VDAudioFilterInstance::Start() {
	mbEnded = false;

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
		OutputPin(j).mLength = max_us;
	}

	mpDefinition->mpStart(this);
}

void VDAudioFilterInstance::Stop() {
	mpDefinition->mpStop(this);
}

bool VDAudioFilterInstance::Configure(VDGUIHandle hParent) {
	if (!mpDefinition->mpConfig)
		return false;

	return mpDefinition->mpConfig(this, (HWND)hParent);
}

void VDAudioFilterInstance::SerializeConfig(VDFilterConfig& config) {
	config.clear();

	const VDFilterConfigEntry *pEnt = mpDefinition->mpConfigInfo;

	if (pEnt) {
		for(; pEnt->next; pEnt = pEnt->next) {
			switch(pEnt->type) {
			case VDFilterConfigEntry::kTypeU32:
				{
					uint32 v;
					mpDefinition->mpGetParam(this, pEnt->idx, &v, sizeof(uint32));
					config[pEnt->idx].SetU32(v);
				}
				break;
			case VDFilterConfigEntry::kTypeS32:
				{
					uint32 v;
					mpDefinition->mpGetParam(this, pEnt->idx, &v, sizeof(sint32));
					config[pEnt->idx].SetS32(v);
				}
				break;
			case VDFilterConfigEntry::kTypeU64:
				{
					uint32 v;
					mpDefinition->mpGetParam(this, pEnt->idx, &v, sizeof(uint64));
					config[pEnt->idx].SetU64(v);
				}
				break;
			case VDFilterConfigEntry::kTypeS64:
				{
					uint32 v;
					mpDefinition->mpGetParam(this, pEnt->idx, &v, sizeof(sint64));
					config[pEnt->idx].SetS64(v);
				}
				break;
			case VDFilterConfigEntry::kTypeDouble:
				{
					double v;
					mpDefinition->mpGetParam(this, pEnt->idx, &v, sizeof(double));
					config[pEnt->idx].SetDouble(v);
				}
				break;
			case VDFilterConfigEntry::kTypeAStr:
				{
					uint32 l = mpDefinition->mpGetParam(this, pEnt->idx, NULL, 0);
					std::vector<char> tmp(l);
					mpDefinition->mpGetParam(this, pEnt->idx, &tmp.front(), l);
					config[pEnt->idx].SetAStr(&tmp.front());
				}
				break;
			case VDFilterConfigEntry::kTypeWStr:
				{
					uint32 l = mpDefinition->mpGetParam(this, pEnt->idx, NULL, 0);
					std::vector<char> tmp(l);
					mpDefinition->mpGetParam(this, pEnt->idx, &tmp.front(), l);
					config[pEnt->idx].SetWStr((const wchar_t *)&tmp.front());
				}
				break;
			case VDFilterConfigEntry::kTypeBlock:
				{
					uint32 l = mpDefinition->mpGetParam(this, pEnt->idx, NULL, 0);
					std::vector<char> tmp(l);
					mpDefinition->mpGetParam(this, pEnt->idx, &tmp.front(), l);
					config[pEnt->idx].SetBlock(&tmp.front(), l);
				}
				break;
			}
		}
	}
}

void VDAudioFilterInstance::DeserializeConfig(const VDFilterConfig& config) {
	VDFilterConfig::const_iterator it(config.begin()), itEnd(config.end());

	for(; it!=itEnd; ++it) {
		const unsigned idx = (*it).first;
		const VDFilterConfigVariant& var = (*it).second;

		switch(var.GetType()) {
		case VDFilterConfigVariant::kTypeU32:		mpDefinition->mpSetParam(this, idx, &var.GetU32(), sizeof(uint32)); break;
		case VDFilterConfigVariant::kTypeS32:		mpDefinition->mpSetParam(this, idx, &var.GetS32(), sizeof(sint32)); break;
		case VDFilterConfigVariant::kTypeU64:		mpDefinition->mpSetParam(this, idx, &var.GetU64(), sizeof(uint64)); break;
		case VDFilterConfigVariant::kTypeS64:		mpDefinition->mpSetParam(this, idx, &var.GetS64(), sizeof(sint64)); break;
		case VDFilterConfigVariant::kTypeDouble:	mpDefinition->mpSetParam(this, idx, &var.GetDouble(), sizeof(double)); break;
		case VDFilterConfigVariant::kTypeAStr:		mpDefinition->mpSetParam(this, idx, var.GetAStr(), strlen(var.GetAStr())+1); break;
		case VDFilterConfigVariant::kTypeWStr:		mpDefinition->mpSetParam(this, idx, var.GetWStr(), (wcslen(var.GetWStr())+1)*sizeof(wchar_t)); break;
		case VDFilterConfigVariant::kTypeBlock:		mpDefinition->mpSetParam(this, idx, var.GetBlockPtr(), var.GetBlockLen()); break;
		default:
			VDASSERT(false);
		}
	}
}

void *VDAudioFilterInstance::GetObject() {
	return mpFilterData;
}

uint32 VDAudioFilterInstance::ReadData(unsigned pin, void *dst, uint32 samples, bool bAllowFill) {
	uint32 actual = mpDefinition->mpRead(this, pin, dst, samples);

	if (actual || !mbEnded)
		return actual;

	if (!bAllowFill)
		return 0;

	if (dst) {
		const VDWaveFormat& format = *mpOutputs[pin]->mpFormat;

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

	mInputSamples	= 0x7fffffff;
	mInputGranules	= 0x7fffffff;
	mInputsEnded = 0;

	for(int i=0; i<mpDefinition->mInputPins; ++i) {
		VDAudioFilterPinImpl& inpin			= InputPin(i);
		VDAudioFilterPinImpl& inpinconn		= *inpin.Connection();

		unsigned	samples		= inpinconn.mCurrentLevel;
		unsigned	granules	= samples / inpin.mGranularity;

		inpin.mCurrentLevel = samples;
		inpin.mbEnded		= inpinconn.mbEnded;

		if (mInputSamples > samples)
			mInputSamples = samples;

		if (mInputGranules > samples)
			mInputGranules = samples;

		if (inpin.mbEnded)
			++mInputsEnded;
	}

	// run the filter

	uint32 res;

	vdprotected1("running audio filter \"%s\"", const char *, mDebugName.c_str()) {
		res = mpDefinition->mpRun(this);
	}

	if (res & kVFARun_Finished) {
		mbEnded = true;

		VDDEBUG("AudioFilterSystem: Filter \"%ls\" has finished.\n", mpDefinition->pszName);
	}

	for(int j=0; j<mpDefinition->mOutputPins; ++j) {
		OutputPin(j).mbEnded = mbEnded;
	}

	return !mbEnded;
}

void VDAudioFilterInstance::EqualizeDelay() {
	sint32 nTargetDelay = 0;

	for(unsigned i=0; i<mpDefinition->mInputPins; ++i)
		nTargetDelay = std::max<sint32>(nTargetDelay, mPins[i].OutputDelay());

	for(unsigned j=0; j<mpDefinition->mInputPins; ++j)
		mPins[j].EqualizeDelay(nTargetDelay);

	mTotalDelay = nTargetDelay;

	VDDEBUG("AudioFilterSystem: Filter \"%-30ls\": delay %d us\n", mpDefinition->pszName, mTotalDelay);
}

void VDAudioFilterInstance::Seek(sint64 us) {
	us = mpDefinition->mpSeek(this, us);

	for(unsigned i=0; i<mpDefinition->mInputPins; ++i)
		InputPin(i).mbEnded = false;

	mInputsEnded = 0;

	if (mpDefinition->mInputPins)
		mPins[0].Connection()->Filter()->Seek(us);
}

///////////////////////////////////////////////////////////////////////////

VDAudioFilterSystem::VDAudioFilterSystem() {
}

VDAudioFilterSystem::~VDAudioFilterSystem() {
	Clear();
}

IVDAudioFilterInstance *VDAudioFilterSystem::Create(const VDAudioFilterDefinition *pDef) {
	vdautoptr<VDAudioFilterInstance> pFilter(new VDAudioFilterInstance(pDef));

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

	VDDEBUG("[AudioFilter] %s[%d] -> %s[%d]\n", VDTextWToA(pFilterIn->GetDefinition()->pszName).c_str(), nPinIn, VDTextWToA(pFilterOut->GetDefinition()->pszName).c_str(), nPinOut);
}

void VDAudioFilterSystem::LoadFromGraph(const VDAudioFilterGraph& graph, std::vector<IVDAudioFilterInstance *>& filterPtrs) {
	Clear();

	filterPtrs.clear();

	// construct filters

	int connidx = 0;

	for(std::list<VDAudioFilterGraph::FilterEntry>::const_iterator it(graph.mFilters.begin()), itEnd(graph.mFilters.end()); it!=itEnd; ++it) {
		const VDAudioFilterGraph::FilterEntry& f = *it;

		const VDAudioFilterDefinition *pDef = VDLookupAudioFilterByName(f.mFilterName.c_str());
		if (!pDef)
			throw MyError("Cannot load audio filter \"%s\" specified in filter graph.", VDTextWToA(f.mFilterName).c_str());

		if (pDef->mInputPins != f.mInputPins || pDef->mOutputPins != f.mOutputPins)
			throw MyError("Audio filter \"%s\" has a different number of pins than specified in filter graph.", VDTextWToA(pDef->pszName).c_str());

		IVDAudioFilterInstance *pInst = Create(pDef);

		pInst->DeserializeConfig(f.mConfig);

		filterPtrs.push_back(pInst);

		if (connidx + f.mInputPins > graph.mConnections.size())
			throw MyInternalError("Audio filter graph has too few connections\n(%s:%d)", __FILE__, __LINE__);

		for(int i=0; i<f.mInputPins; ++i) {
			const VDAudioFilterGraph::FilterConnection& conn = graph.mConnections[connidx];

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
	vdprotected("preparing audio filter chain") {
		Sort();

		for(tFilterList::const_iterator it(mFilters.begin()), itEnd(mFilters.end()); it!=itEnd; ++it) {
			VDAudioFilterInstance *pInst = *it;
			unsigned i, n;

			for(i=0, n=pInst->InputPinCount(); i<n; ++i) {
				if (!pInst->InputPin(i).IsConnected())
					throw MyError("Input pin %d of audio filter \"%s\" is unconnected.", i, VDTextWToA(pInst->GetDefinition()->pszName).c_str());
				pInst->InputPin(i).ResetBufferConfiguration();
				pInst->InputPin(i).PullBufferConfiguration();

				VDASSERT(pInst->InputPin(i).mpFormat);
			}

			for(i=0, n=pInst->OutputPinCount(); i<n; ++i) {
				if (!pInst->OutputPin(i).IsConnected())
					throw MyError("Output pin %d of audio filter \"%s\" is unconnected.", i, VDTextWToA(pInst->GetDefinition()->pszName).c_str());
				pInst->OutputPin(i).ResetBufferConfiguration();
			}

			uint32 rv = pInst->Prepare();

			if (rv == kVFAPrepare_BadFormat)
				throw MyError("Audio filter \"%s\" cannot handle its input. Check that the filter is designed to handle the audio format you are attempting to process.",
					VDTextWToA(pInst->GetDefinition()->pszName).c_str());

			pInst->EqualizeDelay();

			for(i=0, n=pInst->InputPinCount(); i<n; ++i) {
				pInst->InputPin(i).PushBufferConfiguration();
				VDDEBUG("AudioFilterSystem: Filter %s pin %d: size %d\n", VDTextWToA(pInst->GetDefinition()->pszName).c_str(), i, pInst->InputPin(i).mBufferSize);
			}

	#ifdef _DEBUG
			for(i=0, n=pInst->OutputPinCount(); i<n; ++i) {
				VDASSERT(pInst->OutputPin(i).mpFormat);

				const VDWaveFormat& f = *pInst->OutputPin(i).mpFormat;

				if (f.mTag == VDWaveFormat::kTagPCM) {
					VDASSERT(!(f.mSampleBits & 7));
					VDASSERT(f.mChannels > 0);
					VDASSERT(f.mChannels * (f.mSampleBits>>3) == f.mBlockSize);
					VDASSERT(f.mBlockSize * f.mSamplingRate == f.mDataRate);
				}
			}
	#endif
		}
	}
}

void VDAudioFilterSystem::Start() {
	Prepare();

	vdprotected("starting audio filter chain") {
		for(tFilterList::const_iterator it(mFilters.begin()), itEnd(mFilters.end()); it!=itEnd; ++it) {
			VDAudioFilterInstance *pInst = *it;

			pInst->Start();
			mStartedFilters.push_back(pInst);
		}
	}
}

bool VDAudioFilterSystem::Run() {
	return mFilterScheduler.Run();
}

void VDAudioFilterSystem::Stop() {
	while(!mStartedFilters.empty()) {
		VDAudioFilterInstance *pInst = mStartedFilters.front();
		mStartedFilters.pop_front();
		pInst->Stop();
	}
}

void VDAudioFilterSystem::Seek(sint64 us) {
	for(tFilterList::const_iterator it(mStartedFilters.begin()), itEnd(mStartedFilters.end()); it!=itEnd; ++it) {
		VDAudioFilterInstance *pInst = *it;

		if (pInst->InputPinCount() && !pInst->OutputPinCount())
			pInst->Seek(us);
	}	
}

///////////////////////////////////////////////////////////////////////////

std::list<const VDAudioFilterDefinition *> g_audioFilterList;

void VDAddAudioFilter(const VDAudioFilterDefinition *pDef) {
	g_audioFilterList.push_back(pDef);
}

void VDEnumerateAudioFilters(std::list<VDAudioFilterBlurb>& blurbs) {
	std::list<const VDAudioFilterDefinition *>::const_iterator it(g_audioFilterList.begin()), itEnd(g_audioFilterList.end());

	for(; it != itEnd; ++it) {
		const VDAudioFilterDefinition *pDef = *it;
		blurbs.push_back(VDAudioFilterBlurb());
		VDAudioFilterBlurb& b = blurbs.back();

		b.pDef				= pDef;
		b.name				= pDef->pszName;
		b.author			= pDef->pszAuthor ? pDef->pszAuthor : L"(internal)";
		b.description		= pDef->pszDescription;
	}
}

const VDAudioFilterDefinition *VDLookupAudioFilterByName(const wchar_t *name) {
	std::list<const VDAudioFilterDefinition *>::const_iterator it(g_audioFilterList.begin()), itEnd(g_audioFilterList.end());

	for(; it != itEnd; ++it) {
		const VDAudioFilterDefinition *pDef = *it;

		if (!wcscmp(name, pDef->pszName))
			return pDef;
	}

	return NULL;
}
