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

#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include "VideoSourceImages.h"
#include "InputFileImages.h"

extern const char g_szError[];

///////////////////////////////////////////////////////////////////////////

class VDVideoSourceANIM : public VideoSource {
private:
	long	mCachedFrame;
	VBitmap	mvbFrameBuffer;
	VDFile	mFile;
	int		mFrameCount;
	int		mPlanes;

	struct FrameInfo {
		sint64	pos;
		sint32	len;
	};

	std::vector<FrameInfo>	mFrameArray;

	std::vector<uint8>	mDeltaPlanes[2];
	std::vector<uint8>	mPalBuffer;
	uint32	mColorMap[256];

	void parse();

public:
	VDVideoSourceANIM(const wchar_t *pFilename);
	~VDVideoSourceANIM();

	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);
	bool _isKey(VDPosition samp)				{ return !samp; }
	VDPosition nearestKey(VDPosition lSample)	{ return 0; }
	VDPosition prevKey(VDPosition lSample)		{ return lSample>0 ? 0 : -1; }
	VDPosition nextKey(VDPosition lSample)		{ return -1; }

	bool setDecompressedFormat(int depth);
	bool setDecompressedFormat(BITMAPINFOHEADER *pbih);

	void invalidateFrameBuffer()				{ mCachedFrame = -1; }
	bool isFrameBufferValid()					{ return mCachedFrame >= 0; }
	bool isStreaming()							{ return false; }

	const void *getFrame(VDPosition lFrameDesired);
	const void *streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition frame_num);

	char getFrameTypeChar(VDPosition lFrameNum)	{ return !lFrameNum ? 'K' : ' '; }
	eDropType getDropType(VDPosition lFrameNum)	{ return !lFrameNum ? kIndependent : kDependant; }
	bool isKeyframeOnly()						{ return mFrameCount == 1; }
	bool isType1()								{ return false; }
	bool isDecodable(VDPosition sample_num)		{ return !sample_num || mCachedFrame == sample_num-1; }
};

namespace {
#pragma pack(push, 2)
	struct ANIMHeaderOnDisk {
		uint8	operation;
		uint8	mask;
		uint16	w;
		uint16	h;
		sint16	x;
		sint16	y;
		uint32	abstime;
		uint32	reltime;
		uint8	interleave;
		uint8	pad0;
		uint32	bits;
		uint8	pad[16];
	};

	struct ANIMHeader {
		uint8	pad[16];
		uint32	bits;
		uint8	pad0;
		uint8	interleave;
		uint32	reltime;
		uint32	abstime;
		sint16	y;
		sint16	x;
		uint16	h;
		uint16	w;
		uint8	mask;
		uint8	operation;
	};

	struct ILBMHeaderOnDisk {
		uint16	w, h;
		sint16	x, y;
		uint8	nPlanes;
		uint8	masking;
		uint8	compression;
		uint8	pad1;
		uint16	transparentColor;
		uint8	xAspect, yAspect;
		sint16	pageWidth, pageHeight;
	};

	struct ILBMHeader {
		sint16	pageHeight, pageWidth;
		uint8	yAspect, xAspect;
		uint16	transparentColor;
		uint8	pad1;
		uint8	compression;
		uint8	masking;
		uint8	nPlanes;
		sint16	y, x;
		uint16	h, w;
	};
#pragma pack(pop)

	void ConvertANIMHeaderToLocalOrder(ANIMHeader& hdr, ANIMHeaderOnDisk& diskhdr) {
		const char *src = (const char *)&diskhdr + sizeof(ANIMHeader);
		char *dst = (char *)&hdr;

		for(int i=0; i<sizeof(ANIMHeader); ++i)
			*dst++ = *--src;
	}

	void ConvertILBMHeaderToLocalOrder(ILBMHeader& hdr, ILBMHeaderOnDisk& diskhdr) {
		const char *src = (const char *)&diskhdr + sizeof(ILBMHeader);
		char *dst = (char *)&hdr;

		for(int i=0; i<sizeof(ILBMHeader); ++i)
			*dst++ = *--src;
	}

	uint32 VDFromBigEndian32(uint32 v) {
		return (v>>24) + (v<<24) + ((v>>8)&0xff00) + ((v<<8)&0xff0000);
	}
};

VDVideoSourceANIM::VDVideoSourceANIM(const wchar_t *pFilename)
	: mFile(pFilename, nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting)
{
	mpTargetFormatHeader.resize(sizeof(BITMAPINFOHEADER));
	parse();
}

VDVideoSourceANIM::~VDVideoSourceANIM() {
}

void VDVideoSourceANIM::parse() {
	uint32 hdr[3];
	sint64 fsize = mFile.size();
	
	while(mFile.readData(hdr, 8) >= 8) {
		uint32 cksize = VDFromBigEndian32(hdr[1]), tc;
		uint32 ckround = cksize & 1;

		VDDEBUG("Processing: %08lx - %4.4s %d\n", (long)mFile.tell(), &hdr[0], cksize);

		if (cksize > fsize - mFile.tell())
			break;

		switch(hdr[0]) {
		case 'MROF':
			if (cksize < 4 || mFile.readData(hdr + 2, 4) < 4)
				goto parse_finish;
			cksize -= 4;

			switch(hdr[2]) {
			case 'MINA':
				cksize = hdr[1] = 0;		// break open FORM ANIM
				break;
			case 'MBLI':
				cksize = hdr[1] = 0;		// break open FORM ILBM
				break;
			}
			break;

		case 'PAMC':
			{
				uint8 pal[256][3];

				tc = std::min<uint32>(cksize, 768);

				mFile.read(pal, tc);
				cksize -= tc;

				for(int i=0; i<tc/3; ++i) {
					mColorMap[i] = (uint32)pal[i][2] + ((uint32)pal[i][1] << 8) + ((uint32)pal[i][0]<<16);
				}
			}
			break;

		case 'DHMB':
			{
				ILBMHeaderOnDisk ilbmdisk={0};

				tc = std::min<uint32>(sizeof ilbmdisk, cksize);
				mFile.read(&ilbmdisk, tc);
				cksize -= tc;

				ILBMHeader ilbmhdr;
				ConvertILBMHeaderToLocalOrder(ilbmhdr, ilbmdisk);

				BITMAPINFOHEADER *bmih = (BITMAPINFOHEADER *)allocFormat(sizeof(BITMAPINFOHEADER));

				bmih->biSize			= sizeof(BITMAPINFOHEADER);
				bmih->biWidth			= ilbmhdr.w;
				bmih->biHeight			= ilbmhdr.h;
				bmih->biPlanes			= 1;
				bmih->biBitCount		= 16;
				bmih->biCompression		= 0xFFFFFFFF;
				bmih->biSizeImage		= 0;
				bmih->biXPelsPerMeter	= 0;
				bmih->biYPelsPerMeter	= 0;
				bmih->biClrUsed			= 0;
				bmih->biClrImportant	= 0;

				AllocFrameBuffer(ilbmhdr.w * ilbmhdr.h * 4);

				mPlanes = ilbmhdr.nPlanes;

				mDeltaPlanes[0].resize(((ilbmhdr.w + 1) & ~1) * mPlanes * ilbmhdr.h);
				mDeltaPlanes[1].resize(((ilbmhdr.w + 1) & ~1) * mPlanes * ilbmhdr.h);
				mPalBuffer.resize(ilbmhdr.w * ilbmhdr.h);
			}
			break;

		case 'YDOB':
			{
				FrameInfo fi = { mFile.tell(), cksize };

				mFrameArray.push_back(fi);
			}
			break;

		case 'ATLD':
			{
				FrameInfo fi = { mFile.tell(), cksize };

				mFrameArray.push_back(fi);
			}
			break;
		}

		mFile.skip(cksize + ckround);
	}
parse_finish:
	mSampleFirst	= 0;
	mSampleLast		= mFrameArray.size();

	memset(&streamInfo, 0, sizeof streamInfo);

	streamInfo.dwLength		= (DWORD)mSampleLast;
	streamInfo.dwRate		= 15;
	streamInfo.dwScale		= 1;
}

int VDVideoSourceANIM::_read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead) {
	if (lCount > 1)
		lCount = 1;

	uint32 bytes = 0;
	int ret = 0;

	if (lCount > 0) {
		const FrameInfo& fi = mFrameArray[(int)lStart];

		bytes = fi.len;

		if (lpBuffer) {
			if (bytes > cbBuffer)
				ret = AVIERR_BUFFERTOOSMALL;
			else {
				mFile.seek(fi.pos);
				mFile.read(lpBuffer, fi.len);
			}
		}
	}

	if (lBytesRead)
		*lBytesRead = bytes;
	if (lSamplesRead)
		*lSamplesRead = lCount;

	return ret;
}

const void *VDVideoSourceANIM::getFrame(VDPosition lFrameDesired64) {
	long lFrameDesired = (long)lFrameDesired64;
	std::vector<char> dataBuffer;
	LONG lFrameKey, lFrameNum;
	int aviErr;

	// illegal frame number?

	if (lFrameDesired < mSampleFirst || lFrameDesired >= mSampleLast)
		throw MyError("VideoSourceANIM: bad frame # (%d not within [%u, %u])", lFrameDesired, (unsigned)mSampleFirst, (unsigned)(mSampleLast-1));

	// do we already have this frame?

	if (mCachedFrame == lFrameDesired)
		return getFrameBuffer();

	// back us off to the last key frame if we need to

	lFrameNum = lFrameKey = (long)nearestKey(lFrameDesired);

	if (mCachedFrame > lFrameKey && mCachedFrame < lFrameDesired)
		lFrameNum = mCachedFrame+1;

	try {
		do {
			uint32 lBytesRead, lSamplesRead;

			for(;;) {
				if (dataBuffer.empty())
					aviErr = AVIERR_BUFFERTOOSMALL;
				else
					aviErr = read(lFrameNum, 1, &dataBuffer[0], dataBuffer.size(), &lBytesRead, &lSamplesRead);

				if (aviErr == AVIERR_BUFFERTOOSMALL) {
					aviErr = read(lFrameNum, 1, NULL, 0, &lBytesRead, &lSamplesRead);

					if (aviErr)
						throw MyAVIError("VideoSourceANIM", aviErr);

					dataBuffer.resize(lBytesRead ? (lBytesRead+65535) & ~65535 : 1);
				} else if (aviErr) {
					throw MyAVIError("VideoSourceAVI", aviErr);
				} else
					break;
			};

			if (!lBytesRead)
				continue;

			streamGetFrame(&dataBuffer[0], lBytesRead, lFrameNum == lFrameDesired, lFrameNum);
		} while(++lFrameNum <= lFrameDesired);

	} catch(const MyError&) {
		mCachedFrame = -1;
		throw;
	}

	mCachedFrame = lFrameDesired; 

	return getFrameBuffer();
}

const void *VDVideoSourceANIM::streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition frame_num) {
	const uint8 *src = (const uint8 *)inputBuffer;
	uint8 *dst = &mDeltaPlanes[(int)frame_num & 1][0];
	const int w = getImageFormat()->biWidth;
	const int h = getImageFormat()->biHeight;
	const int planepitch = ((w + 15) >> 4) * 2;

	if (isKey(frame_num)) {
		int needed = planepitch * h * mPlanes;

		while(needed > 0) {
			sint8 code = *src++;

			if(code >= 0) {
				needed -= code+1;
				do {
					*dst++ = *src++;
				} while(--code >= 0);
			} else {
				uint8 v = *src++;

				needed -= -code+1;
				do {
					*dst++ = v;
				} while(++code <= 0);
			}
		}

		memcpy(&mDeltaPlanes[(frame_num + 1) & 1][0], &mDeltaPlanes[frame_num & 1][0], planepitch * h * mPlanes);
	} else {
		const uint32 *planeoffsets = (const uint32 *)src;
		ptrdiff_t pitch = planepitch * mPlanes;

		for(int plane = 0; plane < mPlanes; ++plane) {
			src = (const uint8 *)inputBuffer + VDFromBigEndian32(planeoffsets[plane]);

			for(int column = 0; column < planepitch; ++column) {
				uint8 *dstcol = dst++;

				int ops = *src++;

				while(ops-- > 0) {
					uint8 code = *src++;

					if (!code) {
						uint8 count = *src++;
						uint8 v = *src++;

						while(count-- > 0) {
							*dstcol = v;
							dstcol += pitch;
						}
					} else if (code & 0x80) {
						code &= 0x7f;

						while(code-- > 0) {
							*dstcol = *src++;
							dstcol += pitch;
						}
					} else {
						dstcol += pitch * code;
					}
				}
			}
		}
	}

	// convert planar to chunky
	memset(&mPalBuffer[0], 0, mPalBuffer.size());

	for(int plane = 0; plane < mPlanes; ++plane) {
		dst = &mPalBuffer[0];

		for(int y = 0; y < h; ++y) {
			const uint8 *src = &mDeltaPlanes[frame_num & 1][planepitch * (y * mPlanes + plane)];
			const uint8 planemask = (uint8)(1<<plane);

			for(int x = 0; x < w; ++x) {
				if (src[x >> 3] & (0x80 >> (x & 7)))
					*dst += planemask;
				++dst;
			}
		}
	}

	VDPixmap srcbm = {0}, dstbm = {0};

	srcbm.data		= &mPalBuffer[0];
	srcbm.pitch		= w;
	srcbm.w			= w;
	srcbm.h			= h;
	srcbm.format	= nsVDPixmap::kPixFormat_Pal8;
	srcbm.palette	= mColorMap;

	dstbm.pitch		= -(((w * mpTargetFormatHeader->biBitCount + 31) >> 5) << 2);
	dstbm.data		= (char *)getFrameBuffer() - (h-1)*dstbm.pitch;
	dstbm.w			= w;
	dstbm.h			= h;
	dstbm.format	= mpTargetFormatHeader->biBitCount == 32 ? nsVDPixmap::kPixFormat_XRGB8888
					: mpTargetFormatHeader->biBitCount == 24 ? nsVDPixmap::kPixFormat_RGB888
					: mpTargetFormatHeader->biBitCount == 16 ? nsVDPixmap::kPixFormat_XRGB1555
					: nsVDPixmap::kPixFormat_Null;

	VDPixmapBlt(dstbm, srcbm);

	return getFrameBuffer();
}

bool VDVideoSourceANIM::setDecompressedFormat(int depth) {
	if (depth == 16 || depth == 24 || depth == 32) {
		mpTargetFormatHeader->biSize			= sizeof(BITMAPINFOHEADER);
		mpTargetFormatHeader->biWidth			= getImageFormat()->biWidth;
		mpTargetFormatHeader->biHeight		= getImageFormat()->biHeight;
		mpTargetFormatHeader->biPlanes		= 1;
		mpTargetFormatHeader->biCompression	= BI_RGB;
		mpTargetFormatHeader->biBitCount		= (WORD)depth;
		mpTargetFormatHeader->biSizeImage		= ((getImageFormat()->biWidth*depth+31)>>5)*4 * getImageFormat()->biHeight;
		mpTargetFormatHeader->biXPelsPerMeter	= 0;
		mpTargetFormatHeader->biYPelsPerMeter	= 0;
		mpTargetFormatHeader->biClrUsed		= 0;
		mpTargetFormatHeader->biClrImportant	= 0;

		invalidateFrameBuffer();

		mvbFrameBuffer.init((void *)getFrameBuffer(), mpTargetFormatHeader->biWidth, mpTargetFormatHeader->biHeight, depth);
		mvbFrameBuffer.AlignTo4();

		return true;
	}

	return false;
}

bool VDVideoSourceANIM::setDecompressedFormat(BITMAPINFOHEADER *pbih) {
	if (pbih->biCompression == BI_RGB && (pbih->biWidth == getImageFormat()->biWidth) && (pbih->biHeight == getImageFormat()->biHeight) && pbih->biPlanes == 1) {
		return setDecompressedFormat(pbih->biBitCount);
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////

class VDInputFileANIM : public InputFile {
public:
	VDInputFileANIM();
	~VDInputFileANIM();

	void Init(const wchar_t *szFile);

	void setOptions(InputFileOptions *_ifo);
	InputFileOptions *createOptions(const char *buf);
	InputFileOptions *promptForOptions(HWND hwnd);

	void setAutomated(bool fAuto);

	void InfoDialog(HWND hwndParent);
};

VDInputFileANIM::VDInputFileANIM()
{
}

VDInputFileANIM::~VDInputFileANIM() {
}

void VDInputFileANIM::Init(const wchar_t *szFile) {
	videoSrc = new VDVideoSourceANIM(szFile);
}

void VDInputFileANIM::setOptions(InputFileOptions *_ifo) {
}

InputFileOptions *VDInputFileANIM::createOptions(const char *buf) {
	return NULL;
}

InputFileOptions *VDInputFileANIM::promptForOptions(HWND hwnd) {
	return NULL;
}

void VDInputFileANIM::setAutomated(bool fAuto) {
}

void VDInputFileANIM::InfoDialog(HWND hwndParent) {
	MessageBox(hwndParent, "No file information is available for ANIM sequences.", g_szError, MB_OK);
}

///////////////////////////////////////////////////////////////////////////

class VDInputDriverANIM : public vdrefcounted<IVDInputDriver> {
public:
	const wchar_t *GetSignatureName() { return L"IFF ANIM input driver (internal)"; }

	int GetDefaultPriority() {
		return 0;
	}

	uint32 GetFlags() { return kF_Video; }

	const wchar_t *GetFilenamePattern() {
		return L"IFF ANIM (*.anim)\0*.anim\0";
	}

	bool DetectByFilename(const wchar_t *pszFilename) {
		size_t l = wcslen(pszFilename);

		if (l>5 && !wcsicmp(pszFilename + l - 5, L".anim"))
			return true;

		return false;
	}

	int DetectBySignature(const void *pHeader, sint32 nHeaderSize, const void *pFooter, sint32 nFooterSize, sint64 nFileSize) {
		if (nHeaderSize >= 32) {
			const uint8 *buf = (const uint8 *)pHeader;

			if (!memcmp(buf, "FORM", 4) && !memcmp(buf+8, "ANIM", 4))
				return 1;
		}

		return -1;
	}

	InputFile *CreateInputFile(uint32 flags) {
		return new VDInputFileANIM;
	}
};

extern IVDInputDriver *VDCreateInputDriverANIM() { return new VDInputDriverANIM; }
