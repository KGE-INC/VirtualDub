#ifndef f_VIDEOFILTERSYSTEM_H
#define f_VIDEOFILTERSYSTEM_H

#include <vd2/Kasumi/pixmap.h>

#include <vd2/system/refcount.h>
#include <vd2/system/VDString.h>
#include "plugins.h"

class VDScheduler;
struct VDPluginInfo;
struct VDVideoFilterFrame;
class IVDAsyncErrorCallback;

struct VDVideoFilterOutputInfo {
	VDPixmap	*mpFormat;
	sint64		mStart;
	sint64		mLength;
	sint32		mFrameRateHi;
	sint32		mFrameRateLo;
};

class IVDVideoFrameRequest : public IVDRefCount {
public:
	virtual bool IsReady() = 0;
	virtual VDVideoFilterFrame *GetFrame() = 0;
	virtual void Cancel() = 0;
};

class IVDVideoFrameRequestCompleteCallback {
public:
	virtual void NotifyVideoFrameRequestComplete(IVDVideoFrameRequest *pRequest, bool bSuccessful, bool bAborted, void *pToken) = 0;
};

class IVDVideoFilterInstance : public IVDRefCount {
public:
	virtual bool IsPrepared() = 0;

	virtual IVDVideoFrameRequest *RequestPreviewSourceFrame(sint64 frame, IVDVideoFrameRequestCompleteCallback *pCallback, void *pToken) = 0;
	virtual IVDVideoFrameRequest *RequestFrame(sint64 frame, IVDVideoFrameRequestCompleteCallback *pCallback, void *pToken) = 0;
	virtual const VDPixmap& GetFormat() = 0;
	virtual VDVideoFilterOutputInfo GetOutputInfo() = 0;
	virtual bool Config(VDGUIHandle h) = 0;
	virtual VDStringW GetBlurb() = 0;
	virtual void SerializeConfig(VDPluginConfig& config) = 0;
	virtual void DeserializeConfig(const VDPluginConfig& config) = 0;
	virtual void SetConfigVal(unsigned idx, const VDPluginConfigVariant& var) = 0;

	virtual int GetDirectRange(sint64& pos, sint32& len) = 0;
		///< Query if a particular range of frames is direct-possible. If so, a non-zero source ID is returned,
		///< and the position and length are adjusted. The returned length may be shorter than the requested
		///< length, possibly because only the beginning of the range is direct-capable. A return of zero
		///< means that the range must be processed.
};

IVDVideoFilterInstance *VDCreateVideoFilterInstance(const VDPluginInfo *pDef, void *pCreationData = 0);

class IVDVideoFilterSystem : public IVDRefCount {
protected:
	inline ~IVDVideoFilterSystem() {}
public:
	virtual bool IsRunning() = 0;

	virtual void SetScheduler(VDScheduler *pScheduler) = 0;
	virtual void SetErrorCallback(IVDAsyncErrorCallback *pCB) = 0;

	virtual void Clear() = 0;
	virtual IVDVideoFilterInstance *CreateFilter(const VDPluginInfo *pDef, void *pCreationData = 0) = 0;
	virtual void Connect(IVDVideoFilterInstance *src, IVDVideoFilterInstance *dst, int dstpin) = 0;
	virtual void Prepare(bool allowPartial) = 0;
	virtual void Start() = 0;
	virtual void Stop() = 0;
};

IVDVideoFilterSystem *VDCreateVideoFilterSystem();

#endif
