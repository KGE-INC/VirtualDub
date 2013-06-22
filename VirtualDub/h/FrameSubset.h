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

#include <list>
#include <vd2/system/list.h>

struct FrameSubsetNode {
public:
	int start, len;
	bool bMask;			// if set, all frames map to the previous frame

	FrameSubsetNode() {}
	FrameSubsetNode(int _s, int _l, bool _bMask) : start(_s), len(_l), bMask(_bMask) {}
};

class FrameSubset {
	typedef std::list<FrameSubsetNode> tTimeline;
public:
	typedef tTimeline::value_type			value_type;
	typedef tTimeline::reference			reference;
	typedef tTimeline::iterator				iterator;
	typedef tTimeline::const_reference		const_reference;
	typedef tTimeline::const_iterator		const_iterator;

	FrameSubset();
	FrameSubset(int length);
	~FrameSubset();

	void clear();
	void addFrom(FrameSubset&);

	int getTotalFrames();
	void addRange(int start, int len, bool bMask);
	void addRangeMerge(int start, int len, bool bMask);
	int lookupFrame(int frame) {
		bool b;

		return lookupFrame(frame, b);
	}
	int lookupFrame(int frame, bool& bMasked);
	int revLookupFrame(int frame, bool& bMasked);
	int lookupRange(int start, int& len) {
		bool b;

		return lookupRange(start, len, b);
	}
	int lookupRange(int start, int& len, bool& bMasked);
	void deleteInputRange(int start, int len);	// in source coordinates
	void deleteRange(int start, int len);	// in translated coordinates
	void setRange(int start, int len, bool bMask);	// translated coordinates
	void clip(int start, int len);
	void offset(int off);

	////////////////////

	bool					empty() const		{ return mTimeline.empty(); }
	iterator				begin()				{ return mTimeline.begin(); }
	const_iterator			begin() const		{ return mTimeline.begin(); }
	iterator				end()				{ return mTimeline.end(); }
	const_iterator			end() const			{ return mTimeline.end(); }
	reference				front()				{ return mTimeline.front(); }
	const_reference			front() const		{ return mTimeline.front(); }
	reference				back()				{ return mTimeline.back(); }
	const_reference			back() const		{ return mTimeline.back(); }

	void assign(const FrameSubset& src, int start, int len);
	iterator erase(iterator it) { return mTimeline.erase(it); }
	iterator erase(iterator it1, iterator it2) { return mTimeline.erase(it1, it2); }
	void insert(iterator it, const value_type& v) {
		FrameSubset tmp;
		tmp.mTimeline.push_back(v);
		insert(it, tmp);
	}
	void insert(iterator it, const FrameSubset& src);
	void insert(int insertionPoint, const FrameSubset& src);

	iterator findNode(int& poffset, int iDstFrame);

	void dump();

protected:
	tTimeline mTimeline;
};

#endif
