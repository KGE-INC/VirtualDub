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

#ifndef f_VD2_SYSTEM_PROFILE_H
#define f_VD2_SYSTEM_PROFILE_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/thread.h>
#include <vector>

class VDRTProfiler;

void VDInitProfilingSystem();
void VDDeinitProfilingSystem();
VDRTProfiler *VDGetRTProfiler();

//
//	VDRTProfiler		Real-time profiler
//
//	This class forms the base for a very simple real-time profiler: threads
//	record events in channels, and periodically, someone swaps the active
//	recording array with a second array, and draws the sampled events off
//	that array.  In VirtualDub, this is done via RTProfileDisplay.  Events
//	are sampled via the high-performance counter in Win32, but clients need
//	never know this fact.
//
//	All methods in VDRTProfiler are thread-safe.  However, it is assumed
//	that only one client will be calling Swap() and accessing the Paint
//	channel set.  Swap() should be called from rather low-level code as
//	it may introduce deadlocks otherwise.
//
//	Strings passed to VDRTProfiler must be constant data in the main EXE.
//	No dynamic strings or DLLs.  The reason is that there is an
//	indefinite delay between a call to FreeChannel() and the last time
//	data from that channel is displayed.
//
//	Channels are not restricted to a particular thread; it is permissible
//	to allocate a channel in one thread and use it in another.  However,
//	channels must not be simultaneously used by two threads -- that will
//	generate interesting output.
//
class VDRTProfiler {
public:
	VDRTProfiler();
	~VDRTProfiler();

	void BeginCollection();
	void EndCollection();
	void Swap();

	bool IsEnabled() const { return mbEnableCollection; }

	int AllocChannel(const char *name);
	void FreeChannel(int ch);
	void BeginEvent(int channel, uint32 color, const char *name);
	void EndEvent(int channel);

public:
	struct Event {
		uint64		mStartTime;
		uint64		mEndTime;			// only last 32 bits of counter
		uint32		mColor;
		const char *mpName;
	};

	struct Channel {
		const char			*mpName;
		bool				mbEventPending;
		std::vector<Event>	mEventList;
	};

	typedef std::vector<Channel> tChannels;

	VDCriticalSection		mcsChannels;
	tChannels				mChannelArray;
	tChannels				mChannelArrayToPaint;
	uint64					mPerfFreq;
	uint64					mSnapshotTime;

	volatile bool			mbEnableCollection;
};

//
//	VDRTProfileChannel
//
//	This helper simply makes channel acquisition easier.  It automatically
//	stubs out if no profiler is available.  However, it's still advisable
//	not to call this from your inner loop!
//
class VDRTProfileChannel {
public:
	VDRTProfileChannel(const char *name)
		: mpProfiler(VDGetRTProfiler())
		, mProfileChannel(mpProfiler ? mpProfiler->AllocChannel(name) : 0)
	{
	}
	~VDRTProfileChannel() {
		if (mpProfiler)
			mpProfiler->FreeChannel(mProfileChannel);
	}

	void Begin(uint32 color, const char *name) {
		if (mpProfiler)
			mpProfiler->BeginEvent(mProfileChannel, color, name);
	}

	void End() {
		if (mpProfiler)
			mpProfiler->EndEvent(mProfileChannel);
	}

protected:
	VDRTProfiler *const mpProfiler;
	int mProfileChannel;
};

#endif

