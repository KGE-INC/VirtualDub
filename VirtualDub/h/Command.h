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

class InputFile;
class AVIOutput;
class VideoSource;
class AudioSource;
class IDubber;
class DubOptions;
class FrameSubset;
struct VDAudioFilterGraph;

extern InputFile			*inputAVI;

extern VideoSource			*inputVideoAVI;

extern AudioSource			*inputAudio;
extern AudioSource			*inputAudioAVI;
extern AudioSource			*inputAudioWAV;

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

extern BOOL				g_drawDecompressedFrame;
extern BOOL				g_showStatusWindow;
extern BOOL				g_syncroBlit;
extern BOOL				g_vertical;

///////////////////////////

enum {
	FILETYPE_AUTODETECT		= 0,
	FILETYPE_AVI			= 1,
	FILETYPE_MPEG			= 2,
	FILETYPE_ASF			= 3,
	FILETYPE_STRIPEDAVI		= 4,
	FILETYPE_AVICOMPAT		= 5,
	FILETYPE_IMAGE			= 6,
	FILETYPE_AUTODETECT2	= 7,
};

void OpenAVI(const char *szFile, int iFileType, bool fExtendedOpen, bool fQuiet=false, bool fAutoscan=false, const char *pInputOpts=0);
void AppendAVI(const char *pszFile);
void AppendAVIAutoscan(const char *pszFile);
void SetAudioSource();
void CloseAVI();
void OpenWAV(const char *szFile);
void CloseWAV();
void SaveWAV(const char *szFilename, bool fProp = false, DubOptions *quick_opts=NULL);
void SaveAVI(const char *szFilename, bool fProp = false, DubOptions *quick_opts=NULL, bool fCompatibility=false);
void SaveStripedAVI(const char *szFile);
void SaveStripeMaster(const char *szFile);
void SaveSegmentedAVI(char *szFilename, bool fProp, DubOptions *quick_opts, long lSpillThreshold, long lSpillFrameThreshold);
void SaveImageSequence(const char *szPrefix, const char *szSuffix, int minDigits, bool fProp, DubOptions *quick_opts, int targetFormat);
void SetSelectionStart(long ms);
void SetSelectionEnd(long ms);
void RemakePositionSlider();
void RecalcPositionTimeConstant();
void EnsureSubset();
void ScanForUnreadableFrames(FrameSubset *pSubset, VideoSource *pVideoSource);

#endif
