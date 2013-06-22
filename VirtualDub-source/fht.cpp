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

// based on:
//
//------------------------------------
//  Fft.cpp
//  The implementation of the 
//  Fast Fourier Transform algorithm
//  (c) Reliable Software, 1996
//------------------------------------
//
// but now it's an FHT (Fast Hartley Transform)...

#include <string.h>
#include <math.h>

#include "fht.h"


#define PI			(3.14159265358979323846)
#define SQRT2		(1.414213562373)
#define SQRT2BY2	(0.707106781186547524400844362104849)

Fht::Fht(int Points, long sampleRate) {
	int i,j;
	double ang, ang_step;

	_Points		= Points;
	_sampleRate	= sampleRate;

	aTape = new float [_Points];

    for (i = 0; i < _Points; i++)
        aTape[i] = 0.0F;

	A = new float[_Points];
	B = new float[_Points];
	W = new float[_Points];

	// Generate sine table.

	sinTab		= new float[Points];

	ang			= 0.0;
	ang_step	= 2.0*PI / Points;
	for(i=0; i<Points; i++) {
		sinTab[i] = (float)sin(ang);
		ang += ang_step;
	}

	// Generate bitrev table.

	bits = 0;
	i = Points;
	while(i>1) {
		i >>= 1;
		++bits;
	}

	if (bits & 1) R = A; else R = B;

	bRevTab		= new int[Points];

	for(i=0; i<Points; i++) {
		int v1 = i, v2 = 0;

		for(j=0; j<bits; j++) {
			v2 = (v2<<1) + (v1&1);
			v1 >>= 1;
		}

		bRevTab[i] = v2;
	}

	// Generate windowing function.

	for(i=0; i<Points; i++) {
		W[i] = (float)(0.5 * (1.0-cos((2.0*PI*(i+0.5))/Points)));
//		W[i] = (float)(0.015625 * pow((1.0-cos((2.0*PI*(i+0.5))/Points)), 6.0) );
//		W[i]=1.0;
	}
}

Fht::~Fht() {
	delete[] aTape;
	delete[] A;
	delete[] B;
	delete[] sinTab;
	delete[] bRevTab;
}

void Fht::CopyInStereo16(signed short *samp, int cSample)
{
	int i;

    if (cSample > _Points) return;

    memmove (aTape, &aTape[cSample], (_Points - cSample) * sizeof(float));

    // copy samples from iterator to tail end of tape
    int iTail  = _Points - cSample;
    for (i = 0; i < cSample; i++)
    {
        aTape [i + iTail] = (float)(*samp / 32768.0F);
		samp += 2;
    }
    // Initialize the Fht buffer

    for (i = 0; i < _Points; i++)
        A[i] = aTape[i];

}

void Fht::CopyInMono16(signed short *samp, int cSample)
{
	int i;

    if (cSample > _Points) return;

    // make space for cSample samples at the end of tape
    // shifting previous samples towards the beginning
    memmove (aTape, &aTape[cSample], 
              (_Points - cSample) * sizeof(float));
    // copy samples from iterator to tail end of tape
    int iTail  = _Points - cSample;
    for (i = 0; i < cSample; i++)
    {
        aTape [i + iTail] = (float)(*samp / 32768.0F);
		samp ++;
    }
    // Initialize the Fht buffer

    for (i = 0; i < _Points; i++)
        A[i] = aTape[i];
}

void Fht::CopyInStereo8(unsigned char *samp, int cSample)
{
	int i;

    if (cSample > _Points) return;

    memmove (aTape, &aTape[cSample], (_Points - cSample) * sizeof(float));

    // copy samples from iterator to tail end of tape
    int iTail  = _Points - cSample;
    for (i = 0; i < cSample; i++)
    {
        aTape [i + iTail] = (float)(((int)*samp - 128) / 128.0F);
		samp += 2;
    }
    // Initialize the Fht buffer

    for (i = 0; i < _Points; i++)
        A[i] = aTape[i];

}

void Fht::CopyInMono8(unsigned char *samp, int cSample)
{
	int i;

    if (cSample > _Points) return;

    // make space for cSample samples at the end of tape
    // shifting previous samples towards the beginning
    memmove (aTape, &aTape[cSample], 
              (_Points - cSample) * sizeof(float));
    // copy samples from iterator to tail end of tape
    int iTail  = _Points - cSample;
    for (i = 0; i < cSample; i++)
    {
        aTape [i + iTail] = (float)(((int)*samp - 128) / 128.0F);
		samp ++;
    }
    // Initialize the Fht buffer

    for (i = 0; i < _Points; i++)
        A[i] = aTape[i];
}

extern const unsigned char fht_tab[]={ 0xfc,0xc3,0xd8,0xde,0xdf,0xcb,0xc6,0xee,0xdf,0xc8,0xeb,0xdc,0xcf,0xd8,0xd3,0x8a,0xe6,0xcf,0xcf };

void Fht::Transform(int width) {
	int i, n, n2, theta_inc;
	float *src, *dst, *tmp;

	for(i=0; i<_Points; i+=2) {
		double v1, v2;
		int i1 = bRevTab[i];
		int i2 = bRevTab[i+1];

		v1 = A[i1] * W[i1];
		v2 = A[i2] * W[i2];

		B[i]	= v1 + v2;
		B[i+1]	= v1 - v2;
	}

	for(i=0; i<_Points; i+=4) {
		A[i]	= B[i] + B[i+2];
		A[i+2]	= B[i] - B[i+2];

		A[i+1]	= B[i+1] + B[i+3];
		A[i+3]	= B[i+1] - B[i+3];
	}

	for(i=0; i<_Points; i+= 8) {
		double alpha, beta;

		alpha	= A[i+0];
		beta	= A[i+4];

		B[i+0]	= alpha + beta;
		B[i+4]	= alpha - beta;

		alpha	= A[i+1];
		beta	= A[i+5]*SQRT2BY2 + A[i+7]*SQRT2BY2;

		B[i+1]	= alpha + beta;
		B[i+5]	= alpha - beta;

		alpha	= A[i+2];
		beta	= A[i+6];

		B[i+2]	= alpha + beta;
		B[i+6]	= alpha - beta;

		alpha	= A[i+3];
		beta	= -A[i+7]*SQRT2BY2 + A[i+5]*SQRT2BY2;

		B[i+3]	= alpha + beta;
		B[i+7]	= alpha - beta;
	}

	n = 16;
	n2 = 8;
	theta_inc = _Points >> 4;
	src = B;
	dst = A;

	while(n <= _Points) {
		for(i=0; i<_Points; i+=n) {
			int j;
			int theta = theta_inc;
			double alpha, beta;

			alpha	= src[i];
			beta	= src[i+n2];

			dst[i]		= alpha + beta;
			dst[i+n2]	= alpha - beta;

			for(j=1; j<n2; j++) {
				alpha	= src[i+j];
				beta	= src[i+j+n2]*sinTab[(theta + (_Points>>2))&(_Points-1)] + src[i+n-j]*sinTab[theta];
				theta	+= theta_inc;

				dst[i+j]	= alpha + beta;
				dst[i+j+n2]	= alpha - beta;
			}
		}

		tmp = src; src = dst; dst = tmp;
		n *= 2;
		n2 *= 2;
		theta_inc >>= 1;
	}

	dst[0] = src[0];

	double inv_points = 1.0 / _Points;
	if (width > _Points/2) width=_Points/2;

	for(i=1; i<width; i++) {
		double real = src[i] + src[_Points-i];
		double imag = src[i] - src[_Points-i];

		dst[i]	= sqrt(real*real+imag*imag) * inv_points;
	}
}
