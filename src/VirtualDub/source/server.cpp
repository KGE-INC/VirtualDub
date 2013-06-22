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

#include <map>

#include "vdserver.h"

#include "AudioSource.h"
#include "VideoSource.h"
#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/system/filesys.h>
#include <vd2/Dita/services.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "FrameSubset.h"

#include "filters.h"
#include "dub.h"
#include "DubUtils.h"
#include "gui.h"
#include "audio.h"
#include "command.h"
#include "prefs.h"
#include "project.h"
#include "server.h"
#include "resource.h"
#include "uiframe.h"

extern HINSTANCE g_hInst;
extern HWND g_hWnd;

extern VDProject *g_project;

extern wchar_t g_szInputAVIFile[MAX_PATH];

// VideoSource.cpp

///////////////////////////////////////////////////////////////////////////

enum {
	kFileDialog_Signpost		= 'sign'
};

//////////////////////////////////////////////////////////////////////////

class FrameserverSession {
private:
	HANDLE hArena;

public:
	FrameserverSession *next, *prev;

	char *arena;
	long arena_size;
	DWORD id;

	FrameserverSession();
	DWORD Init(LONG arena_size, DWORD session_id);
	~FrameserverSession();
};

FrameserverSession::FrameserverSession() {
	next = prev = NULL;
	hArena = INVALID_HANDLE_VALUE;
}

DWORD FrameserverSession::Init(LONG arena_size, DWORD session_id) {
	char buf[16];

	wsprintf(buf, "VDUBF%08lx", session_id);

	if (INVALID_HANDLE_VALUE == (hArena = OpenFileMapping(FILE_MAP_WRITE, FALSE, buf)))
		return NULL;

	if (!(arena = (char *)MapViewOfFile(hArena, FILE_MAP_WRITE, 0, 0, arena_size)))
		return NULL;

	this->id = (DWORD)this;
	this->arena_size = arena_size;

	return this->id;
}

FrameserverSession::~FrameserverSession() {
	if (arena) UnmapViewOfFile(arena);
	if (hArena != INVALID_HANDLE_VALUE) CloseHandle(hArena);
}

///////////////////////////////////

class Frameserver : public vdrefcounted<IVDUIFrameClient> {
private:
	DubOptions			*opt;
	HWND				hwnd;
	AudioSource			*aSrc;
	VideoSource			*vSrc;

	bool			mbExit;

	DubAudioStreamInfo	aInfo;
	DubVideoStreamInfo	vInfo;
	FrameSubset			audioset;
	long				lVideoSamples;
	long				lAudioSamples;
	FilterStateInfo		fsi;
	VDRenderFrameMap	mVideoFrameMap;
	VDPixmapLayout	mFrameLayout;

	DWORD_PTR			dwUserSave;

	long			lRequestCount, lFrameCount, lAudioSegCount;

	HWND			hwndStatus;

	vdblock<char>	mInputBuffer;

	typedef std::map<uint32, FrameserverSession *> tSessions;
	tSessions	mSessions;

	char *lpszFsname;

	VDUIFrame		*mpUIFrame;

public:
	Frameserver(VideoSource *video, AudioSource *audio, HWND hwndParent, DubOptions *xopt, const FrameSubset& server);
	~Frameserver();

	void Detach();
	LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void Go(IVDubServerLink *ivdsl, char *name);

	static INT_PTR APIENTRY StatusDlgProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	INT_PTR APIENTRY StatusDlgProc2( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	FrameserverSession *SessionLookup(LPARAM lParam);
	LRESULT SessionOpen(LPARAM mmapID, WPARAM arena_len);
	LRESULT SessionClose(LPARAM lParam);
	LRESULT SessionStreamInfo(LPARAM lParam, WPARAM stream);
	LRESULT SessionFormat(LPARAM lParam, WPARAM stream);
	LRESULT SessionFrame(LPARAM lParam, WPARAM sample);
	LRESULT SessionAudio(LPARAM lParam, WPARAM lStart);
	LRESULT SessionAudioInfo(LPARAM lParam, WPARAM lStart);
};

Frameserver::Frameserver(VideoSource *video, AudioSource *audio, HWND hwndParent, DubOptions *xopt, const FrameSubset& subset) {
	opt				= xopt;
	hwnd			= hwndParent;

	aSrc			= audio;
	vSrc			= video;

	lFrameCount = lRequestCount = lAudioSegCount = 0;

	InitStreamValuesStatic(vInfo, aInfo, video, audio, opt, &subset);

	vdfastvector<IVDVideoSource *> vsrcs(1, video);
	mVideoFrameMap.Init(vsrcs, vInfo.start_src, vInfo.frameRateIn / vInfo.frameRate, &subset, vInfo.end_dst, false);

	VDPosition lOffsetStart = video->msToSamples(opt->video.lStartOffsetMS);
	VDPosition lOffsetEnd = video->msToSamples(opt->video.lEndOffsetMS);

	FrameSubset			videoset(subset);

	if (opt->audio.fEndAudio)
		videoset.deleteRange(videoset.getTotalFrames() - lOffsetEnd, videoset.getTotalFrames());

	if (opt->audio.fStartAudio)
		videoset.deleteRange(0, lOffsetStart);

	VDDEBUG("Video subset:\n");
	videoset.dump();

	if (audio)
		AudioTranslateVideoSubset(audioset, videoset, vInfo.frameRateIn, audio->getWaveFormat(), !opt->audio.fEndAudio && (videoset.empty() || videoset.back().end() == video->getEnd()) ? audio->getEnd() : 0, NULL);

	VDDEBUG("Audio subset:\n");
	audioset.dump();

	if (audio) {
		audioset.offset(audio->msToSamples(-opt->audio.offset));
		lAudioSamples = audioset.getTotalFrames();
	} else
		lAudioSamples = 0;

	lVideoSamples = mVideoFrameMap.size();
}

Frameserver::~Frameserver() {
	{
		for(tSessions::iterator it(mSessions.begin()), itEnd(mSessions.end()); it!=itEnd; ++it) {
			FrameserverSession *pSession = (*it).second;

			delete pSession;
		}

		mSessions.clear();
	}

	filters.DeinitFilters();
	filters.DeallocateBuffers();
}

void Frameserver::Detach() {}

void Frameserver::Go(IVDubServerLink *ivdsl, char *name) {
	int server_index = -1;

	lpszFsname = name;
	
	// prepare the sources...

	if (vSrc) {
		if (!vSrc->setTargetFormat(g_dubOpts.video.mInputFormat))
			if (!vSrc->setTargetFormat(nsVDPixmap::kPixFormat_XRGB8888))
				if (!vSrc->setTargetFormat(nsVDPixmap::kPixFormat_RGB888))
					if (!vSrc->setTargetFormat(nsVDPixmap::kPixFormat_XRGB1555))
						if (!vSrc->setTargetFormat(nsVDPixmap::kPixFormat_Pal8))
							throw MyError("The decompression codec cannot decompress to an RGB format. This is very unusual. Check that any \"Force YUY2\" options are not enabled in the codec's properties.");

		vSrc->streamBegin(true, false);

		BITMAPINFOHEADER *bmih = vSrc->getDecompressedFormat();

		filters.initLinearChain(&g_listFA, (Pixel *)(bmih+1), bmih->biWidth, abs(bmih->biHeight), 24);

		if (filters.getFrameLag())
			MessageBox(g_hWnd,
			"One or more filters in the filter chain has a non-zero lag. This will cause the served "
			"video to lag behind the audio!"
			, "VirtualDub warning", MB_OK);

		fsi.lMicrosecsPerFrame		= vInfo.usPerFrame;
		fsi.lMicrosecsPerSrcFrame	= vInfo.usPerFrameIn;
		fsi.flags					= 0;

		if (filters.ReadyFilters(fsi))
			throw MyError("Error readying filters.");

		const VBitmap *pvb = filters.LastBitmap();

		VDPixmapCreateLinearLayout(mFrameLayout, nsVDPixmap::kPixFormat_RGB888, pvb->w, pvb->h, 4);
		VDPixmapLayoutFlipV(mFrameLayout);
	}

	if (aSrc)
		aSrc->streamBegin(true, false);

	// usurp the window

	VDUIFrame *pFrame = VDUIFrame::GetFrame(hwnd);
	mpUIFrame = pFrame;
	pFrame->Attach(this);

	guiSetTitle(hwnd, IDS_TITLE_FRAMESERVER);

	// create dialog box

	mbExit = false;

	if (hwndStatus = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_SERVER), hwnd, Frameserver::StatusDlgProc, (LPARAM)this)) {

		// hide the main window

		ShowWindow(hwnd, SW_HIDE);

		// create the frameserver

		server_index = ivdsl->CreateFrameServer(name, hwnd);

		if (server_index>=0) {

			// kick us into high priority

			SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

			// enter window loop

			{
				MSG msg;

				while(!mbExit) {
					BOOL result = GetMessage(&msg, NULL, 0, 0);

					if (result == (BOOL)-1)
						break;

					if (!result) {
						PostQuitMessage(msg.wParam);
						break;
					}

					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}

			// return to normal priority

			SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

			ivdsl->DestroyFrameServer(server_index);
		}

		if (IsWindow(hwndStatus)) DestroyWindow(hwndStatus);

		// show the main window

		ShowWindow(hwnd, SW_SHOW);
	}

	// unsubclass
	pFrame->Detach();

	if (vSrc) {
		vSrc->streamEnd();
	}

	if (server_index<0) throw MyError("Couldn't create frameserver");
}

LRESULT Frameserver::WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {

	case WM_CLOSE:
		DestroyWindow(hWnd);
		break;

	case WM_DESTROY:                  // message: window being destroyed
		mbExit = true;
		break;

	case VDSRVM_BIGGEST:
		{
			long size=sizeof(AVISTREAMINFO);

			if (vSrc) {
				if (size < sizeof(BITMAPINFOHEADER))
					size = sizeof(BITMAPINFOHEADER);

				if (size < filters.OutputBitmap()->size)
					size = filters.OutputBitmap()->size;
			}

			if (aSrc) {
				if (size < aSrc->getWaveFormat()->nAvgBytesPerSec)
					size = aSrc->getWaveFormat()->nAvgBytesPerSec;

				if (aSrc->getFormatLen()>size)
					size=aSrc->getFormatLen();
			}

			if (size < 65536) size=65536;

			VDDEBUG("VDSRVM_BIGGEST: allocate a frame of size %ld bytes\n", size);
			return size;
		}

	case VDSRVM_OPEN:
		++lRequestCount;
		VDDEBUG("VDSRVM_OPEN(arena size %ld, mmap ID %08lx)\n", wParam, lParam);
		return SessionOpen(lParam, wParam);

	case VDSRVM_CLOSE:
		++lRequestCount;
		VDDEBUG("[session %08lx] VDSRVM_CLOSE()\n", lParam);
		return SessionClose(lParam);

	case VDSRVM_REQ_STREAMINFO:
		++lRequestCount;
		VDDEBUG("[session %08lx] VDSRVM_REQ_STREAMINFO(stream %d)\n", lParam, wParam);
		return SessionStreamInfo(lParam, wParam);

	case VDSRVM_REQ_FORMAT:
		++lRequestCount;
		VDDEBUG("[session %08lx] VDSRVM_REQ_FORMAT(stream %d)\n", lParam, wParam);
		return SessionFormat(lParam, wParam);

	case VDSRVM_REQ_FRAME:
		++lFrameCount;
		VDDEBUG("[session %08lx] VDSRVM_REQ_FRAME(sample %ld)\n", lParam, wParam);
		return SessionFrame(lParam, wParam);

	case VDSRVM_REQ_AUDIO:
		++lAudioSegCount;
		return SessionAudio(lParam, wParam);

	case VDSRVM_REQ_AUDIOINFO:
		++lAudioSegCount;
		VDDEBUG("[session %08lx] VDSRVM_REQ_AUDIOINFO(sample %ld)\n", lParam, wParam);
		return SessionAudioInfo(lParam, wParam);

	default:
		return mpUIFrame->DefProc(hWnd, message, wParam, lParam);
    }
    return (0);
}

///////////////////////

INT_PTR CALLBACK Frameserver::StatusDlgProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	return ((Frameserver *)GetWindowLongPtr(hWnd, DWLP_USER))->StatusDlgProc2(hWnd, message, wParam, lParam);
}

INT_PTR CALLBACK Frameserver::StatusDlgProc2( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
	case WM_INITDIALOG:
		SetWindowLongPtr(hWnd, DWLP_USER, lParam);
		SetDlgItemText(hWnd, IDC_STATIC_FSNAME, ((Frameserver *)lParam)->lpszFsname);
		SetTimer(hWnd,1,1000,NULL);

		{
			HKEY hkey;
			HIC hic;
			BOOL fAVIFile = FALSE, fVCM = FALSE;

			if (RegOpenKeyEx(HKEY_CLASSES_ROOT, "CLSID\\{894288E0-0948-11D2-8109-004845000EB5}\\InProcServer32\\AVIFile", 0, KEY_QUERY_VALUE, &hkey)==ERROR_SUCCESS) {
				RegCloseKey(hkey);
				fAVIFile = TRUE;
			}

			if (hic = ICOpen('CDIV', 'TSDV', ICMODE_DECOMPRESS)) {
				ICClose(hic);
				fVCM = TRUE;
			}

			if (fAVIFile && fVCM)
				SetDlgItemText(hWnd, IDC_STATIC_FCINSTALLED, "AVIFile and VCM");
			else if (fAVIFile)
				SetDlgItemText(hWnd, IDC_STATIC_FCINSTALLED, "AVIFile only");
			else if (fVCM)
				SetDlgItemText(hWnd, IDC_STATIC_FCINSTALLED, "VCM only");
		}
		return TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) != IDOK) break;
	case WM_CLOSE:
		mbExit = true;
		return TRUE;
	case WM_TIMER:
		SetDlgItemInt(hWnd, IDC_STATIC_REQCOUNT, lRequestCount, FALSE);
		SetDlgItemInt(hWnd, IDC_STATIC_FRAMECNT, lFrameCount, FALSE);
		SetDlgItemInt(hWnd, IDC_STATIC_AUDIOSEGS, lAudioSegCount, FALSE);
		return TRUE;
    }
    return FALSE;
}

////////////////////////////////////////////////////////

FrameserverSession *Frameserver::SessionLookup(LPARAM lParam) {
	tSessions::const_iterator it(mSessions.find(lParam));

	if (it != mSessions.end())
		return (*it).second;

	VDDEBUG("Session lookup failed on %08lx\n", lParam);

	return NULL;
}

LRESULT Frameserver::SessionOpen(LPARAM mmapID, WPARAM arena_len) {
	FrameserverSession *fs;
	DWORD id;

	if (fs = new FrameserverSession()) {
		if (id = fs->Init(arena_len, mmapID)) {
			mSessions[id] = fs;
			return id;
		}
		delete fs;
	}

	return NULL;
}

LRESULT Frameserver::SessionClose(LPARAM lParam) {
	FrameserverSession *fs = SessionLookup(lParam);

	if (!fs) return VDSRVERR_BADSESSION;

	delete fs;

	mSessions.erase(lParam);

	return VDSRVERR_OK;
}

LRESULT Frameserver::SessionStreamInfo(LPARAM lParam, WPARAM stream) {
	FrameserverSession *fs = SessionLookup(lParam);

	if (!fs) return VDSRVERR_BADSESSION;

	if (stream<0 || stream>2) return VDSRVERR_NOSTREAM;

	if (stream==0) {
		AVISTREAMINFO *lpasi = (AVISTREAMINFO *)(fs->arena+8);

		if (!vSrc) return VDSRVERR_NOSTREAM;

		*(long *)(fs->arena+0) = 0;										//vSrc->lSampleFirst;
		*(long *)(fs->arena+4) = lVideoSamples;			//vSrc->lSampleLast;
		memcpy(lpasi, &vSrc->getStreamInfo(), sizeof(AVISTREAMINFO));

		lpasi->fccHandler	= ' BID';
		lpasi->dwLength		= *(long *)(fs->arena+4);
		lpasi->dwRate		= vInfo.frameRate.getHi();
		lpasi->dwScale		= vInfo.frameRate.getLo();

		SetRect(&lpasi->rcFrame, 0, 0, filters.OutputBitmap()->w, filters.OutputBitmap()->h);

		lpasi->dwSuggestedBufferSize = filters.OutputBitmap()->size;

	} else {
		if (!aSrc) return VDSRVERR_NOSTREAM;

		*(long *)(fs->arena+0) = 0;
		*(long *)(fs->arena+4) = lAudioSamples;
		memcpy(fs->arena+8, &aSrc->getStreamInfo(), sizeof(AVISTREAMINFO));

		((AVISTREAMINFO *)(fs->arena+8))->dwLength = audioset.getTotalFrames();
	}

	return VDSRVERR_OK;
}

LRESULT Frameserver::SessionFormat(LPARAM lParam, WPARAM stream) {
	FrameserverSession *fs = SessionLookup(lParam);
	DubSource *ds;
	long len;

	if (!fs) return VDSRVERR_BADSESSION;

	if (stream<0 || stream>2) return VDSRVERR_NOSTREAM;

	ds = stream ? (DubSource *)aSrc : (DubSource *)vSrc;

	if (!ds) return VDSRVERR_NOSTREAM;

	if (stream) {
		len = aSrc->getFormatLen();

		if (len > fs->arena_size) return VDSRVERR_TOOBIG;

		memcpy(fs->arena, aSrc->getFormat(), len);
	} else {
		BITMAPINFOHEADER *bmih;

		len = sizeof(BITMAPINFOHEADER);
		if (len > fs->arena_size) return VDSRVERR_TOOBIG;

		memcpy(fs->arena, vSrc->getDecompressedFormat(), len);

		bmih = (BITMAPINFOHEADER *)fs->arena;
//		bmih->biSize		= sizeof(BITMAPINFOHEADER);
		bmih->biWidth		= filters.LastBitmap()->w;
		bmih->biHeight		= filters.LastBitmap()->h;
		bmih->biPlanes		= 1;
		bmih->biCompression	= BI_RGB;
		bmih->biBitCount	= 24;
		bmih->biSizeImage	= ((bmih->biWidth*3+3)&-4)*abs(bmih->biHeight);
		bmih->biClrUsed		= 0;
		bmih->biClrImportant= 0;
	}

	return len;
}

LRESULT Frameserver::SessionFrame(LPARAM lParam, WPARAM original_frame) {
	FrameserverSession *fs = SessionLookup(lParam);

	if (!fs)
		return VDSRVERR_BADSESSION;

	try {
		const void *ptr = vSrc->getFrameBuffer();
		const BITMAPINFOHEADER *bmih = vSrc->getDecompressedFormat();
		VDPosition sample;
		bool is_preroll;

		if (fs->arena_size < ((filters.LastBitmap()->w*3+3)&-4)*filters.LastBitmap()->h)
			return VDSRVERR_TOOBIG;

		sample = mVideoFrameMap[original_frame].mDisplayFrame;

		if (sample < 0)
			return VDSRVERR_FAILED;

		vSrc->streamSetDesiredFrame(sample);

		VDPosition frame = vSrc->streamGetNextRequiredFrame(is_preroll);

		if (frame >= 0) {
			do {
				uint32 lSize;
				int hr;

	//			_RPT1(0,"feeding frame %ld\n", frame);

				hr = vSrc->read(frame, 1, NULL, 0x7FFFFFFF, &lSize, NULL);
				if (hr)
					return VDSRVERR_FAILED;

				uint32 bufSize = (lSize + 65535 + vSrc->streamGetDecodePadding()) & ~65535;
				if (mInputBuffer.size() < bufSize)
					mInputBuffer.resize(bufSize);

				hr = vSrc->read(frame, 1, &mInputBuffer[0], lSize, &lSize, NULL); 
				if (hr)
					return VDSRVERR_FAILED;

				ptr = vSrc->streamGetFrame(&mInputBuffer[0], lSize, is_preroll, frame);
			} while(-1 != (frame = vSrc->streamGetNextRequiredFrame(is_preroll)));

		} else
			ptr = vSrc->streamGetFrame(NULL, 0, FALSE, vSrc->displayToStreamOrder(sample));

		VDPixmap pxdst(VDPixmapFromLayout(mFrameLayout, fs->arena));

		if (!g_listFA.IsEmpty()) {
			VBitmap srcbm((void *)vSrc->getFrameBuffer(), vSrc->getDecompressedFormat());

			VDPixmapBlt(VDAsPixmap(*filters.InputBitmap()), vSrc->getTargetFormat());

			fsi.lCurrentFrame				= original_frame;
			fsi.lCurrentSourceFrame			= sample;
			fsi.lSourceFrameMS				= MulDiv(fsi.lCurrentSourceFrame, fsi.lMicrosecsPerSrcFrame, 1000);
			fsi.lDestFrameMS				= MulDiv(fsi.lCurrentFrame, fsi.lMicrosecsPerFrame, 1000);

			filters.RunFilters(fsi);

			VDPixmapBlt(pxdst, VDAsPixmap(*filters.LastBitmap()));
		} else
			VDPixmapBlt(pxdst, vSrc->getTargetFormat());

	} catch(const MyError&) {
		return VDSRVERR_FAILED;
	}

	return VDSRVERR_OK;
}

LRESULT Frameserver::SessionAudio(LPARAM lParam, WPARAM lStart) {
	FrameserverSession *fs = SessionLookup(lParam);
	if (!fs) return VDSRVERR_BADSESSION;

	LONG lCount = *(LONG *)fs->arena;
	LONG cbBuffer = *(LONG *)(fs->arena+4);

	if (cbBuffer > fs->arena_size - 8) cbBuffer = fs->arena_size - 8;

	VDDEBUG("[session %08lx] VDSRVM_REQ_AUDIO(sample %ld, count %d, cbBuffer %ld)\n", lParam, lCount, lStart, cbBuffer);

	// Do not return an error on an attempt to read beyond the end of
	// the audio stream -- this causes Panasonic to error.

	if (lStart >= lAudioSamples) {
		memset(fs->arena, 0, 8);
		return VDSRVERR_OK;
	}

	if (lStart+lCount > lAudioSamples)
		lCount = lAudioSamples;

	// Read subsets.

	long lTotalBytes = 0, lTotalSamples = 0;
	uint32 lActualBytes, lActualSamples = 1;
	char *pDest = (char *)(fs->arena + 8);

	try {
		while(lCount>0 && lActualSamples>0) {
			sint64 start, len;

			// Translate range.

			start = audioset.lookupRange(lStart, len);

			if (len > lCount)
				len = lCount;

			if (start < aSrc->getStart()) {
				start = aSrc->getStart();
				len = 1;
			}

			if (start >= aSrc->getEnd()) {
				start = aSrc->getEnd() - 1;
				len = 1;
			}

			// Attempt read.

			switch(aSrc->read(start, len, pDest, cbBuffer, &lActualBytes, &lActualSamples)) {
			case AVIERR_OK:
				break;
			case AVIERR_BUFFERTOOSMALL:
				if (!lTotalSamples)
					return VDSRVERR_TOOBIG;
				goto out_of_space;
			default:
				return VDSRVERR_FAILED;
			}

			lCount -= lActualSamples;
			lStart += lActualSamples;
			cbBuffer -= lActualBytes;
			pDest += lActualBytes;
			lTotalSamples += lActualSamples;
			lTotalBytes += lActualBytes;
		}
out_of_space:
		;

	} catch(const MyError&) {
		return VDSRVERR_FAILED;
	}

	*(LONG *)(fs->arena + 0) = lTotalBytes;
	*(LONG *)(fs->arena + 4) = lTotalSamples;

	return VDSRVERR_OK;
}

LRESULT Frameserver::SessionAudioInfo(LPARAM lParam, WPARAM lStart) {
	FrameserverSession *fs = SessionLookup(lParam);
	if (!fs) return VDSRVERR_BADSESSION;

	LONG lCount = *(LONG *)fs->arena;
	LONG cbBuffer = *(LONG *)(fs->arena+4);

	if (lStart < 0)
		return VDSRVERR_FAILED;

	if (lStart + lCount > lAudioSamples)
		lCount = lAudioSamples - lStart;

	if (lCount < 0)
		lCount = 0;

	*(LONG *)(fs->arena + 0) = aSrc->getWaveFormat()->nBlockAlign * lCount;
	*(LONG *)(fs->arena + 4) = lCount;

	return VDSRVERR_OK;
}

//////////////////////////////////////////////////////////////////////////

extern vdrefptr<AudioSource> inputAudio;
extern vdrefptr<VideoSource> inputVideoAVI;

static HMODULE hmodServer;
static IVDubServerLink *ivdsl;

static BOOL InitServerDLL() {
#ifdef _M_AMD64
	hmodServer = LoadLibrary("vdsvrlnk64.dll");
#else
	hmodServer = LoadLibrary("vdsvrlnk.dll");
#endif

	VDDEBUG("VDSVRLNK handle: %p\n", hmodServer);

	if (hmodServer) {
		FARPROC fp;

		if (!(fp = GetProcAddress(hmodServer, "GetDubServerInterface")))
			return FALSE;

		ivdsl = ((IVDubServerLink *(*)(void))fp)();

		return TRUE;
	}

	return FALSE;
}

INT_PTR CALLBACK FrameServerSetupDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:
		{
			char buf[32];

			ivdsl->GetComputerName(buf);
			strcat(buf,"/");

			SetDlgItemText(hDlg, IDC_COMPUTER_NAME, buf);
		}
		SetDlgItemText(hDlg, IDC_FSNAME, VDTextWToA(VDFileSplitPath(g_szInputAVIFile)).c_str());
		SetWindowLongPtr(hDlg, DWLP_USER, lParam);
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			SendDlgItemMessage(hDlg, IDC_FSNAME, WM_GETTEXT, 128, GetWindowLongPtr(hDlg, DWLP_USER));
			EndDialog(hDlg, TRUE);
			break;
		case IDCANCEL:
			EndDialog(hDlg, FALSE);
			break;
		}
		break;
	}

	return FALSE;
}

void ActivateFrameServerDialog(HWND hwnd) {
	static wchar_t fileFilters[]=
		L"VirtualDub AVIFile signpost (*.vdr,*.avi)\0"		L"*.vdr;*.avi\0"
		L"All files\0"										L"*.*\0"
		;

	char szServerName[128];

	if (!InitServerDLL()) return;

	if (!DialogBoxParam(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SERVER_SETUP), hwnd, FrameServerSetupDlgProc, (LPARAM)szServerName))
		return;

	try {
		vdrefptr<Frameserver> fs(new Frameserver(inputVideoAVI, inputAudio, hwnd, &g_dubOpts, g_project->GetTimeline().GetSubset()));

		const VDStringW fname(VDGetSaveFileName(kFileDialog_Signpost, (VDGUIHandle)hwnd, L"Save .VDR signpost for AVIFile handler", fileFilters, g_prefs.main.fAttachExtension ? L"vdr" : NULL, 0, 0));

		if (!fname.empty()) {
			long buf[5];
			char sname[128];
			int slen;

			ivdsl->GetComputerName(sname);
			strcat(sname,"/");
			strcat(sname,szServerName);
			slen = strlen(sname);
			slen += slen&1;

			buf[0] = 'FFIR';
			buf[1] = slen+12;
			buf[2] = 'MRDV';
			buf[3] = 'HTAP';
			buf[4] = slen;

			VDFile file(fname.c_str(), nsVDFile::kWrite | nsVDFile::kDenyRead | nsVDFile::kCreateAlways);

			file.write(buf, 20);
			file.write(sname, strlen(sname));
			if (strlen(sname) & 1)
				file.write("", 1);

			file.close();
		}

		VDDEBUG("Attempting to initialize frameserver...\n");

		fs->Go(ivdsl, szServerName);

		VDDEBUG("Frameserver exit.\n");

	} catch(const MyError& e) {
		e.post(hwnd, "Frameserver error");
	}
}
