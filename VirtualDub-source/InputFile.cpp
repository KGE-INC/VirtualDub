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

#include <process.h>
#include <stdio.h>

#include <windows.h>
#include <vfw.h>
#include <commdlg.h>

#include "InputFile.h"
#include "AudioSource.h"
#include "VideoSource.h"
#include "Error.h"
#include "AVIStripeSystem.h"
#include "AVIReadHandler.h"

#include "gui.h"
#include "oshelper.h"
#include "prefs.h"

#include "resource.h"

extern HINSTANCE g_hInst;
extern char g_msgBuf[128];
extern const char fileFiltersAppend[];
extern HWND g_hWnd;

/////////////////////////////////////////////////////////////////////

InputFileOptions::~InputFileOptions() {
}

/////////////////////////////////////////////////////////////////////

InputFilenameNode::InputFilenameNode(const char *_n) : name(strdup(_n)) {
	if (!name)
		throw MyMemoryError();
}

InputFilenameNode::~InputFilenameNode() {
	delete (char *)name;
}

/////////////////////////////////////////////////////////////////////

InputFile::~InputFile() {
	InputFilenameNode *ifn;

	while(ifn = listFiles.RemoveTail())
		delete ifn;
}

void InputFile::AddFilename(const char *lpszFile) {
	InputFilenameNode *ifn = new InputFilenameNode(lpszFile);

	if (ifn)
		listFiles.AddTail(ifn);
}

bool InputFile::Append(const char *szFile) {
	return false;
}

void InputFile::setOptions(InputFileOptions *) {
}

InputFileOptions *InputFile::promptForOptions(HWND) {
	return NULL;
}

InputFileOptions *InputFile::createOptions(const char *buf) {
	return NULL;
}

void InputFile::InfoDialog(HWND hwndParent) {
}

void InputFile::setAutomated(bool) {
}

bool InputFile::isOptimizedForRealtime() {
	return false;
}

bool InputFile::isStreaming() {
	return false;
}

/////////////////////////////////////////////////////////////////////

char InputFileAVI::szME[]="AVI Import Filter";

InputFileAVI::InputFileAVI(bool) {
	audioSrc = NULL;
	videoSrc = NULL;

	stripesys = NULL;
	stripe_files = NULL;
	fAutomated	= false;

	fAcceptPartial = false;
	fInternalMJPEG = false;
	fDisableFastIO = false;
	iMJPEGMode = 0;
	fccForceVideo = 0;
	fccForceVideoHandler = 0;
	lForceAudioHz = 0;

	pAVIFile = NULL;

	fCompatibilityMode = fRedoKeyFlags = false;

	fAutoscanSegments = false;
}

InputFileAVI::~InputFileAVI() {

	delete videoSrc;
	delete audioSrc;

	if (stripe_files) {
		int i;

		for(i=0; i<stripe_count; i++)
			if (stripe_files[i])
				stripe_files[i]->Release();

		delete stripe_files;
	}
	delete stripesys;

	if (pAVIFile)
		pAVIFile->Release();
}

///////////////////////////////////////////////



class InputFileAVIOptions : public InputFileOptions {
public:
	struct InputFileAVIOpts {
		int len;
		int iMJPEGMode;
		FOURCC fccForceVideo;
		FOURCC fccForceVideoHandler;
		long lForceAudioHz;

		bool fCompatibilityMode;
		bool fAcceptPartial;
		bool fRedoKeyFlags;
		bool fInternalMJPEG;
		bool fDisableFastIO;
	} opts;
		
	~InputFileAVIOptions();

	bool read(const char *buf);
	int write(char *buf, int buflen);

	static BOOL APIENTRY SetupDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam);
};

InputFileAVIOptions::~InputFileAVIOptions() {
}

bool InputFileAVIOptions::read(const char *buf) {
	const InputFileAVIOpts *pp = (const InputFileAVIOpts *)buf;

	if (pp->len != sizeof(InputFileAVIOpts))
		return false;

	opts = *pp;

	return true;
}

int InputFileAVIOptions::write(char *buf, int buflen) {
	InputFileAVIOpts *pp = (InputFileAVIOpts *)buf;

	if (buflen<sizeof(InputFileAVIOpts))
		return 0;

	opts.len = sizeof(InputFileAVIOpts);
	*pp = opts;

	return sizeof(InputFileAVIOpts);
}

///////

BOOL APIENTRY InputFileAVIOptions::SetupDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	InputFileAVIOptions *thisPtr = (InputFileAVIOptions *)GetWindowLong(hDlg, DWL_USER);

	switch(message) {
		case WM_INITDIALOG:
			SetWindowLong(hDlg, DWL_USER, lParam);
			SendDlgItemMessage(hDlg, IDC_FORCE_FOURCC, EM_LIMITTEXT, 4, 0);
			CheckDlgButton(hDlg, IDC_IF_NORMAL, BST_CHECKED);
			return TRUE;

		case WM_COMMAND:
			switch(LOWORD(wParam)) {
			case IDCANCEL:
				if (GetDlgItem(hDlg, IDC_ACCEPTPARTIAL))
					thisPtr->opts.fAcceptPartial = !!IsDlgButtonChecked(hDlg, IDC_ACCEPTPARTIAL);

				thisPtr->opts.fCompatibilityMode = !!IsDlgButtonChecked(hDlg, IDC_AVI_COMPATIBILITYMODE);
				thisPtr->opts.fRedoKeyFlags = !!IsDlgButtonChecked(hDlg, IDC_AVI_REKEY);
				thisPtr->opts.fInternalMJPEG = !!IsDlgButtonChecked(hDlg, IDC_AVI_INTERNALMJPEG);
				thisPtr->opts.fDisableFastIO = !!IsDlgButtonChecked(hDlg, IDC_AVI_DISABLEOPTIMIZEDIO);

				if (IsDlgButtonChecked(hDlg, IDC_IF_NORMAL))
					thisPtr->opts.iMJPEGMode = VideoSourceAVI::IFMODE_NORMAL;
				else if (IsDlgButtonChecked(hDlg, IDC_IF_SWAP))
					thisPtr->opts.iMJPEGMode = VideoSourceAVI::IFMODE_SWAP;
				else if (IsDlgButtonChecked(hDlg, IDC_IF_SPLITNOSWAP))
					thisPtr->opts.iMJPEGMode = VideoSourceAVI::IFMODE_SPLIT1;
				else if (IsDlgButtonChecked(hDlg, IDC_IF_SPLITSWAP))
					thisPtr->opts.iMJPEGMode = VideoSourceAVI::IFMODE_SPLIT2;
				else if (IsDlgButtonChecked(hDlg, IDC_IF_DISCARDFIRST))
					thisPtr->opts.iMJPEGMode = VideoSourceAVI::IFMODE_DISCARD1;
				else if (IsDlgButtonChecked(hDlg, IDC_IF_DISCARDSECOND))
					thisPtr->opts.iMJPEGMode = VideoSourceAVI::IFMODE_DISCARD2;

				if (IsDlgButtonChecked(hDlg, IDC_FORCE_FOURCC)) {
					union {
						char c[5];
						FOURCC fccType;
					};
					int i;

					i = SendDlgItemMessage(hDlg, IDC_FOURCC, WM_GETTEXT, sizeof c, (LPARAM)c);

					memset(c+i, ' ', 5-i);

					if (fccType == 0x20202020)
						fccType = ' BID';		// force nothing to DIB, since 0 means no force

					thisPtr->opts.fccForceVideo = fccType;
				} else
					thisPtr->opts.fccForceVideo = 0;

				if (IsDlgButtonChecked(hDlg, IDC_FORCE_HANDLER)) {
					union {
						char c[5];
						FOURCC fccType;
					};
					int i;

					i = SendDlgItemMessage(hDlg, IDC_FOURCC2, WM_GETTEXT, sizeof c, (LPARAM)c);

					memset(c+i, ' ', 5-i);

					if (fccType == 0x20202020)
						fccType = ' BID';		// force nothing to DIB, since 0 means no force

					thisPtr->opts.fccForceVideoHandler = fccType;
				} else
					thisPtr->opts.fccForceVideoHandler = 0;

				if (IsDlgButtonChecked(hDlg, IDC_FORCE_SAMPRATE))
					thisPtr->opts.lForceAudioHz = GetDlgItemInt(hDlg, IDC_AUDIORATE, NULL, FALSE);
				else
					thisPtr->opts.lForceAudioHz = 0;
				
				EndDialog(hDlg, 0);
				return TRUE;

			case IDC_FORCE_FOURCC:
				EnableWindow(GetDlgItem(hDlg, IDC_FOURCC), IsDlgButtonChecked(hDlg, IDC_FORCE_FOURCC));
				return TRUE;

			case IDC_FORCE_HANDLER:
				EnableWindow(GetDlgItem(hDlg, IDC_FOURCC2), IsDlgButtonChecked(hDlg, IDC_FORCE_HANDLER));
				return TRUE;

			case IDC_FORCE_SAMPRATE:
				EnableWindow(GetDlgItem(hDlg, IDC_AUDIORATE), IsDlgButtonChecked(hDlg, IDC_FORCE_SAMPRATE));
				return TRUE;
			}
			break;
	}

	return FALSE;
}

void InputFileAVI::setOptions(InputFileOptions *_ifo) {
	InputFileAVIOptions *ifo = (InputFileAVIOptions *)_ifo;

	fCompatibilityMode	= ifo->opts.fCompatibilityMode;
	fAcceptPartial		= ifo->opts.fAcceptPartial;
	fRedoKeyFlags		= ifo->opts.fRedoKeyFlags;
	fInternalMJPEG		= ifo->opts.fInternalMJPEG;
	fDisableFastIO		= ifo->opts.fDisableFastIO;
	iMJPEGMode			= ifo->opts.iMJPEGMode;
	fccForceVideo		= ifo->opts.fccForceVideo;
	fccForceVideoHandler= ifo->opts.fccForceVideoHandler;
	lForceAudioHz		= ifo->opts.lForceAudioHz;
}

InputFileOptions *InputFileAVI::createOptions(const char *buf) {
	InputFileAVIOptions *ifo = new InputFileAVIOptions();

	if (!ifo) throw MyMemoryError();

	if (!ifo->read(buf)) {
		delete ifo;
		return NULL;
	}

	return ifo;
}

InputFileOptions *InputFileAVI::promptForOptions(HWND hwnd) {
	InputFileAVIOptions *ifo = new InputFileAVIOptions();

	if (!ifo) throw MyMemoryError();

	DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_EXTOPENOPTS_AVI),
			hwnd, InputFileAVIOptions::SetupDlgProc, (LPARAM)ifo);

	return ifo;
}

///////////////////////////////////////////////

static bool fileExists(const char *fn) {
	DWORD dwAttrib = GetFileAttributes(fn);

	return dwAttrib != -1 && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY);
}

void InputFileAVI::EnableSegmentAutoscan() {
	fAutoscanSegments = true;
}

void InputFileAVI::ForceCompatibility() {
	fCompatibilityMode = true;
}

void InputFileAVI::Init(char *szFile) {
	HRESULT err;
	PAVIFILE paf;

	AddFilename(szFile);

	if (fCompatibilityMode) {
		if (err = AVIFileOpen(&paf, szFile, OF_READ, NULL))
			throw MyAVIError(szME, err);

		if (!(pAVIFile = CreateAVIReadHandler(paf))) {
			AVIFileRelease(paf);
			throw MyMemoryError();
		}
	} else {
		if (!(pAVIFile = CreateAVIReadHandler(szFile)))
			throw MyMemoryError();
	}

	if (fDisableFastIO)
		pAVIFile->EnableFastIO(false);

	if (!(videoSrc = new VideoSourceAVI(pAVIFile,NULL,NULL,fInternalMJPEG, iMJPEGMode, fccForceVideo, fccForceVideoHandler)))
		throw MyMemoryError();

	if (!videoSrc->init())
		throw MyError("%s: problem opening video stream", szME);

	if (fRedoKeyFlags)
		((VideoSourceAVI *)videoSrc)->redoKeyFlags();
	else if (pAVIFile->isIndexFabricated() && !fAutomated)
		MessageBox(NULL,
			"Warning: VirtualDub has reconstructed the index for this file, but you have not specified "
			"rekeying in the extended open options dialog.  Seeking in this file may be slow.",
			"AVI Import Filter Warning",
			MB_OK);


	audioSrc = new AudioSourceAVI(pAVIFile);
	if (!audioSrc->init()) {
		delete audioSrc;
		audioSrc = NULL;
	} else if (lForceAudioHz) {
		WAVEFORMATEX *pwfex = (WAVEFORMATEX *)audioSrc->getFormat();

		pwfex->nAvgBytesPerSec = MulDiv(pwfex->nAvgBytesPerSec, lForceAudioHz, pwfex->nSamplesPerSec);
		pwfex->nSamplesPerSec = lForceAudioHz;
		audioSrc->streamInfo.dwScale = pwfex->nBlockAlign;
		audioSrc->streamInfo.dwRate = pwfex->nAvgBytesPerSec;
	}

	if (fAutoscanSegments) {
		char szPath[MAX_PATH], szNameTail[MAX_PATH];
		const char *pszName = SplitPathName(szFile);
		char *s = szNameTail;

		strcpy(szNameTail, pszName);
		memcpy(szPath, szFile, pszName-szFile);
		szPath[pszName-szFile]=0;

		while(*s)
			++s;

		if (s > szNameTail+7 && !stricmp(s-7, ".00.avi")) {
			int nSegment = 0;
			const char *pszPath;

			s -= 7;
			*s=0;

			MergePath(szPath, szNameTail);
			s = szPath;
			while(*s)
				++s;

			try {
				while(pAVIFile->getSegmentHint(&pszPath)) {

					wsprintf(s, ".%02d.avi", ++nSegment);

					if (!fileExists(szPath)) {
						if (pszPath && *pszPath) {
							strcpy(szPath, pszPath);
							MergePath(szPath, szNameTail);
							s = szPath;
							while(*s)
								++s;
						}
						wsprintf(s, ".%02d.avi", nSegment);
					}

					if (!fileExists(szPath)) {
						OPENFILENAME ofn;
						char szTitle[MAX_PATH];

						wsprintf(szTitle, "Cannot find file %s.%02d.avi", szNameTail, nSegment);

						ofn.lStructSize			= sizeof(OPENFILENAME);
						ofn.hwndOwner			= g_hWnd;
						ofn.lpstrFilter			= fileFiltersAppend;
						ofn.lpstrCustomFilter	= NULL;
						ofn.nFilterIndex		= 1;
						ofn.lpstrFile			= szPath;
						ofn.nMaxFile			= sizeof szPath;
						ofn.lpstrFileTitle		= NULL;
						ofn.lpstrInitialDir		= NULL;
						ofn.lpstrTitle			= szTitle;
						ofn.Flags				= OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
						ofn.lpstrDefExt			= g_prefs.main.fAttachExtension ? "avi" : NULL;

						if (!GetOpenFileName(&ofn))
							throw MyUserAbortError();

						if (!Append(szPath))
							break;

						((char *)SplitPathName(szPath))[0] = 0;
						MergePath(szPath, szNameTail);
						s = szPath;
						while(*s)
							++s;

					} else if (!Append(szPath))
						break;
				}
			} catch(MyError e) {
				wsprintf(szPath, "Cannot load video segment %02d", nSegment);

				e.post(NULL, szPath);
			}
		}
	}
}

bool InputFileAVI::Append(const char *szFile) {
	if (fCompatibilityMode || stripesys)
		return false;

	if (!szFile)
		return true;

	if (pAVIFile->AppendFile(szFile)) {
		if (videoSrc)
			((VideoSourceAVI *)videoSrc)->Reinit();
		if (audioSrc)
			((AudioSourceAVI *)audioSrc)->Reinit();

		AddFilename(szFile);

		return true;
	}

	return false;
}

void InputFileAVI::InitStriped(char *szFile) {
	int i;
	HRESULT err;
	PAVIFILE paf;
	IAVIReadHandler *index_file;

	if (!(stripesys = new AVIStripeSystem(szFile)))
		throw MyMemoryError();

	stripe_count = stripesys->getStripeCount();

	if (!(stripe_files = new IAVIReadHandler *[stripe_count]))
		throw MyMemoryError();

	for(i=0; i<stripe_count; i++)
		stripe_files[i]=NULL;

	for(i=0; i<stripe_count; i++) {
		AVIStripe *asdef = stripesys->getStripeInfo(i);

		// Ordinarily, OF_SHARE_DENY_WRITE would be better, but XingMPEG
		// Encoder requires write access to AVI files... *sigh*

		if (err = AVIFileOpen(&paf, asdef->szName, OF_READ | OF_SHARE_DENY_NONE, NULL))
			throw MyAVIError("AVI Striped Import Filter", err);

		if (!(stripe_files[i] = CreateAVIReadHandler(paf))) {
			AVIFileRelease(paf);
			throw MyMemoryError();
		}

		if (asdef->isIndex())
			index_file = stripe_files[i];
	}

	if (!(videoSrc = new VideoSourceAVI(index_file, stripesys, stripe_files, fInternalMJPEG, iMJPEGMode, fccForceVideo)))
		throw MyMemoryError();

	if (!videoSrc->init())
		throw MyError("%s: problem opening video stream", szME);

	if (fRedoKeyFlags) ((VideoSourceAVI *)videoSrc)->redoKeyFlags();

	if (!(audioSrc = new AudioSourceAVI(index_file)))
		throw MyMemoryError();

	if (!audioSrc->init()) {
		delete audioSrc;
		audioSrc = NULL;
	}
}

bool InputFileAVI::isOptimizedForRealtime() {
	return pAVIFile->isOptimizedForRealtime();
}

bool InputFileAVI::isStreaming() {
	return pAVIFile->isStreaming();
}

///////////////////////////////////////////////////////////////////////////

typedef struct MyFileInfo {
	InputFileAVI *thisPtr;

	volatile HWND hWndAbort;
	UINT statTimer;
	long	lVideoKFrames;
	long	lVideoKMinSize;
	__int64 i64VideoKTotalSize;
	long	lVideoKMaxSize;
	long	lVideoCFrames;
	long	lVideoCMinSize;
	__int64	i64VideoCTotalSize;
	long	lVideoCMaxSize;

	long	lAudioFrames;
	long	lAudioMinSize;
	__int64	i64AudioTotalSize;
	long	lAudioMaxSize;

	long	lAudioPreload;
} MyFileInfo;

void InputFileAVI::_InfoDlgThread(void *pvInfo) {
	MyFileInfo *pInfo = (MyFileInfo *)pvInfo;
	LONG i;
	LONG lActualBytes, lActualSamples;
	VideoSourceAVI *inputVideoAVI = (VideoSourceAVI *)pInfo->thisPtr->videoSrc;
	AudioSourceAVI *inputAudioAVI = (AudioSourceAVI *)pInfo->thisPtr->audioSrc;

	pInfo->lVideoCMinSize = 0x7FFFFFFF;
	pInfo->lVideoKMinSize = 0x7FFFFFFF;

	for(i=inputVideoAVI->lSampleFirst; i<inputVideoAVI->lSampleLast; ++i) {
		if (inputVideoAVI->isKey(i)) {
			++pInfo->lVideoKFrames;

			if (!inputVideoAVI->read(i, 1, NULL, 0, &lActualBytes, NULL)) {
				pInfo->i64VideoKTotalSize += lActualBytes;
				if (lActualBytes < pInfo->lVideoKMinSize) pInfo->lVideoKMinSize = lActualBytes;
				if (lActualBytes > pInfo->lVideoKMaxSize) pInfo->lVideoKMaxSize = lActualBytes;
			}
		} else {
			++pInfo->lVideoCFrames;

			if (!inputVideoAVI->read(i, 1, NULL, 0, &lActualBytes, NULL)) {
				pInfo->i64VideoCTotalSize += lActualBytes;
				if (lActualBytes < pInfo->lVideoCMinSize) pInfo->lVideoCMinSize = lActualBytes;
				if (lActualBytes > pInfo->lVideoCMaxSize) pInfo->lVideoCMaxSize = lActualBytes;
			}
		}

		if (pInfo->hWndAbort) {
			SendMessage(pInfo->hWndAbort, WM_USER+256, 0, 0);
			return;
		}
	}

	if (inputAudioAVI) {
		pInfo->lAudioMinSize = 0x7FFFFFFF;

		i = inputAudioAVI->lSampleFirst;
		while(i < inputAudioAVI->lSampleLast) {
			if (inputAudioAVI->read(i, AVISTREAMREAD_CONVENIENT, NULL, 0, &lActualBytes, &lActualSamples))
				break;

			++pInfo->lAudioFrames;
			i += lActualSamples;

			if (inputAudioAVI->streamInfo.dwInitialFrames == pInfo->lAudioFrames)
				pInfo->lAudioPreload = i - inputAudioAVI->lSampleFirst;

			pInfo->i64AudioTotalSize += lActualBytes;
			if (lActualBytes < pInfo->lAudioMinSize) pInfo->lAudioMinSize = lActualBytes;
			if (lActualBytes > pInfo->lAudioMaxSize) pInfo->lAudioMaxSize = lActualBytes;

			if (pInfo->hWndAbort) {
				SendMessage(pInfo->hWndAbort, WM_USER+256, 0, 0);
				return;
			}
		}
	}

	pInfo->hWndAbort = (HWND)1;
}

BOOL APIENTRY InputFileAVI::_InfoDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam) {
	MyFileInfo *pInfo = (MyFileInfo *)GetWindowLong(hDlg, DWL_USER);
	InputFileAVI *thisPtr;

	if (pInfo)
		thisPtr = pInfo->thisPtr;

    switch (message)
    {
        case WM_INITDIALOG:
			SetWindowLong(hDlg, DWL_USER, lParam);
			pInfo = (MyFileInfo *)lParam;
			thisPtr = pInfo->thisPtr;

			if (thisPtr->videoSrc) {
				ICINFO icinfo;
				char *s;
				HIC hic;
				const VideoSourceAVI *pvs = (const VideoSourceAVI *)thisPtr->videoSrc;

				sprintf(g_msgBuf, "%dx%d, %.3f fps (%ld µs)",
							thisPtr->videoSrc->getImageFormat()->biWidth,
							thisPtr->videoSrc->getImageFormat()->biHeight,
							(float)thisPtr->videoSrc->streamInfo.dwRate / thisPtr->videoSrc->streamInfo.dwScale,
							MulDiv(thisPtr->videoSrc->streamInfo.dwScale, 1000000L, thisPtr->videoSrc->streamInfo.dwRate));
				SetDlgItemText(hDlg, IDC_VIDEO_FORMAT, g_msgBuf);

				s = g_msgBuf + sprintf(g_msgBuf, "%ld (", thisPtr->videoSrc->streamInfo.dwLength);
				ticks_to_str(s, MulDiv(1000L*thisPtr->videoSrc->streamInfo.dwLength, thisPtr->videoSrc->streamInfo.dwScale, thisPtr->videoSrc->streamInfo.dwRate));
				strcat(s,")");
				SetDlgItemText(hDlg, IDC_VIDEO_NUMFRAMES, g_msgBuf);

				strcpy(g_msgBuf, "Unknown");

				if (hic = pvs->getDecompressorHandle()) {
					if (ICGetInfo(hic, &icinfo, sizeof(ICINFO)))
						g_msgBuf[WideCharToMultiByte(CP_ACP, 0, icinfo.szDescription, -1, g_msgBuf, sizeof(g_msgBuf)-7, NULL, NULL)]=0;
				} else if (pvs->isUsingInternalMJPEG())
					strcpy(g_msgBuf, "VirtualDub internal MJPEG");
				else if (pvs->getImageFormat()->biCompression == '2YUY')
					strcpy(g_msgBuf, "YUV 4:2:2 (YUY2)");
				else if (pvs->getImageFormat()->biCompression == '024I')
					strcpy(g_msgBuf, "YUV 4:2:0 (I420)");
				else
					sprintf(g_msgBuf, "Uncompressed RGB%d", pvs->getImageFormat()->biBitCount);

				SetDlgItemText(hDlg, IDC_VIDEO_COMPRESSION, g_msgBuf);
			}
			if (thisPtr->audioSrc) {
				WAVEFORMATEX *fmt = thisPtr->audioSrc->getWaveFormat();
				DWORD cbwfxTemp;
				WAVEFORMATEX *pwfxTemp;
				HACMSTREAM has;
				HACMDRIVERID hadid;
				ACMDRIVERDETAILS add;
				bool fSuccessful = false;

				sprintf(g_msgBuf, "%ldHz", fmt->nSamplesPerSec);
				SetDlgItemText(hDlg, IDC_AUDIO_SAMPLINGRATE, g_msgBuf);

				sprintf(g_msgBuf, "%d (%s)", fmt->nChannels, fmt->nChannels>1 ? "Stereo" : "Mono");
				SetDlgItemText(hDlg, IDC_AUDIO_CHANNELS, g_msgBuf);

				sprintf(g_msgBuf, "%d-bit", fmt->wBitsPerSample);
				SetDlgItemText(hDlg, IDC_AUDIO_PRECISION, g_msgBuf);

				sprintf(g_msgBuf, "%ld", thisPtr->audioSrc->lSampleLast - thisPtr->audioSrc->lSampleFirst);
				SetDlgItemText(hDlg, IDC_AUDIO_NUMFRAMES, g_msgBuf);

				////////// Attempt to detect audio compression //////////

				if (fmt->wFormatTag != WAVE_FORMAT_PCM) {
					// Retrieve maximum format size.

					acmMetrics(NULL, ACM_METRIC_MAX_SIZE_FORMAT, (LPVOID)&cbwfxTemp);

					// Fill out a destination wave format (PCM).

					if (pwfxTemp = (WAVEFORMATEX *)malloc(cbwfxTemp)) {
						pwfxTemp->wFormatTag	= WAVE_FORMAT_PCM;

						// Ask ACM to fill out the details.

						if (!acmFormatSuggest(NULL, fmt, pwfxTemp, cbwfxTemp, ACM_FORMATSUGGESTF_WFORMATTAG)) {
							if (!acmStreamOpen(&has, NULL, fmt, pwfxTemp, NULL, NULL, NULL, ACM_STREAMOPENF_NONREALTIME)) {
								if (!acmDriverID((HACMOBJ)has, &hadid, 0)) {
									memset(&add, 0, sizeof add);

									add.cbStruct = sizeof add;

									if (!acmDriverDetails(hadid, &add, 0)) {
										SetDlgItemText(hDlg, IDC_AUDIO_COMPRESSION, add.szLongName);

										fSuccessful = true;
									}
								}

								acmStreamClose(has, 0);
							}
						}

						free(pwfxTemp);
					}

					if (!fSuccessful) {
						char buf[32];

						wsprintf(buf, "Unknown (tag %04X)", fmt->wFormatTag);
						SetDlgItemText(hDlg, IDC_AUDIO_COMPRESSION, buf);
					}
				} else {
					// It's a PCM format...

					SetDlgItemText(hDlg, IDC_AUDIO_COMPRESSION, "PCM (Uncompressed)");
				}
			}

			_beginthread(_InfoDlgThread, 10000, pInfo);

			pInfo->statTimer = SetTimer(hDlg, 1, 250, NULL);

            return (TRUE);

        case WM_COMMAND:                      
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) 
            {
				if (pInfo->hWndAbort == (HWND)1)
					EndDialog(hDlg, TRUE);
				else
					pInfo->hWndAbort = hDlg;
                return TRUE;
            }
            break;

		case WM_DESTROY:
			if (pInfo->statTimer) KillTimer(hDlg, pInfo->statTimer);
			break;

		case WM_TIMER:
			_RPT0(0,"timer hit\n");
			sprintf(g_msgBuf, "%ld", pInfo->lVideoKFrames);
			SetDlgItemText(hDlg, IDC_VIDEO_NUMKEYFRAMES, g_msgBuf);

			if (pInfo->lVideoKFrames)
				sprintf(g_msgBuf, "%ld/%I64d/%ld (%I64dK)"
							,pInfo->lVideoKMinSize
							,pInfo->i64VideoKTotalSize/pInfo->lVideoKFrames
							,pInfo->lVideoKMaxSize
							,(pInfo->i64VideoKTotalSize+1023)>>10);
			else
				strcpy(g_msgBuf,"(no key frames)");
			SetDlgItemText(hDlg, IDC_VIDEO_KEYFRAMESIZES, g_msgBuf);

			if (pInfo->lVideoCFrames)
				sprintf(g_msgBuf, "%ld/%I64d/%ld (%I64dK)"
							,pInfo->lVideoCMinSize
							,pInfo->i64VideoCTotalSize/pInfo->lVideoCFrames
							,pInfo->lVideoCMaxSize
							,(pInfo->i64VideoCTotalSize+1023)>>10);
			else
				strcpy(g_msgBuf,"(no delta frames)");
			SetDlgItemText(hDlg, IDC_VIDEO_NONKEYFRAMESIZES, g_msgBuf);

			if (thisPtr->audioSrc) {
				sprintf(g_msgBuf,"%ld",pInfo->lAudioFrames);
				SetDlgItemText(hDlg, IDC_AUDIO_NUMFRAMES, g_msgBuf);

				if (pInfo->lAudioFrames)
					sprintf(g_msgBuf, "%ld/%I64d/%ld (%I64dK)"
								,pInfo->lAudioMinSize
								,pInfo->i64AudioTotalSize/pInfo->lAudioFrames
								,pInfo->lAudioMaxSize
								,(pInfo->i64AudioTotalSize+1023)>>10);
				else
					strcpy(g_msgBuf,"(no audio frames)");
				SetDlgItemText(hDlg, IDC_AUDIO_FRAMESIZES, g_msgBuf);

				sprintf(g_msgBuf, "%ld samples (%.2fs)",pInfo->lAudioPreload,(double)pInfo->lAudioPreload/thisPtr->audioSrc->getWaveFormat()->nSamplesPerSec);
				SetDlgItemText(hDlg, IDC_AUDIO_PRELOAD, g_msgBuf);
			}

			/////////

			if (pInfo->hWndAbort) {
				KillTimer(hDlg, pInfo->statTimer);
				return TRUE;
			}

			break;

		case WM_USER+256:
			EndDialog(hDlg, TRUE);  
			break;
    }
    return FALSE;
}

void InputFileAVI::InfoDialog(HWND hwndParent) {
	MyFileInfo mai;

	memset(&mai, 0, sizeof mai);
	mai.thisPtr = this;

	DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_AVI_INFO), hwndParent, _InfoDlgProc, (LPARAM)&mai);
}
