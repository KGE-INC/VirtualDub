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

#include <vd2/system/binary.h>
#include <vd2/system/error.h>
#include <vd2/system/file.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include "VideoSource.h"
#include "VBitmap.h"
#include "InputFile.h"

extern const char g_szError[];

///////////////////////////////////////////////////////////////////////////

class VDVideoSourceANIM : public VideoSource {
public:
	VDVideoSourceANIM(const wchar_t *pFilename);
	~VDVideoSourceANIM();

	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);
	bool _isKey(VDPosition samp);
	VDPosition nearestKey(VDPosition lSample);
	VDPosition prevKey(VDPosition lSample);
	VDPosition nextKey(VDPosition lSample);

	bool setTargetFormat(int format);

	void invalidateFrameBuffer()				{ mCachedFrame = -1; }
	bool isFrameBufferValid()					{ return mCachedFrame >= 0; }
	bool isStreaming()							{ return false; }

	const void *getFrame(VDPosition lFrameDesired);

	void streamBegin(bool fRealTime, bool bForceReset);
	void streamEnd();
	const void *streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition sample_num, VDPosition target_sample);

	char getFrameTypeChar(VDPosition lFrameNum)	{ return !lFrameNum ? 'K' : ' '; }
	eDropType getDropType(VDPosition lFrameNum)	{ return !lFrameNum ? kIndependent : kDependant; }
	bool isKeyframeOnly()						{ return mFrameCount == 1; }
	bool isType1()								{ return false; }
	bool isDecodable(VDPosition sample_num)		{ return !sample_num || mCachedFrame == sample_num-1; }

private:
	void parse();

	void DecompressMode5(const uint8 *src, uint32 srclen, uint8 *dst, ptrdiff_t planepitch);
	void DecompressMode7Short(const uint8 *src, uint32 srclen, uint8 *dst, ptrdiff_t planepitch);
	void DecompressMode7Long(const uint8 *src, uint32 srclen, uint8 *dst, ptrdiff_t planepitch);

	long	mCachedFrame;
	VBitmap	mvbFrameBuffer;
	VDFile	mFile;
	int		mFrameCount;
	int		mPlanes;
	uint8	mCompressionMode;
	uint32	mCompressionOptions;
	uint32	mAmigaViewportMode;
	bool	mbDecodeStarted;
	bool	mbDecodeRealTime;

	struct FrameInfo {
		sint64	pos;
		sint32	len;
		bool	key;
	};

	std::vector<FrameInfo>	mFrameArray;

	vdfastvector<uint8>	mDeltaPlanes[2];
	vdfastvector<uint8>	mPalBuffer;
	uint32	mColorMap[256];
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

	enum {
		kAmigaVPHalfbrite		= 0x80,
		kAmigaVPHoldAndModify	= 0x800
	};
};

VDVideoSourceANIM::VDVideoSourceANIM(const wchar_t *pFilename)
	: mFile(pFilename, nsVDFile::kRead | nsVDFile::kDenyWrite | nsVDFile::kOpenExisting)
	, mbDecodeStarted(false)
	, mbDecodeRealTime(false)
{
	mpTargetFormatHeader.resize(sizeof(BITMAPINFOHEADER));
	parse();
}

VDVideoSourceANIM::~VDVideoSourceANIM() {
}

void VDVideoSourceANIM::parse() {
	uint32 hdr[3];
	sint64 fsize = mFile.size();

	mAmigaViewportMode = 0;
	
	while(mFile.readData(hdr, 8) >= 8) {
		uint32 cksize = VDFromBigEndian32(hdr[1]), tc;
		uint32 ckround = cksize & 1;

		VDDEBUG("Processing: %08lx - %4.4s %d\n", (long)mFile.tell(), &hdr[0], cksize);

		if (cksize > fsize - mFile.tell())
			break;

		switch(hdr[0]) {
		case VDMAKEFOURCC('F', 'O', 'R', 'M'):
			if (cksize < 4 || mFile.readData(hdr + 2, 4) < 4)
				goto parse_finish;
			cksize -= 4;

			switch(hdr[2]) {
			case VDMAKEFOURCC('A', 'N', 'I', 'M'):
				cksize = hdr[1] = 0;		// break open FORM ANIM
				break;
			case VDMAKEFOURCC('I', 'L', 'B', 'M'):
				cksize = hdr[1] = 0;		// break open FORM ILBM
				break;
			}
			break;

		case VDMAKEFOURCC('C', 'M', 'A', 'P'):
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

		case VDMAKEFOURCC('C', 'A', 'M', 'G'):
			if (cksize >= 4) {
				cksize -= 4;
				uint32 vpmode;
				mFile.read(&vpmode, 4);
				mAmigaViewportMode = VDSwizzleU32(vpmode);
			}
			break;

		case VDMAKEFOURCC('A', 'N', 'H', 'D'):
			{
				ANIMHeaderOnDisk animdisk={0};

				tc = std::min<uint32>(sizeof animdisk, cksize);
				mFile.read(&animdisk, tc);
				cksize -= tc;

				ANIMHeader animhdr;
				ConvertANIMHeaderToLocalOrder(animhdr, animdisk);

				mCompressionMode = animhdr.operation;
				mCompressionOptions = animhdr.bits;

				switch(mCompressionMode) {
				case 0:
				case 5:
				case 7:
					break;
				default:
					throw MyError("The file \"%ls\" uses an unsupported compression mode (%d).", mFile.getFilenameForError(), mCompressionMode);
				}
			}
			break;

		case VDMAKEFOURCC('B', 'M', 'H', 'D'):
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

		case VDMAKEFOURCC('B', 'O', 'D', 'Y'):
			{
				FrameInfo fi = { mFile.tell(), cksize, true };

				mFrameArray.push_back(fi);
			}
			break;

		case VDMAKEFOURCC('D', 'L', 'T', 'A'):
			{
				FrameInfo fi = { mFile.tell(), cksize, false };

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

	streamInfo.dwLength		= (uint32)mSampleLast;
	streamInfo.dwRate		= 15;
	streamInfo.dwScale		= 1;

	// If extra halfbrite mode is enabled, fix up the palette.
	if (mAmigaViewportMode & kAmigaVPHalfbrite) {
		for(int i=0; i<32; ++i) {
			mColorMap[i+32] = (mColorMap[i] >> 1) & 0x7f7f7f;
		}
	}
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

bool VDVideoSourceANIM::_isKey(VDPosition samp) {
	return mFrameArray[(uint32)samp].key;
}

VDPosition VDVideoSourceANIM::nearestKey(VDPosition lSample) {
	while(lSample > mSampleFirst && !_isKey(lSample))
		--lSample;

	return lSample;
}

VDPosition VDVideoSourceANIM::prevKey(VDPosition lSample) {
	while(lSample > mSampleFirst) {
		--lSample;
		if (_isKey(lSample))
			return lSample;
	}

	return -1;
}

VDPosition VDVideoSourceANIM::nextKey(VDPosition lSample) {
	while(++lSample < mSampleLast) {
		if (_isKey(lSample))
			return lSample;
	}

	return -1;
}

bool VDVideoSourceANIM::setTargetFormat(int format) {
	if (!format)
		format = nsVDPixmap::kPixFormat_XRGB8888;

	if (mAmigaViewportMode & kAmigaVPHoldAndModify) {
		switch(format) {
			case nsVDPixmap::kPixFormat_XRGB8888:
				return VideoSource::setTargetFormat(format);

			default:
				return false;
		}
	} else {
		switch(format) {
			case nsVDPixmap::kPixFormat_Pal8:
			case nsVDPixmap::kPixFormat_XRGB1555:
			case nsVDPixmap::kPixFormat_RGB565:
			case nsVDPixmap::kPixFormat_RGB888:
			case nsVDPixmap::kPixFormat_XRGB8888:
				return VideoSource::setTargetFormat(format);

			default:
				return false;
		}
	}
}

const void *VDVideoSourceANIM::getFrame(VDPosition lFrameDesired64) {
	long lFrameDesired = (long)lFrameDesired64;
	vdfastvector<char> dataBuffer;
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
					aviErr = read(lFrameNum, 1, dataBuffer.data(), dataBuffer.size(), &lBytesRead, &lSamplesRead);

				if (aviErr == AVIERR_BUFFERTOOSMALL) {
					aviErr = read(lFrameNum, 1, NULL, 0, &lBytesRead, &lSamplesRead);

					if (aviErr)
						throw MyAVIError("VideoSourceANIM", aviErr);

					dataBuffer.resize(lBytesRead ? (lBytesRead+65535) & ~65535 : 1);
				} else if (aviErr) {
					throw MyAVIError("VideoSourceANIM", aviErr);
				} else
					break;
			};

			if (!lBytesRead)
				continue;

			streamGetFrame(dataBuffer.data(), lBytesRead, lFrameNum == lFrameDesired, lFrameNum, lFrameNum);
		} while(++lFrameNum <= lFrameDesired);

	} catch(const MyError&) {
		mCachedFrame = -1;
		throw;
	}

	mCachedFrame = lFrameDesired; 

	return getFrameBuffer();
}

void VDVideoSourceANIM::streamBegin(bool fRealTime, bool bForceReset) {
	if (bForceReset)
		stream_current_frame	= -1;

	if (mbDecodeStarted && fRealTime == mbDecodeRealTime)
		return;

	stream_current_frame	= -1;

	mbDecodeStarted = true;
	mbDecodeRealTime = fRealTime;
}

void VDVideoSourceANIM::streamEnd() {
	if (!mbDecodeStarted)
		return;

	mbDecodeStarted = false;
}

namespace {
	class ANIMDecompressionError {};
}

const void *VDVideoSourceANIM::streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition frame_num, VDPosition target_sample) {
	const int w = mTargetFormat.w;
	const int h = mTargetFormat.h;

	if (inputBuffer) {
		const uint8 *src = (const uint8 *)inputBuffer;
		uint8 *dst = &mDeltaPlanes[(int)frame_num & 1][0];
		const int planepitch = ((w + 15) >> 4) * 2;
		const FrameInfo& frameInfo = mFrameArray[(uint32)frame_num];

		if (frameInfo.key) {
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
			if (!frame_num)
				memset(dst, 0, planepitch * h * mPlanes);

			try {
				switch(mCompressionMode) {
				case 5:
					DecompressMode5(src, data_len, dst, planepitch);
					break;

				case 7:
					if (mCompressionOptions & 1)
						DecompressMode7Long(src, data_len, dst, planepitch);
					else
						DecompressMode7Short(src, data_len, dst, planepitch);
					break;
				}
			} catch(const ANIMDecompressionError&) {
				throw MyError("Decompression error on frame %u.", (unsigned)frame_num);
			}
		}

		// convert planar to chunky
		memset(mPalBuffer.data(), 0, mPalBuffer.size());

		for(int plane = 0; plane < mPlanes; ++plane) {
			dst = mPalBuffer.data();

			for(int y = 0; y < h; ++y) {
				const uint8 *src = &mDeltaPlanes[frame_num & 1][planepitch * (y * mPlanes + plane)];
				const uint8 planemask = (uint8)(1<<plane);

				for(int x = 0; x < w; ++x) {
					if (src[x >> 3] & (0x80 >> (x & 7)))
						*dst = (uint8)(*dst + planemask);
					++dst;
				}
			}
		}
	}

	if (mAmigaViewportMode & kAmigaVPHoldAndModify) {
		VDASSERT(mTargetFormat.format == nsVDPixmap::kPixFormat_XRGB8888);

		const uint8 *src = mPalBuffer.data();
		char *dstrow = (char *)mTargetFormat.data;
		for(int y=0; y<h; ++y) {
			uint32 *dst = (uint32 *)dstrow;
			uint32 color = mColorMap[0];

			if (mPlanes == 8) {				// HAM8
				for(int x=0; x<w; ++x) {
					uint8 v = *src++;

					switch(v & 0xc0) {
						case 0x00:
							color = mColorMap[v & 63];
							break;
						case 0x40:
							color = (color & 0xffffff00) + ((((v & 0x3f)*0x00000041) >> 4) & 0xff);
							break;
						case 0x80:
							color = (color & 0xff00ffff) + (((v & 0x3f)*0x00041000) & 0x00ff0000);
							break;
						case 0xc0:
							color = (color & 0xffff00ff) + (((v & 0x3f)*0x00000410) & 0x0000ff00);
							break;
					}

					*dst++ = color;
				}
			} else {						// HAM6
				for(int x=0; x<w; ++x) {
					uint8 v = *src++;

					switch(v & 0x30) {
						case 0x00:
							color = mColorMap[v & 15];
							break;
						case 0x10:
							color = (color & 0xffffff00) + (v & 0x0f)*0x00000011;
							break;
						case 0x20:
							color = (color & 0xff00ffff) + (v & 0x0f)*0x00110000;
							break;
						case 0x30:
							color = (color & 0xffff00ff) + (v & 0x0f)*0x00001100;
							break;
					}

					*dst++ = color;
				}
			}

			dstrow += mTargetFormat.pitch;
		}
	} else {
		VDPixmap srcbm = {0};

		srcbm.data		= mPalBuffer.data();
		srcbm.pitch		= w;
		srcbm.w			= w;
		srcbm.h			= h;
		srcbm.format	= nsVDPixmap::kPixFormat_Pal8;
		srcbm.palette	= mColorMap;

		VDPixmapBlt(mTargetFormat, srcbm);
	}

	return getFrameBuffer();
}

void VDVideoSourceANIM::DecompressMode5(const uint8 *src, uint32 srclen, uint8 *dst, ptrdiff_t planepitch) {
	const uint32 *planeoffsets = (const uint32 *)src;
	ptrdiff_t pitch = planepitch * mPlanes;
	const int h = mTargetFormat.h;
	const uint8 *limit = (const uint8 *)src + srclen;

	for(int plane = 0; plane < mPlanes; ++plane) {
		uint32 srcOffset = VDFromBigEndian32(planeoffsets[plane]);

		if (srcOffset >= srclen)
			throw ANIMDecompressionError();

		const uint8 *src2 = (const uint8 *)src + srcOffset;

		for(int column = 0; column < planepitch; ++column) {
			uint8 *dstcol = dst++;
			int left = h;

			if (src2 >= limit)
				throw ANIMDecompressionError();

			int ops = *src2++;

			while(ops-- > 0) {
				if (src2 >= limit)
					throw ANIMDecompressionError();
				uint8 code = *src2++;

				if (!code) {
					if (limit-src2 < 2)
						throw ANIMDecompressionError();
					uint8 count = *src2++;
					uint8 v = *src2++;

					left -= count;
					if (left < 0)
						throw ANIMDecompressionError();

					while(count-- > 0) {
						*dstcol = v;
						dstcol += pitch;
					}
				} else if (code & 0x80) {
					code &= 0x7f;

					left -= code;
					if (left < 0)
						throw ANIMDecompressionError();

					if (limit-src2 < code)
						throw ANIMDecompressionError();
					while(code-- > 0) {
						*dstcol = *src2++;
						dstcol += pitch;
					}
				} else {
					dstcol += pitch * code;

					left -= code;
					if (left < 0)
						throw ANIMDecompressionError();
				}
			}
		}
	}
}

void VDVideoSourceANIM::DecompressMode7Short(const uint8 *src, uint32 srclen, uint8 *dst, ptrdiff_t planepitch) {
	const uint32 *opcodeOffsets = (const uint32 *)src;
	const uint32 *dataOffsets = (const uint32 *)src + 8;
	ptrdiff_t pitch = planepitch * mPlanes;
	const int h = mTargetFormat.h;
	const uint8 *limit = (const uint8 *)src + srclen;

	for(int plane = 0; plane < mPlanes; ++plane) {
		uint32 opcodeOffset = VDFromBigEndian32(opcodeOffsets[plane]);
		if (!opcodeOffset) {
			dst += planepitch;
			continue;
		}

		if (opcodeOffset >= srclen)
			throw ANIMDecompressionError();

		const uint8 *opcode = (const uint8 *)src + opcodeOffset;

		uint32 dataOffset = VDFromBigEndian32(dataOffsets[plane]);
		if (dataOffset >= srclen)
			throw ANIMDecompressionError();

		const uint8 *data = (const uint8 *)src + dataOffset;

		for(int column = 0; column < planepitch; column += 2) {
			uint16 *dstcol = (uint16 *)dst;
			dst += 2;
			int left = h;

			if (opcode >= limit)
				throw ANIMDecompressionError();

			int ops = *opcode++;

			while(ops-- > 0) {
				if (opcode >= limit)
					throw ANIMDecompressionError();
				uint8 code = *opcode++;

				if (!code) {
					if (opcode >= limit)
						throw ANIMDecompressionError();
					uint8 count = *opcode++;

					uint16 v = VDReadUnalignedU16(data);
					data += 2;

					left -= count;
					if (left < 0)
						throw ANIMDecompressionError();

					while(count-- > 0) {
						*dstcol = v;
						vdptrstep(dstcol, pitch);
					}
				} else if (code & 0x80) {
					code &= 0x7f;

					left -= code;
					if (left < 0)
						throw ANIMDecompressionError();

					if (limit - data < 2*code)
						throw ANIMDecompressionError();

					while(code-- > 0) {
						*dstcol = VDReadUnalignedU16(data);
						data += 2;
						vdptrstep(dstcol, pitch);
					}
				} else {
					vdptrstep(dstcol, pitch * code);

					left -= code;
					if (left < 0)
						throw ANIMDecompressionError();
				}
			}
		}
	}
}

void VDVideoSourceANIM::DecompressMode7Long(const uint8 *src, uint32 srclen, uint8 *dst, ptrdiff_t planepitch) {
	const uint32 *opcodeOffsets = (const uint32 *)src;
	const uint32 *dataOffsets = (const uint32 *)src + 8;
	ptrdiff_t pitch = planepitch * mPlanes;
	const int h = mTargetFormat.h;
	const uint8 *limit = (const uint8 *)src + srclen;

	for(int plane = 0; plane < mPlanes; ++plane) {
		uint32 opcodeOffset = VDFromBigEndian32(opcodeOffsets[plane]);
		if (!opcodeOffset) {
			dst += planepitch;
			continue;
		}

		if (opcodeOffset >= srclen)
			throw ANIMDecompressionError();

		const uint8 *opcode = (const uint8 *)src + opcodeOffset;

		uint32 dataOffset = VDFromBigEndian32(dataOffsets[plane]);
		if (dataOffset >= srclen)
			throw ANIMDecompressionError();

		const uint8 *data = (const uint8 *)src + dataOffset;

		for(int column = 0; column < planepitch; column += 4) {
			uint32 *dstcol = (uint32 *)dst;
			dst += 4;
			int left = h;

			if (opcode >= limit)
				throw ANIMDecompressionError();

			int ops = *opcode++;

			while(ops-- > 0) {
				if (opcode >= limit)
					throw ANIMDecompressionError();
				uint8 code = *opcode++;

				if (!code) {
					if (opcode >= limit)
						throw ANIMDecompressionError();
					uint8 count = *opcode++;
					uint32 v = VDReadUnalignedU32(data);
					data += 4;

					left -= count;
					if (left < 0)
						throw ANIMDecompressionError();

					while(count-- > 0) {
						*dstcol = v;
						vdptrstep(dstcol, pitch);
					}
				} else if (code & 0x80) {
					code &= 0x7f;

					left -= code;
					if (left < 0)
						throw ANIMDecompressionError();

					while(code-- > 0) {
						*dstcol = VDReadUnalignedU32(data);
						data += 4;
						vdptrstep(dstcol, pitch);
					}
				} else {
						vdptrstep(dstcol, pitch * code);

					left -= code;
					if (left < 0)
						throw ANIMDecompressionError();
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////

class VDInputFileANIM : public InputFile {
public:
	VDInputFileANIM();
	~VDInputFileANIM();

	void Init(const wchar_t *szFile);

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

		if (l>5 && !_wcsicmp(pszFilename + l - 5, L".anim"))
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
