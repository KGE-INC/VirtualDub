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

#include <vector>

void VDCreateRaisedCosineWindow(float *dst, int n, float power);
void VDCreateHalfSineTable(float *dst, int n);
void VDCreateBitRevTable(unsigned *dst, int n);
void VDComputeFHT(float *A, int nPoints, const float *sinTab);

class Fht {
public:
	Fht(unsigned points);
	~Fht();

    void    Transform (int width);
    void    CopyInStereo8 (unsigned char *samples, int count);
    void    CopyInMono8 (unsigned char *samples, int count);
    void    CopyInStereo16 (signed short *samples, int count);
    void    CopyInMono16 (signed short *samples, int count);

    unsigned Points() const { return mWork.size(); }
    double  GetIntensity(int i) const { 
        return sqrt(mWork[i]);
    }
    double  GetPower(int i) const { 
        return mWork[i];
    }
protected:
	void ScrollTape(unsigned count);

	std::vector<float>		mTape;
	std::vector<float>		mWindow;
	std::vector<float>		mWork;
	std::vector<float>		mSinTab;
	std::vector<unsigned>	mBitRev;
};

#endif
