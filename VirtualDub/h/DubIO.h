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

#ifndef f_DUBIO_H
#define f_DUBIO_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/error.h>
#include <vd2/system/thread.h>

class VDAtomicInt;
class IVDStreamSource;
class IVDVideoSource;
class VDRenderFrameIterator;
class AudioStream;
class AVIPipe;
class VDAudioPipeline;
template<class T> class VDRingBuffer;
class DubAudioStreamInfo;
class DubVideoStreamInfo;

///////////////////////////////////////////////////////////////////////////
//
//	VDDubIOThread
//
///////////////////////////////////////////////////////////////////////////

namespace nsVDDub {
	enum {
		kBufferFlagDelta		= 1,
		kBufferFlagPreload		= 2
	};
}

class VDDubIOThread : public VDThread {
public:
	VDDubIOThread(
		bool				bPhantom,
		IVDVideoSource		*pVideo,
		VDRenderFrameIterator& videoFrameIterator,
		AudioStream			*pAudio,
		AVIPipe				*const pVideoPipe,
		VDAudioPipeline		*const pAudioPipe,
		volatile bool&		bAbort,
		DubAudioStreamInfo&	_aInfo,
		DubVideoStreamInfo& _vInfo,
		VDAtomicInt&		threadCounter
		);
	~VDDubIOThread();

	bool GetError(MyError& e) {
		if (mbError) {
			e.TransferFrom(mError);
			return true;
		}
		return false;
	}

protected:
	void ThreadRun();
	void ReadVideoFrame(VDPosition stream_frame, VDPosition display_frame, VDPosition timeline_frame, bool preload);
	void ReadNullVideoFrame(VDPosition displayFrame, VDPosition timelineFrame);
	bool MainAddVideoFrame();
	bool MainAddAudioFrame();

	MyError				mError;
	bool				mbError;

	// config vars (ick)

	bool				mbPhantom;
	IVDVideoSource		*const mpVideo;
	IVDStreamSource		*const mpVideoStream;
	VDRenderFrameIterator& mVideoFrameIterator;
	AudioStream			*const mpAudio;
	AVIPipe				*const mpVideoPipe;
	VDAudioPipeline		*const mpAudioPipe;
	volatile bool&		mbAbort;
	DubAudioStreamInfo&	aInfo;
	DubVideoStreamInfo& vInfo;
	VDAtomicInt&		mThreadCounter;
};


#endif
