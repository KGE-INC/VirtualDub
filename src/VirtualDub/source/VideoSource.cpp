//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#include <windows.h>
#include <vfw.h>
#include <vector>

#include "VideoSource.h"
#include "VBitmap.h"
#include "AVIStripeSystem.h"
#include "ProgressDialog.h"
#include "MJPEGDecoder.h"
#include "crash.h"

#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/text.h>
#include <vd2/system/log.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/Dita/resources.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/bitmap.h>
#include "misc.h"
#include "oshelper.h"
#include "helpfile.h"
#include "resource.h"

#include <vd2/Meia/MPEGIDCT.h>

#if defined(_M_AMD64)
	#define VDPROT_PTR	"%p"
#else
	#define VDPROT_PTR	"%08x"
#endif

///////////////////////////

extern const char *LookupVideoCodec(FOURCC);

extern HINSTANCE g_hInst;
extern HWND g_hWnd;

///////////////////////////

namespace {
	enum { kVDST_VideoSource = 3 };

	enum {
		kVDM_ResumeFromConceal,
		kVDM_DecodingError,
		kVDM_FrameTooShort,
		kVDM_CodecMMXError,
		kVDM_FixingHugeVideoFormat,
		kVDM_CodecRenamingDetected,
		kVDM_CodecAcceptsBS
	};
}

namespace {
	enum { kVDST_VideoSequenceCompressor = 10 };

	enum {
		kVDM_CodecModifiesInput
	};
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDecompressorMJPEG : public IVDVideoDecompressor {
public:
	VDVideoDecompressorMJPEG();
	~VDVideoDecompressorMJPEG();

	void Init(int w, int h);

	bool QueryTargetFormat(int format);
	bool QueryTargetFormat(const void *format);
	bool SetTargetFormat(int format);
	bool SetTargetFormat(const void *format);
	int GetTargetFormat() { return mFormat; }
	int GetTargetFormatVariant() { return 0; }
	void Start();
	void Stop();
	void DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll);
	const void *GetRawCodecHandlePtr();
	const wchar_t *GetName();

protected:
	int		mWidth, mHeight;
	int		mFormat;

	vdautoptr<IMJPEGDecoder>	mpDecoder;
};

VDVideoDecompressorMJPEG::VDVideoDecompressorMJPEG()
	: mFormat(0)
{
}

VDVideoDecompressorMJPEG::~VDVideoDecompressorMJPEG() {
}

void VDVideoDecompressorMJPEG::Init(int w, int h) {
	mWidth = w;
	mHeight = h;

	mpDecoder = CreateMJPEGDecoder(w, h);
}

bool VDVideoDecompressorMJPEG::QueryTargetFormat(int format) {
	return format == nsVDPixmap::kPixFormat_XRGB1555
		|| format == nsVDPixmap::kPixFormat_XRGB8888
		|| format == nsVDPixmap::kPixFormat_YUV422_UYVY
		|| format == nsVDPixmap::kPixFormat_YUV422_YUYV;
}

bool VDVideoDecompressorMJPEG::QueryTargetFormat(const void *format) {
	const BITMAPINFOHEADER& hdr = *(const BITMAPINFOHEADER *)format;

	if (hdr.biWidth != mWidth || hdr.biHeight != mHeight)
		return false;

	int pxformat = VDBitmapFormatToPixmapFormat(hdr);

	return QueryTargetFormat(pxformat);
}

bool VDVideoDecompressorMJPEG::SetTargetFormat(int format) {
	if (!format)
		format = nsVDPixmap::kPixFormat_XRGB8888;

	if (QueryTargetFormat(format)) {
		mFormat = format;
		return true;
	}

	return false;
}

bool VDVideoDecompressorMJPEG::SetTargetFormat(const void *format) {
	const BITMAPINFOHEADER& hdr = *(const BITMAPINFOHEADER *)format;

	if (hdr.biWidth != mWidth || hdr.biHeight != mHeight)
		return false;

	int pxformat = VDBitmapFormatToPixmapFormat(hdr);

	if (QueryTargetFormat(pxformat)) {
		mFormat = pxformat;
		return true;
	}

	return false;
}

void VDVideoDecompressorMJPEG::Start() {
	if (!mFormat)
		throw MyError("Cannot find compatible target format for video decompression.");
}

void VDVideoDecompressorMJPEG::Stop() {
}

void VDVideoDecompressorMJPEG::DecompressFrame(void *dst, const void *src, uint32 srcSize, bool keyframe, bool preroll) {
	switch(mFormat) {
	case nsVDPixmap::kPixFormat_XRGB1555:
		mpDecoder->decodeFrameRGB15((unsigned long *)dst, (unsigned char *)src, srcSize);
		break;
	case nsVDPixmap::kPixFormat_XRGB8888:
		mpDecoder->decodeFrameRGB32((unsigned long *)dst, (unsigned char *)src, srcSize);
		break;
	case nsVDPixmap::kPixFormat_YUV422_UYVY:
		mpDecoder->decodeFrameUYVY((unsigned long *)dst, (unsigned char *)src, srcSize);
		break;
	case nsVDPixmap::kPixFormat_YUV422_YUYV:
		mpDecoder->decodeFrameYUY2((unsigned long *)dst, (unsigned char *)src, srcSize);
		break;
	default:
		throw MyError("Cannot find compatible target format for video decompression.");
	}
}

const void *VDVideoDecompressorMJPEG::GetRawCodecHandlePtr() {
	return NULL;
}

const wchar_t *VDVideoDecompressorMJPEG::GetName() {
	return L"Internal Motion JPEG decoder";
}

///////////////////////////////////////////////////////////////////////////

VideoSource::VideoSource() {
	lpvBuffer = NULL;
	hBufferObject = NULL;
}

VideoSource::~VideoSource() {
	FreeFrameBuffer();
}

void *VideoSource::AllocFrameBuffer(long size) {
	hBufferObject = CreateFileMapping(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			size,
			NULL);

	if (!hBufferObject) return NULL;

	lBufferOffset = 0;

	lpvBuffer = MapViewOfFile(hBufferObject, FILE_MAP_ALL_ACCESS, 0, lBufferOffset, size);
	mFrameBufferSize = size;

	if (!lpvBuffer) {
		CloseHandle(hBufferObject);
		hBufferObject = NULL;
	}

	return lpvBuffer;
}

void VideoSource::FreeFrameBuffer() {
	if (hBufferObject) {
		if (lpvBuffer)
			UnmapViewOfFile(lpvBuffer);
		CloseHandle(hBufferObject);
	} else
		freemem(lpvBuffer);

	lpvBuffer = NULL;
	hBufferObject = NULL;
}

bool VideoSource::setTargetFormat(int format) {
	return setTargetFormatVariant(format, 0);
}

bool VideoSource::setTargetFormatVariant(int format, int variant) {
	using namespace nsVDPixmap;

	if (!format)
		format = kPixFormat_XRGB8888;

	const BITMAPINFOHEADER *bih = getImageFormat();
	const uint32 w = bih->biWidth;
	const uint32 h = bih->biHeight;
	VDPixmapLayout layout;

	VDMakeBitmapCompatiblePixmapLayout(layout, w, h, format, variant);

	mTargetFormat = VDPixmapFromLayout(layout, lpvBuffer);
	mTargetFormatVariant = variant;

	if(format == nsVDPixmap::kPixFormat_Pal8) {
		mpTargetFormatHeader.assign(getImageFormat(), sizeof(BITMAPINFOHEADER));
		mpTargetFormatHeader->biSize			= sizeof(BITMAPINFOHEADER);
		mpTargetFormatHeader->biPlanes			= 1;
		mpTargetFormatHeader->biBitCount		= 8;
		mpTargetFormatHeader->biCompression		= BI_RGB;
		mpTargetFormatHeader->biSizeImage		= ((w+3)&~3)*h;

		mTargetFormat.palette = mPalette;
	} else {
		const vdstructex<BITMAPINFOHEADER> src(bih, getFormatLen());
		if (!VDMakeBitmapFormatFromPixmapFormat(mpTargetFormatHeader, src, format, variant))
			return false;
	}

	invalidateFrameBuffer();

	return true;
}

bool VideoSource::setDecompressedFormat(int depth) {
	switch(depth) {
	case 8:		return setTargetFormat(nsVDPixmap::kPixFormat_Pal8);
	case 16:	return setTargetFormat(nsVDPixmap::kPixFormat_XRGB1555);
	case 24:	return setTargetFormat(nsVDPixmap::kPixFormat_RGB888);
	case 32:	return setTargetFormat(nsVDPixmap::kPixFormat_XRGB8888);
	}

	return false;
}

bool VideoSource::setDecompressedFormat(const BITMAPINFOHEADER *pbih) {
	const BITMAPINFOHEADER *bih = getImageFormat();

	if (pbih->biWidth == bih->biWidth && pbih->biHeight == bih->biHeight) {
		int variant;

		int format = VDBitmapFormatToPixmapFormat(*pbih, variant);

		if (format && variant <= 1)
			return setTargetFormat(format);
	}

	return false;
}

void VideoSource::streamBegin(bool, bool) {
	streamRestart();
}

void VideoSource::streamRestart() {
	stream_current_frame	= -1;
}

void VideoSource::streamSetDesiredFrame(VDPosition frame_num) {
	VDPosition key;

	key = isKey(frame_num) ? frame_num : prevKey(frame_num);
	if (key<0)
		key = mSampleFirst;

	stream_desired_frame	= frame_num;

	if (stream_current_frame<key || stream_current_frame>frame_num)
		stream_current_frame	= key-1;

}

VDPosition VideoSource::streamGetNextRequiredFrame(bool& is_preroll) {
	if (stream_current_frame == stream_desired_frame) {
		is_preroll = false;

		return -1;
	}

	// skip zero-byte preroll frames
	do {
		is_preroll = (++stream_current_frame != stream_desired_frame);
	} while(is_preroll && stream_current_frame < stream_desired_frame && getDropType(stream_current_frame) == kDroppable);

	return stream_current_frame;
}

int VideoSource::streamGetRequiredCount(uint32 *totalsize) {

	VDPosition current = stream_current_frame + 1;
	uint32 size = 0;
	uint32 samp;
	uint32 fetch = 0;

	while(current <= stream_desired_frame) {
		uint32 onesize = 0;

		if (AVIERR_OK == read(current, 1, NULL, NULL, &onesize, &samp))
			size += onesize;

		if (onesize)
			++fetch;

		++current;
	}

	if (totalsize)
		*totalsize = size;

	if (!fetch)
		fetch = 1;

	return (int)fetch;
}

void VideoSource::invalidateFrameBuffer() {
}

bool VideoSource::isKey(VDPosition lSample) {
	if (lSample<mSampleFirst || lSample>=mSampleLast)
		return true;

	return _isKey(lSample);
}

bool VideoSource::_isKey(VDPosition lSample) {
	return true;
}

VDPosition VideoSource::nearestKey(VDPosition lSample) {
	return lSample;
}

VDPosition VideoSource::prevKey(VDPosition lSample) {
	return lSample <= mSampleFirst ? -1 : lSample-1;
}

VDPosition VideoSource::nextKey(VDPosition lSample) {
	return lSample+1 >= mSampleFirst ? -1 : lSample+1;
}

bool VideoSource::isKeyframeOnly() {
   return false;
}

bool VideoSource::isType1() {
   return false;
}

///////////////////////////

VideoSourceAVI::VideoSourceAVI(IAVIReadHandler *pAVI, AVIStripeSystem *stripesys, IAVIReadHandler **stripe_files, bool use_internal, int mjpeg_mode, FOURCC fccForceVideo, FOURCC fccForceVideoHandler)
	: mErrorMode(kErrorModeReportAll)
	, mbMMXBrokenCodecDetected(false)
	, mbConcealingErrors(false)
	, mbDecodeStarted(false)
	, mbDecodeRealTime(false)
{
	pAVIFile	= pAVI;
	pAVIStream	= NULL;
	lpvBuffer	= NULL;
	key_flags	= NULL;
	mjpeg_reorder_buffer = NULL;
	mjpeg_reorder_buffer_size = 0;
	mjpeg_splits = NULL;
	mjpeg_last = -1;
	this->fccForceVideo = fccForceVideo;
	this->fccForceVideoHandler = fccForceVideoHandler;
	hbmLame = NULL;
	fUseGDI = false;
	bDirectDecompress = false;
	bInvertFrames = false;
	lLastFrame = -1;

	// striping...

	stripe_streams	= NULL;
	stripe_index	= NULL;
	this->stripesys = stripesys;
	this->stripe_files = stripe_files;
	this->use_internal = use_internal;
	this->mjpeg_mode	= mjpeg_mode;

	try {
		_construct();
	} catch(const MyError&) {
		_destruct();
		throw;
	}
}

void VideoSourceAVI::_destruct() {
	delete stripe_index;

	if (stripe_streams) {
		int i;

		for(i=0; i<stripe_count; i++)
			if (stripe_streams[i])
				delete stripe_streams[i];

		delete stripe_streams;
	}

	mpDecompressor = NULL;

	if (pAVIStream) delete pAVIStream;

	freemem(mjpeg_reorder_buffer);
	mjpeg_reorder_buffer = NULL;
	delete[] mjpeg_splits;
	mjpeg_splits = NULL;

	delete[] key_flags; key_flags = NULL;

	if (hbmLame) {
		DeleteObject(hbmLame);
		hbmLame = NULL;
	}
}

VideoSourceAVI::~VideoSourceAVI() {
	_destruct();
}

void VideoSourceAVI::_construct() {
	LONG format_len;
	BITMAPINFOHEADER *bmih;
	bool is_mjpeg, is_dv, is_dib;

	// Look for a standard vids stream

	bIsType1 = false;
	pAVIStream = pAVIFile->GetStream(streamtypeVIDEO, 0);
	if (!pAVIStream) {
		pAVIStream = pAVIFile->GetStream('svai', 0);

		if (!pAVIStream)
			throw MyError("No video stream found.");

		bIsType1 = true;
	}

	if (pAVIStream->Info(&streamInfo, sizeof streamInfo))
		throw MyError("Error obtaining video stream info.");

	// Force the video type to be 'vids', in case it was type-1 coming in.

	streamInfo.fccType = 'sdiv';

	// ADDITION FOR STRIPED AVI SUPPORT:
	//
	// If this is an index for a stripe system, then the video stream will have
	// 'VDST' as its fccHandler and video compression.  This will probably
	// correspond to the "VDub Frameserver" compressor, but since VirtualDub can
	// connect to its own frameservers more efficiently though the AVIFile
	// interface, it makes sense to open striped files in native mode.
	//
	// For this to work, we must have been sent the striping specs beforehand,
	// or else we won't be able to open the stripes.

	if (streamInfo.fccHandler == 'TSDV') {
		int i;

		if (!stripesys)
			throw MyError("AVI file is striped - must be opened with stripe definition file.");

		// allocate memory for stripe stream table

		stripe_count = stripesys->getStripeCount();

		if (!(stripe_streams = new IAVIReadStream *[stripe_count]))
			throw MyMemoryError();

		for(i=0; i<stripe_count; i++)
			stripe_streams[i] = NULL;

		// attempt to open a video stream for every stripe that has one

		format_stream = NULL;

		for(i=0; i<stripe_count; i++) {
			if (stripesys->getStripeInfo(i)->isVideo()) {
				stripe_streams[i] = stripe_files[i]->GetStream(streamtypeVIDEO, 0);
				if (!stripe_streams[i])
					throw MyError("Striping: cannot open video stream for stripe #%d", i+1);

				if (!format_stream) format_stream = stripe_streams[i];
			}
		}

		if (!format_stream)
			throw MyError("Striping: No video stripes found!");

		// Reread the streamInfo structure from first video stripe,
		// because ours is fake.

		if (format_stream->Info(&streamInfo, sizeof streamInfo))
			throw MyError("Error obtaining video stream info from first video stripe.");

		// Initialize the index.

		if (!(stripe_index = new AVIStripeIndexLookup(pAVIStream)))
			throw MyMemoryError();
		
	} else {
		if (stripesys)
			throw MyError("This is not a striped AVI file.");

		format_stream = pAVIStream;
	}

	// Read video format.  If we're striping, the index stripe has a fake
	// format, so we have to grab the format from a video stripe.  If it's a
	// type-1 DV, we're going to have to fake it.

	if (bIsType1) {
		format_len = sizeof(BITMAPINFOHEADER);

		if (!(bmih = (BITMAPINFOHEADER *)allocFormat(format_len))) throw MyMemoryError();

		bmih->biSize			= sizeof(BITMAPINFOHEADER);
		bmih->biWidth			= 720;

		if (streamInfo.dwRate > streamInfo.dwScale*26i64)
			bmih->biHeight			= 480;
		else
			bmih->biHeight			= 576;

		bmih->biPlanes			= 1;
		bmih->biBitCount		= 24;
		bmih->biCompression		= 'dsvd';
		bmih->biSizeImage		= streamInfo.dwSuggestedBufferSize;
		bmih->biXPelsPerMeter	= 0;
		bmih->biYPelsPerMeter	= 0;
		bmih->biClrUsed			= 0;
		bmih->biClrImportant	= 0;
	} else {
		format_stream->FormatSize(0, &format_len);

		std::vector<char> format(format_len);

		if (format_stream->ReadFormat(0, &format.front(), &format_len))
			throw MyError("Error obtaining video stream format.");

		// check for a very large BITMAPINFOHEADER -- structs as large as 153K
		// can be written out by some ePSXe video plugins

		const BITMAPINFOHEADER *pFormat = (const BITMAPINFOHEADER *)&format.front();

		if (format.size() >= 16384 && format.size() > pFormat->biSize) {
			int badsize = format.size();
			int realsize = pFormat->biSize;

			if (realsize >= sizeof(BITMAPINFOHEADER)) {
				realsize = VDGetSizeOfBitmapHeaderW32(pFormat);

				if (realsize < format.size()) {
					VDLogAppMessage(kVDLogWarning, kVDST_VideoSource, kVDM_FixingHugeVideoFormat, 2, &badsize, &realsize);

					format.resize(realsize);
					format_len = realsize;
				}
			}
		}

		// copy format to official video stream format

		if (!(bmih = (BITMAPINFOHEADER *)allocFormat(format_len)))
			throw MyMemoryError();

		memcpy(getFormat(), pFormat, format.size());
	}

	mpTargetFormatHeader.resize(format_len);

	// initialize pixmap palette
	int palEnts = 0;

	if (bmih->biBitCount <= 8) {
		palEnts = bmih->biClrUsed;

		if (!palEnts)
			palEnts = 1 << bmih->biBitCount;
	}

	if (bmih->biClrUsed > 0) {
		memset(mPalette, 0, sizeof mPalette);
		memcpy(mPalette, (const uint32 *)((const char *)bmih + bmih->biSize), std::min<size_t>(256, palEnts) * 4);
	}


	// Some Dazzle drivers apparently do not set biSizeImage correctly.  Also,
	// zero is a valid value for BI_RGB, but it's annoying!

	if (bmih->biCompression == BI_RGB || bmih->biCompression == BI_BITFIELDS) {
		// Check for an inverted DIB.  If so, silently flip it around.

		if ((long)bmih->biHeight < 0) {
			bmih->biHeight = abs((long)bmih->biHeight);
			bInvertFrames = true;
		}

		if (bmih->biPlanes == 1) {
			long nPitch = ((bmih->biWidth * bmih->biBitCount + 31) >> 5) * 4 * bmih->biHeight;

			bmih->biSizeImage = nPitch;
		}
	}

	// Force the video format if necessary

	if (fccForceVideo)
		getImageFormat()->biCompression = fccForceVideo;

	if (fccForceVideoHandler)
		streamInfo.fccHandler = fccForceVideoHandler;

	is_mjpeg = isEqualFOURCC(bmih->biCompression, 'GPJM')
			|| isEqualFOURCC(fccForceVideo, 'GPJM')
			|| isEqualFOURCC(bmih->biCompression, '1bmd')
			|| isEqualFOURCC(fccForceVideo, '1bmd');

	is_dv = isEqualFOURCC(bmih->biCompression, 'dsvd')
		 || isEqualFOURCC(fccForceVideo, 'dsvd');

	// Check if we can handle the format directly; if so, convert bitmap format to Kasumi format
	bool inverted = false;

	mSourceLayout.data		= 0;
	mSourceLayout.data2		= 0;
	mSourceLayout.data3		= 0;
	mSourceLayout.pitch		= 0;
	mSourceLayout.pitch2	= 0;
	mSourceLayout.pitch3	= 0;
	mSourceLayout.palette	= 0;
	mSourceLayout.format	= 0;
	mSourceLayout.w			= bmih->biWidth;
	mSourceLayout.h			= abs(bmih->biHeight);
	mSourceVariant			= 1;

	switch(bmih->biCompression) {
	case BI_RGB:
		inverted = bmih->biHeight >= 0;
		switch(bmih->biBitCount) {
		case 8:
			mSourceLayout.format = nsVDPixmap::kPixFormat_Pal8;
			break;
		case 16:
			mSourceLayout.format = nsVDPixmap::kPixFormat_XRGB1555;
			break;
		case 24:
			mSourceLayout.format = nsVDPixmap::kPixFormat_RGB888;
			break;
		case 32:
			mSourceLayout.format = nsVDPixmap::kPixFormat_XRGB8888;
			break;
		}
		break;
	case BI_BITFIELDS:
		if (getFormatLen() >= sizeof(BITMAPINFOHEADER) + sizeof(DWORD)*3) {
			BITMAPV4HEADER& hdr4 = *(BITMAPV4HEADER *)bmih;		// Only the rgb masks are guaranteed to be valid
			const DWORD red		= hdr4.bV4RedMask;
			const DWORD grn		= hdr4.bV4GreenMask;
			const DWORD blu		= hdr4.bV4BlueMask;
			const DWORD count	= bmih->biBitCount;

			if (red == 0x00007c00 && grn == 0x000003e0 && blu == 0x0000001f && count == 16)
				mSourceLayout.format = nsVDPixmap::kPixFormat_XRGB1555;
			else if (red == 0x0000f800 && grn == 0x000007e0 && blu == 0x0000001f && count == 16)
				mSourceLayout.format = nsVDPixmap::kPixFormat_RGB565;
			else if (red == 0x00ff0000 && grn == 0x0000ff00 && blu == 0x000000ff && count == 24)
				mSourceLayout.format = nsVDPixmap::kPixFormat_RGB888;
			else if (red == 0x00ff0000 && grn == 0x0000ff00 && blu == 0x000000ff && count == 32)
				mSourceLayout.format = nsVDPixmap::kPixFormat_XRGB8888;
		}
		break;
	default:
		extern bool VDPreferencesIsDirectYCbCrInputEnabled();
		if (VDPreferencesIsDirectYCbCrInputEnabled()) {
			switch(bmih->biCompression) {
			case 'VYUY':		// YUYV
			case '2YUY':		// YUY2
				mSourceLayout.format = nsVDPixmap::kPixFormat_YUV422_YUYV;
				break;
			case 'YVYU':		// UYVY
				mSourceLayout.format = nsVDPixmap::kPixFormat_YUV422_UYVY;
				break;
			case '21VY':		// YV12
				mSourceLayout.format = nsVDPixmap::kPixFormat_YUV420_Planar;
				break;
			case 'VUYI':		// IYUV
				mSourceLayout.format = nsVDPixmap::kPixFormat_YUV420_Planar;
				mSourceVariant = 3;
				break;
			case '024I':		// I420
				mSourceLayout.format = nsVDPixmap::kPixFormat_YUV420_Planar;
				mSourceVariant = 2;
				break;
			case '61VY':		// YV16
				mSourceLayout.format = nsVDPixmap::kPixFormat_YUV422_Planar;
				break;
			case '9UVY':		// YVU9
				mSourceLayout.format = nsVDPixmap::kPixFormat_YUV410_Planar;
				break;
			case '  8Y':		// Y8
			case '008Y':		// Y800
				mSourceLayout.format = nsVDPixmap::kPixFormat_Y8;
				break;
			}
		}
		break;
	}

	is_dib = (mSourceLayout.format != 0);

	if (mSourceLayout.format) {
		VDMakeBitmapCompatiblePixmapLayout(mSourceLayout, mSourceLayout.w, mSourceLayout.h, mSourceLayout.format, mSourceVariant);

		vdstructex<BITMAPINFOHEADER> format;
		VDMakeBitmapFormatFromPixmapFormat(format, vdstructex<BITMAPINFOHEADER>(getImageFormat(), getFormatLen()), mSourceLayout.format, mSourceVariant);

		mSourceFrameSize = format->biSizeImage;
	}

	// init target format to something sane
	mTargetFormat = VDPixmapFromLayout(mSourceLayout, lpvBuffer);
	mpTargetFormatHeader.assign(getImageFormat(), sizeof(BITMAPINFOHEADER));
	mpTargetFormatHeader->biSize			= sizeof(BITMAPINFOHEADER);
	mpTargetFormatHeader->biPlanes			= 1;
	mpTargetFormatHeader->biBitCount		= 32;
	mpTargetFormatHeader->biCompression		= BI_RGB;
	mpTargetFormatHeader->biSizeImage		= mpTargetFormatHeader->biWidth*mpTargetFormatHeader->biHeight*4;

	// If this is MJPEG, check to see if we should modify the output format and/or stream info

	mSampleFirst = pAVIStream->Start();
	mSampleLast = pAVIStream->End();

	if (is_mjpeg) {
		BITMAPINFOHEADER *pbih = getImageFormat();

		if (mjpeg_mode && mjpeg_mode != IFMODE_SWAP && pbih->biHeight > 288) {
			pbih->biHeight /= 2;

			if (mjpeg_mode == IFMODE_SPLIT1 || mjpeg_mode == IFMODE_SPLIT2) {
				if (streamInfo.dwRate >= 0x7FFFFFFF)
					streamInfo.dwScale >>= 1;
				else
					streamInfo.dwRate *= 2;

				streamInfo.dwLength *= 2;
				mSampleLast = mSampleLast*2 - mSampleFirst;
			}
		}

		if (mjpeg_mode) {
			if (!(mjpeg_splits = new long[(size_t)(mSampleLast - mSampleFirst)]))
				throw MyMemoryError();

			for(int i=0; i<mSampleLast-mSampleFirst; i++)
				mjpeg_splits[i] = -1;
		}
	} else
		mjpeg_mode = 0;

	// allocate framebuffer

	if (!AllocFrameBuffer(bmih->biWidth * 4 * bmih->biHeight + 4))
		throw MyMemoryError();

	// get a decompressor
	//
	// 'DIB ' is the official value for uncompressed AVIs, but some stupid
	// programs also use (null) and 'RAW '
	//
	// NOTE: Don't handle RLE4/RLE8 here.  RLE is slightly different in AVI files!

	if (bmih->biCompression == BI_BITFIELDS
		|| (bmih->biCompression == BI_RGB && bmih->biBitCount<16 && bmih->biBitCount != 8)) {

		// Ugh.  It's one of them weirdo formats.  Let GDI handle it!

		fUseGDI = true;

	} else if (!is_dib) {
		FOURCC fccOriginalCodec = bmih->biCompression;

		// If it's a hacked MPEG-4 driver, try all of the known hacks. #*$&@#(&$)(#$
		// They're all the same driver, so they're mutually compatible.

		bool bFailed = false;

		if (use_internal) {
			bFailed = true;
		} else {
			mpDecompressor = VDFindVideoDecompressor(streamInfo.fccHandler, bmih);
			bFailed = !mpDecompressor;
		}

		if (bFailed) {
			// Is it MJPEG or DV?  Maybe we can do it ourselves.

			const BITMAPINFOHEADER *bihSrc = getImageFormat();
			const int w = bihSrc->biWidth;
			const int h = bihSrc->biHeight;

			// AMD64 currently does not have a working MJPEG decoder. Should fix this.
#ifndef _M_AMD64
			if (is_mjpeg) {
				vdautoptr<VDVideoDecompressorMJPEG> pDecoder(new_nothrow VDVideoDecompressorMJPEG);
				pDecoder->Init(w, h);

				mpDecompressor = pDecoder.release();
			} else
#endif
			if (is_dv && w==720 && (h == 480 || h == 576)) {
				mpDecompressor = VDCreateVideoDecompressorDV(w, h);
			} else {

				// if we were asked to use an internal decoder and failed, try external decoders
				// now
				if (use_internal) {
					mpDecompressor = VDFindVideoDecompressor(streamInfo.fccHandler, bmih);
					bFailed = !mpDecompressor;
				}

				if (bFailed) {
					const char *s = LookupVideoCodec(fccOriginalCodec);

					throw MyError("Couldn't locate decompressor for format '%c%c%c%c' (%s)\n"
									"\n"
									"VirtualDub requires a Video for Windows (VFW) compatible codec to decompress "
									"video. DirectShow codecs, such as those used by Windows Media Player, are not "
									"suitable."
								,(fccOriginalCodec    ) & 0xff
								,(fccOriginalCodec>> 8) & 0xff
								,(fccOriginalCodec>>16) & 0xff
								,(fccOriginalCodec>>24) & 0xff
								,s ? s : "unknown");
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////

void VideoSourceAVI::Reinit() {
	VDPosition nOldFrames, nNewFrames;

	nOldFrames = mSampleLast - mSampleFirst;
	nNewFrames = pAVIStream->End() - pAVIStream->Start();

	if (mjpeg_mode==IFMODE_SPLIT1 || mjpeg_mode==IFMODE_SPLIT2) {
		nOldFrames >>= 1;
	}

	if (nOldFrames != nNewFrames && mjpeg_mode) {
		// We have to resize the mjpeg_splits array.

		long *pNewSplits = new long[nNewFrames];

		if (!pNewSplits)
			throw MyMemoryError();

		VDPosition i;

		memcpy(pNewSplits, mjpeg_splits, sizeof(long)*nOldFrames);

		for(i=nOldFrames; i<nNewFrames; i++)
			pNewSplits[i] = -1;

		delete[] mjpeg_splits;

		mjpeg_splits = pNewSplits;
	}

	if (pAVIStream->Info(&streamInfo, sizeof streamInfo))
		throw MyError("Error obtaining video stream info.");

	streamInfo.fccType = 'sdiv';


	mSampleFirst = pAVIStream->Start();

	if (mjpeg_mode==IFMODE_SPLIT1 || mjpeg_mode==IFMODE_SPLIT2) {
		if (streamInfo.dwRate >= 0x7FFFFFFF)
			streamInfo.dwScale >>= 1;
		else
			streamInfo.dwRate *= 2;

		mSampleLast = pAVIStream->End() * 2 - mSampleFirst;
	} else
		mSampleLast = pAVIStream->End();

	streamInfo.dwLength		= (DWORD)std::min<VDPosition>(0xFFFFFFFF, mSampleLast - mSampleFirst);
}

void VideoSourceAVI::redoKeyFlags() {
	VDPosition lSample;
	long lMaxFrame=0;
	uint32 lActualBytes, lActualSamples;
	int err;
	void *lpInputBuffer = NULL;
	BOOL fStreamBegun = FALSE;
	int iBytes;
	long *pFrameSums;

	if (!mpDecompressor)
		return;

	iBytes = (mSampleLast+7-mSampleFirst)>>3;

	if (!(key_flags = new char[iBytes]))
		throw MyMemoryError();

	memset(key_flags, 0, iBytes);

	// Find maximum frame

	lSample = mSampleFirst;
	while(lSample < mSampleLast) {
		err = _read(lSample, 1, NULL, 0, &lActualBytes, &lActualSamples);
		if (err == AVIERR_OK)
//			throw MyAVIError("VideoSource", err);

			if (lActualBytes > lMaxFrame) lMaxFrame = lActualBytes;

		++lSample;
	}

	if (!setDecompressedFormat(24))
		if (!setDecompressedFormat(32))
			if (!setDecompressedFormat(16))
				if (!setDecompressedFormat(8))
					throw MyError("Video decompressor is incapable of decompressing to an RGB format.");

	if (!(lpInputBuffer = new char[((lMaxFrame+7)&-8) + lMaxFrame]))
		throw MyMemoryError();

	if (!(pFrameSums = new long[mSampleLast - mSampleFirst])) {
		delete[] lpInputBuffer;
		throw MyMemoryError();
	}

	try {
		ProgressDialog pd(NULL, "AVI Import Filter", "Rekeying video stream", (long)(mSampleLast - mSampleFirst), true);
		pd.setValueFormat("Frame %ld of %ld");

		streamBegin(true, false);
		fStreamBegun = TRUE;

		lSample = mSampleFirst;
		while(lSample < mSampleLast) {
			long lWhiteTotal=0;
			long x, y;
			const long lWidth	= (mpTargetFormatHeader->biWidth * mpTargetFormatHeader->biBitCount + 7)/8;
			const long lModulo	= (4-lWidth)&3;
			const long lHeight	= mpTargetFormatHeader->biHeight;
			unsigned char *ptr;

			_RPT1(0,"Rekeying frame %ld\n", lSample);

			err = _read(lSample, 1, lpInputBuffer, lMaxFrame, &lActualBytes, &lActualSamples);
			if (err != AVIERR_OK)
//				throw MyAVIError("VideoSourceAVI", err);
				goto rekey_error;


			streamGetFrame(lpInputBuffer, lActualBytes, FALSE, lSample);

			ptr = (unsigned char *)lpvBuffer;
			y = lHeight;
			do {
				x = lWidth;
				do {
					lWhiteTotal += (long)*ptr++;
					lWhiteTotal ^= 0xAAAAAAAA;
				} while(--x);

				ptr += lModulo;
			} while(--y);


			pFrameSums[lSample - mSampleFirst] = lWhiteTotal;


//			if (lBlackTotal == lWhiteTotal)
//				key_flags[(lSample - mSampleFirst)>>3] |= 1<<((lSample-mSampleFirst)&7);

rekey_error:
			++lSample;

			pd.advance((long)((lSample - mSampleFirst) >> 1));
			pd.check();
		}

		lSample = mSampleFirst;
		do {
			long lWhiteTotal=0;
			long x, y;
			const long lWidth	= (mpTargetFormatHeader->biWidth * mpTargetFormatHeader->biBitCount + 7)/8;
			const long lModulo	= (4-lWidth)&3;
			const long lHeight	= mpTargetFormatHeader->biHeight;
			unsigned char *ptr;

			_RPT1(0,"Rekeying frame %ld\n", lSample);

			err = _read(lSample, 1, lpInputBuffer, lMaxFrame, &lActualBytes, &lActualSamples);
			if (err != AVIERR_OK)
//				throw MyAVIError("VideoSourceAVI", err);
				goto rekey_error2;

			streamGetFrame(lpInputBuffer, lActualBytes, FALSE, lSample);

			ptr = (unsigned char *)lpvBuffer;
			y = lHeight;
			do {
				x = lWidth;
				do {
					lWhiteTotal += (long)*ptr++;
					lWhiteTotal ^= 0xAAAAAAAA;
				} while(--x);

				ptr += lModulo;
			} while(--y);


			if (lWhiteTotal == pFrameSums[lSample - mSampleFirst])
				key_flags[(lSample - mSampleFirst)>>3] |= 1<<((lSample-mSampleFirst)&7);

rekey_error2:
			if (lSample == mSampleFirst)
				lSample = mSampleLast-1;
			else
				--lSample;

			pd.advance((long)(mSampleLast - ((lSample+mSampleFirst) >> 1)));
			pd.check();
		} while(lSample >= mSampleFirst+1);

		streamEnd();
	} catch(...) {
		if (fStreamBegun) streamEnd();
		delete[] lpInputBuffer;
		delete[] pFrameSums;
		throw;
	}

	delete[] lpInputBuffer;
	delete[] pFrameSums;
}

int VideoSourceAVI::_read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead) {
	IAVIReadStream *pSource = pAVIStream;
	bool phase = (lStart - mSampleFirst)&1;

	if (mjpeg_mode == IFMODE_SPLIT1 || mjpeg_mode == IFMODE_SPLIT2)
		lStart = mSampleFirst + (lStart - mSampleFirst)/2;

	// If we're striping, then we have to lookup the correct stripe for this sample.

	if (stripesys) {
		AVIStripeIndexEntry *asie;
		VDPosition offset;

		if (!(asie = stripe_index->lookup(lStart)))
			return AVIERR_FILEREAD;

		offset = lStart - asie->lSampleFirst;

		if (lCount > asie->lSampleCount-offset)
			lCount = asie->lSampleCount-offset;

		if (!stripe_streams[asie->lStripe])
			return AVIERR_FILEREAD;

		pSource = stripe_streams[asie->lStripe];
		lStart = asie->lStripeSample + offset;
	}

	// MJPEG modification mode?

	if (mjpeg_mode) {
		int res;
		LONG lBytes, lSamples;
		long lOffset, lLength;

		// Did we just read in this sample!?

		if (lStart == mjpeg_last) {
			lBytes = mjpeg_last_size;
			res = AVIERR_OK;
		} else {

			// Read the sample into memory.  If we don't have a lpBuffer *and* already know
			// where the split is, just get the size.

			if (lpBuffer || mjpeg_splits[lStart - mSampleFirst]<0) {

				mjpeg_last = -1;

				if (mjpeg_reorder_buffer_size)
					res = pSource->Read(lStart, 1, mjpeg_reorder_buffer, mjpeg_reorder_buffer_size, &lBytes, &lSamples);

				if (res == AVIERR_BUFFERTOOSMALL || !mjpeg_reorder_buffer_size) {
					void *new_buffer;
					int new_size;

					res = pSource->Read(lStart, 1, NULL, 0, &lBytes, &lSamples);

					if (res == AVIERR_OK) {

						_ASSERT(lBytes != 0);

						new_size = (lBytes + 4095) & -4096;
//						new_size = lBytes;
						new_buffer = reallocmem(mjpeg_reorder_buffer, new_size);

						if (!new_buffer)
							return AVIERR_MEMORY;

						mjpeg_reorder_buffer = new_buffer;
						mjpeg_reorder_buffer_size = new_size;

						res = pSource->Read(lStart, 1, mjpeg_reorder_buffer, mjpeg_reorder_buffer_size, &lBytes, &lSamples);
					}
				}

				if (res == AVIERR_OK) {
					mjpeg_last = lStart;
					mjpeg_last_size = lBytes;
				}
			} else
				res = pSource->Read(lStart, 1, NULL, 0, &lBytes, &lSamples);
		}


		if (res != AVIERR_OK) {
			if (lBytesRead)
				*lBytesRead = 0;
			if (lSamplesRead)
				*lSamplesRead = 0;
			return res;
		} else if (!lBytes) {
			if (lBytesRead)
				*lBytesRead = 0;
			if (lSamplesRead)
				*lSamplesRead = 1;
			return AVIERR_OK;
		}

		// Attempt to find SOI tag in sample

		lOffset = 0;
		lLength = lBytes;

		{
			int i;

			// Do we already know where the split is?

			if (mjpeg_splits[lStart - mSampleFirst]<0) {
				for(i=2; i<lBytes-2; i++)
					if (((unsigned char *)mjpeg_reorder_buffer)[i] == (unsigned char)0xFF
							&& ((unsigned char *)mjpeg_reorder_buffer)[i+1] == (unsigned char)0xD8)
						break;

				mjpeg_splits[lStart - mSampleFirst] = i;
			} else {
				i = mjpeg_splits[lStart - mSampleFirst];
			}

			if (i<lBytes-2) {
				if (mjpeg_mode != IFMODE_SWAP) {
					switch(mjpeg_mode) {
					case IFMODE_SPLIT2:
						phase = !phase;
						break;
					case IFMODE_DISCARD1:
						phase = false;
						break;
					case IFMODE_DISCARD2:
						phase = true;
						break;
					}

					if (phase) {
						lOffset = i;
						lLength = lBytes - i;
					} else {
						lOffset = 0;
						lLength = i;
					}
				} else
					lOffset = i;
			}
		}

		if (lpBuffer) {
			if (lSamplesRead)
				*lSamplesRead = 1;
			if (lBytesRead)
				*lBytesRead = lLength;

			if (cbBuffer < lLength)
				return AVIERR_BUFFERTOOSMALL;

			if (mjpeg_mode == IFMODE_SWAP) {
				char *pp1 = (char *)lpBuffer;
				char *pp2 = (char *)lpBuffer + (lBytes - lOffset);

				memcpy(pp1, (char *)mjpeg_reorder_buffer+lOffset, lBytes - lOffset);
				if (lOffset)
					memcpy(pp2, mjpeg_reorder_buffer, lOffset);

				// patch phase on both MJPEG headers

				if (((short *)pp1)[1]==(short)0xE0FF)
					pp1[10] = 1;

				if (((short *)pp2)[1]==(short)0xE0FF)
					pp2[10] = 2;
				
			} else {

				memcpy(lpBuffer, (char *)mjpeg_reorder_buffer+lOffset, lLength);

				// patch phase on MJPEG header by looking for APP0 tag (xFFE0)

				// FFD8 FFE0 0010 'AVI1' polarity

				if (((short *)lpBuffer)[1]==(short)0xE0FF)
					((char *)lpBuffer)[10] = 0;
			}

			return AVIERR_OK;
		} else {
			if (lSamplesRead)
				*lSamplesRead = 1;
			if (lBytesRead)
				*lBytesRead = lLength;

			return AVIERR_OK;
		}

	} else {
		LONG samplesRead, bytesRead;

		int rv = pSource->Read(lStart, lCount, lpBuffer, cbBuffer, &bytesRead, &samplesRead);

		if (lSamplesRead)
			*lSamplesRead = samplesRead;
		if (lBytesRead)
			*lBytesRead = bytesRead;

		if (lpBuffer && bInvertFrames && !rv && samplesRead) {
			const BITMAPINFOHEADER& hdr = *getImageFormat();
			const long dpr = ((hdr.biWidth * hdr.biBitCount + 31)>>5);

			if (bytesRead >= dpr * 4 * hdr.biHeight) {		// safety check
				const int h2 = hdr.biHeight >> 1;
				long *p0 = (long *)lpBuffer;
				long *p1 = (long *)lpBuffer + dpr * (hdr.biHeight - 1);

				for(int y=0; y<h2; ++y) {
					for(int x=0; x<dpr; ++x) {
						long t = p0[x];
						p0[x] = p1[x];
						p1[x] = t;
					}
					p0 += dpr;
					p1 -= dpr;
				}
			}
		}

		return rv;
	}
}

VDPosition VideoSourceAVI::getRealDisplayFrame(VDPosition display_num) {
	if (display_num >= mSampleLast) {
		display_num = mSampleLast - 1;
		if (display_num < mSampleFirst)
			display_num = mSampleFirst;
		return display_num;
	}

	while(display_num > mSampleFirst && getDropType(display_num) == kDroppable)
		--display_num;

	return display_num;
}

sint64 VideoSourceAVI::getSampleBytePosition(VDPosition pos) {
	IAVIReadStream *pSource = pAVIStream;

	if (mjpeg_mode == IFMODE_SPLIT1 || mjpeg_mode == IFMODE_SPLIT2)
		pos = mSampleFirst + (pos - mSampleFirst)/2;

	// If we're striping, then we have to lookup the correct stripe for this sample.

	if (stripesys) {
		AVIStripeIndexEntry *asie;
		VDPosition offset;

		if (!(asie = stripe_index->lookup(pos)))
			return -1;

		offset = pos - asie->lSampleFirst;

		if (!stripe_streams[asie->lStripe])
			return -1;

		pSource = stripe_streams[asie->lStripe];
		pos = asie->lStripeSample + offset;
	}

	return pSource->getSampleBytePosition(pos);
}

bool VideoSourceAVI::_isKey(VDPosition samp) {
	if ((mjpeg_mode & -2) == IFMODE_SPLIT1)
		samp = mSampleFirst + (samp - mSampleFirst)/2;

	if (key_flags) {
		samp -= mSampleFirst;

		return !!(key_flags[samp>>3] & (1<<(samp&7)));
	} else
		return pAVIStream->IsKeyFrame(samp);
}

VDPosition VideoSourceAVI::nearestKey(VDPosition lSample) {
	if (key_flags) {
		if (lSample < mSampleFirst || lSample >= mSampleLast)
			return -1;

		if (_isKey(lSample)) return lSample;

		return prevKey(lSample);
	}

//	if (lNear == -1)
//		throw MyError("VideoSourceAVI: error getting previous key frame");

	if ((mjpeg_mode & -2) == IFMODE_SPLIT1) {
		return pAVIStream->NearestKeyFrame(mSampleFirst + (lSample-mSampleFirst)/2)*2-mSampleFirst;
	} else {
		return pAVIStream->NearestKeyFrame(lSample);
	}
}

VDPosition VideoSourceAVI::prevKey(VDPosition lSample) {
	if ((mjpeg_mode & -2) == IFMODE_SPLIT1) {
		lSample = mSampleFirst + (lSample - mSampleFirst)/2;

		if (key_flags) {
			if (lSample >= mSampleLast) return -1;

			while(--lSample >= mSampleFirst)
				if (_isKey(lSample)) return lSample*2-mSampleFirst;

			return -1;
		} else
			return pAVIStream->PrevKeyFrame(lSample)*2-mSampleFirst;
	} else {
		if (key_flags) {
			if (lSample >= mSampleLast) return -1;

			while(--lSample >= mSampleFirst)
				if (_isKey(lSample)) return lSample;

			return -1;
		} else
			return pAVIStream->PrevKeyFrame(lSample);
	}
}

VDPosition VideoSourceAVI::nextKey(VDPosition lSample) {
	if ((mjpeg_mode & -2) == IFMODE_SPLIT1) {
		lSample = mSampleFirst + (lSample - mSampleFirst)/2;

		if (key_flags) {
			if (lSample < mSampleFirst) return -1;

			while(++lSample < mSampleLast)
				if (_isKey(lSample)) return lSample*2 - mSampleFirst;

			return -1;
		} else
			return pAVIStream->NextKeyFrame(lSample)*2 - mSampleFirst;

	} else {

		if (key_flags) {
			if (lSample < mSampleFirst) return -1;

			while(++lSample < mSampleLast)
				if (_isKey(lSample)) return lSample;

			return -1;
		} else
			return pAVIStream->NextKeyFrame(lSample);

	}
}

bool VideoSourceAVI::setTargetFormat(int format) {
	using namespace nsVDPixmap;

	streamEnd();

	bDirectDecompress = false;

	if (!format && mSourceLayout.format)
		format = mSourceLayout.format;

	if (format && format == mSourceLayout.format) {
		if (VideoSource::setTargetFormatVariant(format, mSourceVariant)) {
			if (format == kPixFormat_Pal8)
				mTargetFormat.palette = mPalette;
			bDirectDecompress = true;
			return true;
		}

		return false;
	}

	if (fUseGDI) {
		void *pv;
		HDC hdc;

		if (!format)
			format = kPixFormat_XRGB8888;

		if (format != kPixFormat_XRGB1555 && format != kPixFormat_RGB888 && format != kPixFormat_XRGB8888)
			return false;

		if (!VideoSource::setTargetFormat(format))
			return false;

		if (hbmLame)
			DeleteObject(hbmLame);

		hbmLame = NULL;

		if (hdc = CreateCompatibleDC(NULL)) {
			hbmLame = CreateDIBSection(hdc, (BITMAPINFO *)mpTargetFormatHeader.data(), DIB_RGB_COLORS, &pv, hBufferObject, 0);
			DeleteDC(hdc);
		}

		return true;
	} else if (mpDecompressor) {
		// attempt direct decompression
		if (format) {
			vdstructex<BITMAPINFOHEADER> desiredFormat;
			vdstructex<BITMAPINFOHEADER> srcFormat(getImageFormat(), getFormatLen());

			const int variants = VDGetPixmapToBitmapVariants(format);

			for(int variant=1; variant<=variants; ++variant) {
				if (VDMakeBitmapFormatFromPixmapFormat(desiredFormat, srcFormat, format, variant)) {
					if (srcFormat->biCompression == desiredFormat->biCompression
						&& srcFormat->biBitCount == desiredFormat->biBitCount
						&& srcFormat->biSizeImage == desiredFormat->biSizeImage
						&& srcFormat->biWidth == desiredFormat->biWidth
						&& srcFormat->biHeight == desiredFormat->biHeight
						&& srcFormat->biPlanes == desiredFormat->biPlanes) {

						mpTargetFormatHeader = srcFormat;

						VDVERIFY(VideoSource::setTargetFormatVariant(format, variant));

						if (format == kPixFormat_Pal8)
							mTargetFormat.palette = mPalette;

						bDirectDecompress = true;

						invalidateFrameBuffer();
						return true;
					}
				}
			}

			// no variants were an exact match
		}

		if (mpDecompressor->SetTargetFormat(format)) {
			VDVERIFY(VideoSource::setTargetFormatVariant(mpDecompressor->GetTargetFormat(), mpDecompressor->GetTargetFormatVariant()));
			return true;
		}
		return false;
	} else {
		if (format != mSourceLayout.format && !VDPixmapIsBltPossible(format, mSourceLayout.format))
			return false;

		if (!VideoSource::setTargetFormat(format))
			return false;

		return true;
	}
}

bool VideoSourceAVI::setDecompressedFormat(const BITMAPINFOHEADER *pbih) {
	streamEnd();

	if (pbih->biCompression == BI_RGB && pbih->biBitCount > 8)
		return setDecompressedFormat(pbih->biBitCount);

	if (pbih->biCompression == getImageFormat()->biCompression) {
		const BITMAPINFOHEADER *pbihSrc = getImageFormat();
		if (pbih->biBitCount == pbihSrc->biBitCount
			&& pbih->biSizeImage == pbihSrc->biSizeImage
			&& pbih->biWidth == pbihSrc->biWidth
			&& pbih->biHeight == pbihSrc->biHeight
			&& pbih->biPlanes == pbihSrc->biPlanes) {

			mpTargetFormatHeader.assign(pbih, sizeof(BITMAPINFOHEADER));
			if (mSourceLayout.format)
				mTargetFormat = VDPixmapFromLayout(mSourceLayout, lpvBuffer);
			else
				mTargetFormat.format = 0;

			mTargetFormat.palette = mPalette;

			bDirectDecompress = true;

			invalidateFrameBuffer();
			return true;
		}
	}

	mTargetFormat.format = 0;

	if (mpDecompressor && mpDecompressor->SetTargetFormat(pbih)) {
		mpTargetFormatHeader.assign(pbih, sizeof(BITMAPINFOHEADER));

		invalidateFrameBuffer();
		bDirectDecompress = false;
		return true;
	}

	return false;
}

////////////////////////////////////////////////

void VideoSourceAVI::DecompressFrame(const void *pSrc) {
	if (!VDINLINEASSERT(mTargetFormat.format))
		return;

	VDPixmap src(VDPixmapFromLayout(mSourceLayout, (void *)pSrc));
	src.palette = mPalette;

	VDPixmapBlt(mTargetFormat, src);
}

////////////////////////////////////////////////

void VideoSourceAVI::invalidateFrameBuffer() {
	if (lLastFrame != -1 && mpDecompressor)
		mpDecompressor->Stop();

	lLastFrame = -1;
	mbConcealingErrors = false;
}

bool VideoSourceAVI::isFrameBufferValid() {
	return lLastFrame != -1;
}

char VideoSourceAVI::getFrameTypeChar(VDPosition lFrameNum) {
	if (lFrameNum<mSampleFirst || lFrameNum >= mSampleLast)
		return ' ';

	if (_isKey(lFrameNum))
		return 'K';

	uint32 lBytes, lSamples;
	int err = _read(lFrameNum, 1, NULL, 0, &lBytes, &lSamples);

	if (err != AVIERR_OK)
		return ' ';

	return lBytes ? ' ' : 'D';
}

VideoSource::eDropType VideoSourceAVI::getDropType(VDPosition lFrameNum) {
	if (lFrameNum<mSampleFirst || lFrameNum >= mSampleLast)
		return kDroppable;

	if (_isKey(lFrameNum))
		return kIndependent;

	uint32 lBytes, lSamples;
	int err = _read(lFrameNum, 1, NULL, 0, &lBytes, &lSamples);

	if (err != AVIERR_OK)
		return kDependant;

	return lBytes ? kDependant : kDroppable;
}

bool VideoSourceAVI::isDecodable(VDPosition sample_num) {
	if (sample_num<mSampleFirst || sample_num >= mSampleLast)
		return false;

	return (isKey(sample_num) || sample_num == lLastFrame+1);
}

bool VideoSourceAVI::isStreaming() {
	return pAVIStream->isStreaming();
}

bool VideoSourceAVI::isKeyframeOnly() {
   return pAVIStream->isKeyframeOnly();
}

bool VideoSourceAVI::isType1() {
   return bIsType1;
}

void VideoSourceAVI::streamBegin(bool fRealTime, bool bForceReset) {
	if (bForceReset)
		stream_current_frame	= -1;

	if (mbDecodeStarted && fRealTime == mbDecodeRealTime)
		return;

	stream_current_frame	= -1;

	pAVIStream->BeginStreaming(mSampleFirst, mSampleLast, fRealTime ? 1000 : 2000);

	if (mpDecompressor && !bDirectDecompress)
		mpDecompressor->Start();

	mbDecodeStarted = true;
	mbDecodeRealTime = fRealTime;
}

const void *VideoSourceAVI::streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition frame_num) {
	if (isKey(frame_num)) {
		if (mbConcealingErrors) {
			const unsigned frame = (unsigned)frame_num;
			VDLogAppMessage(kVDLogWarning, kVDST_VideoSource, kVDM_ResumeFromConceal, 1, &frame);
		}
		mbConcealingErrors = false;
	}

	if (!data_len || mbConcealingErrors) return getFrameBuffer();

	if (bDirectDecompress) {
		// avoid passing runt uncompressed frames
		uint32 to_copy = data_len;
		if (mSourceLayout.format) {
			if (data_len < mSourceFrameSize) {
				if (mErrorMode != kErrorModeReportAll) {
					const unsigned actual = data_len;
					const unsigned expected = mSourceFrameSize;
					const unsigned frame = (unsigned)frame_num;
					VDLogAppMessage(kVDLogWarning, kVDST_VideoSource, kVDM_FrameTooShort, 3, &frame, &actual, &expected);
					to_copy = data_len;
				} else
					throw MyError("VideoSourceAVI: uncompressed frame %I64u is short (expected %d bytes, got %d)", frame_num, mSourceFrameSize, data_len);
			}
		}
		
		memcpy((void *)getFrameBuffer(), inputBuffer, to_copy);
	} else if (fUseGDI) {
		if (!hbmLame)
			throw MyError("Insufficient GDI resources to convert frame.");

		// Windows 95/98 need a DC for this.
		HDC hdc = GetDC(0);
		SetDIBits(hdc, hbmLame, 0, getDecompressedFormat()->biHeight, inputBuffer, (BITMAPINFO *)getFormat(), DIB_RGB_COLORS);
		ReleaseDC(0,hdc);
		GdiFlush();
	} else if (mpDecompressor) {
		// Asus ASV1 crashes with zero byte frames!!!

		if (data_len) {
			try {
				vdprotected2("using output buffer at "VDPROT_PTR"-"VDPROT_PTR, void *, lpvBuffer, void *, (char *)lpvBuffer + mFrameBufferSize - 1) {
					vdprotected2("using input buffer at "VDPROT_PTR"-"VDPROT_PTR, const void *, inputBuffer, const void *, (const char *)inputBuffer + data_len - 1) {
						vdprotected1("decompressing video frame %lu", unsigned long, (unsigned long)frame_num) {
							mpDecompressor->DecompressFrame(lpvBuffer, inputBuffer, data_len, _isKey(frame_num), is_preroll);
						}
					}
				}
			} catch(const MyError& e) {
				if (mErrorMode == kErrorModeReportAll)
					throw MyError("Error decompressing video frame %u:\n\n%s", (unsigned)frame_num, e.gets());
				else {
					const unsigned frame = (unsigned)frame_num;
					VDLogAppMessage(kVDLogWarning, kVDST_VideoSource, kVDM_DecodingError, 1, &frame);

					if (mErrorMode == kErrorModeConceal)
						mbConcealingErrors = true;
				}
			}
		}
	} else {
		void *tmpBuffer = NULL;

		if (data_len < mSourceFrameSize) {
			if (mErrorMode != kErrorModeReportAll) {
				const unsigned frame = (unsigned)frame_num;
				const unsigned actual = data_len;
				const unsigned expected = mSourceFrameSize;
				VDLogAppMessage(kVDLogWarning, kVDST_VideoSource, kVDM_FrameTooShort, 3, &frame, &actual, &expected);

				tmpBuffer = malloc(mSourceFrameSize);
				if (!tmpBuffer)
					throw MyMemoryError();

				memcpy(tmpBuffer, inputBuffer, data_len);

				inputBuffer = tmpBuffer;
			} else
				throw MyError("VideoSourceAVI: uncompressed frame %u is short (expected %d bytes, got %d)", (unsigned)frame_num, mSourceFrameSize, data_len);
		}

		DecompressFrame(inputBuffer);

		free(tmpBuffer);
	}
//		memcpy(getFrameBuffer(), inputBuffer, getDecompressedFormat()->biSizeImage);

	lLastFrame = frame_num;

	return getFrameBuffer();
}

void VideoSourceAVI::streamEnd() {

	if (!mbDecodeStarted)
		return;

	mbDecodeStarted = false;

	// If an error occurs, but no one is there to hear it, was
	// there ever really an error?

	if (mpDecompressor && !bDirectDecompress)
		mpDecompressor->Stop();

	pAVIStream->EndStreaming();

}

const void *VideoSourceAVI::getFrame(VDPosition lFrameDesired) {
	VDPosition lFrameKey, lFrameNum;
	int aviErr;

	// illegal frame number?

	if (lFrameDesired < mSampleFirst || lFrameDesired >= mSampleLast)
		throw MyError("VideoSourceAVI: bad frame # (%d not within [%u, %u])", lFrameDesired, (unsigned)mSampleFirst, (unsigned)(mSampleLast-1));

	// do we already have this frame?

	if (lLastFrame == lFrameDesired)
		return getFrameBuffer();

	// back us off to the last key frame if we need to

	lFrameNum = lFrameKey = nearestKey(lFrameDesired);

	_RPT1(0,"Nearest key frame: %ld\n", lFrameKey);

	if (lLastFrame > lFrameKey && lLastFrame < lFrameDesired)
		lFrameNum = lLastFrame+1;

	if (mpDecompressor)
		mpDecompressor->Start();

	lLastFrame = -1;		// In case we encounter an exception.
	stream_current_frame	= -1;	// invalidate streaming frame

	vdblock<char>	dataBuffer;
	do {
		uint32 lBytesRead, lSamplesRead;

		for(;;) {
			if (dataBuffer.empty())
				aviErr = AVIERR_BUFFERTOOSMALL;
			else
				aviErr = read(lFrameNum, 1, dataBuffer.data(), dataBuffer.size(), &lBytesRead, &lSamplesRead);

			if (aviErr == AVIERR_BUFFERTOOSMALL) {
				aviErr = read(lFrameNum, 1, NULL, 0, &lBytesRead, &lSamplesRead);

				if (aviErr)
					throw MyAVIError("VideoSourceAVI", aviErr);

				uint32 newSize = (lBytesRead + streamGetDecodePadding() + 65535) & -65535;
				if (!newSize)
					++newSize;

				dataBuffer.resize(newSize);
			} else if (aviErr) {
				throw MyAVIError("VideoSourceAVI", aviErr);
			} else
				break;
		};

		if (!lBytesRead)
			continue;

		streamGetFrame(dataBuffer.data(), lBytesRead, false, lFrameNum);

	} while(++lFrameNum <= lFrameDesired);

	lLastFrame = lFrameDesired;

	return getFrameBuffer();
}

void VideoSourceAVI::setDecodeErrorMode(ErrorMode mode) {
	mErrorMode = mode;
}

bool VideoSourceAVI::isDecodeErrorModeSupported(ErrorMode mode) {
	return true;
}

///////////////////////////////////////////////////////////////////////////

class VDVideoCodecBugTrap : public IVDVideoCodecBugTrap {
public:
	void OnCodecRenamingDetected(const wchar_t *pName) {
		static bool sbBadCodecDetected = false;

		if (!sbBadCodecDetected) {
			VDLogAppMessage(kVDLogWarning, kVDST_VideoSource, kVDM_CodecRenamingDetected, 1, &pName);
			sbBadCodecDetected = true;
		}
	}

	void OnAcceptedBS(const wchar_t *pName) {
		static bool sbBSReported = false;	// Only report once per session.

		if (!sbBSReported) {
			VDLogAppMessage(kVDLogWarning, kVDST_VideoSource, kVDM_CodecAcceptsBS, 1, &pName);
			sbBSReported = true;
		}
	}

	void OnCodecModifiedInput(const wchar_t *pName) {
		static bool sbReported = false;	// Only report once per session.

		if (!sbReported) {
			VDLogAppMessage(kVDLogWarning, kVDST_VideoSequenceCompressor, kVDM_CodecModifiesInput, 1, &pName);
			sbReported = true;
		}
	}

} g_videoCodecBugTrap;

void VDInitVideoCodecBugTrap() {
	VDSetVideoCodecBugTrap(&g_videoCodecBugTrap);
}
