#include "stdafx.h"
#include <vd2/system/vdstl.h>
#include <vd2/system/memory.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "capfilter.h"
#include "filters.h"

static const char g_szCannotFilter[]="Cannot use video filtering: ";

///////////////////////////////////////////////////////////////////////////
//
//	filters
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureFilter : public vdlist<VDCaptureFilter>::node {
public:
	virtual void Run(VDPixmap& px) {}
	virtual void Shutdown() {}
};

///////////////////////////////////////////////////////////////////////////
//
//	filter: crop
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureFilterCrop : public VDCaptureFilter {
public:
	void Init(VDPixmapLayout& layout, uint32 x1, uint32 y1, uint32 x2, uint32 y2);
	void Run(VDPixmap& px);

protected:
	VDPixmapLayout mLayout;
};

void VDCaptureFilterCrop::Init(VDPixmapLayout& layout, uint32 x1, uint32 y1, uint32 x2, uint32 y2) {
	const VDPixmapFormatInfo& format = VDPixmapGetInfo(layout.format);

	x1 = (x1 >> (format.qwbits + format.auxwbits)) << (format.qwbits + format.auxwbits);
	y1 = (y1 >> (format.qhbits + format.auxhbits)) << (format.qhbits + format.auxhbits);
	x2 = (x2 >> (format.qwbits + format.auxwbits)) << (format.qwbits + format.auxwbits);
	y2 = (y2 >> (format.qhbits + format.auxhbits)) << (format.qhbits + format.auxhbits);

	mLayout = VDPixmapLayoutOffset(layout, x1, y1);
	mLayout.w -= x2;
	mLayout.h -= y2;

	layout = mLayout;
}

void VDCaptureFilterCrop::Run(VDPixmap& px) {
	px = VDPixmapFromLayout(mLayout, px.data);
}

///////////////////////////////////////////////////////////////////////////
//
//	filter: noise reduction
//
///////////////////////////////////////////////////////////////////////////

#ifndef _M_AMD64
namespace {
	void __declspec(naked) dodnrMMX(uint32 *dst, uint32 *src, vdpixsize w, vdpixsize h, vdpixoffset dstmodulo, vdpixoffset srcmodulo, __int64 thresh1, __int64 thresh2) {
	static const __int64 bythree = 0x5555555555555555i64;
	static const __int64 round2 = 0x0002000200020002i64;
	static const __int64 three = 0x0003000300030003i64;

		__asm {
			push	ebp
			push	edi
			push	esi
			push	ebx

			mov		edi,[esp+4+16]
			mov		esi,[esp+8+16]
			mov		edx,[esp+12+16]
			mov		ecx,[esp+16+16]
			mov		ebx,[esp+20+16]
			mov		eax,[esp+24+16]
			movq	mm6,[esp+36+16]
			movq	mm5,[esp+28+16]

	yloop:
			mov		ebp,edx
	xloop:
			movd	mm0,[esi]		;previous
			pxor	mm7,mm7

			movd	mm1,[edi]		;current
			punpcklbw	mm0,mm7

			punpcklbw	mm1,mm7
			movq	mm2,mm0

			movq	mm4,mm1
			movq	mm3,mm1

			movq	mm7,mm0
			paddw	mm4,mm4

			pmullw	mm0,three
			psubusb	mm2,mm1

			paddw	mm4,mm7
			psubusb	mm3,mm7

			pmulhw	mm4,bythree
			por		mm2,mm3

			movq	mm3,mm2
			paddw	mm0,mm1

			paddw	mm0,round2
			pcmpgtw	mm2,mm5			;set if diff > thresh1

			pcmpgtw	mm3,mm6			;set if diff > thresh2
			psrlw	mm0,2


			;	mm2		mm3		meaning						mm1		mm0		mm4
			;	FALSE	FALSE	diff <= thresh1				off		on		off
			;	FALSE	TRUE	impossible
			;	TRUE	FALSE	thresh1 < diff <= thresh2	off		off		on
			;	TRUE	TRUE	diff > thresh2				on		off		off

			pand	mm1,mm3			;keep pixels exceeding threshold2
			pand	mm4,mm2			;	average pixels <= threshold2...
			pandn	mm2,mm0			;replace pixels below threshold1
			pandn	mm3,mm4			;	but >= threshold1...
			por		mm1,mm2
			add		esi,4
			por		mm1,mm3
			add		edi,4
			packuswb	mm1,mm1
			dec		ebp

			movd	[esi-4],mm1		;store to both
			movd	[edi-4],mm1
			jne		xloop

			add		esi,eax
			add		edi,ebx
			dec		ecx
			jne		yloop

			pop		ebx
			pop		esi
			pop		edi
			pop		ebp
			emms
			ret
		}
	}
}
#else
	void dodnrMMX(uint32 *dst, uint32 *src, vdpixsize w, vdpixsize h, vdpixoffset dstmodulo, vdpixoffset srcmodulo, __int64 thresh1, __int64 thresh2) {
#pragma vdpragma_TODO("need AMD64 implementation of NR filter")
	}

#endif

class VDCaptureFilterNoiseReduction : public VDCaptureFilter {
public:
	void SetThreshold(int threshold);
	void Init(VDPixmapLayout& layout);
	void Run(VDPixmap& px);

protected:
	vdblock<uint32, vdaligned_alloc<uint32> > mBuffer;

	sint64	mThresh1;
	sint64	mThresh2;
};

void VDCaptureFilterNoiseReduction::SetThreshold(int threshold) {
	mThresh1 = 0x0001000100010001i64*((threshold>>1)+1);
	mThresh2 = 0x0001000100010001i64*(threshold);

	if (!threshold)
		mThresh1 = mThresh2;
}

void VDCaptureFilterNoiseReduction::Init(VDPixmapLayout& layout) {
	const VDPixmapFormatInfo& format = VDPixmapGetInfo(layout.format);

	uint32 rowdwords = -(((-layout.w >> format.qwbits) * format.qsize) >> 2);
	uint32 h = -(-layout.h >> format.qhbits);

	mBuffer.resize(rowdwords * h);
}


void VDCaptureFilterNoiseReduction::Run(VDPixmap& px) {
	unsigned w;

	switch(px.format) {
		case nsVDPixmap::kPixFormat_RGB888:
			w = (px.w*3+3)>>2;
			dodnrMMX((uint32 *)px.data, mBuffer.data(), w, px.h, px.pitch - (w<<2), 0, mThresh1, mThresh2);
			break;
		case nsVDPixmap::kPixFormat_XRGB8888:
			w = px.w;
			dodnrMMX((uint32 *)px.data, mBuffer.data(), px.w, px.h, px.pitch - (px.w<<2), 0, mThresh1, mThresh2);
			break;
		case nsVDPixmap::kPixFormat_YUV422_UYVY:
		case nsVDPixmap::kPixFormat_YUV422_YUYV:
			w = (px.w*2+3)>>2;
			dodnrMMX((uint32 *)px.data, mBuffer.data(), w, px.h, px.pitch - (w<<2), 0, mThresh1, mThresh2);
			break;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	filter: luma squish
//
///////////////////////////////////////////////////////////////////////////

#ifndef _M_AMD64
namespace {
	// Squish 0...255 range to 16...235.
	//
	// The bIsUYVY value MUST be either 0 (false) or 1 (true) ....

	static void __declspec(naked) __cdecl lumasquish_MMX(void *dst, ptrdiff_t pitch, long w2, long h, int bIsUYVY) {
		static const __int64 scalers[2] = {
			0x40003b0040003b00i64,		// YUY2
			0x3b0040003b004000i64,		// UYVY
		};
		static const __int64 biases[2] = {
			0x0000000500000005i64,		// YUY2
			0x0005000000050000i64,		// UYVY
		};

		__asm {
			push		ebp
			push		edi
			push		esi
			push		ebx

			mov			eax,[esp+20+16]

			movq		mm6,qword ptr [scalers+eax*8]
			movq		mm5,qword ptr [biases+eax*8]

			mov			ecx,[esp+12+16]
			mov			esi,[esp+4+16]
			mov			ebx,[esp+16+16]
			mov			eax,[esp+8+16]
			mov			edx,ecx
			shl			edx,2
			sub			eax,edx

	yloop:
			mov			edx,ecx
			test		esi,4
			jz			xloop_aligned_start

			movd		mm0,[esi]
			pxor		mm7,mm7
			punpcklbw	mm0,mm7
			add			esi,4
			psllw		mm0,2
			dec			edx
			paddw		mm0,mm5
			pmulhw		mm0,mm6
			packuswb	mm0,mm0
			movd		[esi-4],mm0
			jz			xloop_done

	xloop_aligned_start:
			sub			edx,3
			jbe			xloop_done
	xloop_aligned:
			movq		mm0,[esi]
			pxor		mm7,mm7

			movq		mm2,[esi+8]
			movq		mm1,mm0

			punpcklbw	mm0,mm7
			movq		mm3,mm2

			psllw		mm0,2
			add			esi,16

			paddw		mm0,mm5
			punpckhbw	mm1,mm7

			psllw		mm1,2
			pmulhw		mm0,mm6

			paddw		mm1,mm5
			punpcklbw	mm2,mm7

			pmulhw		mm1,mm6
			psllw		mm2,2

			punpckhbw	mm3,mm7
			paddw		mm2,mm5

			psllw		mm3,2
			pmulhw		mm2,mm6

			paddw		mm3,mm5
			packuswb	mm0,mm1

			pmulhw		mm3,mm6
			sub			edx,4

			movq		[esi-16],mm0

			packuswb	mm2,mm3

			movq		[esi-8],mm2
			ja			xloop_aligned

			add			edx,3
			jz			xloop_done

	xloop_tail:
			movd		mm0,[esi]
			pxor		mm7,mm7
			punpcklbw	mm0,mm7
			add			esi,4
			psllw		mm0,2
			dec			edx
			paddw		mm0,mm5
			pmulhw		mm0,mm6
			packuswb	mm0,mm0
			movd		[esi-4],mm0
			jne			xloop_tail

	xloop_done:
			add			esi,eax

			dec			ebx
			jne			yloop

			pop			ebx
			pop			esi
			pop			edi
			pop			ebp
			emms
			ret
		}
	}
}
#else
namespace {
	void lumasquish_MMX(void *dst, ptrdiff_t pitch, long w2, long h, int bIsUYVY) {
#pragma vdpragma_TODO("need AMD64 implementation of lumasquish filter")
	}
}
#endif

class VDCaptureFilterLumaSquish : public VDCaptureFilter {
public:
	void Init(VDPixmapLayout& layout) {}
	void Run(VDPixmap& px);
};

void VDCaptureFilterLumaSquish::Run(VDPixmap& px) {
	lumasquish_MMX(px.data, px.pitch, (px.w+1)>>1, px.h, px.format == nsVDPixmap::kPixFormat_YUV422_UYVY);
}

///////////////////////////////////////////////////////////////////////////
//
//	filter: swap fields
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureFilterSwapFields : public VDCaptureFilter {
public:
	void Init(VDPixmapLayout& layout) {}
	void Run(VDPixmap& px);

	static void SwapPlaneRows(void *p, ptrdiff_t pitch, unsigned w, unsigned h);
};

void VDCaptureFilterSwapFields::Run(VDPixmap& px) {
	const VDPixmapFormatInfo& format = VDPixmapGetInfo(px.format);

	// swap plane 0 rows
	if (!format.qhbits)
		SwapPlaneRows(px.data, px.pitch, -(-px.w >> format.qwbits) * format.qsize, px.h);

	// swap plane 1 and 2 rows
	if (format.auxbufs && !format.auxhbits) {
		unsigned auxw = -(-px.w >> format.auxwbits);

		SwapPlaneRows(px.data2, px.pitch2, auxw, px.h);

		if (format.auxbufs >= 2)
			SwapPlaneRows(px.data3, px.pitch3, auxw, px.h);
	}
}

void VDCaptureFilterSwapFields::SwapPlaneRows(void *p1, ptrdiff_t pitch, unsigned w, unsigned h) {
	const ptrdiff_t pitch2x = pitch + pitch;
	void *p2 = vdptroffset(p1, pitch);

	for(vdpixsize y=0; y<h-1; y+=2) {
		VDSwapMemory(p1, p2, w);
		vdptrstep(p1, pitch2x);
		vdptrstep(p2, pitch2x);
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	filter: vert squash
//
///////////////////////////////////////////////////////////////////////////

extern "C" long resize_table_col_by2linear_MMX(Pixel *out, Pixel **in_table, vdpixsize w);
extern "C" long resize_table_col_by2cubic_MMX(Pixel *out, Pixel **in_table, vdpixsize w);

class VDCaptureFilterVertSquash : public VDCaptureFilter {
public:
	void SetMode(IVDCaptureFilterSystem::FilterMode mode);
	void Init(VDPixmapLayout& layout);
	void Run(VDPixmap& px);
	void Shutdown();

protected:
	vdblock<uint32, vdaligned_alloc<uint32> >	mRowBuffers;
	uint32	mDwordsPerRow;
	IVDCaptureFilterSystem::FilterMode	mMode;
};

void VDCaptureFilterVertSquash::SetMode(IVDCaptureFilterSystem::FilterMode mode) {
	mMode = mode;
}

void VDCaptureFilterVertSquash::Init(VDPixmapLayout& layout) {
	const VDPixmapFormatInfo& info = VDPixmapGetInfo(layout.format);

	mDwordsPerRow = -(((-layout.w >> info.qwbits) * info.qsize) >> 2);

	if (mMode == IVDCaptureFilterSystem::kFilterCubic)
		mRowBuffers.resize(mDwordsPerRow * 3);

	layout.h >>= 1;
}

void VDCaptureFilterVertSquash::Run(VDPixmap& px) {
#ifndef _M_AMD64
	Pixel *srclimit = vdptroffset((Pixel *)px.data, px.pitch * (px.h - 1));
	Pixel *dst = (Pixel *)px.data;
	uint32 y = px.h >> 1;
	Pixel *src[8];

	switch(mMode) {
	case IVDCaptureFilterSystem::kFilterCubic:
		memcpy(mRowBuffers.data() + mDwordsPerRow*0, (char *)px.data + px.pitch*0, mDwordsPerRow*4);
		memcpy(mRowBuffers.data() + mDwordsPerRow*1, (char *)px.data + px.pitch*1, mDwordsPerRow*4);
		memcpy(mRowBuffers.data() + mDwordsPerRow*2, (char *)px.data + px.pitch*2, mDwordsPerRow*4);

		src[0] = (Pixel *)mRowBuffers.data();
		src[1] = src[0] + mDwordsPerRow*1;
		src[2] = src[1] + mDwordsPerRow*2;

		src[3] = vdptroffset(dst, 3*px.pitch);
		src[4] = vdptroffset(src[3], px.pitch);
		src[5] = vdptroffset(src[4], px.pitch);
		src[6] = vdptroffset(src[5], px.pitch);
		src[7] = vdptroffset(src[6], px.pitch);

		while(y--) {
			resize_table_col_by2cubic_MMX((Pixel *)dst, (Pixel **)src, mDwordsPerRow);

			vdptrstep(dst, px.pitch);
			src[0] = src[2];
			src[1] = src[3];
			src[2] = src[4];
			src[3] = src[5];
			src[4] = src[6];
			src[5] = src[7];
			vdptrstep(src[6], px.pitch*2);
			vdptrstep(src[7], px.pitch*2);

			if (px.pitch < 0) {
				if (src[6] <= srclimit)
					src[6] = srclimit;
				if (src[7] <= srclimit)
					src[7] = srclimit;
			} else {
				if (src[6] >= srclimit)
					src[6] = srclimit;
				if (src[7] >= srclimit)
					src[7] = srclimit;
			}
		}
		__asm emms
		break;

	case IVDCaptureFilterSystem::kFilterLinear:
		src[1] = (Pixel *)px.data;
		src[2] = src[1];
		src[3] = vdptroffset(src[2], px.pitch);

		while(y--) {
			resize_table_col_by2linear_MMX((Pixel *)dst, (Pixel **)src, mDwordsPerRow);

			vdptrstep(dst, px.pitch);
			src[1] = src[3];
			vdptrstep(src[2], px.pitch*2);
			vdptrstep(src[3], px.pitch*2);

			if (px.pitch < 0) {
				if (src[2] <= srclimit)
					src[2] = srclimit;
				if (src[3] <= srclimit)
					src[3] = srclimit;
			} else {
				if (src[2] >= srclimit)
					src[2] = srclimit;
				if (src[3] >= srclimit)
					src[3] = srclimit;
			}
		}
		__asm emms
		break;

	}
#else
#pragma vdpragma_TODO("need AMD64 blah blah blah")
#endif

	px.h >>= 1;
}

void VDCaptureFilterVertSquash::Shutdown() {
	mRowBuffers.clear();
}


///////////////////////////////////////////////////////////////////////////
//
//	filter: ChainAdapter
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureFilterChainAdapter : public VDCaptureFilter {
public:
	void SetFrameRate(uint32 usPerFrame);
	void Init(VDPixmapLayout& layout);
	void Run(VDPixmap& px);
	void Shutdown();

protected:
	FilterStateInfo	mfsi;
};

void VDCaptureFilterChainAdapter::SetFrameRate(uint32 usPerFrame) {
	mfsi.lMicrosecsPerFrame		= usPerFrame;
	mfsi.lMicrosecsPerSrcFrame	= usPerFrame;
}

void VDCaptureFilterChainAdapter::Init(VDPixmapLayout& layout) {
	mfsi.lCurrentFrame			= 0;
	mfsi.lDestFrameMS			= 0;
	mfsi.lCurrentSourceFrame	= 0;
	mfsi.lSourceFrameMS			= 0;
	mfsi.flags					= FilterStateInfo::kStateRealTime;

	filters.initLinearChain(&g_listFA, (Pixel *)layout.palette, layout.w, layout.h, 0);

	if (filters.ReadyFilters(&mfsi))
		throw MyError("%sUnable to initialize filters.", g_szCannotFilter);
}

void VDCaptureFilterChainAdapter::Run(VDPixmap& px) {
	VDPixmapBlt(VDAsPixmap(*filters.InputBitmap()), px);

	mfsi.lSourceFrameMS				= mfsi.lCurrentSourceFrame * mfsi.lMicrosecsPerSrcFrame;
	mfsi.lDestFrameMS				= mfsi.lCurrentFrame * mfsi.lMicrosecsPerFrame;

	filters.RunFilters();

	px = VDAsPixmap(*filters.LastBitmap());

	++mfsi.lCurrentFrame;
	++mfsi.lCurrentSourceFrame;
}

void VDCaptureFilterChainAdapter::Shutdown() {
	filters.DeinitFilters();
	filters.DeallocateBuffers();
}

///////////////////////////////////////////////////////////////////////////
//
//	filter system
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureFilterSystem : public IVDCaptureFilterSystem {
public:
	VDCaptureFilterSystem();
	~VDCaptureFilterSystem();

	void SetCrop(uint32 x1, uint32 y1, uint32 x2, uint32 y2);
	void SetNoiseReduction(uint32 threshold);
	void SetLumaSquish(bool enable);
	void SetFieldSwap(bool enable);
	void SetVertSquashMode(FilterMode mode);
	void SetChainEnable(bool enable);

	void Init(VDPixmapLayout& layout, uint32 usPerFrame);
	void Run(VDPixmap& px);
	void Shutdown();

protected:
	uint32	mCropX1;
	uint32	mCropY1;
	uint32	mCropX2;
	uint32	mCropY2;
	uint32	mNoiseReductionThreshold;
	FilterMode	mVertSquashMode;
	bool	mbLumaSquish;
	bool	mbFieldSwap;
	bool	mbChainEnable;

	typedef vdlist<VDCaptureFilter> tFilterChain;
	tFilterChain		mFilterChain;

	VDCaptureFilterCrop				mFilterCrop;
	VDCaptureFilterNoiseReduction	mFilterNoiseReduction;
	VDCaptureFilterLumaSquish		mFilterLumaSquish;
	VDCaptureFilterSwapFields		mFilterSwapFields;
	VDCaptureFilterVertSquash		mFilterVertSquash;
	VDCaptureFilterChainAdapter		mFilterChainAdapter;
};

IVDCaptureFilterSystem *VDCreateCaptureFilterSystem() {
	return new VDCaptureFilterSystem;
}

VDCaptureFilterSystem::VDCaptureFilterSystem()
	: mCropX1(0)
	, mCropY1(0)
	, mCropX2(0)
	, mCropY2(0)
	, mNoiseReductionThreshold(0)
	, mVertSquashMode(kFilterDisable)
	, mbLumaSquish(false)
	, mbFieldSwap(false)
	, mbChainEnable(false)
{
}

VDCaptureFilterSystem::~VDCaptureFilterSystem() {
	Shutdown();
}

void VDCaptureFilterSystem::SetCrop(uint32 x1, uint32 y1, uint32 x2, uint32 y2) {
	mCropX1 = x1;
	mCropY1 = y1;
	mCropX2 = x2;
	mCropY2 = y2;
}

void VDCaptureFilterSystem::SetNoiseReduction(uint32 threshold) {
	mNoiseReductionThreshold = threshold;
}

void VDCaptureFilterSystem::SetLumaSquish(bool enable) {
	mbLumaSquish = enable;
}

void VDCaptureFilterSystem::SetFieldSwap(bool enable) {
	mbFieldSwap = enable;
}

void VDCaptureFilterSystem::SetVertSquashMode(FilterMode mode) {
	mVertSquashMode = mode;
}

void VDCaptureFilterSystem::SetChainEnable(bool enable) {
	mbChainEnable = enable;
}

void VDCaptureFilterSystem::Init(VDPixmapLayout& pxl, uint32 usPerFrame) {
	if (mCropX1|mCropY1|mCropX2|mCropY2) {
		mFilterCrop.Init(pxl, mCropX1, mCropY1, mCropX2, mCropY2);
		mFilterChain.push_back(&mFilterCrop);
	}

	if (mNoiseReductionThreshold) {
		mFilterNoiseReduction.SetThreshold(mNoiseReductionThreshold);
		mFilterNoiseReduction.Init(pxl);
		mFilterChain.push_back(&mFilterNoiseReduction);
	}

	if (mbLumaSquish) {
		mFilterLumaSquish.Init(pxl);
		mFilterChain.push_back(&mFilterLumaSquish);
	}

	if (mbFieldSwap) {
		mFilterSwapFields.Init(pxl);
		mFilterChain.push_back(&mFilterSwapFields);
	}

	if (mVertSquashMode) {
		mFilterVertSquash.SetMode(mVertSquashMode);
		mFilterVertSquash.Init(pxl);
		mFilterChain.push_back(&mFilterVertSquash);
	}

	if (mbChainEnable) {
		mFilterChainAdapter.SetFrameRate(usPerFrame);
		mFilterChainAdapter.Init(pxl);
		mFilterChain.push_back(&mFilterChainAdapter);
	}
}

void VDCaptureFilterSystem::Run(VDPixmap& px) {
	tFilterChain::iterator it(mFilterChain.begin()), itEnd(mFilterChain.end());

	for(; it!=itEnd; ++it) {
		VDCaptureFilter *pFilt = *it;

		VDAssertValidPixmap(px);
		pFilt->Run(px);
	}

	VDAssertValidPixmap(px);
}

void VDCaptureFilterSystem::Shutdown() {
	tFilterChain::iterator it(mFilterChain.begin()), itEnd(mFilterChain.end());

	for(; it!=itEnd; ++it) {
		VDCaptureFilter *pFilt = *it;

		pFilt->Shutdown();
	}

	mFilterChain.clear();
}
