#ifndef f_VIDEOFILTERFRAME_H
#define f_VIDEOFILTERFRAME_H

#include <vd2/system/atomic.h>
#include <vd2/system/cache.h>
#include <vd2/system/thread.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>

#include "VideoFilterSystem.h"
#include "backface.h"

class IVDVideoFrameRequest;
class VDVideoFrameRequest;
class VDVideoFilterFrameCache;

///////////////////////////////////////////////////////////////////////////
/// \class VDVideoFilterPixmap
/// \brief Holder for pixmaps visible to video filters.
///
struct VDVideoFilterPixmap : public VDPooledObject, public VDBackfaceObject<VDVideoFilterPixmap> {
	VDPixmapBuffer				mBuffer;			///< Actual pixmap buffer for frame data.

	VDVideoFilterPixmap() {}
	VDVideoFilterPixmap(const VDPixmap& src) : mBuffer(src){}

#if VD_BACKFACE_ENABLED
	static const char *BackfaceGetShortName();
	static const char *BackfaceGetLongName();
	void BackfaceDumpObject(IVDBackfaceOutput& out);
	void BackfaceDumpBlurb(IVDBackfaceOutput& out);

	uint32	mLastAllocTime;
	uint32	mAllocFrame;
#endif

};

class IVDVideoFrameRequestScheduler {
public:
	virtual void NotifyFrameRequestReady(VDVideoFrameRequest *pRequest, bool bSuccessful) = 0;
};

///////////////////////////////////////////////////////////////////////////
/// \class VDVideoFilterFrameRef
/// \brief Reference object for frames stored in a video filter frame
///		   cache.
///
/// A VDVideoFilterFrameRef is used to store frames, valid or not, in
/// the output cache of a video filter instance. This adds a layer of
/// indirection so that frames may be aliased, which can happen in a
/// filter that sometimes or always passes frames through.
///
class VDVideoFilterFrameRef : public vdlist<VDVideoFilterFrameRef>::node, public VDBackfaceObject<VDVideoFilterFrameRef> {
	VDVideoFilterFrameRef& operator=(const VDVideoFilterFrameRef&);
public:
	vdrefptr<VDVideoFilterPixmap>	mpPixmapRef;	///< Pointer to filter pixmap backing store.
	VDPixmap						mPixmap;		///< Format used by this reference. This may be different from the backing store format when aliasing occurs.
	VDVideoFilterFrame				mFrame;			///< Frame data referenced by plugins.
	VDVideoFilterFrameCache			*mpCache;		///< Pointer to parent cache managing this frame reference.
	VDAtomicInt						mRefCount;		///< Reference count.
	bool							mbValid;		///< True if this frame reference represents a valid frame; false if it is simply holding a pixmap buffer.

	VDVideoFilterFrameRef(VDVideoFilterFrameCache *pCache);
	VDVideoFilterFrameRef(const VDVideoFilterFrameRef&);
	~VDVideoFilterFrameRef();

	int AddRef() {
		return mRefCount.inc();
	}

	int Release();

	int GetRefCount() const {
		return mRefCount;
	}

#if VD_BACKFACE_ENABLED
	static const char *BackfaceGetShortName();
	static const char *BackfaceGetLongName();
	void BackfaceDumpObject(IVDBackfaceOutput& out);
	void BackfaceDumpBlurb(IVDBackfaceOutput& out);

	uint32	mLastAllocTime;
#endif

protected:
	VDVideoFilterFrame *WriteCopy();
	VDVideoFilterFrame *WriteCopyRef();

	static inline VDVideoFilterFrameRef *AsThis(VDVideoFilterFrame *pFrame) {
		return (VDVideoFilterFrameRef *)((char *)pFrame - offsetof(VDVideoFilterFrameRef, mFrame));
	}

	static int ExtAddRef(VDVideoFilterFrame *pFrame) { return AsThis(pFrame)->AddRef(); }
	static int ExtRelease(VDVideoFilterFrame *pFrame) { return AsThis(pFrame)->Release(); }
	static VDVideoFilterFrame *ExtCopyWrite(VDVideoFilterFrame *pFrame) { return AsThis(pFrame)->WriteCopy(); }
	static VDVideoFilterFrame *ExtCopyWriteRef(VDVideoFilterFrame *pFrame) { return AsThis(pFrame)->WriteCopyRef(); }

	static const VDVideoFilterFrameVtbl sVtbl;
};

///////////////////////////////////////////////////////////////////////////
/// \class VDVideoFilterFrameCache
/// \brief Output frame cache for a video filter instance.
///
/// Stores video frames that have been output by a video filter instance.
/// References are used (VDVideoFilterFrameRef) so that frames may be
/// aliased and stored in more than one cache.
///
class VDVideoFilterFrameCache : public VDBackfaceObject<VDVideoFilterFrameCache> {
public:
	VDVideoFilterFrameCache();
	~VDVideoFilterFrameCache();

	void SetLimit(int limit);
	VDVideoFilterFrameRef *AllocFrame();
	VDVideoFilterFrameRef *AddFrame(VDVideoFilterFrameRef *, sint64 frameno);
	VDVideoFilterFrameRef *LookupFrame(sint64 frame);

	void NotifyVideoFrameIdle(VDVideoFilterFrameRef *);

#if VD_BACKFACE_ENABLED
	static const char *BackfaceGetShortName();
	static const char *BackfaceGetLongName();
	void BackfaceDumpObject(IVDBackfaceOutput& out);
	void BackfaceDumpBlurb(IVDBackfaceOutput& out);
#endif

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

struct VDSourceFrameRequest {
	VDVideoFilterFrame		*mpFrame;			// manually reference counted
	IVDVideoFrameRequest	*mpRequest;
};

///////////////////////////////////////////////////////////////////////////
///	\class VDVideoFrameClientRequest
///
///	Wraps a client request for a video frame. We have to track these
/// individually because each reference has a unique callback that must be
/// removed if the reference is dropped. Otherwise, it simply forwards to
/// the main request.
///
class VDVideoFrameClientRequest : public IVDVideoFrameRequest, public vdlist<VDVideoFrameClientRequest>::node {
public:
	VDVideoFrameClientRequest() : mRefCount(0), mbActive(true) {}
	VDVideoFrameClientRequest(VDVideoFrameRequest *pParent, IVDVideoFrameRequestCompleteCallback *pCB, void *pCBData) : mpParent(pParent), mpCB(pCB), mpCBData(pCBData), mRefCount(0), mbActive(true) {}

	int AddRef();
	int Release();

	int InternalAddRef();
	int InternalRelease();

	bool IsReady();
	VDVideoFilterFrame *GetFrame();
	void Cancel();

public:
	VDVideoFrameRequest *mpParent;
	IVDVideoFrameRequestCompleteCallback *mpCB;
	void			*mpCBData;
	bool			mbActive;
	VDAtomicInt		mRefCount;
};

//
//	VDVideoFrameRequest
//
//	Wraps a request for an output frame from a video filter. It collects
//	frames from the upstream filters and caches the output frame.
//
class VDVideoFrameRequest : public IVDVideoFrameRequestCompleteCallback, public IVDVideoFrameRequest, public VDCachedObject {
	friend VDVideoFrameClientRequest;
public:
	VDVideoFrameRequest(IVDVideoFrameRequestScheduler *pScheduler);
	~VDVideoFrameRequest();

	int AddRef() { return VDCachedObject::AddRef(); }
	int Release() {  return VDCachedObject::Release(); }

	bool IsValid() const { return mbSuccessful; }
	bool IsReady();
	bool IsSuccessful() { return mbSuccessful; }
	bool IsWaiting() {
		return mOutstandingSourceFrames > 0;
	}

	void Cancel();

	void Lock() {
		++mOutstandingSourceFrames;
	}

	void Unlock();

	int GetSourceFrameCount() {
		return mSourceFrames.size();
	}

	void GetSourceFrames(VDVideoFilterFrame **pFrames);

	VDVideoFilterFrame *GetFrame() { return mpFrame; }
	void SetFrame(VDVideoFilterFrame *p) { mpFrame.set(p); }

	sint64	GetFrameNumber() const { return VDCachedObject::GetCacheKey(); }

	VDSourceFrameRequest& NewSourceRequest();
	IVDVideoFrameRequest *AddNotifyTarget(IVDVideoFrameRequestCompleteCallback *pTarget, void *pContext);

	void MarkDone();
	void FlushSourceFrames();
	void Reset();

protected:
	void OnCacheEvict();
	void OnCacheAbortPending();
	void DumpStatus();

	void NotifyClientRequestFree(VDVideoFrameClientRequest *pRequest);
	void CancelClientRequest(VDVideoFrameClientRequest *pRequest);

	void NotifyVideoFrameRequestComplete(IVDVideoFrameRequest *pRequest, bool bSuccessful, bool bAborted, void *pToken);

	vdrefptr<VDVideoFilterFrame>	mpFrame;
	VDAtomicInt						mOutstandingSourceFrames;
	bool							mbReady;
	bool							mbSuccessful;
	bool							mbNotifyComplete;

	IVDVideoFrameRequestScheduler	*mpScheduler;

	VDCriticalSection				mLock;

	std::list<VDSourceFrameRequest>	mSourceFrames;

	/// List of outstanding 
	typedef vdlist<VDVideoFrameClientRequest> tNotifyTargets;
	tNotifyTargets mNotifyTargets;
};

#endif
