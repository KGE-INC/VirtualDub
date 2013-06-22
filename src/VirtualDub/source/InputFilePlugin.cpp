//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2007 Avery Lee
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

#include "stdafx.h"
#include <vd2/plugin/vdinputdriver.h>

///////////////////////////////////////////////////////////////////////////////

#include <vd2/system/refcount.h>
#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/debug.h>
#include <vd2/system/w32assist.h>
#include <vd2/Riza/bitmap.h>
#include "InputFile.h"
#include "plugins.h"

///////////////////////////////////////////////////////////////////////////////

#define VDASSERT_PREEXT_RT(condition, args) VDASSERT(condition)
#define VDASSERT_POSTEXT_RT(condition, args) VDASSERT(condition)

///////////////////////////////////////////////////////////////////////////////

class VDInputDriverContextImpl : public VDInputDriverContext, public IVDPluginCallbacks {
public:
	VDInputDriverContextImpl(const VDPluginDescription *);
	~VDInputDriverContextImpl();

	void BeginExternalCall();
	void EndExternalCall();

public:
	virtual void * VDXAPIENTRY GetExtendedAPI(const char *pExtendedAPIName);
	virtual void VDXAPIENTRYV SetError(const char *format, ...);
	virtual void VDXAPIENTRY SetErrorOutOfMemory();
	virtual uint32 VDXAPIENTRY GetCPUFeatureFlags();

	VDStringW	mName;
	MyError		mError;
};

VDInputDriverContextImpl::VDInputDriverContextImpl(const VDPluginDescription *pInfo) {
	mName.sprintf(L"Input driver plugin \"%S\"", pInfo->mName.c_str());
	mpCallbacks = this;
}

VDInputDriverContextImpl::~VDInputDriverContextImpl() {
}

void VDInputDriverContextImpl::BeginExternalCall() {
	mError.clear();
}

void VDInputDriverContextImpl::EndExternalCall() {
	if (mError.gets()) {
		MyError tmp;
		tmp.TransferFrom(mError);
		throw tmp;
	}
}

void * VDXAPIENTRY VDInputDriverContextImpl::GetExtendedAPI(const char *pExtendedAPIName) {
	return NULL;
}

void VDXAPIENTRYV VDInputDriverContextImpl::SetError(const char *format, ...) {
	va_list val;
	va_start(val, format);
	mError.vsetf(format, val);
	va_end(val);
}

void VDXAPIENTRY VDInputDriverContextImpl::SetErrorOutOfMemory() {
	MyMemoryError e;
	mError.TransferFrom(e);
}

uint32 VDXAPIENTRY VDInputDriverContextImpl::GetCPUFeatureFlags() {
	return CPUGetEnabledExtensions();
}

struct VDInputDriverCallAutoScope {
	VDInputDriverCallAutoScope(VDInputDriverContextImpl& context) : mContext(context) {
		mContext.BeginExternalCall();
	}

	~VDInputDriverCallAutoScope() {
		mContext.EndExternalCall();
	}

	operator bool() const { return false; }

	VDInputDriverContextImpl& mContext;
};

// We have to be careful NOT to call the error cleanup code when an exception
// occurs, because it'll throw another exception!
#define vdwithinputplugin(context) switch(VDExternalCodeBracket _exbracket = ((context).BeginExternalCall(), VDExternalCodeBracketLocation((context).mName.c_str(), __FILE__, __LINE__))) while((context).EndExternalCall(), false) case false: default:

///////////////////////////////////////////////////////////////////////////////

namespace {
	typedef VDAtomicInt VDXAtomicInt;

	template<class T> class vdxunknown : public T {
	public:
		vdxunknown() : mRefCount(0) {}
		vdxunknown(const vdxunknown<T>& src) : mRefCount(0) {}		// do not copy the refcount
		virtual ~vdxunknown() {}

		vdxunknown<T>& operator=(const vdxunknown<T>&) {}			// do not copy the refcount

		inline virtual int VDXAPIENTRY AddRef() {
			return mRefCount.inc();
		}

		inline virtual int VDXAPIENTRY Release() {
			if (mRefCount == 1) {		// We are the only reference, so there is no threading issue.  Don't decrement to zero as this can cause double destruction with a temporary addref/release in destruction.
				delete this;
				return 0;
			}

			VDASSERT(mRefCount > 1);

			return mRefCount.dec();
		}

		virtual void *VDXAPIENTRY AsInterface(uint32 iid) {
			if (iid == IVDXUnknown::kIID)
				return static_cast<IVDXUnknown *>(this);

			if (iid == T::kIID)
				return static_cast<T *>(this);

			return NULL;
		}

	protected:
		VDXAtomicInt		mRefCount;
	};

	template<class T>
	inline uint32 vdxpoly_id_from_ptr(T *p) {
		return T::kIID;
	}

	template<class T>
	T vdxpoly_cast(IVDXUnknown *pUnk) {
		return pUnk ? (T)pUnk->AsInterface(vdxpoly_id_from_ptr(T(NULL))) : NULL;
	}
}

class VDVideoDecoderModelDefaultIP : public vdxunknown<IVDXVideoDecoderModel> {
public:
	VDVideoDecoderModelDefaultIP(IVDVideoSource *pVS);
	~VDVideoDecoderModelDefaultIP();

	void	VDXAPIENTRY Reset();
	void	VDXAPIENTRY SetDesiredFrame(sint64 frame_num);
	sint64	VDXAPIENTRY GetNextRequiredSample(bool& is_preroll);
	int		VDXAPIENTRY GetRequiredCount();
	bool	VDXAPIENTRY IsDecodable(sint64 sample_num);

protected:
	sint64	mLastFrame;
	sint64	mNextFrame;
	sint64	mDesiredFrame;

	IVDVideoSource *mpVS;
};

VDVideoDecoderModelDefaultIP::VDVideoDecoderModelDefaultIP(IVDVideoSource *pVS)
	: mpVS(pVS)
	, mLastFrame(-1)
	, mNextFrame(-1)
	, mDesiredFrame(-1)
{
}

VDVideoDecoderModelDefaultIP::~VDVideoDecoderModelDefaultIP() {
}

void VDXAPIENTRY VDVideoDecoderModelDefaultIP::Reset() {
	mLastFrame = -1;
}

void VDXAPIENTRY VDVideoDecoderModelDefaultIP::SetDesiredFrame(sint64 frame_num) {
	mDesiredFrame = frame_num;

	// Fast path for previous frame or current frame.
	if (frame_num == mLastFrame) {
		mNextFrame = -1;
		mDesiredFrame = -1;
		return;
	}

	if (frame_num == mLastFrame + 1) {
		mNextFrame = frame_num;
		return;
	}

	// Back off to last key frame.
	mNextFrame = mpVS->nearestKey(frame_num);

	// Check if we have already decoded a frame that is nearer; if so we can resume
	// decoding from that point.
	if (mLastFrame >= mNextFrame && mLastFrame < frame_num)
		mNextFrame = mLastFrame + 1;
}

sint64 VDXAPIENTRY VDVideoDecoderModelDefaultIP::GetNextRequiredSample(bool& is_preroll) {
	if (mDesiredFrame < 0) {
		is_preroll = false;
		return -1;
	}

	sint64 frame = mNextFrame++;
	is_preroll = true;

	mLastFrame = frame;

	if (frame == mDesiredFrame) {
		mDesiredFrame = -1;
		is_preroll = false;
	}

	return frame;
}

int VDXAPIENTRY VDVideoDecoderModelDefaultIP::GetRequiredCount() {
	return VDClampToSint32(mDesiredFrame - mLastFrame);
}

bool VDXAPIENTRY VDVideoDecoderModelDefaultIP::IsDecodable(sint64 sample_num) {
	if (mLastFrame == mDesiredFrame)
		return true;
	
	return mpVS->isKey(sample_num);
}

///////////////////////////////////////////////////////////////////////////////
class VDVideoSourcePlugin : public VideoSource {
public:
	VDVideoSourcePlugin(IVDXVideoSource *pVS, VDInputDriverContextImpl *pContext);
	~VDVideoSourcePlugin();

	// DubSource
	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);

	// VideoSource
	const void *getFrameBuffer();
	const VDPixmap& getTargetFormat();
	bool setTargetFormat(int format);
	bool setDecompressedFormat(int depth);
	bool setDecompressedFormat(const BITMAPINFOHEADER *pbih);

	void streamSetDesiredFrame(VDPosition frame_num);
	VDPosition streamGetNextRequiredFrame(bool& is_preroll);
	int	streamGetRequiredCount(uint32 *totalsize);
	const void *streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition sample_num, VDPosition target_sample);
	uint32 streamGetDecodePadding();

	void streamBegin(bool fRealTime, bool bForceReset);
	void streamRestart();

	void invalidateFrameBuffer();
	bool isFrameBufferValid();

	const void *getFrame(VDPosition frameNum);
	char		getFrameTypeChar(VDPosition lFrameNum);

	eDropType	getDropType(VDPosition lFrameNum);

	bool isKey(VDPosition lSample);
	VDPosition nearestKey(VDPosition lSample);
	VDPosition prevKey(VDPosition lSample);
	VDPosition nextKey(VDPosition lSample);

	bool isKeyframeOnly();

	VDPosition	streamToDisplayOrder(VDPosition sample_num);
	VDPosition	displayToStreamOrder(VDPosition display_num);
	VDPosition	getRealDisplayFrame(VDPosition display_num);

	bool		isDecodable(VDPosition sample_num);

	sint64		getSampleBytePosition(VDPosition sample_num);

protected:
	vdrefptr<IVDXVideoSource> mpXVS;
	vdrefptr<IVDXStreamSource> mpXS;
	vdrefptr<IVDXVideoDecoder> mpXVDec;
	vdrefptr<IVDXVideoDecoderModel> mpXVDecModel;

	VDInputDriverContextImpl& mContext;

	VDXStreamSourceInfo	mSSInfo;
	VDXVideoSourceInfo	mVSInfo;
};

VDVideoSourcePlugin::VDVideoSourcePlugin(IVDXVideoSource *pVS, VDInputDriverContextImpl *pContext)
	: mpXVS(pVS)
	, mpXS(vdxpoly_cast<IVDXStreamSource *>(mpXVS))
	, mContext(*pContext)
{
	memset(&mSSInfo, 0, sizeof mSSInfo);
	memset(&mVSInfo, 0, sizeof mVSInfo);

	vdwithinputplugin(mContext) {
		mpXS->GetStreamSourceInfo(mSSInfo);
		pVS->GetVideoSourceInfo(mVSInfo);

		// create a video decoder.
		pVS->CreateVideoDecoder(~mpXVDec);
	}

	if (!mpXVDec)
		throw MyMemoryError();

	// create a video decoder model.
	vdwithinputplugin(mContext) {
		pVS->CreateVideoDecoderModel(~mpXVDecModel);
	}

	switch(mVSInfo.mDecoderModel) {
	case VDXVideoSourceInfo::kDecoderModelCustom:
		break;
	case VDXVideoSourceInfo::kDecoderModelDefaultIP:
		mpXVDecModel = new VDVideoDecoderModelDefaultIP(this);
		break;
	default:
		throw MyError("Error detected in input driver plugin: Unsupported video decoder model (%d).", mVSInfo.mDecoderModel);
	}

	if (!mpXVDecModel)
		throw MyMemoryError();

	mSampleFirst = 0;
	mSampleLast = mSSInfo.mSampleCount;

	const void *format;
	
	vdwithinputplugin(mContext) {
		format = mpXS->GetDirectFormat();
	}

	if (format) {
		int len;
		vdwithinputplugin(mContext) {
			len = mpXS->GetDirectFormatLen();
		}
		memcpy(allocFormat(len), format, len);
	} else {
		BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)allocFormat(sizeof(BITMAPINFOHEADER));

		bih->biSize				= sizeof(BITMAPINFOHEADER);
		bih->biWidth			= mVSInfo.mWidth;
		bih->biHeight			= mVSInfo.mHeight;
		bih->biPlanes			= 1;
		bih->biCompression		= 0xFFFFFFFF;
		bih->biBitCount			= 32;
		bih->biSizeImage		= 0;
		bih->biXPelsPerMeter	= 0;
		bih->biYPelsPerMeter	= 0;
		bih->biClrUsed			= 0;
		bih->biClrImportant		= 0;
	}

	streamInfo.fccType			= streamtypeVIDEO;
	streamInfo.fccHandler		= 0;
	streamInfo.dwFlags			= 0;
	streamInfo.dwCaps			= 0;
	streamInfo.wPriority		= 0;
	streamInfo.wLanguage		= 0;
	streamInfo.dwScale			= mSSInfo.mSampleRate.mDenominator;
	streamInfo.dwRate			= mSSInfo.mSampleRate.mNumerator;
	streamInfo.dwStart			= 0;
	streamInfo.dwLength			= VDClampToUint32(mSSInfo.mSampleCount);
	streamInfo.dwInitialFrames	= 0;
	streamInfo.dwSuggestedBufferSize = 0;
	streamInfo.dwQuality		= (DWORD)-1;
	streamInfo.dwSampleSize		= 0;
	streamInfo.rcFrame.left		= 0;
	streamInfo.rcFrame.top		= 0;
	streamInfo.rcFrame.right	= mVSInfo.mWidth;
	streamInfo.rcFrame.bottom	= mVSInfo.mHeight;
	streamInfo.dwEditCount		= 0;
	streamInfo.dwFormatChangeCount = 0;
	streamInfo.szName[0]		= 0;
}

VDVideoSourcePlugin::~VDVideoSourcePlugin() {
}

int VDVideoSourcePlugin::_read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *pBytesRead, uint32 *pSamplesRead) {
	uint32 actualBytes;
	uint32 actualSamples;

	bool result;
	
	vdwithinputplugin(mContext) {
		result = mpXS->Read(lStart, lCount, lpBuffer, cbBuffer, &actualBytes, &actualSamples);
	}

	if (pBytesRead)
		*pBytesRead = actualBytes;
	if (pSamplesRead)
		*pSamplesRead = actualSamples;

	return result || !lpBuffer ? AVIERR_OK : AVIERR_BUFFERTOOSMALL;
}

const void *VDVideoSourcePlugin::getFrameBuffer() {
	const void *p;
	vdwithinputplugin(mContext) {
		p = mpXVDec->GetFrameBufferBase();
	}

	return p;
}

const VDPixmap& VDVideoSourcePlugin::getTargetFormat() {
	const VDPixmap *p;
	vdwithinputplugin(mContext) {
		p = (const VDPixmap *)&mpXVDec->GetFrameBuffer();
	}

	return *p;
}

bool VDVideoSourcePlugin::setTargetFormat(int format) {
	vdwithinputplugin(mContext) {
		if (!mpXVDec->SetTargetFormat(format, true))
			return false;

		mpXVDecModel->Reset();
	}

	const VDXPixmap *px;

	vdwithinputplugin(mContext) {
		px = &mpXVDec->GetFrameBuffer();
	}

	VDMakeBitmapFormatFromPixmapFormat(mpTargetFormatHeader, px->format, 0, px->w, px->h);

	return true;
}

bool VDVideoSourcePlugin::setDecompressedFormat(int depth) {
	switch(depth) {
		case 8:
			return setTargetFormat(nsVDPixmap::kPixFormat_Pal8);
		case 16:
			return setTargetFormat(nsVDPixmap::kPixFormat_XRGB1555);
		case 24:
			return setTargetFormat(nsVDPixmap::kPixFormat_RGB888);
		case 32:
			return setTargetFormat(nsVDPixmap::kPixFormat_XRGB8888);
		default:
			return false;
	}
}

bool VDVideoSourcePlugin::setDecompressedFormat(const BITMAPINFOHEADER *pbih) {
	// Note that we are deliberately paying attention to sign here; if we have a flipped DIB
	// then we want to punt to the SetDecompressedFormat() function.
	if ((uint32)pbih->biWidth == mVSInfo.mWidth && (uint32)pbih->biHeight == mVSInfo.mHeight) {
		int format = VDBitmapFormatToPixmapFormat(*pbih);
		if (format)
			return setTargetFormat(format);
	}

	vdwithinputplugin(mContext) {
		if (!mpXVDec->SetDecompressedFormat((const VDXBITMAPINFOHEADER *)pbih))
			return false;
	}

	mpTargetFormatHeader.assign(pbih, VDGetSizeOfBitmapHeaderW32(pbih));
	return true;
}

void VDVideoSourcePlugin::streamSetDesiredFrame(VDPosition frame_num) {
	VDASSERT_PREEXT_RT((uint64)frame_num < (uint64)mSampleLast, ("streamSetDesiredFrame(): frame %I64d not in 0-%I64d", frame_num, mSampleLast - 1));

	if (frame_num >= mSampleLast)
		frame_num = mSampleLast - 1;
	if (frame_num < mSampleFirst)
		frame_num = mSampleFirst;

	sint64 stream_num;
	vdwithinputplugin(mContext) {
		stream_num = mpXVS->GetSampleNumberForFrame(frame_num);
	}

	VDASSERT_POSTEXT_RT((uint64)stream_num < (uint64)mSampleLast, ("displayToStreamOrder(%I64d) returned out of range %I64d (should be in 0-%I64d)", frame_num, stream_num, mSampleLast - 1));

	vdwithinputplugin(mContext) {
		mpXVDecModel->SetDesiredFrame(stream_num);
	}
}

VDPosition VDVideoSourcePlugin::streamGetNextRequiredFrame(bool& is_preroll) {
	sint64 pos;
	
	vdwithinputplugin(mContext) {
		pos = mpXVDecModel->GetNextRequiredSample(is_preroll);
	}

	return pos;
}

int	VDVideoSourcePlugin::streamGetRequiredCount(uint32 *totalsize) {
	VDASSERT(!totalsize);
	int count;
	
	vdwithinputplugin(mContext) {
		count = mpXVDecModel->GetRequiredCount();
	}

	return count;
}

const void *VDVideoSourcePlugin::streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition sample_num, VDPosition target_num) {
	const void *fb;
	
	vdwithinputplugin(mContext) {
		fb = mpXVDec->DecodeFrame(data_len ? inputBuffer : NULL, data_len, is_preroll, sample_num, target_num);
	}

	return fb;
}

uint32 VDVideoSourcePlugin::streamGetDecodePadding() {
	uint32 padding;
	
	vdwithinputplugin(mContext) {
		padding = mpXVDec->GetDecodePadding();
	}

	return padding;
}

void VDVideoSourcePlugin::streamBegin(bool fRealTime, bool bForceReset) {
	if (bForceReset) {
		vdwithinputplugin(mContext) {
			mpXVDecModel->Reset();
		}
	}
}

void VDVideoSourcePlugin::streamRestart() {
	vdwithinputplugin(mContext) {
		mpXVDecModel->Reset();
	}
}

void VDVideoSourcePlugin::invalidateFrameBuffer() {
	vdwithinputplugin(mContext) {
		mpXVDec->Reset();
	}
}

bool VDVideoSourcePlugin::isFrameBufferValid() {
	bool fbvalid;
	
	vdwithinputplugin(mContext) {
		fbvalid = mpXVDec->IsFrameBufferValid();
	}

	return fbvalid;
}

const void *VDVideoSourcePlugin::getFrame(VDPosition frameNum) {
	// range check
	if (frameNum < 0 || frameNum >= mSSInfo.mSampleCount)
		return NULL;

	sint64 sampleNum;
	vdwithinputplugin(mContext) {
		sampleNum = mpXVS->GetSampleNumberForFrame(frameNum);
		mpXVDecModel->SetDesiredFrame(sampleNum);
	}

	// decode frames until we get to the desired point
	vdblock<char> buffer;

	bool is_preroll;
	do {
		VDPosition pos = mpXVDecModel->GetNextRequiredSample(is_preroll);
		uint32 padding = mpXVDec->GetDecodePadding();

		if (pos < 0) {
			vdwithinputplugin(mContext) {
				mpXVDec->DecodeFrame(NULL, 0, is_preroll, -1, sampleNum);
			}
		} else {
			uint32 actualSamples;
			uint32 actualBytes;

			for(;;) {
				bool result = false;
				
				if (buffer.size() > padding) {
					vdwithinputplugin(mContext) {
						result = mpXS->Read(pos, 1, buffer.data(), buffer.size() - padding, &actualBytes, &actualSamples);
					}

					if (result && !buffer.empty())
						break;
				}

				vdwithinputplugin(mContext) {
					result = mpXS->Read(pos, 1, NULL, 0, &actualBytes, &actualSamples);
				}

				if (!result)
					throw MyError("Error detected in plugin \"%ls\": A size query call to IVDXStreamSource::Read() returned false for sample %u.", mContext.mName.c_str(), (unsigned)pos);

				buffer.resize(actualBytes + padding);
			}

			vdwithinputplugin(mContext) {
				mpXVDec->DecodeFrame(buffer.data(), actualBytes, is_preroll, pos, sampleNum);
			}
		}
	} while(is_preroll);

	const void *fb;
	
	vdwithinputplugin(mContext) {
		fb = mpXVDec->GetFrameBufferBase();
	}

	return fb;
}

char VDVideoSourcePlugin::getFrameTypeChar(VDPosition lFrameNum) {
	if (lFrameNum < mSampleFirst || lFrameNum >= mSampleLast)
		return ' ';

	vdwithinputplugin(mContext) {
		lFrameNum = mpXVS->GetSampleNumberForFrame(lFrameNum);
	}

	VDXVideoFrameInfo frameInfo;

	vdwithinputplugin(mContext) {
		mpXVS->GetSampleInfo(lFrameNum, frameInfo);
	}

	return frameInfo.mTypeChar;
}

IVDVideoSource::eDropType VDVideoSourcePlugin::getDropType(VDPosition frame) {
	VDXVideoFrameInfo frameInfo;

	if (frame < mSampleFirst || frame >= mSampleLast)
		return IVDVideoSource::kDroppable;

	vdwithinputplugin(mContext) {
		mpXVS->GetSampleInfo(frame, frameInfo);
	}

	switch(frameInfo.mFrameType) {
	case kVDXVFT_Independent:
		return IVDVideoSource::kIndependent;
	case kVDXVFT_Predicted:
		return IVDVideoSource::kDependant;
	case kVDXVFT_Bidirectional:
	case kVDXVFT_Null:
		return IVDVideoSource::kDroppable;
	default:
		return IVDVideoSource::kDroppable;
	}
}

bool VDVideoSourcePlugin::isKey(VDPosition frame) {
	bool iskey;

	if (frame < mSampleFirst || frame >= mSampleLast)
		return false;
	
	vdwithinputplugin(mContext) {
		VDPosition stream_num = mpXVS->GetSampleNumberForFrame(frame);
		iskey = mpXVS->IsKey(stream_num);
	}

	return iskey;
}

VDPosition VDVideoSourcePlugin::nearestKey(VDPosition frame) {
	if (isKey(frame))
		return frame;

	frame = prevKey(frame);
	if (frame < 0)
		frame = 0;

	return frame;
}

VDPosition VDVideoSourcePlugin::prevKey(VDPosition frame) {
	while(--frame >= mSampleFirst) {
		if (isKey(frame))
			return frame;
	}

	return -1;
}

VDPosition VDVideoSourcePlugin::nextKey(VDPosition frame) {
	while(++frame < mSampleLast) {
		if (isKey(frame))
			return frame;
	}

	return -1;
}

bool VDVideoSourcePlugin::isKeyframeOnly() {
	return 0 != (mVSInfo.mFlags & VDXVideoSourceInfo::kFlagKeyframeOnly);
}

VDPosition VDVideoSourcePlugin::streamToDisplayOrder(VDPosition sample_num) {
	if (sample_num < mSampleFirst || sample_num >= mSampleLast)
		return sample_num;

	sint64 display_num;
	vdwithinputplugin(mContext) {
		display_num = mpXVS->GetFrameNumberForSample(sample_num);
	}

	return display_num;
}

VDPosition VDVideoSourcePlugin::displayToStreamOrder(VDPosition frame) {
	if (frame < mSampleFirst || frame >= mSampleLast)
		return frame;

	sint64 stream_num;	
	vdwithinputplugin(mContext) {
		stream_num = mpXVS->GetSampleNumberForFrame(frame);
	}

	return stream_num;
}

VDPosition VDVideoSourcePlugin::getRealDisplayFrame(VDPosition frame) {
	if (frame < mSampleFirst || frame >= mSampleLast)
		return frame;

	VDPosition pos;
	vdwithinputplugin(mContext) {
		pos = mpXVS->GetRealFrame(frame);
	}

	return pos;
}

bool VDVideoSourcePlugin::isDecodable(VDPosition sample_num) {
	if (sample_num < mSampleFirst || sample_num >= mSampleLast)
		return false;

	bool decodable;
	vdwithinputplugin(mContext) {
		decodable = mpXVDec->IsDecodable(sample_num);
	}

	return decodable;
}

sint64 VDVideoSourcePlugin::getSampleBytePosition(VDPosition sample_num) {
	if (sample_num < mSampleFirst || sample_num >= mSampleLast)
		return -1;

	sint64 bytepos;
	vdwithinputplugin(mContext) {
		bytepos = mpXVS->GetSampleBytePosition(sample_num);
	}

	return bytepos;
}

///////////////////////////////////////////////////////////////////////////////
class VDAudioSourcePlugin : public AudioSource {
public:
	VDAudioSourcePlugin(IVDXAudioSource *pVS, VDInputDriverContextImpl *pContext);
	~VDAudioSourcePlugin();

	// DubSource
	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);

	bool IsVBR() const { return mbIsVBR; }
	VDPosition	TimeToPositionVBR(VDTime us) const;
	VDTime		PositionToTimeVBR(VDPosition samples) const;

protected:
	vdrefptr<IVDXAudioSource> mpXAS;
	vdrefptr<IVDXStreamSource> mpXS;

	bool	mbIsVBR;

	VDInputDriverContextImpl&	mContext;

	VDXStreamSourceInfo		mSSInfo;
	VDXAudioSourceInfo		mASInfo;
};

VDAudioSourcePlugin::VDAudioSourcePlugin(IVDXAudioSource *pAS, VDInputDriverContextImpl *pContext)
	: mpXAS(pAS)
	, mpXS(vdxpoly_cast<IVDXStreamSource *>(mpXAS))
	, mContext(*pContext)
{
	memset(&mSSInfo, 0, sizeof mSSInfo);
	memset(&mASInfo, 0, sizeof mASInfo);

	vdwithinputplugin(mContext) {
		mpXS->GetStreamSourceInfo(mSSInfo);
		pAS->GetAudioSourceInfo(mASInfo);
		mbIsVBR = mpXS->IsVBR();
	}

	mSampleFirst = 0;
	mSampleLast = mSSInfo.mSampleCount;

	const void *format;
	vdwithinputplugin(mContext) {
		format = mpXS->GetDirectFormat();
	}
	if (format) {
		int len;
		vdwithinputplugin(mContext) {
			len = mpXS->GetDirectFormatLen();
		}
		memcpy(allocFormat(len), format, len);
	} else {
		throw MyError("The audio stream has a custom format that cannot be supported.");
	}

	streamInfo.fccType			= streamtypeAUDIO;
	streamInfo.fccHandler		= 0;
	streamInfo.dwFlags			= 0;
	streamInfo.dwCaps			= 0;
	streamInfo.wPriority		= 0;
	streamInfo.wLanguage		= 0;
	streamInfo.dwScale			= mSSInfo.mSampleRate.mDenominator;
	streamInfo.dwRate			= mSSInfo.mSampleRate.mNumerator;
	streamInfo.dwStart			= 0;
	streamInfo.dwLength			= VDClampToUint32(mSSInfo.mSampleCount);
	streamInfo.dwInitialFrames	= 0;
	streamInfo.dwSuggestedBufferSize = 0;
	streamInfo.dwQuality		= (DWORD)-1;
	streamInfo.dwSampleSize		= ((const WAVEFORMATEX *)format)->nBlockAlign;
	streamInfo.rcFrame.left		= 0;
	streamInfo.rcFrame.top		= 0;
	streamInfo.rcFrame.right	= 0;
	streamInfo.rcFrame.bottom	= 0;
	streamInfo.dwEditCount		= 0;
	streamInfo.dwFormatChangeCount = 0;
	streamInfo.szName[0]		= 0;
}

VDAudioSourcePlugin::~VDAudioSourcePlugin() {
}

int VDAudioSourcePlugin::_read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *pBytesRead, uint32 *pSamplesRead) {
	uint32 actualBytes;
	uint32 actualSamples;

	bool result;
	
	vdwithinputplugin(mContext) {
		result = mpXS->Read(lStart, lCount, lpBuffer, cbBuffer, &actualBytes, &actualSamples);
	}

	if (pBytesRead)
		*pBytesRead = actualBytes;
	if (pSamplesRead)
		*pSamplesRead = actualSamples;

	return result || !lpBuffer ? AVIERR_OK : AVIERR_BUFFERTOOSMALL;
}

VDPosition VDAudioSourcePlugin::TimeToPositionVBR(VDTime us) const {
	if (mbIsVBR) {
		VDPosition pos;
		vdwithinputplugin(mContext) {
			pos = mpXS->TimeToPositionVBR(us);
		}
		return pos;
	}

	return AudioSource::TimeToPositionVBR(us);
}

VDTime VDAudioSourcePlugin::PositionToTimeVBR(VDPosition samples) const {
	if (mbIsVBR) {
		VDTime t;
		vdwithinputplugin(mContext) {
			t = mpXS->PositionToTimeVBR(samples);
		}
		return t;
	}

	return AudioSource::PositionToTimeVBR(samples);
}

///////////////////////////////////////////////////////////////////////////////
class VDInputFileOptionsPlugin : public InputFileOptions {
public:
	VDInputFileOptionsPlugin(IVDXInputOptions *opts, VDInputDriverContextImpl& context);
	~VDInputFileOptionsPlugin();

	IVDXInputOptions *GetXObject() const { return mpXOptions; }

	int write(char *buf, int buflen);

protected:
	vdrefptr<IVDXInputOptions> mpXOptions;
	VDInputDriverContextImpl& mContext;
};

VDInputFileOptionsPlugin::VDInputFileOptionsPlugin(IVDXInputOptions *opts, VDInputDriverContextImpl& context)
	: mpXOptions(opts)
	, mContext(context)
{
}

VDInputFileOptionsPlugin::~VDInputFileOptionsPlugin() {
}

int VDInputFileOptionsPlugin::write(char *buf, int buflen) {
	int result;

	vdwithinputplugin(mContext) {
		result = mpXOptions->Write(buf, (uint32)buflen);
	}

	return result;
}

///////////////////////////////////////////////////////////////////////////////
class VDInputFilePlugin : public InputFile {
public:
	VDInputFilePlugin(IVDXInputFile *p, VDPluginDescription *pDesc, VDInputDriverContextImpl *pContext);
	~VDInputFilePlugin();

	void Init(const wchar_t *szFile);
	bool Append(const wchar_t *szFile);

	void setOptions(InputFileOptions *);
	InputFileOptions *promptForOptions(HWND);
	InputFileOptions *createOptions(const void *buf, uint32 len);
	void InfoDialog(HWND hwndParent);

	void GetTextInfo(tFileTextInfo& info);

	bool isOptimizedForRealtime();
	bool isStreaming();

protected:
	vdrefptr<IVDXInputFile> mpXObject;
	vdrefptr<IVDXInputOptions> mpXOptions;

	VDInputDriverContextImpl&	mContext;

	VDPluginDescription	*mpPluginDesc;
	const VDPluginInfo				*mpPluginInfo;
};

VDInputFilePlugin::VDInputFilePlugin(IVDXInputFile *p, VDPluginDescription *pDesc, VDInputDriverContextImpl *pContext)
	: mpXObject(p)
	, mpPluginDesc(pDesc)
	, mContext(*pContext)
{
	mpPluginInfo = VDLockPlugin(pDesc);
}

VDInputFilePlugin::~VDInputFilePlugin() {
	audioSrc = NULL;
	videoSrc = NULL;
	mpXOptions = NULL;
	mpXObject = NULL;

	if (mpPluginInfo)
		VDUnlockPlugin(mpPluginDesc);
}

void VDInputFilePlugin::Init(const wchar_t *szFile) {
	vdwithinputplugin(mContext) {
		mpXObject->Init(szFile, mpXOptions);
	}

	vdrefptr<IVDXVideoSource> pVS;
	vdwithinputplugin(mContext) {
		mpXObject->GetVideoSource(0, ~pVS);
	}
	if (pVS)
		videoSrc = new VDVideoSourcePlugin(pVS, &mContext);

	vdrefptr<IVDXAudioSource> pAS;
	vdwithinputplugin(mContext) {
		mpXObject->GetAudioSource(0, ~pAS);
	}

	try {
		if (pAS)
			audioSrc = new VDAudioSourcePlugin(pAS, &mContext);
	} catch(const MyError&) {
#pragma vdpragma_TODO("try/catch needs to be removed in 1.7.X")
	}
}

bool VDInputFilePlugin::Append(const wchar_t *szFile) {
	bool appended;
	
	vdwithinputplugin(mContext) {
		appended = mpXObject->Append(szFile);
	}

	return appended;
}

void VDInputFilePlugin::setOptions(InputFileOptions *opts) {
	mpXOptions = static_cast<VDInputFileOptionsPlugin *>(opts)->GetXObject();
}

InputFileOptions *VDInputFilePlugin::promptForOptions(HWND hwnd) {
	vdrefptr<IVDXInputOptions> opts;

	vdwithinputplugin(mContext) {
		mpXObject->PromptForOptions((VDXHWND)hwnd, ~opts);
	}

	if (!opts)
		return NULL;

	return new VDInputFileOptionsPlugin(opts, mContext);
}

InputFileOptions *VDInputFilePlugin::createOptions(const void *buf, uint32 len) {
	vdrefptr<IVDXInputOptions> opts;

	vdwithinputplugin(mContext) {
		mpXObject->CreateOptions(buf, len, ~opts);
	}

	if (!opts)
		return NULL;

	return new VDInputFileOptionsPlugin(opts, mContext);
}

void VDInputFilePlugin::InfoDialog(HWND hwndParent) {
	vdwithinputplugin(mContext) {
		mpXObject->DisplayInfo((VDXHWND)hwndParent);
	}
}

void VDInputFilePlugin::GetTextInfo(tFileTextInfo& info) {
	info.clear();
}

bool VDInputFilePlugin::isOptimizedForRealtime() {
	return false;
}

bool VDInputFilePlugin::isStreaming() {
	return false;
}

///////////////////////////////////////////////////////////////////////////////

class VDInputDriverPlugin : public vdrefcounted<IVDInputDriver> {
public:
	VDInputDriverPlugin(VDPluginDescription *pDesc);
	~VDInputDriverPlugin();

	int				GetDefaultPriority();
	const wchar_t *	GetSignatureName();
	uint32			GetFlags();
	const wchar_t *	GetFilenamePattern();
	bool			DetectByFilename(const wchar_t *pszFilename);
	int				DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize);
	InputFile *		CreateInputFile(uint32 flags);

protected:
	void			LoadPlugin();
	void			UnloadPlugin();

	vdrefptr<IVDXInputFileDriver> mpXObject;
	VDPluginDescription	*mpPluginDesc;
	const VDPluginInfo				*mpPluginInfo;
	const VDInputDriverDefinition	*mpDef;
	const VDInputDriverDefinition	*mpShadowedDef;
	VDInputDriverContextImpl		mContext;

	VDStringW	mFilenamePattern;
};

VDInputDriverPlugin::VDInputDriverPlugin(VDPluginDescription *pDesc)
	: mpPluginDesc(pDesc)
	, mpPluginInfo(NULL)
	, mpDef(NULL)
	, mpShadowedDef(static_cast<const VDInputDriverDefinition *>(mpPluginDesc->mpShadowedInfo->mpTypeSpecificInfo))
	, mContext(mpPluginDesc)
{
	mContext.mAPIVersion = kVDPlugin_InputDriverAPIVersion;

	if (mpShadowedDef->mpFilenamePattern) {
		mFilenamePattern = mpShadowedDef->mpFilenamePattern;

		VDStringW::size_type pos = 0;

		// convert embedded pipes into nulls
		while(VDStringW::npos != (pos = mFilenamePattern.find('|', pos)))
			mFilenamePattern[pos++] = 0;

		// Add a null on the end. Actually, add two, just in case. We need ONE in case someone
		// forgot the filename pattern, a SECOND to end the filter, and a THIRD to end the list.
		// We get one from c_str().
		mFilenamePattern += (wchar_t)0;
		mFilenamePattern += (wchar_t)0;
	}
}

VDInputDriverPlugin::~VDInputDriverPlugin() {
	UnloadPlugin();
}

int VDInputDriverPlugin::GetDefaultPriority() {
	return mpShadowedDef->mPriority;
}

const wchar_t *VDInputDriverPlugin::GetSignatureName() {
	return mpShadowedDef->mpDriverTagName;
}

uint32 VDInputDriverPlugin::GetFlags() {
	uint32 xflags = mpShadowedDef->mFlags;
	uint32 flags = 0;

	if (xflags & VDInputDriverDefinition::kFlagSupportsVideo)
		flags |= kF_Video;

	if (xflags & VDInputDriverDefinition::kFlagSupportsAudio)
		flags |= kF_Audio;

	return flags;
}

const wchar_t *VDInputDriverPlugin::GetFilenamePattern() {
	return mFilenamePattern.c_str();
}

bool VDInputDriverPlugin::DetectByFilename(const wchar_t *pszFilename) {
	const wchar_t *sig = mpShadowedDef->mpFilenameDetectPattern;

	if (!sig)
		return false;

	pszFilename = VDFileSplitPath(pszFilename);

	while(const wchar_t *t = wcschr(sig, L'|')) {
		VDStringW temp(sig, t);

		if (VDFileWildMatch(temp.c_str(), pszFilename))
			return true;

		sig = t+1;
	}

	return VDFileWildMatch(sig, pszFilename);
}

int VDInputDriverPlugin::DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
	const uint8 *sig = (const uint8 *)mpShadowedDef->mpSignature;

	if (sig) {
		if (nHeaderSize < (mpShadowedDef->mSignatureLength >> 1))
			return -1;

		const uint8 *data = (const uint8 *)pHeader;
		for(uint32 i=0; i<mpShadowedDef->mSignatureLength; i+=2) {
			uint8 byte = sig[0];
			uint8 mask = sig[1];

			if ((*data ^ byte) & mask)
				return -1;

			sig += 2;
			++data;
		}
	}

	if (!(mpShadowedDef->mFlags & VDInputDriverDefinition::kFlagCustomSignature))
		return 1;

	LoadPlugin();
	int retval;
	vdwithinputplugin(mContext) {
		retval = mpXObject->DetectBySignature(pHeader, nHeaderSize, pFooter, nFooterSize, nFileSize);
	}
	UnloadPlugin();

	return retval;
}

InputFile *VDInputDriverPlugin::CreateInputFile(uint32 flags) {
	vdrefptr<IVDXInputFile> ifile;

	LoadPlugin();

	vdwithinputplugin(mContext) {
		mpXObject->CreateInputFile(flags, ~ifile);
	}

	InputFile *p = NULL;

	if (ifile)
		p = new VDInputFilePlugin(ifile, mpPluginDesc, &mContext);

	UnloadPlugin();

	return p;
}

void VDInputDriverPlugin::LoadPlugin() {
	if (!mpPluginInfo) {
		mpPluginInfo = VDLockPlugin(mpPluginDesc);
		mpDef = static_cast<const VDInputDriverDefinition *>(mpPluginInfo->mpTypeSpecificInfo);
		vdwithinputplugin(mContext) {
			mpDef->mpCreate(&mContext, ~mpXObject);
		}
	}
}

void VDInputDriverPlugin::UnloadPlugin() {
	mpXObject.clear();

	if (mpPluginInfo) {
		VDUnlockPlugin(mpPluginDesc);
		mpDef = NULL;
		mpPluginInfo = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

extern IVDInputDriver *VDCreateInputDriverPlugin(VDPluginDescription *desc) {
	return new VDInputDriverPlugin(desc);
}
