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
#include "AVIOutputFile.h"
#include "AVIOutputWAV.h"
#include "AVIOutputImages.h"
#include "AVIOutputStriped.h"
#include "AVIOutputPreview.h"

///////////////////////////////////////////

extern "C" unsigned long version_num;

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

IVDMediaOutput *VDAVIOutputFileSystem::CreateSegment() {
	vdautoptr<IVDMediaOutputAVIFile> pOutput(VDCreateMediaOutputAVIFile());

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

	if (!mVideoFormat.empty()) {
		IVDMediaOutputStream *pVideoOut = pOutput->createVideoStream();
		pVideoOut->setFormat(&mVideoFormat[0], mVideoFormat.size());
		pVideoOut->setStreamInfo(mVideoStreamInfo);
	}

	if (!mAudioFormat.empty()) {
		IVDMediaOutputStream *pAudioOut = pOutput->createAudioStream();
		pAudioOut->setFormat(&mAudioFormat[0], mAudioFormat.size());
		pAudioOut->setStreamInfo(mAudioStreamInfo);
	}

	pOutput->setBuffering(mBufferSize, mBufferSize >> 2);
	pOutput->setInterleaved(mbInterleaved);

	char buf[80];

	sprintf(buf, "VirtualDub build %d/%s", version_num,
#ifdef _DEBUG
		"debug"
#else
		"release"
#endif
				);

	pOutput->setHiddenTag(buf);

	pOutput->init(s.c_str());

	return pOutput.release();
}

void VDAVIOutputFileSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast) {
	if (mSegmentDigits)
		static_cast<IVDMediaOutputAVIFile *>(pSegment)->setSegmentHintBlock(bLast, NULL, 1);
	pSegment->finalize();
	delete pSegment;
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

IVDMediaOutput *VDAVIOutputStripedSystem::CreateSegment() {
	vdautoptr<AVIOutputStriped> pFile(new AVIOutputStriped(mpStripeSystem));

	if (!pFile)
		throw MyMemoryError();

	pFile->set_1Gb_limit();
	return pFile.release();
}

void VDAVIOutputStripedSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast) {
	AVIOutputStriped *pFile = (AVIOutputStriped *)pSegment;
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
	, mBufferSize(1048576)
{
}

VDAVIOutputWAVSystem::~VDAVIOutputWAVSystem() {
}

void VDAVIOutputWAVSystem::SetBuffer(int bufferSize) {
	mBufferSize = bufferSize;
}

IVDMediaOutput *VDAVIOutputWAVSystem::CreateSegment() {
	vdautoptr<AVIOutputWAV> pOutput(new AVIOutputWAV);

	pOutput->createAudioStream()->setFormat(&mAudioFormat[0], mAudioFormat.size());

	pOutput->setBufferSize(mBufferSize);

	pOutput->init(mFilename.c_str());

	return pOutput.release();
}

void VDAVIOutputWAVSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast) {
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

IVDMediaOutput *VDAVIOutputImagesSystem::CreateSegment() {
	vdautoptr<AVIOutputImages> pOutput(new AVIOutputImages(mSegmentPrefix.c_str(), mSegmentSuffix.c_str(), mSegmentDigits, mFormat, mQuality));

	if (!mVideoFormat.empty())
		pOutput->createVideoStream()->setFormat(&mVideoFormat[0], mVideoFormat.size());

	if (!mAudioFormat.empty())
		pOutput->createAudioStream()->setFormat(&mAudioFormat[0], mAudioFormat.size());

	pOutput->init(NULL);

	return pOutput.release();
}

void VDAVIOutputImagesSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast) {
	AVIOutputImages *pFile = (AVIOutputImages *)pSegment;
	pFile->finalize();
	delete pFile;
}

void VDAVIOutputImagesSystem::SetFilenamePattern(const wchar_t *pszPrefix, const wchar_t *pszSuffix, int nMinimumDigits) {
	mSegmentPrefix		= pszPrefix;
	mSegmentSuffix		= pszSuffix;
	mSegmentDigits		= nMinimumDigits;
}

void VDAVIOutputImagesSystem::SetFormat(int format, int quality) {
	mFormat = format;
	mQuality = quality;
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

IVDMediaOutput *VDAVIOutputPreviewSystem::CreateSegment() {
	vdautoptr<AVIOutputPreview> pOutput(new AVIOutputPreview);

	if (!mVideoFormat.empty())
		pOutput->createVideoStream()->setFormat(&mVideoFormat[0], mVideoFormat.size());

	if (!mAudioFormat.empty())
		pOutput->createAudioStream()->setFormat(&mAudioFormat[0], mAudioFormat.size());

	pOutput->init(NULL);

	return pOutput.release();
}

void VDAVIOutputPreviewSystem::CloseSegment(IVDMediaOutput *pSegment, bool bLast) {
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
