#include "stdafx.h"

#include <list>
#include <vector>
#include <utility>

#include <vd2/system/time.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/refcount.h>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofilt.h>

#include "VideoFilterFrame.h"
#include "VideoFilterSystem.h"

#define VDDEBUG_CACHE	(void)sizeof printf
//#define VDDEBUG_CACHE	VDDEBUG
#define VDDEBUG_FRAME	(void)sizeof printf
//#define VDDEBUG_REQUEST	VDDEBUG
#define VDDEBUG_REQUEST	(void)sizeof printf

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

#if VD_BACKFACE_ENABLED

const char *VDVideoFilterPixmap::BackfaceGetShortName() {
	return "vfpix";
}

const char *VDVideoFilterPixmap::BackfaceGetLongName() {
	return "VDVideoFilterPixmap";
}

void VDVideoFilterPixmap::BackfaceDumpObject(IVDBackfaceOutput& out) {
}

void VDVideoFilterPixmap::BackfaceDumpBlurb(IVDBackfaceOutput& out) {
	out("%ux%u [%d]; lastAlloc=%08x, refcount=%d, allocFrame=%u", mBuffer.w, mBuffer.h, (int)mBuffer.pitch, mLastAllocTime, mRefCount, mAllocFrame);
}

#endif

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

VDVideoFilterFrameRef::VDVideoFilterFrameRef(VDVideoFilterFrameCache *pCache)
	: mpCache(pCache)
	, mRefCount(0)
	, mbValid(false)
{
	mFrame.mpVtbl = &sVtbl;
	mFrame.mpPixmap = &mPixmap;
}

VDVideoFilterFrameRef::VDVideoFilterFrameRef(const VDVideoFilterFrameRef& src)
	: mpPixmapRef(src.mpPixmapRef)
	, mPixmap(src.mPixmap)
	, mFrame(src.mFrame)
	, mpCache(NULL)
	, mRefCount(0)
	, mbValid(false)
{
	mFrame.mpPixmap = &mPixmap;
}

VDVideoFilterFrameRef::~VDVideoFilterFrameRef() {
	VDASSERT(mRefCount == (mpCache ? 1 : 0));
}

int VDVideoFilterFrameRef::Release() {
	int rv = mRefCount.dec();

	if (rv == 1) {
		VDASSERT(&this->mPixmap == this->mFrame.mpPixmap);
		if (mpCache)
			mpCache->NotifyVideoFrameIdle(this);
	}

	if (!rv) {
		VDASSERT(!mpCache);
		delete this;
	}

	return rv;
}

VDVideoFilterFrame *VDVideoFilterFrameRef::WriteCopy() {
	VDASSERT(&this->mPixmap == this->mFrame.mpPixmap);
//	if (mRefCount == 1 && mpPixmap->mRefCount == 1)
//		return &mFrame;

	vdrefptr<VDVideoFilterFrameRef> pNewFrame(new VDVideoFilterFrameRef(*this));
	pNewFrame->mpPixmapRef = new VDVideoFilterPixmap(pNewFrame->mpPixmapRef->mBuffer);
	pNewFrame->mPixmap = pNewFrame->mpPixmapRef->mBuffer;

	VDDEBUG_FRAME("WriteCopy: Cloning frameref %p + pixmap %p to frameref %p + pixmap %p\n", this, &*this->mpPixmapRef, &*pNewFrame, &*pNewFrame->mpPixmapRef);

	Release();
	return &pNewFrame.release()->mFrame;
}

VDVideoFilterFrame *VDVideoFilterFrameRef::WriteCopyRef() {
	VDASSERT(&this->mPixmap == this->mFrame.mpPixmap);
//	if (mRefCount == 1 && mpPixmap->mRefCount == 1)
//		return &mFrame;

	vdrefptr<VDVideoFilterFrameRef> pNewFrame(new VDVideoFilterFrameRef(*this));

	VDDEBUG_FRAME("WriteCopyRef: Cloning frameref %p + pixmap %p to frameref %p + pixmap %p\n", this, &*this->mpPixmapRef, &*pNewFrame, &*pNewFrame->mpPixmapRef);

	Release();
	return &pNewFrame.release()->mFrame;
}

#if VD_BACKFACE_ENABLED
const char *VDVideoFilterFrameRef::BackfaceGetShortName() {
	return "vfref";
}

const char *VDVideoFilterFrameRef::BackfaceGetLongName() {
	return "VDVideoFilterFrameRef";
}

void VDVideoFilterFrameRef::BackfaceDumpObject(IVDBackfaceOutput& out) {
}

void VDVideoFilterFrameRef::BackfaceDumpBlurb(IVDBackfaceOutput& out) {
	out("Frame %4u, %ux%u [%d], lastAlloc=%08x, cache=%s, backingStore=%s [%s]", (unsigned)mFrame.mFrameNum, mPixmap.w, mPixmap.h, (int)mPixmap.pitch, mLastAllocTime, out.GetTag(mpCache).c_str(), out.GetTag(mpPixmapRef).c_str(), out.GetBlurb(mpPixmapRef).c_str());
}
#endif

const VDVideoFilterFrameVtbl VDVideoFilterFrameRef::sVtbl={
	ExtAddRef,
	ExtRelease,
	ExtCopyWrite,
	ExtCopyWriteRef
};

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

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
		mIdleFrames.pop_back();
		pFrame->mpCache = NULL;

		VDASSERT(pFrame->GetRefCount() == 1);
		pFrame->Release();
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
			pFrame->mpPixmapRef = NULL;			// MUST reallocate a new frame from cache in case this was shared
			pFrame->mFrame.mFrameNum = -1;
			pFrame->mbValid = false;
			mIdleFrames.pop_back();
			VDASSERT(mIdleFrames.end() == mIdleFrames.find(pFrame));
		}

		mPendingFrames.push_front(pFrame);
		return pFrame;
	}
}

VDVideoFilterFrameRef *VDVideoFilterFrameCache::AddFrame(VDVideoFilterFrameRef *pFrame, sint64 frameno) {
	// check if this reference is from the right cache -- if it isn't, we need to dupe
	// the reference; note that this doesn't dupe the framebuffer itself
	if (pFrame->mpCache != this) {
		VDDEBUG_CACHE("VideoFilterFrameCache[%p]: aliasing frame %ld (frame=%p, pixmap=%p)\n", this, (long)frameno, &pFrame->mFrame, &*pFrame->mFrame.mpPixmap);

		VDVideoFilterFrameRef *pNewFrame = new VDVideoFilterFrameRef(*pFrame);
		pNewFrame->mpCache = this;
		pFrame->Release();		// release client reference
		pFrame = pNewFrame;
		pFrame->AddRef();		// add mPendingFrames reference
		pFrame->AddRef();		// add client reference

#if VD_BACKFACE_ENABLED
		pFrame->mLastAllocTime = VDGetCurrentTick();
#endif

		mPendingFrames.push_back(pFrame);
		++mTotalFrames;
	}

	pFrame->mFrame.mFrameNum = frameno;
	pFrame->mbValid = true;

	VDDEBUG_CACHE("VideoFilterFrameCache[%p]: adding frame %ld (frame=%p, pixmap=%p) to cache\n", this, (long)frameno, &pFrame->mFrame, &*pFrame->mFrame.mpPixmap);

	vdsynchronized(mLock) {
		int rc = pFrame->GetRefCount();

		VDASSERT(mPendingFrames.end() != mPendingFrames.find(pFrame));

		mPendingFrames.erase(mPendingFrames.fast_find(pFrame));

		pFrame->mFrame.mFrameNum = frameno;

		if (rc > 1) {
			mActiveFrames.push_front(pFrame);
			VDASSERT(mIdleFrames.end() == mIdleFrames.find(pFrame));
		} else
			mIdleFrames.push_front(pFrame);

		if (mTotalFrames > mFrameLimit)
			EvictFrames();
	}

	return pFrame;
}

VDVideoFilterFrameRef *VDVideoFilterFrameCache::LookupFrame(sint64 frame) {
	VDVideoFilterFrameRef *pRef = NULL;

	vdsynchronized(mLock) {
		tFrames::iterator it(mIdleFrames.begin()), itEnd(mIdleFrames.end());

		for(; it!=itEnd; ++it) {
			VDVideoFilterFrameRef& ref = **it;

			if (ref.mFrame.mFrameNum == frame && ref.mbValid) {
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
					VDASSERT(ref.mbValid);
					pRef = &ref;
					break;
				}
			}
		}
	}

	if (pRef) {
		pRef->AddRef();
		VDDEBUG_CACHE("VideoFilterFrameCache[%p]: cache hit for frame %ld\n", this, (long)frame);
	} else {
		VDDEBUG_CACHE("VideoFilterFrameCache[%p]: cache miss for frame %ld\n", this, (long)frame);
	}

	return pRef;
}

void VDVideoFilterFrameCache::NotifyVideoFrameIdle(VDVideoFilterFrameRef *pFrame) {
	vdsynchronized(mLock) {
#pragma vdpragma_TODO("I think this check needs to be made, not just an assert, due to race")
		VDASSERT(pFrame->GetRefCount() == 1);

		if (pFrame->mbValid) {
			VDASSERT(mActiveFrames.end() != mActiveFrames.find(pFrame));
			tFrames::iterator it(mActiveFrames.fast_find(pFrame));

			mIdleFrames.splice(mIdleFrames.begin(), mActiveFrames, it);
		} else {
			VDASSERT(mPendingFrames.end() != mPendingFrames.find(pFrame));
			tFrames::iterator it(mPendingFrames.fast_find(pFrame));

			mIdleFrames.splice(mIdleFrames.end(), mPendingFrames, it);

			pFrame->mFrame.mFrameNum = -1;
		}

		VDDEBUG_CACHE("VideoFilterFrameCache[%p]: returning frame %ld to idle state (idle=%d, active=%d, total=%d)\n", this, (long)pFrame->mFrame.mFrameNum, mIdleFrames.size(), mActiveFrames.size(), mTotalFrames);

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

		VDDEBUG_CACHE("VideoFilterFrameCache[%p]: evicting frame %ld\n", this, (long)pFrame->mFrame.mFrameNum);

		delete pFrame;
		--mTotalFrames;
		++evicted;
	}

	if (evicted)
		VDDEBUG_CACHE("VideoFilterFrameCache[%p]: evicted %d frames\n", this, evicted);
}

#if VD_BACKFACE_ENABLED
const char *VDVideoFilterFrameCache::BackfaceGetShortName() {
	return "vfcache";
}

const char *VDVideoFilterFrameCache::BackfaceGetLongName() {
	return "VDVideoFilterFrameCache";
}

void VDVideoFilterFrameCache::BackfaceDumpObject(IVDBackfaceOutput& out) {
}

void VDVideoFilterFrameCache::BackfaceDumpBlurb(IVDBackfaceOutput& out) {
}
#endif

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

int VDVideoFrameClientRequest::AddRef() {
	return (mRefCount += 2) >> 1;
}

int VDVideoFrameClientRequest::Release() {
	int rv = (mRefCount -= 2);

	VDASSERT(rv >= 0);

	if (!rv)
		mpParent->NotifyClientRequestFree(this);

	return rv >> 1;
}

int VDVideoFrameClientRequest::InternalAddRef() {
	int rv = ++mRefCount;
	VDASSERT(rv & 1);

	return 1;
}

int VDVideoFrameClientRequest::InternalRelease() {
	int rv = --mRefCount;
	VDASSERT(!(rv & 1));

	if (!rv)
		mpParent->NotifyClientRequestFree(this);

	return 0;
}

bool VDVideoFrameClientRequest::IsReady() {
	return mpParent->IsReady();
}

VDVideoFilterFrame *VDVideoFrameClientRequest::GetFrame() {
	return mpParent->GetFrame();
}

void VDVideoFrameClientRequest::Cancel() {
	mpParent->CancelClientRequest(this);
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

VDVideoFrameRequest::VDVideoFrameRequest(IVDVideoFrameRequestScheduler *pScheduler)
	: mbReady(false)
	, mbSuccessful(false)
	, mbNotifyComplete(false)
	, mpScheduler(pScheduler)
	, mOutstandingSourceFrames(0)
{
}

VDVideoFrameRequest::~VDVideoFrameRequest() {
	VDASSERT(mNotifyTargets.empty());
	VDASSERT(mSourceFrames.empty());
}

bool VDVideoFrameRequest::IsReady() {
	return mbReady;
}

void VDVideoFrameRequest::Cancel() {
	VDASSERT(false);
}

void VDVideoFrameRequest::Unlock() {
	if (!--mOutstandingSourceFrames)
		mpScheduler->NotifyFrameRequestReady(this, mbSuccessful);
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

IVDVideoFrameRequest *VDVideoFrameRequest::AddNotifyTarget(IVDVideoFrameRequestCompleteCallback *pTarget, void *pContext) {
	VDVideoFrameClientRequest *p = new VDVideoFrameClientRequest(this, pTarget, pContext);
	p->AddRef();	// one for client

	bool notify = true;
	vdsynchronized(mLock) {
		if (!mbNotifyComplete) {
			p->InternalAddRef();	// one for mNotifyTargets

			if (mNotifyTargets.empty())
				AddRef();

			mNotifyTargets.push_back(p);
			notify = false;
		}

		if (notify)
			p->mbActive = false;
	}

	if (notify)
		pTarget->NotifyVideoFrameRequestComplete(p, mbSuccessful, false, pContext);

	return p;
}

void VDVideoFrameRequest::MarkDone() {
	FlushSourceFrames();

	bool isEmpty;
	
	vdsynchronized(mLock) {
		mbReady = true;
		mbSuccessful = true;
		mOutstandingSourceFrames = 0;		// to undo pending lock

		isEmpty = mNotifyTargets.empty();
		if (isEmpty)
			mbNotifyComplete = true;
	}

	if (!isEmpty) {
		bool weSawListGoEmpty = false;

		for(;;) {
			VDVideoFrameClientRequest *pTarget = NULL;
			bool is_active;
			
			vdsynchronized(mLock) {
				if (!mNotifyTargets.empty()) {
					pTarget = mNotifyTargets.back();
					mNotifyTargets.pop_back();

					if (mNotifyTargets.empty())
						weSawListGoEmpty = true;

					is_active = pTarget->mbActive;
					pTarget->mbActive = false;
				} else
					mbNotifyComplete = true;
			}

			if (!pTarget)
				break;

			// notify
			if (is_active)
				pTarget->mpCB->NotifyVideoFrameRequestComplete(pTarget, mbSuccessful, false, pTarget->mpCBData);

			pTarget->InternalRelease();
		}

		// release reference for non-zero mNotifyTargets
		if (weSawListGoEmpty)
			Release();
	}
}

void VDVideoFrameRequest::FlushSourceFrames() {
	vdsynchronized(mLock) {
		std::list<VDSourceFrameRequest>::iterator it(mSourceFrames.begin()), itEnd(mSourceFrames.end());
		for(; it != itEnd; ++it) {
			VDSourceFrameRequest& req = *it;

			if (req.mpRequest) {
				req.mpRequest->Cancel();

				if (req.mpRequest) {
					req.mpRequest->Release();
					req.mpRequest = NULL;
				}
			}

			if (req.mpFrame) {
				req.mpFrame->Release();
				req.mpFrame = NULL;
			}
		}
	}

	mSourceFrames.clear();
	mOutstandingSourceFrames = 0;
}

void VDVideoFrameRequest::Reset() {
	FlushSourceFrames();
	mpFrame = NULL;
	mbReady = false;
	mbSuccessful = false;
	mbNotifyComplete = false;
}

void VDVideoFrameRequest::OnCacheEvict() {
	Reset();
}

void VDVideoFrameRequest::OnCacheAbortPending() {
	Reset();
}

void VDVideoFrameRequest::DumpStatus() {
	vdsynchronized(mLock) {
		VDDEBUG("            Request %p: %d outstanding src frames, %d notify targets\n", this, mOutstandingSourceFrames, mNotifyTargets.size());

		std::list<VDSourceFrameRequest>::iterator it(mSourceFrames.begin()), itEnd(mSourceFrames.end());
		for(; it != itEnd; ++it) {
			VDSourceFrameRequest& req = *it;
			VDDEBUG("                Request[%p](%d) Frame[%p]\n", req.mpRequest, req.mpRequest && req.mpRequest->IsReady(), req.mpFrame);
		}
	}
}

void VDVideoFrameRequest::NotifyClientRequestFree(VDVideoFrameClientRequest *pRequest) {
	delete pRequest;
}

void VDVideoFrameRequest::CancelClientRequest(VDVideoFrameClientRequest *pRequest) {
	bool releaseMe = false;
	bool success = false;

	vdsynchronized(mLock) {
		if (pRequest->mbActive) {
			pRequest->mbActive = false;

			VDASSERT(mNotifyTargets.find(pRequest) != mNotifyTargets.end());
			mNotifyTargets.erase(pRequest);
			if (mNotifyTargets.empty())
				releaseMe = true;

			success = true;
		}
	}

	if (success) {
		pRequest->mpCB->NotifyVideoFrameRequestComplete(pRequest, false, true, pRequest->mpCBData);
		pRequest->InternalRelease();

		if (releaseMe)
			Release();
	}
}

void VDVideoFrameRequest::NotifyVideoFrameRequestComplete(IVDVideoFrameRequest *pRequest, bool bSuccessful, bool bAborted, void *pToken) {
	VDSourceFrameRequest *pReq = (VDSourceFrameRequest *)pToken;

	VDASSERT(pRequest == pReq->mpRequest || !pReq->mpRequest);

	VDDEBUG_REQUEST("Video filter request (frame=%ld): received source frame %ld (successful = %d)\n", (long)GetFrameNumber(), (long)pRequest->GetFrame()->mFrameNum, bSuccessful);

	if (!bSuccessful) {
		mbSuccessful = false;
	} else {
		VDASSERT(!pReq->mpFrame);
		pReq->mpFrame = pRequest->GetFrame();
		pReq->mpFrame->AddRef();
	}

	if (pReq->mpRequest) {
		pReq->mpRequest->Release();
		pReq->mpRequest = NULL;
	}

	if (!bAborted) {
		VDASSERT(mOutstandingSourceFrames > 0);
		if (!--mOutstandingSourceFrames) {
			vdsynchronized(mLock) {
				if (!mNotifyTargets.empty())
					mpScheduler->NotifyFrameRequestReady(this, mbSuccessful);
			}
		}
	}

	WeakRelease();
}
