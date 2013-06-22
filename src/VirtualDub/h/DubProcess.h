#ifndef f_VD2_DUBPROCESS_H
#define f_VD2_DUBPROCESS_H

#include <vd2/system/thread.h>
#include <vd2/system/time.h>
#include <vd2/system/profile.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <deque>

class IVDMediaOutput;
class IVDMediaOutputStream;
class IVDDubberOutputSystem;
class VDAudioPipeline;
class DubOptions;
class IVDVideoSource;
class IVDVideoDisplay;
class AudioStream;
class VideoTelecineRemover;
class VDStreamInterleaver;
class IVDVideoCompressor;
class IVDAsyncBlitter;
class IDubStatusHandler;

class VDDubProcessThread : public VDThread, protected IVDTimerCallback {
public:
	VDDubProcessThread();
	~VDDubProcessThread();

	void SetAbortSignal(volatile bool *pAbort);
	void SetStatusHandler(IDubStatusHandler *pStatusHandler);
	void SetInputDisplay(IVDVideoDisplay *pVideoDisplay);
	void SetOutputDisplay(IVDVideoDisplay *pVideoDisplay);
	void SetVideoFilterOutput(FilterStateInfo *pfsi, void *pBuffer, const VDPixmap& px);
	void SetVideoSources(IVDVideoSource *const *pVideoSources, uint32 count);
	void SetAudioSourcePresent(bool present);
	void SetAudioCorrector(AudioStreamL3Corrector *pCorrector);
	void SetVideoIVTC(VideoTelecineRemover *pIVTC);
	void SetVideoCompressor(IVDVideoCompressor *pCompressor);

	void Init(const DubOptions& opts, DubVideoStreamInfo *pvsi, IVDDubberOutputSystem *pOutputSystem, AVIPipe *pVideoPipe, VDAudioPipeline *pAudioPipe, VDStreamInterleaver *pStreamInterleaver);
	void Shutdown();

	void Abort();
	void UpdateFrames();

	bool GetError(MyError& e) {
		if (mbError) {
			e.TransferFrom(mError);
			return true;
		}
		return false;
	}

	uint32 GetActivityCounter() {
		return mActivityCounter;
	}

	const char *GetCurrentAction() {
		return mpCurrentAction;
	}

	VDSignal *GetBlitterSignal();

	void SetThrottle(float f);

protected:
	enum VideoWriteResult {
		kVideoWriteOK,							// Frame was processed and written
		kVideoWritePushedPendingEmptyFrame,		// A pending null frame was processed instead of the current frame.
		kVideoWriteBufferedEmptyFrame,
		kVideoWriteDelayed,
		kVideoWriteBuffered,
		kVideoWriteDiscarded,
	};

	void NextSegment();

	VideoWriteResult WriteVideoFrame(void *buffer, int exdata, int droptype, LONG lastSize, VDPosition sampleFrame, VDPosition targetFrame, VDPosition displayFrame, VDPosition timelineFrame, int srcIndex);
	void WritePendingEmptyVideoFrame();
	void WriteAudio(void *buffer, long lActualBytes, long lActualSamples);

	void ThreadRun();
	void TimerCallback();
	void UpdateAudioStreamRate();

	static bool AsyncReinitDisplayCallback(int pass, void *pThisAsVoid, void *, bool aborting);

	const DubOptions		*opt;

	VDStreamInterleaver		*mpInterleaver;
	VDLoopThrottle			mLoopThrottle;

	// OUTPUT
	IVDMediaOutput			*mpAVIOut;
	IVDMediaOutputStream	*mpAudioOut;			// alias: AVIout->audioOut
	IVDMediaOutputStream	*mpVideoOut;			// alias: AVIout->videoOut
	IVDDubberOutputSystem	*mpOutputSystem;

	// AUDIO SECTION
	VDAudioPipeline			*mpAudioPipe;
	AudioStreamL3Corrector	*mpAudioCorrector;
	bool				mbAudioPresent;

	// VIDEO SECTION
	AVIPipe					*mpVideoPipe;
	IVDVideoDisplay			*mpInputDisplay;
	IVDVideoDisplay			*mpOutputDisplay;
	VideoTelecineRemover	*mpInvTelecine;

	DubVideoStreamInfo	*mpVInfo;
	IVDAsyncBlitter		*mpBlitter;
	FilterStateInfo		mfsi;
	IDubStatusHandler	*mpStatusHandler;
	IVDVideoCompressor	*mpVideoCompressor;
	vdblock<char>		mVideoCompressionBuffer;
	void				*mpVideoFilterOutputBuffer;
	VDPixmap			mVideoFilterOutputPixmap;

	typedef vdfastvector<IVDVideoSource *> VideoSources;
	VideoSources		mVideoSources;

	std::deque<uint32>	mVideoNullFrameDelayQueue;		///< This is a queue used to track null frames between non-null frames. It runs parallel to a video codec's internal B-frame delay queue.
	uint32				mPendingNullVideoFrames;

	// PREVIEW
	bool				mbAudioFrozen;
	bool				mbAudioFrozenValid;
	bool				mbSyncToAudioEvenClock;
	long				lDropFrames;

	// DECOMPRESSION PREVIEW
	vdautoptr<IVDVideoDecompressor>	mpVideoDecompressor;
	bool				mbVideoDecompressorEnabled;
	bool				mbVideoDecompressorPending;
	bool				mbVideoDecompressorErrored;
	VDPixmapBuffer		mVideoDecompBuffer;

	// ERROR HANDLING
	MyError				mError;
	bool				mbError;
	volatile bool		*mpAbort;

	const char			*volatile mpCurrentAction;
	VDAtomicInt			mActivityCounter;
	VDAtomicInt			mRefreshFlag;

	VDRTProfileChannel	mProcessingProfileChannel;
	VDCallbackTimer		mFrameTimer;
};

#endif
