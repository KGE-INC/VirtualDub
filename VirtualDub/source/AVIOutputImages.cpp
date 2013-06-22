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

#include "VideoSource.h"

#include <vd2/system/error.h>
#include "AVIOutput.h"
#include "AVIOutputImages.h"

class AVIOutputImages;

////////////////////////////////////

class AVIVideoImageOutputStream : public AVIOutputStream {
private:
	DWORD dwFrame;
	const wchar_t *mpszPrefix;
	const wchar_t *mpszSuffix;
	int iDigits;
	bool mbSaveAsTARGA;
	char *mpPackBuffer;

public:
	AVIVideoImageOutputStream(AVIOutput *out, const wchar_t *pszPrefix, const wchar_t *pszSuffix, int iDigits, bool bSaveAsTARGA);
	~AVIVideoImageOutputStream();

	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples);
};

AVIVideoImageOutputStream::AVIVideoImageOutputStream(AVIOutput *out, const wchar_t *pszPrefix, const wchar_t *pszSuffix, int iDigits, bool bSaveAsTARGA)
	: AVIOutputStream(out)
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

void AVIVideoImageOutputStream::write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples) {
	wchar_t szFileName[MAX_PATH];

	const BITMAPINFOHEADER& bih = *(const BITMAPINFOHEADER *)getFormat();

	if (mbSaveAsTARGA && (bih.biCompression != BI_RGB ||
			(bih.biBitCount != 16 && bih.biBitCount != 24 && bih.biBitCount != 32))) {
		throw MyError("Output settings must be 16/24/32-bit RGB, uncompressed in order to save a TARGA sequence.");
	}

	swprintf(szFileName, L"%ls%0*d%ls", mpszPrefix, iDigits, dwFrame++, mpszSuffix);

	using namespace nsVDFile;
	VDFile mFile(szFileName, kWrite | kDenyNone | kCreateAlways | kSequential);

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
		const char *src = (const char *)pBuffer;
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

				VDASSERT(rlesrc<rlesrcend || literalbytes <= 0);
			}

			if (dst >= dstlimit) {
				hdr.ImgType = 2;
				break;
			}

			src += srcpitch;
		}

		mFile.write(&hdr, 18);

		if (hdr.ImgType == 10) {		// RLE
			mFile.write(dstbase, dst-dstbase);
		} else {
			src = (const char *)pBuffer;
			for(y=0; y<bih.biHeight; ++y) {
				mFile.write(src, pelsize*bih.biWidth);

				src += srcpitch;
			}
		}

		mFile.write("\0\0\0\0\0\0\0\0TRUEVISION-XFILE.\0", 26);
	} else {
		BITMAPFILEHEADER bfh;
		bfh.bfType		= 'MB';
		bfh.bfSize		= sizeof(BITMAPFILEHEADER)+getFormatLen()+cbBuffer;
		bfh.bfReserved1	= 0;
		bfh.bfReserved2	= 0;
		bfh.bfOffBits	= sizeof(BITMAPFILEHEADER)+getFormatLen();

		mFile.write(&bfh, sizeof(BITMAPFILEHEADER));
		mFile.write(getFormat(), getFormatLen());
		mFile.write(pBuffer, cbBuffer);
	}

	mFile.close();
}

////////////////////////////////////

AVIOutputImages::AVIOutputImages(const wchar_t *szFilePrefix, const wchar_t *szFileSuffix, int digits, int format) {
	VDASSERT(format == kFormatBMP || format == kFormatTGA);

	mPrefix = szFilePrefix;
	mSuffix = szFileSuffix;
	this->iDigits		= digits;
	mbSaveAsTARGA		= format != kFormatBMP;
}

AVIOutputImages::~AVIOutputImages() {
}

//////////////////////////////////

IVDMediaOutputStream *AVIOutputImages::createVideoStream() {
	VDASSERT(!videoOut);
	if (!(videoOut = new_nothrow AVIVideoImageOutputStream(this, mPrefix.c_str(), mSuffix.c_str(), iDigits, mbSaveAsTARGA)))
		throw MyMemoryError();
	return videoOut;
}

IVDMediaOutputStream *AVIOutputImages::createAudioStream() {
	return NULL;
}

bool AVIOutputImages::init(const wchar_t *szFile) {
	if (!videoOut)
		return false;

	return true;
}

void AVIOutputImages::finalize() {
}
