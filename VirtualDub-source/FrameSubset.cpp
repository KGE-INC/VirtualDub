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

#include <crtdbg.h>

#include "FrameSubset.h"
#include "Error.h"

FrameSubset::FrameSubset() {
}

FrameSubset::FrameSubset(int length) {
	addRange(0, length);
}

void FrameSubset::clear() {
	FrameSubsetNode *pfsn;

	while(pfsn = list.RemoveHead())
		delete pfsn;
}

void FrameSubset::addFrom(FrameSubset& src) {
	FrameSubsetNode *pfsn_next, *pfsn = src.list.AtHead();

	while(pfsn_next = pfsn->NextFromHead()) {
		addRangeMerge(pfsn->start, pfsn->len);
		pfsn = pfsn_next;
	}
}

void FrameSubset::addRange(int start, int len) {
	FrameSubsetNode *pfsn = new FrameSubsetNode(start, len);

	if (!pfsn)
		throw MyMemoryError();

	pfsn->start		= start;
	pfsn->len		= len;

	list.AddTail(pfsn);
}

void FrameSubset::addRangeMerge(int start, int len) {
	FrameSubsetNode *pfsn_next, *pfsn = list.AtHead();

	while(pfsn_next = pfsn->NextFromHead()) {
		if (start + len < pfsn->start) {				// Isolated -- insert
			FrameSubsetNode *pfsn_new = new FrameSubsetNode(start, len);

			if (!pfsn_new)
				throw MyMemoryError();

			pfsn_new->InsertBefore(pfsn);
			return;
		} else if (start + len >= pfsn->start && start <= pfsn->start+pfsn->len) {		// Overlap!
			if (start+len > pfsn->start+pfsn->len) {	// < A [ B ] > or [ B <] A > cases
				if (pfsn->start < start) {
					len += (start - pfsn->start);
					start = pfsn->start;
				}

				pfsn->Remove();
				delete pfsn;
			} else {									// < A [> B ], <A | B>, or [ <A> B ] cases
				if (pfsn->start > start) {
					len += (start - pfsn->start);
					pfsn->start = start;
				}
#ifdef _DEBUG
				goto check_list;
#else
				return;
#endif
			}
		}

		pfsn = pfsn_next;
	}

	// List is empty or element falls after last element

	addRange(start, len);

#ifdef _DEBUG
check_list:
	int lastpt = -1;

	pfsn = list.AtHead();
	while(pfsn_next = pfsn->NextFromHead()) {
		if (pfsn->start <= lastpt) {
			throw MyError("addRangeMerge: FAILED!!  %d <= %d\n", pfsn->start, lastpt);
		}

		lastpt = pfsn->start + pfsn->len;

		pfsn = pfsn_next;
	}

#endif
}

FrameSubset::~FrameSubset() {
	clear();
}

int FrameSubset::getTotalFrames() {
	FrameSubsetNode *pfsn_next, *pfsn = list.AtHead();
	int iFrames = 0;

	while(pfsn_next = pfsn->NextFromHead()) {
		iFrames += pfsn->len;

		pfsn = pfsn_next;
	}

	return iFrames;
}

int FrameSubset::lookupFrame(int frame) {
	int len = 1;

	return lookupRange(frame, len);
}

int FrameSubset::revLookupFrame(int frame) {
	FrameSubsetNode *pfsn_next, *pfsn = list.AtHead();
	int iSrcFrame = 0;

	while(pfsn_next = pfsn->NextFromHead()) {
		if (frame >= pfsn->start && frame < pfsn->start+pfsn->len)
			return iSrcFrame + (frame - pfsn->start);

		iSrcFrame += pfsn->len;

		pfsn = pfsn_next;
	}

	return -1;
}

int FrameSubset::lookupRange(int start, int& len) {
	int offset;
	FrameSubsetNode *pfsn = findNode(offset, start);

	if (!pfsn) return -1;

	len = pfsn->len - offset;
	return pfsn->start + offset;
}

void FrameSubset::deleteRange(int start, int len) {
	int offset;
	FrameSubsetNode *pfsn = findNode(offset, start), *pfsn_t;

	if (!pfsn)
		return;

	while((pfsn_t = pfsn->NextFromHead()) && len>0) {
		if (pfsn->len - offset > len) {
			if (offset) {
				FrameSubsetNode *pfsn2 = new FrameSubsetNode;

				if (!pfsn2) throw MyMemoryError();

				pfsn2->start	= pfsn->start + offset + len;
				pfsn2->len		= pfsn->len - (offset + len);
				pfsn->len		= offset;
				pfsn2->InsertAfter(pfsn);
			} else {
				pfsn->start += len;
				pfsn->len -= len;
			}

			break;
		} else {
			if (offset) {
				len -= pfsn->len - offset;
				pfsn->len = offset;
			} else {
				len -= pfsn->len;
				deleteNode(pfsn);
			}
		}

		offset = 0;
		pfsn = pfsn_t;
	}

#ifdef _DEBUG
	{
		FrameSubsetNode *pfsn;

		_RPT0(0,"Subset dump:\n");

		if (pfsn = getFirstFrame())
			do {
				_RPT2(0,"\tNode: start %ld, len %ld\n", pfsn->start, pfsn->len);
			} while(pfsn = getNextFrame(pfsn));

	}
#endif
}

void FrameSubset::clipToRange(int start, int len) {
	FrameSubsetNode *pfsn = list.AtHead(), *pfsn_t;

	if (!pfsn)
		return;

	while(pfsn_t = pfsn->NextFromHead()) {
		if (pfsn->start >= start) {
			if (pfsn->start > start+len)
				deleteNode(pfsn);
			else {
				if (pfsn->start+pfsn->len > start+len)
					pfsn->len = start+len - pfsn->start;

				if (pfsn->start < start) {
					pfsn->len += pfsn->start - start;
					pfsn->start = start;
				}
			}
		} else if (pfsn->start + pfsn->len >= start) {
			pfsn->len += pfsn->start - start;
			pfsn->start = start;
		} else
			deleteNode(pfsn);

		pfsn = pfsn_t;
	}
}

void FrameSubset::clip(int start, int len) {
	deleteRange(0, start);
	deleteRange(len, 0x7FFFFFFF);
}

void FrameSubset::offset(int off) {
	FrameSubsetNode *pfsn = list.AtHead(), *pfsn_t;

	if (!pfsn)
		return;

	while(pfsn_t = pfsn->NextFromHead()) {
		pfsn->start += off;

		pfsn = pfsn_t;
	}
}

void FrameSubset::deleteNode(FrameSubsetNode *pfsn) {
	pfsn->Remove();
	delete pfsn;

}

FrameSubsetNode *FrameSubset::findNode(int& poffset, int iDstFrame) {
	FrameSubsetNode *pfsn_next, *pfsn = list.AtHead();

	if (iDstFrame<0)
		return NULL;

	while((pfsn_next = pfsn->NextFromHead()) && iDstFrame>=0) {
		if (iDstFrame < pfsn->len) {
			poffset = iDstFrame;
			return pfsn;
		}

		iDstFrame -= pfsn->len;

		pfsn = pfsn_next;
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
class FrameSubsetClassVerifier {
public:
	void check(FrameSubset& fs, int test, ...) {
		va_list val;
		FrameSubsetNode *pfsn = fs.getFirstFrame();

		va_start(val, test);
		while(pfsn) {
			if (pfsn->start != va_arg(val, int))
				throw MyError("fail test #%dA", test);
			if (pfsn->len != va_arg(val, int))
				throw MyError("fail test #%dB", test);

			pfsn = fs.getNextFrame(pfsn);
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

				fs.addRangeMerge(10, 10);
				fs.addRangeMerge(30, 10);
				fs.addRangeMerge(50, 10);
				check(fs, 1, 10, 10, 30, 10, 50, 10, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10);
				fs.addRangeMerge(20, 10);
				fs.addRangeMerge(30, 10);
				check(fs, 2, 10, 30, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10);
				fs.addRangeMerge(20, 10);
				fs.addRangeMerge(50, 10);
				check(fs, 3, 10, 20, 50, 10, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10);
				fs.addRangeMerge(40, 10);
				fs.addRangeMerge(50, 10);
				check(fs, 4, 10, 10, 40, 20, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10);
				fs.addRangeMerge(15, 10);
				fs.addRangeMerge(50, 10);
				check(fs, 5, 10, 15, 50, 10, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10);
				fs.addRangeMerge(45, 10);
				fs.addRangeMerge(50, 10);
				check(fs, 6, 10, 10, 45, 15, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10);
				fs.addRangeMerge(15, 30);
				fs.addRangeMerge(50, 10);
				check(fs, 7, 10, 35, 50, 10, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10);
				fs.addRangeMerge(8, 48);
				fs.addRangeMerge(50, 10);
				check(fs, 8, 8, 52, -1);
			}

			{
				FrameSubset fs;

				fs.addRangeMerge(10, 10);
				fs.addRangeMerge(8, 100);
				fs.addRangeMerge(50, 10);
				check(fs, 9, 8, 100, -1);
			}


		} catch(MyError e) {
			e.post(NULL, "Class verify failed: FrameSubset");
		}
	}
} g_ClassVerifyFrameSubset;
#endif
