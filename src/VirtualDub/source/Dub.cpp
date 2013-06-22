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
#include <vd2/system/w32assist.h>
#include <vd2/Dita/resources.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/bitmap.h>
#include <vd2/Riza/display.h>
#include <vd2/Riza/videocodec.h>
#include "AudioFilterSystem.h"
#include "convert.h"
#include "filters.h"
#include "gui.h"
#include "prefs.h"
#include "command.h"
#include "misc.h"

#include <vd2/system/error.h>
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

#include "Dub.h"
#include "DubOutput.h"
#include "DubStatus.h"
#include "DubUtils.h"
#include "DubIO.h"
#include "DubProcess.h"

using namespace nsVDDub;

/// HACK!!!!
#define vSrc mVideoSources[0]
#define aSrc mAudioSources[0]


///////////////////////////////////////////////////////////////////////////

extern const char g_szError[];
extern HWND g_hWnd;
extern bool g_fWine;
extern uint32& VDPreferencesGetRenderVideoBufferCount();
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
		kVDM_FastRecompressUsingFormat,
		kVDM_SlowRecompressUsingFormat,
		kVDM_FullUsingInputFormat,
		kVDM_FullUsingOutputFormat
	};

	enum {
		kLiveLockMessageLimit = 5
	};
};

///////////////////////////////////////////////////////////////////////////

DubOptions g_dubOpts = {
	{
		-1.0f,			// no amp
		500,			// preload by 500ms
		1,				// every frame
		0,				// no new rate
		0,				// offset: 0ms
		false,			// period is in frames
		true,			// audio interleaving enabled
		true,			// yes, offset audio with video
		true,			// yes, clip audio to video length
		false,			// no high quality
		false,			// use fixed-function audio pipeline
		DubAudioOptions::P_NOCHANGE,		// no precision change
		DubAudioOptions::C_NOCHANGE,		// no channel change
		DubAudioOptions::M_NONE,
	},

	{
		0,								// input: autodetect
		nsVDPixmap::kPixFormat_RGB888,	// output: 24bit
		DubVideoOptions::M_FULL,	// mode: full
		false,						// use smart encoding
		false,						// preserve empty frames
		true,						// show input video
		true,						// show output video
		false,						// decompress output video before display
		true,						// sync to audio
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
		true,					// dynamic enable
		false,
		false,					// directdraw,
		true,					// drop frames
	},

	true,			// show status
	false,			// move slider
	100,			// run at 100%
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
}

///////////////////////////////////////////////////////////////////////////

namespace {
	bool CheckFormatSizeCompatibility(int format, int w, int h) {
		const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(format);

		if ((w & ((1<<formatInfo.qwbits)-1))
			|| (h & ((1<<formatInfo.qhbits)-1))
			|| (w & ((1<<formatInfo.auxwbits)-1))
			|| (h & ((1<<formatInfo.auxhbits)-1))
			)
		{
			return false;
		}

		return true;
	}

	int DegradeFormat(int format, int originalFormat) {
		using namespace nsVDPixmap;

		switch(format) {
		case kPixFormat_YUV410_Planar:	format = kPixFormat_YUV420_Planar; break;
		case kPixFormat_YUV411_Planar:	format = kPixFormat_YUV422_YUYV; break;
		case kPixFormat_YUV420_Planar:	format = kPixFormat_YUV422_YUYV; break;
		case kPixFormat_YUV422_Planar:	format = kPixFormat_YUV422_YUYV; break;
		case kPixFormat_YUV444_Planar:	format = kPixFormat_YUV422_YUYV; break;
		case kPixFormat_YUV422_YUYV:	format = kPixFormat_RGB888; break;
		case kPixFormat_YUV422_UYVY:	format = kPixFormat_RGB888; break;
		case kPixFormat_Y8:				format = kPixFormat_RGB888; break;

		// RGB formats are a bit tricky, as we must always be sure to try the
		// three major formats: 8888, 1555, 888.  The possible chains:
		//
		// 8888 -> 888 -> 1555 -> Pal8
		// 888 -> 8888 -> 1555 -> Pal8
		// 565 -> 8888 -> 888 -> 1555 -> Pal8
		// 1555 -> 8888 -> 888 -> Pal8

		case kPixFormat_RGB888:			format = (originalFormat == kPixFormat_RGB565  ) ? kPixFormat_XRGB1555 : (originalFormat == kPixFormat_XRGB1555) ? kPixFormat_Pal8 : kPixFormat_XRGB8888; break;
		case kPixFormat_XRGB8888:		format = (originalFormat == kPixFormat_RGB888  ) ? kPixFormat_XRGB1555 : kPixFormat_RGB888; break;
		case kPixFormat_RGB565:			format = kPixFormat_XRGB8888; break;
		case kPixFormat_XRGB1555:		format = (originalFormat == kPixFormat_XRGB1555) ? kPixFormat_XRGB8888 : kPixFormat_Pal8; break;
		default:						format = kPixFormat_Null; break;
		};

		if (format == originalFormat)
			format = kPixFormat_Null;

		return format;
	}
}


///////////////////////////////////////////////////////////////////////////
//
//	Dubber
//
///////////////////////////////////////////////////////////////////////////

class Dubber : public IDubber {
private:
	MyError				err;
	bool				fError;

	VDAtomicInt			mStopLock;

	DubOptions			*opt;

	typedef vdfastvector<AudioSource *> AudioSources;
	AudioSources		mAudioSources;

	typedef vdfastvector<IVDVideoSource *> VideoSources;
	VideoSources		mVideoSources;

	IVDDubberOutputSystem	*mpOutputSystem;
	COMPVARS			*compVars;

	DubAudioStreamInfo	aInfo;
	DubVideoStreamInfo	vInfo;

	bool				mbDoVideo;
	bool				mbDoAudio;
	bool				fPreview;
	volatile bool		fAbort;
	volatile bool		fUserAbort;
	bool				fADecompressionOk;
	bool				fVDecompressionOk;

	int					mLiveLockMessages;

	VDDubIOThread		*mpIOThread;
	VDDubProcessThread	mProcessThread;
	VDAtomicInt			mIOThreadCounter;

	vdautoptr<IVDVideoCompressor>	mpVideoCompressor;

	AVIPipe *			mpVideoPipe;
	VDAudioPipeline		mAudioPipe;

	IVDVideoDisplay *	mpInputDisplay;
	IVDVideoDisplay *	mpOutputDisplay;
	bool				mbInputDisplayInitialized;

	vdstructex<BITMAPINFOHEADER>	mpCompressorVideoFormat;

	std::vector<AudioStream *>	mAudioStreams;
	AudioStream			*audioStream;
	AudioStream			*audioStatusStream;
	AudioStreamL3Corrector	*audioCorrector;
	vdautoptr<VDAudioFilterGraph> mpAudioFilterGraph;

	const FrameSubset		*inputSubsetActive;
	FrameSubset				*inputSubsetAlloc;
	VideoTelecineRemover	*pInvTelecine;

	vdstructex<WAVEFORMATEX> mAudioCompressionFormat;
	VDStringA			mAudioCompressionFormatHint;

	vdblock<char>		mVideoFilterOutput;
	VDPixmap			mVideoFilterOutputPixmap;

	FilterStateInfo		fsi;

	bool				fPhantom;

	IDubStatusHandler	*pStatusHandler;

	long				lVideoSizeEstimate;

	// interleaving
	VDStreamInterleaver		mInterleaver;
	VDRenderFrameMap		mVideoFrameMap;
	VDRenderFrameIterator	mVideoFrameIterator;

	///////

	int					mLastProcessingThreadCounter;
	int					mProcessingThreadFailCount;
	int					mLastIOThreadCounter;
	int					mIOThreadFailCount;

	///////

public:
	Dubber(DubOptions *);
	~Dubber();

	void SetAudioCompression(const WAVEFORMATEX *wf, uint32 cb, const char *pShortNameHint);
	void SetPhantomVideoMode();
	void SetInputDisplay(IVDVideoDisplay *);
	void SetOutputDisplay(IVDVideoDisplay *);
	void SetAudioFilterGraph(const VDAudioFilterGraph& graph);

	void InitAudioConversionChain();
	void InitOutputFile();
	bool AttemptInputOverlays();

	void InitDirectDraw();
	bool NegotiateFastFormat(const BITMAPINFOHEADER& bih);
	bool NegotiateFastFormat(int format);
	void InitSelectInputFormat();
	void Init(IVDVideoSource *const *pVideoSources, uint32 nVideoSources, AudioSource *const *pAudioSources, uint32 nAudioSources, IVDDubberOutputSystem *outsys, COMPVARS *videoCompVars, const FrameSubset *);
	void Go(int iPriority = 0);
	void Stop();

	void RealizePalette();
	void Abort();
	void ForceAbort();
	bool isRunning();
	bool isAbortedByUser();
	bool IsPreviewing();

	void SetStatusHandler(IDubStatusHandler *pdsh);
	void SetPriority(int index);
	void UpdateFrames();
	void SetThrottleFactor(float throttleFactor);
};


///////////////////////////////////////////////////////////////////////////

IDubber::~IDubber() {
}

IDubber *CreateDubber(DubOptions *xopt) {
	return new Dubber(xopt);
}

Dubber::Dubber(DubOptions *xopt)
	: mpIOThread(0)
	, mIOThreadCounter(0)
	, mpAudioFilterGraph(NULL)
	, mStopLock(0)
	, mpVideoPipe(NULL)
	, mVideoFrameIterator(mVideoFrameMap)
	, mLastProcessingThreadCounter(0)
	, mProcessingThreadFailCount(0)
	, mLastIOThreadCounter(0)
	, mIOThreadFailCount(0)
{
	opt				= xopt;

	// clear the workin' variables...

	fError				= false;

	fAbort				= false;
	fUserAbort			= false;

	pStatusHandler		= NULL;

	fADecompressionOk	= false;
	fVDecompressionOk	= false;

	mpInputDisplay		= NULL;
	mpOutputDisplay		= NULL;
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

	mLiveLockMessages = 0;
}

Dubber::~Dubber() {
	Stop();
}

/////////////////////////////////////////////////

void Dubber::SetAudioCompression(const WAVEFORMATEX *wf, uint32 cb, const char *pShortNameHint) {
	mAudioCompressionFormat.assign(wf, cb);
	if (pShortNameHint)
		mAudioCompressionFormatHint = pShortNameHint;
	else
		mAudioCompressionFormatHint.clear();
}

void Dubber::SetPhantomVideoMode() {
	fPhantom = true;
	vInfo.fAudioOnly = true;
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

/////////////

void Dubber::SetAudioFilterGraph(const VDAudioFilterGraph& graph) {
	mpAudioFilterGraph = new VDAudioFilterGraph(graph);
}

void InitStreamValuesStatic(DubVideoStreamInfo& vInfo, DubAudioStreamInfo& aInfo, IVDVideoSource *video, AudioSource *audio, DubOptions *opt, const FrameSubset *pfs) {
	IVDStreamSource *pVideoStream = NULL;
	
	if (video) {
		pVideoStream = video->asStream();

		vInfo.start_src		= 0;
		vInfo.end_src		= pfs->getTotalFrames();
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
	} else {
		aInfo.start_src		= 0;
	}

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

		if (vInfo.end_src <= vInfo.start_src)
			vInfo.end_dst = 0;
		else
			vInfo.end_dst		= (long)(vInfo.frameRate / vInfo.frameRateIn).scale64t(vInfo.end_src - vInfo.start_src);
		vInfo.end_proc_dst	= vInfo.end_dst;
	}

	if (audio) {
		// offset the start of the audio appropriately...
		aInfo.start_us = -(sint64)1000*opt->audio.offset;
		aInfo.start_src += audio->TimeToPositionVBR(aInfo.start_us);

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
			}
		}
	}

	vInfo.cur_proc_src = -1;
}

//////////////////////////////////////////////////////////////////////////////

// may be called at any time in Init() after streams setup

void Dubber::InitAudioConversionChain() {

	// ready the audio stream for streaming operation

	aSrc->streamBegin(fPreview, false);
	fADecompressionOk = true;

	// Initialize audio conversion chain

	bool bUseAudioFilterGraph = (opt->audio.mode > DubAudioOptions::M_NONE && mpAudioFilterGraph);

	uint32 audioSourceCount = mAudioSources.size();
	vdfastvector<AudioStream *> sourceStreams(audioSourceCount);

	for(uint32 i=0; i<audioSourceCount; ++i) {
		AudioSource *asrc = mAudioSources[i];

		if (bUseAudioFilterGraph) {
			audioStream = new_nothrow AudioFilterSystemStream(*mpAudioFilterGraph, aInfo.start_us);
			if (!audioStream)
				throw MyMemoryError();

			mAudioStreams.push_back(audioStream);
		} else {
			// First, create a source.

			if (!(audioStream = new_nothrow AudioStreamSource(asrc, aInfo.start_src, asrc->getEnd() - aInfo.start_src, opt->audio.mode > DubAudioOptions::M_NONE, aInfo.start_us)))
				throw MyMemoryError();

			mAudioStreams.push_back(audioStream);
		}

		// check the stream format and coerce to first stream if necessary
		if (i > 0) {
			const WAVEFORMATEX *format1 = sourceStreams[0]->GetFormat();
			const WAVEFORMATEX *format2 = audioStream->GetFormat();

			if (format1->nChannels != format2->nChannels || format1->wBitsPerSample != format2->wBitsPerSample) {
				audioStream = new_nothrow AudioStreamConverter(audioStream, format1->wBitsPerSample > 8, format1->nChannels > 1, false);
				mAudioStreams.push_back(audioStream);
			}

			if (format1->nSamplesPerSec != format2->nSamplesPerSec) {
				audioStream = new_nothrow AudioStreamResampler(audioStream, format1->nSamplesPerSec, true);
				mAudioStreams.push_back(audioStream);
			}
		}

		sourceStreams[i] = audioStream;
	}

	// Tack on a subset filter as well...
	sint64 offset = 0;
	
	if (opt->audio.fStartAudio)
		offset = vInfo.frameRateIn.scale64ir((sint64)1000000 * vInfo.start_src);

	bool applyTail = false;

	if (!opt->audio.fEndAudio && (inputSubsetActive->empty() || inputSubsetActive->back().end() >= vSrc->asStream()->getEnd()))
		applyTail = true;

	if (!(audioStream = new_nothrow AudioSubset(sourceStreams, inputSubsetActive, vInfo.frameRateIn, offset, applyTail)))
		throw MyMemoryError();

	mAudioStreams.push_back(audioStream);

	if (!bUseAudioFilterGraph) {
		// Attach a converter if we need to...

		if (aInfo.converting) {
			bool is_16bit = aInfo.is_16bit;

			// fix precision guess based on actual stream output if we are not changing it
			if (opt->audio.newPrecision == DubAudioOptions::P_NOCHANGE)
				is_16bit = audioStream->GetFormat()->wBitsPerSample > 8;

			if (aInfo.single_channel)
				audioStream = new_nothrow AudioStreamConverter(audioStream, is_16bit, aInfo.is_right, true);
			else
				audioStream = new_nothrow AudioStreamConverter(audioStream, is_16bit, aInfo.is_stereo, false);

			if (!audioStream)
				throw MyMemoryError();

			mAudioStreams.push_back(audioStream);
		}

		// Attach a converter if we need to...

		if (aInfo.resampling) {
			if (!(audioStream = new_nothrow AudioStreamResampler(audioStream, opt->audio.new_rate ? opt->audio.new_rate : aSrc->getWaveFormat()->nSamplesPerSec, opt->audio.fHighQuality)))
				throw MyMemoryError();

			mAudioStreams.push_back(audioStream);
		}

		// Attach an amplifier if needed...

		if (opt->audio.mode > DubAudioOptions::M_NONE && opt->audio.mVolume >= 0) {
			if (!(audioStream = new_nothrow AudioStreamAmplifier(audioStream, opt->audio.mVolume)))
				throw MyMemoryError();

			mAudioStreams.push_back(audioStream);
		}
	}

	// Make sure we only get what we want...

	if (!mVideoSources.empty() && opt->audio.fEndAudio) {
		const WAVEFORMATEX *pAudioFormat = audioStream->GetFormat();
		const sint64 nFrames = (sint64)(vInfo.end_src - vInfo.start_src);
		const VDFraction audioRate(pAudioFormat->nAvgBytesPerSec, pAudioFormat->nBlockAlign);
		const VDFraction audioPerVideo(audioRate / vInfo.frameRateIn);

		audioStream->SetLimit(audioPerVideo.scale64r(nFrames));
	}

	audioStatusStream = audioStream;

	// Tack on a compressor if we want...

	AudioCompressor *pCompressor = NULL;

	if (opt->audio.mode > DubAudioOptions::M_NONE && !mAudioCompressionFormat.empty()) {
		if (!(pCompressor = new_nothrow AudioCompressor(audioStream, &*mAudioCompressionFormat, mAudioCompressionFormat.size(), mAudioCompressionFormatHint.c_str())))
			throw MyMemoryError();

		audioStream = pCompressor;
		mAudioStreams.push_back(audioStream);
	}

	// Check the output format, and if we're compressing to
	// MPEG Layer III, compensate for the lag and create a bitrate corrector

	if (!g_prefs.fNoCorrectLayer3 && pCompressor && pCompressor->GetFormat()->wFormatTag == WAVE_FORMAT_MPEGLAYER3) {
		pCompressor->CompensateForMP3();

		if (!(audioCorrector = new_nothrow AudioStreamL3Corrector(audioStream)))
			throw MyMemoryError();

		audioStream = audioCorrector;
		mAudioStreams.push_back(audioStream);
	}

}

void Dubber::InitOutputFile() {

	// Do audio.

	if (mbDoAudio) {
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

	if (mbDoVideo) {
		VBitmap outputBitmap;
		
		if (opt->video.mode >= DubVideoOptions::M_FULL)
			outputBitmap = *filters.LastBitmap();
		else
			outputBitmap.init((void *)vSrc->getFrameBuffer(), vSrc->getDecompressedFormat());

		outputBitmap.AlignTo4();		// This is a lie, but it keeps the MakeBitmapHeader() call below from fouling

		AVIStreamHeader_fixed hdr;

		AVISTREAMINFOtoAVIStreamHeader(&hdr, &vSrc->asStream()->getStreamInfo());

		hdr.dwSampleSize = 0;

		if (opt->video.mode > DubVideoOptions::M_NONE && !opt->video.mbUseSmartRendering) {
			if (mpVideoCompressor) {
				hdr.fccHandler	= compVars->fccHandler;
				hdr.dwQuality	= compVars->lQ;
			} else {
				hdr.fccHandler	= mmioFOURCC('D','I','B',' ');
			}
		}

		hdr.dwRate			= vInfo.frameRate.getHi();
		hdr.dwScale			= vInfo.frameRate.getLo();
		hdr.dwLength		= vInfo.end_dst >= 0xFFFFFFFFUL ? 0xFFFFFFFFUL : (DWORD)vInfo.end_dst;

		hdr.rcFrame.left	= 0;
		hdr.rcFrame.top		= 0;
		hdr.rcFrame.right	= (short)outputBitmap.w;
		hdr.rcFrame.bottom	= (short)outputBitmap.h;

		// initialize compression

		int outputFormatID = opt->video.mOutputFormat;
		int outputVariantID = 0;

		if (!outputFormatID)
			outputFormatID = vSrc->getTargetFormat().format;

		if (opt->video.mode >= DubVideoOptions::M_FASTREPACK) {
			if (opt->video.mode <= DubVideoOptions::M_SLOWREPACK) {
				const BITMAPINFOHEADER *pFormat = vSrc->getDecompressedFormat();

				mpCompressorVideoFormat.assign(pFormat, VDGetSizeOfBitmapHeaderW32(pFormat));
			} else {
				vdstructex<BITMAPINFOHEADER> bih;
				bih.resize(sizeof(BITMAPINFOHEADER));
				outputBitmap.MakeBitmapHeader(&*bih);

				// try to find a variant that works
				const int variants = VDGetPixmapToBitmapVariants(outputFormatID);
				int variant;

				for(variant=1; variant <= variants; ++variant) {
					VDMakeBitmapFormatFromPixmapFormat(mpCompressorVideoFormat, bih, outputFormatID, variant);

					bool result = true;
					
					if (mpVideoCompressor)
						result = mpVideoCompressor->Query((LPBITMAPINFO)&*mpCompressorVideoFormat, NULL);

					if (result) {
						outputVariantID = variant;
						break;
					}
				}

				if (variant > variants)
					throw MyError("Unable to initialize the output video codec. Check that the video codec is compatible with the output video frame size and that the settings are correct, or try a different one.");
			}
		} else {
			const BITMAPINFOHEADER *pFormat = vSrc->getImageFormat();

			mpCompressorVideoFormat.assign(pFormat, vSrc->asStream()->getFormatLen());
		}

		// Initialize output compressor.
		vdstructex<BITMAPINFOHEADER>	outputFormat;

		if (mpVideoCompressor) {
			mpVideoCompressor->GetOutputFormat(&*mpCompressorVideoFormat, outputFormat);

			// If we are using smart rendering, we have no choice but to match the source format.
			if (opt->video.mbUseSmartRendering) {
				IVDStreamSource *vsrcStream = vSrc->asStream();
				const BITMAPINFOHEADER *srcFormat = vSrc->getImageFormat();

				if (!mpVideoCompressor->Query(&*mpCompressorVideoFormat, srcFormat))
					throw MyError("Cannot initialize smart rendering: The selected video codec is able to compress the source video, but cannot match the same compressed format.");

				outputFormat.assign(srcFormat, vsrcStream->getFormatLen());
			}

			mpVideoCompressor->Start(&*mpCompressorVideoFormat, &*outputFormat, vInfo.frameRate, vInfo.end_dst);

			lVideoSizeEstimate = mpVideoCompressor->GetMaxOutputSize();
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
					uint32 bytes = 0;

					if (!pVideoStream->read(frame, 1, 0, 0, &bytes, 0))
						if (lVideoSizeEstimate < bytes)
							lVideoSizeEstimate = bytes;
				}
			} else {
				if (opt->video.mbUseSmartRendering) {
					throw MyError("Cannot initialize smart rendering: No video codec is selected for compression.");
				}

				if (opt->video.mode == DubVideoOptions::M_FULL) {
					VDMakeBitmapFormatFromPixmapFormat(outputFormat, mpCompressorVideoFormat, outputFormatID, outputVariantID);
				} else
					outputFormat = mpCompressorVideoFormat;

				lVideoSizeEstimate = outputFormat->biSizeImage;
				lVideoSizeEstimate = (lVideoSizeEstimate+1) & -2;
			}
		}

		mpOutputSystem->SetVideo(hdr, &*outputFormat, outputFormat.size());

		if(opt->video.mode >= DubVideoOptions::M_FULL) {
			const VBitmap& bmout = *filters.LastBitmap();

			VDPixmapLayout layout;
			uint32 reqsize = VDMakeBitmapCompatiblePixmapLayout(layout, bmout.w, bmout.h, outputFormatID, outputVariantID);

			mVideoFilterOutput.resize(reqsize);
			mVideoFilterOutputPixmap = VDPixmapFromLayout(layout, mVideoFilterOutput.data());

			const char *s = VDPixmapGetInfo(mVideoFilterOutputPixmap.format).name;

			VDLogAppMessage(kVDLogInfo, kVDST_Dub, kVDM_FullUsingOutputFormat, 1, &s);
		}
	}
}

bool Dubber::AttemptInputOverlays() {
	static const int kFormats[]={
		nsVDPixmap::kPixFormat_YUV420_Planar,
		nsVDPixmap::kPixFormat_YUV422_UYVY,
		nsVDPixmap::kPixFormat_YUV422_YUYV
	};

	for(int i=0; i<sizeof(kFormats)/sizeof(kFormats[0]); ++i) {
		const int format = kFormats[i];

		VideoSources::const_iterator it(mVideoSources.begin()), itEnd(mVideoSources.end());
		for(; it!=itEnd; ++it) {
			IVDVideoSource *vs = *it;

			if (!vs->setTargetFormat(format))
				break;
		}

		if (it == itEnd) {
			if (mpInputDisplay->SetSource(false, mVideoSources.front()->getTargetFormat(), 0, 0, false, opt->video.nPreviewFieldMode>0))
				return true;
		}
	}

	return false;
}

void Dubber::InitDirectDraw() {

	if (!opt->perf.useDirectDraw)
		return;

	// Should we try and establish a DirectDraw overlay?

	if (opt->video.mode == DubVideoOptions::M_SLOWREPACK) {
		if (AttemptInputOverlays())
			mbInputDisplayInitialized = true;
	}
}

bool Dubber::NegotiateFastFormat(const BITMAPINFOHEADER& bih) {
	VideoSources::const_iterator it(mVideoSources.begin()), itEnd(mVideoSources.end());
	for(; it!=itEnd; ++it) {
		IVDVideoSource *vs = *it;

		if (!vs->setDecompressedFormat(&bih))
			return false;
	}
	
	const BITMAPINFOHEADER *pbih = mVideoSources.front()->getDecompressedFormat();

	if (mpVideoCompressor->Query(pbih)) {
		char buf[16]={0};
		const char *s = buf;

		if (pbih->biCompression >= 0x20000000)
			*(uint32 *)buf = pbih->biCompression;
		else
			sprintf(buf, "RGB%d", pbih->biBitCount);

		VDLogAppMessage(kVDLogInfo, kVDST_Dub, kVDM_FastRecompressUsingFormat, 1, &s);
		return true;
	}

	return false;
}

bool Dubber::NegotiateFastFormat(int format) {
	VideoSources::const_iterator it(mVideoSources.begin()), itEnd(mVideoSources.end());
	for(; it!=itEnd; ++it) {
		IVDVideoSource *vs = *it;

		if (!vs->setTargetFormat(format))
			return false;
	}
	
	const BITMAPINFOHEADER *pbih = mVideoSources.front()->getDecompressedFormat();

	if (mpVideoCompressor->Query(pbih)) {
		char buf[16]={0};
		const char *s = buf;

		if (pbih->biCompression >= 0x20000000)
			*(uint32 *)buf = pbih->biCompression;
		else
			sprintf(buf, "RGB%d", pbih->biBitCount);

		VDLogAppMessage(kVDLogInfo, kVDST_Dub, kVDM_FastRecompressUsingFormat, 1, &s);
		return true;
	}

	return false;
}

void Dubber::InitSelectInputFormat() {
	//	DIRECT:			Don't care.
	//	FASTREPACK:		Negotiate with output compressor.
	//	SLOWREPACK:		[Dub]		Use selected format.
	//					[Preview]	Negotiate with display driver.
	//	FULL:			Use selected format.

	if (opt->video.mode == DubVideoOptions::M_NONE)
		return;

	const BITMAPINFOHEADER& bih = *vSrc->getImageFormat();

	if (opt->video.mode == DubVideoOptions::M_FASTREPACK && mpVideoCompressor) {
		// Attempt source format.
		if (NegotiateFastFormat(bih)) {
			mpInputDisplay->Reset();
			mbInputDisplayInitialized = true;
			return;
		}

		// Don't use odd-width YUV formats.  They may technically be allowed, but
		// a lot of codecs crash.  For instance, Huffyuv in "Convert to YUY2"
		// mode will accept a 639x360 format for compression, but crashes trying
		// to decompress it.

		if (!(bih.biWidth & 1)) {
			if (NegotiateFastFormat(nsVDPixmap::kPixFormat_YUV422_UYVY)) {
				mpInputDisplay->SetSource(false, vSrc->getTargetFormat());
				mbInputDisplayInitialized = true;
				return;
			}

			// Attempt YUY2.
			if (NegotiateFastFormat(nsVDPixmap::kPixFormat_YUV422_YUYV)) {
				mpInputDisplay->SetSource(false, vSrc->getTargetFormat());
				mbInputDisplayInitialized = true;
				return;
			}

			if (!(bih.biHeight & 1)) {
				if (NegotiateFastFormat(nsVDPixmap::kPixFormat_YUV420_Planar)) {
					mpInputDisplay->SetSource(false, vSrc->getTargetFormat());
					mbInputDisplayInitialized = true;
					return;
				}
			}
		}

		// Attempt RGB format negotiation.
		int format = opt->video.mInputFormat;

		do {
			if (NegotiateFastFormat(format))
				return;

			format = DegradeFormat(format, opt->video.mInputFormat);
		} while(format);

		throw MyError("Video format negotiation failed: use slow-repack or full mode.");
	}

	// Negotiate RGB format.

	int format = opt->video.mInputFormat;

	do {
		if (vSrc->setTargetFormat(format)) {
			const char *s = VDPixmapGetInfo(vSrc->getTargetFormat().format).name;

			VDLogAppMessage(kVDLogInfo, kVDST_Dub, (opt->video.mode == DubVideoOptions::M_FULL) ? kVDM_FullUsingInputFormat : kVDM_SlowRecompressUsingFormat, 1, &s);
			return;
		}

		format = DegradeFormat(format, opt->video.mInputFormat);
	} while(format);

	throw MyError("The decompression codec cannot decompress to an RGB format. This is very unusual. Check that any \"Force YUY2\" options are not enabled in the codec's properties.");
}

void Dubber::Init(IVDVideoSource *const *pVideoSources, uint32 nVideoSources, AudioSource *const *pAudioSources, uint32 nAudioSources, IVDDubberOutputSystem *pOutputSystem, COMPVARS *videoCompVars, const FrameSubset *pfs) {
	mAudioSources.assign(pAudioSources, pAudioSources + nAudioSources);
	mVideoSources.assign(pVideoSources, pVideoSources + nVideoSources);

	mpOutputSystem		= pOutputSystem;
	mbDoVideo			= !mVideoSources.empty() && mpOutputSystem->AcceptsVideo();
	mbDoAudio			= !mAudioSources.empty() && mpOutputSystem->AcceptsAudio();

	fPreview			= mpOutputSystem->IsRealTime();

	inputSubsetActive	= pfs;
	compVars			= videoCompVars;

	if (!fPreview && pOutputSystem->AcceptsVideo() && opt->video.mode>DubVideoOptions::M_NONE && compVars && (compVars->dwFlags & ICMF_COMPVARS_VALID) && compVars->hic)
		mpVideoCompressor = VDCreateVideoCompressorVCM(compVars->hic, compVars->lDataRate*1024, compVars->lQ, compVars->lKey);

	// check the mode; if we're using DirectStreamCopy mode, we'll need to
	// align the subset to keyframe boundaries!
	if (!mVideoSources.empty() && opt->video.mode == DubVideoOptions::M_NONE) {
		if (!(inputSubsetActive = inputSubsetAlloc = new FrameSubset()))
			throw MyMemoryError();

		IVDStreamSource *pVideoStream = vSrc->asStream();

		const VDPosition videoFrameStart	= pVideoStream->getStart();

		for(FrameSubset::const_iterator it(pfs->begin()), itEnd(pfs->end()); it!=itEnd; ++it) {
			sint64 start = vSrc->nearestKey(it->start + videoFrameStart) - videoFrameStart;

			FrameSubset::iterator itNew(inputSubsetAlloc->addRange(it->start, it->len, it->bMask, it->source));

			// Mask ranges never need to be extended backwards, because they don't hold any
			// data of their own.  If an include range needs to be extended backwards, though,
			// it may need to extend into a previous merge range.  To avoid this problem,
			// we do a delete of the range before adding the tail.

			if (!itNew->bMask) {
				if (start < itNew->start) {
					FrameSubset::iterator it2(itNew);

					while(it2 != inputSubsetAlloc->begin()) {
						--it2;

						sint64 prevtail = it2->start + it2->len;

						if (prevtail < start || prevtail > itNew->start + itNew->len)
							break;

						if (it2->start >= start || !it2->bMask) {	// within extension range: absorb
							itNew->len += itNew->start - it2->start;
							itNew->start = it2->start;
							it2 = inputSubsetAlloc->erase(it2);
						} else {									// before extension range and masked: split merge
							sint64 offset = start - itNew->start;
							it2->len -= offset;
							itNew->start -= offset;
							itNew->len += offset;
							break;
						}
					}

					sint64 left = itNew->start - start;
					
					if (left > 0) {
						itNew->start = start;
						itNew->len += left;
					}
				}
			}
		}
	}

	// initialize stream values

	InitStreamValuesStatic(vInfo, aInfo, vSrc, mAudioSources.empty() ? NULL : mAudioSources.front(), opt, inputSubsetActive);

	vInfo.frameRateNoTelecine = vInfo.frameRate;
	vInfo.usPerFrameNoTelecine = vInfo.usPerFrame;
	if (opt->video.mode >= DubVideoOptions::M_FULL && opt->video.fInvTelecine) {
		vInfo.frameRate = vInfo.frameRate * VDFraction(4, 5);
		vInfo.usPerFrame = (long)vInfo.frameRate.scale64ir(1000000);

		vInfo.end_proc_dst	= (long)(vInfo.frameRate / vInfo.frameRateIn).scale64t(vInfo.end_src - vInfo.start_src);
		vInfo.end_dst += 4;
		vInfo.end_dst -= vInfo.end_dst % 5;
	}

	// initialize directdraw display if in preview

	mbInputDisplayInitialized = false;
	if (fPreview)
		InitDirectDraw();

	// Select an appropriate input format.  This is really tricky...

	vInfo.fAudioOnly = true;
	if (mbDoVideo) {
		if (!mbInputDisplayInitialized)
			InitSelectInputFormat();
		vInfo.fAudioOnly = false;
	}

	// Initialize filter system.

	int nVideoLagOutput = 0;		// Frames that will be buffered in the output frame space (video filters)
	int nVideoLagTimeline = 0;		// Frames that will be buffered in the timeline frame space (IVTC)

	if (mbDoVideo && opt->video.mode >= DubVideoOptions::M_FULL) {
		const VDPixmap& px = vSrc->getTargetFormat();

		filters.initLinearChain(&g_listFA, (Pixel *)px.palette, px.w, px.h, 0);
		
		VBitmap *lastBitmap = filters.LastBitmap();

		int outputFormat = opt->video.mOutputFormat;

		if (!outputFormat)
			outputFormat = vSrc->getTargetFormat().format;

		if (!CheckFormatSizeCompatibility(outputFormat, lastBitmap->w, lastBitmap->h)) {
			const VDPixmapFormatInfo& formatInfo = VDPixmapGetInfo(outputFormat);

			throw MyError("The output frame size is not compatible with the selected output format. (%dx%d, %s)", lastBitmap->w, lastBitmap->h, formatInfo.name);
		}

		fsi.lMicrosecsPerFrame		= vInfo.usPerFrame;
		fsi.lMicrosecsPerSrcFrame	= vInfo.usPerFrameIn;
		fsi.lCurrentFrame			= 0;
		fsi.flags					= fPreview ? FilterStateInfo::kStateRealTime | FilterStateInfo::kStatePreview : 0;

		if (filters.ReadyFilters(fsi))
			throw MyError("Error readying filters.");

		nVideoLagTimeline = nVideoLagOutput = filters.getFrameLag();

		// Inverse telecine?

		if (opt->video.fInvTelecine) {
			if (opt->video.mbUseSmartRendering)
				throw MyError("Inverse telecine cannot be used with smart rendering.");

			if (!(pInvTelecine = CreateVideoTelecineRemover(filters.InputBitmap(), !opt->video.fIVTCMode, opt->video.nIVTCOffset, opt->video.fIVTCPolarity)))
				throw MyMemoryError();

			nVideoLagTimeline = 10 + ((nVideoLagOutput+3)&~3)*5;
		}
	}

	vInfo.cur_dst = -nVideoLagTimeline;

	// initialize input decompressor

	if (mbDoVideo) {
		VideoSources::const_iterator it(mVideoSources.begin()), itEnd(mVideoSources.end());
		for(; it!=itEnd; ++it) {
			IVDVideoSource *vs = *it;

			vs->streamBegin(fPreview, opt->video.mode == DubVideoOptions::M_NONE);
		}
		fVDecompressionOk = true;

	}

	// Initialize audio.
	if (mbDoAudio)
		InitAudioConversionChain();

	// Initialize input window display.

	if (!mbInputDisplayInitialized && mpInputDisplay) {
		if (mbDoVideo)
			mpInputDisplay->SetSource(false, vSrc->getTargetFormat(), NULL, 0, true, opt->video.nPreviewFieldMode>0);
	}

	// initialize output parameters and output file

	InitOutputFile();

	// Initialize output window display.

	if (mpOutputDisplay && mbDoVideo) {
		if (opt->video.mode == DubVideoOptions::M_FULL)
			mpOutputDisplay->SetSource(false, mVideoFilterOutputPixmap, NULL, 0, true, opt->video.nPreviewFieldMode>0);
	}

	// initialize interleaver

	bool bAudio = mbDoAudio;

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

	// initialize frame iterator

	if (mbDoVideo) {
		mVideoFrameMap.Init(mVideoSources, vInfo.start_src, vInfo.frameRateIn / vInfo.frameRateNoTelecine, inputSubsetActive, vInfo.end_dst, opt->video.mode == DubVideoOptions::M_NONE);

		FilterSystem *filtsysToCheck = NULL;

		if (opt->video.mode >= DubVideoOptions::M_FULL && !filters.isEmpty() && opt->video.mbUseSmartRendering) {
			filtsysToCheck = &filters;
		}

		mVideoFrameIterator.Init(mVideoSources, opt->video.mode == DubVideoOptions::M_NONE, opt->video.mode != DubVideoOptions::M_NONE && opt->video.mbUseSmartRendering, filtsysToCheck);
	} else {
		mInterleaver.EndStream(0);
	}

	// Create data pipes.

	if (!(mpVideoPipe = new_nothrow AVIPipe(VDPreferencesGetRenderVideoBufferCount(), 16384)))
		throw MyMemoryError();

	if (mbDoAudio) {
		const WAVEFORMATEX *pwfex = audioStream->GetFormat();

		uint32 bytes = pwfex->nAvgBytesPerSec * 2;		// 2 seconds

		mAudioPipe.Init(bytes - bytes % pwfex->nBlockAlign, pwfex->nBlockAlign);
	}
}

void Dubber::Go(int iPriority) {
	// check the version.  if NT, don't touch the processing priority!
	bool fNoProcessingPriority = VDIsWindowsNT();

	if (!iPriority)
		iPriority = fNoProcessingPriority || !mpOutputSystem->IsRealTime() ? 5 : 6;

	// Initialize threads.
	mProcessThread.SetAbortSignal(&fAbort);
	mProcessThread.SetStatusHandler(pStatusHandler);
	mProcessThread.SetInputDisplay(mpInputDisplay);
	mProcessThread.SetOutputDisplay(mpOutputDisplay);
	mProcessThread.SetVideoIVTC(pInvTelecine);
	mProcessThread.SetVideoCompressor(mpVideoCompressor);

	if (!mVideoFilterOutput.empty())
		mProcessThread.SetVideoFilterOutput(&fsi, mVideoFilterOutput.data(), mVideoFilterOutputPixmap);

	mProcessThread.SetAudioSourcePresent(!mAudioSources.empty() && mAudioSources[0]);
	mProcessThread.SetVideoSources(mVideoSources.data(), mVideoSources.size());
	mProcessThread.SetAudioCorrector(audioCorrector);
	mProcessThread.Init(*opt, &vInfo, mpOutputSystem, mpVideoPipe, &mAudioPipe, &mInterleaver);
	mProcessThread.ThreadStart();

	SetThreadPriority(mProcessThread.getThreadHandle(), g_iPriorities[iPriority-1][0]);

	// Continue with other threads.

	if (!(mpIOThread = new_nothrow VDDubIOThread(
				fPhantom,
				mVideoSources,
				mVideoFrameIterator,
				audioStream,
				mbDoVideo ? mpVideoPipe : NULL,
				mbDoAudio ? &mAudioPipe : NULL,
				fAbort,
				aInfo,
				vInfo,
				mIOThreadCounter)))
		throw MyMemoryError();

	if (!mpIOThread->ThreadStart())
		throw MyError("Couldn't start I/O thread");

	SetThreadPriority(mpIOThread->getThreadHandle(), g_iPriorities[iPriority-1][1]);

	// We need to make sure that 100% actually means 100%.
	SetThrottleFactor((float)(opt->mThrottlePercent * 65536 / 100) / 65536.0f);

	// Create status window during the dub.
	if (pStatusHandler) {
		pStatusHandler->InitLinks(&aInfo, &vInfo, audioStatusStream, this, opt);
		pStatusHandler->Display(NULL, iPriority);
	}
}

//////////////////////////////////////////////

void Dubber::Stop() {
	if (mStopLock.xchg(1))
		return;

	fAbort = true;

	if (mpVideoPipe)
		mpVideoPipe->abort();

	mAudioPipe.Abort();
	mProcessThread.Abort();

	int nObjectsToWaitOn = 0;
	HANDLE hObjects[3];

	if (VDSignal *pBlitterSigComplete = mProcessThread.GetBlitterSignal())
		hObjects[nObjectsToWaitOn++] = pBlitterSigComplete->getHandle();

	if (mProcessThread.isThreadAttached())
		hObjects[nObjectsToWaitOn++] = mProcessThread.getThreadHandle();

	if (mpIOThread && mpIOThread->isThreadAttached())
		hObjects[nObjectsToWaitOn++] = mpIOThread->getThreadHandle();

	uint32 startTime = VDGetCurrentTick();

	bool quitQueued = false;

	while(nObjectsToWaitOn > 0) {
		DWORD dwRes;

		dwRes = MsgWaitForMultipleObjects(nObjectsToWaitOn, hObjects, FALSE, 10000, QS_ALLINPUT);

		if (WAIT_OBJECT_0 + nObjectsToWaitOn == dwRes) {
			if (!guiDlgMessageLoop(NULL))
				quitQueued = true;

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
	}
	if (quitQueued)
		PostQuitMessage(0);

	if (!fError && mpIOThread)
		fError = mpIOThread->GetError(err);

	if (!fError)
		fError = mProcessThread.GetError(err);

	delete mpIOThread;
	mpIOThread = 0;

	mProcessThread.Shutdown();

	if (pStatusHandler)
		pStatusHandler->Freeze();

	mpVideoCompressor = NULL;

	if (mpVideoPipe)	{ delete mpVideoPipe; mpVideoPipe = NULL; }
	mAudioPipe.Shutdown();

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

	// deinitialize DirectDraw

	filters.DeallocateBuffers();
	
	delete pInvTelecine;	pInvTelecine = NULL;

	if (pStatusHandler && vInfo.cur_proc_src >= 0)
		pStatusHandler->SetLastPosition(vInfo.cur_proc_src);

	if (fError) {
		throw err;
		fError = false;
	}
}

///////////////////////////////////////////////////////////////////

void Dubber::Abort() {
	if (!mStopLock) {
		fUserAbort = true;
		fAbort = true;
		mAudioPipe.Abort();
		mpVideoPipe->abort();
		PostMessage(g_hWnd, WM_USER, 0, 0);
	}
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

void Dubber::SetPriority(int index) {
	if (mpIOThread && mpIOThread->isThreadActive())
		SetThreadPriority(mpIOThread->getThreadHandle(), g_iPriorities[index][0]);

	if (mProcessThread.isThreadActive())
		SetThreadPriority(mProcessThread.getThreadHandle(), g_iPriorities[index][1]);
}

void Dubber::UpdateFrames() {
	mProcessThread.UpdateFrames();

	if (mLiveLockMessages < kLiveLockMessageLimit && !mStopLock) {
		uint32 curTime = VDGetCurrentTick();

		int iocount = mIOThreadCounter;
		int prcount = mProcessThread.GetActivityCounter();

		if (mLastIOThreadCounter != iocount) {
			mLastIOThreadCounter = iocount;
			mIOThreadFailCount = curTime;
		} else if (mLastIOThreadCounter && (curTime - mIOThreadFailCount - 30000) < 3600000) {		// 30s to 1hr
			if (mpIOThread->isThreadActive()) {
				void *eip = mpIOThread->ThreadLocation();
				const char *action = mpIOThread->GetCurrentAction();
				VDLogAppMessage(kVDLogInfo, kVDST_Dub, kVDM_IOThreadLivelock, 2, &eip, &action);
				++mLiveLockMessages;
			}
			mLastIOThreadCounter = 0;
		}

		if (mLastProcessingThreadCounter != prcount) {
			mLastProcessingThreadCounter = prcount;
			mProcessingThreadFailCount = curTime;
		} else if (mLastProcessingThreadCounter && (curTime - mProcessingThreadFailCount - 30000) < 3600000) {		// 30s to 1hr
			if (mProcessThread.isThreadActive()) {
				void *eip = mProcessThread.ThreadLocation();
				const char *action = mProcessThread.GetCurrentAction();
				VDLogAppMessage(kVDLogInfo, kVDST_Dub, kVDM_ProcessingThreadLivelock, 2, &eip, &action);
				++mLiveLockMessages;
			}
			mLastProcessingThreadCounter = 0;
		}
	}
}

void Dubber::SetThrottleFactor(float throttleFactor) {
	mProcessThread.SetThrottle(throttleFactor);
	if (mpIOThread)
		mpIOThread->SetThrottle(throttleFactor);
}
