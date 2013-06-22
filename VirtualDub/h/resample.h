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

#ifndef f_RESAMPLE_H
#define f_RESAMPLE_H

#include "VBitmap.h"

void MakeCubic4Table(int *table, double A, bool mmx_table) throw();
const int *GetStandardCubic4Table() throw();

class ResampleInfo {
public:
	union {
		__int64 v;
		struct {
			unsigned long lo;
			long hi;
		};
	} dudx_int, u0_int;

	long x1_int, dx_int;

	struct ResampleBounds {
		long precopy, preclip, postclip, postcopy, unclipped, allclip;
		long preclip2, postclip2;	// cubic4 only
	} clip;

	bool init(double x, double dx, double u, double du, unsigned long xlimit, unsigned long ulimit, int kw, bool bMapCorners, bool bClip4);

protected:
	void computeBounds(__int64 u, __int64 dudx, unsigned int dx, unsigned int kernel, unsigned long limit);
	void computeBounds4(__int64 u, __int64 dudx, unsigned int dx, unsigned long limit);
};

class Resampler {
public:
	enum eFilter{
		kPoint			= 0,
		kLinearInterp	= 1,
		kCubicInterp	= 2,
		kCubicInterp060	= 7,
		kLinearDecimate	= 3,
		kCubicDecimate075	= 4,
		kCubicDecimate060	= 5,
		kCubicDecimate100	= 6,
		kLanczos3		= 8
	};

	Resampler();
	~Resampler();
	void Init(eFilter horiz_filt, eFilter vert_filt, double dx, double dy, double sx, double sy);
	void Free();
	bool Process(const VBitmap *dst, double _x2, double _y2, const VBitmap *src, double _x1, double _y1, bool bMapCorners) throw();

protected:
	ResampleInfo horiz, vert;
	int *xtable, *ytable;
	int xfiltwidth, yfiltwidth;
	double dstw, dsth, srcw, srch;
	double ubias, vbias;

	int rowpitch;
	Pixel32 *rowmem, *rowmemalloc, **rows;
	int rowcount;

	eFilter	nHorizFilt, nVertFilt;
	
	void _DoRow(Pixel32 *dstp, const Pixel32 *srcp, long srcw);
	static int *_CreateLinearDecimateTable(double dx, double sx, int& filtwidth);
	static int *_CreateCubicDecimateTable(double dx, double sx, int& filtwidth, double A);
	static int *_CreateLanczos3DecimateTable(double dx, double sx, int& filtwidth);
};

#endif
