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
#include <commctrl.h>
#include <vfw.h>
#include <shellapi.h>
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/registry.h>
#include <vd2/system/vdalloc.h>
#include <vd2/Dita/services.h>
#include <vd2/Dita/w32peer.h>
#include "gui.h"
#include "prefs.h"
#include "capture.h"
#include "capfilter.h"
#include "capspill.h"
#include "caputils.h"
#include "capaccel.h"
#include "caphisto.h"
#include "capvumeter.h"
#include "resource.h"
#include "oshelper.h"
#include "filtdlg.h"
#include "optdlg.h"
#include "AVIStripeSystem.h"
#include "uiframe.h"
#include "VideoDisplay.h"
#include "misc.h"
#include <vd2/Kasumi/pixmaputils.h>

using namespace nsVDCapture;

///////////////////////////////////////////////////////////////////////////

extern const char g_szCapture				[];
static const char g_szDefaultCaptureFile	[]="Capture File";
static const char g_szCompression			[]="Compression";
static const char g_szCompressorData		[]="Compressor Data";
static const char g_szAudioFormat			[]="Audio Format";
static const char g_szVideoFormat			[]="Video Format";
extern const char g_szChunkSize				[]="Chunk size";
extern const char g_szChunkCount			[]="Chunk count";
extern const char g_szDisableBuffering		[]="Disable buffering";
extern const char g_szAdjustVideoTiming		[]="AdjustVideoTiming";
static const char g_szHideInfoPanel			[]="Hide InfoPanel";
static const char g_szMultisegment			[]="Multisegment";
static const char g_szAutoIncrement			[]="Auto-increment";
static const char g_szStartOnLeft			[]="Start on left";
static const char g_szStopConditions		[]="Stop Conditions";
static const char g_szCapSettings			[]="Settings";
extern const char g_szStartupDriver			[];
static const char g_szDrvOpts				[]="DrvOpts %08lx";

extern HINSTANCE g_hInst;
extern const char g_szError[];

static char g_szStripeFile[MAX_PATH];

extern COMPVARS g_compression;

extern void CPUTest();

extern WAVEFORMATEX *AudioChooseCompressor(HWND hwndParent, WAVEFORMATEX *pwfexOld, WAVEFORMATEX *pwfexSrc);
extern void ChooseCompressor(HWND hwndParent, COMPVARS *lpCompVars, BITMAPINFOHEADER *bihInput);
extern void FreeCompressor(COMPVARS *pCompVars);

static INT_PTR CALLBACK CaptureCustomVidSizeDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK CapturePreferencesDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
static INT_PTR CALLBACK CaptureStopConditionsDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
extern INT_PTR CALLBACK CaptureSpillDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern LRESULT CALLBACK VCMDriverProc(DWORD dwDriverID, HDRVR hDriver, UINT uiMessage, LPARAM lParam1, LPARAM lParam2);

extern void CaptureWarnCheckDrivers(HWND hwnd);

void VDShowCaptureCroppingDialog(VDGUIHandle hwndParent, IVDCaptureProject *pProject);

bool VDShowCaptureDiskIODialog(VDGUIHandle hwndParent, VDCaptureDiskSettings& sets);
void VDShowCaptureToggleNRDialog(VDGUIHandle hwndParent);

extern void CaptureCloseBT848Tweaker();
extern void CaptureDisplayBT848Tweaker(HWND hwndParent);

IVDUIWindow *VDCreateUICaptureVumeter();

///////////////////////////////////////////////////////////////////////////

class VDCaptureCompressionSpecs {
public:
	DWORD	fccType;
	DWORD	fccHandler;
	LONG	lKey;
	LONG	lDataRate;
	LONG	lQ;
};

#define MENU_TO_HELP(x) ID_##x, IDS_CAP_##x

static const UINT iCaptureMenuHelpTranslator[]={
	MENU_TO_HELP(FILE_SETCAPTUREFILE),
	MENU_TO_HELP(FILE_ALLOCATEDISKSPACE),
	MENU_TO_HELP(FILE_EXITCAPTUREMODE),
	MENU_TO_HELP(AUDIO_COMPRESSION),
	MENU_TO_HELP(AUDIO_VOLUMEMETER),
	MENU_TO_HELP(VIDEO_OVERLAY),
	MENU_TO_HELP(VIDEO_PREVIEW),
	MENU_TO_HELP(VIDEO_PREVIEWHISTOGRAM),
	MENU_TO_HELP(VIDEO_FORMAT),
	MENU_TO_HELP(VIDEO_SOURCE),
	MENU_TO_HELP(VIDEO_DISPLAY),
	MENU_TO_HELP(VIDEO_COMPRESSION),
	MENU_TO_HELP(VIDEO_CUSTOMFORMAT),
	MENU_TO_HELP(VIDEO_FILTERS),
	MENU_TO_HELP(VIDEO_ENABLEFILTERING),
	MENU_TO_HELP(VIDEO_HISTOGRAM),
	MENU_TO_HELP(CAPTURE_SETTINGS),
	MENU_TO_HELP(CAPTURE_PREFERENCES),
	MENU_TO_HELP(CAPTURE_CAPTUREVIDEO),
	MENU_TO_HELP(CAPTURE_CAPTUREVIDEOINTERNAL),
	MENU_TO_HELP(CAPTURE_HIDEONCAPTURE),
	NULL,NULL,
};

#define FRAMERATE(x) ((LONG)((1000000 + (x)/2.0) / (x)))

static const LONG g_predefFrameRates[]={
	100003000/6000,
	100001500/3000,
	100001250/2500,
	100001000/2000,
	100000750/1500,
	100000600/1200,
	100000500/1000,
	100000250/ 500,
	100002997/5994,
	100001998/2997,
	100000999/1998,
	100000749/1499,
	100000599/1199,
	100000499/ 999,
	FRAMERATE(30.303),
	FRAMERATE(29.412),
	66000,		//FRAMERATE(15.151),
	67000,		//FRAMERATE(14.925),
};

#define CAPDRV_DISPLAY_OVERLAY	(0)
#define	CAPDRV_DISPLAY_PREVIEW	(1)
#define CAPDRV_DISPLAY_NONE		(2)
#define	CAPDRV_DISPLAY_MASK		(15)

#define CAPDRV_CRAPPY_PREVIEW	(0x00000010L)
#define	CAPDRV_CRAPPY_OVERLAY	(0x00000020L)

///////////////////////////////////////////////////////////////////////////
//
//	VDCaptureProjectUI
//
///////////////////////////////////////////////////////////////////////////

class VDCaptureProjectUI : public IVDCaptureProjectCallback, public IVDCaptureProjectUI, public IVDUIFrameClient {
	enum DisplayAccelMode {
		kDDP_Off = 0,
		kDDP_Top,
		kDDP_Bottom,
		kDDP_Both,
	};

public:
	VDCaptureProjectUI();
	~VDCaptureProjectUI();

	int AddRef();
	int Release();

	bool Attach(VDGUIHandle hwnd, IVDCaptureProject *pProject);
	void Detach();

protected:
	void	SetDisplayMode(DisplayMode mode);
	DisplayMode	GetDisplayMode();

	void	SetDisplayAccelMode(DisplayAccelMode mode);

	void	SetPCMAudioFormat(sint32 sampling_rate, bool is_16bit, bool is_stereo);

	void	LoadLocalSettings();
	void	SaveLocalSettings();

	void	SuspendDisplay(bool force = false);
	void	ResumeDisplay();
	void	UpdateDisplayMode();
	void	UpdateDisplayPos();
	vdrect32 ComputeDisplayArea();
	bool	InitVumeter();
	void	ShutdownVumeter();
	bool	InitVideoHistogram();
	void	ShutdownVideoHistogram();

	void	UICaptureDriversUpdated();
	void	UICaptureDriverChanged(int driver);
	void	UICaptureFileUpdated();
	void	UICaptureAudioFormatUpdated();
	void	UICaptureVideoFormatUpdated();
	void	UICaptureParmsUpdated();
	bool	UICaptureAnalyzeBegin(const VDPixmap& px);
	void	UICaptureAnalyzeFrame(const VDPixmap& px);
	void	UICaptureAnalyzeEnd();
	void	UICaptureVideoHistoBegin();
	void	UICaptureVideoHisto(const float data[256]);
	void	UICaptureVideoHistoEnd();
	void	UICaptureAudioPeaksUpdated(float l, float r);
	void	UICaptureStart();
	bool	UICapturePreroll();
	void	UICaptureStatusUpdated(VDCaptureStatus&);
	void	UICaptureEnd(bool success);

	static LRESULT CALLBACK StaticStatusWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT StatusWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	LRESULT WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT MainWndProc(UINT message, WPARAM wParam, LPARAM lParam);
	LRESULT RestrictedWndProc(UINT message, WPARAM wParam, LPARAM lParam);

	void	OnInitMenu(HMENU hmenu);
	void	OnPaint();
	void	OnTimer();
	void	OnSize();
	bool	OnParentNotify(WPARAM wParam, LPARAM lParam);
	bool	OnCommand(UINT id);
	void	OnUpdateStatus();
	void	OnUpdateVumeter();

	static INT_PTR CALLBACK StaticPanelDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	INT_PTR PanelDlgProc(UINT msg, WPARAM wParam, LPARAM lParam);

	VDGUIHandle			mhwnd;
	IVDCaptureProject	*mpProject;
	LRESULT (VDCaptureProjectUI::*mpWndProc)(UINT, WPARAM, LPARAM);

	HWND	mhwndStatus;
	WNDPROC	mStatusWndProc;
	HWND	mhwndPanel;
	HMENU	mhMenuCapture;
	HMENU	mhMenuAuxCapture;
	HACCEL	mhAccelCapture;
	bool	mbCaptureActive;
	HFONT	mhClockFont;

	uint32	mDriverOptions;

	DisplayMode	mDisplayModeShadow;
	bool		mbDisplayModeShadowed;

	DisplayAccelMode mDisplayAccelMode;

	bool	mbDisplayAccelActive;

	bool	mbStretchToWindow;
	bool	mbInfoPanel;
	bool	mbStartOnLeft;
	bool	mbDisplayLargeTimer;
	bool	mbHideOnCapture;
	bool	mbAutoIncrementAfterCapture;

	// Video display
	HWND	mhwndDisplay;
	IVDVideoDisplay	*mpDisplay;

	// CPU usage reader
	VDCPUUsageReader	mCPUReader;

	// status update fields
	VDCaptureStatus	mLastStatus;
	VDCaptureStatus	mCurStatus;
	sint32			mVideoUncompressedSize;
	sint32			mAudioUncompressedRate;

	UINT			mUpdateTimer;
	uint32			mLastPreviewFrameCount;

	volatile float	mPeakL, mPeakR;

	vdautoptr<IVDUIWindow>	mpVideoHistogram;
	vdautoptr<IVDUIWindow>	mpVumeter;

	VDUIPeerW32		mUIPeer;

	VDAtomicInt		mRefCount;
};

IVDCaptureProjectUI *VDCreateCaptureProjectUI() {
	return new VDCaptureProjectUI;
}

VDCaptureProjectUI::VDCaptureProjectUI()
	: mhwnd(NULL)
	, mpProject(NULL)
	, mhMenuCapture(NULL)
	, mhMenuAuxCapture(NULL)
	, mbCaptureActive(false)
	, mhClockFont(NULL)
	, mDriverOptions(0)
	, mDisplayAccelMode(kDDP_Off)
	, mbDisplayAccelActive(false)
	, mbStretchToWindow(false)
	, mbInfoPanel(false)
	, mbStartOnLeft(false)
	, mbDisplayLargeTimer(false)
	, mbHideOnCapture(false)
	, mbAutoIncrementAfterCapture(false)
	, mhwndDisplay(NULL)
	, mpDisplay(NULL)
	, mUpdateTimer(0)
	, mLastPreviewFrameCount(0)
	, mPeakL(0.f)
	, mPeakR(0.f)
	, mRefCount(0)
{
}

VDCaptureProjectUI::~VDCaptureProjectUI() {
}

int VDCaptureProjectUI::AddRef() {
	return ++mRefCount;
}

int VDCaptureProjectUI::Release() {
	int rc = --mRefCount;

	if (!rc)
		delete this;

	return rc;
}

bool VDCaptureProjectUI::Attach(VDGUIHandle hwnd, IVDCaptureProject *pProject) {
	if (mhwnd == hwnd && mpProject == pProject)
		return true;

	if (mhwnd)
		Detach();

	CPUTest();

	mhwnd = hwnd;
	mpWndProc = MainWndProc;
	mpProject = pProject;

	VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)hwnd);

	pFrame->Attach(this);

	mUIPeer.Attach((HWND)mhwnd);

	// load menus & accelerators
	if (   !(mhMenuAuxCapture	= LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_CAPTURE_AUXMENU)))
		|| !(mhMenuCapture		= LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_CAPTURE_MENU)))
		|| !(mhAccelCapture		= LoadAccelerators(g_hInst, MAKEINTRESOURCE(IDR_CAPTURE_KEYS)))
		)
	{
		Detach();
		return false;
	}

	pFrame->SetAccelTable(mhAccelCapture);

	SetMenu((HWND)mhwnd, mhMenuCapture);

	// create video display
	mhwndDisplay = CreateWindowEx(0, VIDEODISPLAYCONTROLCLASS, "", WS_CHILD, 0, 0, 0, 0, (HWND)mhwnd, (HMENU)-1, g_hInst, NULL);
	if (!mhwndDisplay) {
		Detach();
		return false;
	}
	mpDisplay = VDGetIVideoDisplay(mhwndDisplay);

	// setup the status window
	static const INT kStatusPartWidths[]={ 50, 100, 150, 200, 250, -1 };

	mhwndStatus = CreateStatusWindow(WS_CHILD|WS_VISIBLE, "", (HWND)mhwnd, IDC_STATUS_WINDOW);
	if (!mhwndStatus) {
		Detach();
		return false;
	}
	SendMessage(mhwndStatus, SB_SIMPLE, (WPARAM)FALSE, 0);
	SendMessage(mhwndStatus, SB_SETPARTS, (WPARAM)5, (LPARAM)(LPINT)kStatusPartWidths);
	SendMessage(mhwndStatus, SB_SETTEXT, 5 | SBT_NOBORDERS, (LPARAM)"");

	// subclass the status window
	mStatusWndProc = (WNDPROC)GetWindowLongPtr(mhwndStatus, GWLP_WNDPROC);
	SetWindowLongPtr(mhwndStatus, GWLP_USERDATA, (LONG_PTR)this);
	SetWindowLongPtr(mhwndStatus, GWLP_WNDPROC, (LONG_PTR)StaticStatusWndProc);

	// create the side panel
	CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_PANEL), (HWND)mhwnd, StaticPanelDlgProc, (LPARAM)this);
	if (!mhwndPanel) {
		Detach();
		return false;
	}

	SetWindowLong(mhwndPanel, GWL_ID, IDC_CAPTURE_PANEL);

	VDSetThreadExecutionStateW32(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED | ES_CONTINUOUS);

	mpProject->SetCallback(this);

	LoadLocalSettings();
	if (mbInfoPanel)
		ShowWindow(mhwndPanel, SW_SHOWNORMAL);
	OnSize();

	mpProject->ScanForDrivers();
	CaptureWarnCheckDrivers((HWND)mhwnd);

	mUpdateTimer = SetTimer((HWND)mhwnd, 10, 1000, NULL);
	return true;
}

void VDCaptureProjectUI::Detach() {
	if (!mhwnd)
		return;

	ShutdownVumeter();

	if (mUpdateTimer) {
		KillTimer((HWND)mhwnd, mUpdateTimer);
		mUpdateTimer = NULL;
	}

	CaptureCloseBT848Tweaker();

	mpProject->SetDisplayMode(kDisplayNone);

	if (mpDisplay)
		mpDisplay->Reset();

	SaveLocalSettings();

	mpProject->SetCallback(NULL);

	VDSetThreadExecutionStateW32(ES_CONTINUOUS);

	if (mhwndPanel) {
		DestroyWindow(mhwndPanel);
		mhwndPanel = NULL;
	}

	if (mhwndStatus) {
		DestroyWindow(mhwndStatus);
		mhwndStatus = NULL;
	}

	if (mhwndDisplay) {
		DestroyWindow(mhwndDisplay);
		mhwndDisplay = NULL;
		mpDisplay = NULL;
	}

	if (mhMenuAuxCapture) {
		DestroyMenu(mhMenuAuxCapture);
		mhMenuAuxCapture = NULL;
	}

	if (mhMenuCapture) {
		DestroyMenu(mhMenuCapture);
		mhMenuCapture = NULL;
	}

	mhAccelCapture	= NULL;		// no need to destroy resource-based accelerators
	mpProject		= NULL;

	mUIPeer.Detach();

	VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)mhwnd);
	pFrame->Detach();

	InvalidateRect((HWND)mhwnd, NULL, TRUE);
	mhwnd = NULL;
}

void VDCaptureProjectUI::SetDisplayMode(DisplayMode mode) {
	if (mDisplayModeShadow != mode) {
		mDisplayModeShadow = mode;

		if (!mbDisplayModeShadowed)
			UpdateDisplayMode();
	}
}

DisplayMode VDCaptureProjectUI::GetDisplayMode() {
	return mDisplayModeShadow;
}

void VDCaptureProjectUI::SetDisplayAccelMode(DisplayAccelMode mode) {
	if (mode == mDisplayAccelMode)
		return;

	mDisplayAccelMode = mode;

	UpdateDisplayMode();
}

void VDCaptureProjectUI::SetPCMAudioFormat(sint32 sampling_rate, bool is_16bit, bool is_stereo) {
	WAVEFORMATEX wf;

	wf.wFormatTag		= WAVE_FORMAT_PCM;
	wf.nChannels		= (WORD)(is_stereo ? 2 : 1);
	wf.nSamplesPerSec	= sampling_rate;
	wf.wBitsPerSample	= (WORD)(is_16bit ? 16 : 8);
	wf.nAvgBytesPerSec	= sampling_rate * wf.nChannels * wf.wBitsPerSample/8;
	wf.nBlockAlign		= (WORD)(wf.nChannels * (wf.wBitsPerSample>>3));
	wf.cbSize			= 0;

	if (!mpProject->SetAudioFormat(wf, sizeof wf))
		VDDEBUG("Couldn't set audio format!\n");
}

void VDCaptureProjectUI::LoadLocalSettings() {
	// If the user has selected a default capture file, use it; if not, 

	char fn[MAX_PATH];
	if (QueryConfigString(g_szCapture, g_szDefaultCaptureFile, fn, sizeof fn) && fn[0])
		mpProject->SetCaptureFile(VDTextAToW(fn), false);

	// How about default capture settings?

	VDRegistryAppKey key(g_szCapture);

	{
		VDCaptureCompressionSpecs cs;

		memset(&g_compression, 0, sizeof g_compression);

		if (key.getBinary(g_szCompression, (char *)&cs, sizeof cs)) {
			if (cs.fccType != 'CDIV' || !cs.fccHandler) {
				// err... bad config data.

				DeleteConfigValue(g_szCapture, g_szCompression);
				DeleteConfigValue(g_szCapture, g_szCompressorData);
			} else {
				g_compression.cbSize		= sizeof(COMPVARS);
				g_compression.dwFlags		= ICMF_COMPVARS_VALID;
				g_compression.hic			= ICOpen(cs.fccType, cs.fccHandler, ICMODE_COMPRESS);
				g_compression.fccType		= cs.fccType;
				g_compression.fccHandler	= cs.fccHandler;
				g_compression.lKey			= cs.lKey;
				g_compression.lDataRate		= cs.lDataRate;
				g_compression.lQ			= cs.lQ;

				if (g_compression.hic) {
					int len = key.getBinaryLength(g_szCompressorData);

					if (len >= 0) {

						if (void *lpData = allocmem(len)) {
							memset(lpData, 0, len);

							if (key.getBinary(g_szCompressorData, (char *)lpData, len))
								ICSetState(g_compression.hic, lpData, len);

							freemem(lpData);
						}
					}
				} else
					g_compression.dwFlags = 0;
			}
		}
	}

	{
		CAPTUREPARMS *cp;
		DWORD dwSize, dwSizeAlloc;

		if (dwSize = QueryConfigBinary(g_szCapture, g_szCapSettings, NULL, 0)) {
			dwSizeAlloc = dwSize;
			if (dwSize < sizeof(CAPTUREPARMS)) dwSize = sizeof(CAPTUREPARMS);

			if (cp = (CAPTUREPARMS *)allocmem(dwSizeAlloc)) {
				memset(cp, 0, dwSizeAlloc);

				if (QueryConfigBinary(g_szCapture, g_szCapSettings, (char *)cp, dwSize)) {
					mpProject->SetHardwareBuffering(cp->wNumVideoRequested, cp->wNumAudioRequested, cp->dwAudioBufferSize);
					mpProject->SetFrameTime(cp->dwRequestMicroSecPerFrame);
					mpProject->SetAudioCaptureEnabled(cp->fCaptureAudio != 0);
				}

				freemem(cp);
			}
		}
	}

	// And default video parameters?

	{
		int len;

		len = key.getBinaryLength(g_szVideoFormat);
		if (len >= 0 && len >= sizeof(BITMAPINFOHEADER)) {
			vdstructex<BITMAPINFOHEADER> pbih;
			pbih.resize(len);

			if (key.getBinary(g_szAudioFormat, (char *)&*pbih, len))
				mpProject->SetVideoFormat(*pbih, pbih.size());
		}

		// Audio?
		len = key.getBinaryLength(g_szAudioFormat);
		if (len >= 0 && len >= sizeof(PCMWAVEFORMAT)) {
			vdstructex<WAVEFORMATEX> pwfex;
			pwfex.resize(len);

			if (key.getBinary(g_szAudioFormat, (char *)&*pwfex, len))
				mpProject->SetAudioFormat(*pwfex, pwfex.size());
		}
	}

	// stop conditions?

	{
		DWORD dwSize;

		if (dwSize = QueryConfigBinary(g_szCapture, g_szStopConditions, NULL, 0)) {
			vdblock<char> mem(dwSize);

			if (QueryConfigBinary(g_szCapture, g_szStopConditions, mem.data(), dwSize)) {
				VDCaptureStopPrefs stopPrefs;
				memset(&stopPrefs, 0, sizeof stopPrefs);
				memcpy(&stopPrefs, mem.data(), std::min<uint32>(sizeof stopPrefs, dwSize));

				mpProject->SetStopPrefs(stopPrefs);
			}
		}
	}

	// Disk I/O settings?

	VDCaptureDiskSettings diskSettings;

	diskSettings.mDiskChunkSize			= key.getInt(g_szChunkSize, 512);
	diskSettings.mDiskChunkCount		= key.getInt(g_szChunkCount, 2);
	diskSettings.mbDisableWriteCache	= 0 != key.getInt(g_szDisableBuffering, true);

	mpProject->SetDiskSettings(diskSettings);

	{
		DWORD dw;

		// panel, timing?

		if (QueryConfigDword(g_szCapture, g_szHideInfoPanel, &dw))
			mbInfoPanel = !!dw;

#if 0
#pragma vdpragma_TODO("FIXME")
		if (QueryConfigDword(g_szCapture, g_szAdjustVideoTiming, &dw))
			mpProject->SetSyncMode(dw ? IVDCaptureProject::kSyncToAudio : IVDCaptureProject::kSyncNone);
#endif

		// multisegment?

		if (QueryConfigDword(g_szCapture, g_szMultisegment, &dw)) {
			mpProject->SetSpillSystem(!!dw);
		}

		if (QueryConfigDword(g_szCapture, g_szAutoIncrement, &dw))
			mbAutoIncrementAfterCapture = !!dw;

		if (QueryConfigDword(g_szCapture, g_szStartOnLeft, &dw))
			mbStartOnLeft = !!dw;
	}

	// Spill settings?

	CapSpillRestoreFromRegistry();
}

void VDCaptureProjectUI::SaveLocalSettings() {
	VDRegistryAppKey key(g_szCapture);

	key.setInt(g_szHideInfoPanel, mbInfoPanel);
	key.setInt(g_szMultisegment, mpProject->IsSpillEnabled());
	key.setInt(g_szAutoIncrement, mbAutoIncrementAfterCapture);
	key.setInt(g_szStartOnLeft, mbStartOnLeft);
}

void VDCaptureProjectUI::SuspendDisplay(bool force) {
	if (!force) {
		switch(mDisplayModeShadow) {
		case kDisplayNone:
			return;

		case kDisplayHardware:
			if (!(mDriverOptions & CAPDRV_CRAPPY_OVERLAY))
				return;
			break;

		case kDisplaySoftware:
		case kDisplayAnalyze:
			if (!(mDriverOptions & CAPDRV_CRAPPY_PREVIEW))
				return;
			break;
		}
	}

	mbDisplayModeShadowed = true;
	mpProject->SetDisplayMode(kDisplayNone);
}

void VDCaptureProjectUI::ResumeDisplay() {
	if (mbDisplayModeShadowed) {
		mbDisplayModeShadowed = false;
		UpdateDisplayMode();
	}
}

void VDCaptureProjectUI::UpdateDisplayMode() {
	if ((mDisplayAccelMode || mpProject->IsVideoHistogramEnabled()) && mDisplayModeShadow == kDisplaySoftware) {
		mpProject->SetVideoFrameTransferEnabled(true);
		mpProject->SetDisplayMode(kDisplayAnalyze);
		return;
	}

	mpProject->SetDisplayMode(mDisplayModeShadow);
}

void VDCaptureProjectUI::UpdateDisplayPos() {
	const int xedge = GetSystemMetrics(SM_CXEDGE);
	const int yedge = GetSystemMetrics(SM_CYEDGE);
	vdrect32 r(ComputeDisplayArea());

	r.left		+= xedge;
	r.top		+= yedge;
	r.right		-= xedge;
	r.bottom	-= yedge;

	if (!mbStretchToWindow) {
		sint32 w, h;

		mpProject->GetPreviewImageSize(w, h);

		if (r.width() > w)
			r.right = r.left + w;
		if (r.height() > h)
			r.bottom = r.top + h;
	}

	mpProject->SetDisplayRect(r);

	SetWindowPos(mhwndDisplay, NULL, r.left, r.top, r.width(), r.height(), SWP_NOZORDER|SWP_NOACTIVATE);
}

vdrect32 VDCaptureProjectUI::ComputeDisplayArea() {
	RECT r;

	GetClientRect((HWND)mhwnd, &r);

	if (mbInfoPanel) {
		RECT rPanel;

		GetWindowRect(mhwndPanel, &rPanel);

		r.right -= (rPanel.right - rPanel.left);
	}

	if (mpVumeter)
		r.bottom -= mpVumeter->GetArea().height();

	if (mpVideoHistogram)
		r.bottom -= mpVideoHistogram->GetArea().height();

	RECT rStatus;
	GetWindowRect(mhwndStatus, &rStatus);
	r.bottom -= rStatus.bottom - rStatus.top;

	return vdrect32(r.left, r.top, r.right, r.bottom);
}

void VDCaptureProjectUI::UICaptureDriversUpdated() {
	int i;

	// delete existing strings
	for(i=0; DeleteMenu(mhMenuCapture, ID_VIDEO_CAPTURE_DRIVER+i, MF_BYCOMMAND); ++i)
		;

	// get drivers
	HMENU hmenu = GetSubMenu(mhMenuCapture, 2);
	const int n = mpProject->GetDriverCount();
	int driversFound = 0;

	for(i=0; i<n; ++i) {
		const wchar_t *name = mpProject->GetDriverName(i);

		wchar_t buf[1024];

		if ((unsigned)_snwprintf(buf, sizeof buf / sizeof buf[0], L"&%c %ls", '0'+i, name) < sizeof buf / sizeof buf[0]) {
			VDAppendMenuW32(hmenu, MF_ENABLED, ID_VIDEO_CAPTURE_DRIVER+i, buf);
			++driversFound;
		}
	}

	if (!driversFound)
		VDAppendMenuW32(hmenu, 0, 0, L"No drivers found");
}

bool VDCaptureProjectUI::InitVumeter() {
	if (mpVumeter)
		return true;

	mpVumeter = VDCreateUICaptureVumeter();
	mpVumeter->SetParent(&mUIPeer);
	VDUIParameters vumParams;
	vumParams.SetB(nsVDUI::kUIParam_Sunken, true);
	if (!mpVumeter->Create(&vumParams)) {
		ShutdownVumeter();
		return false;
	}

	mpProject->SetAudioVumeterEnabled(true);
	OnSize();
	return true;
}

void VDCaptureProjectUI::ShutdownVumeter() {
	mpProject->SetAudioVumeterEnabled(false);

	if (mpVumeter) {
		mpVumeter->Destroy();
		mpVumeter = NULL;
	}

	OnSize();
}

bool VDCaptureProjectUI::InitVideoHistogram() {
	if (mpVideoHistogram)
		return true;

	mpVideoHistogram = VDCreateUICaptureVideoHistogram();
	mpVideoHistogram->SetParent(&mUIPeer);
	VDUIParameters vumParams;
	vumParams.SetB(nsVDUI::kUIParam_Sunken, true);
	if (!mpVideoHistogram->Create(&vumParams)) {
		ShutdownVideoHistogram();
		return false;
	}

	mpProject->SetVideoHistogramEnabled(true);
	UpdateDisplayMode();
	OnSize();
	return true;
}

void VDCaptureProjectUI::ShutdownVideoHistogram() {
	mpProject->SetVideoHistogramEnabled(false);

	if (mpVideoHistogram) {
		mpVideoHistogram->Destroy();
		mpVideoHistogram = NULL;
	}

	UpdateDisplayMode();
	OnSize();
}

///////////////////////////////////////////////////////////////////////////
//
// callbacks
//
///////////////////////////////////////////////////////////////////////////

void VDCaptureProjectUI::UICaptureDriverChanged(int driver) {
	if (driver >= 0) {
		// attempt to load settings for this driver
		const wchar_t *name = mpProject->GetDriverName(driver);
		const long hash = VDCaptureHashDriverName(VDTextWToA(name).c_str());
		char buf[128];

		VDRegistryAppKey key(g_szCapture);

		wsprintf(buf, g_szDrvOpts, hash);
		mDriverOptions = key.getInt(buf, 0);

		switch(mDriverOptions & CAPDRV_DISPLAY_MASK) {
		case CAPDRV_DISPLAY_PREVIEW:
			SetDisplayMode(kDisplaySoftware);
			break;
		case CAPDRV_DISPLAY_OVERLAY:
			SetDisplayMode(kDisplayHardware);
			break;
		}
	}

	// update menu

	HMENU hMenu = GetMenu((HWND)mhwnd);
	bool bHardwareDisplayAvailable = mpProject->IsHardwareDisplayAvailable();
	
	EnableMenuItem(hMenu, ID_VIDEO_OVERLAY, bHardwareDisplayAvailable ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);

	if (!bHardwareDisplayAvailable && mDisplayModeShadow == kDisplayHardware)
		mDisplayModeShadow = kDisplaySoftware;

	if (mbDisplayModeShadowed)
		mpProject->SetDisplayMode(kDisplayNone);
	else
		UpdateDisplayMode();

	static const struct {
		UINT id;
		DriverDialog dlg;
	} kIDToDialogMap[]={
		{ ID_VIDEO_SOURCE, kDialogVideoSource },
		{ ID_VIDEO_FORMAT, kDialogVideoFormat },
		{ ID_VIDEO_DISPLAY, kDialogVideoDisplay },
		{ ID_VIDEO_CAPTUREPIN, kDialogVideoCapturePin },
		{ ID_VIDEO_PREVIEWPIN, kDialogVideoPreviewPin },
		{ ID_VIDEO_CAPTUREFILTER, kDialogVideoCaptureFilter },
		{ ID_VIDEO_CROSSBAR, kDialogVideoCrossbar },
		{ ID_VIDEO_CROSSBAR2, kDialogVideoCrossbar2 },
		{ ID_VIDEO_TUNER, kDialogTVTuner }
	};

	for(int i=0; i<sizeof kIDToDialogMap / sizeof kIDToDialogMap[0]; ++i)
		VDEnableMenuItemByCommandW32(hMenu, kIDToDialogMap[i].id, mpProject->IsDriverDialogSupported(kIDToDialogMap[i].dlg));

	CheckMenuRadioItem(hMenu, ID_VIDEO_CAPTURE_DRIVER, ID_VIDEO_CAPTURE_DRIVER+9, ID_VIDEO_CAPTURE_DRIVER+driver, MF_BYCOMMAND);
}

void VDCaptureProjectUI::UICaptureFileUpdated() {
	const char *pszAppend;
	const VDStringA nameA(VDTextWToA(mpProject->GetCaptureFile()));
	
	if (mbCaptureActive)
		pszAppend = " [capture in progress]";
	else if (0xFFFFFFFFUL != GetFileAttributes(nameA.c_str()))
		pszAppend = " [FILE EXISTS]";
	else
		pszAppend = "";

	if (mpProject->IsStripingEnabled())
		guiSetTitle((HWND)mhwnd, IDS_TITLE_CAPTURE2, nameA.c_str(), pszAppend);
	else
		guiSetTitle((HWND)mhwnd, IDS_TITLE_CAPTURE, nameA.c_str(), pszAppend);
}

void VDCaptureProjectUI::UICaptureVideoHistoBegin() {
}

void VDCaptureProjectUI::UICaptureVideoHisto(const float data[256]) {
	if (mpVideoHistogram)
		vdpoly_cast<IVDUICaptureVideoHistogram *>(mpVideoHistogram)->SetHistogram(data);
}

void VDCaptureProjectUI::UICaptureVideoHistoEnd() {
}

void VDCaptureProjectUI::UICaptureAudioFormatUpdated() {
	char bufa[64] = "(unknown)";

	if (!mpProject->IsAudioCaptureEnabled()) {
		strcpy(bufa, "No audio");
	} else {
		vdstructex<WAVEFORMATEX> wf;

		if (mpProject->GetAudioFormat(wf)) {
			if (wf->wFormatTag != WAVE_FORMAT_PCM) {
				wsprintf(bufa, "%d.%03dKHz", wf->nSamplesPerSec/1000, wf->nSamplesPerSec%1000);
			} else {
				PCMWAVEFORMAT *pwf = (PCMWAVEFORMAT *)wf.data();

				wsprintf(bufa, "%dK/%d/%c", (pwf->wf.nSamplesPerSec+500)/1000, pwf->wBitsPerSample, pwf->wf.nChannels>1?'s':'m');
			}
		}
	}

	SendMessage(mhwndStatus, SB_SETTEXT, 1 | SBT_POPOUT, (LPARAM)bufa);
}

void VDCaptureProjectUI::UICaptureVideoFormatUpdated() {
	UpdateDisplayPos();
}

void VDCaptureProjectUI::UICaptureParmsUpdated() {
	char bufv[64];

	strcpy(bufv,"(unknown)");

	long framePeriod = mpProject->GetFrameTime();
	double fps = 1000000.0f / (double)framePeriod;
	sint32 bandwidth = 0;

	sprintf(bufv, "%.02f fps", fps);

	vdstructex<BITMAPINFOHEADER> bih;

	if (mpProject->GetVideoFormat(bih)) {
		DWORD size = bih->biSizeImage;

		if (!size)
			size = bih->biHeight*bih->biPlanes * (((bih->biWidth * bih->biBitCount + 31)/32) * 4);

		bandwidth += MulDiv(
						8 + size,
						1000000,
						framePeriod);
	}

	if (mpProject->IsAudioCaptureEnabled()) {
		vdstructex<WAVEFORMATEX> wf;

		if (mpProject->GetAudioFormat(wf))
			bandwidth += 8 + wf->nAvgBytesPerSec;
	}

	SendMessage(mhwndStatus, SB_SETTEXT, 2 | SBT_POPOUT, (LPARAM)bufv);

	wsprintf(bufv, "%ldKB/s", (bandwidth+1023)>>10);
	SendMessage(mhwndStatus, SB_SETTEXT, 4, (LPARAM)bufv);
}

bool VDCaptureProjectUI::UICaptureAnalyzeBegin(const VDPixmap& px) {
	bool success = false;

#if 0
	int variant;
	int format = VDBitmapFormatToPixmapFormat(*bih, variant);

	if (format) {
		VDMakeBitmapCompatiblePixmapLayout(mDisplayLayout, bih->biWidth, bih->biHeight, format, variant);

		if (mDisplayAccelMode != kDDP_Both) {
			if (mDisplayAccelMode == kDDP_Bottom) {
				const VDPixmapFormatInfo& info = VDPixmapGetInfo(format);

				if (info.qh == 1)
					mDisplayLayout.data += mDisplayLayout.pitch;

				if (!info.auxhbits) {
					mDisplayLayout.data2 += mDisplayLayout.pitch2;
					mDisplayLayout.data3 += mDisplayLayout.pitch3;
				}
			}

			mDisplayLayout.h >>= 1;
			mDisplayLayout.pitch += mDisplayLayout.pitch;
			mDisplayLayout.pitch2 += mDisplayLayout.pitch2;
			mDisplayLayout.pitch3 += mDisplayLayout.pitch3;
		}
#endif

	// Try showing the window.  We may not be able to if the
	// overlay is already in use (ATI with integrated capture).
	if (mpDisplay->SetSource(false, px)) {
		success = true;
		ShowWindow(mhwndDisplay, SW_SHOWNA);
		mbDisplayAccelActive = true;
	}

	return success;
}

void VDCaptureProjectUI::UICaptureAnalyzeFrame(const VDPixmap& format) {
	if (format.format)
		mpDisplay->SetSource(true, format);
}

void VDCaptureProjectUI::UICaptureAnalyzeEnd() {
	mpDisplay->Reset();
	ShowWindow(mhwndDisplay, SW_HIDE);
	mbDisplayAccelActive = false;
}

void VDCaptureProjectUI::UICaptureAudioPeaksUpdated(float l, float r) {
	// This message arrives asynchronously so we must repost.
	mPeakL = l;
	mPeakR = r;

	PostMessage((HWND)mhwnd, WM_APP+1, 0, 0);
}

void VDCaptureProjectUI::UICaptureStart() {
	mbCaptureActive = true;
	UICaptureFileUpdated();

	mLastStatus.mFramesCaptured = 0;

	mVideoUncompressedSize = 0;
	mAudioUncompressedRate = 0;

	vdstructex<BITMAPINFOHEADER> bih;
	if (mpProject->GetVideoFormat(bih)) {
		if (!bih->biBitCount)
			mVideoUncompressedSize		= ((bih->biWidth * 2 + 3) & -3) * bih->biHeight;
		else
			mVideoUncompressedSize		= ((bih->biWidth * ((bih->biBitCount + 7)/8) + 3) & -3) * bih->biHeight;
	}

	vdstructex<WAVEFORMATEX> wfex;
	if (mpProject->GetAudioFormat(wfex) && wfex->wFormatTag != WAVE_FORMAT_PCM)
		mAudioUncompressedRate = wfex->nSamplesPerSec * wfex->nChannels * 2;

	if (mbDisplayLargeTimer)
		mhClockFont = CreateFont(200, 0,
								0, 0, 0,
								FALSE, FALSE, FALSE,
								ANSI_CHARSET,
								OUT_DEFAULT_PRECIS,
								CLIP_DEFAULT_PRECIS,
								DEFAULT_QUALITY,
								FF_DONTCARE|DEFAULT_PITCH,
								"Arial");

	if (mbHideOnCapture)
		mpProject->SetDisplayVisibility(false);

	mCPUReader.Init();
}

bool VDCaptureProjectUI::UICapturePreroll() {
	return IDOK == MessageBox((HWND)mhwnd, "VirtualDub notice", "Ready to capture.", MB_OKCANCEL);
}

void VDCaptureProjectUI::UICaptureStatusUpdated(VDCaptureStatus& status) {
	mCurStatus = status;

	PostMessage((HWND)mhwnd, WM_APP, 0, 0);
}

void VDCaptureProjectUI::UICaptureEnd(bool success) {
	mCPUReader.Shutdown();

	if (mbHideOnCapture)
		mpProject->SetDisplayVisibility(false);

	mbCaptureActive = false;
	UICaptureFileUpdated();

	if (mhClockFont) {
		DeleteObject(mhClockFont);
		mhClockFont = NULL;
	}

	if (success && mbAutoIncrementAfterCapture)
		mpProject->IncrementFileID();
}

LRESULT CALLBACK VDCaptureProjectUI::StaticStatusWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDCaptureProjectUI *pThis = (VDCaptureProjectUI *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	return pThis->StatusWndProc(hwnd, msg, wParam, lParam);
}

LRESULT VDCaptureProjectUI::StatusWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_LBUTTONDOWN:
		{
			POINT pt = { (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam) };

			for(int i=0; i<2; i++) {
				RECT r2;
				if (SendMessage(hwnd, SB_GETRECT, i+1, (LPARAM)&r2)) {
					if (PtInRect(&r2, pt)) {
						MapWindowPoints(hwnd, NULL, (LPPOINT)&r2, 2);

						unsigned len = LOWORD(SendMessage(hwnd, SB_GETTEXTLENGTH, i+1, 0));

						std::vector<char> str(len+1, 0);

						SendMessage(hwnd, SB_GETTEXT, i+1, (LPARAM)&str[0]);
						SendMessage(hwnd, SB_SETTEXT, (i+1), (LPARAM)&str[0]);

						SetForegroundWindow((HWND)mhwnd);
						TrackPopupMenu(
								GetSubMenu(mhMenuAuxCapture, i),
								TPM_BOTTOMALIGN | TPM_RIGHTALIGN | TPM_LEFTBUTTON,
								r2.right, r2.top,
								0, (HWND)mhwnd, NULL);

						SendMessage(hwnd, SB_SETTEXT, (i+1) | SBT_POPOUT, (LPARAM)&str[0]);
						PostMessage((HWND)mhwnd, WM_NULL, 0, 0);
						break;
					}
				}
			}
		}
		break;
	}

	return CallWindowProc(mStatusWndProc, hwnd, msg, wParam, lParam);
}

LRESULT VDCaptureProjectUI::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return (this->*mpWndProc)(msg, wParam, lParam);
}

LRESULT VDCaptureProjectUI::RestrictedWndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case ID_CAPTURE_STOP:
				mpProject->CaptureStop();
				return 0;
			}
			return 0;

		case WM_CLOSE:
			return 0;
			 
		case WM_INITMENU:
			OnInitMenu((HMENU)wParam);
			break;

		case WM_SIZE:
			OnSize();
			break;

		case WM_APP:
			OnUpdateStatus();
			break;

		case WM_APP+1:
			OnUpdateVumeter();
			break;
	}

	return VDUIFrame::GetFrame((HWND)mhwnd)->DefProc((HWND)mhwnd, msg, wParam, lParam);
}

LRESULT VDCaptureProjectUI::MainWndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	if (mbCaptureActive)
		return RestrictedWndProc(msg, wParam, lParam);

    switch (msg) {
		case WM_ENTERMENULOOP:
			SuspendDisplay();
			break;

		case WM_EXITMENULOOP:
			ResumeDisplay();
			break;

		case WM_INITMENU:
			OnInitMenu((HMENU)wParam);
			break;

		case WM_COMMAND:
			if (!OnCommand(LOWORD(wParam)))
				return VDUIFrame::GetFrame((HWND)mhwnd)->DefProc((HWND)mhwnd, msg, wParam, lParam);
			break;

		case WM_MENUSELECT:
			guiMenuHelp((HWND)mhwnd, wParam, 0, iCaptureMenuHelpTranslator);
			break;

		case WM_TIMER:
			OnTimer();
			break;

		case WM_SIZE:
			OnSize();
			break;

		case WM_DESTROY:		// doh!!!!!!!
			PostQuitMessage(0);
			break;

		case WM_PARENTNOTIFY:
			if (OnParentNotify(wParam, lParam))
				break;
			if (LOWORD(wParam) != WM_LBUTTONDOWN)
				break;
			// fall through
		case WM_LBUTTONDOWN:
			if (mbStartOnLeft)
				OnCommand(ID_CAPTURE_CAPTUREVIDEOINTERNAL);
			break;

		case WM_PAINT:
			OnPaint();
			return 0;

		case WM_APP:
			OnUpdateStatus();
			break;

		case WM_APP+1:
			OnUpdateVumeter();
			break;

		default:
			return VDUIFrame::GetFrame((HWND)mhwnd)->DefProc((HWND)mhwnd, msg, wParam, lParam);
    }
    return (0);
}

void VDCaptureProjectUI::OnInitMenu(HMENU hMenu) {
	const VDCaptureFilterSetup& filtsetup = mpProject->GetFilterSetup();

	const bool driverPresent = mpProject->IsDriverConnected();

	VDEnableMenuItemByCommandW32(hMenu, ID_AUDIO_ENABLE,			mpProject->IsAudioCaptureAvailable());
	VDCheckMenuItemByCommandW32	(hMenu, ID_AUDIO_ENABLE,			mpProject->IsAudioCaptureEnabled());
	VDCheckMenuItemByCommandW32	(hMenu, ID_AUDIO_PEAKMETER,			mpProject->IsAudioVumeterEnabled());

	VDCheckMenuItemByCommandW32	(hMenu, ID_VIDEO_NODISPLAY,			mDisplayModeShadow == kDisplayNone);
	VDCheckMenuItemByCommandW32	(hMenu, ID_VIDEO_OVERLAY,			mDisplayModeShadow == kDisplayHardware);
	VDCheckMenuItemByCommandW32	(hMenu, ID_VIDEO_PREVIEW,			mDisplayModeShadow == kDisplaySoftware);
	VDCheckMenuItemByCommandW32	(hMenu, ID_VIDEO_HISTOGRAM,			mpProject->IsVideoHistogramEnabled());

	VDCheckMenuItemByCommandW32	(hMenu, ID_VIDEO_STRETCH,			mbStretchToWindow);
	VDCheckMenuItemByCommandW32	(hMenu, ID_VIDEO_ENABLEFILTERING,	filtsetup.mbEnableRGBFiltering);
	VDCheckMenuItemByCommandW32	(hMenu, ID_VIDEO_NOISEREDUCTION,	filtsetup.mbEnableNoiseReduction);
	VDCheckMenuItemByCommandW32	(hMenu, ID_VIDEO_SWAPFIELDS,		filtsetup.mbEnableFieldSwap);
	VDCheckMenuItemByCommandW32	(hMenu, ID_VIDEO_SQUISH_RANGE,		filtsetup.mbEnableLumaSquish);

	VDCheckMenuItemByCommandW32	(hMenu, ID_VIDEO_VRNONE,			filtsetup.mVertSquashMode == IVDCaptureFilterSystem::kFilterDisable);
	VDCheckMenuItemByCommandW32	(hMenu, ID_VIDEO_VR2LINEAR,			filtsetup.mVertSquashMode == IVDCaptureFilterSystem::kFilterLinear);
	VDCheckMenuItemByCommandW32	(hMenu, ID_VIDEO_VR2CUBIC,			filtsetup.mVertSquashMode == IVDCaptureFilterSystem::kFilterCubic);

	VDEnableMenuItemByCommandW32(hMenu, ID_CAPTURE_CAPTUREVIDEO,	driverPresent && !mbCaptureActive);
	VDEnableMenuItemByCommandW32(hMenu, ID_CAPTURE_TEST,			driverPresent && !mbCaptureActive);
	VDEnableMenuItemByCommandW32(hMenu, ID_CAPTURE_STOP,			mbCaptureActive);

	VDCheckMenuItemByCommandW32	(hMenu, ID_CAPTURE_HIDEONCAPTURE,	mbHideOnCapture);
	VDCheckMenuItemByCommandW32	(hMenu, ID_CAPTURE_DISPLAYLARGETIMER, mbDisplayLargeTimer);
	VDCheckMenuItemByCommandW32	(hMenu, ID_CAPTURE_INFOPANEL,		mbInfoPanel);
	VDCheckMenuItemByCommandW32	(hMenu, ID_CAPTURE_ENABLESPILL,		mpProject->IsSpillEnabled());
	VDCheckMenuItemByCommandW32	(hMenu, ID_CAPTURE_AUTOINCREMENT,	mbAutoIncrementAfterCapture);
	VDCheckMenuItemByCommandW32	(hMenu, ID_CAPTURE_STARTONLEFT,		mbStartOnLeft);

	VDCheckMenuItemByCommandW32	(hMenu, ID_CAPTURE_HWACCEL_NONE,	mDisplayAccelMode == kDDP_Off);
	VDCheckMenuItemByCommandW32	(hMenu, ID_CAPTURE_HWACCEL_TOP,		mDisplayAccelMode == kDDP_Top);
	VDCheckMenuItemByCommandW32	(hMenu, ID_CAPTURE_HWACCEL_BOTTOM,	mDisplayAccelMode == kDDP_Bottom);
	VDCheckMenuItemByCommandW32	(hMenu, ID_CAPTURE_HWACCEL_BOTH,	mDisplayAccelMode == kDDP_Both);
}

void VDCaptureProjectUI::OnPaint() {
	PAINTSTRUCT ps;

	if (HDC hdc = BeginPaint((HWND)mhwnd, &ps)) {
		const vdrect32 r(ComputeDisplayArea());

		RECT r2={r.left,r.top,r.right,r.bottom};

		DrawEdge(hdc, &r2, EDGE_SUNKEN, BF_RECT|BF_ADJUST);
		FillRect(hdc, &r2, (HBRUSH)(COLOR_3DFACE+1));
		EndPaint((HWND)mhwnd, &ps);
	}
}

void VDCaptureProjectUI::OnTimer() {
	uint32 fc = mpProject->GetPreviewFrameCount();

	if (!fc || fc < mLastPreviewFrameCount) {
		if (mLastPreviewFrameCount)
			SendMessage(mhwndStatus, SB_SETTEXT, 3, (LPARAM)L"");
	} else {
		char buf[64];
		wsprintf(buf, "%u fps", fc - mLastPreviewFrameCount);
		SendMessage(mhwndStatus, SB_SETTEXT, 3, (LPARAM)buf);
	}

	mLastPreviewFrameCount = fc;
}

void VDCaptureProjectUI::OnSize() {
	RECT rClient, rStatus, rPanel={0,0,0,0};
	INT aWidth[8];
	int nParts;
	HDWP hdwp;

	GetClientRect((HWND)mhwnd, &rClient);
	GetWindowRect(mhwndStatus, &rStatus);

	if (mbInfoPanel)
		GetWindowRect(mhwndPanel, &rPanel);

	hdwp = BeginDeferWindowPos(2);

	int vhistoHt = 0;
	if (mpVideoHistogram) {
		vhistoHt = 128;
	}

	int vumeterHt = 0;
	if (mpVumeter) {
		vumeterHt = 32;

		if (HDC hdc = GetDC((HWND)mhwnd)) {
			HFONT hfont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

			if (HGDIOBJ hOldFont = SelectObject(hdc, hfont)) {
				TEXTMETRIC tm;
				if (GetTextMetrics(hdc, &tm)) {
					vumeterHt = tm.tmHeight*4;
				}

				SelectObject(hdc, hOldFont);
			}

			ReleaseDC((HWND)mhwnd, hdc);
		}
	}

	int y_bottom = rClient.bottom;

	const int infoPanelWidth = mbInfoPanel ? rPanel.right - rPanel.left : 0;
	const int statusHt = rStatus.bottom - rStatus.top;

	guiDeferWindowPos(hdwp, mhwndStatus,
				NULL,
				rClient.left,
				y_bottom - statusHt,
				rClient.right-rClient.left,
				y_bottom,
				SWP_NOACTIVATE|SWP_NOZORDER/*|SWP_NOCOPYBITS*/);

	y_bottom -= statusHt;

	if (mpVumeter) {
		mpVumeter->SetArea(vduirect(rClient.left, y_bottom - vumeterHt, rClient.right, y_bottom));
		y_bottom -= vumeterHt;
	}

	if (mpVideoHistogram) {
		mpVideoHistogram->SetArea(vduirect(rClient.left, y_bottom - vhistoHt, rClient.right - infoPanelWidth, y_bottom));
		y_bottom -= vhistoHt;
	}

	if (mbInfoPanel) {
		guiDeferWindowPos(hdwp, mhwndPanel,
					NULL,
					rClient.right - (rPanel.right - rPanel.left),
					rClient.top,
					rPanel.right - rPanel.left,
					rClient.bottom - statusHt - vumeterHt,
					SWP_NOACTIVATE|SWP_NOZORDER/*|SWP_NOCOPYBITS*/);
	}

	guiEndDeferWindowPos(hdwp);

	UpdateDisplayPos();

	if ((nParts = SendMessage(mhwndStatus, SB_GETPARTS, 0, 0))>1) {
		int i;
		INT xCoord = (rClient.right-rClient.left) - (rStatus.bottom-rStatus.top);

		aWidth[nParts-2] = xCoord;

		for(i=nParts-3; i>=0; i--) {
			xCoord -= 60;
			aWidth[i] = xCoord;
		}
		aWidth[nParts-1] = -1;

		SendMessage(mhwndStatus, SB_SETPARTS, nParts, (LPARAM)aWidth);
	}

	InvalidateRect((HWND)mhwnd, NULL, FALSE);
}

bool VDCaptureProjectUI::OnParentNotify(WPARAM wParam, LPARAM lParam) {
	if (LOWORD(wParam) != WM_LBUTTONDOWN)
		return false;

	POINT pt = { (SHORT)LOWORD(lParam), (SHORT)HIWORD(lParam) };
	RECT r;

	GetWindowRect(mhwndStatus, &r);
	MapWindowPoints(NULL, (HWND)mhwnd, (LPPOINT)&r, 2);
	if (PtInRect(&r, pt))
		return true;		// always eat status bar messages

	return false;
}

bool VDCaptureProjectUI::OnCommand(UINT id) {
	switch(id) {
	case ID_FILE_SETCAPTUREFILE:
		SuspendDisplay();
		{
			static const VDFileDialogOption opts[]={
				{ VDFileDialogOption::kBool, 0, L"Set this capture filename as the default." },
				{0}
			};

			int optvals[1]={false};

			const VDStringW capfile(VDGetSaveFileName(VDFSPECKEY_CAPTURENAME, mhwnd, L"Set Capture File", L"Audio-Video Interleave (*.avi)\0*.avi\0All Files (*.*)\0*.*\0", g_prefs.main.fAttachExtension ? L"avi" : NULL, opts, optvals));

			if (!capfile.empty()) {
				mpProject->SetCaptureFile(capfile, false);

				if (optvals[0]) {
					VDRegistryAppKey key("Capture");

					key.setString(g_szDefaultCaptureFile, capfile.c_str());
				}
			}
		}
		ResumeDisplay();
		break;
	case ID_FILE_SETSTRIPINGSYSTEM:
		SuspendDisplay();
		{
			const VDStringW capfile(VDGetSaveFileName(VDFSPECKEY_CAPTURENAME, mhwnd, L"Select Striping System for Internal Capture", L"AVI Stripe System (*.stripe)\0*.stripe\0All Files (*.*)\0*.*\0", g_prefs.main.fAttachExtension ? L"stripe" : NULL));

			if (!capfile.empty()) {
				try {
					mpProject->SetCaptureFile(capfile, true);

					UICaptureFileUpdated();
				} catch(const MyError& e) {
					e.post((HWND)mhwnd, g_szError);
				}
			}
		}
		ResumeDisplay();
		break;

	case ID_FILE_ALLOCATEDISKSPACE:
		extern bool VDShowCaptureAllocateDialog(VDGUIHandle hwndParent, const VDStringW& path);

		SuspendDisplay();
		VDShowCaptureAllocateDialog(mhwnd, mpProject->GetCaptureFile());
		ResumeDisplay();
		break;

	case ID_FILE_INCREMENT:
		mpProject->IncrementFileID();
		break;

	case ID_FILE_DECREMENT:
		mpProject->DecrementFileID();
		break;

	case ID_FILE_EXITCAPTUREMODE:
		{
			VDUIFrame *pFrame = VDUIFrame::GetFrame((HWND)mhwnd);

			pFrame->SetNextMode(2);
			pFrame->Detach();
		}
		break;
	case ID_AUDIO_ENABLE:
		mpProject->SetAudioCaptureEnabled(!mpProject->IsAudioCaptureEnabled());
		break;
	case ID_AUDIO_PEAKMETER:
		if (mpVumeter)
			ShutdownVumeter();
		else
			InitVumeter();
		break;
	case ID_AUDIO_RAWCAPTUREFORMAT:
		{
			extern int VDShowCaptureRawAudioFormatDialog(VDGUIHandle h, const std::list<vdstructex<WAVEFORMATEX> >& formats, int sel);

			std::list<vdstructex<WAVEFORMATEX> > aformats;

			mpProject->GetAvailableAudioFormats(aformats);

			int sel = VDShowCaptureRawAudioFormatDialog(mhwnd, aformats, -1);

			if (sel >= 0) {
				std::list<vdstructex<WAVEFORMATEX> >::const_iterator it(aformats.begin());

				std::advance(it, sel);

				const vdstructex<WAVEFORMATEX>& wfex = *it;
				mpProject->SetAudioFormat(*wfex, wfex.size());
			}
		}
		break;
	case ID_AUDIO_COMPRESSION:
		{
			vdstructex<WAVEFORMATEX> wfex;
			vdstructex<WAVEFORMATEX> wfexSrc;
			WAVEFORMATEX *pwfexSrc = NULL;
			WAVEFORMATEX *pwfexOld = NULL;

			if (mpProject->GetAudioCompFormat(wfex)) {
				size_t len = wfex.size();
				pwfexOld = (WAVEFORMATEX*)malloc(len);
				memcpy(pwfexOld, wfex.data(), len);
			}
			
			if (mpProject->GetAudioFormat(wfexSrc))
				pwfexSrc = wfexSrc.data();

			// pwfexOld is freed by AudioChooseCompressor
			WAVEFORMATEX *pwfexNew = AudioChooseCompressor((HWND)mhwnd, pwfexOld, pwfexSrc);

			if (!pwfexNew)
				mpProject->SetAudioCompFormat();
			else if (pwfexNew) {
				mpProject->SetAudioCompFormat(*pwfexNew, sizeof(WAVEFORMATEX) + pwfexNew->cbSize);

				free(pwfexNew);
			}
		}
		break;
	case ID_AUDIO_WINMIXER:
		ShellExecute((HWND)mhwnd, NULL, "sndvol32.exe", "/r", NULL, SW_SHOWNORMAL);
		break;
	case ID_VIDEO_NODISPLAY:
		SetDisplayMode(kDisplayNone);
		break;
	case ID_VIDEO_OVERLAY:
		SetDisplayMode(kDisplayHardware);
		break;
	case ID_VIDEO_PREVIEW:
		SetDisplayMode(kDisplaySoftware);
		break;
	case ID_VIDEO_HISTOGRAM:
		if (mpProject->IsVideoHistogramEnabled())
			ShutdownVideoHistogram();
		else
			InitVideoHistogram();
		break;

	case ID_VIDEO_STRETCH:
		mbStretchToWindow = !mbStretchToWindow;
		UpdateDisplayPos();
		break;

	case ID_VIDEO_FORMAT:
	case ID_VIDEO_CUSTOMFORMAT:
		SuspendDisplay(true);
		if (id == ID_VIDEO_CUSTOMFORMAT)
			DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_CUSTOMVIDEO), (HWND)mhwnd, CaptureCustomVidSizeDlgProc, (LPARAM)static_cast<IVDCaptureProject *>(mpProject));
		else
			mpProject->DisplayDriverDialog(kDialogVideoFormat);
		ResumeDisplay();
		break;

	case ID_VIDEO_SOURCE:
		mpProject->DisplayDriverDialog(kDialogVideoSource);
		break;
	case ID_VIDEO_DISPLAY:
		mpProject->DisplayDriverDialog(kDialogVideoDisplay);
		break;
	case ID_VIDEO_CAPTUREPIN:
		mpProject->DisplayDriverDialog(kDialogVideoCapturePin);
		break;
	case ID_VIDEO_PREVIEWPIN:
		mpProject->DisplayDriverDialog(kDialogVideoPreviewPin);
		break;
	case ID_VIDEO_CAPTUREFILTER:
		SuspendDisplay(true);
		mpProject->DisplayDriverDialog(kDialogVideoCaptureFilter);
		ResumeDisplay();
		break;
	case ID_VIDEO_CROSSBAR:
		mpProject->DisplayDriverDialog(kDialogVideoCrossbar);
		break;
	case ID_VIDEO_CROSSBAR2:
		mpProject->DisplayDriverDialog(kDialogVideoCrossbar2);
		break;
	case ID_VIDEO_TUNER:
		mpProject->DisplayDriverDialog(kDialogTVTuner);
		break;
	case ID_VIDEO_COMPRESSION:
		SuspendDisplay();
		{
			if (!(g_compression.dwFlags & ICMF_COMPVARS_VALID)) {
				memset(&g_compression, 0, sizeof g_compression);
				g_compression.dwFlags |= ICMF_COMPVARS_VALID;
				g_compression.lQ = 10000;
			}

			g_compression.cbSize = sizeof(COMPVARS);

			vdstructex<BITMAPINFOHEADER> bih;

			if (mpProject->GetVideoFormat(bih))
				ChooseCompressor((HWND)mhwnd, &g_compression, bih.data());
			else
				ChooseCompressor((HWND)mhwnd, &g_compression, NULL);
		}
		ResumeDisplay();
		break;

	case ID_VIDEO_FILTERS:
		SuspendDisplay(mbDisplayAccelActive);
		ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_FILTERS), (HWND)mhwnd, FilterDlgProc);
		ResumeDisplay();
		break;

	case ID_VIDEO_ENABLEFILTERING:
		{
			VDCaptureFilterSetup filtsetup = mpProject->GetFilterSetup();
			filtsetup.mbEnableRGBFiltering = !filtsetup.mbEnableRGBFiltering;
			mpProject->SetFilterSetup(filtsetup);
		}
		break;

	case ID_VIDEO_NOISEREDUCTION:
		{
			VDCaptureFilterSetup filtsetup = mpProject->GetFilterSetup();
			filtsetup.mbEnableNoiseReduction = !filtsetup.mbEnableNoiseReduction;
			mpProject->SetFilterSetup(filtsetup);
		}
		break;
	case ID_VIDEO_NOISEREDUCTION_THRESHOLD:
		VDShowCaptureToggleNRDialog(mhwnd);
		break;

	case ID_VIDEO_SWAPFIELDS:
		{
			VDCaptureFilterSetup filtsetup = mpProject->GetFilterSetup();
			filtsetup.mbEnableFieldSwap = !filtsetup.mbEnableFieldSwap;
			mpProject->SetFilterSetup(filtsetup);
		}
		break;

	case ID_VIDEO_SQUISH_RANGE:
		{
			VDCaptureFilterSetup filtsetup = mpProject->GetFilterSetup();
			filtsetup.mbEnableLumaSquish = !filtsetup.mbEnableLumaSquish;
			mpProject->SetFilterSetup(filtsetup);
		}
		break;

	case ID_VIDEO_VRNONE:
		{
			VDCaptureFilterSetup filtsetup = mpProject->GetFilterSetup();
			filtsetup.mVertSquashMode = IVDCaptureFilterSystem::kFilterDisable;
			mpProject->SetFilterSetup(filtsetup);
		}
		break;
	case ID_VIDEO_VR2LINEAR:
		{
			VDCaptureFilterSetup filtsetup = mpProject->GetFilterSetup();
			filtsetup.mVertSquashMode = IVDCaptureFilterSystem::kFilterLinear;
			mpProject->SetFilterSetup(filtsetup);
		}
		break;
	case ID_VIDEO_VR2CUBIC:
		{
			VDCaptureFilterSetup filtsetup = mpProject->GetFilterSetup();
			filtsetup.mVertSquashMode = IVDCaptureFilterSystem::kFilterCubic;
			mpProject->SetFilterSetup(filtsetup);
		}
		break;

	case ID_VIDEO_CLIPPING_SET:
		VDShowCaptureCroppingDialog(mhwnd, mpProject);
		break;

	case ID_VIDEO_BT8X8TWEAKER:
		CaptureDisplayBT848Tweaker((HWND)mhwnd);
		break;

	case ID_VIDEO_DISCONNECT:
		mpProject->SelectDriver(-1);
		break;

	case ID_CAPTURE_SETTINGS:
		{
			extern bool VDShowCaptureSettingsDialog(VDGUIHandle hwndParent, CAPTUREPARMS& parms);

			SuspendDisplay(mbDisplayAccelActive);

			CAPTUREPARMS cp={0};
			int videoCount, audioCount, audioSize;

			mpProject->GetHardwareBuffering(videoCount, audioCount, audioSize);

			cp.dwRequestMicroSecPerFrame = mpProject->GetFrameTime();
			cp.wNumVideoRequested = videoCount;
			cp.wNumAudioRequested = audioCount;
			cp.dwAudioBufferSize = audioSize;
			cp.fCaptureAudio = mpProject->IsAudioCaptureEnabled();

			if (VDShowCaptureSettingsDialog(mhwnd, cp)) {
				mpProject->SetHardwareBuffering(cp.wNumVideoRequested, cp.wNumAudioRequested, cp.dwAudioBufferSize);
				mpProject->SetFrameTime(cp.dwRequestMicroSecPerFrame);
				mpProject->SetAudioCaptureEnabled(cp.fCaptureAudio != 0);
			}

			ResumeDisplay();
		}
		break;

	case ID_CAPTURE_PREFERENCES:
		SuspendDisplay();
#pragma vdpragma_TODO("FIXME")
//		DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_PREFERENCES), (HWND)mhwnd, CapturePreferencesDlgProc, (LPARAM)hWndCapture);
		extern void VDShowCapturePreferencesDialog(VDGUIHandle h);

		VDShowCapturePreferencesDialog(mhwnd);
		ResumeDisplay();
		break;

	case ID_CAPTURE_STOPCONDITIONS:
		{
			bool VDShowCaptureStopPrefsDialog(VDGUIHandle hwndParent, VDCaptureStopPrefs& prefs);

			VDCaptureStopPrefs stopPrefs(mpProject->GetStopPrefs());

			SuspendDisplay();
			if (VDShowCaptureStopPrefsDialog(mhwnd, stopPrefs)) {
				mpProject->SetStopPrefs(stopPrefs);
				SetConfigBinary(g_szCapture, g_szStopConditions, (char *)&stopPrefs, sizeof stopPrefs);
			}
			ResumeDisplay();
		}
		break;

	case ID_CAPTURE_TIMING:
		{
			extern bool VDShowCaptureTimingDialog(VDGUIHandle h, VDCaptureTimingSetup& timing);

			VDCaptureTimingSetup timing(mpProject->GetTimingSetup());

			if (VDShowCaptureTimingDialog((VDGUIHandle)mhwnd, timing))
				mpProject->SetTimingSetup(timing);
		}
		break;

	case ID_CAPTURE_DISKIO:
		{
			VDCaptureDiskSettings diskSettings(mpProject->GetDiskSettings());

			SuspendDisplay();
			if (VDShowCaptureDiskIODialog((VDGUIHandle)mhwnd, diskSettings))
				mpProject->SetDiskSettings(diskSettings);
			ResumeDisplay();
		}
		break;

	case ID_CAPTURE_SPILLSYSTEM:
		SuspendDisplay();
		DialogBox(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_SPILLSETUP), (HWND)mhwnd, CaptureSpillDlgProc);
		ResumeDisplay();
		break;

	case ID_CAPTURE_CAPTUREVIDEO:
		mpProject->Capture(false);
		break;

	case ID_CAPTURE_TEST:
		mpProject->Capture(true);
		break;

	case ID_CAPTURE_SHOWPROFILER:
		extern void VDOpenProfileWindow();
		VDOpenProfileWindow();
		break;

	case ID_CAPTURE_HIDEONCAPTURE:
		mbHideOnCapture = !mbHideOnCapture;
		break;

	case ID_CAPTURE_DISPLAYLARGETIMER:
		mbDisplayLargeTimer = !mbDisplayLargeTimer;
		break;

	case ID_CAPTURE_INFOPANEL:
		mbInfoPanel = !mbInfoPanel;
		OnSize();
		ShowWindow(GetDlgItem((HWND)mhwnd, IDC_CAPTURE_PANEL), mbInfoPanel ? SW_SHOW : SW_HIDE);
		InvalidateRect((HWND)mhwnd, NULL, TRUE);
		break;

	case ID_CAPTURE_ENABLESPILL:
		mpProject->SetSpillSystem(!mpProject->IsSpillEnabled());
		break;

	case ID_CAPTURE_STARTONLEFT:
		mbStartOnLeft = !mbStartOnLeft;
		break;

	case ID_CAPTURE_AUTOINCREMENT:
		mbAutoIncrementAfterCapture = !mbAutoIncrementAfterCapture;
		break;

	case ID_CAPTURE_HWACCEL_NONE:
		SetDisplayAccelMode(kDDP_Off);
		break;

	case ID_CAPTURE_HWACCEL_TOP:
		SetDisplayAccelMode(kDDP_Top);
		break;

	case ID_CAPTURE_HWACCEL_BOTTOM:
		SetDisplayAccelMode(kDDP_Bottom);
		break;

	case ID_CAPTURE_HWACCEL_BOTH:
		SetDisplayAccelMode(kDDP_Both);
		break;

	case ID_HELP_CONTENTS:
		VDShowHelp((HWND)mhwnd);
		break;

	default:
		if (id >= ID_VIDEO_CAPTURE_DRIVER && id < ID_VIDEO_CAPTURE_DRIVER+10) {
			mpProject->SelectDriver(id - ID_VIDEO_CAPTURE_DRIVER);
		} else if (id >= ID_AUDIOMODE_11KHZ_8MONO && id <= ID_AUDIOMODE_44KHZ_16STEREO) {
			id -= ID_AUDIOMODE_11KHZ_8MONO;
			SetPCMAudioFormat(
					11025<<(id/4),
					0 != (id & 2),
					0 != (id & 1));
		} else if (id >= ID_FRAMERATE_6000FPS && id <= ID_FRAMERATE_1493FPS) {
			mpProject->SetFrameTime(g_predefFrameRates[id - ID_FRAMERATE_6000FPS]);
		} else
			return false;

		break;
	}

	return true;
}

void VDCaptureProjectUI::OnUpdateVumeter() {
	if (mpVumeter) {
		IVDUICaptureVumeter *pVumeter = vdpoly_cast<IVDUICaptureVumeter *>(mpVumeter);

		pVumeter->SetPeakLevels(mPeakL, mPeakR);
	}
}

void VDCaptureProjectUI::OnUpdateStatus() {
	if (mbInfoPanel && mhwndPanel)
		SendMessage(mhwndPanel, WM_APP, 0, (LPARAM)&mCurStatus);

	if (!mCurStatus.mFramesCaptured)
		return;

	long totalSizeK = (long)((mCurStatus.mTotalVideoSize + mCurStatus.mTotalAudioSize + 1023) >> 10);
	long jitter = 0;
	long disp	= 0;
	long frameDelta1K = (mCurStatus.mFramesCaptured - mLastStatus.mFramesCaptured)*1000;

	if (frameDelta1K > 0) {
		jitter = (long)(mCurStatus.mTotalJitter / frameDelta1K);
		disp = (long)(mCurStatus.mTotalDisp / frameDelta1K);
	}

	char buf[1024];

	if (mbInfoPanel) {
		sprintf(buf, "%ld frames (%ld dropped), %.3fs, %ldms jitter, %ldms disp, %ld frame size, %ldK total"
					, mCurStatus.mFramesCaptured
					, mCurStatus.mFramesDropped
					, mCurStatus.mElapsedTimeMS / 1000.0
					, jitter
					, disp
					, (long)(mCurStatus.mTotalVideoSize / mCurStatus.mFramesCaptured)
					, totalSizeK
					);
	} else {
		sprintf(buf, "%ldus jitter, %ldus disp, %ldK total, spill seg #%d"
					, jitter
					, disp
					, totalSizeK
					, mCurStatus.mCurrentSegment+1
					);
	}

	SendMessage(mhwndStatus, SB_SETTEXT, 0, (LPARAM)buf);
	RedrawWindow(mhwndStatus, NULL, NULL, RDW_INVALIDATE|RDW_UPDATENOW);

	if (mhClockFont) {
		int y;

		if (mpVumeter) {
			const vduirect& r = mpVumeter->GetArea();

			y = r.top;
		} else {
			RECT r;

			GetWindowRect(mhwndStatus, &r);
			ScreenToClient((HWND)mhwnd, (LPPOINT)&r);

			y = r.top;
		}

		if (HDC hdc = GetDC((HWND)mhwnd)) {
			HGDIOBJ hgoOld;
			long tm = mCurStatus.mElapsedTimeMS / 1000;

			hgoOld = SelectObject(hdc, mhClockFont);
			SetTextAlign(hdc, TA_BASELINE | TA_LEFT);
			SetBkColor(hdc, GetSysColor(COLOR_3DFACE));
			wsprintf(buf, "%d:%02d", tm/60, tm%60);
			TextOut(hdc, 50, y - 50, buf, strlen(buf));
			SelectObject(hdc, hgoOld);

			ReleaseDC((HWND)mhwnd, hdc);
		}
	}


	mLastStatus = mCurStatus;
}

INT_PTR CALLBACK VDCaptureProjectUI::StaticPanelDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDCaptureProjectUI *pThis;
	
	if (msg == WM_INITDIALOG) {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		pThis = (VDCaptureProjectUI *)lParam;
		pThis->mhwndPanel = hwnd;
	} else
		pThis = (VDCaptureProjectUI *)GetWindowLongPtr(hwnd, DWLP_USER);

	if (!pThis)
		return FALSE;

	return pThis->PanelDlgProc(msg, wParam, lParam);
}

INT_PTR VDCaptureProjectUI::PanelDlgProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:
		return TRUE;

	case WM_APP+0:
		{
			VDCaptureStatus& status = *(VDCaptureStatus *)lParam;
			char buf[256];
			long l, time_diff;
			long lVideoRate, lAudioRate;

			sprintf(buf, "%ld", status.mFramesCaptured);
			SetDlgItemText(mhwndPanel, IDC_FRAMES, buf);

			ticks_to_str(buf, status.mElapsedTimeMS);
			SetDlgItemText(mhwndPanel, IDC_TIME_TOTAL, buf);

			l = mCPUReader.read();

			if (l >= 0) {
				sprintf(buf, "%ld%%", l);
				SetDlgItemText(mhwndPanel, IDC_CPU_USAGE, buf);
			}

			////////

			size_to_str(buf, status.mTotalVideoSize);
			SetDlgItemText(mhwndPanel, IDC_VIDEO_SIZE, buf);

			time_diff = status.mVideoLastFrameTimeMS - status.mVideoFirstFrameTimeMS;

			if (time_diff >= 1000 && status.mFramesCaptured >= 2) {
				l = MulDiv(status.mFramesCaptured - 1, 100000000, time_diff);
				if (l<0) l=0;
				sprintf(buf, "%ld.%05ld fps", l/100000, l%100000);
				SetDlgItemText(mhwndPanel, IDC_VIDEO_RATE, buf);
			}

			if (time_diff >= 1000)
				lVideoRate = (long)((status.mTotalVideoSize*1000 + time_diff/2) / time_diff);
			else
				lVideoRate = 0;
			sprintf(buf, "%ldKB/s", (lVideoRate+1023)/1024);
			SetDlgItemText(mhwndPanel, IDC_VIDEO_DATARATE, buf);

			if (status.mTotalVideoSize && status.mFramesCaptured >= 2) {
				double ratio = (double)mVideoUncompressedSize * (status.mFramesCaptured-1) / (double)status.mTotalVideoSize;

				sprintf(buf, "%.1f:1", ratio);
				SetDlgItemText(mhwndPanel, IDC_VIDEO_RATIO, buf);

				sprintf(buf, "%ld", (long)(status.mTotalVideoSize / (status.mFramesCaptured-1)) - 24);
				SetDlgItemText(mhwndPanel, IDC_VIDEO_AVGFRAMESIZE, buf);
			}

			sprintf(buf, "%ld", status.mFramesDropped);
			SetDlgItemText(mhwndPanel, IDC_VIDEO_DROPPED, buf);

			/////////

			size_to_str(buf, status.mTotalAudioSize);
			SetDlgItemText(mhwndPanel, IDC_AUDIO_SIZE, buf);

			// bytes -> samples/sec
			// bytes / (bytes/sec) = sec
			// bytes / (bytes/sec) * (samples/sec) / sec = avg-samples/sec

			if (status.mAudioLastFrameTimeMS >= 1000) {
				if (status.mActualAudioHz > 0) {
					sprintf(buf, "%.2fHz", status.mActualAudioHz);
					SetDlgItemText(mhwndPanel, IDC_AUDIO_RATE, buf);
				}

				if (!mAudioUncompressedRate)
					SetDlgItemText(mhwndPanel, IDC_AUDIO_RATIO, "1.0:1");
				else if (status.mTotalAudioDataSize > status.mAudioFirstSize) {
					double timeDelta = (double)(status.mAudioLastFrameTimeMS - status.mAudioFirstFrameTimeMS) / 1000.0;
					double sizeDelta = (double)(status.mTotalAudioDataSize - status.mAudioFirstSize);
					double ratio = (timeDelta / sizeDelta) * mAudioUncompressedRate;

					sprintf(buf, "%.1f", ratio);
					SetDlgItemText(mhwndPanel, IDC_AUDIO_RATIO, buf);
				}

				lAudioRate = (long)((status.mTotalAudioSize*1000 + status.mElapsedTimeMS/2) / status.mAudioLastFrameTimeMS);
				sprintf(buf,"%ldKB/s", (lAudioRate+1023)/1024);
				SetDlgItemText(mhwndPanel, IDC_AUDIO_DATARATE, buf);

				sprintf(buf,"%+ld ms", status.mVideoTimingAdjustMS);
				SetDlgItemText(mhwndPanel, IDC_AUDIO_CORRECTIONS, buf);

				sprintf(buf, "%+.3f s.t.", log(status.mAudioResamplingRate) * 17.312340490667560888319096172023);
				SetDlgItemText(mhwndPanel, IDC_AUDIO_RESAMPLERATE, buf);
			} else {
				lAudioRate = 0;
				SetDlgItemText(mhwndPanel, IDC_AUDIO_RATE, "(n/a)");
				SetDlgItemText(mhwndPanel, IDC_AUDIO_RATIO, "(n/a)");
				SetDlgItemText(mhwndPanel, IDC_AUDIO_DATARATE, "(n/a)");
			}

			if (status.mDiskFreeSpace >= 0) {
				size_to_str(buf, status.mDiskFreeSpace);
				SetDlgItemText(mhwndPanel, IDC_DISK_FREE, buf);
			}

			if (lVideoRate + lAudioRate > 16) {
				sint64 diskSpace = status.mDiskFreeSpace;
				if (diskSpace < 0)
					diskSpace = 0;

				ticks_to_str(buf, (long)(diskSpace * 1000 / (lVideoRate + lAudioRate)));
				SetDlgItemText(mhwndPanel, IDC_TIME_LEFT, buf);
			}

			size_to_str(buf, 4096 + status.mTotalVideoSize + status.mTotalAudioSize);
			SetDlgItemText(mhwndPanel, IDC_FILE_SIZE, buf);
		}
		return TRUE;
	}

	return FALSE;
}

////////////////////////////////////////////////////////////////////////////
//
//	preferences
//
////////////////////////////////////////////////////////////////////////////

#if 0
static DWORD g_dialog_drvopts[10];
static DWORD *g_dialog_drvoptptr;

static void CapturePreferencesLoadDriverOpts(HWND hDlg) {
	CheckDlgButton(hDlg, IDC_INITIAL_NODISPLAY, (*g_dialog_drvoptptr & CAPDRV_DISPLAY_MASK) == CAPDRV_DISPLAY_NONE);
	CheckDlgButton(hDlg, IDC_INITIAL_PREVIEW, (*g_dialog_drvoptptr & CAPDRV_DISPLAY_MASK) == CAPDRV_DISPLAY_PREVIEW);
	CheckDlgButton(hDlg, IDC_INITIAL_OVERLAY, (*g_dialog_drvoptptr & CAPDRV_DISPLAY_MASK) == CAPDRV_DISPLAY_OVERLAY);
	CheckDlgButton(hDlg, IDC_SLOW_PREVIEW, !!(*g_dialog_drvoptptr & CAPDRV_CRAPPY_PREVIEW));
	CheckDlgButton(hDlg, IDC_SLOW_OVERLAY, !!(*g_dialog_drvoptptr & CAPDRV_CRAPPY_OVERLAY));
}

static BOOL CapturePreferencesDlgInit(HWND hDlg) {
	HWND hwndCombo = GetDlgItem(hDlg, IDC_DEFAULT_DRIVER);
	HWND hwndCombo2 = GetDlgItem(hDlg, IDC_DRIVER_TO_SET);
	char buf[MAX_PATH];
	int index;

	g_dialog_drvoptptr = NULL;
	memcpy(g_dialog_drvopts, g_drvOpts, sizeof g_dialog_drvopts);

	// Set up 'default driver' combo box

	if (QueryConfigString(g_szCapture, g_szStartupDriver, buf, (sizeof buf)-12)) {
		strcat(buf, " (no change)");
		index = SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)buf);
		if (index>=0) SendMessage(hwndCombo, CB_SETITEMDATA, index, 0);
	}

	index = SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)"First available");
	if (index>=0) SendMessage(hwndCombo, CB_SETITEMDATA, index, 1);

	for(int i=0; i<10; i++) {
		wsprintf(buf, "Driver %d - ", i);
		if (capGetDriverDescription(i, buf+11, (sizeof buf)-11, NULL, 0)) {
			index = SendMessage(hwndCombo, CB_ADDSTRING, 0, (LPARAM)buf);
			if (index>=0) SendMessage(hwndCombo, CB_SETITEMDATA, index, i+16);

			index = SendMessage(hwndCombo2, CB_ADDSTRING, 0, (LPARAM)buf);
			if (index>=0) SendMessage(hwndCombo2, CB_SETITEMDATA, index, i);

			if (!g_dialog_drvoptptr)
				g_dialog_drvoptptr = &g_dialog_drvopts[i];

		}
	}

	if (!g_dialog_drvoptptr) g_dialog_drvoptptr = &g_dialog_drvopts[0];

	SendMessage(hwndCombo, CB_SETCURSEL, (WPARAM)0, 0); 
	SendMessage(hwndCombo2, CB_SETCURSEL, (WPARAM)0, 0); 
	CapturePreferencesLoadDriverOpts(hDlg);

	// Set up 'default capture file'

	if (QueryConfigString(g_szCapture, g_szDefaultCaptureFile, buf, sizeof buf))
		SetDlgItemText(hDlg, IDC_DEFAULT_CAPFILE, buf);

	EnableWindow(GetDlgItem(hDlg, IDC_SAVE_COMPRESSION), !!(g_compression.dwFlags & ICMF_COMPVARS_VALID));
	return TRUE;
}

static void CapturePreferencesDlgStore(HWND hDlg, HWND hwndCapture) {
	HWND hwndCombo = GetDlgItem(hDlg, IDC_DEFAULT_DRIVER);
	char buf[MAX_PATH];
	int index;
	DWORD fsize;
	BITMAPINFOHEADER *bih;
	WAVEFORMATEX *wf;

	index = SendMessage(hwndCombo, CB_GETCURSEL, 0, 0);

	if (index>=0) {
		DWORD dwDriver = SendMessage(hwndCombo, CB_GETITEMDATA, index, 0);

		if (dwDriver==1)
			SetConfigString(g_szCapture, g_szStartupDriver, "");
		else if (dwDriver>=16 && dwDriver<256) {
			if (capGetDriverDescription(dwDriver-16, buf, sizeof buf, NULL, 0))
				SetConfigString(g_szCapture, g_szStartupDriver, buf);
		}
	}

	SendDlgItemMessage(hDlg, IDC_DEFAULT_CAPFILE, WM_GETTEXT, sizeof buf, (LPARAM)buf);
	SetConfigString(g_szCapture, g_szDefaultCaptureFile, buf);

	if (IsDlgButtonChecked(hDlg, IDC_SAVE_CAPSETTINGS)) {
		CAPTUREPARMS cp;

		if (capCaptureGetSetup(hwndCapture, &cp, sizeof cp))
			SetConfigBinary(g_szCapture, g_szCapSettings, (char *)&cp, sizeof cp);
	}

	if (IsDlgButtonChecked(hDlg, IDC_SAVE_VIDEOFORMAT)) {
		if (fsize = capGetVideoFormatSize(hwndCapture)) {
			if (bih = (BITMAPINFOHEADER *)allocmem(fsize)) {
				if (capGetVideoFormat(hwndCapture, bih, fsize)) {
					SetConfigBinary(g_szCapture, g_szVideoFormat, (char *)bih, fsize);
				}
				freemem(bih);
			}
		}
	}

	if (IsDlgButtonChecked(hDlg, IDC_SAVE_AUDIOFORMAT)) {
		if (fsize = capGetAudioFormatSize(hwndCapture)) {
			if (wf = (WAVEFORMATEX *)allocmem(fsize)) {
				if (capGetAudioFormat(hwndCapture, wf, fsize)) {
					SetConfigBinary(g_szCapture, g_szAudioFormat, (char *)wf, fsize);
				}
				freemem(wf);
			}
		}
	}

	if (IsDlgButtonChecked(hDlg, IDC_SAVE_COMPRESSION)) {
		VDCaptureCompressionSpecs cs;
		DWORD dwSize;
		void *mem;

		if ((g_compression.dwFlags & ICMF_COMPVARS_VALID) && g_compression.fccHandler) {
			cs.fccType		= g_compression.fccType;
			cs.fccHandler	= g_compression.fccHandler;
			cs.lKey			= g_compression.lKey;
			cs.lDataRate	= g_compression.lDataRate;
			cs.lQ			= g_compression.lQ;

			SetConfigBinary(g_szCapture, g_szCompression, (char *)&cs, sizeof cs);

			if (g_compression.hic
					&& ((dwSize = ICGetStateSize(g_compression.hic))>0)
					&& (mem = allocmem(dwSize))
					) {

				ICGetState(g_compression.hic, mem, dwSize);
				SetConfigBinary(g_szCapture, g_szCompressorData, (char *)mem, dwSize);
				freemem(mem);

			} else
				DeleteConfigValue(g_szCapture, g_szCompressorData);
		} else {
			DeleteConfigValue(g_szCapture, g_szCompression);
			DeleteConfigValue(g_szCapture, g_szCompressorData);
		}
	}

	// Save driver-specific settings

	for(int i=0; i<10; i++)
		if (g_drvHashes[i]) {
			wsprintf(buf, g_szDrvOpts, g_drvHashes[i]);
			SetConfigDword(g_szCapture, buf, g_dialog_drvopts[i]);
			g_drvOpts[i] = g_dialog_drvopts[i];
			if (g_current_driver == i) mDriverOptions = g_drvOpts[i];
		}
}

static void CapturePreferencesDlgBrowse(HWND hDlg) {
	extern const wchar_t fileFilters0[];

	VDStringW fname(VDGetSaveFileName(VDFSPECKEY_SAVEVIDEOFILE, (VDGUIHandle)hDlg, L"Select default capture file", fileFilters0, L"avi", 0, 0));

	if (!fname.empty())
		SetDlgItemText(hDlg, IDC_DEFAULT_CAPFILE, VDTextWToA(fname).c_str());
}

static INT_PTR CALLBACK CapturePreferencesDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
	switch(message) {
		case WM_INITDIALOG:
			SetWindowLongPtr(hDlg, DWLP_USER, lParam);
			return CapturePreferencesDlgInit(hDlg);

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					VDShowHelp(hDlg, L"d-capturepreferences.html");
			}
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				CapturePreferencesDlgStore(hDlg, (HWND)GetWindowLongPtr(hDlg, DWLP_USER));
			case IDCANCEL:
				EndDialog(hDlg, 0);
				return TRUE;

			case IDC_SELECT_CAPTURE_FILE:
				CapturePreferencesDlgBrowse(hDlg);
				return TRUE;

			case IDC_USE_CURRENT_FILE:
#pragma vdpragma_TODO("FIXME")
//				SetDlgItemText(hDlg, IDC_DEFAULT_CAPFILE, g_szCaptureFile);
				return TRUE;

			case IDC_INITIAL_NODISPLAY:
				*g_dialog_drvoptptr = (*g_dialog_drvoptptr & ~CAPDRV_DISPLAY_MASK) | CAPDRV_DISPLAY_NONE;
				return TRUE;

			case IDC_INITIAL_PREVIEW:
				*g_dialog_drvoptptr = (*g_dialog_drvoptptr & ~CAPDRV_DISPLAY_MASK) | CAPDRV_DISPLAY_PREVIEW;
				return TRUE;

			case IDC_INITIAL_OVERLAY:
				*g_dialog_drvoptptr = (*g_dialog_drvoptptr & ~CAPDRV_DISPLAY_MASK) | CAPDRV_DISPLAY_OVERLAY;
				return TRUE;

			case IDC_SLOW_PREVIEW:
				if (SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED)
					*g_dialog_drvoptptr |= CAPDRV_CRAPPY_PREVIEW;
				else
					*g_dialog_drvoptptr &= ~CAPDRV_CRAPPY_PREVIEW;
				return TRUE;

			case IDC_SLOW_OVERLAY:
				if (SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED)
					*g_dialog_drvoptptr |= CAPDRV_CRAPPY_OVERLAY;
				else
					*g_dialog_drvoptptr &= ~CAPDRV_CRAPPY_OVERLAY;
				return TRUE;

			case IDC_DRIVER_TO_SET:
				{
					int index = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);

					if (index>=0) {
						g_dialog_drvoptptr = &g_dialog_drvopts[SendMessage((HWND)lParam, CB_GETITEMDATA, index, 0)];
						CapturePreferencesLoadDriverOpts(hDlg);
					}
				}
				return TRUE;
			}
	}

	return FALSE;
}

#endif


////////////////////////////////////////////////////////////////////////////
//
//	custom size dialog
//
////////////////////////////////////////////////////////////////////////////


static INT_PTR CALLBACK CaptureCustomVidSizeDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {

	static const int s_widths[]={
		160,
		176,
		180,
		192,
		240,
		320,
		352,
		360,
		384,
		400,
		480,
		640,
		704,
		720,
		768,
	};

	static const int s_heights[]={
		120,
		144,
		180,
		240,
		288,
		300,
		360,
		480,
		576,
	};

#define RV(x) ((((x)>>24)&0xff) | (((x)>>8)&0xff00) | (((x)<<8)&0xff0000) | (((x)<<24)&0xff000000))

	static const struct {
		FOURCC fcc;
		int bpp;
		const char *name;
	} s_formats[]={
		{ BI_RGB,		16, "16-bit RGB" },
		{ BI_RGB,		24, "24-bit RGB" },
		{ BI_RGB,		32, "32-bit ARGB" },
		{ RV('CYUV'),	16, "CYUV\tInverted YUV 4:2:2" },
		{ RV('UYVY'),	16, "UYVY\tYUV 4:2:2 interleaved" },
		{ RV('YUYV'),	16, "YUYV\tYUV 4:2:2 interleaved" },
		{ RV('YUY2'),	16, "YUY2\tYUV 4:2:2 interleaved" },
		{ RV('YV12'),	12, "YV12\tYUV 4:2:0 planar" },
		{ RV('I420'),	12, "I420\tYUV 4:2:0 planar" },
		{ RV('IYUV'),	12, "IYUV\tYUV 4:2:0 planar" },
		{ RV('Y41P'),	12, "Y41P\tYUV 4:1:1 planar" },
		{ RV('YVU9'),	9, "YVU9\t9-bit YUV planar" },
		{ RV('MJPG'),	16, "MJPG\tMotion JPEG" },
		{ RV('dmb1'),	16, "dmb1\tMatrox MJPEG" },
	};
#undef RV

	static FOURCC s_fcc;
	static int s_bpp;
	char buf[64];
	HWND hwndItem;
	IVDCaptureProject *pProject;
	int i;
	int ind;

	switch(msg) {
	case WM_INITDIALOG:
		{
			vdstructex<BITMAPINFOHEADER> pbih;
			int w = 320, h = 240;
			int found_w = -1, found_h = -1, found_f = -1;

			SetWindowLongPtr(hdlg, DWLP_USER, lParam);

			pProject = (IVDCaptureProject *)lParam;

			s_fcc = BI_RGB;
			s_bpp = 16;
			if (pProject->GetVideoFormat(pbih)) {
				s_fcc = pbih->biCompression;
				w = pbih->biWidth;
				h = pbih->biHeight;
				s_bpp = pbih->biBitCount;
			}

			hwndItem = GetDlgItem(hdlg, IDC_FRAME_WIDTH);
			for(i=0; i<sizeof s_widths/sizeof s_widths[0]; i++) {
				sprintf(buf, "%d", s_widths[i]);
				ind = SendMessage(hwndItem, LB_ADDSTRING, 0, (LPARAM)buf);
				SendMessage(hwndItem, LB_SETITEMDATA, ind, i);

				if (s_widths[i] == w)
					found_w = i;
			}

			hwndItem = GetDlgItem(hdlg, IDC_FRAME_HEIGHT);
			for(i=0; i<sizeof s_heights/sizeof s_heights[0]; i++) {
				sprintf(buf, "%d", s_heights[i]);
				ind = SendMessage(hwndItem, LB_ADDSTRING, 0, (LPARAM)buf);
				SendMessage(hwndItem, LB_SETITEMDATA, ind, i);

				if (s_heights[i] == h)
					found_h = i;
			}

			hwndItem = GetDlgItem(hdlg, IDC_FORMATS);

			{
				int tabw = 50;

				SendMessage(hwndItem, LB_SETTABSTOPS, 1, (LPARAM)&tabw);
			}

			for(i=0; i<sizeof s_formats/sizeof s_formats[0]; i++) {
				ind = SendMessage(hwndItem, LB_ADDSTRING, 0, (LPARAM)s_formats[i].name);
				SendMessage(hwndItem, LB_SETITEMDATA, ind, i+1);

				if (s_formats[i].fcc == s_fcc && s_formats[i].bpp == s_bpp)
					found_f = i;
			}

			if (found_f >= 0) {
				SendMessage(hwndItem, LB_SETCURSEL, found_f, 0);
			} else {
				union {
					char fccbuf[5];
					FOURCC fcc;
				};

				fccbuf[4] = 0;
				fcc = s_fcc;

				sprintf(buf, "[Current: %s, %d bits per pixel]", fccbuf, s_bpp);

				ind = SendMessage(hwndItem, LB_INSERTSTRING, 0, (LPARAM)buf);
				SendMessage(hwndItem, LB_SETITEMDATA, ind, 0);
				SendMessage(hwndItem, LB_SETCURSEL, 0, 0);
			}

			if (found_w >=0 && found_h >=0) {
				SendDlgItemMessage(hdlg, IDC_FRAME_WIDTH, LB_SETCURSEL, found_w, 0);
				SendDlgItemMessage(hdlg, IDC_FRAME_HEIGHT, LB_SETCURSEL, found_h, 0);
			} else {
				SetDlgItemInt(hdlg, IDC_WIDTH, w, FALSE);
				SetDlgItemInt(hdlg, IDC_HEIGHT, h, FALSE);

				CheckDlgButton(hdlg, IDC_CUSTOM, BST_CHECKED);
			}

			PostMessage(hdlg, WM_COMMAND, IDC_CUSTOM, (LPARAM)GetDlgItem(hdlg, IDC_CUSTOM));
		}

		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			do {
				int w, h, f;
				BOOL b;

				pProject = (IVDCaptureProject *)GetWindowLongPtr(hdlg, DWLP_USER);

				if (IsDlgButtonChecked(hdlg, IDC_CUSTOM)) {

					w = GetDlgItemInt(hdlg, IDC_WIDTH, &b, FALSE);
					if (!b || !w) {
						MessageBeep(MB_ICONEXCLAMATION);
						SetFocus(GetDlgItem(hdlg, IDC_WIDTH));
						return TRUE;
					}

					h = GetDlgItemInt(hdlg, IDC_HEIGHT, &b, FALSE);
					if (!b || !h) {
						MessageBeep(MB_ICONEXCLAMATION);
						SetFocus(GetDlgItem(hdlg, IDC_HEIGHT));
						return TRUE;
					}

				} else {
					w = s_widths[SendDlgItemMessage(hdlg, IDC_FRAME_WIDTH, LB_GETITEMDATA,
							SendDlgItemMessage(hdlg, IDC_FRAME_WIDTH, LB_GETCURSEL, 0, 0), 0)];
					h = s_heights[SendDlgItemMessage(hdlg, IDC_FRAME_HEIGHT, LB_GETITEMDATA,
							SendDlgItemMessage(hdlg, IDC_FRAME_HEIGHT, LB_GETCURSEL, 0, 0), 0)];
				}

				f = SendDlgItemMessage(hdlg, IDC_FORMATS, LB_GETITEMDATA,
						SendDlgItemMessage(hdlg, IDC_FORMATS, LB_GETCURSEL, 0, 0), 0);

				vdstructex<BITMAPINFOHEADER> pbih;
				pbih.resize(sizeof(BITMAPINFOHEADER));

				if (!f) {
					if (!pProject->GetVideoFormat(pbih))
						break;

					if (pbih.size() < sizeof(BITMAPINFOHEADER))
						pbih.resize(sizeof(BITMAPINFOHEADER));
				} else {
					pbih->biSize			= sizeof(BITMAPINFOHEADER);
					pbih->biCompression		= s_formats[f-1].fcc;
					pbih->biBitCount		= (WORD)s_formats[f-1].bpp;
				}

				pbih->biWidth			= w;
				pbih->biHeight			= h;
				pbih->biPlanes			= 1;
				pbih->biSizeImage		= h * ((w * pbih->biBitCount + 31) / 32) * 4 * pbih->biPlanes;
				pbih->biXPelsPerMeter	= 80;
				pbih->biYPelsPerMeter	= 80;
				pbih->biClrUsed			= 0;
				pbih->biClrImportant	= 0;

#pragma vdpragma_TODO("need errror handling here")
				pProject->SetVideoFormat(*pbih, pbih.size());
			} while(false);

			EndDialog(hdlg, 1);
			return TRUE;

		case IDCANCEL:
			EndDialog(hdlg, 0);
			return TRUE;

		case IDC_CUSTOM:
			{
				BOOL fEnabled = SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED;

				EnableWindow(GetDlgItem(hdlg, IDC_WIDTH), fEnabled);
				EnableWindow(GetDlgItem(hdlg, IDC_HEIGHT), fEnabled);

				EnableWindow(GetDlgItem(hdlg, IDC_FRAME_WIDTH), !fEnabled);
				EnableWindow(GetDlgItem(hdlg, IDC_FRAME_HEIGHT), !fEnabled);
			}
			return TRUE;

		}
		return FALSE;
	}

	return FALSE;
}
