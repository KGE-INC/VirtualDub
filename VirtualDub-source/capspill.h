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

#ifndef f_VIRTUALDUB_CAPSPILL_H
#define f_VIRTUALDUB_CAPSPILL_H

template<class T> class ListNode2;

class CapSpillDrive : public ListNode2<CapSpillDrive> {
public:
	int threshold;
	int priority;
	char *path;

	CapSpillDrive();
	~CapSpillDrive();

	void setPath(char *s);
	char *makePath(char *buf, const char *fn) const;
};


__int64 CapSpillGetFreeSpace();
CapSpillDrive *CapSpillFindDrive(const char *path);
void CapSpillSaveToRegistry();
void CapSpillRestoreFromRegistry();
CapSpillDrive *CapSpillPickDrive(bool fAudio);

#endif
