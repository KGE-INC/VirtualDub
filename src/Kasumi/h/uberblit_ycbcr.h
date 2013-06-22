#ifndef f_VD2_KASUMI_UBERBLIT_YCBCR_H
#define f_VD2_KASUMI_UBERBLIT_YCBCR_H

#include <vd2/system/cpuaccel.h>
#include "uberblit.h"

class VDPixmapGenYCbCrToRGBBase : public VDPixmapGenWindowBased {
public:
	void Init(IVDPixmapGen *srcCr, uint32 srcindexCr, IVDPixmapGen *srcY, uint32 srcindexY, IVDPixmapGen *srcCb, uint32 srcindexCb) {
		mpSrcY = srcY;
		mSrcIndexY = srcindexY;
		mpSrcCb = srcCb;
		mSrcIndexCb = srcindexCb;
		mpSrcCr = srcCr;
		mSrcIndexCr = srcindexCr;
		mWidth = srcY->GetWidth();
		mHeight = srcY->GetHeight();

		srcY->AddWindowRequest(0, 0);
		srcCb->AddWindowRequest(0, 0);
		srcCr->AddWindowRequest(0, 0);
	}


protected:
	IVDPixmapGen *mpSrcY;
	uint32 mSrcIndexY;
	IVDPixmapGen *mpSrcCb;
	uint32 mSrcIndexCb;
	IVDPixmapGen *mpSrcCr;
	uint32 mSrcIndexCr;
};

class VDPixmapGenYCbCrToRGB32 : public VDPixmapGenYCbCrToRGBBase {
public:
	void Start() {
		mpSrcY->Start();
		mpSrcCb->Start();
		mpSrcCr->Start();

		StartWindow(mWidth * 4);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrcY->GetType(mSrcIndexY) & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixType_8888 | kVDPixSpace_BGR;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		VDCPUCleanupExtensions();

		uint8 *dst = (uint8 *)dst0;
		const uint8 *srcY = (const uint8 *)mpSrcY->GetRow(y, mSrcIndexY);
		const uint8 *srcCb = (const uint8 *)mpSrcCb->GetRow(y, mSrcIndexCb);
		const uint8 *srcCr = (const uint8 *)mpSrcCr->GetRow(y, mSrcIndexCr);

		for(sint32 i=0; i<mWidth; ++i) {
			sint32 y = srcY[i];
			sint32 cb = srcCb[i];
			sint32 cr = srcCr[i];

			float yf = (1.164f / 255.0f)*(y - 16);

			dst[0] = VDClampedRoundFixedToUint8Fast(yf + (2.018f / 255.0f) * (cb - 128));
			dst[1] = VDClampedRoundFixedToUint8Fast(yf - (0.813f / 255.0f) * (cr - 128) - (0.391f / 255.0f) * (cb - 128));
			dst[2] = VDClampedRoundFixedToUint8Fast(yf + (1.596f / 255.0f) * (cr - 128));
			dst[3] = 0xff;
			dst += 4;
		}
	}
};

class VDPixmapGenYCbCrToRGB32F : public VDPixmapGenYCbCrToRGBBase {
public:
	void Start() {
		mpSrcY->Start();
		mpSrcCb->Start();
		mpSrcCr->Start();

		StartWindow(mWidth * 16);
	}

	uint32 GetType(uint32 output) const {
		return (mpSrcY->GetType(mSrcIndexY) & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixType_32Fx4_LE | kVDPixSpace_BGR;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		VDCPUCleanupExtensions();

		float *dst = (float *)dst0;
		const float *srcY = (const float *)mpSrcY->GetRow(y, mSrcIndexY);
		const float *srcCb = (const float *)mpSrcCb->GetRow(y, mSrcIndexCb);
		const float *srcCr = (const float *)mpSrcCr->GetRow(y, mSrcIndexCr);

		for(sint32 i=0; i<mWidth; ++i) {
			float y = srcY[i];
			float cb = srcCb[i];
			float cr = srcCr[i];

			float yf = 1.164f * (y - 16.0f / 255.0f);

			dst[0] = yf + 1.596f * (cr - 0.5f);
			dst[1] = yf - 0.813f * (cr - 0.5f) - 0.391f * (cb - 0.5f);
			dst[2] = yf + 2.018f * (cb - 0.5f);
			dst[3] = 1.0f;
			dst += 4;
		}
	}
};

class VDPixmapGenRGB32ToYCbCr : public VDPixmapGenWindowBasedOneSource {
public:
	void Init(IVDPixmapGen *src, uint32 srcindex) {
		InitSource(src, srcindex);
	}

	void Start() {
		StartWindow(mWidth, 3);
	}

	const void *GetRow(sint32 y, uint32 index) {
		return (const uint8 *)VDPixmapGenWindowBasedOneSource::GetRow(y, index) + mWindowPitch * index;
	}

	uint32 GetType(uint32 output) const {
		return (mpSrc->GetType(mSrcIndex) & ~(kVDPixType_Mask | kVDPixSpace_Mask)) | kVDPixType_8 | kVDPixSpace_YCC_601;
	}

protected:
	void Compute(void *dst0, sint32 y) {
		uint8 *dstCb = (uint8 *)dst0;
		uint8 *dstY = dstCb + mWindowPitch;
		uint8 *dstCr = dstY + mWindowPitch;

		const uint8 *srcRGB = (const uint8 *)mpSrc->GetRow(y, mSrcIndex);

		for(sint32 i=0; i<mWidth; ++i) {
			int r = (int)srcRGB[2];
			int g = (int)srcRGB[1];
			int b = (int)srcRGB[0];
			srcRGB += 4;			


			// -2->round(inv([1 0 0 0; 0 1 0 0; 0 0 1 0; -16 -128 -128 1] * [1.1643828 1.1643828 1.1643828 0; 1.5960273 -0.8129688 0 0;
			//  0 -0.3917617 2.0172305 0; 0 0 0 1]) .* 65536)
			// ans  =
			// 
			// !   16829.      28784.    - 9714.       0.     !
			// !   33039.    - 24103.    - 19071.      0.     !
			// !   6416.     - 4681.       28784.      0.     !
			// !   1048576.    8388608.    8388608.    65536. !   

			*dstCb++ = (28784*r - 24103*g -  4681*b + 8388608 + 32768) >> 16;
			*dstY ++ = (16829*r + 33039*g +  6416*b + 1048576 + 32768) >> 16;
			*dstCr++ = (-9714*r - 19071*g + 28784*b + 8388608 + 32768) >> 16;
		}
	}
};

#endif
