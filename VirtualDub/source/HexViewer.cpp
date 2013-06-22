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

#define f_HEXVIEWER_CPP

#include <ctype.h>

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>

#include "resource.h"
#include "oshelper.h"
#include "gui.h"
#include <vd2/system/error.h>
#include <vd2/system/list.h>

#include "HexViewer.h"
#include "ProgressDialog.h"

extern HINSTANCE g_hInst;

//////////////////////////////////////////////////////////////////////////////

extern const char szHexEditorClassName[]="birdyHexEditor";
extern const char szHexViewerClassName[]="birdyHexViewer";
static const char g_szHexWarning[]="Hex editor warning";

static const char hexdig[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

static bool isValidFOURCC(unsigned long l) {
	return isprint(l>>24) && isprint((l>>16)&0xff) && isprint((l>>8)&0xff) && isprint(l&0xff);
}

static const struct RIFFFieldInfo {
	int offset;
	enum {
		ksbyte,
		kubyte,
		ksword,
		kuword,
		kslong,
		kulong,
		kfcc
	} type;
	const char *name;
} g_Fields_strh[]={
	{ 8, RIFFFieldInfo::kfcc, "fccType: stream type"},
	{12, RIFFFieldInfo::kfcc, "fccType: stream handler"},
	{16, RIFFFieldInfo::kulong, "dwFlags"},
	{20, RIFFFieldInfo::kuword, "wPriority"},
	{22, RIFFFieldInfo::kuword, "wLanguage"},
	{24, RIFFFieldInfo::kulong, "dwInitialFrames"},
	{28, RIFFFieldInfo::kulong, "dwScale: sample rate denominator"},
	{32, RIFFFieldInfo::kulong, "dwRate: sample rate numerator"},
	{36, RIFFFieldInfo::kulong, "dwStart: time in samples to start first stored sample at"},
	{40, RIFFFieldInfo::kulong, "dwLength: stream length in samples"},
	{44, RIFFFieldInfo::kulong, "dwSuggestedBufferSize"},
	{48, RIFFFieldInfo::kulong, "dwQuality"},
	{52, RIFFFieldInfo::kulong, "dwSampleSize"},
	{56, RIFFFieldInfo::kuword, "rcFrame.left"},
	{58, RIFFFieldInfo::kuword, "rcFrame.top"},
	{60, RIFFFieldInfo::kuword, "rcFrame.right"},
	{62, RIFFFieldInfo::kuword, "rcFrame.bottom"},
	{64}
}, g_Fields_avih[]={
	{ 8, RIFFFieldInfo::kulong, "dwMicroSecPerFrame: frame display rate (may be zero)"},
	{12, RIFFFieldInfo::kulong, "dwMaxBytesPerSec: maximum transfer rate in bytes"},
	{16, RIFFFieldInfo::kulong, "dwPaddingGranularity: bytes to pad data chunks to"},
	{20, RIFFFieldInfo::kulong, "dwFlags"},
	{24, RIFFFieldInfo::kulong, "dwTotalFrames: number of frames in full clip"},
	{28, RIFFFieldInfo::kulong, "dwInitialFrames"},
	{32, RIFFFieldInfo::kulong, "dwStreams: number of audio, video, etc. streams"},
	{36, RIFFFieldInfo::kulong, "dwSuggestedBufferSize"},
	{40, RIFFFieldInfo::kulong, "dwWidth: width of video frame used for composited clip"},
	{44, RIFFFieldInfo::kulong, "dwHeight: height of video frame used for composited clip"},
	{48+16}
}, g_Fields_strf_audio[]={
	{ 8, RIFFFieldInfo::kuword, "wFormatTag: ID tag (0x0001 = PCM, 0x0055 = MP3)" },
	{10, RIFFFieldInfo::kuword, "nChannels: 1=mono, 2=stereo" },
	{12, RIFFFieldInfo::kulong, "nSamplesPerSecond: sampling rate of uncompressed audio in Hz" },
	{16, RIFFFieldInfo::kulong, "nAvgBytesPerSec: average bytes/sec of compressed data" },
	{20, RIFFFieldInfo::kuword, "nBlockAlign: size of a compressed sample in bytes" },
	{22, RIFFFieldInfo::kuword, "wBitsPerSample: bits/sample, for PCM only" },
	{24, RIFFFieldInfo::kuword, "cbSize: number of bytes of format-specific data that follow" },
	{26}
}, g_Fields_strf_video[]={
	{ 8, RIFFFieldInfo::kulong, "biSize: size of bitmap structure (BITMAPINFOHEADER: 0x0028)"},
	{12, RIFFFieldInfo::kulong, "biWidth: width of bitmap in pixels"},
	{16, RIFFFieldInfo::kulong, "biHeight: height of bitmap in pixels"},
	{20, RIFFFieldInfo::kuword, "biPlanes: number of bitplanes"},
	{22, RIFFFieldInfo::kuword, "biBitCount: number of bits per pixel"},
	{24, RIFFFieldInfo::kfcc,	"biCompression: encoding algorithm (0 = BI_RGB)"},
	{28, RIFFFieldInfo::kulong, "biSizeImage: size of compressed image"},
	{32, RIFFFieldInfo::kulong, "biXPelsPerMeter: horizontal resolution in pixels per meter (typically ignored)"},
	{36, RIFFFieldInfo::kulong, "biYPelsPerMeter: vertical resolution in pixels per meter (typically ignored)"},
	{40, RIFFFieldInfo::kulong, "biClrUsed: number of palette entries used by bitmap"},
	{44, RIFFFieldInfo::kulong, "biClrImportant: number of palette entries \"required\" to display bitmap"},
	{48}
};

static const struct RIFFChunkInfo {
	unsigned long id;
	const char *desc;
	const RIFFFieldInfo *fields;
} g_RIFFChunks[]={
	{ ' IVA', "Audio/video interleave file" },
	{ 'XIVA', "AVI2 extension block" },
	{ 'EVAW', "Sound waveform" },
	0,
}, g_LISTChunks[]={
	{ 'lrdh', "AVI file header block" },
	{ 'lrts', "AVI stream header block" },
	{ 'lmdo', "AVI2 extended header block" },
	{ 'ivom', "AVI data block" },
	0,
}, g_JustChunks[]={
	{ 'KNUJ', "padding" },
	{ 'hiva', "AVI file header", g_Fields_avih },
	{ 'hrts', "AVI stream header", g_Fields_strh },
	{ 'frts', "AVI stream format" },
	{ 'drts', "AVI stream codec data" },
	{ 'xdni', "AVI2 hierarchical indexing data" },
	{ 'hlmd', "AVI2 extended header" },
	{ '1xdi', "AVI legacy index" },
	{ 'mges', "VirtualDub next segment pointer" },
	0
},
g_Chunk_strf_audio={ 'strf', "AVI stream format (audio)", g_Fields_strf_audio },
g_Chunk_strf_video={ 'strf', "AVI stream format (video)", g_Fields_strf_video };


static const char *LookupRIFFChunk(const RIFFChunkInfo *tbl, unsigned long ckid) {
	while(tbl->id) {
		if (tbl->id == ckid)
			return tbl->desc;
		++tbl;
	}

	return "unknown";
}

static const RIFFChunkInfo *LookupRIFFChunk(unsigned long ckid) {
	const RIFFChunkInfo *tbl = g_JustChunks;

	while(tbl->id) {
		if (tbl->id == ckid)
			return tbl;

		++tbl;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////////

class HVModifiedLine : public ListNode2<HVModifiedLine> {
public:
	char			data[16];
	__int64			address;
	int				mod_flags;
private:

public:
	HVModifiedLine(__int64 addr);
};

HVModifiedLine::HVModifiedLine(__int64 addr)
	: address(addr)
	, mod_flags(0)
{}

//////////////////////////////////////////////////////////////////////////////

class IHexViewerDataSource {
public:
	virtual const char *GetRow(__int64 start, int& len, long& modified_mask) throw() = 0;		// 16 bytes
	virtual void UndoByte(__int64 byte) throw() = 0;
	virtual void ModifyByte(__int64 byte, char v, char mask) throw() = 0;
	virtual void NewLocation(__int64 pos) throw() = 0;
};

//////////////////////////////////////////////////////////////////////////////

class HexViewer {
public:
	HexViewer(HWND hwnd) throw();
	~HexViewer() throw();

	inline __int64 GetPosition() const throw() { return i64Position; }

	void SetDataSource(IHexViewerDataSource *pDS) throw();
	void SetDetails(__int64 total_size, bool bWrite) throw();
	void SetHighlight(__int64 start, __int64 end) throw();
	void SetMetaHighlight(int offset, int len) throw();

	void ScrollVisible(__int64 nVisPos) throw();
	void MoveToByte(__int64 pos) throw();
	void ScrollTopTo(long lLine) throw();

	void InvalidateLine(__int64 address) throw();
	void InvalidateRegion(__int64 start, __int64 end) throw();

	static LRESULT APIENTRY HexViewerWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) throw();
private:
	const HWND	hwnd;

	HFONT hfont;

	__int64	i64TopOffset;
	__int64 i64FileSize;
	__int64	i64Position;
	__int64	mHighlightStart, mHighlightEnd;
	__int64	mMetaHiStart, mMetaHiEnd;
	int		iMouseWheelDelta;
	int		nCurrentVisLines;
	int		nCurrentWholeLines;
	int		nCharWidth;
	int		nLineHeight;
	int		nLineLimit;
	bool	bCharMode;
	bool	bOddHex;
	bool	bCaretHidden;
	bool	bEnableWrite;

	IHexViewerDataSource	*mpDataSource;

	void Init() throw();

	void MoveCaret() throw();

	void Hide() throw() {
		if (!bCaretHidden) {
			bCaretHidden = true;
			HideCaret(hwnd);
		}
	}

	void Show() throw() {
		if (bCaretHidden) {
			bCaretHidden = false;
			ShowCaret(hwnd);
		}
	}

	LRESULT Handle_WM_MOUSEWHEEL(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_VSCROLL(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_SIZE(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_KEYDOWN(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_CHAR(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_LBUTTONDOWN(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_PAINT(WPARAM wParam, LPARAM lParam) throw();
};

HexViewer::HexViewer(HWND _hwnd) throw()
	: hwnd(_hwnd)
	, hfont(NULL)
	, mpDataSource(NULL)
	, bCaretHidden(true)
{
	SetDetails(0, false);
}

HexViewer::~HexViewer() throw() {
	if (hfont)
		DeleteObject(hfont);
}

void HexViewer::Init() throw() {
	HDC hdc;

	nLineHeight = 16;
	if (hdc = GetDC(hwnd)) {
		TEXTMETRIC tm;
		HGDIOBJ hfOld;

		hfOld = SelectObject(hdc, GetStockObject(ANSI_FIXED_FONT));

		GetTextMetrics(hdc, &tm);

		hfont = CreateFont(tm.tmHeight, 0, 0, 0, 0, FALSE, FALSE, FALSE, ANSI_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH|FF_DONTCARE, "Lucida Console");

		SelectObject(hdc, hfont ? hfont : GetStockObject(ANSI_FIXED_FONT));

		GetTextMetrics(hdc, &tm);
		nCharWidth	= tm.tmAveCharWidth;
		nLineHeight = tm.tmHeight;

		SelectObject(hdc, hfOld);

		ReleaseDC(hwnd, hdc);
	}
}

void HexViewer::SetDataSource(IHexViewerDataSource *pDS) throw() {
	mpDataSource = pDS;
}

void HexViewer::SetDetails(__int64 total_size, bool bWrite) throw() {
	i64FileSize		= total_size;
	i64TopOffset	= 0;
	i64Position		= 0;
	mHighlightStart = mHighlightEnd = 0;
	mMetaHiStart	= mMetaHiEnd = 0;
	nLineLimit		= (int)((i64FileSize+15)>>4);
	bCharMode		= false;
	bOddHex			= false;
	bEnableWrite	= bWrite;

	SetScrollRange(hwnd, SB_VERT, 0, nLineLimit-1, TRUE);

	InvalidateRect(hwnd, NULL, TRUE);
}

static int sorter64(const void *p1, const void *p2) {
	const __int64 n1 = *(const __int64 *)p1;
	const __int64 n2 = *(const __int64 *)p2;

	return n1>n2 ? 1 : n1<n2 ? -1 : 0;
}

void HexViewer::SetHighlight(__int64 start, __int64 end) throw() {

	// This is cheesy as hell, but throw all four addresses into an array,
	// sort it, and invalidate 0-1 and 2-3.

	__int64 array[4] = { start, end, mHighlightStart, mHighlightEnd };

	qsort(array, 4, sizeof array[0], sorter64);

	InvalidateRegion(array[0], array[1]);
	InvalidateRegion(array[2], array[3]);

	mHighlightStart = start;
	mHighlightEnd = end;

	// reset the metahighlight

	SetMetaHighlight(0,0);
}

void HexViewer::SetMetaHighlight(int offset, int len) throw() {
	// clip

	if (offset < 0) {
		len -= offset;
		offset = 0;
	}

	if (mHighlightStart+offset+len > mHighlightEnd)
		len = (int)(mHighlightEnd - offset);

	__int64 array[4] = { mMetaHiStart, mMetaHiEnd, mHighlightStart+offset, mHighlightStart+offset+len };

	mMetaHiStart = array[2];
	mMetaHiEnd = array[3];

	qsort(array, 4, sizeof array[0], sorter64);

	InvalidateRegion(array[0], array[1]);
	InvalidateRegion(array[2], array[3]);
}

void HexViewer::ScrollVisible(__int64 nVisPos) throw() {
	__int64 nTopLine	= i64TopOffset>>4;
	__int64 nCaretLine	= i64Position>>4;

	if (nCaretLine < nTopLine)
		ScrollTopTo((long)nCaretLine);
	else if (nCaretLine >= nTopLine + nCurrentWholeLines)
		ScrollTopTo((long)(nCaretLine - nCurrentWholeLines + 1));
}

void HexViewer::MoveToByte(__int64 pos) throw() {
	if (pos < 0) {
		bOddHex = false;
		pos = 0;
	} else if (pos >= i64FileSize) {
		pos = i64FileSize - 1;
		bOddHex = !bCharMode;
	}
	i64Position = pos;
	ScrollVisible(pos);
	MoveCaret();

	if (mpDataSource)
		mpDataSource->NewLocation(pos);
}

void HexViewer::ScrollTopTo(long lLine) throw() {
	HDC hdc;
	RECT rRedraw;

	if (lLine < 0)
		lLine = 0;

	if (lLine > nLineLimit)
		lLine = nLineLimit;

	long delta = lLine - (long)(i64TopOffset>>4);

	if (!delta)
		return;

	iMouseWheelDelta = 0;

	SetScrollPos(hwnd, SB_VERT, lLine, TRUE);
	i64TopOffset = (__int64)lLine<<4;

	Hide();
	if (abs(delta) > nCurrentVisLines) {
		InvalidateRect(hwnd, NULL, TRUE);
	} else {
	   if (hdc = GetDC(hwnd)) {
		   ScrollDC(hdc, 0, -delta*nLineHeight, NULL, NULL, NULL, &rRedraw);
		   ReleaseDC(hwnd, hdc);
		   InvalidateRect(hwnd, &rRedraw, TRUE);
		   UpdateWindow(hwnd);
	   }
	}
	MoveCaret();
}

void HexViewer::InvalidateRegion(__int64 start, __int64 end) throw() {
	if (start >= end || end <= i64TopOffset || start >= (i64TopOffset + nCurrentVisLines*16))
		return;

	long visidx1 = (long)((start - i64TopOffset) >> 4);
	long visidx2 = (long)((end - i64TopOffset + 15) >> 4);

	RECT r;

	GetClientRect(hwnd, &r);

	r.top		= nLineHeight * visidx1;
	r.bottom	= nLineHeight * visidx2;

	InvalidateRect(hwnd, &r, FALSE);
}

void HexViewer::InvalidateLine(__int64 address) throw() {
	long visidx = (long)((address - i64TopOffset) >> 4);
	RECT r;

	if (visidx < 0 || visidx >= nCurrentVisLines)
		return;

	GetClientRect(hwnd, &r);
	r.top		= nLineHeight * visidx;
	r.bottom	= r.top + nLineHeight;

	InvalidateRect(hwnd, &r, TRUE);
}

///////////////////////////////////////////////////////////////////////////

void HexViewer::MoveCaret() throw() {
	__int64 nTopLine	= i64TopOffset>>4;
	__int64 nCaretLine	= i64Position>>4;

	if (nCaretLine < nTopLine || nCaretLine >= nTopLine + nCurrentVisLines) {
		Hide();
		return;
	}

	int nLine, nByteOffset, x, y;

	nLine			= (int)(nCaretLine - nTopLine);
	nByteOffset		= (int)i64Position & 15;

	y = nLine * nLineHeight;

	if (bCharMode) {
		x = 14 + 3*16 + 1 + nByteOffset;
	} else {
		x = 14 + 3*nByteOffset;

		if (bOddHex)
			++x;
	}

	SetCaretPos(x*nCharWidth, y);
	Show();
}

LRESULT HexViewer::Handle_WM_MOUSEWHEEL(WPARAM wParam, LPARAM lParam) throw() {
	int iNewDelta, nScroll;
	
	iNewDelta = iMouseWheelDelta - (signed short)HIWORD(wParam);
	nScroll = iNewDelta / WHEEL_DELTA;

	if (nScroll) {
		ScrollTopTo((long)(i64TopOffset>>4) + nScroll);
		iNewDelta -= WHEEL_DELTA * nScroll;
	}

	iMouseWheelDelta = iNewDelta;

	return 0;
}

LRESULT HexViewer::Handle_WM_VSCROLL(WPARAM wParam, LPARAM lParam) throw() {
	SCROLLINFO si;

	switch(LOWORD(wParam)) {
	case SB_BOTTOM:		ScrollTopTo(nLineLimit); break;
	case SB_TOP:		ScrollTopTo(0); break;
	case SB_LINEUP:		ScrollTopTo((long)(i64TopOffset>>4) - 1); break;
	case SB_LINEDOWN:	ScrollTopTo((long)(i64TopOffset>>4) + 1); break;
	case SB_PAGEUP:		ScrollTopTo((long)(i64TopOffset>>4) - (nCurrentVisLines - 1)); break;
	case SB_PAGEDOWN:	ScrollTopTo((long)(i64TopOffset>>4) + (nCurrentVisLines - 1)); break;
	case SB_THUMBTRACK:
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_TRACKPOS;
		GetScrollInfo(hwnd, SB_VERT, &si);
		ScrollTopTo(si.nTrackPos);
		break;
	}

	return 0;
}

LRESULT HexViewer::Handle_WM_SIZE(WPARAM wParam, LPARAM lParam) throw() {
	RECT r;

	GetClientRect(hwnd, &r);

	nCurrentWholeLines	= (r.bottom - r.top) / nLineHeight; 
	nCurrentVisLines	= (r.bottom - r.top + nLineHeight - 1) / nLineHeight;

	return 0;
}

LRESULT HexViewer::Handle_WM_KEYDOWN(WPARAM wParam, LPARAM lParam) throw() {
	switch(wParam) {
	case VK_UP:
		MoveToByte(i64Position-16);
		break;
	case VK_DOWN:
		MoveToByte(i64Position+16);
		break;
	case VK_LEFT:
		if (bCharMode || (bOddHex = !bOddHex))
			MoveToByte(i64Position-1);
		else
			MoveToByte(i64Position);
		break;
	case VK_RIGHT:
		if (bCharMode || !(bOddHex = !bOddHex))
			MoveToByte(i64Position+1);
		else
			MoveToByte(i64Position);
		break;
	case VK_PRIOR:
		MoveToByte(i64Position - (nCurrentVisLines-1)*16);
		break;
	case VK_NEXT:
		MoveToByte(i64Position + (nCurrentVisLines-1)*16);
		break;
	case VK_HOME:
		bOddHex = false;
		if ((signed short)GetKeyState(VK_CONTROL)<0)
			MoveToByte(0);
		else
			MoveToByte(i64Position & -16i64);
		break;
	case VK_END:
		bOddHex = true;
		if ((signed short)GetKeyState(VK_CONTROL)<0)
			MoveToByte(i64FileSize - 1);
		else
			MoveToByte(i64Position | 15);
		break;
	case VK_TAB:
		bCharMode = !bCharMode;
		bOddHex = false;
		MoveToByte(i64Position);
		break;
	default:
		return SendMessage(GetParent(hwnd), WM_KEYDOWN, wParam, lParam);
	}

	return 0;
}

LRESULT HexViewer::Handle_WM_CHAR(WPARAM wParam, LPARAM lParam) throw() {
	int key = wParam;

	if (!bEnableWrite || !mpDataSource)
		return 0;

	if (key == '\b') {
		if (bCharMode || !bOddHex)
			MoveToByte(i64Position-1);
		else
			MoveToByte(i64Position);

		bOddHex = false;

		mpDataSource->UndoByte(i64Position);

		return 0;
	}

	if (!isprint(key) || (!bCharMode && !isxdigit(key)))
		return 0;

	char mask = (char)(bOddHex?0xf0:0x0f);

	if (bCharMode)
		mpDataSource->ModifyByte(i64Position, key, (char)0);
	else {
		int v = toupper(key) - '0';
		if (v > 9)
			v -= 7;

		if (bOddHex)
			mpDataSource->ModifyByte(i64Position, v, (char)0xf0);
		else
			mpDataSource->ModifyByte(i64Position, v<<4, 0x0f);
	}	

	// Advance right one char

	SendMessage(hwnd, WM_KEYDOWN, VK_RIGHT, 0);

	return 0;
}

LRESULT HexViewer::Handle_WM_LBUTTONDOWN(WPARAM wParam, LPARAM lParam) throw() {
	int x, y;

	x = LOWORD(lParam) / nCharWidth;
	y = HIWORD(lParam) / nLineHeight;

   if (x < 14)
      x = 14;
   else if (x >= 63+16)
      x = 63+15;

	if (x >= 14 && x < 61) {
		x -= 13;

		bCharMode = false;
		bOddHex = false;

		if (x%3 == 2)
			bOddHex = true;

		x = x/3;
	} else if (x >= 63 && x < 63+16) {
		bCharMode = true;
		bOddHex = false;

		x -= 63;
	} else
		return 0;

	MoveToByte(i64TopOffset + y*16 + x);

	return 0;
}

static int clip_to_row(__int64 v) {
	return v<0 ? 0 : v>16 ? 16 : (int)v;
}

LRESULT HexViewer::Handle_WM_PAINT(WPARAM wParam, LPARAM lParam) throw() {
	HDC hdc;
	PAINTSTRUCT ps;
	char buf[128];
	__int64 i64Offset;
	int y;
	RECT r;
	int i;

	hdc = BeginPaint(hwnd, &ps);

	GetClientRect(hwnd, &r);
	r.left = nCharWidth*79;
	FillRect(hdc, &r, (HBRUSH)(COLOR_WINDOW+1));

	i = GetClipBox(hdc, &r);

	if (i != ERROR && i != NULLREGION) {
		y = r.top - r.top % nLineHeight;

		if (mpDataSource) {
			HGDIOBJ hfOld;

			i64Offset = i64TopOffset + (y/nLineHeight)*16;

			hfOld = SelectObject(hdc, hfont ? hfont : GetStockObject(ANSI_FIXED_FONT));

			SetTextAlign(hdc, TA_TOP | TA_LEFT);
			SetBkMode(hdc, OPAQUE);

			while(y < r.bottom+nLineHeight-1 && i64Offset < i64FileSize) {
				const char *data;
				long mod_flags;
				char *s;
				int i, len;

				data = mpDataSource->GetRow(i64Offset, len, mod_flags);

				s = buf + sprintf(buf, "%12I64X: ", i64Offset);

				for(i=0; i<len; i++) {
					*s++ = hexdig[(unsigned char)data[i] >> 4];
					*s++ = hexdig[data[i] & 15];
					*s++ = ' ';
				}

				while(i<16) {
					*s++ = ' ';
					*s++ = ' ';
					*s++ = ' ';
					++i;
				}

				s[-8*3-1] = '-';
				*s++ = ' ';

				for(i=0; i<len; i++) {
					if (data[i]>=0x20 && data[i]<0x7f)
						*s++ = data[i];
					else
						*s++ = '.';
				}

				while(i<16) {
					*s++ = ' ';
					*s++ = ' ';
					*s++ = ' ';
					++i;
				}

				// Does the highlight region overlap?

				if (mHighlightEnd > i64Offset && mHighlightStart < i64Offset + 16) {
					int hilite_start	= clip_to_row(mHighlightStart - i64Offset);
					int hilite_end		= clip_to_row(mHighlightEnd   - i64Offset);
					int metahi_start	= clip_to_row(mMetaHiStart - i64Offset);
					int metahi_end		= clip_to_row(mMetaHiEnd   - i64Offset);

					// Split into nine (!) regions.

					const int pos1	= 0;					// white: address and hex prebytes
					const int pos2	= 14 + 3*hilite_start;	// orange: hex bytes
					const int pos3	= 14 + 3*metahi_start;	// green: hex bytes
					const int pos4	= 13 + 3*metahi_end;	// orange: hex bytes
					const int pos5	= 13 + 3*hilite_end;	// white: hex postbytes and ascii prebytes
					const int pos6	= 63 + hilite_start;	// orange: ascii bytes
					const int pos7	= 63 + metahi_start;	// green: ascii bytes
					const int pos8	= 63 + metahi_end;		// orange: ascii bytes
					const int pos9	= 63 + hilite_end;		// white: ascii postbytes;
					const int pos10	= 63 + 16;				// and finally, the end

					// Draw metahighlighted areas.

					SetBkColor(hdc, 0xe0ffc0);
					TextOut(hdc, nCharWidth*pos3, y, buf + pos3, pos4 - pos3);
					TextOut(hdc, nCharWidth*pos7, y, buf + pos7, pos8 - pos7);

					// Draw highlighted areas.

					SetBkColor(hdc, 0xc0e0ff);
					TextOut(hdc, nCharWidth*pos2, y, buf + pos2, pos3 - pos2);
					TextOut(hdc, nCharWidth*pos4, y, buf + pos4, pos5 - pos4);
					TextOut(hdc, nCharWidth*pos6, y, buf + pos6, pos7 - pos6);
					TextOut(hdc, nCharWidth*pos8, y, buf + pos8, pos9 - pos8);

					// Draw non-highlighted areas.

					SetBkColor(hdc, 0xffffff);
					TextOut(hdc, nCharWidth*pos1, y, buf + pos1, pos2 - pos1);
					TextOut(hdc, nCharWidth*pos5, y, buf + pos5, pos6 - pos5);
					TextOut(hdc, nCharWidth*pos9, y, buf + pos9, pos10 - pos9);
				} else {

					// Not highlighting, draw single line of text.

					SetBkColor(hdc, 0xffffff);
					TextOut(hdc, 0, y, buf, s - buf);
				}

				// Draw modified constants in blue.

				if (mod_flags) {
					COLORREF crOldTextColor = SetTextColor(hdc, 0xFF0000);

					for(i=0; i<16; i++)
						if (!(mod_flags & (1<<i))) {
							buf[14 + i*3] = ' ';
							buf[15 + i*3] = ' ';
							buf[63+i] =  ' ';
						}

					buf[37] = ' ';

					SetBkMode(hdc, TRANSPARENT);

					TextOut(hdc, nCharWidth*14, y, buf+14, 16*3-1);
					TextOut(hdc, nCharWidth*63, y, buf+63, 16);

					SetBkMode(hdc, OPAQUE);

					SetTextColor(hdc, crOldTextColor);
				}

				y += nLineHeight;
				i64Offset += 16;
			}

			SelectObject(hdc, hfOld);
		}

		if (y < r.bottom+nLineHeight-1) {
			GetClientRect(hwnd, &r);
			r.top = y;
			FillRect(hdc, &r, (HBRUSH)(COLOR_WINDOW+1));
		}
	}
	EndPaint(hwnd, &ps);

	return 0;
}

LRESULT APIENTRY HexViewer::HexViewerWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) throw() {
	HexViewer *pcd = (HexViewer *)GetWindowLong(hwnd, 0);

	switch(msg) {

	case WM_NCCREATE:
		if (!(pcd = new HexViewer(hwnd)))
			return FALSE;

		SetWindowLong(hwnd, 0, (LONG)pcd);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_CREATE:
		pcd->Init();
		return 0;

	case WM_SIZE:
		return pcd->Handle_WM_SIZE(wParam, lParam);

	case WM_DESTROY:
		delete pcd;
		SetWindowLong(hwnd, 0, 0);
		break;

	case WM_MOUSEWHEEL:
		return pcd->Handle_WM_MOUSEWHEEL(wParam, lParam);

	case WM_KEYDOWN:
		return pcd->Handle_WM_KEYDOWN(wParam, lParam);

	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;

	case WM_VSCROLL:
		return pcd->Handle_WM_VSCROLL(wParam, lParam);

	case WM_SETFOCUS:
		CreateCaret(hwnd, NULL, pcd->nCharWidth, pcd->nLineHeight);
		pcd->bCaretHidden = true;
		pcd->MoveCaret();
		return 0;

	case WM_KILLFOCUS:
		DestroyCaret();
		return 0;

	case WM_LBUTTONDOWN:
		return pcd->Handle_WM_LBUTTONDOWN(wParam, lParam);

	case WM_CHAR:
		return pcd->Handle_WM_CHAR(wParam, lParam);

	case WM_PAINT:
		return pcd->Handle_WM_PAINT(wParam, lParam);

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

class HexEditor : public IHexViewerDataSource {
private:
	const HWND	hwnd;
	HWND	hwndFind;
	HWND	hwndTree;
	HWND	hwndStatus;
	HANDLE	hFile;
	HFONT	hfont;
	__int64 i64FileSize;

	HexViewer *mpView;
	HWND	hwndView;

	__int64	i64FileCacheAddr;
	__int64 i64FileReadPosition;

	char	rowcache[16];
	__int64	i64RowCacheAddr;

	List2<HVModifiedLine>	listMods;

	ModelessDlgNode	mdnFind;
	char	*pszFindString;
	int		nFindLength;
	bool	bFindCaseInsensitive;
	bool	bFindHex;
	bool	bFindReverse;

	bool	bEnableWrite;
	bool	bEnableAVIAssist;

	// put this last since it's so big
	
	union {
		char	filecache[4096];
		double	__unused;			// force 8-alignment
	};

public:
	HexEditor(HWND);
	~HexEditor();

	static LRESULT APIENTRY HexEditorWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) throw();
	static BOOL APIENTRY FindDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) throw();
	static BOOL APIENTRY TreeDlgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) throw();

private:
	void Init() throw();
	void Open() throw();
	void Open(const char *pszFile, bool bRW) throw();
	void Close() throw();
	void Commit() throw();

	const char *GetRow(__int64 start, int& len, long& modified_mask) throw();
	void UndoByte(__int64 byte) throw();
	void ModifyByte(__int64 i64Position, char v, char mask) throw();
	void NewLocation(__int64 i64Position) throw();

	const char *FillRowCache(__int64 line) throw();
	void InvalidateLine(__int64 line) throw();

	void SetStatus(const char *format, ...) throw();

	LRESULT Handle_WM_COMMAND(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_KEYDOWN(WPARAM wParam, LPARAM lParam) throw();
	LRESULT Handle_WM_SIZE(WPARAM wParam, LPARAM lParam) throw();

	static BOOL CALLBACK AskForValuesDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) throw();
	bool AskForValues(const char *title, const char *name1, const char *name2, __int64& default1, __int64& default2, int (HexEditor::*verifier)(HWND hdlg, __int64 v1, __int64 v2) throw()) throw();
	int JumpVerifier(HWND hdlg, __int64 v1, __int64 v2) throw();
	int ExtractVerifier(HWND hdlg, __int64 v1, __int64 v2) throw();
	int TruncateVerifier(HWND hdlg, __int64 v1, __int64 v2) throw();

	void Extract() throw();
	void Find() throw();
	void _RIFFScan(struct RIFFScanInfo &rsi, HWND hwndTV, HTREEITEM hti, __int64 pos, __int64 sizeleft);
	void RIFFTree(HWND hwndTV) throw();

	HVModifiedLine *FindModLine(__int64 addr) throw() {
		HVModifiedLine *pLine, *pLineNext;

		pLine = listMods.AtHead();

		while(pLineNext = pLine->NextFromHead()) {
			if (addr == pLine->address)
				return pLine;

			pLine = pLineNext;
		}

		return NULL;
	}

	bool IsValidHeaderAt(__int64 i64Position, bool bChain, unsigned long& length, unsigned long& ckid) throw();
};

////////////////////////////

HexEditor::HexEditor(HWND _hwnd) : hwnd(_hwnd), hwndFind(0), hwndTree(0), pszFindString(NULL), bFindReverse(false), hfont(0) {
	hFile = INVALID_HANDLE_VALUE;

	i64FileSize = 0;
	i64RowCacheAddr = -1;
	i64FileCacheAddr = -1;
}

HexEditor::~HexEditor() {
	delete[] pszFindString;
	pszFindString = NULL;

	Close();

	if (hwndTree)
		DestroyWindow(hwndTree);

	if (hwndFind)
		DestroyWindow(hwndFind);

	if (hfont)
		DeleteObject(hfont);
}

void HexEditor::Init() {
	HDC hdc;

	if (hdc = GetDC(hwnd)) {
		TEXTMETRIC tm;
		HGDIOBJ hfOld;

		hfOld = SelectObject(hdc, GetStockObject(ANSI_FIXED_FONT));

		GetTextMetrics(hdc, &tm);

		hfont = CreateFont(tm.tmHeight, 0, 0, 0, 0, FALSE, FALSE, FALSE, ANSI_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, FIXED_PITCH|FF_DONTCARE, "Lucida Console");

		SelectObject(hdc, hfOld);
		ReleaseDC(hwnd, hdc);
	}

	hwndStatus = CreateStatusWindow(WS_CHILD|WS_VISIBLE, "", hwnd, 500);

	hwndView = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		szHexViewerClassName,
		"",
		WS_VISIBLE|WS_CHILD|WS_VSCROLL,
		0,0,0,0,
		hwnd,
		NULL,
		g_hInst,
		NULL);

	mpView = (HexViewer *)GetWindowLong(hwndView, 0);
}

void HexEditor::Open() {
	char szName[MAX_PATH];
	OPENFILENAME ofn;

	szName[0] = 0;

	memset(&ofn, 0, sizeof ofn);

	ofn.lStructSize			= OPENFILENAME_SIZE_VERSION_400;
	ofn.hwndOwner			= hwnd;
	ofn.lpstrFilter			= "All files (*.*)\0*.*\0";
	ofn.lpstrCustomFilter	= NULL;
	ofn.nFilterIndex		= 1;
	ofn.lpstrFile			= szName;
	ofn.nMaxFile			= sizeof szName;
	ofn.lpstrFileTitle		= NULL;
	ofn.lpstrInitialDir		= NULL;
	ofn.lpstrTitle			= NULL;
	ofn.Flags				= OFN_EXPLORER | OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_READONLY;
	ofn.lpstrDefExt			= NULL;

	if (GetOpenFileName(&ofn))
		Open(szName, !(ofn.Flags & OFN_READONLY));
}

void HexEditor::Open(const char *pszFile, bool bRW) {
	Close();

	bEnableWrite = bRW;

	if (bRW)
		hFile = CreateFile(pszFile, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	else
		hFile = CreateFile(pszFile, GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		MyWin32Error("Cannot open file: %%s", GetLastError()).post(hwnd, "Hex editor error");
		return;
	}

	char buf[512];

	wsprintf(buf, "VirtualDub Hex Editor - [%s]%s", pszFile, bRW ? "" : " (read only)");
	SetWindowText(hwnd, buf);

	DWORD dwLow, dwHigh;
	
	dwLow = GetFileSize(hFile, &dwHigh);

	i64FileSize = dwLow | ((__int64)dwHigh << 32);

	i64RowCacheAddr	= -1;
	i64FileCacheAddr	= -1;
	i64FileReadPosition = 0;

	mpView->SetDataSource(this);
	mpView->SetDetails(i64FileSize, bRW);

	SetStatus("%s: %I64d bytes", pszFile, i64FileSize);

	{
		int len;
		long mask;

		const unsigned long *pHeader = (const unsigned long *)GetRow(0, len, mask);

		bEnableAVIAssist = (pHeader[0] == 'FFIR' && pHeader[2]==' IVA');
	}
}

void HexEditor::Close() {
	HVModifiedLine *pLine;

	while(pLine = listMods.RemoveHead())
		delete pLine;

	if (hFile == INVALID_HANDLE_VALUE)
		return;

	CloseHandle(hFile);
	hFile = INVALID_HANDLE_VALUE;

	mpView->SetDataSource(NULL);
	mpView->SetDetails(0, false);

	if (hwndTree)
		DestroyWindow(hwndTree);

	SetWindowText(hwnd, "VirtualDub Hex Editor");
	SetStatus("");
}

void HexEditor::Commit() {
	HVModifiedLine *pLine;

	while(pLine = listMods.RemoveHead()) {
		DWORD dwBytes = 16, dwActual;

		if (((unsigned __int64)pLine->address>>32) == ((unsigned __int64)i64FileSize >> 32))
			if (!(((long)pLine->address ^ (long)i64FileSize) & 0xfffffff0))
				dwBytes = (long)i64FileSize - (long)pLine->address;

		SetFilePointer(hFile, (LONG)pLine->address, (LONG *)&pLine->address + 1, FILE_BEGIN);
		WriteFile(hFile, pLine->data, dwBytes, &dwActual, NULL);

		i64FileReadPosition = pLine->address + 16;

		delete pLine;
	}

	InvalidateRect(hwnd, NULL, TRUE);
}

const char *HexEditor::GetRow(__int64 start, int& len, long& modified_mask) throw() {
	HVModifiedLine *pModLine = FindModLine(start);
	const char *pszData;
	
	if (pModLine) {
		modified_mask = pModLine->mod_flags;
		pszData = pModLine->data;
	} else {
		modified_mask = 0;
		pszData = FillRowCache(start);
	}

	len = (start < i64FileSize-16) ? 16 : (long)(i64FileSize - start);

	return pszData;
}

void HexEditor::UndoByte(__int64 i64Position) throw() {
	HVModifiedLine *pLine;
	__int64 i64Offset;

	i64Offset = i64Position & -16i64;

	if (pLine = FindModLine(i64Offset)) {
		// Revert byte.

		int offset = (int)i64Position & 15;

		pLine->data[offset] = FillRowCache(i64Offset)[offset];
		pLine->mod_flags &= ~(1<<offset);

		if (!pLine->mod_flags) {
			pLine->Remove();
			delete pLine;
		}

		mpView->InvalidateLine(i64Offset);
	}
}

void HexEditor::ModifyByte(__int64 i64Position, char v, char mask) throw() {
	// Fetch the mod line.

	__int64 i64Offset = i64Position & -16i64;
	HVModifiedLine *pLine = FindModLine(i64Offset);

	if (!pLine) {
		// Line not resident -- fetch from disk and create.

		DWORD dwActual;

		pLine = new HVModifiedLine(i64Offset);

		SetFilePointer(hFile, (LONG)i64Offset, (LONG *)&i64Offset + 1, FILE_BEGIN);
		ReadFile(hFile, pLine->data, 16, &dwActual, NULL);

		i64FileReadPosition = i64Offset + 16;

		listMods.AddTail(pLine);
	}

	// Modify the appropriate byte and redraw the line.

	int offset = (int)i64Position & 15;

	pLine->data[offset] = (pLine->data[offset]&mask) + v;
	pLine->mod_flags |= 1<<offset;

	// invalidate row cache and display

	i64RowCacheAddr = -1;

	mpView->InvalidateLine(i64Position);
}

bool HexEditor::IsValidHeaderAt(__int64 i64Position, bool bChain, unsigned long& length, unsigned long& ckid) throw() {
	union {
		char charbuf[12];
		unsigned long longbuf[4];
	};

	if (i64Position > i64FileSize - 8)
		return false;

	int len;
	int offset = ((long)i64Position&15);
	long modmask;

	if (offset > 4) {
		const char *row2 = GetRow(i64Position - offset + 16, len, modmask);

		memcpy(charbuf + (16-offset), row2, offset-4);
	}

	const char *row1 = GetRow(i64Position - offset, len, modmask);

	memcpy(charbuf, row1+offset, offset>4 ? 16-offset : 12);

	// All four characters in the FOURCC need to be alphanumeric.

	if (!isValidFOURCC(longbuf[0]))
		return false;

	// If it is RIFF or LIST, the second FOURCC must also be alphanumeric.

	if (longbuf[0] == 'TSIL')
		if (!isValidFOURCC(longbuf[2]))
			return false;

	// Size must fit in the remainder of file.

	__int64 end = i64Position + 8 + longbuf[1];

	if (end > i64FileSize)
		return false;

	// Confirm that what comes after is also a chunk.

	if (!bChain || end == i64FileSize || IsValidHeaderAt((end+1)&~1i64, false, length, ckid)) {
		length = longbuf[1];
		ckid = longbuf[0];
		return true;
	}

	return false;
}

void HexEditor::NewLocation(__int64 i64Position) throw() {
	if (bEnableAVIAssist) {
		__int64 basepos = i64Position;

		// Step back up to 4K, looking for a valid chunk.

		i64Position &= ~1i64;

		__int64 limit = i64Position - 1024;

		if (limit < 0)
			limit = 0;

		while(i64Position >= limit) {
			unsigned long len;
			unsigned long ckid;

			if (IsValidHeaderAt(i64Position, true, len, ckid)) {
				mpView->SetHighlight(i64Position, i64Position+8+len);

				// Attempt to lookup field

				const RIFFChunkInfo *rci = LookupRIFFChunk(ckid);

				// The 'strf' chunk is a special case, because it has a different interpretation
				// depending on whether the stream is an audio or video stream.  We cheat a little
				// bit and check if the previous chunk is the 'strh' chunk; it may be 56 or 64
				// bytes depending on which utility wrote it.

				if (ckid == 'frts') {
					unsigned long len2, ckid2;

					if (	IsValidHeaderAt(i64Position - 64, true, len2, ckid2)
						||	IsValidHeaderAt(i64Position - 72, true, len2, ckid2))
					{
						int len;
						long modmask;
						const char *data = GetRow(i64Position - len2, len, modmask);

						switch(data[(int)(i64Position - len2) & 15]) {
						case 'a':
							rci = &g_Chunk_strf_audio;
							break;
						case 'v':
							rci = &g_Chunk_strf_video;
							break;
						}
					}
				}

				if (rci) {
					const RIFFFieldInfo *rfi = rci->fields;

					if (rfi) {
						int offset = (int)basepos - (int)i64Position;
						int size;

						while(rfi[0].name && rfi[1].offset <= offset)
							++rfi;

						if (rfi[0].name) {
							switch(rfi[0].type) {
							case RIFFFieldInfo::kfcc:
							case RIFFFieldInfo::kulong:
							case RIFFFieldInfo::kslong:
								size = 4;
								break;
							case RIFFFieldInfo::kuword:
							case RIFFFieldInfo::ksword:
								size = 2;
								break;
							case RIFFFieldInfo::kubyte:
							case RIFFFieldInfo::ksbyte:
								size = 1;
								break;
							default:
								__assume(false);
							}

							if (offset >= rfi[0].offset && offset < rfi[0].offset + size) {
								mpView->SetMetaHighlight(rfi[0].offset, size);
								SetStatus(rfi[0].name);
							} else {
								SetStatus("");
							}
						}
					}
				}

				return;
			}

			i64Position -= 2;
		}
	}

	mpView->SetHighlight(0,0);
	SetStatus("");
}

const char *HexEditor::FillRowCache(__int64 i64Offset) throw() {
	if (i64Offset == i64RowCacheAddr)
		return rowcache;

	__int64 i64PageOffset = i64Offset & 0xfffffffffffff000;
	DWORD dwActual = 0;

	if (i64PageOffset != i64FileCacheAddr) {
		if (i64FileReadPosition != i64PageOffset) {

			// if the desired position is less than 128K away, just read through

			__int64 delta = i64PageOffset - i64FileReadPosition;

			if (i64FileReadPosition >= 0 && delta > 0 && delta <= 131072) {
				long lDelta = (long)delta;

				while(lDelta > 0) {
					DWORD tc = 4096 - (lDelta & 4095);

					if (!ReadFile(hFile, filecache, tc, &dwActual, NULL) || !dwActual)
						break;

					lDelta -= tc;
				}

			} else
				SetFilePointer(hFile, (LONG)i64PageOffset, (LONG *)&i64PageOffset + 1, FILE_BEGIN);
		}

		if (ReadFile(hFile, filecache, 4096, &dwActual, NULL)) {
			i64FileCacheAddr = i64PageOffset;
			i64FileReadPosition = i64PageOffset + dwActual;
		} else {
			i64FileCacheAddr = i64FileReadPosition = -1;
		}
	}

	memcpy(rowcache, filecache + ((long)i64Offset & 0xff0), 16);

	i64RowCacheAddr = i64Offset;

	return rowcache;
}

void HexEditor::SetStatus(const char *format, ...) throw() {
	char buf[1024];
	va_list val;

	va_start(val,format);
	_vsnprintf(buf, sizeof buf, format, val);
	va_end(val);

	SetWindowText(hwndStatus, buf);
}

LRESULT HexEditor::Handle_WM_COMMAND(WPARAM wParam, LPARAM lParam) throw() {
	switch(LOWORD(wParam)) {
	case ID_FILE_EXIT:
		DestroyWindow(hwnd);
		break;
	case ID_FILE_CLOSE:
		Close();
		break;
	case ID_FILE_OPEN:
		Open();
		break;
	case ID_FILE_SAVE:
		Commit();
		break;
	case ID_FILE_REVERT:
		if (IDOK==MessageBox(hwnd, "Discard all changes?", g_szHexWarning, MB_OKCANCEL)) {
			HVModifiedLine *pLine;

			while(pLine = listMods.RemoveHead())
				delete pLine;
         
			InvalidateRect(hwnd, NULL, TRUE);
		}
		break;
	case ID_EDIT_JUMP:
		{
			__int64 v1 = mpView->GetPosition(), v2;

			if (AskForValues("Jump to address", "Address (hex):", NULL, v1, v2, JumpVerifier))
				mpView->MoveToByte(v1);
		}
		break;
	case ID_EDIT_TRUNCATE:
		{
			__int64 v1 = mpView->GetPosition(), v2;

			if (AskForValues("Truncate file", "Address (hex):", NULL, v1, v2, TruncateVerifier)) {
				if ((0xFFFFFFFF==SetFilePointer(hFile, (LONG)v1, (LONG *)&v1 + 1, FILE_BEGIN) && GetLastError()!=NO_ERROR)
						|| !SetEndOfFile(hFile))
					MyWin32Error("Cannot truncate file: %%s", GetLastError()).post(hwnd, "Error");

				i64FileReadPosition = i64FileSize = v1;

				mpView->SetDetails(i64FileSize, bEnableWrite);
			}
		}
		break;
	case ID_EDIT_EXTRACT:
		Extract();
		break;

	case ID_EDIT_FINDNEXT:
		if (hwndFind) {
			SendMessage(hwndFind, WM_COMMAND, IDC_FIND, 0);
			break;
		} else if (pszFindString) {
			Find();
			break;
		}
	case ID_EDIT_FIND:
		if (hwndFind)
			SetForegroundWindow(hwndFind);
		else
			CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_HEXVIEWER_FIND), hwnd, FindDlgProc, (LPARAM)this);
		break;

	case ID_EDIT_RIFFTREE:
		if (hwndTree)
			DestroyWindow(hwndTree);
		else
			CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_HEXVIEWER_RIFFLIST), hwnd, TreeDlgProc, (LPARAM)this);
		break;

	case ID_EDIT_AVIASSIST:
		bEnableAVIAssist = !bEnableAVIAssist;
		NewLocation(mpView->GetPosition());
		break;

	case ID_HELP_WHY:
		MessageBox(hwnd,
			"I need a quick way for people to send me parts of files that don't load properly "
			"in VirtualDub, and this is a handy way to do it. Well, that, and it's annoying to "
			"check 3GB AVI files if your hex editor tries to load the file into memory.",
			"Why is there a hex editor in VirtualDub?",
			MB_OK);
		break;

	case ID_HELP_KEYS:
		MessageBox(hwnd,
			"arrow keys/PgUp/PgDn: navigation\n"
			"TAB: switch between ASCII/Hex\n"
			"Backspace: undo",
			"Keyboard commands",
			MB_OK);
		break;

	}

	return 0;
}

LRESULT HexEditor::Handle_WM_SIZE(WPARAM wParam, LPARAM lParam) throw() {
	HDWP hdwp;
	RECT r, rstatus;

	GetClientRect(hwnd, &r);
	GetWindowRect(hwndStatus, &rstatus);
	MapWindowPoints(NULL, hwnd, (LPPOINT)&rstatus, 2);

	hdwp = BeginDeferWindowPos(2);
	DeferWindowPos(hdwp, hwndStatus, NULL, 0, r.bottom-rstatus.bottom, r.right, rstatus.bottom-rstatus.top, SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);
	DeferWindowPos(hdwp, hwndView, NULL, 0, 0, r.right, r.bottom - (rstatus.bottom-rstatus.top), SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOMOVE);
	EndDeferWindowPos(hdwp);

	return 0;
}

LRESULT HexEditor::Handle_WM_KEYDOWN(WPARAM wParam, LPARAM lParam) throw() {
	switch(wParam) {
	case VK_F3:
		if (hFile != INVALID_HANDLE_VALUE)
			Handle_WM_COMMAND(ID_EDIT_FINDNEXT, 0);
		break;
	case 'F':
		if (hFile != INVALID_HANDLE_VALUE)
			if (GetKeyState(VK_CONTROL)<0)
				Handle_WM_COMMAND(ID_EDIT_FIND, 0);
		break;
	case 'G':
		if (hFile != INVALID_HANDLE_VALUE)
			if (GetKeyState(VK_CONTROL)<0)
				Handle_WM_COMMAND(ID_EDIT_JUMP, 0);
		break;
	case 'R':
		if (hFile != INVALID_HANDLE_VALUE)
			if (GetKeyState(VK_CONTROL)<0)
				Handle_WM_COMMAND(ID_EDIT_RIFFTREE, 0);
	}

	return 0;
}


////////////////////////////

struct HexEditorAskData {
	HexEditor *thisPtr;
	const char *title, *name1, *name2;
	__int64 v1, v2;
	int (HexEditor::*verifier)(HWND, __int64 v1, __int64 v2);
};

BOOL CALLBACK HexEditor::AskForValuesDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) throw() {
	HexEditorAskData *pData = (HexEditorAskData *)GetWindowLong(hdlg, DWL_USER);
	char buf[32];

	switch(msg) {
	case WM_INITDIALOG:
		pData = (HexEditorAskData *)lParam;
		SetWindowLong(hdlg, DWL_USER, lParam);

		SetWindowText(hdlg, pData->title);
		sprintf(buf, "%I64X", pData->v1);
		SetDlgItemText(hdlg, IDC_EDIT_ADDRESS1, buf);
		SendDlgItemMessage(hdlg, IDC_EDIT_ADDRESS1, EM_LIMITTEXT, 16, 0);
		SetDlgItemText(hdlg, IDC_STATIC_ADDRESS1, pData->name1);
		if (pData->name2) {
			sprintf(buf, "%I64X", pData->v2);
			SetDlgItemText(hdlg, IDC_EDIT_ADDRESS2, buf);
			SendDlgItemMessage(hdlg, IDC_EDIT_ADDRESS1, EM_LIMITTEXT, 16, 0);
			SetDlgItemText(hdlg, IDC_STATIC_ADDRESS2, pData->name2);
		} else {
			ShowWindow(GetDlgItem(hdlg, IDC_EDIT_ADDRESS2), SW_HIDE);
			ShowWindow(GetDlgItem(hdlg, IDC_STATIC_ADDRESS2), SW_HIDE);
		}

		SendDlgItemMessage(hdlg, IDC_EDIT_ADDRESS1, EM_SETSEL, 0, -1);
		SetFocus(GetDlgItem(hdlg, IDC_EDIT_ADDRESS1));

		return FALSE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDCANCEL:
			EndDialog(hdlg, 0);
			break;
		case IDOK:
			{
				__int64 v1=0, v2=0;
				const char *s, *t;
				char c;
				int i;

				GetDlgItemText(hdlg, IDC_EDIT_ADDRESS1, buf, sizeof buf);

				s = buf;
				while(c=*s++) {
					if (!(t=strchr(hexdig, toupper(c)))) {
						SetFocus(GetDlgItem(hdlg, IDC_EDIT_ADDRESS1));
						MessageBeep(MB_ICONEXCLAMATION);
						return TRUE;
					}

					v1 = (v1<<4) | (t-hexdig);
				}

				if (pData->name2) {
					GetDlgItemText(hdlg, IDC_EDIT_ADDRESS2, buf, sizeof buf);

					s = buf;
					while(c=*s++) {
						if (!(t=strchr(hexdig, toupper(c)))) {
							SetFocus(GetDlgItem(hdlg, IDC_EDIT_ADDRESS2));
							MessageBeep(MB_ICONEXCLAMATION);
							return TRUE;
						}

						v2 = (v2<<4) | (t-hexdig);
					}
				}

				if (i = (pData->thisPtr->*(pData->verifier))(hdlg, v1, v2)) {
					if (i>=0) {
						SetFocus(GetDlgItem(hdlg, i==1?IDC_EDIT_ADDRESS1:IDC_EDIT_ADDRESS2));
						MessageBeep(MB_ICONEXCLAMATION);
					}
					return TRUE;
				}

				pData->v1 = v1;
				pData->v2 = v2;
			}
			EndDialog(hdlg, 1);
			break;
		}
		return TRUE;
	}
	return FALSE;
}

bool HexEditor::AskForValues(const char *title, const char *name1, const char *name2, __int64& v1, __int64& v2, int (HexEditor::*verifier)(HWND hdlg, __int64 v1, __int64 v2)) throw() {
	HexEditorAskData hvad;

	hvad.thisPtr = this;
	hvad.title = title;
	hvad.name1 = name1;
	hvad.name2 = name2;
	hvad.v1 = v1;
	hvad.v2 = v2;
	hvad.verifier = verifier;

	if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_HEXVIEWER), hwnd, AskForValuesDlgProc, (LPARAM)&hvad)) {
		v1 = hvad.v1;
		v2 = hvad.v2;
		return true;
	}
	return false;
}

int HexEditor::JumpVerifier(HWND hdlg, __int64 v1, __int64 v2) throw() {
	if (v1>i64FileSize)
		return 1;

	return 0;
}

int HexEditor::ExtractVerifier(HWND hdlg, __int64 v1, __int64 v2) throw() {
	if (v1 > i64FileSize)
		return 1;

	if (v1+v2 > i64FileSize)
		return 2;

	return 0;
}

int HexEditor::TruncateVerifier(HWND hdlg, __int64 v1, __int64 v2) throw() {
	int r;

	if (v1 < i64FileSize)
		r = MessageBox(hdlg, "You will lose all data past the specified address. Are you sure you want to truncate the file?", "Warning", MB_ICONEXCLAMATION|MB_YESNO);
	else if (v1 > i64FileSize)
		r = MessageBox(hdlg, "You have specified an address past the end of the file. Extend file to specified address?", "Warning", MB_ICONEXCLAMATION|MB_YESNO);

	return r==IDYES ? 0 : -1;
}

void HexEditor::Extract() throw() {
	__int64 v1 = mpView->GetPosition(), v2=0x1000;

	if (AskForValues("Extract file segment", "Address (hex):", "Length (hex):", v1, v2, ExtractVerifier)) {
		char szName[MAX_PATH];
		OPENFILENAME ofn;

		szName[0] = 0;

		ofn.lStructSize			= OPENFILENAME_SIZE_VERSION_400;
		ofn.hwndOwner			= hwnd;
		ofn.lpstrFilter			= "All files (*.*)\0*.*\0";
		ofn.lpstrCustomFilter	= NULL;
		ofn.nFilterIndex		= 1;
		ofn.lpstrFile			= szName;
		ofn.nMaxFile			= sizeof szName;
		ofn.lpstrFileTitle		= NULL;
		ofn.lpstrInitialDir		= NULL;
		ofn.lpstrTitle			= NULL;
		ofn.Flags				= OFN_EXPLORER | OFN_ENABLESIZING | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
		ofn.lpstrDefExt			= NULL;

		if (GetSaveFileName(&ofn)) {
			HANDLE hFile2 = INVALID_HANDLE_VALUE;
			char *pBuf = NULL;

			try {
				__int64 fpos = 0;

				if (0xFFFFFFFF==SetFilePointer(hFile, (LONG)v1, (LONG *)&v1 + 1, FILE_BEGIN) && GetLastError()!=NO_ERROR)
					throw GetLastError();

				pBuf = new char[65536];

				if (!pBuf)
					throw MyMemoryError();

				hFile2 = CreateFile(szName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN, NULL);

				if (hFile2 == INVALID_HANDLE_VALUE)
					throw GetLastError();

				if ((0xFFFFFFFF==SetFilePointer(hFile2, (LONG)v2, (LONG *)&v2 + 1, FILE_BEGIN) && GetLastError()!=NO_ERROR)
					|| !SetEndOfFile(hFile2))
					throw GetLastError();

				if (0xFFFFFFFF==SetFilePointer(hFile2, 0, NULL, FILE_BEGIN) && GetLastError()!=NO_ERROR)
					throw GetLastError();

				ProgressDialog pd(hwnd, "Extract segment", "Copying data range", (long)(v2>>10), TRUE);
				pd.setValueFormat("%dK of %dK");

				while(v2 > 0) {
					DWORD dwToCopy = (DWORD)v2, dwActual;

					if (dwToCopy > 65536)
						dwToCopy = 65536;

					pd.check();

					if (!ReadFile(hFile, pBuf, dwToCopy, &dwActual, NULL) || dwActual < dwToCopy)
						throw GetLastError();

					if (!WriteFile(hFile2, pBuf, dwToCopy, &dwActual, NULL) || dwActual < dwToCopy)
						throw GetLastError();

					v2 -= dwActual;

					pd.advance((long)((fpos += dwActual)>>10));
				}

				if (!CloseHandle(hFile2))
					throw GetLastError();

				hFile2 = INVALID_HANDLE_VALUE;
			} catch(DWORD dw) {
				MyWin32Error("Cannot create extract file: %%s", dw).post(hwnd, "Error");
			} catch(MyUserAbortError e) {
				SetEndOfFile(hFile2);
			}

			if (hFile2 != INVALID_HANDLE_VALUE)
				CloseHandle(hFile2);

			delete[] pBuf;
		}
	}

	i64FileReadPosition = -1;
}

void HexEditor::Find() throw() {
	if (!nFindLength || !pszFindString) {
		SendMessage(hwnd, WM_COMMAND, ID_EDIT_FIND, 0);
		return;
	}

	int *next = new int[nFindLength+1];
	char *searchbuffer = new char[65536];
	char *revstring = new char[nFindLength];
	char *findstring = pszFindString;
	int i,j;

	if (!next || !searchbuffer || !revstring) {
		delete[] next;
		delete[] searchbuffer;
		delete[] revstring;
		return;
	}

	if (bFindReverse) {
		for(i=0; i<nFindLength; ++i)
			revstring[i] = pszFindString[nFindLength-i-1];

		findstring = revstring;
	}

	// Initialize next list (Knuth-Morris-Pratt algorithm):

	next[0] = -1;
	i = 0;
	j = -1;

	do {
		if (j==-1 || findstring[i] == findstring[j]) {
			++i;
			++j;
			next[i] = (findstring[i] == findstring[j]) ? next[j] : j;
		} else
			j = next[j];
	} while(i < nFindLength);

	// Begin paging in sectors from disk.

	int limit=0;
	int size = 512;
	__int64 basepos = mpView->GetPosition();
	__int64 pos = basepos;
	__int64 posbase;
	bool bLastPartial = false;

	ProgressDialog pd(GetForegroundWindow(), "Find",
		bFindReverse?"Reverse searching for string":"Forward searching for string", (long)(((bFindReverse ? pos : i64FileSize - pos)+1048575)>>20), TRUE);
	pd.setValueFormat("%dMB of %dMB");

	i = 0;
	j = -1;	// this causes the first char to be skipped

	try {
		if (bFindReverse) {
			List2<HVModifiedLine>::rvit itML = listMods.end();

			while(pos >= 0) {
				{
					DWORD dwActual;

					i = pos & 511;

					pos &= ~511i64;

					SetFilePointer(hFile, (LONG)pos, (LONG *)&pos + 1, FILE_BEGIN);
					if (!ReadFile(hFile, searchbuffer, size, &dwActual, NULL))
						break;

					// we're overloading the bLastPartial variable as a 'first' flag....

					if (!bLastPartial && !dwActual)
						goto xit;

					bLastPartial = true;

					limit = (int)dwActual;

					if (pos + limit > basepos)
						limit = basepos - pos;

					if (!i) 
						i = limit;

					while((bool)itML && itML->address >= pos+limit)
						--itML;

					while((bool)itML && itML->address >= pos) {
						memcpy(searchbuffer + (long)(itML->address - pos), itML->data, 16);
						--itML;
					}

					posbase = pos;
					pos -= size;

					if (size < 65536 && !((long)pos & (size*2-1)))
						size += size;
				}

				if (bFindCaseInsensitive)
					while(i >= 0) {
						if (j == -1 || toupper((unsigned char)searchbuffer[i]) == toupper((unsigned char)findstring[j])) {
							--i;
							++j;

							if (j >= nFindLength) {
								mpView->MoveToByte(posbase+i+1);
								goto xit;
							}
						} else
							j = next[j];
					}
				else
					while(i >= 0) {
						if (j == -1 || searchbuffer[i] == findstring[j]) {
							--i;
							++j;

							if (j >= nFindLength) {
								mpView->MoveToByte(posbase+i+1);
								goto xit;
							}
						} else
							j = next[j];
					}

				pd.advance((long)((basepos - pos)>>20));
				pd.check();
			}
		} else {
			List2<HVModifiedLine>::fwit itML = listMods.begin();

			for(;;) {
				{
					DWORD dwActual;

					if (bLastPartial)
						break;

					i = pos & 511;

					pos &= ~511i64;

					SetFilePointer(hFile, (LONG)pos, (LONG *)&pos + 1, FILE_BEGIN);
					if (!ReadFile(hFile, searchbuffer, size, &dwActual, NULL))
						break;

					limit = (int)dwActual;

					while((bool)itML && itML->address < pos)
						++itML;

					while((bool)itML && itML->address < pos+limit) {
						memcpy(searchbuffer + (long)(itML->address - pos), itML->data, 16);
						++itML;
					}

					if (dwActual < size)
						bLastPartial = true;

					posbase = pos;
					pos += limit;

					if (size < 65536 && !((long)pos & (size*2-1)))
						size += size;
				}

				if (bFindCaseInsensitive)
					while(i < limit) {
						if (j == -1 || toupper((unsigned char)searchbuffer[i]) == toupper((unsigned char)findstring[j])) {
							++i;
							++j;

							if (j >= nFindLength) {
								mpView->MoveToByte(pos-limit+i-nFindLength);
								goto xit;
							}
						} else
							j = next[j];
					}
				else
					while(i < limit) {
						if (j == -1 || searchbuffer[i] == findstring[j]) {
							++i;
							++j;

							if (j >= nFindLength) {
								mpView->MoveToByte(posbase+i-nFindLength);
								goto xit;
							}
						} else
							j = next[j];
					}

				pd.advance((long)((pos - basepos)>>20));
				pd.check();
			}
		}

		pd.close();

		MessageBox(GetForegroundWindow(), "Search string not found", "Find", MB_OK);
xit:
		;
	} catch(MyUserAbortError) {
	}

	delete[] next;
	delete[] searchbuffer;
	delete[] revstring;

	i64FileReadPosition = -1;
}

////////////////////////////

LRESULT APIENTRY HexEditor::HexEditorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) throw() {
	HexEditor *pcd = (HexEditor *)GetWindowLong(hwnd, 0);

	switch(msg) {

	case WM_NCCREATE:
		if (!(pcd = new HexEditor(hwnd)))
			return FALSE;

		SetWindowLong(hwnd, 0, (LONG)pcd);
		return DefWindowProc(hwnd, msg, wParam, lParam);

	case WM_CREATE:
		pcd->Init();
//		return 0;		// intentional fall through to WM_SIZE

	case WM_SIZE:
		return pcd->Handle_WM_SIZE(wParam, lParam);

	case WM_DESTROY:
		delete pcd;
		SetWindowLong(hwnd, 0, 0);
		break;

	case WM_SETFOCUS:
		SetFocus(pcd->hwndView);
		return 0;

	case WM_COMMAND:
		return pcd->Handle_WM_COMMAND(wParam, lParam);

	case WM_CLOSE:
		DestroyWindow(hwnd);
		return 0;

	case WM_KEYDOWN:
		return pcd->Handle_WM_KEYDOWN(wParam, lParam);

	case WM_INITMENU:
		{
			DWORD dwEnableFlags = (pcd->hFile != INVALID_HANDLE_VALUE && pcd->bEnableWrite ? (MF_BYCOMMAND|MF_ENABLED) : (MF_BYCOMMAND|MF_GRAYED));
			HMENU hMenu = (HMENU)wParam;

			EnableMenuItem(hMenu,ID_FILE_SAVE, dwEnableFlags);
			EnableMenuItem(hMenu,ID_FILE_REVERT, dwEnableFlags);
			EnableMenuItem(hMenu,ID_EDIT_TRUNCATE, dwEnableFlags);

			dwEnableFlags = (pcd->hFile != INVALID_HANDLE_VALUE ? (MF_BYCOMMAND|MF_ENABLED) : (MF_BYCOMMAND|MF_GRAYED));

			EnableMenuItem(hMenu,ID_EDIT_JUMP, dwEnableFlags);
			EnableMenuItem(hMenu,ID_EDIT_EXTRACT, dwEnableFlags);
			EnableMenuItem(hMenu,ID_EDIT_RIFFTREE, dwEnableFlags);
			EnableMenuItem(hMenu,ID_EDIT_FIND, dwEnableFlags);
			EnableMenuItem(hMenu,ID_EDIT_FINDNEXT, dwEnableFlags);
			EnableMenuItem(hMenu,ID_FILE_CLOSE, dwEnableFlags);

			CheckMenuItem(hMenu, ID_EDIT_RIFFTREE, pcd->hwndTree ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
			CheckMenuItem(hMenu, ID_EDIT_AVIASSIST, pcd->bEnableAVIAssist ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
		}
		return 0;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

BOOL APIENTRY HexEditor::FindDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) throw() {
	HexEditor *pcd = (HexEditor *)GetWindowLong(hwnd, DWL_USER);

	switch(msg) {
	case WM_INITDIALOG:
		SetWindowLong(hwnd, DWL_USER, lParam);
		pcd = (HexEditor *)lParam;
		pcd->hwndFind = hwnd;
		pcd->mdnFind.hdlg = hwnd;
		guiAddModelessDialog(&pcd->mdnFind);

		if (pcd->pszFindString) {
			if (pcd->bFindHex) {
				char *text = new char[pcd->nFindLength*3];

				if (text) {
					int i;

					for(i=0; i<pcd->nFindLength; ++i) {
						int c = (unsigned char)pcd->pszFindString[i];

						text[i*3+0] = hexdig[c>>4];
						text[i*3+1] = hexdig[c&15];
						text[i*3+2] = ' ';
					}
					text[i*3-1] = 0;

					SetDlgItemText(hwnd, IDC_STRING, text);
				}
				CheckDlgButton(hwnd, IDC_HEX, BST_CHECKED);
			} else {
				SetDlgItemText(hwnd, IDC_STRING, pcd->pszFindString);
			}

			if (pcd->bFindCaseInsensitive)
				CheckDlgButton(hwnd, IDC_CASELESS, BST_CHECKED);

		}

		if (pcd->bFindReverse)
			CheckDlgButton(hwnd, IDC_UP, BST_CHECKED);
		else
			CheckDlgButton(hwnd, IDC_DOWN, BST_CHECKED);

		SetFocus(GetDlgItem(hwnd, IDC_STRING));
		return FALSE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDCANCEL:
			DestroyWindow(hwnd);
			break;

		case IDC_FIND:
			{
				HWND hwndEdit = GetDlgItem(hwnd, IDC_STRING);
				int l = GetWindowTextLength(hwndEdit);

				pcd->bFindHex = !!IsDlgButtonChecked(hwnd, IDC_HEX);
				pcd->bFindCaseInsensitive = !!IsDlgButtonChecked(hwnd, IDC_CASELESS);
				pcd->bFindReverse = !!IsDlgButtonChecked(hwnd, IDC_UP);

				if (l) {
					char *text = new char[l+1];

					if (GetWindowText(hwndEdit, text, l+1)) {
						if (IsDlgButtonChecked(hwnd, IDC_HEX)) {
							char *s = text, *s2;
							char *t = text;
							int c;

							for(;;) {
								while(*s && isspace(*s))
									++s;

								if (!*s)
									break;

								s2 = s;

								if (isxdigit(*s2)) {
									c = strchr(hexdig, toupper((int)(unsigned char)*s2++))-hexdig;
									if (isxdigit(*s2))
										c = c*16 + (strchr(hexdig, toupper((int)(unsigned char)*s2++))-hexdig);

									*t++ = (char)c;
								}

								if (s == s2) {
									SendMessage(hwndEdit, EM_SETSEL, s-text, s-text);
									SetFocus(hwndEdit);
									MessageBeep(MB_ICONEXCLAMATION);

									delete[] text;

									return 0;
								}

								s = s2;
							}

							l = t - text;
						} else {
							l = strlen(text);
						}
					}

					delete[] pcd->pszFindString;
					pcd->pszFindString = text;
					pcd->nFindLength = l;

					pcd->Find();
				}
			}
		}
		return TRUE;

	case WM_DESTROY:
		pcd->hwndFind = NULL;
		pcd->mdnFind.Remove();
		return TRUE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////

struct RIFFScanInfo {
	ProgressDialog& pd;
	int count[100];
	__int64 size[100];

	RIFFScanInfo(ProgressDialog &_pd) : pd(_pd) {}
};

void HexEditor::_RIFFScan(RIFFScanInfo &rsi, HWND hwndTV, HTREEITEM hti, __int64 pos, __int64 sizeleft) {
	char buf[128];

	while(sizeleft > 8) {
		const char *cl;
		struct {
			unsigned long ckid, size, listid;
		} chunk;
		bool bExpand = false;

		rsi.pd.advance((long)(pos >> 10));
		rsi.pd.check();

		// Try to read 12 bytes at the current position.

		int off = (unsigned long)pos & 15;

		cl = FillRowCache(pos - off);

		memcpy(&chunk, cl+off, off<4 ? 12 : 16-off);

		if (off > 4) {
			cl = FillRowCache(pos+16 - off);
			memcpy((char *)&chunk + (16-off), cl, off-4);
		}

		// quick validation tests

		if (chunk.ckid == 'TSIL' || chunk.ckid == 'FFIR') {		// RIFF or LIST
			char *dst = buf+sprintf(buf, "%08I64X [%-4.4s:%-4.4s:%8ld]: ", pos, (const char *)&chunk.ckid, (const char *)&chunk.listid, chunk.size);

			if (sizeleft < 12 || chunk.size < 4 || chunk.size > sizeleft-8 || !isValidFOURCC(chunk.listid)) {
				strcpy(dst, "invalid LIST/RIFF chunk");
				sizeleft = 0;
			} else {
				strcpy(dst, LookupRIFFChunk(chunk.ckid=='TSIL' ? g_LISTChunks : g_RIFFChunks, chunk.listid));
				bExpand = true;
			}
		} else {
			char *dst = buf+sprintf(buf, "%08I64X [%-4.4s:%8ld]: ", pos, &chunk.ckid, chunk.size);

			if (!isValidFOURCC(chunk.ckid) || chunk.size > sizeleft-8) {
				strcpy(dst, "invalid chunk");
				sizeleft = 0;
			} else if (isdigit(chunk.ckid&0xff) && isdigit((chunk.ckid>>8)&0xff)) {
				int stream = 10*(chunk.ckid&15) + ((chunk.ckid&0x0f00)>>8);

				sprintf(dst, "stream %d: byte pos %8I64d, chunk %ld", stream, rsi.size[stream], rsi.count[stream]);
				rsi.size[stream] += chunk.size;
				rsi.count[stream] ++;
			} else
				strcpy(dst, LookupRIFFChunk(g_JustChunks, chunk.ckid));
		}

		TVINSERTSTRUCT tvis;

		tvis.hParent		= hti;
		tvis.hInsertAfter	= TVI_LAST;
		tvis.item.mask		= TVIF_TEXT | TVIF_PARAM | TVIF_STATE;
		tvis.item.lParam	= 1;
		tvis.item.pszText	= buf;
		tvis.item.state		= TVIS_EXPANDED;
		tvis.item.stateMask	= TVIS_EXPANDED;

		HTREEITEM htiNew = TreeView_InsertItem(hwndTV, &tvis);

		if (bExpand)
			_RIFFScan(rsi, hwndTV, htiNew, pos+12, chunk.size-4);

		chunk.size = (chunk.size+1)&~1;

		pos += chunk.size + 8;
		sizeleft -= chunk.size + 8;
	}
}

void HexEditor::RIFFTree(HWND hwndTV) throw() {
	ProgressDialog pd(hwndTree, "Constructing RIFF tree", "Scanning file", (long)((i64FileSize+1023)>>10), true);
	RIFFScanInfo rsi(pd);

	pd.setValueFormat("%dK of %dK");

	memset(&rsi.count, 0, sizeof rsi.count);
	memset(&rsi.size, 0, sizeof rsi.size);

	try {
		_RIFFScan(rsi, hwndTV, TVI_ROOT, 0, i64FileSize);
	} catch(MyUserAbortError) {
	}

	SendMessage(hwndTV, WM_SETFONT, (WPARAM)hfont, TRUE);
}

BOOL APIENTRY HexEditor::TreeDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) throw() {
	HexEditor *pcd = (HexEditor *)GetWindowLong(hdlg, DWL_USER);

	switch(msg) {
	case WM_INITDIALOG:
		SetWindowLong(hdlg, DWL_USER, lParam);
		pcd = (HexEditor *)lParam;
		pcd->hwndTree = hdlg;
		pcd->RIFFTree(GetDlgItem(hdlg, IDC_TREE));
		return TRUE;

	case WM_SIZE:
		SetWindowPos(GetDlgItem(hdlg, IDC_TREE), NULL, 0, 0, LOWORD(lParam), HIWORD(lParam), SWP_NOZORDER|SWP_NOACTIVATE);
		return TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
			DestroyWindow(hdlg);
			return TRUE;
		}
		break;

	case WM_DESTROY:
		pcd->hwndTree = NULL;
		break;

	case WM_NOTIFY:
		if (GetWindowLong(((NMHDR *)lParam)->hwndFrom, GWL_ID) == IDC_TREE) {
			const NMHDR *pnmh = (NMHDR *)lParam;

			if (pnmh->code == NM_DBLCLK || (pnmh->code == TVN_KEYDOWN && ((LPNMTVKEYDOWN)lParam)->wVKey == VK_RETURN)) {
				HTREEITEM hti = TreeView_GetSelection(pnmh->hwndFrom);

				if (hti) {
					char buf[128];
					TVITEM tvi;
					__int64 seekpos;

					tvi.mask		= TVIF_TEXT | TVIF_PARAM;
					tvi.hItem		= hti;
					tvi.pszText		= buf;
					tvi.cchTextMax	= sizeof buf;

					TreeView_GetItem(pnmh->hwndFrom, &tvi);

					if (tvi.lParam && 1==sscanf(buf, "%I64x", &seekpos)) {
						pcd->mpView->MoveToByte(seekpos);
						PostMessage(hdlg, WM_APP, 0, 0);
					}
				}

				SetWindowLong(hdlg, DWL_MSGRESULT, 1);
			}
			return TRUE;
		}
		break;

	case WM_APP:
		SetForegroundWindow(pcd->hwnd);
		SetFocus(pcd->hwnd);
		return TRUE;
	}

	return FALSE;
}

///////////////////////////////////////////////////////////////////////////

ATOM RegisterHexEditor() {
	WNDCLASS wc1, wc2;

	wc1.style			= 0;
	wc1.lpfnWndProc		= HexEditor::HexEditorWndProc;
	wc1.cbClsExtra		= 0;
	wc1.cbWndExtra		= sizeof(HexEditor *);
	wc1.hInstance		= g_hInst;
	wc1.hIcon			= NULL;
    wc1.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wc1.hbrBackground	= NULL; //(HBRUSH)(COLOR_WINDOW+1);
	wc1.lpszMenuName	= MAKEINTRESOURCE(IDR_HEXVIEWER_MENU);
	wc1.lpszClassName	= HEXEDITORCLASS;

	wc2.style			= 0;
	wc2.lpfnWndProc		= HexViewer::HexViewerWndProc;
	wc2.cbClsExtra		= 0;
	wc2.cbWndExtra		= sizeof(HexViewer *);
	wc2.hInstance		= g_hInst;
	wc2.hIcon			= NULL;
    wc2.hCursor			= LoadCursor(NULL, IDC_ARROW);
	wc2.hbrBackground	= NULL; //(HBRUSH)(COLOR_WINDOW+1);
	wc2.lpszMenuName	= NULL;
	wc2.lpszClassName	= szHexViewerClassName;

	return RegisterClass(&wc1) && RegisterClass(&wc2);

}
void HexEdit(HWND hwndParent) {
	CreateWindow(
		HEXEDITORCLASS,
		"VirtualDub Hex Editor",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		hwndParent,
		NULL,
		g_hInst,
		NULL);
}
