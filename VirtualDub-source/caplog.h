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

#ifndef f_CAPLOG_H
#define f_CAPLOG_H

#include <windows.h>

#include "List.h"

struct CapEvent {
	enum { VIDEO=0, AUDIO } type;
	DWORD dwTimeStampReceived;
	DWORD dwBytes;

	union {
		struct {
			DWORD dwTimeStampRecorded;
		} video;

		struct {
			LONG lVideoDelta;
		} audio;
	};
};

class CapEventBlock : public ListNode2<CapEventBlock> {
public:
	CapEvent ev[256];
};

class CaptureLog {
public:
	CaptureLog();
	~CaptureLog();

	bool LogVideo(DWORD dwTimeStampReceived, DWORD dwBytes, DWORD dwTimeStampRecorded);
	bool LogAudio(DWORD dwTimeStampReceived, DWORD dwBytes, LONG lVideoDelta);
	void Display(HWND hwndParent);
	void Dispose();

private:
	List2<CapEventBlock> listBlocks;
	CapEventBlock *pceb;
	int nEvents;
	int nEventsBlock;

	void GetDispInfo(NMLVDISPINFO *nldi);
	CapEvent *GetNewRequest();

	BOOL DlgProc2(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
	static BOOL CALLBACK DlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam);
};

#endif
