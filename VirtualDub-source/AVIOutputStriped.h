//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2000 Avery Lee
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

#ifndef f_AVIOUTPUTSTRIPED_H
#define f_AVIOUTPUTSTRIPED_H

#include <stddef.h>

#include "AVIOutput.h"
#include "AVIStripeSystem.h"

class AVIOutputStripeState;

class AVIOutputStriped : public AVIOutput {
private:
	AVIStripeSystem			*stripesys;
	AVIOutputFile			**stripe_files;
	AVIOutputStripeState	*stripe_data;
	int						stripe_count;
	int						stripe_order;

	AVIOutputFile		*index_file;

	enum { CACHE_SIZE = 256 };

	AVIStripeIndexEntry		audio_index_cache[CACHE_SIZE];
	DWORD					audio_index_flags[CACHE_SIZE];
	LONG					audio_index_count[CACHE_SIZE];
	AVIStripeIndexEntry		video_index_cache[CACHE_SIZE];
	DWORD					video_index_flags[CACHE_SIZE];
	LONG					video_index_count[CACHE_SIZE];
	int audio_index_cache_point;
	int video_index_cache_point;
	bool f1GbMode;

	void FlushCache(BOOL fAudio);

public:
	AVIOutputStriped(AVIStripeSystem *);
	virtual ~AVIOutputStriped();

	void disable_os_caching();
	void set_1Gb_limit();

	BOOL initOutputStreams();
	BOOL init(const char *szFile, LONG xSize, LONG ySize, BOOL videoIn, BOOL audioIn, LONG bufferSize, BOOL is_interleaved);
	BOOL finalize();
	BOOL isPreview();

	void writeChunk(BOOL is_audio, LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer,
											LONG lSampleFirst, LONG lSampleCount);
	void writeIndexedChunk(FOURCC ckid, LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer);
};

#endif
