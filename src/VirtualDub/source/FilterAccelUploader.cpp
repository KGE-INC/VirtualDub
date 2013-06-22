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
#include "FilterAccelUploader.h"
#include "FilterAccelEngine.h"
#include "FilterFrameBufferAccel.h"

VDFilterAccelUploader::VDFilterAccelUploader() {
}

VDFilterAccelUploader::~VDFilterAccelUploader() {
}

void VDFilterAccelUploader::Init(VDFilterAccelEngine *engine, IVDFilterFrameSource *source, const VDPixmapLayout& outputLayout, const VDPixmapLayout *sourceLayoutOverride) {
	mpEngine = engine;
	mpSource = source;
	mSourceLayout = sourceLayoutOverride ? *sourceLayoutOverride : source->GetOutputLayout();
	mLayout = outputLayout;
	mAllocator.Clear();
	mAllocator.AddSizeRequirement((outputLayout.h << 16) + outputLayout.w);
	mAllocator.SetAccelerationRequirement(VDFilterFrameAllocatorProxy::kAccelModeUpload);
}

bool VDFilterAccelUploader::GetDirectMapping(sint64 outputFrame, sint64& sourceFrame, int& sourceIndex) {
	return mpSource->GetDirectMapping(outputFrame, sourceFrame, sourceIndex);
}

sint64 VDFilterAccelUploader::GetSourceFrame(sint64 outputFrame) {
	return outputFrame;
}

sint64 VDFilterAccelUploader::GetSymbolicFrame(sint64 outputFrame, IVDFilterFrameSource *source) {
	if (source == this)
		return outputFrame;

	return source->GetSymbolicFrame(outputFrame, source);
}

sint64 VDFilterAccelUploader::GetNearestUniqueFrame(sint64 outputFrame) {
	return mpSource->GetNearestUniqueFrame(outputFrame);
}

VDFilterAccelUploader::RunResult VDFilterAccelUploader::RunRequests() {
	vdrefptr<VDFilterFrameRequest> req;

	if (!GetNextRequest(~req))
		return kRunResult_Idle;

	VDFilterFrameBuffer *srcbuf = req->GetSource(0);
	if (!srcbuf) {
		req->MarkComplete(false);
		CompleteRequest(req, false);
		return kRunResult_Running;
	}

	if (!AllocateRequestBuffer(req)) {
		req->MarkComplete(false);
		CompleteRequest(req, false);
		return kRunResult_Running;
	}

	VDFilterFrameBufferAccel *dstbuf = static_cast<VDFilterFrameBufferAccel *>(req->GetResultBuffer());
	mpEngine->Upload(dstbuf, srcbuf, mSourceLayout);

	req->MarkComplete(true);
	CompleteRequest(req, true);
	return kRunResult_Running;
}

bool VDFilterAccelUploader::InitNewRequest(VDFilterFrameRequest *req, sint64 outputFrame, bool writable) {
	vdrefptr<IVDFilterFrameClientRequest> creq;
	if (!mpSource->CreateRequest(outputFrame, false, ~creq))
		return false;

	req->SetSourceCount(1);
	req->SetSourceRequest(0, creq);
	return true;
}
