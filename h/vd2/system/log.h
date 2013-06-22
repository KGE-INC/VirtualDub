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

#ifndef f_VD2_SYSTEM_LOG_H
#define f_VD2_SYSTEM_LOG_H

#include <vd2/system/VDString.h>
#include <list>

class IVDLogger {
public:
	virtual void AddLogEntry(int severity, const VDStringW& s) = 0;
};

enum {
	kVDLogInfo, kVDLogMarker, kVDLogWarning, kVDLogError
};

void VDLog(int severity, const VDStringW& s);
void VDAttachLogger(IVDLogger *pLogger, bool bThisThreadOnly, bool bReplayLog);
void VDDetachLogger(IVDLogger *pLogger);

class VDAutoLogger : public IVDLogger {
public:
	struct Entry {
		int severity;
		VDStringW text;

		Entry(int sev, const VDStringW& s) : severity(sev), text(s) {}
	};

	typedef std::list<Entry>	tEntries;

	VDAutoLogger(int min_severity);
	~VDAutoLogger();

	void AddLogEntry(int severity, const VDStringW& s);

	const tEntries& GetEntries();

protected:
	tEntries mEntries;
	const int mMinSeverity;
	bool	mbAttached;
};

#endif
