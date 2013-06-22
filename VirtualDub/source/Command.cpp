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
#include <windows.h>

#include <vd2/system/file.h>
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>

#include "PositionControl.h"

#include "InputFile.h"
#include "InputFileImages.h"
#include "AudioSource.h"
#include "VideoSource.h"
#include "AVIOutput.h"
#include "AVIOutputWAV.h"
#include "AVIOutputImages.h"
#include "AVIOutputStriped.h"
#include "Dub.h"
#include "DubOutput.h"
#include "AudioFilterSystem.h"
#include "FrameSubset.h"
#include "ProgressDialog.h"
#include "oshelper.h"

#include "mpeg.h"
#include "gui.h"
#include "prefs.h"
#include "command.h"
#include "project.h"

///////////////////////////////////////////////////////////////////////////

extern HWND					g_hWnd;
extern DubOptions			g_dubOpts;

extern wchar_t g_szInputWAVFile[MAX_PATH];

extern DubSource::ErrorMode	g_videoErrorMode;
extern DubSource::ErrorMode	g_audioErrorMode;

vdrefptr<InputFile>		inputAVI;
InputFileOptions	*g_pInputOpts			= NULL;

vdrefptr<VideoSource>	inputVideoAVI;
vdrefptr<AudioSource>	inputAudio;
vdrefptr<AudioSource>	inputAudioAVI;
vdrefptr<AudioSource>	inputAudioWAV;
FrameSubset			*inputSubset			= NULL;

int					 audioInputMode = AUDIOIN_AVI;

IDubber				*g_dubber				= NULL;

COMPVARS			g_Vcompression;
WAVEFORMATEX		*g_ACompressionFormat		= NULL;
DWORD				g_ACompressionFormatSize	= 0;

VDAudioFilterGraph	g_audioFilterGraph;

extern VDProject *g_project;


bool				g_drawDecompressedFrame	= FALSE;
bool				g_showStatusWindow		= TRUE;

///////////////////////////////////////////////////////////////////////////

void SetAudioSource() {
	switch(audioInputMode) {
	case AUDIOIN_NONE:		inputAudio = NULL; break;
	case AUDIOIN_AVI:		inputAudio = inputAudioAVI; break;
	case AUDIOIN_WAVE:		inputAudio = inputAudioWAV; break;
	}
}

void AppendAVI(const wchar_t *pszFile) {
	if (inputAVI) {
		long lTail = inputAVI->videoSrc->getEnd();

		if (inputAVI->Append(pszFile)) {
			if (inputSubset)
				inputSubset->insert(inputSubset->end(), FrameSubsetNode(lTail, inputAVI->videoSrc->getEnd() - lTail, false));
			RemakePositionSlider();
		}
	}
}

void AppendAVIAutoscan(const wchar_t *pszFile) {
	wchar_t buf[MAX_PATH];
	wchar_t *s = buf, *t;
	int count = 0;

	if (!inputAVI)
		return;

	VDPosition originalCount = inputAVI->videoSrc->getEnd();

	wcscpy(buf, pszFile);

	t = VDFileSplitExt(VDFileSplitPath(s));

	if (t>buf)
		--t;

	try {
		for(;;) {
			if (!VDDoesPathExist(VDStringW(buf)))
				break;
			
			if (!inputAVI->Append(buf))
				break;

			++count;

			s = t;

			for(;;) {
				if (s<buf || !isdigit(*s)) {
					memmove(s+2, s+1, sizeof(wchar_t) * wcslen(s));
					s[1] = L'1';
					++t;
				} else {
					if (*s == L'9') {
						*s-- = L'0';
						continue;
					}
					++*s;
				}
				break;
			}
		}
	} catch(const MyError& e) {
		// log append errors, but otherwise eat them

		VDLog(kVDLogWarning, VDTextAToW(e.gets()));
	}

	guiSetStatus("Appended %d segments (stopped at \"%s\")", 255, count, VDTextWToA(buf).c_str());

	if (count) {
		if (inputSubset)
			inputSubset->insert(inputSubset->end(), FrameSubsetNode(originalCount, inputAVI->videoSrc->getEnd() - originalCount, false));
		RemakePositionSlider();
	}
}

void CloseAVI() {
	if (g_pInputOpts) {
		delete g_pInputOpts;
		g_pInputOpts = NULL;
	}

	if (inputAudio == inputAudioAVI)
		inputAudio = NULL;

	inputAudioAVI = NULL;
	inputVideoAVI = NULL;
	inputAVI = NULL;

	if (inputSubset) {
		delete inputSubset;
		inputSubset = NULL;
	}

	filters.DeinitFilters();
	filters.DeallocateBuffers();
}

void OpenWAV(const wchar_t *szFile) {
	vdrefptr<AudioSourceWAV> pNewAudio(new AudioSourceWAV(szFile, g_dubOpts.perf.waveBufferSize));
	if (!pNewAudio->init())
		throw MyError("The sound file \"%s\" could not be processed. Please check that it is a valid WAV file.", VDTextWToA(szFile).c_str());

	pNewAudio->setDecodeErrorMode(g_audioErrorMode);

	wcscpy(g_szInputWAVFile, szFile);

	audioInputMode = AUDIOIN_WAVE;
	inputAudioWAV = pNewAudio;
}

void CloseWAV() {
	inputAudioWAV = NULL;
}

void SaveWAV(const wchar_t *szFilename, bool fProp, DubOptions *quick_opts) {
	if (!inputVideoAVI)
		throw MyError("No input file to process.");

	SetAudioSource();

	VDAVIOutputWAVSystem wavout(szFilename);
	g_project->RunOperation(&wavout, TRUE, quick_opts, 0, fProp);
}

///////////////////////////////////////////////////////////////////////////

void SaveAVI(const wchar_t *szFilename, bool fProp, DubOptions *quick_opts, bool fCompatibility) {
	VDAVIOutputFileSystem fileout;

	fileout.Set1GBLimit(g_prefs.fAVIRestrict1Gb != 0);
	fileout.SetCaching(false);
	fileout.SetIndexing(!fCompatibility);
	fileout.SetFilename(szFilename);
	fileout.SetBuffer(g_dubOpts.perf.outputBufferSize);

	g_project->RunOperation(&fileout, FALSE, quick_opts, g_prefs.main.iDubPriority, fProp);
}

void SaveStripedAVI(const wchar_t *szFile) {
	AVIStripeSystem *stripe_def = NULL;

	///////////////

	SetAudioSource();

	if (!inputVideoAVI)
		throw MyError("No input video stream to process.");

	VDAVIOutputStripedSystem outstriped(szFile);

	outstriped.Set1GBLimit(g_prefs.fAVIRestrict1Gb != 0);

	g_project->RunOperation(&outstriped, FALSE, NULL, g_prefs.main.iDubPriority, false, 0, 0);
}

void SaveStripeMaster(const wchar_t *szFile) {
	AVIStripeSystem *stripe_def = NULL;

	///////////////

	SetAudioSource();

	if (!inputVideoAVI)
		throw MyError("No input video stream to process.");

	VDAVIOutputStripedSystem outstriped(szFile);

	outstriped.Set1GBLimit(g_prefs.fAVIRestrict1Gb != 0);

	g_project->RunOperation(&outstriped, 2, NULL, g_prefs.main.iDubPriority, false, 0, 0);
}

void SaveSegmentedAVI(const wchar_t *szFilename, bool fProp, DubOptions *quick_opts, long lSpillThreshold, long lSpillFrameThreshold) {
	if (!inputVideoAVI)
		throw MyError("No input file to process.");

	VDAVIOutputFileSystem outfile;

	outfile.SetIndexing(false);
	outfile.SetCaching(false);
	outfile.SetBuffer(g_dubOpts.perf.outputBufferSize);

	const VDStringW filename(szFilename);
	outfile.SetFilenamePattern(VDFileSplitExtLeft(filename).c_str(), VDFileSplitExtRight(filename).c_str(), 2);

	g_project->RunOperation(&outfile, FALSE, quick_opts, g_prefs.main.iDubPriority, fProp, lSpillThreshold, lSpillFrameThreshold);
}

void SaveImageSequence(const wchar_t *szPrefix, const wchar_t *szSuffix, int minDigits, bool fProp, DubOptions *quick_opts, int targetFormat) {
	VDAVIOutputImagesSystem outimages;

	outimages.SetFilenamePattern(szPrefix, szSuffix, minDigits);
	outimages.SetFormat(targetFormat);
		
	g_project->RunOperation(&outimages, FALSE, quick_opts, g_prefs.main.iDubPriority, fProp, 0, 0);
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
		(inputSubset ? inputSubset->getTotalFrames() : inputVideoAVI->getLength()) - inputVideoAVI->msToSamples(ms));
}

void RemakePositionSlider() {
	if (!inputAVI) return;

	HWND hwndPosition = GetDlgItem(g_hWnd, IDC_POSITION);

	SendMessage(hwndPosition, PCM_SETRANGEMIN, (BOOL)FALSE, 0);
	SendMessage(hwndPosition, PCM_SETRANGEMAX, (BOOL)TRUE , inputSubset->getTotalFrames());

	if (g_dubOpts.video.lStartOffsetMS || g_dubOpts.video.lEndOffsetMS) {
		SendMessage(hwndPosition, PCM_SETSELSTART, (BOOL)FALSE, inputVideoAVI->msToSamples(g_dubOpts.video.lStartOffsetMS));
		SendMessage(hwndPosition, PCM_SETSELEND, (BOOL)TRUE , inputSubset->getTotalFrames() - inputVideoAVI->msToSamples(g_dubOpts.video.lEndOffsetMS));
	} else {
		SendMessage(hwndPosition, PCM_CLEARSEL, (BOOL)TRUE, 0);
	}
}

void RecalcPositionTimeConstant() {
	HWND hwndPosition = GetDlgItem(g_hWnd, IDC_POSITION);

	DubVideoStreamInfo vInfo;
	DubAudioStreamInfo aInfo;

	if (inputVideoAVI) {
		try {
			InitStreamValuesStatic(vInfo, aInfo, inputVideoAVI, inputAudio, &g_dubOpts, NULL);
			SendMessage(hwndPosition, PCM_SETFRAMERATE, vInfo.frameRateIn.getHi(), vInfo.frameRateIn.getLo());
		} catch(const MyError&) {
			// The input stream may throw an error here trying to obtain the nearest key.
			// If so, bail.
		}
	} else {
		SendMessage(hwndPosition, PCM_SETFRAMERATE, 0, 0);
	}
}

void ScanForUnreadableFrames(FrameSubset *pSubset, VideoSource *pVideoSource) {
	const VDPosition lFirst = pVideoSource->getStart();
	const VDPosition lLast = pVideoSource->getEnd();
	VDPosition lFrame = lFirst;
	void *pBuffer = NULL;
	int cbBuffer = 0;

	try {
		ProgressDialog pd(g_hWnd, "Frame scan", "Scanning for unreadable frames", lLast-lFrame, true);
		bool bLastValid = true;
		long lRangeFirst;
		long lDeadFrames = 0;
		long lMaskedFrames = 0;

		pd.setValueFormat("Frame %d of %d");

		pVideoSource->streamBegin(false);

		while(lFrame <= lLast) {
			LONG lActualBytes, lActualSamples;
			int err;
			bool bValid;

			pd.advance(lFrame - lFirst);
			pd.check();

			do {
				bValid = false;

				if (!bLastValid && !pVideoSource->isKey(lFrame))
					break;

				if (lFrame < lLast) {
					err = pVideoSource->read(lFrame, 1, NULL, 0, &lActualBytes, &lActualSamples);

					if (err)
						break;

					if (cbBuffer < lActualBytes) {
						int cbNewBuffer = (lActualBytes + 65535) & ~65535;
						void *pNewBuffer = realloc(pBuffer, cbNewBuffer);

						if (!pNewBuffer)
							throw MyMemoryError();

						cbBuffer = cbNewBuffer;
						pBuffer = pNewBuffer;
					}

					err = pVideoSource->read(lFrame, 1, pBuffer, cbBuffer, &lActualBytes, &lActualSamples);

					if (err)
						break;

					try {
						pVideoSource->streamGetFrame(pBuffer, lActualBytes, pVideoSource->isKey(lFrame), FALSE, lFrame);
					} catch(...) {
						++lDeadFrames;
						break;
					}
				}

				bValid = true;
			} while(false);

			if (!bValid)
				++lMaskedFrames;

			if (bValid ^ bLastValid) {
				if (!bValid)
					lRangeFirst = lFrame;
				else
					pSubset->setRange(lRangeFirst, lFrame - lRangeFirst, true);

				bLastValid = bValid;
			}

			++lFrame;
		}

		pVideoSource->streamEnd();

		guiSetStatus("%ld frames masked (%ld frames bad, %ld frames good but undecodable)", 255, lMaskedFrames, lDeadFrames, lMaskedFrames-lDeadFrames);

	} catch(...) {
		free(pBuffer);
		throw;
	}

	free(pBuffer);
}
