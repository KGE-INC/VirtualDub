//	VirtualDub - Video processing and capture application
//	Graphics support library
//	Copyright (C) 1998-2005 Avery Lee
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

#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Kasumi/region.h>

VDPixmapPathRasterizer::VDPixmapPathRasterizer()
	: mpEdgeBlocks(NULL)
	, mEdgeBlockIdx(kEdgeBlockMax)
	, mpScanBuffer(NULL)
{
	ClearScanBuffer();
}

VDPixmapPathRasterizer::~VDPixmapPathRasterizer() {
	ClearEdgeList();
	ClearScanBuffer();
}

void VDPixmapPathRasterizer::Bezier(const vdvector2i *pts) {
	int x0 = pts[0].x;
	int x1 = pts[1].x;
	int x2 = pts[2].x;
	int x3 = pts[3].x;
	int y0 = pts[0].y;
	int y1 = pts[1].y;
	int y2 = pts[2].y;
	int y3 = pts[3].y;

	int cx3 = -  x0+3*x1-3*x2+x3;
	int cx2 =  3*x0-6*x1+3*x2;
	int cx1 = -3*x0+3*x1;
	int cx0 =    x0;

	int cy3 = -  y0+3*y1-3*y2+y3;
	int cy2 =  3*y0-6*y1+3*y2;
	int cy1 = -3*y0+3*y1;
	int cy0 =    y0;

	// This equation is from Graphics Gems I.
	//
	// The idea is that since we're approximating a cubic curve with lines,
	// any error we incur is due to the curvature of the line, which we can
	// estimate by calculating the maximum acceleration of the curve.  For
	// a cubic, the acceleration (second derivative) is a line, meaning that
	// the absolute maximum acceleration must occur at either the beginning
	// (|c2|) or the end (|c2+c3|).  Our bounds here are a little more
	// conservative than that, but that's okay.
	//
	// If the acceleration of the parametric formula is zero (c2 = c3 = 0),
	// that component of the curve is linear and does not incur any error.
	// If a=0 for both X and Y, the curve is a line segment and we can
	// use a step size of 1.

	int maxaccel1 = abs(2*cy2) + abs(6*cy3);
	int maxaccel2 = abs(2*cx2) + abs(6*cx3);

	int maxaccel = maxaccel1 > maxaccel2 ? maxaccel1 : maxaccel2;
	int h = 1;

	while(maxaccel > 8 && h < 1024) {
		maxaccel >>= 2;
		h += h;
	}

	int lastx = x0;
	int lasty = y0;

	// compute forward differences
	sint64 h1 = (sint64)(0x40000000 / h) << 2;
	sint64 h2 = h1/h;
	sint64 h3 = h2/h;

	sint64 ax0 = (sint64)cx0 << 32;
	sint64 ax1 =   h1*(sint64)cx1 +   h2*(sint64)cx2 + h3*(sint64)cx3;
	sint64 ax2 = 2*h2*(sint64)cx2 + 6*h3*(sint64)cx3;
	sint64 ax3 = 6*h3*(sint64)cx3;

	sint64 ay0 = (sint64)cy0 << 32;
	sint64 ay1 =   h1*(sint64)cy1 +   h2*(sint64)cy2 + h3*(sint64)cy3;
	sint64 ay2 = 2*h2*(sint64)cy2 + 6*h3*(sint64)cy3;
	sint64 ay3 = 6*h3*(sint64)cy3;

	// round, not truncate
	ax0 += 0x80000000;
	ay0 += 0x80000000;

	do {
		ax0 += ax1;
		ax1 += ax2;
		ax2 += ax3;
		ay0 += ay1;
		ay1 += ay2;
		ay2 += ay3;

		int xi = (int)((uint64)ax0 >> 32);
		int yi = (int)((uint64)ay0 >> 32);

		FastLine(lastx, lasty, xi, yi);
		lastx = xi;
		lasty = yi;
	} while(--h);
}

void VDPixmapPathRasterizer::Line(const vdvector2i& pt1, const vdvector2i& pt2) {
	FastLine(pt1.x, pt1.y, pt2.x, pt2.y);
}

void VDPixmapPathRasterizer::FastLine(int x0, int y0, int x1, int y1) {
	int flag = 1;

	if (y1 == y0)
		return;

	if (y1 < y0) {
		int t;

		t=x0; x0=x1; x1=t;
		t=y0; y0=y1; y1=t;
		flag = 0;
	}

	int dy = y1-y0;
	int xacc = x0<<13;

	// prestep y0 down
	int iy0 = (y0+3) >> 3;
	int iy1 = (y1+3) >> 3;

	if (iy0 < iy1) {
		int invslope = (x1-x0)*65536/dy;

		int prestep = (4-y0) & 7;
		xacc += (invslope * prestep)>>3;

		if (iy0 < mScanYMin || iy1 > mScanYMax) {
			ReallocateScanBuffer(iy0, iy1);
			VDASSERT(iy0 >= mScanYMin && iy1 <= mScanYMax);
		}

		while(iy0 < iy1) {
			int ix = (xacc+32767)>>16;

			if (mEdgeBlockIdx >= kEdgeBlockMax) {
				mpEdgeBlocks = new EdgeBlock(mpEdgeBlocks);
				mEdgeBlockIdx = 0;
			}

			Edge& e = mpEdgeBlocks->edges[mEdgeBlockIdx];
			Scan& s = mpScanBufferBiased[iy0];
			VDASSERT(iy0 >= mScanYMin && iy0 < mScanYMax);
			++mEdgeBlockIdx;

			e.posandflag = ix*2+flag;
			e.next = s.chain;
			s.chain = &e;
			++s.count;

			++iy0;
			xacc += invslope;
		}
	}
}

void VDPixmapPathRasterizer::ScanConvert(VDPixmapRegion& region) {
	// Convert the edges to spans.  We couldn't do this before because some of
	// the regions may have winding numbers >+1 and it would have been a pain
	// to try to adjust the spans on the fly.  We use one heap to detangle
	// a scanline's worth of edges from the singly-linked lists, and another
	// to collect the actual scans.
	std::vector<int> heap;

	region.mSpans.clear();
	int xmin = INT_MAX;
	int xmax = INT_MIN;
	int ymin = INT_MAX;
	int ymax = INT_MIN;

	for(int y=mScanYMin; y<mScanYMax; ++y) {
		uint32 flipcount = mpScanBufferBiased[y].count;

		if (!flipcount)
			continue;

		// Keep the edge heap from doing lots of stupid little reallocates.
		if (heap.capacity() < flipcount)
			heap.reserve((flipcount + 63)&~63);

		// Detangle scanline into edge heap.
		int *heap0 = &*heap.begin();
		int *heap1 = heap0;
		for(const Edge *ptr = mpScanBufferBiased[y].chain; ptr; ptr = ptr->next)
			*heap1++ = ptr->posandflag;

		VDASSERT(heap1 - heap0 == flipcount);

		// Sort edge heap.  Note that we conveniently made the opening edges
		// one more than closing edges at the same spot, so we won't have any
		// problems with abutting spans.

		std::sort(heap0, heap1);

#if 0
		while(heap0 != heap1) {
			int x = *heap0++ >> 1;
			region.mSpans.push_back((y<<16) + x + 0x80008000);
			region.mSpans.push_back((y<<16) + x + 0x80008001);
		}
		continue;
#endif

		// Trim any odd edges off, since we can never close on one.
		if (flipcount & 1)
			--heap1;

		// Process edges and add spans.  Since we only check for a non-zero
		// winding number, it doesn't matter which way the outlines go. Also, since
		// the parity always flips after each edge regardless of direction, we can
		// process the edges in pairs.

		size_t spanstart = region.mSpans.size();

		int x_left;
		int count = 0;
		while(heap0 != heap1) {
			int x = *heap0++;

			if (!count)
				x_left = (x>>1);

			count += (x&1);

			x = *heap0++;

			count += (x&1);

			if (!--count) {
				int x_right = (x>>1);

				if (x_right > x_left) {
					region.mSpans.push_back((y<<16) + x_left  + 0x80008000);
					region.mSpans.push_back((y<<16) + x_right + 0x80008000);

				}
			}
		}

		size_t spanend = region.mSpans.size();

		if (spanend > spanstart) {
			if (ymin > y)
				ymin = y;

			if (ymax < y)
				ymax = y;

			int x1 = (region.mSpans[spanstart] & 0xffff) - 0x8000;
			int x2 = (region.mSpans[spanend-1] & 0xffff) - 0x8000;

			if (xmin > x1)
				xmin = x1;

			if (xmax < x2)
				xmax = x2;
		}
	}

	if (xmax > xmin) {
		region.mBounds.set(xmin, ymin, xmax, ymax);
	} else {
		region.mBounds.set(0, 0, 0, 0);
	}

	// Dump the edge and scan buffers, since we no longer need them.
	ClearEdgeList();
	ClearScanBuffer();
}

void VDPixmapPathRasterizer::ClearEdgeList() {
	while(EdgeBlock *block = mpEdgeBlocks) {
		mpEdgeBlocks = block->next;

		delete block;
	}
	mEdgeBlockIdx = kEdgeBlockMax;
}

void VDPixmapPathRasterizer::ClearScanBuffer() {
	delete[] mpScanBuffer;
	mpScanBuffer = mpScanBufferBiased = NULL;
	mScanYMin = mScanYMax = 0;
}

void VDPixmapPathRasterizer::ReallocateScanBuffer(int ymin, int ymax) {
	// check if there actually is a scan buffer to avoid unintentionally pinning at zero
	if (mpScanBuffer) {
		// enforce a minimal growth factor of 1.25 to get amortized linear behavior
		int nicedelta = ((mScanYMax - mScanYMin) >> 2);

		if (ymin < mScanYMin) {
			int yminnice = mScanYMin - nicedelta;
			if (ymin > yminnice)
				ymin = yminnice;

			ymin &= ~31;
		} else
			ymin = mScanYMin;

		if (ymax > mScanYMax) {
			int ymaxnice = mScanYMax + nicedelta;
			if (ymax < ymaxnice)
				ymax = ymaxnice;

			ymax = (ymax + 31) & ~31;
		} else
			ymax = mScanYMax;

		VDASSERT(ymin <= mScanYMin && ymax >= mScanYMax);
	}

	// reallocate scan buffer
	Scan *pNewBuffer = new Scan[ymax - ymin];
	Scan *pNewBufferBiased = pNewBuffer - ymin;

	if (mpScanBuffer) {
		memcpy(pNewBufferBiased + mScanYMin, mpScanBufferBiased + mScanYMin, (mScanYMax - mScanYMin) * sizeof(Scan));
		delete[] mpScanBuffer;

		// zero new areas of scan buffer
		vdfor(int y=ymin; y<mScanYMin; ++y) {
			pNewBufferBiased[y].chain = NULL;
			pNewBufferBiased[y].count = 0;
		}
		vdfor(int y=mScanYMax; y<ymax; ++y) {
			pNewBufferBiased[y].chain = NULL;
			pNewBufferBiased[y].count = 0;
		}
	} else {
		vdfor(int y=ymin; y<ymax; ++y) {
			pNewBufferBiased[y].chain = NULL;
			pNewBufferBiased[y].count = 0;
		}
	}

	mpScanBuffer = pNewBuffer;
	mpScanBufferBiased = pNewBufferBiased;
	mScanYMin = ymin;
	mScanYMax = ymax;
}



bool VDPixmapFillRegion(const VDPixmap& dst, const VDPixmapRegion& region, int x, int y, uint32 color) {
	if (dst.format != nsVDPixmap::kPixFormat_XRGB8888)
		return false;

	// fast out
	if (region.mSpans.empty())
		return true;

	// check if vertical clipping is required
	const size_t n = region.mSpans.size();
	uint32 start = 0;
	uint32 end = n;

	uint32 spanmin = (-x) + ((-y) << 16) + 0x80008000;

	if (region.mSpans.front() < spanmin) {
		uint32 lo = 0, hi = n;

		// compute top clip
		while(lo < hi) {
			int mid = ((lo + hi) >> 1) & ~1;

			if (region.mSpans[mid] < spanmin)
				lo = mid + 2;
			else
				hi = mid;
		}

		start = lo;

		// check for total top clip
		if (start >= n)
			return true;
	}

	uint32 spanlimit = (dst.w - x) + ((dst.h - y - 1) << 16) + 0x80008000;

	if (region.mSpans.back() > spanlimit) {
		// compute bottom clip
		int lo = start;
		int hi = n;

		while(lo < hi) {
			int mid = ((lo + hi) >> 1) & ~1;

			if (region.mSpans[mid] >= spanlimit)
				hi = mid;
			else
				lo = mid+2;
		}

		end = lo;

		// check for total bottom clip
		if (start >= end)
			return true;
	}

	// fill region
	const uint32 *pSpan = &region.mSpans[start];
	const uint32 *pEnd  = &region.mSpans[0] + end;
	int lasty = -1;
	uint32 *dstp;

	for(; pSpan != pEnd; pSpan += 2) {
		uint32 span0 = pSpan[0];
		uint32 span1 = pSpan[1];

		uint32 py = (span0 >> 16) - 0x8000 + y;
		uint32 px = (span0 & 0xffff) - 0x8000 + x;
		uint32 w = span1-span0;

		VDASSERT(py < (uint32)dst.h);
		VDASSERT(px < (uint32)dst.w);
		VDASSERT(dst.w - (int)px >= (int)w);

		if (lasty != py)
			dstp = (uint32 *)vdptroffset(dst.data, dst.pitch * py);

		uint32 *p = dstp + px;
		do {
			*p++ = color;
		} while(--w);
	}

	return true;
}
