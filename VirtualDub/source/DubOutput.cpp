//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2003 Avery Lee
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
#include "DubOutput.h"
#include <vd2/system/error.h>
#include "AVIOutput.h"
#include "AVIOutputWAV.h"
#include "AVIOutputImages.h"
#include "AVIOutputStriped.h"
#include "AVIOutputPreview.h"

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputFileSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputFileSystem::VDAVIOutputFileSystem()
	: mbAllowCaching(false)
	, mbAllowIndexing(true)
	, mbUse1GBLimit(false)
	, mCurrentSegment(0)
{
}

VDAVIOutputFileSystem::~VDAVIOutputFileSystem() {
}

void VDAVIOutputFileSystem::SetCaching(bool bAllowOSCaching) {
	mbAllowCaching = bAllowOSCaching;
}

void VDAVIOutputFileSystem::SetIndexing(bool bAllowHierarchicalExtensions) {
	mbAllowIndexing = bAllowHierarchicalExtensions;
}

void VDAVIOutputFileSystem::Set1GBLimit(bool bUse1GBLimit) {
	mbUse1GBLimit = bUse1GBLimit;
}

void VDAVIOutputFileSystem::SetBuffer(int bufferSize) {
	mBufferSize = bufferSize;
}

AVIOutput *VDAVIOutputFileSystem::CreateSegment() {
	vdautoptr<AVIOutputFile> pOutput(new AVIOutputFile);

	if (!mbAllowCaching)
		pOutput->disable_os_caching();

	if (!mbAllowIndexing)
		pOutput->disable_extended_avi();

	VDStringW s(mSegmentBaseName);

	if (mSegmentDigits) {
		s += VDFastTextPrintfW(L".%0*d", mSegmentDigits, mCurrentSegment++);
		VDFastTextFree();
		s += mSegmentExt;

		pOutput->setSegmentHintBlock(true, NULL, 1);
	}

	pOutput->initOutputStreams();

	if (!mVideoFormat.empty()) {
		memcpy(pOutput->videoOut->allocFormat(mVideoFormat.size()), &mVideoFormat[0], mVideoFormat.size());
		pOutput->videoOut->setCompressed(TRUE);
		pOutput->videoOut->streamInfo = mVideoStreamInfo;
	}

	if (!mAudioFormat.empty()) {
		memcpy(pOutput->audioOut->allocFormat(mAudioFormat.size()), &mAudioFormat[0], mAudioFormat.size());
		pOutput->audioOut->streamInfo = mAudioStreamInfo;
	}

	pOutput->init(VDTextWToA(s).c_str(), !mVideoFormat.empty(), !mAudioFormat.empty(), mBufferSize, mbInterleaved);

	return pOutput.release();
}

void VDAVIOutputFileSystem::CloseSegment(AVIOutput *pSegment, bool bLast) {
	AVIOutputFile *pFile = (AVIOutputFile *)pSegment;
	if (mSegmentDigits)
		pFile->setSegmentHintBlock(bLast, NULL, 1);
	pFile->finalize();
	delete pFile;
}

void VDAVIOutputFileSystem::SetFilename(const wchar_t *pszFilename) {
	mSegmentBaseName	= pszFilename;
	mSegmentDigits		= 0;
}

void VDAVIOutputFileSystem::SetFilenamePattern(const wchar_t *pszFilename, const wchar_t *pszExt, int nMinimumDigits) {
	mSegmentBaseName	= pszFilename;
	mSegmentExt			= pszExt;
	mSegmentDigits		= nMinimumDigits;
}

void VDAVIOutputFileSystem::SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat) {
	mVideoStreamInfo = asi;
	mVideoFormat.resize(cbFormat);
	memcpy(&mVideoFormat[0], pFormat, cbFormat);
}

void VDAVIOutputFileSystem::SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved) {
	mAudioStreamInfo = asi;
	mAudioFormat.resize(cbFormat);
	memcpy(&mAudioFormat[0], pFormat, cbFormat);
	mbInterleaved = bInterleaved;
}

bool VDAVIOutputFileSystem::AcceptsVideo() {
	return true;
}

bool VDAVIOutputFileSystem::AcceptsAudio() {
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputStripedSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputStripedSystem::VDAVIOutputStripedSystem(const wchar_t *filename)
	: mbUse1GBLimit(false)
	, mpStripeSystem(new AVIStripeSystem(VDTextWToA(filename, -1).c_str()))
{
}

VDAVIOutputStripedSystem::~VDAVIOutputStripedSystem() {
}

void VDAVIOutputStripedSystem::Set1GBLimit(bool bUse1GBLimit) {
	mbUse1GBLimit = bUse1GBLimit;
}

AVIOutput *VDAVIOutputStripedSystem::CreateSegment() {
	vdautoptr<AVIOutputStriped> pFile(new AVIOutputStriped(mpStripeSystem));

	if (!pFile)
		throw MyMemoryError();

	pFile->set_1Gb_limit();
	return pFile.release();
}

void VDAVIOutputStripedSystem::CloseSegment(AVIOutput *pSegment, bool bLast) {
	AVIOutputFile *pFile = (AVIOutputFile *)pSegment;
	pFile->setSegmentHintBlock(bLast, NULL, 1);
	pFile->finalize();
	delete pFile;
}

void VDAVIOutputStripedSystem::SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat) {
	mVideoStreamInfo = asi;
	mVideoFormat.resize(cbFormat);
	memcpy(&mVideoFormat[0], pFormat, cbFormat);
}

void VDAVIOutputStripedSystem::SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved) {
	mAudioStreamInfo = asi;
	mAudioFormat.resize(cbFormat);
	memcpy(&mAudioFormat[0], pFormat, cbFormat);
}

bool VDAVIOutputStripedSystem::AcceptsVideo() {
	return true;
}

bool VDAVIOutputStripedSystem::AcceptsAudio() {
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputWAVSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputWAVSystem::VDAVIOutputWAVSystem(const wchar_t *pszFilename)
	: mFilename(pszFilename)
{
}

VDAVIOutputWAVSystem::~VDAVIOutputWAVSystem() {
}

AVIOutput *VDAVIOutputWAVSystem::CreateSegment() {
	vdautoptr<AVIOutputWAV> pOutput(new AVIOutputWAV);

	pOutput->initOutputStreams();

	memcpy(pOutput->audioOut->allocFormat(mAudioFormat.size()), &mAudioFormat[0], mAudioFormat.size());

	pOutput->init(VDTextWToA(mFilename).c_str(), FALSE, !mAudioFormat.empty(), 65536, false);

	return pOutput.release();
}

void VDAVIOutputWAVSystem::CloseSegment(AVIOutput *pSegment, bool bLast) {
	AVIOutputWAV *pFile = (AVIOutputWAV *)pSegment;
	pFile->finalize();
	delete pFile;
}

void VDAVIOutputWAVSystem::SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat) {
}

void VDAVIOutputWAVSystem::SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved) {
	mAudioStreamInfo = asi;
	mAudioFormat.resize(cbFormat);
	memcpy(&mAudioFormat[0], pFormat, cbFormat);
}

bool VDAVIOutputWAVSystem::AcceptsVideo() {
	return false;
}

bool VDAVIOutputWAVSystem::AcceptsAudio() {
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputImagesSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputImagesSystem::VDAVIOutputImagesSystem()
{
}

VDAVIOutputImagesSystem::~VDAVIOutputImagesSystem() {
}

AVIOutput *VDAVIOutputImagesSystem::CreateSegment() {
	vdautoptr<AVIOutputImages> pOutput(new AVIOutputImages(VDTextWToA(mSegmentPrefix).c_str(), VDTextWToA(mSegmentSuffix).c_str(), mSegmentDigits, mFormat));

	pOutput->initOutputStreams();

	if (!mVideoFormat.empty()) {
		memcpy(pOutput->videoOut->allocFormat(mVideoFormat.size()), &mVideoFormat[0], mVideoFormat.size());
		pOutput->videoOut->setCompressed(TRUE);
	}

	if (!mAudioFormat.empty())
		memcpy(pOutput->audioOut->allocFormat(mAudioFormat.size()), &mAudioFormat[0], mAudioFormat.size());

	pOutput->init(NULL, !mVideoFormat.empty(), !mAudioFormat.empty(), 0, false);

	return pOutput.release();
}

void VDAVIOutputImagesSystem::CloseSegment(AVIOutput *pSegment, bool bLast) {
	AVIOutputImages *pFile = (AVIOutputImages *)pSegment;
	pFile->finalize();
	delete pFile;
}

void VDAVIOutputImagesSystem::SetFilenamePattern(const wchar_t *pszPrefix, const wchar_t *pszSuffix, int nMinimumDigits) {
	mSegmentPrefix		= pszPrefix;
	mSegmentSuffix		= pszSuffix;
	mSegmentDigits		= nMinimumDigits;
}

void VDAVIOutputImagesSystem::SetFormat(int format) {
	mFormat = format;
}

void VDAVIOutputImagesSystem::SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat) {
	mVideoStreamInfo = asi;
	mVideoFormat.resize(cbFormat);
	memcpy(&mVideoFormat[0], pFormat, cbFormat);
}

void VDAVIOutputImagesSystem::SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved) {
	mAudioStreamInfo = asi;
	mAudioFormat.resize(cbFormat);
	memcpy(&mAudioFormat[0], pFormat, cbFormat);
}

bool VDAVIOutputImagesSystem::AcceptsVideo() {
	return true;
}

bool VDAVIOutputImagesSystem::AcceptsAudio() {
	return false;
}

///////////////////////////////////////////////////////////////////////////
//
//	VDAVIOutputPreviewSystem
//
///////////////////////////////////////////////////////////////////////////

VDAVIOutputPreviewSystem::VDAVIOutputPreviewSystem()
{
}

VDAVIOutputPreviewSystem::~VDAVIOutputPreviewSystem() {
}

AVIOutput *VDAVIOutputPreviewSystem::CreateSegment() {
	vdautoptr<AVIOutputPreview> pOutput(new AVIOutputPreview);

	pOutput->initOutputStreams();

	if (!mVideoFormat.empty()) {
		memcpy(pOutput->videoOut->allocFormat(mVideoFormat.size()), &mVideoFormat[0], mVideoFormat.size());
		pOutput->videoOut->setCompressed(TRUE);
	}

	if (!mAudioFormat.empty())
		memcpy(pOutput->audioOut->allocFormat(mAudioFormat.size()), &mAudioFormat[0], mAudioFormat.size());

	pOutput->init(NULL, !mVideoFormat.empty(), !mAudioFormat.empty(), 0, false);

	return pOutput.release();
}

void VDAVIOutputPreviewSystem::CloseSegment(AVIOutput *pSegment, bool bLast) {
	AVIOutputPreview *pFile = (AVIOutputPreview *)pSegment;
	pFile->finalize();
	delete pFile;
}

void VDAVIOutputPreviewSystem::SetVideo(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat) {
	mVideoStreamInfo = asi;
	mVideoFormat.resize(cbFormat);
	memcpy(&mVideoFormat[0], pFormat, cbFormat);
}

void VDAVIOutputPreviewSystem::SetAudio(const AVIStreamHeader_fixed& asi, const void *pFormat, int cbFormat, bool bInterleaved) {
	mAudioStreamInfo = asi;
	mAudioFormat.resize(cbFormat);
	memcpy(&mAudioFormat[0], pFormat, cbFormat);
}

bool VDAVIOutputPreviewSystem::AcceptsVideo() {
	return true;
}

bool VDAVIOutputPreviewSystem::AcceptsAudio() {
	return true;
}
