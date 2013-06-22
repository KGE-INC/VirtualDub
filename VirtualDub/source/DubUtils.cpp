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
#include "ddrawsup.h"
#include "vbitmap.h"

///////////////////////////////////////////////////////////////////////////
//
//	VDStreamInterleaver
//
///////////////////////////////////////////////////////////////////////////

void VDStreamInterleaver::Init(int streams) {
	mStreams.resize(streams);
	mNextStream = 0;
	mFrames = 0;

	mFramesPerSegment	= 0;
	mBytesPerSegment	= 0;
	mSegmentCutFrame	= 0;
	mSegmentStartFrame	= 0;
	mSegmentOkFrame		= 0x7fffffffffffffff;
	mPerFrameOverhead	= 0;
	mCurrentSize		= 0;
	mBytesPerFrame		= 0;
	mpCutEstimator		= 0;
	mCutStream			= -1;
	mbSegmentOverflowed	= false;
	mbInterleavingEnabled	= true;
	mNonIntStream		= 0;
	mActiveStreams		= 0;
}

void VDStreamInterleaver::SetSegmentFrameLimit(sint64 frames) {
	mFramesPerSegment	= frames;
	mSegmentCutFrame	= mBytesPerSegment ? 0 : frames;
	mSegmentOkFrame		= 0;
}

void VDStreamInterleaver::SetSegmentByteLimit(sint64 bytes, sint32 nOverheadPerFrame) {
	mBytesPerSegment	= bytes;
	mSegmentCutFrame	= 0;
	mBytesPerFrame		+= nOverheadPerFrame;
	mSegmentOkFrame		= 0;
	mPerFrameOverhead	= nOverheadPerFrame;
}

void VDStreamInterleaver::SetCutEstimator(int stream, IVDStreamInterleaverCutEstimator *pEstimator) {
	mpCutEstimator = pEstimator;
	mCutStream	= stream;
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

void VDStreamInterleaver::AddVBRCorrection(int stream, sint32 bytes) {
	VDASSERT(stream >= 0 && stream < mStreams.size());
	VDASSERT(bytes >= 0 && bytes <= mStreams[stream].mBytesPerFrame);
	mCurrentSize += bytes - mStreams[stream].mBytesPerFrame;

//	VDDEBUG("Dub/Interleaver: CurrentSize = %lu\n", (unsigned long)mCurrentSize);
	VDASSERT(mCurrentSize >= 0);
}

void VDStreamInterleaver::AddCBRCorrection(int stream, sint32 actual) {
	VDASSERT(stream >= 0 && stream < mStreams.size());

	Stream& streaminfo = mStreams[stream];

	VDASSERT(actual >= 0 && actual <= streaminfo.mLastSampleWrite);
	mCurrentSize += streaminfo.mMaxSampleSize * (actual - streaminfo.mLastSampleWrite);
//	VDDEBUG("Dub/Interleaver: CurrentSize = %lu\n", (unsigned long)mCurrentSize);
	VDASSERT(mCurrentSize >= 0);
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

		VDASSERT(!mBytesPerSegment || mCurrentSize <= mBytesPerSegment);

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
				mCurrentSize += samples * streaminfo.mMaxSampleSize;
				streaminfo.mSamplesWrittenToSegment += samples;

//				VDDEBUG("Dub/Interleaver: stream %d, CurrentSize = %lu\n", (int)streamID, (unsigned long)mCurrentSize);
				return kActionWrite;
			}
		}

		mNextStream = 0;

		VDASSERT(!mBytesPerSegment || mCurrentSize <= mBytesPerSegment);
	}
}

VDStreamInterleaver::Action VDStreamInterleaver::PushStreams() {
	const int nStreams = mStreams.size();

	for(;;) {
		int nFeeding = 0;
		sint64 minframe = 0x7fffffffffffffff;
		sint64 microFrames = (sint64)mFrames << 16;

		if (mSegmentCutFrame && mSegmentOkFrame > mSegmentCutFrame)
			mSegmentOkFrame = mSegmentCutFrame;

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

			if (iframe > mSegmentOkFrame) {
				VDASSERT(iframe > 0);

				// If using a byte limit and no cut point has been established,
				// try to push the limit forward.
				if (mBytesPerSegment && !mSegmentCutFrame) {
					VDASSERT(mSegmentOkFrame >= mFrames - 1);

					sint64 targetFrame = iframe;
					if (mFramesPerSegment && iframe > mFramesPerSegment)
						targetFrame = mFramesPerSegment;

					PushLimitFrontier(targetFrame);

	//				VDDEBUG("Dub/Interleaver: Projecting frame %ld -> %ld (size %ld + %ld < %ld)\n", (long)mFrames, (long)mSegmentOkFrame, (long)mCurrentSize, (long)projectedSize - (long)mCurrentSize, (long)mBytesPerSegment);

					if (mSegmentOkFrame <= 0) {
						mbSegmentOverflowed = true;
						mSegmentOkFrame = 1;
					}
				}

				// If still above limit, then we must force a segment break.
				if (iframe > mSegmentOkFrame) {
					mSegmentCutFrame = mSegmentOkFrame;
					iframe = mSegmentCutFrame;
					frame = (double)mSegmentCutFrame;
				} else
					frame = microFrameOffset / 65536.0;
			} else
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

		// If a cut point is active and all streams have hit the cut point....
		if (mSegmentCutFrame && minframe >= mSegmentCutFrame) {
			if (!mbInterleavingEnabled && mNonIntStream+1 < nStreams) {
				VDDEBUG("Dub/Interleaver: Finished with stream %d for this segment; advancing to next.\n", mNonIntStream);
				++mNonIntStream;
				continue;
			} else {
				mCurrentSize = 0;
				mSegmentStartFrame += mSegmentCutFrame;
				mFrames = 0;
				if (mFramesPerSegment && !mBytesPerSegment)
					mSegmentCutFrame = mFramesPerSegment;
				else
					mSegmentCutFrame = 0;
				mSegmentOkFrame = 0;
				mbSegmentOverflowed = false;
				mNonIntStream = 0;
				VDDEBUG("Dub/Interleaver: Advancing to next segment\n");
				return kActionNextSegment;
			}
		}

		// If no streams are feeding and we have no cut point, bump the frame target by 1 frame and
		// try again.

		++mFrames;
		mCurrentSize += mPerFrameOverhead;
	}

	if (!mbInterleavingEnabled)
		mNextStream = mNonIntStream;

	return kActionWrite;
}

void VDStreamInterleaver::PushLimitFrontier(sint64 targetFrame) {
	// If the cut stream (usually video) is still active, use its estimator
	// to place the cut.

	sint64 projectedSize = mCurrentSize;
	sint64 projectedTarget = mSegmentOkFrame;
	int j;

	int nStreams = mStreams.size();

	for(j=0; j<nStreams; ++j)
		mStreams[j].mEstimatedSamples = mStreams[j].mSamplesWrittenToSegment;

	if (mpCutEstimator && mStreams[mCutStream].mbActive)
		projectedTarget = std::min<sint64>(projectedTarget, mStreams[mCutStream].mEstimatedSamples - mSegmentStartFrame);

	while(projectedTarget < targetFrame) {
		if (mpCutEstimator && mStreams[mCutStream].mbActive) {
			sint64 framesToNextCutPoint;
			sint64 bytesToNextCutPoint;

			if (mpCutEstimator->EstimateCutPoint(mCutStream, mStreams[mCutStream].mEstimatedSamples, targetFrame+mSegmentStartFrame, framesToNextCutPoint, bytesToNextCutPoint)) {
				projectedSize += bytesToNextCutPoint;
				projectedSize += mPerFrameOverhead * framesToNextCutPoint;
				projectedTarget = (mStreams[mCutStream].mEstimatedSamples += framesToNextCutPoint) - mSegmentStartFrame;
			} else {
				++projectedTarget;
				projectedSize += mPerFrameOverhead;
			}
		} else {
			++projectedTarget;
			projectedSize += mPerFrameOverhead;
		}

		for(j=0; j<nStreams; ++j) {
			if (j == mCutStream)
				continue;

			Stream& sinfo = mStreams[j];
			if (sinfo.mbActive) {
				sint64 diff = (sint64)ceil(sinfo.mSamplesPerFrame * (projectedTarget + mSegmentStartFrame)) - sinfo.mEstimatedSamples;

				if (diff > 0) {
					sinfo.mEstimatedSamples += diff;
					projectedSize += diff * sinfo.mMaxSampleSize;
				}
			}
		}

		if (projectedSize > mBytesPerSegment)
			break;

		mSegmentOkFrame = projectedTarget;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	VDRenderFrameMap
//
///////////////////////////////////////////////////////////////////////////

void VDRenderFrameMap::Init(IVDVideoSource *pVS, VDPosition nSrcStart, VDFraction srcStep, const FrameSubset *pSubset, VDPosition nFrameCount, bool bDirect) {
	VDPosition directLast = -1;

	const VDPosition len = pVS->asStream()->getLength();

	for(VDPosition frame = 0; frame < nFrameCount; ++frame) {
		VDPosition timelineFrame = nSrcStart + srcStep.scale64t(frame);
		VDPosition srcFrame = timelineFrame;

		if (pSubset) {
			srcFrame = pSubset->lookupFrame((int)srcFrame);
			if (srcFrame < 0)
				break;
		} else {
			if (srcFrame < 0 || srcFrame >= len)
				break;
		}

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

		mFrameMap.push_back(std::make_pair(timelineFrame, srcFrame));
	}

	mMaxFrame = mFrameMap.size();
};

///////////////////////////////////////////////////////////////////////////
//
//	VDRenderFrameIterator
//
///////////////////////////////////////////////////////////////////////////

void VDRenderFrameIterator::Init(IVDVideoSource *pVS, bool bDirect) {
	mpVideoSource	= pVS;
	mpVideoStream	= pVS->asStream();
	mbDirect		= bDirect;
	mDstFrame		= 0;
	mSrcDisplayFrame	= -1;
	mLastSrcDisplayFrame = -1;
	mbFinished		= false;

	Reload();
}

void VDRenderFrameIterator::Next(VDPosition& srcFrame, VDPosition& displayFrame, VDPosition& timelineFrame, bool& bIsPreroll) {
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

			if (mbDirect)
				if (!Reload())
					mbFinished = true;
//			VDDEBUG("Dub/RenderFrameIterator: Issuing %lu\n", (unsigned long)displayFrame);
			return;
		}

		if (!Reload())
			break;
	}		

	srcFrame = -1;
	timelineFrame = mSrcTimelineFrame;
	displayFrame = mSrcDisplayFrame;
	bIsPreroll = false;
	mbFinished = true;
}

bool VDRenderFrameIterator::Reload() {
	mSrcTimelineFrame	= mFrameMap.TimelineFrame(mDstFrame);
	if (mSrcTimelineFrame < 0)
		return false;
	VDPosition nextDisplay = mFrameMap.DisplayFrame(mDstFrame);

	if (mbDirect && nextDisplay == mLastSrcDisplayFrame)
		nextDisplay = -1;
	else {
		mpVideoSource->streamSetDesiredFrame((LONG)nextDisplay);
		mLastSrcDisplayFrame = nextDisplay;
	}

	mSrcDisplayFrame = nextDisplay;
	++mDstFrame;

	mbFirstSourceFrame = true;
	return true;
}

bool VDRenderFrameIterator::EstimateCutPoint(int stream, sint64 start, sint64 target, sint64& framesToNextPoint, sint64& bytesToNextPoint) {
	VDASSERT(mbDirect);		// We shouldn't be used as an estimator in non-direct mode.

//	VDDEBUG("Dub/RenderFrameIterator: Mapping %lu -> %lu\n", (unsigned long)start, (unsigned long)rawFrame);

	VDPosition rangeLo = -1, rangeHi;

	framesToNextPoint = 0;
	bytesToNextPoint = 0;
	for(;;) {
		VDPosition srcFrame = mFrameMap.DisplayFrame(start);

		if (srcFrame < 0) {
			if (start < target)
				framesToNextPoint += target-start;
			break;
		}

		if (rangeLo < 0) {
			rangeLo = mpVideoSource->nearestKey((LONG)srcFrame);
			if (rangeLo < 0)
				rangeLo = 0;
			rangeHi = mpVideoSource->nextKey((LONG)rangeLo);
			if (rangeHi < 0)
				rangeHi = mpVideoStream->getEnd();
		} else if (srcFrame < rangeLo || srcFrame >= rangeHi)
			break;

		uint32 lSize;

		++framesToNextPoint;

		if (start > 0 && mFrameMap.DisplayFrame(start-1) == srcFrame) {
			lSize = 0;		// frame duplication -- special case
		} else {
			int hr = mpVideoStream->read((LONG)srcFrame, 1, NULL, 0x7FFFFFFF, &lSize, NULL);
			if (hr) {
				VDASSERT(false);
				break;	// shouldn't happen
			}
		}

		bytesToNextPoint += lSize;
		++start;
	}

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

void VDAudioPipeline::Init(uint32 bytes) {
	mbInputClosed = false;
	mbOutputClosed = false;

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

