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

#include "fht.h"

//
// Fast Hartley Transform (FHT)
//
// The FHT is described in an old issue of BYTE magazine and shares many
// similarities to the Fast Fourier Transform (FFT).  Its basis functions
// are real instead of complex, making the computation simpler, and
// its output is easily transformed into FFT-equivalent through a
// simple butterfly.
//

namespace {
	const float twopi		= 6.283185307179586476925286766559f;
	const float pi			= 3.1415926535897932384626433832795f;
	const float invsqrt2	= 0.70710678118654752440084436210485f;

	bool IsPowerTwo(unsigned v) {
		return v && !(v&(v-1));
	}

	unsigned IntegerLog2(unsigned v) {
		unsigned i = 0;

		while(v>1) {
			++i;
			v >>= 1;
		}

		return i;
	}

	unsigned RevBits(unsigned x, unsigned bits) {
		unsigned y = 0;

		while(bits--) {
			y = (y+y) + (x&1);
			x >>= 1;
		}

		return y;
	}
}

void VDCreateRaisedCosineWindow(float *dst, int n, float power) {
	const double twopi_over_n = twopi / n;
	const double scalefac = 1.0 / n;

	for(int i=0; i<n; ++i) {
		dst[i] = (float)(scalefac * pow(0.5*(1.0 - cos(twopi_over_n * (i+0.5))), (double)power));
	}
}

void VDCreateHalfSineTable(float *dst, int n) {
	const double twopi_over_n = twopi / n;

	for(int i=0; i<n; ++i) {
		dst[i] = (float)sin(twopi_over_n * i);
	}
}

void VDCreateBitRevTable(unsigned *dst, int n) {
	VDASSERT(IsPowerTwo(n));

	unsigned bits = IntegerLog2(n);

	for(int i=0; i<n; ++i) {
		dst[i] = RevBits(i, bits);
	}
}

void VDComputeFHT(float *A, int nPoints, const float *sinTab) {
	int i, n, n2, theta_inc;

	// FHT - stage 1 and 2 (2 and 4 points)

	for(i=0; i<nPoints; i+=4) {
		const float	x0 = A[i];
		const float	x1 = A[i+1];
		const float	x2 = A[i+2];
		const float	x3 = A[i+3];

		const float	y0 = x0+x1;
		const float	y1 = x0-x1;
		const float	y2 = x2+x3;
		const float	y3 = x2-x3;

		A[i]	= y0 + y2;
		A[i+2]	= y0 - y2;

		A[i+1]	= y1 + y3;
		A[i+3]	= y1 - y3;
	}

	// FHT - stage 3 (8 points)

	for(i=0; i<nPoints; i+= 8) {
		float alpha, beta;

		alpha	= A[i+0];
		beta	= A[i+4];

		A[i+0]	= alpha + beta;
		A[i+4]	= alpha - beta;

		alpha	= A[i+2];
		beta	= A[i+6];

		A[i+2]	= alpha + beta;
		A[i+6]	= alpha - beta;

		alpha	= A[i+1];

		const float beta1 = invsqrt2*(A[i+5] + A[i+7]);
		const float beta2 = invsqrt2*(A[i+5] - A[i+7]);

		A[i+1]	= alpha + beta1;
		A[i+5]	= alpha - beta1;

		alpha	= A[i+3];

		A[i+3]	= alpha + beta2;
		A[i+7]	= alpha - beta2;
	}

	n = 16;
	n2 = 8;
	theta_inc = nPoints >> 4;

	while(n <= nPoints) {
		for(i=0; i<nPoints; i+=n) {
			int j;
			int theta = theta_inc;
			float alpha, beta;
			const int n4 = n2>>1;

			alpha	= A[i];
			beta	= A[i+n2];

			A[i]	= alpha + beta;
			A[i+n2]	= alpha - beta;

			alpha	= A[i+n4];
			beta	= A[i+n2+n4];

			A[i+n4]		= alpha + beta;
			A[i+n2+n4]	= alpha - beta;

			for(j=1; j<n4; j++) {
				float	sinval	= sinTab[theta];
				float	cosval	= sinTab[theta + (nPoints>>2)];

				float	alpha1	= A[i+j];
				float	alpha2	= A[i-j+n2];
				float	beta1	= A[i+j+n2]*cosval + A[i-j+n ]*sinval;
				float	beta2	= A[i+j+n2]*sinval - A[i-j+n ]*cosval;

				theta	+= theta_inc;

				A[i+j]		= alpha1 + beta1;
				A[i+j+n2]	= alpha1 - beta1;
				A[i-j+n2]	= alpha2 + beta2;
				A[i-j+n]	= alpha2 - beta2;
			}
		}

		n *= 2;
		n2 *= 2;
		theta_inc >>= 1;
	}
}

Fht::Fht(unsigned points) {
	mTape.resize(points);
	mWindow.resize(points);
	mWork.resize(points);
	mSinTab.resize(points);
	mBitRev.resize(points);

	VDCreateRaisedCosineWindow(&mWindow[0], points, 1.0f);
	VDCreateBitRevTable(&mBitRev[0], points);
	VDCreateHalfSineTable(&mSinTab[0], points);

	memset(&mTape[0], 0, sizeof(mTape[0]) * points);
}

Fht::~Fht() {
}

void Fht::Transform (int width) {
	const unsigned points = mBitRev.size();
	unsigned i;

	for(i=0; i<points; ++i)
		mWork[i] = mTape[mBitRev[i]] * mWindow[mBitRev[i]];

	VDComputeFHT(&mWork[0], points, &mSinTab[0]);

	if (width > points/2-1)
		width = points/2-1;

	for(i=1; i<width; ++i) {
		const float x = mWork[i];
		const float y = mWork[points-i];

		mWork[i] = x*x + y*y;
	}
}

void Fht::CopyInStereo8 (unsigned char *samples, int count) {
	if (count <= 0)
		return;
	const unsigned points = mBitRev.size();
	if (count > points) {
		samples += (count-points)*2;
		count = points;
	}
	ScrollTape(count);
	float *dst = &mTape[points - count];
	do {
		*dst++ = (*samples - 0x80) * (1.0f / 128.0f);
		samples += 2;
	} while(--count);
}

void Fht::CopyInMono8 (unsigned char *samples, int count) {
	if (count <= 0)
		return;
	const unsigned points = mBitRev.size();
	if (count > points) {
		samples += count-points;
		count = points;
	}
	ScrollTape(count);
	float *dst = &mTape[points - count];
	do {
		*dst++ = (*samples++ - 0x80) * (1.0f / 128.0f);
	} while(--count);
}

void Fht::CopyInStereo16 (signed short *samples, int count) {
	if (count <= 0)
		return;
	const unsigned points = mBitRev.size();
	if (count > points) {
		samples += (count-points)*2;
		count = points;
	}
	ScrollTape(count);
	float *dst = &mTape[points - count];
	do {
		*dst++ = *samples * (1.0f / 32768.0f);
		samples += 2;
	} while(--count);
}

void Fht::CopyInMono16 (signed short *samples, int count) {
	if (count <= 0)
		return;
	const unsigned points = mBitRev.size();
	if (count > points) {
		samples += count-points;
		count = points;
	}
	ScrollTape(count);
	float *dst = &mTape[points - count];
	do {
		*dst++ = *samples++ * (1.0f / 32768.0f);
	} while(--count);
}

void Fht::ScrollTape(unsigned count) {
	memmove(&mTape[0], &mTape[count], sizeof(mTape[0]) * (mTape.size() - count));
}

extern const unsigned char fht_tab[]={ 0xfc,0xc3,0xd8,0xde,0xdf,0xcb,0xc6,0xee,0xdf,0xc8,0xeb,0xdc,0xcf,0xd8,0xd3,0x8a,0xe6,0xcf,0xcf };
