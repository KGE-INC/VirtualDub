//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
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

#include <malloc.h>
#include <ctype.h>
#include <process.h>

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <vfw.h>
#include <shellapi.h>

#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/int128.h>
#include <vd2/system/registry.h>
#include <vd2/system/thread.h>
#include <vd2/system/profile.h>
#include <vd2/system/tls.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/w32assist.h>
#include "AVIOutput.h"
#include "AVIOutputFile.h"
#include "Histogram.h"
#include "FilterSystem.h"
#include "AVIOutputStriped.h"
#include "AVIStripeSystem.h"
#include "VideoSequenceCompressor.h"
#include <vd2/Dita/services.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/bitmap.h>
#include <vd2/Riza/capdriver.h>
#include <vd2/Riza/capdrivers.h>
#include <vd2/Riza/capresync.h>
#include <vd2/Riza/capaudiocomp.h>

#include "crash.h"
#include "gui.h"
#include "oshelper.h"
#include "filters.h"
#include "capture.h"
#include "helpfile.h"
#include "resource.h"
#include "prefs.h"
#include "misc.h"
#include "capspill.h"
#include "caphisto.h"
#include "optdlg.h"
#include "filtdlg.h"
#include "capaccel.h"
#include "caputils.h"
#include "capfilter.h"
#include "capvumeter.h"
#include "uiframe.h"

using namespace nsVDCapture;

///////////////////////////////////////////////////////////////////////////
//
//	externs
//
///////////////////////////////////////////////////////////////////////////

extern HINSTANCE g_hInst;
extern const char g_szError[];
extern List g_listFA;
extern long g_lSpillMinSize;
extern long g_lSpillMaxSize;
extern HWND			g_hWnd;

extern void CaptureBT848Reassert();
extern void FreeCompressor(COMPVARS *pCompVars);

IVDCaptureSystem *VDCreateCaptureSystemEmulation();

///////////////////////////////////////////////////////////////////////////
//
//	structs
//
///////////////////////////////////////////////////////////////////////////

#define VDCM_EXIT		(WM_APP+0)
#define VDCM_SWITCH_FIN (WM_APP+1)

class VDCaptureProject;

class VDCaptureStatsFilter : public IVDCaptureDriverCallback {
public:
	VDCaptureStatsFilter();

	void Init(IVDCaptureDriverCallback *pCB, const WAVEFORMATEX *pwfex);
	void GetStats(VDCaptureStatus& stats);

	void CapBegin(sint64 global_clock);
	void CapEnd(const MyError *pError);
	bool CapControl(bool is_preroll);
	void CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock);

protected:
	IVDCaptureDriverCallback *mpCB;

	long		mVideoFramesCaptured;
	long		mVideoFirstCapTime, mVideoLastCapTime;
	long		mAudioFirstCapTime, mAudioLastCapTime;
	sint32		mAudioFirstSize;
	sint64		mTotalAudioDataSize;

	double		mAudioSamplesPerByteX1000;

	// audio sampling rate estimation
	VDCaptureAudioRateEstimator	mAudioRateEstimator;

	VDCriticalSection	mcsLock;
};

VDCaptureStatsFilter::VDCaptureStatsFilter()
	: mpCB(NULL)
{
}

void VDCaptureStatsFilter::Init(IVDCaptureDriverCallback *pCB, const WAVEFORMATEX *pwfex) {
	mpCB = pCB;

	mAudioSamplesPerByteX1000 = 0.0;
	if (pwfex)
		mAudioSamplesPerByteX1000 = 1000.0 * ((double)pwfex->nSamplesPerSec / (double)pwfex->nAvgBytesPerSec);
}

void VDCaptureStatsFilter::GetStats(VDCaptureStatus& status) {
	vdsynchronized(mcsLock) {
		status.mVideoFirstFrameTimeMS	= mVideoFirstCapTime;
		status.mVideoLastFrameTimeMS	= mVideoLastCapTime;
		status.mAudioFirstFrameTimeMS	= mAudioFirstCapTime;
		status.mAudioLastFrameTimeMS	= mAudioLastCapTime;
		status.mAudioFirstSize			= mAudioFirstSize;
		status.mTotalAudioDataSize		= mTotalAudioDataSize;

		// slope is in (bytes/ms), which we must convert to samples/sec.
		double slope;
		status.mActualAudioHz = 0;
		if (mAudioRateEstimator.GetSlope(slope))
			status.mActualAudioHz = slope * mAudioSamplesPerByteX1000;
	}
}

void VDCaptureStatsFilter::CapBegin(sint64 global_clock) {
	mVideoFramesCaptured	= 0;
	mVideoFirstCapTime		= 0;
	mVideoLastCapTime		= 0;
	mAudioFirstCapTime		= 0;
	mAudioLastCapTime		= 0;
	mAudioFirstSize			= 0;
	mTotalAudioDataSize		= 0;

	mAudioRateEstimator.Reset();

	if (mpCB)
		mpCB->CapBegin(global_clock);
}

void VDCaptureStatsFilter::CapEnd(const MyError *pError) {
	if (mpCB)
		mpCB->CapEnd(pError);
}

bool VDCaptureStatsFilter::CapControl(bool is_preroll) {
	if (mpCB)
		return mpCB->CapControl(is_preroll);

	return true;
}

void VDCaptureStatsFilter::CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock) {
	vdsynchronized(mcsLock) {
		if (stream == 0) {
			uint32 lTimeStamp = (uint32)(timestamp / 1000);

			if (!mVideoFramesCaptured)
				mVideoFirstCapTime = lTimeStamp;
			mVideoLastCapTime = lTimeStamp;

			++mVideoFramesCaptured;
		} else if (stream == 1) {
			uint32 dwTime = (uint32)(global_clock / 1000);

			if (!mAudioFirstSize) {
				mAudioFirstSize = size;
				mAudioFirstCapTime = dwTime;
			}
			mAudioLastCapTime = dwTime;

			mTotalAudioDataSize += size;
			mAudioRateEstimator.AddSample(dwTime, mTotalAudioDataSize);
		}
	}

	if (mpCB)
		mpCB->CapProcessData(stream, data, size, timestamp, key, global_clock);
}

class VDCaptureData : public VDThread {
public:
	VDCaptureProject	*mpProject;
	WAVEFORMATEX	mwfex;
	sint32		mTotalJitter;
	sint32		mTotalDisp;
	sint64		mTotalVideoSize;
	sint64		mTotalAudioSize;
	sint64		mLastVideoSize;
	sint64		mDiskFreeBytes;
	long		mTotalFramesCaptured;
	uint32		mFramesDropped;
	uint32		mFramePeriod;
	uint64		mLastUpdateTime;
	long		mLastTime;
	long		mUncompressedFrameSize;
	int			mSegmentIndex;

	vdautoptr<VideoSequenceCompressor> mpVideoCompressor;
	IVDMediaOutput			*volatile mpOutput;
	IVDMediaOutputAVIFile	*volatile mpOutputFile;
	IVDMediaOutputAVIFile	*volatile mpOutputFilePending;
	IVDMediaOutputStream	*volatile mpVideoOut;
	IVDMediaOutputStream	*volatile mpAudioOut;
	int				mAudioSampleSize;
	uint32			mLastCapturedFrame;
	vdautoptr<MyError>		mpError;
	VDAtomicPtr<MyError>	mpSpillError;
	const wchar_t	*mpszFilename;
	const wchar_t	*mpszPath;
	const wchar_t	*mpszNewPath;
	sint64			mSegmentAudioSize;
	sint64			mSegmentVideoSize;
	sint64			mAudioBlocks;
	sint64			mAudioSwitchPt;
	sint64			mVideoBlocks;
	sint64			mVideoSwitchPt;
	long			mSizeThreshold;
	long			mSizeThresholdPending;

	VDCriticalSection	mCallbackLock;

	VDCaptureStatsFilter	*mpStatsFilter;
	IVDCaptureResyncFilter	*mpResyncFilter;

	VDCaptureTimingSetup	mTimingSetup;

	bool			mbDoSwitch;
	bool			mbAllFull;
	bool			mbNTSC;

	IVDCaptureFilterSystem *mpFilterSys;
	VDPixmapLayout			mInputLayout;

	VDStringW		mCaptureRoot;

	VDRTProfileChannel	mVideoProfileChannel;
	VDRTProfileChannel	mAudioProfileChannel;

	VDCaptureData();
	~VDCaptureData();

	void PostFinalizeRequest() {
		PostThreadMessage(getThreadID(), VDCM_SWITCH_FIN, 0, 0);
	}
	void PostExitRequest() {
		PostThreadMessage(getThreadID(), VDCM_EXIT, 0, 0);
	}

	bool VideoCallback(const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock);
	bool WaveCallback(const void *data, uint32 size, sint64 global_clock);
protected:
	void ThreadRun();
	void CreateNewFile();
	void FinalizeOldFile();
	void DoSpill();
	void CheckVideoAfter();
};

VDCaptureData::VDCaptureData()
	: VDThread("Capture spill")
	, mTotalJitter(0)
	, mTotalDisp(0)
	, mTotalVideoSize(0)
	, mTotalAudioSize(0)
	, mLastVideoSize(0)
	, mDiskFreeBytes(0)
	, mTotalFramesCaptured(0)
	, mFramesDropped(0)
	, mFramePeriod(0)
	, mLastUpdateTime(0)
	, mLastTime(0)
	, mUncompressedFrameSize(0)
	, mSegmentIndex(0)
	, mpVideoCompressor(NULL)
	, mpOutput(NULL)
	, mpOutputFile(NULL)
	, mpOutputFilePending(NULL)
	, mpVideoOut(NULL)
	, mpAudioOut(NULL)
	, mAudioSampleSize(0)
	, mLastCapturedFrame(0)
	, mpError(NULL)
	, mpSpillError(NULL)
	, mpszFilename(NULL)
	, mpszPath(NULL)
	, mpszNewPath(NULL)
	, mSegmentAudioSize(0)
	, mSegmentVideoSize(0)
	, mAudioBlocks(0)
	, mAudioSwitchPt(0)
	, mVideoBlocks(0)
	, mVideoSwitchPt(0)
	, mSizeThreshold(0)
	, mSizeThresholdPending(0)
	, mbDoSwitch(0)
	, mbAllFull(0)
	, mbNTSC(0)
	, mpFilterSys(NULL)
	, mVideoProfileChannel("Video Output")
	, mAudioProfileChannel("Audio Output")
{
}

VDCaptureData::~VDCaptureData() {
	delete mpSpillError;
}

#if 0
extern LONG __stdcall CrashHandler(EXCEPTION_POINTERS *pExc);
#define CAPINT_FATAL_CATCH_START	\
		__try {

#define CAPINT_FATAL_CATCH_END(msg)	\
		} __except(CrashHandler((EXCEPTION_POINTERS*)_exception_info()), 1) {		\
		}
#else
#define CAPINT_FATAL_CATCH_START	\
		__try {

#define CAPINT_FATAL_CATCH_END(msg)	\
		} __except(VDCaptureIsCatchableException(GetExceptionCode())) {		\
			CaptureInternalHandleException(icd, msg, GetExceptionCode());				\
		}

static void CaptureInternalHandleException(VDCaptureData *icd, char *op, DWORD ec) {
	if (!icd->mpError) {
		char *s;

		switch(ec) {
		case EXCEPTION_ACCESS_VIOLATION:
			s = "Access Violation";
			break;
		case EXCEPTION_PRIV_INSTRUCTION:
			s = "Privileged Instruction";
			break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:
			s = "Integer Divide By Zero";
			break;
		case EXCEPTION_BREAKPOINT:
			s = "User Breakpoint";
			break;
		}

		icd->mpError = new MyError("Internal program error during %s handling: %s.", op, s);
	}
}
#endif

//////////////////////////////////////////////////////////////////////

extern const char g_szCapture				[]="Capture";
extern const char g_szStartupDriver			[]="Startup Driver";

static const char g_szWarnTiming1			[]="Warn Timing1";

///////////////////////////////////////////////////////////////////////////
//
//	dynamics
//
///////////////////////////////////////////////////////////////////////////

COMPVARS g_compression;

///////////////////////////////////////////////////////////////////////////
//
//	prototypes
//
///////////////////////////////////////////////////////////////////////////

extern void CaptureWarnCheckDriver(HWND hwnd, const char *s);

static void CaptureInternal(HWND, HWND hWndCapture, bool fTest);

///////////////////////////////////////////////////////////////////////////
//
//	callbacks
//
///////////////////////////////////////////////////////////////////////////

#if 0
LRESULT CALLBACK CaptureHistoFrameCallback(HWND hWnd, VIDEOHDR *vhdr) {
	CAPSTATUS cs;
	RECT r;
	HDC hdc;
	HWND hwndParent;

	if (!g_pHistogram)
		return 0;

	if (!capGetStatus(hWnd, &cs, sizeof cs))
		return 0;

	if (cs.fCapturingNow)
		return 0;

	try {
		if (!g_pHistogram->CheckFrameSize(cs.uiImageWidth, cs.uiImageHeight))
			return 0;

		g_pHistogram->Process(vhdr);

		hwndParent = GetParent(hWnd);

		if (hdc = GetDC(hwndParent)) {

			r.left = GetSystemMetrics(SM_CXEDGE);
			r.top = GetSystemMetrics(SM_CYEDGE) + cs.uiImageHeight;
			r.right = r.left + 256;
			r.bottom = r.top + 128;

			g_pHistogram->Draw(hdc, r);

			ReleaseDC(hwndParent, hdc);
		}
	} catch(const MyError& e) {
		guiSetStatus("Histogram: %s", 0, e.gets());
	}

	return 0;
}
#endif

///////////////////////////////////////////////////////////////////////////
//
//	VDCaptureProjectBaseCallback
//
///////////////////////////////////////////////////////////////////////////

void VDCaptureProjectBaseCallback::UICaptureDriversUpdated() {}
void VDCaptureProjectBaseCallback::UICaptureDriverChanged(int driver) {}
void VDCaptureProjectBaseCallback::UICaptureFileUpdated() {}
void VDCaptureProjectBaseCallback::UICaptureAudioFormatUpdated() {}
void VDCaptureProjectBaseCallback::UICaptureVideoFormatUpdated() {}
void VDCaptureProjectBaseCallback::UICaptureParmsUpdated() {}
bool VDCaptureProjectBaseCallback::UICaptureAnalyzeBegin(const VDPixmap& format) { return false; }
void VDCaptureProjectBaseCallback::UICaptureAnalyzeFrame(const VDPixmap& format) {}
void VDCaptureProjectBaseCallback::UICaptureAnalyzeEnd() {}
void VDCaptureProjectBaseCallback::UICaptureVideoHistoBegin() {}
void VDCaptureProjectBaseCallback::UICaptureVideoHisto(const float data[256]) {}
void VDCaptureProjectBaseCallback::UICaptureVideoHistoEnd() {}
void VDCaptureProjectBaseCallback::UICaptureAudioPeaksUpdated(float l, float r) {}
void VDCaptureProjectBaseCallback::UICaptureStart() {}
bool VDCaptureProjectBaseCallback::UICapturePreroll() { return false; }
void VDCaptureProjectBaseCallback::UICaptureStatusUpdated(VDCaptureStatus&) {}
void VDCaptureProjectBaseCallback::UICaptureEnd(bool success) {}

///////////////////////////////////////////////////////////////////////////
//
//	VDCaptureProject
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureProject : public IVDCaptureProject, public IVDCaptureDriverCallback, public IVDUIFrameEngine {
	friend class VDCaptureData;
public:
	VDCaptureProject();
	~VDCaptureProject();

	int		AddRef();
	int		Release();

	bool	Attach(VDGUIHandle hwnd);
	void	Detach();

	IVDCaptureProjectCallback *GetCallback() { return mpCB; }
	void	SetCallback(IVDCaptureProjectCallback *pCB);

	bool	IsHardwareDisplayAvailable();
	void	SetDisplayMode(DisplayMode mode);
	DisplayMode	GetDisplayMode();
	void	SetDisplayChromaKey(int key) { mDisplayChromaKey = key; }
	void	SetDisplayRect(const vdrect32& r);
	vdrect32 GetDisplayRectAbsolute();
	void	SetDisplayVisibility(bool vis);

	void	SetVideoFrameTransferEnabled(bool ena);
	bool	IsVideoFrameTransferEnabled();

	void	SetVideoHistogramEnabled(bool ena);
	bool	IsVideoHistogramEnabled();

	void	SetFrameTime(sint32 lFrameTime);
	sint32	GetFrameTime();

	void	SetTimingSetup(const VDCaptureTimingSetup& timing) { mTimingSetup = timing; }
	const VDCaptureTimingSetup&	GetTimingSetup() { return mTimingSetup; }

	void	SetAudioCaptureEnabled(bool ena);
	bool	IsAudioCaptureEnabled();
	bool	IsAudioCaptureAvailable();

	void	SetAudioVumeterEnabled(bool b);
	bool	IsAudioVumeterEnabled();

	void	SetHardwareBuffering(int videoBuffers, int audioBuffers, int audioBufferSize);
	bool	GetHardwareBuffering(int& videoBuffers, int& audioBuffers, int& audioBufferSize);

	bool	IsDriverDialogSupported(DriverDialog dlg);
	void	DisplayDriverDialog(DriverDialog dlg);

	void	GetPreviewImageSize(sint32& w, sint32& h);

	void	SetFilterSetup(const VDCaptureFilterSetup& setup);
	const VDCaptureFilterSetup& GetFilterSetup();

	void	SetStopPrefs(const VDCaptureStopPrefs& prefs);
	const VDCaptureStopPrefs& GetStopPrefs();

	void	SetDiskSettings(const VDCaptureDiskSettings& sets);
	const VDCaptureDiskSettings& GetDiskSettings();

	uint32	GetPreviewFrameCount();

	bool	SetVideoFormat(const BITMAPINFOHEADER& bih, LONG cbih);
	bool	GetVideoFormat(vdstructex<BITMAPINFOHEADER>& bih);

	void	GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats);

	bool	SetAudioFormat(const WAVEFORMATEX& wfex, LONG cbwfex);
	bool	GetAudioFormat(vdstructex<WAVEFORMATEX>& wfex);

	void	SetAudioCompFormat();
	void	SetAudioCompFormat(const WAVEFORMATEX& wfex, uint32 cbwfex);
	bool	GetAudioCompFormat(vdstructex<WAVEFORMATEX>& wfex);

	void		SetCaptureFile(const VDStringW& filename, bool bIsStripeSystem);
	VDStringW	GetCaptureFile();
	bool		IsStripingEnabled();

	void	SetSpillSystem(bool enable);
	bool	IsSpillEnabled();

	void	IncrementFileID();
	void	DecrementFileID();

	void	ScanForDrivers();
	int		GetDriverCount();
	const wchar_t *GetDriverName(int i);
	bool	SelectDriver(int nDriver);
	bool	IsDriverConnected();
	int		GetConnectedDriverIndex();

	void	Capture(bool bTest);
	void	CaptureStop();

protected:
	bool	InitFilter();
	void	ShutdownFilter();
	bool	InitVideoHistogram();
	void	ShutdownVideoHistogram();
	bool	InitVideoFrameTransfer();
	void	ShutdownVideoFrameTransfer();
	void	DispatchAnalysis(const VDPixmap&);

	void	CapBegin(sint64 global_clock);
	void	CapEnd(const MyError *pError);
	bool	CapControl(bool is_preroll);
	void	CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock);
protected:

	vdautoptr<IVDCaptureDriver>	mpDriver;
	int			mDriverIndex;
	VDGUIHandle	mhwnd;

	IVDCaptureProjectCallback	*mpCB;

	DisplayMode	mDisplayMode;

	VDStringW	mFilename;
	bool		mbStripingEnabled;

	int			mDisplayChromaKey;

	bool		mbEnableSpill;
	bool		mbEnableAudioVumeter;
	bool		mbEnableVideoHistogram;
	bool		mbEnableVideoFrameTransfer;

	vdautoptr<IVDCaptureFilterSystem>	mpFilterSys;
	VDPixmapLayout			mFilterInputLayout;
	VDPixmapLayout			mFilterOutputLayout;

	VDCaptureData	*mpCaptureData;
	DWORD		mMainThreadId;

	struct DriverEntry {
		VDStringW	mName;
		int			mSystemId;
		int			mId;

		DriverEntry(const wchar_t *name, int system, int id) : mName(name), mSystemId(system), mId(id) {}
	};

	typedef std::list<IVDCaptureSystem *>	tSystems;
	tSystems	mSystems;

	typedef std::list<DriverEntry>	tDrivers;
	tDrivers	mDrivers;

	vdstructex<WAVEFORMATEX>	mAudioCompFormat;
	WAVEFORMATEX	mAudioAnalysisFormat;

	vdautoptr<IVDCaptureVideoHistogram>	mpVideoHistogram;
	VDCriticalSection		mVideoAnalysisLock;

	VDCaptureTimingSetup	mTimingSetup;
	VDCaptureFilterSetup	mFilterSetup;
	VDCaptureStopPrefs		mStopPrefs;
	VDCaptureDiskSettings	mDiskSettings;

	uint32		mFilterPalette[256];

	VDAtomicInt	mRefCount;
};

IVDCaptureProject *VDCreateCaptureProject() { return new VDCaptureProject; }

VDCaptureProject::VDCaptureProject()
	: mDriverIndex(-1)
	, mhwnd(NULL)
	, mpCB(NULL)
	, mDisplayMode(kDisplayNone)
	, mbStripingEnabled(false)
	, mbEnableSpill(false)
	, mbEnableAudioVumeter(false)
	, mbEnableVideoHistogram(false)
	, mbEnableVideoFrameTransfer(false)
	, mpCaptureData(NULL)
	, mRefCount(0)
{
	mAudioAnalysisFormat.wFormatTag		= 0;

	mTimingSetup.mSyncMode				= VDCaptureTimingSetup::kSyncAudioToVideo;
	mTimingSetup.mbAllowEarlyDrops		= true;
	mTimingSetup.mbAllowLateInserts		= true;
	mTimingSetup.mbResyncWithIntegratedAudio	= false;

	mFilterSetup.mCropRect.clear();
	mFilterSetup.mVertSquashMode		= IVDCaptureFilterSystem::kFilterDisable;
	mFilterSetup.mNRThreshold			= 16;

	mFilterSetup.mbEnableRGBFiltering	= false;
	mFilterSetup.mbEnableNoiseReduction	= false;
	mFilterSetup.mbEnableLumaSquish		= false;
	mFilterSetup.mbEnableFieldSwap		= false;

	mStopPrefs.fEnableFlags = 0;

	mDiskSettings.mDiskChunkSize		= 512;
	mDiskSettings.mDiskChunkCount		= 2;
	mDiskSettings.mbDisableWriteCache	= 1;

	mSystems.push_back(VDCreateCaptureSystemVFW());
	mSystems.push_back(VDCreateCaptureSystemDS());
	mSystems.push_back(VDCreateCaptureSystemEmulation());
}

VDCaptureProject::~VDCaptureProject() {
	while(!mSystems.empty()) {
		delete mSystems.back();
		mSystems.pop_back();
	}
}

int VDCaptureProject::AddRef() {
	return ++mRefCount;
}

int VDCaptureProject::Release() {
	if (mRefCount == 1) {
		delete this;
		return 0;
	}

	return --mRefCount;
}

bool VDCaptureProject::Attach(VDGUIHandle hwnd) {
	if (mhwnd == hwnd)
		return true;

	if (mhwnd)
		Detach();

	extern void AnnounceCaptureExperimental(VDGUIHandle h);
	AnnounceCaptureExperimental(hwnd);

	mhwnd = hwnd;

	VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)hwnd);
	pFrame->AttachEngine(this);

	return true;
}

void VDCaptureProject::Detach() {
	if (!mhwnd)
		return;

	mpDriver = NULL;

	FreeCompressor(&g_compression);

	VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)mhwnd);
	pFrame->DetachEngine();

	mhwnd = NULL;
}

void VDCaptureProject::SetCallback(IVDCaptureProjectCallback *pCB) {
	mpCB = pCB;

	if (pCB) {
		pCB->UICaptureFileUpdated();
		pCB->UICaptureDriverChanged(mDriverIndex);
		pCB->UICaptureVideoFormatUpdated();
	}
}

bool VDCaptureProject::IsHardwareDisplayAvailable() {
	return mpDriver && mpDriver->IsHardwareDisplayAvailable();
}

void VDCaptureProject::SetDisplayMode(DisplayMode mode) {
	if (mDisplayMode == mode)
		return;

	if (mDisplayMode == kDisplayAnalyze) {
		if (mpCB)
			mpCB->UICaptureAnalyzeEnd();
		ShutdownVideoHistogram();
		ShutdownFilter();
	}

	mDisplayMode = mode;

	if (mpDriver) {
		if (mDisplayMode == kDisplayAnalyze) {
			InitFilter();

			if (mbEnableVideoHistogram)
				InitVideoHistogram();

			if (mbEnableVideoFrameTransfer)
				InitVideoFrameTransfer();
		}

		mpDriver->SetDisplayMode(mode);
	}
}

DisplayMode VDCaptureProject::GetDisplayMode() {
	return mDisplayMode;
}

void VDCaptureProject::SetDisplayRect(const vdrect32& r) {
	if (mpDriver) {
		mpDriver->SetDisplayRect(r);
	}
}

vdrect32 VDCaptureProject::GetDisplayRectAbsolute() {
	return mpDriver ? mpDriver->GetDisplayRectAbsolute() : vdrect32(0,0,0,0);
}

void VDCaptureProject::SetDisplayVisibility(bool vis) {
	if (mpDriver)
		mpDriver->SetDisplayVisibility(vis);
}

void VDCaptureProject::SetVideoFrameTransferEnabled(bool ena) {
	if (mbEnableVideoFrameTransfer == ena)
		return;

	mbEnableVideoFrameTransfer = ena;

	if (mDisplayMode == kDisplayAnalyze) {
		if (ena)
			InitVideoFrameTransfer();
		else
			ShutdownVideoFrameTransfer();
	}
}

bool VDCaptureProject::IsVideoFrameTransferEnabled() {
	return mbEnableVideoFrameTransfer;
}

void VDCaptureProject::SetVideoHistogramEnabled(bool ena) {
	if (mbEnableVideoHistogram == ena)
		return;

	mbEnableVideoHistogram = ena;

	if (mDisplayMode == kDisplayAnalyze) {
		if (ena)
			InitVideoHistogram();
		else
			ShutdownVideoHistogram();
	}
}

bool VDCaptureProject::IsVideoHistogramEnabled() {
	return mbEnableVideoHistogram;
}

void VDCaptureProject::SetFrameTime(sint32 lFrameTime) {
	if (mpDriver) {
		mpDriver->SetFramePeriod(lFrameTime);

		if (mpCB)
			mpCB->UICaptureParmsUpdated();
	}
}

sint32 VDCaptureProject::GetFrameTime() {
	if (!VDINLINEASSERT(mpDriver))
		return 1000000/15;

	return mpDriver->GetFramePeriod();
}

void VDCaptureProject::SetAudioCaptureEnabled(bool b) {
	if (mpDriver)
		mpDriver->SetAudioCaptureEnabled(b);
}

bool VDCaptureProject::IsAudioCaptureEnabled() {
	return mpDriver && mpDriver->IsAudioCaptureEnabled();
}

bool VDCaptureProject::IsAudioCaptureAvailable() {
	return mpDriver && mpDriver->IsAudioCapturePossible();
}

void VDCaptureProject::SetAudioVumeterEnabled(bool b) {
	// NOTE: Called from SetAudioFormat().
	if (mbEnableAudioVumeter == b)
		return;

	mbEnableAudioVumeter = b;

	if (mpDriver) {
		mpDriver->SetAudioAnalysisEnabled(false);

		if (b) {
			vdstructex<WAVEFORMATEX> wfex;
			if (mpDriver->GetAudioFormat(wfex)) {
				if (wfex->wFormatTag == WAVE_FORMAT_PCM) {
					mAudioAnalysisFormat = *wfex;
					mpDriver->SetAudioAnalysisEnabled(true);
				}
			}
		}
	}

	if (!b)
		mAudioAnalysisFormat.wFormatTag = 0;
}

bool VDCaptureProject::IsAudioVumeterEnabled() {
	return mbEnableAudioVumeter;
}

void VDCaptureProject::SetHardwareBuffering(int videoBuffers, int audioBuffers, int audioBufferSize) {
#if 0
	CAPTUREPARMS cp;

	if (capCaptureGetSetup(mhwndCapture, &cp, sizeof(CAPTUREPARMS))) {
		cp.wNumVideoRequested = videoBuffers;
		cp.wNumAudioRequested = audioBuffers;
		cp.dwAudioBufferSize = audioBufferSize;

		capCaptureSetSetup(mhwndCapture, &cp, sizeof(CAPTUREPARMS));
	}
#endif
}

bool VDCaptureProject::GetHardwareBuffering(int& videoBuffers, int& audioBuffers, int& audioBufferSize) {
#if 0
	CAPTUREPARMS cp;

	if (VDINLINEASSERT(capCaptureGetSetup(mhwndCapture, &cp, sizeof(CAPTUREPARMS)))) {
		videoBuffers = cp.wNumVideoRequested;
		audioBuffers = cp.wNumAudioRequested;
		audioBufferSize = cp.dwAudioBufferSize;
		return true;
	}
#endif
	return false;
}

bool VDCaptureProject::IsDriverDialogSupported(DriverDialog dlg) {
	return mpDriver && mpDriver->IsDriverDialogSupported(dlg);
}

void VDCaptureProject::DisplayDriverDialog(DriverDialog dlg) {
	if (mpDriver)
		mpDriver->DisplayDriverDialog(dlg);
}

void VDCaptureProject::GetPreviewImageSize(sint32& w, sint32& h) {
	w = 320;
	h = 240;

	vdstructex<BITMAPINFOHEADER> vformat;
	if (GetVideoFormat(vformat)) {
		w = vformat->biWidth;
		h = vformat->biHeight;
	}
}

void VDCaptureProject::SetFilterSetup(const VDCaptureFilterSetup& setup) {
	bool analyze = (mDisplayMode == kDisplayAnalyze);

	if (analyze)
		SetDisplayMode(kDisplayNone);
	VDASSERT(!mpFilterSys);
	mFilterSetup = setup;
	if (analyze)
		SetDisplayMode(kDisplayAnalyze);
}

const VDCaptureFilterSetup& VDCaptureProject::GetFilterSetup() {
	return mFilterSetup;
}

void VDCaptureProject::SetStopPrefs(const VDCaptureStopPrefs& prefs) {
	mStopPrefs = prefs;
}

const VDCaptureStopPrefs& VDCaptureProject::GetStopPrefs() {
	return mStopPrefs;
}

void VDCaptureProject::SetDiskSettings(const VDCaptureDiskSettings& sets) {
	mDiskSettings = sets;
}

const VDCaptureDiskSettings& VDCaptureProject::GetDiskSettings() {
	return mDiskSettings;
}

uint32 VDCaptureProject::GetPreviewFrameCount() {
	return mpDriver ? mpDriver->GetPreviewFrameCount() : 0;
}

bool VDCaptureProject::SetVideoFormat(const BITMAPINFOHEADER& bih, LONG cbih) {
	if (!mpDriver)
		return false;

	if (!mpDriver->SetVideoFormat(&bih, cbih))
		return false;
		
	if (mpCB)
		mpCB->UICaptureVideoFormatUpdated();

	return true;
}

bool VDCaptureProject::SetAudioFormat(const WAVEFORMATEX& wfex, LONG cbwfex) {
	if (!mpDriver)
		return false;

	bool bVumeter = mbEnableAudioVumeter;
	bool success = false;

	SetAudioVumeterEnabled(false);
	if (mpDriver->SetAudioFormat(&wfex, cbwfex)) {
		if (mpCB)
			mpCB->UICaptureAudioFormatUpdated();

		success = true;
	}
	SetAudioVumeterEnabled(bVumeter);

	return success;
}

bool VDCaptureProject::GetVideoFormat(vdstructex<BITMAPINFOHEADER>& bih) {
	return mpDriver && mpDriver->GetVideoFormat(bih);
}

void VDCaptureProject::GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats) {
	if (mpDriver)
		mpDriver->GetAvailableAudioFormats(aformats);
	else
		aformats.clear();
}

bool VDCaptureProject::GetAudioFormat(vdstructex<WAVEFORMATEX>& wfex) {
	return mpDriver && mpDriver->GetAudioFormat(wfex);
}

void VDCaptureProject::SetAudioCompFormat() {
	mAudioCompFormat.clear();
}

void VDCaptureProject::SetAudioCompFormat(const WAVEFORMATEX& wfex, uint32 cbwfex) {
	if (wfex.wFormatTag == WAVE_FORMAT_PCM)
		mAudioCompFormat.clear();
	else
		mAudioCompFormat.assign(&wfex, cbwfex);
}

bool VDCaptureProject::GetAudioCompFormat(vdstructex<WAVEFORMATEX>& wfex) {
	wfex = mAudioCompFormat;
	return !wfex.empty();
}

void VDCaptureProject::SetCaptureFile(const VDStringW& filename, bool bIsStripeSystem) {
	mFilename = filename;
	mbStripingEnabled = bIsStripeSystem;
	if (mpCB)
		mpCB->UICaptureFileUpdated();
}

VDStringW VDCaptureProject::GetCaptureFile() {
	return mFilename;
}

bool VDCaptureProject::IsStripingEnabled() {
	return mbStripingEnabled;
}

void VDCaptureProject::SetSpillSystem(bool enable) {
	mbEnableSpill = enable;
}

bool VDCaptureProject::IsSpillEnabled() {
	return mbEnableSpill;
}

void VDCaptureProject::DecrementFileID() {
	VDStringW name(mFilename);
	const wchar_t *s = name.data();

	VDStringW::size_type pos = VDFileSplitExt(s) - s;
	
	while(pos > 0) {
		--pos;

		if (iswdigit(name[pos])) {
			if (name[pos] == L'0')
				name[pos] = L'9';
			else {
				--name[pos];
				SetCaptureFile(name, mbStripingEnabled);
				if (mpCB)
					mpCB->UICaptureFileUpdated();
				return;
			}
		} else
			break;
	}

	guiSetStatus("Can't decrement filename any farther.", 0);
}

void VDCaptureProject::IncrementFileID() {
	VDStringW name(mFilename);
	const wchar_t *s = name.data();

	int pos = VDFileSplitExt(s) - s;
	
	while(--pos >= 0) {
		if (iswdigit(name[pos])) {
			if (name[pos] == '9')
				name[pos] = '0';
			else {
				++name[pos];
				SetCaptureFile(name, mbStripingEnabled);
				if (mpCB)
					mpCB->UICaptureFileUpdated();
				return;
			}
		} else
			break;
	}

	name.insert(pos+1, L'1');
	SetCaptureFile(name, mbStripingEnabled);

	if (mpCB)
		mpCB->UICaptureFileUpdated();
}

void VDCaptureProject::ScanForDrivers() {
	tSystems::const_iterator itSys(mSystems.begin()), itSysEnd(mSystems.end());
	int systemID = 0;

	for(; itSys != itSysEnd; ++itSys, ++systemID) {
		IVDCaptureSystem *pSystem = *itSys;

		pSystem->EnumerateDrivers();

		const int nDevices = pSystem->GetDeviceCount();
		for(int dev=0; dev<nDevices; ++dev)
			mDrivers.push_back(DriverEntry(pSystem->GetDeviceName(dev), systemID, dev));
	}

	if (mpCB)
		mpCB->UICaptureDriversUpdated();
}

int VDCaptureProject::GetDriverCount() {
	return mDrivers.size();
}

const wchar_t *VDCaptureProject::GetDriverName(int i) {
	tDrivers::const_iterator it(mDrivers.begin()), itEnd(mDrivers.end());

	while(i>0 && it!=itEnd) {
		--i;
		++it;
	}

	if (it != itEnd)
		return (*it).mName.c_str();

	return NULL;
}

bool VDCaptureProject::SelectDriver(int nDriver) {
	SetDisplayMode(kDisplayNone);

	mpDriver = NULL;
	mDriverIndex = -1;

	VDASSERT(nDriver == -1 || (unsigned)nDriver < mDrivers.size());

	if ((unsigned)nDriver >= mDrivers.size()) {
		if (mpCB)
			mpCB->UICaptureDriverChanged(-1);
		return false;
	}

	tDrivers::const_iterator it(mDrivers.begin());
	std::advance(it, nDriver);

	const DriverEntry& ent = *it;

	tSystems::const_iterator itSys(mSystems.begin());

	std::advance(itSys, ent.mSystemId);

	IVDCaptureSystem *pSys = *itSys;

	mpDriver = pSys->CreateDriver(ent.mId);

	if (!mpDriver || !mpDriver->Init(mhwnd)) {
		mpDriver = NULL;
		MessageBox((HWND)mhwnd, "VirtualDub cannot connect to the desired capture driver.", g_szError, MB_OK);
		if (mpCB)
			mpCB->UICaptureDriverChanged(-1);
		return false;
	}

	mDriverIndex = nDriver;
	mpDriver->SetCallback(this);

	mDisplayMode = kDisplayNone;

	if (mpCB) {
		mpCB->UICaptureDriverChanged(nDriver);
		mpCB->UICaptureParmsUpdated();
		mpCB->UICaptureAudioFormatUpdated();
		mpCB->UICaptureVideoFormatUpdated();
	}

	mpDriver->SetDisplayVisibility(true);

	bool bEnableVumeter = mbEnableAudioVumeter;
	mbEnableAudioVumeter = false;
	SetAudioVumeterEnabled(bEnableVumeter);

	return true;
}

bool VDCaptureProject::IsDriverConnected() {
	return !!mpDriver;
}

int VDCaptureProject::GetConnectedDriverIndex() {
	return mDriverIndex;
}

void VDCaptureProject::Capture(bool fTest) {
	if (!mpDriver)
		return;

	VDCaptureAutoPriority cpw;

	LONG biSizeToFile;
	VDCaptureData icd;

	bool fMainFinalized = false, fPendingFinalized = false;

	icd.mpProject = this;
	icd.mpError	= NULL;
	icd.mTimingSetup = mTimingSetup;

	vdautoptr<AVIStripeSystem> pStripeSystem;

	VDCaptureStatsFilter statsFilt;
	vdautoptr<IVDCaptureResyncFilter> pResyncFilter(VDCreateCaptureResyncFilter());
	vdautoptr<IVDCaptureAudioCompFilter> pAudioCompFilter(VDCreateCaptureAudioCompFilter());

	try {
		// get the input filename
		icd.mpszFilename = VDFileSplitPath(mFilename.c_str());

		// get capture parms

		const bool bCaptureAudio = IsAudioCaptureEnabled();

		// create an output file object

		if (!fTest) {
			if (mbStripingEnabled) {
				pStripeSystem = new AVIStripeSystem(VDTextWToA(mFilename).c_str());

				if (mbEnableSpill)
					throw MyError("Sorry, striping and spilling are not compatible.");

				icd.mpOutput = new_nothrow AVIOutputStriped(pStripeSystem);
				if (!icd.mpOutput)
					throw MyMemoryError();

				if (g_prefs.fAVIRestrict1Gb)
					((AVIOutputStriped *)icd.mpOutput)->set_1Gb_limit();
			} else {
				icd.mpOutputFile = VDCreateMediaOutputAVIFile();
				if (!icd.mpOutputFile)
					throw MyMemoryError();

				if (g_prefs.fAVIRestrict1Gb)
					icd.mpOutputFile->set_1Gb_limit();

				icd.mpOutputFile->set_capture_mode(true);
				icd.mpOutput = icd.mpOutputFile;
			}

			// initialize the AVIOutputFile object

			icd.mpVideoOut = icd.mpOutput->createVideoStream();
			icd.mpAudioOut = NULL;
			if (bCaptureAudio)
				icd.mpAudioOut = icd.mpOutput->createAudioStream();
		}

		// initialize audio
		vdstructex<WAVEFORMATEX> wfexInput;
		vdstructex<WAVEFORMATEX>& wfexOutput = mAudioCompFormat.empty() ? wfexInput : mAudioCompFormat;

		pResyncFilter->SetVideoRate(1000000.0 / mpDriver->GetFramePeriod());

		if (bCaptureAudio) {
			GetAudioFormat(wfexInput);

			pResyncFilter->SetAudioRate(wfexInput->nAvgBytesPerSec);
			pResyncFilter->SetAudioChannels(wfexInput->nChannels);

			switch(mTimingSetup.mSyncMode) {
			case VDCaptureTimingSetup::kSyncAudioToVideo:
				if (wfexInput->wFormatTag == WAVE_FORMAT_PCM) {
					switch(wfexInput->wBitsPerSample) {
					case 8:
						pResyncFilter->SetAudioFormat(kVDAudioSampleType8U);
						break;
					case 16:
						pResyncFilter->SetAudioFormat(kVDAudioSampleType16S);
						break;
					default:
						goto unknown_PCM_format;
					}

					pResyncFilter->SetResyncMode(IVDCaptureResyncFilter::kModeResampleAudio);
					break;
				}
				// fall through -- format isn't PCM so we can't resample it
			case VDCaptureTimingSetup::kSyncVideoToAudio:
unknown_PCM_format:
				pResyncFilter->SetResyncMode(IVDCaptureResyncFilter::kModeResampleVideo);
				break;
			}
		} else {
			pResyncFilter->SetAudioChannels(0);
		}

		// initialize video
		vdstructex<BITMAPINFOHEADER> bmiInput;
		GetVideoFormat(bmiInput);

		// initialize filtering
		vdstructex<BITMAPINFOHEADER> filteredFormat;
		BITMAPINFOHEADER *bmiToFile = bmiInput.data();
		biSizeToFile = bmiInput.size();

		icd.mInputLayout.format = 0;

		if (InitFilter()) {
			icd.mpFilterSys = mpFilterSys;
			icd.mInputLayout = mFilterInputLayout;

			VDMakeBitmapFormatFromPixmapFormat(filteredFormat, bmiInput, mFilterOutputLayout.format, 0, mFilterOutputLayout.w, mFilterOutputLayout.h);

			bmiToFile = &*filteredFormat;
			biSizeToFile = filteredFormat.size();
		}

		// initialize video compression
		vdstructex<BITMAPINFOHEADER> bmiOutput;

		if (g_compression.hic) {
			LONG formatSize;
			DWORD icErr;

			formatSize = ICCompressGetFormatSize(g_compression.hic, bmiToFile);
			if (formatSize < ICERR_OK)
				throw MyError("Error getting compressor output format size.");

			bmiOutput.resize(formatSize);

			if (ICERR_OK != (icErr = ICCompressGetFormat(g_compression.hic, bmiToFile, (BITMAPINFO *)bmiOutput.data())))
				throw MyICError("Video compressor",icErr);

			if (!(icd.mpVideoCompressor = new VideoSequenceCompressor()))
				throw MyMemoryError();

			icd.mpVideoCompressor->init(g_compression.hic, (BITMAPINFO *)bmiToFile, (BITMAPINFO *)bmiOutput.data(), g_compression.lQ, g_compression.lKey);
			icd.mpVideoCompressor->setDataRate(g_compression.lDataRate*1024, GetFrameTime(), 0x0FFFFFFF);
			icd.mpVideoCompressor->start();

			bmiToFile = bmiOutput.data();
			biSizeToFile = formatSize;
		}

		// set up output file headers and formats

		if (!fTest) {
			// setup stream headers

			AVIStreamHeader_fixed vstrhdr={0};

			vstrhdr.fccType					= streamtypeVIDEO;
			vstrhdr.fccHandler				= bmiToFile->biCompression;
			vstrhdr.dwScale					= GetFrameTime();
			vstrhdr.dwRate					= 1000000;
			vstrhdr.dwSuggestedBufferSize	= 0;
			vstrhdr.dwQuality				= g_compression.hic ? g_compression.lQ : (unsigned long)-1;
			vstrhdr.rcFrame.left			= 0;
			vstrhdr.rcFrame.top				= 0;
			vstrhdr.rcFrame.right			= (short)bmiToFile->biWidth;
			vstrhdr.rcFrame.bottom			= (short)bmiToFile->biHeight;

			icd.mpVideoOut->setFormat(bmiToFile, biSizeToFile);
			icd.mpVideoOut->setStreamInfo(vstrhdr);

			if (bCaptureAudio) {
				AVIStreamHeader_fixed astrhdr={0};
				astrhdr.fccType				= streamtypeAUDIO;
				astrhdr.fccHandler			= 0;
				astrhdr.dwScale				= wfexOutput->nBlockAlign;
				astrhdr.dwRate				= wfexOutput->nAvgBytesPerSec;
				astrhdr.dwQuality			= (unsigned long)-1;
				astrhdr.dwSampleSize		= wfexOutput->nBlockAlign; 

				icd.mpAudioOut->setFormat(wfexOutput.data(), wfexOutput.size());
				icd.mpAudioOut->setStreamInfo(astrhdr);
			}
		}

		// Setup capture structure
		if (bCaptureAudio) {
			memcpy(&icd.mwfex, wfexOutput.data(), std::min<unsigned>(wfexOutput.size(), sizeof icd.mwfex));
			icd.mAudioSampleSize	= wfexOutput->nBlockAlign;
		}

		icd.mCaptureRoot	= VDFileSplitPathLeft(mFilename);
		icd.mpszPath		= icd.mCaptureRoot.c_str();

		icd.mbNTSC			= ((GetFrameTime()|1) == 33367);
		icd.mFramePeriod	= GetFrameTime();

		if (!bmiInput->biBitCount)
			icd.mUncompressedFrameSize		= ((bmiInput->biWidth * 2 + 3) & -3) * bmiInput->biHeight;
		else
			icd.mUncompressedFrameSize		= ((bmiInput->biWidth * ((bmiInput->biBitCount + 7)/8) + 3) & -3) * bmiInput->biHeight;

		// set up resynchronizer and stats filter

		if (bCaptureAudio && !mAudioCompFormat.empty()) {
			pAudioCompFilter->SetChildCallback(this);
			pAudioCompFilter->Init(wfexInput.data(), wfexOutput.data());
			pResyncFilter->SetChildCallback(pAudioCompFilter);
		} else {
			pResyncFilter->SetChildCallback(this);
		}

		statsFilt.Init(pResyncFilter, bCaptureAudio ? wfexInput.data() : NULL);
		mpDriver->SetCallback(&statsFilt);

		icd.mpStatsFilter	= &statsFilt;
		icd.mpResyncFilter	= pResyncFilter;

		// initialize the file
		//
		// this is kinda sick

		if (!fTest) {
			if (!pStripeSystem && mDiskSettings.mbDisableWriteCache) {
				icd.mpOutputFile->disable_os_caching();
				icd.mpOutputFile->setBuffering(1024 * mDiskSettings.mDiskChunkSize * mDiskSettings.mDiskChunkCount, 1024 * mDiskSettings.mDiskChunkSize);
			}

			if (mbEnableSpill) {
				VDStringW firstFileName(VDFileSplitExtLeft(mFilename));

				icd.mpOutputFile->setSegmentHintBlock(true, NULL, MAX_PATH+1);

				firstFileName += L".00.avi";

				icd.mpOutputFile->init(firstFileName.c_str());

				// Figure out what drive the first file is on, to get the disk threshold.  If we
				// don't know, make it 50Mb.

				CapSpillDrive *pcsd;

				if (pcsd = CapSpillFindDrive(firstFileName.c_str()))
					icd.mSizeThreshold = pcsd->threshold;
				else
					icd.mSizeThreshold = 50;
			} else
				if (!icd.mpOutput->init(mFilename.c_str()))
					throw MyError("Error initializing capture file.");
		}

		// Allocate audio buffer and begin IO thread.

		if (mbEnableSpill) {
			if (!icd.ThreadStart())
				throw MyWin32Error("Can't start I/O thread: %%s", GetLastError());
		}

		// capture!!

		mpCaptureData = &icd;
		mMainThreadId = GetCurrentThreadId();

		if (mpCB)
			mpCB->UICaptureStart();

		if (mpDriver->CaptureStart()) {
			VDSamplingAutoProfileScope autoVTProfile;

			MSG msg;

			while(GetMessage(&msg, NULL, 0, 0)) {
				if (!msg.hwnd && msg.message == WM_APP+100)
					break;

				if (!guiCheckDialogs(&msg) && !VDUIFrame::TranslateAcceleratorMessage(msg)) {
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
		}

		mpDriver->CaptureAbort();

		if (mpCB)
			mpCB->UICaptureEnd(!icd.mpError);

VDDEBUG("Capture has stopped.\n");

		if (icd.mpError)
			throw *icd.mpError;

		if (icd.isThreadAttached()) {
			icd.PostExitRequest();
			icd.ThreadWait();
		}

		if (icd.mpVideoCompressor)
			icd.mpVideoCompressor->finish();

		// finalize files

		if (!fTest) {
			VDDEBUG("Finalizing main file.\n");

			fMainFinalized = true;
			icd.mpOutput->finalize();

			fPendingFinalized = true;
			if (icd.mpOutputFilePending && icd.mpOutputFilePending != icd.mpOutputFile) {
				VDDEBUG("Finalizing pending file.\n");

				icd.mpOutputFilePending->finalize();
			}
		}

		VDDEBUG("Yatta!!!\n");
	} catch(const MyError& e) {
		e.post((HWND)mhwnd, "Capture error");
	}

	mpCaptureData = NULL;

	if (icd.mpFilterSys) {
		icd.mpFilterSys->Shutdown();
		icd.mpFilterSys = NULL;
	}

	// Kill the I/O thread.

	if (icd.isThreadAttached()) {
		icd.PostExitRequest();
		icd.ThreadWait();
	}

	// Might as well try and finalize anyway.  If we're finalizing here,
	// we encountered an error, so don't go and throw more out!

	if (!fTest)
		try {
			if (!fMainFinalized)
				icd.mpOutput->finalize();

			if (!fPendingFinalized && icd.mpOutputFilePending && icd.mpOutputFilePending != icd.mpOutputFile)
				icd.mpOutputFilePending->finalize();
		} catch(const MyError&) {
		}

	icd.mpVideoCompressor = NULL;

	if (icd.mpOutputFilePending && icd.mpOutputFilePending == icd.mpOutputFile)
		icd.mpOutputFilePending = NULL;

	delete icd.mpOutput;
	delete icd.mpOutputFilePending;

	// restore the callback
	mpDriver->SetCallback(this);

	// any warnings?

#if 0
	DWORD dw;

	if (icd.mbVideoTimingWrapDetected) {
		if (!QueryConfigDword(g_szCapture, g_szWarnTiming1, &dw) || !dw) {
			if (IDYES != MessageBox((HWND)mhwnd,
					"VirtualDub has detected, and compensated for, a possible bug in your video capture drivers that is causing "
					"its timing information to wrap around at 35 or 71 minutes.  Your capture should be okay, but you may want "
					"to try upgrading your video capture drivers anyway, since this can cause video capture to halt in "
					"other applications.\n"
					"\n"
					"Do you want VirtualDub to warn you the next time this happens?"
					, "VirtualDub Warning", MB_YESNO))

				SetConfigDword(g_szCapture, g_szWarnTiming1, 1);
		}
	}
#endif
}

void VDCaptureProject::CaptureStop() {
	mpDriver->CaptureStop();
}

void VDCaptureProject::CapBegin(sint64 global_clock) {
}

void VDCaptureProject::CapEnd(const MyError *pError) {
	if (pError) {
		if (!mpCaptureData->mpError)
			mpCaptureData->mpError = new MyError(*pError);
	}

	PostThreadMessage(mMainThreadId, WM_APP+100, 0, 0);
}

bool VDCaptureProject::CapControl(bool is_preroll) {
	if (is_preroll) {
#if 0
		CAPTUREPARMS cp;
		if (capCaptureGetSetup(hwnd, &cp, sizeof(CAPTUREPARMS)) && cp.fMakeUserHitOKToCapture) {
			if (pThis->mpCB)
				return pThis->mpCB->UICapturePreroll();
		}
#endif

		CaptureBT848Reassert();
	} else {
		VDCaptureData *const cd = mpCaptureData;

		if (mStopPrefs.fEnableFlags & CAPSTOP_TIME)
			if (cd->mLastTime >= mStopPrefs.lTimeLimit*1000)
				return false;

		if (mStopPrefs.fEnableFlags & CAPSTOP_FILESIZE)
			if ((long)((cd->mTotalVideoSize + cd->mTotalAudioSize + 2048)>>20) > mStopPrefs.lSizeLimit)
				return false;

		if (mStopPrefs.fEnableFlags & CAPSTOP_DISKSPACE)
			if (cd->mDiskFreeBytes && (long)(cd->mDiskFreeBytes>>20) < mStopPrefs.lDiskSpaceThreshold)
				return false;

		if (mStopPrefs.fEnableFlags & CAPSTOP_DROPRATE)
			if (cd->mTotalFramesCaptured > 50 && cd->mFramesDropped*100 > mStopPrefs.lMaxDropRate*cd->mTotalFramesCaptured)
				return false;
	}

	return true;
}

void VDCaptureProject::CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock) {
	if (stream < 0) {
		if (mpCB) {
			if (stream == -1) {
				VDPixmap px(VDPixmapFromLayout(mFilterInputLayout, (void *)data));

				if (mpFilterSys)
					mpFilterSys->Run(px);

				DispatchAnalysis(px);
			} else {
				if (mAudioAnalysisFormat.wFormatTag == WAVE_FORMAT_PCM) {
					float l=0, r=0;
					VDComputeWavePeaks(data, mAudioAnalysisFormat.wBitsPerSample, mAudioAnalysisFormat.nChannels, size / mAudioAnalysisFormat.nBlockAlign, l, r);
					if (mpCB)
						mpCB->UICaptureAudioPeaksUpdated(l, r);
				}
			}
		}
		return;
	}

	VDCaptureData *const icd = mpCaptureData;

	if (icd->mpError)
		return;

	if (MyError *e = icd->mpSpillError.xchg(NULL)) {
		icd->mpError = e;
		mpDriver->CaptureAbort();
		return;
	}

	bool success;

	////////////////////////
	CAPINT_FATAL_CATCH_START
	////////////////////////

	if (stream > 0) {
		success = icd->WaveCallback(data, size, global_clock);

		if (mAudioAnalysisFormat.wFormatTag == WAVE_FORMAT_PCM) {
			float l=0, r=0;
			VDComputeWavePeaks(data, mAudioAnalysisFormat.wBitsPerSample, mAudioAnalysisFormat.nChannels, size / mAudioAnalysisFormat.nBlockAlign, l, r);
			if (mpCB)
				mpCB->UICaptureAudioPeaksUpdated(l, r);
		}
	} else {
		if (!stream)
			success = icd->VideoCallback(data, size, timestamp, key, global_clock);
	}

	///////////////////////////////
	CAPINT_FATAL_CATCH_END("video")
	///////////////////////////////

	return;
}

bool VDCaptureProject::InitFilter() {
	if (mpFilterSys)
		return true;

	mFilterInputLayout.format = 0;

	vdstructex<BITMAPINFOHEADER> vformat;

	if (!GetVideoFormat(vformat))
		return false;

	int variant;
	int format = VDBitmapFormatToPixmapFormat(*vformat, variant);

	VDMakeBitmapCompatiblePixmapLayout(mFilterInputLayout, vformat->biWidth, vformat->biHeight, format, variant);

	bool nonTrivialCrop = mFilterSetup.mCropRect.left
						+ mFilterSetup.mCropRect.top
						+ mFilterSetup.mCropRect.right
						+ mFilterSetup.mCropRect.bottom > 0;

	if (   !nonTrivialCrop
		&& !mFilterSetup.mbEnableRGBFiltering
		&& !mFilterSetup.mbEnableLumaSquish
		&& !mFilterSetup.mbEnableFieldSwap
		&& !mFilterSetup.mVertSquashMode
		&& !mFilterSetup.mbEnableNoiseReduction)
	{
		return false;
	}

	uint32 palEnts = 0;

	memset(mFilterPalette, 0, sizeof mFilterPalette);

	if (vformat->biCompression == BI_RGB && vformat->biBitCount <= 8)
		palEnts = vformat->biClrUsed;

	int palOffset = VDGetSizeOfBitmapHeaderW32(vformat.data());
	int realPalEnts = (vformat.size() - palOffset) >> 2;

	if (realPalEnts > 0) {
		if (palEnts > (uint32)realPalEnts)
			palEnts = realPalEnts;

		if (palEnts > 256)
			palEnts = 256;

		memcpy(mFilterPalette, (char *)vformat.data() + palOffset, sizeof(uint32)*palEnts);
	}

	mFilterInputLayout.palette = mFilterPalette;

	mpFilterSys = VDCreateCaptureFilterSystem();

	mpFilterSys->SetCrop(mFilterSetup.mCropRect.left,
							mFilterSetup.mCropRect.top,
							mFilterSetup.mCropRect.right,
							mFilterSetup.mCropRect.bottom);

	if (mFilterSetup.mbEnableNoiseReduction)
		mpFilterSys->SetNoiseReduction(mFilterSetup.mNRThreshold);

	mpFilterSys->SetLumaSquish(mFilterSetup.mbEnableLumaSquish);
	mpFilterSys->SetFieldSwap(mFilterSetup.mbEnableFieldSwap);
	mpFilterSys->SetVertSquashMode(mFilterSetup.mVertSquashMode);
	mpFilterSys->SetChainEnable(mFilterSetup.mbEnableRGBFiltering);

	mFilterOutputLayout = mFilterInputLayout;
	mpFilterSys->Init(mFilterOutputLayout, GetFrameTime());

	return true;
}

void VDCaptureProject::ShutdownFilter() {
	mpFilterSys = NULL;
}

bool VDCaptureProject::InitVideoHistogram() {
	if (mpVideoHistogram)
		return true;

	vdsynchronized(mVideoAnalysisLock) {
		mpVideoHistogram = VDCreateCaptureVideoHistogram();
		if (!mpVideoHistogram)
			return false;
	}

	if (mpCB)
		mpCB->UICaptureVideoHistoBegin();
	return true;
}

void VDCaptureProject::ShutdownVideoHistogram() {
	if (!mpVideoHistogram)
		return;

	vdsynchronized(mVideoAnalysisLock) {
		if (mpCB)
			mpCB->UICaptureVideoHistoEnd();

		mpVideoHistogram = NULL;
	}
}

bool VDCaptureProject::InitVideoFrameTransfer() {
	if (mpCB) {
		VDPixmap px;
		
		px.data		= NULL;
		px.data2	= NULL;
		px.data3	= NULL;
		px.format	= mFilterInputLayout.format;
		px.w		= mFilterInputLayout.w;
		px.h		= mFilterInputLayout.h;
		px.palette	= mFilterInputLayout.palette;
		px.pitch	= mFilterInputLayout.pitch;
		px.pitch2	= mFilterInputLayout.pitch2;
		px.pitch3	= mFilterInputLayout.pitch3;

		mpCB->UICaptureAnalyzeBegin(px);
	}

	return true;
}

void VDCaptureProject::ShutdownVideoFrameTransfer() {
}

void VDCaptureProject::DispatchAnalysis(const VDPixmap& px) {
	vdsynchronized(mVideoAnalysisLock) {
		if (mDisplayMode == kDisplayAnalyze && mpCB) {
			if (mpVideoHistogram) {
				float data[256];

				float scale = 0.1f;
				if (mpVideoHistogram->Process(px, data, scale)) {
					mpCB->UICaptureVideoHisto(data);
				}

			}

			if (mbEnableVideoFrameTransfer)
				mpCB->UICaptureAnalyzeFrame(px);
		}
	}
}











































///////////////////////////////////////////////////////////////////////////
//
//	Internal capture
//
///////////////////////////////////////////////////////////////////////////

void VDCaptureData::CreateNewFile() {
	IVDMediaOutputAVIFile *pNewFile = NULL;
	BITMAPINFO *bmi;
	wchar_t fname[MAX_PATH];
	CapSpillDrive *pcsd;

	pcsd = CapSpillPickDrive(false);
	if (!pcsd) {
		mbAllFull = true;
		return;
	}

	mpszNewPath = pcsd->path.c_str();

	try {
		pNewFile = VDCreateMediaOutputAVIFile();
		if (!pNewFile)
			throw MyMemoryError();

		pNewFile->setSegmentHintBlock(true, NULL, MAX_PATH+1);

		IVDMediaOutputStream *pNewVideo = pNewFile->createVideoStream();
		IVDMediaOutputStream *pNewAudio = NULL;
		
		if (mpAudioOut)
			pNewAudio = pNewFile->createAudioStream();

		if (g_prefs.fAVIRestrict1Gb)
			pNewFile->set_1Gb_limit();

		pNewFile->set_capture_mode(true);

		// copy over information to new file

		pNewVideo->setStreamInfo(mpVideoOut->getStreamInfo());
		pNewVideo->setFormat(mpVideoOut->getFormat(), mpVideoOut->getFormatLen());

		if (mpAudioOut) {
			pNewAudio->setStreamInfo(mpAudioOut->getStreamInfo());
			pNewAudio->setFormat(mpAudioOut->getFormat(), mpAudioOut->getFormatLen());
		} 

		// init the new file

		if (mpProject->IsStripingEnabled()) {
			const VDCaptureDiskSettings& sets = mpProject->GetDiskSettings();

			if (sets.mbDisableWriteCache) {
				pNewFile->disable_os_caching();
				pNewFile->setBuffering(1024 * sets.mDiskChunkSize * sets.mDiskChunkCount, 1024 * sets.mDiskChunkSize);
			}
		}

		bmi = (BITMAPINFO *)mpVideoOut->getFormat();

		pcsd->makePath(fname, mpszFilename);

		// edit the filename up

		swprintf(const_cast<wchar_t *>(VDFileSplitExt(fname)), L".%02d.avi", mSegmentIndex+1);

		// init the file

		pNewFile->init(fname);

		mpOutputFilePending = pNewFile;

		*const_cast<wchar_t *>(VDFileSplitPath(fname)) = 0;

#pragma vdpragma_TODO("This drops Unicode characters not representable in ANSI")
		VDStringA fnameA(VDTextWToA(fname));

		int len = fnameA.size();
		if (len < MAX_PATH)
			len = MAX_PATH;

		mpOutputFile->setSegmentHintBlock(false, VDTextWToA(fname).c_str(), len+1);

		++mSegmentIndex;
		mSizeThresholdPending = pcsd->threshold;

	} catch(const MyError&) {
		delete pNewFile;
		throw;
	}
}

void VDCaptureData::FinalizeOldFile() {
	IVDMediaOutput *ao = mpOutput;

	mpOutputFile	= mpOutputFilePending;
	mpOutput		= mpOutputFile;
	ao->finalize();
	delete ao;
	mpszPath = mpszNewPath;
	mSizeThreshold = mSizeThresholdPending;
}

void VDCaptureData::ThreadRun() {
	MSG msg;
	bool fSwitch = false;
	DWORD dwTimer = GetTickCount();
	bool fTimerActive = true;

	for(;;) {
		bool fSuccess = false;

		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == VDCM_EXIT)
				return;
			else if (msg.message == VDCM_SWITCH_FIN) {
				fSwitch = true;
				if (!fTimerActive) {
					fTimerActive = true;
					dwTimer = GetTickCount();
				}
			}

			if (msg.message)
				fSuccess = true;
		}

		// We'd like to do this stuff while the system is idle, but it's
		// possible the system is so busy that it's never idle -- so force
		// processing to take place if the timeout expires.  Right now,
		// we set it to 10 seconds.

		if (!fSuccess || (fTimerActive && (GetTickCount()-dwTimer) > 10000) ) {

			// Kill timer.

			fTimerActive = false;

			// Time to initialize new output file?

			if (!mpSpillError) try {

				if (mpOutputFile && !mpOutputFilePending && !mbAllFull)
					CreateNewFile();

				// Finalize old output?

				if (fSwitch) {
					FinalizeOldFile();
					fSwitch = false;

					// Restart timer for new file to open.

					dwTimer = GetTickCount();
					fTimerActive = true;
				}
			} catch(const MyError& e) {
				mpSpillError = new MyError(e);
			}
		}

		if (!fSuccess)
			WaitMessage();
	}

	VDDeinitThreadData();
}

////////////////

void VDCaptureData::DoSpill() {
	if (!mpProject->IsSpillEnabled()) return;

	sint64 nAudioFromVideo;
	sint64 nVideoFromAudio;

	if (mbAllFull)
		throw MyError("Capture stopped: All assigned spill drives are full.");

	// If there is no audio, then switch now.

	if (mpAudioOut) {

		// Find out if the audio or video stream is ahead, and choose a stop point.

		if (mbNTSC)
			nAudioFromVideo = int64divround(mVideoBlocks * 1001i64 * mwfex.nAvgBytesPerSec, mAudioSampleSize * 30000i64);
		else
			nAudioFromVideo = int64divround(mVideoBlocks * (__int64)mFramePeriod * mwfex.nAvgBytesPerSec, mAudioSampleSize * 1000000i64);

		if (nAudioFromVideo < mAudioBlocks) {

			// Audio is ahead of the corresponding video point.  Figure out how many frames ahead
			// we need to trigger from now.

			if (mbNTSC) {
				nVideoFromAudio = int64divroundup(mAudioBlocks * mAudioSampleSize * 30000i64, mwfex.nAvgBytesPerSec * 1001i64);
				nAudioFromVideo = int64divround(nVideoFromAudio * 1001i64 * mwfex.nAvgBytesPerSec, mAudioSampleSize * 30000i64);
			} else {
				nVideoFromAudio = int64divroundup(mAudioBlocks * mAudioSampleSize * 1000000i64, mwfex.nAvgBytesPerSec * (__int64)mFramePeriod);
				nAudioFromVideo = int64divround(nVideoFromAudio * (__int64)mFramePeriod * mwfex.nAvgBytesPerSec, mAudioSampleSize * 1000000i64);
			}

			mVideoSwitchPt = nVideoFromAudio;
			mAudioSwitchPt = nAudioFromVideo;

			VDDEBUG("SPILL: (%I64d,%I64d) > trigger at > (%I64d,%I64d)\n", mVideoBlocks, mAudioBlocks, mVideoSwitchPt, mAudioSwitchPt);

			return;

		} else if (nAudioFromVideo > mAudioBlocks) {

			// Audio is behind the corresponding video point, so switch the video stream now
			// and post a trigger for audio.

			mAudioSwitchPt = nAudioFromVideo;

			VDDEBUG("SPILL: video frozen at %I64d, audio(%I64d) trigger at (%I64d)\n", mVideoBlocks, mAudioBlocks, mAudioSwitchPt);

			mSegmentVideoSize = 0;
			mpVideoOut = mpOutputFilePending->getVideoOutput();

			return;

		}
	}

	// Hey, they're exactly synched!  Well then, let's switch them now!

	VDDEBUG("SPILL: exact sync switch at %I64d, %I64d\n", mVideoBlocks, mAudioBlocks);

	IVDMediaOutput *pOutputPending = mpOutputFilePending;
	mpVideoOut = pOutputPending->getVideoOutput();
	mpAudioOut = pOutputPending->getAudioOutput();
	mSegmentAudioSize = mSegmentVideoSize = 0;

	PostFinalizeRequest();
}

void VDCaptureData::CheckVideoAfter() {
	++mVideoBlocks;
	
	if (mVideoSwitchPt && mVideoBlocks == mVideoSwitchPt) {

		mpVideoOut = mpOutputFilePending->getVideoOutput();

		if (!mAudioSwitchPt) {
			PostFinalizeRequest();

			VDDEBUG("VIDEO: Triggering finalize & switch.\n");
		} else
			VDDEBUG("VIDEO: Switching stripes, waiting for audio to reach sync point (%I64d < %I64d)\n", mAudioBlocks, mAudioSwitchPt);

		mVideoSwitchPt = 0;
		mSegmentVideoSize = 0;
	}
}

bool VDCaptureData::VideoCallback(const void *data, uint32 size, sint64 timestamp64, bool key, sint64 global_clock) {
	VDCriticalSection::AutoLock lock(mCallbackLock);

	// Has the I/O thread successfully completed the switch?
	if (mpOutputFile == mpOutputFilePending)
		mpOutputFilePending = NULL;

	// Determine what frame we are *supposed* to be on.
	//
	// Let's say our capture interval is 500ms:
	//		Frame 0: 0-249ms
	//		Frame 1: 250-749ms
	//		Frame 2: 750-1249ms
	//		...and so on.
	//
	// We have to do this because AVICap doesn't keep track
	// of dropped frames in no-file capture mode.

	mTotalVideoSize += mLastVideoSize;
	mLastVideoSize = 0;

	////////////////////////////

	uint32 dwCurrentFrame;
	if (mbNTSC)
		dwCurrentFrame = (DWORD)((timestamp64 * 30 + 500000) / 1001000);
	else
		dwCurrentFrame = (DWORD)((timestamp64 + mFramePeriod/2) / mFramePeriod);

	if (dwCurrentFrame)
		--dwCurrentFrame;

	long jitter = (long)(timestamp64 % mFramePeriod);

	if (jitter >= mFramePeriod/2)
		jitter -= mFramePeriod;

	mTotalDisp += abs(jitter);
	mTotalJitter += jitter;

	++mTotalFramesCaptured;

	mLastTime = (uint32)(global_clock / 1000);

	// Is the frame too early?

	if (mTimingSetup.mbAllowEarlyDrops && mLastCapturedFrame > dwCurrentFrame+1) {
		++mFramesDropped;
		VDDEBUG("Dropping early frame at %ld ms\n", (long)(timestamp64 / 1000));
		return 0;
	}

	// Run the frame through the filterer.

	uint32 dwBytesUsed = size;
	void *pFilteredData = (void *)data;

	VDPixmap px(VDPixmapFromLayout(mInputLayout, pFilteredData));

	if (mpFilterSys) {
		mVideoProfileChannel.Begin(0x008000, "V-Filter");
		mpFilterSys->Run(px);
		mVideoProfileChannel.End();
#pragma vdpragma_TODO("this is pretty wrong")
		pFilteredData = px.pitch < 0 ? vdptroffset(px.data, px.pitch*(px.h-1)) : px.data;
		dwBytesUsed = (px.pitch < 0 ? -px.pitch : px.pitch) * px.h;
	}

	mpProject->DispatchAnalysis(px);

	// While we are early, write dropped frames (grr)
	//
	// Don't do this for the first frame, since we don't
	// have any frames preceding it!

	if (mTimingSetup.mbAllowLateInserts && mTotalFramesCaptured > 1) {
		while(mLastCapturedFrame < dwCurrentFrame) {
			if (mpOutputFile)
				mpVideoOut->write(0, pFilteredData, 0, 1);

			++mLastCapturedFrame;
			++mFramesDropped;
			VDDEBUG("Late frame detected at %ld ms\n", (long)(timestamp64 / 1000));
			mTotalVideoSize += 24;
			mSegmentVideoSize += 24;

			if (mpVideoCompressor)
				mpVideoCompressor->dropFrame();

			if (mpOutputFile)
				CheckVideoAfter();
		}
	}

	if (mpVideoCompressor) {
		bool isKey;
		long lBytes = 0;
		void *lpCompressedData;

		mVideoProfileChannel.Begin(0x80c080, "V-Compress");
		lpCompressedData = mpVideoCompressor->packFrame(pFilteredData, &isKey, &lBytes);
		mVideoProfileChannel.End();

		if (mpOutputFile) {
			mVideoProfileChannel.Begin(0xe0e0e0, "V-Write");
			mpVideoOut->write(
					isKey ? AVIIF_KEYFRAME : 0,
					lpCompressedData,
					lBytes, 1);
			mVideoProfileChannel.End();

			CheckVideoAfter();
		}

		mLastVideoSize = lBytes + 24;
	} else {
		if (mpOutputFile) {
			mVideoProfileChannel.Begin(0xe0e0e0, "V-Write");
			mpVideoOut->write(key ? AVIIF_KEYFRAME : 0, pFilteredData, dwBytesUsed, 1);
			mVideoProfileChannel.End();
			CheckVideoAfter();
		}

		mLastVideoSize = dwBytesUsed + 24;
	}

	++mLastCapturedFrame;
	mSegmentVideoSize += mLastVideoSize;

	if (global_clock - mLastUpdateTime > 500000)
	{

		if (mpOutputFilePending && !mAudioSwitchPt && !mVideoSwitchPt && mpProject->IsSpillEnabled()) {
			if (mSegmentVideoSize + mSegmentAudioSize >= ((__int64)g_lSpillMaxSize<<20)
				|| VDGetDiskFreeSpace(mpszPath) < ((__int64)mSizeThreshold << 20))

				DoSpill();
		}

		sint64 i64;
		if (mpProject->IsSpillEnabled())
			i64 = CapSpillGetFreeSpace();
		else {
			if (!mCaptureRoot.empty())
				i64 = VDGetDiskFreeSpace(mCaptureRoot.c_str());
			else
				i64 = VDGetDiskFreeSpace(L".");
		}

		mDiskFreeBytes = i64;

		VDCaptureStatus status;

		status.mFramesCaptured	= mTotalFramesCaptured;
		status.mFramesDropped	= mFramesDropped;
		status.mTotalJitter		= mTotalJitter;
		status.mTotalDisp		= mTotalDisp;
		status.mTotalVideoSize	= mTotalVideoSize;
		status.mTotalAudioSize	= mTotalAudioSize;
		status.mCurrentSegment	= mSegmentIndex;
		status.mElapsedTimeMS	= (uint32)(global_clock / 1000);
		status.mDiskFreeSpace	= mDiskFreeBytes;

		status.mVideoFirstFrameTimeMS	= 0;
		status.mVideoLastFrameTimeMS	= 0;
		status.mAudioFirstFrameTimeMS	= 0;
		status.mAudioLastFrameTimeMS	= 0;
		status.mAudioFirstSize			= 0;
		status.mTotalAudioDataSize		= 0;
		status.mActualAudioHz			= 0;
		if (mpStatsFilter)
			mpStatsFilter->GetStats(status);

		status.mVideoTimingAdjustMS = 0;
		status.mAudioResamplingRate	= 0;
		if (mpResyncFilter) {
			VDCaptureResyncStatus rstat;

			mpResyncFilter->GetStatus(rstat);

			status.mVideoTimingAdjustMS = rstat.mVideoTimingAdjust;
			status.mAudioResamplingRate = rstat.mAudioResamplingRate;
		}

		if (mpProject->mpCB)
			mpProject->mpCB->UICaptureStatusUpdated(status);

		mLastUpdateTime = global_clock - global_clock % 500000;
		mTotalJitter = mTotalDisp = 0;
	};

	return true;
}

bool VDCaptureData::WaveCallback(const void *data, uint32 size, sint64 global_clock) {
	VDCriticalSection::AutoLock lock(mCallbackLock);

	// Has the I/O thread successfully completed the switch?

	if (mpOutputFile == mpOutputFilePending)
		mpOutputFilePending = NULL;

	if (mpOutput) {
		if (mpProject->IsSpillEnabled()) {
			const char *pSrc = (const char *)data;
			long left = (long)size;

			// If there is a switch point, write up to it.  Otherwise, write it all!

			while(left > 0) {
				long tc;

				tc = left;

				if (mAudioSwitchPt && mAudioBlocks+tc/mAudioSampleSize >= mAudioSwitchPt)
					tc = (long)((mAudioSwitchPt - mAudioBlocks) * mAudioSampleSize);

				mpAudioOut->write(0, pSrc, tc, tc / mAudioSampleSize);
				mTotalAudioSize += tc + 24;
				mSegmentAudioSize += tc + 24;
				mAudioBlocks += tc / mAudioSampleSize;

				if (mAudioSwitchPt && mAudioBlocks == mAudioSwitchPt) {
					// Switch audio to next stripe.

					mpAudioOut = mpOutputFilePending->getAudioOutput();

					if (!mVideoSwitchPt) {
						PostFinalizeRequest();
						VDDEBUG("AUDIO: Triggering finalize & switch.\n");
					} else
						VDDEBUG("AUDIO: Switching to next, waiting for video to reach sync point (%I64d < %I64d)\n", mVideoBlocks, mVideoSwitchPt);

					mAudioSwitchPt = 0;
					mSegmentAudioSize = 0;
				}

				left -= tc;
				pSrc += tc;
			}
		} else {
			mAudioProfileChannel.Begin(0xe0e0e0, "A-Write");
			mpAudioOut->write(0, data, size, size / mAudioSampleSize);
			mAudioProfileChannel.End();
			mTotalAudioSize += size + 24;
			mSegmentAudioSize += size + 24;
		}
	} else {
		mTotalAudioSize += size + 24;
		mSegmentAudioSize += size + 24;
	}

	return true;
}
