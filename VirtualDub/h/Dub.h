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
#include <vector>

#include <vd2/system/error.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/VDString.h>
#include <vd2/system/fraction.h>
#include "audio.h"
#include "filters.h"
#include "fixes.h"
#include "AVIStripeSystem.h"

class AsyncBlitter;
class AVIPipe;
class AVIOutput;
class DubSource;
class AudioSource;
class IVDVideoSource;
class FrameSubset;
class InputFile;
class IDubStatusHandler;
class IVDDubberOutputSystem;
struct VDAudioFilterGraph;

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
	bool bUseAudioFilterGraph;
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

	int		mInputFormat;
	int		mOutputFormat;
	char	mode;
	bool	fShowInputFrame, fShowOutputFrame, fShowDecompressedFrame;
	bool	fHistogram_unused, fSyncToAudio;
	int		frameRateDecimation;
	uint32	frameRateTargetHi, frameRateTargetLo;
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
	sint64	start_src;
	sint64	cur_src;
	sint64	cur_proc_src;
	sint64	end_src;
	sint64	cur_dst;
	sint64	end_dst;
	sint64	cur_proc_dst;
	sint64	end_proc_dst;
	sint64	total_size;
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
	sint64	start_us;
};

class DubVideoStreamInfo : public DubStreamInfo {
public:
	VDFraction	frameRate;
	VDFraction	frameRateIn;
	VDFraction	frameRateNoTelecine;
	long	usPerFrame;
	long	usPerFrameIn;
	long	usPerFrameNoTelecine;
	long	processed;
	uint32	lastProcessedTimestamp;
	bool	fAudioOnly;
};

class IDubber {
public:
	virtual ~IDubber()					=0;

	virtual void SetAudioCompression(const WAVEFORMATEX *wf, uint32 cb, const char *pShortNameHint) = 0;
	virtual void SetInputFile(InputFile *)=0;
	virtual void SetPhantomVideoMode()=0;
	virtual void SetInputDisplay(IVDVideoDisplay *pDisplay) = 0;
	virtual void SetOutputDisplay(IVDVideoDisplay *pDisplay) = 0;
	virtual void SetAudioFilterGraph(const VDAudioFilterGraph& graph)=0;
	virtual void EnableSpill(__int64 size, long lFrameLimit)=0;
	virtual void Init(IVDVideoSource *video, AudioSource *audio, IVDDubberOutputSystem *out, COMPVARS *videoCompVars, const FrameSubset *pfs) = 0;
	virtual void Go(int iPriority = 0) = 0;
	virtual void Stop() = 0;

	virtual void RealizePalette()	=0;
	virtual void Abort()			=0;
	virtual bool isRunning()		=0;
	virtual bool isAbortedByUser()	=0;
	virtual bool IsPreviewing()		=0;

	virtual void SetStatusHandler(IDubStatusHandler *pdsh)		=0;
	virtual void SetPriority(int index)=0;
	virtual void UpdateFrames()=0;
};

IDubber *CreateDubber(DubOptions *xopt);
void InitStreamValuesStatic(DubVideoStreamInfo& vInfo, DubAudioStreamInfo& aInfo, IVDVideoSource *video, AudioSource *audio, DubOptions *opt, const FrameSubset *pfs=NULL);

#ifndef f_DUB_CPP

extern DubOptions g_dubOpts;
#endif

#endif
