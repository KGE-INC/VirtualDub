//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
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


#ifndef f_TIMELINE_H
#define f_TIMELINE_H

#include "FrameSubset.h"

class IVDVideoSource;
class VDFraction;

class VDTimeline {
public:
	VDTimeline();
	~VDTimeline();

	FrameSubset&	GetSubset() { return mSubset; }

	void SetVideoSource(IVDVideoSource *pVS) { mpVideo = pVS; }
	void SetFromSource();

	VDPosition		GetStart()			{ return 0; }
	VDPosition		GetEnd()			{ return mSubset.getTotalFrames(); }
	VDPosition		GetLength()			{ return mSubset.getTotalFrames(); }
	VDPosition		GetNearestKey(VDPosition pos);
	VDPosition		GetPrevKey(VDPosition pos);
	VDPosition		GetNextKey(VDPosition pos);
	VDPosition		GetPrevDrop(VDPosition pos);
	VDPosition		GetNextDrop(VDPosition pos);
	VDPosition		GetPrevEdit(VDPosition pos);
	VDPosition		GetNextEdit(VDPosition pos);

	VDPosition		TimelineToSourceFrame(VDPosition pos);

	void	Rescale(const VDFraction& oldRate, sint64 oldLength, const VDFraction& newRate, sint64 newLength);

protected:
	FrameSubset	mSubset;

	IVDVideoSource *mpVideo;
};

#endif
