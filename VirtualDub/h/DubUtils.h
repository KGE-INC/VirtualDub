#ifndef f_DUBUTILS_H
#define f_DUBUTILS_H

#include <windows.h>
#include <vfw.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/fraction.h>
#include <vd2/system/thread.h>
#include <vd2/system/VDRingBuffer.h>

class IVDStreamSource;
class IVDVideoSource;
class FrameSubset;
class IDDrawSurface;
struct IDirectDrawClipper;

int VDGetSizeOfBitmapHeaderW32(const BITMAPINFOHEADER *pHdr);


///////////////////////////////////////////////////////////////////////////
//
//	VDFormatStruct
//
//	VDFormatStruct describes an extensible format structure, such as
//	BITMAPINFOHEADER or WAVEFORMATEX, without the pain-in-the-butt
//	casting normally associated with one.
//
///////////////////////////////////////////////////////////////////////////

template<class T>
class VDFormatStruct {
public:
	typedef size_t			size_type;
	typedef T				value_type;

	VDFormatStruct() : mpMemory(NULL), mSize(0) {}

	VDFormatStruct(const T *pStruct, size_t len) : mSize(len), mpMemory((T*)malloc(len)) {
		memcpy(mpMemory, pStruct, len);
	}

	VDFormatStruct(const VDFormatStruct<T>& src) : mSize(src.mSize), mpMemory((T*)malloc(src.mSize)) {
		memcpy(mpMemory, pStruct, len);
	}

	~VDFormatStruct() {
		free(mpMemory);
	}

	bool		empty() const		{ return !mpMemory; }
	size_type	size() const		{ return mSize; }

	T&	operator *() const	{ return *(T *)mpMemory; }
	T*	operator->() const	{ return (T *)mpMemory; }

	VDFormatStruct<T>& operator=(const VDFormatStruct<T>& src) {
		assign(src.mpMemory, src.mSize);
		return *this;
	}

	void assign(const T *pStruct, size_type len) {
		if (mSize < len) {
			free(mpMemory);
			mpMemory = NULL;
			mpMemory = (T *)malloc(len);
			mSize = len;
		}

		memcpy(mpMemory, pStruct, len);
	}

	void clear() {
		free(mpMemory);
		mpMemory = NULL;
		mSize = 0;
	}

	void resize(size_type len) {
		if (mSize < len) {
			mpMemory = (T *)realloc(mpMemory, len);
			mSize = len;
		}
	}

protected:
	size_type	mSize;
	T *mpMemory;
};



///////////////////////////////////////////////////////////////////////////
//
//	VDStreamInterleaver
//
//	VDStreamInterleaver tells the engine when and where to place blocks
//	from various output streams into the final output file.  It is
//	capable of handing any number of streams of both CBR and VBR
//	orientation, and it also handles file slicing with preemptive
//	ahead, i.e. it cuts before rather than after the limit.
//
///////////////////////////////////////////////////////////////////////////

class IVDStreamInterleaverCutEstimator {
public:
	virtual bool EstimateCutPoint(int stream, sint64 start, sint64 target, sint64& framesToNextPoint, sint64& bytesToNextPoint) = 0;
};

class VDStreamInterleaver {
public:
	enum Action {
		kActionWrite,
		kActionNextSegment,
		kActionFinish
	};

	void Init(int streams);
	void EnableInterleaving(bool bEnableInterleave) { mbInterleavingEnabled = bEnableInterleave; }
	void SetSegmentFrameLimit(sint64 frames);
	void SetSegmentByteLimit(sint64 bytes, sint32 nPerFrameOverhead);
	void SetCutEstimator(int stream, IVDStreamInterleaverCutEstimator *pEstimator);
	void InitStream(int stream, uint32 nSampleSize, sint32 nPreload, double nSamplesPerFrame, double nInterval, sint32 nMaxPush);
	void EndStream(int stream);
	void AddVBRCorrection(int stream, sint32 actual);
	void AddCBRCorrection(int stream, sint32 actual);

	Action GetNextAction(int& streamID, sint32& samples);

	bool HasSegmentOverflowed() const { return mbSegmentOverflowed; }

protected:
	struct Stream {
		sint64		mSamplesWrittenToSegment;
		sint32		mSamplesToWrite;
		sint32		mMaxSampleSize;
		double		mSamplesPerFrame;
		sint64		mBytesPerFrame;
		sint32		mIntervalMicroFrames;
		sint32		mPreloadMicroFrames;
		sint64		mEstimatedSamples;
		sint32		mMaxPush;
		sint32		mLastSampleWrite;
		bool		mbActive;
	};


	Action PushStreams();
	void PushLimitFrontier(sint64 targetFrame);


	std::vector<Stream>	mStreams;

	sint64	mCurrentSize;
	sint64	mSegmentCutFrame;		// Frame at which we have begun cutting segment.  Once this is set we cannot push
									// the limit forward even if there is space, as some streams have already been cut.
	sint64	mFramesPerSegment;		// Maximum number of frames per segment.
	sint64	mBytesPerSegment;		// Maximum number of bytes per segment.
	sint64	mBytesPerFrame;			// Current estimate of # of bytes per frame.
	sint64	mSegmentStartFrame;
	sint64	mSegmentOkFrame;		// # of frames that we know we can write, based on frame and size limits
	sint32	mPerFrameOverhead;

	int		mNonIntStream;			// Current stream being worked on in a non-interleaved scenario
	int		mNextStream;
	int		mActiveStreams;
	sint32	mFrames;

	IVDStreamInterleaverCutEstimator	*mpCutEstimator;
	int		mCutStream;

	bool	mbSegmentOverflowed;
	bool	mbInterleavingEnabled;
};

///////////////////////////////////////////////////////////////////////////
//
//	VDRenderFrameMap
//
//	A render frame map holds a mapping from frames to render (timeline)
//	to display frames in the source.  The main benefit to the render frame
//	map is that it automatically handles frame reordering that must occur
//	due to direct stream copy dependencies.
//
///////////////////////////////////////////////////////////////////////////

class VDRenderFrameMap {
public:
	void		Init(IVDVideoSource *pVS, VDPosition nSrcStart, VDFraction srcStep, FrameSubset *pSubset, VDPosition nFrameCount, bool bDirect);

	VDPosition	size() const { return mFrameMap.size(); }

	VDPosition	TimelineFrame(VDPosition outFrame) const {
		return outFrame>=0 && outFrame<mMaxFrame ? mFrameMap[outFrame].first : -1;
	}

	VDPosition	DisplayFrame(VDPosition outFrame) const {
		return outFrame>=0 && outFrame<mMaxFrame ? mFrameMap[outFrame].second : -1;
	}

protected:
	std::vector<std::pair<VDPosition, VDPosition> > mFrameMap;
	VDPosition mMaxFrame;
};

///////////////////////////////////////////////////////////////////////////
//
//	VDRenderFrameIterator
//
//	Render frame iterators sequentially walk down a frame render map,
//	returning a list of display frames to be processed, and the stream
//	frames that must be fetched to decode them.
//
///////////////////////////////////////////////////////////////////////////

class VDRenderFrameIterator : public IVDStreamInterleaverCutEstimator {
public:
	VDRenderFrameIterator(const VDRenderFrameMap& frameMap) : mFrameMap(frameMap) {}

	void		Init(IVDVideoSource *pVS, bool bDirect);
	void		Next(VDPosition& srcFrame, VDPosition& displayFrame, VDPosition& timelineFrame, bool& bIsPreroll);

	bool		EstimateCutPoint(int stream, sint64 start, sint64 target, sint64& framesToNextPoint, sint64& bytesToNextPoint);

protected:
	bool		Reload();
	void		ReloadQueue(sint32 nCount);
	long		ConvertToIdealRawFrame(sint64 frame);

	const VDRenderFrameMap&	mFrameMap;

	VDPosition	mSrcTimelineFrame;
	VDPosition	mSrcDisplayFrame;
	VDPosition	mLastSrcDisplayFrame;
	VDPosition	mDstFrame;
	VDPosition	mDstFrameQueueNext;

	IVDVideoSource	*mpVideoSource;
	IVDStreamSource	*mpVideoStream;

	bool		mbDirect;
	bool		mbFirstSourceFrame;
	bool		mbFinished;
};

///////////////////////////////////////////////////////////////////////////
//
//	VDAudioPipeline
//
///////////////////////////////////////////////////////////////////////////

class VDAudioPipeline {
public:
	VDAudioPipeline();
	~VDAudioPipeline();

	void Init(uint32 bytes);
	void Shutdown();

	void Abort() {
		msigRead.signal();
		msigWrite.signal();
	}

	void CloseInput() {
		mbInputClosed = true;
		msigWrite.signal();
	}

	void CloseOutput() {
		mbOutputClosed = true;
		msigRead.signal();
	}

	void WaitUntilOutputClosed() {
		while(!mbOutputClosed)
			msigRead.wait();
	}

	bool full() const {
		return mBuffer.full();
	}

	int getLevel() const {
		return mBuffer.getLevel();
	}

	int getSpace() const {
		return mBuffer.getSpace();
	}

	bool isInputClosed() const {
		return mbInputClosed;
	}

	bool isOutputClosed() const {
		return mbOutputClosed;
	}

	VDSignal& getReadSignal() { return msigRead; }
	VDSignal& getWriteSignal() { return msigWrite; }

	int Read(void *pBuffer, int bytes);
	void ReadWait() {
		msigWrite.wait();
	}

	void *BeginWrite(int requested, int& actual);
	void EndWrite(int actual);

protected:
	VDRingBuffer<char>	mBuffer;
	VDSignal			msigRead;
	VDSignal			msigWrite;
	volatile bool		mbInputClosed;
	volatile bool		mbOutputClosed;
};

#endif
