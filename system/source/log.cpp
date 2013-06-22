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

#ifdef _MSC_VER
#pragma warning(disable: 4786)		// shut up
#endif

#include <vd2/system/vdtypes.h>
#include <list>
#include <utility>
#include <vd2/system/log.h>
#include <vd2/system/thread.h>
#include <vd2/system/VDString.h>

namespace {
	wchar_t		g_log[16384];			// 32K log
	int			g_logHead, g_logTail;
	VDCriticalSection	g_csLog;

	typedef std::list<std::pair<IVDLogger *, VDThreadID> > tVDLoggers;
	tVDLoggers g_loggers;
}

void VDLog(int severity, const VDStringW& s) {
	int strSize = s.size() + 1;

	if (strSize >= 16384) {
		VDASSERT(false);
		return;
	}

	vdsynchronized(g_csLog) {
		for(;;) {
			int currentSize = (g_logTail - g_logHead) & 16383;

			if (currentSize + strSize < 16384)	// NOTE: This means that the last byte in the ring buffer can never be used.
				break;

			while(g_log[g_logHead++ & 16383])
				;

			g_logHead &= 16383;
		}

		const wchar_t *ps = s.data();

		g_log[g_logTail++] = severity;

		for(int i=1; i<strSize; ++i)
			g_log[g_logTail++ & 16383] = *ps++;

		g_log[g_logTail++ & 16383] = 0;

		g_logTail &= 16383;

		VDThreadID currentThread = VDGetCurrentThreadID();
		for(tVDLoggers::const_iterator it(g_loggers.begin()), itEnd(g_loggers.end()); it!=itEnd; ++it) {
			if (!(*it).second || currentThread == (*it).second)
				(*it).first->AddLogEntry(severity, s);
		}
	}
}

void VDAttachLogger(IVDLogger *pLogger, bool bThisThreadOnly, bool bReplayLog) {
	vdsynchronized(g_csLog) {
		g_loggers.push_back(tVDLoggers::value_type(pLogger, bThisThreadOnly ? VDGetCurrentThreadID() : 0));

		if (bReplayLog) {
			int idx = g_logHead;

			while(idx != g_logTail) {
				int severity = g_log[idx++];
				int headidx = idx;

				idx &= 16383;

				for(;;) {
					wchar_t c = g_log[idx];

					idx = (idx+1) & 16383;

					if (!c)
						break;
				}

				if (idx > headidx) {
					pLogger->AddLogEntry(severity, VDStringW(g_log + headidx, idx-headidx-1));
				} else {
					VDStringW t(idx+16383-headidx);

					std::copy(g_log + headidx, g_log + 16384, const_cast<wchar_t *>(t.data()));
					std::copy(g_log, g_log + idx - 1, const_cast<wchar_t *>(t.data() + (16384 - headidx)));
					pLogger->AddLogEntry(severity, t);
				}
			}
		}
	}
}

void VDDetachLogger(IVDLogger *pLogger) {
	vdsynchronized(g_csLog) {
		for(tVDLoggers::iterator it(g_loggers.begin()), itEnd(g_loggers.end()); it!=itEnd; ++it) {
			if (pLogger == (*it).first) {
				g_loggers.erase(it);
				break;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////
//
//	autologger
//
///////////////////////////////////////////////////////////////////////////

VDAutoLogger::VDAutoLogger(int min_severity)
	: mbAttached(true)
	, mMinSeverity(min_severity)
{
	VDAttachLogger(this, true, false);
}

VDAutoLogger::~VDAutoLogger() {
	if (mbAttached)
		VDDetachLogger(this);
}

void VDAutoLogger::AddLogEntry(int severity, const VDStringW& s) {
	if (severity >= mMinSeverity)
		mEntries.push_back(Entry(severity, s));
}

const VDAutoLogger::tEntries& VDAutoLogger::GetEntries() {
	if (mbAttached) {
		VDDetachLogger(this);
		mbAttached = false;
	}

	return mEntries;
}

