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

#ifndef f_VIRTUALDUB_JOB_H
#define f_VIRTUALDUB_JOB_H

#include <stdio.h>

class DubOptions;
class InputFilenameNode;
template<class T> class List2;

void OpenJobWindow();
void CloseJobWindow();
bool InitJobSystem();
void DeinitJobSystem();
void JobAddConfiguration(const DubOptions *, const char *szFileInput, int iFileMode, const char *szFileOutput, bool fUseCompatibility, List2<InputFilenameNode> *pListAppended, long lSpillThreshold, long lSpillFrameThreshold);
void JobWriteConfiguration(FILE *f, DubOptions *);
void JobLockDubber();
void JobUnlockDubber();
void JobPositionCallback(LONG start, LONG cur, LONG end);
void JobClearList();
void JobRunList();
void JobAddBatchDirectory(const char *srcDir, const char *dstDir);

#endif
