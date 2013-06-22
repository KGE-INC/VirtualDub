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

#include <stdio.h>
#include <crtdbg.h>

#include "VideoSource.h"

#include "Error.h"
#include "AVIOutput.h"
#include "AVIOutputImages.h"

class AVIOutputImages;

////////////////////////////////////

class AVIAudioImageOutputStream : public AVIAudioOutputStream {
public:
	AVIAudioImageOutputStream(AVIOutput *out);

	BOOL write(LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer, LONG lSamples);
	BOOL finalize();
	BOOL flush();
};

AVIAudioImageOutputStream::AVIAudioImageOutputStream(AVIOutput *out) : AVIAudioOutputStream(out) {
}

BOOL AVIAudioImageOutputStream::write(LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer, LONG lSamples) {
	return TRUE;
}

BOOL AVIAudioImageOutputStream::finalize() {
	return TRUE;
}

BOOL AVIAudioImageOutputStream::flush() {
	return TRUE;
}

////////////////////////////////////

class AVIVideoImageOutputStream : public AVIVideoOutputStream {
private:
	DWORD dwFrame;
	char *szFormat;
	int iDigits;

public:
	AVIVideoImageOutputStream(AVIOutput *out, char *szFormat, int iDigits);

	BOOL write(LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer, LONG lSamples);
	BOOL finalize();
};

AVIVideoImageOutputStream::AVIVideoImageOutputStream(AVIOutput *out, char *szFormat, int iDigits) : AVIVideoOutputStream(out) {
	this->szFormat		= szFormat;
	this->iDigits		= iDigits;

	dwFrame = 0;
}

BOOL AVIVideoImageOutputStream::write(LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer, LONG lSamples) {
	BITMAPFILEHEADER bfh;
	char szFileName[MAX_PATH];
	HANDLE hFile;
	DWORD dwActual;

	sprintf(szFileName, szFormat, iDigits, dwFrame++);

	hFile = CreateFile(
				szFileName,
				GENERIC_WRITE,
				0,
				NULL,
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
				NULL
				);

	if (!hFile) return FALSE;

	bfh.bfType		= 'MB';
	bfh.bfSize		= sizeof(BITMAPFILEHEADER)+getFormatLen()+cbBuffer;
	bfh.bfReserved1	= 0;
	bfh.bfReserved2	= 0;
	bfh.bfOffBits	= sizeof(BITMAPFILEHEADER)+getFormatLen();

	try {
		if (!WriteFile(hFile, &bfh, sizeof(BITMAPFILEHEADER), &dwActual, NULL) || dwActual != sizeof(BITMAPFILEHEADER))
			throw 0;
		if (!WriteFile(hFile, getFormat(), getFormatLen(), &dwActual, NULL) || dwActual != getFormatLen())
			throw 0;
		if (!WriteFile(hFile, lpBuffer, cbBuffer, &dwActual, NULL) || dwActual != cbBuffer)
			throw 0;
	} catch(int) {
		CloseHandle(hFile);
		throw MyWin32Error("Error writing image: %%s", GetLastError());
	}

	if (!CloseHandle(hFile)) return FALSE;

	return TRUE;
}

BOOL AVIVideoImageOutputStream::finalize() {
	return TRUE;
}

////////////////////////////////////

AVIOutputImages::AVIOutputImages(char *szFormatString, int digits) {
	strcpy(this->szFormat, szFormatString);
	this->iDigits		= digits;
}

AVIOutputImages::~AVIOutputImages() {
}

//////////////////////////////////

BOOL AVIOutputImages::initOutputStreams() {
	if (!(audioOut = new AVIAudioImageOutputStream(this))) return FALSE;
	if (!(videoOut = new AVIVideoImageOutputStream(this, szFormat, iDigits))) return FALSE;

	return TRUE;
}

BOOL AVIOutputImages::init(const char *szFile, LONG xSize, LONG ySize, BOOL videoIn, BOOL audioIn, LONG bufferSize, BOOL is_interleaved) {
	if (audioIn) {
		if (!audioOut) return FALSE;
	} else {
		delete audioOut;
		audioOut = NULL;
	}

	if (!videoOut) return FALSE;

	return TRUE;
}

BOOL AVIOutputImages::finalize() {
	return TRUE;
}

BOOL AVIOutputImages::isPreview() { return FALSE; }

void AVIOutputImages::writeIndexedChunk(FOURCC ckid, LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer) {
}
