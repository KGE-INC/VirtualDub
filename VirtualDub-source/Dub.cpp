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

#define f_DUB_CPP

#include "VirtualDub.h"

#include <process.h>
#include <crtdbg.h>
#include <time.h>
#include <stdio.h>

#include <windows.h>
#include <commctrl.h>
#include <vfw.h>
#include <ddraw.h>

#include "resource.h"

#include "crash.h"
#include "tls.h"
#include "convert.h"
#include "filters.h"
#include "gui.h"
#include "ddrawsup.h"
#include "prefs.h"
#include "command.h"
#include "misc.h"

//#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)

#include "Error.h"
#include "VideoSequenceCompressor.h"
#include "AsyncBlitter.h"
#include "AVIOutputPreview.h"
#include "AVIOutput.h"
#include "Histogram.h"
#include "AudioSource.h"
#include "VideoSource.h"
#include "AVIPipe.h"
#include "VBitmap.h"
#include "FrameSubset.h"
#include "InputFile.h"
#include "VideoTelecineRemover.h"

#include "dub.h"
#include "dubstatus.h"

//////////////

#define ASYNCHRONOUS_OVERLAY_SUPPORT

//#define STOP_SPEED_DEBUGGING

//#define RELEASE_MODE_DEBUGGING

#ifdef RELEASE_MODE_DEBUGGING
#define _XRPT0(y,x) OutputDebugString(x)
#else
#define _XRPT0(y,x) _RPT0(y,x)
#endif

#define DEBUG_SLEEP
//#define DEBUG_SLEEP Sleep(2000)

//////////////

#ifdef STOP_SPEED_DEBUGGING

	static __int64 start_time, stop_time;

#endif

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
		1048576,					// 1Mb AVI output buffer
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

	_RPT3(0,"scale %ld, rate %ld, length %ld\n",src->dwScale,src->dwRate, src->dwLength);
}
/////////////////////////////////////////////////

extern const char g_szOutOfMemory[];
extern const char g_szError[];
extern BOOL g_syncroBlit, g_vertical;
extern HWND g_hWnd;
extern HINSTANCE g_hInst;
extern bool g_fWine;

///////////////////////////////////////////////////////////////////////////

class Dubber : public IDubber {
private:
	static void CALLBACK DubFrameTimerProc(UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2);
	static int PulseCallbackProc(void *_thisPtr, DWORD framenum);

	void ResizeInputBuffer(long bufsize);
	void ResizeAudioBuffer(long bufsize);
	void ReadVideoFrame(long lVStreamPos, BOOL preload);
	void ReadNullVideoFrame(long lVStreamPos);
	long ReadAudio(long& lAStreamPos, long samples);
	void CheckSpill(long videopt, long audiopt);
	void NextSegment();
	void MainAddVideoFrame();
	void MainAddAudioFrame(int);
	static void MainThreadKickstart(void *thisPtr);
	void MainThread();
	void WriteVideoFrame(void *buffer, int exdata, LONG lastSize, long sample_num);
	void WriteAudio(void *buffer, long lActualBytes, long lActualSamples);
	static void ProcessingThreadKickstart(void *thisPtr);
	void ProcessingThread();

	MyError				err;
	bool				fError;
	LONG				lStopCount;

	DubOptions			*opt;
	AudioSource			*aSrc;
	VideoSource			*vSrc;
	InputFile			*pInput;
	AVIOutput			*AVIout;
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
	BOOL				fFiltersOk;
	BOOL				fNoProcessingPriority;
	UINT				timerInterval;
	MMRESULT			timerID;

	HANDLE				hEventAbortOk;
	volatile LONG		lThreadsActive;
	HANDLE				hThreadMain;
	HANDLE				hThreadProcessor;

	void *				inputBuffer;
	long				inputBufferSize;
	void *				audioBuffer;
	long				audioBufferSize;

	VideoSequenceCompressor	*pVideoPacker;

	unsigned char *		frameBuffer;

	AVIPipe *			pipe;
	AsyncBlitter *		blitter;

	HIC					outputDecompressor;
	HIC					hicOutput;
	HDRAWDIB			hDDInput, hDDOutput;
	HDC					hdcCompatInput, hdcCompatOutput;
	HBITMAP				hbmInput, hbmOutput, hbmInputOld, hbmOutputOld;
	BITMAPINFO			biInput, biOutput;
	void				*lpvInput, *lpvOutput;

	int					x_client, y_client;
	RECT				rInputFrame, rOutputFrame, rInputHistogram, rOutputHistogram;
	bool				fShowDecompressedFrame;
	bool				fDisplay565;
	IDDrawSurface		*pdsInput;
	IDDrawSurface		*pdsOutput;

	int					iOutputDepth;
	BITMAPINFOHEADER	*decompressedVideoFormat;
	BITMAPINFO			*compressorVideoFormat;
	BITMAPINFO			compressorVideoDIBFormat;
	BITMAPV4HEADER		bihDisplayFormat;

	AudioStream			*audioStreamSource;
	AudioStream			*audioStreamConverter;
	AudioStream			*audioStreamResampler;
	AudioStream			*audioStreamAmplifier;
	AudioStream			*audioSubsetFilter;
	AudioStream			*audioStream, *audioTimingStream;
	AudioStream			*audioStatusStream;
	AudioL3Corrector	*audioCorrector;
	AudioCompressor		*audioCompressor;

	FrameSubset				*inputSubsetActive;
	FrameSubset				*inputSubsetAlloc;
	VideoTelecineRemover	*pInvTelecine;
	int					nVideoLag;
	int					nVideoLagNoTelecine;
	int					nVideoLagPreload;

	WAVEFORMATEX		*wfexAudioCompressionFormat;
	long				cbAudioCompressionFormat;

	Histogram			*inputHisto, *outputHisto;

	volatile int		okToDraw;
	HWND				hwndStatus;

	DWORD				timer_counter, timer_period;
	bool				fSyncToAudioEvenClock;
	int					iFrameDisplacement;

	int					iPriority;
	long				lDropFrames;

	FilterStateInfo		fsi;

	bool				fPhantom;

	IDubStatusHandler	*pStatusHandler;

	__int64				i64SegmentSize;
	volatile __int64	i64SegmentCredit;
	__int64				i64SegmentThreshold;
	long				lVideoSizeEstimate;
	const char *		pszSegmentPrefix;
	bool				fEnableSpill;
	int					nSpillSegment;
	long				lSpillVideoPoint, lSpillAudioPoint;
	long				lSpillVideoOk, lSpillAudioOk;
	long				lSegmentFrameLimit;
	long				lSegmentFrameStart;

	///////

public:
	Dubber(DubOptions *);
	~Dubber();

	void SetAudioCompression(WAVEFORMATEX *wf, LONG cb);
	void SetPhantomVideoMode();
	void SetInputFile(InputFile *pInput);
	void SetFrameRectangles(RECT *prInput, RECT *prOutput);
	void SetClientRectOffset(int x, int y);
	void EnableSpill(const char *pszPrefix, __int64 threshold, long framethreshold);

	static void Dubber::SetClientRectOffset2(void *pv);
	static void Dubber::SetFrameRectangles2(void *pv);


	void InitAudioConversionChain();
	void InitOutputFile(char *pszFile);
	bool AttemptInputOverlay(BITMAPINFOHEADER *pbih);
	void AttemptInputOverlays();
	static void AttemptInputOverlays2(void *pThis);

	bool AttemptOutputOverlay();
	void InitDirectDraw();
	void InitDisplay();
	bool NegotiateFastFormat(BITMAPINFOHEADER *pbih);
	bool NegotiateFastFormat(int depth);
	void InitSelectInputFormat();
	void Init(VideoSource *video, AudioSource *audio, AVIOutput *out, char *szFile, HDC hDC, COMPVARS *videoCompVars);
	void Go(int iPriority = 0);
	void Stop();

	void RealizePalette();
	void Abort();
	void ForceAbort();
	bool isAbortedByUser();
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

Dubber::Dubber(DubOptions *xopt) {
	opt				= xopt;
	aSrc			= NULL;
	vSrc			= NULL;
	pInput			= NULL;

	// clear the workin' variables...

	fError				= false;
	lStopCount			= 0;

	fAbort				= false;
	fUserAbort			= false;

	pVideoPacker		= NULL;
	pStatusHandler		= NULL;

	fADecompressionOk	= false;
	fVDecompressionOk	= false;
	fFiltersOk			= FALSE;
	timerInterval		= 0;
	timerID				= NULL;

	hEventAbortOk		= INVALID_HANDLE_VALUE;
	lThreadsActive		= 0;
	hThreadMain			= NULL;
	hThreadProcessor	= NULL;
	inputBuffer			= NULL;
	audioBuffer			= NULL;
	inputBufferSize		= 0;
	audioBufferSize		= 0;
//	hFileShared			= INVALID_HANDLE_VALUE;
//	tempBuffer			= NULL;
//	outputBuffer		= NULL;
	pipe				= NULL;
	blitter				= NULL;
	outputDecompressor	= NULL;
	hDDInput			= NULL;
	hDDOutput			= NULL;
	okToDraw			= 0;
	hwndStatus			= NULL;
	vInfo.total_size	= 0;
	aInfo.total_size	= 0;
	vInfo.fAudioOnly	= false;

	hdcCompatInput = hdcCompatOutput = NULL;
	hbmInput = hbmOutput = NULL;
	pdsInput			= NULL;
	pdsOutput			= NULL;

	audioStreamSource		= NULL;
	audioStreamConverter	= NULL;
	audioStreamResampler	= NULL;
	audioStreamAmplifier	= NULL;
	audioStatusStream		= NULL;
	audioSubsetFilter		= NULL;
	audioCompressor			= NULL;
	audioCorrector			= NULL;

	inputSubsetActive		= NULL;
	inputSubsetAlloc		= NULL;

	wfexAudioCompressionFormat = NULL;

	inputHisto			= NULL;
	outputHisto			= NULL;

	iFrameDisplacement	= 0;

	fPhantom = false;

	pInvTelecine		= NULL;

	i64SegmentSize		= 0;
	i64SegmentCredit	= 0;
	AVIout				= NULL;
	fEnableSpill		= false;

	lSpillVideoPoint	= 0;
	lSpillAudioPoint	= 0;
	lSegmentFrameLimit	= 0;
	lSegmentFrameStart	= 0;

	hicOutput = NULL;
}

Dubber::~Dubber() {
	_RPT0(0,"Dubber: destructor called.\n");

	Stop();
}

/////////////////////////////////////////////////

void Dubber::SetAudioCompression(WAVEFORMATEX *wf, LONG cb) {
	wfexAudioCompressionFormat = wf;
	cbAudioCompressionFormat = cb;
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

#ifdef ASYNCHRONOUS_OVERLAY_SUPPORT
		blitter->postAFC(0x80000000, SetFrameRectangles2, &DubberSetFrameRectangles(pdsInput, &r));
#else
		pdsInput->SetOverlayPos(&r);
#endif
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
#ifdef ASYNCHRONOUS_OVERLAY_SUPPORT
		blitter->postAFC(0x80000000, SetClientRectOffset2, &DubberSetClientRectOffset(pdsInput, x + rInputFrame.left, y + rInputFrame.top));
#else
		pdsInput->MoveOverlay(x + rInputFrame.left, y + rInputFrame.top);
#endif
}

void Dubber::EnableSpill(const char *pszPrefix, __int64 segsize, long framecnt) {
	pszSegmentPrefix = pszPrefix;
	fEnableSpill = true;
	nSpillSegment = 1;
	i64SegmentThreshold = segsize;
	lSegmentFrameLimit = framecnt;
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
		vInfo.start_src		= vInfo.start_dst	= 0;
		vInfo.end_src		= vInfo.end_dst		= 0;
	}

	if (audio) {
		aInfo.start_src		= audio->lSampleFirst;
		aInfo.end_src		= audio->lSampleLast;
	} else {
		aInfo.start_src		= aInfo.start_dst	= 0;
		aInfo.end_src		= aInfo.end_dst		= 0;
	}

	vInfo.cur_src			= vInfo.start_src;
	aInfo.cur_src			= aInfo.start_src;

	if (video) {
		// compute new frame rate

		vInfo.usPerFrame	= MulDiv(video->streamInfo.dwScale,1000000,video->streamInfo.dwRate);

		if (opt->video.frameRateNewMicroSecs == DubVideoOptions::FR_SAMELENGTH) {
			if (audio && audio->streamInfo.dwLength)
				vInfo.usPerFrame	= MulDiv(audio->samplesToMs(audio->streamInfo.dwLength),1000,video->streamInfo.dwLength);
		} else if (opt->video.frameRateNewMicroSecs)
			vInfo.usPerFrame	= opt->video.frameRateNewMicroSecs;

		// are we supposed to offset the video?

		if (opt->video.lStartOffsetMS) {
			vInfo.start_src += video->msToSamples(opt->video.lStartOffsetMS); 
		}

		if (opt->video.lEndOffsetMS)
			vInfo.end_src -= video->msToSamples(opt->video.lEndOffsetMS);

		vInfo.usPerFrameIn	= vInfo.usPerFrame;
		vInfo.usPerFrame	*= opt->video.frameRateDecimation;

		// make sure we start reading on a key frame

		if (opt->video.mode == DubVideoOptions::M_NONE)
			vInfo.start_src	= video->nearestKey(vInfo.start_src);

		vInfo.cur_src		= vInfo.start_src;
		vInfo.cur_dst		= vInfo.start_dst;
	}

	if (audio) {
		long lStartOffsetMS = -opt->audio.offset;

		// offset the start of the audio appropriately...

		if (opt->audio.fStartAudio && video && opt->video.lStartOffsetMS) {
			if (!pfs)
				lStartOffsetMS += MulDiv(vInfo.usPerFrame, vInfo.start_src - video->lSampleFirst, 1000*opt->video.frameRateDecimation);
		}

		aInfo.start_src += audio->msToSamples(lStartOffsetMS);

		// clip the end of the audio if supposed to...

		if (opt->audio.fEndAudio) {
			long lMaxLength;

			lMaxLength = (long)((
							(
								(__int64)(vInfo.end_src - vInfo.start_src)
								*
								audio->streamInfo.dwRate
							)
							* vInfo.usPerFrame
						) / ((__int64)audio->streamInfo.dwScale * 1000000 * opt->video.frameRateDecimation));

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
		aInfo.cur_dst		= aInfo.start_dst;
	}

	vInfo.cur_proc_src = vInfo.cur_src;
	aInfo.cur_proc_src = aInfo.cur_src;

	_RPT3(0,"Dub: Audio is from (%ld,%ld) starting at %ld\n", aInfo.start_src, aInfo.end_src, aInfo.cur_src);
	_RPT3(0,"Dub: Video is from (%ld,%ld) starting at %ld\n", vInfo.start_src, vInfo.end_src, vInfo.cur_src);
}

//////////////////////////////////////////////////////////////////////////////

// may be called at any time in Init() after streams setup

void Dubber::InitAudioConversionChain() {

	// ready the audio stream for streaming operation

	aSrc->streamBegin(fPreview);
	fADecompressionOk = true;

	// Initialize audio conversion chain
	//
	// First, create a source.

	if (!(audioStreamSource = new AudioStreamSource(aSrc, aInfo.start_src, aSrc->lSampleLast - aInfo.start_src, opt->audio.mode > DubAudioOptions::M_NONE)))
		throw MyError("Dub: Unable to create audio stream source");

	audioStream = audioStreamSource;

	// Attach a converter if we need to...

	if (aInfo.converting) {
		if (aInfo.single_channel)
			audioStreamConverter = new AudioStreamConverter(audioStream, aInfo.is_16bit, aInfo.is_right, true);
		else
			audioStreamConverter = new AudioStreamConverter(audioStream, aInfo.is_16bit, aInfo.is_stereo, false);

		if (!audioStreamConverter)
			throw MyError("Dub: Unable to create audio stream converter");

		audioStream = audioStreamConverter;
	}

	// Attach a converter if we need to...

	if (aInfo.resampling) {
		if (!(audioStreamResampler = new AudioStreamResampler(audioStream, opt->audio.new_rate ? opt->audio.new_rate : aSrc->getWaveFormat()->nSamplesPerSec, opt->audio.integral_rate, opt->audio.fHighQuality)))
			throw MyError("Dub: Unable to create audio stream resampler");

		audioStream = audioStreamResampler;
	}

	// Attach an amplifier if needed...

	if (opt->audio.mode > DubAudioOptions::M_NONE && opt->audio.volume) {
		if (!(audioStreamAmplifier = new AudioStreamAmplifier(audioStream, opt->audio.volume)))
			throw MyError("Dub: Unable to create audio stream amplifier");

		audioStream = audioStreamAmplifier;
	}

	audioTimingStream = audioStream;

	// Tack on a subset filter as well...

	if (inputSubsetActive) {
		if (!(audioSubsetFilter = new AudioSubset(audioStream, inputSubsetActive, vInfo.usPerFrameIn,
					opt->audio.fStartAudio ? MulDiv(vInfo.usPerFrameIn, vInfo.start_src, 1000) : 0
				)))
			throw MyError("Dub: Unable to create audio subset filter");

		audioStream = audioSubsetFilter;
	}

	// Make sure we only get what we want...

	if (vSrc && opt->audio.fEndAudio)
		audioStream->SetLimit((long)((
			((__int64)(vInfo.end_src - vInfo.start_src) * audioStream->GetFormat()->nAvgBytesPerSec * (__int64)vInfo.usPerFrameIn)) / ((__int64)1000000 * audioStream->GetFormat()->nBlockAlign)
			));

	audioStatusStream = audioStream;

	// Tack on a compressor if we want...

	if (opt->audio.mode > DubAudioOptions::M_NONE && wfexAudioCompressionFormat) {
		if (!(audioCompressor = new AudioCompressor(audioStream, wfexAudioCompressionFormat, cbAudioCompressionFormat)))
			throw MyError("Dub: Unable to create audio compressor");

		audioStream = audioCompressor;
	}

	// Check the output format, and if we're compressing to
	// MPEG Layer III, compensate for the lag and create a bitrate corrector

	if (!fEnableSpill && !g_prefs.fNoCorrectLayer3 && audioCompressor && audioCompressor->GetFormat()->wFormatTag == WAVE_FORMAT_MPEGLAYER3) {

		audioCompressor->CompensateForMP3();

		if (!(audioCorrector = new AudioL3Corrector()))
			throw MyError("Dub: Unable to create audio corrector");
	}

}

void Dubber::InitOutputFile(char *szFile) {

	// Do audio.

	if (aSrc && AVIout->audioOut) {
		WAVEFORMATEX *outputAudioFormat;
		LONG outputAudioFormatSize;

		// initialize AVI parameters...

		AVISTREAMINFOtoAVIStreamHeader(&AVIout->audioOut->streamInfo, &aSrc->streamInfo);
		AVIout->audioOut->streamInfo.dwStart			= 0;
		AVIout->audioOut->streamInfo.dwInitialFrames	= opt->audio.preload ? 1 : 0;

		if (!(outputAudioFormat = (WAVEFORMATEX *)AVIout->audioOut->allocFormat(outputAudioFormatSize = audioStream->GetFormatLen())))
			throw MyMemoryError();

		memcpy(outputAudioFormat, audioStream->GetFormat(), audioStream->GetFormatLen());

		if (opt->audio.mode > DubAudioOptions::M_NONE) {
			AVIout->audioOut->streamInfo.dwSampleSize = outputAudioFormat->nBlockAlign;
			AVIout->audioOut->streamInfo.dwRate		= outputAudioFormat->nAvgBytesPerSec;
			AVIout->audioOut->streamInfo.dwScale	= outputAudioFormat->nBlockAlign;
			AVIout->audioOut->streamInfo.dwLength	= MulDiv(AVIout->audioOut->streamInfo.dwLength, outputAudioFormat->nSamplesPerSec, aSrc->getWaveFormat()->nSamplesPerSec);
		}
	}

	// Do video.

	if (vSrc && AVIout->videoOut) {
		VBitmap *outputBitmap;
		
		if (opt->video.mode >= DubVideoOptions::M_FULL)
			outputBitmap = filters.OutputBitmap();
		else
			outputBitmap = filters.InputBitmap();

		AVISTREAMINFOtoAVIStreamHeader(&AVIout->videoOut->streamInfo, &vSrc->streamInfo);
		if (opt->video.mode > DubVideoOptions::M_NONE) {
			if (fUseVideoCompression) {
				AVIout->videoOut->streamInfo.fccHandler	= compVars->fccHandler;
				AVIout->videoOut->streamInfo.dwQuality	= compVars->lQ;
			} else {
				AVIout->videoOut->streamInfo.fccHandler	= mmioFOURCC('D','I','B',' ');
			}
		}
		if (opt->video.frameRateNewMicroSecs || pInvTelecine) {
			AVIout->videoOut->streamInfo.dwRate		= 1000000L; // / opt->video.frameRateDecimation;
			AVIout->videoOut->streamInfo.dwScale	= vInfo.usPerFrame; //opt->video.frameRateNewMicroSecs;
		} else {

			// Dividing dwRate isn't good if we get a fraction like 10/1!

			if (AVIout->videoOut->streamInfo.dwScale > 0x7FFFFFFF / opt->video.frameRateDecimation)
				AVIout->videoOut->streamInfo.dwRate		/= opt->video.frameRateDecimation;
			else
				AVIout->videoOut->streamInfo.dwScale	*= opt->video.frameRateDecimation;
		}
		AVIout->videoOut->streamInfo.dwLength		= vInfo.end_src - vInfo.start_src;
		AVIout->videoOut->streamInfo.dwLength		/= opt->video.frameRateDecimation;

		AVIout->videoOut->streamInfo.rcFrame.left	= 0;
		AVIout->videoOut->streamInfo.rcFrame.top	= 0;
		AVIout->videoOut->streamInfo.rcFrame.right	= (short)outputBitmap->w;
		AVIout->videoOut->streamInfo.rcFrame.bottom	= (short)outputBitmap->h;

		AVIout->videoOut->setCompressed(TRUE);

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

		_RPT0(0,"Dub: Initializing output compressor.\n");

		if (fUseVideoCompression) {
			LONG formatSize;
			DWORD icErr;

			formatSize = ICCompressGetFormatSize(compVars->hic, compressorVideoFormat);
			if (formatSize < ICERR_OK)
				throw "Error getting compressor output format size.";

			_RPT1(0,"Video compression format size: %ld\n",formatSize);

			if (!AVIout->videoOut->allocFormat(formatSize))
				throw MyError("Out of memory");

			memset(AVIout->videoOut->getFormat(), 0, formatSize);

			if (ICERR_OK != (icErr = ICCompressGetFormat(compVars->hic,
								(BITMAPINFOHEADER *)compressorVideoFormat,
								AVIout->videoOut->getImageFormat())))
				throw MyICError("Output compressor",icErr);
				//throw "Error getting compressor output format.";

			if (!(pVideoPacker = new VideoSequenceCompressor()))
				throw MyMemoryError();

			pVideoPacker->init(compVars->hic, compressorVideoFormat, (BITMAPINFO *)AVIout->videoOut->getImageFormat(), compVars->lQ, compVars->lKey);
			pVideoPacker->setDataRate(compVars->lDataRate*1024, vInfo.usPerFrameIn, vInfo.end_src - vInfo.start_src);
			pVideoPacker->start();

			lVideoSizeEstimate = pVideoPacker->getMaxSize();

			// attempt to open output decompressor

			if (opt->video.mode <= DubVideoOptions::M_FASTREPACK)
				fShowDecompressedFrame = false;
			else if (fShowDecompressedFrame = !!opt->video.fShowDecompressedFrame) {
				DWORD err;

				if (!(outputDecompressor = ICLocate(
							'CDIV',
							AVIout->videoOut->streamInfo.fccHandler,
							AVIout->videoOut->getImageFormat(),
							&compressorVideoFormat->bmiHeader, ICMODE_DECOMPRESS))) {

					MyError("Output video warning: Could not locate output decompressor.").post(NULL,g_szError);

				} else if (ICERR_OK != (err = ICDecompressBegin(
						outputDecompressor,
						AVIout->videoOut->getImageFormat(),
						&compressorVideoFormat->bmiHeader))) {

					MyICError("Output video warning", err).post(NULL,g_szError);

					ICClose(outputDecompressor);
					outputDecompressor = NULL;

					fShowDecompressedFrame = false;
				}
			}

		} else {
			BITMAPINFOHEADER *outputVideoFormat;

			if (opt->video.mode < DubVideoOptions::M_SLOWREPACK) {

				if (vSrc->getImageFormat()->biCompression == 0xFFFFFFFF)
					throw MyError("The source video stream uses a compression algorithm which is not compatible with AVI files. "
								"Direct stream copy cannot be used with this video stream.");

				AVIout->videoOut->setCompressed(TRUE);
				if (!(outputVideoFormat = (BITMAPINFOHEADER *)AVIout->videoOut->allocFormat(vSrc->getFormatLen())))
					throw MyMemoryError();

				memcpy(outputVideoFormat, vSrc->getImageFormat(), vSrc->getFormatLen());
			} else {
				if (!(outputVideoFormat = (BITMAPINFOHEADER *)AVIout->videoOut->allocFormat(sizeof(BITMAPINFOHEADER))))
					throw MyMemoryError();

				memcpy(outputVideoFormat, vSrc->getDecompressedFormat(), sizeof(BITMAPINFOHEADER));

				if (opt->video.mode == DubVideoOptions::M_FULL) {
					outputVideoFormat->biCompression= BI_RGB;
					outputVideoFormat->biWidth		= outputBitmap->w;
					outputVideoFormat->biHeight		= outputBitmap->h;
					outputVideoFormat->biBitCount	= iOutputDepth;
					outputVideoFormat->biSizeImage	= outputBitmap->pitch * outputBitmap->h;
				}
				AVIout->videoOut->setCompressed(TRUE);

				lVideoSizeEstimate = outputVideoFormat->biSizeImage;
				lVideoSizeEstimate = (lVideoSizeEstimate+1) & -2;
			}

		}
	}

	_RPT0(0,"Dub: Creating output file.\n");

	if (!AVIout->init(
				szFile,
				AVIout->videoOut ? vSrc ? compressorVideoFormat->bmiHeader.biWidth : 320 : 0, //filters.OutputBitmap()->w,
				AVIout->videoOut ? vSrc ? compressorVideoFormat->bmiHeader.biHeight : 240 : 0, //filters.OutputBitmap()->h,
				!!vSrc,
				!!aSrc,
				opt->perf.outputBufferSize,
				opt->audio.enabled))
		throw MyError("Problem initializing AVI output.");
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
	bihDisplayFormat.bV4BitCount		= ddpf.dwRGBBitCount;
	bihDisplayFormat.bV4V4Compression		= BI_RGB;
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

#ifdef ASYNCHRONOUS_OVERLAY_SUPPORT
	if (opt->video.mode == DubVideoOptions::M_SLOWREPACK) {
		blitter->postAFC(0x80000000, AttemptInputOverlays2, this);
	}
#else
	AttemptInputOverlays();
#endif

	// How about DirectShow output acceleration?

	if (opt->video.mode == DubVideoOptions::M_FULL)
		AttemptOutputOverlay();

	if (pdsInput || pdsOutput)
		SetClientRectOffset(x_client, y_client);
}

void Dubber::InitDisplay() {
	_RPT0(0,"Dub: Initializing input window display.\n");


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

			if (hDDInput = DrawDibOpen()) {
				if (!DrawDibBegin(hDDInput, hDCWindow, vSrc->getDecompressedFormat()->biWidth, vSrc->getDecompressedFormat()->biHeight, vSrc->getDecompressedFormat(), vSrc->getDecompressedFormat()->biWidth, vSrc->getDecompressedFormat()->biHeight, 0)) {
					DrawDibClose(hDDInput);
					hDDInput = NULL;
					_RPT0(0,"Dub WARNING: could not init input video window!\n");
				}
			}
		} else {
			if (!(hdcCompatInput = CreateCompatibleDC(hDCWindow)))
				throw MyError("Couldn't create compatible display context for input window");

			if (!(hbmInput = CreateDIBSection(
					hdcCompatInput,
					(LPBITMAPINFO)vSrc->getDecompressedFormat(),
					DIB_RGB_COLORS,
					&lpvInput,
					vSrc->getFrameBufferObject(),
					vSrc->getFrameBufferOffset()
				)))
				throw MyError("Couldn't create DIB section for input window");

			hbmInputOld = (HBITMAP)SelectObject(hdcCompatInput, hbmInput);
		}
	}

	_RPT0(0,"Dub: Initializing output window display.\n");
	if (opt->video.mode == DubVideoOptions::M_FULL) {
		if (!pdsOutput && !g_fWine) {
			if (bitsPerPel < 15) {
				if (hDDOutput = DrawDibOpen()) {
					if (!DrawDibBegin(
								hDDOutput,
								hDCWindow,
								compressorVideoFormat->bmiHeader.biWidth,
								compressorVideoFormat->bmiHeader.biHeight,
								&compressorVideoFormat->bmiHeader,
								compressorVideoFormat->bmiHeader.biWidth,
								compressorVideoFormat->bmiHeader.biHeight,
								0)) {

						DrawDibClose(hDDOutput);
						hDDOutput = NULL;
					}
				}
			} else {
				if (!(hdcCompatOutput = CreateCompatibleDC(hDCWindow)))
					throw MyError("Couldn't create compatible display context for output window");

				// check to see if DC is 565 16-bit, the only mode that does not support a line
				// of grays... hmm... is 15 possible for bitsPerPel?

				COLORREF crTmp;

				fDisplay565 = false;

				if (bitsPerPel==15 || bitsPerPel==16) {
					crTmp = GetPixel(hDCWindow, 0,0);
					SetPixel(hDCWindow,0,0,RGB(0x80, 0x88, 0x80));

					if (GetPixel(hDCWindow,0,0) == RGB(0x80, 0x88, 0x80)) {
						fDisplay565 = true;

						_RPT0(0,"Display is 5-6-5 16-bit\n");
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

				if (!(hbmOutput = CreateDIBSection(
						hdcCompatOutput,
						(LPBITMAPINFO)&bihDisplayFormat,
						DIB_RGB_COLORS,
						&lpvOutput,
						hMapObject,
						lMapOffset
					)))
					throw MyError("Couldn't create DIB section for output window");

				hbmOutputOld = (HBITMAP)SelectObject(hdcCompatOutput, hbmOutput);
			}

			// attempt to open output decompressor
#if 0
			ICINFO info;

			for(int i=0; ICInfo(ICTYPE_VIDEO, i, &info); i++) {
				char szName[256];

				hicOutput = ICOpen(ICTYPE_VIDEO, info.fccHandler, ICMODE_DRAW);
				if (!hicOutput)
					continue;

				ICGetInfo(hicOutput, &info, sizeof info);

				WideCharToMultiByte(CP_ACP, 0, info.szDescription, -1, szName, sizeof szName, NULL, NULL);

				if (strstr(szName, "miroVIDEO")) {

					if (hicOutput) {
						ICDrawBegin(hicOutput, ICDRAW_HDC, NULL, g_hWnd, hDCWindow, 0, 0,
								compressorVideoFormat->bmiHeader.biWidth,
								compressorVideoFormat->bmiHeader.biHeight,
								AVIout->videoOut->getImageFormat(),
								0,
								0,
								compressorVideoFormat->bmiHeader.biWidth,
								compressorVideoFormat->bmiHeader.biHeight,
								1000000,
								vInfo.usPerFrame);

						ICDrawStart(hicOutput);
					}

					break;
				}

				ICClose(hicOutput);
				hicOutput = NULL;
			}
#endif
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

		// Attempt IF09.

#if 0
		int blocks;

		memcpy(&bih, vSrc->getImageFormat(), sizeof(BITMAPINFOHEADER));

		blocks = ((bih.biWidth+3)/4)*((bih.biHeight+3)/4);

		bih.biSize			= sizeof(BITMAPINFOHEADER);
		bih.biPlanes		= 3;
		bih.biBitCount		= 9;		// does it matter?
		bih.biCompression	= '90FI';
		bih.biSizeImage		= blocks*(16 + 2) + ((blocks+31)/32)*4;
		bih.biXPelsPerMeter	= 0;
		bih.biYPelsPerMeter	= 0;
		bih.biClrUsed		= 0;
		bih.biClrImportant	= 0;

		if (NegotiateFastFormat(&bih))
			return;
#endif

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
						throw MyError("VCM cannot decompress to a format we can handle.");
}

void Dubber::Init(VideoSource *video, AudioSource *audio, AVIOutput *out, char *szFile, HDC hDC, COMPVARS *videoCompVars) {

	aSrc				= audio;
	vSrc				= video;
	AVIout				= out;

	fPreview			= !!AVIout->isPreview();

	compVars			= videoCompVars;
	hDCWindow			= hDC;
	fUseVideoCompression = !fPreview && opt->video.mode>DubVideoOptions::M_NONE && compVars && (compVars->dwFlags & ICMF_COMPVARS_VALID) && compVars->hic;
//	fUseVideoCompression = opt->video.mode>DubVideoOptions::M_NONE && compVars && (compVars->dwFlags & ICMF_COMPVARS_VALID) && compVars->hic;

	// check the mode; if we're using DirectStreamCopy or Fast mode, we'll need to
	// align the subset to keyframe boundaries!

	if (vSrc && inputSubset) {
		inputSubsetActive = inputSubset;

		if (inputSubset && opt->video.mode < DubVideoOptions::M_SLOWREPACK) {
			FrameSubsetNode *pfsn;

			if (!(inputSubsetActive = inputSubsetAlloc = new FrameSubset()))
				throw MyMemoryError();

			pfsn = inputSubset->getFirstFrame();
			while(pfsn) {
				long end = pfsn->start + pfsn->len;
				long start = vSrc->nearestKey(pfsn->start + vSrc->lSampleFirst) - vSrc->lSampleFirst;

				_RPT3(0,"   subset: %5d[%5d]-%-5d\n", pfsn->start, start, pfsn->start+pfsn->len-1);
				inputSubsetActive->addRangeMerge(start, end-start);

				pfsn = inputSubset->getNextFrame(pfsn);
			}

#ifdef _DEBUG
			pfsn = inputSubsetActive->getFirstFrame();

			while(pfsn) {
				_RPT2(0,"   padded subset: %8d-%-8d\n", pfsn->start, pfsn->start+pfsn->len-1);
				pfsn = inputSubsetActive->getNextFrame(pfsn);
			}
#endif
		}
	}

	// initialize stream values

	InitStreamValuesStatic(vInfo, aInfo, video, audio, opt, inputSubsetActive);

	vInfo.nLag = 0;
	vInfo.usPerFrameNoTelecine = vInfo.usPerFrame;
	if (opt->video.mode >= DubVideoOptions::M_FULL && opt->video.fInvTelecine) {
		vInfo.usPerFrame = MulDiv(vInfo.usPerFrame, 30, 24);
	}

	lSpillVideoOk = vInfo.cur_src;
	lSpillAudioOk = aInfo.cur_src;

	_RPT0(0,"Dub: Initializing AVI output.\n");

	if (!(AVIout->initOutputStreams())) throw MyError("Out of memory");

	_RPT0(0,"Dub: Creating blitter.\n");

	if (g_syncroBlit || !fPreview)
		blitter = new AsyncBlitter();
	else
		blitter = new AsyncBlitter(8);

	if (!blitter) throw MyError("Couldn't create AsyncBlitter");

	blitter->pulse();

	// Select an appropriate input format.  This is really tricky...

	vInfo.fAudioOnly = true;
	if (vSrc && AVIout->videoOut) {
		InitSelectInputFormat();
		vInfo.fAudioOnly = false;
	}

	iOutputDepth = 16+8*opt->video.outputDepth;

	// Initialize filter system.

	nVideoLag = nVideoLagNoTelecine = 0;

	if (vSrc) {
		BITMAPINFOHEADER *bmih = vSrc->getDecompressedFormat();

		filters.initLinearChain(&g_listFA, (Pixel *)(bmih+1), bmih->biWidth, bmih->biHeight, 32 /*bmih->biBitCount*/, iOutputDepth);

		fsi.lMicrosecsPerFrame		= vInfo.usPerFrame;
		fsi.lMicrosecsPerSrcFrame	= vInfo.usPerFrameIn;
		fsi.lCurrentFrame			= 0;

		if (filters.ReadyFilters(&fsi))
			throw "Error readying filters.";

		fFiltersOk = TRUE;

		nVideoLagNoTelecine = nVideoLag = filters.getFrameLag();

		// Inverse telecine?

		if (opt->video.mode >= DubVideoOptions::M_FULL && opt->video.fInvTelecine) {
			if (!(pInvTelecine = CreateVideoTelecineRemover(filters.InputBitmap(), !opt->video.fIVTCMode, opt->video.nIVTCOffset, opt->video.fIVTCPolarity)))
				throw MyMemoryError();

			nVideoLag += 10;
		}
	}

	nVideoLagPreload = nVideoLagNoTelecine;
	vInfo.nLag = nVideoLag;

	// initialize directdraw display if in preview

	if (fPreview)
		InitDirectDraw();

	// initialize input decompressor

	if (vSrc && AVIout->videoOut) {

		_RPT0(0,"Dub: Initializing input decompressor.\n");

		vSrc->streamBegin(fPreview);
		fVDecompressionOk = TRUE;

	}

	// Initialize audio.

	_RPT0(0,"Dub: Initializing audio.\n");

	if (aSrc)
		InitAudioConversionChain();

	// Initialize output file.

	InitOutputFile(szFile);

	// Initialize input window display.

	if (vSrc && AVIout->videoOut)
		InitDisplay();

	// Allocate input buffer.

	if (!(inputBuffer = allocmem(inputBufferSize = 65536)))
		throw MyMemoryError();

	// Create a pipe.

	_RPT0(0,"Dub: Creating data pipe.\n");

	if (!(pipe = new AVIPipe(opt->perf.pipeBufferCount, 16384)) || !pipe->isOkay())
		throw MyError("Couldn't create pipe");

	// Create events.

	_RPT0(0,"Dub: Creating events.\n");

	if (!(hEventAbortOk = CreateEvent(NULL,FALSE,FALSE,NULL)))
		throw MyError("Couldn't create abort event");
}

void Dubber::Go(int iPriority) {
	OSVERSIONINFO ovi;

	// check the version.  if NT, don't touch the processing priority!

	ovi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	fNoProcessingPriority = GetVersionEx(&ovi) && ovi.dwPlatformId == VER_PLATFORM_WIN32_NT;

	if (!iPriority)
		iPriority = fNoProcessingPriority || !AVIout->isPreview() ? 5 : 6;

	this->iPriority = iPriority;

	// Reset timer.

	_XRPT0(0,"Dub: Starting multimedia timer.\n");

	if (fPreview) {
//		timerInterval = MulDiv(AVIout->videoOut->streamInfo.dwScale, 1000L, AVIout->videoOut->streamInfo.dwRate);
		timerInterval = vInfo.usPerFrame / 1000;

		if (opt->video.fSyncToAudio || opt->video.nPreviewFieldMode) {
//			blitter->setPulseCallback(PulseCallbackProc, this);

			timerInterval /= 2;
		}

//		timerInterval /= 2;

		if (TIMERR_NOERROR != timeBeginPeriod(timerInterval)) {
			timerInterval = 0;
			throw MyError("Couldn't initialize timer!");
		}

		timer_counter = 0;
		timer_period = timerInterval;

		if (!(timerID = timeSetEvent(timerInterval, timerInterval, DubFrameTimerProc, (DWORD)this, TIME_PERIODIC)))
			throw MyError("Couldn't start timer!");

	}

	// Initialize threads.

	_XRPT0(0,"Dub: Kickstarting threads.\n");

	if (!(hThreadProcessor = (HANDLE)_beginthread(ProcessingThreadKickstart, 0, (void *)this)))
		throw MyError("Couldn't create processing thread");

//	if (fPreview && !fNoProcessingPriority)
	SetThreadPriority(hThreadProcessor, g_iPriorities[iPriority-1][0]);

	// Wait for the thread count to increment to 1.  Otherwise, it's possible for
	// one of the later initialization events to fail, and the end() function to
	// be called before the first thread is even scheduled.  If this happens,
	// the thread will be left dangling.

	while(!lThreadsActive) Sleep(100);

	// Continue with other threads.

	if (!(hThreadMain = (HANDLE)_beginthread(MainThreadKickstart, 0, (void *)this)))
		throw "Couldn't create main dubbing thread";

	SetThreadPriority(hThreadMain, g_iPriorities[iPriority-1][1]);

	// Create status window during the dub.

	_XRPT0(0,"Dub: Creating status window.\n");

	pStatusHandler->InitLinks(&aInfo, &vInfo, aSrc, vSrc, pInput, audioStatusStream, this, opt);

	if (hwndStatus = pStatusHandler->Display(NULL, iPriority)) {
		MSG msg;

		// NOTE: WM_QUIT messages seem to get blocked if the window is dragging/sizing
		//		 or has a menu.

		DEFINE_SP(sp);

		while (!fAbort && GetMessage(&msg, (HWND) NULL, 0, 0)) { 

			CHECK_STACK(sp);

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

	_RPT0(0,"Dub: exit.\n");
}

//////////////////////////////////////////////

static void DestroyIDDrawSurface(void *pv) {
	delete (IDDrawSurface *)pv;
	DDrawDeinitialize();
}

void Dubber::Stop() {
	bool fSkipDXShutdown = false;

	if (InterlockedExchange(&lStopCount, 1))
		return;

	_XRPT0(0,"Dub: Beginning stop process.\n");

	if (pipe)
		pipe->abort();

	if (blitter)
		blitter->flush();

	_XRPT0(0,"Dub: Killing threads.\n"); DEBUG_SLEEP;

	fAbort = TRUE;
	while(lThreadsActive) {
		DWORD dwRes;

		dwRes = MsgWaitForMultipleObjects(1, &hEventAbortOk, FALSE, 10000, QS_ALLINPUT);

		if (WAIT_OBJECT_0+1 == dwRes)
			guiDlgMessageLoop(hwndStatus);
		else if (WAIT_TIMEOUT == dwRes) {
			MessageBox(g_hWnd, "Thread deadlock detected attempting to abort - killing process.", "VirtualDub Internal Error", MB_ICONEXCLAMATION|MB_OK);
			ExitProcess(0);
		}

_XRPT0(0,"\tDub: Threads still active\n");

		_RPT1(0,"\tDub: %ld threads active\n", lThreadsActive);

#ifdef _DEBUG
		if (blitter) _RPT1(0,"\t\tBlitter locks active: %08lx\n", blitter->lock_state);
#endif
	}

	if (blitter)
		blitter->flush();

	_XRPT0(0,"Dub: Freezing status handler.\n"); DEBUG_SLEEP;

	if (pStatusHandler)
		pStatusHandler->Freeze();

	_XRPT0(0,"Dub: Killing timers.\n"); DEBUG_SLEEP;

	if (timerID)		{ timeKillEvent(timerID);		timerID = NULL; }
	if (timerInterval)	{ timeEndPeriod(timerInterval);	timerInterval = NULL; }

	if (hicOutput) {
		ICDrawStop(hicOutput);
		ICDrawFlush(hicOutput);
		ICDrawEnd(hicOutput);
		ICClose(hicOutput);
		hicOutput = NULL;
	}

	if (pVideoPacker) {
		_RPT0(0,"Dub: Ending frame compression.\n");

		delete pVideoPacker;

		pVideoPacker = NULL;
	}

	if (pdsInput) {
		_XRPT0(0,"Dub: Destroying input overlay.\n");

#ifdef ASYNCHRONOUS_OVERLAY_SUPPORT
		blitter->postAFC(0x80000000, DestroyIDDrawSurface, (void *)pdsInput);
		pdsInput = NULL;
		fSkipDXShutdown = true;
#else
		delete pdsInput;
		pdsInput = NULL;
#endif
	}

	_XRPT0(0,"Dub: Deallocating resources.\n"); DEBUG_SLEEP;

	if (pipe)			{ delete pipe; pipe = NULL; }
	if (blitter)		{ delete blitter; blitter=NULL; }

	GdiFlush();

	filters.DeinitFilters();

	if (fVDecompressionOk)	{ vSrc->streamEnd(); }
	if (fADecompressionOk)	{ aSrc->streamEnd(); }

	if (hEventAbortOk)	{ CloseHandle(hEventAbortOk); hEventAbortOk = NULL; }
	if (inputBuffer)	{ freemem(inputBuffer); inputBuffer = NULL; }
	if (audioBuffer)	{ freemem(audioBuffer); audioBuffer = NULL; }

	if (audioCorrector)			{ delete audioCorrector; audioCorrector = NULL; }
	if (audioCompressor)		{ delete audioCompressor; audioCompressor = NULL; }
	if (audioSubsetFilter)		{ delete audioSubsetFilter; audioSubsetFilter = NULL; }
	if (audioStreamAmplifier)	{ delete audioStreamAmplifier; audioStreamAmplifier = NULL; }
	if (audioStreamResampler)	{ delete audioStreamResampler; audioStreamResampler = NULL; }
	if (audioStreamConverter)	{ delete audioStreamConverter; audioStreamConverter = NULL; }
	if (audioStreamSource)		{ delete audioStreamSource; audioStreamSource = NULL; }

	if (inputSubsetAlloc)		{ delete inputSubsetAlloc; inputSubsetAlloc = NULL; }

	_XRPT0(0,"Dub: Releasing display elements.\n"); DEBUG_SLEEP;

	if (inputHisto)				{ delete inputHisto; inputHisto = NULL; }
	if (outputHisto)			{ delete outputHisto; outputHisto = NULL; }

	if (hDDInput)				{ DrawDibClose(hDDInput); hDDInput = NULL; }
	if (hDDOutput)				{ DrawDibClose(hDDOutput); hDDOutput = NULL; }

	// deinitialize DirectDraw

	_XRPT0(0,"Dub: Deinitializing DirectDraw.\n");

	if (pdsOutput)	delete pdsOutput;	pdsOutput = NULL;

	if (!fSkipDXShutdown)	DDrawDeinitialize();

	// A pile of **** to Microsoft for a buggy CreateDIBSection().
	//
	// Seems that if you provide a handle to a file mapping section with
	// an offset >=64K under NT4, it fails to unmap the view, resulting
	// in a memory leak if you don't do it yourself...
	//
	// This bug was fixed in Windows 98 and NT5.

	if (hbmInput) {
		SelectObject(hdcCompatInput, hbmInputOld);
		DeleteObject(hbmInput);
		UnmapViewOfFile(lpvInput);
		hbmInput = NULL;
	}
	if (hbmOutput) {
		SelectObject(hdcCompatOutput, hbmOutputOld);
		DeleteObject(hbmOutput);
		UnmapViewOfFile(lpvOutput);
		hbmOutput = NULL;
	}

	if (hdcCompatInput)			{ DeleteDC(hdcCompatInput); hdcCompatInput = NULL; }
	if (hdcCompatOutput)		{ DeleteDC(hdcCompatOutput); hdcCompatOutput = NULL; }

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

	//_RPT0(0,"Dub: Stop complete.\n");
//	OutputDebugString("Dub: Stop complete.\n");

#ifdef STOP_SPEED_DEBUGGING
	__asm {
		rdtsc
		mov dword ptr stop_time+0,eax
		mov dword ptr stop_time+4,edx
	}

	{
		char buf[128];

		wsprintf(buf, "braking time: %d ms\n", (int)((stop_time - start_time)/300000000));
		OutputDebugString(buf);
	}
#endif
}

///////////////////////////////////////////////////////////////////

static long g_lPulseClock;

void CALLBACK Dubber::DubFrameTimerProc(UINT uID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2) {
	Dubber *thisPtr = (Dubber *)dwUser;

	if (thisPtr->opt->video.fSyncToAudio) {
		long lActualPoint;

		lActualPoint = ((AVIAudioPreviewOutputStream *)thisPtr->AVIout->audioOut)->getPosition();

		if (!((AVIAudioPreviewOutputStream *)thisPtr->AVIout->audioOut)->isFrozen()) {
			if (thisPtr->opt->video.nPreviewFieldMode) {
				g_lPulseClock = MulDiv(lActualPoint, 2000, thisPtr->vInfo.usPerFrame);

				g_lPulseClock += thisPtr->nVideoLagNoTelecine*2;
			} else {
				g_lPulseClock = MulDiv(lActualPoint, 1000, thisPtr->vInfo.usPerFrame);

				g_lPulseClock += thisPtr->nVideoLagNoTelecine;
			}

			if (g_lPulseClock<0)
				g_lPulseClock = 0;

			if (lActualPoint != -1) {
				thisPtr->blitter->setPulseClock(g_lPulseClock);
				thisPtr->fSyncToAudioEvenClock = false;
				return;
			}
		}

		// Hmm... we have no clock!

		if (thisPtr->fSyncToAudioEvenClock || thisPtr->opt->video.nPreviewFieldMode) {
			if (thisPtr->blitter) {
				thisPtr->blitter->pulse();
			}
			++thisPtr->iFrameDisplacement;
		}

		thisPtr->fSyncToAudioEvenClock = !thisPtr->fSyncToAudioEvenClock;

		return;
	}


	if (thisPtr->blitter) thisPtr->blitter->pulse();

#if 0
	if (thisPtr->blitter) {
		if (timeGetTime() - thisPtr->timer_counter >= thisPtr->timer_period) {
			thisPtr->timer_counter += thisPtr->timer_period;
			thisPtr->blitter->pulse();
		}
	}
#endif
}

int Dubber::PulseCallbackProc(void *_thisPtr, DWORD framenum) {
#if 1
	return AsyncBlitter::PCR_OKAY;
#else
	Dubber *thisPtr = (Dubber *)_thisPtr;
	long lAudioPoint1, lAudioPoint2, lActualPoint;

	lAudioPoint1 = MulDiv(framenum, thisPtr->vInfo.usPerFrame, 1000);
	lAudioPoint2 = MulDiv(framenum+3, thisPtr->vInfo.usPerFrame, 1000);

	lActualPoint = ((AVIAudioPreviewOutputStream *)thisPtr->AVIout->audioOut)->getPosition();

	if (lActualPoint <= 0) return AsyncBlitter::PCR_OKAY;

//	_RPT3(0,"%d %d %d\n", lAudioPoint1, lActualPoint, lAudioPoint2);

	if      (lActualPoint <  lAudioPoint1)	return AsyncBlitter::PCR_WAIT;
//	else if (lActualPoint >= lAudioPoint2)	return AsyncBlitter::PCR_NOBLIT;
	else									return AsyncBlitter::PCR_OKAY;
#endif
}

/////////////////////////////////////////////////////////////

void Dubber::ResizeInputBuffer(long bufsize) {
	inputBufferSize = (bufsize+65535) & 0xFFFF0000L;

	if (!(inputBuffer = reallocmem(inputBuffer, inputBufferSize)))
		throw "Error reallocating input buffer.\n";
}

void Dubber::ResizeAudioBuffer(long bufsize) {
	audioBufferSize = (bufsize+65535) & 0xFFFF0000L;

	if (!(audioBuffer = reallocmem(audioBuffer, audioBufferSize)))
		throw "Error reallocating audio buffer.\n";
}

void Dubber::ReadVideoFrame(long lVStreamPos, BOOL preload) {
	LONG lActualBytes;
	int hr;

	void *buffer;
	int handle;

	LONG lSize;

	if (fPhantom) {
		buffer = pipe->getWriteBuffer(0, &handle, INFINITE);
		if (!buffer) return;	// hmm, aborted...

		pipe->postBuffer(0, lVStreamPos,
			(vSrc->isKey(lVStreamPos) ? 0 : 1)
			+(preload ? 2 : 0),
			handle);

		return;
	}

//	_RPT2(0,"Reading frame %ld (%s)\n", lVStreamPos, preload ? "preload" : "process");

	hr = vSrc->read(lVStreamPos, 1, NULL, 0x7FFFFFFF, &lSize, NULL);
	if (hr) throw MyAVIError("Dub/IO-Video", hr);

	// Add 4 bytes -- otherwise, we can get crashes with uncompressed video because
	// the bitmap routines expect to be able to read 4 bytes out.

	buffer = pipe->getWriteBuffer(lSize+4, &handle, INFINITE);
	if (!buffer) return;	// hmm, aborted...

	hr = vSrc->read(lVStreamPos, 1, buffer, lSize,	&lActualBytes,NULL); 
	if (hr) throw MyAVIError("Dub/IO-Video", hr);

	if (opt->video.mode > DubVideoOptions::M_NONE)
		i64SegmentSize += lVideoSizeEstimate;
	else
		i64SegmentSize += lActualBytes + (lActualBytes&1);

	i64SegmentSize += 24;

	pipe->postBuffer(lActualBytes, lVStreamPos,
		(vSrc->isKey(lVStreamPos) ? 0 : 1)
		+(preload ? 2 : 0),
//		+((lVStreamPos % opt->video.frameRateDecimation) || preload ? 2 : 0),
		handle);


}

void Dubber::ReadNullVideoFrame(long lVStreamPos) {
	void *buffer;
	int handle;

	buffer = pipe->getWriteBuffer(1, &handle, INFINITE);
	if (!buffer) return;	// hmm, aborted...

	pipe->postBuffer(0, lVStreamPos,
		(vSrc->isKey(lVStreamPos) ? 0 : 1),
//		+((lVStreamPos % opt->video.frameRateDecimation)? 2 : 0),
		handle);

	i64SegmentSize += 24 + lVideoSizeEstimate;

//	_RPT0(0,"posted.\n");
}

long Dubber::ReadAudio(long& lAStreamPos, long samples) {
	LONG lActualBytes=0;
	LONG lActualSamples=0;

	void *buffer;
	int handle;
	long len;

	LONG ltActualBytes, ltActualSamples;
	char *destBuffer;

	if (audioCompressor) {
		void *holdBuffer;
		LONG lSrcSamples;

		holdBuffer = audioCompressor->Compress(samples, &lSrcSamples, &lActualBytes, &lActualSamples);

		if (audioCorrector)
			audioCorrector->Process(holdBuffer, lActualBytes);

		buffer = pipe->getWriteBuffer(lActualBytes, &handle, INFINITE);
		if (!buffer) return 0;

		memcpy(buffer, holdBuffer, lActualBytes);

		lAStreamPos += lSrcSamples;
	} else {
		len = samples * audioStream->GetFormat()->nBlockAlign;
		buffer = pipe->getWriteBuffer(len, &handle, INFINITE);
		if (!buffer) return 0; // aborted

		destBuffer = (char *)buffer;

		do {
			ltActualSamples = audioStream->Read(destBuffer, samples, &ltActualBytes);

			lActualBytes += ltActualBytes;
			lActualSamples += ltActualSamples;

			samples -= ltActualSamples;
			destBuffer += ltActualBytes;
			len -= ltActualBytes;
			lAStreamPos += ltActualSamples;
		} while(samples && ltActualBytes);
	}

	pipe->postBuffer(lActualBytes, lActualSamples, -1, handle);

	aInfo.total_size += lActualBytes + 24;
	i64SegmentSize += lActualBytes + 24;

	return lActualBytes;
}


//////////////////////

void Dubber::NextSegment() {
	char szFile[MAX_PATH];
	bool fVideo;
	bool fAudio;
	AVIOutputFile *AVIout_new;

	pipe->sync();

	fVideo = !!AVIout->videoOut;
	fAudio = !!AVIout->audioOut;

	((AVIOutputFile *)AVIout)->setSegmentHintBlock(false, NULL, 1);

	if (!AVIout->finalize())
		throw MyError("Error finalizing avi segment");

	AVIout_new = new AVIOutputFile();
	if (!AVIout_new)
		throw MyMemoryError();

	try {
		AVIout_new->disable_extended_avi();
		AVIout_new->disable_os_caching();

		AVIout_new->setSegmentHintBlock(true, NULL, 1);

		sprintf(szFile, "%s.%02d.avi", pszSegmentPrefix, nSpillSegment++);

		if (!AVIout_new->initOutputStreams())
			throw MyMemoryError();

		if (fVideo && vSrc) {
			int l;

			AVIout_new->videoOut->setCompressed(TRUE);
			memcpy(&AVIout_new->videoOut->streamInfo, &AVIout->videoOut->streamInfo, sizeof AVIout->videoOut->streamInfo);
			if (!(AVIout_new->videoOut->allocFormat(l = AVIout->videoOut->getFormatLen())))
				throw MyMemoryError();

			memcpy(AVIout_new->videoOut->getFormat(), AVIout->videoOut->getFormat(), l);
		}
		if (fAudio && aSrc) {
			int l;

			memcpy(&AVIout_new->audioOut->streamInfo, &AVIout->audioOut->streamInfo, sizeof AVIout->audioOut->streamInfo);
			if (!(AVIout_new->audioOut->allocFormat(l = AVIout->audioOut->getFormatLen())))
				throw MyMemoryError();

			memcpy(AVIout_new->audioOut->getFormat(), AVIout->audioOut->getFormat(), l);
		}

		if (!AVIout_new->init(szFile, 
					fVideo ? vSrc ? compressorVideoFormat->bmiHeader.biWidth : 320 : 0, //filters.OutputBitmap()->w,
					fVideo ? vSrc ? compressorVideoFormat->bmiHeader.biHeight : 240 : 0, //filters.OutputBitmap()->h,
					!!vSrc,
					!!aSrc,
					opt->perf.outputBufferSize,
					opt->audio.enabled))
			throw MyError("Problem initializing AVI output.");
	} catch(MyError) {
		delete AVIout_new;
		throw;
	}

	if (AVIout)
		delete AVIout;

	AVIout = AVIout_new;

	lSegmentFrameStart = lSpillVideoPoint;
	i64SegmentSize = 0;
	i64SegmentCredit = 0;
	lSpillVideoPoint = lSpillAudioPoint = 0;
	lSpillVideoOk = lSpillAudioOk = 0;
}

void Dubber::CheckSpill(long videopt, long audiopt) {
	long lFrame;
	long lFrame2;
	long lSample;

	// Are the new values still below the last computed 'safe' thresholds?

	if (videopt <= lSpillVideoOk && audiopt <= lSpillAudioOk)
		return;

	// Find out how many sync'ed video frames we'd be pushing ahead.

	__int64 nAdditionalBytes;

	lFrame = ((videopt-vInfo.start_src) / opt->video.frameRateDecimation);

	if (aSrc) {
		const __int64 nBlockAlignMillion = audioTimingStream->GetFormat()->nBlockAlign * 1000000i64;
		const __int64 nAvgBytesPerSecSpeed = (__int64)vInfo.usPerFrameNoTelecine * audioTimingStream->GetFormat()->nAvgBytesPerSec;

		// <audio samples> * <audio bytes per sample> / <audio bytes per second> = <seconds>
		// <seconds> * 1000000 / <microseconds per frame> = <frames>
		// (<audio samples> * <audio bytes per sample> * 1000000) / (<audio bytes per second> * <microseconds per frame>) = <frames>

		lFrame2 = int64divroundup((__int64)(audiopt - aInfo.start_src) * nBlockAlignMillion,
					nAvgBytesPerSecSpeed);

		if (pInvTelecine)
			lFrame2 += nVideoLag;

		if (lFrame2 > lFrame)
			lFrame = lFrame2;

		// Quantize to 5 frames in inverse telecine mode.

		lFrame2 = lFrame;
		if (pInvTelecine) {
			lFrame += 4;
			lFrame -= lFrame % 5;
			lFrame2 = lFrame - 10;
		}

		// Find equivalent audio point.
		//
		// (<audio samples> = (<frames> * <audio bytes per second> * <microseconds per frame>) / (<audio bytes per sample> * 1000000);

		lSample = int64divround((__int64)lFrame2 * nAvgBytesPerSecSpeed, nBlockAlignMillion);
	} else
		lFrame2 = lFrame;

	// Figure out how many more bytes it would be.

	if (opt->video.mode)
		nAdditionalBytes = (__int64)lVideoSizeEstimate * (vInfo.start_src + lFrame2 * opt->video.frameRateDecimation - vInfo.cur_src + nVideoLag);
	else {
		HRESULT hr;
		LONG lSize = 0;
		long lSamp = vInfo.cur_src;
		long lSampLimit = vInfo.start_src + lFrame2 * opt->video.frameRateDecimation + nVideoLag;

		nAdditionalBytes = 0;

		while(lSamp < lSampLimit) {
			hr = vSrc->read(lSamp, 1, NULL, 0x7FFFFFFF, &lSize, NULL);
			if (!hr)
				nAdditionalBytes += lSize;
			lSamp += opt->video.frameRateDecimation;
		}
	}

	if (aSrc && lSample > aInfo.cur_src)
		nAdditionalBytes += (lSample - aInfo.cur_src) * audioTimingStream->GetFormat()->nBlockAlign;

	if (nAdditionalBytes + (i64SegmentSize - i64SegmentCredit) < i64SegmentThreshold && (!lSegmentFrameLimit || lFrame-lSegmentFrameStart<=lSegmentFrameLimit)) {

		// We're fine.  Mark down the new thresholds so we don't have to recompute them.

		lFrame = lFrame * opt->video.frameRateDecimation + vInfo.start_src;

		if (aSrc)
			lSample += aInfo.start_src;

		_RPT4(0,"Pushing threshold to %ld, %ld: current position %ld, %ld\n", lFrame, lSample, vInfo.cur_src, aInfo.cur_src);

		lSpillVideoOk = lFrame;

		if (aSrc)
			lSpillAudioOk = lSample;

		return;
	}

	// Doh!  Force a split at the current thresholds.

	lSpillVideoPoint = lSpillVideoOk;

	if (aSrc)
		lSpillAudioPoint = lSpillAudioOk;

	_RPT4(0,"Forcing split at %ld, %ld: current position %ld, %ld\n", lSpillVideoPoint, lSpillAudioPoint, vInfo.cur_src, aInfo.cur_src);

	lFrame = lFrame * opt->video.frameRateDecimation + vInfo.start_src;
	lSpillVideoOk = lFrame;

	if (aSrc) {
		lSample += aInfo.start_src;
		lSpillAudioOk = lSample;
	}

	// Are we exactly at the right point?

	if (vInfo.cur_src == lSpillVideoPoint && (!aSrc || aInfo.cur_src == lSpillAudioPoint))
		NextSegment();
}

void Dubber::MainAddVideoFrame() {
	long f;
	BOOL is_preroll;

	if (vInfo.cur_src < vInfo.end_src + nVideoLag) {
		BOOL fRead = FALSE;
		long lFrame = vInfo.cur_src;

		// If we're doing segment spilling but don't have an audio stream,
		// break if we can't fit the next frame.

		if (fEnableSpill && !audioStream)
			if (i64SegmentSize - i64SegmentCredit >= i64SegmentThreshold)
				NextSegment();

		// If we're using an input subset, translate the frame.

		if (inputSubsetActive)
			lFrame = inputSubsetActive->lookupFrame(vInfo.cur_src) + vSrc->lSampleFirst;

		if (lFrame >= vSrc->lSampleFirst && lFrame < vSrc->lSampleLast) {
			if (opt->video.mode != DubVideoOptions::M_NONE) {
				long lSize;
				int nFrames;

				vSrc->streamSetDesiredFrame(lFrame);

				nFrames = vSrc->streamGetRequiredCount(&lSize);

				if (fEnableSpill)
					CheckSpill(vInfo.cur_src + opt->video.frameRateDecimation, aInfo.cur_src);

				if (!lSpillVideoPoint || vInfo.cur_src < lSpillVideoPoint) {
					while(-1 != (f = vSrc->streamGetNextRequiredFrame(&is_preroll))) {
						ReadVideoFrame(f, is_preroll && opt->video.mode>=DubVideoOptions::M_FASTREPACK);

						fRead = TRUE;
					}

					if (!fRead) ReadNullVideoFrame(lFrame);
				}
			} else {

				if (fEnableSpill)
					CheckSpill(vInfo.cur_src + opt->video.frameRateDecimation, aInfo.cur_src);

				if (!lSpillVideoPoint || vInfo.cur_src < lSpillVideoPoint)
					ReadVideoFrame(lFrame, FALSE);
			}
		} else {
			// Flushing out the lag -- read a null frame.

			ReadNullVideoFrame(lFrame);
		}
	}

	vInfo.cur_src += opt->video.frameRateDecimation;
}

void Dubber::MainAddAudioFrame(int lag) {
	long lAvgBytesPerSec = audioTimingStream->GetFormat()->nAvgBytesPerSec;
	long lBlockSize;
	LONG lAudioPoint;
	LONG lFrame = ((vInfo.cur_src-vInfo.start_src-lag) / opt->video.frameRateDecimation);

	// Per-frame interleaving?

	if (!opt->audio.is_ms || opt->audio.interval<=1) {
		if (opt->audio.interval > 1)
			lFrame = ((lFrame+opt->audio.interval-1)/opt->audio.interval)*opt->audio.interval;

		lAudioPoint = (long)(aInfo.start_src + aInfo.lPreloadSamples +
			(
				((__int64)lAvgBytesPerSec*(__int64)vInfo.usPerFrameNoTelecine*lFrame)
				/
				((__int64)1000000*audioTimingStream->GetFormat()->nBlockAlign)
			));

	} else {						// Per n-ms interleaving

		__int64 i64CurrentFrameMs;

		i64CurrentFrameMs = ((__int64)vInfo.usPerFrameNoTelecine * lFrame)/1000;

		// Round up lCurrentFrameMs to next interval

		i64CurrentFrameMs = ((i64CurrentFrameMs+opt->audio.interval-1)/opt->audio.interval)*opt->audio.interval;

		// nAvgBytesPerSec/nBlockAlign = samples per second

		lAudioPoint = aInfo.start_src + aInfo.lPreloadSamples +
				(LONG)((i64CurrentFrameMs * lAvgBytesPerSec) / (audioTimingStream->GetFormat()->nBlockAlign*1000));

	}

	// Round lAudioPoint to next block size if preview

	if (fPreview) {
		lBlockSize = (lAvgBytesPerSec / audioTimingStream->GetFormat()->nBlockAlign+4)/5;

		lAudioPoint += lBlockSize - 1;
		lAudioPoint -= lAudioPoint % lBlockSize;
	}

	if (lAudioPoint <= aInfo.cur_src)
		return;

	if (fEnableSpill)
		CheckSpill(vInfo.cur_src, lAudioPoint);

	if (lSpillAudioPoint && lAudioPoint > lSpillAudioPoint)
		lAudioPoint = lSpillAudioPoint;

	if (lAudioPoint > aInfo.cur_src)
		ReadAudio(aInfo.cur_src,lAudioPoint - aInfo.cur_src);

	_ASSERT(aInfo.cur_src <= lAudioPoint);
}

void Dubber::MainThreadKickstart(void *thisPtr) {
	InitThreadData("I/O processing");
	((Dubber *)thisPtr)->MainThread();
	DeinitThreadData();
}

void Dubber::MainThread() {

	///////////

	_XRPT0(0,"Dub/Main: Start.\n");

	InterlockedIncrement((LONG *)&lThreadsActive);

	try {

		DEFINE_SP(sp);

		// Preload audio before the first video frame.

//		_RPT1(0,"Before preload: %ld\n", aInfo.cur_src);

		if (aSrc) {
			aInfo.lPreloadSamples	= (long)(((__int64)opt->audio.preload * audioTimingStream->GetFormat()->nAvgBytesPerSec)/(1000 * audioTimingStream->GetFormat()->nBlockAlign));

			if (aInfo.lPreloadSamples>0) {
				_RPT1(0,"Dub/Main: Prewriting %ld samples\n", aInfo.lPreloadSamples);
				ReadAudio(aInfo.cur_src, aInfo.lPreloadSamples);
			}
		}

//		_RPT1(0,"After preload: %ld\n", aInfo.cur_src);

		// Do it!!!

		try {
			if (opt->audio.enabled && aSrc && vSrc && AVIout->videoOut) {
				LONG lStreamCounter = 0;

				_RPT0(0,"Dub/Main: Taking the **Interleaved** path.\n");

				while(!fAbort && (vInfo.cur_src<vInfo.end_src+nVideoLag || !audioStream->isEnd())) { 
					BOOL doAudio = TRUE;

					CHECK_STACK(sp);

					if (!lSpillVideoPoint || vInfo.cur_src < lSpillVideoPoint)
						MainAddVideoFrame();

					if ((!lSpillAudioPoint || aInfo.cur_src < lSpillAudioPoint) && audioStream && !audioStream->isEnd()) {
						MainAddAudioFrame(nVideoLag);
					}

					if (lSpillVideoPoint && vInfo.cur_src == lSpillVideoPoint && aInfo.cur_src == lSpillAudioPoint)
						NextSegment();

//					_RPT3(0,"segment size: %I64d - %I64d = %I64d\n", i64SegmentSize, i64SegmentCredit, i64SegmentSize - i64SegmentCredit);
				}

			} else {
				_RPT0(0,"Dub/Main: Taking the **Non-Interleaved** path.\n");

				if (aSrc)
					while(!fAbort && !audioStream->isEnd()) {
						ReadAudio(aInfo.cur_src, 1024); //8192);
					}

				if (vSrc && AVIout->videoOut)
					while(!fAbort && vInfo.cur_src < vInfo.end_src) { 
						BOOL fRead = FALSE;
						long lFrame = vInfo.cur_src;

						CHECK_STACK(sp);

						if (!lSpillVideoPoint || vInfo.cur_src < lSpillVideoPoint)
							MainAddVideoFrame();

						if (lSpillVideoPoint && vInfo.cur_src == lSpillVideoPoint)
							NextSegment();

					}
			}
		} catch(MyError e) {
			e.post(NULL, "Dub Error (will attempt to finalize)");
		}

		// wait for the pipeline to clear...

		if (!fAbort) pipe->finalize();

		// finalize the output.. if it's not a preview...

		if (!AVIout->isPreview()) {
			while(lThreadsActive>1) {
//			_RPT1(0,"\tDub/Main: %ld threads active\n", lThreadsActive);

				WaitForSingleObject(hEventAbortOk, 200);
			}

			_RPT0(0,"Dub/Main: finalizing...\n");

			// update audio rate...

			if (audioCorrector) {
				WAVEFORMATEX *wfex = AVIout->audioOut->getWaveFormat();
				
				wfex->nAvgBytesPerSec = audioCorrector->ComputeByterate(wfex->nSamplesPerSec);

				AVIout->audioOut->streamInfo.dwRate = wfex->nAvgBytesPerSec
					* AVIout->audioOut->streamInfo.dwScale;
			}

			// finalize avi

			if (!AVIout->finalize()) throw MyError("Error finalizing AVI!");
			_RPT0(0,"Dub/Main: finalized.\n");
		}

		// kill everyone else...

		fAbort = true;

	} catch(MyError e) {
//		e.post(NULL,"Dub Error");

		if (!fError) {
			err = e;
			e.discard();
			fError = true;
		}
		fAbort = TRUE;
	} catch(int) {
		;	// do nothing
	}

	// All done, time to get the pooper-scooper and clean up...

	hThreadMain = NULL;

	InterlockedDecrement((LONG *)&lThreadsActive);
	SetEvent(hEventAbortOk);

#ifdef _DEBUG
	_CrtCheckMemory();
#endif

	_XRPT0(0,"Dub/Main: End.\n");
}

///////////////////////////////////////////////////////////////////

#define BUFFERID_INPUT (1)
#define BUFFERID_OUTPUT (2)
#define BUFFERID_PACKED (4)

void Dubber::WriteVideoFrame(void *buffer, int exdata, LONG lastSize, long sample_num) {
	LONG dwBytes;
	bool isKey;
	void *frameBuffer;
	LPVOID lpCompressedData;

	// With Direct mode, write video data directly to output.

	if (opt->video.mode == DubVideoOptions::M_NONE || fPhantom) {

//		_RPT2(0,"Processing frame %ld (#%ld)\n", sample_num, vInfo.cur_proc_src+1);

		if (!AVIout->videoOut->write((exdata & 1) ? 0 : AVIIF_KEYFRAME, (char *)buffer, lastSize, 1))
			throw MyError("Error writing video frame.");

		vInfo.total_size += lastSize + 24;
		++vInfo.cur_proc_src;
		++vInfo.processed;

		pStatusHandler->NotifyNewFrame(lastSize | (exdata&1 ? 0x80000000 : 0));

		return;
	}

	// Fast Repack: Decompress data and send to compressor (possibly non-RGB).
	// Slow Repack: Decompress data and send to compressor.
	// Full:		Decompress, process, filter, convert, send to compressor.

	blitter->lock(BUFFERID_INPUT);

//	_RPT2(0,"Sample %ld (keyframe: %d)\n", sample_num, !(exdata &1));

	VDCHECKPOINT;
	CHECK_FPU_STACK
	vSrc->streamGetFrame(buffer, lastSize, !(exdata&1), FALSE, sample_num);
	CHECK_FPU_STACK
	VDCHECKPOINT;

//	guiSetStatus("Pulse clock: %ld, Delta: %ld\n", g_lPulseClock, blitter->getFrameDelta());

	if (exdata & 2) {
		blitter->unlock(BUFFERID_INPUT);
//		++vInfo.cur_proc_src;
		return;
	}

	if (lDropFrames && fPreview) {
		blitter->unlock(BUFFERID_INPUT);
		blitter->nextFrame();
		vInfo.cur_proc_src += opt->video.frameRateDecimation;
		++fsi.lCurrentFrame;
		--lDropFrames;

		pStatusHandler->NotifyNewFrame(0);

		return;
	}

	// Process frame to backbuffer for Full video mode.  Do not process if we are
	// running in Repack mode only!
	if (opt->video.mode == DubVideoOptions::M_FULL) {
		VBitmap *initialBitmap = filters.InputBitmap();
		VBitmap *lastBitmap = filters.LastBitmap();
		VBitmap *outputBitmap = filters.OutputBitmap();
		VBitmap destbm;
		long lInputFrameNum, lInputFrameNum2;

		lInputFrameNum = sample_num - vSrc->lSampleFirst;

		if (pInvTelecine) {
			lInputFrameNum2 = pInvTelecine->ProcessOut(initialBitmap);
			pInvTelecine->ProcessIn(&VBitmap(vSrc->getFrameBuffer(), vSrc->getDecompressedFormat()), lInputFrameNum);

			lInputFrameNum = lInputFrameNum2;

			if (lInputFrameNum < 0) {
				blitter->unlock(BUFFERID_INPUT);
				vInfo.cur_proc_src += opt->video.frameRateDecimation;
				return;
			}
		} else
			initialBitmap->BitBlt(0, 0, &VBitmap(vSrc->getFrameBuffer(), vSrc->getDecompressedFormat()), 0, 0, -1, -1);


		if (inputHisto) {
			inputHisto->Zero();
			inputHisto->Process(filters.InputBitmap());
		}

		// process frame

//		fsi.lCurrentFrame		= (sample_num - vInfo.start_src) / opt->video.frameRateDecimation;

		fsi.lCurrentSourceFrame	= lInputFrameNum;
		fsi.lSourceFrameMS		= MulDiv(fsi.lCurrentSourceFrame, fsi.lMicrosecsPerSrcFrame, 1000);
		fsi.lDestFrameMS		= MulDiv(fsi.lCurrentFrame, fsi.lMicrosecsPerFrame, 1000);

		CHECK_FPU_STACK

		filters.RunFilters();

		CHECK_FPU_STACK

		++fsi.lCurrentFrame;

		if (nVideoLagPreload>0) {
			--nVideoLagPreload;
			blitter->unlock(BUFFERID_INPUT);
			vInfo.cur_proc_src += opt->video.frameRateDecimation;
			return;
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


	if (fUseVideoCompression) {
/*		if (!(lpCompressedData = ICSeqCompressFrame(compVars, 0, frameBuffer, 
						&isKey, &dwBytes)))
			throw MyError("Error compressing video data.");*/

		if (hicOutput)
			blitter->lock(BUFFERID_PACKED);

		CHECK_FPU_STACK
		lpCompressedData = pVideoPacker->packFrame(frameBuffer, &isKey, &dwBytes);
		CHECK_FPU_STACK

		if (fShowDecompressedFrame && outputDecompressor && dwBytes) {
			DWORD err;
			VBitmap *outputBitmap = filters.OutputBitmap();
			Pixel *outputBuffer = outputBitmap->data;

//			memset(outputBuffer, 0, outputBitmap->size);

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

//					throw "Error decompressing output video frame.\n";
//					throw MyICError("Dub/Processor-Video (output)", err);

				fShowDecompressedFrame = false;
			VDCHECKPOINT;

			compressorVideoFormat->bmiHeader.biSizeImage = dwSize;

			CHECK_FPU_STACK
		}

		if (!AVIout->videoOut->write(isKey ? AVIIF_KEYFRAME : 0, (char *)lpCompressedData, dwBytes, 1))
			throw "Error writing video frame.";

	} else {

/*		if (!AVIout->videoOut->write(AVIIF_KEYFRAME, (char *)frameBuffer, filters.OutputBitmap()->size, 1))
			throw MyError("Error writing video frame.");

		dwBytes = filters.OutputBitmap()->size;*/

		VDCHECKPOINT;
		if (!AVIout->videoOut->write(AVIIF_KEYFRAME, (char *)frameBuffer, AVIout->videoOut->getImageFormat()->biSizeImage, 1))
			throw MyError("Error writing video frame.");
		VDCHECKPOINT;

		dwBytes = AVIout->videoOut->getImageFormat()->biSizeImage;
		isKey = true;
	}

	vInfo.total_size += dwBytes + 24;

	VDCHECKPOINT;

	if (okToDraw || fPreview) {
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
			} else if (hbmInput) {
				if (opt->video.nPreviewFieldMode)
					blitter->postBitBltLaced(
							BUFFERID_INPUT,
							hDCWindow,
							rInputFrame.left, rInputFrame.top,
							rInputFrame.right-rInputFrame.left, rInputFrame.bottom-rInputFrame.top,
							hdcCompatInput,
							0,0,
							opt->video.nPreviewFieldMode>=2
					);
				else
					blitter->postStretchBlt(
							BUFFERID_INPUT,
							hDCWindow,
							rInputFrame.left, rInputFrame.top,
							rInputFrame.right-rInputFrame.left, rInputFrame.bottom-rInputFrame.top,
							hdcCompatInput,
							0,0,
							vSrc->getDecompressedFormat()->biWidth,vSrc->getDecompressedFormat()->biHeight
					);
			} else if (hDDInput)
				blitter->post(
						BUFFERID_INPUT,
						hDDInput,
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

		if (hicOutput && lpCompressedData)
			blitter->postICDraw(BUFFERID_PACKED, hicOutput, isKey ? 0 : ICDRAW_NOTKEYFRAME, AVIout->videoOut->getImageFormat(), lpCompressedData, dwBytes, vInfo.processed);

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
			} else if (hbmOutput) {
				if (opt->video.nPreviewFieldMode)
					blitter->postBitBltLaced(
							BUFFERID_OUTPUT,
							hDCWindow,
							rOutputFrame.left, rOutputFrame.top,
							rOutputFrame.right-rOutputFrame.left, rOutputFrame.bottom-rOutputFrame.top,
							hdcCompatOutput,
							0,0,
							opt->video.nPreviewFieldMode>=2
					);
				else
					blitter->postStretchBlt(
							BUFFERID_OUTPUT,
							hDCWindow,
							rOutputFrame.left, rOutputFrame.top,
							rOutputFrame.right-rOutputFrame.left, rOutputFrame.bottom-rOutputFrame.top,
							hdcCompatOutput,
							0,0,
							filters.OutputBitmap()->w,
							filters.OutputBitmap()->h
					);
			} else if (hDDOutput)
				blitter->post(
						BUFFERID_OUTPUT,
						hDDOutput,
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

		--okToDraw;
	} else {
		blitter->unlock(BUFFERID_OUTPUT);
		blitter->unlock(BUFFERID_INPUT);
		blitter->unlock(BUFFERID_PACKED);
	}

//	guiSetStatus("Pulse clock: %ld\n", g_lPulseClock);

	if (opt->perf.fDropFrames && fPreview) {
		long lFrameDelta;

		lFrameDelta = blitter->getFrameDelta();

		if (opt->video.nPreviewFieldMode)
			lFrameDelta >>= 1;

//		guiSetStatus("Pulse clock: %ld, Delta: %ld\n", g_lPulseClock, lFrameDelta);

		if (lFrameDelta < 1) lFrameDelta = 1;
		
		if (lFrameDelta > 1) {
			lDropFrames = lFrameDelta/2;
		}

//		vInfo.cur_proc_src = opt->video.frameRateDecimation * lFrameDelta;
	}

	blitter->nextFrame(opt->video.nPreviewFieldMode ? 2 : 1);

	vInfo.cur_proc_src += opt->video.frameRateDecimation;
	++vInfo.processed;

	if (opt->video.mode)
		i64SegmentCredit += lVideoSizeEstimate - (dwBytes + (dwBytes&1));

	pStatusHandler->NotifyNewFrame(isKey ? dwBytes : dwBytes | 0x80000000);

	VDCHECKPOINT;

}

void Dubber::WriteAudio(void *buffer, long lActualBytes, long lActualSamples) {
	if (!lActualBytes) return;

	if (!AVIout->audioOut->write(AVIIF_KEYFRAME, (char *)buffer, lActualBytes, lActualSamples))
		throw MyError("Error writing audio data.");

	aInfo.cur_proc_src += lActualBytes;
}

void Dubber::ProcessingThreadKickstart(void *thisPtr) {
	InitThreadData("Processing");
	((Dubber *)thisPtr)->ProcessingThread();
	DeinitThreadData();
}

void Dubber::ProcessingThread() {
	BOOL quit = FALSE;
	BOOL firstPacket = TRUE;
	BOOL stillAudio = TRUE;

	lDropFrames = 0;
	vInfo.processed = 0;

	_RPT0(0,"Dub/Processor: start\n");

	InterlockedIncrement((LONG *)&lThreadsActive);

	try {
		DEFINE_SP(sp);

		do {
			void *buf;
			long len;
			long samples;
			int exdata;
			int handle;

			while(!fAbort && (buf = pipe->getReadBuffer(&len, &samples, &exdata, &handle, 1000))) {

				CHECK_STACK(sp);

				if (exdata<0) {
					WriteAudio(buf, len, samples);
					if (firstPacket && fPreview) {
						AVIout->audioOut->flush();
						blitter->enablePulsing(TRUE);
						firstPacket = FALSE;

						if (hicOutput)
							ICDrawStart(hicOutput);
					}

				} else {
					if (firstPacket && fPreview && !aSrc) {
						blitter->enablePulsing(TRUE);
						firstPacket = FALSE;
					}
					WriteVideoFrame(buf, exdata, len, samples);
				}
				pipe->releaseBuffer(handle);

				if (stillAudio && pipe->isNoMoreAudio()) {
					// HACK!! if it's a preview, flush the audio

					if (AVIout->isPreview()) {
						_RPT0(0,"Dub/Processor: flushing audio...\n");
						AVIout->audioOut->flush();
						_RPT0(0,"Dub/Processor: flushing audio....\n");
					}

					stillAudio = FALSE;
				}

			}
		} while(!fAbort && !pipe->isFinalized());
	} catch(MyError e) {
		if (!fError) {
			err = e;
			e.discard();
			fError = true;
		}
		pipe->abort();
		fAbort = TRUE;
	}

	pipe->isFinalized();

	// if preview mode, choke the audio

	if (AVIout->audioOut && AVIout->isPreview())
		((AVIAudioPreviewOutputStream *)AVIout->audioOut)->stop();

	_XRPT0(0,"Dub/Processor: end\n");

	hThreadProcessor = NULL;

	InterlockedDecrement((LONG *)&lThreadsActive);
	SetEvent(hEventAbortOk);
}

///////////////////////////////////////////////////////////////////

void Dubber::Abort() {
#ifdef STOP_SPEED_DEBUGGING
	__asm {
		rdtsc
		mov dword ptr start_time+0,eax
		mov dword ptr start_time+4,edx
	}
#endif

	fUserAbort = true;
	fAbort = true;
	PostMessage(g_hWnd, WM_USER, 0, 0);
}

bool Dubber::isAbortedByUser() {
	return fUserAbort;
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
	if (hDDOutput)
		DrawDibRealize(hDDOutput, hDCWindow, FALSE);
}

void Dubber::SetPriority(int index) {
	SetThreadPriority(hThreadMain, g_iPriorities[index][0]);
	SetThreadPriority(hThreadProcessor, g_iPriorities[index][1]);
}

void Dubber::UpdateFrames() {
	++okToDraw;
}