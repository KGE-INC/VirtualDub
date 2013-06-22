//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2001 Avery Lee
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

#define f_DUB_CPP


#include <process.h>
#include <time.h>
#include <vector>
#include <deque>
#include <utility>

#include <windows.h>
#include <vfw.h>
#include <ddraw.h>

#include "resource.h"

#include "crash.h"
#include <vd2/system/thread.h>
#include <vd2/system/tls.h>
#include <vd2/system/time.h>
#include <vd2/system/atomic.h>
#include <vd2/system/fraction.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/VDRingBuffer.h>
#include <vd2/system/profile.h>
#include <vd2/Dita/resources.h>
#include "AudioFilterSystem.h"
#include "convert.h"
#include "filters.h"
#include "gui.h"
#include "ddrawsup.h"
#include "prefs.h"
#include "command.h"
#include "misc.h"

//#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)

#include <vd2/system/error.h>
#include "VideoSequenceCompressor.h"
#include "AsyncBlitter.h"
#include "AVIOutputPreview.h"
#include "AVIOutput.h"
#include "AVIOutputWAV.h"
#include "AVIOutputImages.h"
#include "AVIOutputStriped.h"
#include "Histogram.h"
#include "AudioSource.h"
#include "VideoSource.h"
#include "AVIPipe.h"
#include "VBitmap.h"
#include "FrameSubset.h"
#include "InputFile.h"
#include "VideoTelecineRemover.h"

#include "Dub.h"
#include "DubOutput.h"
#include "DubStatus.h"

///////////////////////////////////////////////////////////////////////////

extern const char g_szError[];
extern bool g_syncroBlit, g_vertical;
extern HWND g_hWnd;
extern HINSTANCE g_hInst;
extern bool g_fWine;

///////////////////////////////////////////////////////////////////////////

namespace {
	enum { kVDST_Dub = 1 };

	enum {
		kVDM_SegmentOverflowOccurred,
		kVDM_BeginningNextSegment,
		kVDM_IOThreadLivelock,
		kVDM_ProcessingThreadLivelock,
	};
};

///////////////////////////////////////////////////////////////////////////

DubOptions g_dubOpts = {
	{
		0,				// no amp
		500,			// preload by 500ms
		1,				// every frame
		0,				// no new rate
		0,				// offset: 0ms
		false,			// period is in frames
		true,			// audio interleaving enabled
		true,			// yes, offset audio with video
		true,			// yes, clip audio to video length
		false,			// no integral change
		false,			// no high quality
		false,			// use fixed-function audio pipeline
		DubAudioOptions::P_NOCHANGE,		// no precision change
		DubAudioOptions::C_NOCHANGE,		// no channel change
		DubAudioOptions::M_NONE,
	},

	{
		DubVideoOptions::D_24BIT,	// input: 24bit
		DubVideoOptions::D_24BIT,	// output: 24bit
		DubVideoOptions::M_FULL,	// mode: full
		TRUE,						// show input video
		TRUE,						// show output video
		FALSE,						// decompress output video before display
		FALSE,						// histograms
		TRUE,						// sync to audio
		1,							// no frame rate decimation
		0,0,						// no target
		0,							// no change in frame rate
		0,							// start offset: 0ms
		0,							// end offset: 0ms
		false,						// No inverse telecine
		false,						// (IVTC mode)
		-1,							// (IVTC offset)
		false,						// (IVTC polarity)
		0,							// progressive preview
	},

	{
		2097152,				// 2Mb AVI output buffer
		65536,					// 64K WAV input buffer
		32,						// 32 pipe buffers
		true,					// dynamic enable
		false,
		false,					// directdraw,
		true,					// drop frames
	},

	true,			// show status
	false,			// move slider
};

static const int g_iPriorities[][2]={

	// I/O							processor
	{ THREAD_PRIORITY_IDLE,			THREAD_PRIORITY_IDLE,			},
	{ THREAD_PRIORITY_LOWEST,		THREAD_PRIORITY_LOWEST,			},
	{ THREAD_PRIORITY_BELOW_NORMAL,	THREAD_PRIORITY_LOWEST,			},
	{ THREAD_PRIORITY_NORMAL,		THREAD_PRIORITY_BELOW_NORMAL,	},
	{ THREAD_PRIORITY_NORMAL,		THREAD_PRIORITY_NORMAL,			},
	{ THREAD_PRIORITY_ABOVE_NORMAL,	THREAD_PRIORITY_NORMAL,			},
	{ THREAD_PRIORITY_HIGHEST,		THREAD_PRIORITY_ABOVE_NORMAL,	},
	{ THREAD_PRIORITY_HIGHEST,		THREAD_PRIORITY_HIGHEST,		}
};

/////////////////////////////////////////////////
void AVISTREAMINFOtoAVIStreamHeader(AVIStreamHeader_fixed *dest, AVISTREAMINFO *src) {
	dest->fccType			= src->fccType;
	dest->fccHandler		= src->fccHandler;
	dest->dwFlags			= src->dwFlags;
	dest->wPriority			= src->wPriority;
	dest->wLanguage			= src->wLanguage;
	dest->dwInitialFrames	= src->dwInitialFrames;
	dest->dwStart			= src->dwStart;
	dest->dwScale			= src->dwScale;
	dest->dwRate			= src->dwRate;
	dest->dwLength			= src->dwLength;
	dest->dwSuggestedBufferSize = src->dwSuggestedBufferSize;
	dest->dwQuality			= src->dwQuality;
	dest->dwSampleSize		= src->dwSampleSize;
	dest->rcFrame.left		= (short)src->rcFrame.left;
	dest->rcFrame.top		= (short)src->rcFrame.top;
	dest->rcFrame.right		= (short)src->rcFrame.right;
	dest->rcFrame.bottom	= (short)src->rcFrame.bottom;

	VDDEBUG("scale %ld, rate %ld, length %ld\n",src->dwScale,src->dwRate, src->dwLength);
}

///////////////////////////////////////////////////////////////////////////
//
//	VDMappedBitmap
//
///////////////////////////////////////////////////////////////////////////

class VDMappedBitmap {
public:
	VDMappedBitmap();
	~VDMappedBitmap();

	void InitDrawDib(HDC hdc, const BITMAPINFOHEADER *bih);
	void InitGDI(HDC hdc, const BITMAPINFOHEADER *bih, HANDLE hMapObj, LONG nMapOffset);
	void Shutdown();

	HDRAWDIB	GetHDD() const { return mhdd; }
	HDC			GetHDC() const { return mhdc; }

protected:
	HDC			mhdc;
	HBITMAP		mhbm;
	HGDIOBJ		mhbmOld;
	HDRAWDIB	mhdd;
	void		*mpvBits;
};

VDMappedBitmap::VDMappedBitmap()
	: mhdc(NULL)
	, mhbm(NULL)
	, mhdd(NULL)
{
}

VDMappedBitmap::~VDMappedBitmap() {
	Shutdown();
}

void VDMappedBitmap::InitDrawDib(HDC hdc, const BITMAPINFOHEADER *bih) {
	VDASSERT(!mhdd && !mhdc);

	mhdd = DrawDibOpen();
	if (!mhdd)
		throw MyError("Failed to create display.");

	if (!DrawDibBegin(mhdd, hdc, bih->biWidth, bih->biHeight, (BITMAPINFOHEADER *)bih, bih->biWidth, bih->biHeight, 0))
		throw MyError("Failed to initialize display.");
}

void VDMappedBitmap::InitGDI(HDC hdc, const BITMAPINFOHEADER *bih, HANDLE hMapObj, LONG nMapOffset) {
	VDASSERT(!mhdd && !mhdc);

	mhdc = CreateCompatibleDC(hdc);
	if (!mhdc)
		throw MyWin32Error("Failed to create display:\n%%s", GetLastError());

	mhbm = CreateDIBSection(mhdc, (const BITMAPINFO *)bih, DIB_RGB_COLORS, &mpvBits, hMapObj, nMapOffset);
	if (!mhbm)
		throw MyWin32Error("Failed to create display:\n%%s", GetLastError());

	mhbmOld = SelectObject(mhdc, mhbm);
}

void VDMappedBitmap::Shutdown() {
	if (mhdd) {
		DrawDibClose(mhdd);
		mhdd = NULL;
	}

	if (mhbm) {
		DeleteObject(SelectObject(mhdc, mhbmOld));

		// Explicitly unmap the view.  NT4's GDI leaks memory if you create
		// a DIBSection with a mapping offset >64K and don't explicitly
		// unmap it.
		UnmapViewOfFile(mpvBits);

		mhbm = NULL;
	}

	if (mhdc) {
		DeleteDC(mhdc);
		mhdc = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	VDFormatStruct
//
///////////////////////////////////////////////////////////////////////////

template<class T>
class VDFormatStruct {
public:
	typedef size_t			size_type;
	typedef T				value_type;

	VDFormatStruct() : mpMemory(NULL), mSize(0) {}

	VDFormatStruct(const T *pStruct, size_t len) : mSize(len), mpMemory(new char[len])) {
		memcpy(mpMemory, pStruct, len);
	}

	VDFormatStruct(const VDFormatStruct<T>& src) : mSize(src.mSize), mpMemory(new char[src.mSize]) {
		memcpy(mpMemory, pStruct, len);
	}

	~VDFormatStruct() {
		delete[] mpMemory;
	}

	bool		empty() const		{ return !mpMemory; }
	size_type	size() const		{ return mSize; }

	T&	operator *() const	{ return *(T *)mpMemory; }
	T*	operator->() const	{ return (T *)mpMemory; }

	VDFormatStruct<T>& operator=(const VDFormatStruct<T>& src) {
		assign(pStruct, len);
	}

	void assign(const T *pStruct, size_type len) {
		if (mSize < len) {
			delete[] mpMemory;
			mpMemory = NULL;
			mpMemory = (T *)new char[len];
			mSize = len;
		}

		memcpy(mpMemory, pStruct, len);
	}

	void clear() {
		delete[] mpMemory;
		mpMemory = NULL;
		mSize = 0;
	}

	void resize(size_type len) {
		if (mSize < len) {
			delete[] mpMemory;
			mpMemory = NULL;
			mpMemory = (T *)new char[len];
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
	void SetSegmentFrameLimit(sint64 frames);
	void SetSegmentByteLimit(sint64 bytes, sint32 nPerFrameOverhead);
	void SetCutEstimator(int stream, IVDStreamInterleaverCutEstimator *pEstimator);
	void InitStream(int stream, uint32 nSampleSize, sint32 nPreload, double nSamplesPerFrame, double nInterval);
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
		bool		mbActive;
	};

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

	int	mNextStream;
	sint32	mFrames;

	IVDStreamInterleaverCutEstimator	*mpCutEstimator;
	int		mCutStream;

	bool	mbSegmentOverflowed;
	bool	mbForceBreakNext;
};

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
	mbForceBreakNext	= false;
}

void VDStreamInterleaver::SetSegmentFrameLimit(sint64 frames) {
	mFramesPerSegment	= frames;
	mSegmentCutFrame	= frames;
	mSegmentOkFrame		= 0;
}

void VDStreamInterleaver::SetSegmentByteLimit(sint64 bytes, sint32 nOverheadPerFrame) {
	mBytesPerSegment	= bytes;
	mBytesPerFrame		+= nOverheadPerFrame;
	mSegmentOkFrame		= 0;
	mPerFrameOverhead	= nOverheadPerFrame;
}

void VDStreamInterleaver::SetCutEstimator(int stream, IVDStreamInterleaverCutEstimator *pEstimator) {
	mpCutEstimator = pEstimator;
	mCutStream	= stream;
}

void VDStreamInterleaver::InitStream(int stream, uint32 nSampleSize, sint32 nPreload, double nSamplesPerFrame, double nInterval) {
	VDASSERT(stream>=0 && stream<mStreams.size());

	Stream& streaminfo = mStreams[stream];

	streaminfo.mSamplesWrittenToSegment = 0;
	streaminfo.mMaxSampleSize		= nSampleSize;
	streaminfo.mPreloadMicroFrames	= (sint64)((double)nPreload / nSamplesPerFrame * 65536);
	streaminfo.mSamplesPerFrame		= nSamplesPerFrame;
	streaminfo.mBytesPerFrame		= (sint64)ceil(nSampleSize * nSamplesPerFrame);
	streaminfo.mIntervalMicroFrames	= (sint64)(65536.0 / nInterval);
	streaminfo.mbActive				= true;

	mBytesPerFrame += streaminfo.mBytesPerFrame;
}

void VDStreamInterleaver::EndStream(int stream) {
	Stream& streaminfo = mStreams[stream];

	if (streaminfo.mbActive) {
		streaminfo.mbActive		= false;
		streaminfo.mSamplesToWrite	= 0;
		mBytesPerFrame -= streaminfo.mBytesPerFrame;
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
	VDASSERT(actual >= 0 && actual <= mStreams[stream].mSamplesToWrite);
	mCurrentSize += mStreams[stream].mMaxSampleSize * (actual - mStreams[stream].mSamplesToWrite);
//	VDDEBUG("Dub/Interleaver: CurrentSize = %lu\n", (unsigned long)mCurrentSize);
	VDASSERT(mCurrentSize >= 0);
}

VDStreamInterleaver::Action VDStreamInterleaver::GetNextAction(int& streamID, sint32& samples) {
	const int nStreams = mStreams.size();

	for(;;) {
		if (!mNextStream) {
			if (mbForceBreakNext) {
				mCurrentSize = 0;
				mSegmentStartFrame += mFrames;
				mFrames = 0;
				if (mFramesPerSegment)
					mSegmentCutFrame = mFramesPerSegment;
				else
					mSegmentCutFrame = 0;
				mSegmentOkFrame = 0;
				mbSegmentOverflowed = false;
				mbForceBreakNext = false;
				return kActionNextSegment;
			}

			int nActive = 0;
			sint64 minframe = 0x7fffffffffffffff;
			sint64 microFrames = (sint64)mFrames << 16;

//			mBytesPerSegment ? mFrames + (mBytesPerSegment - mCurrentSize) / mBytesPerFrame : 0x7fffffffffffffff;

			if (mSegmentCutFrame && mSegmentOkFrame > mSegmentCutFrame)
				mSegmentOkFrame = mSegmentCutFrame;

			for(int i=0; i<nStreams; ++i) {
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

						// If the cut stream (usually video) is still active, use its estimator
						// to place the cut.

						sint64 projectedSize = mCurrentSize;
						sint64 projectedTarget = mSegmentOkFrame;
						int j;

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

//						VDDEBUG("Dub/Interleaver: Projecting frame %ld -> %ld (size %ld + %ld < %ld)\n", (long)mFrames, (long)mSegmentOkFrame, (long)mCurrentSize, (long)projectedSize - (long)mCurrentSize, (long)mBytesPerSegment);

						if (mSegmentOkFrame <= 0) {
							mbSegmentOverflowed = true;
							mSegmentOkFrame = 1;
						}
					}

					// If still above limit, then we must force a segment break.
					if (iframe > mSegmentOkFrame) {
						mSegmentCutFrame = mSegmentOkFrame;
						frame = iframe = mSegmentCutFrame;
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

				++nActive;
			}

			if (!nActive)
				return kActionFinish;

			if (mSegmentCutFrame && minframe >= mSegmentCutFrame) {
				mbForceBreakNext = true;
			}
		}

		VDASSERT(!mBytesPerSegment || mCurrentSize <= mBytesPerSegment);

		for(; mNextStream<nStreams; ++mNextStream) {
			Stream& streaminfo = mStreams[mNextStream];

			if (streaminfo.mSamplesToWrite > 0) {
				samples = streaminfo.mSamplesToWrite;
				streamID = mNextStream++;
				VDASSERT(samples < 2147483647);
				mCurrentSize += samples * streaminfo.mMaxSampleSize;
//	VDDEBUG("Dub/Interleaver: stream %d, CurrentSize = %lu\n", (int)streamID, (unsigned long)mCurrentSize);
				streaminfo.mSamplesWrittenToSegment += samples;
				return kActionWrite;
			}
		}

		mCurrentSize += mPerFrameOverhead;

		VDASSERT(!mBytesPerSegment || mCurrentSize <= mBytesPerSegment);

		mNextStream = 0;
		if (!mbForceBreakNext)
			++mFrames;
	}
}

///////////////////////////////////////////////////////////////////////////

class VDRenderFrameMap {
public:
	void		Init(VideoSource *pVS, VDPosition nSrcStart, VDFraction srcStep, FrameSubset *pSubset, VDPosition nFrameCount, bool bDirect);

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

void VDRenderFrameMap::Init(VideoSource *pVS, VDPosition nSrcStart, VDFraction srcStep, FrameSubset *pSubset, VDPosition nFrameCount, bool bDirect) {
	VDPosition directLast = -1;

	for(VDPosition frame = 0; frame < nFrameCount; ++frame) {
		VDPosition timelineFrame = nSrcStart + srcStep.scale64t(frame);
		VDPosition srcFrame = timelineFrame;

		if (pSubset) {
			srcFrame = pSubset->lookupFrame(srcFrame);
			if (srcFrame < 0)
				break;
		} else {
			if (srcFrame < 0 || srcFrame >= pVS->lSampleLast)
				break;
		}

		if (bDirect) {
			VDPosition key = pVS->nearestKey((LONG)srcFrame);

			if (directLast < key)
				directLast = key;
			else if (directLast > srcFrame)
				directLast = key;
			else if (directLast < srcFrame)
				++directLast;

			srcFrame = directLast;
		}

		mFrameMap.push_back(std::make_pair(timelineFrame, srcFrame));
	}

	mMaxFrame = mFrameMap.size();
};

class VDRenderFrameIterator : public IVDStreamInterleaverCutEstimator {
public:
	VDRenderFrameIterator(const VDRenderFrameMap& frameMap) : mFrameMap(frameMap) {}

	void		Init(VideoSource *pVS, bool bDirect);
	void		Next(VDPosition& srcFrame, VDPosition& displayFrame, VDPosition& timelineFrame, bool& bIsPreroll);

	bool		EstimateCutPoint(int stream, sint64 start, sint64 target, sint64& framesToNextPoint, sint64& bytesToNextPoint);

protected:
	bool		Reload();
	void		ReloadQueue(sint32 nCount);
	long		ConvertToIdealRawFrame(sint64 frame);

	const VDRenderFrameMap&	mFrameMap;

	VDPosition	mSrcTimelineFrame;
	VDPosition	mSrcDisplayFrame;
	VDPosition	mDstFrame;
	VDPosition	mDstFrameQueueNext;

	VideoSource	*mpVideoSource;

	bool		mbDirect;
	bool		mbFirstSourceFrame;
	bool		mbFinished;
};

void VDRenderFrameIterator::Init(VideoSource *pVS, bool bDirect) {
	mpVideoSource	= pVS;
	mbDirect		= bDirect;
	mDstFrame		= 0;
	mSrcDisplayFrame	= 0;
	mSrcTimelineFrame	= 0;
	mbFinished		= false;

	Reload();
}

void VDRenderFrameIterator::Next(VDPosition& srcFrame, VDPosition& displayFrame, VDPosition& timelineFrame, bool& bIsPreroll) {
	while(!mbFinished) {
		BOOL b;
		long f = mpVideoSource->streamGetNextRequiredFrame(&b);

		bIsPreroll = (b!=0) && !mbDirect;

		if (f!=-1 || mbFirstSourceFrame) {
			mbFirstSourceFrame = false;
			if (mbDirect)
				if (!Reload())
					break;

			srcFrame = f;
			displayFrame = mSrcDisplayFrame;
			timelineFrame = mSrcTimelineFrame;
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
	mSrcDisplayFrame	= mFrameMap.DisplayFrame(mDstFrame);
	++mDstFrame;

	mpVideoSource->streamSetDesiredFrame((LONG)mSrcDisplayFrame);
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
				rangeHi = mpVideoSource->lSampleLast;
		} else if (srcFrame < rangeLo || srcFrame >= rangeHi)
			break;

		LONG lSize;

		++framesToNextPoint;

		if (start > 0 && mFrameMap.DisplayFrame(start-1) == srcFrame) {
			lSize = 0;		// frame duplication -- special case
		} else {
			int hr = mpVideoSource->read((LONG)srcFrame, 1, NULL, 0x7FFFFFFF, &lSize, NULL);
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
//	VDDubIOThread
//
///////////////////////////////////////////////////////////////////////////

namespace {
	enum {
		kBufferFlagDelta		= 1,
		kBufferFlagPreload		= 2
	};
}


class VDDubIOThread : public VDThread {
public:
	VDDubIOThread(
		bool				bPhantom,
		bool				bRealTime,
		VideoSource			*pVideo,
		VDRenderFrameIterator& videoFrameIterator,
		AudioStream			*pAudio,
		AVIPipe				*const pVideoPipe,
		VDRingBuffer<char>	*const pAudioPipe,
		VDSignal&			sigAudioPipeRead,
		VDSignal&			sigAudioPipeWrite,
		AudioL3Corrector	*const pAudioStreamCorrector,
		volatile bool&		bAudioPipeFinalized,
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
	bool				mbRealTime;
	VideoSource			*const mpVideo;
	VDRenderFrameIterator& mVideoFrameIterator;
	AudioStream			*const mpAudio;
	AVIPipe				*const mpVideoPipe;
	VDRingBuffer<char>	*const mpAudioPipe;
	VDSignal&			msigAudioPipeRead;
	VDSignal&			msigAudioPipeWrite;
	AudioL3Corrector	*const mpAudioStreamCorrector;
	volatile bool&		mbAudioPipeFinalized;
	volatile bool&		mbAbort;
	DubAudioStreamInfo&	aInfo;
	DubVideoStreamInfo& vInfo;
	VDAtomicInt&		mThreadCounter;
};

VDDubIOThread::VDDubIOThread(
		bool				bPhantom,
		bool				bRealTime,
		VideoSource			*pVideo,
		VDRenderFrameIterator& videoFrameIterator,
		AudioStream			*pAudio,
		AVIPipe				*const pVideoPipe,
		VDRingBuffer<char>	*const pAudioPipe,
		VDSignal&			sigAudioPipeRead,
		VDSignal&			sigAudioPipeWrite,
		AudioL3Corrector	*const pAudioStreamCorrector,
		volatile bool&		bAudioPipeFinalized,
		volatile bool&		bAbort,
		DubAudioStreamInfo&	_aInfo,
		DubVideoStreamInfo& _vInfo,
		VDAtomicInt&		threadCounter
							 )
	: VDThread("Dub-I/O")
	, mbError(false)
	, mbPhantom(bPhantom)
	, mbRealTime(bRealTime)
	, mpVideo(pVideo)
	, mVideoFrameIterator(videoFrameIterator)
	, mpAudio(pAudio)
	, mpVideoPipe(pVideoPipe)
	, mpAudioPipe(pAudioPipe)
	, msigAudioPipeRead(sigAudioPipeRead)
	, msigAudioPipeWrite(sigAudioPipeWrite)
	, mpAudioStreamCorrector(pAudioStreamCorrector)
	, mbAudioPipeFinalized(bAudioPipeFinalized)
	, mbAbort(bAbort)
	, aInfo(_aInfo)
	, vInfo(_vInfo)
	, mThreadCounter(threadCounter)
{
}

VDDubIOThread::~VDDubIOThread() {
}

void VDDubIOThread::ThreadRun() {

	///////////

	VDDEBUG("Dub/Main: Start.\n");

	VDRTProfileChannel profchan("I/O");		// sensei?

	try {
		try {
			bool bAudioActive = (mpAudio != 0);
			bool bVideoActive = (mpVideo != 0);

			while(!mbAbort && (bAudioActive || bVideoActive)) { 
				bool bBlocked = true;

				++mThreadCounter;

				if (bVideoActive && !mpVideoPipe->full()) {
					bBlocked = false;

					profchan.Begin(0xffe0e0, "Video");

					if (!MainAddVideoFrame() && vInfo.cur_dst >= vInfo.end_dst) {
						bVideoActive = false;
						mpVideoPipe->finalize();
					}

					profchan.End();
				}

				if (bAudioActive && !mpAudioPipe->full()) {
					bBlocked = false;

					profchan.Begin(0xe0e0ff, "Audio");

					if (!MainAddAudioFrame() && mpAudio->isEnd()) {
						bAudioActive = false;
						mbAudioPipeFinalized = true;
						msigAudioPipeWrite.signal();
					}

					profchan.End();
				}

				if (bBlocked) {
					mpVideoPipe->getReadSignal().wait(&msigAudioPipeRead);
				}
			}
		} catch(MyError& e) {
			if (!mbError) {
				mError.TransferFrom(e);
				mbError = true;
			}
//			e.post(NULL, "Dub Error (will attempt to finalize)");
		}

		// wait for the pipelines to clear...

		if (!mbAbort) {
			mbAudioPipeFinalized = true;
			msigAudioPipeWrite.signal();
			mpVideoPipe->finalizeAndWait();
			while(!mpAudioPipe->empty())
				msigAudioPipeRead.wait();
		}

		// kill everyone else...

		mbAbort = true;

	} catch(MyError& e) {
//		e.post(NULL,"Dub Error");

		if (!mbError) {
			mError.TransferFrom(e);
			mbError = true;
		}
		mbAbort = true;
	}

	// All done, time to get the pooper-scooper and clean up...

	VDDEBUG("Dub/Main: End.\n");
}

void VDDubIOThread::ReadVideoFrame(VDPosition stream_frame, VDPosition display_frame, VDPosition timeline_frame, bool preload) {
	LONG lActualBytes;
	int hr;

	void *buffer;
	int handle;

	LONG lSize;

	if (mbPhantom) {
		buffer = mpVideoPipe->getWriteBuffer(0, &handle);
		if (!buffer) return;	// hmm, aborted...

		bool bIsKey = !!mpVideo->isKey(display_frame);

		mpVideoPipe->postBuffer(0, stream_frame, display_frame, timeline_frame,
			(bIsKey ? 0 : kBufferFlagDelta)
			+(preload ? kBufferFlagPreload : 0),
			0,
			handle);

		return;
	}

//	VDDEBUG("Reading frame %ld (%s)\n", lVStreamPos, preload ? "preload" : "process");

	hr = mpVideo->read(stream_frame, 1, NULL, 0x7FFFFFFF, &lSize, NULL);
	if (hr) {
		if (hr == AVIERR_FILEREAD)
			throw MyError("Video frame %d could not be read from the source. The file may be corrupt.", stream_frame);
		else
			throw MyAVIError("Dub/IO-Video", hr);
	}

	// Add 4 bytes -- otherwise, we can get crashes with uncompressed video because
	// the bitmap routines expect to be able to read 4 bytes out.

	buffer = mpVideoPipe->getWriteBuffer(lSize+4, &handle);
	if (!buffer) return;	// hmm, aborted...

	hr = mpVideo->read(stream_frame, 1, buffer, lSize,	&lActualBytes,NULL); 
	if (hr) {
		if (hr == AVIERR_FILEREAD)
			throw MyError("Video frame %d could not be read from the source. The file may be corrupt.", stream_frame);
		else
			throw MyAVIError("Dub/IO-Video", hr);
	}

	display_frame = mpVideo->streamToDisplayOrder(stream_frame);

	mpVideoPipe->postBuffer(lActualBytes, stream_frame, display_frame, timeline_frame,
		(mpVideo->isKey(display_frame) ? 0 : kBufferFlagDelta)
		+(preload ? kBufferFlagPreload : 0),
		mpVideo->getDropType(display_frame),
		handle);


}

void VDDubIOThread::ReadNullVideoFrame(VDPosition displayFrame, VDPosition timelineFrame) {
	void *buffer;
	int handle;

	buffer = mpVideoPipe->getWriteBuffer(1, &handle);
	if (!buffer) return;	// hmm, aborted...

	if (displayFrame >= 0) {
		mpVideoPipe->postBuffer(0, mpVideo->displayToStreamOrder((long)displayFrame), displayFrame, timelineFrame,
			(mpVideo->isKey((long)displayFrame) ? 0 : kBufferFlagDelta),
			mpVideo->getDropType((long)displayFrame),
			handle);
	} else {
		mpVideoPipe->postBuffer(0, displayFrame, displayFrame, timelineFrame,
			0,
			AVIPipe::kDroppable,
			handle);
	}
}

//////////////////////

bool VDDubIOThread::MainAddVideoFrame() {
	if (vInfo.cur_dst >= vInfo.end_dst)
		return false;

	VDPosition streamFrame, displayFrame, timelineFrame;
	bool bIsPreroll;
	
	mVideoFrameIterator.Next(streamFrame, displayFrame, timelineFrame, bIsPreroll);

	vInfo.cur_src = vInfo.start_src + timelineFrame;

	if (streamFrame<0)
		ReadNullVideoFrame(displayFrame, timelineFrame);
	else
		ReadVideoFrame(streamFrame, displayFrame, timelineFrame, bIsPreroll);

	if (!bIsPreroll)
		++vInfo.cur_dst;

	return true;
}

bool VDDubIOThread::MainAddAudioFrame() {
	LONG lActualBytes=0;
	LONG lActualSamples=0;

	const int blocksize = mpAudio->GetFormat()->nBlockAlign;
	int samples = mpAudioPipe->getSpace();

	while(samples > 0) {
		int len = samples * blocksize;

		int tc;
		void *dst;
		
		dst = mpAudioPipe->LockWrite(len, tc);

		if (!tc)
			break;

		if (mbAbort)
			break;

		LONG ltActualBytes, ltActualSamples;
		ltActualSamples = mpAudio->Read(dst, tc / blocksize, &ltActualBytes);

		if (ltActualSamples <= 0)
			break;

		int residue = ltActualBytes % blocksize;

		if (residue) {
			VDASSERT(false);	// This is bad -- it means the input file has partial samples.

			ltActualBytes += blocksize - residue;
		}

		if (mpAudioStreamCorrector)
			mpAudioStreamCorrector->Process(dst, ltActualBytes);

		mpAudioPipe->UnlockWrite(ltActualBytes);
		msigAudioPipeWrite.signal();

		aInfo.total_size += ltActualBytes;
		aInfo.cur_src += ltActualSamples;

		lActualBytes += ltActualBytes;
		lActualSamples += ltActualSamples;

		samples -= ltActualSamples;
	}

	return lActualSamples > 0;
}

///////////////////////////////////////////////////////////////////////////
//
//	Dubber
//
///////////////////////////////////////////////////////////////////////////

class Dubber : public IDubber, protected VDThread, protected IVDTimerCallback {
private:
	void TimerCallback();

	void NextSegment();

	enum VideoWriteResult {
		kVideoWriteOK,
		kVideoWriteDelayed,
		kVideoWriteBuffered,
		kVideoWriteDiscarded
	};

	VideoWriteResult WriteVideoFrame(void *buffer, int exdata, int droptype, LONG lastSize, VDPosition sampleFrame, VDPosition displayFrame, VDPosition timelineFrame);
	void WriteAudio(void *buffer, long lActualBytes, long lActualSamples);

	void ThreadRun();

	MyError				err;
	bool				fError;

	VDAtomicInt			mStopLock;

	DubOptions			*opt;
	AudioSource			*aSrc;
	VideoSource			*vSrc;
	InputFile			*pInput;
	AVIOutput			*AVIout;
	IVDDubberOutputSystem	*mpOutputSystem;
	COMPVARS			*compVars;
	HDC					hDCWindow;

	DubAudioStreamInfo	aInfo;
	DubVideoStreamInfo	vInfo;

	bool				fUseVideoCompression;
	bool				fPreview;
	volatile bool		fAbort;
	volatile bool		fUserAbort;
	bool				fADecompressionOk;
	bool				fVDecompressionOk;
	bool				fFiltersOk;
	bool				fNoProcessingPriority;

	VDCallbackTimer		mFrameTimer;
	long				mPulseClock;

	VDDubIOThread		*mpIOThread;
	VDAtomicInt			mIOThreadCounter;

	VideoSequenceCompressor	*pVideoPacker;

	AVIPipe *			mpVideoPipe;
	VDRingBuffer<char>	mAudioPipe;
	VDSignal			msigAudioPipeRead;
	VDSignal			msigAudioPipeWrite;

	volatile bool		mbAudioPipeFinalized;

	AsyncBlitter *		blitter;

	HIC					outputDecompressor;

	VDMappedBitmap		mInputDisplay, mOutputDisplay;

	int					x_client, y_client;
	RECT				rInputFrame, rOutputFrame, rInputHistogram, rOutputHistogram;
	bool				fShowDecompressedFrame;
	bool				fDisplay565;
	IDDrawSurface		*pdsInput;
	IDDrawSurface		*pdsOutput;

	int					iOutputDepth;
	BITMAPINFO			*compressorVideoFormat;
	BITMAPINFO			compressorVideoDIBFormat;
	BITMAPV4HEADER		bihDisplayFormat;

	std::vector<AudioStream *>	mAudioStreams;
	AudioStream			*audioStream, *audioTimingStream;
	AudioStream			*audioStatusStream;
	AudioL3Corrector	*audioCorrector;
	vdautoptr<VDAudioFilterGraph> mpAudioFilterGraph;

	FrameSubset				*inputSubsetActive;
	FrameSubset				*inputSubsetAlloc;
	VideoTelecineRemover	*pInvTelecine;
	int					nVideoLagPreload;

	VDFormatStruct<WAVEFORMATEX> mAudioCompressionFormat;

	Histogram			*inputHisto, *outputHisto;

	VDAtomicInt			mRefreshFlag;
	HWND				hwndStatus;

	bool				fSyncToAudioEvenClock;
	bool				mbAudioFrozen;
	bool				mbAudioFrozenValid;

	long				lDropFrames;

	FilterStateInfo		fsi;

	bool				fPhantom;

	IDubStatusHandler	*pStatusHandler;

	long				lVideoSizeEstimate;
	bool				fEnableSpill;
	long				lSegmentFrameLimit;
	sint64				mSegmentByteLimit;

	// interleaving
	VDStreamInterleaver		mInterleaver;
	VDRenderFrameMap		mVideoFrameMap;
	VDRenderFrameIterator	mVideoFrameIterator;

	VDAtomicInt			mProcessingThreadCounter;
	VDRTProfileChannel	mProcessingProfileChannel;

	///////

	int					mLastProcessingThreadCounter;
	int					mProcessingThreadFailCount;
	int					mLastIOThreadCounter;
	int					mIOThreadFailCount;

	///////

public:
	Dubber(DubOptions *);
	~Dubber();

	void SetAudioCompression(WAVEFORMATEX *wf, LONG cb);
	void SetPhantomVideoMode();
	void SetInputFile(InputFile *pInput);
	void SetFrameRectangles(RECT *prInput, RECT *prOutput);
	void SetClientRectOffset(int x, int y);
	void SetAudioFilterGraph(const VDAudioFilterGraph& graph);
	void EnableSpill(sint64 threshold, long framethreshold);

	static void Dubber::SetClientRectOffset2(void *pv);
	static void Dubber::SetFrameRectangles2(void *pv);


	void InitAudioConversionChain();
	void InitOutputFile();
	bool AttemptInputOverlay(BITMAPINFOHEADER *pbih);
	void AttemptInputOverlays();
	static void AttemptInputOverlays2(void *pThis);

	bool AttemptOutputOverlay();
	void InitDirectDraw();
	void InitDisplay();
	bool NegotiateFastFormat(BITMAPINFOHEADER *pbih);
	bool NegotiateFastFormat(int depth);
	void InitSelectInputFormat();
	void Init(VideoSource *video, AudioSource *audio, IVDDubberOutputSystem *outsys, HDC hDC, COMPVARS *videoCompVars);
	void Go(int iPriority = 0);
	void Stop();

	void RealizePalette();
	void Abort();
	void ForceAbort();
	bool isAbortedByUser();
	bool IsPreviewing();
	void Tag(int x, int y);

	void SetStatusHandler(IDubStatusHandler *pdsh);
	void SetPriority(int index);
	void UpdateFrames();
};


///////////////////////////////////////////////////////////////////////////

IDubber::~IDubber() {
}

IDubber *CreateDubber(DubOptions *xopt) {
	return new Dubber(xopt);
}

Dubber::Dubber(DubOptions *xopt)
	: VDThread("Processing")
	, mpIOThread(0)
	, mIOThreadCounter(0)
	, mpAudioFilterGraph(NULL)
	, mStopLock(0)
	, mpVideoPipe(NULL)
	, mbAudioPipeFinalized(false)
	, mSegmentByteLimit(0)
	, mVideoFrameIterator(mVideoFrameMap)
	, mProcessingThreadCounter(0)
	, mProcessingProfileChannel("Processor")
	, mLastProcessingThreadCounter(0)
	, mProcessingThreadFailCount(0)
	, mLastIOThreadCounter(0)
	, mIOThreadFailCount(0)
{
	opt				= xopt;
	aSrc			= NULL;
	vSrc			= NULL;
	pInput			= NULL;

	// clear the workin' variables...

	fError				= false;

	fAbort				= false;
	fUserAbort			= false;

	pVideoPacker		= NULL;
	pStatusHandler		= NULL;

	fADecompressionOk	= false;
	fVDecompressionOk	= false;
	fFiltersOk			= false;

	blitter				= NULL;
	outputDecompressor	= NULL;
	hwndStatus			= NULL;
	vInfo.total_size	= 0;
	aInfo.total_size	= 0;
	vInfo.fAudioOnly	= false;

	pdsInput			= NULL;
	pdsOutput			= NULL;

	audioStream			= NULL;
	audioStatusStream	= NULL;
	audioCorrector		= NULL;

	inputSubsetActive	= NULL;
	inputSubsetAlloc	= NULL;

	inputHisto			= NULL;
	outputHisto			= NULL;

	fPhantom = false;

	pInvTelecine		= NULL;

	AVIout				= NULL;
	fEnableSpill		= false;

	lSegmentFrameLimit	= 0;

	fSyncToAudioEvenClock = false;
	mbAudioFrozen = false;
	mbAudioFrozenValid = false;
}

Dubber::~Dubber() {
	VDDEBUG("Dubber: destructor called.\n");

	Stop();
}

/////////////////////////////////////////////////

void Dubber::SetAudioCompression(WAVEFORMATEX *wf, LONG cb) {
	mAudioCompressionFormat.assign(wf, cb);
}

void Dubber::SetPhantomVideoMode() {
	fPhantom = true;
	vInfo.fAudioOnly = true;
}

void Dubber::SetInputFile(InputFile *pInput) {
	this->pInput = pInput;
}

void Dubber::SetStatusHandler(IDubStatusHandler *pdsh) {
	pStatusHandler = pdsh;
}


/////////////

struct DubberSetFrameRectangles {
	IDDrawSurface *pDD;
	RECT *pr;

	DubberSetFrameRectangles(IDDrawSurface *_pDD, RECT *_pr) : pDD(_pDD), pr(_pr) {}
};

void Dubber::SetFrameRectangles2(void *pv) {
	DubberSetFrameRectangles *pData = (DubberSetFrameRectangles *)pv;

	pData->pDD->SetOverlayPos(pData->pr);
}

void Dubber::SetFrameRectangles(RECT *prInput, RECT *prOutput) {
	rInputFrame = *prInput;
	rOutputFrame = *prOutput;

	if (g_vertical) {
		rInputHistogram.left	= rInputFrame.right + 6;
		rInputHistogram.top		= rInputFrame.top;
		rOutputHistogram.left	= rOutputFrame.right + 6;
		rOutputHistogram.top	= rOutputFrame.top;
	} else {
		rInputHistogram.left	= rInputFrame.left;
		rInputHistogram.top		= rInputFrame.bottom + 6;
		rOutputHistogram.left	= rOutputFrame.left;
		rOutputHistogram.top	= rOutputFrame.bottom + 6;
	}

	rInputHistogram.right	= rInputHistogram.left + 256;
	rInputHistogram.bottom	= rInputHistogram.top  + 128;

	rOutputHistogram.right	= rOutputHistogram.left + 256;
	rOutputHistogram.bottom	= rOutputHistogram.top  + 128;

	if (pdsInput) {
		RECT r = rInputFrame;

		r.left += x_client;
		r.right += x_client;
		r.top += y_client;
		r.bottom += y_client;

		blitter->postAFC(0x80000000, SetFrameRectangles2, &DubberSetFrameRectangles(pdsInput, &r));
	}
}

/////////////

struct DubberSetClientRectOffset {
	IDDrawSurface *pDD;
	long x,y;

	DubberSetClientRectOffset(IDDrawSurface *_pDD, long _x, long _y) : pDD(_pDD), x(_x), y(_y) {}
};


void Dubber::SetClientRectOffset2(void *pv) {
	DubberSetClientRectOffset *pData = (DubberSetClientRectOffset *)pv;

	pData->pDD->MoveOverlay(pData->x, pData->y);
}

void Dubber::SetClientRectOffset(int x, int y) {
	x_client = x;
	y_client = y;

	if (pdsInput)
		blitter->postAFC(0x80000000, SetClientRectOffset2, &DubberSetClientRectOffset(pdsInput, x + rInputFrame.left, y + rInputFrame.top));
}

void Dubber::EnableSpill(sint64 segsize, long framecnt) {
	fEnableSpill = true;
	mSegmentByteLimit = segsize;
	lSegmentFrameLimit = framecnt;
}

void Dubber::SetAudioFilterGraph(const VDAudioFilterGraph& graph) {
	mpAudioFilterGraph = new VDAudioFilterGraph(graph);
}

void InitStreamValuesStatic(DubVideoStreamInfo& vInfo, DubAudioStreamInfo& aInfo, VideoSource *video, AudioSource *audio, DubOptions *opt, FrameSubset *pfs) {
	if (!pfs)
		pfs = inputSubset;

	if (video) {

		if (pfs) {
			vInfo.start_src		= 0;
			vInfo.end_src		= pfs->getTotalFrames();
		} else {
			vInfo.start_src		= video->lSampleFirst;
			vInfo.end_src		= video->lSampleLast;
		}
	} else {
		vInfo.start_src		= 0;
		vInfo.end_src		= 0;
	}
	vInfo.cur_dst		= 0;
	vInfo.end_dst		= 0;
	vInfo.cur_proc_dst	= 0;
	vInfo.end_proc_dst	= 0;

	if (audio) {
		aInfo.start_src		= audio->lSampleFirst;
		aInfo.end_src		= audio->lSampleLast;
	} else {
		aInfo.start_src		= 0;
		aInfo.end_src		= aInfo.end_dst		= 0;
	}

	vInfo.cur_src			= vInfo.start_src;
	aInfo.cur_src			= aInfo.start_src;

	if (video) {
		// compute new frame rate

		VDFraction framerate(video->streamInfo.dwRate, video->streamInfo.dwScale);

		if (opt->video.frameRateNewMicroSecs == DubVideoOptions::FR_SAMELENGTH) {
			if (audio && audio->streamInfo.dwLength) {
				framerate = VDFraction::reduce64(video->streamInfo.dwLength * (sint64)1000, audio->samplesToMs(audio->streamInfo.dwLength));
			}
		} else if (opt->video.frameRateNewMicroSecs)
			framerate = VDFraction(1000000, opt->video.frameRateNewMicroSecs);

		// are we supposed to offset the video?

		if (opt->video.lStartOffsetMS) {
			vInfo.start_src += video->msToSamples(opt->video.lStartOffsetMS); 
		}

		if (opt->video.lEndOffsetMS)
			vInfo.end_src -= video->msToSamples(opt->video.lEndOffsetMS);

		vInfo.frameRateIn	= framerate;

		if (opt->video.frameRateDecimation==1 && opt->video.frameRateTargetLo)
			vInfo.frameRate	= VDFraction(opt->video.frameRateTargetHi, opt->video.frameRateTargetLo);
		else
			vInfo.frameRate	= framerate / opt->video.frameRateDecimation;

		vInfo.usPerFrameIn	= (long)vInfo.frameRateIn.scale64ir(1000000);
		vInfo.usPerFrame	= (long)vInfo.frameRate.scale64ir(1000000);

		// make sure we start reading on a key frame

		if (opt->video.mode == DubVideoOptions::M_NONE)
			vInfo.start_src	= video->nearestKey(vInfo.start_src);

		vInfo.cur_src		= vInfo.start_src;
		vInfo.end_dst		= (long)(vInfo.frameRate / vInfo.frameRateIn).scale64t(vInfo.end_src - vInfo.start_src);
		vInfo.end_proc_dst	= vInfo.end_dst;
	}

	if (audio) {
		// offset the start of the audio appropriately...

		aInfo.start_us = -1000*opt->audio.offset;

		if (video) {
			const sint64 video_start	= vInfo.start_src - video->lSampleFirst;
			const sint64 video_length	= vInfo.end_src - vInfo.start_src;


			if (opt->audio.fStartAudio && video && opt->video.lStartOffsetMS) {
				if (!pfs) {
					aInfo.start_us += vInfo.frameRateIn.scale64ir(1000000 * video_start);
				}
			}
		}

		aInfo.start_src += audio->msToSamples((long)(aInfo.start_us / 1000));

		// clip the end of the audio if supposed to...

		if (video && opt->audio.fEndAudio) {
			const sint64 video_length	= vInfo.end_src - vInfo.start_src;
			long lMaxLength;

			const VDFraction audiorate(audio->streamInfo.dwRate, audio->streamInfo.dwScale);

			lMaxLength = (long)(audiorate / vInfo.frameRateIn).scale64r(video_length);

			if (aInfo.end_src - aInfo.start_src > lMaxLength)
				aInfo.end_src = aInfo.start_src + lMaxLength;
		}

		// resampling audio?

		aInfo.resampling = false;
		aInfo.converting = false;

		if (opt->audio.mode > DubAudioOptions::M_NONE) {
			if (opt->audio.new_rate) {
				aInfo.resampling = true;
			}

			if (opt->audio.newPrecision != DubAudioOptions::P_NOCHANGE || opt->audio.newChannels != DubAudioOptions::C_NOCHANGE) {
				aInfo.converting = true;

				aInfo.is_16bit = opt->audio.newPrecision==DubAudioOptions::P_16BIT
								|| (opt->audio.newPrecision==DubAudioOptions::P_NOCHANGE && audio->getWaveFormat()->wBitsPerSample>8);
				aInfo.is_stereo = opt->audio.newChannels==DubAudioOptions::C_STEREO
								|| (opt->audio.newChannels==DubAudioOptions::C_NOCHANGE && audio->getWaveFormat()->nChannels>1);
				aInfo.is_right = (opt->audio.newChannels==DubAudioOptions::C_MONORIGHT);
				aInfo.single_channel = (opt->audio.newChannels==DubAudioOptions::C_MONOLEFT || opt->audio.newChannels==DubAudioOptions::C_MONORIGHT);
				aInfo.bytesPerSample = (aInfo.is_16bit ? 2 : 1) * (aInfo.is_stereo ? 2 : 1);

			}
		}

		aInfo.cur_src		= audio->nearestKey(aInfo.start_src);
		aInfo.cur_dst		= 0;
	}

	vInfo.cur_proc_src = vInfo.cur_src;
	aInfo.cur_proc_src = aInfo.cur_src;

	VDDEBUG("Dub: Audio is from (%ld,%ld) starting at %ld\n", aInfo.start_src, aInfo.end_src, aInfo.cur_src);
	VDDEBUG("Dub: Video is from (%ld,%ld) starting at %ld\n", vInfo.start_src, vInfo.end_src, vInfo.cur_src);
}

//////////////////////////////////////////////////////////////////////////////

// may be called at any time in Init() after streams setup

void Dubber::InitAudioConversionChain() {

	// ready the audio stream for streaming operation

	aSrc->streamBegin(fPreview);
	fADecompressionOk = true;

	// Initialize audio conversion chain

	if (opt->audio.mode > DubAudioOptions::M_NONE && mpAudioFilterGraph) {
		audioStream = new_nothrow AudioFilterSystemStream(*mpAudioFilterGraph, aInfo.start_us);
		if (!audioStream)
			throw MyMemoryError();

		mAudioStreams.push_back(audioStream);
	} else {
		// First, create a source.

		if (!(audioStream = new_nothrow AudioStreamSource(aSrc, aInfo.start_src, aSrc->lSampleLast - aInfo.start_src, opt->audio.mode > DubAudioOptions::M_NONE)))
			throw MyMemoryError();

		mAudioStreams.push_back(audioStream);

		// Attach a converter if we need to...

		if (aInfo.converting) {
			if (aInfo.single_channel)
				audioStream = new_nothrow AudioStreamConverter(audioStream, aInfo.is_16bit, aInfo.is_right, true);
			else
				audioStream = new_nothrow AudioStreamConverter(audioStream, aInfo.is_16bit, aInfo.is_stereo, false);

			if (!audioStream)
				throw MyMemoryError();

			mAudioStreams.push_back(audioStream);
		}

		// Attach a converter if we need to...

		if (aInfo.resampling) {
			if (!(audioStream = new_nothrow AudioStreamResampler(audioStream, opt->audio.new_rate ? opt->audio.new_rate : aSrc->getWaveFormat()->nSamplesPerSec, opt->audio.integral_rate, opt->audio.fHighQuality)))
				throw MyMemoryError();

			mAudioStreams.push_back(audioStream);
		}

		// Attach an amplifier if needed...

		if (opt->audio.mode > DubAudioOptions::M_NONE && opt->audio.volume) {
			if (!(audioStream = new_nothrow AudioStreamAmplifier(audioStream, opt->audio.volume)))
				throw MyMemoryError();

			mAudioStreams.push_back(audioStream);
		}
	}

	// Tack on a subset filter as well...

	if (inputSubsetActive) {
		sint64 offset = 0;
		
		if (opt->audio.fStartAudio)
			offset = vInfo.frameRateIn.scale64ir((sint64)1000000 * vInfo.start_src);

		if (!(audioStream = new_nothrow AudioSubset(audioStream, inputSubsetActive, vInfo.frameRateIn, offset)))
			throw MyMemoryError();

		mAudioStreams.push_back(audioStream);
	}

	// Make sure we only get what we want...

	if (vSrc && opt->audio.fEndAudio) {
		const WAVEFORMATEX *pAudioFormat = audioStream->GetFormat();
		const sint64 nFrames = (sint64)(vInfo.end_src - vInfo.start_src);
		const VDFraction audioRate(pAudioFormat->nAvgBytesPerSec, pAudioFormat->nBlockAlign);
		const VDFraction audioPerVideo(audioRate / vInfo.frameRateIn);

		audioStream->SetLimit((long)audioPerVideo.scale64r(nFrames));
	}

	audioTimingStream = audioStream;
	audioStatusStream = audioStream;

	// Tack on a compressor if we want...

	AudioCompressor *pCompressor = NULL;

	if (opt->audio.mode > DubAudioOptions::M_NONE && !mAudioCompressionFormat.empty()) {
		if (!(pCompressor = new_nothrow AudioCompressor(audioStream, &*mAudioCompressionFormat, mAudioCompressionFormat.size())))
			throw MyMemoryError();

		audioStream = pCompressor;
		mAudioStreams.push_back(audioStream);
	}

	// Check the output format, and if we're compressing to
	// MPEG Layer III, compensate for the lag and create a bitrate corrector

	if (!fEnableSpill && !g_prefs.fNoCorrectLayer3 && pCompressor && pCompressor->GetFormat()->wFormatTag == WAVE_FORMAT_MPEGLAYER3) {
		pCompressor->CompensateForMP3();

		if (!(audioCorrector = new_nothrow AudioL3Corrector()))
			throw MyMemoryError();
	}

}

void Dubber::InitOutputFile() {

	// Do audio.

	if (aSrc && mpOutputSystem->AcceptsAudio()) {
		// initialize AVI parameters...

		AVIStreamHeader_fixed	hdr;

		AVISTREAMINFOtoAVIStreamHeader(&hdr, &aSrc->streamInfo);
		hdr.dwStart			= 0;
		hdr.dwInitialFrames	= opt->audio.preload ? 1 : 0;

		if (opt->audio.mode > DubAudioOptions::M_NONE) {
			const WAVEFORMATEX *outputAudioFormat = audioStream->GetFormat();
			hdr.dwSampleSize	= outputAudioFormat->nBlockAlign;
			hdr.dwRate			= outputAudioFormat->nAvgBytesPerSec;
			hdr.dwScale			= outputAudioFormat->nBlockAlign;
			hdr.dwLength		= MulDiv(hdr.dwLength, outputAudioFormat->nSamplesPerSec, aSrc->getWaveFormat()->nSamplesPerSec);
		}

		mpOutputSystem->SetAudio(hdr, audioStream->GetFormat(), audioStream->GetFormatLen(), opt->audio.enabled);
	}

	// Do video.

	if (vSrc && mpOutputSystem->AcceptsVideo()) {
		VBitmap *outputBitmap;
		
		if (opt->video.mode >= DubVideoOptions::M_FULL)
			outputBitmap = filters.OutputBitmap();
		else
			outputBitmap = filters.InputBitmap();

		AVIStreamHeader_fixed hdr;

		AVISTREAMINFOtoAVIStreamHeader(&hdr, &vSrc->streamInfo);

		hdr.dwSampleSize = 0;

		if (opt->video.mode > DubVideoOptions::M_NONE) {
			if (fUseVideoCompression) {
				hdr.fccHandler	= compVars->fccHandler;
				hdr.dwQuality	= compVars->lQ;
			} else {
				hdr.fccHandler	= mmioFOURCC('D','I','B',' ');
			}
		}

		hdr.dwRate			= vInfo.frameRate.getHi();
		hdr.dwScale			= vInfo.frameRate.getLo();
		hdr.dwLength		= vInfo.end_dst;

		hdr.rcFrame.left	= 0;
		hdr.rcFrame.top		= 0;
		hdr.rcFrame.right	= (short)outputBitmap->w;
		hdr.rcFrame.bottom	= (short)outputBitmap->h;

		// initialize compression

		if (opt->video.mode >= DubVideoOptions::M_FASTREPACK) {
			if (opt->video.mode <= DubVideoOptions::M_SLOWREPACK)
				compressorVideoFormat = (BITMAPINFO *)vSrc->getDecompressedFormat();
			else {
				memset(&compressorVideoDIBFormat, 0, sizeof compressorVideoDIBFormat);
				compressorVideoDIBFormat.bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
				compressorVideoDIBFormat.bmiHeader.biWidth			= outputBitmap->w;
				compressorVideoDIBFormat.bmiHeader.biHeight			= outputBitmap->h;
				compressorVideoDIBFormat.bmiHeader.biPlanes			= 1;
				compressorVideoDIBFormat.bmiHeader.biBitCount		= iOutputDepth;
				compressorVideoDIBFormat.bmiHeader.biCompression	= BI_RGB;
				compressorVideoDIBFormat.bmiHeader.biSizeImage		= outputBitmap->pitch * outputBitmap->h;

				compressorVideoFormat = &compressorVideoDIBFormat;
			}
		} else {
			compressorVideoFormat = (BITMAPINFO *)vSrc->getImageFormat();
		}

		// Initialize output compressor.

		VDDEBUG("Dub: Initializing output compressor.\n");

		VDFormatStruct<BITMAPINFOHEADER>	outputFormat;

		if (fUseVideoCompression) {
			LONG formatSize;
			DWORD icErr;

			formatSize = ICCompressGetFormatSize(compVars->hic, compressorVideoFormat);
			if (formatSize < ICERR_OK)
				throw "Error getting compressor output format size.";

			VDDEBUG("Video compression format size: %ld\n",formatSize);

			outputFormat.resize(formatSize);

			// Huffyuv doesn't initialize a few padding bytes at the end of its format
			// struct, so we clear them here.
			memset(&*outputFormat, 0, outputFormat.size());

			if (ICERR_OK != (icErr = ICCompressGetFormat(compVars->hic,
								compressorVideoFormat,
								&*outputFormat)))
				throw MyICError("Output compressor", icErr);

			if (!(pVideoPacker = new VideoSequenceCompressor()))
				throw MyMemoryError();

			pVideoPacker->init(compVars->hic, compressorVideoFormat, (BITMAPINFO *)&*outputFormat, compVars->lQ, compVars->lKey);
			pVideoPacker->setDataRate(compVars->lDataRate*1024, vInfo.usPerFrame, vInfo.end_src - vInfo.start_src);
			pVideoPacker->start();

			lVideoSizeEstimate = pVideoPacker->getMaxSize();

			// attempt to open output decompressor

			if (opt->video.mode <= DubVideoOptions::M_FASTREPACK)
				fShowDecompressedFrame = false;
			else if (fShowDecompressedFrame = !!opt->video.fShowDecompressedFrame) {
				DWORD err;

				if (!(outputDecompressor = ICLocate(
							'CDIV',
							hdr.fccHandler,
							&*outputFormat,
							&compressorVideoFormat->bmiHeader, ICMODE_DECOMPRESS))) {

					MyError("Output video warning: Could not locate output decompressor.").post(NULL,g_szError);

				} else if (ICERR_OK != (err = ICDecompressBegin(
						outputDecompressor,
						&*outputFormat,
						&compressorVideoFormat->bmiHeader))) {

					MyICError("Output video warning", err).post(NULL,g_szError);

					ICClose(outputDecompressor);
					outputDecompressor = NULL;

					fShowDecompressedFrame = false;
				}
			}

		} else {
			if (opt->video.mode < DubVideoOptions::M_FASTREPACK) {

				if (vSrc->getImageFormat()->biCompression == 0xFFFFFFFF)
					throw MyError("The source video stream uses a compression algorithm which is not compatible with AVI files. "
								"Direct stream copy cannot be used with this video stream.");

				outputFormat.assign(vSrc->getImageFormat(), vSrc->getFormatLen());

				// cheese
				lVideoSizeEstimate = 0;
				for(long frame = vSrc->lSampleFirst; frame < vSrc->lSampleLast; ++frame) {
					long bytes = 0;

					if (!vSrc->read(frame, 1, 0, 0, &bytes, 0))
						if (lVideoSizeEstimate < bytes)
							lVideoSizeEstimate = bytes;
				}
			} else {
				outputFormat.assign(vSrc->getDecompressedFormat(), sizeof(BITMAPINFOHEADER));

				if (opt->video.mode == DubVideoOptions::M_FULL) {
					outputFormat->biCompression= BI_RGB;
					outputFormat->biWidth		= outputBitmap->w;
					outputFormat->biHeight		= outputBitmap->h;
					outputFormat->biBitCount	= iOutputDepth;
					outputFormat->biSizeImage	= outputBitmap->pitch * outputBitmap->h;
				}

				lVideoSizeEstimate = outputFormat->biSizeImage;
				lVideoSizeEstimate = (lVideoSizeEstimate+1) & -2;
			}
		}

		mpOutputSystem->SetVideo(hdr, &*outputFormat, outputFormat.size());
	}

	VDDEBUG("Dub: Creating output file.\n");

	AVIout = mpOutputSystem->CreateSegment();
}

bool Dubber::AttemptInputOverlay(BITMAPINFOHEADER *pbih) {
	if (vSrc->setDecompressedFormat(pbih)) {
		DDSURFACEDESC ddsdOverlay;
		DDPIXELFORMAT ddpf;
		IDirectDrawSurface *lpddsOverlay;
		HRESULT res;

		memset(&ddpf, 0, sizeof ddpf);
		ddpf.dwSize			= sizeof ddpf;
		ddpf.dwFlags		= DDPF_FOURCC;
		ddpf.dwFourCC		= pbih->biCompression;
		ddpf.dwYUVBitCount	= pbih->biBitCount;

		memset(&ddsdOverlay, 0, sizeof ddsdOverlay);
		ddsdOverlay.dwSize = sizeof(ddsdOverlay);
		ddsdOverlay.dwFlags= DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
		ddsdOverlay.ddsCaps.dwCaps = DDSCAPS_OVERLAY | DDSCAPS_VIDEOMEMORY;
		ddsdOverlay.dwWidth  = vSrc->getImageFormat()->biWidth;
		ddsdOverlay.dwHeight = vSrc->getImageFormat()->biHeight;
		ddsdOverlay.ddpfPixelFormat = ddpf;
		
		res = DDrawObtainInterface()->CreateSurface(&ddsdOverlay, &lpddsOverlay, NULL);

		if (DD_OK == res) {
			if (!(pdsInput = CreateDDrawSurface(lpddsOverlay))) {
				lpddsOverlay->Release();
				throw MyMemoryError();
			}

			RECT r = rInputFrame;

			pdsInput->SetOverlayPos(&r);

			return true;
		}
	}

	return false;
}

void Dubber::AttemptInputOverlays() {
	if (DDrawInitialize(g_hWnd)) {
		BITMAPINFOHEADER bih;

		memcpy(&bih, vSrc->getImageFormat(), sizeof(BITMAPINFOHEADER));

		bih.biSize			= sizeof(BITMAPINFOHEADER);
		bih.biPlanes		= 1;
		bih.biBitCount		= 16;
		bih.biSizeImage		= (bih.biWidth+(bih.biWidth&1))*2*bih.biHeight;
		bih.biXPelsPerMeter	= 0;
		bih.biYPelsPerMeter	= 0;
		bih.biClrUsed		= 0;
		bih.biClrImportant	= 0;

		do {
			//---- begin 16-bit YUV negotiation ----

			// Attempt CYUV (YUV 4:2:2, Y?Y? ordering)

			bih.biCompression = 'VUYC';

			if (AttemptInputOverlay(&bih))
				break;

			// Attempt UYVY (YUV 4:2:2)

			bih.biCompression = 'YVYU';

			if (AttemptInputOverlay(&bih))
				break;

			// Attempt YUYV (YUV 4:2:2)

			bih.biCompression = 'VYUY';

			if (AttemptInputOverlay(&bih))
				break;

			// Attempt YUY2 (YUV 4:2:2, YUYV ordering)

			bih.biCompression = '2YUY';

			if (AttemptInputOverlay(&bih))
				break;

			//---- begin 12-bit YUV negotiation ----
#if 0
			// Attempt YV12 (YUV 4:2:0)

			bih.biCompression = '21VY';
			bih.biSizeImage		= (bih.biWidth/2)*(bih.biHeight/2)*6;
			bih.biBitCount		= 12;

			if (AttemptInputOverlay(&bih))
				break;
#endif

			DDrawDeinitialize();
		} while(0);

	}
}

void Dubber::AttemptInputOverlays2(void *pThis) {
	((Dubber *)pThis)->AttemptInputOverlays();
}

bool Dubber::AttemptOutputOverlay() {

	if (!DDrawInitialize(g_hWnd))
		return false;

	// Try and get the pixel format for the primary surface.

	DDPIXELFORMAT ddpf;

	memset(&ddpf, 0, sizeof ddpf);
	ddpf.dwSize		= sizeof ddpf;

	if (DD_OK != DDrawObtainPrimary()->GetPixelFormat(&ddpf))
		return false;

	// Check output pixel format; we can support:
	//
	//	15-bit RGB	00007c00	000003e0	0000001f
	//	16-bit RGB	0000f800	000007e0	0000001f
	//	24-bit RGB	00ff0000	0000ff00	000000ff
	//	32-bit RGB	00ff0000	0000ff00	000000ff

	if (!(ddpf.dwFlags & DDPF_RGB))
		return false;

	const VBitmap *outputBitmap = filters.OutputBitmap();

	memset(&compressorVideoDIBFormat, 0, sizeof compressorVideoDIBFormat);
	bihDisplayFormat.bV4Size			= sizeof(BITMAPINFOHEADER);
	bihDisplayFormat.bV4Width			= outputBitmap->w;
	bihDisplayFormat.bV4Height			= outputBitmap->h;
	bihDisplayFormat.bV4Planes			= 1;
	bihDisplayFormat.bV4BitCount		= (WORD)ddpf.dwRGBBitCount;
	bihDisplayFormat.bV4V4Compression	= BI_RGB;
	bihDisplayFormat.bV4SizeImage		= outputBitmap->pitch * outputBitmap->h;

	switch(ddpf.dwRGBBitCount) {
	case 16:
		if (ddpf.dwRBitMask == 0xf800 && ddpf.dwGBitMask == 0x07e0 && ddpf.dwBBitMask == 0x001f) {
			bihDisplayFormat.bV4Size			= sizeof(BITMAPV4HEADER);
			bihDisplayFormat.bV4V4Compression	= BI_BITFIELDS;
			bihDisplayFormat.bV4RedMask			= 0xf800;
			bihDisplayFormat.bV4GreenMask		= 0x07e0;
			bihDisplayFormat.bV4BlueMask		= 0x001f;
			bihDisplayFormat.bV4AlphaMask		= 0x0000;
			bihDisplayFormat.bV4CSType			= 0;
			fDisplay565 = true;
		} else if (ddpf.dwRBitMask != 0x7c00 || ddpf.dwGBitMask != 0x03e0 || ddpf.dwBBitMask != 0x001f)
			return false;

		break;

	case 24:
	case 32:
		if (ddpf.dwRBitMask != 0x00FF0000) return false;
		if (ddpf.dwGBitMask != 0x0000FF00) return false;
		if (ddpf.dwBBitMask != 0x000000FF) return false;
		break;
	}

	// Create off-screen surface.

	DDSURFACEDESC ddsdOverlay;
	IDirectDrawSurface *lpddsOutput;

	memset(&ddsdOverlay, 0, sizeof ddsdOverlay);
	ddsdOverlay.dwSize			= sizeof(ddsdOverlay);
	ddsdOverlay.dwFlags			= DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
	ddsdOverlay.ddsCaps.dwCaps	= DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
	ddsdOverlay.dwWidth			= bihDisplayFormat.bV4Width;
	ddsdOverlay.dwHeight		= bihDisplayFormat.bV4Height;
	ddsdOverlay.ddpfPixelFormat = ddpf;

	if (DD_OK != DDrawObtainInterface()->CreateSurface(&ddsdOverlay, &lpddsOutput, NULL))
		return false;

	if (!(pdsOutput = CreateDDrawSurface(lpddsOutput))) {
		lpddsOutput->Release();
		throw MyMemoryError();
	}

	return true;
}

void Dubber::InitDirectDraw() {

	if (!opt->perf.useDirectDraw)
		return;

	// Should we try and establish a DirectDraw overlay?

	if (opt->video.mode == DubVideoOptions::M_SLOWREPACK) {
		blitter->postAFC(0x80000000, AttemptInputOverlays2, this);
	}

	// How about DirectShow output acceleration?

	if (opt->video.mode == DubVideoOptions::M_FULL)
		AttemptOutputOverlay();

	if (pdsInput || pdsOutput)
		SetClientRectOffset(x_client, y_client);
}

void Dubber::InitDisplay() {
	VDDEBUG("Dub: Initializing input window display.\n");


	// Check color depth of output device.  If it is 8-bit, we're
	// stuck with DrawDibDraw().  If it's at least 15 bits, then
	// we should create a DIBSection, select it into a memory
	// context, and BltBlt() to the screen instead.  It's about
	// 5-10% faster under Win95 and about 400x (!) faster under WINE.
	//
	// Okay, never mind... WINE still doesn't support DIBSection
	// windows. :(

	int bitsPerPel;

	bitsPerPel = GetDeviceCaps(hDCWindow, BITSPIXEL);

	if (!pdsInput && opt->video.mode > DubVideoOptions::M_FASTREPACK && !g_fWine) {
		if (bitsPerPel < 15 || vSrc->getDecompressedFormat()->biBitCount < 15
				|| !vSrc->getFrameBufferObject()) {

			mInputDisplay.InitDrawDib(hDCWindow, vSrc->getDecompressedFormat());
		} else {
			mInputDisplay.InitGDI(hDCWindow, vSrc->getDecompressedFormat(), vSrc->getFrameBufferObject(), vSrc->getFrameBufferOffset());
		}
	}

	VDDEBUG("Dub: Initializing output window display.\n");
	if (opt->video.mode == DubVideoOptions::M_FULL) {
		if (!pdsOutput && !g_fWine) {
			if (bitsPerPel < 15) {
				mOutputDisplay.InitDrawDib(hDCWindow, &compressorVideoFormat->bmiHeader);
			} else {
				// check to see if DC is 565 16-bit, the only mode that does not support a line
				// of grays... hmm... is 15 possible for bitsPerPel?

				COLORREF crTmp;

				fDisplay565 = false;

				if (bitsPerPel==15 || bitsPerPel==16) {
					crTmp = GetPixel(hDCWindow, 0,0);
					SetPixel(hDCWindow,0,0,RGB(0x80, 0x88, 0x80));

					if (GetPixel(hDCWindow,0,0) == RGB(0x80, 0x88, 0x80)) {
						fDisplay565 = true;

						VDDEBUG("Display is 5-6-5 16-bit\n");
					}
					SetPixel(hDCWindow, 0, 0, crTmp);
				}

				memcpy(&bihDisplayFormat, compressorVideoFormat, sizeof(BITMAPINFOHEADER));
				if (fDisplay565 && fPreview && bihDisplayFormat.bV4BitCount == 16) {
					bihDisplayFormat.bV4Size			= sizeof(BITMAPV4HEADER);
					bihDisplayFormat.bV4V4Compression	= BI_BITFIELDS;
					bihDisplayFormat.bV4RedMask			= 0xf800;
					bihDisplayFormat.bV4GreenMask		= 0x07e0;
					bihDisplayFormat.bV4BlueMask		= 0x001f;
					bihDisplayFormat.bV4AlphaMask		= 0x0000;
					bihDisplayFormat.bV4CSType			= 0;
				}

				HANDLE hMapObject;
				LONG lMapOffset;

				filters.getOutputMappingParams(hMapObject, lMapOffset);

				mOutputDisplay.InitGDI(hDCWindow, &compressorVideoFormat->bmiHeader, hMapObject, lMapOffset);
			}

		}

		if (opt->video.fHistogram) {
			inputHisto = new Histogram(hDCWindow, 128);
			outputHisto = new Histogram(hDCWindow, 128);
		}
	}

}

bool Dubber::NegotiateFastFormat(BITMAPINFOHEADER *pbih) {
	return vSrc->setDecompressedFormat(pbih) &&
			ICERR_OK == ICCompressQuery(compVars->hic, pbih, NULL);
}

bool Dubber::NegotiateFastFormat(int depth) {
	return vSrc->setDecompressedFormat(depth) &&
			ICERR_OK == ICCompressQuery(compVars->hic, vSrc->getDecompressedFormat(), NULL);
}

void Dubber::InitSelectInputFormat() {
	//	DIRECT:			Don't care.
	//	FASTREPACK:		Negotiate with output compressor.
	//	SLOWREPACK:		[Dub]		Use selected format.
	//					[Preview]	Negotiate with display driver.
	//	FULL:			Use selected format.

	if (opt->video.mode == DubVideoOptions::M_NONE)
		return;

	if (opt->video.mode == DubVideoOptions::M_FASTREPACK && fUseVideoCompression) {
		BITMAPINFOHEADER bih;

		// Begin decompressor to compressor format negotiation.
		//

		// Attempt UYVY.

		memcpy(&bih, vSrc->getImageFormat(), sizeof(BITMAPINFOHEADER));

		bih.biSize			= sizeof(BITMAPINFOHEADER);
		bih.biPlanes		= 1;
		bih.biBitCount		= 16;
		bih.biCompression	= 'YVYU';
		bih.biSizeImage		= (bih.biWidth+(bih.biWidth&1))*2*bih.biHeight;
		bih.biXPelsPerMeter	= 0;
		bih.biYPelsPerMeter	= 0;
		bih.biClrUsed		= 0;
		bih.biClrImportant	= 0;

		if (NegotiateFastFormat(&bih))
			return;

		// Attempt YUYV.

		bih.biCompression	= 'VYUY';

		if (NegotiateFastFormat(&bih))
			return;

		// Attempt YUY2.

		bih.biCompression	= '2YUY';

		if (NegotiateFastFormat(&bih))
			return;

		// Attempt RGB format negotiation.

		if (NegotiateFastFormat(16+8*opt->video.inputDepth))
			return;

		if (NegotiateFastFormat(24))
			return;
		if (NegotiateFastFormat(32))
			return;
		if (NegotiateFastFormat(16))
			return;
		if (NegotiateFastFormat(8))
			return;

		throw MyError("Video format negotiation failed: use slow-repack or full mode.");
	}

	// Negotiate RGB format.

	if (!vSrc->setDecompressedFormat(16+8*opt->video.inputDepth))
		if (!vSrc->setDecompressedFormat(32))
			if (!vSrc->setDecompressedFormat(24))
				if (!vSrc->setDecompressedFormat(16))
					if (!vSrc->setDecompressedFormat(8))
						throw MyError("The decompression codec cannot decompress to an RGB format. This is very unusual. Check that any \"Force YUY2\" options are not enabled in the codec's properties.");
}

void Dubber::Init(VideoSource *video, AudioSource *audio, IVDDubberOutputSystem *pOutputSystem, HDC hDC, COMPVARS *videoCompVars) {

	aSrc				= audio;
	vSrc				= video;
	mpOutputSystem		= pOutputSystem;

	fPreview			= mpOutputSystem->IsRealTime();

	compVars			= videoCompVars;
	hDCWindow			= hDC;
	fUseVideoCompression = !fPreview && opt->video.mode>DubVideoOptions::M_NONE && compVars && (compVars->dwFlags & ICMF_COMPVARS_VALID) && compVars->hic;
//	fUseVideoCompression = opt->video.mode>DubVideoOptions::M_NONE && compVars && (compVars->dwFlags & ICMF_COMPVARS_VALID) && compVars->hic;

	// check the mode; if we're using DirectStreamCopy mode, we'll need to
	// align the subset to keyframe boundaries!

	if (vSrc && inputSubset) {
		inputSubsetActive = inputSubset;

		if (opt->video.mode == DubVideoOptions::M_NONE) {
			FrameSubsetNode *pfsn;

			if (!(inputSubsetActive = inputSubsetAlloc = new FrameSubset()))
				throw MyMemoryError();

			pfsn = inputSubset->getFirstFrame();
			while(pfsn) {
				long end = pfsn->start + pfsn->len;
				long start = vSrc->nearestKey(pfsn->start + vSrc->lSampleFirst) - vSrc->lSampleFirst;

				VDDEBUG("   subset: %5d[%5d]-%-5d\n", pfsn->start, start, pfsn->start+pfsn->len-1);
				inputSubsetActive->addRange(pfsn->start, pfsn->len, pfsn->bMask);

				// Mask ranges never need to be extended backwards, because they don't hold any
				// data of their own.  If an include range needs to be extended backwards, though,
				// it may need to extend into a previous merge range.  To avoid this problem,
				// we do a delete of the range before adding the tail.

				if (!pfsn->bMask) {
					if (start < pfsn->start) {
						inputSubsetActive->deleteInputRange(start, pfsn->start-start);
						inputSubsetActive->addRangeMerge(start, pfsn->start-start, false);
					}
				}

				pfsn = inputSubset->getNextFrame(pfsn);
			}

#ifdef _DEBUG
			pfsn = inputSubsetActive->getFirstFrame();

			while(pfsn) {
				VDDEBUG("   padded subset: %8d-%-8d\n", pfsn->start, pfsn->start+pfsn->len-1);
				pfsn = inputSubsetActive->getNextFrame(pfsn);
			}
#endif
		}
	}

	// initialize stream values

	InitStreamValuesStatic(vInfo, aInfo, video, audio, opt, inputSubsetActive);

	vInfo.frameRateNoTelecine = vInfo.frameRate;
	vInfo.usPerFrameNoTelecine = vInfo.usPerFrame;
	if (opt->video.mode >= DubVideoOptions::M_FULL && opt->video.fInvTelecine) {
		vInfo.frameRate = vInfo.frameRate * VDFraction(4, 5);
		vInfo.usPerFrame = (long)vInfo.frameRate.scale64ir(1000000);

		vInfo.end_proc_dst	= (long)(vInfo.frameRate / vInfo.frameRateIn).scale64t(vInfo.end_src - vInfo.start_src);
		vInfo.end_dst += 4;
		vInfo.end_dst -= vInfo.end_dst % 5;
	}

	// initialize blitter

	VDDEBUG("Dub: Creating blitter.\n");

	if (g_syncroBlit || !fPreview)
		blitter = new AsyncBlitter();
	else
		blitter = new AsyncBlitter(8);

	if (!blitter) throw MyError("Couldn't create AsyncBlitter");

	blitter->pulse();

	// Select an appropriate input format.  This is really tricky...

	vInfo.fAudioOnly = true;
	if (vSrc && mpOutputSystem->AcceptsVideo()) {
		InitSelectInputFormat();
		vInfo.fAudioOnly = false;
	}

	iOutputDepth = 16+8*opt->video.outputDepth;

	// Initialize filter system.

	int nVideoLagOutput = 0;		// Frames that will be buffered in the output frame space (video filters)
	int nVideoLagTimeline = 0;		// Frames that will be buffered in the timeline frame space (IVTC)

	if (vSrc && opt->video.mode >= DubVideoOptions::M_FULL) {
		BITMAPINFOHEADER *bmih = vSrc->getDecompressedFormat();

		filters.initLinearChain(&g_listFA, (Pixel *)(bmih+1), bmih->biWidth, bmih->biHeight, 32 /*bmih->biBitCount*/, iOutputDepth);

		fsi.lMicrosecsPerFrame		= vInfo.usPerFrame;
		fsi.lMicrosecsPerSrcFrame	= vInfo.usPerFrameIn;
		fsi.lCurrentFrame			= 0;

		if (filters.ReadyFilters(&fsi))
			throw "Error readying filters.";

		fFiltersOk = true;

		nVideoLagTimeline = nVideoLagOutput = filters.getFrameLag();

		// Inverse telecine?

		if (opt->video.fInvTelecine) {
			if (!(pInvTelecine = CreateVideoTelecineRemover(filters.InputBitmap(), !opt->video.fIVTCMode, opt->video.nIVTCOffset, opt->video.fIVTCPolarity)))
				throw MyMemoryError();

			nVideoLagTimeline = 10 + ((nVideoLagOutput+3)&~3)*5;
		}
	}

	nVideoLagPreload = nVideoLagOutput;
	vInfo.cur_dst = -nVideoLagTimeline;

	// initialize directdraw display if in preview

	if (fPreview)
		InitDirectDraw();

	// initialize input decompressor

	if (vSrc && mpOutputSystem->AcceptsVideo()) {

		VDDEBUG("Dub: Initializing input decompressor.\n");

		vSrc->streamBegin(fPreview);
		fVDecompressionOk = true;

	}

	// Initialize audio.

	VDDEBUG("Dub: Initializing audio.\n");

	if (aSrc && mpOutputSystem->AcceptsAudio())
		InitAudioConversionChain();

	// initialize output parameters and output file

	InitOutputFile();

	// initialize interleaver

	bool bAudio = aSrc && mpOutputSystem->AcceptsAudio();

	mInterleaver.Init(bAudio ? 2 : 1);
	mInterleaver.InitStream(0, lVideoSizeEstimate, 0, 1, 1);

	if (bAudio) {
		Fraction audioBlocksPerVideoFrame;

		if (opt->audio.is_ms) {
			// blocks / frame = (ms / frame) / (ms / block)
			audioBlocksPerVideoFrame = Fraction(vInfo.usPerFrame, 1000) / Fraction(opt->audio.interval, 1);
		} else {
			audioBlocksPerVideoFrame = Fraction(1, opt->audio.interval);
		}

		// (bytes/sec) / (bytes/samples) = (samples/sec)
		// (samples/sec) / (frames/sec) = (samples/frame)
		// (samples/frame) / (blocks/frame) = (samples/block)

		const WAVEFORMATEX *pwfex = audioStream->GetFormat();
		Fraction samplesPerSec(pwfex->nAvgBytesPerSec, pwfex->nBlockAlign);
		sint32 preload = (sint32)(samplesPerSec * Fraction(opt->audio.preload, 1000)).roundup32ul();

		double samplesPerFrame = (double)samplesPerSec / (double)vInfo.frameRate;

		mInterleaver.InitStream(1, pwfex->nBlockAlign, preload, samplesPerFrame, (double)audioBlocksPerVideoFrame);
	}

	if (fEnableSpill) {
		if (lSegmentFrameLimit)
			mInterleaver.SetSegmentFrameLimit(lSegmentFrameLimit);

		if (mSegmentByteLimit)
			mInterleaver.SetSegmentByteLimit(mSegmentByteLimit, 64);
	}

	// initialize frame iterator

	if (vSrc && mpOutputSystem->AcceptsVideo()) {
		mVideoFrameMap.Init(vSrc, vInfo.start_src, vInfo.frameRateIn / vInfo.frameRateNoTelecine, inputSubsetActive, vInfo.end_dst, opt->video.mode == DubVideoOptions::M_NONE);
		mVideoFrameIterator.Init(vSrc, opt->video.mode == DubVideoOptions::M_NONE);

		if (opt->video.mode == DubVideoOptions::M_NONE && fEnableSpill && mSegmentByteLimit)
			mInterleaver.SetCutEstimator(0, &mVideoFrameIterator);
	} else {
		mInterleaver.EndStream(0);
	}

	// Initialize input window display.

	if (vSrc && mpOutputSystem->AcceptsVideo())
		InitDisplay();

	// Create data pipes.

	if (!(mpVideoPipe = new_nothrow AVIPipe(opt->perf.pipeBufferCount, 16384)))
		throw MyMemoryError();

	if (aSrc && mpOutputSystem->AcceptsAudio()) {
		const WAVEFORMATEX *pwfex = audioStream->GetFormat();

		uint32 bytes = pwfex->nAvgBytesPerSec * 2;		// 2 seconds

		mAudioPipe.Init(bytes -= bytes % pwfex->nBlockAlign);
	}
}

void Dubber::Go(int iPriority) {
	OSVERSIONINFO ovi;

	// check the version.  if NT, don't touch the processing priority!

	ovi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	fNoProcessingPriority = GetVersionEx(&ovi) && ovi.dwPlatformId == VER_PLATFORM_WIN32_NT;

	if (!iPriority)
		iPriority = fNoProcessingPriority || !mpOutputSystem->IsRealTime() ? 5 : 6;

	// Reset timer.

	VDDEBUG("Dub: Starting multimedia timer.\n");

	if (fPreview) {
		int timerInterval = vInfo.usPerFrame / 1000;

		if (opt->video.fSyncToAudio || opt->video.nPreviewFieldMode) {
			timerInterval /= 2;
		}

		if (!mFrameTimer.Init(this, timerInterval))
			throw MyError("Couldn't initialize timer!");
	}

	// Initialize threads.

	VDDEBUG("Dub: Kickstarting threads.\n");

	if (!ThreadStart())
		throw MyError("Couldn't create processing thread");

//	if (fPreview && !fNoProcessingPriority)
	SetThreadPriority(getThreadHandle(), g_iPriorities[iPriority-1][0]);

	// Continue with other threads.

	if (!(mpIOThread = new_nothrow VDDubIOThread(
				fPhantom,
				mpOutputSystem->IsRealTime(),
				mpOutputSystem->AcceptsVideo() ? vSrc : NULL,
				mVideoFrameIterator,
				audioStream,
				mpVideoPipe,
				&mAudioPipe,
				msigAudioPipeRead,
				msigAudioPipeWrite,
				audioCorrector,
				mbAudioPipeFinalized,
				fAbort,
				aInfo,
				vInfo,
				mIOThreadCounter)))
		throw MyMemoryError();

	if (!mpIOThread->ThreadStart())
		throw MyError("Couldn't start I/O thread");

	SetThreadPriority(mpIOThread->getThreadHandle(), g_iPriorities[iPriority-1][1]);

	// Create status window during the dub.

	VDDEBUG("Dub: Creating status window.\n");

	pStatusHandler->InitLinks(&aInfo, &vInfo, aSrc, vSrc, pInput, audioStatusStream, this, opt);

	if (hwndStatus = pStatusHandler->Display(NULL, iPriority)) {
		MSG msg;

		// NOTE: WM_QUIT messages seem to get blocked if the window is dragging/sizing
		//		 or has a menu.
		while (!fAbort && GetMessage(&msg, (HWND) NULL, 0, 0)) { 
			if (guiCheckDialogs(&msg)) continue;
			if (!IsWindow(hwndStatus) || !IsDialogMessage(hwndStatus, &msg)) { 
				TranslateMessage(&msg); 
			    DispatchMessage(&msg); 
			}
	    }

	}

	Stop();

	if (fError)
		throw err;

	pStatusHandler->SetLastPosition(vInfo.cur_proc_src);
//	if (positionCallback)
//		positionCallback(vInfo.start_src, vInfo.cur_proc_src < vInfo.start_src ? vInfo.start_src : vInfo.cur_proc_src > vInfo.end_src ? vInfo.end_src : vInfo.cur_proc_src, vInfo.end_src);

	VDDEBUG("Dub: exit.\n");
}

//////////////////////////////////////////////

static void DestroyIDDrawSurface(void *pv) {
	delete (IDDrawSurface *)pv;
	DDrawDeinitialize();
}

void Dubber::Stop() {
	bool fSkipDXShutdown = false;

	if (mStopLock.xchg(1))
		return;

	fAbort = true;

	VDDEBUG("Dub: Beginning stop process.\n");

	if (mpVideoPipe)
		mpVideoPipe->abort();

	msigAudioPipeRead.signal();
	msigAudioPipeWrite.signal();

	if (blitter)
		blitter->flush();

	VDDEBUG("Dub: Killing threads.\n");

	int nThreadsToWaitOn = 0;
	HANDLE hThreads[2];

	if (this->isThreadAttached())
		hThreads[nThreadsToWaitOn++] = this->getThreadHandle();

	if (mpIOThread && mpIOThread->isThreadAttached())
		hThreads[nThreadsToWaitOn++] = mpIOThread->getThreadHandle();

	while(nThreadsToWaitOn > 0) {
		DWORD dwRes;

		dwRes = MsgWaitForMultipleObjects(nThreadsToWaitOn, hThreads, FALSE, 10000, QS_ALLINPUT);

		if (WAIT_OBJECT_0 + nThreadsToWaitOn == dwRes)
			guiDlgMessageLoop(hwndStatus);
		else if (WAIT_TIMEOUT == dwRes) {
			if (IDOK == MessageBox(g_hWnd, "Something appears to be stuck while trying to stop (thread deadlock). Abort operation and exit program?", "VirtualDub Internal Error", MB_ICONEXCLAMATION|MB_OKCANCEL)) {
				vdprotected("aborting process due to a thread deadlock") {
					ExitProcess(0);
				}
			}
		} else if ((dwRes -= WAIT_OBJECT_0) < nThreadsToWaitOn) {
			if (dwRes+1 < nThreadsToWaitOn)
				hThreads[dwRes] = hThreads[nThreadsToWaitOn - 1];
			--nThreadsToWaitOn;
		}

		VDDEBUG("\tDub: %ld threads active\n", nThreadsToWaitOn);

#ifdef _DEBUG
		if (blitter) VDDEBUG("\t\tBlitter locks active: %08lx\n", blitter->lock_state);
#endif
	}

	if (!fError && mpIOThread)
		fError = mpIOThread->GetError(err);

	delete mpIOThread;
	mpIOThread = 0;

	if (blitter)
		blitter->flush();

	VDDEBUG("Dub: Freezing status handler.\n");

	if (pStatusHandler)
		pStatusHandler->Freeze();

	VDDEBUG("Dub: Killing timers.\n");

	mFrameTimer.Shutdown();

	if (pVideoPacker) {
		VDDEBUG("Dub: Ending frame compression.\n");

		delete pVideoPacker;

		pVideoPacker = NULL;
	}

	if (pdsInput) {
		VDDEBUG("Dub: Destroying input overlay.\n");

		blitter->postAFC(0x80000000, DestroyIDDrawSurface, (void *)pdsInput);
		pdsInput = NULL;
		fSkipDXShutdown = true;
	}

	VDDEBUG("Dub: Deallocating resources.\n");

	if (mpVideoPipe)	{ delete mpVideoPipe; mpVideoPipe = NULL; }
	mAudioPipe.Shutdown();

	if (blitter)		{ delete blitter; blitter=NULL; }

	GdiFlush();

	filters.DeinitFilters();

	if (fVDecompressionOk)	{ vSrc->streamEnd(); }
	if (fADecompressionOk)	{ aSrc->streamEnd(); }

	if (audioCorrector)			{ delete audioCorrector; audioCorrector = NULL; }

	{
		std::vector<AudioStream *>::const_iterator it(mAudioStreams.begin()), itEnd(mAudioStreams.end());

		for(; it!=itEnd; ++it)
			delete *it;

		mAudioStreams.clear();
	}

	if (inputSubsetAlloc)		{ delete inputSubsetAlloc; inputSubsetAlloc = NULL; }

	VDDEBUG("Dub: Releasing display elements.\n");

	if (inputHisto)				{ delete inputHisto; inputHisto = NULL; }
	if (outputHisto)			{ delete outputHisto; outputHisto = NULL; }

	// deinitialize DirectDraw

	VDDEBUG("Dub: Deinitializing DirectDraw.\n");

	if (pdsOutput)	delete pdsOutput;	pdsOutput = NULL;

	if (!fSkipDXShutdown)	DDrawDeinitialize();

	mInputDisplay.Shutdown();
	mOutputDisplay.Shutdown();

	filters.DeallocateBuffers();
	
	delete pInvTelecine;	pInvTelecine = NULL;

	if (outputDecompressor)	{
		ICDecompressEnd(outputDecompressor);
		ICClose(outputDecompressor);
		outputDecompressor = NULL;
	}

	if (AVIout) {
		delete AVIout;
		AVIout = NULL;
	}
}

///////////////////////////////////////////////////////////////////

void Dubber::TimerCallback() {
	if (opt->video.fSyncToAudio) {
		long lActualPoint;
		AVIAudioPreviewOutputStream *pAudioOut = (AVIAudioPreviewOutputStream *)AVIout->audioOut;

		lActualPoint = pAudioOut->getPosition();

		if (!pAudioOut->isFrozen()) {
			if (opt->video.nPreviewFieldMode) {
				mPulseClock = MulDiv(lActualPoint, 2000, vInfo.usPerFrame);
			} else {
				mPulseClock = MulDiv(lActualPoint, 1000, vInfo.usPerFrame);
			}

			if (mPulseClock<0)
				mPulseClock = 0;

			if (lActualPoint != -1) {
				blitter->setPulseClock(mPulseClock);
				fSyncToAudioEvenClock = false;
				mbAudioFrozen = false;
				return;
			}
		}

		// Hmm... we have no clock!

		mbAudioFrozen = true;

		if (fSyncToAudioEvenClock || opt->video.nPreviewFieldMode) {
			if (blitter) {
				blitter->pulse();
			}
		}

		fSyncToAudioEvenClock = !fSyncToAudioEvenClock;

		return;
	}


	if (blitter) blitter->pulse();
}

///////////////////////////////////////////////////////////////////

void Dubber::NextSegment() {
	vdautoptr<AVIOutput> AVIout_new(mpOutputSystem->CreateSegment());
	mpOutputSystem->CloseSegment(AVIout, false);
	AVIout = AVIout_new.release();
}

#define BUFFERID_INPUT (1)
#define BUFFERID_OUTPUT (2)
#define BUFFERID_PACKED (4)

Dubber::VideoWriteResult Dubber::WriteVideoFrame(void *buffer, int exdata, int droptype, LONG lastSize, VDPosition sample_num, VDPosition display_num, VDPosition timeline_num) {
	LONG dwBytes;
	bool isKey;
	void *frameBuffer;
	LPVOID lpCompressedData;

	if (timeline_num >= 0)		// exclude injected frames
		vInfo.cur_proc_src = timeline_num;

	// Preview fast drop -- if there is another keyframe in the pipe, we can drop
	// all the frames to it without even decoding them!
	//
	// Anime song played during development of this feature: "Trust" from the
	// Vandread OST.

	if (fPreview && opt->perf.fDropFrames) {

		// If audio is frozen, force frames to be dropped.

		bool bDrop = true;

		bDrop = !vSrc->isDecodable(sample_num);

		if (mbAudioFrozen && mbAudioFrozenValid) {
			lDropFrames = 1;
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

				if (indep == 0x3FFFFFFF && vSrc->nearestKey(display_num + opt->video.frameRateDecimation*2) > display_num)
					indep = 0;

				if (indep < lDropFrames) {
					bDrop = true;
				}
			}
		}

		if (bDrop) {
			if (!(exdata&2)) {
				blitter->nextFrame(opt->video.nPreviewFieldMode ? 2 : 1);
			}
			++fsi.lCurrentFrame;
			if (lDropFrames)
				--lDropFrames;

			pStatusHandler->NotifyNewFrame(0);

			return kVideoWriteDiscarded;
		}
	}

	// With Direct mode, write video data directly to output.

	if (opt->video.mode == DubVideoOptions::M_NONE || fPhantom) {
		if (!AVIout->videoOut->write((exdata & 1) ? 0 : AVIIF_KEYFRAME, (char *)buffer, lastSize, 1))
			throw MyError("Error writing video frame.");

		vInfo.total_size += lastSize + 24;
		++vInfo.processed;
		pStatusHandler->NotifyNewFrame(lastSize | (exdata&1 ? 0x80000000 : 0));
		mInterleaver.AddVBRCorrection(0, lastSize);

		return kVideoWriteOK;
	}

	// Fast Repack: Decompress data and send to compressor (possibly non-RGB).
	// Slow Repack: Decompress data and send to compressor.
	// Full:		Decompress, process, filter, convert, send to compressor.

	blitter->lock(BUFFERID_INPUT);

	if (exdata & kBufferFlagPreload)
		mProcessingProfileChannel.Begin(0xfff0f0, "V-Preload");
	else
		mProcessingProfileChannel.Begin(0xffe0e0, "V-Decode");

	VDCHECKPOINT;
	CHECK_FPU_STACK
	vSrc->streamGetFrame(buffer, lastSize, !(exdata&kBufferFlagDelta), false, sample_num);
	CHECK_FPU_STACK

	VDCHECKPOINT;

	mProcessingProfileChannel.End();

	if (exdata & kBufferFlagPreload) {
		blitter->unlock(BUFFERID_INPUT);
		return kVideoWriteBuffered;
	}

	if (lDropFrames && fPreview) {
		blitter->unlock(BUFFERID_INPUT);
		blitter->nextFrame(opt->video.nPreviewFieldMode ? 2 : 1);
		++fsi.lCurrentFrame;
		--lDropFrames;

		pStatusHandler->NotifyNewFrame(0);

		return kVideoWriteDiscarded;
	}

	// Process frame to backbuffer for Full video mode.  Do not process if we are
	// running in Repack mode only!
	if (opt->video.mode == DubVideoOptions::M_FULL) {
		VBitmap *initialBitmap = filters.InputBitmap();
		VBitmap *lastBitmap = filters.LastBitmap();
		VBitmap *outputBitmap = filters.OutputBitmap();
		VBitmap destbm;
		long lInputFrameNum, lInputFrameNum2;

		lInputFrameNum = display_num - vSrc->lSampleFirst;

		if (pInvTelecine) {
			lInputFrameNum2 = pInvTelecine->ProcessOut(initialBitmap);
			pInvTelecine->ProcessIn(&VBitmap(vSrc->getFrameBuffer(), vSrc->getDecompressedFormat()), lInputFrameNum);

			lInputFrameNum = lInputFrameNum2;

			if (lInputFrameNum < 0) {
				blitter->unlock(BUFFERID_INPUT);
				return kVideoWriteBuffered;
			}
		} else
			initialBitmap->BitBlt(0, 0, &VBitmap(vSrc->getFrameBuffer(), vSrc->getDecompressedFormat()), 0, 0, -1, -1);


		if (inputHisto) {
			inputHisto->Zero();
			inputHisto->Process(filters.InputBitmap());
		}

		// process frame

		fsi.lCurrentSourceFrame	= lInputFrameNum;
		fsi.lSourceFrameMS		= (long)vInfo.frameRateIn.scale64ir(fsi.lCurrentSourceFrame * (sint64)1000);
		fsi.lDestFrameMS		= (long)vInfo.frameRateIn.scale64ir(fsi.lCurrentFrame * (sint64)1000);

		mProcessingProfileChannel.Begin(0x008000, "V-Filter");
		filters.RunFilters();
		mProcessingProfileChannel.End();

		++fsi.lCurrentFrame;

		if (nVideoLagPreload>0) {
			--nVideoLagPreload;
			blitter->unlock(BUFFERID_INPUT);
			return kVideoWriteBuffered;
		}


		blitter->lock(BUFFERID_OUTPUT);

//		if (!outputDecompressor)
//			outputBitmap.data = outputBuffer;

		do {
			if (pdsOutput) {
				if (!pdsOutput->LockInverted(&destbm))
					break;

				outputBitmap = &destbm;
			}

			if (fPreview && g_prefs.fDisplay & Preferences::DISPF_DITHER16)
				outputBitmap->BitBltDither(0, 0, lastBitmap, 0, 0, -1, -1, fDisplay565);
			else if (bihDisplayFormat.bV4V4Compression == BI_BITFIELDS)
				outputBitmap->BitBlt565(0, 0, lastBitmap, 0, 0, -1, -1);
			else
				outputBitmap->BitBlt(0, 0, lastBitmap, 0, 0, -1, -1);

			if (pdsOutput)
				pdsOutput->Unlock();

		} while(false);
	}

	// write it to the file
	
	frameBuffer = 		/*(opt->video.mode == DubVideoOptions::M_FASTREPACK ? buffer : */
						opt->video.mode <= DubVideoOptions::M_SLOWREPACK ? vSrc->getFrameBuffer()
						:filters.OutputBitmap()->data;


	if (pVideoPacker) {
		mProcessingProfileChannel.Begin(0x80c080, "V-Compress");
		lpCompressedData = pVideoPacker->packFrame(frameBuffer, &isKey, &dwBytes);
		mProcessingProfileChannel.End();

		// Check if codec buffered a frame.

		if (!lpCompressedData) {
			return kVideoWriteDelayed;
		}

		if (fShowDecompressedFrame && outputDecompressor && dwBytes) {
			DWORD err;
			VBitmap *outputBitmap = filters.OutputBitmap();
			Pixel *outputBuffer = outputBitmap->data;

			CHECK_FPU_STACK

			DWORD dwSize = compressorVideoFormat->bmiHeader.biSizeImage;

			compressorVideoFormat->bmiHeader.biSizeImage = dwBytes;

			VDCHECKPOINT;
			if (ICERR_OK != (err = ICDecompress(outputDecompressor,
				isKey ? 0 : ICDECOMPRESS_NOTKEYFRAME,
				AVIout->videoOut->getImageFormat(),
				lpCompressedData,
				&compressorVideoFormat->bmiHeader,
				outputBuffer
				)))

				fShowDecompressedFrame = false;
			VDCHECKPOINT;

			compressorVideoFormat->bmiHeader.biSizeImage = dwSize;

			CHECK_FPU_STACK
		}

		if (!AVIout->videoOut->write(isKey ? AVIIF_KEYFRAME : 0, (char *)lpCompressedData, dwBytes, 1))
			throw "Error writing video frame.";

	} else {

		VDCHECKPOINT;
		if (!AVIout->videoOut->write(AVIIF_KEYFRAME, (char *)frameBuffer, AVIout->videoOut->getImageFormat()->biSizeImage, 1))
			throw MyError("Error writing video frame.");
		VDCHECKPOINT;

		dwBytes = AVIout->videoOut->getImageFormat()->biSizeImage;
		isKey = true;
	}

	vInfo.total_size += dwBytes + 24;
	mInterleaver.AddVBRCorrection(0, dwBytes + (dwBytes&1));

	VDCHECKPOINT;

	if (fPreview || mRefreshFlag.xchg(0)) {
		if (opt->video.fShowInputFrame) {
			if (inputHisto) {
				inputHisto->Draw(hDCWindow, &rInputHistogram);
			}

			if (pdsInput) {
				if (opt->video.nPreviewFieldMode)
					blitter->postDirectDrawCopyLaced(
							BUFFERID_INPUT,
							vSrc->getFrameBuffer(),
							vSrc->getDecompressedFormat(),
							pdsInput,
							opt->video.nPreviewFieldMode>=2
					);
				else
					blitter->postDirectDrawCopy(
							BUFFERID_INPUT,
							vSrc->getFrameBuffer(),
							vSrc->getDecompressedFormat(),
							pdsInput
					);
			} else if (HDC hdcInput = mInputDisplay.GetHDC()) {
				if (opt->video.nPreviewFieldMode)
					blitter->postBitBltLaced(
							BUFFERID_INPUT,
							hDCWindow,
							rInputFrame.left, rInputFrame.top,
							rInputFrame.right-rInputFrame.left, rInputFrame.bottom-rInputFrame.top,
							hdcInput,
							0,0,
							opt->video.nPreviewFieldMode>=2
					);
				else
					blitter->postStretchBlt(
							BUFFERID_INPUT,
							hDCWindow,
							rInputFrame.left, rInputFrame.top,
							rInputFrame.right-rInputFrame.left, rInputFrame.bottom-rInputFrame.top,
							hdcInput,
							0,0,
							vSrc->getDecompressedFormat()->biWidth,vSrc->getDecompressedFormat()->biHeight
					);
			} else if (HDRAWDIB hddInput = mInputDisplay.GetHDD())
				blitter->post(
						BUFFERID_INPUT,
						hddInput,
						hDCWindow,
						rInputFrame.left, rInputFrame.top,
						rInputFrame.right-rInputFrame.left, rInputFrame.bottom-rInputFrame.top,
						vSrc->getDecompressedFormat(),
						vSrc->getFrameBuffer(),
						0,0,
						vSrc->getDecompressedFormat()->biWidth,vSrc->getDecompressedFormat()->biHeight,
						DDF_SAME_HDC | DDF_SAME_DRAW
					);
			else if (g_fWine)
				blitter->postStretchDIBits(
						BUFFERID_INPUT,
						hDCWindow,
						rInputFrame.left, rInputFrame.top,
						rInputFrame.right-rInputFrame.left, rInputFrame.bottom-rInputFrame.top,
						0,0,
						vSrc->getDecompressedFormat()->biWidth,vSrc->getDecompressedFormat()->biHeight,
						vSrc->getFrameBuffer(),
						(LPBITMAPINFO)vSrc->getDecompressedFormat(),
						DIB_RGB_COLORS,
						SRCCOPY
					);
			else
				blitter->unlock(BUFFERID_INPUT);
		} else
			blitter->unlock(BUFFERID_INPUT);

		if (opt->video.mode == DubVideoOptions::M_FULL && opt->video.fShowOutputFrame && (!outputDecompressor || dwBytes)) {
			if (outputHisto) {
				outputHisto->Zero();
				outputHisto->Process(filters.LastBitmap());

				outputHisto->Draw(hDCWindow, &rOutputHistogram);
			}

			if (pdsOutput) {
				if (opt->video.nPreviewFieldMode)
					blitter->postDirectDrawBlitLaced(
							BUFFERID_OUTPUT,
							DDrawObtainPrimary(),
							pdsOutput,
							rOutputFrame.left+x_client, rOutputFrame.top+y_client,
							rOutputFrame.right-rOutputFrame.left, rOutputFrame.bottom-rOutputFrame.top,
							opt->video.nPreviewFieldMode>=2
							);
				else
					blitter->postDirectDrawBlit(
							BUFFERID_OUTPUT,
							DDrawObtainPrimary(),
							pdsOutput,
							rOutputFrame.left+x_client, rOutputFrame.top+y_client,
							rOutputFrame.right-rOutputFrame.left, rOutputFrame.bottom-rOutputFrame.top);
			} else if (HDC hdcOutput = mOutputDisplay.GetHDC()) {
				if (opt->video.nPreviewFieldMode)
					blitter->postBitBltLaced(
							BUFFERID_OUTPUT,
							hDCWindow,
							rOutputFrame.left, rOutputFrame.top,
							rOutputFrame.right-rOutputFrame.left, rOutputFrame.bottom-rOutputFrame.top,
							hdcOutput,
							0,0,
							opt->video.nPreviewFieldMode>=2
					);
				else
					blitter->postStretchBlt(
							BUFFERID_OUTPUT,
							hDCWindow,
							rOutputFrame.left, rOutputFrame.top,
							rOutputFrame.right-rOutputFrame.left, rOutputFrame.bottom-rOutputFrame.top,
							hdcOutput,
							0,0,
							filters.OutputBitmap()->w,
							filters.OutputBitmap()->h
					);
			} else if (HDRAWDIB hddOutput = mOutputDisplay.GetHDD())
				blitter->post(
						BUFFERID_OUTPUT,
						hddOutput,
						hDCWindow,
						rOutputFrame.left, rOutputFrame.top,
						rOutputFrame.right-rOutputFrame.left, rOutputFrame.bottom-rOutputFrame.top,
						&compressorVideoFormat->bmiHeader,
						filters.OutputBitmap()->data, //outputBuffer,
						0,0,
						filters.OutputBitmap()->w,
						filters.OutputBitmap()->h,
						DDF_SAME_HDC | DDF_SAME_DRAW
				);
			else if (g_fWine)
				blitter->postStretchDIBits(
						BUFFERID_OUTPUT,
						hDCWindow,
						rOutputFrame.left, rOutputFrame.top,
						rOutputFrame.right-rOutputFrame.left, rOutputFrame.bottom-rOutputFrame.top,
						0,0,
						filters.OutputBitmap()->w,
						filters.OutputBitmap()->h,
						filters.OutputBitmap()->data, //outputBuffer,
						(LPBITMAPINFO)&compressorVideoFormat->bmiHeader,
						DIB_RGB_COLORS,
						SRCCOPY
					);
			else
				blitter->unlock(BUFFERID_OUTPUT);

		} else
			blitter->unlock(BUFFERID_OUTPUT);
	} else {
		blitter->unlock(BUFFERID_OUTPUT);
		blitter->unlock(BUFFERID_INPUT);
		blitter->unlock(BUFFERID_PACKED);
	}

	if (opt->perf.fDropFrames && fPreview) {
		long lFrameDelta;

		lFrameDelta = blitter->getFrameDelta();

		if (opt->video.nPreviewFieldMode)
			lFrameDelta >>= 1;

		if (lFrameDelta < 0) lFrameDelta = 0;
		
		if (lFrameDelta > 0) {
			lDropFrames = lFrameDelta;
		}
	}


	blitter->nextFrame(opt->video.nPreviewFieldMode ? 2 : 1);

	++vInfo.processed;

	pStatusHandler->NotifyNewFrame(isKey ? dwBytes : dwBytes | 0x80000000);

	VDCHECKPOINT;
	return kVideoWriteOK;
}

void Dubber::WriteAudio(void *buffer, long lActualBytes, long lActualSamples) {
	if (!lActualBytes) return;

	if (!AVIout->audioOut->write(AVIIF_KEYFRAME, (char *)buffer, lActualBytes, lActualSamples))
		throw MyError("Error writing audio data.");

	aInfo.cur_proc_src += lActualBytes;
}

void Dubber::ThreadRun() {
	bool quit = false;
	bool firstPacket = true;
	bool bVideoEnded = (vSrc && mpOutputSystem->AcceptsVideo());
	bool bAudioEnded = (aSrc && mpOutputSystem->AcceptsAudio());
	bool bOverflowReportedThisSegment = false;
	uint32	nVideoFramesDelayed = 0;

	lDropFrames = 0;
	vInfo.processed = 0;

	VDDEBUG("Dub/Processor: start\n");

	std::vector<char>	audioBuffer;

	try {
		DEFINE_SP(sp);

		for(;;) {
			int stream;
			sint32 count;

			VDStreamInterleaver::Action nextAction = mInterleaver.GetNextAction(stream, count);

			if (!bOverflowReportedThisSegment && mInterleaver.HasSegmentOverflowed()) {
				VDLogAppMessage(kVDLogWarning, kVDST_Dub, kVDM_SegmentOverflowOccurred);
				bOverflowReportedThisSegment = true;
			}

			++mProcessingThreadCounter;

			if (nextAction == VDStreamInterleaver::kActionFinish)
				break;
			else if (nextAction == VDStreamInterleaver::kActionNextSegment) {
				VDLogAppMessage(kVDLogMarker, kVDST_Dub, kVDM_BeginningNextSegment);
				NextSegment();
				bOverflowReportedThisSegment = false;
			} else if (nextAction == VDStreamInterleaver::kActionWrite) {
				if (stream == 0) {
					if (vInfo.cur_proc_dst >= vInfo.end_proc_dst) {
						if (fPreview && aSrc) {
							((AVIAudioPreviewOutputStream *)AVIout->audioOut)->start();
							mbAudioFrozenValid = true;
						}
						mInterleaver.EndStream(0);
						bVideoEnded = true;
					} else {
						// We cannot wrap the entire loop with a profiling event because typically
						// involves a nice wait in preview mode.

						for(;;) {
							void *buf;
							long len;
							VDPosition	rawFrame;
							VDPosition	displayFrame;
							VDPosition	timelineFrame;
							int exdata;
							int handle;
							int droptype;

							buf = mpVideoPipe->getReadBuffer(len, rawFrame, displayFrame, timelineFrame, &exdata, &droptype, &handle);
							if (!buf) {
								if (nVideoFramesDelayed > 0) {
									--nVideoFramesDelayed;

									buf			= "";
									len			= 0;
									rawFrame		= -1;
									displayFrame	= -1;
									timelineFrame	= -1;
									exdata		= 0;
									droptype	= AVIPipe::kDroppable;
									handle		= -1;
								} else {
									mInterleaver.EndStream(0);
									bVideoEnded = true;
									break;
								}
							}

							if (firstPacket && fPreview && !aSrc) {
								blitter->enablePulsing(true);
								firstPacket = false;
							}

							VideoWriteResult result = WriteVideoFrame(buf, exdata, droptype, len, rawFrame, displayFrame, timelineFrame);

							if (result == kVideoWriteDelayed)
								++nVideoFramesDelayed;

							if (fPreview && aSrc) {
								((AVIAudioPreviewOutputStream *)AVIout->audioOut)->start();
								mbAudioFrozenValid = true;
							}

							if (handle >= 0)
								mpVideoPipe->releaseBuffer(handle);

							if (result == kVideoWriteOK || result == kVideoWriteDiscarded)
								break;
						}
						++vInfo.cur_proc_dst;
					}
				} else if (stream == 1) {
					mProcessingProfileChannel.Begin(0xe0e0ff, "Audio");

					const int nBlockAlign = audioStream->GetFormat()->nBlockAlign;
					int bytes = count * nBlockAlign;
					int bytesread = 0;

					if (audioBuffer.size() < bytes)
						audioBuffer.resize(bytes);

					while(bytesread < bytes) {
						int tc = mAudioPipe.Read(&audioBuffer[bytesread], bytes-bytesread);

						if (fAbort)
							goto abort_requested;

						if (!tc) {
							if (mbAudioPipeFinalized) {
								bytesread -= bytesread % nBlockAlign;
								count = bytesread / nBlockAlign;
								mInterleaver.AddCBRCorrection(1, count);
								mInterleaver.EndStream(1);
								bAudioEnded = true;
								break;
							}

							msigAudioPipeWrite.wait();
						}

						bytesread += tc;
					}

					if (count > 0) {
						WriteAudio(&audioBuffer.front(), bytesread, count);
						msigAudioPipeRead.signal();

						if (firstPacket && fPreview) {
							AVIout->audioOut->flush();
							blitter->enablePulsing(true);
							firstPacket = false;
							mbAudioFrozen = false;
						}
					}

					mProcessingProfileChannel.End();
				} else {
					VDNEVERHERE;
				}
			}

			CHECK_STACK(sp);

			if (fAbort)
				break;

			if (!bVideoEnded || !bAudioEnded)
				continue;
		}
abort_requested:
		;

	} catch(MyError& e) {
		if (!fError) {
			err.TransferFrom(e);
			fError = true;
		}
		mpVideoPipe->abort();
		fAbort = true;
	}

	mpVideoPipe->isFinalized();

	// if preview mode, choke the audio

	if (AVIout && mpOutputSystem->AcceptsAudio() && mpOutputSystem->IsRealTime())
		((AVIAudioPreviewOutputStream *)AVIout->audioOut)->stop();

	// finalize the output.. if it's not a preview...
	if (!mpOutputSystem->IsRealTime()) {
		// update audio rate...

		if (audioCorrector) {
			WAVEFORMATEX *wfex = AVIout->audioOut->getWaveFormat();
			
			wfex->nAvgBytesPerSec = audioCorrector->ComputeByterate(wfex->nSamplesPerSec);

			AVIout->audioOut->streamInfo.dwRate = wfex->nAvgBytesPerSec
				* AVIout->audioOut->streamInfo.dwScale;
		}

		// finalize avi

		mpOutputSystem->CloseSegment(AVIout, true);
		AVIout = NULL;
		VDDEBUG("Dub/Processor: finalized.\n");
	}

	VDDEBUG("Dub/Processor: end\n");
}

///////////////////////////////////////////////////////////////////

void Dubber::Abort() {
	fUserAbort = true;
	fAbort = true;
	PostMessage(g_hWnd, WM_USER, 0, 0);
}

bool Dubber::isAbortedByUser() {
	return fUserAbort;
}

bool Dubber::IsPreviewing() {
	return fPreview;
}

void Dubber::Tag(int x, int y) {
	POINT p;

	p.x = x;
	p.y = y;

	if (PtInRect(&rInputFrame, p))
		opt->video.fShowInputFrame = !opt->video.fShowInputFrame;
	else if (PtInRect(&rOutputFrame, p))
		opt->video.fShowOutputFrame = !opt->video.fShowOutputFrame;
	else if (PtInRect(&rInputHistogram, p)) {
		if (inputHisto) inputHisto->SetMode(Histogram::MODE_NEXT);
	} else if (PtInRect(&rOutputHistogram, p)) {
		if (outputHisto) outputHisto->SetMode(Histogram::MODE_NEXT);
	}
}

void Dubber::RealizePalette() {
	if (HDRAWDIB hddOutput = mOutputDisplay.GetHDD())
		DrawDibRealize(hddOutput, hDCWindow, FALSE);
}

void Dubber::SetPriority(int index) {
	SetThreadPriority(mpIOThread->getThreadHandle(), g_iPriorities[index][0]);
	SetThreadPriority(getThreadHandle(), g_iPriorities[index][1]);
}

void Dubber::UpdateFrames() {
	mRefreshFlag = 1;

	if (!mStopLock) {
		uint32 curTime = VDGetCurrentTick();

		int iocount = mIOThreadCounter;
		int prcount = mProcessingThreadCounter;

		if (mLastIOThreadCounter != iocount) {
			mLastIOThreadCounter = iocount;
			mIOThreadFailCount = curTime;
		} else if (mLastIOThreadCounter && (curTime - mIOThreadFailCount - 10000) < 3600000) {		// 10s to 1hr
			VDLogAppMessage(kVDLogWarning, kVDST_Dub, kVDM_IOThreadLivelock);
			mLastIOThreadCounter = 0;
		}

		if (mLastProcessingThreadCounter != prcount) {
			mLastProcessingThreadCounter = prcount;
			mProcessingThreadFailCount = curTime;
		} else if (mLastProcessingThreadCounter && (curTime - mProcessingThreadFailCount - 10000) < 3600000) {		// 10s to 1hr
			VDLogAppMessage(kVDLogWarning, kVDST_Dub, kVDM_ProcessingThreadLivelock);
			mLastProcessingThreadCounter = 0;
		}
	}
}
