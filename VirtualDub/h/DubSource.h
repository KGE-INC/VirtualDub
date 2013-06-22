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

#ifndef f_DUBSOURCE_H
#define f_DUBSOURCE_H

#include <windows.h>
#include <vfw.h>

#include <vd2/system/fraction.h>
#include <vd2/system/refcount.h>

class InputFile;

class IVDStreamSource : public IVDRefCount {
public:
	virtual bool init() = 0;

	virtual VDPosition	getLength() = 0;
	virtual VDPosition	getStart() = 0;
	virtual VDPosition	getEnd() = 0;
	virtual const VDFraction getRate() = 0;

	virtual const AVISTREAMINFO& getStreamInfo() = 0;		// DELETE ME I'M STUPID

	virtual int read(LONG lStart, LONG lCount, LPVOID lpBuffer, LONG cbBuffer, LONG *lBytesRead, LONG *lSamplesRead) = 0;

	virtual void *getFormat() const = 0;
	virtual int getFormatLen() const = 0;

	virtual bool isStreaming() = 0;

	virtual void streamBegin( bool fRealTime) = 0;
	virtual void streamEnd() = 0;

	enum ErrorMode {
		kErrorModeReportAll = 0,
		kErrorModeConceal,
		kErrorModeDecodeAnyway,
		kErrorModeCount
	};

	virtual void setDecodeErrorMode(ErrorMode mode) = 0;
	virtual bool isDecodeErrorModeSupported(ErrorMode mode) = 0;

	virtual LONG msToSamples(LONG lMs) const = 0;
	virtual LONG samplesToMs(LONG lSamples) const = 0;

//	virtual LONG samplesToSamples(const DubSource *source, LONG lSamples) const = 0;
};

class DubSource : public vdrefcounted<IVDStreamSource> {
private:
	void *	format;
	int		format_len;

protected:
	void *allocFormat(int format_len);

	LONG lSampleFirst, lSampleLast;
	AVISTREAMINFO	streamInfo;

	DubSource();
	virtual ~DubSource();

public:
	virtual bool init();

	virtual VDPosition getLength() {
		return lSampleLast - lSampleFirst;
	}

	virtual VDPosition getStart() {
		return lSampleFirst;
	}

	virtual VDPosition getEnd() {
		return lSampleLast;
	}

	virtual const VDFraction getRate() {
		return VDFraction(streamInfo.dwRate, streamInfo.dwScale);
	}

	virtual const AVISTREAMINFO& getStreamInfo() {
		return streamInfo;
	}

	virtual int read(LONG lStart, LONG lCount, LPVOID lpBuffer, LONG cbBuffer, LONG *lBytesRead, LONG *lSamplesRead);
	virtual int _read(LONG lStart, LONG lCount, LPVOID lpBuffer, LONG cbBuffer, LONG *lBytesRead, LONG *lSamplesRead) = 0;

	void *getFormat() const { return format; }
	int getFormatLen() const { return format_len; }

	virtual bool isStreaming();


	virtual void streamBegin( bool fRealTime);
	virtual void streamEnd();

	virtual void setDecodeErrorMode(ErrorMode mode) {}
	virtual bool isDecodeErrorModeSupported(ErrorMode mode) { return false; }

	LONG msToSamples(LONG lMs) const {
		return (LONG)(((__int64)lMs * streamInfo.dwRate + (__int64)500 * streamInfo.dwScale) / ((__int64)1000 * streamInfo.dwScale));
	}
	LONG samplesToMs(LONG lSamples) const {
		return (LONG)(
				(((__int64)lSamples * streamInfo.dwScale) * 1000 + streamInfo.dwRate/2) / streamInfo.dwRate
			);
	}

	// This is more accurate than AVIStreamSampleToSample(), which does a conversion to
	// milliseconds and back.

	static LONG samplesToSamples(const AVISTREAMINFO *dest, const AVISTREAMINFO *source, LONG lSamples) {
		__int64 divisor = (__int64)source->dwRate * dest->dwScale;

		return (LONG)((((__int64)lSamples * source->dwScale) * dest->dwRate + divisor/2)
				/ divisor);
	}


	LONG samplesToSamples(const DubSource *source, LONG lSamples) const {
		return samplesToSamples(&streamInfo, &source->streamInfo, lSamples);
	}
};

#endif