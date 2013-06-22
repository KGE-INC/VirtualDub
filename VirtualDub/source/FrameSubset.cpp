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

#include "stdafx.h"

#include "FrameSubset.h"
#include <vd2/system/error.h>

FrameSubset::FrameSubset() {
}

FrameSubset::FrameSubset(int length) {
	addRange(0, length, false);
}

FrameSubset::~FrameSubset() {
}

void FrameSubset::clear() {
	mTimeline.clear();
}

void FrameSubset::addFrom(FrameSubset& src) {
	for(iterator it(src.begin()), itEnd(src.end()); it!=itEnd; ++it)
		addRangeMerge(it->start, it->len, it->bMask);
}

void FrameSubset::addRange(int start, int len, bool bMask) {
	mTimeline.push_back(FrameSubsetNode(start, len, bMask));
}

void FrameSubset::addRangeMerge(int start, int len, bool bMask) {
	tTimeline::iterator it(begin()), itEnd(end());

	while(it != itEnd) {
		if (start + len < it->start) {				// Isolated -- insert
			mTimeline.insert(it, FrameSubsetNode(start, len, bMask));
			return;
		} else if (start + len >= it->start && start <= it->start+it->len) {		// Overlap!

			if (start+len > it->start+it->len) {	// < A [ B ] > or [ B <] A > cases
				// If the types are compatible, accumulate.  Otherwise, write out
				// the head portion if it exists, and trim to the tail.

				if (bMask != it->bMask) {
					if (start < it->start)
						mTimeline.insert(it, FrameSubsetNode(start, it->start - start, bMask));

					len -= (it->start + it->len - start);
					start = it->start + it->len;
				} else {
					if (it->start < start) {
						len += (start - it->start);
						start = it->start;
					}

					it = mTimeline.erase(it);
					continue;
				}
			} else {									// < A [> B ], <A | B>, or [ <A> B ] cases

				// Check the types.  If the types are compatible, great -- merge
				// the blocks and be done.  If the types are different, trim the
				// new block.

				if (bMask != it->bMask) {
					len = it->start - start;

					if (len > 0)
						mTimeline.insert(it, FrameSubsetNode(start, len, bMask));

					return;
				} else if (it->start > start) {
					it->len += (it->start - start);
					it->start = start;
				}
#ifdef _DEBUG
				goto check_list;
#else
				return;
#endif
			}
		}

		++it;
	}

	// List is empty or element falls after last element

	addRange(start, len, bMask);

#ifdef _DEBUG
check_list:
	int lastpt = -1;
	bool bLastWasMasked;

	for(it = begin(); it!=itEnd; ++it) {
		if (it->start <= lastpt && bLastWasMasked == it->bMask) {
			throw MyError("addRangeMerge: FAILED!!  %d <= %d\n", it->start, lastpt);
		}

		lastpt = it->start + it->len;
		bLastWasMasked = it->bMask;
	}

#endif
}

int FrameSubset::getTotalFrames() {
	int iFrames = 0;

	for(const_iterator it(mTimeline.begin()), itEnd(mTimeline.end()); it!=itEnd; ++it)
		iFrames += it->len;

	return iFrames;
}

int FrameSubset::lookupFrame(int frame, bool& bMasked) {
	int len = 1;

	return lookupRange(frame, len, bMasked);
}

int FrameSubset::revLookupFrame(int frame, bool& bMasked) {
	int iSrcFrame = 0;

	for(const_iterator it(begin()), itEnd(end()); it!=itEnd; ++it) {
		if (frame >= it->start && frame < it->start+it->len) {
			bMasked = it->bMask;
			return iSrcFrame + (frame - it->start);
		}

		iSrcFrame += it->len;
	}

	return -1;
}

int FrameSubset::lookupRange(int start, int& len, bool& bMasked) {
	int offset;
	const_iterator it = findNode(offset, start);

	if (it == end()) return -1;

	bMasked = it->bMask;
	if (it->bMask) {
		len = 1;

		while(it->bMask) {
			if (it == begin()) {
				// First range is masked... this is bad.  Oh well, just
				// return the first frame in the first range.

				return it->start;
			}

			--it;
		}

		return it->start + it->len - 1;
	} else {
		len = it->len - offset;
		return it->start + offset;
	}
}

void FrameSubset::deleteInputRange(int start, int len) {
	for(iterator it(begin()), itEnd(end()); it != itEnd; ++it) {
		if (it->start >= start+len)
			break;

		if (it->start + it->len >= start && it->start < start+len) {
			bool bSectionBeforeDelete = it->start < start;
			bool bSectionAfterDelete = it->start + it->len > start + len;

			if (bSectionAfterDelete) {
				if (bSectionBeforeDelete)
					mTimeline.insert(it, FrameSubsetNode(it->start, start - it->start, it->bMask));

				it->len = (it->start + it->len) - (start+len);
				it->start = start+len;
				break;
			} else {
				if (bSectionBeforeDelete)			// before only
					it->len = start - it->start;
				else {								// absorbed
					it = mTimeline.erase(it);
					continue;
				}
			}
		}

		++it;
	}
}

void FrameSubset::deleteRange(int start, int len) {
	int offset;
	iterator it = findNode(offset, start), itEnd = end();

	while(it != itEnd && len>0) {
		if (len+offset < it->len) {
			if (offset) {
				FrameSubsetNode *pfsn2 = new FrameSubsetNode;

				if (!pfsn2) throw MyMemoryError();

				mTimeline.insert(it, FrameSubsetNode(it->start, start - it->start, it->bMask));
			}

			it->start += (offset+len);
			it->len -= (offset+len);

			break;
		} else {
			if (offset) {
				len -= it->len - offset;
				it->len = offset;
				offset = 0;
				++it;
			} else {
				len -= it->len;
				it = mTimeline.erase(it);
				continue;
			}
		}
	}
	dump();
}

void FrameSubset::setRange(int start, int len, bool bMask) {
	int offset;
	iterator it = findNode(offset, start), itEnd(end());

	while(it != itEnd && len>0) {
		if (len+offset < it->len) {
			if (it->bMask != bMask) {
				if (offset)
					mTimeline.insert(it, FrameSubsetNode(it->start, offset, it->bMask));

				mTimeline.insert(it, FrameSubsetNode(it->start + offset, len, bMask));

				it->start += (offset+len);
				it->len -= (offset+len);
			}

			break;
		} else {
			if (offset) {
				if (it->bMask != bMask) {
					mTimeline.insert(it, FrameSubsetNode(it->start, offset, it->bMask));

					it->start += offset;
					it->len -= offset;
					it->bMask = bMask;
				}

				len -= it->len;
				offset = 0;
			} else {
				it->bMask = bMask;
				len -= it->len;
				it = mTimeline.erase(it);
			}
		}

		++it;
	}
}

void FrameSubset::clip(int start, int len) {
	deleteRange(0, start);
	deleteRange(len, 0x7FFFFFFF - len);
}

void FrameSubset::offset(int off) {
	for(iterator it = begin(), itEnd = end(); it != itEnd; ++it)
		it->start += off;
}

void FrameSubset::assign(const FrameSubset& src, int start, int len) {
	mTimeline = src.mTimeline;
	clip(start, len);
}

void FrameSubset::insert(iterator it, const FrameSubset& src) {
	if (src.empty())
		return;

	const_iterator itSrc(src.begin()), itSrcEnd(src.end());

	iterator itFront = mTimeline.insert(it, *itSrc);
	iterator itBack = itFront;
	++itSrc;

	for(; itSrc != itSrcEnd; ++itSrc)
		itBack = mTimeline.insert(++itBack, *itSrc);

	// check for merge in front

	if (itFront != begin()) {
		iterator itBeforeFront = itFront;
		--itBeforeFront;
		if (itBeforeFront->bMask == itFront->bMask && itBeforeFront->start + itBeforeFront->len == itFront->start) {
			itFront->len += itBeforeFront->len;
			itFront->start -= itBeforeFront->len;
			mTimeline.erase(itBeforeFront);
		}
	}

	// check for merge in back
	iterator itAfterBack = itBack;
	++itAfterBack;
	if (itAfterBack != end()) {
		if (itBack->bMask != itAfterBack->bMask && itBack->start + itBack->len == itAfterBack->start) {
			itBack->len += itAfterBack->len;
			mTimeline.erase(itAfterBack);
		}
	}

	dump();
}

void FrameSubset::insert(int insertionPoint, const FrameSubset& src) {
	int offset;
	FrameSubset::iterator it;
	
	if (insertionPoint < 0)
		it = begin();
	else
		it = findNode(offset, insertionPoint);

	if (it != end() && offset > 0) {
		mTimeline.insert(it, FrameSubsetNode(it->start, offset, it->bMask));
		it->start += offset;
		it->len -= offset;
	}

	insert(it, src);
}

FrameSubset::iterator FrameSubset::findNode(int& poffset, int iDstFrame) {
	if (iDstFrame<0)
		return end();

	for(iterator it(begin()), itEnd(end()); it!=itEnd && iDstFrame >= 0; ++it) {
		if (iDstFrame < it->len) {
			poffset = iDstFrame;
			return it;
		}

		iDstFrame -= it->len;
	}

	poffset = 0;
	return end();
}

void FrameSubset::dump() {
#ifdef _DEBUG
	VDDEBUG("Frame subset dump:\n");
	for(const_iterator it(begin()), itEnd(end()); it!=itEnd; ++it) {
		VDDEBUG("   start: %6d   len:%4d   bMask:%d\n", it->start, it->len, it->bMask);
	}
#endif
}

///////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
class FrameSubsetClassVerifier {
public:
	void check(FrameSubset& fs, int test, ...) {
		va_list val;
		FrameSubset::iterator pfsn = fs.begin();

		va_start(val, test);
		while(pfsn != fs.end()) {
			if (pfsn->start != va_arg(val, int))
				throw MyError("fail test #%dA", test);
			if (pfsn->len != va_arg(val, int))
				throw MyError("fail test #%dB", test);

			++pfsn;
		}
		if (va_arg(val, int) != -1)
			throw MyError("fail test #%dC", test);

		va_end(val);

	}

	FrameSubsetClassVerifier() {
		_RPT0(0,"Verifying class: FrameSubset\n");
		try {
			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false);
				fs.addRangeMerge(30, 10, false);
				fs.addRangeMerge(50, 10, false);
				check(fs, 1, 10, 10, 30, 10, 50, 10, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false);
				fs.addRangeMerge(20, 10, false);
				fs.addRangeMerge(30, 10, false);
				check(fs, 2, 10, 30, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false);
				fs.addRangeMerge(20, 10, false);
				fs.addRangeMerge(50, 10, false);
				check(fs, 3, 10, 20, 50, 10, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false);
				fs.addRangeMerge(40, 10, false);
				fs.addRangeMerge(50, 10, false);
				check(fs, 4, 10, 10, 40, 20, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false);
				fs.addRangeMerge(15, 10, false);
				fs.addRangeMerge(50, 10, false);
				check(fs, 5, 10, 15, 50, 10, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false);
				fs.addRangeMerge(45, 10, false);
				fs.addRangeMerge(50, 10, false);
				check(fs, 6, 10, 10, 45, 15, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false);
				fs.addRangeMerge(15, 30, false);
				fs.addRangeMerge(50, 10, false);
				check(fs, 7, 10, 35, 50, 10, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false);
				fs.addRangeMerge(8, 48, false);
				fs.addRangeMerge(50, 10, false);
				check(fs, 8, 8, 52, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10, false);
				fs.addRangeMerge(8, 100, false);
				fs.addRangeMerge(50, 10, false);
				check(fs, 9, 8, 100, -1);
			}


		} catch(const MyError& e) {
			e.post(NULL, "Class verify failed: FrameSubset");
		}
	}
} g_ClassVerifyFrameSubset;
#endif
