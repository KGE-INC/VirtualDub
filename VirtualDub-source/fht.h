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

#ifndef f_FHT_H
#define f_FHT_H

// based on:
//
//------------------------------------
//  fft.cpp
//  The implementation of the 
//  Fast Fourier Transform algorithm
//  (c) Reliable Software, 1996
//------------------------------------
//
// but now it's an FHT (Fast Hartley Transform)...

class Fht
{
public:
    Fht (int Points, long sampleRate);
    ~Fht ();
    int     Points () const { return _Points; }
    void    Transform (int width);
    void    CopyInStereo8 (unsigned char *samples, int count);
    void    CopyInMono8 (unsigned char *samples, int count);
    void    CopyInStereo16 (signed short *samples, int count);
    void    CopyInMono16 (signed short *samples, int count);

    double  GetIntensity (int i) const
    { 
        return R[i];
    }

    int     GetFrequency (int point) const
    {
        // return frequency in Hz of a given point
        long x =_sampleRate * point;
        return x / _Points;
    }

    int     HzToPoint (int freq) const 
    { 
        return (long)_Points * freq / _sampleRate; 
    }

    int     MaxFreq() const { return _sampleRate; }

    int     Tape (int i) const
    {
        return (int) aTape[i];
    }

private:
    int			_Points;
    long		_sampleRate;

	int			bits;

	float		*aTape;
	float		*sinTab;
	int			*bRevTab;
	float		*A, *B, *R, *W;
};

#endif
