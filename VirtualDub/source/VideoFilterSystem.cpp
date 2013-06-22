#include "stdafx.h"

#if 0			// Not yet used and references fibers.


#include <list>
#include <vector>
#include <utility>
#include <vd2/system/error.h>
#include <vd2/system/thread.h>
#include <vd2/system/refcount.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/vdstl.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "VideoFilterSystem.h"

///////////////////////////////////////////////////////////////////////////

class VDVideoFrameRequest;

///////////////////////////////////////////////////////////////////////////

class VDVideoFilterFiberScheduler : protected VDThread {
public:
	VDVideoFilterFiberScheduler();
	~VDVideoFilterFiberScheduler();

	void Start();
	void Stop();

	VDScheduler& Scheduler() { return mScheduler; }

protected:
	void ThreadRun();

	VDAtomicInt		mExit;
	VDScheduler		mScheduler;
	VDSignal		mWakeup;
};

VDVideoFilterFiberScheduler::VDVideoFilterFiberScheduler()
	: VDThread("video fibers")
	, mExit(0)
{
	mScheduler.setSignal(&mWakeup);
}

VDVideoFilterFiberScheduler::~VDVideoFilterFiberScheduler() {
}

void VDVideoFilterFiberScheduler::Start() {
	if (!isThreadAttached()) {
		mExit = 0;

		ThreadStart();
	}
}

void VDVideoFilterFiberScheduler::Stop() {
	mExit = 1;
	mWakeup.signal();

	ThreadWait();
}

void VDVideoFilterFiberScheduler::ThreadRun() {
	ConvertThreadToFiber(NULL);

	while(!mExit) {
		if (!mScheduler.Run())
			mWakeup.wait();
	}
}


///////////////////////////////////////////////////////////////////////////

struct VDVideoFilterPixmap {
	VDPixmapBuffer				mBuffer;
	VDAtomicInt					mRefCount;

	VDVideoFilterPixmap() : mRefCount(0) {}

	int AddRef() {
		return mRefCount.inc();
	}

	int Release() {
		if (mRefCount == 1) {
			delete this;
			return 0;
		}

		VDASSERT(mRefCount > 0);

		return mRefCount.dec();
	}
};

///////////////////////////////////////////////////////////////////////////

class IVDVideoFilterFrameCache {
public:
	virtual void NotifyVideoFrameIdle(class VDVideoFilterFrameRef *) = 0;
};

class VDVideoFilterFrameRef : public vdlist<VDVideoFilterFrameRef>::node {
public:
	vdrefptr<VDVideoFilterPixmap>	mpPixmap;
	VDVideoFilterFrame				mFrame;
	IVDVideoFilterFrameCache		*mpCache;
	VDAtomicInt						mRefCount;

	VDVideoFilterFrameRef(IVDVideoFilterFrameCache *pCache);
	~VDVideoFilterFrameRef();

	int AddRef() {
		return mRefCount.inc();
	}

	int Release() {
		int rv = mRefCount.dec();

		if (rv == 1) {
			if (mpCache)
				mpCache->NotifyVideoFrameIdle(this);
		}

		VDASSERT(rv > 0);

		return rv;
	}

	int GetRefCount() const {
		return mRefCount;
	}

protected:
	VDVideoFilterFrame *WriteCopy();

	static inline VDVideoFilterFrameRef *AsThis(VDVideoFilterFrame *pFrame) {
		return (VDVideoFilterFrameRef *)((char *)pFrame - offsetof(VDVideoFilterFrameRef, mFrame));
	}

	static int ExtAddRef(VDVideoFilterFrame *pFrame) { return AsThis(pFrame)->AddRef(); }
	static int ExtRelease(VDVideoFilterFrame *pFrame) { return AsThis(pFrame)->Release(); }
	static VDVideoFilterFrame *ExtCopyWrite(VDVideoFilterFrame *pFrame) { return AsThis(pFrame)->WriteCopy(); }

	static const VDVideoFilterFrameVtbl sVtbl;
};

///////////////////////////////////////////////////////////////////////////

VDVideoFilterFrameRef::VDVideoFilterFrameRef(IVDVideoFilterFrameCache *pCache)
	: mpCache(pCache)
	, mRefCount(0)
{
	mFrame.mpVtbl = &sVtbl;
}

VDVideoFilterFrameRef::~VDVideoFilterFrameRef() {
	VDASSERT(mRefCount == 1);
}

VDVideoFilterFrame *VDVideoFilterFrameRef::WriteCopy() {
	if (mRefCount == 1)
		return &mFrame;

	VDVideoFilterFrameRef *pNewFrame = new VDVideoFilterFrameRef(*this);
	pNewFrame->AddRef();
	Release();
	return &pNewFrame->mFrame;
}

const VDVideoFilterFrameVtbl VDVideoFilterFrameRef::sVtbl={
	ExtAddRef,
	ExtRelease,
	ExtCopyWrite
};

///////////////////////////////////////////////////////////////////////////

class VDVideoFilterFrameCache : public IVDVideoFilterFrameCache {
public:
	VDVideoFilterFrameCache();
	~VDVideoFilterFrameCache();

	void SetLimit(int limit);
	VDVideoFilterFrameRef *AllocFrame();
	void AddFrame(VDVideoFilterFrameRef *);
	VDVideoFilterFrameRef *LookupFrame(sint64 frame);

	void NotifyVideoFrameIdle(VDVideoFilterFrameRef *);

protected:
	void EvictFrames();

	typedef vdlist<VDVideoFilterFrameRef> tFrames;
	tFrames				mIdleFrames;
	tFrames				mActiveFrames;
	tFrames				mPendingFrames;
	VDCriticalSection	mLock;
	int					mTotalFrames;
	int					mFrameLimit;
};

VDVideoFilterFrameCache::VDVideoFilterFrameCache()
	: mTotalFrames(0)
	, mFrameLimit(1)
{
}

VDVideoFilterFrameCache::~VDVideoFilterFrameCache() {
	VDASSERT(mActiveFrames.empty());
	VDASSERT(mPendingFrames.empty());

	while(!mIdleFrames.empty()) {
		VDVideoFilterFrameRef *pFrame = mIdleFrames.back();
		VDVERIFY(!pFrame->Release());
		mIdleFrames.pop_back();
	}
}

void VDVideoFilterFrameCache::SetLimit(int limit) {
	vdsynchronized(mLock) {
		mFrameLimit = limit;
		if (mTotalFrames > mFrameLimit)
			EvictFrames();
	}
}

VDVideoFilterFrameRef *VDVideoFilterFrameCache::AllocFrame() {
	vdsynchronized(mLock) {
		VDVideoFilterFrameRef *pFrame;

		if (mTotalFrames < mFrameLimit || mIdleFrames.empty()) {
			pFrame = new VDVideoFilterFrameRef(this);
			pFrame->AddRef();		// one for the cache....
			++mTotalFrames;
		} else {
			pFrame = mIdleFrames.back();
			mIdleFrames.pop_back();
			VDASSERT(mIdleFrames.end() == mIdleFrames.find(pFrame));
		}

		mPendingFrames.push_front(pFrame);
		return pFrame;
	}
}

void VDVideoFilterFrameCache::AddFrame(VDVideoFilterFrameRef *pFrame) {
	sint64 framenum = pFrame->mFrame.mFrameNum;

	VDDEBUG("VideoFilterFrameCache[%p]: adding frame %ld to cache\n", this, (long)framenum);

	vdsynchronized(mLock) {
		int rc = pFrame->GetRefCount();

		VDASSERT(mPendingFrames.end() != mPendingFrames.find(pFrame));
		mPendingFrames.erase(mPendingFrames.fast_find(pFrame));

		if (rc > 1) {
			mActiveFrames.push_front(pFrame);
			VDASSERT(mIdleFrames.end() == mIdleFrames.find(pFrame));
		} else
			mIdleFrames.push_front(pFrame);

		if (mTotalFrames > mFrameLimit)
			EvictFrames();
	}
}

VDVideoFilterFrameRef *VDVideoFilterFrameCache::LookupFrame(sint64 frame) {
	VDVideoFilterFrameRef *pRef = NULL;

	vdsynchronized(mLock) {
		tFrames::iterator it(mIdleFrames.begin()), itEnd(mIdleFrames.end());

		for(; it!=itEnd; ++it) {
			VDVideoFilterFrameRef& ref = **it;

			if (ref.mFrame.mFrameNum == frame) {
				mActiveFrames.splice(mActiveFrames.begin(), mIdleFrames, it);
				pRef = &ref;
				break;
			}
		}

		if (!pRef) {
			it = mActiveFrames.begin();
			itEnd = mActiveFrames.end();

			for(; it!=itEnd; ++it) {
				VDVideoFilterFrameRef& ref = **it;

				if (ref.mFrame.mFrameNum == frame) {
					pRef = &ref;
					break;
				}
			}
		}
	}

	if (pRef) {
		pRef->AddRef();
		VDDEBUG("VideoFilterFrameCache[%p]: cache hit for frame %ld\n", this, (long)frame);
	} else {
		VDDEBUG("VideoFilterFrameCache[%p]: cache miss for frame %ld\n", this, (long)frame);
	}

	return pRef;
}

void VDVideoFilterFrameCache::NotifyVideoFrameIdle(VDVideoFilterFrameRef *pFrame) {
	VDASSERT(pFrame->GetRefCount() == 1);

	vdsynchronized(mLock) {
		VDASSERT(mActiveFrames.end() != mActiveFrames.find(pFrame));
		tFrames::iterator it(mActiveFrames.fast_find(pFrame));

		mIdleFrames.splice(mIdleFrames.begin(), mActiveFrames, it);

		VDDEBUG("VideoFilterFrameCache[%p]: returning frame %ld to idle state (idle=%d, active=%d, total=%d)\n", this, (long)pFrame->mFrame.mFrameNum, mIdleFrames.size(), mActiveFrames.size(), mTotalFrames);

		if (mTotalFrames > mFrameLimit)
			EvictFrames();
	}
}

void VDVideoFilterFrameCache::EvictFrames() {
	int evicted = 0;

	while(mTotalFrames > mFrameLimit && !mIdleFrames.empty()) {
		VDVideoFilterFrameRef *pFrame = mIdleFrames.back();
		mIdleFrames.pop_back();
		VDASSERT(mActiveFrames.end() == mActiveFrames.find(pFrame));
		VDASSERT(pFrame->GetRefCount() == 1);

		VDDEBUG("VideoFilterFrameCache[%p]: evicting frame %ld\n", this, (long)pFrame->mFrame.mFrameNum);

		delete pFrame;
		--mTotalFrames;
		++evicted;
	}

	VDDEBUG("VideoFilterFrameCache[%p]: evicted %d frames\n", this, evicted);
}

///////////////////////////////////////////////////////////////////////////

struct VDSourceFrameRequest {
	VDVideoFilterFrame		*mpFrame;			// manually reference counted
	IVDVideoFrameRequest	*mpRequest;
};

class IVDVideoFrameRequestCache {
public:
	virtual void NotifyFrameRequestReady(VDVideoFrameRequest *pRequest, bool bSuccessful) = 0;
	virtual void NotifyFrameRequestIdle(VDVideoFrameRequest *pRequest) = 0;
};

class VDVideoFrameRequest : public IVDVideoFrameRequest, public IVDVideoFrameRequestCompleteCallback {
public:
	VDVideoFrameRequest(IVDVideoFrameRequestCache *pCache) : mpCache(pCache), mbReady(false), mbSuccessful(false), mRefCount(1), mOutstandingSourceFrames(0) {}

	int AddRef() {
		VDASSERT(mRefCount < 500);
		return mRefCount.inc();
	}
	int Release() {
		VDASSERT(mRefCount > 1);

		int rc = mRefCount.dec();
		if (rc == 1)
			mpCache->NotifyFrameRequestIdle(this);
		return rc;
	}
	int GetRefCount() const {
		return mRefCount;
	}
	bool IsReady();
	bool IsSuccessful() { return mbSuccessful; }
	bool IsWaiting() {
		return mOutstandingSourceFrames > 0;
	}

	void Lock() {
		++mOutstandingSourceFrames;
	}

	void Unlock() {
		--mOutstandingSourceFrames;
	}

	int GetSourceFrameCount() {
		return mSourceFrames.size();
	}

	void GetSourceFrames(VDVideoFilterFrame **pFrames);

	VDVideoFilterFrame *GetFrame() { return mpFrame; }
	void SetFrame(VDVideoFilterFrame *p) { mpFrame.set(p); }
	void	SetFrameNumber(sint64 f) { mFrameNum = f; }
	sint64	GetFrameNumber() const { return mFrameNum; }

	VDSourceFrameRequest& NewSourceRequest();
	void AddNotifyTarget(IVDVideoFrameRequestCompleteCallback *pTarget, void *pContext);

	void MarkDone();
	void FlushSourceFrames();
	void Reset();

protected:
	void NotifyVideoFrameRequestComplete(IVDVideoFrameRequest *pRequest, bool bSuccessful, void *pToken);

	vdrefptr<VDVideoFilterFrame>	mpFrame;
	sint64							mFrameNum;
	VDAtomicInt						mOutstandingSourceFrames;
	VDAtomicInt						mRefCount;
	bool							mbReady;
	bool							mbSuccessful;

	IVDVideoFrameRequestCache		*mpCache;

	std::list<VDSourceFrameRequest>		mSourceFrames;
	typedef std::list<std::pair<IVDVideoFrameRequestCompleteCallback *, void *> > tNotifyTargets;
	tNotifyTargets mNotifyTargets;
};

bool VDVideoFrameRequest::IsReady() {
	return mbReady;
}

void VDVideoFrameRequest::GetSourceFrames(VDVideoFilterFrame **pFrames) {
	std::list<VDSourceFrameRequest>::const_iterator it(mSourceFrames.begin()), itEnd(mSourceFrames.end());

	for(; it!=itEnd; ++it) {
		const VDSourceFrameRequest& req = *it;

		*pFrames++ = req.mpFrame;
	}
}

VDSourceFrameRequest& VDVideoFrameRequest::NewSourceRequest() {
	VDSourceFrameRequest req = { NULL, NULL };

	mSourceFrames.push_back(req);

	++mOutstandingSourceFrames;

	return mSourceFrames.back();
}

void VDVideoFrameRequest::AddNotifyTarget(IVDVideoFrameRequestCompleteCallback *pTarget, void *pContext) {
	mNotifyTargets.push_back(std::make_pair(pTarget, pContext));
}

void VDVideoFrameRequest::MarkDone() {
	FlushSourceFrames();

	mbReady = true;
	mbSuccessful = true;
	mOutstandingSourceFrames = 0;		// to undo pending lock

	while(!mNotifyTargets.empty()) {
		tNotifyTargets::const_reference tgt = mNotifyTargets.back();

		tgt.first->NotifyVideoFrameRequestComplete(this, mbSuccessful, tgt.second);

		mNotifyTargets.pop_back();
	}
}

void VDVideoFrameRequest::FlushSourceFrames() {
	std::list<VDSourceFrameRequest>::iterator it(mSourceFrames.begin()), itEnd(mSourceFrames.end());
	for(; it != itEnd; ++it) {
		VDSourceFrameRequest& req = *it;

		req.mpFrame->Release();
		req.mpFrame = NULL;
	}
	mSourceFrames.clear();
	mOutstandingSourceFrames = 0;
}

void VDVideoFrameRequest::Reset() {
	FlushSourceFrames();
	mpFrame = NULL;
	mbReady = false;
	mbSuccessful = false;
}

void VDVideoFrameRequest::NotifyVideoFrameRequestComplete(IVDVideoFrameRequest *pRequest, bool bSuccessful, void *pToken) {
	VDSourceFrameRequest *pReq = (VDSourceFrameRequest *)pToken;

	VDASSERT(pRequest == pReq->mpRequest || !pReq->mpRequest);

	VDDEBUG("Video filter request (frame=%ld): received source frame %ld (successful = %d)\n", (long)GetFrameNumber(), (long)pRequest->GetFrame()->mFrameNum, bSuccessful);

	if (!bSuccessful) {
		mbSuccessful = false;
	} else {
		pReq->mpFrame = static_cast<VDVideoFrameRequest *>(pRequest)->GetFrame();
		pReq->mpFrame->AddRef();
	}

	if (pReq->mpRequest) {
		pReq->mpRequest->Release();
		pReq->mpRequest = NULL;
	}

	VDASSERT(mOutstandingSourceFrames > 0);
	if (!mOutstandingSourceFrames.dec())
		mpCache->NotifyFrameRequestReady(this, mbSuccessful);
}

///////////////////////////////////////////////////////////////////////////
//
//	VDVideoFilterInstance
//
///////////////////////////////////////////////////////////////////////////

class VDVideoFilterInstance : public IVDVideoFilterInstance, public VDSchedulerNode, protected IVDVideoFrameRequestCache {
public:
	VDVideoFilterInstance(const VDPluginInfo *pInfo, void *pCreationData);
	~VDVideoFilterInstance();

	void Connect(int pin, VDVideoFilterInstance *pSource);

	int GetSourceCount() const { return mSources.size(); }
	VDVideoFilterInstance *GetSource(int n) const { return mSources[n]; }

	bool IsFiberDependent() const { return mbScheduleAsFiber; }

	bool Config(VDGUIHandle);
	void Prepare();
	void Start();
	void Stop();

	IVDVideoFrameRequest *RequestFrame(sint64 frame, IVDVideoFrameRequestCompleteCallback *pCallback, void *pToken);
	const VDPixmap& GetFormat() {
		return *mContext.mpOutput->mpFormat;
	}

	bool Service();

protected:
	void NotifyFrameRequestReady(VDVideoFrameRequest *pRequest, bool bSuccessful);
	void NotifyFrameRequestIdle(VDVideoFrameRequest *pRequest);

	void ExtPrefetch(int pin, sint64 frameno, uint32 flags);
	void ExtAllocFrame();

	static inline VDVideoFilterInstance *AsThis(const VDVideoFilterContext *pContext) {
		return (VDVideoFilterInstance *)((char *)pContext - offsetof(VDVideoFilterInstance, mContext));
	}

	static void ExtEntryPrefetch(const VDVideoFilterContext *pContext, int pin, sint64 frameno, uint32 flags) {
		AsThis(pContext)->ExtPrefetch(pin, frameno, flags);
	}

	static void ExtEntryAllocFrame(const VDVideoFilterContext *pContext) {
		AsThis(pContext)->ExtAllocFrame();
	}

	const VDPluginInfo			*mpInfo;
	const VDVideoFilterDefinition	*mpDef;

	typedef std::list<VDVideoFrameRequest> tRequests;
	tRequests		mPendingRequests;		// requests that need more frames
	tRequests		mReadyRequests;			// requests that are ready to be processed
	tRequests		mActiveRequests;		// request currently being processed
	tRequests		mCompleteRequests;		// requests that have been processed
	tRequests		mFreeRequests;			// requests ready to be reused

	volatile uint32	mFlags;
	bool			mbSerialized;			// if set, we must serialize requests
	bool			mbScheduleAsFiber;
	VDAtomicInt		mRequestedState;
	VDAtomicInt		mState;

	enum {
		kStateIdle,
		kStatePrepared,
		kStateRunning,
		kStateError
	};

	VDVideoFrameRequest		*mpPrefetchRequest;

	std::vector<VDVideoFilterInstance *>	mSources;
	std::vector<VDVideoFilterFrame *>		mSourceFramePtrs;

	std::vector<VDVideoFilterPin>			mPins;
	std::vector<VDVideoFilterPin *>			mPinPtrs;
	std::vector<VDPixmap>					mPinFormats;

	VDCriticalSection					mLock;
	VDVideoFilterContext				mContext;

	VDVideoFilterFrameCache			mCache;

	VDSignal				mFiberSignal;

	MyError					mError;

	static const VDVideoFilterCallbacks	sVideoCallbacks;
};

const VDVideoFilterCallbacks VDVideoFilterInstance::sVideoCallbacks = {
	ExtEntryPrefetch,
	ExtEntryAllocFrame
};


VDVideoFilterInstance::VDVideoFilterInstance(const VDPluginInfo *pInfo, void *pCreationData)
	: mpInfo(pInfo)
	, mpDef(reinterpret_cast<const VDVideoFilterDefinition *>(pInfo->mpTypeSpecificInfo))
	, mPins(mpDef->mInputPins + mpDef->mOutputPins)
	, mPinPtrs(mpDef->mInputPins + mpDef->mOutputPins)
	, mPinFormats(mpDef->mInputPins + mpDef->mOutputPins)
	, mSources(mpDef->mInputPins)
	, mRequestedState(kStateIdle)
	, mState(kStateIdle)
{
	for(int i=0; i<mpDef->mInputPins + mpDef->mOutputPins; ++i) {
		mPins[i].mpFormat = &mPinFormats[i];
		mPinPtrs[i] = &mPins[i];
	}

	mContext.mpFilterData		= pCreationData;
	mContext.mpDefinition		= mpDef;
	mContext.mpFilterData		= mpDef->mpCreate(&mContext);
	mContext.mpVideoCallbacks	= &sVideoCallbacks;
	mContext.mpInputs			= &mPinPtrs[0];
	mContext.mpOutput			= &mPins[mpDef->mInputPins];

	mCache.SetLimit(8);

	mbScheduleAsFiber = true;
}

VDVideoFilterInstance::~VDVideoFilterInstance() {
	mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Destroy, 0, 0, 0);
}

void VDVideoFilterInstance::Connect(int pin, VDVideoFilterInstance *pSource) {
	VDASSERT((unsigned)pin <= mSources.size());
	mSources[pin] = pSource;
}

bool VDVideoFilterInstance::Config(VDGUIHandle h) {
	HWND hwnd = (HWND)h;

	return 0 != mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Config, 0, &hwnd, sizeof hwnd);
}

void VDVideoFilterInstance::Prepare() {
	const int inpins = mContext.mpDefinition->mInputPins;
	const int outpins = mContext.mpDefinition->mOutputPins;

	for(int i=0; i<mContext.mpDefinition->mInputPins; ++i) {
		VDVideoFilterPin& outpin = *mSources[i]->mContext.mpOutput;
		VDVideoFilterPin& inpin = *mContext.mpInputs[i];

		*inpin.mpFormat		= *outpin.mpFormat;
		inpin.mFrameRateLo	= outpin.mFrameRateLo;
		inpin.mFrameRateHi	= outpin.mFrameRateHi;
		inpin.mLength		= outpin.mLength;
		inpin.mStart		= outpin.mStart;
	}

	if (inpins && outpins) {
		VDVideoFilterPin& inpin = *mContext.mpInputs[0];
		VDVideoFilterPin& outpin = *mContext.mpOutput;

		outpin.mFrameRateLo = inpin.mFrameRateLo;
		outpin.mFrameRateHi = inpin.mFrameRateHi;
		outpin.mLength		= inpin.mLength;
		outpin.mStart		= inpin.mStart;
	}

	mbSerialized = false;
	mbScheduleAsFiber = true;		// FIXME

	mRequestedState = kStatePrepared;
	if (mbScheduleAsFiber) {
		while(mState != kStatePrepared) {
			Reschedule();
			mFiberSignal.wait();

			if (mState == kStateError)
				throw mError;
		}
	} else {
		mFlags = mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Prepare, 0, 0, 0);
		mState = kStatePrepared;
	}

	if (mFlags & kVFVPrepare_Serialize)
		mbSerialized = true;

	if (mFlags & kVFVPrepare_RunAsFiber)
		mbScheduleAsFiber = true;
}

void VDVideoFilterInstance::Start() {
	mRequestedState = kStateRunning;
	if (mbScheduleAsFiber) {
		while(mState != kStateRunning) {
			Reschedule();
			mFiberSignal.wait();

			if (mState == kStateError)
				throw mError;
		}
	} else {
		mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Start, 0, 0, 0);
		mState = kStateRunning;
		Reschedule();
	}
}

void VDVideoFilterInstance::Stop() {
	mRequestedState = kStateIdle;
	if (mbScheduleAsFiber) {
		while(mState != kStateIdle) {
			Reschedule();
			mFiberSignal.wait();

			if (mState == kStateError) {
				VDASSERT(false);		// Stop() isn't supposed to throw!
				mState = kStateIdle;
				break;
			}
		}
	} else {
		mState = kStateIdle;
		mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Stop, 0, 0, 0);
	}
}

bool VDVideoFilterInstance::Service() {
	if (mRequestedState != mState) {
		try {
			switch(mRequestedState) {
			case kStateRunning:
				mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Start, 0, 0, 0);
				break;
			case kStateIdle:
				mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Stop, 0, 0, 0);
				break;
			case kStatePrepared:
				mFlags = mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Prepare, 0, 0, 0);
				break;
			}
			mState = mRequestedState;
		} catch(const MyError& e) {
			mError.assign(e);
			mState = kStateError;
		}

		mFiberSignal.signal();

		return false;
	}

	if (mState != kStateRunning)
		return false;

	vdsynchronized(mLock) {
		VDASSERT(mActiveRequests.empty());

		if (mReadyRequests.empty())
			return false;

		mActiveRequests.splice(mActiveRequests.begin(), mReadyRequests, mReadyRequests.begin());
	}


	bool bMoreRequestsReady;
	VDVideoFrameRequest& req = mActiveRequests.front();

	// lock request so it can't retrigger if prefetches occur from run()
	req.Lock();

	// prepare context and source frames
	mContext.mpDstFrame				= 0;
	mContext.mpOutput->mFrameNum	= req.GetFrameNumber();

	mSourceFramePtrs.resize(req.GetSourceFrameCount());
	req.GetSourceFrames(&mSourceFramePtrs[0]);
	mContext.mpSrcFrames			= &mSourceFramePtrs[0];
	mContext.mSrcFrameCount			= mSourceFramePtrs.size();

	// launch filter
	sint32 result = mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Run, 0, 0, 0);

	if (result == kVFVRun_NeedMoreFrames) {
		VDDEBUG("Video filter[%-16ls]: recycling frame %ld for additional source frames\n", mpInfo->mpName, (long)req.GetFrameNumber());
		vdsynchronized(mLock) {
			VDASSERT(!mActiveRequests.empty());
			mPendingRequests.splice(mPendingRequests.begin(), mActiveRequests, mActiveRequests.begin());
			bMoreRequestsReady = !mReadyRequests.empty();
		}

		req.Unlock();
	} else {
		VDDEBUG("Video filter[%-16ls]: frame %ld complete\n", mpInfo->mpName, (long)req.GetFrameNumber());

		req.SetFrame(mContext.mpDstFrame);
		req.MarkDone();

		mCache.AddFrame((VDVideoFilterFrameRef *)((char *)req.GetFrame() - offsetof(VDVideoFilterFrameRef, mFrame)));

		vdsynchronized(mLock) {
			VDASSERT(!mActiveRequests.empty());

			// If the request has been abandoned, move it directly to Free.
			//
			// Note that the request's own reference count is not under lock.  However,
			// our Idle handler only checks mCompleteRequests and is also critsec'ed, so
			// this is safe.

			if (req.GetRefCount() == 1) {
				req.Reset();
				mFreeRequests.splice(mFreeRequests.begin(), mActiveRequests, mActiveRequests.begin());
			} else {
				mCompleteRequests.splice(mCompleteRequests.begin(), mActiveRequests, mActiveRequests.begin());
			}

			bMoreRequestsReady = !mReadyRequests.empty();
		}
	}

	return bMoreRequestsReady;
}

IVDVideoFrameRequest *VDVideoFilterInstance::RequestFrame(sint64 frame, IVDVideoFrameRequestCompleteCallback *pCallback, void *pToken) {
	VDVideoFrameRequest *pReq = NULL;
	bool bNewRequest = false;

	std::list<VDVideoFrameRequest> treq;

	vdsynchronized(mLock) {
		tRequests::iterator it, itEnd;

		it = mPendingRequests.begin();
		itEnd = mPendingRequests.end();
		for(; it != itEnd; ++it) {
			VDVideoFrameRequest& req = *it;

			if (req.GetFrameNumber() == frame) {
				pReq = &req;
				goto found;
			}
		}

		it = mReadyRequests.begin();
		itEnd = mReadyRequests.end();
		for(; it != itEnd; ++it) {
			VDVideoFrameRequest& req = *it;

			if (req.GetFrameNumber() == frame) {
				pReq = &req;
				goto found;
			}
		}

		it = mActiveRequests.begin();
		itEnd = mActiveRequests.end();
		for(; it != itEnd; ++it) {
			VDVideoFrameRequest& req = *it;

			if (req.GetFrameNumber() == frame) {
				pReq = &req;
				goto found;
			}
		}

		it = mCompleteRequests.begin();
		itEnd = mCompleteRequests.end();
		for(; it != itEnd; ++it) {
			VDVideoFrameRequest& req = *it;

			if (req.GetFrameNumber() == frame) {
				pReq = &req;
				goto found;
			}
		}

		if (mFreeRequests.empty())
			mFreeRequests.push_back(VDVideoFrameRequest(this));

		treq.splice(treq.begin(), mFreeRequests, mFreeRequests.begin());

		pReq = &treq.front();
		bNewRequest = true;

found:
		pReq->AddRef();		// must be done under lock!
	}

	if (bNewRequest) {
		pReq->Reset();
		pReq->SetFrameNumber(frame);

		VDVideoFilterFrameRef *pFrame = mCache.LookupFrame(frame);

		if (pFrame) {
			pReq->SetFrame(&pFrame->mFrame);
			pReq->MarkDone();

			if (pCallback)
				pCallback->NotifyVideoFrameRequestComplete(pReq, pReq->IsSuccessful(), pToken);

			vdsynchronized(mLock) {
				mCompleteRequests.splice(mCompleteRequests.begin(), treq, treq.begin());
			}

			if (pCallback) {
				pReq->Release();
				pReq = NULL;
			}
		} else {
			if (pCallback)
				pReq->AddNotifyTarget(pCallback, pToken);

			vdsynchronized(mLock) {
				mpPrefetchRequest = pReq;
				mContext.mpDefinition->mpMain(&mContext, kVFVCmd_Prefetch, frame, 0, 0);

				if (!pReq->IsWaiting()) {
					if (mReadyRequests.empty())
						Reschedule();
					mReadyRequests.splice(mReadyRequests.begin(), treq, treq.begin());
				} else
					mPendingRequests.splice(mPendingRequests.begin(), treq, treq.begin());
			}
		}
	} else {
		if (pCallback && pReq->IsReady()) {
			pCallback->NotifyVideoFrameRequestComplete(pReq, pReq->IsSuccessful(), pToken);
			pReq->Release();
			pReq = NULL;
		}
	}

	return pReq;
}

void VDVideoFilterInstance::NotifyFrameRequestReady(VDVideoFrameRequest *pRequest, bool bSuccessful) {
	VDDEBUG("Video filter[%-16ls]: frame %ld ready -- scheduling for processing.\n", mpInfo->mpName, (long)pRequest->GetFrameNumber());

	vdsynchronized(mLock) {
		tRequests::iterator it(mPendingRequests.begin()), itEnd(mPendingRequests.end());
		
		for(; it != itEnd; ++it) {
			if (&*it == pRequest) {
				if (mReadyRequests.empty())
					Reschedule();

				if (mbSerialized) {
					tRequests::iterator it2(it);

					do {
						++it2;
					} while(it2 != itEnd && (*it2).IsReady());

					mReadyRequests.splice(mReadyRequests.end(), mPendingRequests, it, it2);
				} else {
					mReadyRequests.splice(mReadyRequests.end(), mPendingRequests, it);
				}
				break;
			}

			if (mbSerialized)
				break;
		}
		VDASSERT(it != itEnd);
	}
}

void VDVideoFilterInstance::NotifyFrameRequestIdle(VDVideoFrameRequest *pRequest) {
	vdsynchronized(mLock) {
		if (pRequest->GetRefCount() == 1) {
			tRequests::iterator it(mCompleteRequests.begin()), itEnd(mCompleteRequests.end());

			for(; it!=itEnd; ++it) {
				VDVideoFrameRequest& req = *it;

				if (&req == static_cast<VDVideoFrameRequest *>(pRequest)) {
					req.Reset();
					mFreeRequests.splice(mFreeRequests.begin(), mCompleteRequests, it);
					break;
				}
			}
		}
	}
}

void VDVideoFilterInstance::ExtPrefetch(int pin, sint64 frameno, uint32 flags) {
	VDASSERT((unsigned)pin < mSources.size());

	VDVideoFrameRequest& req = *mpPrefetchRequest;
	VDSourceFrameRequest& srcreq = req.NewSourceRequest();
	VDVideoFilterInstance *pSrc = mSources[pin];
	VDVideoFilterPin& outpin = *pSrc->mContext.mpOutput;

	if (frameno >= outpin.mStart + outpin.mLength)
		frameno = outpin.mStart + outpin.mLength - 1;

	if (frameno < outpin.mStart)
		frameno = outpin.mStart;

	srcreq.mpFrame = NULL;
	srcreq.mpRequest = pSrc->RequestFrame(frameno, &req, &srcreq);
}

void VDVideoFilterInstance::ExtAllocFrame() {
	if (mContext.mpDstFrame)
		mContext.mpDstFrame->Release();

	const VDPixmap& dstformat = *mContext.mpOutput->mpFormat;

	vdrefptr<VDVideoFilterFrameRef> frameref(mCache.AllocFrame());

	if (!frameref->mpPixmap) {
		frameref->mpPixmap = new VDVideoFilterPixmap;
		frameref->mpPixmap->mBuffer.init(dstformat.w, dstformat.h, dstformat.format);
	}

	frameref->mFrame.mFrameNum = mContext.mpOutput->mFrameNum;
	frameref->mFrame.mpPixmap = &frameref->mpPixmap->mBuffer;

	mContext.mpDstFrame = &frameref.release()->mFrame;
}

///////////////////////////////////////////////////////////////////////////

class VDVideoFilterSystem : public IVDVideoFilterSystem {
public:
	VDVideoFilterSystem();
	~VDVideoFilterSystem();

	void SetScheduler(VDScheduler *pScheduler) { mpScheduler = pScheduler; }

	void Clear();
	IVDVideoFilterInstance *CreateFilter(const VDPluginInfo *pDef, void *pCreationData);
	void Connect(IVDVideoFilterInstance *src, IVDVideoFilterInstance *dst, int dstpin);
	void Prepare();
	void Start();
	void Stop();

protected:
	VDScheduler *mpScheduler;

	typedef std::list<VDVideoFilterInstance *> tFilters;
	tFilters	mFilters;
	tFilters	mStartedFilters;

	VDVideoFilterFiberScheduler		mFiberScheduler;
};

IVDVideoFilterSystem *VDCreateVideoFilterSystem() {
	return new VDVideoFilterSystem;
}

VDVideoFilterSystem::VDVideoFilterSystem() {
}

VDVideoFilterSystem::~VDVideoFilterSystem() {
	Clear();
}

void VDVideoFilterSystem::Clear() {
	Stop();

	while(!mFilters.empty()) {
		VDVideoFilterInstance *pInst = mFilters.back();

		delete pInst;

		mFilters.pop_back();
	}
}

IVDVideoFilterInstance *VDVideoFilterSystem::CreateFilter(const VDPluginInfo *pDef, void *pCreationData) {
	mFilters.push_back(new VDVideoFilterInstance(pDef, pCreationData));

	return mFilters.back();
}

void VDVideoFilterSystem::Connect(IVDVideoFilterInstance *src, IVDVideoFilterInstance *dst, int dstpin) {
	static_cast<VDVideoFilterInstance *>(dst)->Connect(dstpin, static_cast<VDVideoFilterInstance *>(src));
}

void VDVideoFilterSystem::Prepare() {
	typedef std::list<std::pair<VDVideoFilterInstance *, int> > tSortedFilters;
	tSortedFilters filters;

	for(tFilters::iterator it(mFilters.begin()), itEnd(mFilters.end()); it != itEnd; ++it) {
		VDVideoFilterInstance *pInst = *it;

		int nSources = pInst->GetSourceCount();

		if (nSources)
			filters.push_back(std::make_pair(pInst, nSources));
		else
			filters.push_front(std::make_pair(pInst, nSources));
	}

	while(!filters.empty()) {
		VDASSERT(!filters.front().second);
		VDVideoFilterInstance *pInst = filters.front().first;

		if (pInst->IsFiberDependent()) {
			mFiberScheduler.Start();
			mFiberScheduler.Scheduler().Add(pInst);
			pInst->Prepare();
			mFiberScheduler.Scheduler().Remove(pInst);
		} else {
			pInst->Prepare();
		}

		filters.pop_front();

		tSortedFilters::iterator it(filters.begin()), itEnd(filters.end());

		while(it != itEnd) {
			VDVideoFilterInstance *pInst2 = (*it).first;
			int srcs = pInst2->GetSourceCount();

			for(int i=0; i<srcs; ++i) {
				if (pInst2->GetSource(i) == pInst)
					--(*it).second;
			}

			tSortedFilters::iterator itNext(it);
			++itNext;

			if (!(*it).second)
				filters.splice(filters.begin(), filters, it);

			it = itNext;
		}
	}
}

void VDVideoFilterSystem::Start() {
	for(tFilters::iterator it(mFilters.begin()), itEnd(mFilters.end()); it != itEnd; ++it) {
		VDVideoFilterInstance *pInst = *it;

		if (pInst->IsFiberDependent()) {
			mFiberScheduler.Start();
			mFiberScheduler.Scheduler().Add(pInst);
		} else {
			mpScheduler->Add(pInst);
		}

		pInst->Start();
		mStartedFilters.push_back(pInst);
	}
}

void VDVideoFilterSystem::Stop() {
	while(!mStartedFilters.empty()) {
		VDVideoFilterInstance *pInst = mStartedFilters.back();

		pInst->Stop();
		if (pInst->IsFiberDependent()) {
			mFiberScheduler.Scheduler().Remove(pInst);
		} else {
			mpScheduler->Remove(pInst);
		}

		mStartedFilters.pop_back();
	}

	mFiberScheduler.Stop();
}


#endif
