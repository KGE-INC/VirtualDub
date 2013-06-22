//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#include <vd2/system/profile.h>

///////////////////////////////////////////////////////////////////////////

VDRTProfiler *g_pCentralProfiler;

void VDInitProfilingSystem() {
	if (!g_pCentralProfiler)
		g_pCentralProfiler = new VDRTProfiler;
}

void VDDeinitProfilingSystem() {
	delete g_pCentralProfiler;
	g_pCentralProfiler = 0;
}

VDRTProfiler *VDGetRTProfiler() {
	return g_pCentralProfiler;
}

///////////////////////////////////////////////////////////////////////////

VDRTProfiler::VDRTProfiler()
	: mbEnableCollection(false)
{
	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);
	mPerfFreq = freq.QuadPart;
}

VDRTProfiler::~VDRTProfiler() {
}

void VDRTProfiler::BeginCollection() {
	mbEnableCollection = true;
}

void VDRTProfiler::EndCollection() {
	mbEnableCollection = false;
}

void VDRTProfiler::Swap() {
	vdsynchronized(mcsChannels) {
		LARGE_INTEGER tim;
		QueryPerformanceCounter(&tim);

		mSnapshotTime = tim.QuadPart;

		mChannelArrayToPaint.resize(mChannelArray.size());

		for(int i=0; i<mChannelArray.size(); ++i) {
			Channel& src = mChannelArray[i];
			Channel& dst = mChannelArrayToPaint[i];

			dst.mpName = src.mpName;

			dst.mEventList.clear();
			dst.mEventList.swap(src.mEventList);
			if (src.mbEventPending) {
				src.mEventList.push_back(dst.mEventList.back());
				dst.mEventList.back().mEndTime = mSnapshotTime;
			}
		}
	}
}

int VDRTProfiler::AllocChannel(const char *name) {
	int i;

	vdsynchronized(mcsChannels) {
		const int nChannels = mChannelArray.size();

		for(i=0; i<nChannels; ++i)
			if (!mChannelArray[i].mpName)
				break;

		if (mChannelArray.size() <= i)
			mChannelArray.resize(i + 1);

		mChannelArray[i].mpName = name;
		mChannelArray[i].mbEventPending = false;
	}

	return i;
}

void VDRTProfiler::FreeChannel(int ch) {
	vdsynchronized(mcsChannels) {
		mChannelArray[ch].mpName = 0;
		mChannelArray[ch].mEventList.clear();
	}
}

void VDRTProfiler::BeginEvent(int channel, uint32 color, const char *name) {
	if (mbEnableCollection) {
		LARGE_INTEGER tim;
		QueryPerformanceCounter(&tim);
		vdsynchronized(mcsChannels) {
			Channel& chan = mChannelArray[channel];

			if (!chan.mbEventPending) {
				chan.mbEventPending = true;
				chan.mEventList.push_back(Event());
				Event& ev = chan.mEventList.back();
				ev.mpName = name;
				ev.mColor = color;
				ev.mStartTime = tim.QuadPart;
				ev.mEndTime = tim.QuadPart;
			}
		}
	}
}

void VDRTProfiler::EndEvent(int channel) {
	if (mbEnableCollection) {
		LARGE_INTEGER tim;

		QueryPerformanceCounter(&tim);
		vdsynchronized(mcsChannels) {
			Channel& chan = mChannelArray[channel];

			if (chan.mbEventPending) {
				chan.mEventList.back().mEndTime = tim.QuadPart;
				chan.mbEventPending = false;
			}
		}
	}
}
