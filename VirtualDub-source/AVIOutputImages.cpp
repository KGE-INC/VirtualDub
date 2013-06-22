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
	const char *mpszPrefix;
	const char *mpszSuffix;
	int iDigits;
	bool mbSaveAsTARGA;
	char *mpPackBuffer;

public:
	AVIVideoImageOutputStream(AVIOutput *out, const char *pszPrefix, const char *pszSuffix, int iDigits, bool bSaveAsTARGA);
	~AVIVideoImageOutputStream();

	BOOL write(LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer, LONG lSamples);
	BOOL finalize();
};

AVIVideoImageOutputStream::AVIVideoImageOutputStream(AVIOutput *out, const char *pszPrefix, const char *pszSuffix, int iDigits, bool bSaveAsTARGA)
	: AVIVideoOutputStream(out)
	, mpPackBuffer(NULL)
	, mpszPrefix(pszPrefix)
	, mpszSuffix(pszSuffix)
{
	this->iDigits		= iDigits;
	mbSaveAsTARGA		= bSaveAsTARGA;

	dwFrame = 0;
}

AVIVideoImageOutputStream::~AVIVideoImageOutputStream() {
	delete[] mpPackBuffer;
}

BOOL AVIVideoImageOutputStream::write(LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer, LONG lSamples) {
	char szFileName[MAX_PATH];
	HANDLE hFile;
	DWORD dwActual;

	const BITMAPINFOHEADER& bih = *getImageFormat();

	if (mbSaveAsTARGA && (bih.biCompression != BI_RGB ||
			(bih.biBitCount != 16 && bih.biBitCount != 24 && bih.biBitCount != 32))) {
		throw MyError("Output settings must be 16/24/32-bit RGB, uncompressed in order to save a TARGA sequence.");
	}

	sprintf(szFileName, "%s%0*d%s", mpszPrefix, iDigits, dwFrame++, mpszSuffix);

	hFile = CreateFile(
				szFileName,
				GENERIC_WRITE,
				0,
				NULL,
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
				NULL
				);

	if (hFile == INVALID_HANDLE_VALUE)
		throw MyWin32Error("Error writing image file \"%s\":\n%%s", GetLastError(), szFileName);

	try {
		if (mbSaveAsTARGA) {
			struct TGAHeader {
				unsigned char	IDLength;
				unsigned char	CoMapType;
				unsigned char	ImgType;
				unsigned char	IndexLo, IndexHi;
				unsigned char	LengthLo, LengthHi;
				unsigned char	CoSize;
				unsigned char	X_OrgLo, X_OrgHi;
				unsigned char	Y_OrgLo, Y_OrgHi;
				unsigned char	WidthLo, WidthHi;
				unsigned char	HeightLo, HeightHi;
				unsigned char	PixelSize;
				unsigned char	AttBits;
			} hdr;

			hdr.IDLength	= 0;
			hdr.CoMapType	= 0;		// no color map
			hdr.ImgType		= 10;		// run-length, true-color image
			hdr.IndexLo		= 0;		// color map start = 0
			hdr.IndexHi		= 0;
			hdr.LengthLo	= 0;		// color map length = 0
			hdr.LengthHi	= 0;
			hdr.CoSize		= 0;
			hdr.X_OrgLo		= 0;
			hdr.X_OrgHi		= 0;
			hdr.Y_OrgLo		= 0;
			hdr.Y_OrgHi		= 0;
			hdr.WidthLo		= bih.biWidth & 0xff;
			hdr.WidthHi		= bih.biWidth >> 8;
			hdr.HeightLo	= bih.biHeight & 0xff;
			hdr.HeightHi	= bih.biHeight >> 8;
			hdr.PixelSize	= bih.biBitCount;
			hdr.AttBits		= bih.biBitCount==2 ? 1 : bih.biBitCount==4 ? 8 : 0;		// origin is bottom-left, x alpha bits

			// Do we have a pack buffer yet?

			int packrowsize = 129 * ((bih.biWidth * (bih.biBitCount>>3) + 127)>>7);

			if (!mpPackBuffer) {
				mpPackBuffer = new char[(bih.biHeight + 2) * packrowsize];
				if (!mpPackBuffer)
					throw MyMemoryError();
			}

			// Begin RLE packing.

			const int pelsize = bih.biBitCount >> 3;
			const char *src = (const char *)lpBuffer;
			int srcpitch = (pelsize * bih.biWidth+3)&~3;
			char *dstbase = (char *)mpPackBuffer + packrowsize, *dst=dstbase, *dstlimit = dstbase + pelsize * bih.biWidth * bih.biHeight;
			int x, y;

			for(y=0; y<bih.biHeight; ++y) {
				const char *rlesrc = src;

				// copy row into scan buffer and perform necessary masking
				if (pelsize == 2) {
					short *maskdst = (short *)mpPackBuffer;
					const short *masksrc = (const short *)src;

					for(x=0; x<bih.biWidth; ++x) {
						maskdst[x] = masksrc[x] | 0x8000;
					}
					rlesrc = (const char *)mpPackBuffer;
				} else if (pelsize == 4) {
					int *maskdst = (int *)mpPackBuffer;
					const int *masksrc = (const int *)src;

					for(x=0; x<bih.biWidth; ++x) {
						maskdst[x] = masksrc[x] & 0xff000000;
					}
					rlesrc = (const char *)mpPackBuffer;
				}

				// RLE pack row
				const char *rlesrcend = rlesrc + pelsize * bih.biWidth;
				const char *rlecompare = rlesrc;
				int literalbytes = pelsize;
				const char *literalstart = rlesrc;

				rlesrc += pelsize;

				while(rlesrc < rlesrcend) {
					while(rlesrc < rlesrcend && *rlecompare != *rlesrc) {
						++rlecompare;
						++rlesrc;
						++literalbytes;
					}

					int runbytes = 0;
					while(rlesrc < rlesrcend && *rlecompare == *rlesrc) {
						++rlecompare;
						++rlesrc;
						++runbytes;
					}

					int round;
					
					if (pelsize == 3) {
						round = 3 - literalbytes % 3;
						if (round == 3)
							round = 0;
					} else
						round = -literalbytes & (pelsize-1);

					if (runbytes < round) {
						literalbytes += runbytes;
					} else {
						literalbytes += round;
						runbytes -= round;

						int q = runbytes / pelsize;

						if (q > 2 || rlesrc >= rlesrcend) {
							int lq = literalbytes / pelsize;

							while(lq > 128) {
								*dst++ = 0x7f;
								memcpy(dst, literalstart, 128*pelsize);
								dst += 128*pelsize;
								literalstart += 128*pelsize;
								lq -= 128;
							}

							if (lq) {
								*dst++ = lq-1;
								memcpy(dst, literalstart, lq*pelsize);
								dst += lq*pelsize;
							}

							literalbytes = runbytes - q*pelsize;
							literalstart = rlesrc - literalbytes;

							while (q > 128) {
								*dst++ = (char)0xff;
								for(int i=0; i<pelsize; ++i)
									*dst++ = rlesrc[i-runbytes];
								q -= 128;
							}

							if (q) {
								*dst++ = (char)(0x7f + q);
								for(int i=0; i<pelsize; ++i)
									*dst++ = rlesrc[i-runbytes];
							}

						} else {
							literalbytes += runbytes;
						}
					}
					if (rlesrc>=rlesrcend && literalbytes > 0) {
						__asm int 3
					}
				}

				if (dst >= dstlimit) {
					hdr.ImgType = 2;
					break;
				}

				src += srcpitch;
			}

			if (!WriteFile(hFile, &hdr, 18, &dwActual, NULL) || dwActual != 18)
				throw 0;

			if (hdr.ImgType == 10) {		// RLE
				if (!WriteFile(hFile, dstbase, dst-dstbase, &dwActual, NULL) || dwActual != dst-dstbase)
					throw 0;
			} else {
				src = (const char *)lpBuffer;
				for(y=0; y<bih.biHeight; ++y) {
					if (!WriteFile(hFile, src, pelsize*bih.biWidth, &dwActual, NULL) || dwActual != pelsize*bih.biWidth)
						throw 0;

					src += srcpitch;
				}
			}

			if (!WriteFile(hFile, "\0\0\0\0\0\0\0\0TRUEVISION-XFILE.\0", 26, &dwActual, NULL) || dwActual != 26)
				throw 0;

		} else {
			BITMAPFILEHEADER bfh;
			bfh.bfType		= 'MB';
			bfh.bfSize		= sizeof(BITMAPFILEHEADER)+getFormatLen()+cbBuffer;
			bfh.bfReserved1	= 0;
			bfh.bfReserved2	= 0;
			bfh.bfOffBits	= sizeof(BITMAPFILEHEADER)+getFormatLen();

			if (!WriteFile(hFile, &bfh, sizeof(BITMAPFILEHEADER), &dwActual, NULL) || dwActual != sizeof(BITMAPFILEHEADER))
				throw 0;
			if (!WriteFile(hFile, getFormat(), getFormatLen(), &dwActual, NULL) || dwActual != getFormatLen())
				throw 0;
			if (!WriteFile(hFile, lpBuffer, cbBuffer, &dwActual, NULL) || dwActual != cbBuffer)
				throw 0;
		}
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

AVIOutputImages::AVIOutputImages(const char *szFilePrefix, const char *szFileSuffix, int digits, bool bSaveAsTARGA) {
	strcpy(mszFilePrefix, szFilePrefix);
	strcpy(mszFileSuffix, szFileSuffix);
	this->iDigits		= digits;
	mbSaveAsTARGA		= bSaveAsTARGA;
}

AVIOutputImages::~AVIOutputImages() {
}

//////////////////////////////////

BOOL AVIOutputImages::initOutputStreams() {
	if (!(audioOut = new AVIAudioImageOutputStream(this))) return FALSE;
	if (!(videoOut = new AVIVideoImageOutputStream(this, mszFilePrefix, mszFileSuffix, iDigits, mbSaveAsTARGA))) return FALSE;

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
