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

#include <vd2/system/bitmath.h>
#include <vd2/system/debug.h>
#include <vd2/system/error.h>
#include <vd2/system/protscope.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "VBitmap.h"
#include "crash.h"
#include "misc.h"

#include "filters.h"

#include "FilterInstance.h"
#include "FilterSystem.h"
#include "FilterFrame.h"
#include "FilterFrameAllocator.h"
#include "FilterFrameAllocatorManager.h"
#include "FilterFrameConverter.h"
#include "FilterFrameRequest.h"
#include "FilterAccelEngine.h"
#include "FilterAccelUploader.h"
#include "FilterAccelDownloader.h"
#include "FilterAccelConverter.h"

extern FilterFunctions g_filterFuncs;

namespace {
	bool IsVDXAFormat(int format) {
		return format == nsVDXPixmap::kPixFormat_VDXA_RGB || format == nsVDXPixmap::kPixFormat_VDXA_YUV;
	}
}

///////////////////////////////////////////////////////////////////////////

class VDFilterSystemDefaultScheduler : public vdrefcounted<IVDFilterSystemScheduler> {
public:
	void Reschedule();
	bool Block();

protected:
	VDSignal	mPendingTasks;
};

void VDFilterSystemDefaultScheduler::Reschedule() {
	mPendingTasks.signal();
}

bool VDFilterSystemDefaultScheduler::Block() {
	mPendingTasks.wait();
	return true;
}

///////////////////////////////////////////////////////////////////////////

struct FilterSystem::Bitmaps {
	vdrefptr<IVDFilterFrameSource> mpSource;
	vdrefptr<IVDFilterSystemScheduler> mpScheduler;
	IVDFilterFrameSource *mpTailSource;

	VFBitmapInternal	mInitialBitmap;
	VDPixmapLayout		mFinalLayout;

	VDFilterFrameAllocatorManager	mAllocatorManager;

	vdrefptr<VDFilterAccelEngine>	mpAccelEngine;
};

///////////////////////////////////////////////////////////////////////////

FilterSystem::FilterSystem()
	: mbFiltersInited(false)
	, mbFiltersError(false)
	, mbAccelEnabled(false)
	, mbAccelDebugVisual(false)
	, mOutputFrameRate(0, 0)
	, mOutputFrameCount(0)
	, mpBitmaps(new Bitmaps)
	, lpBuffer(NULL)
	, lRequiredSize(0)
{
}

FilterSystem::~FilterSystem() {
	DeinitFilters();
	DeallocateBuffers();

	if (mpBitmaps)
		delete mpBitmaps;
}

void FilterSystem::SetAccelEnabled(bool enable) {
	mbAccelEnabled = enable;
}

void FilterSystem::SetVisualAccelDebugEnabled(bool enable) {
	mbAccelDebugVisual = enable;
}

// prepareLinearChain(): init bitmaps in a linear filtering system
void FilterSystem::prepareLinearChain(List *listFA, uint32 src_width, uint32 src_height, int src_format, const VDFraction& sourceFrameRate, sint64 sourceFrameCount, const VDFraction& sourcePixelAspect) {
	FilterInstance *fa;

	if (mbFiltersInited)
		return;

	DeallocateBuffers();

	VDPixmapCreateLinearLayout(mpBitmaps->mInitialBitmap.mPixmapLayout, src_format, src_width, src_height, 16);

	if (VDPixmapGetInfo(src_format).palsize > 0)
		mpBitmaps->mInitialBitmap.mPixmapLayout.palette = mPalette;

	if (src_format == nsVDPixmap::kPixFormat_XRGB8888)
		VDPixmapLayoutFlipV(mpBitmaps->mInitialBitmap.mPixmapLayout);

	mpBitmaps->mInitialBitmap.mFrameRateHi		= sourceFrameRate.getHi();
	mpBitmaps->mInitialBitmap.mFrameRateLo		= sourceFrameRate.getLo();
	mpBitmaps->mInitialBitmap.mFrameCount		= sourceFrameCount;
	mpBitmaps->mInitialBitmap.ConvertPixmapLayoutToBitmapLayout();
	mpBitmaps->mInitialBitmap.mAspectRatioHi	= sourcePixelAspect.getHi();
	mpBitmaps->mInitialBitmap.mAspectRatioLo	= sourcePixelAspect.getLo();

	VFBitmapInternal *bmLast = &mpBitmaps->mInitialBitmap;

	fa = (FilterInstance *)listFA->tail.next;

	lRequiredSize = 0;
	mbFiltersUseAcceleration = false;

	VFBitmapInternal bmTemp;
	VFBitmapInternal bmTemp2;

	while(fa->next) {
		FilterInstance *fa_next = (FilterInstance *)fa->next;

		if (!fa->IsEnabled()) {
			fa = fa_next;
			continue;
		}

		fa->mbBlitOnEntry	= false;
		fa->mbConvertOnEntry = false;
		fa->mbAlignOnEntry	= false;

		// check if we need to blit
		uint32 flags;
		if (bmLast->mPixmapLayout.format != nsVDPixmap::kPixFormat_XRGB8888 || bmLast->mPixmapLayout.pitch > 0
			|| VDPixmapGetInfo(bmLast->mPixmapLayout.format).palsize) {
			bmTemp = *bmLast;
			VDPixmapCreateLinearLayout(bmTemp.mPixmapLayout, nsVDPixmap::kPixFormat_XRGB8888, bmLast->w, bmLast->h, 16);
			VDPixmapLayoutFlipV(bmTemp.mPixmapLayout);
			bmTemp.ConvertPixmapLayoutToBitmapLayout();

			flags = fa->Prepare(bmTemp);
			fa->mbConvertOnEntry = true;
		} else {
			flags = fa->Prepare(*bmLast);
		}

		if (flags == FILTERPARAM_NOT_SUPPORTED || (flags & FILTERPARAM_SUPPORTS_ALTFORMATS)) {
			using namespace nsVDPixmap;
			VDASSERTCT(kPixFormat_Max_Standard < 32);
			VDASSERTCT(kPixFormat_Max_Standard == kPixFormat_YUV420_NV12 + 1);
			uint32 formatMask	= (1 << kPixFormat_XRGB1555)
								| (1 << kPixFormat_RGB565)
								| (1 << kPixFormat_RGB888)
								| (1 << kPixFormat_XRGB8888)
								| (1 << kPixFormat_Y8)
								| (1 << kPixFormat_YUV422_UYVY)
								| (1 << kPixFormat_YUV422_YUYV)
								| (1 << kPixFormat_YUV444_Planar)
								| (1 << kPixFormat_YUV422_Planar)
								| (1 << kPixFormat_YUV420_Planar)
								| (1 << kPixFormat_YUV411_Planar)
								| (1 << kPixFormat_YUV410_Planar);

			static const int kStaticOrder[]={
				kPixFormat_YUV444_Planar,
				kPixFormat_YUV422_Planar,
				kPixFormat_YUV422_UYVY,
				kPixFormat_YUV422_YUYV,
				kPixFormat_YUV420_Planar,
				kPixFormat_YUV411_Planar,
				kPixFormat_YUV410_Planar,
				kPixFormat_XRGB8888
			};

			int staticOrderIndex = 0;

			// test an invalid format and make sure the filter DOESN'T accept it
			bmTemp = *bmLast;
			bmTemp.mPixmapLayout.format = 255;
			flags = fa->Prepare(bmTemp);

			int originalFormat = bmLast->mPixmapLayout.format;
			int format = originalFormat;
			if (flags != FILTERPARAM_NOT_SUPPORTED) {
				formatMask = 0;
				VDASSERT(fa->GetInvalidFormatHandlingState());
			} else {
				if (mbAccelEnabled && fa->IsAcceleratable()) {
					static const int kFormats[]={
						nsVDXPixmap::kPixFormat_VDXA_RGB,
						nsVDXPixmap::kPixFormat_VDXA_YUV,
						nsVDXPixmap::kPixFormat_VDXA_RGB
					};

					const int *formats = kFormats + 1;

					switch(originalFormat) {
						case nsVDXPixmap::kPixFormat_RGB565:
						case nsVDXPixmap::kPixFormat_XRGB1555:
						case nsVDXPixmap::kPixFormat_RGB888:
						case nsVDXPixmap::kPixFormat_XRGB8888:
						case nsVDXPixmap::kPixFormat_VDXA_RGB:
							formats = kFormats;
							break;
					}

					for(int i=0; i<2; ++i) {
						format = formats[i];

						bmTemp = *bmLast;
						VDPixmapCreateLinearLayout(bmTemp.mPixmapLayout, nsVDPixmap::kPixFormat_XRGB8888, bmLast->w, bmLast->h, 16);
						bmTemp.mPixmapLayout.format = format;
						bmTemp.ConvertPixmapLayoutToBitmapLayout();
						flags = fa->Prepare(bmTemp);

						if (flags != FILTERPARAM_NOT_SUPPORTED) {
							formatMask = 0;
							break;
						}
					}
				}

				while(format && formatMask) {
					if (formatMask & (1 << format)) {
						if (format == originalFormat)
							flags = fa->Prepare(*bmLast);
						else {
							bmTemp = *bmLast;
							VDPixmapCreateLinearLayout(bmTemp.mPixmapLayout, format, bmLast->w, bmLast->h, 16);
							if (format == nsVDPixmap::kPixFormat_XRGB8888)
								VDPixmapLayoutFlipV(bmTemp.mPixmapLayout);
							bmTemp.ConvertPixmapLayoutToBitmapLayout();
							flags = fa->Prepare(bmTemp);
						}

						if (flags != FILTERPARAM_NOT_SUPPORTED)
							break;

						formatMask &= ~(1 << format);
					}

					switch(format) {
					case kPixFormat_YUV422_UYVY:
						if (formatMask & (1 << kPixFormat_YUV422_YUYV))
							format = kPixFormat_YUV422_YUYV;
						else
							format = kPixFormat_YUV422_Planar;
						break;

					case kPixFormat_YUV422_YUYV:
						if (formatMask & (1 << kPixFormat_YUV422_UYVY))
							format = kPixFormat_YUV422_UYVY;
						else
							format = kPixFormat_YUV422_Planar;
						break;

					case kPixFormat_Y8:
					case kPixFormat_YUV422_Planar:
						format = kPixFormat_YUV444_Planar;
						break;

					case kPixFormat_YUV420_Planar:
					case kPixFormat_YUV411_Planar:
						format = kPixFormat_YUV422_Planar;
						break;

					case kPixFormat_YUV410_Planar:
					case kPixFormat_YUV420_NV12:
						format = kPixFormat_YUV420_Planar;
						break;

					case kPixFormat_YUV422_V210:
						format = kPixFormat_YUV422_Planar;
						break;

					case kPixFormat_YUV422_UYVY_709:
						format = kPixFormat_YUV422_UYVY;
						break;

					case kPixFormat_XRGB1555:
					case kPixFormat_RGB565:
					case kPixFormat_RGB888:
						if (formatMask & (1 << kPixFormat_XRGB8888)) {
							format = kPixFormat_XRGB8888;
							break;
						}

						// fall through

					default:
						if (staticOrderIndex < sizeof(kStaticOrder)/sizeof(kStaticOrder[0]))
							format = kStaticOrder[staticOrderIndex++];
						else if (formatMask & (1 << kPixFormat_XRGB8888))
							format = kPixFormat_XRGB8888;
						else
							format = VDFindLowestSetBit(formatMask);
						break;
					}
				}
			}

			fa->mbConvertOnEntry = (format != originalFormat);
		}

		if (fa->GetInvalidFormatState()) {
			fa->mbConvertOnEntry = false;
			fa->mRealSrc = *bmLast;
			fa->mRealDst = fa->mRealSrc;
			flags = 0;
		}

		fa->mbBlitOnEntry = fa->mbConvertOnEntry || fa->mbAlignOnEntry;

		if (flags & FILTERPARAM_NEEDS_LAST)
			lRequiredSize += fa->mRealLast.size + fa->mRealLast.offset;

		bmLast = &fa->mRealDst;

		if (IsVDXAFormat(bmLast->mPixmapLayout.format))
			mbFiltersUseAcceleration = true;

		// Next filter.
		fa = fa_next;
	}

	// 2/3) Temp buffers
	fa = (FilterInstance *)listFA->tail.next;

	mOutputPixelAspect.Assign(bmLast->mAspectRatioHi, bmLast->mAspectRatioLo);
	mOutputFrameRate.Assign(bmLast->mFrameRateHi, bmLast->mFrameRateLo);
	mOutputFrameCount = bmLast->mFrameCount;
	mpBitmaps->mFinalLayout = bmLast->mPixmapLayout;

	if (IsVDXAFormat(mpBitmaps->mFinalLayout.format)) {
		int format;

		if (mpBitmaps->mFinalLayout.format == nsVDXPixmap::kPixFormat_VDXA_RGB)
			format = nsVDXPixmap::kPixFormat_XRGB8888;
		else if (mpBitmaps->mFinalLayout.format == nsVDXPixmap::kPixFormat_VDXA_YUV)
			format = nsVDXPixmap::kPixFormat_YUV444_Planar;

		VDPixmapCreateLinearLayout(mpBitmaps->mFinalLayout, format, mpBitmaps->mFinalLayout.w, mpBitmaps->mFinalLayout.h, 16);
	}
}

// initLinearChain(): prepare for a linear filtering system
void FilterSystem::initLinearChain(IVDFilterSystemScheduler *scheduler, uint32 filterStateFlags, List *listFA, IVDFilterFrameSource *src, uint32 src_width, uint32 src_height, int src_format, const uint32 *palette, const VDFraction& sourceFrameRate, sint64 sourceFrameCount, const VDFraction& sourcePixelAspect) {
	FilterInstance *fa;
	long lLastBufPtr = 0;

	DeinitFilters();
	DeallocateBuffers();

	if (!scheduler)
		scheduler = new VDFilterSystemDefaultScheduler;

	mpBitmaps->mpScheduler = scheduler;

	mFilterStateFlags = filterStateFlags;

	// buffers required:
	//
	// 1) Input buffer (8/16/24/32 bits)
	// 2) Temp buffer #1 (32 bits)
	// 3) Temp buffer #2 (32 bits)
	//
	// All temporary buffers must be aligned on an 8-byte boundary, and all
	// pitches must be a multiple of 8 bytes.  The exceptions are the source
	// and destination buffers, which may have pitches that are only 4-byte
	// multiples.

	int palSize = VDPixmapGetInfo(src_format).palsize;
	if (palette && palSize)
		memcpy(mPalette, palette, palSize*sizeof(uint32));

	prepareLinearChain(listFA, src_width, src_height, src_format, sourceFrameRate, sourceFrameCount, sourcePixelAspect);

	if (mbFiltersUseAcceleration) {
		mpBitmaps->mpAccelEngine = new VDFilterAccelEngine;

		if (!mpBitmaps->mpAccelEngine->Init(mbAccelDebugVisual))
			throw MyError("Cannot start filter chain: The 3D accelerator device is not available.");
	}

	AllocateBuffers(lRequiredSize);

	mpBitmaps->mAllocatorManager.Shutdown();

	mpBitmaps->mpSource = src;
	mpBitmaps->mpSource->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager, NULL);

	mpBitmaps->mInitialBitmap.mPixmap = VDPixmap();

	fa = (FilterInstance *)listFA->tail.next;

	const VFBitmapInternal *rawSrc = &mpBitmaps->mInitialBitmap;
	VDFilterFrameAllocatorProxy *prevProxy = src->GetOutputAllocatorProxy();

	while(fa->next) {
		FilterInstance *fa_next = (FilterInstance *)fa->next;

		if (!fa->IsEnabled()) {
			fa = fa_next;
			continue;
		}

		if (fa->GetInvalidFormatHandlingState())
			throw MyError("Cannot start filters: Filter \"%s\" is not handling image formats correctly.",
				fa->GetName());

		if (fa->mRealDst.w < 1 || fa->mRealDst.h < 1)
			throw MyError("Cannot start filter chain: The output of filter \"%s\" is smaller than 1x1.", fa->GetName());


		if (fa->GetAlphaParameterCurve()) {
				// size check
			if (fa->mRealSrc.w != fa->mRealDst.w || fa->mRealSrc.h != fa->mRealDst.h) {
				throw MyError("Cannot start filter chain: Filter \"%s\" has a blend curve attached and has differing input and output sizes (%dx%d -> %dx%d). Input and output sizes must match."
					, fa->GetName()
					, fa->mRealSrc.w
					, fa->mRealSrc.h
					, fa->mRealDst.w
					, fa->mRealDst.h
					);
			}
		}

		const VDPixmapLayout& srcLayout0 = src->GetOutputLayout();
		int srcFormat = srcLayout0.format;
		if (fa->IsConversionRequired() || (IsVDXAFormat(srcFormat) && fa->IsCroppingEnabled())) {
			int dstFormat = fa->mExternalSrc.mPixmapLayout.format;

			vdrect32 srcCrop(fa->GetCropInsets());
			vdrect32 srcRect(srcCrop.left, srcCrop.top, srcLayout0.w - srcCrop.right, srcLayout0.h - srcCrop.bottom);

			if (IsVDXAFormat(dstFormat)) {
				if (IsVDXAFormat(srcFormat)) {		// VDXA -> VDXA
					vdrefptr<VDFilterAccelConverter> conv(new VDFilterAccelConverter);

					VDPixmapLayout layout;
					VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_XRGB8888, fa->mExternalSrcCropped.w, fa->mExternalSrcCropped.h, 16);

					layout.format = dstFormat;

					conv->Init(mpBitmaps->mpAccelEngine, src, layout, &srcRect);
					conv->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager, prevProxy);
					src = conv;
					mFilters.push_back(conv.release());
					prevProxy = src->GetOutputAllocatorProxy();
				} else {	// CPU -> VDXA
					// convert to 8888/888 if necessary
					if (srcFormat != nsVDPixmap::kPixFormat_XRGB8888 && srcFormat != nsVDPixmap::kPixFormat_YUV444_Planar) {
						vdrefptr<VDFilterFrameConverter> conv(new VDFilterFrameConverter);

						VDPixmapLayout layout;

						if (dstFormat == nsVDXPixmap::kPixFormat_VDXA_YUV)
							VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_YUV444_Planar, fa->mExternalSrc.w, fa->mExternalSrc.h, 16);
						else
							VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_XRGB8888, fa->mExternalSrc.w, fa->mExternalSrc.h, 16);

						conv->Init(src, layout, NULL);
						conv->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager, prevProxy);
						src = conv;
						mFilters.push_back(conv.release());
						prevProxy = src->GetOutputAllocatorProxy();
						srcFormat = layout.format;
					}

					// upload to accelerator
					{
						vdrefptr<VDFilterAccelUploader> conv(new VDFilterAccelUploader);

						VDPixmapLayout layout;
						VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_XRGB8888, fa->mExternalSrcCropped.w, fa->mExternalSrcCropped.h, 16);

						bool isRGB = false;
						if (srcFormat == nsVDXPixmap::kPixFormat_XRGB8888) {
							layout.format = nsVDXPixmap::kPixFormat_VDXA_RGB;
							isRGB = true;
						} else
							layout.format = nsVDXPixmap::kPixFormat_VDXA_YUV;

						VDPixmapLayout srcLayout(src->GetOutputLayout());

						srcLayout.data += srcCrop.top * srcLayout.pitch + (isRGB ? srcCrop.left << 2 : srcCrop.left);
						srcLayout.data2 += srcCrop.top * srcLayout.pitch2 + srcCrop.left;
						srcLayout.data3 += srcCrop.top * srcLayout.pitch3 + srcCrop.left;
						srcLayout.w -= srcCrop.left + srcCrop.right;
						srcLayout.h -= srcCrop.bottom + srcCrop.top;

						conv->Init(mpBitmaps->mpAccelEngine, src, layout, &srcLayout);
						conv->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager, prevProxy);
						src = conv;
						mFilters.push_back(conv.release());
						prevProxy = src->GetOutputAllocatorProxy();
						srcFormat = layout.format;
					}

					// apply conversion on VDXA side if necessary
					if (srcFormat != dstFormat) {
						vdrefptr<VDFilterAccelConverter> conv(new VDFilterAccelConverter);

						VDPixmapLayout layout;
						VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_XRGB8888, fa->mExternalSrcCropped.w, fa->mExternalSrcCropped.h, 16);

						layout.format = dstFormat;

						conv->Init(mpBitmaps->mpAccelEngine, src, layout, NULL);
						conv->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager, prevProxy);
						src = conv;
						mFilters.push_back(conv.release());
						prevProxy = src->GetOutputAllocatorProxy();
					}
				}
			} else {	// VDXA, CPU -> CPU
				bool cpuConversionRequired = true;
				
				if (IsVDXAFormat(srcFormat)) {		// VDXA -> CPU
					const VDPixmapLayout& finalLayout = src->GetOutputLayout();

					int targetFormat;
					if (finalLayout.format == nsVDXPixmap::kPixFormat_VDXA_RGB)
						targetFormat = nsVDPixmap::kPixFormat_XRGB8888;
					else
						targetFormat = nsVDPixmap::kPixFormat_YUV444_Planar;

					VDPixmapLayout layout;
					if (dstFormat == targetFormat) {
						layout = fa->mExternalSrc.mPixmapLayout;
						cpuConversionRequired = false;
					} else
						VDPixmapCreateLinearLayout(layout, targetFormat, finalLayout.w, finalLayout.h, 16);

					vdrefptr<VDFilterAccelDownloader> conv(new VDFilterAccelDownloader);
					conv->Init(mpBitmaps->mpAccelEngine, mpBitmaps->mpScheduler, src, layout, NULL);
					conv->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager, prevProxy);
					src = conv;
					prevProxy = src->GetOutputAllocatorProxy();
					mFilters.push_back(conv.release());
				}

				if (cpuConversionRequired) {
					vdrefptr<VDFilterFrameConverter> conv(new VDFilterFrameConverter);

					conv->Init(src, fa->mExternalSrc.mPixmapLayout, NULL);
					conv->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager, prevProxy);
					src = conv;
					mFilters.push_back(conv.release());
					prevProxy = src->GetOutputAllocatorProxy();
				}
			}
		}

		if (fa->IsAlignmentRequired()) {
			vdrefptr<VDFilterFrameConverter> conv(new VDFilterFrameConverter);

			conv->Init(src, fa->mExternalSrcCropped.mPixmapLayout, &fa->mExternalSrcPreAlign.mPixmapLayout);
			conv->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager, prevProxy);
			src = conv;
			mFilters.push_back(conv.release());
			prevProxy = fa->GetOutputAllocatorProxy();
		}

		fa->AddRef();
		mFilters.push_back(fa);
		src = fa;

//		fa->mExternalSrc = *rawSrc;
		fa->mExternalDst = fa->mRealDst;

		VDFileMappingW32 *mapping = NULL;
		if (((fa->mRealSrc.dwFlags | fa->mRealDst.dwFlags) & VFBitmapInternal::NEEDS_HDC) && !(fa->GetFlags() & FILTERPARAM_SWAP_BUFFERS)) {
			uint32 mapSize = std::max<uint32>(VDPixmapLayoutGetMinSize(fa->mRealSrc.mPixmapLayout), VDPixmapLayoutGetMinSize(fa->mRealDst.mPixmapLayout));

			if (!fa->mFileMapping.Init(mapSize))
				throw MyMemoryError();

			mapping = &fa->mFileMapping;
		}

		fa->mRealSrc.hdc = NULL;
		if (mapping || (fa->mRealSrc.dwFlags & VDXFBitmap::NEEDS_HDC))
			fa->mRealSrc.BindToDIBSection(mapping);

		fa->mRealDst.hdc = NULL;
		if (mapping || (fa->mRealDst.dwFlags & VDXFBitmap::NEEDS_HDC))
			fa->mRealDst.BindToDIBSection(mapping);

		fa->mRealLast.hdc = NULL;
		if (fa->GetFlags() & FILTERPARAM_NEEDS_LAST) {
			fa->mRealLast.Fixup(lpBuffer + lLastBufPtr);

			if (fa->mRealLast.dwFlags & VDXFBitmap::NEEDS_HDC)
				fa->mRealLast.BindToDIBSection(NULL);

			lLastBufPtr += fa->mRealLast.size + fa->mRealLast.offset;
		}

		fa->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager, prevProxy);
		prevProxy = fa->GetOutputAllocatorProxy();

		rawSrc = &fa->mExternalDst;

		fa = fa_next;
	}

	// check if the last format is accelerated, and add a downloader if necessary
	const VDPixmapLayout& finalLayout = src->GetOutputLayout();
	if (finalLayout.format == nsVDXPixmap::kPixFormat_VDXA_RGB) {
		vdrefptr<VDFilterAccelDownloader> conv(new VDFilterAccelDownloader);

		VDPixmapLayout layout;
		VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_XRGB8888, finalLayout.w, finalLayout.h, 16);

		conv->Init(mpBitmaps->mpAccelEngine, mpBitmaps->mpScheduler, src, layout, NULL);
		conv->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager, prevProxy);
		mFilters.push_back(conv.release());
	} else if (finalLayout.format == nsVDXPixmap::kPixFormat_VDXA_YUV) {
		vdrefptr<VDFilterAccelDownloader> conv(new VDFilterAccelDownloader);

		VDPixmapLayout layout;
		VDPixmapCreateLinearLayout(layout, nsVDPixmap::kPixFormat_YUV444_Planar, finalLayout.w, finalLayout.h, 16);

		conv->Init(mpBitmaps->mpAccelEngine, mpBitmaps->mpScheduler, src, layout, NULL);
		conv->RegisterAllocatorProxies(&mpBitmaps->mAllocatorManager, prevProxy);
		mFilters.push_back(conv.release());
	}
}

void FilterSystem::ReadyFilters() {
	if (mbFiltersInited)
		return;

	mbFiltersError = false;

	mpBitmaps->mAllocatorManager.AssignAllocators(mpBitmaps->mpAccelEngine);

	IVDFilterFrameSource *pLastSource = mpBitmaps->mpSource;

	mActiveFilters.clear();
	mActiveFilters.reserve(mFilters.size());

	try {
		for(Filters::const_iterator it(mFilters.begin()), itEnd(mFilters.end()); it != itEnd; ++it) {
			IVDFilterFrameSource *src = *it;
			mActiveFilters.push_back(src);

			FilterInstance *fa = vdpoly_cast<FilterInstance *>(src);
			if (fa)
				fa->Start(mFilterStateFlags, pLastSource, mpBitmaps->mpScheduler, mpBitmaps->mpAccelEngine);

			pLastSource = src;
		}
	} catch(const MyError&) {
		// roll back previously initialized filters (similar to deinit)
		while(!mActiveFilters.empty()) {
			IVDFilterFrameSource *fs = mActiveFilters.back();
			mActiveFilters.pop_back();

			fs->Stop();
		}

		throw;
	}

	mbFiltersInited = true;
	mpBitmaps->mpTailSource = pLastSource;
}

bool FilterSystem::RequestFrame(sint64 outputFrame, IVDFilterFrameClientRequest **creq) {
	if (!mpBitmaps->mpTailSource)
		return false;

	return mpBitmaps->mpTailSource->CreateRequest(outputFrame, false, creq);
}

FilterSystem::RunResult FilterSystem::Run(bool runToCompletion) {
	if (mbFiltersError)
		return kRunResult_Idle;

	if (!mbFiltersInited)
		return kRunResult_Idle;

	bool activity = false;
	for(;;) {
		bool didSomething = false;

		Filters::const_iterator it(mActiveFilters.end()), itEnd(mActiveFilters.begin());
		while(it != itEnd) {
			IVDFilterFrameSource *fa = *--it;

			try {
				IVDFilterFrameSource::RunResult rr = fa->RunRequests();
				if (rr == IVDFilterFrameSource::kRunResult_Blocked && runToCompletion) {
					mpBitmaps->mpScheduler->Block();
					++it;
					continue;
				}

				if (rr == IVDFilterFrameSource::kRunResult_Running) {
					++it;

					if (!runToCompletion)
						return kRunResult_Running;

					didSomething = true;
				}
			} catch(const MyError&) {
				mbFiltersError = true;
				throw;
			}
		}

		if (!didSomething)
			break;

		activity = true;
	}

	return activity ? kRunResult_Running : kRunResult_Idle;
}

void FilterSystem::InvalidateCachedFrames(FilterInstance *startingFilter) {
	Filters::const_iterator it(mActiveFilters.begin()), itEnd(mActiveFilters.end());
	bool invalidating = !startingFilter;

	for(; it != itEnd; ++it) {
		IVDFilterFrameSource *fi = *it;

		if (fi == startingFilter)
			invalidating = true;

		if (invalidating)
			fi->InvalidateAllCachedFrames();
	}
}

void FilterSystem::DeinitFilters() {
	// send all filters a 'stop'

	while(!mActiveFilters.empty()) {
		IVDFilterFrameSource *fi = mActiveFilters.back();
		mActiveFilters.pop_back();

		fi->Stop();
	}

	while(!mFilters.empty()) {
		IVDFilterFrameSource *fi = mFilters.back();
		mFilters.pop_back();

		fi->Release();
	}

	mpBitmaps->mAllocatorManager.Shutdown();
	mpBitmaps->mpTailSource = NULL;
	mpBitmaps->mpScheduler = NULL;

	if (mpBitmaps->mpAccelEngine) {
		mpBitmaps->mpAccelEngine->Shutdown();
		mpBitmaps->mpAccelEngine = NULL;
	}

	mbFiltersInited = false;
}

const VDPixmapLayout& FilterSystem::GetInputLayout() const {
	return mpBitmaps->mInitialBitmap.mPixmapLayout;
}

const VDPixmapLayout& FilterSystem::GetOutputLayout() const {
	return mpBitmaps->mFinalLayout;
}

bool FilterSystem::isRunning() const {
	return mbFiltersInited;
}

bool FilterSystem::isEmpty() const {
	return mActiveFilters.empty();
}

bool FilterSystem::GetDirectFrameMapping(VDPosition outputFrame, VDPosition& sourceFrame, int& sourceIndex) const {
	if (mbFiltersError)
		return false;

	if (!mbFiltersInited)
		return false;

	return mpBitmaps->mpTailSource->GetDirectMapping(outputFrame, sourceFrame, sourceIndex);
}

sint64 FilterSystem::GetSourceFrame(sint64 frame) const {
	Filters::const_iterator it(mActiveFilters.end()), itEnd(mActiveFilters.begin());
	while(it != itEnd) {
		IVDFilterFrameSource *fa = *--it;

		frame = fa->GetSourceFrame(frame);
	}

	return frame;
}

sint64 FilterSystem::GetSymbolicFrame(sint64 outframe, IVDFilterFrameSource *source) const {
	if (!mbFiltersInited)
		return outframe;

	if (mbFiltersError)
		return outframe;

	return mpBitmaps->mpTailSource->GetSymbolicFrame(outframe, source);
}

sint64 FilterSystem::GetNearestUniqueFrame(sint64 outframe) const {
	if (!mbFiltersInited)
		return outframe;

	return mpBitmaps->mpTailSource->GetNearestUniqueFrame(outframe);
}

const VDFraction FilterSystem::GetOutputFrameRate() const {
	return mOutputFrameRate;
}

const VDFraction FilterSystem::GetOutputPixelAspect() const {
	return mOutputPixelAspect;
}

sint64 FilterSystem::GetOutputFrameCount() const {
	return mOutputFrameCount;
}

/////////////////////////////////////////////////////////////////////////////
//
//	FilterSystem::private_methods
//
/////////////////////////////////////////////////////////////////////////////

void FilterSystem::AllocateBuffers(uint32 lTotalBufferNeeded) {
	DeallocateBuffers();

	if (!(lpBuffer = (unsigned char *)VirtualAlloc(NULL, lTotalBufferNeeded+8, MEM_COMMIT, PAGE_READWRITE)))
		throw MyMemoryError();

	memset(lpBuffer, 0, lTotalBufferNeeded+8);
}

void FilterSystem::DeallocateBuffers() {
	if (mpBitmaps) {
		mpBitmaps->mAllocatorManager.Shutdown();
		mpBitmaps->mpSource = NULL;
	}

	if (lpBuffer) {
		VirtualFree(lpBuffer, 0, MEM_RELEASE);

		lpBuffer = NULL;
	}
}
