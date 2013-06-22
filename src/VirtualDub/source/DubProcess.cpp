#include "stdafx.h"
#include <vd2/Dita/resources.h>
#include <vd2/system/error.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Riza/display.h>
#include "crash.h"
#include "Dub.h"
#include "DubIO.h"
#include "DubProcess.h"
#include "DubUtils.h"
#include "DubOutput.h"
#include "DubStatus.h"
#include "AVIPipe.h"
#include "AVIOutput.h"
#include "AVIOutputPreview.h"
#include "VideoSource.h"
#include "VideoTelecineRemover.h"
#include "VideoSequenceCompressor.h"
#include "prefs.h"
#include "AsyncBlitter.h"

/// HACK!!!!
#define vSrc mVideoSources[0]

using namespace nsVDDub;

namespace {
	enum { kVDST_Dub = 1 };

	enum {
		kVDM_SegmentOverflowOccurred,
		kVDM_BeginningNextSegment,
		kVDM_IOThreadLivelock,
		kVDM_ProcessingThreadLivelock,
		kVDM_CodecDelayedDuringDelayedFlush,
		kVDM_CodecLoopingDuringDelayedFlush,
		kVDM_FastRecompressUsingFormat,
		kVDM_SlowRecompressUsingFormat,
		kVDM_FullUsingInputFormat,
		kVDM_FullUsingOutputFormat
	};
};

VDDubProcessThread::VDDubProcessThread()
	: VDThread("Processing")
	, mpAVIOut(NULL)
	, mpVideoOut(NULL)
	, mpAudioOut(NULL)
	, mpOutputSystem(NULL)
	, mpVideoPipe(NULL)
	, mpAudioPipe(NULL)
	, mbAudioFrozen(false)
	, mbAudioFrozenValid(false)
	, mpBlitter(NULL)
	, lDropFrames(0)
	, mbSyncToAudioEvenClock(false)
	, mbError(false)
	, mpCurrentAction("starting up")
	, mActivityCounter(0)
	, mRefreshFlag(1)
	, mProcessingProfileChannel("Processor")
	, mpVideoFilterOutputBuffer(NULL)
	, mpInvTelecine(NULL)
	, mpAbort(NULL)
	, mpStatusHandler(NULL)
{
}

VDDubProcessThread::~VDDubProcessThread() {
}

void VDDubProcessThread::SetAbortSignal(volatile bool *pAbort) {
	mpAbort = pAbort;
}

void VDDubProcessThread::SetStatusHandler(IDubStatusHandler *pStatusHandler) {
	mpStatusHandler = pStatusHandler;
}

void VDDubProcessThread::SetInputDisplay(IVDVideoDisplay *pVideoDisplay) {
	mpInputDisplay = pVideoDisplay;
}

void VDDubProcessThread::SetOutputDisplay(IVDVideoDisplay *pVideoDisplay) {
	mpOutputDisplay = pVideoDisplay;
}

void VDDubProcessThread::SetVideoFilterOutput(FilterStateInfo *pfsi, void *pBuffer, const VDPixmap& px) {
	mfsi = *pfsi;
	mpVideoFilterOutputBuffer = pBuffer;
	mVideoFilterOutputPixmap = px;
}

void VDDubProcessThread::SetVideoSources(IVDVideoSource *const *pVideoSources, uint32 count) {
	mVideoSources.assign(pVideoSources, pVideoSources + count);
}

void VDDubProcessThread::SetAudioSourcePresent(bool present) {
	mbAudioPresent = present;
}

void VDDubProcessThread::SetAudioCorrector(AudioStreamL3Corrector *pCorrector) {
	mpAudioCorrector = pCorrector;
}

void VDDubProcessThread::SetVideoIVTC(VideoTelecineRemover *pIVTC) {
	mpInvTelecine = pIVTC;
}

void VDDubProcessThread::SetVideoCompressor(IVDVideoCompressor *pCompressor) {
	mpVideoCompressor = pCompressor;
	if (pCompressor)
		mVideoCompressionBuffer.resize(pCompressor->GetMaxOutputSize());
}

void VDDubProcessThread::Init(const DubOptions& opts, DubVideoStreamInfo *pvsi, IVDDubberOutputSystem *pOutputSystem, AVIPipe *pVideoPipe, VDAudioPipeline *pAudioPipe, VDStreamInterleaver *pStreamInterleaver) {
	opt = &opts;
	mpVInfo = pvsi;
	mpOutputSystem = pOutputSystem;
	mpVideoPipe = pVideoPipe;
	mpAudioPipe = pAudioPipe;
	mpInterleaver = pStreamInterleaver;

	if (!mpOutputSystem->IsRealTime())
		mpBlitter = VDCreateAsyncBlitter();
	else
		mpBlitter = VDCreateAsyncBlitter(8);

	if (!mpBlitter)
		throw MyError("Couldn't create AsyncBlitter");

	mpBlitter->pulse(1);

	// Init playback timer.
	if (mpOutputSystem->IsRealTime()) {
		int timerInterval = mpVInfo->usPerFrame / 1000;

		if (opt->video.fSyncToAudio || opt->video.nPreviewFieldMode) {
			timerInterval /= 2;
		}

		if (!mFrameTimer.Init(this, timerInterval))
			throw MyError("Couldn't initialize timer!");
	}

	NextSegment();
}

void VDDubProcessThread::Shutdown() {
	mFrameTimer.Shutdown();

	if (mpBlitter) {
		mpBlitter->abort();
		delete mpBlitter;
		mpBlitter = NULL;
	}

	if (mpAVIOut) {
		delete mpAVIOut;
		mpAVIOut = NULL;
		mpAudioOut = NULL;
		mpVideoOut = NULL;
	}
}

void VDDubProcessThread::Abort() {
	if (mpBlitter)
		mpBlitter->beginFlush();
}

void VDDubProcessThread::UpdateFrames() {
	mRefreshFlag = 1;
}

VDSignal *VDDubProcessThread::GetBlitterSignal() {
	return mpBlitter ? mpBlitter->getFlushCompleteSignal() : NULL;
}

void VDDubProcessThread::NextSegment() {
	if (mpAVIOut) {
		IVDMediaOutput *temp = mpAVIOut;
		mpAVIOut = NULL;
		mpAudioOut = NULL;
		mpVideoOut = NULL;
		mpOutputSystem->CloseSegment(temp, false);
	}

	mpAVIOut = mpOutputSystem->CreateSegment();
	mpAudioOut = mpAVIOut->getAudioOutput();
	mpVideoOut = mpAVIOut->getVideoOutput();
}

void VDDubProcessThread::ThreadRun() {
	bool firstPacket = true;
	bool bVideoEnded = !(vSrc && mpOutputSystem->AcceptsVideo());
	bool bVideoNonDelayedFrameReceived = false;
	bool bAudioEnded = !(mbAudioPresent && mpOutputSystem->AcceptsAudio());
	uint32	nVideoFramesDelayed = 0;

	lDropFrames = 0;
	mpVInfo->processed = 0;

	VDDEBUG("Dub/Processor: start\n");

	std::vector<char>	audioBuffer;
	const bool fPreview = mpOutputSystem->IsRealTime();

	int lastVideoSourceIndex = 0;

	IVDMediaOutputAutoInterleave *pOutAI = vdpoly_cast<IVDMediaOutputAutoInterleave *>(mpAVIOut);

	if (pOutAI)
		mpInterleaver = NULL;

	try {
		DEFINE_SP(sp);

		mpCurrentAction = "running main loop";

		for(;;) {
			int stream;
			sint32 count;

			VDStreamInterleaver::Action nextAction;
			
			if (mpInterleaver)
				nextAction = mpInterleaver->GetNextAction(stream, count);
			else {
				if (bAudioEnded && bVideoEnded)
					break;

				nextAction = VDStreamInterleaver::kActionWrite;
				pOutAI->GetNextPreferredStreamWrite(stream, count);
			}

			++mActivityCounter;

			if (nextAction == VDStreamInterleaver::kActionFinish)
				break;
			else if (nextAction == VDStreamInterleaver::kActionWrite) {
				if (stream == 0) {
					if (mpVInfo->cur_proc_dst >= mpVInfo->end_proc_dst) {
						if (fPreview && mbAudioPresent) {
							static_cast<AVIAudioPreviewOutputStream *>(mpAudioOut)->start();
							mbAudioFrozenValid = true;
						}
						if (mpInterleaver)
							mpInterleaver->EndStream(0);
						mpVideoOut->finish();
						bVideoEnded = true;
					} else {
						// We cannot wrap the entire loop with a profiling event because typically
						// involves a nice wait in preview mode.

						uint32 nFramesPushedTryingToFlushCodec = 0;

						for(;;) {
							const VDRenderVideoPipeFrameInfo *pFrameInfo;
							VDRenderVideoPipeFrameInfo dummyFrameInfo;
							bool bAttemptingToFlushCodecBuffer = false;

							if (*mpAbort)
								goto abort_requested;

							{
								VDDubAutoThreadLocation loc(mpCurrentAction, "waiting for video frame from I/O thread");
								pFrameInfo = mpVideoPipe->getReadBuffer();
							}

							if (!pFrameInfo) {
								if (nVideoFramesDelayed > 0) {
									--nVideoFramesDelayed;

									dummyFrameInfo.mpData			= "";
									dummyFrameInfo.mLength			= 0;
									dummyFrameInfo.mRawFrame		= -1;
									dummyFrameInfo.mDisplayFrame	= -1;
									dummyFrameInfo.mTimelineFrame	= -1;
									dummyFrameInfo.mFlags			= 0;
									dummyFrameInfo.mDroptype		= AVIPipe::kDroppable;
									dummyFrameInfo.mSrcIndex		= lastVideoSourceIndex;
									pFrameInfo = &dummyFrameInfo;

									bAttemptingToFlushCodecBuffer = true;
								} else {
									if (mpInterleaver)
										mpInterleaver->EndStream(0);
									mpVideoOut->finish();
									bVideoEnded = true;
									break;
								}
							}

							if (firstPacket && fPreview && !mbAudioPresent) {
								mpBlitter->enablePulsing(true);
								firstPacket = false;
							}

							VideoWriteResult result = WriteVideoFrame(
								pFrameInfo->mpData,
								pFrameInfo->mFlags,
								pFrameInfo->mDroptype,
								pFrameInfo->mLength,
								pFrameInfo->mRawFrame,
								pFrameInfo->mDisplayFrame,
								pFrameInfo->mTimelineFrame,
								pFrameInfo->mSrcIndex);

							lastVideoSourceIndex = pFrameInfo->mSrcIndex;

							if (result == kVideoWriteDelayed) {
								if (bAttemptingToFlushCodecBuffer) {
									const int kReasonableBFrameBufferLimit = 100;

									++nFramesPushedTryingToFlushCodec;

									// DivX 5.0.5 seems to have a bug where in the second pass of a multipass operation
									// it outputs an endless number of delay frames at the end!  This causes us to loop
									// infinitely trying to flush a codec delay that never ends.  Unfortunately, there is
									// one case where such a string of delay frames is valid: when the length of video
									// being compressed is shorter than the B-frame delay.  We attempt to detect when
									// this situation occurs and avert the loop.

									if (bVideoNonDelayedFrameReceived) {
										VDLogAppMessage(kVDLogWarning, kVDST_Dub, kVDM_CodecDelayedDuringDelayedFlush);
										--nVideoFramesDelayed;	// cancel increment below (might underflow but that's OK)
									} else if (nFramesPushedTryingToFlushCodec > kReasonableBFrameBufferLimit) {
										VDLogAppMessage(kVDLogWarning, kVDST_Dub, kVDM_CodecLoopingDuringDelayedFlush, 1, &kReasonableBFrameBufferLimit);
										nVideoFramesDelayed = 0;
										continue;
									}
								}
								++nVideoFramesDelayed;
							}

							bVideoNonDelayedFrameReceived = true;

							if (fPreview && mbAudioPresent) {
								static_cast<AVIAudioPreviewOutputStream *>(mpAudioOut)->start();
								mbAudioFrozenValid = true;
							}

							if (pFrameInfo != &dummyFrameInfo)
								mpVideoPipe->releaseBuffer();

							if (result == kVideoWriteOK || result == kVideoWriteDiscarded)
								break;
						}
						++mpVInfo->cur_proc_dst;
					}
				} else if (stream == 1) {
					mProcessingProfileChannel.Begin(0xe0e0ff, "Audio");

					const int nBlockAlign = mpAudioPipe->GetSampleSize();
					int bytes = count * nBlockAlign;
					int bytesread = 0;

					if (audioBuffer.size() < bytes)
						audioBuffer.resize(bytes);

					while(bytesread < bytes) {
						int tc = mpAudioPipe->Read(&audioBuffer[bytesread], bytes-bytesread);

						if (*mpAbort)
							goto abort_requested;

						if (!tc) {
							if (mpAudioPipe->isInputClosed()) {
								mpAudioPipe->CloseOutput();
								bytesread -= bytesread % nBlockAlign;
								count = bytesread / nBlockAlign;
								if (mpInterleaver)
									mpInterleaver->EndStream(1);
								mpAudioOut->finish();
								bAudioEnded = true;
								break;
							}

							VDDubAutoThreadLocation loc(mpCurrentAction, "waiting for audio data from I/O thread");
							mpAudioPipe->ReadWait();
						}

						bytesread += tc;
					}

					// apply audio correction on the fly if we are doing L3
					//
					// NOTE: Don't begin correction until we have at least 20 MPEG frames.  The error
					//       is generally under 5% and we don't want the beginning of the stream to go
					//       nuts.
					if (mpAudioCorrector && mpAudioCorrector->GetFrameCount() >= 20) {
						vdstructex<WAVEFORMATEX> wfex((const WAVEFORMATEX *)mpAudioOut->getFormat(), mpAudioOut->getFormatLen());
						
						double bytesPerSec = mpAudioCorrector->ComputeByterateDouble(wfex->nSamplesPerSec);

						if (mpInterleaver)
							mpInterleaver->AdjustStreamRate(1, bytesPerSec / (double)mpVInfo->frameRate);
						UpdateAudioStreamRate();
					}

					mProcessingProfileChannel.End();
					if (count > 0) {
						mProcessingProfileChannel.Begin(0xe0e0ff, "A-Write");
						{
							VDDubAutoThreadLocation loc(mpCurrentAction, "writing audio data to disk");
							WriteAudio(&audioBuffer.front(), bytesread, count);
						}

						if (firstPacket && fPreview) {
							mpAudioOut->flush();
							mpBlitter->enablePulsing(true);
							firstPacket = false;
							mbAudioFrozen = false;
						}
						mProcessingProfileChannel.End();
					}

				} else {
					VDNEVERHERE;
				}
			}

			CHECK_STACK(sp);

			if (*mpAbort)
				break;

			if (bVideoEnded && bAudioEnded)
				break;
		}
abort_requested:
		;

	} catch(MyError& e) {
		if (!mbError) {
			mError.TransferFrom(e);
			mbError = true;
		}
		mpVideoPipe->abort();
		*mpAbort = true;
	}

	mpVideoPipe->finalizeAck();
	mpAudioPipe->CloseOutput();

	// attempt a graceful shutdown at this point...
	try {
		// if preview mode, choke the audio

		if (mpAudioOut && mpOutputSystem->IsRealTime())
			static_cast<AVIAudioPreviewOutputStream *>(mpAudioOut)->stop();

		// finalize the output.. if it's not a preview...
		if (!mpOutputSystem->IsRealTime()) {
			// update audio rate...

			if (mpAudioCorrector) {
				UpdateAudioStreamRate();
			}

			// finalize avi
			mpAudioOut = NULL;
			mpVideoOut = NULL;
			IVDMediaOutput *temp = mpAVIOut;
			mpAVIOut = NULL;
			mpOutputSystem->CloseSegment(temp, true);
			VDDEBUG("Dub/Processor: finalized.\n");
		}
	} catch(MyError& e) {
		if (!mbError) {
			mError.TransferFrom(e);
			mbError = true;
		}
	}

	VDDEBUG("Dub/Processor: end\n");

	*mpAbort = true;
}

#define BUFFERID_INPUT (1)
#define BUFFERID_OUTPUT (2)

namespace {
	bool AsyncUpdateCallback(int pass, void *pDisplayAsVoid, void *pInterlaced, bool aborting) {
		if (aborting)
			return false;

		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;
		int nFieldMode = *(int *)pInterlaced;

		uint32 baseFlags = IVDVideoDisplay::kVisibleOnly;

		if (g_prefs.fDisplay & Preferences::kDisplayEnableVSync)
			baseFlags |= IVDVideoDisplay::kVSync;

		if (nFieldMode) {
			if ((pass^nFieldMode)&1)
				pVideoDisplay->Update(IVDVideoDisplay::kEvenFieldOnly | baseFlags);
			else
				pVideoDisplay->Update(IVDVideoDisplay::kOddFieldOnly | baseFlags);

			return !pass;
		} else {
			pVideoDisplay->Update(IVDVideoDisplay::kAllFields | baseFlags);
			return false;
		}
	}
}

VDDubProcessThread::VideoWriteResult VDDubProcessThread::WriteVideoFrame(void *buffer, int exdata, int droptype, LONG lastSize, VDPosition sample_num, VDPosition display_num, VDPosition timeline_num, int srcIndex) {
	uint32 dwBytes;
	bool isKey;
	const void *frameBuffer;
	IVDVideoSource *vsrc = mVideoSources[srcIndex];

	if (timeline_num >= 0)		// exclude injected frames
		mpVInfo->cur_proc_src = timeline_num;

	// Preview fast drop -- if there is another keyframe in the pipe, we can drop
	// all the frames to it without even decoding them!
	//
	// Anime song played during development of this feature: "Trust" from the
	// Vandread OST.

	if (mpOutputSystem->IsRealTime()) {
		bool bDrop = false;

		if (opt->perf.fDropFrames) {
			// If audio is frozen, force frames to be dropped.
			bDrop = !vsrc->isDecodable(sample_num);

			if (mbAudioFrozen && mbAudioFrozenValid) {
				lDropFrames = 1;
			}

			if (lDropFrames) {
				long lCurrentDelta = mpBlitter->getFrameDelta();
				if (opt->video.nPreviewFieldMode)
					lCurrentDelta >>= 1;
				if (lDropFrames > lCurrentDelta) {
					lDropFrames = lCurrentDelta;
					if (lDropFrames < 0)
						lDropFrames = 0;
				}
			}

			if (lDropFrames && !bDrop) {

				// Attempt to drop a frame before the decoder.  Droppable frames (zero-byte
				// or B-frames) can be dropped without any problem without question.  Dependant
				// (P-frames or delta frames) and independent frames (I-frames or keyframes)
				// should only be dropped if there is a reasonable expectation that another
				// independent frame will arrive around the time that we want to stop dropping
				// frames, since we'll basically kill decoding until then.

				if (droptype == AVIPipe::kDroppable) {
					bDrop = true;
				} else {
					int total, indep;

					mpVideoPipe->getDropDistances(total, indep);

					// Do a blind drop if we know a keyframe will arrive within two frames.

					if (indep == 0x3FFFFFFF && vsrc->nearestKey(display_num + opt->video.frameRateDecimation*2) > display_num)
						indep = 0;

					if (indep > 0 && indep < lDropFrames) {
						bDrop = true;
					}
				}
			}
		}

		// Zero-byte drop frame? Just nuke it now.
		if (!lastSize && (!(exdata & kBufferFlagInternalDecode) || (exdata & kBufferFlagSameAsLast)))
			bDrop = true;

		if (bDrop) {
			if (!(exdata&kBufferFlagPreload)) {
				mpBlitter->nextFrame(opt->video.nPreviewFieldMode ? 2 : 1);
			}
			++mfsi.lCurrentFrame;
			if (lDropFrames)
				--lDropFrames;

			if (mpStatusHandler)
				mpStatusHandler->NotifyNewFrame(0);

			return kVideoWriteDiscarded;
		}
	}

//	VDDEBUG("Handling frame %d -> %d (%6d bytes) as %s\n", (int)sample_num, (int)display_num, (int)lastSize, (exdata & kBufferFlagDirectWrite) ? "DIRECT" : (exdata & kBufferFlagPreload) ? "PRELOAD" : "FULL");

	// With Direct mode, write video data directly to output.
	if ((exdata & kBufferFlagDirectWrite) || (opt->video.mbPreserveEmptyFrames && !lastSize && (exdata & kBufferFlagSameAsLast))) {
		mpVideoOut->write((exdata & kBufferFlagDelta) ? 0 : AVIIF_KEYFRAME, (char *)buffer, lastSize, 1);

		mpVInfo->total_size += lastSize + 24;
		mpVInfo->lastProcessedTimestamp = VDGetCurrentTick();
		++mpVInfo->processed;
		if (mpStatusHandler)
			mpStatusHandler->NotifyNewFrame(lastSize | (exdata&1 ? 0x80000000 : 0));

		if (exdata & kBufferFlagDirectWrite) {
			if (mpVideoCompressor)
				mpVideoCompressor->Restart();
		}

		return kVideoWriteOK;
	}

	// Fast Repack: Decompress data and send to compressor (possibly non-RGB).
	// Slow Repack: Decompress data and send to compressor.
	// Full:		Decompress, process, filter, convert, send to compressor.

	mProcessingProfileChannel.Begin(0xe0e0e0, "V-Lock1");
	bool bLockSuccessful;

	const bool fPreview = mpOutputSystem->IsRealTime();
	do {
		bLockSuccessful = mpBlitter->lock(BUFFERID_INPUT, fPreview ? 500 : -1);
	} while(!bLockSuccessful && !*mpAbort);
	mProcessingProfileChannel.End();

	if (!bLockSuccessful)
		return kVideoWriteDiscarded;

	if (exdata & kBufferFlagPreload)
		mProcessingProfileChannel.Begin(0xfff0f0, "V-Preload");
	else
		mProcessingProfileChannel.Begin(0xffe0e0, "V-Decode");

	VDCHECKPOINT;
	CHECK_FPU_STACK
	{
		VDDubAutoThreadLocation loc(mpCurrentAction, "decompressing video frame");
		vsrc->streamGetFrame(buffer, lastSize, false, sample_num);
	}
	CHECK_FPU_STACK

	VDCHECKPOINT;

	mProcessingProfileChannel.End();

	if (exdata & kBufferFlagPreload) {
		mpBlitter->unlock(BUFFERID_INPUT);
		return kVideoWriteBuffered;
	}

	if (lDropFrames && fPreview) {
		mpBlitter->unlock(BUFFERID_INPUT);
		mpBlitter->nextFrame(opt->video.nPreviewFieldMode ? 2 : 1);
		++mfsi.lCurrentFrame;

//		VDDEBUG2("Dubber: skipped frame\n");
		--lDropFrames;

		if (mpStatusHandler)
			mpStatusHandler->NotifyNewFrame(0);

		return kVideoWriteDiscarded;
	}

	// Process frame to backbuffer for Full video mode.  Do not process if we are
	// running in Repack mode only!
	if (opt->video.mode == DubVideoOptions::M_FULL) {
		VBitmap *initialBitmap = filters.InputBitmap();
		VBitmap *lastBitmap = filters.LastBitmap();
		VBitmap destbm;
		VDPosition startOffset = vsrc->asStream()->getStart();

		if (!mpInvTelecine && filters.isEmpty()) {
			mProcessingProfileChannel.Begin(0xe0e0e0, "V-Lock2");
			mpBlitter->lock(BUFFERID_OUTPUT);
			mProcessingProfileChannel.End();
			VDPixmapBlt(mVideoFilterOutputPixmap, vsrc->getTargetFormat());
		} else {
			if (mpInvTelecine) {
				VBitmap srcbm((void *)vsrc->getFrameBuffer(), vsrc->getDecompressedFormat());

				VDPosition timelineFrameOut, srcFrameOut;
				bool valid = mpInvTelecine->ProcessOut(initialBitmap, srcFrameOut, timelineFrameOut);

				mpInvTelecine->ProcessIn(&srcbm, display_num, timeline_num);

				if (!valid) {
					mpBlitter->unlock(BUFFERID_INPUT);
					return kVideoWriteBuffered;
				}

				timeline_num = timelineFrameOut;
				display_num = srcFrameOut;
			} else
				VDPixmapBlt(VDAsPixmap(*initialBitmap), vsrc->getTargetFormat());

			// process frame

			mfsi.lCurrentSourceFrame	= (long)(display_num - startOffset);
			mfsi.lCurrentFrame			= (long)timeline_num;
			mfsi.lSourceFrameMS			= (long)mpVInfo->frameRateIn.scale64ir(mfsi.lCurrentSourceFrame * (sint64)1000);
			mfsi.lDestFrameMS			= (long)mpVInfo->frameRateIn.scale64ir(mfsi.lCurrentFrame * (sint64)1000);

			mProcessingProfileChannel.Begin(0x008000, "V-Filter");
			bool frameOutput = filters.RunFilters(mfsi);
			mProcessingProfileChannel.End();

			++mfsi.lCurrentFrame;

			if (!frameOutput) {
				mpBlitter->unlock(BUFFERID_INPUT);
				return kVideoWriteBuffered;
			}

			mProcessingProfileChannel.Begin(0xe0e0e0, "V-Lock2");
			mpBlitter->lock(BUFFERID_OUTPUT);
			mProcessingProfileChannel.End();

			VDPixmapBlt(mVideoFilterOutputPixmap, VDAsPixmap(*lastBitmap));
		}
	}

	// write it to the file
	
	frameBuffer = mpVideoFilterOutputBuffer;
	if (!frameBuffer)
		frameBuffer = vsrc->getFrameBuffer();

	if (mpVideoCompressor) {
		bool gotFrame;

		mProcessingProfileChannel.Begin(0x80c080, "V-Compress");
		{
			VDDubAutoThreadLocation loc(mpCurrentAction, "compressing video frame");
			gotFrame = mpVideoCompressor->CompressFrame(mVideoCompressionBuffer.data(), frameBuffer, isKey, dwBytes);
		}
		mProcessingProfileChannel.End();

		// Check if codec buffered a frame.
		if (!gotFrame) {
			return kVideoWriteDelayed;
		}

		VDDubAutoThreadLocation loc(mpCurrentAction, "writing compressed video frame to disk");
		mpVideoOut->write(isKey ? AVIIF_KEYFRAME : 0, mVideoCompressionBuffer.data(), dwBytes, 1);

	} else {

		dwBytes = ((const BITMAPINFOHEADER *)mpVideoOut->getFormat())->biSizeImage;

		VDCHECKPOINT;
		{
			VDDubAutoThreadLocation loc(mpCurrentAction, "writing uncompressed video frame to disk");
			mpVideoOut->write(AVIIF_KEYFRAME, (char *)frameBuffer, dwBytes, 1);
		}
		VDCHECKPOINT;

		isKey = true;
	}

	mpVInfo->total_size += dwBytes + 24;

	VDCHECKPOINT;

	if (fPreview || mRefreshFlag.xchg(0)) {
		if (mpInputDisplay && opt->video.fShowInputFrame) {
			mpBlitter->postAPC(BUFFERID_INPUT, AsyncUpdateCallback, mpInputDisplay, (void *)&opt->video.nPreviewFieldMode);
		} else
			mpBlitter->unlock(BUFFERID_INPUT);

		if (mpOutputDisplay && opt->video.mode == DubVideoOptions::M_FULL && opt->video.fShowOutputFrame && dwBytes) {
			mpBlitter->postAPC(BUFFERID_OUTPUT, AsyncUpdateCallback, mpOutputDisplay, (void *)&opt->video.nPreviewFieldMode);
		} else
			mpBlitter->unlock(BUFFERID_OUTPUT);
	} else {
		mpBlitter->unlock(BUFFERID_OUTPUT);
		mpBlitter->unlock(BUFFERID_INPUT);
	}

	if (opt->perf.fDropFrames && fPreview) {
		long lFrameDelta;

		lFrameDelta = mpBlitter->getFrameDelta();

		if (opt->video.nPreviewFieldMode)
			lFrameDelta >>= 1;

		if (lFrameDelta < 0) lFrameDelta = 0;
		
		if (lFrameDelta > 0) {
			lDropFrames = lFrameDelta;
//			VDDEBUG2("Dubber: Skipping %d frames\n", lDropFrames);
		}
	}


	mpBlitter->nextFrame(opt->video.nPreviewFieldMode ? 2 : 1);

	mpVInfo->lastProcessedTimestamp = VDGetCurrentTick();
	++mpVInfo->processed;

	if (mpStatusHandler)
		mpStatusHandler->NotifyNewFrame(isKey ? dwBytes : dwBytes | 0x80000000);

	VDCHECKPOINT;
	return kVideoWriteOK;
}

void VDDubProcessThread::WriteAudio(void *buffer, long lActualBytes, long lActualSamples) {
	if (!lActualBytes) return;

	mpAudioOut->write(AVIIF_KEYFRAME, (char *)buffer, lActualBytes, lActualSamples);
}

void VDDubProcessThread::TimerCallback() {
	if (opt->video.fSyncToAudio) {
		if (mpAudioOut) {
			long lActualPoint;
			AVIAudioPreviewOutputStream *pAudioOut = static_cast<AVIAudioPreviewOutputStream *>(mpAudioOut);

			lActualPoint = pAudioOut->getPosition();

			if (!pAudioOut->isFrozen()) {
				long mPulseClock;

				if (opt->video.nPreviewFieldMode) {
					mPulseClock = MulDiv(lActualPoint, 2000, mpVInfo->usPerFrame);
				} else {
					mPulseClock = MulDiv(lActualPoint, 1000, mpVInfo->usPerFrame);
				}

				if (mPulseClock<0)
					mPulseClock = 0;

				if (lActualPoint != -1) {
					mpBlitter->setPulseClock(mPulseClock);
					mbSyncToAudioEvenClock = false;
					mbAudioFrozen = false;
					return;
				}
			}
		}

		// Audio's frozen, so we have to free-run.  When 'sync to audio' is on
		// and field-based preview is off, we have to divide the 2x clock down
		// to 1x.

		mbAudioFrozen = true;

		if (mbSyncToAudioEvenClock || opt->video.nPreviewFieldMode) {
			if (mpBlitter) {
				mpBlitter->pulse(1);
			}
		}

		mbSyncToAudioEvenClock = !mbSyncToAudioEvenClock;

		return;
	}

	// When 'sync to audio' is off we use a 1x clock.
	if (mpBlitter)
		mpBlitter->pulse(1);
}

void VDDubProcessThread::UpdateAudioStreamRate() {
	vdstructex<WAVEFORMATEX> wfex((const WAVEFORMATEX *)mpAudioOut->getFormat(), mpAudioOut->getFormatLen());
	
	wfex->nAvgBytesPerSec = mpAudioCorrector->ComputeByterate(wfex->nSamplesPerSec);

	mpAudioOut->setFormat(&*wfex, wfex.size());

	AVIStreamHeader_fixed hdr(mpAudioOut->getStreamInfo());
	hdr.dwRate = wfex->nAvgBytesPerSec * hdr.dwScale;
	mpAudioOut->updateStreamInfo(hdr);
}
