#include "stdafx.h"
#include <ctype.h>

#include <windows.h>
#include <vfw.h>

#include "oshelper.h"
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/vdalloc.h>
#include "ProgressDialog.h"
#include "VideoSourceImages.h"
#include "image.h"
#include "imagejpegdec.h"

extern HWND g_hWnd;

VideoSourceImages::VideoSourceImages(const wchar_t *pszBaseFormat)
	: mCachedHandleFrame(-1)
{
	// Attempt to discern path format.
	//
	// First, find the start of the filename.  Then skip
	// backwards until the first period is found, then to the
	// beginning of the first number.

	const wchar_t *pszFileBase = VDFileSplitPath(pszBaseFormat);
	const wchar_t *s = pszFileBase;
	const wchar_t *pszLastDigit = NULL;

	while(*s)
		++s;

	while(s > pszFileBase && s[-1] != L'.')
		--s;

	while(s > pszFileBase) {
		--s;

		if (iswdigit(*s)) {
			pszLastDigit = s;
			break;
		}
	}

//	Explicitly allow no sequence number to open a single image.
//
//	if (!pszLastDigit)
//		throw MyError("Cannot load image sequence: unable to find sequence number in base filename:\n\"%s\"", pszBaseFormat);

	if (!pszLastDigit) {
		mImageBaseNumber = 0;
		wcscpy(mszPathFormat, pszBaseFormat);
	} else {
		const wchar_t *pszFirstDigit = pszLastDigit;

		while(pszFirstDigit > pszBaseFormat && iswdigit(pszFirstDigit[-1]))
			--pszFirstDigit;

		// Compute # of digits and first number.

		mImageBaseNumber = wcstol(pszFirstDigit, NULL, 10);

		swprintf(mszPathFormat, L"%.*s%%0%dd%s", pszFirstDigit - pszBaseFormat, pszBaseFormat, pszLastDigit+1 - pszFirstDigit, pszLastDigit+1);
	}

	// Continue on.

	mSampleFirst = 0;

	mpTargetFormatHeader.resize(sizeof(BITMAPINFOHEADER));
	allocFormat(sizeof(BITMAPINFOHEADER));

	invalidateFrameBuffer();

	// This has to be 1 so that read() doesn't kick away the request.

	mSampleLast = 1;
	getFrame(0);

	// Stat as many files as we can until we get an error.

	if (pszLastDigit) {
		VDStringA statusFormat(VDTextWToA(mszPathFormat + (pszFileBase - pszBaseFormat)));
		try {
			ProgressDialog pd(g_hWnd, "Image import filter", "Scanning for images", 0x3FFFFFFF, true);

			pd.setValueFormat(statusFormat.c_str());

			while(!_read(mSampleLast, 1, NULL, 0x7FFFFFFF, NULL, NULL)) {
				pd.advance(mSampleLast + mImageBaseNumber);
				pd.check();
				++mSampleLast;
			}
		} catch(const MyError&) {
			/* nothing */
		}
	}

	// Fill out streamInfo

	streamInfo.fccType					= streamtypeVIDEO;
	streamInfo.fccHandler				= NULL;
	streamInfo.dwFlags					= 0;
	streamInfo.dwCaps					= 0;
	streamInfo.wPriority				= 0;
	streamInfo.wLanguage				= 0;
	streamInfo.dwScale					= 1001;
	streamInfo.dwRate					= 30000;
	streamInfo.dwStart					= 0;
	streamInfo.dwLength					= mSampleLast > 0xFFFFFFFF ? 0xFFFFFFFF : (DWORD)mSampleLast;		// don't use min<> -- code misgeneration!
	streamInfo.dwInitialFrames			= 0;
	streamInfo.dwSuggestedBufferSize	= 0;
	streamInfo.dwQuality				= (DWORD)-1;
	streamInfo.dwSampleSize				= 0;
	streamInfo.rcFrame.left				= 0;
	streamInfo.rcFrame.top				= 0;
	streamInfo.rcFrame.right			= getImageFormat()->biWidth;
	streamInfo.rcFrame.bottom			= getImageFormat()->biHeight;
}

VideoSourceImages::~VideoSourceImages() {
}

int VideoSourceImages::_read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *plBytesRead, uint32 *plSamplesRead) {
	if (plBytesRead)
		*plBytesRead = 0;

	if (plSamplesRead)
		*plSamplesRead = 0;

	wchar_t buf[512];

	if (_snwprintf(buf, sizeof buf / sizeof buf[0], mszPathFormat, lStart + mImageBaseNumber) < 0)
		return 0;

	// Check if we already have the file handle cached.  If not, open the file.
	
	if (lStart == mCachedHandleFrame) {
		mCachedFile.seek(0);
	} else{
		mCachedFile.closeNT();
		mCachedFile.open(buf, nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting);
		mCachedHandleFrame = lStart;
	}

	// Replace

	uint32 size = (uint32)mCachedFile.size();

	if (size > 0x3fffffff)
		throw MyError("VideoSourceImages: File \"%s\" is too large (>1GB).", VDTextWToA(buf).c_str());

	if (!lpBuffer) {
		if (plBytesRead)
			*plBytesRead = size;

		if (plSamplesRead)
			*plSamplesRead = 1;

		return 0;
	}

	if (size > cbBuffer) {
		if (plBytesRead)
			*plBytesRead = size;

		return AVIERR_BUFFERTOOSMALL;
	}

	mCachedFile.read(lpBuffer, size);
	
	if (plBytesRead)
		*plBytesRead = size;

	if (plSamplesRead)
		*plSamplesRead = 1;

	return 0;
}

bool VideoSourceImages::setTargetFormat(int format) {
	if (!format)
		format = nsVDPixmap::kPixFormat_XRGB8888;

	switch(format) {
	case nsVDPixmap::kPixFormat_XRGB1555:
	case nsVDPixmap::kPixFormat_RGB888:
	case nsVDPixmap::kPixFormat_XRGB8888:
		if (!VideoSource::setTargetFormat(format))
			return false;

		invalidateFrameBuffer();

		mvbFrameBuffer.init((void *)getFrameBuffer(), mpTargetFormatHeader->biWidth, mpTargetFormatHeader->biHeight, mpTargetFormatHeader->biBitCount);
		mvbFrameBuffer.AlignTo4();

		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////

const void *VideoSourceImages::streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition frame_num) {
	// We may get a zero-byte frame if we already have the image.

	if (!data_len)
		return getFrameBuffer();

	int w, h;
	bool bHasAlpha;
	bool bIsJPG = IsJPEGHeader(inputBuffer, data_len);
	bool bIsBMP = !bIsJPG && DecodeBMPHeader(inputBuffer, data_len, w, h, bHasAlpha);
	bool bIsTGA = !bIsJPG && !bIsBMP && DecodeTGAHeader(inputBuffer, data_len, w, h, bHasAlpha);

	if (!bIsJPG && !bIsBMP && !bIsTGA)
		throw MyError("Image file must be in Windows BMP, truecolor TARGA, or sequential JPEG format.");

	vdautoptr<IVDJPEGDecoder> pDecoder;

	if (bIsJPG) {
		pDecoder = VDCreateJPEGDecoder();
		pDecoder->Begin(inputBuffer, data_len);
		pDecoder->DecodeHeader(w, h);
	}


	// Check image header.

	BITMAPINFOHEADER *pFormat = getImageFormat();

	if (getFrameBuffer()) {
		if (w != pFormat->biWidth || h != pFormat->biHeight)
			throw MyError("Image %lu (%dx%d) doesn't match the image dimensions of the first image (%dx%d)."
					, (unsigned long)(frame_num + mImageBaseNumber), w, h, pFormat->biWidth, pFormat->biHeight);

	} else {
		AllocFrameBuffer(w * h * 4);

		pFormat->biSize				= sizeof(BITMAPINFOHEADER);
		pFormat->biWidth			= w;
		pFormat->biHeight			= h;
		pFormat->biPlanes			= 1;
		pFormat->biCompression		= 0xFFFFFFFFUL;
		pFormat->biBitCount			= 0;
		pFormat->biXPelsPerMeter	= 0;
		pFormat->biYPelsPerMeter	= 0;
		pFormat->biClrUsed			= 0;
		pFormat->biClrImportant		= 0;

		// special case for initial read in constructor

		return NULL;

	}

	if (bIsJPG) {
		int format;

		switch(mvbFrameBuffer.depth) {
		case 16:	format = IVDJPEGDecoder::kFormatXRGB1555;	break;
		case 24:	format = IVDJPEGDecoder::kFormatRGB888;		break;
		case 32:	format = IVDJPEGDecoder::kFormatXRGB8888;	break;
		}

		pDecoder->DecodeImage((char *)mvbFrameBuffer.data + mvbFrameBuffer.pitch * (mvbFrameBuffer.h - 1), -mvbFrameBuffer.pitch, format);
		pDecoder->End();
	}

	if (bIsBMP)
		DecodeBMP(inputBuffer, data_len, mvbFrameBuffer);
	if (bIsTGA)
		DecodeTGA(inputBuffer, data_len, mvbFrameBuffer);

	mCachedFrame = frame_num;

	return lpvBuffer;
}

const void *VideoSourceImages::getFrame(VDPosition frameNum) {
	uint32 lBytes;
	const void *pFrame = NULL;

	if (mCachedFrame == frameNum)
		return lpvBuffer;

	if (!read(frameNum, 1, NULL, 0x7FFFFFFF, &lBytes, NULL) && lBytes) {
		char *pBuffer = new char[lBytes];

		try {
			uint32 lReadBytes;

			read(frameNum, 1, pBuffer, lBytes, &lReadBytes, NULL);
			pFrame = streamGetFrame(pBuffer, lReadBytes, FALSE, frameNum);
		} catch(MyError e) {
			delete[] pBuffer;
			throw;
		}

		delete[] pBuffer;
	}

	return pFrame;
}

