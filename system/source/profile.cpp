//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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
