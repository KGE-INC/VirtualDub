//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2004 Avery Lee
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

#include <vd2/system/thread.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/math.h>
#include <vd2/system/profile.h>
#include <vd2/Riza/capresync.h>
#include <vd2/Priss/convert.h>

///////////////////////////////////////////////////////////////////////////

extern "C" __declspec(align(16)) const sint16 gVDCaptureAudioResamplingKernel[32][8] = {
	{+0x0000,+0x0000,+0x0000,+0x4000,+0x0000,+0x0000,+0x0000,+0x0000 },
	{-0x000a,+0x0052,-0x0179,+0x3fe2,+0x019f,-0x005b,+0x000c,+0x0000 },
	{-0x0013,+0x009c,-0x02cc,+0x3f86,+0x0362,-0x00c0,+0x001a,+0x0000 },
	{-0x001a,+0x00dc,-0x03f9,+0x3eef,+0x054a,-0x012c,+0x002b,+0x0000 },
	{-0x001f,+0x0113,-0x0500,+0x3e1d,+0x0753,-0x01a0,+0x003d,+0x0000 },
	{-0x0023,+0x0141,-0x05e1,+0x3d12,+0x097c,-0x021a,+0x0050,-0x0001 },
	{-0x0026,+0x0166,-0x069e,+0x3bd0,+0x0bc4,-0x029a,+0x0066,-0x0001 },
	{-0x0027,+0x0182,-0x0738,+0x3a5a,+0x0e27,-0x031f,+0x007d,-0x0002 },
	{-0x0028,+0x0197,-0x07b0,+0x38b2,+0x10a2,-0x03a7,+0x0096,-0x0003 },
	{-0x0027,+0x01a5,-0x0807,+0x36dc,+0x1333,-0x0430,+0x00af,-0x0005 },
	{-0x0026,+0x01ab,-0x083f,+0x34db,+0x15d5,-0x04ba,+0x00ca,-0x0007 },
	{-0x0024,+0x01ac,-0x085b,+0x32b3,+0x1886,-0x0541,+0x00e5,-0x0008 },
	{-0x0022,+0x01a6,-0x085d,+0x3068,+0x1b40,-0x05c6,+0x0101,-0x000b },
	{-0x001f,+0x019c,-0x0846,+0x2dfe,+0x1e00,-0x0644,+0x011c,-0x000d },
	{-0x001c,+0x018e,-0x0819,+0x2b7a,+0x20c1,-0x06bb,+0x0136,-0x0010 },
	{-0x0019,+0x017c,-0x07d9,+0x28e1,+0x2380,-0x0727,+0x014f,-0x0013 },
	{-0x0016,+0x0167,-0x0788,+0x2637,+0x2637,-0x0788,+0x0167,-0x0016 },
	{-0x0013,+0x014f,-0x0727,+0x2380,+0x28e1,-0x07d9,+0x017c,-0x0019 },
	{-0x0010,+0x0136,-0x06bb,+0x20c1,+0x2b7a,-0x0819,+0x018e,-0x001c },
	{-0x000d,+0x011c,-0x0644,+0x1e00,+0x2dfe,-0x0846,+0x019c,-0x001f },
	{-0x000b,+0x0101,-0x05c6,+0x1b40,+0x3068,-0x085d,+0x01a6,-0x0022 },
	{-0x0008,+0x00e5,-0x0541,+0x1886,+0x32b3,-0x085b,+0x01ac,-0x0024 },
	{-0x0007,+0x00ca,-0x04ba,+0x15d5,+0x34db,-0x083f,+0x01ab,-0x0026 },
	{-0x0005,+0x00af,-0x0430,+0x1333,+0x36dc,-0x0807,+0x01a5,-0x0027 },
	{-0x0003,+0x0096,-0x03a7,+0x10a2,+0x38b2,-0x07b0,+0x0197,-0x0028 },
	{-0x0002,+0x007d,-0x031f,+0x0e27,+0x3a5a,-0x0738,+0x0182,-0x0027 },
	{-0x0001,+0x0066,-0x029a,+0x0bc4,+0x3bd0,-0x069e,+0x0166,-0x0026 },
	{-0x0001,+0x0050,-0x021a,+0x097c,+0x3d12,-0x05e1,+0x0141,-0x0023 },
	{+0x0000,+0x003d,-0x01a0,+0x0753,+0x3e1d,-0x0500,+0x0113,-0x001f },
	{+0x0000,+0x002b,-0x012c,+0x054a,+0x3eef,-0x03f9,+0x00dc,-0x001a },
	{+0x0000,+0x001a,-0x00c0,+0x0362,+0x3f86,-0x02cc,+0x009c,-0x0013 },
	{+0x0000,+0x000c,-0x005b,+0x019f,+0x3fe2,-0x0179,+0x0052,-0x000a },
};

#ifdef _M_IX86
	extern "C" void __cdecl vdasm_capture_resample16_MMX(sint16 *d, int stride, const sint16 *s, uint32 count, uint64 accum, sint64 inc);
#endif

namespace {
	uint64 resample16(sint16 *d, int stride, const sint16 *s, uint32 count, uint64 accum, sint64 inc) {
#ifdef _M_IX86
		if (MMX_enabled) {
			vdasm_capture_resample16_MMX(d, stride, s, count, accum, inc);
		}
#endif

		do {
			const sint16 *s2 = s + (accum >> 32);
			const sint16 *f = gVDCaptureAudioResamplingKernel[(uint32)accum >> 27];

			accum += inc;

			uint32 v= (sint32)s2[0]*(sint32)f[0]
					+ (sint32)s2[1]*(sint32)f[1]
					+ (sint32)s2[2]*(sint32)f[2]
					+ (sint32)s2[3]*(sint32)f[3]
					+ (sint32)s2[4]*(sint32)f[4]
					+ (sint32)s2[5]*(sint32)f[5]
					+ (sint32)s2[6]*(sint32)f[6]
					+ (sint32)s2[7]*(sint32)f[7]
					+ 0x20002000;

			v >>= 14;

			if (v >= 0x10000)
				v = ~v >> 31;

			*d = (sint16)(v - 0x8000);
			d += stride;
		} while(--count);

		return accum;
	}
}

///////////////////////////////////////////////////////////////////////////

void VDCaptureAudioRateEstimator::Reset() {
	mX = 0;
	mY = 0;
	mX2 = 0;
	mXY = 0;
	mSamples = 0;
}

void VDCaptureAudioRateEstimator::AddSample(sint64 x, sint64 y) {
	++mSamples;

	int128 x2, y2;
	x2.setSquare(x);

	mX	+= x;
	mX2	+= x2;
	mY	+= y;
	mXY	+= (int128)x * (int128)y;
}

bool VDCaptureAudioRateEstimator::GetSlope(double& slope) const {
	if (mSamples < 4)
		return false;

	const double x	= (double)mX;
	const double y	= (double)mY;
	const double x2	= mX2;
	const double xy	= mXY;
	const double n	= mSamples;

	slope = (n*xy - x*y) / (n*x2 - x*x);

	return true;
}

bool VDCaptureAudioRateEstimator::GetXIntercept(double slope, double& xintercept) const {
	xintercept = ((double)mX - (double)mY/slope) / mSamples;
	return true;
}

bool VDCaptureAudioRateEstimator::GetYIntercept(double slope, double& yintercept) const {
	yintercept = ((double)mY - (double)mX*slope) / mSamples;
	return true;
}

namespace {
	template<class T, unsigned N>
	class MovingAverage {
	public:
		MovingAverage()
			: mPos(0)
			, mSum(0)
		{
			for(unsigned i=0; i<N; ++i)
				mArray[i] = 0;
		}


		T operator()(T v) {
			mArray[mPos] = v;
			if (++mPos >= N)
				mPos = 0;

			return operator()();
		}

		T operator()() const {
			// Note: This is an O(n) sum rather than an O(1) sum table approach
			// because the latter is unsafe with floating-point values.
			T sum(0);
			for(unsigned i=0; i<N; ++i)
				sum += mArray[i];

			return sum * (T(1)/N);
		}

	protected:
		unsigned mPos;
		T mSum;
		T mArray[N];
	};
}

///////////////////////////////////////////////////////////////////////////

class VDCaptureResyncFilter : public IVDCaptureResyncFilter {
public:
	VDCaptureResyncFilter();
	~VDCaptureResyncFilter();

	void SetChildCallback(IVDCaptureDriverCallback *pChild);
	void SetVideoRate(double fps);
	void SetAudioRate(double bytesPerSec);
	void SetAudioChannels(int chans);
	void SetAudioFormat(VDAudioSampleType type);
	void SetResyncMode(Mode mode);
	void EnableVideoTimingCorrection(bool en);

	void GetStatus(VDCaptureResyncStatus&);

	void CapBegin(sint64 global_clock);
	void CapEnd(const MyError *pError);
	bool CapEvent(nsVDCapture::DriverEvent event);
	void CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock);

protected:
	void ResampleAndDispatchAudio(const void *data, uint32 size, bool key, sint64 global_clock);
	void UnpackSamples(sint16 *dst, ptrdiff_t dstStride, const void *src, uint32 samples, uint32 channels);

	IVDCaptureDriverCallback *mpCB;

	bool		mbAdjustVideoTime;
	Mode		mMode;

	sint64		mGlobalClockBase;
	sint64		mVideoLastTime;
	sint64		mVideoLastRawTime;
	sint64		mVideoTimingWrapAdjust;
	sint64		mVideoTimingAdjust;
	sint64		mAudioBytes;
	sint64		mAudioWrittenBytes;
	double		mVideoTimeLastAudioBlock;
	double		mInvAudioRate;
	double		mVideoRate;
	double		mVideoRateScale;
	double		mAudioRate;
	double		mAudioResamplingRate;
	double		mAudioTrackingRate;
	int			mChannels;
	VDCaptureAudioRateEstimator	mVideoRealRateEstimator;
	VDCaptureAudioRateEstimator	mAudioRelativeRateEstimator;
	VDCriticalSection	mcsLock;

	tpVDConvertPCM	mpAudioDecoder16;
	tpVDConvertPCM	mpAudioEncoder16;
	uint32		mBytesPerInputSample;

	sint32		mInputLevel;
	uint32		mAccum;
	vdblock<sint16>	mInputBuffer;
	vdblock<sint16>	mOutputBuffer;
	vdblock<char>	mEncodingBuffer;

	MovingAverage<double, 8>	mCurrentLatencyAverage;

	VDRTProfileChannel	mProfileChannel;
};

VDCaptureResyncFilter::VDCaptureResyncFilter()
	: mbAdjustVideoTime(true)
	, mMode(kModeNone)
	, mVideoLastTime(0)
	, mVideoLastRawTime(0)
	, mVideoTimingWrapAdjust(0)
	, mVideoTimingAdjust(0)
	, mAudioBytes(0)
	, mAudioRate(0)
	, mInvAudioRate(0)
	, mChannels(0)
	, mProfileChannel("Resynchronizer")
{
}

VDCaptureResyncFilter::~VDCaptureResyncFilter() {
}

IVDCaptureResyncFilter *VDCreateCaptureResyncFilter() {
	return new VDCaptureResyncFilter;
}

void VDCaptureResyncFilter::SetChildCallback(IVDCaptureDriverCallback *pChild) {
	mpCB = pChild;
}

void VDCaptureResyncFilter::SetVideoRate(double fps) {
	mVideoRate = fps;
}

void VDCaptureResyncFilter::SetAudioRate(double bytesPerSec) {
	mAudioRate = bytesPerSec;
	mInvAudioRate = 1.0 / bytesPerSec;
}

void VDCaptureResyncFilter::SetAudioChannels(int chans) {
	mChannels = chans;
}

void VDCaptureResyncFilter::SetAudioFormat(VDAudioSampleType type) {
	mpAudioDecoder16 = NULL;
	mpAudioEncoder16 = NULL;

	if (type != kVDAudioSampleType16S) {
		tpVDConvertPCMVtbl tbl = VDGetPCMConversionVtable();
		mpAudioDecoder16 = tbl[type][kVDAudioSampleType16S];
		mpAudioEncoder16 = tbl[kVDAudioSampleType16S][type];
	}

	mBytesPerInputSample = 1<<type;
}

void VDCaptureResyncFilter::SetResyncMode(Mode mode) {
	mMode = mode;
}

void VDCaptureResyncFilter::EnableVideoTimingCorrection(bool en) {
	mbAdjustVideoTime = en;
}

void VDCaptureResyncFilter::GetStatus(VDCaptureResyncStatus& status) {
	vdsynchronized(mcsLock) {
		double rate, latency;
		status.mVideoTimingAdjust	= mVideoTimingAdjust;
		status.mVideoRateScale		= mVideoRateScale;
		status.mAudioResamplingRate	= (float)mAudioResamplingRate;
		status.mCurrentLatency		= -mCurrentLatencyAverage();
		status.mMeasuredLatency		= 0.f;

		if (mAudioRelativeRateEstimator.GetSlope(rate) && mAudioRelativeRateEstimator.GetXIntercept(rate, latency)) {
			status.mMeasuredLatency = -latency;
		}
	}
}

void VDCaptureResyncFilter::CapBegin(sint64 global_clock) {
	mInputLevel = 0;
	mAccum = 0;
	mAudioResamplingRate = 1.0;
	mAudioTrackingRate = 1.0;
	mAudioWrittenBytes = 0;
	mVideoRateScale		= 1.0;
	mVideoTimeLastAudioBlock = 0;
	mVideoLastRawTime	= 0;

	mInputBuffer.resize(4096 * mChannels);
	memset(mInputBuffer.data(), 0, mInputBuffer.size() * sizeof(mInputBuffer[0]));
	mOutputBuffer.resize(4096 * mChannels);

	if (mpAudioEncoder16)
		mEncodingBuffer.resize(4096 * mChannels * mBytesPerInputSample);

	mpCB->CapBegin(global_clock);
}

void VDCaptureResyncFilter::CapEnd(const MyError *pError) {
	mpCB->CapEnd(pError);
}

bool VDCaptureResyncFilter::CapEvent(nsVDCapture::DriverEvent event) {
	return mpCB->CapEvent(event);
}

void VDCaptureResyncFilter::CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock)  {

	if (stream == 0) {
		// Correct for one form of the 71-minute bug.
		//
		// The video capture driver apparently computes a time in microseconds and then divides by
		// 1000 to convert to milliseconds, but doesn't compensate for when the microsecond counter
		// overflows past 2^32.  This results in a wraparound from 4294967ms (1h 11m 34s) to 0ms.
		// We must detect this and compensate for the wrap.
		//
		// Some Matrox drivers wrap at 2^31 too....

		if (timestamp < mVideoLastRawTime && timestamp < 10000000 && mVideoLastRawTime >= VD64(2138000000)) {

			// Perform sanity checks.  We should be within ten seconds of the last frame.
			sint64 bias;
			
			if (mVideoLastRawTime >= VD64(4285000000))
				bias = VD64(4294967296);	// 71 minute bug
			else
				bias = VD64(2147483648);	// 35 minute bug

			sint64 newtimestamp = timestamp + bias;

			if (newtimestamp < mVideoLastRawTime + 5000000 && newtimestamp >= mVideoLastRawTime - 5000000)
				mVideoTimingWrapAdjust += bias;
		}

		mVideoLastRawTime = timestamp;

		timestamp += mVideoTimingWrapAdjust;
	}

	vdsynchronized(mcsLock) {
		if (stream == 0) {
			int vcount = mVideoRealRateEstimator.GetCount();

			// update video-to-real-time regression
			mVideoRealRateEstimator.AddSample(global_clock, timestamp);

			// apply video timing correction
			if (mbAdjustVideoTime) {
				timestamp = VDRoundToInt64((double)timestamp * mVideoRateScale);

				double estimatedVideoToRealTimeSlope;
				if (vcount > 8 && mVideoRealRateEstimator.GetSlope(estimatedVideoToRealTimeSlope)) {
					double desiredVideoScale = 1.0 / estimatedVideoToRealTimeSlope;

					// gradually lerp correction up over first 100 samples
					if (vcount < 108)
						desiredVideoScale += (1.0 - desiredVideoScale) * ((108-vcount) * (1.0 / 100.0));

					// apply low-pass to damp correction (error decays to 10% in 229 samples)
					double correctionDelta = (desiredVideoScale - mVideoRateScale) * 0.01;

					// clamp change to 0.5ms per frame
					double maxDelta = 500.0 / timestamp;

					if (fabs(correctionDelta) > maxDelta)
						correctionDelta = (correctionDelta < 0) ? -maxDelta : maxDelta;

					// apply delta
					mVideoRateScale += correctionDelta;

					// clamp delta to within 20%
					if (mVideoRateScale < 0.8)
						mVideoRateScale = 0.8;
					else if (mVideoRateScale > 1.2)
						mVideoRateScale = 1.2;
				}
			}

			// apply video timing adjustment (AV sync by video time bump)
			mVideoLastTime = timestamp;
			timestamp += mVideoTimingAdjust;

		} else if (stream == 1 && mMode) {
			mAudioBytes += size;

			if (mbAdjustVideoTime)
				mAudioRelativeRateEstimator.AddSample(global_clock, mAudioBytes);
			else
				mAudioRelativeRateEstimator.AddSample(mVideoLastTime, mAudioBytes);

			double estimatedVideoTimeSlope, estimatedVideoTimeIntercept;
			double estimatedVideoTime;

			bool videoTimingOK = mVideoRealRateEstimator.GetSlope(estimatedVideoTimeSlope);

			if (videoTimingOK) {
				// Are we using the video clock or global clock as the time base?
				if (mbAdjustVideoTime) {
					// Project the video trend back to video time zero to get the global time start.
					videoTimingOK = mVideoRealRateEstimator.GetXIntercept(estimatedVideoTimeSlope, estimatedVideoTimeIntercept);

					if (videoTimingOK)
						estimatedVideoTime = global_clock;
				} else {
					// Project the video trend back to global time zero to get the video time start.
					videoTimingOK = mVideoRealRateEstimator.GetYIntercept(estimatedVideoTimeSlope, estimatedVideoTimeIntercept);

					// Estimate interpolated video clock from global clock.
					if (videoTimingOK)
						estimatedVideoTime = global_clock * estimatedVideoTimeSlope + estimatedVideoTimeIntercept;
				}
			}

			if (videoTimingOK) {
				double rate, latency;

				int count = mAudioRelativeRateEstimator.GetCount();

				if (count > 8 && mAudioRelativeRateEstimator.GetSlope(rate) && mAudioRelativeRateEstimator.GetXIntercept(rate, latency)) {
					if (mMode == kModeResampleVideo) {
						double adjustedRate = rate * ((double)(mVideoLastTime + mVideoTimingAdjust) / mVideoLastTime);
						double audioBytesPerSecond = adjustedRate * 1000000.0;
						double audioSecondsPerSecond = audioBytesPerSecond * mInvAudioRate;
						double errorSecondsPerSecond = audioSecondsPerSecond - 1.0;
						double errorSeconds = errorSecondsPerSecond * mVideoLastTime / 1000000.0;
						double errorFrames = errorSeconds * mVideoRate;

						if (fabs(errorFrames) >= 0.8) {
							sint32 errorAdj = -(sint32)(errorFrames / mVideoRate * 1000000.0);

							VDDEBUG("Applying delta of %d us -- total delta %d us\n", errorAdj, (sint32)mVideoTimingAdjust);
							mVideoTimingAdjust += errorAdj;
						}
					} else if (mMode == kModeResampleAudio) {
						double currentLatency = mCurrentLatencyAverage(mVideoTimeLastAudioBlock - mAudioWrittenBytes * mInvAudioRate * 1000000.0);
						double latencyError = currentLatency - latency;			// negative means too much data; positive means too little data
						double targetRate = mAudioTrackingRate - (1e-7)*latencyError;

						double resamplingRateFactor = 0.1;
						double trackingRateFactor = 0.002;

						// interpolate gradually over 100 samples
						if (count < 100) {
							resamplingRateFactor *= count/100.0;
							trackingRateFactor *= count/100.0;
						}

						mAudioResamplingRate += resamplingRateFactor * (targetRate - mAudioResamplingRate);
						mAudioTrackingRate += trackingRateFactor * (rate*1000000.0*mInvAudioRate - mAudioTrackingRate);

						if (mAudioResamplingRate < 0.1)
							mAudioResamplingRate = 0.1;
						else if (mAudioResamplingRate > 10.0)
							mAudioResamplingRate = 10.0;

#if 0
//						static int counter=0;
//						if (!(counter += 0x04000000))
						VDDEBUG("audio rate: %.2fHz (%.2fHz), latency: %.2fms (latency error: %+.2fms; current rate: %.6g; target: %.6g)\n"
								, rate * 1000000.0 / 4.0
								, mAudioRate * mAudioTrackingRate / 4.0
								, latency / 1000.0
								, latencyError / 1000.0
								, mAudioResamplingRate
								, targetRate);
#endif
					}
				}

				mVideoTimeLastAudioBlock = estimatedVideoTime;
			}
		}
	}

	if (stream == 1 && mMode == kModeResampleAudio)
		ResampleAndDispatchAudio(data, size, key, global_clock);
	else
		mpCB->CapProcessData(stream, data, size, timestamp, key, global_clock);
}

void VDCaptureResyncFilter::ResampleAndDispatchAudio(const void *data, uint32 size, bool key, sint64 global_clock) {
	int samples = size / (mChannels * mBytesPerInputSample);

	while(samples > 0) {
		int tc = 4096 - mInputLevel;

		VDASSERT(tc >= 0);
		if (tc > samples)
			tc = samples;

		int base = mInputLevel;

		samples -= tc;
		mInputLevel += tc;

		// resample
		uint32 inc = VDRoundToInt(mAudioResamplingRate * 65536.0);
		int limit = 0;

		const int chans = mChannels;

		mProfileChannel.Begin(0xc0e0ff, "A-Copy");
		UnpackSamples(mInputBuffer.data() + base, 4096*sizeof(mInputBuffer[0]), data, tc, chans);
		mProfileChannel.End();

		if (mInputLevel >= (mAccum>>16) + 8) {
			limit = ((mInputLevel << 16)-0x70000-mAccum + (inc-1)) / inc;
			if (limit > 4096)
				limit = 4096;

			uint32 accum0 = mAccum;

			mProfileChannel.Begin(0xffe0c0, "A-Filter");
			for(int chan=0; chan<chans; ++chan) {
				const sint16 *src = mInputBuffer.data() + 4096*chan;
				sint16 *dst = mOutputBuffer.data() + chan;

				mAccum = (uint32)(resample16(dst, chans, src, limit, (uint64)accum0<<16, (sint64)inc<<16) >> 16);

				int pos = mAccum >> 16;

				if (pos <= mInputLevel)
					memmove(&mInputBuffer[4096*chan], &mInputBuffer[4096*chan + pos], (mInputLevel - pos)*2);
			}
			mProfileChannel.End();
		}

		int shift = mAccum >> 16;
		if (shift > mInputLevel)
			shift = mInputLevel;
		mInputLevel -= shift;
		VDASSERT((unsigned)mInputLevel < 4096);
		mAccum -= shift << 16;

		const void *p = mOutputBuffer.data();
		uint32 size = limit * chans * 2;

		if (mpAudioEncoder16) {
			void *dst = mEncodingBuffer.data();

			mpAudioEncoder16(dst, p, limit * chans);

			size = limit * chans * mBytesPerInputSample;
			p = dst;
		}

		if (limit) {
			mAudioWrittenBytes += size;

			mpCB->CapProcessData(1, p, size, 0, key, global_clock);
		}

		data = (char *)data + mBytesPerInputSample*chans*tc;
	}
}

namespace {
	void strided_copy_16(sint16 *dst, ptrdiff_t dstStride, const sint16 *src, uint32 samples, uint32 channels) {
		for(uint32 ch=0; ch<channels; ++ch) {
			const sint16 *src2 = src++;
			dstStride -= samples*sizeof(sint16);

			for(uint32 s=0; s<samples; ++s) {
				*dst++ = *src2;
				src2 += channels;
			}

			dst = (sint16 *)((char *)dst + dstStride);
		}
	}
}

void VDCaptureResyncFilter::UnpackSamples(sint16 *dst, ptrdiff_t dstStride, const void *src, uint32 samples, uint32 channels) {
	if (!samples)
		return;

	if (!mpAudioDecoder16) {
		strided_copy_16(dst, dstStride, (const sint16 *)src, samples, channels);
		return;
	}

	sint16 buf[512];
	uint32 stripSize = 512 / channels;

	while(samples > 0) {
		uint32 tc = std::min<uint32>(stripSize, samples);

		mpAudioDecoder16(buf, src, tc * channels);

		strided_copy_16(dst, dstStride, buf, tc, channels);

		dst += tc;
		src = (const char *)src + tc*channels*mBytesPerInputSample;
		samples -= tc;
	}
}
