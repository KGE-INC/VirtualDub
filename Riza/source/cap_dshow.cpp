//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2004 Avery Lee
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

#pragma warning(disable: 4786)	// STFU

#define INITGUID
#include <vd2/Riza/capdriver.h>
#include <vd2/system/vdstring.h>
#include <vd2/system/time.h>
#include <vd2/system/fraction.h>
#include <vd2/system/error.h>
#include <windows.h>
#include <objbase.h>
#include <strmif.h>
#include <uuids.h>
#include <control.h>
#include <qedit.h>
#include <evcode.h>
#include <ks.h>
#include <ksmedia.h>
#include <amvideo.h>
#include <vfwmsgs.h>
#include <olectl.h>
#include <vector>

using namespace nsVDCapture;

#pragma comment(lib, "amstrmid.lib")

extern HINSTANCE g_hInst;


#define DS_VERIFY(exp, msg) if (FAILED(hr = (exp))) { VDDEBUG("Failed: " msg " [%08lx : %s]\n", hr, GetDXErrorName(hr)); return false; } else

///////////////////////////////////////////////////////////////////////////
//
//	smart pointers
//
///////////////////////////////////////////////////////////////////////////

namespace {
	// New auto ptr for COM because the MS one has some really unsafe
	// overloads -- for instance, operator&() makes it a landmine if you
	// try putting it in an STL container. Also, the DDK doesn't come
	// with comsupp.lib.

	template<class T, const IID *T_IID>
	class VD_MSCOMAutoPtr {
	public:
		VD_MSCOMAutoPtr() : mp(NULL) {}

		VD_MSCOMAutoPtr(const VD_MSCOMAutoPtr& src)
			: mp(src.mp)
		{
			if (mp)
				mp->AddRef();
		}

		VD_MSCOMAutoPtr(T *p) : mp(p) {
			if (mp)
				mp->AddRef();
		}

		~VD_MSCOMAutoPtr() {
			if (mp)
				mp->Release();
		}

		VD_MSCOMAutoPtr& operator=(const VD_MSCOMAutoPtr& src) {
			T *const p = mp;

			if (p != mp) {
				T *pOld = mp;
				if (p)
					p->AddRef();
				mp = p;
				if (pOld)
					pOld->Release();
			}
			return *this;
		}

		VD_MSCOMAutoPtr& operator=(T *p) {
			if (mp != p) {
				T *pOld = mp;
				if(p)
					p->AddRef();
				mp = p;
				if (pOld)
					pOld->Release();
			}
			return *this;
		}

		operator T*() const {
			return mp;
		}

		T& operator*() const {
			return *mp;
		}

		T *operator->() const {
			return mp;
		}

		T **operator~() {
			if (mp) {
				mp->Release();
				mp = NULL;
			}
			return &mp;
		}

		void operator&() {}

		bool operator!() const { return !mp; }
		bool operator==(T* p) const { return mp == p; }
		bool operator!=(T* p) const { return mp != p; }

		HRESULT CreateInstance(const CLSID& clsid, IUnknown *pOuter = NULL, DWORD dwClsContext = CLSCTX_ALL) {
			if (mp) {
				mp->Release();
				mp = NULL;
			}

			return CoCreateInstance(clsid, pOuter, dwClsContext, *T_IID, (void **)&mp);
		}

		T *mp;
	};

	template<class T>
	bool operator==(T *p, const VD_MSCOMAutoPtr<T, &__uuidof(T)>& ap) {
		return p == ap.mp;
	}

	template<class T>
	bool operator!=(T *p, const VD_MSCOMAutoPtr<T, &__uuidof(T)>& ap) {
		return p != ap.mp;
	}

	#define I_HATE(x) typedef VD_MSCOMAutoPtr<x, &__uuidof(x)> x##Ptr

	I_HATE(IMoniker);
	I_HATE(IBaseFilter);
	I_HATE(IGraphBuilder);
	I_HATE(ICaptureGraphBuilder2);
	I_HATE(IMediaControl);
	I_HATE(IPin);
	I_HATE(IAMVfwCaptureDialogs);
	I_HATE(ISampleGrabber);
	I_HATE(IAMStreamConfig);
	I_HATE(IAMCrossbar);
	I_HATE(IAMTVTuner);
	I_HATE(ISpecifyPropertyPages);
	I_HATE(IVideoWindow);
	I_HATE(IMediaEventEx);
	I_HATE(IMediaSample);
	I_HATE(IEnumMediaTypes);

	#undef I_HATE
}

namespace {
	#ifdef _DEBUG
		const char *GetDXErrorName(const HRESULT hr) {
#define X(err) case err: return #err
			switch(hr) {
				X(VFW_E_INVALIDMEDIATYPE);
				X(VFW_E_INVALIDSUBTYPE);
				X(VFW_E_NEED_OWNER);
				X(VFW_E_ENUM_OUT_OF_SYNC);
				X(VFW_E_ALREADY_CONNECTED);
				X(VFW_E_FILTER_ACTIVE);
				X(VFW_E_NO_TYPES);
				X(VFW_E_NO_ACCEPTABLE_TYPES);
				X(VFW_E_INVALID_DIRECTION);
				X(VFW_E_NOT_CONNECTED);
				X(VFW_E_NO_ALLOCATOR);
				X(VFW_E_RUNTIME_ERROR);
				X(VFW_E_BUFFER_NOTSET);
				X(VFW_E_BUFFER_OVERFLOW);
				X(VFW_E_BADALIGN);
				X(VFW_E_ALREADY_COMMITTED);
				X(VFW_E_BUFFERS_OUTSTANDING);
				X(VFW_E_NOT_COMMITTED);
				X(VFW_E_SIZENOTSET);
				X(VFW_E_NO_CLOCK);
				X(VFW_E_NO_SINK);
				X(VFW_E_NO_INTERFACE);
				X(VFW_E_NOT_FOUND);
				X(VFW_E_CANNOT_CONNECT);
				X(VFW_E_CANNOT_RENDER);
				X(VFW_E_CHANGING_FORMAT);
				X(VFW_E_NO_COLOR_KEY_SET);
				X(VFW_E_NOT_OVERLAY_CONNECTION);
				X(VFW_E_NOT_SAMPLE_CONNECTION);
				X(VFW_E_PALETTE_SET);
				X(VFW_E_COLOR_KEY_SET);
				X(VFW_E_NO_COLOR_KEY_FOUND);
				X(VFW_E_NO_PALETTE_AVAILABLE);
				X(VFW_E_NO_DISPLAY_PALETTE);
				X(VFW_E_TOO_MANY_COLORS);
				X(VFW_E_STATE_CHANGED);
				X(VFW_E_NOT_STOPPED);
				X(VFW_E_NOT_PAUSED);
				X(VFW_E_NOT_RUNNING);
				X(VFW_E_WRONG_STATE);
				X(VFW_E_START_TIME_AFTER_END);
				X(VFW_E_INVALID_RECT);
				X(VFW_E_TYPE_NOT_ACCEPTED);
				X(VFW_E_SAMPLE_REJECTED);
				X(VFW_E_SAMPLE_REJECTED_EOS);
				X(VFW_E_DUPLICATE_NAME);
				X(VFW_S_DUPLICATE_NAME);
				X(VFW_E_TIMEOUT);
				X(VFW_E_INVALID_FILE_FORMAT);
				X(VFW_E_ENUM_OUT_OF_RANGE);
				X(VFW_E_CIRCULAR_GRAPH);
				X(VFW_E_NOT_ALLOWED_TO_SAVE);
				X(VFW_E_TIME_ALREADY_PASSED);
				X(VFW_E_ALREADY_CANCELLED);
				X(VFW_E_CORRUPT_GRAPH_FILE);
				X(VFW_E_ADVISE_ALREADY_SET);
				X(VFW_S_STATE_INTERMEDIATE);
				X(VFW_E_NO_MODEX_AVAILABLE);
				X(VFW_E_NO_ADVISE_SET);
				X(VFW_E_NO_FULLSCREEN);
				X(VFW_E_IN_FULLSCREEN_MODE);
				X(VFW_E_UNKNOWN_FILE_TYPE);
				X(VFW_E_CANNOT_LOAD_SOURCE_FILTER);
				X(VFW_S_PARTIAL_RENDER);
				X(VFW_E_FILE_TOO_SHORT);
				X(VFW_E_INVALID_FILE_VERSION);
				X(VFW_S_SOME_DATA_IGNORED);
				X(VFW_S_CONNECTIONS_DEFERRED);
				X(VFW_E_INVALID_CLSID);
				X(VFW_E_INVALID_MEDIA_TYPE);
				X(VFW_E_SAMPLE_TIME_NOT_SET);
				X(VFW_S_RESOURCE_NOT_NEEDED);
				X(VFW_E_MEDIA_TIME_NOT_SET);
				X(VFW_E_NO_TIME_FORMAT_SET);
				X(VFW_E_MONO_AUDIO_HW);
				X(VFW_S_MEDIA_TYPE_IGNORED);
				X(VFW_E_NO_DECOMPRESSOR);
				X(VFW_E_NO_AUDIO_HARDWARE);
				X(VFW_S_VIDEO_NOT_RENDERED);
				X(VFW_S_AUDIO_NOT_RENDERED);
				X(VFW_E_RPZA);
				X(VFW_S_RPZA);
				X(VFW_E_PROCESSOR_NOT_SUITABLE);
				X(VFW_E_UNSUPPORTED_AUDIO);
				X(VFW_E_UNSUPPORTED_VIDEO);
				X(VFW_E_MPEG_NOT_CONSTRAINED);
				X(VFW_E_NOT_IN_GRAPH);
				X(VFW_S_ESTIMATED);
				X(VFW_E_NO_TIME_FORMAT);
				X(VFW_E_READ_ONLY);
				X(VFW_S_RESERVED);
				X(VFW_E_BUFFER_UNDERFLOW);
				X(VFW_E_UNSUPPORTED_STREAM);
				X(VFW_E_NO_TRANSPORT);
				X(VFW_S_STREAM_OFF);
				X(VFW_S_CANT_CUE);
				X(VFW_E_BAD_VIDEOCD);
				X(VFW_S_NO_STOP_TIME);
				X(VFW_E_OUT_OF_VIDEO_MEMORY);
				X(VFW_E_VP_NEGOTIATION_FAILED);
				X(VFW_E_DDRAW_CAPS_NOT_SUITABLE);
				X(VFW_E_NO_VP_HARDWARE);
				X(VFW_E_NO_CAPTURE_HARDWARE);
				X(VFW_E_DVD_OPERATION_INHIBITED);
				X(VFW_E_DVD_INVALIDDOMAIN);
				X(VFW_E_DVD_NO_BUTTON);
				X(VFW_E_DVD_GRAPHNOTREADY);
				X(VFW_E_DVD_RENDERFAIL);
				X(VFW_E_DVD_DECNOTENOUGH);
				X(VFW_E_DVD_NOT_IN_KARAOKE_MODE);
				X(VFW_E_FRAME_STEP_UNSUPPORTED);
				X(VFW_E_PIN_ALREADY_BLOCKED_ON_THIS_THREAD);
				X(VFW_E_PIN_ALREADY_BLOCKED);
				X(VFW_E_CERTIFICATION_FAILURE);
				X(VFW_E_VMR_NOT_IN_MIXER_MODE);
				X(VFW_E_VMR_NO_AP_SUPPLIED);
				X(VFW_E_VMR_NO_DEINTERLACE_HW);
				X(VFW_E_VMR_NO_PROCAMP_HW);
				X(VFW_E_DVD_VMR9_INCOMPATIBLEDEC);
				X(VFW_E_BAD_KEY);
			default:
				return "";
			}
#undef X
		}
	#else
		const char *GetDXErrorName(const HRESULT hr) {
			return "";
		}
	#endif

	HRESULT AddToRot(IUnknown *pUnkGraph, DWORD *pdwRegister) 
	{
		IMoniker * pMoniker;
		IRunningObjectTable *pROT;
		if (FAILED(GetRunningObjectTable(0, &pROT))) {
			return E_FAIL;
		}
		WCHAR wsz[256];
		wsprintfW(wsz, L"FilterGraph %08p pid %08x", (DWORD_PTR)pUnkGraph, GetCurrentProcessId());
		HRESULT hr = CreateItemMoniker(L"!", wsz, &pMoniker);
		if (SUCCEEDED(hr)) {
			hr = pROT->Register(0, pUnkGraph, pMoniker, pdwRegister);
			pMoniker->Release();
		}
		pROT->Release();
		return hr;
	}

	void RemoveFromRot(DWORD pdwRegister)
	{
		IRunningObjectTable *pROT;
		if (SUCCEEDED(GetRunningObjectTable(0, &pROT))) {
			pROT->Revoke(pdwRegister);
			pROT->Release();
		}
	}

	void DestroySubgraph(IFilterGraph *pGraph, IBaseFilter *pFilt) {
		IEnumPins *pEnum;

		if (!pFilt)
			return;

		if (SUCCEEDED(pFilt->EnumPins(&pEnum))) {
			IPin *pPin;

			pEnum->Reset();

			while(S_OK == pEnum->Next(1, &pPin, 0)) {
				PIN_DIRECTION dir;

				pPin->QueryDirection(&dir);

				if (dir == PINDIR_OUTPUT) {
					IPin *pPin2;

					if (SUCCEEDED(pPin->ConnectedTo(&pPin2))) {
						PIN_INFO pi;

						if (SUCCEEDED(pPin2->QueryPinInfo(&pi))) {
							DestroySubgraph(pGraph, pi.pFilter);

							pGraph->Disconnect(pPin);
							pGraph->Disconnect(pPin2);
							pGraph->RemoveFilter(pi.pFilter);
							pi.pFilter->Release();
						}

						pPin2->Release();
					}
				}

				pPin->Release();
			}

			pEnum->Release();
		}
	}

	AM_MEDIA_TYPE *RizaCopyMediaType(const AM_MEDIA_TYPE *pSrc) {
		AM_MEDIA_TYPE *pamt;

		if (pamt = (AM_MEDIA_TYPE *)CoTaskMemAlloc(sizeof(AM_MEDIA_TYPE))) {

			*pamt = *pSrc;

			if (pamt->pbFormat = (BYTE *)CoTaskMemAlloc(pSrc->cbFormat)) {
				memcpy(pamt->pbFormat, pSrc->pbFormat, pSrc->cbFormat);

				if (pamt->pUnk)
					pamt->pUnk->AddRef();

				return pamt;
			}

			CoTaskMemFree(pamt);
		}

		return NULL;
	}

	bool RizaCopyMediaType(AM_MEDIA_TYPE *pDst, const AM_MEDIA_TYPE *pSrc) {
		*pDst = *pSrc;

		if (pDst->pbFormat = (BYTE *)CoTaskMemAlloc(pSrc->cbFormat)) {
			memcpy(pDst->pbFormat, pSrc->pbFormat, pSrc->cbFormat);

			if (pDst->pUnk)
				pDst->pUnk->AddRef();

			return true;
		}

		return false;
	}

	void RizaDeleteMediaType(AM_MEDIA_TYPE *pamt) {
		if (!pamt)
			return;

		if (pamt->pUnk)
			pamt->pUnk->Release();

		if (pamt->pbFormat)
			CoTaskMemFree(pamt->pbFormat);

		CoTaskMemFree(pamt);
	}

	class VDWaveFormatAsDShowMediaType : public AM_MEDIA_TYPE {
	public:
		VDWaveFormatAsDShowMediaType(const WAVEFORMATEX *pwfex, UINT size) {
			majortype		= MEDIATYPE_Audio;
			subtype.Data1	= pwfex->wFormatTag;
			subtype.Data2	= 0;
			subtype.Data3	= 0x0010;
			subtype.Data4[0] = 0x80;
			subtype.Data4[1] = 0x00;
			subtype.Data4[2] = 0x00;
			subtype.Data4[3] = 0xAA;
			subtype.Data4[4] = 0x00;
			subtype.Data4[5] = 0x38;
			subtype.Data4[6] = 0x9B;
			subtype.Data4[7] = 0x71;
			bFixedSizeSamples	= TRUE;
			bTemporalCompression	= FALSE;
			lSampleSize		= pwfex->nBlockAlign;
			formattype		= FORMAT_WaveFormatEx;
			pUnk			= NULL;
			cbFormat		= size;
			pbFormat		= (BYTE *)pwfex;
		}
	};
}

///////////////////////////////////////////////////////////////////////////
//
//	DirectShow sample callbacks.
//
//	We place these in separate classes because we need two of them, and
//	that means we can't place it in the device. *sigh*
//
///////////////////////////////////////////////////////////////////////////

class IVDCaptureDSCallback {
public:
	virtual void CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key) = 0;
};

class VDCapDevDSCallback : public ISampleGrabberCB {
protected:

	VDAtomicInt mRefCount;
	IVDCaptureDSCallback *mpCallback;

public:

	VDCapDevDSCallback(IVDCaptureDSCallback *pCB) : mRefCount(1), mpCallback(pCB) {
	}

	// IUnknown

	HRESULT __stdcall QueryInterface(REFIID iid, void **ppvObject) {
		if (iid == IID_IUnknown) {
			*ppvObject = static_cast<IUnknown *>(this);
			return S_OK;
		} else if (iid == IID_ISampleGrabberCB) {
			*ppvObject = static_cast<ISampleGrabberCB *>(this);
			return S_OK;
		}

		return E_NOINTERFACE;
	}

	ULONG __stdcall AddRef() {
		return ++mRefCount;
	}

	ULONG __stdcall Release() {
		int rv = --mRefCount;

		if (!rv)
			delete this;

		return (ULONG)rv;
	}

	// ISampleGrabberCB

	HRESULT __stdcall BufferCB(double SampleTime, BYTE *pBuffer, long BufferLen) {
		return E_FAIL;
	}
};

class VDCapDevDSVideoCallback : public VDCapDevDSCallback {
public:
	VDCapDevDSVideoCallback(IVDCaptureDSCallback *pCB)
		: VDCapDevDSCallback(pCB)
		, mChannel(0)
	{
	}

	void SetChannel(int chan) { mChannel = chan; }

	HRESULT __stdcall SampleCB(double SampleTime, IMediaSample *pSample) {
		BYTE *pData;
		HRESULT hr;

		// retrieve sample pointer

		if (FAILED(hr = pSample->GetPointer(&pData)))
			return hr;

		// retrieve times
		__int64 t1, t2;
		if (FAILED(hr = pSample->GetTime(&t1, &t2)))
			return hr;

#if 0
		// create buffer -- must be in heap!!

		VDCaptureBufferDS *pBuffer = new VDCaptureBufferDS(pSample, pData, pSample->GetActualDataLength(), S_OK == pSample->IsSyncPoint(), (t1+5)/10);

		// invoke callback

		if (pBuffer) {
			pBuffer->AddRef();
			mpCallback->PostVideo(pBuffer);
		}
#endif

		mpCallback->CapProcessData(mChannel, pData, pSample->GetActualDataLength(), (t1+5)/10, S_OK == pSample->IsSyncPoint());

		return S_OK;
	}

protected:
	int mChannel;
};

class VDCapDevDSAudioCallback : public VDCapDevDSCallback {
public:
	VDCapDevDSAudioCallback(IVDCaptureDSCallback *pCB)
		: VDCapDevDSCallback(pCB)
		, mChannel(1)
	{
	}

	void SetChannel(int chan) { mChannel = chan; }

	HRESULT __stdcall SampleCB(double SampleTime, IMediaSample *pSample) {
		BYTE *pData;
		HRESULT hr;

		// retrieve sample pointer
		if (FAILED(hr = pSample->GetPointer(&pData)))
			return hr;

		// retrieve times
		__int64 t1, t2;
		if (FAILED(hr = pSample->GetTime(&t1, &t2)))
			return hr;

		// create buffer -- must be in heap!!
#if 0
		VDCaptureBufferDS *pBuffer = new VDCaptureBufferDS(pSample, pData, pSample->GetActualDataLength(), true, -1);

		// invoke callback

		if (pBuffer) {
			pBuffer->AddRef();
			mpCallback->PostAudio(pBuffer);
		}
#endif
		mpCallback->CapProcessData(mChannel, pData, pSample->GetActualDataLength(), (t1+5)/10, S_OK == pSample->IsSyncPoint());
		return S_OK;
	}

protected:
	int mChannel;
};

///////////////////////////////////////////////////////////////////////////
//
//	capture driver: DirectShow
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureDriverDS : public IVDCaptureDriver, public IVDCaptureDSCallback {
	VDCaptureDriverDS(const VDCaptureDriverDS&);
	VDCaptureDriverDS& operator=(const VDCaptureDriverDS&);
public:
	VDCaptureDriverDS(IMoniker *pVideoDevice);
	~VDCaptureDriverDS();

	void	SetCallback(IVDCaptureDriverCallback *pCB);
	bool	Init(VDGUIHandle hParent);
	void	Shutdown();

	bool	IsHardwareDisplayAvailable();

	void	SetDisplayMode(nsVDCapture::DisplayMode m);
	nsVDCapture::DisplayMode		GetDisplayMode();

	void	SetDisplayRect(const vdrect32& r);
	vdrect32	GetDisplayRectAbsolute();
	void	SetDisplayVisibility(bool vis);

	void	SetFramePeriod(sint32 ms);
	sint32	GetFramePeriod();

	uint32	GetPreviewFrameCount();

	bool	GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat);
	bool	SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size);

	bool	IsAudioCapturePossible();
	bool	IsAudioCaptureEnabled();
	void	SetAudioCaptureEnabled(bool b);
	void	SetAudioAnalysisEnabled(bool b);

	void	GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats);

	bool	GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat);
	bool	SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size);

	bool	IsDriverDialogSupported(nsVDCapture::DriverDialog dlg);
	void	DisplayDriverDialog(nsVDCapture::DriverDialog dlg);

	bool	CaptureStart();
	void	CaptureStop();
	void	CaptureAbort();

protected:
	bool	StartGraph();
	bool	StopGraph();
	bool	BuildPreviewGraph();
	bool	BuildCaptureGraph();
	bool	BuildGraph(bool bNeedCapture, bool bEnableAudio);
	void	TearDownGraph();
	void	CheckForWindow();
	void	CheckForChanges();
	bool	DisplayPropertyPages(IUnknown *ptr, HWND hwndParent);
	void	DoEvents();

	static LRESULT CALLBACK StaticMessageSinkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key);

	IVDCaptureDriverCallback	*mpCB;
	DisplayMode			mDisplayMode;

	typedef std::vector<std::pair<IMonikerPtr, VDStringW> > tDeviceVector;

	IMonikerPtr			mVideoDeviceMoniker;

	// Some essentials for the filter graph.
	IGraphBuilderPtr			mpGraph;
	ICaptureGraphBuilder2Ptr	mpGraphBuilder;
	IMediaControlPtr			mpGraphControl;
	IMediaEventExPtr			mpMediaEventEx;

	// Pointers to filters and pins in the graph.
	IBaseFilterPtr		mpCapFilt;
	IPinPtr				mpRealCapturePin;		// the one on the cap filt
	IPinPtr				mpRealPreviewPin;		// the one on the cap filt
	IPinPtr				mpRealAudioPin;			// the one on the cap filt
	IAMCrossbarPtr		mpCrossbar;
	IAMCrossbarPtr		mpCrossbar2;
	IAMTVTunerPtr		mpTVTuner;
	IVideoWindowPtr		mpVideoWindow;
	IQualProp			*mpVideoQualProp;

	// Audio filter.  We may not have one of these, actually.
	IBaseFilterPtr		mpAudioCapFilt;

	// These have to be nullified when we destroy parts of the graph.
	IPinPtr				mpVideoGrabberInputPin;	// capture
	IBaseFilterPtr		mpVideoPullFilt;
	ISampleGrabberPtr	mpVideoGrabber;

	IPinPtr				mpAudioGrabberInPin;
	IBaseFilterPtr		mpAudioPullFilt;
	ISampleGrabberPtr	mpAudioGrabber;

	IAMVfwCaptureDialogsPtr		mpVFWDialogs;
	IAMStreamConfigPtr			mpVideoConfigCap;
	IAMStreamConfigPtr			mpVideoConfigPrv;
	IAMStreamConfigPtr			mpAudioConfig;

	// Callbacks
	VDCapDevDSVideoCallback		mVideoCallback;
	VDCapDevDSAudioCallback		mAudioCallback;
//	IVDCaptureEventCallback *mpEventCallback;

	// Misc flags & state
	DWORD mdwRegisterGraph;				// Used to register filter graph in ROT for graphedit
	HWND mhwndParent;					// Parent window
	HWND mhwndEventSink;				// Our dummy window used as DS event sink

	bool mbAudioCaptureEnabled;
	bool mbAudioAnalysisEnabled;
	bool mbGraphActive;					// true if the graph is currently running
	bool mbGraphHasPreview;				// true if the graph has separate capture and preview pins

	VDFraction			mfrFrameRate;
	vdstructex<WAVEFORMATEX>	mAudioFormat;

	VDTime	mCaptureStart;

	HANDLE	mCaptureThread;
	VDAtomicPtr<MyError>	mpCaptureError;

	static ATOM sMsgSinkClass;
};

ATOM VDCaptureDriverDS::sMsgSinkClass;

VDCaptureDriverDS::VDCaptureDriverDS(IMoniker *pVideoDevice)
	: mVideoDeviceMoniker(pVideoDevice)
	, mpVideoQualProp(NULL)
	, mdwRegisterGraph(0)
	, mbAudioCaptureEnabled(true)
	, mbAudioAnalysisEnabled(false)
	, mbGraphActive(false)
	, mVideoCallback(this)
	, mAudioCallback(this)
	, mpCB(NULL)
//	, mpEventCallback(NULL)
	, mhwndParent(NULL)
	, mhwndEventSink(NULL)
	, mDisplayMode(kDisplayHardware)
	, mfrFrameRate(0,0)
	, mCaptureThread(NULL)
	, mpCaptureError(NULL)
{
}

VDCaptureDriverDS::~VDCaptureDriverDS() {
	Shutdown();
}

void VDCaptureDriverDS::SetCallback(IVDCaptureDriverCallback *pCB) {
	mpCB = pCB;
}

bool VDCaptureDriverDS::Init(VDGUIHandle hParent) {
	mhwndParent = (HWND)hParent;

	HRESULT hr;

	if (!sMsgSinkClass) {
		WNDCLASS wc = { 0, StaticMessageSinkWndProc, 0, 0, g_hInst, NULL, NULL, NULL, NULL, "Riza DirectShow event sink" };

		sMsgSinkClass = RegisterClass(&wc);

		if (!sMsgSinkClass)
			return false;
	}

	// Create message sink.

	if (!(mhwndEventSink = CreateWindow((LPCTSTR)sMsgSinkClass, "", WS_POPUP, 0, 0, 0, 0, mhwndParent, NULL, g_hInst, NULL)))
		return false;

	// Create a filter graph manager.

	DS_VERIFY(mpGraph.CreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER), "create filter graph manager");

	// Create a capture filter graph builder (we're lazy).

	DS_VERIFY(mpGraphBuilder.CreateInstance(CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER), "create filter graph builder");

	mpGraphBuilder->SetFiltergraph(mpGraph);

	AddToRot(mpGraph, &mdwRegisterGraph);

	VDDEBUG("ROT entry: %x   PID: %x\n", mdwRegisterGraph, GetCurrentProcessId());

	// Try to find the event sink interface.
	if (SUCCEEDED(mpGraph->QueryInterface(IID_IMediaEventEx,(void **)~mpMediaEventEx))) {
		mpMediaEventEx->SetNotifyWindow((OAHWND)mhwndEventSink, WM_APP, (long)this);
		mpMediaEventEx->CancelDefaultHandling(EC_VIDEO_SIZE_CHANGED);
	}

	// Attempt to instantiate the capture filter.

	DS_VERIFY(mVideoDeviceMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void **)~mpCapFilt), "create capture filter");
	DS_VERIFY(mpGraph->AddFilter(mpCapFilt, L"Capture device"), "add capture filter");

	// Find the capture pin first.  If we don't have one of these, we
	// might as well give up.

	DS_VERIFY(mpGraphBuilder->FindPin(mpCapFilt, PINDIR_OUTPUT, &PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, TRUE, 0, ~mpRealCapturePin), "find capture pin");

	// Look for a preview pin.  It's actually likely that we won't get
	// one if someone has a USB webcam, so we have to be prepared for
	// it.  If we have a VideoPort pin, prefer it to a Preview pin, if
	// both are there.

	hr = mpGraphBuilder->FindPin(mpCapFilt, PINDIR_OUTPUT, &PIN_CATEGORY_VIDEOPORT, &MEDIATYPE_Video, TRUE, 0, ~mpRealPreviewPin);
	if (SUCCEEDED(hr))
		mbGraphHasPreview = true;
	else {
		hr = mpGraphBuilder->FindPin(mpCapFilt, PINDIR_OUTPUT, &PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, TRUE, 0, ~mpRealPreviewPin);
		mbGraphHasPreview = SUCCEEDED(hr);
	}

	// Look for an audio pin. We may need to attach a null renderer to
	// it for smooth previews.
	hr = mpGraphBuilder->FindPin(mpCapFilt, PINDIR_OUTPUT, NULL, &MEDIATYPE_Audio, TRUE, 0, ~mpRealAudioPin);

	// Get video format configurator

	hr = mpGraphBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Interleaved, mpCapFilt, IID_IAMStreamConfig, (void **)~mpVideoConfigCap);

	if (FAILED(hr))
		hr = mpGraphBuilder->FindInterface(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video, mpCapFilt, IID_IAMStreamConfig, (void **)~mpVideoConfigCap);

	DS_VERIFY(hr, "find video format config if");

	hr = mpGraphBuilder->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Interleaved, mpCapFilt, IID_IAMStreamConfig, (void **)~mpVideoConfigPrv);

	if (FAILED(hr))
		hr = mpGraphBuilder->FindInterface(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, mpCapFilt, IID_IAMStreamConfig, (void **)~mpVideoConfigPrv);

	// Check for VFW capture dialogs, TV tuner, and crossbar

	mpGraphBuilder->FindInterface(NULL, NULL, mpCapFilt, IID_IAMVfwCaptureDialogs, (void **)~mpVFWDialogs);
	mpGraphBuilder->FindInterface(NULL, NULL, mpCapFilt, IID_IAMCrossbar, (void **)~mpCrossbar);
	if (mpCrossbar) {
		IBaseFilterPtr pXFilt;

		if (SUCCEEDED(mpCrossbar->QueryInterface(IID_IBaseFilter, (void **)~pXFilt)))
			mpGraphBuilder->FindInterface(&LOOK_UPSTREAM_ONLY, NULL, pXFilt, IID_IAMCrossbar, (void **)~mpCrossbar2);
	}
	mpGraphBuilder->FindInterface(NULL, NULL, mpCapFilt, IID_IAMTVTuner, (void **)~mpTVTuner);



	// Initialize audio format.
	if (mpRealAudioPin) {
		std::list<vdstructex<WAVEFORMATEX> > aformats;

		GetAvailableAudioFormats(aformats);

		mAudioFormat.clear();
		if (!aformats.empty())
			mAudioFormat = aformats.front();
	}


	// Find the control interface to the graph and start it.

	DS_VERIFY(mpGraph->QueryInterface(IID_IMediaControl, (void **)~mpGraphControl), "find graph control interface");

	return BuildPreviewGraph() && StartGraph();
}

void VDCaptureDriverDS::Shutdown() {
	if (mpVideoQualProp) {
		mpVideoQualProp->Release();
		mpVideoQualProp = NULL;
	}

	if (mpVideoWindow) {
		mpVideoWindow->put_Visible(FALSE);
		mpVideoWindow->put_Owner(NULL);
		mpVideoWindow = NULL;
	}

	if (mpGraph)
		TearDownGraph();

	// FIXME:	We need to tear down the filter graph manager, but to do so we
	//			need to manually kill the smart pointers.  This is not the
	//			cleanest way to wipe a graph and there's probably something
	//			wrong with doing it this way.

	mpAudioCapFilt		= NULL;
	mpAudioGrabber		= NULL;
	mpAudioGrabberInPin	= NULL;
	mpAudioPullFilt		= NULL;
	mpAudioConfig		= NULL;

	mpCapFilt			= NULL;
	mpRealCapturePin	= NULL;
	mpRealPreviewPin	= NULL;
	mpRealAudioPin		= NULL;
	mpCrossbar			= NULL;
	mpCrossbar2			= NULL;
	mpTVTuner			= NULL;
	mpVideoWindow		= NULL;
	mpVideoGrabberInputPin = NULL;
	mpVideoPullFilt		= NULL;
	mpVideoGrabber		= NULL;
	mpVFWDialogs		= NULL;
	mpVideoConfigCap	= NULL;
	mpVideoConfigPrv	= NULL;
	mpMediaEventEx		= NULL;
	mpGraph				= NULL;
	mpGraphBuilder		= NULL;
	mpGraphControl		= NULL;

	if (mhwndEventSink) {
		DestroyWindow(mhwndEventSink);
		mhwndEventSink = NULL;
	}

	if (mdwRegisterGraph) {
		RemoveFromRot(mdwRegisterGraph);
		mdwRegisterGraph = 0;
	}

	if (mCaptureThread) {
		CloseHandle(mCaptureThread);
		mCaptureThread = NULL;
	}

	if (MyError *e = mpCaptureError.xchg(NULL))
		delete e;
}

bool VDCaptureDriverDS::IsHardwareDisplayAvailable() {
	return true;
}

void VDCaptureDriverDS::SetDisplayMode(nsVDCapture::DisplayMode mode) {
	if (mDisplayMode == mode)
		return;

	mDisplayMode = mode;

	if (mode == kDisplayNone) {
		TearDownGraph();
		return;
	}

	if (BuildPreviewGraph())
		StartGraph();
}

nsVDCapture::DisplayMode VDCaptureDriverDS::GetDisplayMode() {
	return mDisplayMode;
}

void VDCaptureDriverDS::SetDisplayRect(const vdrect32& r) {
}

vdrect32 VDCaptureDriverDS::GetDisplayRectAbsolute() {
	return vdrect32(0,0,0,0);
}

void VDCaptureDriverDS::SetDisplayVisibility(bool vis) {
}

void VDCaptureDriverDS::SetFramePeriod(sint32 ms) {
	AM_MEDIA_TYPE *past;
	VDFraction pf;
	bool bRet = false;

	StopGraph();

	if (SUCCEEDED(mpVideoConfigCap->GetFormat(&past))) {
		if (past->formattype == FORMAT_VideoInfo) {
			VIDEOINFOHEADER *pvih = (VIDEOINFOHEADER *)past->pbFormat;

			pvih->AvgTimePerFrame = ms*10;

			bRet = SUCCEEDED(mpVideoConfigCap->SetFormat(past));
		}

		RizaDeleteMediaType(past);
	}

	if (SUCCEEDED(mpVideoConfigPrv->GetFormat(&past))) {
		if (past->formattype == FORMAT_VideoInfo) {
			VIDEOINFOHEADER *pvih = (VIDEOINFOHEADER *)past->pbFormat;

			pvih->AvgTimePerFrame = ms*10;

			bRet = SUCCEEDED(mpVideoConfigPrv->SetFormat(past));
		}

		RizaDeleteMediaType(past);
	}

	VDDEBUG("Desired frame period = %u us, actual = %u us\n", ms, GetFramePeriod());

	StartGraph();

	VDASSERT(bRet);
}

sint32 VDCaptureDriverDS::GetFramePeriod() {
	AM_MEDIA_TYPE *past;
	VDFraction pf;
	bool bRet = false;

	if (SUCCEEDED(mpVideoConfigCap->GetFormat(&past))) {
		if (past->formattype == FORMAT_VideoInfo) {
			const VIDEOINFOHEADER *pvih = (const VIDEOINFOHEADER *)past->pbFormat;

			pf = Fraction::reduce64(10000000, pvih->AvgTimePerFrame);

			bRet = true;
		}

		RizaDeleteMediaType(past);
	}

	if (!bRet) {
		VDASSERT(false);
		return 1000000/15;
	}

	return (sint32)pf.scale64ir(1000000);
}

uint32 VDCaptureDriverDS::GetPreviewFrameCount() {
	int framesDrawn;

	if (mpVideoQualProp && SUCCEEDED(mpVideoQualProp->get_FramesDrawn(&framesDrawn)))
		return (uint32)framesDrawn;

	return 0;
}

bool VDCaptureDriverDS::GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat) {
	AM_MEDIA_TYPE *pamt;
	IEnumMediaTypes *pEnum;

	// ConnectionMediaType would be the ideal way to do this, but unfortunately
	// in preview mode the capture pin isn't connected.  However, the sample
	// grabber doesn't impose any restraints, so it's not unreasonable to assume
	// that the first format enumerated is the one that gets grabbed.

	if (FAILED(mpRealCapturePin->EnumMediaTypes(&pEnum)))
		return false;

	vformat.clear();

	pEnum->Reset();

	if (S_OK == pEnum->Next(1, &pamt, 0)) {
		if (pamt->formattype == FORMAT_VideoInfo) {
			const VIDEOINFOHEADER *pvih = (const VIDEOINFOHEADER *)pamt->pbFormat;

			vformat.assign(&pvih->bmiHeader, pamt->cbFormat - offsetof(VIDEOINFOHEADER, bmiHeader));
		}

		RizaDeleteMediaType(pamt);
	}

	pEnum->Release();

	return !vformat.empty();
}

bool VDCaptureDriverDS::SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size) {
	return false;
}

bool VDCaptureDriverDS::IsAudioCapturePossible() {
	return !!mpRealAudioPin;
}

bool VDCaptureDriverDS::IsAudioCaptureEnabled() {
	return mbAudioCaptureEnabled && !!mpRealAudioPin;
}

void VDCaptureDriverDS::SetAudioCaptureEnabled(bool b) {
	if (mbAudioCaptureEnabled == b)
		return;

	mbAudioCaptureEnabled = b;

	if (mbAudioAnalysisEnabled) {
		TearDownGraph();
		if (BuildPreviewGraph())
			StartGraph();
	}
}

void VDCaptureDriverDS::SetAudioAnalysisEnabled(bool b) {
	if (mbAudioAnalysisEnabled == b)
		return;

	mbAudioAnalysisEnabled = b;

	if (mbAudioCaptureEnabled) {
		TearDownGraph();
		if (BuildPreviewGraph())
			StartGraph();
	}
}

void VDCaptureDriverDS::GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats) {
	aformats.clear();

	if (!mpRealAudioPin)
		return;

	IEnumMediaTypesPtr pEnum;

	if (FAILED(mpRealAudioPin->EnumMediaTypes(~pEnum)))
		return;

	AM_MEDIA_TYPE *pMediaType;
	while(S_OK == pEnum->Next(1, &pMediaType, NULL)) {

		if (pMediaType->majortype == MEDIATYPE_Audio && pMediaType->formattype == FORMAT_WaveFormatEx
			&& pMediaType->cbFormat >= sizeof(WAVEFORMATEX)) {
			const WAVEFORMATEX *pwfex = (const WAVEFORMATEX *)pMediaType->pbFormat;

			if (pwfex->wFormatTag == WAVE_FORMAT_PCM)
				aformats.push_back(vdstructex<WAVEFORMATEX>(pwfex, sizeof(WAVEFORMATEX)));
			else if (pwfex->cbSize + sizeof(WAVEFORMATEX) <= pMediaType->cbFormat)
				aformats.push_back(vdstructex<WAVEFORMATEX>(pwfex, pwfex->cbSize + sizeof(WAVEFORMATEX)));
		}

		RizaDeleteMediaType(pMediaType);
	}
}

bool VDCaptureDriverDS::GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat) {
	if (!mpRealAudioPin)
		return false;

	if (!mAudioFormat.empty()) {
		aformat = mAudioFormat;
		return true;
	}

	return false;
}

bool VDCaptureDriverDS::SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size) {
	VDWaveFormatAsDShowMediaType	amt(pwfex, size);

	if (SUCCEEDED(mpRealAudioPin->QueryAccept(&amt))) {
		mAudioFormat.assign(pwfex, size);

		if (mbAudioCaptureEnabled && mbAudioAnalysisEnabled) {
			if (BuildPreviewGraph())
				StartGraph();
		}	

		return true;
	}

	return false;
}

bool VDCaptureDriverDS::IsDriverDialogSupported(nsVDCapture::DriverDialog dlg) {
	switch(dlg) {
	case kDialogVideoFormat:		return mpVFWDialogs && SUCCEEDED(mpVFWDialogs->HasDialog(VfwCaptureDialog_Format));
	case kDialogVideoSource:		return mpVFWDialogs && SUCCEEDED(mpVFWDialogs->HasDialog(VfwCaptureDialog_Source));
	case kDialogVideoDisplay:		return mpVFWDialogs && SUCCEEDED(mpVFWDialogs->HasDialog(VfwCaptureDialog_Display));
	case kDialogVideoCaptureFilter:	return DisplayPropertyPages(mpCapFilt, NULL);
	case kDialogVideoCapturePin:	return DisplayPropertyPages(mpRealCapturePin, NULL);
	case kDialogVideoPreviewPin:	return DisplayPropertyPages(mpRealPreviewPin, NULL);
	case kDialogVideoCrossbar:		return DisplayPropertyPages(mpCrossbar, NULL);
	case kDialogVideoCrossbar2:		return DisplayPropertyPages(mpCrossbar2, NULL);
	case kDialogTVTuner:			return DisplayPropertyPages(mpTVTuner, NULL);
	}

	return false;
}

void VDCaptureDriverDS::DisplayDriverDialog(nsVDCapture::DriverDialog dlg) {
	bool bRestart = mbGraphActive;

	// The filter graph must be stopped for VFW dialogs.

	switch(dlg) {
	case kDialogVideoFormat:
	case kDialogVideoSource:
	case kDialogVideoDisplay:

		if (mpVFWDialogs) {						
			StopGraph();

			switch(dlg) {
			case kDialogVideoFormat:
				VDVERIFY(SUCCEEDED(mpVFWDialogs->ShowDialog(VfwCaptureDialog_Format, mhwndParent)));
				break;
			case kDialogVideoSource:
				VDVERIFY(SUCCEEDED(mpVFWDialogs->ShowDialog(VfwCaptureDialog_Source, mhwndParent)));
				break;
			case kDialogVideoDisplay:
				VDVERIFY(SUCCEEDED(mpVFWDialogs->ShowDialog(VfwCaptureDialog_Display, mhwndParent)));
				break;
			}

			// Restart the filter graph.

			if (bRestart)
				StartGraph();
		}

		break;

	case kDialogVideoCapturePin:
	case kDialogVideoPreviewPin:
		TearDownGraph();

		switch(dlg) {
		case kDialogVideoCapturePin:
			DisplayPropertyPages(mpRealCapturePin, mhwndParent);
			break;
		case kDialogVideoPreviewPin:
			DisplayPropertyPages(mpRealPreviewPin, mhwndParent);
			break;
		}

		// Restart the filter graph.

		BuildPreviewGraph();
		if (bRestart)
			StartGraph();
		break;

	case kDialogVideoCaptureFilter:
		DisplayPropertyPages(mpCapFilt, mhwndParent);
		break;
	case kDialogVideoCrossbar:
		DisplayPropertyPages(mpCrossbar, mhwndParent);
		break;
	case kDialogVideoCrossbar2:
		DisplayPropertyPages(mpCrossbar2, mhwndParent);
		break;
	case kDialogTVTuner:
		DisplayPropertyPages(mpTVTuner, mhwndParent);
		break;
	}

	CheckForChanges();
}

bool VDCaptureDriverDS::CaptureStart() {
	if (VDINLINEASSERTFALSE(mCaptureThread)) {
		CloseHandle(mCaptureThread);
		mCaptureThread = NULL;
	}
	HANDLE hProcess = GetCurrentProcess();
	if (!DuplicateHandle(hProcess, GetCurrentThread(), hProcess, &mCaptureThread, 0, FALSE, DUPLICATE_SAME_ACCESS))
		return false;

	// switch to a capture graph, but don't start it.
	if (BuildCaptureGraph()) {

		// cancel default handling for EC_REPAINT, otherwise
		// the Video Renderer will start sending back extra requests
		mpMediaEventEx->CancelDefaultHandling(EC_REPAINT);

		// kick the sample grabbers and go!!
		mpVideoGrabber->SetCallback(&mVideoCallback, 0);

		if (mpAudioGrabber)
			mpAudioGrabber->SetCallback(&mAudioCallback, 0);

		mCaptureStart = VDGetPreciseTick();

		bool success = false;
		if (mpCB) {
			try {
				mpCB->CapBegin(0);
				success = true;
			} catch(MyError& e) {
				MyError *p = new MyError;
				p->TransferFrom(e);
				delete mpCaptureError.xchg(p);
			}
		}
		if (success && StartGraph())
			return true;

		if (mpCB)
			mpCB->CapEnd(mpCaptureError ? mpCaptureError : NULL);

		delete mpCaptureError.xchg(NULL);

		if (mpAudioGrabber)
			mpAudioGrabber->SetCallback(NULL, 0);

		mpVideoGrabber->SetCallback(NULL, 0);
	}

	mpMediaEventEx->RestoreDefaultHandling(EC_REPAINT);

	BuildPreviewGraph();

	StartGraph();
	return false;
}

void VDCaptureDriverDS::CaptureStop() {
	StopGraph();

	if (mpVideoGrabber)
		mpVideoGrabber->SetCallback(NULL, 0);

	mpMediaEventEx->RestoreDefaultHandling(EC_REPAINT);

	if (mpCB) {
		mpCB->CapEnd(mpCaptureError ? mpCaptureError : NULL);
	}
	delete mpCaptureError.xchg(NULL);

	if (mCaptureThread) {
		CloseHandle(mCaptureThread);
		mCaptureThread = NULL;
	}

	// Switch to a preview graph.

	BuildPreviewGraph();
	StartGraph();
}

void VDCaptureDriverDS::CaptureAbort() {
	CaptureStop();
}

bool VDCaptureDriverDS::StopGraph() {
	if (!mbGraphActive)
		return true;

	mbGraphActive = false;

	return SUCCEEDED(mpGraphControl->Stop());
}

bool VDCaptureDriverDS::StartGraph() {
	if (mbGraphActive)
		return true;

	return mbGraphActive = SUCCEEDED(mpGraphControl->Run());
}

//	BuildPreviewGraph()
//
//	This routine builds the preview part of a graph.  It turns out that
//	if you try to leave the Capture pin connected, VFW drivers may not
//	send anything over their Preview pin (@#*&$*($).
//
//	Usually, this simply involves creating a video renderer and slapping
//	it on the Preview pin.
//
bool VDCaptureDriverDS::BuildPreviewGraph() {
	mVideoCallback.SetChannel(-1);
	mAudioCallback.SetChannel(-2);
	if (!BuildGraph(mDisplayMode == kDisplaySoftware, mbAudioCaptureEnabled && mbAudioAnalysisEnabled))
		return false;

	if (mpAudioGrabber && mbAudioCaptureEnabled && mbAudioAnalysisEnabled)
		mpAudioGrabber->SetCallback(&mAudioCallback, 0);


	return true;
}

//	BuildCaptureGraph()
//
//	This routine builds the capture part of the graph:
//
//	* Check if the capture filter only has a capture pin.  If so, insert
//	  a smart tee filter to make a fake preview pin.
//
//	* Connect a sample grabber and then a null renderer onto the capture
//	  pin.
//
//	* Render the preview pin.
//
bool VDCaptureDriverDS::BuildCaptureGraph() {
	mVideoCallback.SetChannel(0);
	mAudioCallback.SetChannel(1);
	return BuildGraph(true, mbAudioCaptureEnabled);
}

bool VDCaptureDriverDS::BuildGraph(bool bNeedCapture, bool bEnableAudio) {
	IPinPtr pCapturePin = mpRealCapturePin, pPreviewPin = mpRealPreviewPin;
	HRESULT hr;

	// Tear down existing graph.

	TearDownGraph();

	// If we have an audio capture pin, attach a renderer to it. We do this even if we are
	// just previewing, because the Plextor PX-M402U doesn't output frames reliably
	// otherwise.
	if (mpRealAudioPin && ((!bNeedCapture && !mbAudioAnalysisEnabled) || !bEnableAudio)) {
		IBaseFilterPtr pAudioNullRenderer;
		IPinPtr pAudioNullRendererInput;

		DS_VERIFY(pAudioNullRenderer.CreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER), "create audio null renderer");

		DS_VERIFY(mpGraph->AddFilter(pAudioNullRenderer, L"audio sink"), "add audio sink");
		DS_VERIFY(mpGraphBuilder->FindPin(pAudioNullRenderer, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pAudioNullRendererInput), "find input on null renderer");
		DS_VERIFY(mpRealAudioPin->Connect(pAudioNullRendererInput, NULL), "attach audio null renderer");
	}

	// Determine if we need a smart tee -- we need a smart tee only if
	// both preview and capture are desired, and either the capture
	// filter has no preview pin, or we are going to force software
	// display.

	bool bNeedPreview = (mDisplayMode != kDisplayNone && mDisplayMode != kDisplaySoftware);

//	if ((mDisplayMode == kSoftware || !mbGraphHasPreview) && bNeedPreview && bNeedCapture) {
	if (!mbGraphHasPreview) {
		IBaseFilterPtr pSmartTee;
		IPinPtr pSmartTeeInput;

		DS_VERIFY(pSmartTee.CreateInstance(CLSID_SmartTee, NULL, CLSCTX_INPROC_SERVER), "create smart tee");
		DS_VERIFY(mpGraph->AddFilter(pSmartTee, L"Preview-maker"), "add smart tee");

		// Find the input pin and attach it to the capture pin we found.

		DS_VERIFY(mpGraphBuilder->FindPin(pSmartTee, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pSmartTeeInput), "find smart tee input");
		DS_VERIFY(pSmartTeeInput->Connect(mpRealCapturePin, NULL), "attach smart tee");

		// Grab the pins off the smart tee.

		DS_VERIFY(mpGraphBuilder->FindPin(pSmartTee, PINDIR_OUTPUT, NULL, NULL, TRUE, 0, ~pCapturePin), "find capture pin on smart tee");
		DS_VERIFY(mpGraphBuilder->FindPin(pSmartTee, PINDIR_OUTPUT, NULL, NULL, TRUE, 1, ~pPreviewPin), "find preview pin on smart tee");
	}

	// Construct the capture portion of the graph.

	if (bNeedCapture) {
		// First, create a sample grabber and null renderer.

		IBaseFilterPtr pNullRenderer;

		DS_VERIFY(mpVideoPullFilt.CreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER), "create sample grabber");
		DS_VERIFY(mpGraph->AddFilter(mpVideoPullFilt, L"Video pulldown"), "add sample grabber");
		DS_VERIFY(pNullRenderer.CreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER), "create null renderer");
		DS_VERIFY(mpGraph->AddFilter(pNullRenderer, L"Video sink"), "add null renderer");

		// Set the sample grabber to continuous mode.

		DS_VERIFY(mpVideoPullFilt->QueryInterface(IID_ISampleGrabber, (void **)~mpVideoGrabber), "find sample grabber if");
		DS_VERIFY(mpVideoGrabber->SetOneShot(FALSE), "switch sample grabber to continuous");

		AM_MEDIA_TYPE vamt;

		vamt.majortype	= MEDIATYPE_Video;
		vamt.subtype	= GUID_NULL;
		vamt.bFixedSizeSamples = FALSE;
		vamt.bTemporalCompression = FALSE;
		vamt.lSampleSize	= 0;
		vamt.formattype	= FORMAT_VideoInfo;
		vamt.pUnk		= NULL;
		vamt.cbFormat	= 0;
		vamt.pbFormat	= NULL;

		DS_VERIFY(mpVideoGrabber->SetMediaType(&vamt), "set video sample grabber format");

		// Attach the sink to the grabber, and the grabber to the capture pin.

		IPinPtr pPinSGIn, pPinSGOut, pPinNRIn;

		DS_VERIFY(mpGraphBuilder->FindPin(mpVideoPullFilt, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pPinSGIn), "find sample grabber input");
		DS_VERIFY(mpGraphBuilder->FindPin(mpVideoPullFilt, PINDIR_OUTPUT, NULL, NULL, TRUE, 0, ~pPinSGOut), "find sample grabber output");
		DS_VERIFY(mpGraphBuilder->FindPin(pNullRenderer, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pPinNRIn), "find null renderer input");

		DS_VERIFY(pCapturePin->Connect(pPinSGIn, NULL), "connect capture -> grabber");
		DS_VERIFY(pPinSGOut->Connect(pPinNRIn, NULL), "connect grabber -> sink");

		mpVideoGrabberInputPin = pPinSGIn;
	}

	if (bNeedCapture || (bEnableAudio && mbAudioAnalysisEnabled)) {
		// We need to do the audio now.

		if (mpRealAudioPin && bEnableAudio) {
			IBaseFilterPtr pNullRenderer;
			IPinPtr pPinSGIn, pPinSGOut, pPinNRIn;

			DS_VERIFY(mpAudioPullFilt.CreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER), "create sample grabber");
			DS_VERIFY(mpGraph->AddFilter(mpAudioPullFilt, L"Audio pulldown"), "add sample grabber");
			DS_VERIFY(pNullRenderer.CreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER), "create null renderer");
			DS_VERIFY(mpGraph->AddFilter(pNullRenderer, L"Audio sink"), "add null renderer");

			// Set the sample grabber to continuous mode.

			DS_VERIFY(mpAudioPullFilt->QueryInterface(IID_ISampleGrabber, (void **)~mpAudioGrabber), "find sample grabber if");

			if (mAudioFormat.empty())
				return false;

			VDWaveFormatAsDShowMediaType amt(mAudioFormat.data(), mAudioFormat.size());

			DS_VERIFY(mpAudioGrabber->SetMediaType(&amt), "set media type");
			DS_VERIFY(mpAudioGrabber->SetOneShot(FALSE), "switch sample grabber to continuous");

			// Attach the sink to the grabber, and the grabber to the capture pin.

			DS_VERIFY(mpGraphBuilder->FindPin(mpAudioPullFilt, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pPinSGIn), "find sample grabber input");
			DS_VERIFY(mpGraphBuilder->FindPin(mpAudioPullFilt, PINDIR_OUTPUT, NULL, NULL, TRUE, 0, ~pPinSGOut), "find sample grabber output");
			DS_VERIFY(mpGraphBuilder->FindPin(pNullRenderer, PINDIR_INPUT, NULL, NULL, TRUE, 0, ~pPinNRIn), "find null renderer input");

			DS_VERIFY(mpRealAudioPin->Connect(pPinSGIn, NULL), "connect capture -> grabber");
			DS_VERIFY(pPinSGOut->Connect(pPinNRIn, NULL), "connect grabber -> sink");

			mpAudioGrabberInPin = pPinSGIn;
		}
	}

	// Render the preview path.

	switch(mDisplayMode) {
	case kDisplaySoftware:

#if 0
		{
			// Construct the preview part of the graph.  First, attach our special
			// YUV converter.

			IBaseFilterPtr pFilt(CreateRizaYUVFlipper());
			IPinPtr pPinIn, pPinOut;

			if (NULL == pFilt)
				return false;

			DS_VERIFY(mpGraph->AddFilter(pFilt, L"Riza YUV converter"), "add YUV converter");
			DS_VERIFY(mpGraphBuilder->FindPin(pFilt, PINDIR_INPUT, NULL, NULL, TRUE, 0, &pPinIn), "find YUV converter input");
			DS_VERIFY(mpGraphBuilder->FindPin(pFilt, PINDIR_OUTPUT, NULL, NULL, TRUE, 0, &pPinOut), "find YUV converter output");

			DS_VERIFY((bNeedCapture ? pPreviewPin : pCapturePin)->Connect(pPinIn, NULL), "connect YUV converter input");

			// Render the capture pin, or the forked preview pin.

			DS_VERIFY(mpGraphBuilder->RenderStream(NULL, &MEDIATYPE_Video,
				pPinOut, NULL, NULL), "render preview pin");
		}
		break;
#endif

	case kDisplayHardware:

		// Render the preview pin if it exists, else go to capture.

		DS_VERIFY(mpGraphBuilder->RenderStream(NULL, &MEDIATYPE_Video,
			pPreviewPin ? pPreviewPin : pCapturePin, NULL, NULL), "render preview pin");
		break;

	}

	// Check for a window and return.
	CheckForWindow();

	// Broadcast events if values changed.
	CheckForChanges();

	return true;
}

//
//	TearDownGraph()
//
//	This rips up everything in the filter graph beyond the capture filter.
//
void VDCaptureDriverDS::TearDownGraph() {
	StopGraph();

	if (mpAudioGrabber)
		mpAudioGrabber->SetCallback(NULL, 0);

	mpVideoGrabberInputPin	= NULL;
	mpVideoPullFilt			= NULL;
	mpVideoGrabber			= NULL;
	mpVideoWindow			= NULL;
	if (mpVideoQualProp) {
		mpVideoQualProp->Release();
		mpVideoQualProp = NULL;
	}

	mpAudioGrabberInPin = NULL;
	mpAudioPullFilt = NULL;
	mpAudioGrabber = NULL;

	DestroySubgraph(mpGraph, mpCapFilt);
	DestroySubgraph(mpGraph, mpAudioCapFilt);
}

//
//	CheckForWindow()
//
//	Look for a video window in the capture graph, and make it ours if
//	there is one.
//
void VDCaptureDriverDS::CheckForWindow() {
	if (mpVideoQualProp) {
		mpVideoQualProp->Release();
		mpVideoQualProp = NULL;
	}

	IEnumFilters *pEnumFilters = NULL;

	if (SUCCEEDED(mpGraph->EnumFilters(&pEnumFilters))) {
		IBaseFilter *pFilter = NULL;

		while(S_OK == pEnumFilters->Next(1, &pFilter, NULL)) {
			bool success = SUCCEEDED(pFilter->QueryInterface(IID_IQualProp, (void **)&mpVideoQualProp));

			pFilter->Release();

			if (success)
				break;
		}

		pEnumFilters->Release();
	}

	if (SUCCEEDED(mpGraph->QueryInterface(IID_IVideoWindow, (void **)~mpVideoWindow))) {
		long styles;

		if (SUCCEEDED(mpVideoWindow->get_WindowStyle(&styles))) {
			mpVideoWindow->put_WindowStyle(styles & ~(WS_CAPTION|WS_THICKFRAME));
		}

		// Add WS_EX_NOPARENTNOTIFY to try to avoid certain kinds of deadlocks,
		// since the video window is in a different thread than its parent.

		if (SUCCEEDED(mpVideoWindow->get_WindowStyleEx(&styles))) {
			mpVideoWindow->put_WindowStyleEx(styles | WS_EX_NOPARENTNOTIFY);
		}


		mpVideoWindow->put_Left(0);
		mpVideoWindow->put_Top(0);
		mpVideoWindow->put_Owner((OAHWND)mhwndParent);
	}
}

void VDCaptureDriverDS::CheckForChanges() {
#if 0
	if (mpEventCallback) {
		Fraction f;

		if (getParameterf(kPFramerate, f) && f != mfrFrameRate) {
			mpEventCallback->EventFrameRateChanged(mfrFrameRate);
		}

	}
#endif
}

bool VDCaptureDriverDS::DisplayPropertyPages(IUnknown *ptr, HWND hwndParent) {
	if (!ptr)
		return false;

	ISpecifyPropertyPages *pPages;

	if (FAILED(ptr->QueryInterface(IID_ISpecifyPropertyPages, (void **)&pPages)))
		return false;

	CAUUID cauuid;
	bool bSuccess = false;

	if (SUCCEEDED(pPages->GetPages(&cauuid))) {
		if (cauuid.cElems) {
			if (hwndParent) {
				HRESULT hr = OleCreatePropertyFrame(hwndParent, 0, 0, NULL, 1,
					(IUnknown **)&pPages, cauuid.cElems, (GUID *)cauuid.pElems, 0, 0, NULL);

				bSuccess = SUCCEEDED(hr);
			} else
				bSuccess = true;
		}

		CoTaskMemFree(cauuid.pElems);
	}

	pPages->Release();
	return bSuccess;
}

void VDCaptureDriverDS::DoEvents() {
	long evCode;
	LONG_PTR param1, param2;

	while(SUCCEEDED(mpMediaEventEx->GetEvent(&evCode, &param1, &param2, 0))) {
#if 0
		if (mpEventCallback)
			switch(evCode) {
			case EC_VIDEO_SIZE_CHANGED:
				mpEventCallback->EventVideoSizeChanged(LOWORD(param1), HIWORD(param1));
				break;
			}
#endif
		mpMediaEventEx->FreeEventParams(evCode, param1, param2);
	}
}

LRESULT CALLBACK VDCaptureDriverDS::StaticMessageSinkWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	if (msg == WM_APP)
		((VDCaptureDriverDS *)lParam)->DoEvents();
	else if (msg == WM_APP+1) {
		((VDCaptureDriverDS *)lParam)->CaptureStop();
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void VDCaptureDriverDS::CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key) {
	if (mpCaptureError)
		return;

	if (mpCB) {
		sint32 reltime = (sint64)((sint64)(VDGetPreciseTick() - mCaptureStart) * 1000000.0 / VDGetPreciseTicksPerSecond());

		try {
			mpCB->CapProcessData(stream, data, size, timestamp, key, reltime);
		} catch(MyError& e) {
			MyError *e2 = new MyError;
			e2->TransferFrom(e);
			delete mpCaptureError.xchg(e2);

			PostMessage(mhwndEventSink, WM_APP+1, 0, (LPARAM)this);
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	capture system: DirectShow
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureSystemDS : public IVDCaptureSystem {
public:
	VDCaptureSystemDS();
	~VDCaptureSystemDS();

	void EnumerateDrivers();

	int GetDeviceCount();
	const wchar_t *GetDeviceName(int index);

	IVDCaptureDriver *CreateDriver(int deviceIndex);

protected:
	typedef std::vector<std::pair<IMonikerPtr, VDStringW> > tDeviceVector;

	void Enumerate(tDeviceVector& devlist, REFCLSID devclsid);

	int mDriverCount;

	tDeviceVector mVideoDevices;
};

IVDCaptureSystem *VDCreateCaptureSystemDS() {
	return new VDCaptureSystemDS;
}

VDCaptureSystemDS::VDCaptureSystemDS()
	: mDriverCount(0)
{
	CoInitialize(NULL);
}

VDCaptureSystemDS::~VDCaptureSystemDS() {
	CoUninitialize();
}

void VDCaptureSystemDS::EnumerateDrivers() {
	mDriverCount = 0;
	mVideoDevices.clear();

	Enumerate(mVideoDevices, CLSID_VideoInputDeviceCategory);
	mDriverCount = mVideoDevices.size();
}

int VDCaptureSystemDS::GetDeviceCount() {
	return mDriverCount;
}

const wchar_t *VDCaptureSystemDS::GetDeviceName(int index) {
	if ((unsigned)index >= mDriverCount)
		return NULL;

	return mVideoDevices[index].second.c_str();
}

IVDCaptureDriver *VDCaptureSystemDS::CreateDriver(int index) {
	if ((unsigned)index >= mDriverCount)
		return NULL;

	return new VDCaptureDriverDS(mVideoDevices[index].first);
}

void VDCaptureSystemDS::Enumerate(tDeviceVector& devlist, REFCLSID devclsid) {
	ICreateDevEnum *pCreateDevEnum;
	IEnumMoniker *pEm = NULL;

	if (SUCCEEDED(CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void **)&pCreateDevEnum))) {
		pCreateDevEnum->CreateClassEnumerator(devclsid, &pEm, 0);
		pCreateDevEnum->Release();
	}

	if (pEm) {
		IMoniker *pM;
		ULONG cFetched;

		while(SUCCEEDED(pEm->Next(1, &pM, &cFetched)) && cFetched==1) {
			IPropertyBag *pPropBag;

			if (SUCCEEDED(pM->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pPropBag))) {
				VARIANT varName;

				varName.vt = VT_BSTR;

				if (SUCCEEDED(pPropBag->Read(L"FriendlyName", &varName, 0))) {
					bool isVFWDriver = false;
					LPOLESTR displayName;
					if (SUCCEEDED(pM->GetDisplayName(NULL, NULL, &displayName))) {
						// Detect a VFW driver by the compression manager tag.
						if (!wcsncmp(displayName, L"@device:cm:", 11))
							isVFWDriver = true;
						CoTaskMemFree(displayName);
					}

					devlist.push_back(tDeviceVector::value_type(pM, VDStringW(varName.bstrVal) + (isVFWDriver ? L" (VFW>DirectShow)" : L" (DirectShow)")));

					SysFreeString(varName.bstrVal);
				}

				pPropBag->Release();
			}

			pM->Release();
		}

		pEm->Release();
	}
}
