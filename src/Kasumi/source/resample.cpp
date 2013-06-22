//	VirtualDub - Video processing and capture application
//	Graphics support library
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

#include <float.h>
#include <math.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/memory.h>
#include <vd2/system/math.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/resample.h>

///////////////////////////////////////////////////////////////////////////
//
// utility functions
//
///////////////////////////////////////////////////////////////////////////

namespace {
	sint32 scale32x32_fp16(sint32 x, sint32 y) {
		return (sint32)(((sint64)x * y + 0x8000) >> 16);
	}
}

///////////////////////////////////////////////////////////////////////////
//
// sampling calculations
//
///////////////////////////////////////////////////////////////////////////

namespace {
	struct VDResamplerAxis {
		sint32		dx;
		sint32		u;
		sint32		dudx;
		uint32		dx_precopy;
		uint32		dx_preclip;
		uint32		dx_active;
		uint32		dx_postclip;
		uint32		dx_postcopy;
		uint32		dx_dualclip;

		void Init(sint32 dudx);
		void Compute(sint32 x1, sint32 x2, sint32 fx1_unclipped, sint32 u0, sint32 w, sint32 kernel_width);
	};

	void VDResamplerAxis::Init(sint32 dudx) {
		this->dudx = dudx;
	}

	void VDResamplerAxis::Compute(sint32 x1, sint32 x2, sint32 fx1_unclipped, sint32 u0, sint32 w, sint32 kernel_width) {
		u = u0 + scale32x32_fp16(dudx, ((x1<<16)+0x8000) - fx1_unclipped);

		dx = x2-x1;

		sint32 du_kern	= (kernel_width-1) << 16;
		sint32 u2		= u + dudx*(dx-1);
		sint32 u_limit	= w << 16;

		dx_precopy	= 0;
		dx_preclip	= 0;
		dx_active	= 0;
		dx_postclip	= 0;
		dx_postcopy = 0;
		dx_dualclip	= 0;

		sint32 dx_temp = dx;
		sint32 u_start = u;

		// (desired - u0 + (dudx-1)) / dudx : first pixel >= desired

		sint32 dudx_m1_mu0	= dudx - 1 - u;
		sint32 first_preclip	= (dudx_m1_mu0 + 0x10000 - du_kern) / dudx;
		sint32 first_active		= (dudx_m1_mu0                    ) / dudx;
		sint32 first_postclip	= (dudx_m1_mu0 + u_limit - du_kern) / dudx;
		sint32 first_postcopy	= (dudx_m1_mu0 + u_limit - 0x10000) / dudx;

		// clamp
		if (first_preclip < 0)
			first_preclip = 0;
		if (first_active < first_preclip)
			first_active = first_preclip;
		if (first_postclip < first_active)
			first_postclip = first_active;
		if (first_postcopy < first_postclip)
			first_postcopy = first_postclip;
		if (first_preclip > dx)
			first_preclip = dx;
		if (first_active > dx)
			first_active = dx;
		if (first_postclip > dx)
			first_postclip = dx;
		if (first_postcopy > dx)
			first_postcopy = dx;

		// determine widths

		dx_precopy	= first_preclip;
		dx_preclip	= first_active - first_preclip;
		dx_active	= first_postclip - first_active;
		dx_postclip	= first_postcopy - first_postclip;
		dx_postcopy	= dx - first_postcopy;

		// sanity checks
		sint32 pos0 = dx_precopy;
		sint32 pos1 = pos0 + dx_preclip;
		sint32 pos2 = pos1 + dx_active;
		sint32 pos3 = pos2 + dx_postclip;

		VDASSERT(!((dx_precopy|dx_preclip|dx_active|dx_postcopy|dx_postclip) & 0x80000000));
		VDASSERT(dx_precopy + dx_preclip + dx_active + dx_postcopy + dx_postclip == dx);

		VDASSERT(!pos0			|| u_start + dudx*(pos0 - 1) <  0x10000 - du_kern);	// precopy -> preclip
		VDASSERT( pos0 >= pos1	|| u_start + dudx*(pos0    ) >= 0x10000 - du_kern);
		VDASSERT( pos1 <= pos0	|| u_start + dudx*(pos1 - 1) <  0);					// preclip -> active
		VDASSERT( pos1 >= pos2	|| u_start + dudx*(pos1    ) >= 0 || !dx_active);
		VDASSERT( pos2 <= pos1	|| u_start + dudx*(pos2 - 1) <  u_limit - du_kern || !dx_active);	// active -> postclip
		VDASSERT( pos2 >= pos3	|| u_start + dudx*(pos2    ) >= u_limit - du_kern);
		VDASSERT( pos3 <= pos2	|| u_start + dudx*(pos3 - 1) <  u_limit - 0x10000);	// postclip -> postcopy
		VDASSERT( pos3 >= dx	|| u_start + dudx*(pos3    ) >= u_limit - 0x10000);

		u += dx_precopy * dudx;

		// test for overlapping clipping regions
		if (!dx_active) {
			dx_dualclip = dx_preclip + dx_postclip;
			dx_preclip = dx_postclip = 0;
		}
	}

	struct VDResamplerInfo {
		void		*mpDst;
		ptrdiff_t	mDstPitch;
		const void	*mpSrc;
		ptrdiff_t	mSrcPitch;
		vdpixsize	mSrcW;
		vdpixsize	mSrcH;

		VDResamplerAxis mXAxis;
		VDResamplerAxis mYAxis;
	};
}

///////////////////////////////////////////////////////////////////////////
//
// allocation
//
///////////////////////////////////////////////////////////////////////////

namespace {
	class VDSteppedAllocator {
	public:
		typedef	size_t		size_type;
		typedef	ptrdiff_t	difference_type;

		VDSteppedAllocator(size_t initialSize = 1024);
		~VDSteppedAllocator();

		void clear();
		void *allocate(size_type n);

	protected:
		struct Block {
			Block *next;
		};

		Block *mpHead;
		char *mpAllocNext;
		size_t	mAllocLeft;
		size_t	mAllocNext;
		size_t	mAllocInit;
	};
	
	VDSteppedAllocator::VDSteppedAllocator(size_t initialSize)
		: mpHead(NULL)
		, mpAllocNext(NULL)
		, mAllocLeft(0)
		, mAllocNext(initialSize)
		, mAllocInit(initialSize)
	{
	}

	VDSteppedAllocator::~VDSteppedAllocator() {
		clear();
	}

	void VDSteppedAllocator::clear() {
		while(Block *p = mpHead) {
			mpHead = mpHead->next;
			free(p);
		}
		mAllocLeft = 0;
		mAllocNext = mAllocInit;
	}

	void *VDSteppedAllocator::allocate(size_type n) {
		n = (n+15) & ~15;
		if (mAllocLeft < n) {
			mAllocLeft = mAllocNext;
			mAllocNext += (mAllocNext >> 1);
			if (mAllocLeft < n)
				mAllocLeft = n;

			Block *t = (Block *)malloc(sizeof(Block) + mAllocLeft);

			if (mpHead)
				mpHead->next = t;

			mpHead = t;
			mpHead->next = NULL;

			mpAllocNext = (char *)(mpHead + 1);
		}

		void *p = mpAllocNext;
		mpAllocNext += n;
		mAllocLeft -= n;
		return p;
	}
}

///////////////////////////////////////////////////////////////////////////
//
// filter kernels
//
///////////////////////////////////////////////////////////////////////////

namespace {
	class IVDResamplerFilter {
	public:
		virtual int GetFilterWidth() const = 0;
		virtual void GenerateFilterBank(float *dst) const = 0;
	};

	class VDResamplerLinearFilter : public IVDResamplerFilter {
	public:
		VDResamplerLinearFilter(double twofc)
			: mScale(twofc)
			, mTaps((int)ceil(1.0 / twofc) * 2)
		{
		}

		int GetFilterWidth() const { return mTaps; }

		void GenerateFilterBank(float *dst) const {
			const double basepos = -(double)((mTaps>>1)-1) * mScale;
			for(int offset=0; offset<256; ++offset) {
				double pos = basepos - offset*((1.0/256.0) * mScale);

				for(unsigned i=0; i<mTaps; ++i) {
					double t = 1.0 - fabs(pos);

					*dst++ = (float)(t+fabs(t));
					pos += mScale;
				}
			}
		}

	protected:
		double		mScale;
		unsigned	mTaps;
	};

	class VDResamplerCubicFilter : public IVDResamplerFilter {
	public:
		VDResamplerCubicFilter(double twofc, double A)
			: mScale(twofc)
			, mA(A)
			, mTaps((int)ceil(2.0 / twofc)*2)
		{
		}

		int GetFilterWidth() const { return mTaps; }

		void GenerateFilterBank(float *dst) const {
			const double a0 = ( 1.0  );
			const double a2 = (-3.0-mA);
			const double a3 = ( 2.0+mA);
			const double b0 = (-4.0*mA);
			const double b1 = ( 8.0*mA);
			const double b2 = (-5.0*mA);
			const double b3 = (     mA);
			const double basepos = -(double)((mTaps>>1)-1) * mScale;

			for(int offset=0; offset<256; ++offset) {
				double pos = basepos - offset*((1.0f/256.0f) * mScale);

				for(unsigned i=0; i<mTaps; ++i) {
					double t = fabs(pos);
					double v = 0;

					if (t < 1.0)
						v = a0 + (t*t)*(a2 + t*a3);
					else if (t < 2.0)
						v = b0 + t*(b1 + t*(b2 + t*b3));

					*dst++ = (float)v;
					pos += mScale;
				}
			}
		}

	protected:
		double		mScale;
		double		mA;
		unsigned	mTaps;
	};

	static inline double sinc(double x) {
		return fabs(x) < 1e-9 ? 1.0 : sin(x) / x;
	}

	class VDResamplerLanczos3Filter : public IVDResamplerFilter {
	public:
		VDResamplerLanczos3Filter(double twofc)
			: mScale(twofc)
			, mTaps((int)ceil(3.0 / twofc)*2)
		{
		}

		int GetFilterWidth() const { return mTaps; }

		void GenerateFilterBank(float *dst) const {
			static const double pi  = 3.1415926535897932384626433832795;	// pi
			static const double pi3 = 1.0471975511965977461542144610932;	// pi/3
			const double basepos = -(double)((mTaps>>1)-1) * mScale;

			for(int offset=0; offset<256; ++offset) {
				double t = basepos - offset*((1.0/256.0) * mScale);

				for(unsigned i=0; i<mTaps; ++i) {
					double v = 0;

					if (t < 3.0)
						v = sinc(pi*t) * sinc(pi3*t);

					*dst++ = (float)v;
					t += mScale;
				}
			}
		}

	protected:
		double		mScale;
		unsigned	mTaps;
	};
}

///////////////////////////////////////////////////////////////////////////
//
// resampler stages (common)
//
///////////////////////////////////////////////////////////////////////////

class IVDResamplerStage {
public:
	virtual ~IVDResamplerStage() {}

	void *operator new(size_t n, VDSteppedAllocator& a) {
		return a.allocate(n);
	}

	void operator delete(void *p, VDSteppedAllocator& a) {
	}

	virtual void Process(const VDResamplerInfo& info) {}

	virtual sint32 GetHorizWindowSize() const { return 1; }
	virtual sint32 GetVertWindowSize() const { return 1; }

private:
	// these should NEVER be called
	void operator delete(void *p) {}
};

class IVDResamplerSeparableRowStage : public IVDResamplerStage {
public:
	virtual void Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) = 0;
	virtual int GetWindowSize() const = 0;
};

class IVDResamplerSeparableColStage : public IVDResamplerStage {
public:
	virtual int GetWindowSize() const = 0;
	virtual void Process(void *dst, const void *const *src, uint32 w, sint32 phase) = 0;
};

///////////////////////////////////////////////////////////////////////////
//
// resampler stages (portable)
//
///////////////////////////////////////////////////////////////////////////

namespace {
	void GenerateTable(sint32 *dst, const IVDResamplerFilter& filter) {
		const unsigned width = filter.GetFilterWidth();
		vdblock<float> filters(width * 256);
		float *src = filters.data();

		filter.GenerateFilterBank(src);

		for(unsigned phase=0; phase < 256; ++phase) {
			float sum = 0;

			for(unsigned i=0; i<width; ++i)
				sum += src[i];

			float scalefac = 16384.0f / sum;

			for(unsigned j=0; j<width; j += 2) {
				int v0 = VDRoundToIntFast(src[j+0] * scalefac);
				int v1 = VDRoundToIntFast(src[j+1] * scalefac);

				dst[j+0] = v0;
				dst[j+1] = v1;
			}

			src += width;
			dst += width;
		}
	}

	void SwizzleTable(sint32 *dst, unsigned pairs) {
		do {
			sint32 v0 = dst[0];
			sint32 v1 = dst[1];

			dst[0] = dst[1] = (v0 & 0xffff) + (v1<<16);
			dst += 2;
		} while(--pairs);
	}
}

class VDResamplerSeparablePointRowStage : public IVDResamplerSeparableRowStage {
public:
	int GetWindowSize() const {return 1;}
	void Process(void *dst0, const void *src0, uint32 w, uint32 u, uint32 dudx) {
		uint32 *dst = (uint32 *)dst0;
		const uint32 *src = (const uint32 *)src0;

		do {
			*dst++ = src[u>>16];
			u += dudx;
		} while(--w);
	}
};

class VDResamplerSeparableLinearRowStage : public IVDResamplerSeparableRowStage {
public:
	int GetWindowSize() const {return 2;}
	void Process(void *dst0, const void *src0, uint32 w, uint32 u, uint32 dudx) {
		uint32 *dst = (uint32 *)dst0;
		const uint32 *src = (const uint32 *)src0;

		do {
			const sint32 iu = u>>16;
			const uint32 p0 = src[iu];
			const uint32 p1 = src[iu+1];
			const uint32 f = (u >> 8) & 0xff;

			const uint32 p0_rb = p0 & 0xff00ff;
			const uint32 p1_rb = p1 & 0xff00ff;
			const uint32 p0_g = p0 & 0xff00;
			const uint32 p1_g = p1 & 0xff00;

			*dst++	= ((p0_rb + (((p1_rb - p0_rb)*f + 0x800080)>>8)) & 0xff00ff)
					+ ((p0_g  + (((p1_g  - p0_g )*f + 0x008000)>>8)) & 0x00ff00);
			u += dudx;
		} while(--w);
	}
};

class VDResamplerSeparableLinearColStage : public IVDResamplerSeparableColStage {
public:
	int GetWindowSize() const {return 2;}
	void Process(void *dst0, const void *const *srcarray, uint32 w, sint32 phase) {
		uint32 *dst = (uint32 *)dst0;
		const uint32 *src0 = (const uint32 *)srcarray[0];
		const uint32 *src1 = (const uint32 *)srcarray[1];
		const uint32 f = (phase >> 8) & 0xff;

		do {
			const uint32 p0 = *src0++;
			const uint32 p1 = *src1++;

			const uint32 p0_rb = p0 & 0xff00ff;
			const uint32 p1_rb = p1 & 0xff00ff;
			const uint32 p0_g = p0 & 0xff00;
			const uint32 p1_g = p1 & 0xff00;

			*dst++	= ((p0_rb + (((p1_rb - p0_rb)*f + 0x800080)>>8)) & 0xff00ff)
					+ ((p0_g  + (((p1_g  - p0_g )*f + 0x008000)>>8)) & 0x00ff00);
		} while(--w);
	}
};

class VDResamplerSeparableTableRowStage : public IVDResamplerSeparableRowStage {
public:
	VDResamplerSeparableTableRowStage(const IVDResamplerFilter& filter) {
		mFilterBank.resize(filter.GetFilterWidth() * 256);
		GenerateTable(mFilterBank.data(), filter);
	}

	int GetWindowSize() const {return mFilterBank.size() >> 8;}

	void Process(void *dst0, const void *src0, uint32 w, uint32 u, uint32 dudx) {
		uint32 *dst = (uint32 *)dst0;
		const uint32 *src = (const uint32 *)src0;
		const unsigned ksize = mFilterBank.size() >> 8;
		const sint32 *filterBase = mFilterBank.data();

		do {
			const uint32 *src2 = src + (u>>16);
			const sint32 *filter = filterBase + ksize*((u>>8)&0xff);
			u += dudx;

			int r = 0x8000, g = 0x8000, b = 0x8000;
			for(unsigned i = ksize; i; --i) {
				uint32 p = *src2++;
				sint32 coeff = *filter++;

				r += ((p>>16)&0xff)*coeff;
				g += ((p>> 8)&0xff)*coeff;
				b += ((p    )&0xff)*coeff;
			}

			r <<= 2;
			g >>= 6;
			b >>= 14;

			if ((uint32)r >= 0x01000000)
				r = ~r >> 31;
			if ((uint32)g >= 0x00010000)
				g = ~g >> 31;
			if ((uint32)b >= 0x00000100)
				b = ~b >> 31;

			*dst++ = (r & 0xff0000) + (g & 0xff00) + (b & 0xff);
		} while(--w);
	}

protected:
	vdblock<sint32, vdaligned_alloc<sint32> >	mFilterBank;
};

class VDResamplerSeparableTableColStage : public IVDResamplerSeparableColStage {
public:
	VDResamplerSeparableTableColStage(const IVDResamplerFilter& filter) {
		mFilterBank.resize(filter.GetFilterWidth() * 256);
		GenerateTable(mFilterBank.data(), filter);
	}

	int GetWindowSize() const {return mFilterBank.size() >> 8;}

	void Process(void *dst0, const void *const *src0, uint32 w, sint32 phase) {
		uint32 *dst = (uint32 *)dst0;
		const uint32 *const *src = (const uint32 *const *)src0;
		const unsigned ksize = mFilterBank.size() >> 8;
		const sint32 *filter = &mFilterBank[((phase>>8)&0xff) * ksize];

		for(uint32 i=0; i<w; ++i) {
			int r = 0x8000, g = 0x8000, b = 0x8000;
			const sint32 *filter2 = filter;
			const uint32 *const *src2 = src;

			for(unsigned j = ksize; j; --j) {
				uint32 p = (*src2++)[i];
				sint32 coeff = *filter2++;

				r += ((p>>16)&0xff)*coeff;
				g += ((p>> 8)&0xff)*coeff;
				b += ((p    )&0xff)*coeff;
			}

			r <<= 2;
			g >>= 6;
			b >>= 14;

			if ((uint32)r >= 0x01000000)
				r = ~r >> 31;
			if ((uint32)g >= 0x00010000)
				g = ~g >> 31;
			if ((uint32)b >= 0x00000100)
				b = ~b >> 31;

			*dst++ = (r & 0xff0000) + (g & 0xff00) + (b & 0xff);
		} while(--w);
	}

protected:
	vdblock<sint32, vdaligned_alloc<sint32> >	mFilterBank;
};

class VDResamplerSeparableStage : public IVDResamplerStage {
public:
	VDResamplerSeparableStage(IVDResamplerSeparableRowStage *pRow, IVDResamplerSeparableColStage *pCol);
	~VDResamplerSeparableStage() {
		if (mpRowStage)
			mpRowStage->~IVDResamplerSeparableRowStage();
		if (mpColStage)
			mpColStage->~IVDResamplerSeparableColStage();
	}

	void Process(const VDResamplerInfo&);

	sint32 GetHorizWindowSize() const { return mpRowStage ? mpRowStage->GetWindowSize() : 1; }
	sint32 GetVertWindowSize() const { return mpColStage ? mpColStage->GetWindowSize() : 1; }

protected:
	void ProcessPoint();
	void ProcessComplex();
	void ProcessRow(void *dst, const void *src);

	IVDResamplerSeparableRowStage *const mpRowStage;
	IVDResamplerSeparableColStage *const mpColStage;

	uint32				mWinSize;
	uint32				mRowFiltW;

	VDResamplerInfo		mInfo;

	vdblock<void *>	mWindow;
	void				**mpAllocWindow;
	vdblock<uint32, vdaligned_alloc<uint32> >		mTempSpace;
};

VDResamplerSeparableStage::VDResamplerSeparableStage(IVDResamplerSeparableRowStage *pRow, IVDResamplerSeparableColStage *pCol)
	: mpRowStage(pRow)
	, mpColStage(pCol)
{
	mWinSize = mpColStage ? mpColStage->GetWindowSize() : 1;
	mRowFiltW = mpRowStage->GetWindowSize();
	mWindow.resize(3*mWinSize);
}

void VDResamplerSeparableStage::Process(const VDResamplerInfo& info) {
	mInfo = info;

	if (mpColStage || (mpRowStage->GetWindowSize()>1 && mInfo.mYAxis.dudx < 0x10000))
		ProcessComplex();
	else
		ProcessPoint();
}

void VDResamplerSeparableStage::ProcessPoint() {
	mTempSpace.resize(mRowFiltW*3);

	const uint32 winsize = mWinSize;
	const uint32 dx = mInfo.mXAxis.dx;

	const uint32 *src = (const uint32 *)mInfo.mpSrc;
	const ptrdiff_t srcpitch = mInfo.mSrcPitch;
	const sint32 srch = mInfo.mSrcH;
	void *dst = mInfo.mpDst;
	const ptrdiff_t dstpitch = mInfo.mDstPitch;


	if (uint32 count = mInfo.mYAxis.dx_precopy) {
		do {
			ProcessRow(dst, src);
			vdptrstep(dst, dstpitch);
		} while(--count);
	}

	if (uint32 count = mInfo.mYAxis.dx_preclip + mInfo.mYAxis.dx_active + mInfo.mYAxis.dx_postclip + mInfo.mYAxis.dx_dualclip) {
		sint32 v = mInfo.mYAxis.u + ((winsize-1) >> 16);
		const sint32 dvdy = mInfo.mYAxis.dudx;

		do {
			sint32 y = (v >> 16);

			if (y<0)
				y=0;
			else if (y >= srch)
				y = srch-1;

			ProcessRow(dst, vdptroffset(src, y*srcpitch));

			vdptrstep(dst, dstpitch);
			v += dvdy;
		} while(--count);
	}

	if (uint32 count = mInfo.mYAxis.dx_postcopy) {
		const uint32 *srcrow = vdptroffset(src, srcpitch*(srch-1));
		do {
			ProcessRow(dst, srcrow);
			vdptrstep(dst, dstpitch);
		} while(--count);
	}
}

void VDResamplerSeparableStage::ProcessComplex() {
	uint32 clipSpace = (mRowFiltW*3 + 3) & ~3;
	uint32 paddedWidth = (mInfo.mXAxis.dx + 3) & ~3;
	mTempSpace.resize(clipSpace + paddedWidth * mWinSize);

	uint32 *p = mTempSpace.data();
	p += clipSpace;

	mpAllocWindow = &mWindow[2*mWinSize];

	for(uint32 i=0; i<mWinSize; ++i) {
		mpAllocWindow[i] = p;
		p += paddedWidth;
	}

	const uint32 winsize = mWinSize;
	const uint32 dx = mInfo.mXAxis.dx;

	const uint32 *src = (const uint32 *)mInfo.mpSrc;
	const ptrdiff_t srcpitch = mInfo.mSrcPitch;
	const sint32 srch = mInfo.mSrcH;
	void *dst = mInfo.mpDst;
	const ptrdiff_t dstpitch = mInfo.mDstPitch;
	sint32 winpos = -1;
	uint32 winidx = mWinSize>1 ? 1 : 0;
	uint32 winallocnext = mWinSize>1 ? 1 : 0;

	std::fill(mWindow.begin(), mWindow.begin() + 2*mWinSize, mpAllocWindow[0]);

	if (mInfo.mYAxis.dx_precopy || mInfo.mYAxis.u < 0x10000) {
		winpos = 0;

		ProcessRow(mWindow[0], src);
	}

	if (uint32 count = mInfo.mYAxis.dx_precopy) {
		do {
			memcpy(dst, mWindow[0], dx * sizeof(uint32));
			vdptrstep(dst, dstpitch);
		} while(--count);
	}

	if (uint32 count = mInfo.mYAxis.dx_preclip + mInfo.mYAxis.dx_active + mInfo.mYAxis.dx_postclip + mInfo.mYAxis.dx_dualclip) {
		sint32 v = mInfo.mYAxis.u + ((winsize-1) >> 16);
		const sint32 dvdy = mInfo.mYAxis.dudx;

		do {
			sint32 desiredpos = (v >> 16) + winsize - 1;

			if (winpos < desiredpos - (sint32)winsize)
				winpos = desiredpos - (sint32)winsize;

			while(winpos+1 <= desiredpos) {
				++winpos;
				if (winpos >= srch) {
					mWindow[winidx] = mWindow[winidx + winsize] = mWindow[winidx ? winidx-1 : winsize-1];
				} else {
					mWindow[winidx] = mWindow[winidx + winsize] = mpAllocWindow[winallocnext];
					if (++winallocnext >= winsize)
						winallocnext = 0;

					ProcessRow(mWindow[winidx], vdptroffset(src, winpos*srcpitch));
				}
				if (++winidx >= winsize)
					winidx = 0;
			}

			if (mpColStage)
				mpColStage->Process(dst, &mWindow[winidx], dx, v);
			else
				memcpy(dst, mWindow[winidx], dx*sizeof(uint32));

			vdptrstep(dst, dstpitch);
			v += dvdy;
		} while(--count);
	}

	if (uint32 count = mInfo.mYAxis.dx_postcopy) {
		if (winpos < srch - 1) {
			mWindow[winidx] = mWindow[winidx + winsize] = mpAllocWindow[winallocnext];
			ProcessRow(mWindow[winidx], vdptroffset(src, srcpitch*(srch-1)));
			if (++winidx >= winsize)
				winidx = 0;
		}

		const void *p = mWindow[winidx + winsize - 1];
		do {
			memcpy(dst, p, dx * sizeof(uint32));
			vdptrstep(dst, dstpitch);
		} while(--count);
	}
}

void VDResamplerSeparableStage::ProcessRow(void *dst0, const void *src0) {
	const uint32 *src = (const uint32 *)src0;
	uint32 *dst = (uint32 *)dst0;

	// process pre-copy region
	if (uint32 count = mInfo.mXAxis.dx_precopy) {
		VDMemset32(dst, src[0], count);
		dst += count;
	}

	uint32 *p = mTempSpace.data();
	sint32 u = mInfo.mXAxis.u;
	const sint32 dudx = mInfo.mXAxis.dudx;

	// process dual-clip region
	if (uint32 count = mInfo.mXAxis.dx_dualclip) {
		VDMemset32(p, src[0], mRowFiltW);
		memcpy(p + mRowFiltW, src+1, (mInfo.mSrcW-2)*sizeof(uint32));
		VDMemset32(p + mRowFiltW + (mInfo.mSrcW-2), src[mInfo.mSrcW-1], mRowFiltW);

		mpRowStage->Process(dst, p, count, u + ((mRowFiltW-1)<<16), dudx);
		u += dudx*count;
		dst += count;
	} else {
		// process pre-clip region
		if (uint32 count = mInfo.mXAxis.dx_preclip) {
			VDMemset32(p, src[0], mRowFiltW);
			memcpy(p + mRowFiltW, src+1, (mRowFiltW-1)*sizeof(uint32));

			mpRowStage->Process(dst, p, count, u + ((mRowFiltW-1)<<16), dudx);
			u += dudx*count;
			dst += count;
		}

		// process active region
		if (uint32 count = mInfo.mXAxis.dx_active) {
			mpRowStage->Process(dst, src, count, u, dudx);
			u += dudx*count;
			dst += count;
		}

		// process post-clip region
		if (uint32 count = mInfo.mXAxis.dx_postclip) {
			uint32 offset = mInfo.mSrcW + 1 - mRowFiltW;

			memcpy(p, src+offset, (mRowFiltW-1)*sizeof(uint32));
			VDMemset32(p + (mRowFiltW-1), src[mInfo.mSrcW-1], mRowFiltW);

			mpRowStage->Process(dst, p, count, u - (offset<<16), dudx);
			dst += count;
		}
	}

	// process post-copy region
	if (uint32 count = mInfo.mXAxis.dx_postcopy) {
		VDMemset32(dst, src[mInfo.mSrcW-1], count);
	}
}

///////////////////////////////////////////////////////////////////////////
//
// resampler stages (scalar, x86)
//
///////////////////////////////////////////////////////////////////////////

namespace {
	struct ScaleInfo {
		void *dst;
		uintptr	src;
		uint32	accum;
		uint32	fracinc;
		sint32	intinc;
		uint32	count;
	};
}

#ifndef _M_AMD64
	extern "C" void vdasm_resize_point32(const ScaleInfo *);

	class VDResamplerSeparablePointRowStageX86 : public IVDResamplerSeparableRowStage {
	public:
		int GetWindowSize() const {return 1;}
		void Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) {
			ScaleInfo info;

			info.dst = (uint32 *)dst + w;
			info.src = ((uintptr)src >> 2) + (u>>16);
			info.accum = u<<16;
			info.fracinc = dudx << 16;
			info.intinc = (sint32)dudx >> 16;
			info.count = -(sint32)w*4;

			vdasm_resize_point32(&info);
		}
	};
#endif

///////////////////////////////////////////////////////////////////////////
//
// resampler stages (MMX, x86)
//
///////////////////////////////////////////////////////////////////////////

#ifndef _M_AMD64
	extern "C" void vdasm_resize_point32_MMX(const ScaleInfo *);

	class VDResamplerSeparablePointRowStageMMX : public IVDResamplerSeparableRowStage {
	public:
		int GetWindowSize() const {return 1;}
		void Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) {
			ScaleInfo info;

			info.dst = (uint32 *)dst + w;
			info.src = ((uintptr)src >> 2) + (u>>16);
			info.accum = u<<16;
			info.fracinc = dudx << 16;
			info.intinc = (sint32)dudx >> 16;
			info.count = -(sint32)w*4;

			vdasm_resize_point32_MMX(&info);
		}
	};

	extern "C" void vdasm_resize_interp_row_run_MMX(void *dst, const void *src, uint32 width, sint64 xaccum, sint64 x_inc);
	extern "C" void vdasm_resize_interp_col_run_MMX(void *dst, const void *src1, const void *src2, uint32 width, uint32 yaccum);
	extern "C" void vdasm_resize_ccint_row_MMX(void *dst, const void *src, uint32 count, uint32 xaccum, sint32 xinc, const void *tbl);
	extern "C" void vdasm_resize_ccint_col_MMX(void *dst, const void *src1, const void *src2, const void *src3, const void *src4, uint32 count, const void *tbl);
	extern "C" long vdasm_resize_table_col_MMX(uint32 *out, const uint32 *const*in_table, const int *filter, int filter_width, uint32 w, long frac);
	extern "C" long vdasm_resize_table_row_MMX(uint32 *out, const uint32 *in, const int *filter, int filter_width, uint32 w, long accum, long frac);

	class VDResamplerSeparableLinearRowStageMMX : public IVDResamplerSeparableRowStage {
	public:
		int GetWindowSize() const {return 2;}
		void Process(void *dst0, const void *src0, uint32 w, uint32 u, uint32 dudx) {
			vdasm_resize_interp_row_run_MMX(dst0, src0, w, (sint64)u << 16, (sint64)dudx << 16);
		}
	};

	class VDResamplerSeparableLinearColStageMMX : public IVDResamplerSeparableColStage {
	public:
		int GetWindowSize() const {return 2;}
		void Process(void *dst0, const void *const *srcarray, uint32 w, sint32 phase) {
			vdasm_resize_interp_col_run_MMX(dst0, srcarray[0], srcarray[1], w, phase);
		}
	};

	class VDResamplerSeparableCubicRowStageMMX : public IVDResamplerSeparableRowStage {
	public:
		VDResamplerSeparableCubicRowStageMMX(double A)
			: mFilterBank(1024)
		{
			sint32 *p = mFilterBank.data();
			GenerateTable(p, VDResamplerCubicFilter(1.0, A));
			SwizzleTable(p, 512);
		}

		int GetWindowSize() const {return 4;}
		void Process(void *dst0, const void *src0, uint32 w, uint32 u, uint32 dudx) {
			vdasm_resize_ccint_row_MMX(dst0, src0, w, u, dudx, mFilterBank.data());
		}

	protected:
		vdblock<sint32, vdaligned_alloc<sint32> > mFilterBank;
	};

	class VDResamplerSeparableCubicColStageMMX : public IVDResamplerSeparableColStage {
	public:
		VDResamplerSeparableCubicColStageMMX(double A)
			: mFilterBank(1024)
		{
			sint32 *p = mFilterBank.data();
			GenerateTable(p, VDResamplerCubicFilter(1.0, A));
			SwizzleTable(p, 512);
		}

		int GetWindowSize() const {return 4;}
		void Process(void *dst0, const void *const *srcarray, uint32 w, sint32 phase) {
			vdasm_resize_ccint_col_MMX(dst0, srcarray[0], srcarray[1], srcarray[2], srcarray[3], w, mFilterBank.data() + ((phase>>6)&0x3fc));
		}

	protected:
		vdblock<sint32, vdaligned_alloc<sint32> > mFilterBank;
	};

	class VDResamplerSeparableTableRowStageMMX : public VDResamplerSeparableTableRowStage {
	public:
		VDResamplerSeparableTableRowStageMMX(const IVDResamplerFilter& filter)
			: VDResamplerSeparableTableRowStage(filter)
		{
			SwizzleTable(mFilterBank.data(), mFilterBank.size() >> 1);
		}

		void Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) {
			vdasm_resize_table_row_MMX((uint32 *)dst, (const uint32 *)src, (const int *)mFilterBank.data(), mFilterBank.size() >> 8, w, u, dudx);
		}
	};

	class VDResamplerSeparableTableColStageMMX : public VDResamplerSeparableTableColStage {
	public:
		VDResamplerSeparableTableColStageMMX(const IVDResamplerFilter& filter)
			: VDResamplerSeparableTableColStage(filter)
		{
			SwizzleTable(mFilterBank.data(), mFilterBank.size() >> 1);
		}

		void Process(void *dst, const void *const *src, uint32 w, sint32 phase) {
			vdasm_resize_table_col_MMX((uint32*)dst, (const uint32 *const *)src, (const int *)mFilterBank.data(), mFilterBank.size() >> 8, w, (phase >> 8) & 0xff);
		}
	};
#endif

///////////////////////////////////////////////////////////////////////////
//
// resampler stages (SSE2, x86)
//
///////////////////////////////////////////////////////////////////////////

#ifndef _M_AMD64
	extern "C" long vdasm_resize_table_col_SSE2(uint32 *out, const uint32 *const*in_table, const int *filter, int filter_width, uint32 w, long frac);
	extern "C" long vdasm_resize_table_row_SSE2(uint32 *out, const uint32 *in, const int *filter, int filter_width, uint32 w, long accum, long frac);
	extern "C" void vdasm_resize_ccint_col_SSE2(void *dst, const void *src1, const void *src2, const void *src3, const void *src4, uint32 count, const void *tbl);

	class VDResamplerSeparableCubicColStageSSE2 : public VDResamplerSeparableCubicColStageMMX {
	public:
		VDResamplerSeparableCubicColStageSSE2(double A)
			: VDResamplerSeparableCubicColStageMMX(A)
		{
		}

		void Process(void *dst0, const void *const *srcarray, uint32 w, sint32 phase) {
			vdasm_resize_ccint_col_SSE2(dst0, srcarray[0], srcarray[1], srcarray[2], srcarray[3], w, mFilterBank.data() + ((phase>>6)&0x3fc));
		}
	};

	class VDResamplerSeparableTableRowStageSSE2 : public VDResamplerSeparableTableRowStageMMX {
	public:
		VDResamplerSeparableTableRowStageSSE2(const IVDResamplerFilter& filter)
			: VDResamplerSeparableTableRowStageMMX(filter)
		{
		}

		void Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) {
			vdasm_resize_table_row_MMX((uint32 *)dst, (const uint32 *)src, (const int *)mFilterBank.data(), mFilterBank.size() >> 8, w, u, dudx);
		}
	};

	class VDResamplerSeparableTableColStageSSE2 : public VDResamplerSeparableTableColStageMMX {
	public:
		VDResamplerSeparableTableColStageSSE2(const IVDResamplerFilter& filter)
			: VDResamplerSeparableTableColStageMMX(filter)
		{
		}

		void Process(void *dst, const void *const *src, uint32 w, sint32 phase) {
			vdasm_resize_table_col_SSE2((uint32*)dst, (const uint32 *const *)src, (const int *)mFilterBank.data(), mFilterBank.size() >> 8, w, (phase >> 8) & 0xff);
		}
	};
#endif

///////////////////////////////////////////////////////////////////////////
//
// resampler stages (SSE2, AMD64)
//
///////////////////////////////////////////////////////////////////////////

#ifdef _M_AMD64
	extern "C" long vdasm_resize_table_col_SSE2(uint32 *out, const uint32 *const*in_table, const int *filter, int filter_width, uint32 w);
	extern "C" long vdasm_resize_table_row_SSE2(uint32 *out, const uint32 *in, const int *filter, int filter_width, uint32 w, long accum, long frac);

	class VDResamplerSeparableTableRowStageSSE2 : public VDResamplerSeparableTableRowStage {
	public:
		VDResamplerSeparableTableRowStageSSE2(const IVDResamplerFilter& filter)
			: VDResamplerSeparableTableRowStage(filter)
		{
			SwizzleTable(mFilterBank.data(), mFilterBank.size() >> 1);
		}

		void Process(void *dst, const void *src, uint32 w, uint32 u, uint32 dudx) {
			vdasm_resize_table_row_SSE2((uint32 *)dst, (const uint32 *)src, (const int *)mFilterBank.data(), mFilterBank.size() >> 8, w, u, dudx);
		}
	};

	class VDResamplerSeparableTableColStageSSE2 : public VDResamplerSeparableTableColStage {
	public:
		VDResamplerSeparableTableColStageSSE2(const IVDResamplerFilter& filter)
			: VDResamplerSeparableTableColStage(filter)
		{
			SwizzleTable(mFilterBank.data(), mFilterBank.size() >> 1);
		}

		void Process(void *dst, const void *const *src, uint32 w, sint32 phase) {
			const unsigned filtSize = mFilterBank.size() >> 8;

			vdasm_resize_table_col_SSE2((uint32*)dst, (const uint32 *const *)src, (const int *)mFilterBank.data() + filtSize*((phase >> 8) & 0xff), filtSize, w);
		}
	};
#endif


///////////////////////////////////////////////////////////////////////////
//
// the resampler (finally)
//
///////////////////////////////////////////////////////////////////////////

class VDPixmapResampler : public IVDPixmapResampler {
public:
	VDPixmapResampler();
	~VDPixmapResampler();

	void SetSplineFactor(double A) { mSplineFactor = A; }
	bool Init(double dw, double dh, int dstformat, double sw, double sh, int srcformat, FilterMode hfilter, FilterMode vfilter, bool bInterpolationOnly);
	void Shutdown();

	void Process(const VDPixmap& dst, double dx1, double dy1, double dx2, double dy2, const VDPixmap& src, double sx, double sy);

protected:
	IVDResamplerSeparableRowStage *CreateRowStage(IVDResamplerFilter&);
	IVDResamplerSeparableColStage *CreateColStage(IVDResamplerFilter&);

	IVDResamplerStage	*mpRoot;
	double				mSplineFactor;
	VDResamplerInfo		mInfo;
	VDSteppedAllocator	mStageAllocator;
};

IVDPixmapResampler *VDCreatePixmapResampler() { return new VDPixmapResampler; }

VDPixmapResampler::VDPixmapResampler()
	: mpRoot(NULL)
	, mSplineFactor(-0.6)
{
}

VDPixmapResampler::~VDPixmapResampler() {
	Shutdown();
}

bool VDPixmapResampler::Init(double dw, double dh, int dstformat, double sw, double sh, int srcformat, FilterMode hfilter, FilterMode vfilter, bool bInterpolationOnly) {
	Shutdown();

	if (dstformat != srcformat || srcformat != nsVDPixmap::kPixFormat_XRGB8888)
		return false;

	// kill flips
	dw = fabs(dw);
	dh = fabs(dh);
	sw = fabs(sw);
	sh = fabs(sh);

	// compute gradients, using truncation toward zero
	unsigned fpucwsave = _controlfp(0, 0);
	_controlfp(_RC_CHOP, _MCW_RC);

	sint32 dudx = (sint32)((sw * 65536.0) / dw);
	sint32 dvdy = (sint32)((sh * 65536.0) / dh);

	_controlfp(fpucwsave, _MCW_RC);

	mInfo.mXAxis.Init(dudx);
	mInfo.mYAxis.Init(dvdy);

	// construct stages
	double x_2fc = 1.0;
	double y_2fc = 1.0;

	if (!bInterpolationOnly && dw < sw)
		x_2fc = dw/sw;
	if (!bInterpolationOnly && dh < sh)
		y_2fc = dh/sh;

	IVDResamplerSeparableRowStage *pRowStage = NULL;
	IVDResamplerSeparableColStage *pColStage = NULL;

	if (hfilter == kFilterPoint) {
#ifndef _M_AMD64
		long flags = CPUGetEnabledExtensions();

		if (flags & CPUF_SUPPORTS_MMX)
			pRowStage = new(mStageAllocator) VDResamplerSeparablePointRowStageMMX;
		else
			pRowStage = new(mStageAllocator) VDResamplerSeparablePointRowStageX86;
#else		
		pRowStage = new(mStageAllocator) VDResamplerSeparablePointRowStage;
#endif
	} else if (hfilter == kFilterLinear) {
		if (x_2fc >= 1.0) {
#ifndef _M_AMD64
			long flags = CPUGetEnabledExtensions();

			if (flags & CPUF_SUPPORTS_MMX)
				pRowStage = new(mStageAllocator) VDResamplerSeparableLinearRowStageMMX;
			else
#endif
				pRowStage = new(mStageAllocator) VDResamplerSeparableLinearRowStage;
		} else
			pRowStage = CreateRowStage(VDResamplerLinearFilter(x_2fc));
	} else if (hfilter == kFilterCubic) {
#ifndef _M_AMD64
		long flags = CPUGetEnabledExtensions();

		if (x_2fc >= 1.0 && (flags & CPUF_SUPPORTS_MMX))
			pRowStage = new(mStageAllocator) VDResamplerSeparableCubicRowStageMMX(mSplineFactor);
		else
#endif
			pRowStage = CreateRowStage(VDResamplerCubicFilter(x_2fc, mSplineFactor));
	} else if (hfilter == kFilterLanczos3)
		pRowStage = CreateRowStage(VDResamplerLanczos3Filter(x_2fc));

	if (hfilter == kFilterLinear) {
		if (y_2fc >= 1.0) {
#ifndef _M_AMD64
			long flags = CPUGetEnabledExtensions();

			if (flags & CPUF_SUPPORTS_MMX)
				pColStage = new(mStageAllocator) VDResamplerSeparableLinearColStageMMX;
			else
#endif
				pColStage = new(mStageAllocator) VDResamplerSeparableLinearColStage;
		} else
			pColStage = CreateColStage(VDResamplerLinearFilter(y_2fc));
	} else if (hfilter == kFilterCubic) {
#ifndef _M_AMD64
		long flags = CPUGetEnabledExtensions();

		if (y_2fc >= 1.0 && (flags & CPUF_SUPPORTS_MMX)) {
			if (flags & CPUF_SUPPORTS_SSE2)
				pColStage = new(mStageAllocator) VDResamplerSeparableCubicColStageSSE2(mSplineFactor);
			else
				pColStage = new(mStageAllocator) VDResamplerSeparableCubicColStageMMX(mSplineFactor);
		} else
#endif
			pColStage = CreateColStage(VDResamplerCubicFilter(y_2fc, mSplineFactor));
	} else if (hfilter == kFilterLanczos3)
		pColStage = CreateColStage(VDResamplerLanczos3Filter(y_2fc));

	mpRoot = new(mStageAllocator) VDResamplerSeparableStage(
					pRowStage,
					pColStage
					);

	return true;
}

IVDResamplerSeparableRowStage *VDPixmapResampler::CreateRowStage(IVDResamplerFilter& filter) {
#ifndef _M_AMD64
	long flags = CPUGetEnabledExtensions();

	if (flags & CPUF_SUPPORTS_SSE2)
		return new(mStageAllocator) VDResamplerSeparableTableRowStageSSE2(filter);
	else if (flags & CPUF_SUPPORTS_MMX)
		return new(mStageAllocator) VDResamplerSeparableTableRowStageMMX(filter);
	else
		return new(mStageAllocator) VDResamplerSeparableTableRowStage(filter);
#else
	return new(mStageAllocator) VDResamplerSeparableTableRowStageSSE2(filter);
#endif
}

IVDResamplerSeparableColStage *VDPixmapResampler::CreateColStage(IVDResamplerFilter& filter) {
#ifndef _M_AMD64
	long flags = CPUGetEnabledExtensions();

	if (flags & CPUF_SUPPORTS_SSE2)
		return new(mStageAllocator) VDResamplerSeparableTableColStageSSE2(filter);
	else if (flags & CPUF_SUPPORTS_MMX)
		return new(mStageAllocator) VDResamplerSeparableTableColStageMMX(filter);
	else
		return new(mStageAllocator) VDResamplerSeparableTableColStage(filter);
#else
	return new(mStageAllocator) VDResamplerSeparableTableColStageSSE2(filter);
#endif
}

void VDPixmapResampler::Shutdown() {
	if (mpRoot) {
		mpRoot->~IVDResamplerStage();
		mpRoot = NULL;
	}

	mStageAllocator.clear();
}

void VDPixmapResampler::Process(const VDPixmap& dst, double dx1, double dy1, double dx2, double dy2, const VDPixmap& src, double sx1, double sy1) {
	if (!mpRoot)
		return;

	// convert coordinates to fixed point
	sint32 fdx1 = (sint32)(dx1 * 65536.0);
	sint32 fdy1 = (sint32)(dy1 * 65536.0);
	sint32 fdx2 = (sint32)(dx2 * 65536.0);
	sint32 fdy2 = (sint32)(dy2 * 65536.0);
	sint32 fsx1 = (sint32)(sx1 * 65536.0);
	sint32 fsy1 = (sint32)(sy1 * 65536.0);

	// determine integer destination rectangle (OpenGL coordinates)
	sint32 idx1 = (fdx1 + 0x7fff) >> 16;
	sint32 idy1 = (fdy1 + 0x7fff) >> 16;
	sint32 idx2 = (fdx2 + 0x7fff) >> 16;
	sint32 idy2 = (fdy2 + 0x7fff) >> 16;

	// convert destination flips to source flips
	if (idx1 > idx2) {
		std::swap(idx1, idx2);
		sx1 += scale32x32_fp16(mInfo.mXAxis.dudx, idx2 - idx1);
	}

	if (idy1 > idy2) {
		std::swap(idy1, idy2);
		sy1 += scale32x32_fp16(mInfo.mYAxis.dudx, idy2 - idy1);
	}

	// clip destination rect
	if (idx1 < 0)
		idx1 = 0;
	if (idy1 < 0)
		idy1 = 0;
	if (idx2 > dst.w)
		idx2 = dst.w;
	if (idy2 > dst.h)
		idy2 = dst.h;

	// check for degenerate conditions
	if (idx1 >= idx2 || idy1 >= idy2)
		return;

	// render
	const sint32 winw = mpRoot->GetHorizWindowSize();
	const sint32 winh = mpRoot->GetVertWindowSize();

	fsx1 -= (winw-1)<<15;
	fsy1 -= (winh-1)<<15;

	mInfo.mXAxis.Compute(idx1, idx2, fdx1, fsx1, src.w, winw);
	mInfo.mYAxis.Compute(idy1, idy2, fdy1, fsy1, src.h, winh);

	mInfo.mpSrc = src.data;
	mInfo.mSrcPitch = src.pitch;
	mInfo.mSrcW = src.w;
	mInfo.mSrcH = src.h;
	mInfo.mpDst = vdptroffset(dst.data, dst.pitch * idy1 + idx1*4);
	mInfo.mDstPitch = dst.pitch;

	if (mpRoot) {
		mpRoot->Process(mInfo);

		VDCPUCleanupExtensions();
	}
}

bool VDPixmapResample(const VDPixmap& dst, const VDPixmap& src, IVDPixmapResampler::FilterMode filter) {
	VDPixmapResampler r;

	if (!r.Init(dst.w, dst.h, dst.format, src.w, src.h, src.format, filter, filter, false))
		return false;

	r.Process(dst, 0, 0, dst.w, dst.h, src, 0, 0);
	return true;
}
