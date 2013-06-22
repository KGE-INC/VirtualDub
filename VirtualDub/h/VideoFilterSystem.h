#ifndef f_VIDEOFILTERSYSTEM_H
#define f_VIDEOFILTERSYSTEM_H

#include <vd2/Kasumi/pixmap.h>

class IVDVideoFrameRequest : public IVDRefCount {
public:
	virtual bool IsReady() = 0;
	virtual VDVideoFilterFrame *GetFrame() = 0;
};

class IVDVideoFrameRequestCompleteCallback {
public:
	virtual void NotifyVideoFrameRequestComplete(IVDVideoFrameRequest *pRequest, bool bSuccessful, void *pToken) = 0;
};

class IVDVideoFilterInstance {
public:
	virtual IVDVideoFrameRequest *RequestFrame(sint64 frame, IVDVideoFrameRequestCompleteCallback *pCallback, void *pToken) = 0;
	virtual const VDPixmap& GetFormat() = 0;
	virtual bool Config(VDGUIHandle h) = 0;
};

class IVDVideoFilterSystem {
public:
	virtual void SetScheduler(VDScheduler *pScheduler) = 0;

	virtual void Clear() = 0;
	virtual IVDVideoFilterInstance *CreateFilter(const VDPluginInfo *pDef, void *pCreationData = 0) = 0;
	virtual void Connect(IVDVideoFilterInstance *src, IVDVideoFilterInstance *dst, int dstpin) = 0;
	virtual void Prepare() = 0;
	virtual void Start() = 0;
	virtual void Stop() = 0;
};

IVDVideoFilterSystem *VDCreateVideoFilterSystem();

#endif
