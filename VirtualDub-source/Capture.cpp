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

#include "VirtualDub.h"

#include <stdio.h>
#include <malloc.h>
#include <ctype.h>
#include <math.h>
#include <crtdbg.h>
#include <process.h>

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <vfw.h>

#include "Error.h"
#include "AVIOutput.h"
#include "FastWriteStream.h"
#include "Histogram.h"
#include "FilterSystem.h"
#include "AVIOutputStriped.h"
#include "AVIStripeSystem.h"
#include "VideoSequenceCompressor.h"
#include "int128.h"

#include "crash.h"
#include "tls.h"
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
#include "cpuaccel.h"
#include "caplog.h"
#include "capaccel.h"

#define TAG2(x) OutputDebugString("At line " #x "\n")
#define TAG1(x) TAG2(x)
#define TAG TAG1(__LINE__)

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

extern void ChooseCompressor(HWND hwndParent, COMPVARS *lpCompVars, BITMAPINFOHEADER *bihInput);
extern void FreeCompressor(COMPVARS *pCompVars);

extern LRESULT CALLBACK VCMDriverProc(DWORD dwDriverID, HDRVR hDriver, UINT uiMessage, LPARAM lParam1, LPARAM lParam2);

extern void CaptureDisplayBT848Tweaker(HWND hwndParent);
extern void CaptureCloseBT848Tweaker();
extern void CaptureBT848Reassert();

///////////////////////////////////////////////////////////////////////////
//
//	structs
//
///////////////////////////////////////////////////////////////////////////

class CaptureCompressionSpecs {
public:
	DWORD	fccType;
	DWORD	fccHandler;
	LONG	lKey;
	LONG	lDataRate;
	LONG	lQ;
};

class CaptureVars {
public:
	WAVEFORMATEX	wfex;
	HWND			hwndStatus, hwndPanel;
	__int64		total_jitter, total_disp, total_video_size, total_audio_size, total_audio_data_size, audio_first_size;
	__int64		last_video_size;
	__int64		disk_free;
	long		total_cap, last_cap, total_audio_cap;
	DWORD		dropped;
	DWORD		interval;
	DWORD		lastMessage;
	long		lCurrentMS;
	long		lVideoFirstMS, lVideoLastMS;
	long		lAudioFirstMS, lAudioLastMS;
	long		uncompressed_frame_size;
	int			iSpillNumber;
	char		szCaptureRoot[MAX_PATH];
	char		*pNoiseReductionBuffer;
	char		*pVertRowBuffer;
	long		bpr;
	ptrdiff_t			pdClipOffset;
	int					rowdwords;
	bool				fClipping;

	// audio sampling rate estimation

	__int64		i64AudioHzX;
	__int64		i64AudioHzY;
	int128		i64AudioHzX2;
	int128		i64AudioHzY2;
	int128		i64AudioHzXY;
	int			iAudioHzSamples;

	// video timing correction (non-compat only)

	long			lVideoAdjust;


	CaptureVars() { memset(this, 0, sizeof *this); }
};

class CaptureData : public CaptureVars {
public:
	CPUUsageReader		CPU;
	FilterStateInfo		fsi;
	BITMAPINFOHEADER	bihInputFormat;
	BITMAPINFOHEADER	bihFiltered, bihFiltered2;
	BITMAPINFOHEADER	bihClipFormat;
};

#define	CAPSTOP_TIME			(0x00000001L)
#define	CAPSTOP_FILESIZE		(0x00000002L)
#define	CAPSTOP_DISKSPACE		(0x00000004L)
#define	CAPSTOP_DROPRATE		(0x00000008L)

struct CaptureStopPrefs {
	long		fEnableFlags;
	long		lTimeLimit;
	long		lSizeLimit;
	long		lDiskSpaceThreshold;
	long		lMaxDropRate;
};

///////////////////////////////////////////////////////////////////////////
//
//	statics
//
///////////////////////////////////////////////////////////////////////////

static CAPTUREPARMS g_defaultCaptureParms={
	1000000/15,		//15fps
	FALSE,
	10,
	FALSE,			// callbacks won't work if Yield is TRUE
	324000,			// we like index entries
	4,
	FALSE,
	10,
	TRUE,
	0,
	VK_ESCAPE,
	TRUE,
	FALSE,
	FALSE,
	0,
	FALSE,
	FALSE,
	0,0,
	FALSE,
	10,
	0,
	FALSE,
	AVSTREAMMASTER_NONE,				//	AVSTREAMMASTER_AUDIO
};

#define FRAMERATE(x) ((LONG)((1000000 + (x)/2.0) / (x)))

static LONG g_predefFrameRates[]={
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

//////////////////////////////////////////////////////////////////////

#define MENU_TO_HELP(x) ID_##x, IDS_CAP_##x

static UINT iCaptureMenuHelpTranslator[]={
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
	MENU_TO_HELP(VIDEO_COMPRESSION_AVICAP),
	MENU_TO_HELP(VIDEO_COMPRESSION_INTERNAL),
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

extern const char g_szCapture				[]="Capture";
static const char g_szStartupDriver			[]="Startup Driver";
static const char g_szDefaultCaptureFile	[]="Capture File";
static const char g_szCapSettings			[]="Settings";
static const char g_szAudioFormat			[]="Audio Format";
static const char g_szVideoFormat			[]="Video Format";
static const char g_szDrvOpts				[]="DrvOpts %08lx";
static const char g_szCompression			[]="Compression";
static const char g_szCompressorData		[]="Compressor Data";
static const char g_szStopConditions		[]="Stop Conditions";
static const char g_szHideInfoPanel			[]="Hide InfoPanel";

static const char g_szAdjustVideoTiming		[]="AdjustVideoTiming";

static const char g_szChunkSize				[]="Chunk size";
static const char g_szChunkCount			[]="Chunk count";
static const char g_szDisableBuffering		[]="Disable buffering";
static const char g_szWarnTiming1			[]="Warn Timing1";

static const char g_szCannotFilter[]="Cannot use video filtering: ";

///////////////////////////////////////////////////////////////////////////
//
//	dynamics
//
///////////////////////////////////////////////////////////////////////////

static HMENU g_hMenuAuxCapture = NULL;
static HACCEL g_hAccelCapture = NULL;
static char g_szCaptureFile[MAX_PATH];
static char g_szStripeFile[MAX_PATH];

static bool g_fHideOnCapture = false;
static bool g_fDisplayLargeTimer = false;
static bool g_fEnableSpill = false;
static bool g_fStretch = false;
static bool g_fInfoPanel = true;

#define CAPDRV_DISPLAY_OVERLAY	(0)
#define	CAPDRV_DISPLAY_PREVIEW	(1)
#define CAPDRV_DISPLAY_NONE		(2)
#define	CAPDRV_DISPLAY_MASK		(15)

#define CAPDRV_CRAPPY_PREVIEW	(0x00000010L)
#define	CAPDRV_CRAPPY_OVERLAY	(0x00000020L)

static DWORD g_drvOpts[10];
static long g_drvHashes[10];
static DWORD g_driver_options;
static int g_current_driver;
static BOOL g_fCrappyMode;

static AVIStripeSystem *g_capStripeSystem = NULL;
static COMPVARS g_compression;

static CaptureStopPrefs			g_stopPrefs;

static long			g_diskChunkSize		= 512;
static int			g_diskChunkCount	= 2;
static DWORD		g_diskDisableBuffer	= 1;

static CaptureHistogram	*g_pHistogram;

static bool			g_fEnableClipping = false;
RECT				g_rCaptureClip;

static bool			g_fEnableRGBFiltering = false;
static bool			g_fEnableNoiseReduction = false;
static bool			g_fEnableLumaSquish = false;
static bool			g_fAdjustVideoTimer	= true;
static bool			g_fSwapFields		= false;
static enum {
	VERTSQUASH_NONE			=0,
	VERTSQUASH_BY2LINEAR	=1,
	VERTSQUASH_BY2CUBIC		=2
} g_iVertSquash = VERTSQUASH_NONE;

static int			g_iNoiseReduceThreshold = 16;

static bool			g_fRestricted = false;

static CaptureLog g_capLog;
static bool			g_fLogEvents = false;

static enum {
	kDDP_Off = 0,
	kDDP_Top,
	kDDP_Bottom,
	kDDP_Both,
} g_nCaptureDDraw;
static bool			g_bCaptureDDrawActive;
static RydiaDirectDrawContext	g_DDContext;
static WNDPROC		g_pCapWndProc;

///////////////////////////////////////////////////////////////////////////
//
//	prototypes
//
///////////////////////////////////////////////////////////////////////////

extern void CaptureWarnCheckDriver(HWND hwnd, const char *s);
extern void CaptureWarnCheckDrivers(HWND hwnd);
static void CaptureEnablePreviewHistogram(HWND hWndCapture, bool fEnable);
static void CaptureSetPreview(HWND, bool);

static LRESULT CALLBACK CaptureYieldCallback(HWND hwnd);

extern BOOL APIENTRY CaptureVumeterDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam);
extern BOOL APIENTRY CaptureHistogramDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam);
extern BOOL CALLBACK CaptureSpillDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static BOOL APIENTRY CaptureAllocateDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam);
static BOOL APIENTRY CaptureSettingsDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam);

static BOOL APIENTRY CapturePreferencesDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam);
static BOOL APIENTRY CaptureStopConditionsDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL APIENTRY CaptureDiskIODlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL APIENTRY CaptureCustomVidSizeDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
static BOOL APIENTRY CaptureTimingDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);

static void CaptureAVICap(HWND hWnd, HWND hWndCapture);
static void CaptureInternal(HWND, HWND hWndCapture, bool fTest);
static void CaptureInternalSelectCompression(HWND);
static void CaptureInternalLoadFromRegistry();

LRESULT CALLBACK CaptureHistoFrameCallback(HWND hWnd, VIDEOHDR *vhdr);
LRESULT CALLBACK CaptureOverlayFrameCallback(HWND hWnd, VIDEOHDR *vhdr);
static void CaptureToggleNRDialog(HWND);
void CaptureShowClippingDialog(HWND hwndCapture);
static void CaptureMoveWindow(HWND);

///////////////////////////////////////////////////////////////////////////
//
//	misc
//
///////////////////////////////////////////////////////////////////////////

// time to abuse C++

class CapturePriorityWhacker {
private:
	HWND hwndCapture;
	BOOL fPowerOffState;
	BOOL fLowPowerState;
	BOOL fScreenSaverState;

public:
	CapturePriorityWhacker(HWND);
	~CapturePriorityWhacker();
};

CapturePriorityWhacker::CapturePriorityWhacker(HWND hwnd) : hwndCapture(hwnd) {

	SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &fScreenSaverState, FALSE);
	SystemParametersInfo(SPI_GETLOWPOWERACTIVE, 0, &fLowPowerState, FALSE);
	SystemParametersInfo(SPI_GETPOWEROFFACTIVE, 0, &fPowerOffState, FALSE);

	SystemParametersInfo(SPI_SETPOWEROFFACTIVE, fPowerOffState, FALSE, FALSE);
	SystemParametersInfo(SPI_SETLOWPOWERACTIVE, fLowPowerState, FALSE, FALSE);
	SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, fScreenSaverState, FALSE, FALSE);

	if (g_fHideOnCapture)
		ShowWindow(hwndCapture, SW_HIDE);
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
}

CapturePriorityWhacker::~CapturePriorityWhacker() {
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
	SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
	if (g_fHideOnCapture)
		ShowWindow(hwndCapture, SW_SHOWNA);

	SystemParametersInfo(SPI_SETPOWEROFFACTIVE, fPowerOffState, NULL, FALSE);
	SystemParametersInfo(SPI_SETLOWPOWERACTIVE, fLowPowerState, NULL, FALSE);
	SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, fScreenSaverState, NULL, FALSE);
}



static int CaptureIsCatchableException(DWORD ec) {
	switch(ec) {
	case EXCEPTION_ACCESS_VIOLATION:
	case EXCEPTION_PRIV_INSTRUCTION:
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
	case EXCEPTION_BREAKPOINT:
		return 1;
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////
//
//	driver schtuff
//
///////////////////////////////////////////////////////////////////////////

static long CaptureHashDriverName(char *name) {
	long hash;
	int len=0;
	char c;

	// We don't want to have to deal with hash collisions in the Registry,
	// so instead we use the Prayer method, in conjunction with a careful
	// hash algorithm.  LMSB is the length of the string, clamped at 255;
	// LSB is the first byte of the string.  The upper 2 bytes are the
	// modulo sum of all the bytes in the string.  This way, two drivers
	// would have to start with the same letter and have description
	// strings of the exact same length in order to collide.
	//
	// It's impossible to distinguish two identical capture cards this way,
	// but what moron puts two exact same cards in his system?  Besides,
	// this way if someone yanks a card and alters the driver numbers, we
	// can still find the right config for each driver.

	hash = (long)(unsigned char)name[0];

	while(c=*name++) {
		hash += (long)(unsigned char)c << 16;
		++len;
	}
	if (len>255) len=255;
	hash |= (len<<8);

	// If some idiot driver gives us no name, we have a hash of zero.
	// We do not like zero hashes.

	if (!hash) ++hash;

	return hash;
}

static int CaptureAddDrivers(HWND hwnd, HMENU hMenu) {
	int i, firstDriver=-1, nDriver=-1;
	char szName[128];
	char szMenu[128];
	char szDesiredDriver[128];
	DWORD dwDrvOpts;
	long hash;

	if (!QueryConfigString(g_szCapture, g_szStartupDriver, szDesiredDriver, sizeof szDesiredDriver))
		szDesiredDriver[0] = 0;

	memset(g_drvOpts, 0, sizeof g_drvOpts);
	memset(g_drvHashes, 0, sizeof g_drvHashes);

	for(i=0; i<10; i++) {
		if (capGetDriverDescription(i, szName, sizeof szName, NULL, 0)) {
			wsprintf(szMenu, "&%c %s", '0'+i, szName);
			AppendMenu(hMenu, nDriver<0 ? MF_ENABLED|MF_CHECKED:MF_ENABLED, ID_VIDEO_CAPTURE_DRIVER+i, szMenu);

			if (firstDriver<0) firstDriver = i;
			if (nDriver<0 && !stricmp(szName, szDesiredDriver)) nDriver = i;

			// check for a problematic driver

			CaptureWarnCheckDriver(hwnd, szName);

			// config?

			g_drvHashes[i] = hash = CaptureHashDriverName(szName);
			wsprintf(szName, g_szDrvOpts, hash);
			if (QueryConfigDword(g_szCapture, szName, &dwDrvOpts))
				g_drvOpts[i] = dwDrvOpts;
		}
	}

	return nDriver==-1 ? firstDriver : nDriver;
}

static bool CaptureSelectDriver(HWND hWnd, HWND hWndCapture, int nDriver) {
	HMENU hMenu = GetMenu(hWnd);
	CAPDRIVERCAPS cdc;

	CaptureEnablePreviewHistogram(hWndCapture, false);

	if (!capDriverConnect(hWndCapture, nDriver)) {
		MessageBox(hWnd, "VirtualDub cannot connect to the desired capture driver. Trying all available drivers.", g_szError, MB_OK);

		int nDriverOriginal = nDriver;

		nDriver = 0;
		while(nDriver < 10) {
			if (nDriver != nDriverOriginal && capGetDriverDescription(nDriver, NULL, 0, NULL, 0) && capDriverConnect(hWndCapture, nDriver))
				break;

			++nDriver;
		}

		if (nDriver >= 10) {
			MessageBox(hWnd, "PANIC: VirtualDub cannot connect to any capture drivers!", g_szError, MB_OK);
			return false;
		}
	}

	CheckMenuRadioItem(hMenu, ID_VIDEO_CAPTURE_DRIVER, ID_VIDEO_CAPTURE_DRIVER+9, ID_VIDEO_CAPTURE_DRIVER+nDriver, MF_BYCOMMAND);

	g_driver_options = g_drvOpts[nDriver];
	g_current_driver = nDriver;

	cdc.fHasOverlay = TRUE;

	if (capDriverGetCaps(hWndCapture, &cdc, sizeof(CAPDRIVERCAPS))) {
		EnableMenuItem(hMenu, ID_VIDEO_OVERLAY, cdc.fHasOverlay ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
		EnableMenuItem(hMenu, ID_VIDEO_SOURCE, cdc.fHasDlgVideoSource ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
		EnableMenuItem(hMenu, ID_VIDEO_FORMAT, cdc.fHasDlgVideoFormat ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
		EnableMenuItem(hMenu, ID_VIDEO_DISPLAY, cdc.fHasDlgVideoDisplay ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
	}

	switch(g_driver_options & CAPDRV_DISPLAY_MASK) {
	case CAPDRV_DISPLAY_PREVIEW:
		CaptureSetPreview(hWndCapture, true);
		break;
	case CAPDRV_DISPLAY_OVERLAY:
		if (cdc.fHasOverlay) capOverlay(hWndCapture, TRUE);
		break;
	}

	return true;
}

static void CaptureEnablePreviewHistogram(HWND hWndCapture, bool fEnable) {
	if (fEnable) {
		if (!g_pHistogram) {
			if (g_bCaptureDDrawActive)
				CaptureSetPreview(hWndCapture, false);

			try {
				g_pHistogram = new CaptureHistogram(hWndCapture, NULL, 128);

				if (!g_pHistogram)
					throw MyMemoryError();

				capSetCallbackOnFrame(hWndCapture, (LPVOID)CaptureHistoFrameCallback);

			} catch(MyError e) {
				guiSetStatus("Cannot initialize histogram: %s", 0, e.gets());
			}
		}

		capPreview(hWndCapture, true);
	} else {
		if (g_pHistogram) {
			CaptureSetPreview(hWndCapture, true);
			delete g_pHistogram;
			g_pHistogram = NULL;
			InvalidateRect(GetParent(hWndCapture), NULL, TRUE);
		}
	}
	CaptureBT848Reassert();
}

static void CaptureSetPreview(HWND hwndCapture, bool b) {
	capSetCallbackOnFrame(hwndCapture, NULL);

	if (!b) {
		RydiaEnableAVICapPreview(false);
		g_bCaptureDDrawActive = false;
		capPreview(hwndCapture, FALSE);
		g_DDContext.DestroyOverlay();
	} else {
		if (g_nCaptureDDraw) {
			BITMAPINFOHEADER *bih;
			LONG fsize;

			g_bCaptureDDrawActive = false;

			if (g_DDContext.isReady() || g_DDContext.Init()) {
				if (fsize = capGetVideoFormatSize(hwndCapture)) {
					if (bih = (BITMAPINFOHEADER *)allocmem(fsize)) {
						if (capGetVideoFormat(hwndCapture, bih, fsize)) {
							if (g_DDContext.CreateOverlay(bih->biWidth, g_nCaptureDDraw==kDDP_Both ? bih->biHeight : bih->biHeight/2, bih->biBitCount, bih->biCompression)) {
								g_bCaptureDDrawActive = true;
								RydiaInitAVICapHotPatch();
								RydiaEnableAVICapPreview(true);
								CaptureMoveWindow(hwndCapture);
								capSetCallbackOnFrame(hwndCapture, CaptureOverlayFrameCallback);
							}
						}
						freemem(bih);
					}
				}
			}
		}
		capPreview(hwndCapture, TRUE);
	}
}
///////////////////////////////////////////////////////////////////////////
//
//	'gooey' (interface)
//
///////////////////////////////////////////////////////////////////////////

static int g_cap_modeBeforeSlow;

static void CaptureEnterSlowPeriod(HWND hwnd) {
	HWND hwndCapture = GetDlgItem(hwnd, IDC_CAPTURE_WINDOW);
	CAPSTATUS cs;

	if (!(g_driver_options & (CAPDRV_CRAPPY_OVERLAY | CAPDRV_CRAPPY_PREVIEW))) return;

	g_cap_modeBeforeSlow = 0;

	if (capGetStatus(hwndCapture, &cs, sizeof cs)) {
		if (cs.fOverlayWindow && (g_driver_options & CAPDRV_CRAPPY_OVERLAY)) {
			g_cap_modeBeforeSlow = 1;
			capOverlay(hwndCapture, FALSE);
		}
		if (cs.fLiveWindow && (g_driver_options & CAPDRV_CRAPPY_PREVIEW)) {
			g_cap_modeBeforeSlow |= 2;
			CaptureSetPreview(hwndCapture, false);
		}
	}
}

static void CaptureAbortSlowPeriod() {
	g_cap_modeBeforeSlow = 0;
}

static void CaptureExitSlowPeriod(HWND hwnd) {
	HWND hwndCapture = GetDlgItem(hwnd, IDC_CAPTURE_WINDOW);

	if (g_cap_modeBeforeSlow & 1) capOverlay(hwndCapture, TRUE);
	if (g_cap_modeBeforeSlow & 2) CaptureSetPreview(hwndCapture, TRUE);

	CaptureBT848Reassert();
}

static void CaptureMoveWindow(HWND hwnd) {
	if (!g_bCaptureDDrawActive)
		return;

	RECT r;

	GetClientRect(hwnd, &r);
	ClientToScreen(hwnd, (LPPOINT)&r+0);
	ClientToScreen(hwnd, (LPPOINT)&r+1);

	g_DDContext.PositionOverlay(r.left, r.top, r.right-r.left, r.bottom-r.top);
}

static void CaptureResizeWindow(HWND hWnd) {
	CAPSTATUS cs;

	if (!capGetStatus(hWnd, &cs, sizeof(CAPSTATUS)))
		return;

//	MoveWindow(hWnd, 0, 0, cs.uiImageWidth, cs.uiImageHeight, TRUE);

	HWND hwndParent = GetParent(hWnd);
	HWND hwndPanel = GetDlgItem(hwndParent, IDC_CAPTURE_PANEL);
	HWND hwndStatus = GetDlgItem(hwndParent, IDC_STATUS_WINDOW);
	RECT r;
	int		xedge = GetSystemMetrics(SM_CXEDGE);
	int		yedge = GetSystemMetrics(SM_CYEDGE);
	int		sx, sy;

	if (g_fInfoPanel) {
		GetWindowRect(hwndPanel, &r);
		ScreenToClient(hwndParent, (LPPOINT)&r + 0);
		ScreenToClient(hwndParent, (LPPOINT)&r + 1);
	} else {
		GetClientRect(hwndParent, &r);
		r.left = r.right;
	}

	sx = r.left-xedge*2;

	GetWindowRect(hwndStatus, &r);
	ScreenToClient(hwndParent, (LPPOINT)&r);

	sy = r.top-yedge*2;

	if (!g_fStretch) {
		if (sx > cs.uiImageWidth)
			sx = cs.uiImageWidth;

		if (sy > cs.uiImageHeight)
			sy = cs.uiImageHeight;
	}

	SetWindowPos(hWnd, NULL, xedge, yedge, sx, sy, SWP_NOZORDER|SWP_NOACTIVATE);

	CaptureMoveWindow(hWnd);

}

static void CaptureShowParms(HWND hWnd) {
	HWND hWndCapture = GetDlgItem(hWnd, IDC_CAPTURE_WINDOW);
	HWND hWndStatus = GetDlgItem(hWnd, IDC_STATUS_WINDOW);
	CAPTUREPARMS cp;
	char buf[64];
	WAVEFORMATEX *wf;
	BITMAPINFOHEADER *bih;
	LONG fsize;
	LONG bandwidth = 0;

	strcpy(buf,"(unknown)");
	if (capCaptureGetSetup(hWndCapture, &cp, sizeof(CAPTUREPARMS))) {
		LONG fps100 = (100000000 + cp.dwRequestMicroSecPerFrame/2)/ cp.dwRequestMicroSecPerFrame;

		wsprintf(buf,"%d.%02d fps", fps100/100, fps100%100);

		SendMessage(hWndStatus, SB_SETTEXT, 2 | SBT_POPOUT, (LPARAM)buf);

		if (fsize = capGetVideoFormatSize(hWndCapture)) {
			if (bih = (BITMAPINFOHEADER *)allocmem(fsize)) {
				if (capGetVideoFormat(hWndCapture, bih, fsize)) {
					DWORD size = bih->biSizeImage;

					if (!size)
						size = bih->biHeight*bih->biPlanes * (((bih->biWidth * bih->biBitCount + 31)/32) * 4);

					bandwidth += MulDiv(
									8 + size,
									1000000,
									cp.dwRequestMicroSecPerFrame);
				}
				freemem(bih);
			}
		}
	}

	strcpy(buf,"(unknown)");
	if (fsize = capGetAudioFormatSize(hWndCapture)) {
		if (wf = (WAVEFORMATEX *)allocmem(fsize)) {
			if (capGetAudioFormat(hWndCapture, wf, fsize)) {
				if (wf->wFormatTag != WAVE_FORMAT_PCM) {
					wsprintf(buf, "%d.%03dKHz", wf->nSamplesPerSec/1000, wf->nSamplesPerSec%1000);
				} else {
					PCMWAVEFORMAT *pwf = (PCMWAVEFORMAT *)wf;

					wsprintf(buf, "%dK/%d/%c", (pwf->wf.nSamplesPerSec+500)/1000, pwf->wBitsPerSample, pwf->wf.nChannels>1?'s':'m');
				}

				bandwidth += 8 + wf->nAvgBytesPerSec;
			}
			freemem(wf);
		}
	}

	SendMessage(hWndStatus, SB_SETTEXT, 1 | SBT_POPOUT, (LPARAM)buf);

	wsprintf(buf, "%ldK/s", (bandwidth+1023)>>10);
	SendMessage(hWndStatus, SB_SETTEXT, 3, (LPARAM)buf);
}

static void CaptureSetPCMAudioFormat(HWND hWndCapture, LONG sampling_rate, BOOL is_16bit, BOOL is_stereo) {
	WAVEFORMATEX wf;

	_RPT3(0,"Setting format %d/%d/%s\n", sampling_rate, is_16bit?16:8, is_stereo?"stereo":"mono");

	wf.wFormatTag		= WAVE_FORMAT_PCM;
	wf.nChannels		= is_stereo ? 2 : 1;
	wf.nSamplesPerSec	= sampling_rate;
	wf.wBitsPerSample	= is_16bit ? 16 : 8;
	wf.nAvgBytesPerSec	= sampling_rate * wf.nChannels * wf.wBitsPerSample/8;
	wf.nBlockAlign		= wf.nChannels * wf.wBitsPerSample/8;
	wf.cbSize			= 0;

	if (!capSetAudioFormat(hWndCapture, &wf, sizeof(WAVEFORMATEX)))
		_RPT0(0,"Couldn't set audio format!\n");
}

static void CaptureSetFrameTime(HWND hWndCapture, LONG lFrameTime) {
	CAPTUREPARMS cp;

	if (capCaptureGetSetup(hWndCapture, &cp, sizeof(CAPTUREPARMS))) {
		cp.dwRequestMicroSecPerFrame = lFrameTime;

		_RPT1(0,"Setting %ld microseconds per frame.\n", lFrameTime);

		capCaptureSetSetup(hWndCapture, &cp, sizeof(CAPTUREPARMS));
	}
}

static void CaptureShowFile(HWND hwnd, HWND hwndCapture, bool fCaptureActive) {
	const char *pszAppend = fCaptureActive ? " [capture in progress]" : "";

	if (g_capStripeSystem)
		guiSetTitle(hwnd, IDS_TITLE_CAPTURE2, g_szStripeFile, pszAppend);
	else
		guiSetTitle(hwnd, IDS_TITLE_CAPTURE, g_szCaptureFile, pszAppend);
}

static bool CaptureSetCaptureFile(HWND hwndCapture) {
	if (!capFileSetCaptureFile(hwndCapture, g_szCaptureFile)) {
		guiMessageBoxF(GetParent(hwndCapture),
				"VirtualDub warning", 
				MB_OK,
				"Unable to set file \"%s\" as the capture file.  It may be open in VirtualDub's editor or another program "
				"may be using it."
				,
				g_szCaptureFile);

		capFileGetCaptureFile(hwndCapture, g_szCaptureFile, sizeof g_szCaptureFile);

		return false;
	}

	return true;
}

static void CaptureSetFile(HWND hWnd, HWND hWndCapture) {
	OPENFILENAME ofn;

	///////////////

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= hWnd;
	ofn.lpstrFilter			= "Audio-Video Interleave (*.avi)\0*.avi\0All Files (*.*)\0*.*\0";
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= g_szCaptureFile;
	ofn.nMaxFile			= sizeof g_szCaptureFile;
	ofn.lpstrFileTitle		= NULL;
	ofn.nMaxFileTitle		= 0;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= "Set Capture File";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
	ofn.lpstrDefExt			= g_prefs.main.fAttachExtension ? "avi" : NULL;

	if (GetSaveFileName(&ofn)) {
		if (g_capStripeSystem) {
			delete g_capStripeSystem;
			g_capStripeSystem = NULL;
		}

		CaptureSetCaptureFile(hWndCapture);
		_RPT1(0,"Capture file: [%s]\n", g_szCaptureFile);
		CaptureShowFile(hWnd, hWndCapture, false);
	}
}

static void CaptureSetStripingSystem(HWND hwnd, HWND hwndCapture) {
	OPENFILENAME ofn;

	///////////////

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= hwnd;
	ofn.lpstrFilter			= "AVI Stripe System (*.stripe)\0*.stripe\0All Files (*.*)\0*.*\0";
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= g_szStripeFile;
	ofn.nMaxFile			= sizeof g_szStripeFile;
	ofn.lpstrFileTitle		= NULL;
	ofn.nMaxFileTitle		= 0;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= "Select Striping System for Internal Capture";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
	ofn.lpstrDefExt			= NULL;

	if (GetSaveFileName(&ofn)) {
		try {
			if (g_capStripeSystem) {
				delete g_capStripeSystem;
				g_capStripeSystem = NULL;
			}

			if (!(g_capStripeSystem = new AVIStripeSystem(g_szStripeFile)))
				throw MyMemoryError();

			CaptureShowFile(hwnd, hwndCapture, false);
		} catch(MyError e) {
			e.post(hwnd, g_szError);
		}
	}
}

static void CaptureChooseAudioCompression(HWND hWnd, HWND hWndCapture) {
	ACMFORMATCHOOSE afc;
	DWORD dwFormatSize1, dwFormatSize2;
	WAVEFORMATEX *wf;

	dwFormatSize1 = capGetAudioFormatSize(hWndCapture);
	if (acmMetrics(NULL, ACM_METRIC_MAX_SIZE_FORMAT, &dwFormatSize2))
		return;

	if (dwFormatSize2 > dwFormatSize1) dwFormatSize1 = dwFormatSize2;

	if (!(wf = (WAVEFORMATEX *)allocmem(dwFormatSize1)))
		return;

	memset(&afc, 0, sizeof afc);
	afc.cbStruct		= sizeof(ACMFORMATCHOOSE);
	afc.fdwStyle		= ACMFORMATCHOOSE_STYLEF_INITTOWFXSTRUCT;
	afc.hwndOwner		= hWnd;
	afc.pwfx			= wf;
	afc.cbwfx			= dwFormatSize1;
	afc.pszTitle		= "Set Audio Compression";
	afc.fdwEnum			= ACM_FORMATENUMF_INPUT;
	afc.pwfxEnum		= NULL;
	afc.hInstance		= NULL;
	afc.pszTemplateName	= NULL;

	if (!capGetAudioFormat(hWndCapture, wf, dwFormatSize1))
		afc.fdwStyle = 0;

	if (MMSYSERR_NOERROR == acmFormatChoose(&afc)) {
		capSetAudioFormat(hWndCapture, wf, sizeof(WAVEFORMATEX) + wf->cbSize);
	}

	freemem(wf);
}

static void CaptureInitMenu(HWND hWnd, HMENU hMenu) {
	HWND hWndCapture = GetDlgItem(hWnd, IDC_CAPTURE_WINDOW);
	CAPSTATUS cs;

	if (capGetStatus(hWndCapture, &cs, sizeof(CAPSTATUS))) {
		bool fOverlay = cs.fOverlayWindow || (g_cap_modeBeforeSlow & 1);
		bool fPreview = cs.fLiveWindow || (g_cap_modeBeforeSlow & 2);
		bool fPreviewNormal = fPreview && !g_pHistogram;
		bool fPreviewHisto = fPreview && g_pHistogram;

		CheckMenuItem(hMenu, ID_VIDEO_OVERLAY, fOverlay ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		CheckMenuItem(hMenu, ID_VIDEO_PREVIEW, fPreviewNormal ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		CheckMenuItem(hMenu, ID_VIDEO_PREVIEWHISTOGRAM, fPreviewHisto ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	}
	CheckMenuItem(hMenu, ID_VIDEO_STRETCH, g_fStretch ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIDEO_CLIPPING, g_fEnableClipping ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIDEO_ENABLEFILTERING, g_fEnableRGBFiltering ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIDEO_NOISEREDUCTION, g_fEnableNoiseReduction ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIDEO_SWAPFIELDS, g_fSwapFields ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIDEO_SQUISH_RANGE, g_fEnableLumaSquish ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);

	CheckMenuItem(hMenu, ID_VIDEO_VRNONE, g_iVertSquash == VERTSQUASH_NONE ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIDEO_VR2LINEAR, g_iVertSquash == VERTSQUASH_BY2LINEAR ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIDEO_VR2CUBIC, g_iVertSquash == VERTSQUASH_BY2CUBIC ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);

	CheckMenuItem(hMenu, ID_CAPTURE_HIDEONCAPTURE, g_fHideOnCapture ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_CAPTURE_DISPLAYLARGETIMER, g_fDisplayLargeTimer ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_CAPTURE_INFOPANEL, g_fInfoPanel ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_CAPTURE_ENABLESPILL, g_fEnableSpill ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_CAPTURE_ENABLELOGGING, g_fLogEvents ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);

	CheckMenuItem(hMenu, ID_CAPTURE_HWACCEL_NONE, g_nCaptureDDraw == kDDP_Off ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_CAPTURE_HWACCEL_TOP, g_nCaptureDDraw == kDDP_Top ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_CAPTURE_HWACCEL_BOTTOM, g_nCaptureDDraw == kDDP_Bottom ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_CAPTURE_HWACCEL_BOTH, g_nCaptureDDraw == kDDP_Both ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
}

static BOOL CaptureMenuHit(HWND hWnd, UINT id) {
	HWND hWndCapture = GetDlgItem(hWnd, IDC_CAPTURE_WINDOW);

	switch(id) {
	case ID_FILE_SETCAPTUREFILE:
		CaptureEnterSlowPeriod(hWnd);
		CaptureSetFile(hWnd, hWndCapture);
		CaptureExitSlowPeriod(hWnd);
		break;
	case ID_FILE_SETSTRIPINGSYSTEM:
		CaptureEnterSlowPeriod(hWnd);
		CaptureSetStripingSystem(hWnd, hWndCapture);
		CaptureExitSlowPeriod(hWnd);
		break;

	case ID_FILE_ALLOCATEDISKSPACE:
		CaptureEnterSlowPeriod(hWnd);
		DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_PREALLOCATE), hWnd, CaptureAllocateDlgProc, (LPARAM)hWndCapture);
		CaptureExitSlowPeriod(hWnd);
		break;
	case ID_FILE_EXITCAPTUREMODE:
		PostQuitMessage(1);
		break;
	case ID_AUDIO_COMPRESSION:
		CaptureEnterSlowPeriod(hWnd);
		CaptureChooseAudioCompression(hWnd, hWndCapture);
		CaptureExitSlowPeriod(hWnd);
		break;
	case ID_AUDIO_VOLUMEMETER:
		DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_AUDIO_VUMETER), hWnd, CaptureVumeterDlgProc, (LPARAM)hWndCapture);
		break;
	case ID_VIDEO_OVERLAY:
		{
			CAPSTATUS cs;
			BOOL fNewMode = TRUE;

			if (capGetStatus(hWndCapture, &cs, sizeof(CAPSTATUS)))
				fNewMode = !cs.fOverlayWindow;

			CaptureSetPreview(hWndCapture, false);
			capOverlay(hWndCapture, fNewMode);
			CaptureAbortSlowPeriod();
			CaptureEnablePreviewHistogram(hWndCapture, false);
		}
		break;
	case ID_VIDEO_PREVIEWHISTOGRAM:
	case ID_VIDEO_PREVIEW:
		{
			CAPSTATUS cs;
			BOOL fNewMode = TRUE;

			if (!((id==ID_VIDEO_PREVIEWHISTOGRAM)^!!g_pHistogram) && capGetStatus(hWndCapture, &cs, sizeof(CAPSTATUS)))
				fNewMode = !cs.fLiveWindow;

			capPreviewRate(hWndCapture, 1000 / 15);

			if (fNewMode) {
				if (id == ID_VIDEO_PREVIEWHISTOGRAM) {
					CaptureSetPreview(hWndCapture, false);
					CaptureAbortSlowPeriod();
					CaptureEnablePreviewHistogram(hWndCapture, true);
				} else {
					CaptureSetPreview(hWndCapture, true);
					CaptureAbortSlowPeriod();
					CaptureEnablePreviewHistogram(hWndCapture, false);
				}
			} else {
				CaptureSetPreview(hWndCapture, false);
			}
		}
		break;

	case ID_VIDEO_STRETCH:
		g_fStretch = !g_fStretch;
		break;

	case ID_VIDEO_FORMAT:
	case ID_VIDEO_CUSTOMFORMAT:
		{
			bool fHistoEnabled = !!g_pHistogram;
			bool bAccelPreview = g_bCaptureDDrawActive;

			CaptureEnablePreviewHistogram(hWndCapture, false);
			if (bAccelPreview)
				CaptureSetPreview(hWndCapture, false);

			if (id == ID_VIDEO_CUSTOMFORMAT)
				DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_CUSTOMVIDEO), hWnd, CaptureCustomVidSizeDlgProc, (LPARAM)hWndCapture);
			else
				capDlgVideoFormat(hWndCapture);

			if (bAccelPreview)
				CaptureSetPreview(hWndCapture, true);
			if (fHistoEnabled)
				CaptureEnablePreviewHistogram(hWndCapture, true);
		}
		break;

	case ID_VIDEO_SOURCE:
		capDlgVideoSource(hWndCapture);
		break;
	case ID_VIDEO_DISPLAY:
		capDlgVideoDisplay(hWndCapture);
		break;
	case ID_VIDEO_COMPRESSION_AVICAP:
		CaptureEnterSlowPeriod(hWnd);
		capDlgVideoCompression(hWndCapture);
		CaptureExitSlowPeriod(hWnd);
		break;

	case ID_VIDEO_COMPRESSION_INTERNAL:
		CaptureEnterSlowPeriod(hWnd);
		CaptureInternalSelectCompression(hWndCapture);
		CaptureExitSlowPeriod(hWnd);
		break;

	case ID_VIDEO_FILTERS:
		ActivateDubDialog(g_hInst, MAKEINTRESOURCE(IDD_FILTERS), hWnd, FilterDlgProc);
		break;

	case ID_VIDEO_ENABLEFILTERING:
		g_fEnableRGBFiltering = !g_fEnableRGBFiltering;
		break;

	case ID_VIDEO_NOISEREDUCTION:
		g_fEnableNoiseReduction = !g_fEnableNoiseReduction;
		break;
	case ID_VIDEO_NOISEREDUCTION_THRESHOLD:
		CaptureToggleNRDialog(hWnd);
		break;

	case ID_VIDEO_SWAPFIELDS:
		g_fSwapFields = !g_fSwapFields;
		break;

	case ID_VIDEO_SQUISH_RANGE:
		g_fEnableLumaSquish = !g_fEnableLumaSquish;
		break;

	case ID_VIDEO_VRNONE:		g_iVertSquash = VERTSQUASH_NONE;		break;
	case ID_VIDEO_VR2LINEAR:	g_iVertSquash = VERTSQUASH_BY2LINEAR;	break;
	case ID_VIDEO_VR2CUBIC:		g_iVertSquash = VERTSQUASH_BY2CUBIC;	break;

	case ID_VIDEO_CLIPPING:		g_fEnableClipping = !g_fEnableClipping;	break;
	case ID_VIDEO_CLIPPING_SET:
		{
			bool fHistoEnabled = !!g_pHistogram;

			CaptureEnablePreviewHistogram(hWndCapture, false);

			CaptureShowClippingDialog(hWndCapture);

			if (fHistoEnabled)
				CaptureEnablePreviewHistogram(hWndCapture, true);
		}
		break;

	case ID_VIDEO_HISTOGRAM:
		DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_HISTOGRAM), hWnd, CaptureHistogramDlgProc, (LPARAM)hWndCapture);
		break;

	case ID_VIDEO_BT8X8TWEAKER:
		CaptureDisplayBT848Tweaker(hWnd);
		break;

	case ID_CAPTURE_SETTINGS:
		{
			bool b = g_bCaptureDDrawActive;

			if (b)
				CaptureSetPreview(hWndCapture, false);
			else
				CaptureEnterSlowPeriod(hWnd);

			DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_SETTINGS), hWnd, CaptureSettingsDlgProc, (LPARAM)hWndCapture);

			if (b)
				CaptureSetPreview(hWndCapture, true);
			else
				CaptureExitSlowPeriod(hWnd);
		}
		break;

	case ID_CAPTURE_PREFERENCES:
		CaptureEnterSlowPeriod(hWnd);
		DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_PREFERENCES), hWnd, CapturePreferencesDlgProc, (LPARAM)hWndCapture);
		CaptureExitSlowPeriod(hWnd);
		break;

	case ID_CAPTURE_STOPCONDITIONS:
		CaptureEnterSlowPeriod(hWnd);
		DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_STOPCOND), hWnd, CaptureStopConditionsDlgProc, (LPARAM)hWndCapture);
		CaptureExitSlowPeriod(hWnd);
		break;

	case ID_CAPTURE_TIMING:
		CaptureEnterSlowPeriod(hWnd);
		DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_TIMING), hWnd, CaptureTimingDlgProc, (LPARAM)hWndCapture);
		CaptureExitSlowPeriod(hWnd);
		break;

	case ID_CAPTURE_DISKIO:
		CaptureEnterSlowPeriod(hWnd);
		DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_DISKIO), hWnd, CaptureDiskIODlgProc, (LPARAM)hWndCapture);
		CaptureExitSlowPeriod(hWnd);
		break;

	case ID_CAPTURE_SPILLSYSTEM:
		CaptureEnterSlowPeriod(hWnd);
		DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_SPILLSETUP), hWnd, CaptureSpillDlgProc, (LPARAM)hWndCapture);
		CaptureExitSlowPeriod(hWnd);
		break;

	case ID_CAPTURE_DISPLAYCAPTURELOG:
		CaptureEnterSlowPeriod(hWnd);
		g_capLog.Display(hWnd);
		CaptureExitSlowPeriod(hWnd);
		break;

	case ID_CAPTURE_CAPTUREVIDEO:
		if (g_capStripeSystem) {
			MessageBox(hWnd, "AVICap cannot capture to a striped AVI system.", g_szError, MB_OK);
		} else {
			CapturePriorityWhacker cpw(hWndCapture);

			CaptureAVICap(hWnd, hWndCapture);
		}
		break;

	case ID_CAPTURE_CAPTUREVIDEOINTERNAL:
		{
			CapturePriorityWhacker cpw(hWndCapture);

			CaptureInternal(hWnd, hWndCapture, false);
		}
		break;

	case ID_CAPTURE_TEST:
		{
			CapturePriorityWhacker cpw(hWndCapture);

			CaptureInternal(hWnd, hWndCapture, true);
		}
		break;

	case ID_CAPTURE_HIDEONCAPTURE:
		g_fHideOnCapture = !g_fHideOnCapture;
		break;

	case ID_CAPTURE_DISPLAYLARGETIMER:
		g_fDisplayLargeTimer = !g_fDisplayLargeTimer;
		break;

	case ID_CAPTURE_INFOPANEL:
		g_fInfoPanel = !g_fInfoPanel;
		ShowWindow(GetDlgItem(hWnd, IDC_CAPTURE_PANEL), g_fInfoPanel ? SW_SHOW : SW_HIDE);
		InvalidateRect(hWnd, NULL, TRUE);
		break;

	case ID_CAPTURE_ENABLESPILL:
		g_fEnableSpill = !g_fEnableSpill;
		break;

	case ID_CAPTURE_ENABLELOGGING:
		g_fLogEvents = !g_fLogEvents;
		break;

	case ID_CAPTURE_HWACCEL_NONE:
		if (g_bCaptureDDrawActive)
			CaptureSetPreview(hWndCapture, false);

		g_nCaptureDDraw = kDDP_Off;
		g_DDContext.Shutdown();
		break;

	case ID_CAPTURE_HWACCEL_TOP:
		{
			CAPSTATUS cs;

			if (capGetStatus(hWndCapture, &cs, sizeof(CAPSTATUS))) {
				if (cs.fLiveWindow)
					CaptureSetPreview(hWndCapture, false);

				g_nCaptureDDraw = kDDP_Top;

				if (cs.fLiveWindow)
					CaptureSetPreview(hWndCapture, true);
			}
		}
		break;

	case ID_CAPTURE_HWACCEL_BOTTOM:
		{
			CAPSTATUS cs;

			if (capGetStatus(hWndCapture, &cs, sizeof(CAPSTATUS))) {
				if (cs.fLiveWindow)
					CaptureSetPreview(hWndCapture, false);

				g_nCaptureDDraw = kDDP_Bottom;

				if (cs.fLiveWindow)
					CaptureSetPreview(hWndCapture, true);
			}
		}
		break;

	case ID_CAPTURE_HWACCEL_BOTH:
		{
			CAPSTATUS cs;

			if (capGetStatus(hWndCapture, &cs, sizeof(CAPSTATUS))) {
				if (cs.fLiveWindow)
					CaptureSetPreview(hWndCapture, false);

				g_nCaptureDDraw = kDDP_Both;

				if (cs.fLiveWindow)
					CaptureSetPreview(hWndCapture, true);
			}
		}
		break;

	case ID_HELP_CONTENTS:
		HelpShowHelp(hWnd);
		break;

	default:
		if (id >= ID_VIDEO_CAPTURE_DRIVER && id < ID_VIDEO_CAPTURE_DRIVER+10) {
			CaptureSelectDriver(hWnd, hWndCapture, id - ID_VIDEO_CAPTURE_DRIVER);
			CaptureAbortSlowPeriod();
		} else if (id >= ID_AUDIOMODE_11KHZ_8MONO && id <= ID_AUDIOMODE_44KHZ_16STEREO) {
			id -= ID_AUDIOMODE_11KHZ_8MONO;
			CaptureSetPCMAudioFormat(hWndCapture,
					11025<<(id/4),
					id & 2,
					id & 1);
		} else if (id >= ID_FRAMERATE_6000FPS && id <= ID_FRAMERATE_1493FPS) {
			CaptureSetFrameTime(hWndCapture, g_predefFrameRates[id - ID_FRAMERATE_6000FPS]);
		} else
			return FALSE;

		break;
	}
	CaptureResizeWindow(hWndCapture);
	CaptureShowParms(hWnd);

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////
//
//	callbacks
//
///////////////////////////////////////////////////////////////////////////

static const struct {
	int id;
	const char *szError;
} g_betterCaptureErrors[]={
	{ 434,	"Warning: No frames captured.\n"
			"\n"
			"Make sure your capture card is functioning correctly and that a valid video source "
			"is connected.  You might also try turning off overlay, reducing the image size, or "
			"reducing the image depth to 24 or 16-bit." },
	{ 439,	"Error: Cannot find a driver to draw this non-RGB image format.  Preview and histogram functions will be unavailable." },
};

LRESULT CALLBACK CaptureErrorCallback(HWND hWnd, int nID, LPCSTR lpsz) {
	char buf[256];
	int i;

	if (!nID) return 0;

	for(i=0; i<sizeof g_betterCaptureErrors/sizeof g_betterCaptureErrors[0]; i++)
		if (g_betterCaptureErrors[i].id == nID) {
			MessageBox(GetParent(hWnd), g_betterCaptureErrors[i].szError, "VirtualDub capture error", MB_OK);
			return 0;
		}

	wsprintf(buf, "Error %d: %s", nID, lpsz);
	MessageBox(GetParent(hWnd), buf, "VirtualDub capture error", MB_OK);
	_RPT1(0,"%s\n",buf);

	return 0;
}

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
	} catch(MyError e) {
		guiSetStatus("Histogram: %s", 0, e.gets());
	}

	return 0;
}

LRESULT CALLBACK CaptureOverlayFrameCallback(HWND hWnd, VIDEOHDR *lpVHdr) {
	if (g_bCaptureDDrawActive) {
		switch(g_nCaptureDDraw) {
		case kDDP_Top:
			g_DDContext.LockAndLoad(lpVHdr->lpData, 0, 2);
			break;
		case kDDP_Bottom:
			g_DDContext.LockAndLoad(lpVHdr->lpData, 1, 2);
			break;
		case kDDP_Both:
			g_DDContext.LockAndLoad(lpVHdr->lpData, 0, 1);
			break;
		}
	}

	return 0;
}

LRESULT CALLBACK CaptureStatusCallback(HWND hWnd, int nID, LPCSTR lpsz) {
	char buf[256];

	// Intercept nID=510 (per frame info)

	if (nID == 510)
		return 0;

	if (nID) {
		wsprintf(buf, "Status %d: %s", nID, lpsz);
		SendMessage(GetDlgItem(GetParent(hWnd), IDC_STATUS_WINDOW), SB_SETTEXT, 0, (LPARAM)buf);
		_RPT1(0,"%s\n",buf);
	} else {
		SendMessage(GetDlgItem(GetParent(hWnd), IDC_STATUS_WINDOW), SB_SETTEXT, 0, (LPARAM)"");
	}

	return 0;
}


///////////////////////////////////////////////////////////////////////////
//
//	window procedures
//
///////////////////////////////////////////////////////////////////////////

#define MYWM_STATUSBAR_HIT	(WM_USER+100)

void CaptureRedoWindows(HWND hWnd) {
	HWND hWndStatus = GetDlgItem(hWnd, IDC_STATUS_WINDOW);
	HWND hWndPanel = GetDlgItem(hWnd, IDC_CAPTURE_PANEL);
	HWND hwndCapture = GetDlgItem(hWnd, IDC_CAPTURE_WINDOW);
	RECT rClient, rStatus, rPanel, rCapture;
	INT aWidth[8];
	int nParts;
	HDWP hdwp;
	HDC hdc;
	int		xedge = GetSystemMetrics(SM_CXEDGE);
	int		yedge = GetSystemMetrics(SM_CYEDGE);

	GetClientRect(hWnd, &rClient);
	GetWindowRect(hWndStatus, &rStatus);
	GetWindowRect(hWndPanel, &rPanel);
	GetWindowRect(hwndCapture, &rCapture);

	hdwp = BeginDeferWindowPos(3);

	guiDeferWindowPos(hdwp, hWndStatus,
				NULL,
				rClient.left,
				rClient.bottom - (rStatus.bottom-rStatus.top),
				rClient.right-rClient.left,
				rStatus.bottom-rStatus.top,
				SWP_NOACTIVATE|SWP_NOZORDER/*|SWP_NOCOPYBITS*/);

	guiDeferWindowPos(hdwp, hWndPanel,
				NULL,
				rClient.right - (rPanel.right - rPanel.left),
				rClient.top,
				rPanel.right - rPanel.left,
				rClient.bottom - (rStatus.bottom-rStatus.top),
				SWP_NOACTIVATE|SWP_NOZORDER/*|SWP_NOCOPYBITS*/);

	if (g_fStretch) {
		guiDeferWindowPos(hdwp, GetDlgItem(hWnd, IDC_CAPTURE_WINDOW),
				NULL,
				xedge, yedge,
				rClient.right - (g_fInfoPanel ? (rPanel.right-rPanel.left) : 0) - xedge*2,
				rClient.bottom - (g_fInfoPanel ? (rStatus.bottom-rStatus.top) : 0) - yedge*2,
				SWP_NOACTIVATE|SWP_NOZORDER/*|SWP_NOCOPYBITS*/);

	} else {
		int sx = rClient.right - (g_fInfoPanel ? (rPanel.right-rPanel.left) : 0) - xedge*2; 
		int sy = rClient.bottom - (g_fInfoPanel ? (rStatus.bottom-rStatus.top) : 0) - yedge*2;
		CAPSTATUS cs;

		capGetStatus(hwndCapture, &cs, sizeof(CAPSTATUS));

		if (sx > cs.uiImageWidth)
			sx = cs.uiImageWidth;

		if (sy > cs.uiImageHeight)
			sy = cs.uiImageHeight;

		guiDeferWindowPos(hdwp, GetDlgItem(hWnd, IDC_CAPTURE_WINDOW),
				NULL,
				xedge, yedge,
				sx, sy,
				SWP_NOACTIVATE|SWP_NOZORDER/*|SWP_NOCOPYBITS*/);
	}

	guiEndDeferWindowPos(hdwp);

	CaptureMoveWindow(hwndCapture);

	if ((nParts = SendMessage(hWndStatus, SB_GETPARTS, 0, 0))>1) {
		int i;
		INT xCoord = (rClient.right-rClient.left) - (rStatus.bottom-rStatus.top);

		aWidth[nParts-2] = xCoord;

		for(i=nParts-3; i>=0; i--) {
			xCoord -= 60;
			aWidth[i] = xCoord;
		}
		aWidth[nParts-1] = -1;

		SendMessage(hWndStatus, SB_SETPARTS, nParts, (LPARAM)aWidth);
	}

	if (hdc = GetDC(hWnd)) {
		RECT r;

		r.left = r.top = 0;
		r.right = rClient.right;
		r.bottom = rClient.bottom;

		if (g_fInfoPanel) {
			r.right -= (rPanel.right - rPanel.left);
			r.bottom -= (rStatus.bottom-rStatus.top);
		}

		DrawEdge(hdc, &r, EDGE_SUNKEN, BF_RECT);

		// Yes, this is lame, but oh well.

		r.left = xedge;
		r.top = yedge;
		r.right -= xedge;
		r.bottom -= yedge;

		FillRect(hdc, &r, (HBRUSH)(COLOR_3DFACE+1));

		ReleaseDC(hWnd, hdc);
	}
}

static LONG APIENTRY CaptureWndProc( HWND hWnd, UINT message, UINT wParam, LONG lParam)
{
	if (g_fRestricted) {
		switch (message) {

		case WM_COMMAND:
			break;

		case WM_CLOSE:
			break;

		case WM_DESTROY:		// doh!!!!!!!
			PostQuitMessage(0);
			break;

		case WM_NCHITTEST:
			return HTCLIENT;

		default:
			return (DefWindowProc(hWnd, message, wParam, lParam));
		}
		return (0);
	}

    switch (message) {

	case WM_ENTERMENULOOP:
		CaptureEnterSlowPeriod(hWnd);
		break;

	case WM_EXITMENULOOP:
		CaptureExitSlowPeriod(hWnd);
		break;

	case WM_INITMENU:
		CaptureInitMenu(hWnd, (HMENU)wParam);
		break;

	case WM_COMMAND:
		if (!CaptureMenuHit(hWnd, LOWORD(wParam)))
			return (DefWindowProc(hWnd, message, wParam, lParam));
		break;

	case WM_MENUSELECT:
		guiMenuHelp(hWnd, wParam, 0, iCaptureMenuHelpTranslator);
		break;

	case MYWM_STATUSBAR_HIT:
		TrackPopupMenu(
				GetSubMenu(g_hMenuAuxCapture, wParam),
				TPM_CENTERALIGN | TPM_LEFTBUTTON,
				LOWORD(lParam), HIWORD(lParam),
				0, hWnd, NULL);
		break;

	case WM_MOVE:
		CaptureMoveWindow(GetDlgItem(hWnd, IDC_CAPTURE_WINDOW));
		break;

	case WM_SIZE:
		CaptureRedoWindows(hWnd);
		break;

	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;

	case WM_DESTROY:		// doh!!!!!!!
		PostQuitMessage(0);
		break;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc;

			if (hdc = BeginPaint(hWnd, &ps)) {
				RECT r;

				if (g_fInfoPanel) {
					GetWindowRect(GetDlgItem(hWnd, IDC_CAPTURE_PANEL), &r);
					ScreenToClient(hWnd, (LPPOINT)&r + 0);
					ScreenToClient(hWnd, (LPPOINT)&r + 1);

					r.right = r.left;
					r.left = 0;
				} else {
					GetClientRect(hWnd, &r);
				}

				DrawEdge(hdc, &r, EDGE_SUNKEN, BF_RECT);
				EndPaint(hWnd, &ps);
			}
		}
		return 0;

	default:
		return (DefWindowProc(hWnd, message, wParam, lParam));
    }
    return (0);
}

static LONG APIENTRY CaptureStatusWndProc( HWND hWnd, UINT message, UINT wParam, LONG lParam) {
	switch(message) {
	case WM_LBUTTONDOWN:
		if (wParam & MK_LBUTTON) {
			RECT r;
			POINT pt;
			int i;

			pt.x = LOWORD(lParam);
			pt.y = HIWORD(lParam);

			for(i=0; i<2; i++) {
				if (SendMessage(hWnd, SB_GETRECT, i+1, (LPARAM)&r)) {
					if (PtInRect(&r, pt)) {
						ClientToScreen(hWnd, &pt);
						SendMessage(GetParent(hWnd), MYWM_STATUSBAR_HIT, i, (LPARAM)MAKELONG(pt.x, pt.y));
					}
				}
			}
		}
		break;

	case WM_NCHITTEST:
		if (g_fRestricted)
			return HTCLIENT;
	default:
		return CallWindowProc((WNDPROC)GetWindowLong(hWnd, GWL_USERDATA), hWnd, message, wParam, lParam);
	}
	return 0;
}


static BOOL CALLBACK CapturePanelDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:
		return TRUE;

	case WM_APP+0:
		{
			CaptureData *pcd = (CaptureData *)lParam;
			char buf[256];
			long l, time_diff;
			long lVideoRate, lAudioRate;
			__int64 i64;

			sprintf(buf, "%ld", pcd->total_cap);
			SetDlgItemText(hdlg, IDC_FRAMES, buf);

			ticks_to_str(buf, pcd->lCurrentMS);
			SetDlgItemText(hdlg, IDC_TIME_TOTAL, buf);

			l = pcd->CPU.read();

			if (l >= 0) {
				sprintf(buf, "%ld%%", l);
				SetDlgItemText(hdlg, IDC_CPU_USAGE, buf);
			}

			////////

			size_to_str(buf, pcd->total_video_size);
			SetDlgItemText(hdlg, IDC_VIDEO_SIZE, buf);

			time_diff = pcd->lVideoLastMS - pcd->lVideoFirstMS;

#if 1
			if (time_diff >= 1000 && pcd->total_cap >= 2) {
				l = MulDiv(pcd->total_cap-1, 100000000, time_diff);
				if (l<0) l=0;
				sprintf(buf, "%ld.%05ld fps", l/100000, l%100000);
				SetDlgItemText(hdlg, IDC_VIDEO_RATE, buf);
			}
#else
			if (time_diff >= 1000 && pcd->total_cap >= 2) {
				double d = ((double)time_diff*1000.0 / (pcd->total_cap-1));

				sprintf(buf, "%.2lf us", d);
				SetDlgItemText(hdlg, IDC_VIDEO_RATE, buf);
			}
#endif

			if (time_diff >= 1000)
				lVideoRate = (pcd->total_video_size*1000 + time_diff/2) / time_diff;
			else
				lVideoRate = 0;
			sprintf(buf, "%ldK/s", (lVideoRate+1023)/1024);
			SetDlgItemText(hdlg, IDC_VIDEO_DATARATE, buf);

			if (pcd->total_video_size && pcd->total_cap>=2) {
				long l;

				l = ((__int64)pcd->uncompressed_frame_size * (pcd->total_cap-1) * 10 + pcd->total_video_size/2) / pcd->total_video_size;
				sprintf(buf, "%ld.%c:1", l/10, (char)('0' + l%10));
				SetDlgItemText(hdlg, IDC_VIDEO_RATIO, buf);

				l = pcd->total_video_size / (pcd->total_cap-1) - 24;
				sprintf(buf, "%ld", l);
				SetDlgItemText(hdlg, IDC_VIDEO_AVGFRAMESIZE, buf);
			}

			sprintf(buf, "%ld", pcd->dropped);
			SetDlgItemText(hdlg, IDC_VIDEO_DROPPED, buf);

			/////////

			size_to_str(buf, pcd->total_audio_size);
			SetDlgItemText(hdlg, IDC_AUDIO_SIZE, buf);

			// bytes -> samples/sec
			// bytes / (bytes/sec) = sec
			// bytes / (bytes/sec) * (samples/sec) / sec = avg-samples/sec

			if (pcd->lAudioLastMS >= 1000) {
				long ratio;

				if (pcd->iAudioHzSamples >= 4) {

					// m = [n(sumXY) - (sumX)(sumY)] / [n(sumX^2)-(sumX)^2)]
					//
					// this m would be a ratio in (bytes/ms).
					//
					// (bytes/ms) * 1000 = actual bytes/sec
					// multiply by nSamplesPerSec/nAvgBytesPerSec -> actual samples/sec
					//
					// *sigh* we need doubles here...

					double x = pcd->i64AudioHzX;
					double y = pcd->i64AudioHzY;
					double x2 = pcd->i64AudioHzX2;
					double y2 = pcd->i64AudioHzY2;
					double xy = pcd->i64AudioHzXY;
					double n = pcd->iAudioHzSamples;

					l = (long)floor(0.5 + 
								((n*xy - x*y) * (100.0 * 1000 * pcd->wfex.nSamplesPerSec))
								/
								((n*x2 - x*x) * pcd->wfex.nAvgBytesPerSec)
						);

					sprintf(buf, "%d.%02dHz", l/100, l%100

								

//							int64divto32(
//								(pcd->total_audio_data_size-pcd->audio_first_size) * pcd->wfex.nSamplesPerSec * 1000,
//								(__int64)pcd->wfex.nAvgBytesPerSec * (pcd->lAudioLastMS-pcd->lAudioFirstMS)
//								)
							);
					SetDlgItemText(hdlg, IDC_AUDIO_RATE, buf);
				}

				if (pcd->wfex.wFormatTag == WAVE_FORMAT_PCM)
					SetDlgItemText(hdlg, IDC_AUDIO_RATIO, "1.0:1");
				else if (pcd->lAudioLastMS > pcd->lAudioFirstMS) {
					ratio = int64divto32(
							(__int64)(pcd->lAudioLastMS-pcd->lAudioFirstMS) * pcd->wfex.nChannels * pcd->wfex.nSamplesPerSec,
							(pcd->total_audio_data_size-pcd->audio_first_size) * 50
						);

					sprintf(buf, "%ld.%c:1", ratio/10, (char)(ratio%10 + '0'));
					SetDlgItemText(hdlg, IDC_AUDIO_RATIO, buf);
				}

				lAudioRate = (pcd->total_audio_size*1000 + pcd->lCurrentMS/2) / pcd->lAudioLastMS;
				sprintf(buf,"%ldK/s", (lAudioRate+1023)/1024);
				SetDlgItemText(hdlg, IDC_AUDIO_DATARATE, buf);

				sprintf(buf,"%+ld ms", pcd->lVideoAdjust);
				SetDlgItemText(hdlg, IDC_AUDIO_CORRECTIONS, buf);
			} else {
				lAudioRate = 0;
				SetDlgItemText(hdlg, IDC_AUDIO_RATE, "(n/a)");
				SetDlgItemText(hdlg, IDC_AUDIO_RATIO, "(n/a)");
				SetDlgItemText(hdlg, IDC_AUDIO_DATARATE, "(n/a)");
			}

			///////////////

			if (g_fEnableSpill)
				i64 = CapSpillGetFreeSpace();
			else
				i64 = MyGetDiskFreeSpace(pcd->szCaptureRoot[0] ? pcd->szCaptureRoot : NULL);

			if (i64>=0) {
				size_to_str(buf, i64);
				SetDlgItemText(hdlg, IDC_DISK_FREE, buf);

				if (i64)
					pcd->disk_free = i64;
				else
					pcd->disk_free = -1;
			}

			if (lVideoRate + lAudioRate > 16) {

				// 2Gb restriction lifted

//				l = 0x7FFFFFFF - 2048 - pcd->total_video_size - pcd->total_audio_size;
				if (i64 < 0) i64=0;
//				if (i64 > l) i64 = l;

				ticks_to_str(buf, (long)(i64 * 1000 / (lVideoRate + lAudioRate)));
				SetDlgItemText(hdlg, IDC_TIME_LEFT, buf);
			}

			size_to_str(buf, 4096 + pcd->total_video_size + pcd->total_audio_size);
			SetDlgItemText(hdlg, IDC_FILE_SIZE, buf);
		}
		return TRUE;
	}

	return FALSE;
}



static LONG APIENTRY CaptureSubWndProc( HWND hWnd, UINT message, UINT wParam, LONG lParam) {
	if (g_bCaptureDDrawActive) {
		switch(message) {
		case WM_ERASEBKGND:
			return 0;

		case WM_PAINT:
			{
				int key = g_DDContext.getColorKey();
				PAINTSTRUCT ps;
				HDC hdc;

				hdc = BeginPaint(hWnd, &ps);

				if (key >= 0) {
					HBRUSH hbrColorKey;
					RECT r;

					if (hbrColorKey = CreateSolidBrush((COLORREF)key)) {
						GetClientRect(hWnd, &r);
						FillRect(hdc, &r, hbrColorKey);
						DeleteObject(hbrColorKey);
					}
				}

				EndPaint(hWnd, &ps);
			}
			return 0;

		case WM_TIMER:
			RydiaEnableAVICapInvalidate(true);
			CallWindowProc(g_pCapWndProc, hWnd, message, wParam, lParam);
			RydiaEnableAVICapInvalidate(false);
			return 0;
		}
	}
	return CallWindowProc(g_pCapWndProc, hWnd, message, wParam, lParam);
}

///////////////////////////////////////////////////////////////////////////
//
//	entry point
//
///////////////////////////////////////////////////////////////////////////


static LRESULT CaptureMsgPump(HWND hWnd) {
	MSG msg;

	__try {

	    while (GetMessage(&msg,NULL,0,0)) {
			if (guiCheckDialogs(&msg)) continue;
			if (!TranslateAccelerator(hWnd, g_hAccelCapture, &msg)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	} __except(CaptureIsCatchableException(GetExceptionCode())) {
		throw MyCrashError(
				"%s in capture routine.\n\nVirtualDub has intercepted a crash condition in one of its "
				"routines. You should save any remaining data and exit VirtualDub immediately. If you "
				"are running Windows 95/98, you should also reboot afterward.", GetExceptionCode());
	}

	return msg.wParam;
}

void Capture(HWND hWnd) {
	static INT aWidths[]={ 50, 100, 150, 200, -1 };

	HMENU	hMenuCapture	= NULL;
	HMENU	hMenuOld		= NULL;
	DWORD	dwOldWndProc	= 0;
	DWORD	dwOldStatusWndProc	= 0;
	HWND	hWndCapture		= NULL;
	HWND	hWndStatus;
	HWND	hwndItem;
	int		nDriver;
	bool	fCodecInstalled;

	int		xedge = GetSystemMetrics(SM_CXEDGE);
	int		yedge = GetSystemMetrics(SM_CYEDGE);

	// Mark starting point.

	// ICInstall() is a stupid function.  Really, it's a moron.

	VDCHECKPOINT;

	fCodecInstalled = !!ICInstall(ICTYPE_VIDEO, 'BUDV', (LPARAM)VCMDriverProc, NULL, ICINSTALL_FUNCTION);

	try {
		hWndStatus = GetDlgItem(hWnd, IDC_STATUS_WINDOW);

		SendMessage(hWndStatus, SB_SETTEXT, 0, (LPARAM)"Initializing Capture Mode...");
		UpdateWindow(hWndStatus);

		// load menus & accelerators

		if (	!(g_hMenuAuxCapture = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_CAPTURE_AUXMENU)))
			||	!(hMenuCapture = LoadMenu(g_hInst, MAKEINTRESOURCE(IDR_CAPTURE_MENU))))

			throw MyError("Can't load capture menus.");

		if (!(g_hAccelCapture = LoadAccelerators(g_hInst, MAKEINTRESOURCE(IDR_CAPTURE_KEYS))))
			throw MyError("Can't load accelerators.");

		// ferret out drivers

		VDCHECKPOINT;

		if ((nDriver = CaptureAddDrivers(hWnd, GetSubMenu(hMenuCapture,2))) < 0)
			throw MyError("No capture driver available.");

		CaptureWarnCheckDrivers(hWnd);

		hMenuOld = GetMenu(hWnd);
		SetMenu(hWnd, hMenuCapture);

		VDCHECKPOINT;

		if (!(hWndCapture = capCreateCaptureWindow((LPSTR)"Capture window", WS_VISIBLE|WS_CHILD, xedge, yedge, 160, 120, hWnd, IDC_CAPTURE_WINDOW)))
			throw MyError("Can't create capture window.");

		// subclass the capture window

		g_pCapWndProc = (WNDPROC)GetWindowLong(hWndCapture, GWL_WNDPROC);

		SetWindowLong(hWndCapture, GWL_WNDPROC, (LONG)CaptureSubWndProc);

		capSetCallbackOnError(hWndCapture, (LPVOID)CaptureErrorCallback);
		capSetCallbackOnStatus(hWndCapture, (LPVOID)CaptureStatusCallback);
		capSetCallbackOnYield(hWndCapture, (LPVOID)CaptureYieldCallback);

		VDCHECKPOINT;

		if (!CaptureSelectDriver(hWnd, hWndCapture, nDriver))
			throw MyUserAbortError();

		capCaptureSetSetup(hWndCapture, &g_defaultCaptureParms, sizeof(CAPTUREPARMS));
		
		// If the user has selected a default capture file, use it; if not, 

		if (QueryConfigString(g_szCapture, g_szDefaultCaptureFile, g_szCaptureFile, sizeof g_szCaptureFile) && g_szCaptureFile[0])
			CaptureSetCaptureFile(hWndCapture);
		else
			capFileGetCaptureFile(hWndCapture, g_szCaptureFile, sizeof g_szCaptureFile);

		// How about default capture settings?

		VDCHECKPOINT;

		CaptureInternalLoadFromRegistry();

		{
			CAPTUREPARMS *cp;
			DWORD dwSize, dwSizeAlloc;

			if (dwSize = QueryConfigBinary(g_szCapture, g_szCapSettings, NULL, 0)) {
				dwSizeAlloc = dwSize;
				if (dwSize < sizeof(CAPTUREPARMS)) dwSize = sizeof(CAPTUREPARMS);

				if (cp = (CAPTUREPARMS *)allocmem(dwSizeAlloc)) {
					memset(cp, 0, dwSizeAlloc);

					if (QueryConfigBinary(g_szCapture, g_szCapSettings, (char *)cp, dwSize)) {
						cp->fYield = FALSE;
						cp->fMCIControl = FALSE;

						capCaptureSetSetup(hWndCapture, cp, dwSize);
					}

					freemem(cp);
				}
			}
		}

		// And default video parameters?

		{
			BITMAPINFOHEADER *bih;
			DWORD dwSize;

			if (dwSize = QueryConfigBinary(g_szCapture, g_szVideoFormat, NULL, 0)) {
				if (bih = (BITMAPINFOHEADER *)allocmem(dwSize)) {
					if (QueryConfigBinary(g_szCapture, g_szVideoFormat, (char *)bih, dwSize))
						capSetVideoFormat(hWndCapture, bih, dwSize);

					freemem(bih);
				}
			}
		}

		// Audio?

		{
			WAVEFORMATEX *wfex;
			DWORD dwSize;

			if (dwSize = QueryConfigBinary(g_szCapture, g_szAudioFormat, NULL, 0)) {
				if (wfex = (WAVEFORMATEX *)allocmem(dwSize)) {
					if (QueryConfigBinary(g_szCapture, g_szAudioFormat, (char *)wfex, dwSize))
						capSetAudioFormat(hWndCapture, wfex, dwSize);

					freemem(wfex);
				}
			}
		}

		// stop conditions?

		{
			void *mem;
			DWORD dwSize;

			if (dwSize = QueryConfigBinary(g_szCapture, g_szStopConditions, NULL, 0)) {
				if (mem = (WAVEFORMATEX *)allocmem(dwSize)) {
					if (QueryConfigBinary(g_szCapture, g_szStopConditions, (char *)mem, dwSize)) {
						memset(&g_stopPrefs, 0, sizeof g_stopPrefs);
						memcpy(&g_stopPrefs, mem, min(sizeof g_stopPrefs, dwSize));
					}

					freemem(mem);
				}
			}
		}

		// Disk I/O settings?

		QueryConfigDword(g_szCapture, g_szChunkSize, (DWORD *)&g_diskChunkSize);
		QueryConfigDword(g_szCapture, g_szChunkCount, (DWORD *)&g_diskChunkCount);
		QueryConfigDword(g_szCapture, g_szDisableBuffering, (DWORD *)&g_diskDisableBuffer);

		// panel, timing?

		{
			DWORD dw;

			if (QueryConfigDword(g_szCapture, g_szHideInfoPanel, &dw))
				g_fInfoPanel = !!dw;

			if (QueryConfigDword(g_szCapture, g_szAdjustVideoTiming, &dw))
				g_fAdjustVideoTimer = !!dw;
		}

		// Spill settings?

		CapSpillRestoreFromRegistry();

		// Hide the position window

		VDCHECKPOINT;

		ShowWindow(GetDlgItem(hWnd, IDC_POSITION), SW_HIDE); 

		// Setup the status window.

		SendMessage(hWndStatus, SB_SIMPLE, (WPARAM)FALSE, 0);
		SendMessage(hWndStatus, SB_SETPARTS, (WPARAM)5, (LPARAM)(LPINT)aWidths);
		SendMessage(hWndStatus, SB_SETTEXT, 4 | SBT_NOBORDERS, (LPARAM)"");

		// Subclass the status window.

		dwOldStatusWndProc = GetWindowLong(hWndStatus, GWL_WNDPROC);
		SetWindowLong(hWndStatus, GWL_USERDATA, (DWORD)dwOldStatusWndProc);
		SetWindowLong(hWndStatus, GWL_WNDPROC, (DWORD)CaptureStatusWndProc);

		// Update status

		VDCHECKPOINT;

		CaptureResizeWindow(hWndCapture);
		CaptureShowParms(hWnd);
		CaptureShowFile(hWnd, hWndCapture, false);

		// Create capture panel

		VDCHECKPOINT;

		hwndItem = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_PANEL), hWnd, CapturePanelDlgProc);
		if (hwndItem) {
			SetWindowLong(hwndItem, GWL_ID, IDC_CAPTURE_PANEL);

			if (g_fInfoPanel)
				ShowWindow(hwndItem, SW_SHOWNORMAL);
		}

		// Subclass the main window.

		dwOldWndProc = GetWindowLong(hWnd, GWL_WNDPROC);
		SetWindowLong(hWnd, GWL_WNDPROC, (DWORD)CaptureWndProc);

		VDCHECKPOINT;

		CaptureRedoWindows(hWnd);

		SendMessage(hWndStatus, SB_SETTEXT, 0, (LPARAM)"Capture mode ready.");

		VDCHECKPOINT;

		if (!CaptureMsgPump(hWnd)) PostQuitMessage(0);

		VDCHECKPOINT;

		// save settings

		{
			DWORD dw;

			if (QueryConfigDword(g_szCapture, g_szHideInfoPanel, &dw) && !!dw != g_fInfoPanel)
				SetConfigDword(g_szCapture, g_szHideInfoPanel, g_fInfoPanel);
		}

	} catch(MyUserAbortError e) {
	} catch(MyError e) {
		e.post(hWnd, g_szError);
	}

	// close up shop

	VDCHECKPOINT;

	CaptureCloseBT848Tweaker();

	if (g_pHistogram)
		CaptureEnablePreviewHistogram(hWndCapture, false);

	if (hwndItem = GetDlgItem(hWnd, IDC_CAPTURE_PANEL))
		DestroyWindow(hwndItem);

	ShowWindow(GetDlgItem(hWnd, IDC_POSITION), SW_SHOWNORMAL); 
	SendMessage(GetDlgItem(hWnd, IDC_STATUS_WINDOW), SB_SIMPLE, (WPARAM)TRUE, 0);
	guiRedoWindows(hWnd);

	if (dwOldWndProc)	SetWindowLong(hWnd, GWL_WNDPROC, dwOldWndProc);
	if (dwOldStatusWndProc)	SetWindowLong(hWndStatus, GWL_WNDPROC, dwOldStatusWndProc);

	VDCHECKPOINT;

	if (hWndCapture) {
		capOverlay(hWndCapture, FALSE);
		capPreview(hWndCapture, FALSE);
		capDriverDisconnect(hWndCapture);
		DestroyWindow(hWndCapture);
	}

	g_DDContext.Shutdown();

	if (hMenuOld)		SetMenu(hWnd, hMenuOld);
	if (hMenuCapture)	DestroyMenu(hMenuCapture);
	if (g_hMenuAuxCapture)	{ DestroyMenu(g_hMenuAuxCapture); g_hMenuAuxCapture=NULL; }

	FreeCompressor(&g_compression);

	InvalidateRect(hWnd, NULL, TRUE);

	if (fCodecInstalled)
		ICRemove(ICTYPE_VIDEO, 'BUDV', 0);

	g_capLog.Dispose();

	VDCHECKPOINT;
}

///////////////////////////////////////////////////////////////////////////
//
//	'Allocate disk space' dialog
//
///////////////////////////////////////////////////////////////////////////

static BOOL APIENTRY CaptureAllocateDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	HWND hWndCapture = (HWND)GetWindowLong(hDlg, DWL_USER);

	switch(message) {

		case WM_INITDIALOG:
			{
				char *pb = (char *)allocmem(MAX_PATH*2);

				if (!pb) return FALSE;

				SetWindowLong(hDlg, DWL_USER, (DWORD)lParam);
				hWndCapture = (HWND)lParam;

				if (capFileGetCaptureFile(hWndCapture, pb, MAX_PATH)) {
					__int64 client_free = MyGetDiskFreeSpace(pb);

					if (!SplitPathRoot(pb, pb))
						strcpy(pb+MAX_PATH, "Free disk space:");
					else
						wsprintf(pb+MAX_PATH, "Free disk space on %s:", pb);

					SetDlgItemText(hDlg, IDC_STATIC_DISK_FREE_SPACE, pb+MAX_PATH);

					if (client_free>=0) {
						wsprintf(pb, "%ld Mb ", client_free>>20);
						SendMessage(GetDlgItem(hDlg, IDC_DISK_FREE_SPACE), WM_SETTEXT, 0, (LPARAM)pb);
					}


				}

				freemem(pb);

				SetFocus(GetDlgItem(hDlg, IDC_DISK_SPACE_ALLOCATE));
			}	

			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					LONG lAllocate;
					BOOL fOkay;

					lAllocate = GetDlgItemInt(hDlg, IDC_DISK_SPACE_ALLOCATE, &fOkay, FALSE);

					if (!fOkay || lAllocate<0) {
						MessageBeep(MB_ICONQUESTION);
						SetFocus(GetDlgItem(hDlg, IDC_DISK_SPACE_ALLOCATE));
						return TRUE;
					}

					capFileAlloc(hWndCapture, lAllocate<<20);
				}

				EndDialog(hDlg, TRUE);
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
			break;
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////
//
//	'Settings...' dialog
//
///////////////////////////////////////////////////////////////////////////

static DWORD dwCaptureSettingsHelpLookup[]={
	IDC_CAPTURE_FRAMERATE,			IDH_CAPTURE_SETTINGS_FRAMERATE,
	IDC_CAPTURE_ENABLE_TIMELIMIT,	IDH_CAPTURE_SETTINGS_LIMITLENGTH,
	IDC_CAPTURE_TIMELIMIT,			IDH_CAPTURE_SETTINGS_LIMITLENGTH,
	IDC_CAPTURE_DROP_LIMIT,			IDH_CAPTURE_SETTINGS_DROPLIMIT,
	IDC_CAPTURE_MAX_INDEX,			IDH_CAPTURE_SETTINGS_MAXINDEX,
	IDC_CAPTURE_VIDEO_BUFFERS,		IDH_CAPTURE_SETTINGS_VIDBUFLIMIT,
	IDC_CAPTURE_AUDIO_BUFFERS,		IDH_CAPTURE_SETTINGS_AUDIOBUFFERS,
	IDC_CAPTURE_AUDIO_BUFFERSIZE,	IDH_CAPTURE_SETTINGS_AUDIOBUFFERS,
	IDC_CAPTURE_LOCK_TO_AUDIO,		IDH_CAPTURE_SETTINGS_LOCKDURATION,
	0
};

static BOOL APIENTRY CaptureSettingsDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	HWND hWndCapture = (HWND)GetWindowLong(hDlg, DWL_USER);

	switch(message) {

		case WM_INITDIALOG:
			{
				CAPTUREPARMS cp;
				LONG lV;
				char buf[32];

				SetWindowLong(hDlg, DWL_USER, (DWORD)lParam);
				hWndCapture = (HWND)lParam;

				if (!capCaptureGetSetup(hWndCapture, &cp, sizeof(CAPTUREPARMS)))
					return FALSE;

				lV = (10000000000+cp.dwRequestMicroSecPerFrame/2) / cp.dwRequestMicroSecPerFrame;
				wsprintf(buf, "%ld.%04d", lV/10000, lV%10000);
				SendMessage(GetDlgItem(hDlg, IDC_CAPTURE_FRAMERATE), WM_SETTEXT, 0, (LPARAM)buf);

				SetDlgItemInt(hDlg, IDC_CAPTURE_DROP_LIMIT, cp.wPercentDropForError, FALSE);
				SetDlgItemInt(hDlg, IDC_CAPTURE_MAX_INDEX, cp.dwIndexSize, FALSE);
				SetDlgItemInt(hDlg, IDC_CAPTURE_VIDEO_BUFFERS, cp.wNumVideoRequested, FALSE);
				SetDlgItemInt(hDlg, IDC_CAPTURE_AUDIO_BUFFERS, cp.wNumAudioRequested, FALSE);
				SetDlgItemInt(hDlg, IDC_CAPTURE_AUDIO_BUFFERSIZE, cp.dwAudioBufferSize, FALSE);
				CheckDlgButton(hDlg, IDC_CAPTURE_AUDIO, cp.fCaptureAudio ? 1 : 0);
				CheckDlgButton(hDlg, IDC_CAPTURE_ON_OK, cp.fMakeUserHitOKToCapture ? 1 : 0);
				CheckDlgButton(hDlg, IDC_CAPTURE_ABORT_ON_LEFT, cp.fAbortLeftMouse ? 1 : 0);
				CheckDlgButton(hDlg, IDC_CAPTURE_ABORT_ON_RIGHT, cp.fAbortRightMouse ? 1 : 0);
				CheckDlgButton(hDlg, IDC_CAPTURE_LOCK_TO_AUDIO, cp.AVStreamMaster == AVSTREAMMASTER_AUDIO ? 1 : 0);

				switch(cp.vKeyAbort) {
				case VK_ESCAPE:
					CheckDlgButton(hDlg, IDC_CAPTURE_ABORT_ESCAPE, TRUE);
					break;
				case VK_SPACE:
					CheckDlgButton(hDlg, IDC_CAPTURE_ABORT_SPACE, TRUE);
					break;
				default:
					CheckDlgButton(hDlg, IDC_CAPTURE_ABORT_NONE, TRUE);
					break;
				};

			}	

			return TRUE;

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					HelpPopupByID(hDlg, lphi->iCtrlId, dwCaptureSettingsHelpLookup);
			}
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {

			case IDC_ROUND_FRAMERATE:
				{
					double dFrameRate;
					char buf[32];

					SendMessage(GetDlgItem(hDlg, IDC_CAPTURE_FRAMERATE), WM_GETTEXT, sizeof buf, (LPARAM)buf);
					if (1!=sscanf(buf, " %lg ", &dFrameRate) || dFrameRate<=0.01 || dFrameRate>1000.0) {
						MessageBeep(MB_ICONQUESTION);
						SetFocus(GetDlgItem(hDlg, IDC_CAPTURE_FRAMERATE));
						return TRUE;
					}

					// Man, the LifeView driver really sucks...

					sprintf(buf, "%.4lf", floor(10000000.0 / floor(1000.0 / dFrameRate + .5))/10000.0);
					SetDlgItemText(hDlg, IDC_CAPTURE_FRAMERATE, buf);
				}
				return TRUE;

			case IDOK:
				do {
					CAPTUREPARMS cp;
					LONG lV;
					BOOL fOkay;
					double dFrameRate;
					char buf[32];

					if (!capCaptureGetSetup(hWndCapture, &cp, sizeof(CAPTUREPARMS)))
						break;

					SendMessage(GetDlgItem(hDlg, IDC_CAPTURE_FRAMERATE), WM_GETTEXT, sizeof buf, (LPARAM)buf);
					if (1!=sscanf(buf, " %lg ", &dFrameRate) || dFrameRate<=0.01 || dFrameRate>1000.0) {
						MessageBeep(MB_ICONQUESTION);
						SetFocus(GetDlgItem(hDlg, IDC_CAPTURE_FRAMERATE));
						return TRUE;
					}
					cp.dwRequestMicroSecPerFrame = (DWORD)(1000000.0 / dFrameRate);

					lV = GetDlgItemInt(hDlg, IDC_CAPTURE_DROP_LIMIT, &fOkay, FALSE);
					if (!fOkay || lV<0 || lV>100) {
						MessageBeep(MB_ICONQUESTION);
						SetFocus(GetDlgItem(hDlg, IDC_CAPTURE_DROP_LIMIT));
						return TRUE;
					}
					cp.wPercentDropForError = lV;

					lV = GetDlgItemInt(hDlg, IDC_CAPTURE_MAX_INDEX, &fOkay, FALSE);
					if (!fOkay || lV<0) {
						MessageBeep(MB_ICONQUESTION);
						SetFocus(GetDlgItem(hDlg, IDC_CAPTURE_MAX_INDEX));
						return TRUE;
					}
					cp.dwIndexSize = lV;

					lV = GetDlgItemInt(hDlg, IDC_CAPTURE_VIDEO_BUFFERS, &fOkay, FALSE);
					if (!fOkay || lV<0) {
						MessageBeep(MB_ICONQUESTION);
						SetFocus(GetDlgItem(hDlg, IDC_CAPTURE_VIDEO_BUFFERS));
						return TRUE;
					}
					cp.wNumVideoRequested = lV;

					lV = GetDlgItemInt(hDlg, IDC_CAPTURE_AUDIO_BUFFERS, &fOkay, FALSE);
					if (!fOkay || lV<0) {
						MessageBeep(MB_ICONQUESTION);
						SetFocus(GetDlgItem(hDlg, IDC_CAPTURE_AUDIO_BUFFERS));
						return TRUE;
					}
					cp.wNumAudioRequested = lV;

					lV = GetDlgItemInt(hDlg, IDC_CAPTURE_AUDIO_BUFFERSIZE, &fOkay, FALSE);
					if (!fOkay || lV<0) {
						MessageBeep(MB_ICONQUESTION);
						SetFocus(GetDlgItem(hDlg, IDC_CAPTURE_AUDIO_BUFFERSIZE));
						return TRUE;
					}
					cp.dwAudioBufferSize = lV;

					cp.fCaptureAudio				= IsDlgButtonChecked(hDlg, IDC_CAPTURE_AUDIO);
					cp.fMakeUserHitOKToCapture		= IsDlgButtonChecked(hDlg, IDC_CAPTURE_ON_OK);
					cp.fAbortLeftMouse				= IsDlgButtonChecked(hDlg, IDC_CAPTURE_ABORT_ON_LEFT);
					cp.fAbortRightMouse				= IsDlgButtonChecked(hDlg, IDC_CAPTURE_ABORT_ON_RIGHT);
					cp.AVStreamMaster				= IsDlgButtonChecked(hDlg, IDC_CAPTURE_LOCK_TO_AUDIO) ? AVSTREAMMASTER_AUDIO : AVSTREAMMASTER_NONE;

					if (IsDlgButtonChecked(hDlg, IDC_CAPTURE_ABORT_NONE))
						cp.vKeyAbort = 0;
					else if (IsDlgButtonChecked(hDlg, IDC_CAPTURE_ABORT_ESCAPE))
						cp.vKeyAbort = VK_ESCAPE;
					else if (IsDlgButtonChecked(hDlg, IDC_CAPTURE_ABORT_SPACE))
						cp.vKeyAbort = VK_SPACE;

					cp.fMCIControl = false;

					capCaptureSetSetup(hWndCapture, &cp, sizeof(CAPTUREPARMS));
				} while(0);

				EndDialog(hDlg, TRUE);
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
			break;
	}

	return FALSE;
}


///////////////////////////////////////////////////////////////////////////
//
//	Capture control hook
//
///////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK CaptureControlCallbackProc(HWND hwnd, int nState) {
	if (nState == CONTROLCALLBACK_CAPTURING) {
		CaptureData *cd = (CaptureData *)capGetUserData(hwnd);

		if (g_stopPrefs.fEnableFlags & CAPSTOP_TIME)
			if (cd->lCurrentMS >= g_stopPrefs.lTimeLimit*1000)
				return FALSE;

		if (g_stopPrefs.fEnableFlags & CAPSTOP_FILESIZE)
			if ((long)((cd->total_video_size + cd->total_audio_size + 2048)>>20) > g_stopPrefs.lSizeLimit)
				return FALSE;

		if (g_stopPrefs.fEnableFlags & CAPSTOP_DISKSPACE)
			if (cd->disk_free && (long)(cd->disk_free>>20) < g_stopPrefs.lDiskSpaceThreshold)
				return FALSE;

		if (g_stopPrefs.fEnableFlags & CAPSTOP_DROPRATE)
			if (cd->total_cap > 50 && cd->dropped*100 > g_stopPrefs.lMaxDropRate*cd->total_cap)
				return FALSE;
	} else if (nState == CONTROLCALLBACK_PREROLL) {
		CaptureBT848Reassert();
	}

	return TRUE;
}








///////////////////////////////////////////////////////////////////////////
//
//	Common capture routines
//
///////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK CaptureYieldCallback(HWND hwnd) {
	MSG msg;

	if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
		if (!guiCheckDialogs(&msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return TRUE;
}

static void CaptureUpdateAudioTiming(CaptureData *icd, HWND hwnd, DWORD dwTime) {
	if (!icd->total_cap)
		return;

//	CAPSTATUS cs;

//	capGetStatus(hwnd, &cs, sizeof cs);

//	DWORD dwTime = icd->lVideoLastMS;

//	DWORD dwTime = cs.dwCurrentTimeElapsedMS;

	// Update statistics variables for audio stream, so we can correllate
	// audio samples to time.  This includes sums for X, Y, X^2, Y^2, and
	// XY.

	++icd->iAudioHzSamples;
	icd->i64AudioHzX	+= dwTime;
	icd->i64AudioHzX2	+= (__int64)dwTime * dwTime;
	icd->i64AudioHzY	+= icd->total_audio_data_size;
	icd->i64AudioHzY2	+= (int128)icd->total_audio_data_size*(int128)icd->total_audio_data_size;
	icd->i64AudioHzXY	+= (int128)icd->total_audio_data_size * (int128)dwTime;
}

static struct CapClipFormats {
	FOURCC fcc;
	int nBits;
	int nHorizAlign;
	int nBytesPerGroup;
	bool fInverted;
} g_capClipFormats[]={
	{ BI_RGB,	8,	4, 4, true },
	{ BI_RGB,	16,	2, 4, true },
	{ BI_RGB,	24,	4, 12, true },
	{ BI_RGB,	32,	1, 4, true },
	{ '2YUY',	16,	2, 4, false },
	{ 'YUYV',	16, 2, 4, false },			// VYUY: ATi All-in-Blunder clone of YUY2
	{ 'YVYU',	16, 2, 4, false },
	{ 'UYVY',	16, 2, 4, false },
	{ 'P14Y',	12, 8, 12, false }, 
};

#define NUM_CLIP_FORMATS (sizeof g_capClipFormats / sizeof g_capClipFormats[0])

static BITMAPINFOHEADER *CaptureInitFiltering(CaptureData *icd, BITMAPINFOHEADER *bihInput, DWORD dwRequestMicroSecPerFrame, bool fPermitSizeAlteration) {
	BITMAPINFOHEADER *bihOut = &icd->bihClipFormat;
	bool fFormatAltered = false;

	icd->bihInputFormat	= *bihInput;
	icd->bihInputFormat.biSize = sizeof(BITMAPINFOHEADER);

	icd->bihClipFormat = icd->bihInputFormat;
	icd->pNoiseReductionBuffer = NULL;
	icd->pVertRowBuffer = NULL;
	icd->bpr = ((icd->bihInputFormat.biWidth * icd->bihInputFormat.biBitCount + 31)>>5) * 4;
	icd->rowdwords = icd->bpr/4;
	icd->pdClipOffset = 0;

	icd->fClipping = false;

	if (g_fEnableClipping) {
		int i;

		for(i=0; i<NUM_CLIP_FORMATS; i++)
			if (icd->bihInputFormat.biCompression == g_capClipFormats[i].fcc
				&& icd->bihInputFormat.biBitCount == g_capClipFormats[i].nBits)
				break;

		if (i >= NUM_CLIP_FORMATS)
			throw MyError("Frame clipping is only supported for: RGB8, RGB16, RGB24, RGB32, YUY2, YVYU, UYVY, Y41P.");

		int x1, y1, x2, y2;

		x1 = g_rCaptureClip.left;	x1 -= x1 % g_capClipFormats[i].nHorizAlign;
		x2 = g_rCaptureClip.right;	x2 -= x2 % g_capClipFormats[i].nHorizAlign;
		y1 = g_rCaptureClip.top;
		y2 = g_rCaptureClip.bottom;

		icd->bihClipFormat.biHeight = bihInput->biHeight - y1 - y2;
		icd->bihClipFormat.biWidth = bihInput->biWidth - x1 - x2;

		icd->rowdwords = (g_capClipFormats[i].nBytesPerGroup * icd->bihClipFormat.biWidth / g_capClipFormats[i].nHorizAlign)/4;

		icd->bihClipFormat.biSizeImage = icd->rowdwords * 4 * icd->bihClipFormat.biHeight;

		if (g_capClipFormats[i].fInverted)
			icd->pdClipOffset = y2*icd->bpr;
		else
			icd->pdClipOffset = y1*icd->bpr;

		icd->pdClipOffset += (x1 / g_capClipFormats[i].nHorizAlign) * g_capClipFormats[i].nBytesPerGroup;

		icd->fClipping = true;
		fFormatAltered = true;
	}

	if (g_fEnableNoiseReduction) {

		// We can only accept 24-bit and 32-bit RGB, and YUY2.

		do {
			if (bihInput->biCompression == BI_RGB && (bihInput->biBitCount == 24 || bihInput->biBitCount == 32))
				break;

			if (bihInput->biCompression == '2YUY' && bihInput->biBitCount == 16)
				break;

			if (bihInput->biCompression == 'YUYV' && bihInput->biBitCount == 16)
				break;

			throw MyError("Noise reduction is only supported for 24-bit RGB, 32-bit RGB, and 16-bit 4:2:2 YUV (YUY2/VYUY).");

		} while(false);

		// Allocate the NR buffer.

		if (!(icd->pNoiseReductionBuffer = new char [icd->bihClipFormat.biSizeImage]))
			throw MyMemoryError();
	}

	if (g_fEnableLumaSquish) {

		// Right now, only YUY2 is supported.

		if (bihInput->biCompression != '2YUY')
			throw MyError("Luma squishing is only supported for YUY2.");
	}

	if (g_fSwapFields) {

		// We can swap all RGB formats and some YUV ones.

		if (bihInput->biCompression != BI_RGB && bihInput->biCompression != '2YUY' && bihInput->biCompression != 'YVYU' && bihInput->biCompression!='VYUY' && bihInput->biCompression!='YUYV')
			throw MyError("Field swapping is only supported for RGB, YUY2, UYVY, YUYV, VYUY formats.");
	}

	icd->bihFiltered	= icd->bihClipFormat;

	if (g_iVertSquash) {
		if (bihInput->biCompression != BI_RGB && bihInput->biCompression != '2YUY' && bihInput->biCompression != 'YUYV' && bihInput->biCompression != 'YVYU' && bihInput->biCompression!='VYUY')
			throw MyError("2:1 vertical reduction is only supported for RGB, YUY2, VYUY, UYVY, and YUYV formats.");

		// Allocate temporary row buffer in bicubic mode.

		if (g_iVertSquash == VERTSQUASH_BY2CUBIC)
			if (!(icd->pVertRowBuffer = new char[icd->bpr * 3]))
				throw MyMemoryError();

		icd->bihFiltered.biHeight >>= 1;
		icd->bihFiltered.biSizeImage = icd->bpr * icd->bihFiltered.biHeight;
		bihOut = &icd->bihFiltered;
		fFormatAltered = true;
	}

	if (g_fEnableRGBFiltering) {

		if (icd->bihFiltered.biCompression != BI_RGB && (!fPermitSizeAlteration || (icd->bihFiltered.biCompression != '2YUY' && icd->bihFiltered.biCompression != 'YUYV')))
			throw MyError("%sThe capture video format must be RGB, YUY2, or VYUY.", g_szCannotFilter);

		if (fPermitSizeAlteration)
			icd->bihFiltered2.biBitCount		= 24;
		else
			icd->bihFiltered2.biBitCount		= bihOut->biBitCount;

		filters.initLinearChain(&g_listFA, (Pixel *)((char *)bihInput + bihInput->biSize), bihOut->biWidth, bihOut->biHeight, 32, icd->bihFiltered2.biBitCount);
		if (filters.ReadyFilters(&icd->fsi))
			throw MyError("%sUnable to initialize filters.", g_szCannotFilter);

		icd->fsi.lCurrentFrame		= 0;
		icd->fsi.lMicrosecsPerFrame	= dwRequestMicroSecPerFrame;
		icd->fsi.lCurrentSourceFrame	= 0;
		icd->fsi.lMicrosecsPerSrcFrame	= dwRequestMicroSecPerFrame;

		icd->bihFiltered2.biSize			= sizeof(BITMAPINFOHEADER);
		icd->bihFiltered2.biPlanes			= 1;
		icd->bihFiltered2.biCompression		= BI_RGB;
		icd->bihFiltered2.biWidth			= filters.LastBitmap()->w;
		icd->bihFiltered2.biHeight			= filters.LastBitmap()->h;
		icd->bihFiltered2.biSizeImage		= (((icd->bihFiltered2.biWidth*icd->bihFiltered2.biBitCount+31)&-32)>>3)*icd->bihFiltered2.biHeight;
		icd->bihFiltered2.biClrUsed			= 0;
		icd->bihFiltered2.biClrImportant	= 0;
		icd->bihFiltered2.biXPelsPerMeter	= 0;
		icd->bihFiltered2.biYPelsPerMeter	= 0;
		bihOut = &icd->bihFiltered2;
		fFormatAltered = true;
	} else
		icd->bihFiltered2 = icd->bihFiltered;

	if (!fPermitSizeAlteration && (icd->bihFiltered2.biWidth != bihInput->biWidth
		|| icd->bihFiltered2.biHeight != bihInput->biHeight))
		throw MyError("%sThe filtered frame size must match the input in compatibility (AVICap) mode.", g_szCannotFilter);

	return fFormatAltered ? bihOut : bihInput;
}


void __declspec(naked) dodnrMMX(Pixel32 *dst, Pixel32 *src, PixDim w, PixDim h, PixOffset dstmodulo, PixOffset srcmodulo, __int64 thresh1, __int64 thresh2) {
static const __int64 bythree = 0x5555555555555555i64;
static const __int64 round2 = 0x0002000200020002i64;
static const __int64 three = 0x0003000300030003i64;

	__asm {
		push	ebp
		push	edi
		push	esi
		push	ebx

		mov		edi,[esp+4+16]
		mov		esi,[esp+8+16]
		mov		edx,[esp+12+16]
		mov		ecx,[esp+16+16]
		mov		ebx,[esp+20+16]
		mov		eax,[esp+24+16]
		movq	mm6,[esp+36+16]
		movq	mm5,[esp+28+16]

yloop:
		mov		ebp,edx
xloop:
		movd	mm0,[esi]		;previous
		pxor	mm7,mm7

		movd	mm1,[edi]		;current
		punpcklbw	mm0,mm7

		punpcklbw	mm1,mm7
		movq	mm2,mm0

		movq	mm4,mm1
		movq	mm3,mm1

		movq	mm7,mm0
		paddw	mm4,mm4

		pmullw	mm0,three
		psubusb	mm2,mm1

		paddw	mm4,mm7
		psubusb	mm3,mm7

		pmulhw	mm4,bythree
		por		mm2,mm3

		movq	mm3,mm2
		paddw	mm0,mm1

		paddw	mm0,round2
		pcmpgtw	mm2,mm5			;set if diff > thresh1

		pcmpgtw	mm3,mm6			;set if diff > thresh2
		psrlw	mm0,2


		;	mm2		mm3		meaning						mm1		mm0		mm4
		;	FALSE	FALSE	diff <= thresh1				off		on		off
		;	FALSE	TRUE	impossible
		;	TRUE	FALSE	thresh1 < diff <= thresh2	off		off		on
		;	TRUE	TRUE	diff > thresh2				on		off		off

		pand	mm1,mm3			;keep pixels exceeding threshold2
		pand	mm4,mm2			;	average pixels <= threshold2...
		pandn	mm2,mm0			;replace pixels below threshold1
		pandn	mm3,mm4			;	but >= threshold1...
		por		mm1,mm2
		add		esi,4
		por		mm1,mm3
		add		edi,4
		packuswb	mm1,mm1
		dec		ebp

		movd	[esi-4],mm1		;store to both
		movd	[edi-4],mm1
		jne		xloop

		add		esi,eax
		add		edi,ebx
		dec		ecx
		jne		yloop

		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		emms
		ret
	}
}

static void __declspec(naked) swaprows(void *dst1, void *dst2, ptrdiff_t pitch1, ptrdiff_t pitch2, long w, long h) {
	__asm {
		push	ebp
		push	edi
		push	esi
		push	ebx

		mov		ecx,[esp+24+16]
		mov		esi,[esp+4+16]
		mov		edi,[esp+8+16]

		mov		ebp,[esp+20+16]
		shl		ebp,2
		add		esi,ebp
		add		edi,ebp
		neg		ebp
		mov		[esp+20+16],ebp

yloop:
		mov		ebp,[esp+20+16]
xloop:
		mov		eax,[esi+ebp]
		mov		ebx,[edi+ebp]
		mov		[esi+ebp],ebx
		mov		[edi+ebp],eax
		add		ebp,4
		jne		xloop

		add		esi,[esp+12+16]
		add		edi,[esp+16+16]

		dec		ecx
		jne		yloop

		pop		ebx
		pop		esi
		pop		edi
		pop		ebp
		ret
	}
}


// Squish 0...255 range to 16...235.

static void __declspec(naked) lumasquishYUY2_MMX(void *dst, ptrdiff_t pitch, long w2, long h) {
	static const __int64 scaler = 0x40003b0040003b00i64;
	static const __int64 bias   = 0x0000000500000005i64;

	__asm {
		push		ebp
		push		edi
		push		esi
		push		ebx

		mov			ecx,[esp+12+16]
		mov			esi,[esp+4+16]
		mov			ebx,[esp+16+16]
		mov			eax,[esp+8+16]
		mov			edx,ecx
		shl			edx,2
		sub			eax,edx

		movq		mm6,scaler
		movq		mm5,bias

yloop:
		mov			edx,ecx
		test		esi,4
		jz			xloop_aligned_start

		movd		mm0,[esi]
		pxor		mm7,mm7
		punpcklbw	mm0,mm7
		add			esi,4
		psllw		mm0,2
		dec			edx
		paddw		mm0,mm5
		pmulhw		mm0,mm6
		packuswb	mm0,mm0
		movd		[esi-4],mm0
		jz			xloop_done

xloop_aligned_start:
		sub			edx,3
		jbe			xloop_done
xloop_aligned:
		movq		mm0,[esi]
		pxor		mm7,mm7

		movq		mm2,[esi+8]
		movq		mm1,mm0

		punpcklbw	mm0,mm7
		movq		mm3,mm2

		psllw		mm0,2
		add			esi,16

		paddw		mm0,mm5
		punpckhbw	mm1,mm7

		psllw		mm1,2
		pmulhw		mm0,mm6

		paddw		mm1,mm5
		punpcklbw	mm2,mm7

		pmulhw		mm1,mm6
		psllw		mm2,2

		punpckhbw	mm3,mm7
		paddw		mm2,mm5

		psllw		mm3,2
		pmulhw		mm2,mm6

		paddw		mm3,mm5
		packuswb	mm0,mm1

		pmulhw		mm3,mm6
		sub			edx,4

		movq		[esi-16],mm0

		packuswb	mm2,mm3

		movq		[esi-8],mm2
		ja			xloop_aligned

		add			edx,3
		jz			xloop_done

xloop_tail:
		movd		mm0,[esi]
		pxor		mm7,mm7
		punpcklbw	mm0,mm7
		add			esi,4
		psllw		mm0,2
		dec			edx
		paddw		mm0,mm5
		pmulhw		mm0,mm6
		packuswb	mm0,mm0
		movd		[esi-4],mm0
		jne			xloop_tail

xloop_done:
		add			esi,eax

		dec			ebx
		jne			yloop

		pop			ebx
		pop			esi
		pop			edi
		pop			ebp
		emms
		ret
	}
}

extern "C" long resize_table_col_by2linear_MMX(Pixel *out, Pixel **in_table, PixDim w);
extern "C" long resize_table_col_by2cubic_MMX(Pixel *out, Pixel **in_table, PixDim w);


static void *CaptureDoFiltering(CaptureData *icd, VIDEOHDR *lpVHdr, bool fInPlace, DWORD& dwFrameSize) {
	long bpr = icd->bpr;
	long rowdwords = icd->rowdwords;
	void *pSrc = lpVHdr->lpData + icd->pdClipOffset;

	if (g_fEnableNoiseReduction) {
		__int64 thresh1 = 0x0001000100010001i64*((g_iNoiseReduceThreshold>>1)+1);
		__int64 thresh2 = 0x0001000100010001i64*(g_iNoiseReduceThreshold);

		if (!g_iNoiseReduceThreshold)
			thresh1 = thresh2;

		dodnrMMX((Pixel32 *)lpVHdr->lpData,
			(Pixel32 *)icd->pNoiseReductionBuffer,
			icd->rowdwords,
			icd->bihClipFormat.biHeight,
			bpr - rowdwords*4,
			0,
			thresh1,
			thresh2);
	}

	if (g_fEnableLumaSquish)
		lumasquishYUY2_MMX(pSrc, bpr, ((icd->bihClipFormat.biWidth+1) & ~1)/2, icd->bihClipFormat.biHeight);

	if (g_fSwapFields) {
		swaprows(pSrc, (char *)pSrc + bpr, bpr*2, bpr*2, bpr/4, icd->bihClipFormat.biHeight/2);
	}

	switch(g_iVertSquash) {
	case VERTSQUASH_BY2CUBIC:
		{
			char *src[8], *dst;
			int y = icd->bihClipFormat.biHeight/2;
			char *srclimit = (char *)pSrc + bpr * (icd->bihClipFormat.biHeight - 1);

			memcpy(icd->pVertRowBuffer + rowdwords*4*0, (char *)pSrc + bpr*0, rowdwords*4);
			memcpy(icd->pVertRowBuffer + rowdwords*4*1, (char *)pSrc + bpr*1, rowdwords*4);
			memcpy(icd->pVertRowBuffer + rowdwords*4*2, (char *)pSrc + bpr*2, rowdwords*4);

			dst = (char *)pSrc;
			src[0] = icd->pVertRowBuffer;
			src[1] = src[0] + rowdwords*4;
			src[2] = src[1] + rowdwords*8;

			src[3] = dst + 3*bpr;
			src[4] = src[3] + bpr;
			src[5] = src[4] + bpr;
			src[6] = src[5] + bpr;
			src[7] = src[6] + bpr;

			while(y--) {
				resize_table_col_by2cubic_MMX((Pixel *)dst, (Pixel **)src, rowdwords);

				dst += bpr;
				src[0] = src[2];
				src[1] = src[3];
				src[2] = src[4];
				src[3] = src[5];
				src[4] = src[6];
				src[5] = src[7];
				src[6] += bpr*2;
				src[7] += bpr*2;

				if (src[6] >= srclimit)
					src[6] = srclimit;
				if (src[7] >= srclimit)
					src[7] = srclimit;
			}
			__asm emms

			dwFrameSize = MulDiv(dwFrameSize, icd->bihClipFormat.biHeight/2, icd->bihClipFormat.biHeight);
		}
		break;

	case VERTSQUASH_BY2LINEAR:
		{
			char *src[4], *dst;
			int y = icd->bihClipFormat.biHeight/2;
			char *srclimit = (char *)pSrc + bpr * (icd->bihClipFormat.biHeight - 1);

			src[1] = (char *)pSrc;
			src[2] = src[1];
			src[3] = src[2] + bpr;

			dst = src[1];

			while(y--) {
				resize_table_col_by2linear_MMX((Pixel *)dst, (Pixel **)src, rowdwords);

				dst += bpr;
				src[1] = src[3];
				src[2] += bpr*2;
				src[3] += bpr*2;

				if (src[2] >= srclimit)
					src[2] = srclimit;
				if (src[3] >= srclimit)
					src[3] = srclimit;
			}
			__asm emms

			dwFrameSize = MulDiv(dwFrameSize, icd->bihClipFormat.biHeight/2, icd->bihClipFormat.biHeight);
		}
		break;

	}

	if (g_fEnableRGBFiltering) {
		VBitmap vbmSrc(pSrc, &icd->bihFiltered);

		vbmSrc.pitch = bpr;
		vbmSrc.modulo = vbmSrc.Modulo();
		vbmSrc.size = bpr*vbmSrc.h;

		if (icd->bihFiltered.biCompression == '2YUY' || icd->bihFiltered.biCompression == 'YUYV')
			filters.InputBitmap()->BitBltFromYUY2(0, 0, &vbmSrc, 0, 0, -1, -1);
		else
			filters.InputBitmap()->BitBlt(0, 0, &vbmSrc, 0, 0, -1, -1);

		filters.RunFilters();

		icd->fsi.lSourceFrameMS				= icd->fsi.lCurrentSourceFrame * icd->fsi.lMicrosecsPerSrcFrame;
		icd->fsi.lDestFrameMS				= icd->fsi.lCurrentFrame * icd->fsi.lMicrosecsPerFrame;

		if (fInPlace)
			vbmSrc.BitBlt(0, 0, filters.LastBitmap(), 0, 0, -1, -1);
		else {
			filters.OutputBitmap()->BitBlt(0, 0, filters.LastBitmap(), 0, 0, -1, -1);
			dwFrameSize = filters.OutputBitmap()->size;
			pSrc = filters.OutputBitmap()->data;
		}

		++icd->fsi.lCurrentFrame;
		++icd->fsi.lCurrentSourceFrame;
	} else if (icd->fClipping) {
		int y = icd->bihFiltered.biHeight;
		char *src = (char *)pSrc;
		char *dst = (char *)lpVHdr->lpData;

		pSrc = dst;
		dwFrameSize = rowdwords*4*y;

		do {
			memmove(dst, src, rowdwords*4);
			dst += rowdwords*4;
			src += bpr;
		} while(--y);
	}

	return pSrc;
}

static void CaptureDeinitFiltering(CaptureData *icd) {
	filters.DeinitFilters();
	filters.DeallocateBuffers();

	if (icd->pVertRowBuffer) {
		delete icd->pVertRowBuffer;
		icd->pVertRowBuffer = NULL;
	}

	if (icd->pNoiseReductionBuffer) {
		delete icd->pNoiseReductionBuffer;
		icd->pNoiseReductionBuffer = NULL;
	}
}







///////////////////////////////////////////////////////////////////////////
//
//	Stupid capture (AVICap)
//
///////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK CaptureAVICapVideoCallbackProc(HWND hWnd, LPVIDEOHDR lpVHdr)
{
	CaptureData *icd = (CaptureData *)capGetUserData(hWnd);
	CAPSTATUS capStatus;
	char buf[128];
	__int64 jitter;
	DWORD dwFrameSize = lpVHdr->dwBytesUsed;

	capGetStatus(hWnd, (LPARAM)&capStatus, sizeof(CAPSTATUS));

	try {		// FIXME
		CaptureDoFiltering(icd, lpVHdr, true, dwFrameSize);
	} catch(MyError e) {
	}

	if (!icd->total_cap)
		icd->lVideoFirstMS = lpVHdr->dwTimeCaptured;

	icd->lVideoLastMS = lpVHdr->dwTimeCaptured;

	icd->total_video_size += icd->last_video_size;
	icd->last_video_size = 0;

	jitter = (long)(((lpVHdr->dwTimeCaptured - icd->lVideoFirstMS)*1000i64) % icd->interval);

	if (jitter >= icd->interval/2) {
		jitter -= icd->interval;
		icd->total_disp -= jitter;
	} else {
		icd->total_disp += jitter;
	}
	icd->total_jitter += jitter;
	++icd->total_cap;
	++icd->last_cap;

	icd->lCurrentMS = capStatus.dwCurrentTimeElapsedMS;

	icd->last_video_size = lpVHdr->dwBytesUsed + 24;

	icd->dropped = capStatus.dwCurrentVideoFramesDropped;

	if (capStatus.dwCurrentTimeElapsedMS - icd->lastMessage > 500) {
		if (g_fInfoPanel) {
			if (icd->hwndPanel)
				SendMessage(icd->hwndPanel, WM_APP, 0, (LPARAM)(CaptureData *)icd);

			wsprintf(buf, "%ldms jitter, %ldms disp, %ldK total"
						,icd->last_cap ? (long)(icd->total_jitter/(icd->last_cap*1000)) : 0
						,icd->last_cap ? (long)(icd->total_disp/(icd->last_cap*1000)) : 0
						,(long)((icd->total_video_size + icd->total_audio_size + 1023)/1024)
						);
		} else {
			__int64 i64;

			if (g_fEnableSpill)
				i64 = CapSpillGetFreeSpace();
			else
				i64 = MyGetDiskFreeSpace(icd->szCaptureRoot[0] ? icd->szCaptureRoot : NULL);

			if (i64>=0) {
				if (i64)
					icd->disk_free = i64;
				else
					icd->disk_free = -1;
			}

			wsprintf(buf, "%ld frames (%ld dropped), %d.%03ds, %ldms jitter, %ldms disp, %ld frame size, %ldK total"
						,capStatus.dwCurrentVideoFrame
						,icd->dropped
						,capStatus.dwCurrentTimeElapsedMS/1000
						,capStatus.dwCurrentTimeElapsedMS%1000
						,icd->last_cap ? (long)(icd->total_jitter/(icd->last_cap*1000)) : 0
						,icd->last_cap ? (long)(icd->total_disp/(icd->last_cap*1000)) : 0
						,(long)(icd->total_video_size/icd->total_cap)
						,(long)((icd->total_video_size + icd->total_audio_size + 1023)/1024));
		}

		SendMessage(icd->hwndStatus, SB_SETTEXT, 0, (LPARAM)buf);
		RedrawWindow(icd->hwndStatus, NULL, NULL, RDW_INVALIDATE|RDW_UPDATENOW);

		icd->lastMessage = capStatus.dwCurrentTimeElapsedMS - capStatus.dwCurrentTimeElapsedMS%500;
		icd->last_cap	= 0;
		icd->total_jitter = icd->total_disp = 0;
	};

	if (g_bCaptureDDrawActive)
		CaptureOverlayFrameCallback(hWnd, lpVHdr);

	return 0;
}

/* this is called in Internal capture mode to handle frame timing */

static LRESULT CALLBACK CaptureAVICapWaveCallbackProc(HWND hWnd, LPWAVEHDR lpWHdr)
{
	CaptureData *icd = (CaptureData *)capGetUserData(hWnd);
	CAPSTATUS capStatus;

	capGetStatus(hWnd, (LPARAM)&capStatus, sizeof(CAPSTATUS));

	icd->lAudioLastMS = capStatus.dwCurrentTimeElapsedMS;

	if (!icd->audio_first_size) {
		icd->audio_first_size = lpWHdr->dwBytesRecorded;
		icd->lAudioFirstMS = capStatus.dwCurrentTimeElapsedMS;
	}

	++icd->total_audio_cap;

	icd->total_audio_data_size += lpWHdr->dwBytesRecorded;
	icd->total_audio_size += lpWHdr->dwBytesRecorded + 24;

	CaptureUpdateAudioTiming(icd, hWnd, capStatus.dwCurrentTimeElapsedMS);

    return 0;
}

static void CaptureAVICap(HWND hWnd, HWND hWndCapture) {
	char fname[MAX_PATH];
	LRESULT lRes;
	BITMAPINFO *bmi = NULL, *bmiTemp = NULL;
	WAVEFORMAT *wf = NULL, *wfTemp = NULL;
	LPARAM biSize, wfSize;
	CAPTUREPARMS cp;
	CaptureData cd;
	BOOL fCompressionOk = FALSE;

//	memset(&cd, 0, sizeof cd);

	g_capLog.Dispose();

	try {
		// get the input filename

		if (!capFileGetCaptureFile(hWndCapture, fname, sizeof fname))
			throw MyError("Couldn't get capture filename.");

		// get capture parms

		if (!capCaptureGetSetup(hWndCapture, &cp, sizeof(CAPTUREPARMS)))
			throw MyError("Couldn't get capture setup info.");

		// copy over time limit information

		if (g_stopPrefs.fEnableFlags & CAPSTOP_TIME) {
			cp.fLimitEnabled	= true;
			cp.wTimeLimit		= g_stopPrefs.lTimeLimit;
		} else
			cp.fLimitEnabled	= false;

		if (!capCaptureSetSetup(hWndCapture, &cp, sizeof(CAPTUREPARMS)))
			throw MyError("Couldn't set capture setup info.");

		// get audio format

		wfSize = capGetAudioFormatSize(hWndCapture);

		if (!(wfTemp = wf = (WAVEFORMAT *)allocmem(wfSize))) throw MyMemoryError();

		if (!capGetAudioFormat(hWndCapture, wf, wfSize))
			throw MyError("Couldn't get audio format");

		// initialize video compression

		biSize = capGetVideoFormatSize(hWndCapture);

		if (!(bmi = bmiTemp = (BITMAPINFO *)allocmem(biSize)))
			throw MyMemoryError();

		if (!capGetVideoFormat(hWndCapture, bmiTemp, biSize))
			throw MyError("Couldn't get video format");

		// Setup capture structure

		memcpy(&cd.wfex, wf, min(wfSize, sizeof cd.wfex));

		cd.hwndStatus	= GetDlgItem(hWnd, IDC_STATUS_WINDOW);
		cd.hwndPanel	= GetDlgItem(hWnd, IDC_CAPTURE_PANEL);
		cd.interval		= cp.dwRequestMicroSecPerFrame;

		if (!bmi->bmiHeader.biBitCount)
			cd.uncompressed_frame_size		= ((bmi->bmiHeader.biWidth * 2 + 3) & -3) * bmi->bmiHeader.biHeight;
		else
			cd.uncompressed_frame_size		= ((bmi->bmiHeader.biWidth * ((bmi->bmiHeader.biBitCount + 7)/8) + 3) & -3) * bmi->bmiHeader.biHeight;

		CaptureInitFiltering(&cd, &bmi->bmiHeader, cp.dwRequestMicroSecPerFrame, false);

		if (!SplitPathRoot(cd.szCaptureRoot, fname)) {
			cd.szCaptureRoot[0] = 0;
			MyGetDiskFreeSpace(NULL);
		} else
			MyGetDiskFreeSpace(cd.szCaptureRoot);

		// capture!!

		capSetUserData(hWndCapture, (LPARAM)&cd);
		capSetCallbackOnVideoStream(hWndCapture, CaptureAVICapVideoCallbackProc);
		if (cp.fCaptureAudio)
			capSetCallbackOnWaveStream(hWndCapture, CaptureAVICapWaveCallbackProc);
		capSetCallbackOnCapControl(hWndCapture, CaptureControlCallbackProc);

		CaptureShowFile(hWnd, hWndCapture, true);
		g_fRestricted = true;
		lRes = capCaptureSequence(hWndCapture);
		g_fRestricted = false;
		CaptureShowFile(hWnd, hWndCapture, false);

		capSetCallbackOnCapControl(hWndCapture, NULL);
		capSetCallbackOnWaveStream(hWndCapture, NULL);
		capSetCallbackOnVideoStream(hWndCapture, NULL);
	} catch(MyError e) {
		e.post(hWnd, "Capture error");
	}

	CaptureDeinitFiltering(&cd);

	freemem(bmiTemp);
	freemem(wfTemp);

}

///////////////////////////////////////////////////////////////////////////
//
//	Internal capture
//
///////////////////////////////////////////////////////////////////////////

class InternalCapVars {
public:
	VideoSequenceCompressor *pvsc;
	AVIOutput		*aoFile;
	AVIOutput		*aoFilePending;
	FastWriteStream	*fwsActive;
	FastWriteStream	*fwsPending;
	int				blockAlign;
	DWORD			lastFrame;
	MyError *		fatal_error;
	MyError *		fatal_error_2;
	HFONT			hFont;
	const char	*	pszFilename;
	const char	*	pszPath;
	const char	*	pszNewPath;
	__int64			segment_audio_size, segment_video_size;
	__int64			nAudioBlocks;
	__int64			nAudioSwitchPt;
	__int64			nVideoBlocks;
	__int64			nVideoSwitchPt;
	long			lDiskThresh;
	long			lDiskThresh2;
	long			lVideoMSBias;			// Compensates for 71 minute flipping on some drivers
	long			lLastVideoUncorrectedMS;

	AVIOutput		*aoFileAudio, *aoFileVideo;
	HANDLE			hIOThread;
	DWORD			dwThreadID;
	bool			fDoSwitch;
	bool			fAllFull;
	bool			fNTSC;
	bool			fWarnVideoCaptureTiming1;	// 71 minute bug #1 found

	// video clock correction

	long			lFirstVideoPt;

	InternalCapVars() {
		memset(this, 0, sizeof *this);
	}
};

class InternalCapData : public CaptureData, public InternalCapVars {
public:
};

////////////////

extern LONG __stdcall CrashHandler(EXCEPTION_POINTERS *pExc);

#if 0
#define CAPINT_FATAL_CATCH_START	\
		__try {

#define CAPINT_FATAL_CATCH_END(msg)	\
		} __except(CrashHandler((EXCEPTION_POINTERS*)_exception_info()), 1) {		\
		}
#else
#define CAPINT_FATAL_CATCH_START	\
		__try {

#define CAPINT_FATAL_CATCH_END(msg)	\
		} __except(CaptureIsCatchableException(GetExceptionCode())) {		\
			CaptureInternalHandleException(icd, msg, GetExceptionCode());				\
		}

static void CaptureInternalHandleException(InternalCapData *icd, char *op, DWORD ec) {
	if (!icd->fatal_error) {
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

		icd->fatal_error = new MyError("Internal program error during %s handling: %s.", op, s);
	}
}
#endif

static void CaptureInternalSpillNewFile(InternalCapData *const icd) {
	AVIOutputFile *aoNew = NULL;
	BITMAPINFO *bmi;
	char fname[MAX_PATH];
	CapSpillDrive *pcsd;

	pcsd = CapSpillPickDrive(false);
	if (!pcsd) {
		icd->fAllFull = true;
		return;
	}

	icd->pszNewPath = pcsd->path;

	try {
		aoNew = new AVIOutputFile();

		aoNew->setSegmentHintBlock(true, NULL, MAX_PATH+1);

		if (!aoNew)
			throw MyMemoryError();

		if (!(aoNew->initOutputStreams()))
			throw MyMemoryError();

		if (g_prefs.fAVIRestrict1Gb)
				aoNew->set_1Gb_limit();

		aoNew->set_capture_mode(true);

		// copy over information to new file

		memcpy(&aoNew->videoOut->streamInfo, &icd->aoFile->videoOut->streamInfo, sizeof icd->aoFile->videoOut->streamInfo);

		if (!(aoNew->videoOut->allocFormat(icd->aoFile->videoOut->getFormatLen())))
			throw MyMemoryError();

		memcpy(aoNew->videoOut->getFormat(), icd->aoFile->videoOut->getFormat(), icd->aoFile->videoOut->getFormatLen());

		if (icd->aoFile->audioOut) {
			memcpy(&aoNew->audioOut->streamInfo, &icd->aoFile->audioOut->streamInfo, sizeof icd->aoFile->audioOut->streamInfo);

			if (!(aoNew->audioOut->allocFormat(icd->aoFile->audioOut->getFormatLen())))
				throw MyMemoryError();

			memcpy(aoNew->audioOut->getFormat(), icd->aoFile->audioOut->getFormat(), icd->aoFile->audioOut->getFormatLen());
		} 

		// init the new file

		if (!g_capStripeSystem && g_diskDisableBuffer) {
			aoNew->disable_os_caching();
			aoNew->set_chunk_size(1024 * g_diskChunkSize);
		}

		bmi = (BITMAPINFO *)icd->aoFile->videoOut->getFormat();
		aoNew->videoOut->setCompressed(bmi->bmiHeader.biCompression != BI_RGB);

		pcsd->makePath(fname, icd->pszFilename);

		// edit the filename up

		sprintf((char *)SplitPathExt(fname), ".%02d.avi", icd->iSpillNumber+1);

		// init the file

		if (!(icd->fwsPending = aoNew->initCapture(fname, bmi->bmiHeader.biWidth, bmi->bmiHeader.biHeight,
			TRUE, !!icd->aoFile->audioOut, 1024 * g_diskChunkSize * g_diskChunkCount, TRUE)))
			throw MyError("Error initializing spill capture file \"%s\".", fname);

		icd->aoFilePending = aoNew;

		*(char *)SplitPathName(fname) = 0;

		((AVIOutputFile *)icd->aoFile)->setSegmentHintBlock(false, fname, MAX_PATH);

		++icd->iSpillNumber;
		icd->lDiskThresh2 = pcsd->threshold;

	} catch(MyError e) {
		delete aoNew;
		throw;
	}
}

static void CaptureInternalSpillFinalizeOld(InternalCapData *const icd) {
	AVIOutput *ao = icd->aoFile;

	icd->aoFile = icd->aoFilePending;
	icd->fwsActive->setSynchronous(true);
	ao->finalize();
	delete ao;
	icd->fwsActive = icd->fwsPending;
	icd->pszPath = icd->pszNewPath;
	icd->lDiskThresh = icd->lDiskThresh2;
}

#define VDCM_EXIT		(WM_APP+0)
#define VDCM_SWITCH_FIN (WM_APP+1)

static unsigned __stdcall CaptureInternalSpillThread(void *pp) {
	InternalCapData *const icd = (InternalCapData *)pp;
	HANDLE hActive[2];
	MSG msg;
	bool fSwitch = false;
	DWORD dwTimer = GetTickCount(), dwNewTime;
	bool fTimerActive = true;

	InitThreadData("Capture spill");

	for(;;) {
		bool fSuccess = false;

		if (icd->aoFile) {
			try {
				fSuccess = icd->fwsActive->BackgroundCheck();
			} catch(DWORD dw) {
				icd->fwsActive->putError(dw);
			}
			hActive[0] = icd->fwsActive->getSyncHandle();
		}

		if (icd->aoFilePending) {
			try {
				fSuccess |= icd->fwsPending->BackgroundCheck();
			} catch(DWORD dw) {
				icd->fwsActive->putError(dw);
			}
			hActive[1] = icd->fwsPending->getSyncHandle();
		}

		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == VDCM_EXIT)
				return 0;
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

			if (!icd->fatal_error_2) try {

				if (icd->aoFile && !icd->aoFilePending && !icd->fAllFull) {
					CaptureInternalSpillNewFile(icd);
				}

				// Finalize old output?

				if (fSwitch) {
					CaptureInternalSpillFinalizeOld(icd);
					fSwitch = false;

					// Restart timer for new file to open.

					dwTimer = GetTickCount();
					fTimerActive = true;
				}
			} catch(MyError e) {
				icd->fatal_error_2 = new MyError(e);
			}

			MsgWaitForMultipleObjects(icd->aoFilePending?2:1, hActive, FALSE, INFINITE, QS_ALLEVENTS);
		}
	}

	DeinitThreadData();
}

static void CaptureInternalDoSpill(InternalCapData *icd) {
	if (!g_fEnableSpill) return;

	__int64 nAudioFromVideo;
	__int64 nVideoFromAudio;

	if (icd->fAllFull)
		throw MyError("Capture stopped: All assigned spill drives are full.");

	// If there is no audio, then switch now.

	if (icd->aoFileVideo->audioOut) {

		// Find out if the audio or video stream is ahead, and choose a stop point.

		if (icd->fNTSC)
			nAudioFromVideo = int64divround(icd->nVideoBlocks * 1001i64 * icd->wfex.nAvgBytesPerSec, icd->blockAlign * 30000i64);
		else
			nAudioFromVideo = int64divround(icd->nVideoBlocks * (__int64)icd->interval * icd->wfex.nAvgBytesPerSec, icd->blockAlign * 1000000i64);

		if (nAudioFromVideo < icd->nAudioBlocks) {

			// Audio is ahead of the corresponding video point.  Figure out how many frames ahead
			// we need to trigger from now.

			if (icd->fNTSC) {
				nVideoFromAudio = int64divroundup(icd->nAudioBlocks * icd->blockAlign * 30000i64, icd->wfex.nAvgBytesPerSec * 1001i64);
				nAudioFromVideo = int64divround(nVideoFromAudio * 1001i64 * icd->wfex.nAvgBytesPerSec, icd->blockAlign * 30000i64);
			} else {
				nVideoFromAudio = int64divroundup(icd->nAudioBlocks * icd->blockAlign * 1000000i64, icd->wfex.nAvgBytesPerSec * (__int64)icd->interval);
				nAudioFromVideo = int64divround(nVideoFromAudio * (__int64)icd->interval * icd->wfex.nAvgBytesPerSec, icd->blockAlign * 1000000i64);
			}

			icd->nVideoSwitchPt = nVideoFromAudio;
			icd->nAudioSwitchPt = nAudioFromVideo;

			_RPT4(0,"SPILL: (%I64d,%I64d) > trigger at > (%I64d,%I64d)\n", icd->nVideoBlocks, icd->nAudioBlocks, icd->nVideoSwitchPt, icd->nAudioSwitchPt);

			return;

		} else if (nAudioFromVideo > icd->nAudioBlocks) {

			// Audio is behind the corresponding video point, so switch the video stream now
			// and post a trigger for audio.

			icd->nAudioSwitchPt = nAudioFromVideo;

			_RPT3(0,"SPILL: video frozen at %I64d, audio(%I64d) trigger at (%I64d)\n", icd->nVideoBlocks, icd->nAudioBlocks, icd->nAudioSwitchPt);

			icd->segment_video_size = 0;
			icd->aoFileVideo = icd->aoFilePending;

			return;

		}
	}

	// Hey, they're exactly synched!  Well then, let's switch them now!

	_RPT2(0,"SPILL: exact sync switch at %I64d, %I64d\n", icd->nVideoBlocks, icd->nAudioBlocks);

	icd->aoFileAudio = icd->aoFilePending;
	icd->aoFileVideo = icd->aoFilePending;
	icd->segment_audio_size = icd->segment_video_size = 0;

	PostThreadMessage(icd->dwThreadID, VDCM_SWITCH_FIN, 0, 0);
}

static void CaptureInternalCheckVideoAfter(InternalCapData *icd) {
	++icd->nVideoBlocks;
	
	if (icd->nVideoSwitchPt && icd->nVideoBlocks == icd->nVideoSwitchPt) {

		icd->aoFileVideo = icd->aoFilePending;

		if (!icd->nAudioSwitchPt) {
			PostThreadMessage(icd->dwThreadID, VDCM_SWITCH_FIN, 0, 0);

			_RPT0(0,"VIDEO: Triggering finalize & switch.\n");
		} else
			_RPT2(0,"VIDEO: Switching stripes, waiting for audio to reach sync point (%I64d < %I64d)\n", icd->nAudioBlocks, icd->nAudioSwitchPt);

		icd->nVideoSwitchPt = 0;
		icd->segment_video_size = 0;
	}
}

static long g_dropforward=0, g_dropback=0;

#if 0
class xyzinitobject {
public:
	xyzinitobject() {
		_CrtSetReportFile(0, CreateFile("f:\\log.txt", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL));
		_CrtSetReportMode(0, _CRTDBG_MODE_FILE); 
	}
} g_xyzinitobject;
#endif

static LRESULT CaptureInternalVideoCallbackProc2(InternalCapData *icd, HWND hWnd, LPVIDEOHDR lpVHdr)
{
	LRESULT hr;
	CAPSTATUS capStatus;
	char buf[256];
	DWORD dwTime;
	DWORD dwCurrentFrame;
	long	lTimeStamp;
	__int64 jitter;

	// Has the I/O thread successfully completed the switch?

	if (icd->aoFile == icd->aoFilePending) {
		icd->aoFile = icd->aoFilePending;
		icd->aoFilePending = NULL;
	}

	// Get timestamp

	capGetStatus(hWnd, (LPARAM)&capStatus, sizeof(CAPSTATUS));

	// Log event

	if (g_fLogEvents)
		g_capLog.LogVideo(capStatus.dwCurrentTimeElapsedMS, lpVHdr->dwBytesUsed, lpVHdr->dwTimeCaptured);

	// Correct for one form of the 71-minute bug.
	//
	// The video capture driver apparently computes a time in microseconds and then divides by
	// 1000 to convert to milliseconds, but doesn't compensate for when the microsecond counter
	// overflows past 2^32.  This results in a wraparound from 4294967ms (1h 11m 34s) to 0ms.
	// We must detect this and add 4294967ms to the count.  This will be off by 1ms every three
	// times this occurs, but 1ms of error every 3.5 hours is not that big of a deal.

	lTimeStamp = lpVHdr->dwTimeCaptured;

	if (lTimeStamp < icd->lLastVideoUncorrectedMS && lTimeStamp < 10000 && icd->lLastVideoUncorrectedMS >= 4285000) {

		// Perform sanity checks.  We should be within ten seconds of the last frame.

		long lNewTimeStamp = lTimeStamp + 4294967;

		if (lNewTimeStamp < icd->lLastVideoUncorrectedMS + 5000 && lNewTimeStamp >= icd->lLastVideoUncorrectedMS - 5000) {
			icd->lVideoMSBias += 4294967;

			icd->fWarnVideoCaptureTiming1 = true;
		}

	}

	icd->lLastVideoUncorrectedMS = lTimeStamp;

	lTimeStamp += icd->lVideoMSBias;

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

//_RPT1(0,"%d\n", lpVHdr->dwTimeCaptured);

	if (g_fAdjustVideoTimer) {
		if (icd->lVideoAdjust < 0 && (DWORD)-icd->lVideoAdjust > lTimeStamp)
			lTimeStamp = 0;
		else
			lTimeStamp += icd->lVideoAdjust;
	}

	if (!icd->total_cap)
		icd->lVideoFirstMS = lTimeStamp;

//	dwTime = lpVHdr->dwTimeCaptured - icd->lVideoFirstMS;
//	dwTime = lTimeStamp - icd->lVideoFirstMS;
	dwTime = lTimeStamp;

	icd->lVideoLastMS = lTimeStamp;

	icd->total_video_size += icd->last_video_size;
	icd->last_video_size = 0;

	////////////////////////////

	if (icd->fNTSC)
		dwCurrentFrame = ((__int64)dwTime * 30 + 500) / 1001;
	else
		dwCurrentFrame = ((__int64)dwTime * 1000 + icd->interval/2) / icd->interval;

	if (dwCurrentFrame) --dwCurrentFrame;

//	_RPT2(0,"lastFrame=%d, dwCurrentFrame=%d\n", icd->lastFrame, dwCurrentFrame);

//	jitter = ((__int64)lTimeStasmp*1000 - (__int64)icd->lastFrame*icd->interval);

//	jitter = (long)(((lTimeStamp - icd->lVideoFirstMS)*1000i64) % icd->interval);
	jitter = (long)((dwTime*1000i64) % icd->interval);

//	_RPT1(0,"jitter: %ld\n", jitter);

	if (jitter >= icd->interval/2) {
		jitter -= icd->interval;
		icd->total_disp -= jitter;
	} else {
		icd->total_disp += jitter;
	}
	icd->total_jitter += jitter;
	++icd->total_cap;
	++icd->last_cap;

	icd->lCurrentMS = capStatus.dwCurrentTimeElapsedMS;

	// Is the frame too early?

	if (icd->lastFrame > dwCurrentFrame+1) {
		++icd->dropped;
++g_dropforward;
_RPT2(0,"Drop forward at %ld ms (%ld ms corrected)\n", lpVHdr->dwTimeCaptured, lTimeStamp);
		return 0;
	}

	// Run the frame through the filterer.

	DWORD dwBytesUsed = lpVHdr->dwBytesUsed;
	void *pFilteredData = CaptureDoFiltering(icd, lpVHdr, false, dwBytesUsed);

	if (icd->aoFile) {
		try {
			// While we are early, write dropped frames (grr)
			//
			// Don't do this for the first frame, since we don't
			// have any frames preceding it!

			if (icd->total_cap > 1)
				while(icd->lastFrame < dwCurrentFrame) {
					hr = icd->aoFileVideo->videoOut->write(0, lpVHdr->lpData, 0, 1);
					++icd->lastFrame;
					++icd->dropped;
_RPT2(0,"Drop back at %ld ms (%ld ms corrected)\n", lpVHdr->dwTimeCaptured, lTimeStamp);
++g_dropback;
					icd->total_video_size += 24;
					icd->segment_video_size += 24;

					if (icd->pvsc)
						icd->pvsc->dropFrame();

					CaptureInternalCheckVideoAfter(icd);
				}

			if (icd->pvsc) {
				bool isKey;
				long lBytes = 0;
				void *lpCompressedData;

				lpCompressedData = icd->pvsc->packFrame(pFilteredData, &isKey, &lBytes);

				hr = icd->aoFileVideo->videoOut->write(
						isKey ? AVIIF_KEYFRAME : 0,
						lpCompressedData,
						lBytes, 1);

				CaptureInternalCheckVideoAfter(icd);

				icd->last_video_size = lBytes + 24;
			} else {
				hr = icd->aoFileVideo->videoOut->write(lpVHdr->dwFlags & VHDR_KEYFRAME ? AVIIF_KEYFRAME : 0, pFilteredData, dwBytesUsed, 1);
				CaptureInternalCheckVideoAfter(icd);

				icd->last_video_size = dwBytesUsed + 24;
			}
		} catch(MyError e) {
			if (!icd->fatal_error)
				icd->fatal_error = new MyError(e);

			capCaptureAbort(hWnd);

			return FALSE;
		}
	} else {
		// testing

		while(icd->lastFrame < dwCurrentFrame) {
			++icd->lastFrame;
			++icd->dropped;
++g_dropback;
			icd->total_video_size += 24;
			icd->segment_video_size += 24;
		}

		if (icd->pvsc) {
			bool isKey;
			long lBytes = 0;
			void *lpCompressedData;

			lpCompressedData = icd->pvsc->packFrame(pFilteredData, &isKey, &lBytes);

			icd->last_video_size = lBytes + 24;
		} else {
			icd->last_video_size = dwBytesUsed + 24;
		}
	}

	++icd->lastFrame;
	icd->segment_video_size += icd->last_video_size;

	if (capStatus.dwCurrentTimeElapsedMS - icd->lastMessage > 500)
	{

		if (icd->aoFilePending && !icd->nAudioSwitchPt && !icd->nVideoSwitchPt && g_fEnableSpill) {
			if (icd->segment_video_size + icd->segment_audio_size >= ((__int64)g_lSpillMaxSize<<20)
				|| MyGetDiskFreeSpace(icd->pszPath) < ((__int64)icd->lDiskThresh << 20))

				CaptureInternalDoSpill(icd);
		}

		if (g_fInfoPanel) {
			if (icd->hwndPanel)
				SendMessage(icd->hwndPanel, WM_APP, 0, (LPARAM)(CaptureData *)icd);

			sprintf(buf, "%ldus jitter, %ldus disp, %ldK total, spill seg #%d, %d/%d"
						,icd->last_cap ? (long)(icd->total_jitter/(icd->last_cap*1)) : 0
						,icd->last_cap ? (long)(icd->total_disp/(icd->last_cap*1)) : 0
						,(long)((icd->total_video_size + icd->total_audio_size + 1023)/1024)
						,icd->iSpillNumber+1
						,g_dropback, g_dropforward
						);
		} else {
			__int64 i64;

			if (g_fEnableSpill)
				i64 = CapSpillGetFreeSpace();
			else
				i64 = MyGetDiskFreeSpace(icd->szCaptureRoot[0] ? icd->szCaptureRoot : NULL);

			if (i64>=0) {
				if (i64)
					icd->disk_free = i64;
				else
					icd->disk_free = -1;
			}

			wsprintf(buf, "%ld frames (%ld dropped), %d.%03ds, %ldms jitter, %ldms disp, %ld frame size, %ldK total"
						,icd->total_cap
						,icd->dropped
						,capStatus.dwCurrentTimeElapsedMS/1000
						,capStatus.dwCurrentTimeElapsedMS%1000
						,icd->last_cap ? (long)(icd->total_jitter/(icd->last_cap*1000)) : 0
						,icd->last_cap ? (long)(icd->total_disp/(icd->last_cap*1000)) : 0
						,(long)(icd->total_video_size/icd->total_cap)
						,(long)((icd->total_video_size + icd->total_audio_size + 1023)/1024));
		}

		SendMessage(icd->hwndStatus, SB_SETTEXT, 0, (LPARAM)buf);
		RedrawWindow(icd->hwndStatus, NULL, NULL, RDW_INVALIDATE|RDW_UPDATENOW);

		if (icd->hFont) {
			HWND hwndParent = GetParent(hWnd);
			RECT r;
			HDC hdc;

			GetWindowRect(icd->hwndStatus, &r);
			ScreenToClient(hwndParent, (LPPOINT)&r);

			if (hdc = GetDC(hwndParent)) {
				HGDIOBJ hgoOld;
				long tm = capStatus.dwCurrentTimeElapsedMS/1000;

				hgoOld = SelectObject(hdc, icd->hFont);
				SetTextAlign(hdc, TA_BASELINE | TA_LEFT);
				SetBkColor(hdc, GetSysColor(COLOR_3DFACE));
				wsprintf(buf, "%d:%02d", tm/60, tm%60);
				TextOut(hdc, 50, r.top - 50, buf, strlen(buf));
				SelectObject(hdc, hgoOld);
			}
		}

		icd->lastMessage = capStatus.dwCurrentTimeElapsedMS - capStatus.dwCurrentTimeElapsedMS%500;
		icd->last_cap	= 0;
		icd->total_jitter = icd->total_disp = 0;
	};

	return TRUE;
}

static int inline square(int x) { return x*x; }

static LRESULT CaptureInternalWaveCallbackProc2(InternalCapData *icd, HWND hWnd, LPWAVEHDR lpWHdr)
{
	LRESULT hr = 0;
	CAPSTATUS capStatus;
	DWORD dwTime, dwOTime;

	// Get current timestamp

	capGetStatus(hWnd, (LPARAM)&capStatus, sizeof(CAPSTATUS));

	dwOTime = dwTime = capStatus.dwCurrentTimeElapsedMS;

	if (!icd->total_audio_cap)
		icd->lFirstVideoPt = icd->lastFrame;

#if 0
	char buf[256];
		wsprintf(buf,"time: %ld ms  freq: %ld hz\n", capStatus.dwCurrentTimeElapsedMS, MulDiv(icd->total_audio_data_size - icd->audio_first_size + lpWHdr->dwBytesRecorded, 2997, (icd->lastFrame - icd->lFirstVideoPt)*400));
		SendMessage(icd->hwndStatus, SB_SETTEXT, 0, (LPARAM)buf);
		RedrawWindow(icd->hwndStatus, NULL, NULL, RDW_INVALIDATE|RDW_UPDATENOW);
#endif

	// Adjust video timing

	dwTime += icd->lVideoAdjust;

	if (g_fAdjustVideoTimer && icd->total_audio_cap) {
		long lDesiredVideoFrame;
		long lDelta;

		// Truncate the division, then accept desired or desired+1.

		if (icd->fNTSC)
			lDesiredVideoFrame = icd->lFirstVideoPt + ((icd->total_audio_data_size - icd->audio_first_size + lpWHdr->dwBytesRecorded) * 30000i64) / (icd->wfex.nAvgBytesPerSec*1001i64);
		else
			lDesiredVideoFrame = icd->lFirstVideoPt + ((icd->total_audio_data_size - icd->audio_first_size + lpWHdr->dwBytesRecorded) * 1000000i64) / (icd->wfex.nAvgBytesPerSec*(__int64)icd->interval);

		lDelta = (long)icd->lastFrame - lDesiredVideoFrame;

		if (lDelta > 1) {
			icd->lVideoAdjust -= lDelta-1;
//			icd->lVideoAdjust -= icd->interval/1000;
_RPT2(0,"Timing reverse at %ld ms (%ld ms corrected)\n", dwOTime, dwTime);
		} else if (lDelta < 0) {
			icd->lVideoAdjust -= lDelta;
//			icd->lVideoAdjust += icd->interval/1000;
_RPT2(0,"Timing forward at %ld ms (%ld ms corrected)\n", dwOTime, dwTime);
		}

	}

	if (g_fLogEvents)
		g_capLog.LogAudio(dwTime, lpWHdr->dwBytesRecorded, 0);

	// Has the I/O thread successfully completed the switch?

	if (icd->aoFile == icd->aoFilePending) {
		icd->aoFile = icd->aoFilePending;
		icd->aoFilePending = NULL;
	}

	icd->lAudioLastMS = capStatus.dwCurrentTimeElapsedMS;

	++icd->total_audio_cap;

	if (icd->aoFile) {
		try {
			if (g_fEnableSpill) {
				char *pSrc = (char *)lpWHdr->lpData;
				long left = (long)lpWHdr->dwBytesRecorded;

				// If there is a switch point, write up to it.  Otherwise, write it all!

				while(left > 0) {
					long tc;

					tc = left;

					if (icd->nAudioSwitchPt && icd->nAudioBlocks+tc/icd->blockAlign >= icd->nAudioSwitchPt)
						tc = (long)((icd->nAudioSwitchPt - icd->nAudioBlocks) * icd->blockAlign);

					hr = icd->aoFileAudio->audioOut->write(0, pSrc, tc, tc / icd->blockAlign);
					icd->total_audio_size += tc + 24;
					icd->segment_audio_size += tc + 24;
					icd->nAudioBlocks += tc / icd->blockAlign;

					if (icd->nAudioSwitchPt && icd->nAudioBlocks == icd->nAudioSwitchPt) {
						// Switch audio to next stripe.

						icd->aoFileAudio = icd->aoFilePending;

						if (!icd->nVideoSwitchPt) {
							PostThreadMessage(icd->dwThreadID, VDCM_SWITCH_FIN, 0, 0);
							_RPT0(0,"AUDIO: Triggering finalize & switch.\n");
						} else
							_RPT2(0,"AUDIO: Switching to next, waiting for video to reach sync point (%I64d < %I64d)\n", icd->nVideoBlocks, icd->nVideoSwitchPt);

						icd->nAudioSwitchPt = 0;
						icd->segment_audio_size = 0;
					}

					left -= tc;
					pSrc += tc;
				}
			} else {
				hr = icd->aoFile->audioOut->write(0, lpWHdr->lpData, lpWHdr->dwBytesRecorded, lpWHdr->dwBytesRecorded / icd->blockAlign);
				icd->total_audio_size += lpWHdr->dwBytesRecorded + 24;
				icd->segment_audio_size += lpWHdr->dwBytesRecorded + 24;
			}
		} catch(MyError e) {
			if (!icd->fatal_error)
				icd->fatal_error = new MyError(e);

			capCaptureAbort(hWnd);

			return FALSE;
		}
	} else {
		icd->total_audio_size += lpWHdr->dwBytesRecorded + 24;
		icd->segment_audio_size += lpWHdr->dwBytesRecorded + 24;
		hr = 0;
	}

	if (!icd->audio_first_size) {
		icd->audio_first_size = lpWHdr->dwBytesRecorded;
		icd->lAudioFirstMS = capStatus.dwCurrentTimeElapsedMS;
	}

	icd->total_audio_data_size += lpWHdr->dwBytesRecorded;

	CaptureUpdateAudioTiming(icd, hWnd, dwTime);

    return TRUE;
}

//////

static LRESULT CALLBACK CaptureInternalVideoCallbackProc(HWND hWnd, LPVIDEOHDR lpVHdr)
{
	InternalCapData *icd = (InternalCapData *)capGetUserData(hWnd);
	LRESULT lr = 0;

	if (icd->fatal_error) return 0;
	if (icd->fatal_error_2) {
		icd->fatal_error = icd->fatal_error_2;
		icd->fatal_error_2 = NULL;
		capCaptureAbort(hWnd);
		return 0;
	}

	////////////////////////
	CAPINT_FATAL_CATCH_START
	////////////////////////

	lr = CaptureInternalVideoCallbackProc2(icd, hWnd, lpVHdr);

	if (g_bCaptureDDrawActive)
		CaptureOverlayFrameCallback(hWnd, lpVHdr);

	///////////////////////////////
	CAPINT_FATAL_CATCH_END("video")
	///////////////////////////////

	return lr;
}

static LRESULT CALLBACK CaptureInternalWaveCallbackProc(HWND hWnd, LPWAVEHDR lpWHdr)
{
	InternalCapData *icd = (InternalCapData *)capGetUserData(hWnd);
	LRESULT lr = 0;

	if (icd->fatal_error) return 0;
	if (icd->fatal_error_2) {
		icd->fatal_error = icd->fatal_error_2;
		icd->fatal_error_2 = NULL;
		capCaptureAbort(hWnd);
		return 0;
	}

	////////////////////////
	CAPINT_FATAL_CATCH_START
	////////////////////////

	lr = CaptureInternalWaveCallbackProc2(icd, hWnd, lpWHdr);

	///////////////////////////////
	CAPINT_FATAL_CATCH_END("audio")
	///////////////////////////////

	return lr;
}

//////

static BOOL CALLBACK CaptureInternalHitOKDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:
		{
			CAPSTATUS cs;

			memset(&cs, 0, sizeof cs);

			capGetStatus((HWND)lParam, &cs, sizeof cs);

			SetDlgItemInt(hDlg, IDC_AUDIO_BUFFERS, cs.wNumAudioAllocated, FALSE);
			SetDlgItemInt(hDlg, IDC_VIDEO_BUFFERS, cs.wNumVideoAllocated, FALSE);
		}
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			EndDialog(hDlg, TRUE);
			return TRUE;
		case IDCANCEL:
			EndDialog(hDlg, FALSE);
			return TRUE;
		}
	}

	return FALSE;
}

static LRESULT CALLBACK CaptureInternalControlCallbackProc(HWND hwnd, int nState) {
	if (nState == CONTROLCALLBACK_PREROLL) {
//		InternalCapData *icd = (InternalCapData *)capGetUserData(hwnd);

		return DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_HITOK), hwnd, CaptureInternalHitOKDlgProc, (LPARAM)hwnd)
			&& CaptureControlCallbackProc(hwnd, nState);
	} else
		return CaptureControlCallbackProc(hwnd, nState);

//	return TRUE;
}

static void CaptureInternal(HWND hWnd, HWND hWndCapture, bool fTest) {
	char fname[MAX_PATH];
	LRESULT lRes;
	LONG biSize, biSizeToFile, wfSize;
	CAPTUREPARMS cp, cp_back;
	AVIStreamHeader_fixed *astrhdr, *vstrhdr;
	InternalCapData icd;

	BITMAPINFO *bmiInput = NULL, *bmiOutput = NULL, *bmiToFile;
	WAVEFORMATEX *wfexInput = NULL;
	bool fMainFinalized = false, fPendingFinalized = false;

//	memset(&icd, 0, sizeof icd);

	icd.fatal_error	= NULL;

	g_capLog.Dispose();

	try {
		// get the input filename

		if (!capFileGetCaptureFile(hWndCapture, fname, sizeof fname))
			throw MyError("Couldn't get capture filename.");

		icd.pszFilename = SplitPathName(fname);

		// get capture parms

		if (!capCaptureGetSetup(hWndCapture, &cp, sizeof(CAPTUREPARMS)))
			throw MyError("Couldn't get capture setup info.");

		// create an output file object

		if (!fTest) {
			if (g_capStripeSystem) {
				if (g_fEnableSpill)
					throw MyError("Sorry, striping and spilling are not compatible.");

				icd.aoFile = new AVIOutputStriped(g_capStripeSystem);

				if (g_prefs.fAVIRestrict1Gb)
					((AVIOutputStriped *)icd.aoFile)->set_1Gb_limit();
			} else {
				icd.aoFile = new AVIOutputFile();

				if (g_prefs.fAVIRestrict1Gb)
					((AVIOutputFile *)icd.aoFile)->set_1Gb_limit();
			}

			icd.aoFileAudio = icd.aoFile;
			icd.aoFileVideo = icd.aoFile;

			((AVIOutputFile *)icd.aoFile)->set_capture_mode(true);

			if (!icd.aoFile) throw MyMemoryError();

			// initialize the AVIOutputFile object

			if (!icd.aoFile->initOutputStreams())
				throw MyError("Error initializing output streams.");
		}

		// initialize audio

		wfSize = capGetAudioFormatSize(hWndCapture);

		if (!(wfexInput = (WAVEFORMATEX *)allocmem(wfSize)))
			throw MyMemoryError();

		if (!capGetAudioFormat(hWndCapture, wfexInput, wfSize))
			throw MyError("Couldn't get audio format");

		// initialize video

		biSize = capGetVideoFormatSize(hWndCapture);

		if (!(bmiInput = (BITMAPINFO *)allocmem(biSize)))
			throw MyMemoryError();

		if (!capGetVideoFormat(hWndCapture, bmiInput, biSize))
			throw MyError("Couldn't get video format");

		// initialize filtering

		bmiToFile = (BITMAPINFO *)CaptureInitFiltering(&icd, &bmiInput->bmiHeader, cp.dwRequestMicroSecPerFrame, true);
		biSizeToFile = bmiToFile->bmiHeader.biSize;

		// initialize video compression

		if (g_compression.hic) {
			LONG formatSize;
			DWORD icErr;

			formatSize = ICCompressGetFormatSize(g_compression.hic, bmiToFile);
			if (formatSize < ICERR_OK)
				throw MyError("Error getting compressor output format size.");

			if (!(bmiOutput = (BITMAPINFO *)allocmem(formatSize)))
				throw MyMemoryError();

			if (ICERR_OK != (icErr = ICCompressGetFormat(g_compression.hic, &bmiToFile->bmiHeader, bmiOutput)))
				throw MyICError("Video compressor",icErr);

			if (!(icd.pvsc = new VideoSequenceCompressor()))
				throw MyMemoryError();

			icd.pvsc->init(g_compression.hic, bmiToFile, bmiOutput, g_compression.lQ, g_compression.lKey);
			icd.pvsc->setDataRate(g_compression.lDataRate*1024, cp.dwRequestMicroSecPerFrame, 0x0FFFFFFF);
			icd.pvsc->start();

			bmiToFile = bmiOutput;
			biSizeToFile = formatSize;
		}

		// set up output file headers and formats

		if (!fTest) {
			BITMAPINFO *bmi;
			WAVEFORMATEX *wf;

			bmi = (BITMAPINFO *)icd.aoFile->videoOut->allocFormat(biSizeToFile);

			if (!bmi)
				throw MyMemoryError();

			memcpy(bmi, bmiToFile, biSizeToFile);

			// setup stream headers

			vstrhdr = &icd.aoFile->videoOut->streamInfo;

			memset(vstrhdr,0,sizeof *vstrhdr);
			vstrhdr->fccType				= streamtypeVIDEO;
			vstrhdr->fccHandler				= bmiToFile->bmiHeader.biCompression;
			vstrhdr->dwScale				= cp.dwRequestMicroSecPerFrame;
			vstrhdr->dwRate					= 1000000;
			vstrhdr->dwSuggestedBufferSize	= 0;
			vstrhdr->dwQuality				= g_compression.hic ? g_compression.lQ : (unsigned long)-1;
			vstrhdr->rcFrame.left			= 0;
			vstrhdr->rcFrame.top			= 0;
			vstrhdr->rcFrame.right			= (short)bmiToFile->bmiHeader.biWidth;
			vstrhdr->rcFrame.bottom			= (short)bmiToFile->bmiHeader.biHeight;

			icd.aoFile->videoOut->setCompressed(bmiToFile->bmiHeader.biCompression!=BI_RGB);

			if (cp.fCaptureAudio) {
				if (!(wf = (WAVEFORMATEX *)icd.aoFile->audioOut->allocFormat(wfSize)))
					throw MyMemoryError();

				memcpy(wf, wfexInput, wfSize);

				astrhdr = &icd.aoFile->audioOut->streamInfo;

				memset(astrhdr,0,sizeof *astrhdr);
				astrhdr->fccType				= streamtypeAUDIO;
				astrhdr->fccHandler				= 0;
				astrhdr->dwScale				= wf->nBlockAlign;
				astrhdr->dwRate					= wf->nAvgBytesPerSec;
				astrhdr->dwQuality				= (unsigned long)-1;
				astrhdr->dwSampleSize			= wf->nBlockAlign; 
			}
		}

		// Setup capture structure

		memcpy(&icd.wfex, wfexInput, min(wfSize, sizeof icd.wfex));

		icd.hwndStatus	= GetDlgItem(hWnd, IDC_STATUS_WINDOW);
		icd.hwndPanel	= GetDlgItem(hWnd, IDC_CAPTURE_PANEL);
		icd.blockAlign	= wfexInput->nBlockAlign;
		icd.pszPath		= icd.szCaptureRoot;

		icd.fNTSC = ((cp.dwRequestMicroSecPerFrame|1) == 33367);
		icd.interval	= cp.dwRequestMicroSecPerFrame;

		icd.lVideoAdjust		= 0;

		if (!bmiInput->bmiHeader.biBitCount)
			icd.uncompressed_frame_size		= ((bmiInput->bmiHeader.biWidth * 2 + 3) & -3) * bmiInput->bmiHeader.biHeight;
		else
			icd.uncompressed_frame_size		= ((bmiInput->bmiHeader.biWidth * ((bmiInput->bmiHeader.biBitCount + 7)/8) + 3) & -3) * bmiInput->bmiHeader.biHeight;

		// create font

		if (g_fDisplayLargeTimer)
			icd.hFont = CreateFont(200, 0,
									0, 0, 0,
									FALSE, FALSE, FALSE,
									ANSI_CHARSET,
									OUT_DEFAULT_PRECIS,
									CLIP_DEFAULT_PRECIS,
									DEFAULT_QUALITY,
									FF_DONTCARE|DEFAULT_PITCH,
									"Arial");

		if (!SplitPathRoot(icd.szCaptureRoot, fname)) {
			icd.szCaptureRoot[0] = 0;
			MyGetDiskFreeSpace(NULL);
		} else
			MyGetDiskFreeSpace(icd.szCaptureRoot);

		// initialize the file
		//
		// this is kinda sick

		if (!fTest) {
			if (!g_capStripeSystem && g_diskDisableBuffer) {
				((AVIOutputFile *)icd.aoFile)->disable_os_caching();
				((AVIOutputFile *)icd.aoFile)->set_chunk_size(1024 * g_diskChunkSize);
			}

			if (g_fEnableSpill) {
				char szNameFirst[MAX_PATH];

				((AVIOutputFile *)icd.aoFile)->setSegmentHintBlock(true, NULL, MAX_PATH+1);

				strcpy(szNameFirst, fname);
				strcpy((char *)SplitPathExt(szNameFirst), ".00.avi");

				if (!(icd.fwsActive = ((AVIOutputFile *)icd.aoFile)->initCapture(szNameFirst, bmiToFile->bmiHeader.biWidth, bmiToFile->bmiHeader.biHeight,
					TRUE, cp.fCaptureAudio, 1024 * g_diskChunkSize * g_diskChunkCount, TRUE)))
					throw MyError("Error initializing capture file.");

				// Figure out what drive the first file is on, to get the disk threshold.  If we
				// don't know, make it 50Mb.

				CapSpillDrive *pcsd;

				if (pcsd = CapSpillFindDrive(szNameFirst))
					icd.lDiskThresh = pcsd->threshold;
				else
					icd.lDiskThresh = 50;
			} else
				if (!icd.aoFile->init(fname, bmiToFile->bmiHeader.biWidth, bmiToFile->bmiHeader.biHeight,
					TRUE, cp.fCaptureAudio, 1024 * g_diskChunkSize * g_diskChunkCount, TRUE))
					throw MyError("Error initializing capture file.");
		}

		// Allocate audio buffer and begin IO thread.

		if (g_fEnableSpill) {
			HANDLE hTemp;

			hTemp = (HANDLE)_beginthreadex(NULL, 0, CaptureInternalSpillThread, (void *)&icd, CREATE_SUSPENDED, (unsigned *)&icd.dwThreadID);

			if (!hTemp)
				throw MyWin32Error("Can't start I/O thread: %%s", GetLastError());

			if (!DuplicateHandle(GetCurrentProcess(), hTemp, GetCurrentProcess(), &icd.hIOThread, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
				ResumeThread(hTemp);
				while(!PostThreadMessage(icd.dwThreadID, VDCM_EXIT, 0, 0))
					Sleep(100);
				WaitForSingleObject(hTemp, INFINITE);
				CloseHandle(hTemp);
				throw MyWin32Error("Can't start I/O thread: %%s", GetLastError());
			}

			CloseHandle(hTemp);
			ResumeThread(icd.hIOThread);
		}

		// capture!!

		cp_back = cp;

		if (cp.fMakeUserHitOKToCapture) {
			if (cp.fAbortLeftMouse && !cp.fAbortRightMouse) {
				cp.fAbortLeftMouse = FALSE;
				cp.fAbortRightMouse = TRUE;

			}
			capSetCallbackOnCapControl(hWndCapture, CaptureInternalControlCallbackProc);
		} else
			capSetCallbackOnCapControl(hWndCapture, CaptureControlCallbackProc);

		// Turn off time limiting!!

		cp.fLimitEnabled = false;

		capCaptureSetSetup(hWndCapture, &cp, sizeof cp);
		capSetUserData(hWndCapture, (LPARAM)&icd);
		capSetCallbackOnVideoStream(hWndCapture, CaptureInternalVideoCallbackProc);
		if (cp.fCaptureAudio) capSetCallbackOnWaveStream(hWndCapture, CaptureInternalWaveCallbackProc);

		CaptureShowFile(hWnd, hWndCapture, true);
		g_fRestricted = true;
		lRes = capCaptureSequenceNoFile(hWndCapture);
		g_fRestricted = false;
		CaptureShowFile(hWnd, hWndCapture, false);

		capSetCallbackOnCapControl(hWndCapture, NULL);
		capSetCallbackOnWaveStream(hWndCapture, NULL);
		capSetCallbackOnVideoStream(hWndCapture, NULL);

_RPT0(0,"Capture has stopped.\n");

		capCaptureSetSetup(hWndCapture, &cp_back, sizeof cp_back);

		if (icd.fatal_error)
			throw *icd.fatal_error;

		if (icd.hIOThread) {
			PostThreadMessage(icd.dwThreadID, VDCM_EXIT, 0, 0);
			WaitForSingleObject(icd.hIOThread, INFINITE);
			CloseHandle(icd.hIOThread);
			icd.hIOThread = NULL;
		}

		if (icd.pvsc)
			icd.pvsc->finish();

		// finalize files

		if (!fTest) {
			_RPT0(0,"Finalizing main file.\n");

			fMainFinalized = true;
			if (g_fEnableSpill)
				icd.fwsActive->setSynchronous(true);

			if (!icd.aoFile->finalize())
				throw MyError("Error finalizing file.");

			fPendingFinalized = true;
			if (icd.aoFilePending && icd.aoFilePending != icd.aoFile) {
				_RPT0(0,"Finalizing pending file.\n");

				if (g_fEnableSpill)
					icd.fwsPending->setSynchronous(true);

				if (!icd.aoFilePending->finalize())
					throw MyError("Error finalizing file.");
			}
		}

		_RPT0(0,"Yatta!!!\n");

	} catch(MyError e) {
		e.post(hWnd, "Capture error");
	}

	CaptureDeinitFiltering(&icd);

	// Kill the I/O thread.

	if (icd.hIOThread) {
		PostThreadMessage(icd.dwThreadID, VDCM_EXIT, 0, 0);
		WaitForSingleObject(icd.hIOThread, INFINITE);
		CloseHandle(icd.hIOThread);
		icd.hIOThread = NULL;
	}

	// Might as well try and finalize anyway.  If we're finalizing here,
	// we encountered an error, so don't go and throw more out!

	if (!fTest)
		try {
			if (!fMainFinalized) {
				if (g_fEnableSpill)
					icd.fwsActive->setSynchronous(true);

				icd.aoFile->finalize();
			}

			if (!fPendingFinalized && icd.aoFilePending && icd.aoFilePending != icd.aoFile) {
				if (g_fEnableSpill)
					icd.fwsPending->setSynchronous(true);

				icd.aoFilePending->finalize();
			}
		} catch(MyError e) {
		}

	if (icd.fatal_error) delete icd.fatal_error;
	if (icd.fatal_error_2) delete icd.fatal_error_2;
	if (icd.hFont)
		DeleteObject(icd.hFont);
	freemem(bmiInput);
	freemem(bmiOutput);
	freemem(wfexInput);
	if (icd.pvsc)
		delete icd.pvsc;
	delete icd.aoFile;

	if (icd.aoFilePending && icd.aoFilePending != icd.aoFile)
		delete icd.aoFilePending;

	// any warnings?

	DWORD dw;

	if (icd.fWarnVideoCaptureTiming1) {
		if (!QueryConfigDword(g_szCapture, g_szWarnTiming1, &dw) || !dw) {
			if (IDYES != MessageBox(hWnd,
					"VirtualDub has detected, and compensated for, a possible bug in your video capture drivers that is causing "
					"its timing information to wrap around at 71 minutes.  Your capture should be okay, but you may want "
					"to try upgrading your video capture drivers anyway, since this can cause video capture to halt in "
					"other applications.\n"
					"\n"
					"Do you want VirtualDub to warn you the next time this happens?"
					, "VirtualDub Warning", MB_YESNO))

				SetConfigDword(g_szCapture, g_szWarnTiming1, 1);
		}
	}
}

void CaptureInternalSelectCompression(HWND hwndCapture) {
	HWND hwnd = GetParent(hwndCapture);
	BITMAPINFOHEADER *bih;
	DWORD fsize;

	if (!(g_compression.dwFlags & ICMF_COMPVARS_VALID)) {
		memset(&g_compression, 0, sizeof g_compression);
		g_compression.dwFlags |= ICMF_COMPVARS_VALID;
		g_compression.lQ = 10000;
	}

	g_compression.cbSize = sizeof(COMPVARS);

	if (fsize = capGetVideoFormatSize(hwndCapture)) {
		if (bih = (BITMAPINFOHEADER *)allocmem(fsize)) {
			if (capGetVideoFormat(hwndCapture, bih, fsize)) {
//				ICCompressorChoose(hwnd, ICMF_CHOOSE_DATARATE | ICMF_CHOOSE_KEYFRAME, (void *)bih, NULL, &g_compression, "Video compression (internal mode)");
				ChooseCompressor(hwnd, &g_compression, bih);
				freemem(bih);
				return;
			}
			freemem(bih);
		}
	}
	ChooseCompressor(hwnd, &g_compression, NULL);
//	ICCompressorChoose(hwnd, ICMF_CHOOSE_ALLCOMPRESSORS | ICMF_CHOOSE_DATARATE | ICMF_CHOOSE_KEYFRAME, NULL, NULL, &g_compression, "Video compression (internal mode)");
}


static void CaptureInternalLoadFromRegistry() {
	CaptureCompressionSpecs cs;

	memset(&g_compression, 0, sizeof g_compression);

	if (QueryConfigBinary(g_szCapture, g_szCompression, (char *)&cs, sizeof cs)) {
		void *lpData;
		DWORD dwSize;

		if (cs.fccType != 'CDIV' || !cs.fccHandler) {
			// err... bad config data.

			DeleteConfigValue(g_szCapture, g_szCompression);
			DeleteConfigValue(g_szCapture, g_szCompressorData);
			return;
		}

		g_compression.cbSize		= sizeof(COMPVARS);
		g_compression.dwFlags		= ICMF_COMPVARS_VALID;
		g_compression.hic			= ICOpen(cs.fccType, cs.fccHandler, ICMODE_COMPRESS);
		g_compression.fccType		= cs.fccType;
		g_compression.fccHandler	= cs.fccHandler;
		g_compression.lKey			= cs.lKey;
		g_compression.lDataRate		= cs.lDataRate;
		g_compression.lQ			= cs.lQ;

		if (g_compression.hic) {
			if (dwSize = QueryConfigBinary(g_szCapture, g_szCompressorData, NULL, 0)) {

				if (lpData = allocmem(dwSize)) {
					memset(lpData, 0, dwSize);

					if (QueryConfigBinary(g_szCapture, g_szCompressorData, (char *)lpData, dwSize))
						ICSetState(g_compression.hic, lpData, dwSize);

					freemem(lpData);
				}
			}
		} else
			g_compression.dwFlags = 0;
	}
}



////////////////////////////////////////////////////////////////////////////
//
//	preferences
//
////////////////////////////////////////////////////////////////////////////

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
		CaptureCompressionSpecs cs;
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
			if (g_current_driver == i) g_driver_options = g_drvOpts[i];
		}
}

static void CapturePreferencesDlgBrowse(HWND hDlg) {
	extern const char fileFilters0[];

	OPENFILENAME ofn;
	char szFile[MAX_PATH];

	///////////////

	strcpy(szFile, g_szCaptureFile);

	ofn.lStructSize			= sizeof(OPENFILENAME);
	ofn.hwndOwner			= hDlg;
	ofn.lpstrFilter			= fileFilters0;
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= szFile;
	ofn.nMaxFile			= sizeof szFile;
	ofn.lpstrFileTitle		= NULL;
	ofn.nMaxFileTitle		= 0;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= "Select default capture file";
	ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_ENABLESIZING;
	ofn.lpstrDefExt			= NULL;

	if (GetSaveFileName(&ofn))
		SetDlgItemText(hDlg, IDC_DEFAULT_CAPFILE, szFile);
}

DWORD dwCapturePrefsHelpLookup[]={
	IDC_DEFAULT_DRIVER,			IDH_CAPTURE_PREFS_DEFAULTDRIVER,
	IDC_INITIAL_NODISPLAY,		IDH_CAPTURE_PREFS_INITIALDISPLAY,
	IDC_INITIAL_PREVIEW,		IDH_CAPTURE_PREFS_INITIALDISPLAY,
	IDC_INITIAL_OVERLAY,		IDH_CAPTURE_PREFS_INITIALDISPLAY,
	IDC_SLOW_PREVIEW,			IDH_CAPTURE_PREFS_SLOW,
	IDC_SLOW_OVERLAY,			IDH_CAPTURE_PREFS_SLOW,
	IDC_DRIVER_TO_SET,			IDH_CAPTURE_PREFS_PERDRIVER,
	0
};

static BOOL APIENTRY CapturePreferencesDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	switch(message) {
		case WM_INITDIALOG:
			SetWindowLong(hDlg, DWL_USER, lParam);
			return CapturePreferencesDlgInit(hDlg);

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					HelpPopupByID(hDlg, lphi->iCtrlId, dwCapturePrefsHelpLookup);
			}
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				CapturePreferencesDlgStore(hDlg, (HWND)GetWindowLong(hDlg, DWL_USER));
			case IDCANCEL:
				EndDialog(hDlg, 0);
				return TRUE;

			case IDC_SELECT_CAPTURE_FILE:
				CapturePreferencesDlgBrowse(hDlg);
				return TRUE;

			case IDC_USE_CURRENT_FILE:
				SetDlgItemText(hDlg, IDC_DEFAULT_CAPFILE, g_szCaptureFile);
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




////////////////////////////////////////////////////////////////////////////
//
//	stop conditions
//
////////////////////////////////////////////////////////////////////////////

static BOOL APIENTRY CaptureStopConditionsDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {

	case WM_INITDIALOG:
		EnableWindow(GetDlgItem(hdlg, IDC_TIMELIMIT_SETTING), g_stopPrefs.fEnableFlags & CAPSTOP_TIME);
		EnableWindow(GetDlgItem(hdlg, IDC_FILELIMIT_SETTING), g_stopPrefs.fEnableFlags & CAPSTOP_FILESIZE);
		EnableWindow(GetDlgItem(hdlg, IDC_DISKLIMIT_SETTING), g_stopPrefs.fEnableFlags & CAPSTOP_DISKSPACE);
		EnableWindow(GetDlgItem(hdlg, IDC_DROPLIMIT_SETTING), g_stopPrefs.fEnableFlags & CAPSTOP_DROPRATE);

		CheckDlgButton(hdlg, IDC_TIMELIMIT, g_stopPrefs.fEnableFlags & CAPSTOP_TIME ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hdlg, IDC_FILELIMIT, g_stopPrefs.fEnableFlags & CAPSTOP_FILESIZE ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hdlg, IDC_DISKLIMIT, g_stopPrefs.fEnableFlags & CAPSTOP_DISKSPACE ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hdlg, IDC_DROPLIMIT, g_stopPrefs.fEnableFlags & CAPSTOP_DROPRATE ? BST_CHECKED : BST_UNCHECKED);

		SetDlgItemInt(hdlg, IDC_TIMELIMIT_SETTING, g_stopPrefs.lTimeLimit, FALSE);
		SetDlgItemInt(hdlg, IDC_FILELIMIT_SETTING, g_stopPrefs.lSizeLimit, FALSE);
		SetDlgItemInt(hdlg, IDC_DISKLIMIT_SETTING, g_stopPrefs.lDiskSpaceThreshold, FALSE);
		SetDlgItemInt(hdlg, IDC_DROPLIMIT_SETTING, g_stopPrefs.lMaxDropRate, FALSE);

		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
		case IDC_ACCEPT:
			g_stopPrefs.lTimeLimit = GetDlgItemInt(hdlg, IDC_TIMELIMIT_SETTING, NULL, FALSE);
			g_stopPrefs.lSizeLimit = GetDlgItemInt(hdlg, IDC_FILELIMIT_SETTING, NULL, FALSE);
			g_stopPrefs.lDiskSpaceThreshold = GetDlgItemInt(hdlg, IDC_DISKLIMIT_SETTING, NULL, FALSE);
			g_stopPrefs.lMaxDropRate = GetDlgItemInt(hdlg, IDC_DROPLIMIT_SETTING, NULL, FALSE);
			g_stopPrefs.fEnableFlags = 0;

			if (IsDlgButtonChecked(hdlg, IDC_TIMELIMIT))
				g_stopPrefs.fEnableFlags |= CAPSTOP_TIME;

			if (IsDlgButtonChecked(hdlg, IDC_FILELIMIT))
				g_stopPrefs.fEnableFlags |= CAPSTOP_FILESIZE;

			if (IsDlgButtonChecked(hdlg, IDC_DISKLIMIT))
				g_stopPrefs.fEnableFlags |= CAPSTOP_DISKSPACE;

			if (IsDlgButtonChecked(hdlg, IDC_DROPLIMIT))
				g_stopPrefs.fEnableFlags |= CAPSTOP_DROPRATE;

			if (LOWORD(wParam) == IDOK)
				SetConfigBinary(g_szCapture, g_szStopConditions, (char *)&g_stopPrefs, sizeof g_stopPrefs);

		case IDCANCEL:
			EndDialog(hdlg, 0);
			return TRUE;

		case IDC_TIMELIMIT:
			EnableWindow(GetDlgItem(hdlg, IDC_TIMELIMIT_SETTING), SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED);
			return TRUE;

		case IDC_FILELIMIT:
			EnableWindow(GetDlgItem(hdlg, IDC_FILELIMIT_SETTING), SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED);
			return TRUE;

		case IDC_DISKLIMIT:
			EnableWindow(GetDlgItem(hdlg, IDC_DISKLIMIT_SETTING), SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED);
			return TRUE;

		case IDC_DROPLIMIT:
			EnableWindow(GetDlgItem(hdlg, IDC_DROPLIMIT_SETTING), SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED);
			return TRUE;
		}
		break;
	}
	return FALSE;
}






////////////////////////////////////////////////////////////////////////////
//
//	disk I/O dialog
//
////////////////////////////////////////////////////////////////////////////


static BOOL APIENTRY CaptureDiskIODlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {

	static const long sizes[]={
		64,
		128,
		256,
		512,
		1*1024,
		2*1024,
		4*1024,
		6*1024,
		8*1024,
		12*1024,
		16*1024,
	};

	static const char *size_names[]={
		"64K",
		"128K",
		"256K",
		"512K",
		"1Mb",
		"2Mb",
		"4Mb",
		"6Mb",
		"8Mb",
		"12Mb",
		"16Mb",
	};

	switch(msg) {
	case WM_INITDIALOG:
		{
			int i;
			HWND hwndItem;

			hwndItem = GetDlgItem(hdlg, IDC_CHUNKSIZE);
			for(i=0; i<sizeof sizes/sizeof sizes[0]; i++)
				SendMessage(hwndItem, CB_ADDSTRING, 0, (LPARAM)size_names[i]);
			SendMessage(hwndItem, CB_SETCURSEL, NearestLongValue(g_diskChunkSize, sizes, sizeof sizes/sizeof sizes[0]), 0);

			SendDlgItemMessage(hdlg, IDC_CHUNKS_UPDOWN, UDM_SETBUDDY, (WPARAM)GetDlgItem(hdlg, IDC_CHUNKS), 0);
			SendDlgItemMessage(hdlg, IDC_CHUNKS_UPDOWN, UDM_SETRANGE, 0, MAKELONG(256, 1));

			SetDlgItemInt(hdlg, IDC_CHUNKS, g_diskChunkCount, FALSE);
			CheckDlgButton(hdlg, IDC_DISABLEBUFFERING, g_diskDisableBuffer ? BST_CHECKED : BST_UNCHECKED);
		}
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
		case IDC_ACCEPT:
			{
				BOOL fOk;
				int chunks;

				chunks = GetDlgItemInt(hdlg, IDC_CHUNKS, &fOk, FALSE);

				if (!fOk || chunks<1 || chunks>256) {
					SetFocus(GetDlgItem(hdlg, IDC_CHUNKS));
					MessageBeep(MB_ICONQUESTION);
					return TRUE;
				}

				g_diskChunkCount = chunks;
				g_diskChunkSize = sizes[SendDlgItemMessage(hdlg, IDC_CHUNKSIZE, CB_GETCURSEL, 0, 0)];
				g_diskDisableBuffer = IsDlgButtonChecked(hdlg, IDC_DISABLEBUFFERING);

				if (LOWORD(wParam) == IDOK) {
					SetConfigDword(g_szCapture, g_szChunkCount, g_diskChunkCount);
					SetConfigDword(g_szCapture, g_szChunkSize, g_diskChunkSize);
					SetConfigDword(g_szCapture, g_szDisableBuffering, g_diskDisableBuffer);
				}
			}
		case IDCANCEL:
			EndDialog(hdlg, 0);
			return TRUE;

		case IDC_CHUNKS:
		case IDC_CHUNKSIZE:
			{
				BOOL fOk;
				int chunks;
				int cs;

				chunks = GetDlgItemInt(hdlg, IDC_CHUNKS, &fOk, FALSE);

				if (fOk) {
					char buf[64];

					cs = SendDlgItemMessage(hdlg, IDC_CHUNKSIZE, CB_GETCURSEL, 0, 0);

					sprintf(buf, "Total buffer: %ldK", sizes[cs] * chunks);
					SetDlgItemText(hdlg, IDC_STATIC_BUFFERSIZE, buf);
				} else
					SetDlgItemText(hdlg, IDC_STATIC_BUFFERSIZE, "Total buffer: ---");
			}
			return TRUE;
		}
		return FALSE;
	}

	return FALSE;
}



////////////////////////////////////////////////////////////////////////////
//
//	custom size dialog
//
////////////////////////////////////////////////////////////////////////////


static BOOL APIENTRY CaptureCustomVidSizeDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {

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
	HWND hwndItem, hwndCapture;
	int i;
	int ind;

	switch(msg) {
	case WM_INITDIALOG:
		{
			BITMAPINFOHEADER *pbih;
			int w = 320, h = 240;
			int found_w = -1, found_h = -1, found_f = -1;

			SetWindowLong(hdlg, DWL_USER, lParam);

			hwndCapture = (HWND)lParam;

			s_fcc = BI_RGB;
			s_bpp = 16;
			i = capGetVideoFormatSize(hwndCapture);
			if (pbih = (BITMAPINFOHEADER *)allocmem(i)) {
				if (capGetVideoFormat(hwndCapture, pbih, i)) {
					s_fcc = pbih->biCompression;
					w = pbih->biWidth;
					h = pbih->biHeight;
					s_bpp = pbih->biBitCount;
				}
				freemem(pbih);
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
				int w, h, f, cb;
				BOOL b;
				BITMAPINFOHEADER bih;
				BITMAPINFOHEADER *pbih;

				hwndCapture = (HWND)GetWindowLong(hdlg, DWL_USER);

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

				pbih = &bih;
				cb = sizeof(BITMAPINFOHEADER);

				if (!f) {
					cb = capGetVideoFormatSize(hwndCapture);
					if (pbih = (BITMAPINFOHEADER *)allocmem(cb)) {
						if (!capGetVideoFormat(hwndCapture, pbih, cb)) {
							freemem(pbih);
							pbih = &bih;
						}
					} else
						break;
				} else {
					pbih->biSize			= sizeof(BITMAPINFOHEADER);
					pbih->biCompression		= s_formats[f-1].fcc;
					pbih->biBitCount		= s_formats[f-1].bpp;
				}

				pbih->biWidth			= w;
				pbih->biHeight			= h;
				pbih->biPlanes			= 1;
				pbih->biSizeImage		= h * ((w * pbih->biBitCount + 31) / 32) * 4 * pbih->biPlanes;
				pbih->biXPelsPerMeter	= 80;
				pbih->biYPelsPerMeter	= 80;
				pbih->biClrUsed			= 0;
				pbih->biClrImportant	= 0;

				capSetVideoFormat(hwndCapture, (BITMAPINFO *)pbih, cb);

				if (pbih != &bih)
					freemem(pbih);

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

////////////////////////////////////////////////////////////////////////////
//
//	timing dialog
//
////////////////////////////////////////////////////////////////////////////


static BOOL APIENTRY CaptureTimingDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:
		CheckDlgButton(hdlg, IDC_ADJUSTVIDEOTIMING, g_fAdjustVideoTimer);

		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			g_fAdjustVideoTimer = !!IsDlgButtonChecked(hdlg, IDC_ADJUSTVIDEOTIMING);

			SetConfigDword(g_szCapture, g_szAdjustVideoTiming, g_fAdjustVideoTimer);

			EndDialog(hdlg, 1);
			return TRUE;

		case IDCANCEL:
			EndDialog(hdlg, 0);
			return TRUE;
		}
		return FALSE;
	}

	return FALSE;
}

////////////////////////////////////////////////////////////////////////////
//
//	noise reduction threshold dlg
//
////////////////////////////////////////////////////////////////////////////

static ModelessDlgNode g_mdnCapNRThreshold(NULL);

static BOOL CALLBACK CaptureNRThresholdDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	HWND hwndItem;
	switch(msg) {
		case WM_INITDIALOG:
			hwndItem = GetDlgItem(hdlg, IDC_THRESHOLD);
			SendMessage(hwndItem, TBM_SETRANGE, FALSE, MAKELONG(0, 64));
			SendMessage(hwndItem, TBM_SETPOS, TRUE, g_iNoiseReduceThreshold);
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDCANCEL:
				DestroyWindow(hdlg);
				break;
			}
			return TRUE;

		case WM_CLOSE:
			DestroyWindow(hdlg);
			break;

		case WM_DESTROY:
			g_mdnCapNRThreshold.Remove();
			g_mdnCapNRThreshold.hdlg = NULL;
			return TRUE;

		case WM_HSCROLL:
			g_iNoiseReduceThreshold = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);
			return TRUE;
	}

	return FALSE;
}

static void CaptureToggleNRDialog(HWND hwndParent) {
	if (!hwndParent) {
		if (g_mdnCapNRThreshold.hdlg) {
			DestroyWindow(g_mdnCapNRThreshold.hdlg);
		}
		return;
	}

	if (g_mdnCapNRThreshold.hdlg)
		SetForegroundWindow(g_mdnCapNRThreshold.hdlg);
	else {
		g_mdnCapNRThreshold.hdlg = CreateDialog(g_hInst, MAKEINTRESOURCE(IDD_CAPTURE_NOISEREDUCTION), hwndParent, CaptureNRThresholdDlgProc);
		guiAddModelessDialog(&g_mdnCapNRThreshold);
	}
}

