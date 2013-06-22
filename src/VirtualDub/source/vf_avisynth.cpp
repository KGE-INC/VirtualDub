#include "stdafx.h"

#if 0		// Not yet used and references fibers

#include <list>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofiltold.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/memory.h>
#include <vd2/system/error.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/pixmapops.h>
#include "vf_base.h"
#include "VBitmap.h"

///////////////////////////////////////////////////////////////////////////
//
//	Avisynth definitions
//
///////////////////////////////////////////////////////////////////////////

class IScriptEnvironment;

namespace {
	enum {
		kAVSVersion = 1
	};

	enum {
		kAVSPixFormat_RGB888		= 0x13,
		kAVSPixFormat_XRGB8888		= 0x14,
		kAVSPixFormat_YUY2			= 0x22,
	};

	enum {
		kAVSImageType_BottomField	= 1,
		kAVSImageType_TopField		= 2,
		kAVSImageType_FieldBased	= 4
	};

	struct AVSVideoInfo {		// VideoInfo structure, Avisynth 2.5 (Avisynth 2.0 compatible)
		sint32	width, height;
		uint32	fps_numerator, fps_denominator;
		sint32	num_frames;

		sint32	pixel_type;
		sint32	audio_sampling_rate;
		sint32	audio_sample_type;
		sint64	audio_length;			// length in samples
		sint32	audio_channels;

		sint32	image_type;
	};

	struct AVSVideoFrameBuffer {
		uint8	*data;
		sint32	data_size;
		sint32	sequence_number;
		sint32	refcount;
	};

	struct AVSVideoFrame {
		sint32					refcount;
		AVSVideoFrameBuffer		*pfb;
		sint32					offset;
		sint32					pitch;
		sint32					row_size;
		sint32					height;
		sint32					offsetU;
		sint32					offsetV;
		sint32					pitchUV;
	};

	int ConvertAVSFormatToPixmapFormat(int pixel_type) {
		if (pixel_type == kAVSPixFormat_RGB888)
			return nsVDPixmap::kPixFormat_RGB888;
		else if (pixel_type == kAVSPixFormat_XRGB8888)
			return nsVDPixmap::kPixFormat_XRGB8888;
		else if (pixel_type == kAVSPixFormat_YUY2)
			return nsVDPixmap::kPixFormat_YUV422_YUYV;
		else
			return nsVDPixmap::kPixFormat_Null;
	}

	VDPixmap ConvertAVSVideoFrameToPixmap(const AVSVideoInfo& vi, const AVSVideoFrame& vfr) {
		VDPixmap pixmap = {0};

		pixmap.data		= (char *)vfr.pfb->data + vfr.offset;
		pixmap.palette	= NULL;
		pixmap.format	= ConvertAVSFormatToPixmapFormat(vi.pixel_type);
		pixmap.w		= vi.width;
		pixmap.h		= vi.height;
		pixmap.pitch	= vfr.pitch;

		if (VDPixmapGetInfo(pixmap.format).auxbufs) {
			pixmap.data2	= (char *)vfr.pfb->data + vfr.offsetU;
			pixmap.pitch2	= vfr.pitchUV;
			pixmap.data2	= (char *)vfr.pfb->data + vfr.offsetV;
			pixmap.pitch3	= vfr.pitchUV;
		}

		return pixmap;
	}

	struct AVSVideoFrameRefPtr {
		AVSVideoFrame *p;
	};

	struct IAVSClip {
		int		refcount;

		virtual int					__stdcall AVSGetVersion() = 0;
		virtual AVSVideoFrameRefPtr	__stdcall AVSGetFrame(int n, IScriptEnvironment *env) = 0;
		virtual bool				__stdcall AVSGetParity(int n) = 0;
		virtual void				__stdcall AVSGetAudio(void *buf, int start, int count, IScriptEnvironment *env) = 0;
		virtual const AVSVideoInfo&	__stdcall AVSGetVideoInfo() = 0;
		virtual						__stdcall ~IAVSClip() {}
	};

	struct AVSValue {
		sint16	type;
		sint16	array_size;
		union {						// 'v' = void
			IAVSClip *pClip;		// 'c'
			bool		b;			// 'b'
			sint32		i;			// 'i'
			float		f;			// 'f'
			const char *s;			// 's'
			const AVSValue *a;		// 'a' = array
		} value;

		// These are required to force VC++ to pass AVSValue return values by
		// reference instead of using EDX:EAX.
		AVSValue() {}
		~AVSValue() {}
	};
};

// This must be global as "NotFound" is a named exception.
class IScriptEnvironment {
public:
	class NotFound /*exception*/ {};  // thrown by Invoke and GetVar

	typedef AVSValue (__cdecl *ApplyFunc)(AVSValue args, void* user_data, IScriptEnvironment* env);
	typedef void (__cdecl *ShutdownFunc)(void* user_data, IScriptEnvironment* env);

	virtual				__stdcall ~IScriptEnvironment() {}

	virtual long				__stdcall AVSGetCPUFlags() = 0;
	virtual char*				__stdcall AVSSaveString(const char* s, int length = -1) = 0;
	virtual char*				__stdcall AVSSprintf(const char* fmt, ...) = 0;
	virtual char*				__stdcall AVSVSprintf(const char* fmt, va_list val) = 0;
	__declspec(noreturn)	// cont...
	virtual void				__stdcall AVSThrowError(const char* fmt, ...) = 0;
	virtual void				__stdcall AVSAddFunction(const char* name, const char* params, ApplyFunc apply, void* user_data) = 0;
	virtual bool				__stdcall AVSFunctionExists(const char* name) = 0;
	virtual AVSValue			__stdcall AVSInvoke(const char* name, const AVSValue args, const char** arg_names=0) = 0;
	virtual AVSValue			__stdcall AVSGetVar(const char* name) = 0;
	virtual bool				__stdcall AVSSetVar(const char* name, const AVSValue& val) = 0;
	virtual bool				__stdcall AVSSetGlobalVar(const char* name, const AVSValue& val) = 0;
	virtual void				__stdcall AVSPushContext(int level=0) = 0;
	virtual void				__stdcall AVSPopContext() = 0;
	virtual AVSVideoFrameRefPtr	__stdcall AVSNewVideoFrame(const AVSVideoInfo& vi, int align) = 0;		// align should be 4 or 8
	virtual bool				__stdcall AVSMakeWritable(AVSVideoFrameRefPtr *pvf) = 0;
	virtual void				__stdcall AVSBitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) = 0;
	virtual void				__stdcall AVSAtExit(ShutdownFunc function, void* user_data) = 0;
	virtual void				__stdcall AVSCheckVersion(int version) = 0;
	virtual AVSVideoFrameRefPtr	__stdcall AVSSubframe(AVSVideoFrameRefPtr src, int rel_offset, int new_pitch, int new_row_size, int new_height) = 0;
	virtual int					__stdcall AVSSetMemoryMax(int mem) = 0;
	virtual int					__stdcall AVSSetWorkingDir(const char * newdir) = 0;
};

///////////////////////////////////////////////////////////////////////////
//
//	adapter definition
//
///////////////////////////////////////////////////////////////////////////

class VDVideoFilterAvisynthAdapter : public VDVideoFilterBase, public IScriptEnvironment {
	friend class VDVideoFilterAvisynthAdapterSource;
public:
	static void *Create(const VDVideoFilterContext *pContext) {
		return new VDVideoFilterAvisynthAdapter(pContext);
	}

	VDVideoFilterAvisynthAdapter(const VDVideoFilterContext *pContext);
	~VDVideoFilterAvisynthAdapter();

	sint32 Run();
	void Prefetch(sint64 frame);
	sint32 Prepare();
	void Start();
	void Stop();
	unsigned Suspend(void *dst, unsigned size);
	void Resume(const void *src, unsigned size);
	bool Config(HWND hwnd);

protected:
	static void CALLBACK AvisynthFiberStatic(void *pThis);
	void AvisynthFiber();

	void GCFrameBuffers();

	AVSVideoFrameRefPtr GetSourceFrame(int pos, int pin);

	long				__stdcall AVSGetCPUFlags();
	char*				__stdcall AVSSaveString(const char* s, int length = -1);
	char*				__stdcall AVSSprintf(const char* fmt, ...);
	char*				__stdcall AVSVSprintf(const char* fmt, va_list val);
	__declspec(noreturn)	// cont...
	void				__stdcall AVSThrowError(const char* fmt, ...);
	void				__stdcall AVSAddFunction(const char* name, const char* params, ApplyFunc apply, void* user_data);
	bool				__stdcall AVSFunctionExists(const char* name);
	AVSValue			__stdcall AVSInvoke(const char* name, const AVSValue args, const char** arg_names=0);
	AVSValue			__stdcall AVSGetVar(const char* name);
	bool				__stdcall AVSSetVar(const char* name, const AVSValue& val);
	bool				__stdcall AVSSetGlobalVar(const char* name, const AVSValue& val);
	void				__stdcall AVSPushContext(int level=0);
	void				__stdcall AVSPopContext();
	AVSVideoFrameRefPtr	__stdcall AVSNewVideoFrame(const AVSVideoInfo& vi, int align);		// align should be 4 or 8
	bool				__stdcall AVSMakeWritable(AVSVideoFrameRefPtr *pvf);
	void				__stdcall AVSBitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height);
	void				__stdcall AVSAtExit(ShutdownFunc function, void* user_data);
	void				__stdcall AVSCheckVersion(int version);
	AVSVideoFrameRefPtr	__stdcall AVSSubframe(AVSVideoFrameRefPtr src, int rel_offset, int new_pitch, int new_row_size, int new_height);
	int					__stdcall AVSSetMemoryMax(int mem);
	int					__stdcall AVSSetWorkingDir(const char * newdir);

	void *mpMainFiber;
	void *mpFilterFiber;
	IAVSClip *mpClip;

	HMODULE		mhmodAVSFilter;

	int				mSourceFetchPin;
	VDPosition		mSourceFetchFrame;
	bool			mbExit;
	bool			mbFrameError;

	AVSVideoInfo	mVideoInfo;

	typedef std::pair<IScriptEnvironment::ShutdownFunc, void *> ShutdownEntry;
	std::vector<ShutdownEntry>	mShutdownFunctions;

	struct FrameBufferEntry {
		AVSVideoFrameBuffer *pAFB;
		VDVideoFilterFrame	*pVFB;
		VDPixmapBuffer		*pCFB;
	};
	typedef std::vector<FrameBufferEntry>		tFrameBuffers;
	tFrameBuffers mFrameBuffers;

	typedef std::vector<class VDVideoFilterAvisynthAdapterSource *> tSources;
	tSources	mSources;

	struct AVSFunctionEntry {
		VDStringA						mName;
		VDStringA						mArguments;
		IScriptEnvironment::ApplyFunc	mpFunc;
		void							*mpContext;
	};

	typedef std::list<AVSFunctionEntry> tFunctions;
	tFunctions		mFunctions;

	VDStringW		mFilterDLLName;

	struct AvisynthFilterForcedExit {};
};

class VDVideoFilterAvisynthAdapterSource : public IAVSClip {
public:
	VDVideoFilterAvisynthAdapterSource(VDVideoFilterAvisynthAdapter *pParent, const VDVideoFilterPin& srcfmt, int pin);

	int					__stdcall AVSGetVersion();
	AVSVideoFrameRefPtr	__stdcall AVSGetFrame(int n, IScriptEnvironment *env);
	bool				__stdcall AVSGetParity(int n);
	void				__stdcall AVSGetAudio(void *buf, int start, int count, IScriptEnvironment *env);
	const AVSVideoInfo&	__stdcall AVSGetVideoInfo();

protected:
	VDVideoFilterAvisynthAdapter *const mpParent;
	const int mPin;

	AVSVideoInfo	mVideoInfo;
};

VDVideoFilterAvisynthAdapterSource::VDVideoFilterAvisynthAdapterSource(VDVideoFilterAvisynthAdapter *pParent, const VDVideoFilterPin& srcfmt, int pin)
	: mpParent(pParent)
	, mPin(pin)
{
	mVideoInfo.width			= srcfmt.mpFormat->w;
	mVideoInfo.height			= srcfmt.mpFormat->h;
	mVideoInfo.fps_numerator	= srcfmt.mFrameRateHi;
	mVideoInfo.fps_denominator	= srcfmt.mFrameRateLo;
	mVideoInfo.num_frames		= srcfmt.mLength > (sint64)0x7fffffff ? 0x7fffffff : (sint32)srcfmt.mLength;
	mVideoInfo.pixel_type		= kAVSPixFormat_YUY2;
	mVideoInfo.audio_sampling_rate	= 0;
	mVideoInfo.audio_sample_type	= 0;
	mVideoInfo.audio_length			= 0;
	mVideoInfo.audio_channels		= 0;
	mVideoInfo.image_type			= 0;
}

int __stdcall VDVideoFilterAvisynthAdapterSource::AVSGetVersion() {
	return kAVSVersion;
}

AVSVideoFrameRefPtr __stdcall VDVideoFilterAvisynthAdapterSource::AVSGetFrame(int n, IScriptEnvironment *env) {
	return mpParent->GetSourceFrame(n, mPin);
}

bool __stdcall VDVideoFilterAvisynthAdapterSource::AVSGetParity(int n) {
	return false;
}

void __stdcall VDVideoFilterAvisynthAdapterSource::AVSGetAudio(void *buf, int start, int count, IScriptEnvironment *env) {
	// no audio
}

const AVSVideoInfo&	__stdcall VDVideoFilterAvisynthAdapterSource::AVSGetVideoInfo() {
	return mVideoInfo;
}

///////////////////////////////////////////////////////////////////////////
//
//	VirtualDub filter layer
//
///////////////////////////////////////////////////////////////////////////

VDVideoFilterAvisynthAdapter::VDVideoFilterAvisynthAdapter(const VDVideoFilterContext *pContext)
	: VDVideoFilterBase(pContext)
	, mpClip(NULL)
	, mhmodAVSFilter(NULL)
	, mFilterDLLName((const wchar_t *)pContext->mpFilterData)
{
}

VDVideoFilterAvisynthAdapter::~VDVideoFilterAvisynthAdapter()
{
}

sint32 VDVideoFilterAvisynthAdapter::Run() {
	// call Avisynth filter
	SwitchToFiber(mpFilterFiber);

	// what happened?
	if (mbFrameError)				// error during processing
		mpContext->mpServices->Except("error thrown by Avisynth filter");

	if (mpContext->mpDstFrame)		// dest frame produced
		return kVFVRun_OK;

	// must be a source fetch
	mpContext->Prefetch(mSourceFetchPin, mSourceFetchFrame, 0);
	return kVFVRun_NeedMoreFrames;
}

void VDVideoFilterAvisynthAdapter::Prefetch(sint64 frame) {
	// no frames needed yet (schedule immediately)
}

sint32 VDVideoFilterAvisynthAdapter::Prepare() {
	VDPixmap& dst = *mpContext->mpOutput->mpFormat;

	Start();
	Stop();

	dst.format = ConvertAVSFormatToPixmapFormat(mVideoInfo.pixel_type);
	dst.w = mVideoInfo.width;
	dst.h = mVideoInfo.height;

	return kVFVPrepare_Serialize;
}

void VDVideoFilterAvisynthAdapter::Start() {
	mSources.clear();
	mSources.push_back(new VDVideoFilterAvisynthAdapterSource(this, *mpContext->mpInputs[0], 0));

	mbExit = false;
	mbFrameError = false;

	mpMainFiber = GetCurrentFiber();
	mpFilterFiber = CreateFiber(65536, AvisynthFiberStatic, this);

	// launch Avisynth emulation and create filter
	SwitchToFiber(mpFilterFiber);

	if (mbFrameError) {
		Stop();
		throw MyError("Avisynth filter initialization failed");
	}
}

void VDVideoFilterAvisynthAdapter::Stop() {
	// terminate "Avisynth"
	mbExit = true;
	SwitchToFiber(mpFilterFiber);
	DeleteFiber(mpFilterFiber);
	mpFilterFiber = NULL;

	while(!mSources.empty()) {
		delete mSources.back();
		mSources.pop_back();
	}
}

unsigned VDVideoFilterAvisynthAdapter::Suspend(void *dst, unsigned size) {
	return 0;
}

void VDVideoFilterAvisynthAdapter::Resume(const void *src, unsigned size) {
}

bool VDVideoFilterAvisynthAdapter::Config(HWND hwnd) {
	return false;
}

///////////////////////////////////////////////////////////////////////////
//
//	Avisynth fiber layer
//
///////////////////////////////////////////////////////////////////////////

void CALLBACK VDVideoFilterAvisynthAdapter::AvisynthFiberStatic(void *pThis) {
	((VDVideoFilterAvisynthAdapter *)pThis)->AvisynthFiber();
}

void VDVideoFilterAvisynthAdapter::AvisynthFiber() {
	try {
		// load filter
		mhmodAVSFilter = LoadLibraryW(mFilterDLLName.c_str());
		if (!mhmodAVSFilter) {
			mbFrameError = true;
			SwitchToFiber(mpMainFiber);
			throw AvisynthFilterForcedExit();
		}

		FARPROC fp = GetProcAddress(mhmodAVSFilter, "AvisynthPluginInit");
		if (!fp)
			fp = GetProcAddress(mhmodAVSFilter, "_AvisynthPluginInit@4");

		try {
			const char *result = ((const char * (__stdcall *)(IScriptEnvironment *))fp)(this);

			// create new filter
			const AVSFunctionEntry& fent = mFunctions.back();

			AVSValue inputs[5];
			inputs[0].type = 'c';
			inputs[0].value.pClip = mSources[0];
			inputs[0].array_size = 0;
			inputs[1].type = 'f';
			inputs[1].value.f = 60.0f;
			inputs[1].array_size = 0;
			inputs[2].type = 'f';
			inputs[2].value.f = 1.0f;
			inputs[2].array_size = 0;
			inputs[3].type = 'f';
			inputs[3].value.f = 0.0f;
			inputs[3].array_size = 0;
			inputs[4].type = 'f';
			inputs[4].value.f = 1.0f;
			inputs[4].array_size = 0;

			AVSValue args;
			args.type = 'a';
			args.value.a = inputs;
			args.array_size = 5;

			AVSValue v = fent.mpFunc(args, fent.mpContext, this);

			if (v.type != 'c')
				mbFrameError = true;
			else {
				mpClip = v.value.pClip;
			}

			mVideoInfo = mpClip->AVSGetVideoInfo();
		} catch(const MyError&) {
			mbFrameError = true;
		}

		for(;;) {
			SwitchToFiber(mpMainFiber);
			if (mbExit)
				break;

			try {
				AVSVideoFrameRefPtr p = mpClip->AVSGetFrame((sint32)mpContext->mpOutput->mFrameNum, this);
//				AVSVideoFrameRefPtr p = mSources[0]->AVSGetFrame((sint32)mpContext->mpOutput->mFrameNum, this);

				// allocate frame and do blit
				mpContext->AllocFrame();

				VDPixmapBlt(*mpContext->mpDstFrame->mpPixmap, ConvertAVSVideoFrameToPixmap(mVideoInfo, *p.p));

				// free AVS frame
				if (!--p.p->refcount)
					--p.p->pfb->refcount;
			} catch(const MyError&) {
				mbFrameError = true;
			}

			GCFrameBuffers();
		}
	} catch(const AvisynthFilterForcedExit&) {
	}

	if (mpClip) {
		delete mpClip;
		mpClip = NULL;
	}

	while(!mShutdownFunctions.empty()) {
		ShutdownEntry& func = mShutdownFunctions.back();

		func.first(func.second, this);

		mShutdownFunctions.pop_back();
	}

	SwitchToFiber(mpMainFiber);

	VDNEVERHERE;
}

void VDVideoFilterAvisynthAdapter::GCFrameBuffers() {
	tFrameBuffers::iterator it(mFrameBuffers.begin());
	
	while(it != mFrameBuffers.end()) {
		FrameBufferEntry& e = *it;

		if (!e.pAFB->refcount) {
			if (e.pVFB)
				e.pVFB->Release();
			if (e.pCFB)
				delete e.pCFB;

			*it = mFrameBuffers.back();
			mFrameBuffers.pop_back();
		} else
			++it;
	}

	VDDEBUG("Avisynth adapter: %d frame buffers outstanding\n", mFrameBuffers.size());
}

AVSVideoFrameRefPtr VDVideoFilterAvisynthAdapter::GetSourceFrame(int pos, int pin) {

	mSourceFetchPin		= pin;
	mSourceFetchFrame	= pos;

	// fiber callback to VirtualDub for video frame
	SwitchToFiber(mpMainFiber);
	if (mbExit)
		throw AvisynthFilterForcedExit();

	// construct Avisynth frame and frame buffer from VirtualDub frame
	AVSVideoFrameBuffer *pFB = new AVSVideoFrameBuffer();
	AVSVideoFrame *pFR = new AVSVideoFrame();

	FrameBufferEntry fbe;
#if 1
	fbe.pAFB	= pFB;
	fbe.pVFB	= NULL;
	fbe.pCFB	= new VDPixmapBuffer(mpContext->mpSrcFrames[0]->mpPixmap->w, mpContext->mpSrcFrames[0]->mpPixmap->h, nsVDPixmap::kPixFormat_YUV422_YUYV);
	VDPixmap *pixmap = fbe.pCFB;

	VDVERIFY(VDPixmapBlt(*fbe.pCFB, *mpContext->mpSrcFrames[0]->mpPixmap));
#else
	fbe.pAFB	= pFB;
	fbe.pVFB	= mpContext->mpSrcFrames[0];
	fbe.pCFB	= NULL;

	VDPixmap *pixmap = fbe.pVFB->mpPixmap;
#endif

	const VDPixmapFormatInfo& formatinfo = VDPixmapGetInfo(pixmap->format);

	pFB->data				= (unsigned char *)pixmap->data + (pixmap->pitch > 0 ? 0 : -pixmap->pitch * (pixmap->h-1));
	pFB->data_size			= abs(pixmap->pitch) * pixmap->h;
	pFB->refcount			= 1;
	pFB->sequence_number	= 0;

	pFR->refcount			= 1;
	pFR->pfb				= pFB;
	pFR->offset				= 0;
	pFR->pitch				= pixmap->pitch;
	pFR->row_size			= -(-pixmap->w >> formatinfo.qwbits) * formatinfo.qsize;
	pFR->height				= -(-pixmap->h >> formatinfo.qhbits);

	if (formatinfo.auxbufs) {
		pFR->offsetU			= (sint32)((char *)pixmap->data3 - (char *)pixmap->data);
		pFR->offsetV			= (sint32)((char *)pixmap->data2 - (char *)pixmap->data);
		pFR->pitchUV			= pixmap->pitch2;
	} else {
		pFR->offsetU = 0;
		pFR->offsetV = 0;
		pFR->pitchUV = 0;
	}

	mFrameBuffers.push_back(fbe);

	AVSVideoFrameRefPtr p = { pFR };
	return p;
}

///////////////////////////////////////////////////////////////////////////
//
//	Avisynth IScriptEnvironment callbacks
//
///////////////////////////////////////////////////////////////////////////

long __stdcall VDVideoFilterAvisynthAdapter::AVSGetCPUFlags() {
	return CPUGetEnabledExtensions();
}

char *__stdcall VDVideoFilterAvisynthAdapter::AVSSaveString(const char* s, int length) {
	if (length < 0)
		length = strlen(s);
	
	char *t = (char *)malloc(length) + 1;

	strncpy(t, s, length);
	t[length] = 0;

	return t;
}

char *__stdcall VDVideoFilterAvisynthAdapter::AVSSprintf(const char* fmt, ...) {
	char buf[3072];
	va_list val;

	va_start(val, fmt);
	if ((unsigned)_vsnprintf(buf, sizeof buf, fmt, val) < 3072)
		buf[0] = 0;
	va_end(val);

	return AVSSaveString(buf);
}

char *__stdcall VDVideoFilterAvisynthAdapter::AVSVSprintf(const char* fmt, va_list val) {
	char buf[3072];

	if ((unsigned)_vsnprintf(buf, sizeof buf, fmt, val) < 3072)
		buf[0] = 0;

	return AVSSaveString(buf);
}

__declspec(noreturn) void __stdcall VDVideoFilterAvisynthAdapter::AVSThrowError(const char* fmt, ...) {
	MyError e;
	va_list val;

	va_start(val, fmt);
	e.vsetf(fmt, val);
	va_end(val);

	VDDEBUG("Avisynth adapter: AVSThrowError(\"%s\")\n", e.gets());

	throw e;
}

void __stdcall VDVideoFilterAvisynthAdapter::AVSAddFunction(const char* name, const char* params, ApplyFunc apply, void* user_data) {
	AVSFunctionEntry e;

	e.mName			= name;
	e.mArguments	= params;
	e.mpFunc		= apply;
	e.mpContext		= user_data;

	mFunctions.push_back(e);
}

bool __stdcall VDVideoFilterAvisynthAdapter::AVSFunctionExists(const char* name) {
	tFunctions::const_iterator it(mFunctions.begin()), itEnd(mFunctions.end());

	for(; it!=itEnd; ++it) {
		const AVSFunctionEntry& e = *it;

		if (e.mName == name)
			return true;
	}
	return false;
}

AVSValue __stdcall VDVideoFilterAvisynthAdapter::AVSInvoke(const char* name, const AVSValue args, const char** arg_names) {
	VDASSERT(false);		// not implemented properly
	throw IScriptEnvironment::NotFound();
}

AVSValue __stdcall VDVideoFilterAvisynthAdapter::AVSGetVar(const char* name) {
	VDASSERT(false);		// not implemented properly
	throw IScriptEnvironment::NotFound();
}

bool __stdcall VDVideoFilterAvisynthAdapter::AVSSetVar(const char* name, const AVSValue& val) {
	VDASSERT(false);		// not implemented properly
	return true;
}

bool __stdcall VDVideoFilterAvisynthAdapter::AVSSetGlobalVar(const char* name, const AVSValue& val) {
	VDASSERT(false);		// not implemented properly
	return true;
}

void __stdcall VDVideoFilterAvisynthAdapter::AVSPushContext(int level) {
	VDASSERT(false);		// not implemented properly
}

void __stdcall VDVideoFilterAvisynthAdapter::AVSPopContext() {
	VDASSERT(false);		// not implemented properly
}

AVSVideoFrameRefPtr	__stdcall VDVideoFilterAvisynthAdapter::AVSNewVideoFrame(const AVSVideoInfo& vi, int align) {
	const VDPixmap& dst = *mpContext->mpDstFrame->mpPixmap;
	AVSVideoFrameBuffer *pFB = new AVSVideoFrameBuffer();
	AVSVideoFrame *pFR = new AVSVideoFrame();

	int format = 0;
	if (vi.pixel_type == kAVSPixFormat_RGB888)
		format = nsVDPixmap::kPixFormat_RGB888;
	else if (vi.pixel_type == kAVSPixFormat_XRGB8888)
		format = nsVDPixmap::kPixFormat_XRGB8888;
	else if (vi.pixel_type == kAVSPixFormat_YUY2)
		format = nsVDPixmap::kPixFormat_YUV422_YUYV;
	else
		VDASSERT(false);

	const VDPixmapFormatInfo& formatinfo = VDPixmapGetInfo(format);

	// check compatibility against destination
	FrameBufferEntry fbe;
	fbe.pAFB	= pFB;
	fbe.pVFB	= NULL;
	fbe.pCFB	= NULL;

	VDPixmap *pixmap;

	if (vi.width == dst.w && vi.height == dst.h && dst.format == format) {
		// Assume it's a destination frame -- alloc frame
		mpContext->AllocFrame();
		pixmap = mpContext->mpDstFrame->mpPixmap;
		fbe.pVFB = mpContext->mpDstFrame;
		const_cast<VDVideoFilterFrame *&>(mpContext->mpDstFrame) = NULL;
	} else {
		fbe.pCFB	= new VDPixmapBuffer(vi.width, vi.height, format);
		pixmap = fbe.pCFB;
	}

	pFB->data				= (unsigned char *)pixmap->data + (pixmap->pitch > 0 ? 0 : -pixmap->pitch * (pixmap->h-1));
	pFB->data_size			= abs(pixmap->pitch) * pixmap->h;
	pFB->refcount			= 1;
	pFB->sequence_number	= 0;

	pFR->refcount			= 1;
	pFR->pfb				= pFB;
	pFR->offset				= 0;
	pFR->pitch				= pixmap->pitch;
	pFR->row_size			= -(-pixmap->w >> formatinfo.qwbits) * formatinfo.qsize;
	pFR->height				= -(-pixmap->h >> formatinfo.qhbits);

	if (formatinfo.auxbufs) {
		pFR->offsetU			= (sint32)((char *)pixmap->data3 - (char *)pixmap->data);
		pFR->offsetV			= (sint32)((char *)pixmap->data2 - (char *)pixmap->data);
		pFR->pitchUV			= pixmap->pitch2;
	} else {
		pFR->offsetU = 0;
		pFR->offsetV = 0;
		pFR->pitchUV = 0;
	}

	mFrameBuffers.push_back(fbe);

	AVSVideoFrameRefPtr p = { pFR };
	return p;
}

bool __stdcall VDVideoFilterAvisynthAdapter::AVSMakeWritable(AVSVideoFrameRefPtr *pvf) {
	AVSVideoFrameBuffer *pFrameBuffer = pvf->p->pfb;

	tFrameBuffers::const_iterator it(mFrameBuffers.begin()), itEnd(mFrameBuffers.end());

	for(; it != itEnd; ++it) {
		const FrameBufferEntry& ent = *it;

		if (ent.pAFB == pFrameBuffer)
			break;
	}

	if (it == itEnd)
		throw MyError("Avisynth filter adapter: AVSMakeWritable() called with illegal frame pointer");

	VDPixmap *pixmap = (*it).pCFB ? (*it).pCFB : (*it).pVFB->mpPixmap;
	AVSVideoFrameBuffer *pFB = new AVSVideoFrameBuffer();
	AVSVideoFrame *pFR = new AVSVideoFrame();

	FrameBufferEntry fbe;
	fbe.pAFB	= pFB;
	fbe.pVFB	= NULL;
	fbe.pCFB	= new VDPixmapBuffer(pvf->p->refcount >> 1, pvf->p->height, nsVDPixmap::kPixFormat_YUV422_YUYV);

	VDPixmapBlt(*fbe.pCFB, *pixmap);

	const VDPixmapFormatInfo& formatinfo = VDPixmapGetInfo(fbe.pCFB->format);

	pFB->data				= (unsigned char *)pixmap->data + (pixmap->pitch > 0 ? 0 : -pixmap->pitch * (pixmap->h-1));
	pFB->data_size			= abs(pixmap->pitch) * pixmap->h;
	pFB->refcount			= 1;
	pFB->sequence_number	= 0;

	pFR->refcount			= 1;
	pFR->pfb				= pFB;
	pFR->offset				= 0;
	pFR->pitch				= pixmap->pitch;
	pFR->row_size			= -(-pixmap->w >> formatinfo.qwbits) * formatinfo.qsize;
	pFR->height				= -(-pixmap->h >> formatinfo.qhbits);

	if (formatinfo.auxbufs) {
		pFR->offsetU			= (sint32)((char *)pixmap->data3 - (char *)pixmap->data);
		pFR->offsetV			= (sint32)((char *)pixmap->data2 - (char *)pixmap->data);
		pFR->pitchUV			= pixmap->pitch2;
	} else {
		pFR->offsetU = 0;
		pFR->offsetV = 0;
		pFR->pitchUV = 0;
	}

	mFrameBuffers.push_back(fbe);

	// swap the pointers

	if (!--pvf->p->refcount)
		--pvf->p->pfb->refcount;
	pvf->p = pFR;

	return false;
}

void __stdcall VDVideoFilterAvisynthAdapter::AVSBitBlt(BYTE* dstp, int dst_pitch, const BYTE* srcp, int src_pitch, int row_size, int height) {
	VDMemcpyRect(dstp, dst_pitch, srcp, src_pitch, row_size, height);
}

void __stdcall VDVideoFilterAvisynthAdapter::AVSAtExit(ShutdownFunc function, void* user_data) {
	mShutdownFunctions.push_back(ShutdownEntry(function, user_data));
}

void __stdcall VDVideoFilterAvisynthAdapter::AVSCheckVersion(int version) {
	VDASSERT(false);		// not implemented properly
}

AVSVideoFrameRefPtr	__stdcall VDVideoFilterAvisynthAdapter::AVSSubframe(AVSVideoFrameRefPtr src, int rel_offset, int new_pitch, int new_row_size, int new_height) {
	AVSVideoFrame *pNewFrame = new AVSVideoFrame();

	*pNewFrame = *src.p;
	pNewFrame->refcount		= 1;
	pNewFrame->offset		+= rel_offset;
	pNewFrame->pitch		= new_pitch;
	pNewFrame->row_size		= new_row_size;
	pNewFrame->height		= new_height;

	AVSVideoFrameRefPtr p = {pNewFrame};
	return p;
}

int __stdcall VDVideoFilterAvisynthAdapter::AVSSetMemoryMax(int mem) {
	return 0;
}

int __stdcall VDVideoFilterAvisynthAdapter::AVSSetWorkingDir(const char * newdir) {
	// I don't think so, Tim
	return 0;
}

///////////////////////////////////////////////////////////////////////////
//
//	video filter definition
//
///////////////////////////////////////////////////////////////////////////

const VDFilterConfigEntry vfilterDef_adapter_config={
	NULL, 0, VDFilterConfigEntry::kTypeWStr, L"config", L"Configuration string", L"Script configuration string for V1.x filter"
};

extern const struct VDVideoFilterDefinition vfilterDef_avsadapter = {
	sizeof(VDVideoFilterDefinition),
	kVFVDef_RunAsFiber,
	1,	1,
	&vfilterDef_adapter_config,
	VDVideoFilterAvisynthAdapter::Create,
	VDVideoFilterAvisynthAdapter::MainProc,
};

extern const struct VDPluginInfo vpluginDef_avsadapter = {
	sizeof(VDPluginInfo),
	L"Avisynth adapter",
	NULL,
	L"Adapts filters written against the Avisynth API.",
	0,
	kVDPluginType_Video,
	0,

	kVDPlugin_APIVersion,
	kVDPlugin_APIVersion,
	kVDPlugin_VideoAPIVersion,
	kVDPlugin_VideoAPIVersion,

	&vfilterDef_avsadapter
};
#endif
