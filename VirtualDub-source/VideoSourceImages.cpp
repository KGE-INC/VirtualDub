#include <stdio.h>
#include <ctype.h>

#include <windows.h>
#include <vfw.h>

#include "oshelper.h"
#include "Error.h"
#include "ProgressDialog.h"
#include "VideoSourceImages.h"
#include "image.h"

extern HWND g_hWnd;

VideoSourceImages::VideoSourceImages(const char *pszBaseFormat)
: mCachedHandleFrame(-1)
, mCachedHandle(INVALID_HANDLE_VALUE)
{
	try {
		_construct(pszBaseFormat);
	} catch(...) {
		_destruct();
		throw;
	}
}

void VideoSourceImages::_construct(const char *pszBaseFormat) {
	// Attempt to discern path format.
	//
	// First, find the start of the filename.  Then skip
	// backwards until the first period is found, then to the
	// beginning of the first number.

	const char *pszFileBase = SplitPathName(pszBaseFormat);
	const char *s = pszFileBase;
	const char *pszLastDigit = NULL;

	while(*s)
		++s;

	while(s > pszFileBase && s[-1] != '.')
		--s;

	while(s > pszFileBase) {
		--s;

		if (isdigit((unsigned char)*s)) {
			pszLastDigit = s;
			break;
		}
	}

	if (!pszLastDigit)
		throw MyError("Cannot load image sequence: unable to find sequence number in base filename:\n\"%s\"", pszBaseFormat);

	const char *pszFirstDigit = pszLastDigit;

	while(pszFirstDigit > pszBaseFormat && isdigit((unsigned char)pszFirstDigit[-1]))
		--pszFirstDigit;

	// Compute # of digits and first number.

	mImageBaseNumber = atoi(pszFirstDigit);

	sprintf(mszPathFormat, "%.*s%%0%dd%s", pszFirstDigit - pszBaseFormat, pszBaseFormat, pszLastDigit+1 - pszFirstDigit, pszLastDigit+1);

	// Continue on.

	lSampleFirst = 0;

	bmihDecompressedFormat = new BITMAPINFOHEADER;
	allocFormat(sizeof(BITMAPINFOHEADER));

	invalidateFrameBuffer();

	// This has to be 1 so that read() doesn't kick away the request.

	lSampleLast = 1;
	getFrame(0);
	lSampleLast = 0;

	// Stat as many files as we can until we get an error.

	try {
		ProgressDialog pd(g_hWnd, "Image import filter", "Scanning for images", 0x3FFFFFFF, true);

		pd.setValueFormat(mszPathFormat + (pszFileBase - pszBaseFormat));

		while(!_read(lSampleLast, 1, NULL, 0x7FFFFFFF, NULL, NULL)) {
			pd.advance(lSampleLast + mImageBaseNumber);
			pd.check();
			++lSampleLast;
		}
	} catch(const MyError&) {
		/* nothing */
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
	streamInfo.dwLength					= lSampleLast;
	streamInfo.dwInitialFrames			= 0;
	streamInfo.dwSuggestedBufferSize	= 0;
	streamInfo.dwQuality				= (DWORD)-1;
	streamInfo.dwSampleSize				= 0;
	streamInfo.rcFrame.left				= 0;
	streamInfo.rcFrame.top				= 0;
	streamInfo.rcFrame.right			= getImageFormat()->biWidth;
	streamInfo.rcFrame.bottom			= getImageFormat()->biHeight;
}

void VideoSourceImages::_destruct() {
	delete bmihDecompressedFormat;
	bmihDecompressedFormat = NULL;

	if (mCachedHandle != INVALID_HANDLE_VALUE) {
		CloseHandle(mCachedHandle);
		mCachedHandle = INVALID_HANDLE_VALUE;
	}
}

VideoSourceImages::~VideoSourceImages() {
	_destruct();
}

int VideoSourceImages::_read(LONG lStart, LONG lCount, LPVOID lpBuffer, LONG cbBuffer, LONG *plBytesRead, LONG *plSamplesRead) {
	if (plBytesRead)
		*plBytesRead = 0;

	if (plSamplesRead)
		*plSamplesRead = 0;

	char buf[512];

	if (_snprintf(buf, sizeof buf, mszPathFormat, lStart + mImageBaseNumber) < 0)
		return 0;

	// Check if we already have the file handle cached.  If not, open the file.

	HANDLE h;
	
	if (lStart == mCachedHandleFrame) {
		h = mCachedHandle;
		if (0xFFFFFFFF == SetFilePointer(h, 0, NULL, FILE_BEGIN))
			goto read_fail;
	} else{
		h = CreateFile(buf, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

		if (h == INVALID_HANDLE_VALUE)
			goto read_fail;

		// Close the old cached handle and replace it with the new one.

		if (mCachedHandle != INVALID_HANDLE_VALUE)
			CloseHandle(mCachedHandle);

		mCachedHandle = h;
		mCachedHandleFrame = lStart;
	}

	// Replace

	DWORD dwSizeHi, dwSizeLo;
	
	dwSizeLo = GetFileSize(h, &dwSizeHi);

	if (dwSizeLo == 0xFFFFFFFFUL && GetLastError() != NO_ERROR) {
		goto read_fail;
	};

	if (!lpBuffer) {
		if (plBytesRead)
			*plBytesRead = dwSizeLo;

		if (plSamplesRead)
			*plSamplesRead = 1;

		return 0;
	}

	if (dwSizeHi || dwSizeLo > cbBuffer) {
		if (plBytesRead) {
			if (dwSizeHi || dwSizeLo > 0x7FFFFFFFUL)
				*plBytesRead = 0x7FFFFFFFUL;		// uh oh....
			else
				*plBytesRead = dwSizeLo;
		}

		return AVIERR_BUFFERTOOSMALL;
	}

	DWORD dwActual;
	BOOL bSucceeded;
	
	bSucceeded = ReadFile(h, lpBuffer, dwSizeLo, &dwActual, NULL);
	
	if (!bSucceeded)
		goto read_fail;

	if (plBytesRead)
		*plBytesRead = (LONG)dwActual;

	if (plSamplesRead)
		*plSamplesRead = 1;

	return 0;

read_fail:
	throw MyWin32Error("Cannot read image file \"%s\":\n%%s", GetLastError(), buf);
}

bool VideoSourceImages::setDecompressedFormat(int depth) {
	if (depth == 16 || depth == 24 || depth == 32) {
		bmihDecompressedFormat->biSize			= sizeof(BITMAPINFOHEADER);
		bmihDecompressedFormat->biWidth			= getImageFormat()->biWidth;
		bmihDecompressedFormat->biHeight		= getImageFormat()->biHeight;
		bmihDecompressedFormat->biPlanes		= 1;
		bmihDecompressedFormat->biCompression	= BI_RGB;
		bmihDecompressedFormat->biBitCount		= depth;
		bmihDecompressedFormat->biSizeImage		= ((getImageFormat()->biWidth*depth+31)>>5)*4 * getImageFormat()->biHeight;
		bmihDecompressedFormat->biXPelsPerMeter	= 0;
		bmihDecompressedFormat->biYPelsPerMeter	= 0;
		bmihDecompressedFormat->biClrUsed		= 0;
		bmihDecompressedFormat->biClrImportant	= 0;

		invalidateFrameBuffer();

		mvbFrameBuffer.init(getFrameBuffer(), bmihDecompressedFormat->biWidth, bmihDecompressedFormat->biHeight, depth);
		mvbFrameBuffer.AlignTo4();

		return true;
	}

	return false;
}

bool VideoSourceImages::setDecompressedFormat(BITMAPINFOHEADER *pbih) {
	if (pbih->biCompression == BI_RGB && (pbih->biWidth == getImageFormat()->biWidth) && (pbih->biHeight == getImageFormat()->biHeight) && pbih->biPlanes == 1) {
		return setDecompressedFormat(pbih->biBitCount);
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////

void *VideoSourceImages::streamGetFrame(void *inputBuffer, long data_len, BOOL is_key, BOOL is_preroll, long frame_num) {
	// We may get a zero-byte frame if we already have the image.

	if (!data_len)
		return getFrameBuffer();

	int w, h;
	bool bHasAlpha;
	bool bIsBMP = DecodeBMPHeader(inputBuffer, data_len, w, h, bHasAlpha);
	bool bIsTGA = !bIsBMP && DecodeTGAHeader(inputBuffer, data_len, w, h, bHasAlpha);

	if (!bIsBMP && !bIsTGA)
		throw MyError("Image file must be in Windows BMP or truecolor TARGA format.");

	// Check image header.

	BITMAPINFOHEADER *pFormat = getImageFormat();

	if (getFrameBuffer()) {
		if (w != pFormat->biWidth || h != pFormat->biHeight)
			throw MyError("Image %d (%dx%d) doesn't match the image dimensions of the first image (%dx%d)."
					, frame_num + mImageBaseNumber, w, h, pFormat->biWidth, pFormat->biHeight);

	} else {
		void *pFrameBuffer = AllocFrameBuffer(w * h * 4);

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

	if (bIsBMP)
		DecodeBMP(inputBuffer, data_len, mvbFrameBuffer);
	if (bIsTGA)
		DecodeTGA(inputBuffer, data_len, mvbFrameBuffer);

	mCachedFrame = frame_num;

	return lpvBuffer;
}

void *VideoSourceImages::getFrame(LONG frameNum) {
	long lBytes;
	void *pFrame = NULL;

	if (!read(frameNum, 1, NULL, 0x7FFFFFFF, &lBytes, NULL) && lBytes) {
		char *pBuffer = new char[lBytes];

		try {
			long lReadBytes;

			read(frameNum, 1, pBuffer, lBytes, &lReadBytes, NULL);
			pFrame = streamGetFrame(pBuffer, lReadBytes, TRUE, FALSE, frameNum);
		} catch(MyError e) {
			delete[] pBuffer;
			throw;
		}

		delete[] pBuffer;
	}

	return pFrame;
}

