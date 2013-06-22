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

#ifndef f_FRAMESUBSET_H
#define f_FRAMESUBSET_H

#include "List.h"

class FrameSubsetNode : public ListNode2<FrameSubsetNode> {
friend class FrameSubset;
public:
	int start, len;

	FrameSubsetNode() {};
	FrameSubsetNode(int _s, int _l) : start(_s), len(_l) {}
};

class FrameSubset {
public:
	FrameSubset();
	FrameSubset(int length);
	~FrameSubset();

	void clear();
	void addFrom(FrameSubset&);

	int getTotalFrames();
	void addRange(int start, int len);
	void addRangeMerge(int start, int len);
	int lookupFrame(int frame);
	int revLookupFrame(int frame);
	int lookupRange(int start, int& len);
	void deleteRange(int start, int len);
	void clipToRange(int start, int len);
	void clip(int start, int len);
	void offset(int off);
	FrameSubsetNode *getFirstFrame() {
		FrameSubsetNode *fsn = list.AtHead();

		if (fsn->NextFromHead())
			return fsn;
		else
			return 0;
	}
	FrameSubsetNode *getNextFrame(FrameSubsetNode *fsn) {
		fsn = fsn->NextFromHead();

		if (fsn->NextFromHead())
			return fsn;
		else
			return 0;
	}

private:
	List2<FrameSubsetNode> list;

	void deleteNode(FrameSubsetNode *pfsn);
	FrameSubsetNode *findNode(int& poffset, int iDstFrame);
};

#endif
