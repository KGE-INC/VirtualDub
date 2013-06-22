//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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

#ifndef f_INPUTFILEAVI_H
#define f_INPUTFILEAVI_H

#include "InputFile.h"

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
	static INT_PTR APIENTRY _InfoDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
public:
	InputFileAVI();
	~InputFileAVI();

	void Init(const wchar_t *szFile);
	void InitStriped(const char *szFile);
	bool Append(const wchar_t *szFile);

	bool isOptimizedForRealtime();
	bool isStreaming();

	void setOptions(InputFileOptions *_ifo);
	InputFileOptions *createOptions(const char *buf);
	InputFileOptions *promptForOptions(HWND hwnd);
	void EnableSegmentAutoscan();
	void ForceCompatibility();
	void setAutomated(bool fAuto);

	void InfoDialog(HWND hwndParent);
};

#endif
