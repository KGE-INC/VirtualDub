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

#ifndef f_AVIOUTPUT_H
#define f_AVIOUTPUT_H

#include <windows.h>
#include <vfw.h>
#include <vector>
#include <vd2/system/file.h>

#include "Fixes.h"

class AVIIndex;
class AudioSource;
class VideoSource;
class AVIIndexEntry2;
typedef struct _avisuperindex_chunk AVISUPERINDEX;
struct _avisuperindex_entry;

class IVDMediaOutputStream {
public:
	virtual ~IVDMediaOutputStream() {}		// shouldn't be here but need to get rid of common delete in root destructor

	virtual void *	getFormat() = 0;
	virtual int		getFormatLen() = 0;
	virtual void	setFormat(const void *pFormat, int len) = 0;

	virtual const AVIStreamHeader_fixed& getStreamInfo() = 0;
	virtual void	setStreamInfo(const AVIStreamHeader_fixed& hdr) = 0;
	virtual void	updateStreamInfo(const AVIStreamHeader_fixed& hdr) = 0;

	virtual void	write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) = 0;
	virtual void	flush() = 0;
};

class AVIOutputStream : public IVDMediaOutputStream {
private:
	std::vector<char>	mFormat;

protected:
	AVIStreamHeader_fixed		streamInfo;

public:
	AVIOutputStream();
	virtual ~AVIOutputStream();

	virtual void setFormat(const void *pFormat, int len) {
		mFormat.resize(len);
		memcpy(&mFormat[0], pFormat, len);
	}

	virtual void *getFormat() { return &mFormat[0]; }
	virtual int getFormatLen() { return mFormat.size(); }

	virtual const AVIStreamHeader_fixed& getStreamInfo() {
		return streamInfo;
	}

	virtual void setStreamInfo(const AVIStreamHeader_fixed& hdr) {
		streamInfo = hdr;
		streamInfo.dwLength = 0;
		streamInfo.dwSuggestedBufferSize = 0;
	}

	virtual void updateStreamInfo(const AVIStreamHeader_fixed& hdr) {
		streamInfo = hdr;
	}

	virtual void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 samples) = 0;
	virtual void flush() {}
};

class VDINTERFACE IVDMediaOutput {
public:
	virtual ~IVDMediaOutput() {}

	virtual bool init(const wchar_t *szFile)=0;
	virtual void finalize()=0;

	virtual IVDMediaOutputStream *createAudioStream() = 0;
	virtual IVDMediaOutputStream *createVideoStream() = 0;
	virtual IVDMediaOutputStream *getAudioOutput() = 0;		// DEPRECATED
	virtual IVDMediaOutputStream *getVideoOutput() = 0;		// DEPRECATED
};

class AVIOutput : public IVDMediaOutput {
protected:
	IVDMediaOutputStream	*audioOut;
	IVDMediaOutputStream	*videoOut;

public:
	AVIOutput();
	virtual ~AVIOutput();

	virtual bool init(const wchar_t *szFile)=0;
	virtual void finalize()=0;

	IVDMediaOutputStream *getAudioOutput() { return audioOut; }
	IVDMediaOutputStream *getVideoOutput() { return videoOut; }
};

#endif
