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

#include <crtdbg.h>

#include <windows.h>
#include <vfw.h>

#include "VideoSource.h"
#include "VBitmap.h"
#include "AVIStripeSystem.h"
#include "ProgressDialog.h"
#include "MJPEGDecoder.h"
#include "crash.h"

#include "error.h"
#include "misc.h"
#include "oshelper.h"
#include "helpfile.h"
#include "resource.h"

///////////////////////////

extern const char *LookupVideoCodec(FOURCC);

extern HINSTANCE g_hInst;
extern HWND g_hWnd;

///////////////////////////

const char g_szNoMPEG4Test[]="No MPEG-4 Test";

static BOOL CALLBACK MP4CodecWarningDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch(msg) {
	case WM_INITDIALOG:
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case IDOK:
			HelpContext(hdlg, IDH_WARN_MPEG4);
		case IDCANCEL:
			if (IsDlgButtonChecked(hdlg, IDC_NOMORE))
				SetConfigDword(NULL, g_szNoMPEG4Test, 1);
			EndDialog(hdlg, 0);
			break;
		}
		break;
	}
	return FALSE;
}

static bool CheckMPEG4Codec(HIC hic, bool isV3) {
	char frame[0x380];
	BITMAPINFOHEADER bih;
	DWORD dw;

	if (QueryConfigDword(NULL, g_szNoMPEG4Test, &dw) && dw)
		return true;

	// Form a completely black frame if it's V3.

	bih.biSize			= 40;
	bih.biWidth			= 320;
	bih.biHeight		= 240;
	bih.biPlanes		= 1;
	bih.biBitCount		= 24;
	bih.biCompression	= '24PM';
	bih.biSizeImage		= 0;
	bih.biXPelsPerMeter	= 0;
	bih.biYPelsPerMeter	= 0;
	bih.biClrUsed		= 0;
	bih.biClrImportant	= 0;

	if (isV3) {
		int i;

		frame[0] = (char)0x3f;
		frame[1] = (char)0x71;
		frame[2] = (char)0x1b;
		frame[3] = (char)0x7c;

		for(i=4; i<0x179; i+=5) {
			frame[i+0] = (char)0x2f;
			frame[i+1] = (char)0x0b;
			frame[i+2] = (char)0xc2;
			frame[i+3] = (char)0xf0;
			frame[i+4] = (char)0xbc;
		}

		frame[0x179] = (char)0xf0;
		frame[0x17a] = (char)0xb8;
		frame[0x17b] = (char)0x01;

		bih.biCompression	= '34PM';
		bih.biSizeImage		= 0x17c;
	}

	// Attempt to decompress.

	HANDLE h;

	h = ICImageDecompress(hic, 0, (BITMAPINFO *)&bih, frame, NULL);

//	if (!h)
//		DialogBox(g_hInst, MAKEINTRESOURCE(IDD_WARN_MPEG4), g_hWnd, MP4CodecWarningDlgProc);
//	else
//		GlobalFree(h);

	if (h) {
		GlobalFree(h);
		return true;
	} else {
		return false;
	}
}

///////////////////////////

VideoSource::VideoSource() {
	lpvBuffer = NULL;
	hBufferObject = NULL;
	bmihDecompressedFormat = NULL;
}

VideoSource::~VideoSource() {
	freemem(bmihDecompressedFormat);
	FreeFrameBuffer();
}

void *VideoSource::AllocFrameBuffer(long size) {
	hBufferObject = CreateFileMapping(
			(HANDLE)0xFFFFFFFF,
			NULL,
			PAGE_READWRITE,
			0,
			size,
			NULL);

	if (!hBufferObject) return NULL;

	lBufferOffset = 0;

	lpvBuffer = MapViewOfFile(hBufferObject, FILE_MAP_ALL_ACCESS, 0, lBufferOffset, size);

	if (!lpvBuffer) {
		CloseHandle(hBufferObject);
		hBufferObject = NULL;
	}

	return lpvBuffer;
}

void VideoSource::FreeFrameBuffer() {
	if (hBufferObject) {
		if (lpvBuffer)
			UnmapViewOfFile(lpvBuffer);
		CloseHandle(hBufferObject);
	} else
		freemem(lpvBuffer);

	lpvBuffer = NULL;
	hBufferObject = NULL;
}

bool VideoSource::setDecompressedFormat(int depth) {
	memcpy(bmihDecompressedFormat, getImageFormat(), getFormatLen());
	bmihDecompressedFormat->biSize			= sizeof(BITMAPINFOHEADER);
	bmihDecompressedFormat->biPlanes			= 1;
	bmihDecompressedFormat->biBitCount		= depth;
	bmihDecompressedFormat->biCompression	= BI_RGB;
	bmihDecompressedFormat->biSizeImage		= ((bmihDecompressedFormat->biWidth * depth + 31)/32)*4*bmihDecompressedFormat->biHeight;

	if (depth>8) {
		bmihDecompressedFormat->biClrUsed		= 0;
		bmihDecompressedFormat->biClrImportant	= 0;
	}

	invalidateFrameBuffer();

	return true;
}

bool VideoSource::setDecompressedFormat(BITMAPINFOHEADER *pbih) {
	if (pbih->biCompression == BI_RGB) {
		setDecompressedFormat(pbih->biBitCount);
		return true;
	}

	return false;
}

void VideoSource::streamBegin(bool) {
	stream_current_frame	= -1;
}

void VideoSource::streamSetDesiredFrame(long frame_num) {
	long key;

	key = isKey(frame_num) ? frame_num : prevKey(frame_num);
	if (key<0) key = lSampleFirst;

	stream_desired_frame	= frame_num;

	if (stream_current_frame<key || stream_current_frame>frame_num)
		stream_current_frame	= key-1;

}

long VideoSource::streamGetNextRequiredFrame(BOOL *is_preroll) {
	if (stream_current_frame == stream_desired_frame) {
		*is_preroll = FALSE;

		return -1;
	}

	*is_preroll = (++stream_current_frame != stream_desired_frame);

	return stream_current_frame;
}

int VideoSource::streamGetRequiredCount(long *pSize) {

	if (pSize) {
		long current = stream_current_frame;
		long size = 0, onesize;
		long samp;

		while(current < stream_desired_frame) {
			if (AVIERR_OK == read(current, 1, NULL, NULL, &onesize, &samp))
				size += onesize;

			++current;
		}

		*pSize = size;
	}

	return stream_desired_frame - stream_current_frame;
}

void VideoSource::invalidateFrameBuffer() {
}

bool VideoSource::isKeyframeOnly() {
   return false;
}

bool VideoSource::isType1() {
   return false;
}

///////////////////////////

VideoSourceAVI::VideoSourceAVI(IAVIReadHandler *pAVI, AVIStripeSystem *stripesys, IAVIReadHandler **stripe_files, bool use_internal, int mjpeg_mode, FOURCC fccForceVideo, FOURCC fccForceVideoHandler) {
	pAVIFile	= pAVI;
	pAVIStream	= NULL;
	lpvBuffer	= NULL;
	hicDecomp	= NULL;
	bmihTemp	= NULL;
	key_flags	= NULL;
	mjpeg_reorder_buffer = NULL;
	mjpeg_reorder_buffer_size = 0;
	mjpeg_splits = NULL;
	mjpeg_last = -1;
	this->fccForceVideo = fccForceVideo;
	this->fccForceVideoHandler = fccForceVideoHandler;
	hbmLame = NULL;
	fUseGDI = false;
	bDirectDecompress = false;

	// striping...

	stripe_streams	= NULL;
	stripe_index	= NULL;
	this->stripesys = stripesys;
	this->stripe_files = stripe_files;
	this->use_internal = use_internal;
	this->mjpeg_mode	= mjpeg_mode;

	mdec = NULL;

	try {
		_construct();
	} catch(...) {
		_destruct();
		throw;
	}
}

void VideoSourceAVI::_destruct() {
	delete stripe_index;

	if (stripe_streams) {
		int i;

		for(i=0; i<stripe_count; i++)
			if (stripe_streams[i])
				delete stripe_streams[i];

		delete stripe_streams;
	}

	if (bmihTemp) freemem(bmihTemp);
	if (hicDecomp) ICClose(hicDecomp);

	if (pAVIStream) delete pAVIStream;

	delete mdec;
	freemem(mjpeg_reorder_buffer);
	mjpeg_reorder_buffer = NULL;
	delete[] mjpeg_splits;
	mjpeg_splits = NULL;

	delete[] key_flags; key_flags = NULL;

	if (hbmLame) {
		DeleteObject(hbmLame);
		hbmLame = NULL;
	}
}

VideoSourceAVI::~VideoSourceAVI() {
	_destruct();
}

void VideoSourceAVI::_construct() {
	LONG format_len;
	BITMAPINFOHEADER *bmih;
	bool is_mjpeg, is_dib;

	// Look for a standard vids stream

	bIsType1 = false;
	pAVIStream = pAVIFile->GetStream(streamtypeVIDEO, 0);
	if (!pAVIStream) {
		pAVIStream = pAVIFile->GetStream('svai', 0);
/*		if (pAVIStream) {
			delete pAVIStream;
			pAVIStream = NULL;
			throw MyError("Type-1 DV files are not currently supported by VirtualDub.");
		}*/

		if (!pAVIStream)
			throw MyError("No video stream found.");

		bIsType1 = true;
	}

	if (pAVIStream->Info(&streamInfo, sizeof streamInfo))
		throw MyError("Error obtaining video stream info.");

	// ADDITION FOR STRIPED AVI SUPPORT:
	//
	// If this is an index for a stripe system, then the video stream will have
	// 'VDST' as its fccHandler and video compression.  This will probably
	// correspond to the "VDub Frameserver" compressor, but since VirtualDub can
	// connect to its own frameservers more efficiently though the AVIFile
	// interface, it makes sense to open striped files in native mode.
	//
	// For this to work, we must have been sent the striping specs beforehand,
	// or else we won't be able to open the stripes.

	if (streamInfo.fccHandler == 'TSDV') {
		int i;

		if (!stripesys)
			throw MyError("AVI file is striped - must be opened with stripe definition file.");

		// allocate memory for stripe stream table

		stripe_count = stripesys->getStripeCount();

		if (!(stripe_streams = new IAVIReadStream *[stripe_count]))
			throw MyMemoryError();

		for(i=0; i<stripe_count; i++)
			stripe_streams[i] = NULL;

		// attempt to open a video stream for every stripe that has one

		format_stream = NULL;

		for(i=0; i<stripe_count; i++) {
			if (stripesys->getStripeInfo(i)->isVideo()) {
				stripe_streams[i] = stripe_files[i]->GetStream(streamtypeVIDEO, 0);
				if (!stripe_streams[i])
					throw MyError("Striping: cannot open video stream for stripe #%d", i+1);

				if (!format_stream) format_stream = stripe_streams[i];
			}
		}

		if (!format_stream)
			throw MyError("Striping: No video stripes found!");

		// Reread the streamInfo structure from first video stripe,
		// because ours is fake.

		if (format_stream->Info(&streamInfo, sizeof streamInfo))
			throw MyError("Error obtaining video stream info from first video stripe.");

		// Initialize the index.

		if (!(stripe_index = new AVIStripeIndexLookup(pAVIStream)))
			throw MyMemoryError();
		
	} else {
		if (stripesys)
			throw MyError("This is not a striped AVI file.");

		format_stream = pAVIStream;
	}

	// Read video format.  If we're striping, the index stripe has a fake
	// format, so we have to grab the format from a video stripe.  If it's a
	// type-1 DV, we're going to have to fake it.

	if (bIsType1) {
		format_len = sizeof(BITMAPINFOHEADER);

		if (!(bmih = (BITMAPINFOHEADER *)allocFormat(format_len))) throw MyMemoryError();

		bmih->biSize			= sizeof(BITMAPINFOHEADER);
		bmih->biWidth			= 720;

		if (streamInfo.dwRate > streamInfo.dwScale*26i64)
			bmih->biHeight			= 480;
		else
			bmih->biHeight			= 576;

		bmih->biPlanes			= 1;
		bmih->biBitCount		= 24;
		bmih->biCompression		= 'dsvd';
		bmih->biSizeImage		= streamInfo.dwSuggestedBufferSize;
		bmih->biXPelsPerMeter	= 0;
		bmih->biYPelsPerMeter	= 0;
		bmih->biClrUsed			= 0;
		bmih->biClrImportant	= 0;
	} else {
		format_stream->FormatSize(0, &format_len);

		if (!(bmih = (BITMAPINFOHEADER *)allocFormat(format_len))) throw MyMemoryError();

		if (format_stream->ReadFormat(0, getFormat(), &format_len))
			throw MyError("Error obtaining video stream format.");
	}
	if (!(bmihTemp = (BITMAPINFOHEADER *)allocmem(format_len))) throw MyMemoryError();
	if (!(bmihDecompressedFormat = (BITMAPINFOHEADER *)allocmem(format_len))) throw MyMemoryError();

	// We can handle RGB8/16/24/32 and YUY2.

	is_dib = (bmih->biCompression == BI_RGB) || (bmih->biCompression == '2YUY');

	// Force the video format if necessary

	if (fccForceVideo)
		getImageFormat()->biCompression = fccForceVideo;

	if (fccForceVideoHandler)
		streamInfo.fccHandler = fccForceVideoHandler;

	is_mjpeg = isEqualFOURCC(bmih->biCompression, 'GPJM')
			|| isEqualFOURCC(fccForceVideo, 'GPJM')
			|| isEqualFOURCC(bmih->biCompression, '1bmd')
			|| isEqualFOURCC(fccForceVideo, '1bmd');

	// If this is MJPEG, check to see if we should modify the output format and/or stream info

	lSampleFirst = pAVIStream->Start();
	lSampleLast = pAVIStream->End();

	if (is_mjpeg) {
		BITMAPINFOHEADER *pbih = getImageFormat();

		if (mjpeg_mode && mjpeg_mode != IFMODE_SWAP && pbih->biHeight > 288) {
			pbih->biHeight /= 2;

			if (mjpeg_mode == IFMODE_SPLIT1 || mjpeg_mode == IFMODE_SPLIT2) {
				streamInfo.dwRate *= 2;
				streamInfo.dwLength *= 2;
				lSampleLast = lSampleLast*2 - lSampleFirst;
			}
		}

		if (mjpeg_mode) {
			if (!(mjpeg_splits = new long[lSampleLast - lSampleFirst]))
				throw MyMemoryError();

			for(int i=0; i<lSampleLast-lSampleFirst; i++)
				mjpeg_splits[i] = -1;
		}
	} else
		mjpeg_mode = 0;

	memcpy(bmihTemp, getFormat(), format_len);

	// allocate framebuffer

	if (!AllocFrameBuffer(bmih->biWidth * 4 * bmih->biHeight + 4))
		throw MyMemoryError();

	// get a decompressor
	//
	// 'DIB ' is the official value for uncompressed AVIs, but some stupid
	// programs also use (null) and 'RAW '

	hicDecomp = NULL;

	if (bmih->biCompression == BI_BITFIELDS || bmih->biCompression == BI_RLE8 || bmih->biCompression == BI_RLE4
		|| (bmih->biCompression == BI_RGB && bmih->biBitCount<16 && bmih->biBitCount != 8)) {

		// Ugh.  It's one of them weirdo formats.  Let GDI handle it!

		fUseGDI = true;

	} else if (!is_dib) {
		FOURCC fccOriginalCodec = bmih->biCompression;

		// If it's a hacked MPEG-4 driver, try all of the known hacks. #*$&@#(&$)(#$
		// They're all the same driver, so they're mutually compatible.

		VDCHECKPOINT;
		switch(bmih->biCompression) {
		case '34PM':		// Microsoft MPEG-4 V3
		case '3VID':		// "DivX Low-Motion" (4.10.0.3917)
		case '4VID':		// "DivX Fast-Motion" (4.10.0.3920)
		case '5VID':		// unknown
		case '14PA':		// "AngelPotion Definitive" (4.0.00.3688)
			if (AttemptCodecNegotiation(bmih, is_mjpeg)) return;
			bmih->biCompression = '34PM';
			if (AttemptCodecNegotiation(bmih, is_mjpeg)) return;
			bmih->biCompression = '3VID';
			if (AttemptCodecNegotiation(bmih, is_mjpeg)) return;
			bmih->biCompression = '4VID';
			if (AttemptCodecNegotiation(bmih, is_mjpeg)) return;
			bmih->biCompression = '14PA';
		default:
			if (AttemptCodecNegotiation(bmih, is_mjpeg)) return;
			break;
		}
		VDCHECKPOINT;

		const char *s = LookupVideoCodec(fccOriginalCodec);

		throw MyError("Couldn't locate decompressor for format '%c%c%c%c' (%s)\n"
						"\n"
						"VirtualDub requires a Video for Windows (VFW) compatible codec to decompress "
						"video. DirectShow codecs, such as those used by Windows Media Player, are not "
						"suitable."
					,(fccOriginalCodec    ) & 0xff
					,(fccOriginalCodec>> 8) & 0xff
					,(fccOriginalCodec>>16) & 0xff
					,(fccOriginalCodec>>24) & 0xff
					,s ? s : "unknown");
	}
}

bool VideoSourceAVI::AttemptCodecNegotiation(BITMAPINFOHEADER *bmih, bool is_mjpeg) {

	// VideoMatrix sets streamInfo.fccHandler to NULL.  Danger, Will Robinson.

	if (!use_internal) {

		// Try the handler specified in the file first.  In some cases, it'll
		// be wrong or missing.

		if (streamInfo.fccHandler)
			hicDecomp = ICOpen(ICTYPE_VIDEO, streamInfo.fccHandler, ICMODE_DECOMPRESS);

		if (!hicDecomp || ICERR_OK!=ICDecompressQuery(hicDecomp, bmih, NULL)) {
			if (hicDecomp)
				ICClose(hicDecomp);

			// Pick a handler based on the biCompression field instead.

			hicDecomp = ICOpen(ICTYPE_VIDEO, bmih->biCompression, ICMODE_DECOMPRESS);

			if (!hicDecomp || ICERR_OK!=ICDecompressQuery(hicDecomp, bmih, NULL)) {
				if (hicDecomp)
					ICClose(hicDecomp);

				// AngelPotion f*cks us over here and overwrites our format if we keep this in.
				// So let's write protect the format.  You'll see a nice C0000005 error when
				// the AP DLL tries to modify it.

//				hicDecomp = ICLocate(ICTYPE_VIDEO, NULL, getImageFormat(), NULL, ICMODE_DECOMPRESS);

				int siz = getFormatLen();

				BITMAPINFOHEADER *pbih_protected = (BITMAPINFOHEADER *)VirtualAlloc(NULL, siz, MEM_COMMIT, PAGE_READWRITE);

				if (pbih_protected) {
					DWORD dwOldProtect;

					memcpy(pbih_protected, bmih, siz);
					VirtualProtect(pbih_protected, siz, PAGE_READONLY, &dwOldProtect);

					hicDecomp = ICLocate(ICTYPE_VIDEO, NULL, pbih_protected, NULL, ICMODE_DECOMPRESS);

					VirtualFree(pbih_protected, 0, MEM_RELEASE);
				}
			}
		}
	}

	if (!hicDecomp) {

		// Is it MJPEG or I420?  Maybe we can do it ourselves.

		if (is_mjpeg) {
			if (!(mdec = CreateMJPEGDecoder(getImageFormat()->biWidth, getImageFormat()->biHeight)))
				throw MyMemoryError();

			return true;
		} else {
			return false;
		}
	} else {

		// check for bad MPEG-4 V2/V3 codec

		if (isEqualFOURCC(bmih->biCompression, '24PM'))
			return CheckMPEG4Codec(hicDecomp, false);
		else if (isEqualFOURCC(bmih->biCompression, '34PM'))
			return CheckMPEG4Codec(hicDecomp, true);
		else
			return true;
	}
}

///////////////////////////////////////////////////////////////////////////

void VideoSourceAVI::Reinit() {
	long nOldFrames, nNewFrames;

	nOldFrames = lSampleLast - lSampleFirst;
	nNewFrames = pAVIStream->End() - pAVIStream->Start();

	if (mjpeg_splits) {
		nOldFrames >>= 1;
	}

	if (nOldFrames != nNewFrames && (mjpeg_mode==IFMODE_SPLIT1 || mjpeg_mode==IFMODE_SPLIT2)) {
		// We have to resize the mjpeg_splits array.

		long *pNewSplits = new long[lSampleLast - lSampleFirst];

		if (!pNewSplits)
			throw MyMemoryError();

		int i;

		memcpy(pNewSplits, mjpeg_splits, sizeof(long)*nOldFrames);

		for(i=nOldFrames; i<nNewFrames; i++)
			pNewSplits[i] = -1;

		delete[] mjpeg_splits;

		mjpeg_splits = pNewSplits;
	}

	if (pAVIStream->Info(&streamInfo, sizeof streamInfo))
		throw MyError("Error obtaining video stream info.");

	lSampleFirst = pAVIStream->Start();

	if (mjpeg_splits) {
		streamInfo.dwRate *= 2;
		streamInfo.dwLength *= 2;
		lSampleLast = pAVIStream->End() * 2 - lSampleFirst;
	} else
		lSampleLast = pAVIStream->End();

	streamInfo.dwLength		= lSampleLast - lSampleFirst;
}

void VideoSourceAVI::redoKeyFlags() {
	long lSample;
	long lMaxFrame=0;
	long lActualBytes, lActualSamples;
	int err;
	void *lpInputBuffer = NULL;
	void *lpKeyBuffer = NULL;
	BOOL fStreamBegun = FALSE;
	int iBytes;
	long *pFrameSums;

	if (!hicDecomp)
		return;

	iBytes = (lSampleLast+7-lSampleFirst)>>3;

	if (!(key_flags = new char[iBytes]))
		throw MyMemoryError();

	memset(key_flags, 0, iBytes);

	// Find maximum frame

	lSample = lSampleFirst;
	while(lSample < lSampleLast) {
		err = _read(lSample, 1, NULL, 0, &lActualBytes, &lActualSamples);
		if (err == AVIERR_OK)
//			throw MyAVIError("VideoSource", err);

			if (lActualBytes > lMaxFrame) lMaxFrame = lActualBytes;

		++lSample;
	}

	if (!setDecompressedFormat(24))
		if (!setDecompressedFormat(32))
			if (!setDecompressedFormat(16))
				if (!setDecompressedFormat(8))
					throw MyError("Video decompressor is incapable of decompressing to an RGB format.");

	if (!(lpInputBuffer = new char[((lMaxFrame+7)&-8) + lMaxFrame]))
		throw MyMemoryError();

	if (!(pFrameSums = new long[lSampleLast - lSampleFirst])) {
		delete[] lpInputBuffer;
		throw MyMemoryError();
	}

	try {
		ProgressDialog pd(NULL, "AVI Import Filter", "Rekeying video stream", (lSampleLast - lSampleFirst)*2, true);
		pd.setValueFormat("Frame %ld of %ld");

		streamBegin(true);
		fStreamBegun = TRUE;

		lSample = lSampleFirst;
		while(lSample < lSampleLast) {
			long lBlackTotal=0, lWhiteTotal=0;
			long x, y;
			const long lWidth	= (bmihDecompressedFormat->biWidth * bmihDecompressedFormat->biBitCount + 7)/8;
			const long lModulo	= (4-lWidth)&3;
			const long lHeight	= bmihDecompressedFormat->biHeight;
			unsigned char *ptr;

			_RPT1(0,"Rekeying frame %ld\n", lSample);

			err = _read(lSample, 1, lpInputBuffer, lMaxFrame, &lActualBytes, &lActualSamples);
			if (err != AVIERR_OK)
//				throw MyAVIError("VideoSourceAVI", err);
				goto rekey_error;

#if 0
			// decompress frame with an all black background

			memset(lpvBuffer, 0, bmihDecompressedFormat->biSizeImage);
			streamGetFrame(lpInputBuffer, lActualBytes, FALSE, FALSE, lSample);

			ptr = (unsigned char *)lpvBuffer;
			y = lHeight;
			do {
				x = lWidth;
				do {
					lBlackTotal += (long)*ptr++;
				} while(--x);

				ptr += lModulo;
			} while(--y);

			// decompress frame with an all white background

			memset(lpvBuffer, 0xff, bmihDecompressedFormat->biSizeImage);
			streamGetFrame(lpInputBuffer, lActualBytes, FALSE, FALSE, lSample);

			ptr = (unsigned char *)lpvBuffer;
			y = lHeight;
			do {
				x = lWidth;
				do {
					lWhiteTotal += (long)*ptr++;
				} while(--x);

				ptr += lModulo;
			} while(--y);
#else

			streamGetFrame(lpInputBuffer, lActualBytes, FALSE, FALSE, lSample);

			ptr = (unsigned char *)lpvBuffer;
			y = lHeight;
			do {
				x = lWidth;
				do {
					lWhiteTotal += (long)*ptr++;
					lWhiteTotal ^= 0xAAAAAAAA;
				} while(--x);

				ptr += lModulo;
			} while(--y);


			pFrameSums[lSample - lSampleFirst] = lWhiteTotal;
#endif

//			if (lBlackTotal == lWhiteTotal)
//				key_flags[(lSample - lSampleFirst)>>3] |= 1<<((lSample-lSampleFirst)&7);

rekey_error:
			++lSample;

			pd.advance(lSample - lSampleFirst);
			pd.check();
		}

		lSample = lSampleFirst;
		do {
			long lBlackTotal=0, lWhiteTotal=0;
			long x, y;
			const long lWidth	= (bmihDecompressedFormat->biWidth * bmihDecompressedFormat->biBitCount + 7)/8;
			const long lModulo	= (4-lWidth)&3;
			const long lHeight	= bmihDecompressedFormat->biHeight;
			unsigned char *ptr;

			_RPT1(0,"Rekeying frame %ld\n", lSample);

			err = _read(lSample, 1, lpInputBuffer, lMaxFrame, &lActualBytes, &lActualSamples);
			if (err != AVIERR_OK)
//				throw MyAVIError("VideoSourceAVI", err);
				goto rekey_error2;

			streamGetFrame(lpInputBuffer, lActualBytes, FALSE, FALSE, lSample);

			ptr = (unsigned char *)lpvBuffer;
			y = lHeight;
			do {
				x = lWidth;
				do {
					lWhiteTotal += (long)*ptr++;
					lWhiteTotal ^= 0xAAAAAAAA;
				} while(--x);

				ptr += lModulo;
			} while(--y);


			if (lWhiteTotal == pFrameSums[lSample - lSampleFirst])
				key_flags[(lSample - lSampleFirst)>>3] |= 1<<((lSample-lSampleFirst)&7);

rekey_error2:
			if (lSample == lSampleFirst)
				lSample = lSampleLast-1;
			else
				--lSample;

			pd.advance(lSampleLast*2 - (lSample+lSampleFirst));
			pd.check();
		} while(lSample >= lSampleFirst+1);

		streamEnd();
	} catch(...) {
		if (fStreamBegun) streamEnd();
		delete[] lpInputBuffer;
		delete[] pFrameSums;
		throw;
	}

	delete[] lpInputBuffer;
	delete[] pFrameSums;
}

int VideoSourceAVI::_read(LONG lStart, LONG lCount, LPVOID lpBuffer, LONG cbBuffer, LONG *lBytesRead, LONG *lSamplesRead) {
	IAVIReadStream *pSource = pAVIStream;
	bool phase = (lStart - lSampleFirst)&1;

	if (mjpeg_mode == IFMODE_SPLIT1 || mjpeg_mode == IFMODE_SPLIT2)
		lStart = lSampleFirst + (lStart - lSampleFirst)/2;

	// If we're striping, then we have to lookup the correct stripe for this sample.

	if (stripesys) {
		AVIStripeIndexEntry *asie;
		long offset;

		if (!(asie = stripe_index->lookup(lStart)))
			return AVIERR_FILEREAD;

		offset = lStart - asie->lSampleFirst;

		if (lCount > asie->lSampleCount-offset)
			lCount = asie->lSampleCount-offset;

		if (!stripe_streams[asie->lStripe])
			return AVIERR_FILEREAD;

		pSource = stripe_streams[asie->lStripe];
		lStart = asie->lStripeSample + offset;
	}

	// MJPEG modification mode?

	if (mjpeg_mode) {
		int res;
		LONG lBytes, lSamples;
		long lRealSample = lStart;
		long lOffset, lLength;

		// Did we just read in this sample!?

		if (lStart == mjpeg_last) {
			lBytes = mjpeg_last_size;
			res = AVIERR_OK;
		} else {

			// Read the sample into memory.  If we don't have a lpBuffer *and* already know
			// where the split is, just get the size.

			if (lpBuffer || mjpeg_splits[lStart - lSampleFirst]<0) {

				mjpeg_last = -1;

				if (mjpeg_reorder_buffer_size)
					res = pSource->Read(lStart, 1, mjpeg_reorder_buffer, mjpeg_reorder_buffer_size, &lBytes, &lSamples);

				if (res == AVIERR_BUFFERTOOSMALL || !mjpeg_reorder_buffer_size) {
					void *new_buffer;
					int new_size;

					res = pSource->Read(lStart, 1, NULL, 0, &lBytes, &lSamples);

					if (res == AVIERR_OK) {

						_ASSERT(lBytes != 0);

						new_size = (lBytes + 4095) & -4096;
//						new_size = lBytes;
						new_buffer = reallocmem(mjpeg_reorder_buffer, new_size);

						if (!new_buffer)
							return AVIERR_MEMORY;

						mjpeg_reorder_buffer = new_buffer;
						mjpeg_reorder_buffer_size = new_size;

						res = pSource->Read(lStart, 1, mjpeg_reorder_buffer, mjpeg_reorder_buffer_size, &lBytes, &lSamples);
					}
				}

				if (res == AVIERR_OK) {
					mjpeg_last = lStart;
					mjpeg_last_size = lBytes;
				}
			} else
				res = pSource->Read(lStart, 1, NULL, 0, &lBytes, &lSamples);
		}


		if (res != AVIERR_OK) {
			if (lBytesRead)
				*lBytesRead = 0;
			if (lSamplesRead)
				*lSamplesRead = 0;
			return res;
		} else if (!lBytes) {
			if (lBytesRead)
				*lBytesRead = 0;
			if (lSamplesRead)
				*lSamplesRead = 1;
			return AVIERR_OK;
		}

		// Attempt to find SOI tag in sample

		lOffset = 0;
		lLength = lBytes;

		{
			int i;

			// Do we already know where the split is?

			if (mjpeg_splits[lStart-lSampleFirst]<0) {
				for(i=2; i<lBytes-2; i++)
					if (((unsigned char *)mjpeg_reorder_buffer)[i] == (unsigned char)0xFF
							&& ((unsigned char *)mjpeg_reorder_buffer)[i+1] == (unsigned char)0xD8)
						break;

				mjpeg_splits[lStart - lSampleFirst] = i;
			} else {
				i = mjpeg_splits[lStart - lSampleFirst];
			}

			if (i<lBytes-2) {
				if (mjpeg_mode != IFMODE_SWAP) {
					switch(mjpeg_mode) {
					case IFMODE_SPLIT2:
						phase = !phase;
						break;
					case IFMODE_DISCARD1:
						phase = false;
						break;
					case IFMODE_DISCARD2:
						phase = true;
						break;
					}

					if (phase) {
						lOffset = i;
						lLength = lBytes - i;
					} else {
						lOffset = 0;
						lLength = i;
					}
				} else
					lOffset = i;
			}
		}

		if (lpBuffer) {
			if (lSamplesRead)
				*lSamplesRead = 1;
			if (lBytesRead)
				*lBytesRead = lLength;

			if (cbBuffer < lLength)
				return AVIERR_BUFFERTOOSMALL;

			if (mjpeg_mode == IFMODE_SWAP) {
				char *pp1 = (char *)lpBuffer;
				char *pp2 = (char *)lpBuffer + (lBytes - lOffset);

				memcpy(pp1, (char *)mjpeg_reorder_buffer+lOffset, lBytes - lOffset);
				if (lOffset)
					memcpy(pp2, mjpeg_reorder_buffer, lOffset);

				// patch phase on both MJPEG headers

				if (((short *)pp1)[1]==(short)0xE0FF)
					pp1[10] = 1;

				if (((short *)pp2)[1]==(short)0xE0FF)
					pp2[10] = 2;
				
			} else {

				memcpy(lpBuffer, (char *)mjpeg_reorder_buffer+lOffset, lLength);

				// patch phase on MJPEG header by looking for APP0 tag (xFFE0)

				// FFD8 FFE0 0010 'AVI1' polarity

				if (((short *)lpBuffer)[1]==(short)0xE0FF)
					((char *)lpBuffer)[10] = 0;
			}

			return AVIERR_OK;
		} else {
			if (lSamplesRead)
				*lSamplesRead = 1;
			if (lBytesRead)
				*lBytesRead = lLength;

			return AVIERR_OK;
		}

	} else
		return pSource->Read(lStart, lCount, lpBuffer, cbBuffer, lBytesRead, lSamplesRead);
}

BOOL VideoSourceAVI::_isKey(LONG samp) {
	if ((mjpeg_mode & -2) == IFMODE_SPLIT1)
		samp = lSampleFirst + (samp - lSampleFirst)/2;

	if (key_flags) {
		samp -= lSampleFirst;

		return !!(key_flags[samp>>3] & (1<<(samp&7)));
	} else
		return pAVIStream->IsKeyFrame(samp);
}

LONG VideoSourceAVI::nearestKey(LONG lSample) {
	if (key_flags) {
		if (lSample < lSampleFirst || lSample >= lSampleLast)
			return -1;

		if (_isKey(lSample)) return lSample;

		return prevKey(lSample);
	}

//	if (lNear == -1)
//		throw MyError("VideoSourceAVI: error getting previous key frame");

	if ((mjpeg_mode & -2) == IFMODE_SPLIT1) {
		return pAVIStream->NearestKeyFrame(lSampleFirst + (lSample-lSampleFirst)/2)*2-lSampleFirst;
	} else {
		return pAVIStream->NearestKeyFrame(lSample);
	}
}

LONG VideoSourceAVI::prevKey(LONG lSample) {
	if ((mjpeg_mode & -2) == IFMODE_SPLIT1) {
		lSample = lSampleFirst + (lSample - lSampleFirst)/2;

		if (key_flags) {
			if (lSample >= lSampleLast) return -1;

			while(--lSample >= lSampleFirst)
				if (_isKey(lSample)) return lSample*2-lSampleFirst;

			return -1;
		} else
			return pAVIStream->PrevKeyFrame(lSample)*2-lSampleFirst;
	} else {
		if (key_flags) {
			if (lSample >= lSampleLast) return -1;

			while(--lSample >= lSampleFirst)
				if (_isKey(lSample)) return lSample;

			return -1;
		} else
			return pAVIStream->PrevKeyFrame(lSample);
	}
}

LONG VideoSourceAVI::nextKey(LONG lSample) {
	if ((mjpeg_mode & -2) == IFMODE_SPLIT1) {
		lSample = lSampleFirst + (lSample - lSampleFirst)/2;

		if (key_flags) {
			if (lSample < lSampleFirst) return -1;

			while(++lSample < lSampleLast)
				if (_isKey(lSample)) return lSample*2 - lSampleFirst;

			return -1;
		} else
			return pAVIStream->NextKeyFrame(lSample)*2 - lSampleFirst;

	} else {

		if (key_flags) {
			if (lSample < lSampleFirst) return -1;

			while(++lSample < lSampleLast)
				if (_isKey(lSample)) return lSample;

			return -1;
		} else
			return pAVIStream->NextKeyFrame(lSample);

	}
}

bool VideoSourceAVI::setDecompressedFormat(int depth) {
	bDirectDecompress = false;

	VideoSource::setDecompressedFormat(depth);

	if (fUseGDI) {
		void *pv;
		HDC hdc;

		if (depth != 24 && depth != 32 && depth != 16)
			return false;

		if (hbmLame)
			DeleteObject(hbmLame);

		hbmLame = NULL;

		if (hdc = CreateCompatibleDC(NULL)) {
			hbmLame = CreateDIBSection(hdc, (BITMAPINFO *)bmihDecompressedFormat, DIB_RGB_COLORS, &pv, hBufferObject, 0);
			DeleteDC(hdc);
		}

		return true;

	} else if (mdec)
		return depth == 32 || depth == 16;
	else if (hicDecomp)
		return ICERR_OK == ICDecompressQuery(hicDecomp, getImageFormat(), bmihDecompressedFormat);
	else if (getImageFormat()->biCompression == '024I')
		return depth == 32;
	else
		return depth == 32 || depth == 24 || depth == 16;
}

bool VideoSourceAVI::setDecompressedFormat(BITMAPINFOHEADER *pbih) {
	if (pbih->biCompression == BI_RGB)
		return setDecompressedFormat(pbih->biBitCount);

	if (mdec)
		return false;

	if (pbih->biCompression == getImageFormat()->biCompression) {
		const BITMAPINFOHEADER *pbihSrc = getImageFormat();
		if (pbih->biBitCount == pbihSrc->biBitCount
			&& pbih->biSizeImage == pbihSrc->biSizeImage
			&& pbih->biWidth == pbihSrc->biWidth
			&& pbih->biHeight == pbihSrc->biHeight
			&& pbih->biPlanes == pbihSrc->biPlanes) {

			memcpy(bmihDecompressedFormat, pbih, sizeof(BITMAPINFOHEADER));

			bDirectDecompress = true;

			invalidateFrameBuffer();
			return true;
		}
	}

	if (hicDecomp && ICERR_OK == ICDecompressQuery(hicDecomp, getImageFormat(), pbih)) {
		memcpy(bmihDecompressedFormat, pbih, sizeof(BITMAPINFOHEADER));

		invalidateFrameBuffer();
		bDirectDecompress = false;
		return true;
	}

	return false;
}

////////////////////////////////////////////////

void DIBconvert(void *src0, BITMAPINFOHEADER *srcfmt, void *dst0, BITMAPINFOHEADER *dstfmt) {
	if (srcfmt->biCompression == '2YUY')
		VBitmap(dst0, dstfmt).BitBltFromYUY2(0, 0, &VBitmap(src0, srcfmt), 0, 0, -1, -1);
	else if (srcfmt->biCompression == '024I')
		VBitmap(dst0, dstfmt).BitBltFromI420(0, 0, &VBitmap(src0, srcfmt), 0, 0, -1, -1);
	else
		VBitmap(dst0, dstfmt).BitBlt(0, 0, &VBitmap(src0, srcfmt), 0, 0, -1, -1);
}

////////////////////////////////////////////////

void VideoSourceAVI::invalidateFrameBuffer() {
	if (lLastFrame != -1 && hicDecomp)
		ICDecompressEnd(hicDecomp);
	lLastFrame = -1;
}

BOOL VideoSourceAVI::isFrameBufferValid() {
	return lLastFrame != -1;
}

char VideoSourceAVI::getFrameTypeChar(long lFrameNum) {
	if (lFrameNum<lSampleFirst || lFrameNum >= lSampleLast)
		return ' ';

	if (_isKey(lFrameNum))
		return 'K';

	long lBytes, lSamples;
	int err = _read(lFrameNum, 1, NULL, 0, &lBytes, &lSamples);

	if (err != AVIERR_OK)
		return ' ';

	return lBytes ? ' ' : 'D';
}

bool VideoSourceAVI::isStreaming() {
	return pAVIStream->isStreaming();
}

bool VideoSourceAVI::isKeyframeOnly() {
   return pAVIStream->isKeyframeOnly();
}

bool VideoSourceAVI::isType1() {
   return bIsType1;
}

void VideoSourceAVI::streamBegin(bool fRealTime) {
	DWORD err;

	stream_current_frame	= -1;

	pAVIStream->BeginStreaming(lSampleFirst, lSampleLast, fRealTime ? 1000 : 2000);

	use_ICDecompressEx = FALSE;

	if (hicDecomp) {
		BITMAPINFOHEADER *bih_src = bmihTemp;
		BITMAPINFOHEADER *bih_dst = getDecompressedFormat();

		if (ICERR_OK != (err = ICDecompressBegin(
					hicDecomp,
					getImageFormat(),
					getDecompressedFormat()
					)))

			if (err == ICERR_UNSUPPORTED) {
				use_ICDecompressEx = TRUE;

				err = ICDecompressExBegin(
						hicDecomp,
						0,
						bih_src,
						NULL,
						0,0,
						bih_src->biWidth,bih_src->biHeight,
						bih_dst,
						lpvBuffer,
						0,0,
						bih_dst->biWidth,bih_dst->biHeight
						);
				
			}

			if (ICERR_UNSUPPORTED != err && ICERR_OK != err)
				throw MyICError("VideoSourceAVI", err);
	}
}

void *VideoSourceAVI::streamGetFrame(void *inputBuffer, LONG data_len, BOOL is_key, BOOL is_preroll, long frame_num) {
	DWORD err;

	if (!data_len) return getFrameBuffer();

	if (bDirectDecompress) {
		if (data_len < getImageFormat()->biSizeImage)
			throw MyError("VideoSourceAVI: uncompressed frame is short (expected %d bytes, got %d)", getImageFormat()->biSizeImage, data_len);
		
		memcpy(getFrameBuffer(), inputBuffer, getDecompressedFormat()->biSizeImage);
	} else if (fUseGDI) {
		if (!hbmLame)
			throw MyError("Insufficient GDI resources to convert frame.");

		SetDIBits(NULL, hbmLame, 0, getDecompressedFormat()->biHeight, inputBuffer, (BITMAPINFO *)getFormat(),
			DIB_RGB_COLORS);
		GdiFlush();

	} else if (hicDecomp && !bDirectDecompress) {
		// Asus ASV1 crashes with zero byte frames!!!

		if (data_len) {
			BITMAPINFOHEADER *bih_src = bmihTemp;
			BITMAPINFOHEADER *bih_dst = getDecompressedFormat();

			bmihTemp->biSizeImage = data_len;

			VDCHECKPOINT;
			if (use_ICDecompressEx)
				err = 	ICDecompressEx(
							hicDecomp,
	//						  (is_preroll ? ICDECOMPRESS_PREROLL : 0) |
	//						  | (data_len ? 0 : ICDECOMPRESS_NULLFRAME)
							  (is_key ? 0 : ICDECOMPRESS_NOTKEYFRAME),
							bih_src,
							inputBuffer,
							0,0,
							bih_src->biWidth, bih_src->biHeight,
							bih_dst,
							lpvBuffer,
							0,0,
							bih_dst->biWidth, bih_dst->biHeight
							);

			else
				err = 	ICDecompress(
							hicDecomp,
	//						  (is_preroll ? ICDECOMPRESS_PREROLL : 0) |
	//						  | (data_len ? 0 : ICDECOMPRESS_NULLFRAME)
							  (is_key ? 0 : ICDECOMPRESS_NOTKEYFRAME),
							bih_src,
							inputBuffer,
							bih_dst,
							lpvBuffer
							);
			VDCHECKPOINT;

			if (ICERR_OK != err)
				throw MyICError(use_ICDecompressEx ? "VideoSourceAVI [ICDecompressEx]" : "VideoSourceAVI [ICDecompress]", err);
		}

	} else if (mdec) {

		try {
			if (getDecompressedFormat()->biBitCount == 32)
				mdec->decodeFrame32((unsigned long *)getFrameBuffer(), (unsigned char *)inputBuffer, data_len);
			else
				mdec->decodeFrame16((unsigned long *)getFrameBuffer(), (unsigned char *)inputBuffer, data_len);
		} catch(char *s) {
			throw MyError(s);
		}

   } else {
      if (data_len < getImageFormat()->biSizeImage)
         throw MyError("VideoSourceAVI: uncompressed frame is short (expected %d bytes, got %d)", getImageFormat()->biSizeImage, data_len);

      DIBconvert(inputBuffer, getImageFormat(), getFrameBuffer(), getDecompressedFormat());
   }
//		memcpy(getFrameBuffer(), inputBuffer, getDecompressedFormat()->biSizeImage);

	return getFrameBuffer();
}

void VideoSourceAVI::streamEnd() {

	// If an error occurs, but no one is there to hear it, was
	// there ever really an error?

	if (hicDecomp) {
		if (use_ICDecompressEx)
			ICDecompressExEnd(hicDecomp);
		else
			ICDecompressEnd(hicDecomp);
	}

	pAVIStream->EndStreaming();

}

void *VideoSourceAVI::getFrame(LONG lFrameDesired) {
	void *dataBuffer = NULL;
	LONG dataBufferSize = 0;
	LONG lFrameKey, lFrameNum;
	LONG lBytesRead, lSamplesRead;
	DWORD err;
	int aviErr;

	// illegal frame number?

	if (lFrameDesired < lSampleFirst || lFrameDesired >= lSampleLast)
		throw MyError("VideoSourceAVI: bad frame # (%d not within [%d, %d])", lFrameDesired, lSampleFirst, lSampleLast-1);

	// do we already have this frame?

	if (lLastFrame == lFrameDesired)
		return getFrameBuffer();

	// 

	// back us off to the last key frame if we need to

	lFrameNum = lFrameKey = nearestKey(lFrameDesired);

	_RPT1(0,"Nearest key frame: %ld\n", lFrameKey);

	if (lLastFrame > lFrameKey && lLastFrame < lFrameDesired)
		lFrameNum = lLastFrame+1;

	if (hicDecomp) {

		// tell VCM we're going to do a little decompression...

		if (lLastFrame == -1) {
			err = ICDecompressBegin(hicDecomp, getImageFormat(), getDecompressedFormat());
			if (err != ICERR_OK) throw MyICError("VideoSourceAVI", err);
		}
	}

	_RPT2(0,"VideoSourceAVI: obtaining frame %ld, last was %ld\n", lFrameDesired, lLastFrame);

	try {
		do {
			_RPT1(0,"VideoSourceAVI: decompressing frame %ld\n", lFrameNum);

			for(;;) {
				if (!dataBuffer)
					aviErr = AVIERR_BUFFERTOOSMALL;
				else
					aviErr = read(lFrameNum, 1, dataBuffer, dataBufferSize, &lBytesRead, &lSamplesRead);

				if (aviErr == AVIERR_BUFFERTOOSMALL) {
					void *newDataBuffer;

					aviErr = read(lFrameNum, 1, NULL, 0, &lBytesRead, &lSamplesRead);

					if (aviErr) throw MyAVIError("VideoSourceAVI", aviErr);

					dataBufferSize = (lBytesRead+65535) & -65535;

					if (!(newDataBuffer = reallocmem(dataBuffer, dataBufferSize)))
						throw MyMemoryError();

					dataBuffer = newDataBuffer;

				} else if (aviErr) {
					throw MyAVIError("VideoSourceAVI", aviErr);
				} else {
//					if (hicDecomp || lBytesRead) break;
//					--lFrameNum;
					break;
				}
			};

			if (!lBytesRead) continue;

			if (fUseGDI)
				streamGetFrame(dataBuffer, lBytesRead, TRUE, FALSE, lFrameNum);
			else if (hicDecomp && !bDirectDecompress) {
				bmihTemp->biSizeImage = lBytesRead;

				if (lBytesRead) {
					VDCHECKPOINT;
					if (ICERR_OK != (err = 	ICDecompress(
									hicDecomp,
//									  (lFrameNum<lFrameDesired ? ICDECOMPRESS_PREROLL : 0) |
//									  (lBytesRead ? 0 : ICDECOMPRESS_NULLFRAME) |
									  (lFrameNum > lFrameKey ? ICDECOMPRESS_NOTKEYFRAME : 0),
									bmihTemp,
									dataBuffer,
									getDecompressedFormat(),
									lpvBuffer)))

						throw MyICError("VideoSourceAVI", err);
					VDCHECKPOINT;
				}
			} else if (mdec) {
				try {
					if (getDecompressedFormat()->biBitCount == 32)
						mdec->decodeFrame32((unsigned long *)getFrameBuffer(), (unsigned char *)dataBuffer, lBytesRead);
					else
						mdec->decodeFrame16((unsigned long *)getFrameBuffer(), (unsigned char *)dataBuffer, lBytesRead);
				} catch(char *s) {
					throw MyError(s);
				}
			} else {
				if (lBytesRead < getImageFormat()->biSizeImage)
					throw MyError("VideoSourceAVI: uncompressed frame is short (expected %d bytes, got %d)", getImageFormat()->biSizeImage, lBytesRead);

				if (!bDirectDecompress)
					DIBconvert(dataBuffer, getImageFormat(), getFrameBuffer(), getDecompressedFormat());
				else
					memcpy(getFrameBuffer(), dataBuffer, getDecompressedFormat()->biSizeImage);
			}
//			} else memcpy(getFrameBuffer(), dataBuffer, getDecompressedFormat()->biSizeImage);

		} while(++lFrameNum <= lFrameDesired);

	} catch(MyError e) {
		if (dataBuffer) freemem(dataBuffer);
		ICDecompressEnd(hicDecomp);
		lLastFrame = -1;
		throw e;
	}

	if (dataBuffer) freemem(dataBuffer);
//	if (hicDecomp) ICDecompressEnd(hicDecomp);

	lLastFrame = lFrameDesired; 

	return getFrameBuffer();
}
