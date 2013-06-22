//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2009 Avery Lee
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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#include "stdafx.h"
#include "FilterFrameAllocatorManager.h"
#include "FilterFrameAllocatorProxy.h"
#include "FilterFrameAllocator.h"

struct VDFilterFrameAllocatorManager::ProxyEntrySort {
	bool operator()(ProxyEntry *x, ProxyEntry *y) const {
		return x->mMinSize < y->mMinSize;
	}
};

VDFilterFrameAllocatorManager::VDFilterFrameAllocatorManager() {
}

VDFilterFrameAllocatorManager::~VDFilterFrameAllocatorManager() {
	Shutdown();
}

void VDFilterFrameAllocatorManager::Shutdown() {
	while(!mProxies.empty()) {
		ProxyEntry& ent = mProxies.back();

		if (ent.mpAllocator)
			ent.mpAllocator->Release();

		mProxies.pop_back();
	}
}

void VDFilterFrameAllocatorManager::AddAllocatorProxy(VDFilterFrameAllocatorProxy *proxy) {
	ProxyEntry& ent = mProxies.push_back();

	ent.mpProxy = proxy;
	ent.mpAllocator = NULL;
	ent.mMinSize = 0;
	ent.mMaxSize = 0;
	ent.mpParent = NULL;
}

void VDFilterFrameAllocatorManager::AssignAllocators() {
	// Push down all size requirements through links.
	for(Proxies::iterator it(mProxies.begin()), itEnd(mProxies.end()); it != itEnd; ++it) {
		ProxyEntry& ent = *it;
		VDFilterFrameAllocatorProxy *proxy = ent.mpProxy;
		VDFilterFrameAllocatorProxy *link = NULL;
		VDFilterFrameAllocatorProxy *linkNext;
		
		while(linkNext = proxy->GetLink())
			link = linkNext;

		if (link) {
			proxy->Link(link);
			link->AddSizeRequirement(proxy->GetSizeRequirement());
		}
	}

	// Push size requirements back up through links. This has to be a separate pass
	// since multiple proxies may have the same link and we won't know the requirement
	// on the master.
	for(Proxies::iterator it(mProxies.begin()), itEnd(mProxies.end()); it != itEnd; ++it) {
		ProxyEntry& ent = *it;
		VDFilterFrameAllocatorProxy *proxy = ent.mpProxy;
		VDFilterFrameAllocatorProxy *link = proxy->GetLink();
		
		if (link)
			proxy->AddSizeRequirement(link->GetSizeRequirement());
	}

	for(Proxies::iterator it(mProxies.begin()), itEnd(mProxies.end()); it != itEnd; ++it) {
		ProxyEntry& ent = *it;

		uint32 req = ent.mpProxy->GetSizeRequirement();
		ent.mMinSize = req;
		ent.mMaxSize = req;
	}

	typedef vdfastvector<ProxyEntry *> ProxyRefs;
	ProxyRefs proxyRefs(mProxies.size());

	int n = mProxies.size();
	for(int i=0; i<n; ++i) {
		proxyRefs[i] = &mProxies[i];
	}

	std::sort(proxyRefs.begin(), proxyRefs.end(), ProxyEntrySort());

	for(;;) {
		int bestMerge = -1;
		float bestMergeRatio = 0.75f;

		for(int i=0; i<n-1; ++i) {
			ProxyEntry& ent1 = *proxyRefs[i];
			ProxyEntry& ent2 = *proxyRefs[i+1];

			VDASSERT(ent1.mMaxSize <= ent2.mMinSize);

			float mergeRatio = 0.f;

			if (ent2.mMaxSize)
				mergeRatio = (float)(ent2.mMaxSize - ent1.mMinSize) / (float)ent2.mMaxSize;

			if (mergeRatio > bestMergeRatio) {
				bestMerge = i;
				bestMergeRatio = mergeRatio;
			}
		}

		if (bestMerge < 0)
			break;

		ProxyEntry& dst = *proxyRefs[bestMerge];
		ProxyEntry& src = *proxyRefs[bestMerge+1];

		VDASSERT(src.mMaxSize >= dst.mMaxSize);

		src.mpParent = &dst;
		dst.mMaxSize = src.mMaxSize;

		proxyRefs.erase(proxyRefs.begin() + bestMerge + 1);
		--n;
	}

	// init allocators
	for(int i=0; i<n; ++i) {
		ProxyEntry& ent = *proxyRefs[i];

		ent.mpAllocator = new VDFilterFrameAllocator;
		ent.mpAllocator->AddRef();

		ent.mpAllocator->AddSizeRequirement(ent.mMaxSize);
		ent.mpAllocator->Init(0, 0x7fffffff);
	}

	// set allocators for all proxies
	for(Proxies::iterator it(mProxies.begin()), itEnd(mProxies.end()); it != itEnd; ++it) {
		ProxyEntry& ent = *it;
		ProxyEntry *src = &ent;

		while(src->mpParent)
			src = src->mpParent;

		ent.mpProxy->SetAllocator(src->mpAllocator);
	}
}
