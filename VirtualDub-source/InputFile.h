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

#ifndef f_INPUTFILE_H
#define f_INPUTFILE_H

#include <windows.h>
#include <vfw.h>

#include "List.h"

class AudioSource;
class VideoSource;
class AVIStripeSystem;
class IAVIReadHandler;
class IAVIReadStream;


class InputFileOptions {
public:
	virtual ~InputFileOptions()=0;
	virtual bool read(const char *buf)=0;
	virtual int write(char *buf, int buflen)=0;
};

class InputFilenameNode : public ListNode2<InputFilenameNode> {
public:
	const char *name;

	InputFilenameNode(const char *_n);
	~InputFilenameNode();
};

class InputFile {
public:
	AudioSource *audioSrc;
	VideoSource *videoSrc;
	List2<InputFilenameNode> listFiles;

	virtual ~InputFile();
	virtual void Init(char *szFile) = 0;
	virtual bool Append(const char *szFile);

	virtual void setOptions(InputFileOptions *);
	virtual void setAutomated(bool);
	virtual InputFileOptions *promptForOptions(HWND);
	virtual InputFileOptions *createOptions(const char *buf);
	virtual void InfoDialog(HWND hwndParent);

	virtual bool isOptimizedForRealtime();
	virtual bool isStreaming();

protected:
	void AddFilename(const char *lpszFile);
};

class InputFileAVI : public InputFile {
private:
	IAVIReadHandler *pAVIFile;
	IAVIReadStream *pAVIStreamAudio, *pAVIStreamVideo;

	AVIStripeSystem *stripesys;
	IAVIReadHandler **stripe_files;
	int stripe_count;
	bool isASF;
	bool fAutomated;

	bool fCompatibilityMode, fRedoKeyFlags, fInternalMJPEG, fDisableFastIO, fAcceptPartial, fAutoscanSegments;
	int iMJPEGMode;
	FOURCC fccForceVideo;
	FOURCC fccForceVideoHandler;
	long lForceAudioHz;

	static char szME[];

	static void _InfoDlgThread(void *pvInfo);
	static BOOL APIENTRY _InfoDlgProc( HWND hDlg, UINT message, UINT wParam, LONG lParam);
public:
	InputFileAVI(bool isASF);
	~InputFileAVI();

	void Init(char *szFile);
	void InitStriped(char *szFile);
	bool Append(const char *szFile);

	bool isOptimizedForRealtime();
	bool isStreaming();

	void setOptions(InputFileOptions *_ifo);
	InputFileOptions *createOptions(const char *buf);
	InputFileOptions *promptForOptions(HWND hwnd);
	void EnableSegmentAutoscan();
	void ForceCompatibility();

	void InfoDialog(HWND hwndParent);
};

#endif
