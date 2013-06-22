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

#ifndef f_AVIOUTPUTPREVIEW_H
#define f_AVIOUTPUTPREVIEW_H

#include "AVIOutput.h"
#include "AVIAudioOutput.h"

class AVIAudioPreviewOutputStream : public AVIOutputStream {
private:
	AVIAudioOutput *myAudioOut;
	bool initialized, started;
	bool fInitialized;

	bool initAudio();
public:
	AVIAudioPreviewOutputStream();
	~AVIAudioPreviewOutputStream();

	bool init();
	void write(uint32 flags, const void *pBuffer, uint32 cbBuffer, uint32 lSamples);
	void start();
	void finalize();
	void flush();
	long getPosition();
	long getAvailable();
	bool isFrozen();
	bool isSilent();
	void stop();
};

class AVIOutputPreview : public AVIOutput {
private:
public:
	AVIOutputPreview();
	~AVIOutputPreview();

	IVDMediaOutputStream *createVideoStream();
	IVDMediaOutputStream *createAudioStream();

	bool init(const wchar_t *szFile);
	void finalize();
};

#endif
