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

#include "dub.h"
#include "dubstatus.h"

//////////////

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

VDAVIOutputFileSystem::VDAVIOutputFileSystem()
	: mbAllowCaching(false)
	, mbAllowIndexing(true)
	, mbUse1GBLimit(false)
	, mCurrentSegment(0)
{
}

VDAVIOutputFileSystem::~VDAVIOutputFileSystem() {
}

void VDAVIOutputFileSystem::SetCaching(bool bAllowOSCaching) {
	mbAllowCaching = bAllowOSCaching;
}

void VDAVIOutputFileSystem::SetIndexing(bool bAllowHierarchicalExtensions) {
	mbAllowIndexing = bAllowHierarchicalExtensions;
}

void VDAVIOutputFileSystem::Set1GBLimit(bool bUse1GBLimit) {
	mbUse1GBLimit = bUse1GBLimit;
}

void VDAVIOutputFileSystem::SetBuffer(int bufferSize) {
	mBufferSize = bufferSize;
}

AVIOutput *VDAVIOutputFileSystem::CreateSegment() {
	vdautoptr<AVIOutputFile> pOutput(new AVIOutputFile);

	if (!mbAllowCaching)
		pOutput->disable_os_caching();

	if (!mbAllowIndexing)
		pOutput->disable_extended_avi();

	VDStringW s(mSegmentBaseName);

	if (mSegmentDigits) {
		s += VDFastTextPrintfW(L".%0*d", mSegmentDigits, mCurrentSegment++);
		VDFastTextFree();
		s += mSegmentExt;

		pOutput->setSegmentHintBlock(true, NULL, 1);
	}

	pOutput->initOutputStreams();

	if (!mVideoFormat.empty()) {
		memcpy(pOutput->videoOut->allocFormat(mVideoFormat.size()), &mVideoFormat[0], mVideoFormat.size());
		pOutput->videoOut->setCompressed(TRUE);
		pOutput->videoOut->streamInfo = mVideoStreamInfo;
	}

	if (!mAudioFormat.empty()) {
		memcpy(pOutput->audioOut->allocFormat(mAudioFormat.size()), &mAudioFormat[0], mAudioFormat.size());
		pOutput->audioOut->streamInfo = mAudioStreamInfo;
	}

	pOutput->init(VDTextWToA(s).c_str(), !mVideoFormat.empty(), !mAudioFormat.empty(), mBufferSize, mbInterleaved);

	return pOutput.release();
}

void VDAVIOutputFileSystem::CloseSegment(AVIOutput *pSegment, bool bLast) {
	AVIOutputFile *pFile = (AVIOutputFile *)pSegment;
	if (mSegmentDigits)
		pFile->setSegmentHintBlock(bLast, NULL, 1);
	pFile->finalize();
	delete pFile;
}

void VDAVIOutputFileSystem::SetFilename(const wchar_t *pszFilename) {
	mSegmentBaseName	= pszFilename;
	mSegmentDigits		= 0;
}

void VDAVIOutputFileSystem::SetFilenamePattern(const wchar_t *pszFilename, const wchar_t *pszExt, int nMinimumDigits) {
	mSegmentBaseName	= pszFilename;
	mSegmentExt			= pszExt;
	mSegmentDigits		= nMinimumDigits;
}

void VDAVIOutputFileSystem::SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat) {
	mVideoStreamInfo = asi;
	mVideoFormat.resize(cbFormat);
	memcpy(&mVideoFormat[0], pFormat, cbFormat);
}

void VDAVIOutputFileSystem::SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved) {
	mAudioStreamInfo = asi;
	mAudioFormat.resize(cbFormat);
	memcpy(&mAudioFormat[0], pFormat, cbFormat);
	mbInterleaved = bInterleaved;
}

bool VDAVIOutputFileSystem::AcceptsVideo() {
	return true;
}

bool VDAVIOutputFileSystem::AcceptsAudio() {
	return true;
}

///////////////////////////////////////////////////////////////////////////

VDAVIOutputStripedSystem::VDAVIOutputStripedSystem(const wchar_t *filename)
	: mbUse1GBLimit(false)
	, mpStripeSystem(new AVIStripeSystem(VDTextWToA(filename, -1).c_str()))
{
}

VDAVIOutputStripedSystem::~VDAVIOutputStripedSystem() {
}

void VDAVIOutputStripedSystem::Set1GBLimit(bool bUse1GBLimit) {
	mbUse1GBLimit = bUse1GBLimit;
}

AVIOutput *VDAVIOutputStripedSystem::CreateSegment() {
	vdautoptr<AVIOutputStriped> pFile(new AVIOutputStriped(mpStripeSystem));

	if (!pFile)
		throw MyMemoryError();

	pFile->set_1Gb_limit();
	return pFile.release();
}

void VDAVIOutputStripedSystem::CloseSegment(AVIOutput *pSegment, bool bLast) {
	AVIOutputFile *pFile = (AVIOutputFile *)pSegment;
	pFile->setSegmentHintBlock(bLast, NULL, 1);
	pFile->finalize();
	delete pFile;
}

void VDAVIOutputStripedSystem::SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat) {
	mVideoStreamInfo = asi;
	mVideoFormat.resize(cbFormat);
	memcpy(&mVideoFormat[0], pFormat, cbFormat);
}

void VDAVIOutputStripedSystem::SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved) {
	mAudioStreamInfo = asi;
	mAudioFormat.resize(cbFormat);
	memcpy(&mAudioFormat[0], pFormat, cbFormat);
}

bool VDAVIOutputStripedSystem::AcceptsVideo() {
	return true;
}

bool VDAVIOutputStripedSystem::AcceptsAudio() {
	return true;
}

///////////////////////////////////////////////////////////////////////////

VDAVIOutputWAVSystem::VDAVIOutputWAVSystem(const wchar_t *pszFilename)
	: mFilename(pszFilename)
{
}

VDAVIOutputWAVSystem::~VDAVIOutputWAVSystem() {
}

AVIOutput *VDAVIOutputWAVSystem::CreateSegment() {
	vdautoptr<AVIOutputWAV> pOutput(new AVIOutputWAV);

	pOutput->initOutputStreams();

	memcpy(pOutput->audioOut->allocFormat(mAudioFormat.size()), &mAudioFormat[0], mAudioFormat.size());

	pOutput->init(VDTextWToA(mFilename).c_str(), FALSE, !mAudioFormat.empty(), 65536, false);

	return pOutput.release();
}

void VDAVIOutputWAVSystem::CloseSegment(AVIOutput *pSegment, bool bLast) {
	AVIOutputWAV *pFile = (AVIOutputWAV *)pSegment;
	pFile->finalize();
	delete pFile;
}

void VDAVIOutputWAVSystem::SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat) {
}

void VDAVIOutputWAVSystem::SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved) {
	mAudioStreamInfo = asi;
	mAudioFormat.resize(cbFormat);
	memcpy(&mAudioFormat[0], pFormat, cbFormat);
}

bool VDAVIOutputWAVSystem::AcceptsVideo() {
	return false;
}

bool VDAVIOutputWAVSystem::AcceptsAudio() {
	return true;
}

///////////////////////////////////////////////////////////////////////////

VDAVIOutputImagesSystem::VDAVIOutputImagesSystem()
{
}

VDAVIOutputImagesSystem::~VDAVIOutputImagesSystem() {
}

AVIOutput *VDAVIOutputImagesSystem::CreateSegment() {
	vdautoptr<AVIOutputImages> pOutput(new AVIOutputImages(VDTextWToA(mSegmentPrefix).c_str(), VDTextWToA(mSegmentSuffix).c_str(), mSegmentDigits, mFormat));

	pOutput->initOutputStreams();

	if (!mVideoFormat.empty()) {
		memcpy(pOutput->videoOut->allocFormat(mVideoFormat.size()), &mVideoFormat[0], mVideoFormat.size());
		pOutput->videoOut->setCompressed(TRUE);
	}

	if (!mAudioFormat.empty())
		memcpy(pOutput->audioOut->allocFormat(mAudioFormat.size()), &mAudioFormat[0], mAudioFormat.size());

	pOutput->init(NULL, !mVideoFormat.empty(), !mAudioFormat.empty(), 0, false);

	return pOutput.release();
}

void VDAVIOutputImagesSystem::CloseSegment(AVIOutput *pSegment, bool bLast) {
	AVIOutputImages *pFile = (AVIOutputImages *)pSegment;
	pFile->finalize();
	delete pFile;
}

void VDAVIOutputImagesSystem::SetFilenamePattern(const wchar_t *pszPrefix, const wchar_t *pszSuffix, int nMinimumDigits) {
	mSegmentPrefix		= pszPrefix;
	mSegmentSuffix		= pszSuffix;
	mSegmentDigits		= nMinimumDigits;
}

void VDAVIOutputImagesSystem::SetFormat(int format) {
	mFormat = format;
}

void VDAVIOutputImagesSystem::SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat) {
	mVideoStreamInfo = asi;
	mVideoFormat.resize(cbFormat);
	memcpy(&mVideoFormat[0], pFormat, cbFormat);
}

void VDAVIOutputImagesSystem::SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved) {
	mAudioStreamInfo = asi;
	mAudioFormat.resize(cbFormat);
	memcpy(&mAudioFormat[0], pFormat, cbFormat);
}

bool VDAVIOutputImagesSystem::AcceptsVideo() {
	return true;
}

bool VDAVIOutputImagesSystem::AcceptsAudio() {
	return false;
}

///////////////////////////////////////////////////////////////////////////

VDAVIOutputPreviewSystem::VDAVIOutputPreviewSystem()
{
}

VDAVIOutputPreviewSystem::~VDAVIOutputPreviewSystem() {
}

AVIOutput *VDAVIOutputPreviewSystem::CreateSegment() {
	vdautoptr<AVIOutputPreview> pOutput(new AVIOutputPreview);

	pOutput->initOutputStreams();

	if (!mVideoFormat.empty()) {
		memcpy(pOutput->videoOut->allocFormat(mVideoFormat.size()), &mVideoFormat[0], mVideoFormat.size());
		pOutput->videoOut->setCompressed(TRUE);
	}

	if (!mAudioFormat.empty())
		memcpy(pOutput->audioOut->allocFormat(mAudioFormat.size()), &mAudioFormat[0], mAudioFormat.size());

	pOutput->init(NULL, !mVideoFormat.empty(), !mAudioFormat.empty(), 0, false);

	return pOutput.release();
}

void VDAVIOutputPreviewSystem::CloseSegment(AVIOutput *pSegment, bool bLast) {
	AVIOutputPreview *pFile = (AVIOutputPreview *)pSegment;
	pFile->finalize();
	delete pFile;
}

void VDAVIOutputPreviewSystem::SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat) {
	mVideoStreamInfo = asi;
	mVideoFormat.resize(cbFormat);
	memcpy(&mVideoFormat[0], pFormat, cbFormat);
}

void VDAVIOutputPreviewSystem::SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved) {
	mAudioStreamInfo = asi;
	mAudioFormat.resize(cbFormat);
	memcpy(&mAudioFormat[0], pFormat, cbFormat);
}

bool VDAVIOutputPreviewSystem::AcceptsVideo() {
	return true;
}

bool VDAVIOutputPreviewSystem::AcceptsAudio() {
	return true;
}

/////////////////////////////////////////////////

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

/////////////////////////////////////////////////

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

/////////////////////////////////////////////////

extern const char g_szError[];
extern BOOL g_syncroBlit, g_vertical;
extern HWND g_hWnd;
extern HINSTANCE g_hInst;
extern bool g_fWine;

///////////////////////////////////////////////////////////////////////////

class Dubber : public IDubber, protected VDThread, protected IVDTimerCallback {
private:
	void TimerCallback();

	void ReadVideoFrame(long stream_frame, long display_frame, bool preload);
	void ReadNullVideoFrame(long lVStreamPos);
	long ReadAudio(long& lAStreamPos, long samples);
	void CheckSpill(long videopt, long audiopt);
	void NextSegment();
	void MainAddVideoFrame();
	void MainAddAudioFrame(int);
	static void MainThreadKickstart(void *thisPtr);
	void MainThread();
	void WriteVideoFrame(void *buffer, int exdata, int droptype, LONG lastSize, long sample_num, long display_num);
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

	VDSignal			mEventAbortOk;
	VDAtomicInt			mThreadsActive;
	HANDLE				hThreadMain;

	VideoSequenceCompressor	*pVideoPacker;

	AVIPipe *			pipe;
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
	AudioCompressor		*audioCompressor;
	vdautoptr<VDAudioFilterGraph> mpAudioFilterGraph;

	FrameSubset				*inputSubsetActive;
	FrameSubset				*inputSubsetAlloc;
	VideoTelecineRemover	*pInvTelecine;
	int					nVideoLag;
	int					nVideoLagNoTelecine;
	int					nVideoLagPreload;

	VDFormatStruct<WAVEFORMATEX> mAudioCompressionFormat;

	Histogram			*inputHisto, *outputHisto;

	VDAtomicInt			mRefreshFlag;
	HWND				hwndStatus;

	bool				fSyncToAudioEvenClock;
	bool				mbAudioFrozen;
	bool				mbAudioFrozenValid;

	int					iPriority;
	long				lDropFrames;

	FilterStateInfo		fsi;

	bool				fPhantom;

	IDubStatusHandler	*pStatusHandler;

	uint32				mSegmentSizeIVTCCounter;		// We omit every 5th size addition with IVTC.  This is a hack that really needs to go!

	sint64				i64SegmentSize;
	volatile sint64	i64SegmentCredit;
	sint64				i64SegmentThreshold;
	long				lVideoSizeEstimate;
	bool				fEnableSpill;
	int					nSpillSegment;
	long				lSpillVideoPoint, lSpillAudioPoint;
	long				lSpillVideoOk, lSpillAudioOk;
	long				lSegmentFrameLimit;
	long				lSegmentFrameStart;

	// This is used to keep track of the last video frame when decimating in DSC mode.
	long				mDSCLastVideoFrame;

	volatile int		mFramesDelayedByCodec;
	volatile int		mFramesPushedToFlushCodec;

	///////

	enum {
		kBufferFlagDelta		= 1,
		kBufferFlagPreload		= 2
	};

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
	, mpAudioFilterGraph(NULL)
	, mSegmentSizeIVTCCounter(0)
	, mFramesDelayedByCodec(0)
	, mFramesPushedToFlushCodec(0)
	, mStopLock(0)
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

	mThreadsActive		= 0;
	hThreadMain			= NULL;
	pipe				= NULL;
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
	audioCompressor		= NULL;
	audioCorrector		= NULL;

	inputSubsetActive	= NULL;
	inputSubsetAlloc	= NULL;

	inputHisto			= NULL;
	outputHisto			= NULL;

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

	mDSCLastVideoFrame = -1;

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
	nSpillSegment = 1;
	i64SegmentThreshold = segsize;
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

		VDFraction framerate(video->streamInfo.dwRate, video->streamInfo.dwScale);

		if (opt->video.frameRateNewMicroSecs == DubVideoOptions::FR_SAMELENGTH) {
			if (audio && audio->streamInfo.dwLength) {
				framerate = VDFraction::reduce64(audio->samplesToMs(audio->streamInfo.dwLength) * (sint64)1000, video->streamInfo.dwLength);
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
		vInfo.frameRate		= framerate / opt->video.frameRateDecimation;

		vInfo.usPerFrameIn	= (long)vInfo.frameRateIn.scale64ir(1000000);
		vInfo.usPerFrame	= (long)vInfo.frameRate.scale64ir(1000000);

		// make sure we start reading on a key frame

		if (opt->video.mode == DubVideoOptions::M_NONE)
			vInfo.start_src	= video->nearestKey(vInfo.start_src);

		vInfo.cur_src		= vInfo.start_src;
		vInfo.cur_dst		= vInfo.start_dst;
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
		aInfo.cur_dst		= aInfo.start_dst;
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
		audioStream = new AudioFilterSystemStream(*mpAudioFilterGraph, aInfo.start_us);
		if (!audioStream)
			throw MyMemoryError();

		mAudioStreams.push_back(audioStream);
	} else {
		// First, create a source.

		if (!(audioStream = new AudioStreamSource(aSrc, aInfo.start_src, aSrc->lSampleLast - aInfo.start_src, opt->audio.mode > DubAudioOptions::M_NONE)))
			throw MyError("Dub: Unable to create audio stream source");

		mAudioStreams.push_back(audioStream);

		// Attach a converter if we need to...

		if (aInfo.converting) {
			if (aInfo.single_channel)
				audioStream = new AudioStreamConverter(audioStream, aInfo.is_16bit, aInfo.is_right, true);
			else
				audioStream = new AudioStreamConverter(audioStream, aInfo.is_16bit, aInfo.is_stereo, false);

			if (!audioStream)
				throw MyError("Dub: Unable to create audio stream converter");

			mAudioStreams.push_back(audioStream);
		}

		// Attach a converter if we need to...

		if (aInfo.resampling) {
			if (!(audioStream = new AudioStreamResampler(audioStream, opt->audio.new_rate ? opt->audio.new_rate : aSrc->getWaveFormat()->nSamplesPerSec, opt->audio.integral_rate, opt->audio.fHighQuality)))
				throw MyError("Dub: Unable to create audio stream resampler");

			mAudioStreams.push_back(audioStream);
		}

		// Attach an amplifier if needed...

		if (opt->audio.mode > DubAudioOptions::M_NONE && opt->audio.volume) {
			if (!(audioStream = new AudioStreamAmplifier(audioStream, opt->audio.volume)))
				throw MyError("Dub: Unable to create audio stream amplifier");

			mAudioStreams.push_back(audioStream);
		}
	}

	// Tack on a subset filter as well...

	if (inputSubsetActive) {
		sint64 offset = 0;
		
		if (opt->audio.fStartAudio)
			offset = vInfo.frameRateIn.scale64ir((sint64)1000000 * vInfo.start_src);

		if (!(audioStream = new AudioSubset(audioStream, inputSubsetActive, vInfo.frameRateIn, offset)))
			throw MyError("Dub: Unable to create audio subset filter");

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

	if (opt->audio.mode > DubAudioOptions::M_NONE && !mAudioCompressionFormat.empty()) {
		if (!(audioCompressor = new AudioCompressor(audioStream, &*mAudioCompressionFormat, mAudioCompressionFormat.size())))
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
		hdr.dwLength		= (vInfo.end_src - vInfo.start_src) / opt->video.frameRateDecimation;

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

	vInfo.nLag = 0;
	vInfo.frameRateNoTelecine = vInfo.frameRate;
	vInfo.usPerFrameNoTelecine = vInfo.usPerFrame;
	if (opt->video.mode >= DubVideoOptions::M_FULL && opt->video.fInvTelecine) {
		vInfo.frameRate = vInfo.frameRate * VDFraction(4, 5);
		vInfo.usPerFrame = (long)vInfo.frameRate.scale64ir(1000000);
	}

	lSpillVideoOk = vInfo.cur_src;
	lSpillAudioOk = aInfo.cur_src;


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

	nVideoLag = nVideoLagNoTelecine = 0;

	if (vSrc) {
		BITMAPINFOHEADER *bmih = vSrc->getDecompressedFormat();

		filters.initLinearChain(&g_listFA, (Pixel *)(bmih+1), bmih->biWidth, bmih->biHeight, 32 /*bmih->biBitCount*/, iOutputDepth);

		fsi.lMicrosecsPerFrame		= vInfo.usPerFrame;
		fsi.lMicrosecsPerSrcFrame	= vInfo.usPerFrameIn;
		fsi.lCurrentFrame			= 0;

		if (filters.ReadyFilters(&fsi))
			throw "Error readying filters.";

		fFiltersOk = true;

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

	if (vSrc && mpOutputSystem->AcceptsVideo()) {

		VDDEBUG("Dub: Initializing input decompressor.\n");

		vSrc->streamBegin(fPreview);
		fVDecompressionOk = true;

	}

	// Initialize audio.

	VDDEBUG("Dub: Initializing audio.\n");

	if (aSrc && mpOutputSystem->AcceptsAudio())
		InitAudioConversionChain();

	// Initialize output file.

	InitOutputFile();

	// Initialize input window display.

	if (vSrc && mpOutputSystem->AcceptsVideo())
		InitDisplay();

	// Create a pipe.

	VDDEBUG("Dub: Creating data pipe.\n");

	if (!(pipe = new AVIPipe(opt->perf.pipeBufferCount, 16384)) || !pipe->isOkay())
		throw MyError("Couldn't create pipe");
}

void Dubber::Go(int iPriority) {
	OSVERSIONINFO ovi;

	// check the version.  if NT, don't touch the processing priority!

	ovi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

	fNoProcessingPriority = GetVersionEx(&ovi) && ovi.dwPlatformId == VER_PLATFORM_WIN32_NT;

	if (!iPriority)
		iPriority = fNoProcessingPriority || !mpOutputSystem->IsRealTime() ? 5 : 6;

	this->iPriority = iPriority;

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

	// Wait for the thread count to increment to 1.  Otherwise, it's possible for
	// one of the later initialization events to fail, and the end() function to
	// be called before the first thread is even scheduled.  If this happens,
	// the thread will be left dangling.

	while(!mThreadsActive) Sleep(100);

	// Continue with other threads.

	if (!(hThreadMain = (HANDLE)_beginthread(MainThreadKickstart, 0, (void *)this)))
		throw "Couldn't create main dubbing thread";

	SetThreadPriority(hThreadMain, g_iPriorities[iPriority-1][1]);

	// Create status window during the dub.

	VDDEBUG("Dub: Creating status window.\n");

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

	VDDEBUG("Dub: Beginning stop process.\n");

	if (pipe)
		pipe->abort();

	if (blitter)
		blitter->flush();

	VDDEBUG("Dub: Killing threads.\n");

	fAbort = true;
	while(mThreadsActive) {
		DWORD dwRes;
		HANDLE hAbort = mEventAbortOk.getHandle();

		dwRes = MsgWaitForMultipleObjects(1, &hAbort, FALSE, 10000, QS_ALLINPUT);

		if (WAIT_OBJECT_0+1 == dwRes)
			guiDlgMessageLoop(hwndStatus);
		else if (WAIT_TIMEOUT == dwRes) {
			if (IDOK == MessageBox(g_hWnd, "Something appears to be stuck while trying to stop (thread deadlock). Abort operation and exit program?", "VirtualDub Internal Error", MB_ICONEXCLAMATION|MB_OKCANCEL)) {
				vdprotected("aborting process due to a thread deadlock") {
					ExitProcess(0);
				}
			}
		}

VDDEBUG("\tDub: Threads still active\n");

		VDDEBUG("\tDub: %ld threads active\n", mThreadsActive);

#ifdef _DEBUG
		if (blitter) VDDEBUG("\t\tBlitter locks active: %08lx\n", blitter->lock_state);
#endif
	}

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

	if (pipe)			{ delete pipe; pipe = NULL; }
	if (blitter)		{ delete blitter; blitter=NULL; }

	GdiFlush();

	filters.DeinitFilters();

	if (fVDecompressionOk)	{ vSrc->streamEnd(); }
	if (fADecompressionOk)	{ aSrc->streamEnd(); }

	if (audioCorrector)			{ delete audioCorrector; audioCorrector = NULL; }
	if (audioCompressor)		{ delete audioCompressor; audioCompressor = NULL; }

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

				mPulseClock += nVideoLagNoTelecine*2;
			} else {
				mPulseClock = MulDiv(lActualPoint, 1000, vInfo.usPerFrame);

				mPulseClock += nVideoLagNoTelecine;
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

/////////////////////////////////////////////////////////////

void Dubber::ReadVideoFrame(long stream_frame, long display_frame, bool preload) {
	LONG lActualBytes;
	int hr;

	void *buffer;
	int handle;

	LONG lSize;

	if (fPhantom) {
		buffer = pipe->getWriteBuffer(0, &handle, INFINITE);
		if (!buffer) return;	// hmm, aborted...

		bool bIsKey = !!vSrc->isKey(display_frame);

		pipe->postBuffer(0, stream_frame, display_frame,
			(bIsKey ? 0 : kBufferFlagDelta)
			+(preload ? kBufferFlagPreload : 0),
			0,
			handle);

		return;
	}

//	VDDEBUG("Reading frame %ld (%s)\n", lVStreamPos, preload ? "preload" : "process");

	hr = vSrc->read(stream_frame, 1, NULL, 0x7FFFFFFF, &lSize, NULL);
	if (hr) {
		if (hr == AVIERR_FILEREAD)
			throw MyError("Video frame %d could not be read from the source. The file may be corrupt.", stream_frame);
		else
			throw MyAVIError("Dub/IO-Video", hr);
	}

	// Add 4 bytes -- otherwise, we can get crashes with uncompressed video because
	// the bitmap routines expect to be able to read 4 bytes out.

	buffer = pipe->getWriteBuffer(lSize+4, &handle, INFINITE);
	if (!buffer) return;	// hmm, aborted...

	hr = vSrc->read(stream_frame, 1, buffer, lSize,	&lActualBytes,NULL); 
	if (hr) {
		if (hr == AVIERR_FILEREAD)
			throw MyError("Video frame %d could not be read from the source. The file may be corrupt.", stream_frame);
		else
			throw MyAVIError("Dub/IO-Video", hr);
	}

	if (!preload) {
		if (!pInvTelecine || mSegmentSizeIVTCCounter<4) {
			if (opt->video.mode > DubVideoOptions::M_NONE)
				i64SegmentSize += lVideoSizeEstimate;
			else
				i64SegmentSize += lActualBytes + (lActualBytes&1);

			i64SegmentSize += 24;
		}
		if (++mSegmentSizeIVTCCounter >= 5)
			mSegmentSizeIVTCCounter = 0;
	}

	display_frame = vSrc->streamToDisplayOrder(stream_frame);

	pipe->postBuffer(lActualBytes, stream_frame, display_frame,
		(vSrc->isKey(display_frame) ? 0 : kBufferFlagDelta)
		+(preload ? kBufferFlagPreload : 0),
		vSrc->getDropType(display_frame),
		handle);


}

void Dubber::ReadNullVideoFrame(long display_frame) {
	void *buffer;
	int handle;

	buffer = pipe->getWriteBuffer(1, &handle, INFINITE);
	if (!buffer) return;	// hmm, aborted...

	if (display_frame >= 0) {
		pipe->postBuffer(0, vSrc->displayToStreamOrder(display_frame), display_frame,
			(vSrc->isKey(display_frame) ? 0 : kBufferFlagDelta),
			vSrc->getDropType(display_frame),
			handle);
	} else {
		pipe->postBuffer(0, display_frame, display_frame,
			0,
			AVIPipe::kDroppable,
			handle);
	}

	if (!pInvTelecine || mSegmentSizeIVTCCounter<4)
		i64SegmentSize += 24 + lVideoSizeEstimate;

	if (++mSegmentSizeIVTCCounter >= 5)
		mSegmentSizeIVTCCounter = 0;

//	VDDEBUG("posted.\n");
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

	pipe->postBuffer(lActualBytes, lActualSamples, 0, -1, 0, handle);

	aInfo.total_size += lActualBytes + 24;
	i64SegmentSize += lActualBytes + 24;

	return lActualBytes;
}


//////////////////////

void Dubber::NextSegment() {
	pipe->sync();

	vdautoptr<AVIOutput> AVIout_new(mpOutputSystem->CreateSegment());
	mpOutputSystem->CloseSegment(AVIout, false);
	AVIout = AVIout_new.release();

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

	sint64 nAdditionalBytes;

	lFrame = ((videopt-vInfo.start_src) / opt->video.frameRateDecimation);

	if (audioStream) {
		const WAVEFORMATEX *const pFormat = audioTimingStream->GetFormat();
		const sint32 nBlockAlign	= pFormat->nBlockAlign;
		const sint32 nDataRate		= pFormat->nAvgBytesPerSec;
		const VDFraction frAudioPerVideo(VDFraction(nDataRate, nBlockAlign) / vInfo.frameRateNoTelecine);

		// <audio samples> * <audio bytes per sample> / <audio bytes per second> = <seconds>
		// <seconds> * 1000000 / <microseconds per frame> = <frames>
		// (<audio samples> * <audio bytes per sample> * 1000000) / (<audio bytes per second> * <microseconds per frame>) = <frames>

		lFrame2 = (long)frAudioPerVideo.scale64ir(audiopt - aInfo.start_src);

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

		lSample = (long)frAudioPerVideo.scale64r(lFrame2);
	} else
		lFrame2 = lFrame;

	// Figure out how many more bytes it would be.

	if (opt->video.mode)
		nAdditionalBytes = (sint64)lVideoSizeEstimate * (vInfo.start_src + lFrame2 * opt->video.frameRateDecimation - vInfo.cur_src + nVideoLag);
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

	if (audioStream && lSample > aInfo.cur_src)
		nAdditionalBytes += (lSample - aInfo.cur_src) * audioTimingStream->GetFormat()->nBlockAlign;

	if (nAdditionalBytes + (i64SegmentSize - i64SegmentCredit) < i64SegmentThreshold && (!lSegmentFrameLimit || lFrame-lSegmentFrameStart<=lSegmentFrameLimit)) {

		// We're fine.  Mark down the new thresholds so we don't have to recompute them.

		lFrame = lFrame * opt->video.frameRateDecimation + vInfo.start_src;

		if (audioStream)
			lSample += aInfo.start_src;

		VDDEBUG("Pushing threshold to %ld, %ld: current position %ld, %ld\n", lFrame, lSample, vInfo.cur_src, aInfo.cur_src);

		lSpillVideoOk = lFrame;

		if (audioStream)
			lSpillAudioOk = lSample;

		return;
	}

	// Doh!  Force a split at the current thresholds.

	lSpillVideoPoint = lSpillVideoOk;

	if (audioStream)
		lSpillAudioPoint = lSpillAudioOk;

	VDASSERT(lSpillVideoPoint >= vInfo.cur_src);
	VDASSERT(lSpillAudioPoint >= aInfo.cur_src);

	VDDEBUG("Forcing split at %ld, %ld: current position %ld, %ld\n", lSpillVideoPoint, lSpillAudioPoint, vInfo.cur_src, aInfo.cur_src);

	lFrame = lFrame * opt->video.frameRateDecimation + vInfo.start_src;
	lSpillVideoOk = lFrame;

	if (audioStream) {
		lSample += aInfo.start_src;
		lSpillAudioOk = lSample;
	}

	// Are we exactly at the right point?

	if (vInfo.cur_src == lSpillVideoPoint && (!audioStream || aInfo.cur_src == lSpillAudioPoint))
		NextSegment();
}

void Dubber::MainAddVideoFrame() {
	long f;
	BOOL is_preroll;

	if (vInfo.cur_src < vInfo.end_src + nVideoLag) {
		bool fRead = false;
		long lFrame = vInfo.cur_src;

		// If we're doing segment spilling but don't have an audio stream,
		// break if we can't fit the next frame.

		if (fEnableSpill && !audioStream)
			if (i64SegmentSize - i64SegmentCredit >= i64SegmentThreshold)
				NextSegment();

		// If we're using an input subset, translate the frame.

		if (vInfo.cur_src >= vInfo.end_src)
			lFrame = -1;		// force null read
		else if (inputSubsetActive)
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
						ReadVideoFrame(f, lFrame, is_preroll && opt->video.mode>=DubVideoOptions::M_FASTREPACK);

						fRead = true;
					}

					if (!fRead)
						ReadNullVideoFrame(lFrame);
				}
			} else {

				if (fEnableSpill)
					CheckSpill(vInfo.cur_src + opt->video.frameRateDecimation, aInfo.cur_src);

				if (!lSpillVideoPoint || vInfo.cur_src < lSpillVideoPoint) {

					// If we just copied the last frame, drop in a null frame.

					if (mDSCLastVideoFrame == lFrame)
						ReadNullVideoFrame(lFrame);
					else {
						// In DSC mode, we may not be able to copy the desired frames due to
						// frame dependencies.  In that case, copy sequential frames from the
						// last keyframe.

						if (opt->video.frameRateDecimation > 1) {
							long lKey = vSrc->nearestKey(lFrame);

							if (lKey > mDSCLastVideoFrame)
								mDSCLastVideoFrame = lKey;
							else
								++mDSCLastVideoFrame;

							lFrame = mDSCLastVideoFrame;
						}

						ReadVideoFrame(lFrame, lFrame, false);
						mDSCLastVideoFrame = lFrame;
					}
				}
			}
		} else {
			// Flushing out the lag -- read a null frame.

			ReadNullVideoFrame(lFrame);
		}
	} else if (mFramesPushedToFlushCodec < mFramesDelayedByCodec) {
		ReadNullVideoFrame(-1);
		++mFramesPushedToFlushCodec;
	}

	vInfo.cur_src += opt->video.frameRateDecimation;
}

void Dubber::MainAddAudioFrame(int lag) {
	long lAvgBytesPerSec = audioTimingStream->GetFormat()->nAvgBytesPerSec;
	long lBlockSize;
	LONG lAudioPoint;
	LONG lFrame = ((vInfo.cur_src-vInfo.start_src-lag) / opt->video.frameRateDecimation);

	// If IVTC is active, round up to a multiple of five.

	if (pInvTelecine) {
		lFrame += 4;
		lFrame -= lFrame % 5;
	}

	// Per-frame interleaving?

	if (!opt->audio.is_ms || opt->audio.interval<=1) {
		if (opt->audio.interval > 1)
			lFrame = ((lFrame+opt->audio.interval-1)/opt->audio.interval)*opt->audio.interval;

		const VDFraction audioRate(lAvgBytesPerSec, audioTimingStream->GetFormat()->nBlockAlign);
		const VDFraction audioPerVideo(audioRate / vInfo.frameRateNoTelecine);

		lAudioPoint = (long)(aInfo.start_src + aInfo.lPreloadSamples + audioPerVideo.scale64r(lFrame));

	} else {						// Per n-ms interleaving

		sint64 i64CurrentFrameMs;

		i64CurrentFrameMs = ((sint64)vInfo.usPerFrameNoTelecine * lFrame)/1000;

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

	if (lAudioPoint > aInfo.cur_src) {
		ReadAudio(aInfo.cur_src, lAudioPoint - aInfo.cur_src);
	}

	VDASSERT(aInfo.cur_src <= lAudioPoint);
}

void Dubber::MainThreadKickstart(void *thisPtr) {
	VDInitThreadData("I/O processing");
	((Dubber *)thisPtr)->MainThread();
	VDDeinitThreadData();
}

void Dubber::MainThread() {

	///////////

	VDDEBUG("Dub/Main: Start.\n");

	++mThreadsActive;

	try {

		DEFINE_SP(sp);

		// Preload audio before the first video frame.

//		VDDEBUG("Before preload: %ld\n", aInfo.cur_src);

		if (audioStream) {
			aInfo.lPreloadSamples	= (long)(((sint64)opt->audio.preload * audioTimingStream->GetFormat()->nAvgBytesPerSec)/(1000 * audioTimingStream->GetFormat()->nBlockAlign));

			if (aInfo.lPreloadSamples>0) {
				VDDEBUG("Dub/Main: Prewriting %ld samples\n", aInfo.lPreloadSamples);
				ReadAudio(aInfo.cur_src, aInfo.lPreloadSamples);
			}
		}

//		VDDEBUG("After preload: %ld\n", aInfo.cur_src);

		// Do it!!!

		try {
			if (opt->audio.enabled && audioStream && vSrc && mpOutputSystem->AcceptsVideo()) {
				LONG lStreamCounter = 0;

				VDDEBUG("Dub/Main: Taking the **Interleaved** path.\n");

				while(!fAbort && (vInfo.cur_src<vInfo.end_src+nVideoLag || !audioStream->isEnd() || mFramesPushedToFlushCodec < mFramesDelayedByCodec)) { 
					bool doAudio = true;

					CHECK_STACK(sp);

					if (!lSpillVideoPoint || vInfo.cur_src < lSpillVideoPoint)
						MainAddVideoFrame();

					if ((!lSpillAudioPoint || aInfo.cur_src < lSpillAudioPoint) && audioStream && !audioStream->isEnd()) {
						MainAddAudioFrame(nVideoLag + mFramesDelayedByCodec);
					}

					if (lSpillVideoPoint && vInfo.cur_src == lSpillVideoPoint && aInfo.cur_src == lSpillAudioPoint)
						NextSegment();

//					VDDEBUG("segment size: %I64d - %I64d = %I64d\n", i64SegmentSize, i64SegmentCredit, i64SegmentSize - i64SegmentCredit);
				}

			} else {
				VDDEBUG("Dub/Main: Taking the **Non-Interleaved** path.\n");

				if (audioStream)
					while(!fAbort && !audioStream->isEnd()) {
						ReadAudio(aInfo.cur_src, 1024); //8192);
					}

				if (vSrc && mpOutputSystem->AcceptsVideo())
					while(!fAbort && (vInfo.cur_src < vInfo.end_src+nVideoLag || mFramesPushedToFlushCodec < mFramesDelayedByCodec)) { 
						bool fRead = false;
						long lFrame = vInfo.cur_src;

						CHECK_STACK(sp);

						if (!lSpillVideoPoint || vInfo.cur_src < lSpillVideoPoint)
							MainAddVideoFrame();

						if (lSpillVideoPoint && vInfo.cur_src == lSpillVideoPoint)
							NextSegment();

					}
			}
		} catch(MyError& e) {
			if (!fError) {
				err.TransferFrom(e);
				fError = true;
			}
//			e.post(NULL, "Dub Error (will attempt to finalize)");
		}

		// wait for the pipeline to clear...

		if (!fAbort) pipe->finalize();

		// finalize the output.. if it's not a preview...

		if (!mpOutputSystem->IsRealTime()) {
			while(mThreadsActive>1) {
//			VDDEBUG("\tDub/Main: %ld threads active\n", mThreadsActive);

				WaitForSingleObject(mEventAbortOk.getHandle(), 200);
			}

			VDDEBUG("Dub/Main: finalizing...\n");

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
			VDDEBUG("Dub/Main: finalized.\n");
		}

		// kill everyone else...

		fAbort = true;

	} catch(MyError& e) {
//		e.post(NULL,"Dub Error");

		if (!fError) {
			err.TransferFrom(e);
			fError = true;
		}
		fAbort = true;
	} catch(int) {
		;	// do nothing
	}

	// All done, time to get the pooper-scooper and clean up...

	hThreadMain = NULL;

	--mThreadsActive;
	mEventAbortOk.signal();

#ifdef _DEBUG
	_CrtCheckMemory();
#endif

	VDDEBUG("Dub/Main: End.\n");
}

///////////////////////////////////////////////////////////////////

#define BUFFERID_INPUT (1)
#define BUFFERID_OUTPUT (2)
#define BUFFERID_PACKED (4)

void Dubber::WriteVideoFrame(void *buffer, int exdata, int droptype, LONG lastSize, long sample_num, long display_num) {
	LONG dwBytes;
	bool isKey;
	void *frameBuffer;
	LPVOID lpCompressedData;

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

				pipe->getDropDistances(total, indep);

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
				vInfo.cur_proc_src += opt->video.frameRateDecimation;
			}
			++fsi.lCurrentFrame;
			if (lDropFrames)
				--lDropFrames;

			pStatusHandler->NotifyNewFrame(0);

			return;
		}
	}

	// With Direct mode, write video data directly to output.

	if (opt->video.mode == DubVideoOptions::M_NONE || fPhantom) {
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

	VDCHECKPOINT;
	CHECK_FPU_STACK
	vSrc->streamGetFrame(buffer, lastSize, !(exdata&kBufferFlagDelta), false, sample_num);
	CHECK_FPU_STACK

	VDCHECKPOINT;

	if (exdata & 2) {
		blitter->unlock(BUFFERID_INPUT);
		return;
	}

	if (lDropFrames && fPreview) {
		blitter->unlock(BUFFERID_INPUT);
		blitter->nextFrame(opt->video.nPreviewFieldMode ? 2 : 1);
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

		lInputFrameNum = display_num - vSrc->lSampleFirst;

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

		fsi.lCurrentSourceFrame	= lInputFrameNum;
		fsi.lSourceFrameMS		= (long)vInfo.frameRateIn.scale64ir(fsi.lCurrentSourceFrame * (sint64)1000);
		fsi.lDestFrameMS		= (long)vInfo.frameRateIn.scale64ir(fsi.lCurrentFrame * (sint64)1000);

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


	if (pVideoPacker) {
		CHECK_FPU_STACK
		lpCompressedData = pVideoPacker->packFrame(frameBuffer, &isKey, &dwBytes);
		CHECK_FPU_STACK

		// Check if codec buffered a frame.

		if (!lpCompressedData) {
			++mFramesDelayedByCodec;
			return;
		}

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

//	guiSetStatus("Pulse clock: %ld\n", g_lPulseClock);
	if (opt->perf.fDropFrames && fPreview) {
		long lFrameDelta;

		lFrameDelta = blitter->getFrameDelta();

		if (opt->video.nPreviewFieldMode)
			lFrameDelta >>= 1;

//		guiSetStatus("Pulse clock: %ld, Delta: %ld\n", 255, g_lPulseClock, lFrameDelta);

		if (lFrameDelta < 0) lFrameDelta = 0;
		
		if (lFrameDelta > 0) {
			lDropFrames = lFrameDelta;
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

void Dubber::ThreadRun() {
	bool quit = false;
	bool firstPacket = true;
	bool stillAudio = true;

	lDropFrames = 0;
	vInfo.processed = 0;

	VDDEBUG("Dub/Processor: start\n");

	++mThreadsActive;

	try {
		DEFINE_SP(sp);

		do {
			void *buf;
			long len;
			long samples;
			long dframe;
			int exdata;
			int handle;
			int droptype;

			while(!fAbort && (buf = pipe->getReadBuffer(&len, &samples, &dframe, &exdata, &droptype, &handle, 1000))) {

				CHECK_STACK(sp);

				if (exdata<0) {
					WriteAudio(buf, len, samples);
					if (firstPacket && fPreview) {
						AVIout->audioOut->flush();
						blitter->enablePulsing(true);
						firstPacket = false;
						mbAudioFrozen = false;
					}

				} else {
					if (firstPacket && fPreview && !aSrc) {
						blitter->enablePulsing(true);
						firstPacket = false;
					}
					WriteVideoFrame(buf, exdata, droptype, len, samples, dframe);

					if (fPreview && aSrc) {
						((AVIAudioPreviewOutputStream *)AVIout->audioOut)->start();
						mbAudioFrozenValid = true;
					}
				}
				pipe->releaseBuffer(handle);

				if (stillAudio && pipe->isNoMoreAudio()) {
					// HACK!! if it's a preview, flush the audio

					if (AVIout->isPreview()) {
						VDDEBUG("Dub/Processor: flushing audio...\n");
						AVIout->audioOut->flush();
						VDDEBUG("Dub/Processor: flushing audio....\n");
					}

					stillAudio = false;
				}

			}
		} while(!fAbort && !pipe->isFinalized());
	} catch(MyError& e) {
		if (!fError) {
			err.TransferFrom(e);
			fError = true;
		}
		pipe->abort();
		fAbort = true;
	}

	pipe->isFinalized();

	// if preview mode, choke the audio

	if (AVIout && mpOutputSystem->AcceptsAudio() && mpOutputSystem->IsRealTime())
		((AVIAudioPreviewOutputStream *)AVIout->audioOut)->stop();

	VDDEBUG("Dub/Processor: end\n");

	--mThreadsActive;
	mEventAbortOk.signal();
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
	SetThreadPriority(hThreadMain, g_iPriorities[index][0]);
	SetThreadPriority(getThreadHandle(), g_iPriorities[index][1]);
}

void Dubber::UpdateFrames() {
	mRefreshFlag = 1;
}