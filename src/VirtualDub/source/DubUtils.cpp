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

#include "stdafx.h"

#include <ddraw.h>

#include <vd2/system/error.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/w32assist.h>
#include <vector>

#include "DubUtils.h"
#include "VideoSource.h"
#include "FrameSubset.h"
#include "vbitmap.h"
#include "FilterSystem.h"

///////////////////////////////////////////////////////////////////////////
//
//	VDStreamInterleaver
//
///////////////////////////////////////////////////////////////////////////

void VDStreamInterleaver::Init(int streams) {
	mStreams.resize(streams);
	mNextStream = 0;
	mFrames = 0;

	mSegmentStartFrame	= 0;
	mBytesPerFrame		= 0;
	mbInterleavingEnabled	= true;
	mNonIntStream		= 0;
	mActiveStreams		= 0;
}

void VDStreamInterleaver::InitStream(int stream, uint32 nSampleSize, sint32 nPreload, double nSamplesPerFrame, double nInterval, sint32 nMaxPush) {
	VDASSERT(stream>=0 && stream<mStreams.size());

	Stream& streaminfo = mStreams[stream];

	streaminfo.mSamplesWrittenToSegment = 0;
	streaminfo.mMaxSampleSize		= nSampleSize;
	streaminfo.mPreloadMicroFrames	= (sint32)((double)nPreload / nSamplesPerFrame * 65536);
	streaminfo.mSamplesPerFrame		= nSamplesPerFrame;
	streaminfo.mBytesPerFrame		= (sint64)ceil(nSampleSize * nSamplesPerFrame);
	streaminfo.mIntervalMicroFrames	= (sint32)(65536.0 / nInterval);
	streaminfo.mLastSampleWrite		= 0;
	streaminfo.mbActive				= true;
	streaminfo.mMaxPush				= nMaxPush;

	++mActiveStreams;

	mBytesPerFrame += streaminfo.mBytesPerFrame;
}

void VDStreamInterleaver::EndStream(int stream) {
	Stream& streaminfo = mStreams[stream];

	if (streaminfo.mbActive) {
		streaminfo.mbActive		= false;
		streaminfo.mSamplesToWrite	= 0;
		mBytesPerFrame -= streaminfo.mBytesPerFrame;
		--mActiveStreams;

		while(mNonIntStream < mStreams.size() && !mStreams[mNonIntStream].mbActive)
			++mNonIntStream;
	}
}

void VDStreamInterleaver::AdjustStreamRate(int stream, double samplesPerFrame) {
	VDASSERT(stream >= 0 && stream < mStreams.size());
	Stream& streaminfo = mStreams[stream];

	streaminfo.mSamplesPerFrame = samplesPerFrame;
}

VDStreamInterleaver::Action VDStreamInterleaver::GetNextAction(int& streamID, sint32& samples) {
	const int nStreams = mStreams.size();

	if (!mActiveStreams)
		return kActionFinish;

	for(;;) {
		if (!mNextStream) {
			Action act = PushStreams();

			if (act != kActionWrite)
				return act;
		}

		for(; mNextStream<nStreams; ++mNextStream) {
			Stream& streaminfo = mStreams[mNextStream];

			if (!mbInterleavingEnabled && mNextStream > mNonIntStream)
				break;

			if (streaminfo.mSamplesToWrite > 0) {
				samples = streaminfo.mSamplesToWrite;
				if (samples > streaminfo.mMaxPush)
					samples = streaminfo.mMaxPush;
				streaminfo.mSamplesToWrite -= samples;
				streaminfo.mLastSampleWrite = samples;
				streamID = mNextStream;
				VDASSERT(samples < 2147483647);
				streaminfo.mSamplesWrittenToSegment += samples;
				return kActionWrite;
			}
		}

		mNextStream = 0;
	}
}

VDStreamInterleaver::Action VDStreamInterleaver::PushStreams() {
	const int nStreams = mStreams.size();

	for(;;) {
		int nFeeding = 0;
		sint64 minframe = 0x7fffffffffffffff;
		sint64 microFrames = (sint64)mFrames << 16;

		for(int i=mNonIntStream; i<nStreams; ++i) {
			Stream& streaminfo = mStreams[i];

			if (!streaminfo.mbActive)
				continue;

			sint64 microFrameOffset = microFrames;
			
			if (streaminfo.mIntervalMicroFrames != 65536) {
				microFrameOffset += streaminfo.mIntervalMicroFrames - 1;
				microFrameOffset -= microFrameOffset % streaminfo.mIntervalMicroFrames;
			}

			microFrameOffset += streaminfo.mPreloadMicroFrames;

			sint64 iframe = (microFrameOffset + 65535) >> 16;
			double frame;

			frame = microFrameOffset / 65536.0;

			if (iframe < minframe)
				minframe = iframe;

			sint64 toread = (sint64)ceil(streaminfo.mSamplesPerFrame * (frame + mSegmentStartFrame)) - streaminfo.mSamplesWrittenToSegment;

			if (toread < 0)
				toread = 0;
		
			VDASSERT((sint32)toread == toread);
			streaminfo.mSamplesToWrite = (sint32)toread;

//			VDDEBUG("Dub/Interleaver: Stream #%d: feeding %d samples\n", i, (int)toread);
			if (toread > 0)
				++nFeeding;

			if (!mbInterleavingEnabled)
				break;
		}

		if (nFeeding > 0)
			break;

		// If no streams are feeding and we have no cut point, bump the frame target by 1 frame and
		// try again.

		++mFrames;
	}

	if (!mbInterleavingEnabled)
		mNextStream = mNonIntStream;

	return kActionWrite;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDRenderFrameMap
//
///////////////////////////////////////////////////////////////////////////

void VDRenderFrameMap::Init(const vdfastvector<IVDVideoSource *>& videoSources, VDPosition nSrcStart, VDFraction srcStep, const FrameSubset *pSubset, VDPosition nFrameCount, bool bDirect) {
	VDPosition directLast = -1;
	int sourceLast = -1;
	IVDVideoSource *pVS = NULL;
	VDPosition len = 0;

	mFrameMap.reserve((size_t)nFrameCount);
	for(VDPosition frame = 0; frame < nFrameCount; ++frame) {
		VDPosition timelineFrame = nSrcStart + srcStep.scale64t(frame);
		VDPosition srcFrame = timelineFrame;
		int source = 0;

		if (pSubset) {
			bool masked;
			srcFrame = pSubset->lookupFrame((int)srcFrame, masked, source);
			if (srcFrame < 0)
				break;
		} else {
			if (srcFrame < 0 || srcFrame >= len)
				break;
		}

		if (sourceLast != source) {
			sourceLast = source;
			pVS = videoSources[source];
			len = pVS->asStream()->getLength();
			directLast = -1;
		}

		srcFrame = pVS->getRealDisplayFrame(srcFrame);

		if (bDirect) {
			VDPosition key = pVS->nearestKey((LONG)srcFrame);

			if (directLast < key)
				directLast = key;
			else if (directLast > srcFrame)
				directLast = key;
			else {
				while(directLast < srcFrame) {
					++directLast;

					if (pVS->getDropType(directLast) != VideoSource::kDroppable)
						break;
				}
			}

			srcFrame = directLast;
		}

		FrameEntry ent;
		ent.mSrcIndex = source;
		ent.mTimelineFrame = timelineFrame;
		ent.mDisplayFrame = srcFrame;

		mFrameMap.push_back(ent);
	}

	mMaxFrame = mFrameMap.size();

	mInvalidEntry.mSrcIndex = -1;
	mInvalidEntry.mTimelineFrame = -1;
	mInvalidEntry.mDisplayFrame = -1;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDRenderFrameIterator
//
///////////////////////////////////////////////////////////////////////////

void VDRenderFrameIterator::Init(const vdfastvector<IVDVideoSource *>& videoSources, bool bDirect, bool bSmart, const FilterSystem *filtsys) {
	mVideoSources = videoSources;
	mpFilterSystem	= filtsys;
	mbDirect		= bDirect || bSmart;
	mbSmart			= bSmart;
	mDstFrame		= 0;
	mSrcDisplayFrame	= -1;
	mLastSrcDisplayFrame = -1;
	mSrcIndex = -1;
	mLastSrcIndex = -1;
	mpVideoSource = NULL;
	mbFinished		= false;
	mbSameAsLast	= false;

	Reload();
}

void VDRenderFrameIterator::Next(VDPosition& srcFrame, VDPosition& displayFrame, VDPosition& timelineFrame, bool& bIsPreroll, int& srcIndex, bool& direct, bool& sameAsLast) {
	while(!mbFinished) {
		bool b;
		VDPosition f = -1;

		if (mSrcDisplayFrame >= 0) {
			f = mpVideoSource->streamGetNextRequiredFrame(b);
			bIsPreroll = (b!=0) && !mbDirect;
		} else {
			f = -1;
			bIsPreroll = false;
		}

		if (f!=-1 || mbFirstSourceFrame) {
			mbFirstSourceFrame = false;

			srcFrame = f;
			displayFrame = mSrcDisplayFrame;
			timelineFrame = mSrcTimelineFrame;
			srcIndex = mSrcIndex;
			direct = mbDirect;
			sameAsLast = mbSameAsLast;

			if (mbDirect) {
				if (!Reload())
					mbFinished = true;
			}
			return;
		}

		if (!Reload())
			break;
	}		

	srcFrame = -1;
	srcIndex = mSrcIndex;
	timelineFrame = mSrcTimelineFrame;
	displayFrame = mSrcDisplayFrame;
	bIsPreroll = false;
	sameAsLast = true;
	mbFinished = true;
	direct = mbDirect;
}

bool VDRenderFrameIterator::Reload() {
	const VDRenderFrameMap::FrameEntry& ent = mFrameMap[mDstFrame];

	mSrcIndex = ent.mSrcIndex;
	if (mSrcIndex < 0)
		return false;

	if (mLastSrcIndex != mSrcIndex) {
		mLastSrcIndex = mSrcIndex;
		mLastSrcDisplayFrame = -1;
	}

	mpVideoSource = mVideoSources[mSrcIndex];

	mSrcTimelineFrame	= ent.mTimelineFrame;
	VDPosition nextDisplay = ent.mDisplayFrame;

	if (mbSmart) {
		bool isFiltered = mpFilterSystem && mpFilterSystem->IsFiltered(nextDisplay);

		if (mbDirect) {
			mpVideoSource->streamSetDesiredFrame(nextDisplay);
			if (isFiltered || mpVideoSource->streamGetRequiredCount(NULL) != 1) {
				mpVideoSource->streamRestart();
				mbDirect = false;
			}
		} else {
			if (!isFiltered && mpVideoSource->isKey(mpVideoSource->displayToStreamOrder(nextDisplay))) {
				mpVideoSource->streamRestart();
				mbDirect = true;
			}
		}
	}

	mbSameAsLast = (nextDisplay == mLastSrcDisplayFrame);

	if (mbDirect && mbSameAsLast)
		nextDisplay = -1;
	else {
		mpVideoSource->streamSetDesiredFrame(nextDisplay);
		mLastSrcDisplayFrame = nextDisplay;
	}

	mSrcDisplayFrame = nextDisplay;
	++mDstFrame;

	mbFirstSourceFrame = true;
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAudioPipeline
//
///////////////////////////////////////////////////////////////////////////

VDAudioPipeline::VDAudioPipeline() {
}

VDAudioPipeline::~VDAudioPipeline() {
}

void VDAudioPipeline::Init(uint32 bytes, uint32 sampleSize) {
	mbInputClosed = false;
	mbOutputClosed = false;
	mSampleSize = sampleSize;

	mBuffer.Init(bytes);
}

void VDAudioPipeline::Shutdown() {
	mBuffer.Shutdown();
}

int VDAudioPipeline::Read(void *pBuffer, int bytes) {
	int actual = mBuffer.Read((char *)pBuffer, bytes);

	if (actual)
		msigRead.signal();

	return actual;
}

void *VDAudioPipeline::BeginWrite(int requested, int& actual) {
	return mBuffer.LockWrite(requested, actual);
}

void VDAudioPipeline::EndWrite(int actual) {
	if (actual) {
		mBuffer.UnlockWrite(actual);
		msigWrite.signal();
	}
}

