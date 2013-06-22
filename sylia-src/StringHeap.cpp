#include <string.h>
#include <crtdbg.h>

#include "ScriptError.h"

#include "StringHeap.h"

CStringHeap::CStringHeap(long quads, long handles) {
	lpHeap = new StringDescriptor[lQuads = quads];
	lpHandleTable = new char *[lHandles = handles];

	if (lpHeap)
		Clear();
}

CStringHeap::~CStringHeap() {
	delete[] lpHeap;
	delete[] lpHandleTable;
}

void CStringHeap::Clear() {
	lpHeap[0].handle	= NULL;
	lpHeap[0].len		= lQuads-1;

	lQuadsFree			= lQuads;

	memset(lpHandleTable, 0, sizeof(char **)*lHandles);
}

void CStringHeap::Compact() {
	StringDescriptor sd_temp;
	long cur_used = 0, cur_free = 0;

	// find first free quad

	while(cur_free < lQuads) {
		if (!lpHeap[cur_free].handle)
			break;

		cur_free += (lpHeap[cur_free].len&0x0FFFFFFF)/8 + 2;
	}

	// heap is full, can't compact...

	if (cur_free >= lQuads)
		return;

	cur_used = cur_free + lpHeap[cur_free].len + 1;

	// only one free block (no compacting needed)?

	if (cur_used >= lQuads)
		return;

	// begin compaction

	while(cur_used < lQuads) {
		long lQuadSize;

		if (lpHeap[cur_used].handle) {

			// used block... swap it with the free block

			lQuadSize = (lpHeap[cur_used].len & 0x0FFFFFFF)/8 + 2;

			sd_temp = lpHeap[cur_free];
			memmove(&lpHeap[cur_free], &lpHeap[cur_used], lQuadSize * 8);
			*lpHeap[cur_free].handle = (char *)&lpHeap[cur_free+1];

			cur_free += lQuadSize;
			lpHeap[cur_free] = sd_temp;

		} else {

			// free block... merge it in

			lQuadSize = lpHeap[cur_used].len+1;

			lpHeap[cur_free].len += lQuadSize;
		}

		cur_used += lQuadSize;
	}
}

bool CStringHeap::_Allocate(char **handle, int len, bool temp) {
	long lCurQuad = 0;
	long lQuadsNeeded = len/8+1;

	_RPT2(0,"_Allocate(%ld) [%ld quads]\n", len, lQuadsNeeded);

	while(lCurQuad < lQuads) {
		if (lpHeap[lCurQuad].handle) {
			_RPT4(0,"\t%05ld used - %ld quads (%ld bytes) (%s)\n",
						lCurQuad,
						(lpHeap[lCurQuad].len & 0x0FFFFFFF)/8+2,
						(lpHeap[lCurQuad].len & 0x0FFFFFFF),
						lpHeap[lCurQuad].len & 0x80000000 ? "temp" : "normal");

			lCurQuad += (lpHeap[lCurQuad].len&0x0FFFFFFF)/8 + 2;
		} else if (lpHeap[lCurQuad].len >= lQuadsNeeded) {
			_RPT2(0,"\t%05ld free - %ld quads\n", lCurQuad, lpHeap[lCurQuad].len+1);

			if (lpHeap[lCurQuad].len > lQuadsNeeded) {
				lpHeap[lCurQuad + lQuadsNeeded + 1].handle	= NULL;
				lpHeap[lCurQuad + lQuadsNeeded + 1].len		= lpHeap[lCurQuad].len-1-lQuadsNeeded;
			}

			*handle = (char *)&lpHeap[lCurQuad+1];
			lpHeap[lCurQuad].handle	= handle;
			lpHeap[lCurQuad].len	= temp ? len | 0x80000000 : len;

			lQuadsFree -= lQuadsNeeded+1;
			return true;
		} else {
			_RPT2(0,"\t%05ld free - %ld quads\n", lCurQuad, lpHeap[lCurQuad].len+1);
			lCurQuad += lpHeap[lCurQuad].len+1;
		}
	}
	return false;
}

char **CStringHeap::Allocate(int len, bool temp) {
	long i;

	_RPT1(0,"Allocate: %ld quads free\n", lQuadsFree);

	for(i=0; i<lHandles; i++)
		if (!lpHandleTable[i]) break;

	if (i>=lHandles)
		SCRIPT_ERROR(OUT_OF_STRING_SPACE);

	if (lQuadsFree < len/8+2)
		SCRIPT_ERROR(OUT_OF_STRING_SPACE);

	if (_Allocate(&lpHandleTable[i],len,temp)) return &lpHandleTable[i];

	Compact();

	if (_Allocate(&lpHandleTable[i],len,temp)) return &lpHandleTable[i];

	_RPT0(0,"Sylia/StringHeap: should have been able to allocate, but wasn't\n");
	SCRIPT_ERROR(INTERNAL_ERROR);
}

void CStringHeap::Free(char **handle, bool temp_only) {
	char *s = *handle;
	StringDescriptor *sdp = (StringDescriptor *)s - 1;
	long lQuads;

	if (temp_only && !(sdp->len & 0x80000000))
		return;

	lQuads = (sdp->len&0x0FFFFFFF)/8 + 1;

	*sdp->handle = NULL;
	sdp->handle = NULL;
	sdp->len	= lQuads;

	lQuadsFree += lQuads;
}
