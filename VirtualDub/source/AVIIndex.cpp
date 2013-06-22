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

#include <vd2/system/error.h>

#include "AVIIndex.h"

///////////////////////////////////////////////////////////////////////////

class AVIIndexChainNode {
public:
	enum { ENTS=2048 };

	AVIIndexChainNode *next;

	AVIIndexEntry2 ient[ENTS];
	int num_ents;

	AVIIndexChainNode() {
		num_ents = 0;
		next = NULL;
	}

	bool add(FOURCC ckid, __int64 pos, long size, bool is_keyframe) {
		if (num_ents < ENTS) {
			ient[num_ents].ckid = ckid;
			ient[num_ents].pos = pos;
			ient[num_ents].size = is_keyframe ? size : 0x80000000+size;
			++num_ents;
			return true;
		}
		return false;
	}

	void put(AVIINDEXENTRY *&avieptr) {
		int i;

		for(i=0; i<num_ents; i++) {
			avieptr->ckid			= ient[i].ckid;
			avieptr->dwFlags		= ient[i].size & 0x80000000 ? 0 : AVIIF_KEYFRAME;
			avieptr->dwChunkOffset	= ient[i].pos;
			avieptr->dwChunkLength	= ient[i].size & 0x7FFFFFFF;

			++avieptr;
		}
	}

	void put(AVIIndexEntry2 *&avie2ptr) {
		int i;

		for(i=0; i<num_ents; i++)
			*avie2ptr++ = ient[i];
	}

	void put(AVIIndexEntry3 *&avie3ptr, __int64 offset) {
		int i;

		for(i=0; i<num_ents; i++) {
			avie3ptr->dwSizeKeyframe	= ient[i].size;
			avie3ptr->dwOffset			= (DWORD)(ient[i].pos - offset);

			++avie3ptr;
		}
	}
};

///////////////////////////////////////////////////////////////////////////

AVIIndexChain::AVIIndexChain() {
	head = tail = NULL;
	total_ents = 0;
}

void AVIIndexChain::delete_chain() {
	AVIIndexChainNode *aicn = head,*aicn2;

	while(aicn) {
		aicn2 = aicn->next;
		delete aicn;
		aicn = aicn2;
	}

	head = tail = NULL;
}

AVIIndexChain::~AVIIndexChain() {
	delete_chain();
}

void AVIIndexChain::add(AVIINDEXENTRY *avie) {
	if (!tail || !tail->add(avie->ckid, avie->dwChunkOffset, avie->dwChunkLength, !!(avie->dwFlags & AVIIF_KEYFRAME))) {
		AVIIndexChainNode *aicn = new AVIIndexChainNode();

		if (tail) tail->next = aicn; else head=aicn;
		tail = aicn;

		tail->add(avie->ckid, avie->dwChunkOffset, avie->dwChunkLength, !!(avie->dwFlags & AVIIF_KEYFRAME));
	}

	++total_ents;
}

void AVIIndexChain::add(AVIIndexEntry2 *avie2) {
	add(avie2->ckid, avie2->pos, avie2->size & 0x7FFFFFFF, !!(avie2->size & 0x80000000));
}

void AVIIndexChain::add(FOURCC ckid, __int64 pos, long size, bool is_keyframe) {
	if (!tail || !tail->add(ckid, pos, size, is_keyframe)) {
		AVIIndexChainNode *aicn = new AVIIndexChainNode();

		if (tail) tail->next = aicn; else head=aicn;
		tail = aicn;

		tail->add(ckid, pos, size, is_keyframe);
	}

	++total_ents;
}

void AVIIndexChain::put(AVIINDEXENTRY *avietbl) {
	AVIIndexChainNode *aicn = head;

	while(aicn) {
		aicn->put(avietbl);
		aicn=aicn->next;
	}

	delete_chain();
}

void AVIIndexChain::put(AVIIndexEntry2 *avie2tbl) {
	AVIIndexChainNode *aicn = head;

	while(aicn) {
		aicn->put(avie2tbl);
		aicn=aicn->next;
	}

	delete_chain();
}

void AVIIndexChain::put(AVIIndexEntry3 *avie3tbl, __int64 offset) {
	AVIIndexChainNode *aicn = head;

	while(aicn) {
		aicn->put(avie3tbl, offset);
		aicn=aicn->next;
	}

	delete_chain();
}

AVIIndex::AVIIndex() {
	index = NULL;
	index2 = NULL;
	index3 = NULL;
}

AVIIndex::~AVIIndex() {
	delete[] index;
	delete[] index2;
	delete[] index3;
}

void AVIIndex::makeIndex() {
	if (!allocateIndex(total_ents))
		throw MyMemoryError();

	put(indexPtr());
}

void AVIIndex::makeIndex2() {
	if (!allocateIndex2(total_ents))
		throw MyMemoryError();

	put(index2Ptr());
}

void AVIIndex::clear() {
	delete_chain();
	delete[] index;
	delete[] index2;
	delete[] index3;
	index = NULL;
	index2 = NULL;
	index3 = NULL;
	total_ents = 0;
}
