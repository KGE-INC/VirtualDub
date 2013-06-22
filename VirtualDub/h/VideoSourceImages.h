#ifndef f_VIDEOSOURCEIMAGES_H
#define f_VIDEOSOURCEIMAGES_H

#include <windows.h>

#include "VideoSource.h"
#include "VBitmap.h"

class VideoSourceImages : public VideoSource {
private:
	long	mCachedFrame;
	int		mImageBaseNumber;
	VBitmap	mvbFrameBuffer;

	long	mCachedHandleFrame;
	HANDLE	mCachedHandle;

	char	mszPathFormat[512];

	void _construct(const char *pszBaseFormat);
	void _destruct();

public:
	VideoSourceImages(const char *pszBaseFormat);
	~VideoSourceImages();

	int _read(LONG lStart, LONG lCount, LPVOID lpBuffer, LONG cbBuffer, LONG *lBytesRead, LONG *lSamplesRead);
	BOOL _isKey(LONG samp)					{ return true; }
	LONG nearestKey(LONG lSample)			{ return lSample; }
	LONG prevKey(LONG lSample)				{ return lSample>0 ? lSample-1 : -1; }
	LONG nextKey(LONG lSample)				{ return lSample<lSampleLast ? lSample+1 : -1; }

	bool setDecompressedFormat(int depth);
	bool setDecompressedFormat(BITMAPINFOHEADER *pbih);

	void invalidateFrameBuffer()			{ mCachedFrame = -1; }
	BOOL isFrameBufferValid()				{ return mCachedFrame >= 0; }
	bool isStreaming()						{ return false; }

	void *streamGetFrame(void *inputBuffer, long data_len, BOOL is_key, BOOL is_preroll, long frame_num);

	void *getFrame(LONG frameNum);

	char getFrameTypeChar(long lFrameNum)	{ return 'K'; }
	eDropType getDropType(long lFrameNum)	{ return kIndependent; }
	bool isKeyframeOnly()					{ return true; }
	bool isType1()							{ return false; }
	bool isDecodable(long sample_num)		{ return true; }
};

#endif
