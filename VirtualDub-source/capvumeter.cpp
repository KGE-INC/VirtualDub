//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2000 Avery Lee
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

#include <crtdbg.h>
#include <math.h>
#include <windows.h>
#include <commctrl.h>
#include <vfw.h>

#include "resource.h"
#include "oshelper.h"
#include "helpfile.h"
#include "MonoBitmap.h"
#include "FHT.h"
#include "Error.h"


#define BALANCE_DEADZONE		(2048)

///////////////////////////////////////////////////////////////////////////
//
//	Volume meter
//
///////////////////////////////////////////////////////////////////////////

enum {
	VMMODE_VUMETER,
	VMMODE_SCOPE,
	VMMODE_ANALYZER,
};

typedef struct VumeterDlgData {
	RECT rect[2];
	LONG last[2], peak[2];
	void *buffer;
	char *scrollBuffer;
	int scrollSize;
	int bufCnt;
	WAVEFORMATEX wfex;
	HWAVEIN hWaveIn;
	WAVEHDR bufs[2];
	char *pBitmap;
	MonoBitmap *pbm;
	int mode;
	Fht *pfht_left, *pfht_right;
	HWND hwndVolume, hwndBalance;
	long lVolume, lBalance;
	MIXERCONTROLDETAILS mcdVolume;
	MIXERCONTROLDETAILS_UNSIGNED mcdVolumeData[2];

	BOOL fOpened;
	BOOL fRecording;
	BOOL fStereo;
	BOOL fOddByte;
} VumeterDlgData;

static void CaptureVumeterDestruct(VumeterDlgData *vdd) {
	if (vdd->fRecording) waveInReset(vdd->hWaveIn);

	while(vdd->bufCnt--) {
		--vdd->bufCnt;
		waveInUnprepareHeader(vdd->hWaveIn, &vdd->bufs[vdd->bufCnt], sizeof(WAVEHDR));
	}
	GdiFlush();
	delete vdd->pbm;
	delete vdd->pfht_left;
	delete vdd->pfht_right;
	if (vdd->fOpened) waveInClose(vdd->hWaveIn);
	if (vdd->buffer) free(vdd->buffer);
	free(vdd);
}

static void CaptureVumeterRepaintVumeter(VumeterDlgData *vdd, HDC hDC) {
	HBRUSH hbr, hbrp;
	int i;

	for(i=0; i<(vdd->fStereo?2:1); i++)
		Draw3DRect(hDC,
				vdd->rect[i].left,
				vdd->rect[i].top,
				vdd->rect[i].right  - vdd->rect[i].left,
				vdd->rect[i].bottom - vdd->rect[i].top,
				TRUE);

	if ((hbr = CreateSolidBrush(RGB(0x00,0x00,0xff))) && (hbrp = CreateSolidBrush(RGB(0xff,0x00,0x00)))) {
		RECT r;

		for(i=0; i<(vdd->fStereo?2:1); i++) {
			r.top		= vdd->rect[i].top+1;
			r.bottom	= vdd->rect[i].bottom-1;

			if (vdd->peak[i]) {
				r.left		= vdd->rect[i].left+1 + vdd->last[i];
				r.right		= vdd->rect[i].left+1 + vdd->peak[i];
				FillRect(hDC, &r, hbrp);
			}

			if (vdd->last[i]) {
				r.left		= vdd->rect[i].left+1;
				r.right		= vdd->rect[i].left+1 + vdd->last[i];
				FillRect(hDC, &r, hbr);
			}

			r.left		= vdd->rect[i].left+1 + vdd->peak[i];
			r.right		= vdd->rect[i].right-1;
			FillRect(hDC, &r, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
		}

	}
	if (hbr) DeleteObject(hbr);
	if (hbrp) DeleteObject(hbrp);
}

static void CaptureVumeterDoVumeter(VumeterDlgData *vdd, HDC hdc, unsigned long *total, unsigned long *peak, DWORD dwBytes) {
	HBRUSH hbr = CreateSolidBrush(RGB(0x00,0x00,0xff));
	HBRUSH hbrp = CreateSolidBrush(RGB(0xff,0x00,0x00));
	LONG lvl, plvl;
	RECT r;
	int i;

	for(i=0; i<(vdd->fStereo?2:1); i++) {
		lvl = MulDiv(total[i],vdd->rect[i].right-vdd->rect[i].left-2,dwBytes*128);
		plvl = MulDiv(peak[i],vdd->rect[i].right-vdd->rect[i].left-2,128);

		r.top		= vdd->rect[i].top+1;
		r.bottom	= vdd->rect[i].bottom-1;

		if (lvl < vdd->last[i] && hbrp) {
			r.left		= vdd->rect[i].left+1 + max(lvl, plvl);
			r.right		= vdd->rect[i].left+1 + min(vdd->peak[i], vdd->last[i]);
			FillRect(hdc, &r, hbrp);
		} else if (lvl > vdd->last[i] && hbr) {
			r.left		= vdd->rect[i].left+1 + vdd->last[i];
			r.right		= vdd->rect[i].left+1 + min(lvl, plvl);
			FillRect(hdc, &r, hbr);
		}

		if (plvl < vdd->peak[i]) {
			r.left		= vdd->rect[i].left+1 + plvl;
			r.right		= vdd->rect[i].left+1 + vdd->peak[i];
			FillRect(hdc, &r, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
		} else if (plvl > vdd->peak[i] && hbrp) {
			r.left		= vdd->rect[i].left+1 + max(lvl, vdd->peak[i]);
			r.right		= vdd->rect[i].left+1 + plvl;
			FillRect(hdc, &r, hbrp);
		}

		vdd->last[i] = lvl;
		vdd->peak[i] = plvl;
	}

	if (hbr) DeleteObject(hbr);
	if (hbrp) DeleteObject(hbrp);
}

static void CaptureVumeterRepaintScopeAnalyzer(VumeterDlgData *vdd, HDC hDC) {
	vdd->pbm->BitBlt(hDC, vdd->rect[0].left, vdd->rect[0].top, 0, 0, vdd->rect[0].right-vdd->rect[0].left, vdd->rect[0].bottom-vdd->rect[0].top);
	if (vdd->fStereo)
		vdd->pbm->BitBlt(hDC, vdd->rect[1].left, vdd->rect[1].top, 0, vdd->rect[0].bottom-vdd->rect[0].top, vdd->rect[1].right-vdd->rect[1].left, vdd->rect[1].bottom-vdd->rect[1].top);
}

static void CaptureVumeterDoScope(VumeterDlgData *vdd, HDC hDC) {
	int w = vdd->rect[0].right - vdd->rect[0].left;
	if (w > 4096) w = 4096;

	unsigned char *src = (unsigned char *)vdd->scrollBuffer + vdd->scrollSize - (vdd->fStereo ? w*2 : w);
	unsigned char *dst_col = (unsigned char *)vdd->pbm->getBits(), *dst;
	int hl = vdd->rect[0].bottom - vdd->rect[0].top;
	int hr = vdd->rect[1].bottom - vdd->rect[1].top;
	int h1, h2, h;
	unsigned char mask, c;
	int iPitch = vdd->pbm->getPitch();
	int left_offset = hl*iPitch;

	GdiFlush();
	vdd->pbm->Clear();

	mask = 0x80;
	do {
		c = *src++;

		h1 = (c * hl) >> 8;
		h2 = hl>>1;

		if (h1 < h2)
			h = h2+1-h1;
		else {
			h = h1+1-h2;
			h1 = h2;
		}

		dst = dst_col + left_offset + iPitch * h1;
		do {
			*dst |= mask;
			dst += iPitch;
		} while(--h);

		if (vdd->fStereo) {
			c = *src++;

			h1 = (c * hr) >> 8;
			h2 = hr>>1;

			if (h1 < h2)
				h = h2+1-h1;
			else {
				h = h1+1-h2;
				h1 = h2;
			}

			dst = dst_col + iPitch * h1;
			do {
				*dst |= mask;
				dst += iPitch;
			} while(--h);
		}

		if (!(mask>>=1)) {
			mask = 0x80;
			++dst_col;
		}
	} while(--w);

	CaptureVumeterRepaintScopeAnalyzer(vdd, hDC);
}

static void CaptureVumeterDoAnalyzer(VumeterDlgData *vdd, HDC hDC) {
	int w = vdd->rect[0].right - vdd->rect[0].left;
	if (w > 1024) w = 1024;

	unsigned char *src = (unsigned char *)vdd->scrollBuffer + vdd->scrollSize - (vdd->fStereo ? w*2 : w);
	unsigned char *dst_col = (unsigned char *)vdd->pbm->getBits(), *dst;
	int hl = vdd->rect[0].bottom - vdd->rect[0].top;
	int hr = vdd->rect[1].bottom - vdd->rect[1].top;
	int h1;
	int x=0;
	unsigned char mask;
	int iPitch = vdd->pbm->getPitch();
	int left_offset = hl*iPitch;

	if (vdd->fStereo) {
		vdd->pfht_left->CopyInStereo8((unsigned char *)vdd->scrollBuffer, 1024);
		vdd->pfht_left->Transform(w);
		vdd->pfht_right->CopyInStereo8((unsigned char *)vdd->scrollBuffer + 1, 1024);
		vdd->pfht_right->Transform(w);
	} else {
		vdd->pfht_left->CopyInMono8((unsigned char *)vdd->scrollBuffer, 1024);
		vdd->pfht_left->Transform(w);
	}

	GdiFlush();
	vdd->pbm->Clear();

	mask = 0x80;
	do {
		h1 = floor(0.5 + vdd->pfht_left->GetIntensity(x) * 8 * hl);
		if (h1 > hl) h1 = hl;

		if (h1>0) {
			dst = dst_col + left_offset;
			do {
				*dst |= mask;
				dst += iPitch;
			} while(--h1);
		}

		if (vdd->fStereo) {
			h1 = floor(0.5 + vdd->pfht_right->GetIntensity(x) * 8 * hr);
			if (h1 > hr) h1 = hr;

			if (h1>0) {
				dst = dst_col;
				do {
					*dst |= mask;
					dst += iPitch;
				} while(--h1);
			}
		}

		++x;

		if (!(mask>>=1)) {
			mask = 0x80;
			++dst_col;
		}
	} while(--w);

	CaptureVumeterRepaintScopeAnalyzer(vdd, hDC);
}

static void CaptureVumeterSetupMixer(HWND hdlg, VumeterDlgData *vdd) {
	MMRESULT res;
	UINT mixerID;
	MIXERCAPS mixerCaps;
	MIXERLINE mainMixerLine;
	MIXERLINE lineinMixerLine;
	MIXERLINECONTROLS mixerLineControls;
	MIXERCONTROL control;
	bool fVolumeOk = false;

	//
	// I have only this to say:
	//
	//		I hate the Windows mixer architecture.
	//
	// We have to use this braindead code because there's no good way
	// to get the mixer controls associated with a wave handle.  If you
	// give the mixer functions a wave handle, you always get the direct
	// line to that channel, and can't get to any of the sources...
	//

	mainMixerLine.cbStruct		= sizeof(MIXERLINE);

	_RPT0(0,"CaptureVumeterSetupMixer() begin\n");

	// Find the mixer line associated with the wave input device

	if (MMSYSERR_NOERROR != (res = mixerGetID((HMIXEROBJ)vdd->hWaveIn, &mixerID, MIXER_OBJECTF_HWAVEIN)))
		return;

	if (MMSYSERR_NOERROR != (res = mixerGetDevCaps(mixerID, &mixerCaps, sizeof(MIXERCAPS))))
		return;

	mainMixerLine.cbStruct					= sizeof(MIXERLINE);
	mainMixerLine.Target.dwType				= MIXERLINE_TARGETTYPE_WAVEIN;
	mainMixerLine.Target.wMid				= mixerCaps.wMid;
	mainMixerLine.Target.wPid				= mixerCaps.wPid;
	mainMixerLine.Target.vDriverVersion		= mixerCaps.vDriverVersion;
	strcpy(mainMixerLine.Target.szPname,mixerCaps.szPname);

	if (MMSYSERR_NOERROR == (res = mixerGetLineInfo((HMIXEROBJ)vdd->hWaveIn, &mainMixerLine, MIXER_OBJECTF_HWAVEIN))) {
		int src;

		_RPT1(0,"Found input line - %d connections\n", mainMixerLine.cConnections);

		// Look for the line-in line that feeds to us

		for(src = 0; src < mainMixerLine.cConnections; src++) {

			lineinMixerLine.cbStruct		= sizeof(MIXERLINE);
			lineinMixerLine.dwDestination	= mainMixerLine.dwDestination;
			lineinMixerLine.dwSource		= src;

			if (MMSYSERR_NOERROR == (res = mixerGetLineInfo((HMIXEROBJ)mixerID, &lineinMixerLine, MIXER_OBJECTF_MIXER| MIXER_GETLINEINFOF_SOURCE))
				&& (lineinMixerLine.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_AUXILIARY
					|| lineinMixerLine.dwComponentType == MIXERLINE_COMPONENTTYPE_SRC_LINE)) {

				_RPT0(0,"Found Line-In for this input device\n");

				vdd->mcdVolume.paDetails = vdd->mcdVolumeData;

				// Look for a volume control

				mixerLineControls.cbStruct		= sizeof mixerLineControls;
				mixerLineControls.dwLineID		= lineinMixerLine.dwLineID;
				mixerLineControls.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
				mixerLineControls.cbmxctrl		= sizeof(MIXERCONTROL);
				mixerLineControls.pamxctrl		= &control;

				if (MMSYSERR_NOERROR == (res = mixerGetLineControls((HMIXEROBJ)mixerID, &mixerLineControls, MIXER_GETLINECONTROLSF_ONEBYTYPE | MIXER_OBJECTF_MIXER))) {
					_RPT0(0,"Got the control.\n");

					vdd->mcdVolume.cbStruct			= sizeof(MIXERCONTROLDETAILS);
					vdd->mcdVolume.dwControlID		= control.dwControlID;
					vdd->mcdVolume.cChannels		= lineinMixerLine.cChannels==2 ? 2 : 1;
					vdd->mcdVolume.cMultipleItems	= 0;
					vdd->mcdVolume.cbDetails		= sizeof(MIXERCONTROLDETAILS_UNSIGNED);

					if (MMSYSERR_NOERROR == (res = mixerGetControlDetails((HMIXEROBJ)mixerID, &vdd->mcdVolume, MIXER_GETCONTROLDETAILSF_VALUE | MIXER_OBJECTF_MIXER)))
						fVolumeOk = true;

				}

				break;

			}
		}
	}

	if (fVolumeOk) {
		vdd->hwndVolume = GetDlgItem(hdlg, IDC_VOLUME);

		if (vdd->mcdVolume.cChannels > 1) {
			long l, r;

			l = vdd->mcdVolumeData[0].dwValue;
			r = vdd->mcdVolumeData[1].dwValue;

			vdd->lVolume = max(l,r);
			if (vdd->lVolume)
				vdd->lBalance = MulDiv(r-l, 32768-BALANCE_DEADZONE, vdd->lVolume);
			else
				vdd->lBalance = 0;
		} else {
			vdd->lVolume = vdd->mcdVolumeData[0].dwValue;
			vdd->lBalance = 0;
		}

		EnableWindow(vdd->hwndVolume, TRUE);
		SendMessage(vdd->hwndVolume, TBM_SETRANGEMIN, TRUE, 0);
		SendMessage(vdd->hwndVolume, TBM_SETRANGEMAX, TRUE, 65535);
		SendMessage(vdd->hwndVolume, TBM_SETPOS, TRUE, vdd->lVolume);

		SendMessage(vdd->hwndVolume, TBM_SETTICFREQ, 8192, 0);

		EnableWindow(GetDlgItem(hdlg, IDC_STATIC_VOLUME), TRUE);

		if (vdd->mcdVolume.cChannels>1) {
			vdd->hwndBalance = GetDlgItem(hdlg, IDC_BALANCE);

			EnableWindow(vdd->hwndBalance, TRUE);
			SendMessage(vdd->hwndBalance, TBM_SETRANGEMIN, TRUE, 0);
			SendMessage(vdd->hwndBalance, TBM_SETRANGEMAX, TRUE, 65535);

			if (vdd->lBalance<0)
				SendMessage(vdd->hwndBalance, TBM_SETPOS, TRUE, vdd->lBalance + 32768 - BALANCE_DEADZONE);
			else if (vdd->lBalance>0)
				SendMessage(vdd->hwndBalance, TBM_SETPOS, TRUE, vdd->lBalance + 32768 + BALANCE_DEADZONE);
			else
				SendMessage(vdd->hwndBalance, TBM_SETPOS, TRUE, 32768);

			SendMessage(vdd->hwndBalance, TBM_SETTIC, 0, 0);
			SendMessage(vdd->hwndBalance, TBM_SETTIC, 0, 32768 - BALANCE_DEADZONE);
			SendMessage(vdd->hwndBalance, TBM_SETTIC, 0, 32768 + BALANCE_DEADZONE);
			SendMessage(vdd->hwndBalance, TBM_SETTIC, 0, 65536);

			EnableWindow(GetDlgItem(hdlg, IDC_STATIC_BALANCE), TRUE);
		}
	}

	_RPT0(0,"CaptureVumeterSetupMixer() end\n");
}

BOOL APIENTRY CaptureVumeterDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	VumeterDlgData *vdd = (VumeterDlgData *)GetWindowLong(hDlg, DWL_USER);
	MMRESULT res;

	switch(message) {

		case WM_INITDIALOG:
			try {
				HWND hWndLeft, hWndRight;
				LONG fsize;
				WAVEFORMATEX *wf;

				////

				if (!(vdd = new VumeterDlgData)) throw MyError("Out of memory");
				memset(vdd, 0, sizeof(VumeterDlgData));

				if (fsize = capGetAudioFormatSize((HWND)lParam)) {
					if (wf = (WAVEFORMATEX *)malloc(fsize)) {
						if (capGetAudioFormat((HWND)lParam, wf, fsize)) {
							vdd->fStereo = wf->nChannels>1 ? TRUE : FALSE;
						}
						free(wf);
					}
				}

				if (!(vdd->buffer = malloc(vdd->fStereo?1024 + 2048 : 512 + 1024))) throw MyError("Out of memory");
				if (!(vdd->pfht_left = new Fht(1024, 11025))
					|| (vdd->fStereo && !(vdd->pfht_right = new Fht(1024, 11025))))
					throw MyMemoryError();

				vdd->scrollBuffer = (char *)vdd->buffer + (vdd->fStereo ? 1024 : 512);
				vdd->scrollSize = vdd->fStereo ? 2048 : 1024;

				SetWindowLong(hDlg, DWL_USER, (DWORD)vdd);

				GetWindowRect(hWndLeft = GetDlgItem(hDlg, IDC_VOLUME_LEFT), &vdd->rect[0]);
				GetWindowRect(hWndRight = GetDlgItem(hDlg, IDC_VOLUME_RIGHT), &vdd->rect[1]);

				if (!(vdd->pbm = new MonoBitmap(NULL, (vdd->rect[0].right - vdd->rect[0].left), (vdd->rect[0].bottom - vdd->rect[0].top) + (vdd->rect[1].bottom - vdd->rect[1].top), RGB(0x00, 0x80, 0xff), RGB(0,0,0))))
					throw MyMemoryError();

				ShowWindow(hWndLeft, SW_HIDE);
				ShowWindow(hWndRight, SW_HIDE);

				ScreenToClient(hDlg, (LPPOINT)&vdd->rect[0] + 0);
				ScreenToClient(hDlg, (LPPOINT)&vdd->rect[0] + 1);
				ScreenToClient(hDlg, (LPPOINT)&vdd->rect[1] + 0);
				ScreenToClient(hDlg, (LPPOINT)&vdd->rect[1] + 1);

				vdd->wfex.wFormatTag		= WAVE_FORMAT_PCM;
				vdd->wfex.nChannels			= vdd->fStereo?2:1;
				vdd->wfex.nSamplesPerSec	= 11025;
				vdd->wfex.nAvgBytesPerSec	= vdd->fStereo?22050:11025;
				vdd->wfex.nBlockAlign		= vdd->fStereo?2:1;
				vdd->wfex.wBitsPerSample	= 8;
				vdd->wfex.cbSize			= 0;

				if (MMSYSERR_NOERROR != (res = waveInOpen(&vdd->hWaveIn, WAVE_MAPPER, &vdd->wfex, (DWORD)hDlg, 0, CALLBACK_WINDOW)))
					throw MyError("MM system: error %ld", res);

				vdd->fOpened = TRUE;

				for(vdd->bufCnt=0; vdd->bufCnt<2; vdd->bufCnt++) {
					vdd->bufs[vdd->bufCnt].lpData			= (char *)vdd->buffer + (vdd->fStereo?512:256)*vdd->bufCnt;
					vdd->bufs[vdd->bufCnt].dwBufferLength	= vdd->fStereo?512:256;
					vdd->bufs[vdd->bufCnt].dwFlags			= NULL;

					if (MMSYSERR_NOERROR != (res = waveInPrepareHeader(vdd->hWaveIn,&vdd->bufs[vdd->bufCnt],sizeof(WAVEHDR))))
						throw MyError("MM system: error %ld", res);
					if (MMSYSERR_NOERROR != (res = waveInAddBuffer(vdd->hWaveIn,&vdd->bufs[vdd->bufCnt],sizeof(WAVEHDR))))
						throw MyError("MM system: error %ld", res);
				}

				CaptureVumeterSetupMixer(hDlg, vdd);

				if (MMSYSERR_NOERROR != (res = waveInStart(vdd->hWaveIn)))
					throw MyError("MM system: error %ld", res);

				vdd->fRecording = TRUE;
				vdd->mode = VMMODE_VUMETER;

				CheckDlgButton(hDlg, IDC_VMODE_VUMETER, BST_CHECKED);

			} catch(MyError e) {
				e.post(NULL,"Vumeter error");

				if (vdd) CaptureVumeterDestruct(vdd);

				EndDialog(hDlg, FALSE);
				return FALSE;
			}

			return TRUE;

		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hDC;

				hDC = BeginPaint(hDlg, &ps);

				if (vdd->mode == VMMODE_VUMETER)
					CaptureVumeterRepaintVumeter(vdd, hDC);
				else
					CaptureVumeterRepaintScopeAnalyzer(vdd, hDC);

				EndPaint(hDlg, &ps);
			}
			return TRUE;

		case WM_SYSCOMMAND:
			if ((wParam & 0xFFF0) == SC_CONTEXTHELP) {
				HelpPopup(hDlg, IDH_CAPTURE_VUMETER);
				return TRUE;
			}
			break;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {

			case IDOK:
			case IDCANCEL:
				if (vdd) CaptureVumeterDestruct(vdd);
				EndDialog(hDlg, FALSE);
				return TRUE;
			case IDC_VMODE_VUMETER:
				vdd->mode = VMMODE_VUMETER;
				InvalidateRect(hDlg, &vdd->rect[0], FALSE);
				InvalidateRect(hDlg, &vdd->rect[1], FALSE);
				return TRUE;
			case IDC_VMODE_OSCILLOSCOPE:
				vdd->mode = VMMODE_SCOPE;
				return TRUE;
			case IDC_VMODE_ANALYZER:
				vdd->mode = VMMODE_ANALYZER;
				return TRUE;
			}
			break;

		case WM_HSCROLL:
			if (!lParam) return FALSE;

			if ((HWND)lParam == vdd->hwndVolume || (HWND)lParam == vdd->hwndBalance) {
				if ((HWND)lParam == vdd->hwndVolume) {
					vdd->lVolume = SendMessage(vdd->hwndVolume, TBM_GETPOS, 0, 0);
				} else {
					long lPos;

					lPos = SendMessage(vdd->hwndBalance, TBM_GETPOS, 0, 0) - 32768;

					if (lPos < -BALANCE_DEADZONE)
						vdd->lBalance = lPos + BALANCE_DEADZONE;
					else if (lPos > BALANCE_DEADZONE)
						vdd->lBalance = lPos - BALANCE_DEADZONE;
					else
						vdd->lBalance = 0;
				}

				// compute L/R volume

				if (vdd->mcdVolume.cChannels == 1)
					vdd->mcdVolumeData[0].dwValue = vdd->lVolume;
				else if (vdd->lBalance < 0) {
					vdd->mcdVolumeData[0].dwValue = vdd->lVolume;
					vdd->mcdVolumeData[1].dwValue = MulDiv(vdd->lVolume, (32768-BALANCE_DEADZONE)+vdd->lBalance, 32768-BALANCE_DEADZONE);
				} else {
					vdd->mcdVolumeData[0].dwValue = MulDiv(vdd->lVolume, (32768-BALANCE_DEADZONE)-vdd->lBalance, 32768-BALANCE_DEADZONE);
					vdd->mcdVolumeData[1].dwValue =	vdd->lVolume;
				}
				mixerSetControlDetails(
						(HMIXEROBJ)vdd->hWaveIn,
						&vdd->mcdVolume,
						MIXER_GETCONTROLDETAILSF_VALUE | MIXER_OBJECTF_HWAVEIN);
			}
			return TRUE;

		case MM_WIM_DATA:
			{
				WAVEHDR *hdr = (WAVEHDR *)lParam;
				unsigned char c,*src = (unsigned char *)hdr->lpData;
				long dwBytes = hdr->dwBytesRecorded, len;

				if (dwBytes) {
					memmove(vdd->scrollBuffer, vdd->scrollBuffer + dwBytes, vdd->scrollSize - dwBytes);
					memcpy(vdd->scrollBuffer + vdd->scrollSize - dwBytes, src, dwBytes);
				}

				if (vdd->fStereo) {
					if (vdd->fOddByte) {
						++src;
						--dwBytes;
					}

					vdd->fOddByte = dwBytes & 1;

					dwBytes/=2;
				}
				if (len = dwBytes) {
					unsigned long total[2]={0,0}, peak[2]={0,0}, v;
					HDC hdc;

					if (!vdd->fStereo) {
						do {
							c = *src++;

							if (c>=0x80)	v = (c-0x80);
							else			v = (0x80-c);

							if (v>peak[0]) peak[0]=v;

							total[0] += v;

						} while(--len);
					} else {
						do {
							c = *src++;

							if (c>=0x80)	v = (c-0x80);
							else			v = (0x80-c);

							if (v>peak[0]) peak[0]=v;

							total[0] += v;

							//////

							c = *src++;

							if (c>=0x80)	v = (c-0x80);
							else			v = (0x80-c);

							if (v>peak[1]) peak[1]=v;

							total[1] += v;
						} while(--len);
					}

					if (hdc = GetDC(hDlg)) {
						if (vdd->mode == VMMODE_VUMETER)
							CaptureVumeterDoVumeter(vdd, hdc, total, peak, dwBytes);
						else if (vdd->mode == VMMODE_ANALYZER)
							CaptureVumeterDoAnalyzer(vdd, hdc);
						else
							CaptureVumeterDoScope(vdd, hdc);
						ReleaseDC(hDlg, hdc);
					}
				}

				hdr->dwFlags &= ~WHDR_DONE;

//				_RPT1(0,"Received buffer: %ld bytes\n", hdr->dwBytesRecorded);

				if (MMSYSERR_NOERROR != (res = waveInAddBuffer(vdd->hWaveIn,hdr,sizeof(WAVEHDR))))
					MyError("MM system: error %ld",res).post(hDlg,"Vumeter error");
			}
			break;
	}

	return FALSE;
}
