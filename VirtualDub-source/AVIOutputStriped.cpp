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

#include <string.h>

#include "Error.h"

#include "AVIOutputStriped.h"
#include "AVIStripeSystem.h"

///////////////////////////////////////////////////////////////////////////
//
//	output streams
//
///////////////////////////////////////////////////////////////////////////

class AVIStripedAudioOutputStream : public AVIAudioOutputStream {
public:
	AVIStripedAudioOutputStream(AVIOutput *);

	BOOL write(LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer, LONG lSamples);
};

AVIStripedAudioOutputStream::AVIStripedAudioOutputStream(AVIOutput *out) : AVIAudioOutputStream(out) {
}

BOOL AVIStripedAudioOutputStream::write(LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer, LONG lSamples) {
	((AVIOutputStriped *)output)->writeChunk(TRUE, dwIndexFlags, lpBuffer, cbBuffer, lTotalSamplesWritten, lSamples);

	lTotalSamplesWritten += lSamples;

	return TRUE;
}

////////////////////////////////////

class AVIStripedVideoOutputStream : public AVIVideoOutputStream {
public:
	AVIStripedVideoOutputStream(AVIOutput *);

	BOOL write(LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer, LONG lSamples);
};

AVIStripedVideoOutputStream::AVIStripedVideoOutputStream(AVIOutput *out) : AVIVideoOutputStream(out) {
}

BOOL AVIStripedVideoOutputStream::write(LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer, LONG lSamples) {
	((AVIOutputStriped *)output)->writeChunk(FALSE, dwIndexFlags, lpBuffer, cbBuffer, lTotalSamplesWritten, lSamples);

	lTotalSamplesWritten += lSamples;

	return TRUE;
}



///////////////////////////////////////////////////////////////////////////
//
//	AVIOutputStriped
//
///////////////////////////////////////////////////////////////////////////

class AVIOutputStripeState {
public:
	__int64		size;
	long		audio_sample, video_sample;
};



AVIOutputStriped::AVIOutputStriped(AVIStripeSystem *stripesys) {
	this->stripesys		= stripesys;

	stripe_files		= NULL;
	stripe_data			= NULL;

	audio_index_cache_point		= 0;
	video_index_cache_point		= 0;

	f1GbMode = false;
}

AVIOutputStriped::~AVIOutputStriped() {
	int i;

	if (stripe_files) {
		for(i=0; i<stripe_count; i++)
			delete stripe_files[i];

		delete[] stripe_files;
	}
	delete[] stripe_data;
}

//////////////////////////////////

BOOL AVIOutputStriped::initOutputStreams() {
	if (!(audioOut = new AVIStripedAudioOutputStream(this))) return FALSE;
	if (!(videoOut = new AVIStripedVideoOutputStream(this))) return FALSE;

	return TRUE;
}

void AVIOutputStriped::set_1Gb_limit() {
	f1GbMode = true;
}

BOOL AVIOutputStriped::init(const char *szFile, LONG xSize, LONG ySize, BOOL hasVideo, BOOL hasAudio, LONG bufferSize, BOOL is_interleaved) {
	int i;

	if (hasAudio) {
		if (!audioOut) return FALSE;
	} else {
		delete audioOut;
		audioOut = NULL;
	}

	if (hasVideo) {
		if (!videoOut) return FALSE;
	} else {
		delete videoOut;
		videoOut = NULL;
	}

	stripe_count = stripesys->getStripeCount();

	if (!(stripe_data = new AVIOutputStripeState [stripe_count]))
		throw MyMemoryError();

	memset(stripe_data, 0, sizeof(AVIOutputStripeState)*stripe_count);

	if (!(stripe_files = new AVIOutputFile *[stripe_count]))
		throw MyMemoryError();

	bool fFoundIndex = false;

	for(i=0; i<stripe_count; i++) {
		stripe_files[i] = NULL;
		if (stripesys->getStripeInfo(i)->isIndex())
			fFoundIndex = true;
	}

	if (!fFoundIndex)
		throw MyError("Cannot create output: stripe system has no index stripe");

	for(i=0; i<stripe_count; i++) {
		AVIStripe *sinfo = stripesys->getStripeInfo(i);

		stripe_files[i] = new AVIOutputFile();

		if (f1GbMode)
			stripe_files[i]->set_1Gb_limit();

		if (!stripe_files[i]->initOutputStreams())
			throw MyMemoryError();

		if (hasVideo && (sinfo->isVideo() || sinfo->isIndex())) {

			if (sinfo->isIndex()) {
				BITMAPINFOHEADER *bmih;

				if (!(bmih = (BITMAPINFOHEADER *)stripe_files[i]->videoOut->allocFormat(sizeof(BITMAPINFOHEADER))))
					throw MyMemoryError();

				memcpy(bmih, videoOut->getFormat(), sizeof(BITMAPINFOHEADER));

				bmih->biSize			= sizeof(BITMAPINFOHEADER);
				bmih->biCompression		= 'TSDV';
			} else {
				if (!stripe_files[i]->videoOut->allocFormat(videoOut->getFormatLen()))
					throw MyMemoryError();

				memcpy(stripe_files[i]->videoOut->getFormat(), videoOut->getFormat(), videoOut->getFormatLen());
			}

			memcpy(&stripe_files[i]->videoOut->streamInfo, &videoOut->streamInfo, sizeof(AVIStreamHeader_fixed));

			if (sinfo->isIndex()) {
				stripe_files[i]->videoOut->streamInfo.fccHandler = 'TSDV';
				stripe_files[i]->videoOut->streamInfo.dwSampleSize	= 16;

				index_file = stripe_files[i];
			}
		}
		if (hasAudio && (sinfo->isAudio() || sinfo->isIndex())) {
			if (!stripe_files[i]->audioOut->allocFormat(audioOut->getFormatLen()))
				throw MyMemoryError();

			memcpy(stripe_files[i]->audioOut->getFormat(), audioOut->getFormat(), audioOut->getFormatLen());

			memcpy(&stripe_files[i]->audioOut->streamInfo, &audioOut->streamInfo, sizeof(AVIStreamHeader_fixed));

			if (!sinfo->isAudio()) {
				stripe_files[i]->audioOut->streamInfo.fccType		= 'idua';
				stripe_files[i]->audioOut->streamInfo.fccHandler	= 'TSDV';
				stripe_files[i]->audioOut->streamInfo.dwSampleSize	= 16;

				index_file = stripe_files[i];
			}
		}

		stripe_files[i]->disable_os_caching();

		if (!stripe_files[i]->init(
					sinfo->szName,
					xSize,
					ySize,
					TRUE,
					sinfo->isAudio() || sinfo->isIndex(),
					sinfo->lBufferSize,
					TRUE))
			throw MyError("Error initializing stripe #%d", i+1);

		stripe_files[i]->videoOut->setCompressed(videoOut->isCompressed());
	}

	return TRUE;
}

BOOL AVIOutputStriped::finalize() {
	int i;

	FlushCache(FALSE);
	FlushCache(TRUE);

	for(i=0; i<stripe_count; i++)
		if (!stripe_files[i]->finalize())
			return FALSE;

	return TRUE;
}

BOOL AVIOutputStriped::isPreview() { return FALSE; }

void AVIOutputStriped::writeChunk(BOOL is_audio, LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer,
										LONG lSampleFirst, LONG lSampleCount) {
	AVIStripeIndexEntry *asie;
	int s, best_stripe=-1;
	long lBufferSize;
	BOOL best_will_block = TRUE;

	// Pick a stripe to send the data to.
	//
	// Prioritize stripes that can hold the data without blocking,
	// then on total data fed to that stripe at that point.

	for(s=0; s<stripe_count; s++) {

		if (is_audio) {
			if (!stripesys->getStripeInfo(s)->isAudio())
				continue;
		} else {
			if (!stripesys->getStripeInfo(s)->isVideo())
				continue;
		}

		if (stripe_files[s]->bufferStatus(&lBufferSize) >= ((cbBuffer+11)&-4)) {
			// can accept without blocking

			if (best_will_block) { // automatic win
				best_will_block = FALSE;
				best_stripe = s;
				continue;
			}
		} else {
			// will block

			if (!best_will_block)	// automatic loss
				continue;
		}

		// compare total data sizes

		if (best_stripe<0 || stripe_data[best_stripe].size > stripe_data[s].size)
			best_stripe = s;
	}

	// Write data to stripe.

	if (best_stripe >= 0) {
		if (is_audio)
			stripe_files[best_stripe]->audioOut->write(dwIndexFlags, lpBuffer, cbBuffer, lSampleCount);
		else
			stripe_files[best_stripe]->videoOut->write(dwIndexFlags, lpBuffer, cbBuffer, lSampleCount);

		stripe_data[best_stripe].size += cbBuffer+8;
	}

	// Write lookup chunk to index stream.
	//
	// NOTE: Do not write index marks to the same stream the data
	//       was written to!

	if (index_file != stripe_files[best_stripe]) {
		if (is_audio) {
			if (audio_index_cache_point >= CACHE_SIZE)
				FlushCache(TRUE);

			asie = &audio_index_cache[audio_index_cache_point++];
		} else {
			if (video_index_cache_point >= CACHE_SIZE)
				FlushCache(FALSE);

			asie = &video_index_cache[video_index_cache_point++];
		}

		asie->lSampleFirst	= lSampleFirst;
		asie->lSampleCount	= lSampleCount;
		asie->lStripe		= best_stripe;
		asie->lStripeSample	= 0;

		if (is_audio) {
			if (best_stripe >= 0) {
				asie->lStripeSample = stripe_data[best_stripe].audio_sample;

				stripe_data[best_stripe].audio_sample += lSampleCount;
			}
			audio_index_flags[audio_index_cache_point-1] = dwIndexFlags;
			audio_index_count[audio_index_cache_point-1] = lSampleCount;

//			index_file->audioOut->write(dwIndexFlags, dwIndexData, sizeof dwIndexData, lSampleCount);
		} else {
			if (best_stripe >= 0) {
				asie->lStripeSample = stripe_data[best_stripe].video_sample;

				stripe_data[best_stripe].video_sample += lSampleCount;
			}
			video_index_flags[video_index_cache_point-1] = dwIndexFlags;
			video_index_count[video_index_cache_point-1] = lSampleCount;

//			index_file->videoOut->write(dwIndexFlags, dwIndexData, sizeof dwIndexData, lSampleCount);
		}
	}
}

void AVIOutputStriped::writeIndexedChunk(FOURCC ckid, LONG dwIndexFlags, LPVOID lpBuffer, LONG cbBuffer) {
	index_file->writeIndexedChunk(ckid, dwIndexFlags, lpBuffer, cbBuffer);
}

void AVIOutputStriped::FlushCache(BOOL fAudio) {
	int i;

	if (fAudio) {
		for(i=0; i<audio_index_cache_point; i++)
			index_file->audioOut->write(audio_index_flags[i], &audio_index_cache[i], sizeof AVIStripeIndexEntry, audio_index_count[i]);

		audio_index_cache_point = 0;
	} else {
		for(i=0; i<video_index_cache_point; i++)
			index_file->videoOut->write(video_index_flags[i], &video_index_cache[i], sizeof AVIStripeIndexEntry, video_index_count[i]);

		video_index_cache_point = 0;
	}
}
