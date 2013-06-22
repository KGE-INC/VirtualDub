#ifndef f_VIDEOSOURCEIMAGES_H
#define f_VIDEOSOURCEIMAGES_H

#include <vd2/system/file.h>

#include "VideoSource.h"
#include "VBitmap.h"

class VideoSourceImages : public VideoSource {
private:
	long	mCachedFrame;
	int		mImageBaseNumber;
	VBitmap	mvbFrameBuffer;

	long	mCachedHandleFrame;
	VDFile	mCachedFile;

	wchar_t	mszPathFormat[512];

public:
	VideoSourceImages(const wchar_t *pszBaseFormat);
	~VideoSourceImages();

	int _read(VDPosition lStart, uint32 lCount, void *lpBuffer, uint32 cbBuffer, uint32 *lBytesRead, uint32 *lSamplesRead);
	bool _isKey(VDPosition samp)					{ return true; }
	VDPosition nearestKey(VDPosition lSample)			{ return lSample; }
	VDPosition prevKey(VDPosition lSample)				{ return lSample>0 ? lSample-1 : -1; }
	VDPosition nextKey(VDPosition lSample)				{ return lSample<mSampleLast ? lSample+1 : -1; }

	bool setTargetFormat(int depth);

	void invalidateFrameBuffer()			{ mCachedFrame = -1; }
	bool isFrameBufferValid()				{ return mCachedFrame >= 0; }
	bool isStreaming()						{ return false; }

	const void *streamGetFrame(const void *inputBuffer, uint32 data_len, bool is_preroll, VDPosition frame_num);

	const void *getFrame(VDPosition frameNum);

	char getFrameTypeChar(VDPosition lFrameNum)	{ return 'K'; }
	eDropType getDropType(VDPosition lFrameNum)	{ return kIndependent; }
	bool isKeyframeOnly()					{ return true; }
	bool isType1()							{ return false; }
	bool isDecodable(VDPosition sample_num)		{ return true; }
};

#endif
