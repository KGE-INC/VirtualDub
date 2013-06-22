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

#ifndef f_POSITIONCONTROL_H
#define f_POSITIONCONTROL_H

#include <windows.h>

#define POSITIONCONTROLCLASS (szPositionControlName)

#ifndef f_POSITIONCONTROL_CPP
extern const char szPositionControlName[];
#endif

typedef char (*PosCtlFTCallback)(HWND hwnd, void *data, long pos);

#define PCS_PLAYBACK			(0x00000001L)
#define PCS_MARK				(0x00000002L)
#define	PCS_SCENE				(0x00000004L)

#define PCN_THUMBTRACK			(NM_FIRST+0)
#define PCN_THUMBPOSITION		(NM_FIRST+1)
#define PCN_PAGELEFT			(NM_FIRST+2)
#define PCN_PAGERIGHT			(NM_FIRST+3)
#define PCN_BEGINTRACK			(NM_FIRST+4)
#define PCN_ENDTRACK			(NM_FIRST+5)

#define PCN_STOP				(0)
#define PCN_PLAY				(1)
#define	PCN_PLAYPREVIEW			(10)
#define PCN_MARKIN				(2)
#define PCN_MARKOUT				(3)
#define PCN_START				(4)
#define PCN_BACKWARD			(5)
#define PCN_FORWARD				(6)
#define PCN_END					(7)
#define PCN_KEYPREV				(8)
#define PCN_KEYNEXT				(9)
#define	PCN_SCENEREV			(11)
#define	PCN_SCENEFWD			(12)
#define	PCN_SCENESTOP			(13)

#define PCM_SETRANGEMIN			(WM_USER+0x100)
#define PCM_SETRANGEMAX			(WM_USER+0x101)
#define PCM_GETPOS				(WM_USER+0x102)
#define PCM_SETPOS				(WM_USER+0x103)
#define PCM_GETSELSTART			(WM_USER+0x104)
#define PCM_GETSELEND			(WM_USER+0x105)
#define PCM_SETSELSTART			(WM_USER+0x106)
#define PCM_SETSELEND			(WM_USER+0x107)
#define PCM_SETFRAMERATE		(WM_USER+0x108)
#define	PCM_RESETSHUTTLE		(WM_USER+0x109)
#define PCM_CLEARSEL			(WM_USER+0x10A)
#define PCM_SETFRAMETYPECB		(WM_USER+0x10B)
#define PCM_CTLAUTOFRAME		(WM_USER+0x10C)
#define PCM_SETDISPFRAME		(WM_USER+0x10D)

ATOM RegisterPositionControl();

#endif
