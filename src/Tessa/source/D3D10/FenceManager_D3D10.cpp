#include "stdafx.h"
#include <d3d10.h>
#include "D3D10/FenceManager_D3D10.h"

VDTFenceManagerD3D10::VDTFenceManagerD3D10()
: mpD3DDevice(NULL)
{
}

VDTFenceManagerD3D10::~VDTFenceManagerD3D10() {
}

void VDTFenceManagerD3D10::Init(ID3D10Device *dev) {
	D3D10_QUERY_DESC desc;
	desc.Query = D3D10_QUERY_EVENT;
	desc.MiscFlags = 0;
	HRESULT hr = dev->CreateQuery(&desc, NULL);

	mpD3DDevice = dev;
	mpD3DDevice->AddRef();

	mFirstFenceId = 1;
	mNextFenceId = 1;
}

void VDTFenceManagerD3D10::Shutdown() {
	FlushDefaultResources();

	if (mpD3DDevice) {
		mpD3DDevice->Release();
		mpD3DDevice = NULL;
	}
}

void VDTFenceManagerD3D10::FlushDefaultResources() {
	while(!mIdleQueries.empty()) {
		ID3D10Query *q = mIdleQueries.back();
		mIdleQueries.pop_back();

		q->Release();
	}

	while(!mActiveQueries.empty()) {
		ID3D10Query *q = mActiveQueries.back();
		mActiveQueries.pop_back();

		if (q)
			q->Release();
	}

	mFirstFenceId = mNextFenceId;
}

uint32 VDTFenceManagerD3D10::InsertFence() {
	ID3D10Query *q = NULL;
	HRESULT hr;

	if (mIdleQueries.empty()) {
		D3D10_QUERY_DESC desc;
		desc.Query = D3D10_QUERY_EVENT;
		desc.MiscFlags = 0;
		hr = mpD3DDevice->CreateQuery(&desc, &q);
		if (FAILED(hr))
			q = NULL;
	} else {
		q = mIdleQueries.back();
		mIdleQueries.pop_back();
	}

	if (q)
		q->End();

	mActiveQueries.push_back(q);
	return mNextFenceId++;
}

bool VDTFenceManagerD3D10::CheckFence(uint32 fenceId) {
	uint32 distance = mNextFenceId - fenceId;
	if (distance > mActiveQueries.size())
		return true;

	while(!mActiveQueries.empty()) {
		ID3D10Query *q = mActiveQueries.front();

		if (q) {
			BOOL data = FALSE;
			HRESULT hr = q->GetData(&data, sizeof data, 0);
			if (hr == S_FALSE)
				break;
		}

		if (q)
			mIdleQueries.push_back(q);

		mActiveQueries.pop_front();
		++mFirstFenceId;
	}

	return distance > mActiveQueries.size();
}
