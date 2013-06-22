//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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
#include <math.h>
#include <vector>

#include "oshelper.h"
#include "fht.h"

#include "AudioDisplay.h"

#include "AVIOutputWAV.h"

extern HINSTANCE g_hInst;

extern const char g_szAudioDisplayControlName[]="birdyAudioDisplayControl";

/////////////////////////////////////////////////////////////////////////////
//
//	VDAudioDisplayControl
//
/////////////////////////////////////////////////////////////////////////////

class VDAudioDisplayControl {
public:
	static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	VDAudioDisplayControl(HWND hwnd);
	~VDAudioDisplayControl();

	void SetSpectralPaletteDefault();

	LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);

	void OnPaint(HDC hdc);
	void OnSize();
	void PaintChannel(HDC hdc, int ch);

	void ProcessAudio(sint16 *src, int count, int stride);
	static void FastFill(HDC hdc, int x1, int y1, int x2, int y2, DWORD c);

	const HWND	mhwnd;
	int			mChanHeight;
	int			mChanWidth;
	bool		mbSpectrumMode;

	std::vector<unsigned char>	mImage;
	struct {
		BITMAPINFOHEADER hdr;
		RGBQUAD pal[256];
	} mbihSpectrum;
};

ATOM RegisterAudioDisplayControl() {
	WNDCLASS wc;

	wc.style		= 0;
	wc.lpfnWndProc	= VDAudioDisplayControl::StaticWndProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= sizeof(VDAudioDisplayControl *);
	wc.hInstance	= g_hInst;
	wc.hIcon		= NULL;
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= (HBRUSH)(COLOR_3DFACE+1);		//GetStockObject(LTGRAY_BRUSH);
	wc.lpszMenuName	= NULL;
	wc.lpszClassName= AUDIODISPLAYCONTROLCLASS;

	return RegisterClass(&wc);
}

VDAudioDisplayControl::VDAudioDisplayControl(HWND hwnd)
	: mhwnd(hwnd)
	, mChanWidth(0)
	, mChanHeight(0)
	, mbSpectrumMode(false)
{
	SetSpectralPaletteDefault();

	mbihSpectrum.hdr.biSize				= sizeof(BITMAPINFOHEADER);
	mbihSpectrum.hdr.biWidth			= 0;
	mbihSpectrum.hdr.biHeight			= 256;
	mbihSpectrum.hdr.biPlanes			= 1;
	mbihSpectrum.hdr.biCompression		= BI_RGB;
	mbihSpectrum.hdr.biBitCount			= 8;
	mbihSpectrum.hdr.biSizeImage		= 0;
	mbihSpectrum.hdr.biXPelsPerMeter	= 0;
	mbihSpectrum.hdr.biYPelsPerMeter	= 0;
	mbihSpectrum.hdr.biClrUsed			= 256;
	mbihSpectrum.hdr.biClrImportant		= 256;

//	mImage.resize(640);
//	for(int x=0; x<640; ++x)
//		mImage[x] = 0x80 + 0x7f * sin(x*x*0.0001);

	std::vector<sint16> samples(65536*2);

	for(int x=0; x<65536; ++x)
		samples[x*2] = 0x3fff * (sin((double)x*x*0.00001) + sin((double)x*x*x*0.000000001));

	mbSpectrumMode = true;
	ProcessAudio(&samples[0], 65536, 2);

#if 0
	AVIOutputWAV aow;

	aow.initOutputStreams();
	PCMWAVEFORMAT *pwf = (PCMWAVEFORMAT *)aow.audioOut->allocFormat(sizeof(PCMWAVEFORMAT));

	pwf->wf.wFormatTag = WAVE_FORMAT_PCM;
	pwf->wf.nBlockAlign = 4;
	pwf->wf.nChannels = 2;
	pwf->wf.nAvgBytesPerSec = 176400;
	pwf->wf.nSamplesPerSec = 44100;
	pwf->wBitsPerSample = 16;

	aow.init("c:/test/sweep.wav", FALSE, TRUE, 65536, FALSE);
	aow.audioOut->write(0, &samples[0], 65536*4, 65536);
	aow.finalize();
#endif
}

VDAudioDisplayControl::~VDAudioDisplayControl() {
}

void VDAudioDisplayControl::SetSpectralPaletteDefault() {
	// We create a color ramp from black>red>yellow>white, with roughly
	// linear luminance up the ramp. Given luminance defined as:
	//
	//	Y = 0.21R + 0.70G + 0.09B
	//
	// the red range uses 54 entries, the green range 179, and the
	// blue range 23 entries.

	unsigned i;

	for(i=0; i<54; ++i) {
		mbihSpectrum.pal[i].rgbRed		= i*256/54;
		mbihSpectrum.pal[i].rgbGreen	= 0;
		mbihSpectrum.pal[i].rgbBlue		= 0;
		mbihSpectrum.pal[i].rgbReserved	= 0;
	}
	for(i=0; i<179; ++i) {
		mbihSpectrum.pal[i+54].rgbRed		= 255;
		mbihSpectrum.pal[i+54].rgbGreen	= i*256/179;
		mbihSpectrum.pal[i+54].rgbBlue		= 0;
		mbihSpectrum.pal[i+54].rgbReserved	= 0;
	}
	for(i=0; i<23; ++i) {
		mbihSpectrum.pal[i+233].rgbRed		= 255;
		mbihSpectrum.pal[i+233].rgbGreen	= 255;
		mbihSpectrum.pal[i+233].rgbBlue		= i*256/23;
		mbihSpectrum.pal[i+233].rgbReserved	= 0;
	}
}

LRESULT CALLBACK VDAudioDisplayControl::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	VDAudioDisplayControl *pThis = (VDAudioDisplayControl *)GetWindowLong(hwnd, 0);

	if (msg == WM_NCCREATE) {
		pThis = new VDAudioDisplayControl(hwnd);

		if (!pThis)
			return FALSE;

		SetWindowLong(hwnd, 0, (DWORD)pThis);
	} else if (msg == WM_NCDESTROY) {
		delete pThis;
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return pThis->WndProc(msg, wParam, lParam);
}

LRESULT VDAudioDisplayControl::WndProc(UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {

	case WM_CREATE:
		OnSize();
		break;

	case WM_SIZE:
		OnSize();
		return 0;

	case WM_ERASEBKGND:
		return FALSE;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hDC;

			hDC = BeginPaint(mhwnd, &ps);
			OnPaint(hDC);
			EndPaint(mhwnd, &ps);
		}
		return 0;

	case ADCM_SETAUDIO:
		ProcessAudio((sint16 *)lParam, wParam, 2);
		InvalidateRect(mhwnd, NULL, TRUE);
		return 0;
	}

	return DefWindowProc(mhwnd, msg, wParam, lParam);
}

void VDAudioDisplayControl::OnSize() {
	RECT r;
	GetClientRect(mhwnd, &r);

	mChanWidth		= r.right;
	mChanHeight		= (r.bottom - 1) | 1;
}

void VDAudioDisplayControl::OnPaint(HDC hdc) {
	if (mbSpectrumMode) {
		SetDIBitsToDevice(hdc, 0, 0, mbihSpectrum.hdr.biWidth, mbihSpectrum.hdr.biHeight, 0, 0, 0, mbihSpectrum.hdr.biHeight, &mImage[0], (const BITMAPINFO *)&mbihSpectrum, DIB_RGB_COLORS);
		return;
	}

	FastFill(hdc, 0, 0, mChanWidth, mChanHeight, RGB(0,0,0));
	FastFill(hdc, 0, mChanHeight>>1, mChanWidth, 1, RGB(0,128,96));

	XFORM xfOld;

#pragma message(__TODO__ "GM_ADVANCED is not 9x safe")

	SetGraphicsMode(hdc, GM_ADVANCED);
	GetWorldTransform(hdc, &xfOld);
	const XFORM xform = {
		(FLOAT)1.0f/16.0f,
		(FLOAT)0.0f,
		(FLOAT)0.0f,
		(FLOAT)((mChanHeight>>1) / 128.0),
		(FLOAT)0.0f,
		(FLOAT)0.0f,
	};
	SetWorldTransform(hdc, &xform);

	if (HPEN hpen = CreatePen(PS_SOLID, 0, RGB(255, 0, 0))) {
		if (HGDIOBJ hOldPen = SelectObject(hdc, hpen)) {
			int w = std::min<int>(mChanWidth*16, mImage.size());
			for(int x=0; x<w; ++x) {
				int y = mImage[x];
				if (!x)
					MoveToEx(hdc, x, y, NULL);
				else
					LineTo(hdc, x, y);
			}
			SelectObject(hdc, hOldPen);
		}
		DeleteObject(hpen);
	}

	SetWorldTransform(hdc, &xfOld);
	SetGraphicsMode(hdc, GM_COMPATIBLE);
}

void VDAudioDisplayControl::ProcessAudio(sint16 *src, int count, int stride) {
	if (mbSpectrumMode) {
		Fht xform(1024);

		int samples = (count-1024) / 64 + 1;
		unsigned pitch = (samples+3) & ~3;

		mImage.resize(256*pitch);
		unsigned char *dst = &mImage[0];

		mbihSpectrum.hdr.biWidth		= samples;
		mbihSpectrum.hdr.biHeight		= 256;
		mbihSpectrum.hdr.biSizeImage	= mImage.size();

		xform.CopyInStereo16(src, 1024 - 64);
		src += (1024-64)*2;
		while(samples--) {
			xform.CopyInStereo16(src, 64);
			src += 64*2;

			xform.Transform(256);
			
			for(unsigned i=0; i<256; ++i) {
				double x = xform.GetPower(i);
//				long y = x * 16384 * 0.5;
				long y = x * 16384 * 8.0;

				y += abs(y);

				if ((unsigned)y >= 256)
					y = 255;

				dst[i*pitch] = y;
			}
			++dst;
		}
	} else {
		mImage.reserve(count);
		mImage.clear();
		while(count--) {
			mImage.push_back((*src + 0x8000) >> 8);
			src += stride;
		}
	}
}

void VDAudioDisplayControl::FastFill(HDC hdc, int x1, int y1, int w, int h, DWORD c) {
	RECT r = {x1,y1,x1+w,y1+h};

	SetBkColor(hdc, c);
	ExtTextOut(hdc, 0, 0, ETO_OPAQUE, &r, (LPCSTR)&r, 0, NULL);
}
