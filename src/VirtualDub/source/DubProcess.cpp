#include "stdafx.h"
#include <vd2/Dita/resources.h>
#include <vd2/system/error.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Riza/display.h>
#include <vd2/Riza/videocodec.h>
#include <vd2/Riza/bitmap.h>
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
#include "filters.h"
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

	enum {
		// This is to work around an XviD decode bug (see VideoSource.h).
		kDecodeOverflowWorkaroundSize = 16
	};
};

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
			if (nFieldMode == 2) {
				if (pass)
					pVideoDisplay->Update(IVDVideoDisplay::kEvenFieldOnly | baseFlags);
				else
					pVideoDisplay->Update(IVDVideoDisplay::kOddFieldOnly | IVDVideoDisplay::kFirstField | baseFlags);
			} else {
				if (pass)
					pVideoDisplay->Update(IVDVideoDisplay::kOddFieldOnly | baseFlags);
				else
					pVideoDisplay->Update(IVDVideoDisplay::kEvenFieldOnly | IVDVideoDisplay::kFirstField | baseFlags);
			}

			return !pass;
		} else {
			pVideoDisplay->Update(IVDVideoDisplay::kAllFields | baseFlags);
			return false;
		}
	}

	bool AsyncDecompressorFailedCallback(int pass, void *pDisplayAsVoid, void *, bool aborting) {
		if (aborting)
			return false;

		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;

		pVideoDisplay->SetSourceMessage(L"Unable to display compressed video: no decompressor is available to decode the compressed video.");
		return false;
	}

	bool AsyncDecompressorSuccessfulCallback(int pass, void *pDisplayAsVoid, void *, bool aborting) {
		if (aborting)
			return false;

		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;

		pVideoDisplay->SetSourceMessage(L"Compressed video preview is enabled.\n\nPreview will resume starting with the next key frame.");
		return false;
	}

	bool AsyncDecompressorErrorCallback(int pass, void *pDisplayAsVoid, void *, bool aborting) {
		if (aborting)
			return false;

		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;

		pVideoDisplay->SetSourceMessage(L"Unable to display compressed video: An error has occurred during decompression.");
		return false;
	}

	bool AsyncDecompressorUpdateCallback(int pass, void *pDisplayAsVoid, void *pPixmapAsVoid, bool aborting) {
		if (aborting)
			return false;

		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;
		VDPixmap *pPixmap = (VDPixmap *)pPixmapAsVoid;

		pVideoDisplay->SetSource(true, *pPixmap);
		return false;
	}
}

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
	, mbCompleted(false)
	, mpCurrentAction("starting up")
	, mActivityCounter(0)
	, mRefreshFlag(1)
	, mProcessingProfileChannel("Processor")
	, mpVideoFilterOutputBuffer(NULL)
	, mPendingNullVideoFrames(0)
	, mpInvTelecine(NULL)
	, mbVideoDecompressorEnabled(false)
	, mpAbort(NULL)
	, mpStatusHandler(NULL)
{
}

VDDubProcessThread::~VDDubProcessThread() {
}

void VDDubProcessThread::SetParent(IDubberInternal *pParent) {
	mpParent = pParent;
}

void VDDubProcessThread::SetAbortSignal(VDAtomicInt *pAbort) {
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

void VDDubProcessThread::SetVideoFilterOutput(void *pBuffer, const VDPixmap& px) {
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
		mVideoCompressionBuffer.resize(pCompressor->GetMaxOutputSize() + kDecodeOverflowWorkaroundSize);
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
		int timerInterval = VDClampToSint32(mpVInfo->frameRate.scale64ir(1000));

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

void VDDubProcessThread::SetThrottle(float f) {
	mLoopThrottle.SetThrottleFactor(f);
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
	bool bVideoEnded = !(vSrc && mpOutputSystem->AcceptsVideo());
	bool bVideoNonDelayedFrameReceived = false;
	uint32	nVideoFramesDelayed = 0;

	lDropFrames = 0;
	mpVInfo->processed = 0;

	mbAudioEnded = !(mbAudioPresent && mpOutputSystem->AcceptsAudio());
	mbFirstPacket = false;
	mbPreview = mpOutputSystem->IsRealTime();

	int lastVideoSourceIndex = 0;

	IVDMediaOutputAutoInterleave *pOutAI = vdpoly_cast<IVDMediaOutputAutoInterleave *>(mpAVIOut);

	if (pOutAI)
		mpInterleaver = NULL;

	try {
		mpCurrentAction = "running main loop";

		for(;;) {
			int stream;
			sint32 count;

			if (!mLoopThrottle.Delay()) {
				++mActivityCounter;
				if (*mpAbort)
					break;
				continue;
			}

			VDStreamInterleaver::Action nextAction;
			
			if (mpInterleaver)
				nextAction = mpInterleaver->GetNextAction(stream, count);
			else {
				if (mbAudioEnded && bVideoEnded)
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
						if (mbPreview && mbAudioPresent) {
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

								mLoopThrottle.BeginWait();
								pFrameInfo = mpVideoPipe->getReadBuffer();
								mLoopThrottle.EndWait();
							}

							// Check if we have frames buffered in the codec due to B-frame encoding. If we do,
							// we can't immediately switch from compression to direct copy for smart rendering
							// purposes -- we have to flush the B-frames first.
							//
							// Note that there is a trick here: we DON'T wait until the whole frame queue is
							// flushed here. The reason is that the codec's frame delay has caused us to read
							// farther ahead in the frame queue than the interleaver is tracking. We need to
							// only write out one frame whenever requested to maintain interleaving; this
							// effectively stalls the input pipe until the queue is drained. It looks something
							// like this (assume the codec has a delay of 2):
							//
							//	Interleaver request		0			1	2	3	4	5	6	7	8	9	10	11	12	13	14	15
							//	Input pipe				0F	1F	2F	3F	4F	5F	6F	(*)	(*)	7D	8D	9D	10D	11D	12D	13F	14F	15F	-	-
							//	Codec input				0	1	2	3	4	5	6									13	14	15	-	-
							//	Codec output			-	-	0	1	2	3	4	5	6							-	-	13	14	15
							//	Written to disk			-	-	0	1	2	3	4	5	6	7D	8D	9D	10D	11D	12D	-	-	13	14	15
							//
							// The key is at timeline frame 7, which gets hit at interleaver frame 5 due to
							// the delay. This is the point at which we switch from full frames to direct mode.
							// The asterisks (*) in the chart indicate where we stalling the input pipe and
							// pushing in null frames to clear out the delay.
							//
							if (pFrameInfo && pFrameInfo->mFlags & kBufferFlagDirectWrite) {
								bVideoNonDelayedFrameReceived = false;
								if (nVideoFramesDelayed > 0 && pFrameInfo)
									pFrameInfo = NULL;
							}

							if (!pFrameInfo) {
								if (nVideoFramesDelayed > 0) {
									dummyFrameInfo.mpData			= "";
									dummyFrameInfo.mLength			= 0;
									dummyFrameInfo.mRawFrame		= -1;
									dummyFrameInfo.mOrigDisplayFrame	= -1;
									dummyFrameInfo.mDisplayFrame	= -1;
									dummyFrameInfo.mTimelineFrame	= -1;
									dummyFrameInfo.mTargetFrame		= -1;
									dummyFrameInfo.mFlags			= kBufferFlagDelta | kBufferFlagFlushCodec;
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

							if (mbFirstPacket && mbPreview && !mbAudioPresent) {
								mpBlitter->enablePulsing(true);
								mbFirstPacket = false;
							}

							VideoWriteResult result = WriteVideoFrame(
								pFrameInfo->mpData,
								pFrameInfo->mFlags,
								pFrameInfo->mDroptype,
								pFrameInfo->mLength,
								pFrameInfo->mRawFrame,
								pFrameInfo->mTargetFrame,
								pFrameInfo->mOrigDisplayFrame,
								pFrameInfo->mDisplayFrame,
								pFrameInfo->mTimelineFrame,
								pFrameInfo->mSequenceFrame,
								pFrameInfo->mSrcIndex);

							// If we pushed an empty frame that was pending, we didn't actually process a real frame in
							// the queue and should exit now.
							if (result == kVideoWritePushedPendingEmptyFrame)
								break;

							// If we pushed a frame to empty the video codec's B-frame queue, decrement the count here.
							// Note that we must do this AFTER we check if an empty frame has been pushed, because in
							// that case no frame actually got pushed through the video codec.
							if (bAttemptingToFlushCodecBuffer)
								--nVideoFramesDelayed;

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

							if (mbPreview && mbAudioPresent) {
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
					if (!WriteAudio(count))
						goto abort_requested;
				} else {
					VDNEVERHERE;
				}
			}

			if (*mpAbort)
				break;

			if (bVideoEnded && mbAudioEnded)
				break;

			// check for video decompressor switch
			if (mpVideoCompressor && mbVideoDecompressorEnabled != opt->video.fShowDecompressedFrame) {
				mbVideoDecompressorEnabled = opt->video.fShowDecompressedFrame;

				if (mbVideoDecompressorEnabled) {
					mbVideoDecompressorErrored = false;
					mbVideoDecompressorPending = true;

					const BITMAPINFOHEADER *pbih = (const BITMAPINFOHEADER *)mpVideoCompressor->GetOutputFormat();
					mpVideoDecompressor = VDFindVideoDecompressor(0, pbih);

					if (mpVideoDecompressor) {
						if (!mpVideoDecompressor->SetTargetFormat(0))
							mpVideoDecompressor = NULL;
						else {
							try {
								mpVideoDecompressor->Start();

								mLoopThrottle.BeginWait();
								mpBlitter->lock(BUFFERID_OUTPUT);
								mLoopThrottle.EndWait();
								mpBlitter->postAPC(BUFFERID_OUTPUT, AsyncDecompressorSuccessfulCallback, mpOutputDisplay, NULL);					

								int format = mpVideoDecompressor->GetTargetFormat();
								int variant = mpVideoDecompressor->GetTargetFormatVariant();

								VDPixmapLayout layout;
								VDMakeBitmapCompatiblePixmapLayout(layout, abs(pbih->biWidth), abs(pbih->biHeight), format, variant);

								mVideoDecompBuffer.init(layout);
							} catch(const MyError&) {
								mpVideoDecompressor = NULL;
							}
						}
					}

					if (!mpVideoDecompressor) {
						mLoopThrottle.BeginWait();
						mpBlitter->lock(BUFFERID_OUTPUT);
						mLoopThrottle.EndWait();
						mpBlitter->postAPC(BUFFERID_OUTPUT, AsyncDecompressorFailedCallback, mpOutputDisplay, NULL);
					}
				} else {
					if (mpVideoDecompressor) {
						mLoopThrottle.BeginWait();
						mpBlitter->lock(BUFFERID_OUTPUT);
						mLoopThrottle.EndWait();
						mpBlitter->unlock(BUFFERID_OUTPUT);
						mpVideoDecompressor->Stop();
						mpVideoDecompressor = NULL;
					}
					mpBlitter->postAPC(0, AsyncReinitDisplayCallback, this, NULL);
				}
			}
		}
abort_requested:
		;

	} catch(MyError& e) {
		if (!mbError) {
			mError.TransferFrom(e);
			mbError = true;
		}

		mpVideoPipe->abort();
		mpParent->InternalSignalStop();
	}

	mbCompleted = mbAudioEnded && bVideoEnded;

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
		}
	} catch(MyError& e) {
		if (!mbError) {
			mError.TransferFrom(e);
			mbError = true;
		}
	}

	mpParent->InternalSignalStop();
}

VDDubProcessThread::VideoWriteResult VDDubProcessThread::WriteVideoFrame(void *buffer, int exdata, int droptype, LONG lastSize, VDPosition sample_num, VDPosition target_num, VDPosition orig_display_num, VDPosition display_num, VDPosition timeline_num, VDPosition sequence_num, int srcIndex) {
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
		if (opt->video.mbPreserveEmptyFrames || (opt->video.mode == DubVideoOptions::M_FULL && filters.isEmpty())) {
			if (!lastSize && (!(exdata & kBufferFlagInternalDecode) || (exdata & kBufferFlagSameAsLast)))
				bDrop = true;
		}

		if (bDrop) {
			if (!(exdata&kBufferFlagPreload)) {
				mpBlitter->nextFrame(opt->video.nPreviewFieldMode ? 2 : 1);
			}

			if (lDropFrames)
				--lDropFrames;

			if (mpStatusHandler)
				mpStatusHandler->NotifyNewFrame(0);

			return kVideoWriteDiscarded;
		}
	}

	// We have to handle the case when the "preserve empty frames" option is enabled
	// and a B-frame encoding delay is present. The problem is that the empty frames
	// are interspersed with the *input* frames, but they need to go out with the
	// *output* frames. What we do is collapse runs of the preserved empty frames
	// and track them in parallel with the video codec's queue as
	// mVideoNullFrameDelayQueue.
	//
	// The following sequence diagram shows how this works:
	//
	//	Input buffer	0	1n	2n	3	4n	5	6	7n	8n	9
	//	Codec input		0			3		5	6			9		-	-
	//	Codec output	-			-			0			3		5	6			9
	//	Output buffer							0	1n	2n	3	4n	5	6	7n	8n	9
	//
	// The catch is that this ALSO has an interaction with the smart rendering
	// option in that the pending frames must also be flushed whenever a switch to
	// direct mode occurs.

	bool preservingEmptyFrame = opt->video.mbPreserveEmptyFrames && !lastSize && (exdata & kBufferFlagSameAsLast);

	if (preservingEmptyFrame && !mVideoNullFrameDelayQueue.empty()) {
		// We have an empty frame that we want to preserve, but can't write it out yet
		// because the adjacent frames are still buffered in the codec, so we must
		// buffer this too.

		if (mpStatusHandler)
			mpStatusHandler->NotifyNewFrame(lastSize | (exdata&1 ? 0x80000000 : 0));

		if (mpVideoCompressor)
			mpVideoCompressor->SkipFrame();

		++mVideoNullFrameDelayQueue.back();
		return kVideoWriteBufferedEmptyFrame;
	}

	// We're about to process and write out some non-trivial frame, so if we have pending
	// empty frames, write one of those out instead.
	if (mPendingNullVideoFrames) {
		--mPendingNullVideoFrames;

		WritePendingEmptyVideoFrame();
		return kVideoWritePushedPendingEmptyFrame;
	}

	// If:
	//
	//	- we're in direct mode, or
	//	- we've got an empty frame and "Preserve Empty Frames" is enabled
	//
	// ...write it directly to the output and skip the rest.

	if ((exdata & kBufferFlagDirectWrite) || preservingEmptyFrame) {
		uint32 flags = (exdata & kBufferFlagDelta) || !(exdata & kBufferFlagDirectWrite) ? 0 : AVIOutputStream::kFlagKeyFrame;
		mpVideoOut->write(flags, (char *)buffer, lastSize, 1);

		mpVInfo->total_size += lastSize + 24;
		mpVInfo->lastProcessedTimestamp = VDGetCurrentTick();
		++mpVInfo->processed;
		if (mpStatusHandler)
			mpStatusHandler->NotifyNewFrame(lastSize | (exdata&1 ? 0x80000000 : 0));

		if (mpVideoCompressor) {
			mpVideoCompressor->SkipFrame();

			if (exdata & kBufferFlagDirectWrite)
				mpVideoCompressor->Restart();
		}

		mVideoNullFrameDelayQueue.clear();

		return kVideoWriteOK;
	}

	// Fast Repack: Decompress data and send to compressor (possibly non-RGB).
	// Slow Repack: Decompress data and send to compressor.
	// Full:		Decompress, process, filter, convert, send to compressor.

	mProcessingProfileChannel.Begin(0xe0e0e0, "V-Lock1");
	bool bLockSuccessful;

	mLoopThrottle.BeginWait();
	do {
		bLockSuccessful = mpBlitter->lock(BUFFERID_INPUT, mbPreview ? 500 : -1);
	} while(!bLockSuccessful && !*mpAbort);
	mLoopThrottle.EndWait();
	mProcessingProfileChannel.End();

	if (!bLockSuccessful)
		return kVideoWriteDiscarded;

	if (exdata & kBufferFlagPreload)
		mProcessingProfileChannel.Begin(0xfff0f0, "V-Preload");
	else
		mProcessingProfileChannel.Begin(0xffe0e0, "V-Decode");

	if (!(exdata & kBufferFlagFlushCodec)) {
		VDDubAutoThreadLocation loc(mpCurrentAction, "decompressing video frame");
		vsrc->streamGetFrame(buffer, lastSize, 0 != (exdata & kBufferFlagPreload), sample_num, target_num);
	}

	mProcessingProfileChannel.End();

	if (exdata & kBufferFlagPreload) {
		mpBlitter->unlock(BUFFERID_INPUT);
		return kVideoWriteBuffered;
	}

	if (lDropFrames && mbPreview) {
		mpBlitter->unlock(BUFFERID_INPUT);
		mpBlitter->nextFrame(opt->video.nPreviewFieldMode ? 2 : 1);

		--lDropFrames;

		if (mpStatusHandler)
			mpStatusHandler->NotifyNewFrame(0);

		return kVideoWriteDiscarded;
	}

	// Process frame to backbuffer for Full video mode.  Do not process if we are
	// running in Repack mode only!
	if (opt->video.mode == DubVideoOptions::M_FULL) {
		const VDPixmap& input = filters.GetInput();
		const VDPixmap& output = filters.GetOutput();
		VBitmap destbm;

		if (exdata & kBufferFlagFlushCodec) {
			mProcessingProfileChannel.Begin(0xe0e0e0, "V-Lock2");
			mLoopThrottle.BeginWait();
			mpBlitter->lock(BUFFERID_OUTPUT);
			mLoopThrottle.EndWait();
			mProcessingProfileChannel.End();
		} else if (!mpInvTelecine && filters.isEmpty()) {
			mProcessingProfileChannel.Begin(0xe0e0e0, "V-Lock2");
			mLoopThrottle.BeginWait();
			mpBlitter->lock(BUFFERID_OUTPUT);
			mLoopThrottle.EndWait();
			mProcessingProfileChannel.End();

			VDPixmapBlt(mVideoFilterOutputPixmap, vsrc->getTargetFormat());
		} else {
			if (mpInvTelecine) {
				VDPosition timelineFrameOut, srcFrameOut;
				bool valid = mpInvTelecine->ProcessOut(input, srcFrameOut, timelineFrameOut);

				mpInvTelecine->ProcessIn(vsrc->getTargetFormat(), display_num, timeline_num);

				if (!valid) {
					mpBlitter->unlock(BUFFERID_INPUT);
					return kVideoWriteBuffered;
				}

				timeline_num = timelineFrameOut;
				display_num = srcFrameOut;
			} else
				VDPixmapBlt(input, vsrc->getTargetFormat());

			// process frame
			sint64 sequenceTime = VDRoundToInt64(mpVInfo->frameRate.AsInverseDouble() * 1000.0 * sequence_num);

			mProcessingProfileChannel.Begin(0x008000, "V-Filter");
			bool frameOutput = filters.RunFilters(orig_display_num, timeline_num, sequence_num, sequenceTime, NULL, mbPreview ? VDXFilterStateInfo::kStateRealTime | VDXFilterStateInfo::kStatePreview : 0);
			mProcessingProfileChannel.End();

			if (!frameOutput) {
				mpBlitter->unlock(BUFFERID_INPUT);
				return kVideoWriteBuffered;
			}

			mProcessingProfileChannel.Begin(0xe0e0e0, "V-Lock2");
			mLoopThrottle.BeginWait();
			mpBlitter->lock(BUFFERID_OUTPUT);
			mLoopThrottle.EndWait();
			mProcessingProfileChannel.End();

			VDPixmapBlt(mVideoFilterOutputPixmap, output);
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
			if (opt->video.mbPreserveEmptyFrames)
				mVideoNullFrameDelayQueue.push_back(0);

			return kVideoWriteDelayed;
		}

		if (mpVideoDecompressor && !mbVideoDecompressorErrored) {
			mProcessingProfileChannel.Begin(0x80c080, "V-Decompress");

			if (mbVideoDecompressorPending && isKey) {
				mbVideoDecompressorPending = false;
			}

			if (!mbVideoDecompressorPending && dwBytes) {
				try {
					memset(mVideoCompressionBuffer.data() + dwBytes, 0xA5, kDecodeOverflowWorkaroundSize);
					mpVideoDecompressor->DecompressFrame(mVideoDecompBuffer.base(), mVideoCompressionBuffer.data(), dwBytes, isKey, false);
				} catch(const MyError&) {
					mpBlitter->postAPC(0, AsyncDecompressorErrorCallback, mpOutputDisplay, NULL);
					mbVideoDecompressorErrored = true;
				}
			}

			mProcessingProfileChannel.End();
		}

		VDDubAutoThreadLocation loc(mpCurrentAction, "writing compressed video frame to disk");
		mpVideoOut->write(isKey ? AVIOutputStream::kFlagKeyFrame : 0, mVideoCompressionBuffer.data(), dwBytes, 1);

		if (opt->video.mbPreserveEmptyFrames) {
			if (!mVideoNullFrameDelayQueue.empty()) {
				VDASSERT(!mPendingNullVideoFrames);
				mPendingNullVideoFrames = mVideoNullFrameDelayQueue.front();
				mVideoNullFrameDelayQueue.pop_front();
				mVideoNullFrameDelayQueue.push_back(0);
			}
		}
	} else {

		dwBytes = ((const BITMAPINFOHEADER *)mpVideoOut->getFormat())->biSizeImage;

		VDCHECKPOINT;
		{
			VDDubAutoThreadLocation loc(mpCurrentAction, "writing uncompressed video frame to disk");
			mpVideoOut->write(AVIOutputStream::kFlagKeyFrame, (char *)frameBuffer, dwBytes, 1);
		}
		VDCHECKPOINT;

		isKey = true;
	}

	mpVInfo->total_size += dwBytes + 24;

	VDCHECKPOINT;

	bool renderFrame = mbPreview || mRefreshFlag.xchg(0);
	bool renderInputFrame = renderFrame && mpInputDisplay && opt->video.fShowInputFrame;
	bool renderOutputFrame = (renderFrame || mbVideoDecompressorEnabled) && mpOutputDisplay && opt->video.mode == DubVideoOptions::M_FULL && opt->video.fShowOutputFrame && dwBytes;

	if (renderInputFrame) {
		mpBlitter->postAPC(BUFFERID_INPUT, AsyncUpdateCallback, mpInputDisplay, (void *)&opt->video.nPreviewFieldMode);
	} else
		mpBlitter->unlock(BUFFERID_INPUT);

	if (renderOutputFrame) {
		if (mbVideoDecompressorEnabled) {
			if (mpVideoDecompressor && !mbVideoDecompressorErrored && !mbVideoDecompressorPending)
				mpBlitter->postAPC(BUFFERID_OUTPUT, AsyncDecompressorUpdateCallback, mpOutputDisplay, &mVideoDecompBuffer);
			else
				mpBlitter->unlock(BUFFERID_OUTPUT);
		} else {
			mpBlitter->postAPC(BUFFERID_OUTPUT, AsyncUpdateCallback, mpOutputDisplay, (void *)&opt->video.nPreviewFieldMode);
		}
	} else
		mpBlitter->unlock(BUFFERID_OUTPUT);

	if (opt->perf.fDropFrames && mbPreview) {
		long lFrameDelta;

		lFrameDelta = mpBlitter->getFrameDelta();

		if (opt->video.nPreviewFieldMode)
			lFrameDelta >>= 1;

		if (lFrameDelta < 0) lFrameDelta = 0;
		
		if (lFrameDelta > 0) {
			lDropFrames = lFrameDelta;
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

void VDDubProcessThread::WritePendingEmptyVideoFrame() {
	mpVideoOut->write(0, NULL, 0, 1);

	mpVInfo->total_size += 24;
	mpVInfo->lastProcessedTimestamp = VDGetCurrentTick();
	++mpVInfo->processed;
}

bool VDDubProcessThread::WriteAudio(sint32 count) {
	if (count <= 0)
		return true;

	const int nBlockAlign = mpAudioPipe->GetSampleSize();

	int totalBytes = 0;
	int totalSamples = 0;

	if (mpAudioPipe->IsVBRModeEnabled()) {
		mAudioBuffer.resize(nBlockAlign);
		char *buf = mAudioBuffer.data();

		mProcessingProfileChannel.Begin(0xe0e0ff, "Audio");
		while(totalSamples < count) {
			while(mpAudioPipe->getLevel() < sizeof(int)) {
				if (mpAudioPipe->isInputClosed()) {
					mpAudioPipe->CloseOutput();
					if (mpInterleaver)
						mpInterleaver->EndStream(1);
					mpAudioOut->finish();
					mbAudioEnded = true;
					goto ended;
				}

				VDDubAutoThreadLocation loc(mpCurrentAction, "waiting for audio data from I/O thread");
				mLoopThrottle.BeginWait();
				mpAudioPipe->ReadWait();
				mLoopThrottle.EndWait();
			}

			int sampleSize;
			int tc = mpAudioPipe->ReadPartial(&sampleSize, sizeof(int));
			VDASSERT(tc == sizeof(int));

			VDASSERT(sampleSize <= nBlockAlign);

			int pos = 0;

			while(pos < sampleSize) {
				if (*mpAbort)
					return false;

				tc = mpAudioPipe->ReadPartial(buf + pos, sampleSize - pos);
				if (!tc) {
					if (mpAudioPipe->isInputClosed()) {
						mpAudioPipe->CloseOutput();
						if (mpInterleaver)
							mpInterleaver->EndStream(1);
						mpAudioOut->finish();
						mbAudioEnded = true;
						break;
					}

					VDDubAutoThreadLocation loc(mpCurrentAction, "waiting for audio data from I/O thread");
					mLoopThrottle.BeginWait();
					mpAudioPipe->ReadWait();
					mLoopThrottle.EndWait();
				}

				pos += tc;
				VDASSERT(pos <= sampleSize);
			}

			mProcessingProfileChannel.Begin(0xe0e0ff, "A-Write");
			{
				VDDubAutoThreadLocation loc(mpCurrentAction, "writing audio data to disk");

				mpAudioOut->write(AVIOutputStream::kFlagKeyFrame, buf, sampleSize, 1);
			}
			mProcessingProfileChannel.End();

			totalBytes += sampleSize;
			++totalSamples;
		}
ended:
		mProcessingProfileChannel.End();

		if (!totalSamples)
			return true;
	} else {
		int bytes = count * nBlockAlign;

		if (mAudioBuffer.size() < bytes)
			mAudioBuffer.resize(bytes);

		mProcessingProfileChannel.Begin(0xe0e0ff, "Audio");
		while(totalBytes < bytes) {
			int tc = mpAudioPipe->ReadPartial(&mAudioBuffer[totalBytes], bytes-totalBytes);

			if (*mpAbort)
				return false;

			if (!tc) {
				if (mpAudioPipe->isInputClosed()) {
					mpAudioPipe->CloseOutput();
					totalBytes -= totalBytes % nBlockAlign;
					count = totalBytes / nBlockAlign;
					if (mpInterleaver)
						mpInterleaver->EndStream(1);
					mpAudioOut->finish();
					mbAudioEnded = true;
					break;
				}

				VDDubAutoThreadLocation loc(mpCurrentAction, "waiting for audio data from I/O thread");
				mLoopThrottle.BeginWait();
				mpAudioPipe->ReadWait();
				mLoopThrottle.EndWait();
			}

			totalBytes += tc;
		}
		mProcessingProfileChannel.End();

		if (totalBytes <= 0)
			return true;

		totalSamples = totalBytes / nBlockAlign;

		mProcessingProfileChannel.Begin(0xe0e0ff, "A-Write");
		{
			VDDubAutoThreadLocation loc(mpCurrentAction, "writing audio data to disk");

			mpAudioOut->write(AVIOutputStream::kFlagKeyFrame, mAudioBuffer.data(), totalBytes, totalSamples);
		}
		mProcessingProfileChannel.End();
	}

	// if audio and video are ready, start preview
	if (mbFirstPacket && mbPreview) {
		mpAudioOut->flush();
		mpBlitter->enablePulsing(true);
		mbFirstPacket = false;
		mbAudioFrozen = false;
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
			mpInterleaver->AdjustStreamRate(1, bytesPerSec / mpVInfo->frameRate.asDouble());
		UpdateAudioStreamRate();
	}

	return true;
}

void VDDubProcessThread::TimerCallback() {
	if (opt->video.fSyncToAudio) {
		if (mpAudioOut) {
			AVIAudioPreviewOutputStream *pAudioOut = static_cast<AVIAudioPreviewOutputStream *>(mpAudioOut);

			double audioTime = pAudioOut->GetPosition();

			if (!pAudioOut->isFrozen()) {
				int mPulseClock;

				if (opt->video.nPreviewFieldMode)
					mPulseClock = VDRoundToInt32(audioTime * mpVInfo->frameRate.asDouble() * 2.0);
				else
					mPulseClock = VDRoundToInt32(audioTime * mpVInfo->frameRate.asDouble());

				if (mPulseClock < 0)
					mPulseClock = 0;

				if (audioTime >= 0) {
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

bool VDDubProcessThread::AsyncReinitDisplayCallback(int pass, void *pThisAsVoid, void *, bool aborting) {
	if (aborting)
		return false;

	VDDubProcessThread *pThis = (VDDubProcessThread *)pThisAsVoid;
	if (pThis->opt->video.mode == DubVideoOptions::M_FULL)
		pThis->mpOutputDisplay->SetSource(false, pThis->mVideoFilterOutputPixmap, NULL, 0, true, pThis->opt->video.nPreviewFieldMode>0);
	else
		pThis->mpOutputDisplay->Reset();
	return false;
}
