#ifndef f_VD2_PLUGIN_VDVIDEOFILT_H
#define f_VD2_PLUGIN_VDVIDEOFILT_H

#include <vd2/Kasumi/pixmap.h>

struct VDVideoFilterDefinition;
struct VDVideoFilterContext;
struct VDVideoFilterFrame;

enum VDVideoFilterFieldMode {
	kVFV_Unknown				= 0,
	kVFV_Progressive			= 1,
	kVFV_InterlacedEvenFirst	= 2,
	kVFV_InterlacedOddFirst		= 3
};

struct VDVideoFilterPin {
	VDPixmap	*mpFormat;
	sint64		mFrameNum;
	sint64		mStart;
	sint64		mLength;
	sint32		mFrameRateHi;
	sint32		mFrameRateLo;
	sint32		mFieldMode;		// one of VDVideoFilterFieldMode
};

struct VDVideoFilterCallbacks {
	void (VDAPIENTRY *mpPrefetchProc)(const VDVideoFilterContext *pContext, int pin, sint64 frameno, uint32 flags);
	void (VDAPIENTRY *mpAllocFrameProc)(const VDVideoFilterContext *pContext);
	void (VDAPIENTRY *mpCopyFrameProc)(const VDVideoFilterContext *pContext, VDVideoFilterFrame *pFrame);
};

struct VDVideoFilterFrameVtbl {
	int (VDAPIENTRY *mpAddRef)(VDVideoFilterFrame *);
	int (VDAPIENTRY *mpRelease)(VDVideoFilterFrame *);
	VDVideoFilterFrame *(VDAPIENTRY *mpWriteCopy)(VDVideoFilterFrame *);
	VDVideoFilterFrame *(VDAPIENTRY *mpWriteCopyRef)(VDVideoFilterFrame *);
};

struct VDVideoFilterFrame {
	const VDVideoFilterFrameVtbl	*mpVtbl;
	VDPixmap	*mpPixmap;
	sint64		mFrameNum;

	inline int AddRef() { return mpVtbl->mpAddRef(this); }
	inline int Release() { return mpVtbl->mpRelease(this); }
	inline VDVideoFilterFrame *WriteCopy() { return mpVtbl->mpWriteCopy(this); }
	inline VDVideoFilterFrame *WriteCopyRef() { return mpVtbl->mpWriteCopyRef(this); }
};

struct VDVideoFilterContext {
	void *mpFilterData;
	VDVideoFilterPin	**mpInputs;
	VDVideoFilterPin	*mpOutput;
	VDVideoFilterFrame	**mpSrcFrames;
	VDVideoFilterFrame	*mpDstFrame;
	IVDPluginCallbacks *mpServices;
	const VDVideoFilterCallbacks *mpVideoCallbacks;
	const VDVideoFilterDefinition *mpDefinition;
	uint32	mAPIVersion;
	int		mSrcFrameCount;


	inline void Prefetch(int pin, sint64 frameno, uint32 flags) const {
		mpVideoCallbacks->mpPrefetchProc(this, pin, frameno, flags);
	}

	inline void AllocFrame() const {
		mpVideoCallbacks->mpAllocFrameProc(this);
	}

	inline void CopyFrame(VDVideoFilterFrame *pFrame) const {
		mpVideoCallbacks->mpCopyFrameProc(this, pFrame);
	}
};

enum {
	kVFVRun_OK				= 0,
	kVFVRun_NeedMoreFrames	= 1,
	kVFVPrepare_Serialize	= 1,
};

typedef void *		(VDAPIENTRY *VDVideoFilterCreateProc		)(const VDVideoFilterContext *pContext);
typedef sint32		(VDAPIENTRY *VDVideoFilterMainProc			)(const VDVideoFilterContext *pContext, sint32 cmd, sint64 n, void *p, sint32 size);

struct VFVCmdInfo_GetDirectRange {
	sint64	pos;
	sint32	len;
};

enum {									//	n			p					Action
	kVFVCmd_Null			= 0,		//									Do nothing.
	kVFVCmd_Destroy			= 1,		//									Commit suicide (delete this).
	kVFVCmd_Ext				= 2,		//				interface name		Return command base for extension or return 0 if not supported.
	kVFVCmd_Run				= 3,		//									Process a frame.
	kVFVCmd_Prefetch		= 4,		//	frameno							Prefetch frames required to compute desired output frame.
	kVFVCmd_Prepare			= 5,		//									Validate parameters and compute valid output frame for input frame types.
	kVFVCmd_Start			= 6,		//									Jump to running state.
	kVFVCmd_Stop			= 7,		//									Revert to idle state.
	kVFVCmd_Suspend			= 8,		//				buffer				Freeze filter state and serialize to memory block.  Return byte size used, or bytes necessary if buffer is too small.
	kVFVCmd_Resume			= 9,		//				buffer				Deserialize filter state from memory block.
	kVFVCmd_GetParam		= 10,		//	index		buffer				Retrieve a parameter.  Return bytes used or bytes required.
	kVFVCmd_SetParam		= 11,		//	index		buffer				Set a parameter.
	kVFVCmd_Config			= 12,		//				&hwnd				Do modal GUI configuration using given window handle as parent.
	kVFVCmd_Blurb			= 13,		//				buffer (wchar_t*)	Create short text description for current filter configuration. Return byte size used, or bytes necessary if buffer is too small.
	kVFVCmd_PrefetchSrcPrv	= 14,		//	frameno							Prefetch a frame for source preview.
	kVFVCmd_GetDirectRange	= 15,		//				&pos,len			Compute valid range for direct stream copy. Returns input ID or zero if not possible.
	kVFVCmd_Count
};

enum VDVideoFilterFlags {
	kVFVDef_RunAsFiber	= 1,			///< Video filter must be run within the fiber scheduler.
};

struct VDVideoFilterDefinition {
	uint32							mSize;				// size of this structure in bytes
	uint32							mFlags;

	uint32							mInputPins;
	uint32							mOutputPins;

	const VDPluginConfigEntry		*mpConfigInfo;
	void							*mpDefaultCreationData;

	VDVideoFilterCreateProc			mpCreate;
	VDVideoFilterMainProc			mpMain;
};

enum { kVDPlugin_VideoAPIVersion = 1 };

#endif
