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

#ifndef f_DUB_H
#define f_DUB_H

#include <windows.h>
#include <mmsystem.h>
#include <vfw.h>

#include "Error.h"
#include "audio.h"
#include "filters.h"

class Histogram;
class AsyncBlitter;
class AVIPipe;
class AVIOutput;
class DubSource;
class AudioSource;
class VideoSource;
class FrameSubset;
class InputFile;
class IDubStatusHandler;

////////////////////////

class DubAudioOptions {
public:
	enum {
		P_NOCHANGE=0,
		P_8BIT=1,
		P_16BIT=2,
		C_NOCHANGE=0,
		C_MONO=1,
		C_STEREO=2,
		C_MONOLEFT=3,
		C_MONORIGHT=4,
		M_NONE			= 0,
		M_FULL			= 1,
	};

	long volume;		// 0, or 8-bit fixed point fraction

	long preload;
	long interval;
	long new_rate;
	long offset;
	bool is_ms;
	bool enabled;
	bool fStartAudio, fEndAudio;
	bool integral_rate;
	bool fHighQuality;
	char newPrecision;
	char newChannels;
	char mode;
};

class DubVideoOptions {
public:
	enum {
		D_16BIT = 0,
		D_24BIT = 1,
		D_32BIT	= 2,
	};
	enum {
		M_NONE		= 0,
		M_FASTREPACK= 1,
		M_SLOWREPACK= 2,
		M_FULL		= 3,
	};
	enum {
		FR_SAMELENGTH = -1
	};

	char	inputDepth, outputDepth;
	char	mode;
	BOOL	fShowInputFrame, fShowOutputFrame, fShowDecompressedFrame;
	BOOL	fHistogram, fSyncToAudio;
	int		frameRateDecimation;
	long	frameRateNewMicroSecs;

	long	lStartOffsetMS;
	long	lEndOffsetMS;

	bool	fInvTelecine;
	bool	fIVTCMode;
	int		nIVTCOffset;
	bool	fIVTCPolarity;

	int		nPreviewFieldMode;
};

class DubPerfOptions {
public:
	long	outputBufferSize;
	long	waveBufferSize;
	long	pipeBufferCount;

	bool	dynamicEnable;
	bool	dynamicShowDisassembly;
	bool	useDirectDraw;
	bool	fDropFrames;
};

class DubOptions {
public:
	DubAudioOptions audio;
	DubVideoOptions video;
	DubPerfOptions perf;

	bool	fShowStatus, fMoveSlider;
};

class DubStreamInfo {
public:
	long	start_src;
	long	cur_src;
	long	cur_proc_src;
	long	end_src;
	long	start_dst;
	long	cur_dst;
	long	end_dst;
	__int64	total_size;
};

class DubAudioStreamInfo : public DubStreamInfo {
public:
	AudioFormatConverter formatConverter;
	long	samp_frac;
	bool	converting, resampling;
	bool	is_16bit;
	bool	is_stereo;
	bool	is_right;
	bool	single_channel;
	char	bytesPerSample;

	long	lPreloadSamples;
};

class DubVideoStreamInfo : public DubStreamInfo {
public:
	long	usPerFrame;
	long	usPerFrameIn;
	long	usPerFrameNoTelecine;
	long	processed;
	int		nLag;
	bool	fAudioOnly;
};

class IDubber {
public:
	virtual ~IDubber()					=0;

	virtual void SetAudioCompression(WAVEFORMATEX *wf, LONG cb)	=0;
	virtual void SetInputFile(InputFile *)=0;
	virtual void SetPhantomVideoMode()=0;
	virtual void SetFrameRectangles(RECT *prInput, RECT *prOutput)=0;
	virtual void EnableSpill(const char *pszPrefix, __int64 size, long lFrameLimit)=0;
//	virtual void InitStreamValues(VideoSource *video, AudioSource *audio, AVIOutput *out)	=0;
	virtual void Init(VideoSource *video, AudioSource *audio, AVIOutput *out, char *szFile, HDC hDC, COMPVARS *videoCompVars)	=0;
	virtual void Go(int iPriority = 0)	=0;

	virtual void RealizePalette()	=0;
	virtual void Abort()			=0;
	virtual bool isAbortedByUser()	=0;
	virtual void Tag(int x, int y)	=0;
	virtual void SetClientRectOffset(int x, int y) = 0;

	virtual void SetStatusHandler(IDubStatusHandler *pdsh)		=0;
	virtual void SetPriority(int index)=0;
	virtual void UpdateFrames()=0;
};

IDubber *CreateDubber(DubOptions *xopt);
void InitStreamValuesStatic(DubVideoStreamInfo& vInfo, DubAudioStreamInfo& aInfo, VideoSource *video, AudioSource *audio, DubOptions *opt, FrameSubset *pfs=NULL);

#ifndef f_DUB_CPP

extern DubOptions g_dubOpts;
#endif

#endif
