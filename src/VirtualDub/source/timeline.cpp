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

#include "stdafx.h"

#include "DubSource.h"
#include "VideoSource.h"
#include "timeline.h"

VDTimeline::VDTimeline() {
}

VDTimeline::~VDTimeline() {
}

void VDTimeline::SetFromSource() {
	IVDStreamSource *ps = mpVideo->asStream();

	mSubset.clear();
	mSubset.insert(mSubset.begin(), FrameSubsetNode(ps->getStart(), ps->getLength(), false, 0));
}

VDPosition VDTimeline::GetNearestKey(VDPosition pos) {
	if (pos <= 0)
		pos = 0;
	else {
		sint64 offset;
		FrameSubset::iterator it(mSubset.findNode(offset, pos)), itBegin(mSubset.begin()), itEnd(mSubset.end());

		do {
			if (it!=itEnd) {
				const FrameSubsetNode& fsn0 = *it;

				if (!fsn0.bMask) {
					pos = mpVideo->nearestKey(fsn0.start + offset) - fsn0.start;

					if (pos >= 0)
						break;
				}
			}

			while(it != itBegin) {
				--it;
				const FrameSubsetNode& fsn = *it;

				if (!fsn.bMask) {
					pos = mpVideo->nearestKey(fsn.start + fsn.len - 1) - fsn.start;

					if (pos >= 0)
						break;
				}

				pos = 0;
			}
		} while(false);

		while(it != itBegin) {
			--it;
			const FrameSubsetNode& fsn2 = *it;

			pos += fsn2.len;
		}
	}

	return pos;
}

VDPosition VDTimeline::GetPrevKey(VDPosition pos) {
	if (pos <= 0)
		return -1;

	sint64 offset;
	FrameSubset::iterator it(mSubset.findNode(offset, pos)), itBegin(mSubset.begin()), itEnd(mSubset.end());

	do {
		if (it!=itEnd) {
			const FrameSubsetNode& fsn0 = *it;

			if (!fsn0.bMask) {
				pos = mpVideo->prevKey(fsn0.start + offset) - fsn0.start;

				if (pos >= 0)
					break;
			}
		}

		while(it != itBegin) {
			--it;
			const FrameSubsetNode& fsn = *it;

			if (!fsn.bMask) {
				pos = mpVideo->nearestKey(fsn.start + fsn.len - 1) - fsn.start;

				if (pos >= 0)
					break;
			}

			pos = 0;
		}
	} while(false);

	while(it != itBegin) {
		--it;
		const FrameSubsetNode& fsn2 = *it;

		pos += fsn2.len;
	}

	return pos;
}

VDPosition VDTimeline::GetNextKey(VDPosition pos) {
	if (pos >= mSubset.getTotalFrames() - 1)
		return -1;

	else {
		sint64 offset;
		FrameSubset::iterator it(mSubset.findNode(offset, pos)), itBegin(mSubset.begin()), itEnd(mSubset.end());

		do {
			if (it==itEnd) {
				VDASSERT(false);
				return -1;
			}

			const FrameSubsetNode& fsn0 = *it;

			if (!fsn0.bMask) {
				pos = mpVideo->nextKey(fsn0.start + offset) - fsn0.start;

				if (pos >= 0 && pos < fsn0.len)
					break;

				pos = 0;
			}

			for(;;) {
				if (++it == itEnd)
					return -1;

				const FrameSubsetNode& fsn = *it;

				if (!fsn.bMask) {
					pos = 0;
					if (mpVideo->isKey(fsn.start))
						break;

					pos = mpVideo->nextKey(fsn.start) - fsn.start;

					if (pos >= 0 && pos < fsn.len)
						break;
				}

				pos = 0;
			}
		} while(false);

		while(it != itBegin) {
			--it;
			const FrameSubsetNode& fsn2 = *it;

			pos += fsn2.len;
		}
	}

	return pos;
}

VDPosition VDTimeline::GetPrevDrop(VDPosition pos) {
	IVDStreamSource *pVS = mpVideo->asStream();

	while(--pos >= 0) {
		int err;
		uint32 lBytes, lSamples;

		err = pVS->read(mSubset.lookupFrame(pos), 1, NULL, 0, &lBytes, &lSamples);
		if (err != AVIERR_OK)
			break;

		if (!lBytes)
			return pos;
	}

	return pos;
}

VDPosition VDTimeline::GetNextDrop(VDPosition pos) {
	IVDStreamSource *pVS = mpVideo->asStream();
	const VDPosition len = mSubset.getTotalFrames();

	while(++pos < len) {
		int err;
		uint32 lBytes, lSamples;

		err = pVS->read(mSubset.lookupFrame(pos), 1, NULL, 0, &lBytes, &lSamples);
		if (err != AVIERR_OK)
			break;

		if (!lBytes)
			return pos;
	}

	return -1;
}

VDPosition VDTimeline::GetPrevEdit(VDPosition pos) {
	sint64 offset;

	FrameSubset::iterator pfsn = mSubset.findNode(offset, pos);

	if (pfsn == mSubset.end()) {
		if (pos >= 0) {
			if (pfsn != mSubset.begin()) {
				--pfsn;
				return mSubset.getTotalFrames() - pfsn->len;
			}
		}
		return -1;
	}
	
	if (offset)
		return pos - offset;

	if (pfsn != mSubset.begin()) {
		--pfsn;
		return pos - pfsn->len;
	}

	return -1;
}

VDPosition VDTimeline::GetNextEdit(VDPosition pos) {
	sint64 offset;

	FrameSubset::iterator pfsn = mSubset.findNode(offset, pos);

	if (pfsn == mSubset.end())
		return -1;

	pos -= offset;
	pos += pfsn->len;

	++pfsn;

	if (pfsn == mSubset.end())
		return -1;

	return pos;
}

VDPosition VDTimeline::TimelineToSourceFrame(VDPosition pos) {
	return mSubset.lookupFrame(pos);
}
