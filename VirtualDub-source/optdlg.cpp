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

#include <stdio.h>
#include <math.h>

#include <windows.h>
#include <commctrl.h>

#define OPTDLG_STATICS
#include "optdlg.h"

#include "resource.h"
#include "helpfile.h"
#include "oshelper.h"
#include "misc.h"
#include "gui.h"

#include "AudioSource.h"
#include "VideoSource.h"
#include "Dub.h"

extern char g_msgBuf[];
extern HINSTANCE g_hInst;

extern AudioSource *inputAudio;
extern VideoSource *inputVideoAVI;

extern void SetAudioSource();

///////////////////////////////////////////

void ActivateDubDialog(HINSTANCE hInst, LPCTSTR lpResource, HWND hDlg, DLGPROC dlgProc) {
	DubOptions duh;

	duh = g_dubOpts;
	if (DialogBoxParam(hInst, lpResource, hDlg, dlgProc, (LPARAM)&duh))
		g_dubOpts = duh;
}

///////////////////////////////////////////

static void AudioConversionDlgComputeBandwidth(HWND hDlg) {
	long bps=0;

	if (	 IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_NOCHANGE))	bps = inputAudio ? inputAudio->getWaveFormat()->nSamplesPerSec : 0;
	else if (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_11KHZ))		bps = 11025;
	else if (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_22KHZ))		bps = 22050;
	else if (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_44KHZ))		bps = 44100;
	else if (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_8KHZ))		bps = 8000;
	else if (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_16KHZ))		bps = 16000;
	else if (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_48KHZ))		bps = 48000;
	else if (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_CUSTOM))
		bps = GetDlgItemInt(hDlg, IDC_SAMPLINGRATE_CUSTOM_VAL, NULL, FALSE);

	if (	 IsDlgButtonChecked(hDlg, IDC_PRECISION_NOCHANGE))	bps *= inputAudio ? inputAudio->getWaveFormat()->wBitsPerSample>8 ? 2 : 1 : 1;
	else if (IsDlgButtonChecked(hDlg, IDC_PRECISION_16BIT))		bps *= 2;

	if (	 IsDlgButtonChecked(hDlg, IDC_CHANNELS_NOCHANGE))	bps *= inputAudio ? inputAudio->getWaveFormat()->nChannels>1 ? 2 : 1 : 1;
	else if (IsDlgButtonChecked(hDlg, IDC_CHANNELS_STEREO))		bps *= 2;

	if (bps)
		wsprintf(g_msgBuf, "Bandwidth required: %ldK/s", (bps+1023)>>10);
	else
		strcpy(g_msgBuf,"Bandwidth required: (unknown)");
	SetDlgItemText(hDlg, IDC_BANDWIDTH_REQD, g_msgBuf);
}

DWORD dwAudioConversionHelpLookup[]={
	IDC_SAMPLINGRATE_NOCHANGE,		IDH_AUDCONV_SAMPLINGRATE,
	IDC_SAMPLINGRATE_11KHZ,			IDH_AUDCONV_SAMPLINGRATE,
	IDC_SAMPLINGRATE_22KHZ,			IDH_AUDCONV_SAMPLINGRATE,
	IDC_SAMPLINGRATE_44KHZ,			IDH_AUDCONV_SAMPLINGRATE,
	IDC_SAMPLINGRATE_8KHZ,			IDH_AUDCONV_SAMPLINGRATE,
	IDC_SAMPLINGRATE_16KHZ,			IDH_AUDCONV_SAMPLINGRATE,
	IDC_SAMPLINGRATE_48KHZ,			IDH_AUDCONV_SAMPLINGRATE,
	IDC_SAMPLINGRATE_CUSTOM,		IDH_AUDCONV_SAMPLINGRATE,
	IDC_SAMPLINGRATE_CUSTOM_VAL,	IDH_AUDCONV_SAMPLINGRATE,
	IDC_SAMPLINGRATE_INTEGRAL,		IDH_AUDCONV_INTEGRALCONVERSION,
	IDC_SAMPLINGRATE_HQ,			IDH_AUDCONV_HIGHQUALITY,
	IDC_PRECISION_NOCHANGE,			IDH_AUDCONV_PRECISION,
	IDC_PRECISION_8BIT,				IDH_AUDCONV_PRECISION,
	IDC_PRECISION_16BIT,			IDH_AUDCONV_PRECISION,
	IDC_CHANNELS_NOCHANGE,			IDH_AUDCONV_CHANNELS,
	IDC_CHANNELS_MONO,				IDH_AUDCONV_CHANNELS,
	IDC_CHANNELS_STEREO,			IDH_AUDCONV_CHANNELS,
	IDC_BANDWIDTH_REQD,				IDH_AUDCONV_BANDWIDTHREQUIRED,
	NULL
};

BOOL APIENTRY AudioConversionDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
	DubOptions *dopt = (DubOptions *)GetWindowLong(hDlg, DWL_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			SetWindowLong(hDlg, DWL_USER, lParam);
			dopt = (DubOptions *)lParam;

			SetAudioSource();
			if (inputAudio) {
				wsprintf(g_msgBuf, "No change (%ldHz)", inputAudio->getWaveFormat()->nSamplesPerSec);
				SetDlgItemText(hDlg, IDC_SAMPLINGRATE_NOCHANGE, g_msgBuf);
				wsprintf(g_msgBuf, "No change (%ld-bit)", inputAudio->getWaveFormat()->wBitsPerSample>8 ? 16 : 8);
				SetDlgItemText(hDlg, IDC_PRECISION_NOCHANGE, g_msgBuf);
				wsprintf(g_msgBuf, "No change (%s)", inputAudio->getWaveFormat()->nChannels>1 ? "stereo" : "mono");
				SetDlgItemText(hDlg, IDC_CHANNELS_NOCHANGE, g_msgBuf);
			}

			switch(dopt->audio.new_rate) {
			case 0:		CheckDlgButton(hDlg, IDC_SAMPLINGRATE_NOCHANGE, TRUE); break;
			case 8000:	CheckDlgButton(hDlg, IDC_SAMPLINGRATE_8KHZ, TRUE);	break;
			case 11025:	CheckDlgButton(hDlg, IDC_SAMPLINGRATE_11KHZ, TRUE);	break;
			case 16000:	CheckDlgButton(hDlg, IDC_SAMPLINGRATE_16KHZ, TRUE);	break;
			case 22050:	CheckDlgButton(hDlg, IDC_SAMPLINGRATE_22KHZ, TRUE);	break;
			case 44100:	CheckDlgButton(hDlg, IDC_SAMPLINGRATE_44KHZ, TRUE);	break;
			case 48000:	CheckDlgButton(hDlg, IDC_SAMPLINGRATE_48KHZ, TRUE);	break;
			default:
				CheckDlgButton(hDlg, IDC_SAMPLINGRATE_CUSTOM, TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_SAMPLINGRATE_CUSTOM_VAL), TRUE);
				SetDlgItemInt(hDlg, IDC_SAMPLINGRATE_CUSTOM_VAL, dopt->audio.new_rate, FALSE);
				break;
			}
			CheckDlgButton(hDlg, IDC_SAMPLINGRATE_INTEGRAL, !!dopt->audio.integral_rate);
			CheckDlgButton(hDlg, IDC_SAMPLINGRATE_HQ, !!dopt->audio.fHighQuality);
			CheckDlgButton(hDlg, IDC_PRECISION_NOCHANGE+dopt->audio.newPrecision,TRUE);
			CheckDlgButton(hDlg, IDC_CHANNELS_NOCHANGE+dopt->audio.newChannels,TRUE);

			AudioConversionDlgComputeBandwidth(hDlg);

            return (TRUE);

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_SAMPLINGRATE_NOCHANGE:
			case IDC_SAMPLINGRATE_8KHZ:
			case IDC_SAMPLINGRATE_11KHZ:
			case IDC_SAMPLINGRATE_16KHZ:
			case IDC_SAMPLINGRATE_22KHZ:
			case IDC_SAMPLINGRATE_44KHZ:
			case IDC_SAMPLINGRATE_48KHZ:
				if (!IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_CUSTOM))
					EnableWindow(GetDlgItem(hDlg, IDC_SAMPLINGRATE_CUSTOM_VAL), FALSE);
			case IDC_PRECISION_NOCHANGE:
			case IDC_PRECISION_8BIT:
			case IDC_PRECISION_16BIT:
			case IDC_CHANNELS_NOCHANGE:
			case IDC_CHANNELS_MONO:
			case IDC_CHANNELS_STEREO:
			case IDC_CHANNELS_LEFT:
			case IDC_CHANNELS_RIGHT:
			case IDC_SAMPLINGRATE_CUSTOM_VAL:
				AudioConversionDlgComputeBandwidth(hDlg);
				break;

			case IDC_SAMPLINGRATE_CUSTOM:
				EnableWindow(GetDlgItem(hDlg, IDC_SAMPLINGRATE_CUSTOM_VAL), TRUE);
				AudioConversionDlgComputeBandwidth(hDlg);
				break;

			case IDOK:
				if      (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_NOCHANGE)) dopt->audio.new_rate = 0;
				else if (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_8KHZ   )) dopt->audio.new_rate = 8000;
				else if (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_11KHZ   )) dopt->audio.new_rate = 11025;
				else if (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_16KHZ   )) dopt->audio.new_rate = 16000;
				else if (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_22KHZ   )) dopt->audio.new_rate = 22050;
				else if (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_44KHZ   )) dopt->audio.new_rate = 44100;
				else if (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_48KHZ   )) dopt->audio.new_rate = 48000;
				else if (IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_CUSTOM))
					dopt->audio.new_rate = GetDlgItemInt(hDlg, IDC_SAMPLINGRATE_CUSTOM_VAL, NULL, FALSE);

				if		(IsDlgButtonChecked(hDlg, IDC_PRECISION_NOCHANGE)) dopt->audio.newPrecision = DubAudioOptions::P_NOCHANGE;
				else if	(IsDlgButtonChecked(hDlg, IDC_PRECISION_8BIT    )) dopt->audio.newPrecision = DubAudioOptions::P_8BIT;
				else if	(IsDlgButtonChecked(hDlg, IDC_PRECISION_16BIT   )) dopt->audio.newPrecision = DubAudioOptions::P_16BIT;

				if		(IsDlgButtonChecked(hDlg, IDC_CHANNELS_NOCHANGE)) dopt->audio.newChannels = DubAudioOptions::C_NOCHANGE;
				else if	(IsDlgButtonChecked(hDlg, IDC_CHANNELS_MONO    )) dopt->audio.newChannels = DubAudioOptions::C_MONO;
				else if	(IsDlgButtonChecked(hDlg, IDC_CHANNELS_STEREO  )) dopt->audio.newChannels = DubAudioOptions::C_STEREO;
				else if	(IsDlgButtonChecked(hDlg, IDC_CHANNELS_LEFT    )) dopt->audio.newChannels = DubAudioOptions::C_MONOLEFT;
				else if	(IsDlgButtonChecked(hDlg, IDC_CHANNELS_RIGHT   )) dopt->audio.newChannels = DubAudioOptions::C_MONORIGHT;

				dopt->audio.integral_rate = !!IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_INTEGRAL);
				dopt->audio.fHighQuality = !!IsDlgButtonChecked(hDlg, IDC_SAMPLINGRATE_HQ);

				EndDialog(hDlg, TRUE);
				return TRUE;
			case IDCANCEL:
				EndDialog(hDlg, FALSE);
				return TRUE;
			}
            break;

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					HelpPopupByID(hDlg, lphi->iCtrlId, dwAudioConversionHelpLookup);
			}
			return TRUE;
    }
    return FALSE;
}

///////////////////////////////////////////

void AudioInterleaveDlgEnableStuff(HWND hDlg, BOOL en) {
	EnableWindow(GetDlgItem(hDlg, IDC_PRELOAD), en);
	EnableWindow(GetDlgItem(hDlg, IDC_INTERVAL), en);
	EnableWindow(GetDlgItem(hDlg, IDC_FRAMES), en);
	EnableWindow(GetDlgItem(hDlg, IDC_MS), en);
}

BOOL APIENTRY AudioInterleaveDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
	DubOptions *dopt = (DubOptions *)GetWindowLong(hDlg, DWL_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			SetWindowLong(hDlg, DWL_USER, lParam);
			dopt = (DubOptions *)lParam;

			CheckDlgButton(hDlg, IDC_INTERLEAVE, dopt->audio.enabled);
			AudioInterleaveDlgEnableStuff(hDlg, dopt->audio.enabled);
//			if (dopt->audio.enabled) {
				SetDlgItemInt(hDlg, IDC_PRELOAD, dopt->audio.preload, FALSE);
				SetDlgItemInt(hDlg, IDC_INTERVAL, dopt->audio.interval, FALSE);
				CheckDlgButton(hDlg, IDC_FRAMES, !dopt->audio.is_ms);
				CheckDlgButton(hDlg, IDC_MS, dopt->audio.is_ms);
//			}
			SetDlgItemInt(hDlg, IDC_DISPLACEMENT, dopt->audio.offset, TRUE);
            return (TRUE);

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_INTERLEAVE:
				AudioInterleaveDlgEnableStuff(hDlg, IsDlgButtonChecked(hDlg, IDC_INTERLEAVE));
				break;

			case IDOK:
				dopt->audio.enabled = !!IsDlgButtonChecked(hDlg, IDC_INTERLEAVE);

				if (dopt->audio.enabled) {
					dopt->audio.preload = GetDlgItemInt(hDlg, IDC_PRELOAD, NULL, TRUE);
					if (dopt->audio.preload<0 || dopt->audio.preload>60000) {
						SetFocus(GetDlgItem(hDlg, IDC_PRELOAD));
						MessageBeep(MB_ICONQUESTION);
						break;
					}

					dopt->audio.interval = GetDlgItemInt(hDlg, IDC_INTERVAL, NULL, TRUE);
					if (dopt->audio.interval<0 || dopt->audio.interval>3600000) {
						SetFocus(GetDlgItem(hDlg, IDC_INTERVAL));
						MessageBeep(MB_ICONQUESTION);
						break;
					}

					dopt->audio.is_ms = !!IsDlgButtonChecked(hDlg, IDC_MS);
				}

				dopt->audio.offset = GetDlgItemInt(hDlg, IDC_DISPLACEMENT, NULL, TRUE);

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

/////////////////////////////////

static DWORD dwVideoDepthHelpLookup[]={
	IDC_INPUT_16BIT,			IDH_DLG_VDEPTH_INPUT,
	IDC_INPUT_24BIT,			IDH_DLG_VDEPTH_INPUT,
	IDC_OUTPUT_16BIT,			IDH_DLG_VDEPTH_OUTPUT,
	IDC_OUTPUT_24BIT,			IDH_DLG_VDEPTH_OUTPUT,
	IDC_OUTPUT_32BIT,			IDH_DLG_VDEPTH_OUTPUT,
	0,0
};

BOOL APIENTRY VideoDepthDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
	DubOptions *dopt = (DubOptions *)GetWindowLong(hDlg, DWL_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			SetWindowLong(hDlg, DWL_USER, lParam);
			dopt = (DubOptions *)lParam;

			CheckDlgButton(hDlg, IDC_INPUT_16BIT, dopt->video.inputDepth == DubVideoOptions::D_16BIT);
			CheckDlgButton(hDlg, IDC_INPUT_24BIT, dopt->video.inputDepth != DubVideoOptions::D_16BIT);
			CheckDlgButton(hDlg, IDC_OUTPUT_16BIT, dopt->video.outputDepth == DubVideoOptions::D_16BIT);
			CheckDlgButton(hDlg, IDC_OUTPUT_24BIT, dopt->video.outputDepth == DubVideoOptions::D_24BIT);
			CheckDlgButton(hDlg, IDC_OUTPUT_32BIT, dopt->video.outputDepth == DubVideoOptions::D_32BIT);
            return (TRUE);

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					HelpPopupByID(hDlg, lphi->iCtrlId, dwVideoDepthHelpLookup);
			}
			return TRUE;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				dopt->video.inputDepth = IsDlgButtonChecked(hDlg, IDC_INPUT_24BIT) ? DubVideoOptions::D_24BIT : DubVideoOptions::D_16BIT;

				if (IsDlgButtonChecked(hDlg, IDC_OUTPUT_16BIT)) dopt->video.outputDepth = DubVideoOptions::D_16BIT;
				if (IsDlgButtonChecked(hDlg, IDC_OUTPUT_24BIT)) dopt->video.outputDepth = DubVideoOptions::D_24BIT;
				if (IsDlgButtonChecked(hDlg, IDC_OUTPUT_32BIT)) dopt->video.outputDepth = DubVideoOptions::D_32BIT;
				
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
//	Performance dialog
//
///////////////////////////////////////////////////////////////////////////

static long outputBufferSizeArray[]={
	128*1024,
	192*1024,
	256*1024,
	512*1024,
	768*1024,
	1*1024*1024,
	2*1024*1024,
	3*1024*1024,
	4*1024*1024,
	6*1024*1024,
	8*1024*1024,
	12*1024*1024,
	16*1024*1024,
	20*1024*1024,
	24*1024*1024,
	32*1024*1024,
	48*1024*1024,
	64*1024*1024,
};

static long waveBufferSizeArray[]={
	8*1024,
	12*1024,
	16*1024,
	24*1024,
	32*1024,
	48*1024,
	64*1024,
	96*1024,
	128*1024,
	192*1024,
	256*1024,
	384*1024,
	512*1024,
	768*1024,
	1024*1024,
	1536*1024,
	2048*1024,
	3*1024*1024,
	4*1024*1024,
	6*1024*1024,
	8*1024*1024
};

static long pipeBufferCountArray[]={
	4,
	6,
	8,
	12,
	16,
	24,
	32,
	48,
	64,
	96,
	128,
	192,
	256,
};

#define ELEMENTS(x) (sizeof (x)/sizeof(x)[0])

BOOL APIENTRY PerformanceOptionsDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
	DubOptions *dopt = (DubOptions *)GetWindowLong(hDlg, DWL_USER);
	LONG pos;
	HWND hWndItem;

    switch (message)
    {
        case WM_INITDIALOG:
			SetWindowLong(hDlg, DWL_USER, lParam);
			dopt = (DubOptions *)lParam;

			hWndItem = GetDlgItem(hDlg, IDC_OUTPUT_BUFFER);
			SendMessage(hWndItem, TBM_SETRANGE, FALSE, MAKELONG(0,sizeof outputBufferSizeArray / sizeof outputBufferSizeArray[0] - 1));
			SendMessage(hWndItem, TBM_SETPOS, TRUE, NearestLongValue(dopt->perf.outputBufferSize, outputBufferSizeArray, ELEMENTS(outputBufferSizeArray)));
			SendMessage(hDlg, WM_HSCROLL, 0, (LPARAM)hWndItem);
			hWndItem = GetDlgItem(hDlg, IDC_WAVE_INPUT_BUFFER);
			SendMessage(hWndItem, TBM_SETRANGE, FALSE, MAKELONG(0,sizeof waveBufferSizeArray / sizeof waveBufferSizeArray[0] - 1));
			SendMessage(hWndItem, TBM_SETPOS, TRUE, NearestLongValue(dopt->perf.waveBufferSize, waveBufferSizeArray, ELEMENTS(waveBufferSizeArray)));
			SendMessage(hDlg, WM_HSCROLL, 0, (LPARAM)hWndItem);
			hWndItem = GetDlgItem(hDlg, IDC_PIPE_BUFFERS);
			SendMessage(hWndItem, TBM_SETRANGE, FALSE, MAKELONG(0,sizeof pipeBufferCountArray / sizeof pipeBufferCountArray[0] - 1));
			SendMessage(hWndItem, TBM_SETPOS, TRUE, NearestLongValue(dopt->perf.pipeBufferCount, pipeBufferCountArray, ELEMENTS(pipeBufferCountArray)));
			SendMessage(hDlg, WM_HSCROLL, 0, (LPARAM)hWndItem);
            return (TRUE);

		case WM_HSCROLL:

			pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);

			switch(GetWindowLong((HWND)lParam, GWL_ID)) {
			case IDC_OUTPUT_BUFFER:
				if (pos >= 5)
					wsprintf(g_msgBuf, "VirtualDub will use %ldMb of memory for output buffering.",outputBufferSizeArray[pos]>>20);
				else
					wsprintf(g_msgBuf, "VirtualDub will use %ldk of memory for output buffering.",outputBufferSizeArray[pos]>>10);

				SetDlgItemText(hDlg, IDC_OUTPUT_BUFFER_SIZE, g_msgBuf);
				return TRUE;

			case IDC_WAVE_INPUT_BUFFER:
				if (pos >= 14)
					wsprintf(g_msgBuf, "Replacement WAV audio tracks will use %ldMb of memory for input buffering.",waveBufferSizeArray[pos]>>20);
				else
					wsprintf(g_msgBuf, "Replacement WAV audio tracks will use %ldk of memory for input buffering.",waveBufferSizeArray[pos]>>10);

				SetDlgItemText(hDlg, IDC_WAVE_BUFFER_SIZE, g_msgBuf);
				return TRUE;

			case IDC_PIPE_BUFFERS:
				wsprintf(g_msgBuf, "Pipelining will be limited to %ld buffers.\n", pipeBufferCountArray[pos]);
				SetDlgItemText(hDlg, IDC_STATIC_PIPE_BUFFERS, g_msgBuf);
				return TRUE;
			}
			break;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				{
					long index;

					index = SendMessage(GetDlgItem(hDlg, IDC_OUTPUT_BUFFER), TBM_GETPOS, 0, 0);
					dopt->perf.outputBufferSize = outputBufferSizeArray[index];

					index = SendMessage(GetDlgItem(hDlg, IDC_WAVE_INPUT_BUFFER), TBM_GETPOS, 0, 0);
					dopt->perf.waveBufferSize = waveBufferSizeArray[index];

					index = SendMessage(GetDlgItem(hDlg, IDC_PIPE_BUFFERS), TBM_GETPOS, 0, 0);
					dopt->perf.pipeBufferCount = pipeBufferCountArray[index];
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

BOOL APIENTRY DynamicCompileOptionsDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
	DubOptions *dopt = (DubOptions *)GetWindowLong(hDlg, DWL_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			SetWindowLong(hDlg, DWL_USER, lParam);
			dopt = (DubOptions *)lParam;

			CheckDlgButton(hDlg, IDC_ENABLE, dopt->perf.dynamicEnable);
			CheckDlgButton(hDlg, IDC_DISPLAY_CODE, dopt->perf.dynamicShowDisassembly);

            return (TRUE);

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				dopt->perf.dynamicEnable = !!IsDlgButtonChecked(hDlg, IDC_ENABLE);
				dopt->perf.dynamicShowDisassembly = !!IsDlgButtonChecked(hDlg, IDC_DISPLAY_CODE);
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

//////////////////////////////////////////////

static DWORD dwVideoFramerateHelpLookup[]={
	IDC_DECIMATE_1,			IDH_DLG_VFR_DECIMATION,
	IDC_DECIMATE_2,			IDH_DLG_VFR_DECIMATION,
	IDC_DECIMATE_3,			IDH_DLG_VFR_DECIMATION,
	IDC_DECIMATE_N,			IDH_DLG_VFR_DECIMATION,
	IDC_DECIMATE_VALUE,		IDH_DLG_VFR_DECIMATION,
	IDC_FRAMERATE,			IDH_DLG_VFR_FRAMERATE,
	IDC_FRAMERATE_CHANGE,	IDH_DLG_VFR_FRAMERATE,
	IDC_FRAMERATE_NOCHANGE,	IDH_DLG_VFR_FRAMERATE,
	0,0
};

static void VideoDecimationRedoIVTCEnables(HWND hDlg) {
	bool f3, f4;
	BOOL e;

	f3 = !!IsDlgButtonChecked(hDlg, IDC_IVTC_RECONFIELDSFIXED);
	f4 = !!IsDlgButtonChecked(hDlg, IDC_IVTC_RECONFRAMESMANUAL);

	e = f3 || f4;

	EnableWindow(GetDlgItem(hDlg, IDC_STATIC_IVTCOFFSET), e);
	EnableWindow(GetDlgItem(hDlg, IDC_IVTCOFFSET), e);
	EnableWindow(GetDlgItem(hDlg, IDC_INVPOLARITY), e);
}

BOOL APIENTRY VideoDecimationDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
	DubOptions *dopt = (DubOptions *)GetWindowLong(hDlg, DWL_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			SetWindowLong(hDlg, DWL_USER, lParam);
			dopt = (DubOptions *)lParam;

			SetAudioSource();

			CheckDlgButton(hDlg, IDC_INVTELECINE, dopt->video.fInvTelecine);

			if (dopt->video.fInvTelecine) {
				EnableWindow(GetDlgItem(hDlg, IDC_DECIMATE_1), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_DECIMATE_2), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_DECIMATE_3), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_DECIMATE_N), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_DECIMATE_VALUE), FALSE);
			}

			CheckDlgButton(hDlg, IDC_DECIMATE_1, dopt->video.frameRateDecimation==1);
			CheckDlgButton(hDlg, IDC_DECIMATE_2, dopt->video.frameRateDecimation==2);
			CheckDlgButton(hDlg, IDC_DECIMATE_3, dopt->video.frameRateDecimation==3);
			CheckDlgButton(hDlg, IDC_DECIMATE_N, dopt->video.frameRateDecimation>3);
			if (dopt->video.frameRateDecimation>3)
				SetDlgItemInt(hDlg, IDC_DECIMATE_VALUE, dopt->video.frameRateDecimation, FALSE);
			else
				EnableWindow(GetDlgItem(hDlg, IDC_DECIMATE_VALUE), FALSE);

			if (inputVideoAVI) {
				sprintf(g_msgBuf, "No change (current: %.3f fps)", (double)inputVideoAVI->streamInfo.dwRate / inputVideoAVI->streamInfo.dwScale);
				SetDlgItemText(hDlg, IDC_FRAMERATE_NOCHANGE, g_msgBuf);

				if (inputAudio && inputAudio->streamInfo.dwLength) {
					sprintf(g_msgBuf, "(%.3f fps)", (inputVideoAVI->streamInfo.dwLength*1000.0) / inputAudio->samplesToMs(inputAudio->streamInfo.dwLength));
					SetDlgItemText(hDlg, IDC_FRAMERATE_SAMELENGTH_VALUE, g_msgBuf);
				} else
					EnableWindow(GetDlgItem(hDlg, IDC_FRAMERATE_SAMELENGTH), FALSE);
			}

			if (dopt->video.frameRateNewMicroSecs == DubVideoOptions::FR_SAMELENGTH) {
				if (!inputAudio)
					CheckDlgButton(hDlg, IDC_FRAMERATE_NOCHANGE, TRUE);
				else
					CheckDlgButton(hDlg, IDC_FRAMERATE_SAMELENGTH, TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_FRAMERATE), FALSE);
			} else if (dopt->video.frameRateNewMicroSecs) {
				sprintf(g_msgBuf, "%.3f", 1000000.0/dopt->video.frameRateNewMicroSecs);
				SetDlgItemText(hDlg, IDC_FRAMERATE, g_msgBuf);
				CheckDlgButton(hDlg, IDC_FRAMERATE_CHANGE, TRUE);
			} else {
				CheckDlgButton(hDlg, IDC_FRAMERATE_NOCHANGE, TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_FRAMERATE), FALSE);
			}

			if (dopt->video.fInvTelecine) {
				if (dopt->video.fIVTCMode)
					CheckDlgButton(hDlg, IDC_IVTC_RECONFRAMESMANUAL, TRUE);
				else if (dopt->video.nIVTCOffset<0)
					CheckDlgButton(hDlg, IDC_IVTC_RECONFIELDS, TRUE);
				else
					CheckDlgButton(hDlg, IDC_IVTC_RECONFIELDSFIXED, TRUE);
			} else
				CheckDlgButton(hDlg, IDC_IVTC_OFF, TRUE);

			SetDlgItemInt(hDlg, IDC_IVTCOFFSET, dopt->video.nIVTCOffset<0 ? 1 : dopt->video.nIVTCOffset, FALSE);
			CheckDlgButton(hDlg, IDC_INVPOLARITY, dopt->video.fIVTCPolarity);

			VideoDecimationRedoIVTCEnables(hDlg);

            return (TRUE);

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					HelpPopupByID(hDlg, lphi->iCtrlId, dwVideoFramerateHelpLookup);
			}
			return TRUE;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_DECIMATE_1:
			case IDC_DECIMATE_2:
			case IDC_DECIMATE_3:
				EnableWindow(GetDlgItem(hDlg, IDC_DECIMATE_VALUE), FALSE);
				break;
			case IDC_DECIMATE_N:
				EnableWindow(GetDlgItem(hDlg, IDC_DECIMATE_VALUE), TRUE);
				SetFocus(GetDlgItem(hDlg, IDC_DECIMATE_VALUE));
				break;

			case IDC_FRAMERATE_CHANGE:
				if (SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED)
					EnableWindow(GetDlgItem(hDlg, IDC_FRAMERATE),TRUE);
				break;

			case IDC_FRAMERATE_SAMELENGTH:
			case IDC_FRAMERATE_NOCHANGE:
				if (SendMessage((HWND)lParam, BM_GETSTATE, 0, 0) & BST_CHECKED)
					EnableWindow(GetDlgItem(hDlg, IDC_FRAMERATE),FALSE);
				break;

			case IDC_IVTC_OFF:
			case IDC_IVTC_RECONFIELDS:
			case IDC_IVTC_RECONFIELDSFIXED:
			case IDC_IVTC_RECONFRAMESMANUAL:
				{
					BOOL f = IsDlgButtonChecked(hDlg, IDC_IVTC_OFF);

					EnableWindow(GetDlgItem(hDlg, IDC_DECIMATE_1), f);
					EnableWindow(GetDlgItem(hDlg, IDC_DECIMATE_2), f);
					EnableWindow(GetDlgItem(hDlg, IDC_DECIMATE_3), f);
					EnableWindow(GetDlgItem(hDlg, IDC_DECIMATE_N), f);
					EnableWindow(GetDlgItem(hDlg, IDC_DECIMATE_VALUE), f && IsDlgButtonChecked(hDlg, IDC_DECIMATE_N));
					VideoDecimationRedoIVTCEnables(hDlg);
				}
				break;

			case IDOK:
				{
					int newFRD;

					if (IsDlgButtonChecked(hDlg, IDC_DECIMATE_N)) {
						LONG lv = GetDlgItemInt(hDlg, IDC_DECIMATE_VALUE, NULL, TRUE);

						if (lv<1) {
							SetFocus(GetDlgItem(hDlg, IDC_DECIMATE_VALUE));
							MessageBeep(MB_ICONQUESTION);
							return FALSE;
						}

						newFRD = lv;
					} else if (IsDlgButtonChecked(hDlg, IDC_DECIMATE_1))
						newFRD = 1;
					else if (IsDlgButtonChecked(hDlg, IDC_DECIMATE_2))
						newFRD = 2;
					else if (IsDlgButtonChecked(hDlg, IDC_DECIMATE_3))
						newFRD = 3;

					if (IsDlgButtonChecked(hDlg, IDC_FRAMERATE_CHANGE)) {
						double newFR;

						GetDlgItemText(hDlg, IDC_FRAMERATE, g_msgBuf, 64);
						newFR = atof(g_msgBuf);

						if (newFR<=0.0 || newFR>=200.0) {
							SetFocus(GetDlgItem(hDlg, IDC_FRAMERATE));
							MessageBeep(MB_ICONQUESTION);
							return FALSE;
						}

						dopt->video.frameRateNewMicroSecs = (long)(1000000.0/newFR + .5);
					} else if (IsDlgButtonChecked(hDlg, IDC_FRAMERATE_SAMELENGTH)) {
						dopt->video.frameRateNewMicroSecs = DubVideoOptions::FR_SAMELENGTH;
					} else dopt->video.frameRateNewMicroSecs = 0;

					dopt->video.frameRateDecimation = newFRD;

					if (IsDlgButtonChecked(hDlg, IDC_IVTC_RECONFIELDS)) {
						dopt->video.fInvTelecine = true;
						dopt->video.fIVTCMode = false;
						dopt->video.nIVTCOffset = -1;
						dopt->video.frameRateDecimation = 1;
					} else if (IsDlgButtonChecked(hDlg, IDC_IVTC_RECONFIELDSFIXED)) {
						BOOL fSuccess;
						LONG lv = GetDlgItemInt(hDlg, IDC_IVTCOFFSET, &fSuccess, FALSE);

						dopt->video.fInvTelecine = true;
						dopt->video.fIVTCMode = false;
						dopt->video.nIVTCOffset = lv % 5;
						dopt->video.fIVTCPolarity = !!IsDlgButtonChecked(hDlg, IDC_INVPOLARITY);
						dopt->video.frameRateDecimation = 1;
					} else if (IsDlgButtonChecked(hDlg, IDC_IVTC_RECONFRAMESMANUAL)) {
						BOOL fSuccess;
						LONG lv = GetDlgItemInt(hDlg, IDC_IVTCOFFSET, &fSuccess, FALSE);

						dopt->video.fInvTelecine = true;
						dopt->video.fIVTCMode = true;
						dopt->video.nIVTCOffset = lv % 5;
						dopt->video.fIVTCPolarity = !!IsDlgButtonChecked(hDlg, IDC_INVPOLARITY);
						dopt->video.frameRateDecimation = 1;
					} else {
						dopt->video.fInvTelecine = false;
					}
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

///////////////////////////////////////////

static BOOL videoClippingDlgEditReentry=FALSE;

static void VideoClippingDlgMSToFrames(HWND hDlg, UINT idFrames, UINT idMS) {
	LONG lv,lFrames;
	BOOL ok;

	if (!inputVideoAVI) return;

	lv = GetDlgItemInt(hDlg, idMS, &ok, FALSE);
	if (!ok) return;
	videoClippingDlgEditReentry = TRUE;
	SetDlgItemInt(hDlg, idFrames, lFrames=inputVideoAVI->msToSamples(lv), FALSE);
	SetDlgItemInt(hDlg, IDC_LENGTH_MS,
				inputVideoAVI->samplesToMs(inputVideoAVI->lSampleLast - inputVideoAVI->lSampleFirst)
				-GetDlgItemInt(hDlg, IDC_END_MS, NULL, FALSE)
				-GetDlgItemInt(hDlg, IDC_START_MS, NULL, FALSE), TRUE);
	SetDlgItemInt(hDlg, IDC_LENGTH_FRAMES,
				inputVideoAVI->lSampleLast - inputVideoAVI->lSampleFirst
				-GetDlgItemInt(hDlg, IDC_END_FRAMES, NULL, FALSE)
				-GetDlgItemInt(hDlg, IDC_START_FRAMES, NULL, FALSE), TRUE);
	videoClippingDlgEditReentry = FALSE;
}

static void VideoClippingDlgFramesToMS(HWND hDlg, UINT idMS, UINT idFrames) {
	LONG lv, lMS;
	BOOL ok;

	if (!inputVideoAVI) return;

	lv = GetDlgItemInt(hDlg, idFrames, &ok, FALSE);
	if (!ok) return;
	videoClippingDlgEditReentry = TRUE;
	SetDlgItemInt(hDlg, idMS, lMS = inputVideoAVI->samplesToMs(lv), FALSE);
	SetDlgItemInt(hDlg, IDC_LENGTH_MS,
				inputVideoAVI->samplesToMs(inputVideoAVI->lSampleLast - inputVideoAVI->lSampleFirst)
				-GetDlgItemInt(hDlg, IDC_END_MS, NULL, FALSE)
				-GetDlgItemInt(hDlg, IDC_START_MS, NULL, FALSE), TRUE);
	SetDlgItemInt(hDlg, IDC_LENGTH_FRAMES,
				inputVideoAVI->streamInfo.dwLength
				-GetDlgItemInt(hDlg, IDC_END_FRAMES, NULL, FALSE)
				-GetDlgItemInt(hDlg, IDC_START_FRAMES, NULL, FALSE), TRUE);
	videoClippingDlgEditReentry = FALSE;
}

static void VideoClippingDlgLengthFrames(HWND hDlg) {
	LONG lv, lMS;
	BOOL ok;

	if (!inputVideoAVI) return;

	lv = GetDlgItemInt(hDlg, IDC_LENGTH_FRAMES, &ok, TRUE);
	if (!ok) return;
	videoClippingDlgEditReentry = TRUE;
	SetDlgItemInt(hDlg, IDC_LENGTH_MS, lMS = inputVideoAVI->samplesToMs(lv), FALSE);
	SetDlgItemInt(hDlg, IDC_END_MS,
				inputVideoAVI->samplesToMs(inputVideoAVI->streamInfo.dwLength)
				-lMS
				-GetDlgItemInt(hDlg, IDC_START_MS, NULL, TRUE), TRUE);
	SetDlgItemInt(hDlg, IDC_END_FRAMES,
				inputVideoAVI->streamInfo.dwLength
				-lv
				-GetDlgItemInt(hDlg, IDC_START_FRAMES, NULL, TRUE), TRUE);
	videoClippingDlgEditReentry = FALSE;
}

static void VideoClippingDlgLengthMS(HWND hDlg) {
	LONG lv,lFrames;
	BOOL ok;

	if (!inputVideoAVI) return;

	lv = GetDlgItemInt(hDlg, IDC_LENGTH_MS, &ok, TRUE);
	if (!ok) return;
	videoClippingDlgEditReentry = TRUE;
	SetDlgItemInt(hDlg, IDC_LENGTH_FRAMES, lFrames=inputVideoAVI->msToSamples(lv), FALSE);
	SetDlgItemInt(hDlg, IDC_END_MS,
				inputVideoAVI->samplesToMs(inputVideoAVI->streamInfo.dwLength)
				-lv
				-GetDlgItemInt(hDlg, IDC_START_MS, NULL, TRUE), TRUE);
	SetDlgItemInt(hDlg, IDC_END_FRAMES,
				inputVideoAVI->streamInfo.dwLength
				-lFrames
				-GetDlgItemInt(hDlg, IDC_START_FRAMES, NULL, TRUE), TRUE);
	videoClippingDlgEditReentry = FALSE;
}

static DWORD dwVideoClippingHelpLookup[]={
	IDC_START_MS,				IDH_DLG_VCLIP_RANGES,
	IDC_LENGTH_MS,				IDH_DLG_VCLIP_RANGES,
	IDC_END_MS,					IDH_DLG_VCLIP_RANGES,
	IDC_START_FRAMES,			IDH_DLG_VCLIP_RANGES,
	IDC_LENGTH_FRAMES,			IDH_DLG_VCLIP_RANGES,
	IDC_END_FRAMES,				IDH_DLG_VCLIP_RANGES,
	IDC_OFFSET_AUDIO,			IDH_DLG_VCLIP_OFFSETAUDIO,
	IDC_CLIP_AUDIO,				IDH_DLG_VCLIP_CLIPAUDIO,
	0,0
};

BOOL APIENTRY VideoClippingDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam)
{
	DubOptions *dopt = (DubOptions *)GetWindowLong(hDlg, DWL_USER);

    switch (message)
    {
        case WM_INITDIALOG:
			SetWindowLong(hDlg, DWL_USER, lParam);
			dopt = (DubOptions *)lParam;

			EnableWindow(GetDlgItem(hDlg, IDC_LENGTH_MS), !!inputVideoAVI);
			EnableWindow(GetDlgItem(hDlg, IDC_START_FRAMES), !!inputVideoAVI);
			EnableWindow(GetDlgItem(hDlg, IDC_LENGTH_FRAMES), !!inputVideoAVI);
			EnableWindow(GetDlgItem(hDlg, IDC_END_FRAMES), !!inputVideoAVI);
			SetDlgItemInt(hDlg, IDC_START_MS, dopt->video.lStartOffsetMS, FALSE);
			SetDlgItemInt(hDlg, IDC_END_MS, dopt->video.lEndOffsetMS, FALSE);
			CheckDlgButton(hDlg, IDC_OFFSET_AUDIO, dopt->audio.fStartAudio);
			CheckDlgButton(hDlg, IDC_CLIP_AUDIO, dopt->audio.fEndAudio);
            return (TRUE);

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					HelpPopupByID(hDlg, lphi->iCtrlId, dwVideoClippingHelpLookup);
			}
			return TRUE;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDC_START_MS:
				if (HIWORD(wParam)==EN_CHANGE && !videoClippingDlgEditReentry)
					VideoClippingDlgMSToFrames(hDlg, IDC_START_FRAMES, IDC_START_MS);
				break;
			case IDC_START_FRAMES:
				if (HIWORD(wParam)==EN_CHANGE && !videoClippingDlgEditReentry)
					VideoClippingDlgFramesToMS(hDlg, IDC_START_MS, IDC_START_FRAMES);
				break;
			case IDC_END_MS:
				if (HIWORD(wParam)==EN_CHANGE && !videoClippingDlgEditReentry)
					VideoClippingDlgMSToFrames(hDlg, IDC_END_FRAMES, IDC_END_MS);
				break;
			case IDC_END_FRAMES:
				if (HIWORD(wParam)==EN_CHANGE && !videoClippingDlgEditReentry)
					VideoClippingDlgFramesToMS(hDlg, IDC_END_MS, IDC_END_FRAMES);
				break;
			case IDC_LENGTH_MS:
				if (HIWORD(wParam)==EN_CHANGE && !videoClippingDlgEditReentry)
					VideoClippingDlgLengthMS(hDlg);
				break;
			case IDC_LENGTH_FRAMES:
				if (HIWORD(wParam)==EN_CHANGE && !videoClippingDlgEditReentry)
					VideoClippingDlgLengthFrames(hDlg);
				break;
			case IDOK:
				dopt->video.lStartOffsetMS	= GetDlgItemInt(hDlg, IDC_START_MS, NULL, FALSE);
				dopt->video.lEndOffsetMS	= GetDlgItemInt(hDlg, IDC_END_MS, NULL, FALSE);
				dopt->audio.fStartAudio		= !!IsDlgButtonChecked(hDlg, IDC_OFFSET_AUDIO);
				dopt->audio.fEndAudio		= !!IsDlgButtonChecked(hDlg, IDC_CLIP_AUDIO);
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

BOOL APIENTRY AudioVolumeDlgProc( HWND hdlg, UINT message, UINT wParam, LONG lParam)
{
	DubOptions *dopt = (DubOptions *)GetWindowLong(hdlg, DWL_USER);
	static const double log2 = 0.69314718055994530941723212145818;

    switch (message)
    {
        case WM_INITDIALOG:
			SetWindowLong(hdlg, DWL_USER, lParam);
			dopt = (DubOptions *)lParam;

			{
				HWND hwndSlider = GetDlgItem(hdlg, IDC_SLIDER_VOLUME);

				SendMessage(hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 65));

				if (dopt->audio.volume) {
					CheckDlgButton(hdlg, IDC_ADJUSTVOL, BST_CHECKED);

					SendMessage(hwndSlider, TBM_SETPOS, TRUE, (int)(32.5 - 80.0 + log(dopt->audio.volume)/(log2/10.0)));

					AudioVolumeDlgProc(hdlg, WM_HSCROLL, 0, (LPARAM)hwndSlider);
				} else {
					SendMessage(hwndSlider, TBM_SETPOS, TRUE, 32);
					EnableWindow(GetDlgItem(hdlg, IDC_SLIDER_VOLUME), FALSE);
					EnableWindow(GetDlgItem(hdlg, IDC_STATIC_VOLUME), FALSE);
				}
			}
            return (TRUE);

		case WM_HELP:
			{
				HELPINFO *lphi = (HELPINFO *)lParam;

				if (lphi->iContextType == HELPINFO_WINDOW)
					HelpPopupByID(hdlg, lphi->iCtrlId, dwVideoClippingHelpLookup);
			}
			return TRUE;

        case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDOK:
				if (IsDlgButtonChecked(hdlg, IDC_ADJUSTVOL)) {
					int pos = SendDlgItemMessage(hdlg, IDC_SLIDER_VOLUME, TBM_GETPOS, 0, 0);

					dopt->audio.volume = (int)(0.5 + 256.0 * pow(2.0, (pos-32)/10.0));
				} else
					dopt->audio.volume = 0;

				EndDialog(hdlg, TRUE);
				return TRUE;
			case IDCANCEL:
				EndDialog(hdlg, FALSE);
				return TRUE;

			case IDC_ADJUSTVOL:
				if (HIWORD(wParam)==BN_CLICKED) {
					BOOL f = !!IsDlgButtonChecked(hdlg, IDC_ADJUSTVOL);

					EnableWindow(GetDlgItem(hdlg, IDC_SLIDER_VOLUME), f);
					EnableWindow(GetDlgItem(hdlg, IDC_STATIC_VOLUME), f);
				}
				return TRUE;
			}
            break;

		case WM_HSCROLL:
			if (lParam) {
				char buf[64];
				int pos = SendMessage((HWND)lParam, TBM_GETPOS, 0, 0);

				sprintf(buf, "%d%%", (int)(0.5 + 100.0*pow(2.0, (pos-32)/10.0)));
				SetDlgItemText(hdlg, IDC_STATIC_VOLUME, buf);
			}
			break;
    }
    return FALSE;
}

BOOL CALLBACK VideoJumpDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	char buf[32];

	switch(msg) {
	case WM_INITDIALOG:
		{
			long ticks = inputVideoAVI->samplesToMs(lParam);
			long ms, sec, min;

			CheckDlgButton(hdlg, IDC_JUMPTOFRAME, BST_CHECKED);
			SendDlgItemMessage(hdlg, IDC_FRAMETIME, EM_LIMITTEXT, 30, 0);
			SetDlgItemInt(hdlg, IDC_FRAMENUMBER, lParam, FALSE);
			SetFocus(GetDlgItem(hdlg, IDC_FRAMENUMBER));
			SendDlgItemMessage(hdlg, IDC_FRAMENUMBER, EM_SETSEL, 0, -1);

			ms  = ticks %1000; ticks /= 1000;
			sec	= ticks %  60; ticks /=  60;
			min	= ticks %  60; ticks /=  60;

			if (ticks)
				wsprintf(buf, "%d:%02d:%02d.%03d", ticks, min, sec, ms);
			else
				wsprintf(buf, "%d:%02d.%03d", min, sec, ms);

			SetDlgItemText(hdlg, IDC_FRAMETIME, buf);
		}
		return FALSE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDCANCEL:
			EndDialog(hdlg, -1);
			break;
		case IDOK:
			if (IsDlgButtonChecked(hdlg, IDC_JUMPTOFRAME)) {
				BOOL fOk;
				UINT uiFrame = GetDlgItemInt(hdlg, IDC_FRAMENUMBER, &fOk, FALSE);

				if (!fOk) {
					SetFocus(GetDlgItem(hdlg, IDC_FRAMENUMBER));
					MessageBeep(MB_ICONEXCLAMATION);
					return TRUE;
				}

				EndDialog(hdlg, uiFrame);
			} else {
				unsigned int hr, min, sec, ms=0;
				int n;

				GetDlgItemText(hdlg, IDC_FRAMETIME, buf, sizeof buf);

				n = sscanf(buf, "%u:%u:%u.%u", &hr, &min, &sec, &ms);

				if (n < 3) {
					hr = 0;
					n = sscanf(buf, "%u:%u.%u", &min, &sec, &ms);
				}

				if (n < 2) {
					min = 0;
					n = sscanf(buf, "%u.%u", &sec, &ms);
				}

				if (n < 1) {
					SetFocus(GetDlgItem(hdlg, IDC_FRAMETIME));
					MessageBeep(MB_ICONEXCLAMATION);
					return TRUE;
				}

				while(ms > 1000)
					ms /= 10;

				EndDialog(hdlg, inputVideoAVI->msToSamples(ms+sec*1000+min*60000+hr*3600000));
			}
			break;
		case IDC_FRAMENUMBER:
			if (HIWORD(wParam) == EN_CHANGE) {
				CheckDlgButton(hdlg, IDC_JUMPTOFRAME, BST_CHECKED);
				CheckDlgButton(hdlg, IDC_JUMPTOTIME, BST_UNCHECKED);
			}
			break;
		case IDC_FRAMETIME:
			if (HIWORD(wParam) == EN_CHANGE) {
				CheckDlgButton(hdlg, IDC_JUMPTOFRAME, BST_UNCHECKED);
				CheckDlgButton(hdlg, IDC_JUMPTOTIME, BST_CHECKED);
			}
			break;
		}
		return TRUE;
	}
	return FALSE;
}
