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
#include <vd2/Riza/capdriver.h>
#include <vd2/Riza/capdrivers.h>
#include <vd2/Riza/capresync.h>

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
	void CapEnd();
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

void VDCaptureStatsFilter::CapEnd() {
	if (mpCB)
		mpCB->CapEnd();
}

bool VDCaptureStatsFilter::CapControl(bool is_preroll) {
	if (mpCB)
		return mpCB->CapControl(is_preroll);

	return true;
}

void VDCaptureStatsFilter::CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock) {
	vdsynchronized(mcsLock) {
		if (stream == 0) {
			long lTimeStamp = timestamp / 1000;

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
	const char	*	mpszFilename;
	const char	*	mpszPath;
	const char	*	mpszNewPath;
	sint64			mSegmentAudioSize;
	sint64			mSegmentVideoSize;
	sint64			mAudioBlocks;
	sint64			mAudioSwitchPt;
	sint64			mVideoBlocks;
	sint64			mVideoSwitchPt;
	long			mSizeThreshold;
	long			mSizeThresholdPending;

	VDCaptureStatsFilter	*mpStatsFilter;
	IVDCaptureResyncFilter	*mpResyncFilter;

	bool			mbDoSwitch;
	bool			mbAllFull;
	bool			mbNTSC;

	IVDCaptureFilterSystem *mpFilterSys;
	VDPixmapLayout			mInputLayout;

	char		mCaptureRoot[MAX_PATH];

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
{
	mCaptureRoot[0] = 0;
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

static CaptureHistogram	*g_pHistogram;

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

	void	SetCallback(IVDCaptureProjectCallback *pCB);

	void	SetDisplayMode(DisplayMode mode);
	DisplayMode	GetDisplayMode();
	void	SetDisplayChromaKey(int key) { mDisplayChromaKey = key; }
	void	SetDisplayRect(const vdrect32& r);
	vdrect32 GetDisplayRectAbsolute();
	void	SetDisplayVisibility(bool vis);

	void	SetFrameTime(sint32 lFrameTime);
	sint32	GetFrameTime();

	void	SetSyncMode(SyncMode mode) { mSyncMode = mode; }
	SyncMode	GetSyncMode() { return mSyncMode; }

	void	SetAudioCaptureEnabled(bool ena);
	bool	IsAudioCaptureEnabled();

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

	bool	SetAudioFormat(const WAVEFORMATEX& wfex, LONG cbwfex);
	bool	GetAudioFormat(vdstructex<WAVEFORMATEX>& wfex);

	void		SetCaptureFile(const VDStringW& filename, bool bIsStripeSystem);
	VDStringW	GetCaptureFile();
	bool		IsStripingEnabled();

	void	SetSpillSystem(bool enable);
	bool	IsSpillEnabled();

	void	IncrementFileID();
	void	DecrementFileID();

	void	ScanForDrivers();
	int		GetDriverCount();
	const char *GetDriverName(int i);
	bool	SelectDriver(int nDriver);

	void	Capture(bool bTest);
	void	CaptureStop();

protected:
	void	EnablePreviewHistogram(bool enable);
	void	SetPreview(bool b);

	bool	InitFilter();
	void	ShutdownFilter();

	void	CapBegin(sint64 global_clock);
	void	CapEnd();
	bool	CapControl(bool is_preroll);
	void	CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock);
protected:

	vdautoptr<IVDCaptureDriver>	mpDriver;
	VDGUIHandle	mhwnd;

	IVDCaptureProjectCallback	*mpCB;

	DisplayMode	mDisplayMode;
	SyncMode	mSyncMode;

	VDStringW	mFilename;
	bool		mbStripingEnabled;

	int			mDisplayChromaKey;

	bool		mbEnableSpill;

	vdautoptr<IVDCaptureFilterSystem>	mpFilterSys;
	VDPixmapLayout			mFilterInputLayout;
	VDPixmapLayout			mFilterOutputLayout;

	VDCaptureData	*mpCaptureData;
	DWORD		mMainThreadId;

	struct DriverEntry {
		VDStringA	mName;
		int			mSystemId;
		int			mId;

		DriverEntry(const char *name, int system, int id) : mName(name), mSystemId(system), mId(id) {}
	};

	typedef std::list<IVDCaptureSystem *>	tSystems;
	tSystems	mSystems;

	typedef std::list<DriverEntry>	tDrivers;
	tDrivers	mDrivers;

	VDCaptureFilterSetup	mFilterSetup;
	VDCaptureStopPrefs		mStopPrefs;
	VDCaptureDiskSettings	mDiskSettings;

	uint32		mFilterPalette[256];

	VDAtomicInt	mRefCount;
};

IVDCaptureProject *VDCreateCaptureProject() { return new VDCaptureProject; }

VDCaptureProject::VDCaptureProject()
	: mhwnd(NULL)
	, mpCB(NULL)
	, mDisplayMode(kDisplayNone)
	, mSyncMode(kSyncToVideo)
	, mbStripingEnabled(false)
	, mbEnableSpill(false)
	, mRefCount(0)
{
	mFilterSetup.mCropRect.clear();
	mFilterSetup.mVertSquashMode		= IVDCaptureFilterSystem::kFilterDisable;
	mFilterSetup.mNRThreshold			= 16;

	mFilterSetup.mbEnableCrop			= false;
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
		pCB->UICaptureVideoFormatUpdated();
	}
}

void VDCaptureProject::SetDisplayMode(DisplayMode mode) {
	if (mDisplayMode == mode)
		return;

	if (mDisplayMode == kDisplayAnalyze) {
		if (mpCB)
			mpCB->UICaptureAnalyzeEnd();
		ShutdownFilter();
	}

	mDisplayMode = mode;

	if (g_pHistogram && mode != kDisplayAnalyze)
		EnablePreviewHistogram(false);

	if (g_pHistogram && mode < kDisplaySoftware)
		SetPreview(false);

	if (mpDriver) {
		if (mDisplayMode == kDisplayAnalyze) {
			InitFilter();

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

void VDCaptureProject::SetFrameTime(sint32 lFrameTime) {
	if (mpDriver)
		mpDriver->SetFramePeriod(lFrameTime);
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
	return mpDriver && mpDriver->SetAudioFormat(&wfex, cbwfex);
}

bool VDCaptureProject::GetVideoFormat(vdstructex<BITMAPINFOHEADER>& bih) {
	return mpDriver && mpDriver->GetVideoFormat(bih);
}

bool VDCaptureProject::GetAudioFormat(vdstructex<WAVEFORMATEX>& wfex) {
	return mpDriver && mpDriver->GetAudioFormat(wfex);
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
		for(int dev=0; dev<nDevices; ++dev) {
			VDStringA name(VDTextWToA(pSystem->GetDeviceName(dev)));
			mDrivers.push_back(DriverEntry(name.c_str(), systemID, dev));
		}
	}

	if (mpCB)
		mpCB->UICaptureDriversUpdated();
}

int VDCaptureProject::GetDriverCount() {
	return mDrivers.size();
}

const char *VDCaptureProject::GetDriverName(int i) {
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

	VDASSERT(nDriver == -1 || (unsigned)nDriver < mDrivers.size());

	if ((unsigned)nDriver >= mDrivers.size())
		return false;

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
		return false;
	}

	mpDriver->SetCallback(this);

	mDisplayMode = kDisplayNone;

	if (mpCB) {
		mpCB->UICaptureDriverChanged(nDriver);
		mpCB->UICaptureParmsUpdated();
		mpCB->UICaptureVideoFormatUpdated();
	}

	mpDriver->SetDisplayVisibility(true);

	return true;
}

void VDCaptureProject::EnablePreviewHistogram(bool fEnable) {
#if 0
	if (fEnable) {
		if (!g_pHistogram) {
			if (mbPreviewAnalysisEnabled)
				SetPreview(false);

			try {
				g_pHistogram = new CaptureHistogram(mhwndCapture, NULL, 128);

				if (!g_pHistogram)
					throw MyMemoryError();

				capSetCallbackOnFrame(mhwndCapture, (LPVOID)CaptureHistoFrameCallback);

			} catch(const MyError& e) {
				guiSetStatus("Cannot initialize histogram: %s", 0, e.gets());
			}
		}

		capPreview(mhwndCapture, true);
	} else {
		if (g_pHistogram) {
			SetPreview(true);
			delete g_pHistogram;
			g_pHistogram = NULL;
			InvalidateRect((HWND)mhwnd, NULL, TRUE);
		}
	}
	CaptureBT848Reassert();
#endif
}

void VDCaptureProject::SetPreview(bool b) {
#if 0
	capSetCallbackOnFrame(mhwndCapture, NULL);

	if (!b) {
		RydiaEnableAVICapPreview(false);
		mbPreviewAnalysisEnabled = false;

		if (g_pHistogram) {
			delete g_pHistogram;
			g_pHistogram = NULL;
			InvalidateRect(mhwndCapture, NULL, TRUE);
		}

		capPreview(mhwndCapture, FALSE);
	} else {
		if (mpCB) {
			RydiaEnableAVICapPreview(false);
			if (mpCB->UICaptureBeginAnalyze()) {
				mbPreviewAnalysisEnabled = true;
				RydiaInitAVICapHotPatch();
				RydiaEnableAVICapPreview(true);
				capSetCallbackOnFrame(mhwndCapture, OverlayFrameCallback);
			}
		}
		capPreview(mhwndCapture, TRUE);
	}
	CaptureBT848Reassert();
#endif
}

void VDCaptureProject::Capture(bool fTest) {
	VDCaptureAutoPriority cpw;

	LONG biSizeToFile;
	VDCaptureData icd;

	bool fMainFinalized = false, fPendingFinalized = false;

	icd.mpProject = this;
	icd.mpError	= NULL;

	vdautoptr<AVIStripeSystem> pStripeSystem;

	VDCaptureStatsFilter statsFilt;
	vdautoptr<IVDCaptureResyncFilter> pResyncFilter(VDCreateCaptureResyncFilter());

	try {
		// get the input filename
		VDStringA fname(VDTextWToA(mFilename));

		icd.mpszFilename = VDFileSplitPath(fname.c_str());

		// get capture parms

		const bool bCaptureAudio = IsAudioCaptureEnabled();

		// create an output file object

		if (!fTest) {
			if (mbStripingEnabled) {
				pStripeSystem = new AVIStripeSystem(fname.c_str());

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

		if (bCaptureAudio) {
			GetAudioFormat(wfexInput);

			pResyncFilter->SetVideoRate(1000000.0 / mpDriver->GetFramePeriod());
			pResyncFilter->SetAudioRate(wfexInput->nAvgBytesPerSec);
			pResyncFilter->SetAudioChannels(wfexInput->nChannels);
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
				astrhdr.dwScale				= wfexInput->nBlockAlign;
				astrhdr.dwRate				= wfexInput->nAvgBytesPerSec;
				astrhdr.dwQuality			= (unsigned long)-1;
				astrhdr.dwSampleSize		= wfexInput->nBlockAlign; 

				icd.mpAudioOut->setFormat(wfexInput.data(), wfexInput.size());
				icd.mpAudioOut->setStreamInfo(astrhdr);
			}
		}

		// Setup capture structure
		if (bCaptureAudio) {
			memcpy(&icd.mwfex, wfexInput.data(), std::min<unsigned>(wfexInput.size(), sizeof icd.mwfex));
			icd.mAudioSampleSize	= wfexInput->nBlockAlign;
		}

		icd.mpszPath		= icd.mCaptureRoot;

		icd.mbNTSC = ((GetFrameTime()|1) == 33367);
		icd.mFramePeriod	= GetFrameTime();

		if (!bmiInput->biBitCount)
			icd.mUncompressedFrameSize		= ((bmiInput->biWidth * 2 + 3) & -3) * bmiInput->biHeight;
		else
			icd.mUncompressedFrameSize		= ((bmiInput->biWidth * ((bmiInput->biBitCount + 7)/8) + 3) & -3) * bmiInput->biHeight;

		if (!SplitPathRoot(icd.mCaptureRoot, fname.c_str())) {
			icd.mCaptureRoot[0] = 0;
		}

		// set up resynchronizer and stats filter
		pResyncFilter->SetChildCallback(this);
		statsFilt.Init(pResyncFilter, bCaptureAudio ? &icd.mwfex : NULL);
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
				char szNameFirst[MAX_PATH];

				icd.mpOutputFile->setSegmentHintBlock(true, NULL, MAX_PATH+1);

				strcpy(szNameFirst, fname.c_str());
				strcpy((char *)VDFileSplitExt(szNameFirst), ".00.avi");

				icd.mpOutputFile->init(VDTextAToW(szNameFirst).c_str());

				// Figure out what drive the first file is on, to get the disk threshold.  If we
				// don't know, make it 50Mb.

				CapSpillDrive *pcsd;

				if (pcsd = CapSpillFindDrive(szNameFirst))
					icd.mSizeThreshold = pcsd->threshold;
				else
					icd.mSizeThreshold = 50;
			} else
				if (!icd.mpOutput->init(VDTextAToW(fname).c_str()))
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
	PostThreadMessage(mMainThreadId, WM_APP+100, 0, 0);
}

void VDCaptureProject::CapBegin(sint64 global_clock) {
}

void VDCaptureProject::CapEnd() {
	CaptureStop();
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
			VDPixmap px(VDPixmapFromLayout(mFilterInputLayout, (void *)data));

			if (mpFilterSys)
				mpFilterSys->Run(px);

			mpCB->UICaptureAnalyzeFrame(px);
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

	if (stream > 0)
		success = icd->WaveCallback(data, size, global_clock);
	else {
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

	if (!mFilterSetup.mbEnableCrop
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

	if (mFilterSetup.mbEnableCrop)
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













































///////////////////////////////////////////////////////////////////////////
//
//	Internal capture
//
///////////////////////////////////////////////////////////////////////////

void VDCaptureData::CreateNewFile() {
	IVDMediaOutputAVIFile *pNewFile = NULL;
	BITMAPINFO *bmi;
	char fname[MAX_PATH];
	CapSpillDrive *pcsd;

	pcsd = CapSpillPickDrive(false);
	if (!pcsd) {
		mbAllFull = true;
		return;
	}

	mpszNewPath = pcsd->path;

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

		sprintf((char *)VDFileSplitExt(fname), ".%02d.avi", mSegmentIndex+1);

		// init the file

		pNewFile->init(VDTextAToW(fname).c_str());

		mpOutputFilePending = pNewFile;

		*(char *)VDFileSplitPath(fname) = 0;

		mpOutputFile->setSegmentHintBlock(false, fname, MAX_PATH);

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

	long lTimeStamp = timestamp64 / 1000;

	mTotalVideoSize += mLastVideoSize;
	mLastVideoSize = 0;

	////////////////////////////

	uint32 dwCurrentFrame;
	if (mbNTSC)
		dwCurrentFrame = (DWORD)(((__int64)lTimeStamp * 30 + 500) / 1001);
	else
		dwCurrentFrame = (DWORD)(((__int64)lTimeStamp * 1000 + mFramePeriod/2) / mFramePeriod);

	if (dwCurrentFrame)
		--dwCurrentFrame;

	long jitter = (long)((lTimeStamp*1000i64) % mFramePeriod);

	if (jitter >= mFramePeriod/2)
		jitter -= mFramePeriod;

	mTotalDisp += abs(jitter);
	mTotalJitter += jitter;

	++mTotalFramesCaptured;

	mLastTime = (uint32)(global_clock / 1000);

	// Is the frame too early?

	if (mLastCapturedFrame > dwCurrentFrame+1) {
		++mFramesDropped;
		VDDEBUG("Drop forward at %ld ms (%ld ms corrected)\n", (long)(timestamp64 / 1000), lTimeStamp);
		return 0;
	}

	// Run the frame through the filterer.

	uint32 dwBytesUsed = size;
	void *pFilteredData = (void *)data;

	VDPixmap px(VDPixmapFromLayout(mInputLayout, pFilteredData));

	if (mpFilterSys) {
		mpFilterSys->Run(px);
#pragma vdpragma_TODO("this is pretty wrong")
		pFilteredData = px.pitch < 0 ? vdptroffset(px.data, px.pitch*(px.h-1)) : px.data;
		dwBytesUsed = (px.pitch < 0 ? -px.pitch : px.pitch) * px.h;
	}

	if (mpProject->mDisplayMode == kDisplayAnalyze) {
		if (mpProject->mpCB)
			mpProject->mpCB->UICaptureAnalyzeFrame(px);
	}

	try {
		// While we are early, write dropped frames (grr)
		//
		// Don't do this for the first frame, since we don't
		// have any frames preceding it!

		if (mTotalFramesCaptured > 1) {
			while(mLastCapturedFrame < dwCurrentFrame) {
				if (mpOutputFile)
					mpVideoOut->write(0, pFilteredData, 0, 1);

				++mLastCapturedFrame;
				++mFramesDropped;
				VDDEBUG("Drop back at %ld ms (%ld ms corrected)\n", (long)(timestamp64 / 1000), lTimeStamp);
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

			lpCompressedData = mpVideoCompressor->packFrame(pFilteredData, &isKey, &lBytes);

			if (mpOutputFile) {
				mpVideoOut->write(
						isKey ? AVIIF_KEYFRAME : 0,
						lpCompressedData,
						lBytes, 1);

				CheckVideoAfter();
			}

			mLastVideoSize = lBytes + 24;
		} else {
			if (mpOutputFile) {
				mpVideoOut->write(key ? AVIIF_KEYFRAME : 0, pFilteredData, dwBytesUsed, 1);
				CheckVideoAfter();
			}

			mLastVideoSize = dwBytesUsed + 24;
		}
	} catch(const MyError& e) {
		if (!mpError)
			mpError = new MyError(e);

		return false;
	}

	++mLastCapturedFrame;
	mSegmentVideoSize += mLastVideoSize;

	if (global_clock - mLastUpdateTime > 500000)
	{

		if (mpOutputFilePending && !mAudioSwitchPt && !mVideoSwitchPt && mpProject->IsSpillEnabled()) {
			if (mSegmentVideoSize + mSegmentAudioSize >= ((__int64)g_lSpillMaxSize<<20)
				|| VDGetDiskFreeSpace(VDTextAToW(VDString(mpszPath))) < ((__int64)mSizeThreshold << 20))

				DoSpill();
		}

		sint64 i64;
		if (mpProject->IsSpillEnabled())
			i64 = CapSpillGetFreeSpace();
		else {
			if (mCaptureRoot[0])
				i64 = VDGetDiskFreeSpace(VDTextAToW(VDString(mCaptureRoot)));
			else
				i64 = VDGetDiskFreeSpace(VDStringW(L"."));
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
	// Has the I/O thread successfully completed the switch?

	if (mpOutputFile == mpOutputFilePending)
		mpOutputFilePending = NULL;

	if (mpOutput) {
		try {
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
				mpAudioOut->write(0, data, size, size / mAudioSampleSize);
				mTotalAudioSize += size + 24;
				mSegmentAudioSize += size + 24;
			}
		} catch(const MyError& e) {
			if (!mpError)
				mpError = new MyError(e);

			return false;
		}
	} else {
		mTotalAudioSize += size + 24;
		mSegmentAudioSize += size + 24;
	}

	return true;
}
