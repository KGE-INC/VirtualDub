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

#ifndef f_VIRTUALDUB_COMMAND_H
#define f_VIRTUALDUB_COMMAND_H

#include <windows.h>
#include <vfw.h>

#include <vd2/system/refcount.h>

class InputFile;
class IVDInputDriver;
class AVIOutput;
class VideoSource;
class AudioSource;
class IDubber;
class DubOptions;
class FrameSubset;
struct VDAudioFilterGraph;

extern vdrefptr<InputFile>		inputAVI;

extern vdrefptr<VideoSource>	inputVideoAVI;

extern vdrefptr<AudioSource>	inputAudio;
extern vdrefptr<AudioSource>	inputAudioAVI;
extern vdrefptr<AudioSource>	inputAudioWAV;

extern FrameSubset			*inputSubset;

enum {
	AUDIOIN_NONE	= 0,
	AUDIOIN_AVI		= 1,
	AUDIOIN_WAVE	= 2,
};

extern int					audioInputMode;
extern IDubber				*g_dubber;

extern COMPVARS			g_Vcompression;
extern WAVEFORMATEX		*g_ACompressionFormat;
extern DWORD			g_ACompressionFormatSize;

extern VDAudioFilterGraph	g_audioFilterGraph;

extern bool				g_drawDecompressedFrame;
extern bool				g_showStatusWindow;
extern bool				g_syncroBlit;

///////////////////////////

void AppendAVI(const wchar_t *pszFile);
void AppendAVIAutoscan(const wchar_t *pszFile);
void SetAudioSource();
void CloseAVI();
void OpenWAV(const wchar_t *szFile);
void CloseWAV();
void SaveWAV(const wchar_t *szFilename, bool fProp = false, DubOptions *quick_opts=NULL);
void SaveAVI(const wchar_t *szFilename, bool fProp = false, DubOptions *quick_opts=NULL, bool fCompatibility=false);
void SaveStripedAVI(const wchar_t *szFile);
void SaveStripeMaster(const wchar_t *szFile);
void SaveSegmentedAVI(const wchar_t *szFilename, bool fProp, DubOptions *quick_opts, long lSpillThreshold, long lSpillFrameThreshold);
void SaveImageSequence(const wchar_t *szPrefix, const wchar_t *szSuffix, int minDigits, bool fProp, DubOptions *quick_opts, int targetFormat);
void SetSelectionStart(long ms);
void SetSelectionEnd(long ms);
void RemakePositionSlider();
void RecalcPositionTimeConstant();
void EnsureSubset();
void ScanForUnreadableFrames(FrameSubset *pSubset, VideoSource *pVideoSource);

#endif
