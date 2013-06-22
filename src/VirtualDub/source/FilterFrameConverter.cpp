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
//	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#include "stdafx.h"
#include <vd2/Kasumi/pixmaputils.h>
#include "FilterFrameConverter.h"

VDFilterFrameConverter::VDFilterFrameConverter() {
}

VDFilterFrameConverter::~VDFilterFrameConverter() {
}

void VDFilterFrameConverter::Init(IVDFilterFrameSource *source, const VDPixmapLayout& outputLayout, const VDPixmapLayout *sourceLayoutOverride) {
	mpSource = source;
	mSourceLayout = sourceLayoutOverride ? *sourceLayoutOverride : source->GetOutputLayout();
	SetOutputLayout(outputLayout);
	mpBlitter = VDPixmapCreateBlitter(mLayout, mSourceLayout);
}

bool VDFilterFrameConverter::GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex) {
	return mpSource->GetDirectMapping(outputFrame, sourceFrame, sourceIndex);
}

sint64 VDFilterFrameConverter::GetSourceFrame(sint64 outputFrame) {
	return outputFrame;
}

sint64 VDFilterFrameConverter::GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source) {
	if (source == this)
		return outputFrame;

	return source->GetSymbolicFrame(outputFrame, source);
}

sint64 VDFilterFrameConverter::GetNearestUniqueFrame(sint64 outputFrame) {
	return mpSource->GetNearestUniqueFrame(outputFrame);
}

VDFilterFrameConverter::RunResult VDFilterFrameConverter::RunRequests() {
	if (!mpRequest) {
		if (!GetNextRequest(~mpRequest))
			return kRunResult_Idle;
	} else {
		if (!mpRequest->IsActive()) {
			mpRequest->MarkComplete(false);
			CompleteRequest(mpRequest, false);
			mpRequest = NULL;
			return kRunResult_Running;
		}
	}

	VDFilterFrameBuffer *srcbuf = mpRequest->GetSource(0);
	if (!srcbuf) {
		mpRequest->MarkComplete(false);
		CompleteRequest(mpRequest, false);
		mpRequest = NULL;
		return kRunResult_Running;
	}

	if (!AllocateRequestBuffer(mpRequest)) {
		mpRequest->MarkComplete(false);
		CompleteRequest(mpRequest, false);
		mpRequest = NULL;
		return kRunResult_Running;
	}

	VDFilterFrameBuffer *dstbuf = mpRequest->GetResultBuffer();
	const VDPixmap& pxsrc = VDPixmapFromLayout(mSourceLayout, (void *)srcbuf->LockRead());
	const VDPixmap& pxdst = VDPixmapFromLayout(mLayout, dstbuf->LockWrite());

	mpBlitter->Blit(pxdst, pxsrc);

	dstbuf->Unlock();
	srcbuf->Unlock();

	mpRequest->MarkComplete(true);
	CompleteRequest(mpRequest, true);
	mpRequest = NULL;
	return kRunResult_Running;
}

bool VDFilterFrameConverter::InitNewRequest(VDFilterFrameRequest *req, sint64 outputFrame, bool writable) {
	vdrefptr<IVDFilterFrameClientRequest> creq;
	if (!mpSource->CreateRequest(outputFrame, false, ~creq))
		return false;

	req->SetSourceCount(1);
	req->SetSourceRequest(0, creq);
	return true;
}
