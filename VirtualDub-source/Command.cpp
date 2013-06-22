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

#include <windows.h>

#include "PositionControl.h"

#include "InputFile.h"
#include "AudioSource.h"
#include "VideoSource.h"
#include "AVIOutput.h"
#include "AVIOutputWAV.h"
#include "AVIOutputStriped.h"
#include "Dub.h"
#include "Error.h"
#include "FrameSubset.h"

#include "mpeg.h"
#include "gui.h"
#include "prefs.h"
#include "command.h"

///////////////////////////////////////////////////////////////////////////

extern HWND					g_hWnd;
extern DubOptions			g_dubOpts;
extern HDC					hDCWindow;
extern HDRAWDIB				hDDWindow;

InputFile			*inputAVI				= NULL;
AVIOutput			*outputAVI				= NULL;
InputFileOptions	*g_pInputOpts			= NULL;

VideoSource			*inputVideoAVI			= NULL;
AudioSource			*inputAudio				= NULL;
AudioSource			*inputAudioAVI			= NULL;
AudioSource			*inputAudioWAV			= NULL;
FrameSubset			*inputSubset			= NULL;

int					 audioInputMode = AUDIOIN_AVI;

IDubber				*g_dubber				= NULL;

COMPVARS			g_Vcompression;
WAVEFORMATEX		*g_ACompressionFormat		= NULL;
DWORD				g_ACompressionFormatSize	= 0;

BOOL				g_drawDecompressedFrame	= FALSE;
BOOL				g_showStatusWindow		= TRUE;
BOOL				g_syncroBlit			= FALSE;
BOOL				g_vertical				= FALSE;

///////////////////////////////////////////////////////////////////////////

void InitDubAVI(char *szFile, int fAudioOnly, DubOptions *quick_options, int iPriority=0, bool fPropagateErrors = false, long lSpillThreshold=0, long lSpillFrameThreshold=0);

void SetAudioSource() {
	switch(audioInputMode) {
	case AUDIOIN_NONE:		inputAudio = NULL; break;
	case AUDIOIN_AVI:		inputAudio = inputAudioAVI; break;
	case AUDIOIN_WAVE:		inputAudio = inputAudioWAV; break;
	}
}

void OpenAVI(char *szFile, int iFileType, bool fExtendedOpen, bool fQuiet, bool fAutoscan, const char *pInputOpts) {
	CloseAVI();

	// Reset dub option parameters.

	g_dubOpts.video.lStartOffsetMS = g_dubOpts.video.lEndOffsetMS = 0;

	try {
		HWND hWndPosition = GetDlgItem(g_hWnd, IDC_POSITION);

		// attempt to determine input file type

		if (iFileType == FILETYPE_AUTODETECT || iFileType == FILETYPE_AUTODETECT2) {
			HANDLE hFile;
			char buf[64];
			DWORD dwActual;

			hFile = CreateFile(szFile, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

			if (INVALID_HANDLE_VALUE == hFile)
				throw MyWin32Error("Can't open \"%s\": %%s", GetLastError(), szFile);

			if (!ReadFile(hFile, buf, 64, &dwActual, NULL))
				throw MyWin32Error("Error reading \"%s\": %%s", GetLastError(), szFile);

			if (dwActual<12)
				throw MyError("Can't read \"%s\": The file is too short (<12 bytes).", szFile);

			CloseHandle(hFile);

			// 'RIFF' <size> 'AVI ' -> AVI
			// 'RIFF' <size> 'CDXA' -> VideoCD stream
			// 00 00 01 b3 -> MPEG video stream (video sequence start packet)
			// 00 00 01 ba -> Interleaved MPEG
			// '#str' -> stripe
			// 30 26 B2 75 8E 66 CF 11 A6 D9 00 AA 00 62 CE 6C
			//		->  (no this is not a joke)
			// PK 03 04 -> Zip file

			const static unsigned char asf_sig[]={
				0x30, 0x26, 0xb2, 0x75, 0x8e, 0x66, 0xcf, 0x11,
				0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c
			};


			if (*(long *)(buf+0)=='FFIR' && *(long *)(buf+8)==' IVA')
				iFileType = FILETYPE_AVI;
			else if (*(long *)(buf+0)=='FFIR' && *(long *)(buf+8)=='AXDC')
//				throw MyError("\"%s\" is a VideoCD stream that Windows has wrapped in a RIFF/CDXA file, which VirtualDub cannot read. You will need a utility "
//							"such as VCDGear to convert this back to a regular MPEG stream.", szFile);
				iFileType = FILETYPE_MPEG;
			else if (*(long *)(buf+0) == 0x04034b50)
				throw MyError("\"%s\" is a Zip file!  Try unzipping it.", szFile);
			else if (*(long *)(buf+0) == 0xba010000 || *(long *)(buf+0)==0xb3010000)
				iFileType = FILETYPE_MPEG;
			else if (*(long *)buf == 'rts#')
				iFileType = FILETYPE_STRIPEDAVI;
			else if (!memcmp(buf, asf_sig, 16)) {
				iFileType = FILETYPE_ASF;
			} else {

				// Last ditch: try AVIFile.  This will make Ben happy. :)

				PAVIFILE paf;
				HRESULT hr = AVIFileOpen(&paf, szFile, OF_READ, NULL);

				if (hr)
					throw MyError("Cannot determine file type of \"%s\"", szFile);

				AVIFileRelease(paf);

				iFileType = FILETYPE_AVICOMPAT;
			}
		}

		// open file

		switch(iFileType) {
		case FILETYPE_AVI:
		case FILETYPE_STRIPEDAVI:
			inputAVI = new InputFileAVI(false);
			if (fAutoscan)
				((InputFileAVI *)inputAVI)->EnableSegmentAutoscan();
			break;
		case FILETYPE_AVICOMPAT:
			inputAVI = new InputFileAVI(false);
			break;
		case FILETYPE_MPEG:
			inputAVI = CreateInputFileMPEG();
			break;
		case FILETYPE_ASF:
			throw MyError("ASF file support has been removed at the request of Microsoft.");
			break;
		}

		if (!inputAVI) throw MyMemoryError();

		if (fQuiet)
			inputAVI->setAutomated(true);

		// Extended open?

		if (fExtendedOpen) {
			g_pInputOpts = inputAVI->promptForOptions(g_hWnd);
		} else if (pInputOpts)
			g_pInputOpts = inputAVI->createOptions(pInputOpts);

		if (g_pInputOpts) inputAVI->setOptions(g_pInputOpts);

		if (iFileType == FILETYPE_AVICOMPAT)
			((InputFileAVI *)inputAVI)->ForceCompatibility();


		if (iFileType == FILETYPE_STRIPEDAVI) {
			((InputFileAVI *)inputAVI)->InitStriped(szFile);
		} else
			inputAVI->Init(szFile);

		inputAudioAVI = inputAVI->audioSrc;
		inputVideoAVI = inputAVI->videoSrc;

		if (!inputVideoAVI->setDecompressedFormat(24))
			if (!inputVideoAVI->setDecompressedFormat(32))
				if (!inputVideoAVI->setDecompressedFormat(16))
					inputVideoAVI->setDecompressedFormat(8);

		// How many items did we get?

		{
			InputFilenameNode *pnode = inputAVI->listFiles.AtHead();
			InputFilenameNode *pnode_next;
			int nFiles = 0;

			while(pnode_next = pnode->NextFromHead()) {
				++nFiles;
				pnode = pnode_next;
			}

			if (nFiles > 1)
				guiSetStatus("Autoloaded %d segments (last was \"%s\")", 255, nFiles, pnode->NextFromTail()->name);
		}

//		SendMessage(hWndPosition, PCM_SETRANGEMIN, (BOOL)FALSE, inputAVI->videoSrc->lSampleFirst);
//		SendMessage(hWndPosition, PCM_SETRANGEMAX, (BOOL)TRUE , inputAVI->videoSrc->lSampleLast);
		RemakePositionSlider();
		SendMessage(hWndPosition, PCM_SETFRAMERATE, 0, MulDiv(1000000, inputAVI->videoSrc->streamInfo.dwScale, inputAVI->videoSrc->streamInfo.dwRate));
		SendMessage(hWndPosition, PCM_SETPOS, 0, 0);
	} catch(...) {
		CloseAVI();
		throw;
	}
}

void AppendAVI(const char *pszFile) {
	if (inputAVI) {
		long lTail = inputAVI->videoSrc->lSampleLast;

		if (inputAVI->Append(pszFile)) {
			if (inputSubset)
				inputSubset->addRange(lTail, inputAVI->videoSrc->lSampleLast - lTail);
			RemakePositionSlider();
		}
	}
}

void AppendAVIAutoscan(const char *pszFile) {
	char buf[MAX_PATH];
	char *s = buf, *t;
	int count = 0;

	if (!inputAVI)
		return;

	strcpy(buf, pszFile);

	while(*s) ++s;

	t = s;

	while(t>buf && t[-1]!='\\' && t[-1]!='/' && t[-1]!=':' && t[-1]!='.')
		--t;

	if (t>buf && t[-1]=='.')
		--t;

	if (t>buf)
		--t;

	while(-1!=GetFileAttributes(buf) && inputAVI->Append(buf)) {
		++count;

		s = t;

		for(;;) {
			if (s<buf || !isdigit(*s)) {
				memmove(s+2, s+1, strlen(s));
				s[1] = '1';
				++t;
			} else {
				if (*s == '9') {
					*s-- = '0';
					continue;
				}
				++*s;
			}
			break;
		}
	}

	guiSetStatus("Appended %d segments (stopped at \"%s\")", 255, count, buf);

	if (count)
		RemakePositionSlider();
}

void CloseAVI() {
	if (g_pInputOpts) {
		delete g_pInputOpts;
		g_pInputOpts = NULL;
	}

	if (inputAVI) {
		delete inputAVI;
		inputAVI = NULL;
	}
	inputAudioAVI = NULL;
	inputVideoAVI = NULL;

	if (inputSubset) {
		delete inputSubset;
		inputSubset = NULL;
	}

	filters.DeinitFilters();
	filters.DeallocateBuffers();
}

void OpenWAV(char *szFile) {
	CloseWAV();

	try {
		inputAudioWAV = new AudioSourceWAV(szFile, g_dubOpts.perf.waveBufferSize);
		if (!inputAudioWAV->init()) {
			delete inputAudioWAV;
			inputAudioWAV = NULL;
		}

		audioInputMode = AUDIOIN_WAVE;
	} catch(...) {
		CloseWAV();
		throw;
	}
}

void CloseWAV() {
	if (inputAudioWAV) { delete inputAudioWAV; inputAudioWAV = NULL; }
}

void SaveWAV(char *szFilename, bool fProp, DubOptions *quick_opts) {
	try {
		if (!(outputAVI = new AVIOutputWAV())) throw MyMemoryError();

		InitDubAVI(szFilename, TRUE, quick_opts, 0, fProp, 0, 0);
	} catch(...) {
		CloseNewAVI();
		throw;
	}
	CloseNewAVI();
}

///////////////////////////////////////////////////////////////////////////

void CloseNewAVI() {
	_RPT0(0,"Deleting output AVI...\n");
	if (outputAVI) { delete outputAVI; outputAVI = NULL; }
}

///////////////////////////////////////////////////////////////////////////

void SaveAVI(char *szFilename, bool fProp, DubOptions *quick_opts, bool fCompatibility) {
	try {
		if (!(outputAVI = new AVIOutputFile()))
			throw MyMemoryError();

		if (g_prefs.fAVIRestrict1Gb)
			((AVIOutputFile *)outputAVI)->set_1Gb_limit();

		((AVIOutputFile *)outputAVI)->disable_os_caching();

		if (fCompatibility)
			((AVIOutputFile *)outputAVI)->disable_extended_avi();

		InitDubAVI(szFilename, FALSE, quick_opts, g_prefs.main.iDubPriority, fProp, 0, 0);
	} catch(...) {
		CloseNewAVI();
		throw;
	}
	CloseNewAVI();
}

void SaveStripedAVI(char *szFile) {
	AVIStripeSystem *stripe_def = NULL;

	///////////////

	SetAudioSource();

	if (!inputVideoAVI)
		throw MyError("No input video stream to process.");

	try {
		if (!(stripe_def = new AVIStripeSystem(szFile)))
			throw MyMemoryError();

		if (!(outputAVI = new AVIOutputStriped(stripe_def)))
			throw MyMemoryError();
		else {
			if (g_prefs.fAVIRestrict1Gb)
				((AVIOutputStriped *)outputAVI)->set_1Gb_limit();

			InitDubAVI(NULL, FALSE, NULL, g_prefs.main.iDubPriority, false, 0, 0);
		}
	} catch(...) {
		delete stripe_def;
		throw;
	}

	delete stripe_def;
}

void SaveStripeMaster(char *szFile) {
	AVIStripeSystem *stripe_def = NULL;

	///////////////

	SetAudioSource();

	if (!inputVideoAVI)
		throw MyError("No input video stream to process.");

	try {
		if (!(stripe_def = new AVIStripeSystem(szFile)))
			throw MyMemoryError();

		if (!(outputAVI = new AVIOutputStriped(stripe_def)))
			throw MyMemoryError();
		else {
			if (g_prefs.fAVIRestrict1Gb)
				((AVIOutputStriped *)outputAVI)->set_1Gb_limit();

			InitDubAVI(NULL, 2, NULL, g_prefs.main.iDubPriority, false, 0, 0);
		}
	} catch(...) {
		delete stripe_def;
		throw;
	}

	delete stripe_def;
}

void SaveSegmentedAVI(char *szFilename, bool fProp, DubOptions *quick_opts, long lSpillThreshold, long lSpillFrameThreshold) {
	try {
		if (!(outputAVI = new AVIOutputFile()))
			throw MyMemoryError();

		((AVIOutputFile *)outputAVI)->disable_os_caching();
		((AVIOutputFile *)outputAVI)->disable_extended_avi();
		((AVIOutputFile *)outputAVI)->setSegmentHintBlock(true, NULL, 1);

		InitDubAVI(szFilename, FALSE, quick_opts, g_prefs.main.iDubPriority, fProp, lSpillThreshold, lSpillFrameThreshold);
	} catch(...) {
		CloseNewAVI();
		throw;
	}
	CloseNewAVI();
}


///////////////////////////////////////////////////////////////////////////


void SetSelectionStart(long ms) {
	if (!inputVideoAVI)
		return;

	g_dubOpts.video.lStartOffsetMS = ms;
	SendDlgItemMessage(g_hWnd, IDC_POSITION, PCM_SETSELSTART, (WPARAM)TRUE, inputVideoAVI->msToSamples(ms));
}

void SetSelectionEnd(long ms) {
	if (!inputVideoAVI)
		return;

	g_dubOpts.video.lEndOffsetMS = ms;
	SendDlgItemMessage(g_hWnd, IDC_POSITION, PCM_SETSELEND, (WPARAM)TRUE,
		(inputSubset ? inputSubset->getTotalFrames() : (inputVideoAVI->lSampleLast - inputVideoAVI->lSampleFirst)) - inputVideoAVI->msToSamples(ms));
}

void RemakePositionSlider() {
	if (!inputAVI) return;

	HWND hwndPosition = GetDlgItem(g_hWnd, IDC_POSITION);

	if (inputSubset) {
		SendMessage(hwndPosition, PCM_SETRANGEMIN, (BOOL)FALSE, 0);
		SendMessage(hwndPosition, PCM_SETRANGEMAX, (BOOL)TRUE , inputSubset->getTotalFrames());
	} else {
		SendMessage(hwndPosition, PCM_SETRANGEMIN, (BOOL)FALSE, inputAVI->videoSrc->lSampleFirst);
		SendMessage(hwndPosition, PCM_SETRANGEMAX, (BOOL)TRUE , inputAVI->videoSrc->lSampleLast);
	}

	if (g_dubOpts.video.lStartOffsetMS || g_dubOpts.video.lEndOffsetMS) {
		SendMessage(hwndPosition, PCM_SETSELSTART, (BOOL)FALSE, inputVideoAVI->msToSamples(g_dubOpts.video.lStartOffsetMS));
		SendMessage(hwndPosition, PCM_SETSELEND, (BOOL)TRUE , (inputSubset ? inputSubset->getTotalFrames() : (inputVideoAVI->lSampleLast - inputVideoAVI->lSampleFirst)) - inputVideoAVI->msToSamples(g_dubOpts.video.lStartOffsetMS));
	} else {
		SendMessage(hwndPosition, PCM_CLEARSEL, (BOOL)TRUE, 0);
	}
}
