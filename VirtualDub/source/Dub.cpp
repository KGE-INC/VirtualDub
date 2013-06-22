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

#define f_DUB_CPP


#include <process.h>
#include <time.h>
#include <vector>
#include <deque>
#include <utility>

#include <windows.h>
#include <vfw.h>

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
#include "AudioSource.h"
#include "VideoSource.h"
#include "AVIPipe.h"
#include "VBitmap.h"
#include "FrameSubset.h"
#include "InputFile.h"
#include "VideoTelecineRemover.h"
#include "VideoDisplay.h"

#include "Dub.h"
#include "DubOutput.h"
#include "DubStatus.h"
#include "DubUtils.h"
#include "DubIO.h"

using namespace nsVDDub;

///////////////////////////////////////////////////////////////////////////

extern const char g_szError[];
extern bool g_syncroBlit;
extern HWND g_hWnd;
extern bool g_fWine;

///////////////////////////////////////////////////////////////////////////

namespace {
	enum { kVDST_Dub = 1 };

	enum {
		kVDM_SegmentOverflowOccurred,
		kVDM_BeginningNextSegment,
		kVDM_IOThreadLivelock,
		kVDM_ProcessingThreadLivelock,
		kVDM_CodecDelayedDuringDelayedFlush,
		kVDM_CodecLoopingDuringDelayedFlush,
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
void AVISTREAMINFOtoAVIStreamHeader(AVIStreamHeader_fixed *dest, const AVISTREAMINFO *src) {
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

namespace {
	bool VDIsDC565(HDC hdc) {
		bool bIs565 = false;

		int bitsPerPel = GetDeviceCaps(hdc, BITSPIXEL);

		if (bitsPerPel==15 || bitsPerPel==16) {
			COLORREF crTmp;
			crTmp = GetPixel(hdc, 0,0);
			SetPixel(hdc, 0, 0, RGB(0x80, 0x88, 0x80));

			if (GetPixel(hdc,0,0) == RGB(0x80, 0x88, 0x80))
				bIs565 = true;

			SetPixel(hdc, 0, 0, crTmp);
		}

		return bIs565;
	}
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
	IVDVideoSource		*vSrc;
	InputFile			*pInput;
	AVIOutput			*AVIout;
	IVDMediaOutputStream		*mpAudioOut;			// alias: AVIout->audioOut
	IVDMediaOutputStream		*mpVideoOut;			// alias: AVIout->videoOut
	IVDDubberOutputSystem	*mpOutputSystem;
	COMPVARS			*compVars;

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
	VDAudioPipeline		mAudioPipe;

	AsyncBlitter *		blitter;

	HIC					outputDecompressor;

	IVDVideoDisplay *	mpInputDisplay;
	IVDVideoDisplay *	mpOutputDisplay;
	bool				mbInputDisplayInitialized;
	bool				fShowDecompressedFrame;
	bool				fDisplay565;

	int					iOutputDepth;
	VDFormatStruct<BITMAPINFOHEADER>	mpCompressorVideoFormat;
	VDFormatStruct<BITMAPINFOHEADER>	mpDisplayFormat;

	std::vector<AudioStream *>	mAudioStreams;
	AudioStream			*audioStream, *audioTimingStream;
	AudioStream			*audioStatusStream;
	AudioStreamL3Corrector	*audioCorrector;
	vdautoptr<VDAudioFilterGraph> mpAudioFilterGraph;

	FrameSubset				*inputSubsetActive;
	FrameSubset				*inputSubsetAlloc;
	VideoTelecineRemover	*pInvTelecine;
	int					nVideoLagPreload;

	VDFormatStruct<WAVEFORMATEX> mAudioCompressionFormat;

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
	void SetInputDisplay(IVDVideoDisplay *);
	void SetOutputDisplay(IVDVideoDisplay *);
	void SetInputFile(InputFile *pInput);
	void SetFrameRectangles(RECT *prInput, RECT *prOutput);
	void SetAudioFilterGraph(const VDAudioFilterGraph& graph);
	void EnableSpill(sint64 threshold, long framethreshold);

	void InitAudioConversionChain();
	void InitOutputFile();
	bool AttemptInputOverlay(BITMAPINFOHEADER *pbih);
	bool AttemptInputOverlays();

	void InitDirectDraw();
	void InitInputDisplay();
	void InitOutputDisplay();
	bool NegotiateFastFormat(BITMAPINFOHEADER *pbih);
	bool NegotiateFastFormat(int depth);
	void InitSelectInputFormat();
	void Init(IVDVideoSource *video, AudioSource *audio, IVDDubberOutputSystem *outsys, COMPVARS *videoCompVars);
	void Go(int iPriority = 0);
	void Stop();

	void RealizePalette();
	void Abort();
	void ForceAbort();
	bool isRunning();
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

	audioStream			= NULL;
	audioStatusStream	= NULL;
	audioCorrector		= NULL;

	inputSubsetActive	= NULL;
	inputSubsetAlloc	= NULL;

	fPhantom = false;

	pInvTelecine		= NULL;

	AVIout				= NULL;
	mpVideoOut			= NULL;
	mpAudioOut			= NULL;
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

void Dubber::SetInputDisplay(IVDVideoDisplay *pDisplay) {
	mpInputDisplay = pDisplay;
}

void Dubber::SetOutputDisplay(IVDVideoDisplay *pDisplay) {
	mpOutputDisplay = pDisplay;
}

void Dubber::SetFrameRectangles(RECT *prInput, RECT *prOutput) {
//	rInputFrame = *prInput;
//	rOutputFrame = *prOutput;

//	mInputDisplay.SetDestRect(rInputFrame.left, rInputFrame.top, rInputFrame.right-rInputFrame.left, rInputFrame.bottom - rInputFrame.top);
//	mOutputDisplay.SetDestRect(rOutputFrame.left, rOutputFrame.top, rOutputFrame.right-rOutputFrame.left, rOutputFrame.bottom - rOutputFrame.top);
}

/////////////

void Dubber::EnableSpill(sint64 segsize, long framecnt) {
	fEnableSpill = true;
	mSegmentByteLimit = segsize;
	lSegmentFrameLimit = framecnt;
}

void Dubber::SetAudioFilterGraph(const VDAudioFilterGraph& graph) {
	mpAudioFilterGraph = new VDAudioFilterGraph(graph);
}

void InitStreamValuesStatic(DubVideoStreamInfo& vInfo, DubAudioStreamInfo& aInfo, IVDVideoSource *video, AudioSource *audio, DubOptions *opt, FrameSubset *pfs) {
	if (!pfs)
		pfs = inputSubset;

	IVDStreamSource *pVideoStream = 0;
	
	if (video) {
		pVideoStream = video->asStream();

		if (pfs) {
			vInfo.start_src		= 0;
			vInfo.end_src		= pfs->getTotalFrames();
		} else {
			vInfo.start_src		= pVideoStream->getStart();
			vInfo.end_src		= pVideoStream->getEnd();
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
		aInfo.start_src		= audio->getStart();
		aInfo.end_src		= audio->getEnd();
	} else {
		aInfo.start_src		= 0;
		aInfo.end_src		= aInfo.end_dst		= 0;
	}

	vInfo.cur_src			= vInfo.start_src;
	aInfo.cur_src			= aInfo.start_src;

	if (video) {
		// compute new frame rate

		VDFraction framerate(pVideoStream->getRate());

		if (opt->video.frameRateNewMicroSecs == DubVideoOptions::FR_SAMELENGTH) {
			if (audio && audio->getLength()) {
				framerate = VDFraction::reduce64(pVideoStream->getLength() * (sint64)1000, audio->samplesToMs(audio->getLength()));
			}
		} else if (opt->video.frameRateNewMicroSecs)
			framerate = VDFraction(1000000, opt->video.frameRateNewMicroSecs);

		// are we supposed to offset the video?

		if (opt->video.lStartOffsetMS) {
			vInfo.start_src += pVideoStream->msToSamples(opt->video.lStartOffsetMS); 
		}

		if (opt->video.lEndOffsetMS)
			vInfo.end_src -= pVideoStream->msToSamples(opt->video.lEndOffsetMS);

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
			const sint64 video_start	= vInfo.start_src - pVideoStream->getStart();
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

			const VDFraction audiorate(audio->getRate());

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

		aInfo.cur_src		= aInfo.start_src;
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

	bool bUseAudioFilterGraph = (opt->audio.mode > DubAudioOptions::M_NONE && mpAudioFilterGraph);

	if (bUseAudioFilterGraph) {
		audioStream = new_nothrow AudioFilterSystemStream(*mpAudioFilterGraph, aInfo.start_us);
		if (!audioStream)
			throw MyMemoryError();

		mAudioStreams.push_back(audioStream);
	} else {
		// First, create a source.

		if (!(audioStream = new_nothrow AudioStreamSource(aSrc, aInfo.start_src, aSrc->getEnd() - aInfo.start_src, opt->audio.mode > DubAudioOptions::M_NONE)))
			throw MyMemoryError();

		mAudioStreams.push_back(audioStream);
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

	if (!bUseAudioFilterGraph) {
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

		if (!(audioCorrector = new_nothrow AudioStreamL3Corrector(audioStream)))
			throw MyMemoryError();

		audioStream = audioCorrector;
		mAudioStreams.push_back(audioStream);
	}

}

void Dubber::InitOutputFile() {

	// Do audio.

	if (aSrc && mpOutputSystem->AcceptsAudio()) {
		// initialize AVI parameters...

		AVIStreamHeader_fixed	hdr;

		AVISTREAMINFOtoAVIStreamHeader(&hdr, &aSrc->getStreamInfo());
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
		VBitmap outputBitmap;
		
		if (opt->video.mode >= DubVideoOptions::M_FULL)
			outputBitmap = *filters.OutputBitmap();
		else
			outputBitmap.init(vSrc->getFrameBuffer(), vSrc->getDecompressedFormat());

		AVIStreamHeader_fixed hdr;

		AVISTREAMINFOtoAVIStreamHeader(&hdr, &vSrc->asStream()->getStreamInfo());

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
		hdr.rcFrame.right	= (short)outputBitmap.w;
		hdr.rcFrame.bottom	= (short)outputBitmap.h;

		// initialize compression

		if (opt->video.mode >= DubVideoOptions::M_FASTREPACK) {
			if (opt->video.mode <= DubVideoOptions::M_SLOWREPACK) {
				const BITMAPINFOHEADER *pFormat = vSrc->getDecompressedFormat();

				mpCompressorVideoFormat.assign(pFormat, VDGetSizeOfBitmapHeaderW32(pFormat));
			} else {
				mpCompressorVideoFormat.resize(sizeof(BITMAPINFOHEADER));

				mpCompressorVideoFormat->biSize				= sizeof(BITMAPINFOHEADER);
				mpCompressorVideoFormat->biWidth			= outputBitmap.w;
				mpCompressorVideoFormat->biHeight			= outputBitmap.h;
				mpCompressorVideoFormat->biPlanes			= 1;
				mpCompressorVideoFormat->biBitCount			= iOutputDepth;
				mpCompressorVideoFormat->biCompression		= BI_RGB;
				mpCompressorVideoFormat->biSizeImage		= outputBitmap.pitch * outputBitmap.h;
				mpCompressorVideoFormat->biXPelsPerMeter	= 0;
				mpCompressorVideoFormat->biYPelsPerMeter	= 0;
				mpCompressorVideoFormat->biClrUsed			= 0;
				mpCompressorVideoFormat->biClrImportant		= 0;
			}
		} else {
			const BITMAPINFOHEADER *pFormat = vSrc->getImageFormat();

			mpCompressorVideoFormat.assign(pFormat, vSrc->asStream()->getFormatLen());
		}

		// Initialize output compressor.

		VDDEBUG("Dub: Initializing output compressor.\n");

		VDFormatStruct<BITMAPINFOHEADER>	outputFormat;

		if (fUseVideoCompression) {
			LONG formatSize;
			DWORD icErr;

			formatSize = ICCompressGetFormatSize(compVars->hic, (LPBITMAPINFO)&*mpCompressorVideoFormat);
			if (formatSize < ICERR_OK)
				throw "Error getting compressor output format size.";

			VDDEBUG("Video compression format size: %ld\n",formatSize);

			outputFormat.resize(formatSize);

			// Huffyuv doesn't initialize a few padding bytes at the end of its format
			// struct, so we clear them here.
			memset(&*outputFormat, 0, outputFormat.size());

			if (ICERR_OK != (icErr = ICCompressGetFormat(compVars->hic,
								&*mpCompressorVideoFormat,
								&*outputFormat)))
				throw MyICError("Output compressor", icErr);

			if (!(pVideoPacker = new VideoSequenceCompressor()))
				throw MyMemoryError();

			pVideoPacker->init(compVars->hic, (LPBITMAPINFO)&*mpCompressorVideoFormat, (BITMAPINFO *)&*outputFormat, compVars->lQ, compVars->lKey);
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
							&*mpCompressorVideoFormat, ICMODE_DECOMPRESS))) {

					MyError("Output video warning: Could not locate output decompressor.").post(NULL,g_szError);

				} else if (ICERR_OK != (err = ICDecompressBegin(
						outputDecompressor,
						&*outputFormat,
						&*mpCompressorVideoFormat))) {

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

				IVDStreamSource *pVideoStream = vSrc->asStream();

				outputFormat.assign(vSrc->getImageFormat(), pVideoStream->getFormatLen());

				// cheese
				const VDPosition videoFrameStart	= pVideoStream->getStart();
				const VDPosition videoFrameEnd		= pVideoStream->getEnd();

				lVideoSizeEstimate = 0;
				for(VDPosition frame = videoFrameStart; frame < videoFrameEnd; ++frame) {
					long bytes = 0;

					if (!pVideoStream->read(frame, 1, 0, 0, &bytes, 0))
						if (lVideoSizeEstimate < bytes)
							lVideoSizeEstimate = bytes;
				}
			} else {
				outputFormat.assign(vSrc->getDecompressedFormat(), sizeof(BITMAPINFOHEADER));

				if (opt->video.mode == DubVideoOptions::M_FULL) {
					outputFormat->biCompression= BI_RGB;
					outputFormat->biWidth		= outputBitmap.w;
					outputFormat->biHeight		= outputBitmap.h;
					outputFormat->biBitCount	= iOutputDepth;
					outputFormat->biSizeImage	= outputBitmap.pitch * outputBitmap.h;
				}

				lVideoSizeEstimate = outputFormat->biSizeImage;
				lVideoSizeEstimate = (lVideoSizeEstimate+1) & -2;
			}
		}

		mpOutputSystem->SetVideo(hdr, &*outputFormat, outputFormat.size());
	}

	VDDEBUG("Dub: Creating output file.\n");

	AVIout = mpOutputSystem->CreateSegment();
	mpVideoOut = AVIout->getVideoOutput();
	mpAudioOut = AVIout->getAudioOutput();
}

bool Dubber::AttemptInputOverlay(BITMAPINFOHEADER *pbih) {
//	return vSrc->setDecompressedFormat(pbih) && mInputDisplay.InitDirectDrawYUVOverlay(mhwndInputDisplay, pbih, vSrc->getFrameBuffer());

	if (!vSrc->setDecompressedFormat(pbih))
		return false;

	int format;

	switch(pbih->biCompression) {
		case 'YVYU':
			format = IVDVideoDisplay::kFormatYUV422_UYVY;
			break;
		case '2YUY':
			format = IVDVideoDisplay::kFormatYUV422_YUYV;
			break;
		default:
			return false;
	}

	return mpInputDisplay->SetSource(vSrc->getFrameBuffer(), (pbih->biWidth*2 + 3) & ~3, pbih->biWidth, pbih->biHeight, format, 0, 0, false, opt->video.nPreviewFieldMode>0);
}

bool Dubber::AttemptInputOverlays() {
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

	//---- begin 16-bit YUV negotiation ----

	// Attempt CYUV (YUV 4:2:2, Y?Y? ordering)

//	bih.biCompression = 'VUYC';
//	if (AttemptInputOverlay(&bih))
//		break;

	// Attempt UYVY (YUV 4:2:2)

	bih.biCompression = 'YVYU';
	if (AttemptInputOverlay(&bih))
		return true;

	// Attempt YUYV (YUV 4:2:2)
//	bih.biCompression = 'VYUY';
//	if (AttemptInputOverlay(&bih))
//		break;

	// Attempt YUY2 (YUV 4:2:2, YUYV ordering)

	bih.biCompression = '2YUY';

	if (AttemptInputOverlay(&bih))
		return true;

	//---- begin 12-bit YUV negotiation ----
#if 0
	// Attempt YV12 (YUV 4:2:0)

	bih.biCompression = '21VY';
	bih.biSizeImage		= (bih.biWidth/2)*(bih.biHeight/2)*6;
	bih.biBitCount		= 12;

	if (AttemptInputOverlay(&bih))
		return true;
#endif

	return false;
}

void Dubber::InitDirectDraw() {

	if (!opt->perf.useDirectDraw)
		return;

	// Should we try and establish a DirectDraw overlay?

	if (opt->video.mode == DubVideoOptions::M_SLOWREPACK) {
//		blitter->sendAFC(0x80000000, AttemptInputOverlays2, this);
		if (AttemptInputOverlays())
			mbInputDisplayInitialized = true;
	}
}

void Dubber::InitInputDisplay() {
	VDDEBUG("Dub: Initializing input window display.\n");


	// Check color depth of output device.  If it is 8-bit, we're
	// stuck with DrawDibDraw().  If it's at least 15 bits, then
	// we should create a DIBSection, select it into a memory
	// context, and BltBlt() to the screen instead.  It's about
	// 5-10% faster under Win95 and about 400x (!) faster under WINE.
	//
	// Okay, never mind... WINE still doesn't support DIBSection
	// windows. :(

	const BITMAPINFOHEADER& hdr = *vSrc->getDecompressedFormat();
	const void *pFB = vSrc->getFrameBuffer();
	const ptrdiff_t stride = ((hdr.biWidth * hdr.biBitCount + 31) >> 3) & ~3;
	int format = 0;

	if (hdr.biCompression == BI_RGB && hdr.biBitCount == 16)
		format = IVDVideoDisplay::kFormatRGB1555;
	else if (hdr.biCompression == BI_RGB && hdr.biBitCount == 24)
		format = IVDVideoDisplay::kFormatRGB888;
	else if (hdr.biCompression == BI_RGB && hdr.biBitCount == 32)
		format = IVDVideoDisplay::kFormatRGB8888;
	else if (hdr.biCompression == BI_BITFIELDS && hdr.biBitCount == 16)
		format = IVDVideoDisplay::kFormatRGB565;

	mpInputDisplay->SetSource((const char *)pFB + stride * (hdr.biHeight-1), -stride, hdr.biWidth, hdr.biHeight, format, 0, 0, true, opt->video.nPreviewFieldMode>0);

//	mInputDisplay.SetInterlacing(opt->video.nPreviewFieldMode != 0, opt->video.nPreviewFieldMode > 1);
}

void Dubber::InitOutputDisplay() {
	VDDEBUG("Dub: Initializing output window display.\n");

	if (opt->video.mode == DubVideoOptions::M_FULL) {
		mpDisplayFormat = mpCompressorVideoFormat;

		// check to see if DC is 565 16-bit, the only mode that does not support a line
		// of grays... hmm... is 15 possible for bitsPerPel?

		fDisplay565 = false;
		if (HDC hdc = GetDC(NULL)) {
			fDisplay565 = VDIsDC565(hdc);
			ReleaseDC(NULL, hdc);
		}

		if (fDisplay565 && fPreview && mpDisplayFormat->biBitCount == 16) {
			mpDisplayFormat.resize(sizeof(BITMAPV4HEADER));

			BITMAPV4HEADER *pV4Hdr = (BITMAPV4HEADER *)&*mpDisplayFormat;

			pV4Hdr->bV4Size				= sizeof(BITMAPV4HEADER);
			pV4Hdr->bV4V4Compression	= BI_BITFIELDS;
			pV4Hdr->bV4RedMask			= 0xf800;
			pV4Hdr->bV4GreenMask		= 0x07e0;
			pV4Hdr->bV4BlueMask			= 0x001f;
			pV4Hdr->bV4AlphaMask		= 0x0000;
			pV4Hdr->bV4CSType			= 0;
		}

		HANDLE hMapObject;
		LONG lMapOffset;

		filters.getOutputMappingParams(hMapObject, lMapOffset);

		int format = 0;
		if (mpDisplayFormat->biCompression == BI_RGB && mpDisplayFormat->biBitCount == 16)
			format = IVDVideoDisplay::kFormatRGB1555;
		else if (mpDisplayFormat->biCompression == BI_RGB && mpDisplayFormat->biBitCount == 24)
			format = IVDVideoDisplay::kFormatRGB888;
		else if (mpDisplayFormat->biCompression == BI_RGB && mpDisplayFormat->biBitCount == 32)
			format = IVDVideoDisplay::kFormatRGB8888;
		else if (mpDisplayFormat->biCompression == BI_BITFIELDS && mpDisplayFormat->biBitCount == 16)
			format = IVDVideoDisplay::kFormatRGB565;

		const VBitmap& vbm = *filters.OutputBitmap();

		mpOutputDisplay->SetSource((const char *)vbm.data + vbm.pitch * (vbm.h - 1), -vbm.pitch, vbm.w, vbm.h, format, hMapObject, lMapOffset, true, opt->video.nPreviewFieldMode>0);

//		mOutputDisplay.InitGDI(mhwndOutputDisplay, &*mpCompressorVideoFormat, hMapObject, lMapOffset);
	}

//	mOutputDisplay.SetInterlacing(opt->video.nPreviewFieldMode != 0, opt->video.nPreviewFieldMode > 1);
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

void Dubber::Init(IVDVideoSource *video, AudioSource *audio, IVDDubberOutputSystem *pOutputSystem, COMPVARS *videoCompVars) {

	aSrc				= audio;
	vSrc				= video;
	mpOutputSystem		= pOutputSystem;

	fPreview			= mpOutputSystem->IsRealTime();

	compVars			= videoCompVars;
	fUseVideoCompression = !fPreview && opt->video.mode>DubVideoOptions::M_NONE && compVars && (compVars->dwFlags & ICMF_COMPVARS_VALID) && compVars->hic;
//	fUseVideoCompression = opt->video.mode>DubVideoOptions::M_NONE && compVars && (compVars->dwFlags & ICMF_COMPVARS_VALID) && compVars->hic;

	// check the mode; if we're using DirectStreamCopy mode, we'll need to
	// align the subset to keyframe boundaries!

	if (vSrc && inputSubset) {
		inputSubsetActive = inputSubset;

		if (opt->video.mode == DubVideoOptions::M_NONE) {
			if (!(inputSubsetActive = inputSubsetAlloc = new FrameSubset()))
				throw MyMemoryError();

			IVDStreamSource *pVideoStream = vSrc->asStream();

			const VDPosition videoFrameStart	= pVideoStream->getStart();

			for(FrameSubset::iterator it(inputSubset->begin()), itEnd(inputSubset->end()); it!=itEnd; ++it) {
				long end = it->start + it->len;
				long start = vSrc->nearestKey(it->start + videoFrameStart) - videoFrameStart;

				VDDEBUG("   subset: %5d[%5d]-%-5d\n", it->start, start, it->start+it->len-1);
				inputSubsetActive->addRange(it->start, it->len, it->bMask);

				// Mask ranges never need to be extended backwards, because they don't hold any
				// data of their own.  If an include range needs to be extended backwards, though,
				// it may need to extend into a previous merge range.  To avoid this problem,
				// we do a delete of the range before adding the tail.

				if (!it->bMask) {
					if (start < it->start) {
						FrameSubset::iterator it2(it);

						while(it2 != inputSubset->begin()) {
							--it2;

							long prevtail = it2->start + it2->len;

							if (prevtail < start || prevtail > it->start + it->len)
								break;

							if (it2->start >= start || !it2->bMask) {	// within extension range: absorb
								it->len += it->start - it2->start;
								it->start = it2->start;
								it2 = inputSubset->erase(it2);
							} else {									// before extension range and masked: split merge
								int offset = start - it->start;
								it2->len -= offset;
								it->start -= offset;
								it->len += offset;
								break;
							}
						}

						int left = it->start - start;
						
						if (left > 0) {
							it->start = start;
							it->len += left;
						}
					}
				}
			}
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

	// initialize directdraw display if in preview

	mbInputDisplayInitialized = false;
	if (fPreview)
		InitDirectDraw();

	// Select an appropriate input format.  This is really tricky...

	vInfo.fAudioOnly = true;
	if (vSrc && mpOutputSystem->AcceptsVideo()) {
		if (!mbInputDisplayInitialized)
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

	// Initialize input window display.

	if (!mbInputDisplayInitialized) {
		if (vSrc && mpOutputSystem->AcceptsVideo())
			InitInputDisplay();
	}

	// initialize output parameters and output file

	InitOutputFile();

	// Initialize output window display.

	if (vSrc && mpOutputSystem->AcceptsVideo())
		InitOutputDisplay();


	// initialize interleaver

	bool bAudio = aSrc && mpOutputSystem->AcceptsAudio();

	mInterleaver.Init(bAudio ? 2 : 1);
	mInterleaver.EnableInterleaving(opt->audio.enabled);
	mInterleaver.InitStream(0, lVideoSizeEstimate, 0, 1, 1, 1);

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

		mInterleaver.InitStream(1, pwfex->nBlockAlign, preload, samplesPerFrame, (double)audioBlocksPerVideoFrame, 262144);		// don't write TOO many samples at once
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

	// Create data pipes.

	if (!(mpVideoPipe = new_nothrow AVIPipe(opt->perf.pipeBufferCount, 16384)))
		throw MyMemoryError();

	if (aSrc && mpOutputSystem->AcceptsAudio()) {
		const WAVEFORMATEX *pwfex = audioStream->GetFormat();

		uint32 bytes = pwfex->nAvgBytesPerSec * 2;		// 2 seconds

		mAudioPipe.Init(bytes - bytes % pwfex->nBlockAlign);
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
				mpOutputSystem->AcceptsVideo() ? vSrc : NULL,
				mVideoFrameIterator,
				audioStream,
				mpVideoPipe,
				&mAudioPipe,
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

	pStatusHandler->Display(NULL, iPriority);
}

//////////////////////////////////////////////

void Dubber::Stop() {
	bool fSkipDXShutdown = false;

	if (mStopLock.xchg(1))
		return;

	fAbort = true;

	VDDEBUG("Dub: Beginning stop process.\n");

	if (mpVideoPipe)
		mpVideoPipe->abort();

	mAudioPipe.Abort();

	if (blitter)
		blitter->beginFlush();

	VDDEBUG("Dub: Killing threads.\n");

	int nObjectsToWaitOn = 0;
	HANDLE hObjects[3];

	if (VDSignal *pBlitterSigComplete = blitter->getFlushCompleteSignal())
		hObjects[nObjectsToWaitOn++] = pBlitterSigComplete->getHandle();

	if (this->isThreadAttached())
		hObjects[nObjectsToWaitOn++] = this->getThreadHandle();

	if (mpIOThread && mpIOThread->isThreadAttached())
		hObjects[nObjectsToWaitOn++] = mpIOThread->getThreadHandle();

	uint32 startTime = VDGetCurrentTick();

	while(nObjectsToWaitOn > 0) {
		DWORD dwRes;

		dwRes = MsgWaitForMultipleObjects(nObjectsToWaitOn, hObjects, FALSE, 10000, QS_ALLINPUT);

		if (WAIT_OBJECT_0 + nObjectsToWaitOn == dwRes) {
			guiDlgMessageLoop(hwndStatus);
			continue;
		}
		
		uint32 currentTime = VDGetCurrentTick();

		if ((dwRes -= WAIT_OBJECT_0) < nObjectsToWaitOn) {
			if (dwRes+1 < nObjectsToWaitOn)
				hObjects[dwRes] = hObjects[nObjectsToWaitOn - 1];
			--nObjectsToWaitOn;
			startTime = currentTime;
			continue;
		}

		if (currentTime - startTime > 10000) {
			if (IDOK == MessageBox(g_hWnd, "Something appears to be stuck while trying to stop (thread deadlock). Abort operation and exit program?", "VirtualDub Internal Error", MB_ICONEXCLAMATION|MB_OKCANCEL)) {
				vdprotected("aborting process due to a thread deadlock") {
					ExitProcess(0);
				}
			}

			startTime = currentTime;
		}


		VDDEBUG("\tDub: %ld threads active\n", nObjectsToWaitOn);

#ifdef _DEBUG
		if (blitter) VDDEBUG("\t\tBlitter locks active: %08lx\n", blitter->lock_state);
#endif
	}

	if (!fError && mpIOThread)
		fError = mpIOThread->GetError(err);

	delete mpIOThread;
	mpIOThread = 0;

	if (blitter)
		blitter->abort();

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

	VDDEBUG("Dub: Deallocating resources.\n");

	if (mpVideoPipe)	{ delete mpVideoPipe; mpVideoPipe = NULL; }
	mAudioPipe.Shutdown();

	if (blitter)		{ delete blitter; blitter=NULL; }

	filters.DeinitFilters();

	if (fVDecompressionOk)	{ vSrc->asStream()->streamEnd(); }
	if (fADecompressionOk)	{ aSrc->streamEnd(); }

	{
		std::vector<AudioStream *>::const_iterator it(mAudioStreams.begin()), itEnd(mAudioStreams.end());

		for(; it!=itEnd; ++it)
			delete *it;

		mAudioStreams.clear();
	}

	if (inputSubsetAlloc)		{ delete inputSubsetAlloc; inputSubsetAlloc = NULL; }

	VDDEBUG("Dub: Releasing display elements.\n");

	// deinitialize DirectDraw

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
		mpAudioOut = NULL;
		mpVideoOut = NULL;
	}

	if (pStatusHandler)
		pStatusHandler->SetLastPosition(vInfo.cur_proc_src);

	if (fError) {
		throw err;
		fError = false;
	}
}

///////////////////////////////////////////////////////////////////

void Dubber::TimerCallback() {
	if (opt->video.fSyncToAudio) {
		if (mpAudioOut) {
			long lActualPoint;
			AVIAudioPreviewOutputStream *pAudioOut = static_cast<AVIAudioPreviewOutputStream *>(mpAudioOut);

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
		}

		// Audio's frozen, so we have to free-run.  When 'sync to audio' is on
		// and field-based preview is off, we have to divide the 2x clock down
		// to 1x.

		mbAudioFrozen = true;

		if (fSyncToAudioEvenClock || opt->video.nPreviewFieldMode) {
			if (blitter) {
				blitter->pulse();
			}
		}

		fSyncToAudioEvenClock = !fSyncToAudioEvenClock;

		return;
	}

	// When 'sync to audio' is off we use a 1x clock.
	if (blitter)
		blitter->pulse();
}

///////////////////////////////////////////////////////////////////

void Dubber::NextSegment() {
	vdautoptr<AVIOutput> AVIout_new(mpOutputSystem->CreateSegment());
	mpOutputSystem->CloseSegment(AVIout, false);
	AVIout = AVIout_new.release();
	mpAudioOut = AVIout->getAudioOutput();
	mpVideoOut = AVIout->getVideoOutput();
}

#define BUFFERID_INPUT (1)
#define BUFFERID_OUTPUT (2)

namespace {
	bool AsyncUpdateCallback(int pass, void *pDisplayAsVoid, void *pInterlaced) {
		IVDVideoDisplay *pVideoDisplay = (IVDVideoDisplay *)pDisplayAsVoid;
		int nFieldMode = *(int *)pInterlaced;

		if (nFieldMode) {
			if ((pass^nFieldMode)&1)
				pVideoDisplay->Update(IVDVideoDisplay::kEvenFieldOnly);
			else
				pVideoDisplay->Update(IVDVideoDisplay::kOddFieldOnly);

			return !pass;
		} else {
			pVideoDisplay->Update(IVDVideoDisplay::kAllFields);
			return false;
		}
	}
}

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

//	VDDEBUG2("writing sample %d (display %d), type = %s, drop = %d\n", (int)sample_num, (int)display_num, droptype==AVIPipe::kDroppable ? "droppable" : droptype == AVIPipe::kDependant ? "dependant" : "independent", lDropFrames);

	if (fPreview && opt->perf.fDropFrames) {

		// If audio is frozen, force frames to be dropped.

		bool bDrop = true;

		bDrop = !vSrc->isDecodable(sample_num);
//		if (bDrop)
//			VDDEBUG2("Dubber: Forced drop (undecodable)\n");

		if (mbAudioFrozen && mbAudioFrozenValid) {
			lDropFrames = 1;
		}

		if (lDropFrames) {
			long lCurrentDelta = blitter->getFrameDelta();
			if (opt->video.nPreviewFieldMode)
				lCurrentDelta >>= 1;
			if (lDropFrames > lCurrentDelta) {
				lDropFrames = lCurrentDelta;
				if (lDropFrames < 0)
					lDropFrames = 0;
//				VDDEBUG2("Dubber: Dropping frame skip to %d\n", lDropFrames);
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

				if (indep == 0x3FFFFFFF && vSrc->streamToDisplayOrder(vSrc->nearestKey(vSrc->displayToStreamOrder(display_num) + opt->video.frameRateDecimation*2)) > display_num)
					indep = 0;

				if (indep > 0 && indep < lDropFrames) {
//					VDDEBUG2("Dubber: Proactive drop (%d < %d)\n", indep, lDropFrames);
					bDrop = true;
				}
			}
		}

		if (bDrop) {
			if (!(exdata&2)) {
				blitter->nextFrame(opt->video.nPreviewFieldMode ? 2 : 1);
//				VDDEBUG2("Dubber: skipped frame\n");
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
		mpVideoOut->write((exdata & 1) ? 0 : AVIIF_KEYFRAME, (char *)buffer, lastSize, 1);

		vInfo.total_size += lastSize + 24;
		++vInfo.processed;
		pStatusHandler->NotifyNewFrame(lastSize | (exdata&1 ? 0x80000000 : 0));
		mInterleaver.AddVBRCorrection(0, lastSize);

		return kVideoWriteOK;
	}

	// Fast Repack: Decompress data and send to compressor (possibly non-RGB).
	// Slow Repack: Decompress data and send to compressor.
	// Full:		Decompress, process, filter, convert, send to compressor.

	mProcessingProfileChannel.Begin(0xe0e0e0, "V-Lock1");
	blitter->lock(BUFFERID_INPUT);
	mProcessingProfileChannel.End();

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

//		VDDEBUG2("Dubber: skipped frame\n");
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

		lInputFrameNum = display_num - vSrc->asStream()->getStart();

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


		mProcessingProfileChannel.Begin(0xe0e0e0, "V-Lock2");
		blitter->lock(BUFFERID_OUTPUT);
		mProcessingProfileChannel.End();

//		if (!outputDecompressor)
//			outputBitmap.data = outputBuffer;

		do {
//			IDDrawSurface *pds = mOutputDisplay.GetDD();
//
//			if (pds) {
//				if (!pds->LockInverted(&destbm))
//					break;
//
//				outputBitmap = &destbm;
//			}

			if (fPreview && g_prefs.fDisplay & Preferences::DISPF_DITHER16)
				outputBitmap->BitBltDither(0, 0, lastBitmap, 0, 0, -1, -1, fDisplay565);
			else if (mpDisplayFormat->biCompression == BI_BITFIELDS)
				outputBitmap->BitBlt565(0, 0, lastBitmap, 0, 0, -1, -1);
			else
				outputBitmap->BitBlt(0, 0, lastBitmap, 0, 0, -1, -1);

//			if (pds)
//				pds->Unlock();

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

			DWORD dwSize = mpCompressorVideoFormat->biSizeImage;

			mpCompressorVideoFormat->biSizeImage = dwBytes;

			VDCHECKPOINT;
			if (ICERR_OK != (err = ICDecompress(outputDecompressor,
				isKey ? 0 : ICDECOMPRESS_NOTKEYFRAME,
				(BITMAPINFOHEADER *)mpVideoOut->getFormat(),
				lpCompressedData,
				&*mpCompressorVideoFormat,
				outputBuffer
				)))

				fShowDecompressedFrame = false;
			VDCHECKPOINT;

			mpCompressorVideoFormat->biSizeImage = dwSize;

			CHECK_FPU_STACK
		}

		mpVideoOut->write(isKey ? AVIIF_KEYFRAME : 0, (char *)lpCompressedData, dwBytes, 1);

	} else {

		dwBytes = ((const BITMAPINFOHEADER *)mpVideoOut->getFormat())->biSizeImage;

		VDCHECKPOINT;
		mpVideoOut->write(AVIIF_KEYFRAME, (char *)frameBuffer, dwBytes, 1);
		VDCHECKPOINT;

		isKey = true;
	}

	vInfo.total_size += dwBytes + 24;
	mInterleaver.AddVBRCorrection(0, dwBytes + (dwBytes&1));

	VDCHECKPOINT;

	if (fPreview || mRefreshFlag.xchg(0)) {
		if (opt->video.fShowInputFrame) {
			blitter->postAPC(BUFFERID_INPUT, AsyncUpdateCallback, mpInputDisplay, &opt->video.nPreviewFieldMode);
		} else
			blitter->unlock(BUFFERID_INPUT);

		if (opt->video.mode == DubVideoOptions::M_FULL && opt->video.fShowOutputFrame && (!outputDecompressor || dwBytes)) {
			blitter->postAPC(BUFFERID_OUTPUT, AsyncUpdateCallback, mpOutputDisplay, &opt->video.nPreviewFieldMode);
		} else
			blitter->unlock(BUFFERID_OUTPUT);
	} else {
		blitter->unlock(BUFFERID_OUTPUT);
		blitter->unlock(BUFFERID_INPUT);
	}

	if (opt->perf.fDropFrames && fPreview) {
		long lFrameDelta;

		lFrameDelta = blitter->getFrameDelta();

		if (opt->video.nPreviewFieldMode)
			lFrameDelta >>= 1;

		if (lFrameDelta < 0) lFrameDelta = 0;
		
		if (lFrameDelta > 0) {
			lDropFrames = lFrameDelta;
//			VDDEBUG2("Dubber: Skipping %d frames\n", lDropFrames);
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

	mpAudioOut->write(AVIIF_KEYFRAME, (char *)buffer, lActualBytes, lActualSamples);

	aInfo.cur_proc_src += lActualBytes;
}

void Dubber::ThreadRun() {
	bool quit = false;
	bool firstPacket = true;
	bool bVideoEnded = !(vSrc && mpOutputSystem->AcceptsVideo());
	bool bVideoNonDelayedFrameReceived = false;
	bool bAudioEnded = !(aSrc && mpOutputSystem->AcceptsAudio());
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
							static_cast<AVIAudioPreviewOutputStream *>(mpAudioOut)->start();
							mbAudioFrozenValid = true;
						}
						mInterleaver.EndStream(0);
						bVideoEnded = true;
					} else {
						// We cannot wrap the entire loop with a profiling event because typically
						// involves a nice wait in preview mode.

						uint32 nFramesPushedTryingToFlushCodec = 0;

						for(;;) {
							void *buf;
							long len;
							VDPosition	rawFrame;
							VDPosition	displayFrame;
							VDPosition	timelineFrame;
							int exdata;
							int handle;
							int droptype;
							bool bAttemptingToFlushCodecBuffer = false;

							if (fAbort)
								goto abort_requested;

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
									bAttemptingToFlushCodecBuffer = true;
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

							if (fPreview && aSrc) {
								static_cast<AVIAudioPreviewOutputStream *>(mpAudioOut)->start();
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
							if (mAudioPipe.isInputClosed()) {
								mAudioPipe.CloseOutput();
								bytesread -= bytesread % nBlockAlign;
								count = bytesread / nBlockAlign;
								mInterleaver.AddCBRCorrection(1, count);
								mInterleaver.EndStream(1);
								bAudioEnded = true;
								break;
							}

							mAudioPipe.ReadWait();
						}

						bytesread += tc;
					}

					mProcessingProfileChannel.End();
					if (count > 0) {
						mProcessingProfileChannel.Begin(0xe0e0ff, "A-Write");
						WriteAudio(&audioBuffer.front(), bytesread, count);

						if (firstPacket && fPreview) {
							mpAudioOut->flush();
							blitter->enablePulsing(true);
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

			if (fAbort)
				break;

			if (bVideoEnded && bAudioEnded)
				break;
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

	mpVideoPipe->finalizeAck();
	mAudioPipe.CloseOutput();

	// attempt a graceful shutdown at this point...
	try {
		// if preview mode, choke the audio

		if (mpAudioOut && mpOutputSystem->IsRealTime())
			static_cast<AVIAudioPreviewOutputStream *>(mpAudioOut)->stop();

		// finalize the output.. if it's not a preview...
		if (!mpOutputSystem->IsRealTime()) {
			// update audio rate...

			if (audioCorrector) {
				VDFormatStruct<WAVEFORMATEX> wfex((const WAVEFORMATEX *)mpAudioOut->getFormat(), mpAudioOut->getFormatLen());
				
				wfex->nAvgBytesPerSec = audioCorrector->ComputeByterate(wfex->nSamplesPerSec);

				mpAudioOut->setFormat(&*wfex, wfex.size());

				AVIStreamHeader_fixed hdr(mpAudioOut->getStreamInfo());
				hdr.dwRate = wfex->nAvgBytesPerSec * hdr.dwScale;
				mpAudioOut->setStreamInfo(hdr);
			}

			// finalize avi
			mpOutputSystem->CloseSegment(AVIout, true);
			AVIout = NULL;
			mpAudioOut = NULL;
			mpVideoOut = NULL;
			VDDEBUG("Dub/Processor: finalized.\n");
		}
	} catch(MyError& e) {
		if (!fError) {
			err.TransferFrom(e);
			fError = true;
		}
	}

	VDDEBUG("Dub/Processor: end\n");
}

///////////////////////////////////////////////////////////////////

void Dubber::Abort() {
	fUserAbort = true;
	fAbort = true;
	mAudioPipe.Abort();
	mpVideoPipe->abort();
	PostMessage(g_hWnd, WM_USER, 0, 0);
}

bool Dubber::isRunning() {
	return !fAbort;
}

bool Dubber::isAbortedByUser() {
	return fUserAbort;
}

bool Dubber::IsPreviewing() {
	return fPreview;
}

void Dubber::RealizePalette() {
//	if (HDRAWDIB hddOutput = mOutputDisplay.GetHDD())
//		DrawDibRealize(hddOutput, hDCWindow, FALSE);
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
			if (mpIOThread->isThreadActive()) {
				void *eip = ThreadLocation();
				VDLogAppMessage(kVDLogWarning, kVDST_Dub, kVDM_IOThreadLivelock, 1, &eip);
			}
			mLastIOThreadCounter = 0;
		}

		if (mLastProcessingThreadCounter != prcount) {
			mLastProcessingThreadCounter = prcount;
			mProcessingThreadFailCount = curTime;
		} else if (mLastProcessingThreadCounter && (curTime - mProcessingThreadFailCount - 10000) < 3600000) {		// 10s to 1hr
			if (isThreadActive()) {
				void *eip = ThreadLocation();
				VDLogAppMessage(kVDLogWarning, kVDST_Dub, kVDM_ProcessingThreadLivelock, 1, &eip);

				HANDLE h = (HANDLE)getThreadHandle();

				CONTEXT ctx;
				ctx.ContextFlags = CONTEXT_CONTROL;
				SuspendThread(h);
				ctx.Eip = 0;
				ResumeThread(h);
			}
			mLastProcessingThreadCounter = 0;
		}
	}
}
