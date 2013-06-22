//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
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

#ifndef f_CAPTURE_H
#define f_CAPTURE_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/vdstring.h>
#include <vd2/system/vectors.h>
#include <vd2/system/refcount.h>
#include <vd2/Riza/capdriver.h>
#include <windows.h>
#include <mmsystem.h>
#include "capfilter.h"

struct VDPixmap;

#define	CAPSTOP_TIME			(0x00000001L)
#define	CAPSTOP_FILESIZE		(0x00000002L)
#define	CAPSTOP_DISKSPACE		(0x00000004L)
#define	CAPSTOP_DROPRATE		(0x00000008L)

struct VDCaptureStopPrefs {
	long		fEnableFlags;
	long		lTimeLimit;
	long		lSizeLimit;
	long		lDiskSpaceThreshold;
	long		lMaxDropRate;
};

struct VDCaptureFilterSetup {
	vdrect32	mCropRect;
	IVDCaptureFilterSystem::FilterMode mVertSquashMode;
	int			mNRThreshold;		// default 16

	bool		mbEnableRGBFiltering;
	bool		mbEnableNoiseReduction;
	bool		mbEnableLumaSquishBlack;
	bool		mbEnableLumaSquishWhite;
	bool		mbEnableFieldSwap;
};

struct VDCaptureDiskSettings {
	sint32		mDiskChunkSize;
	sint32		mDiskChunkCount;
	bool		mbDisableWriteCache;
};

struct VDCaptureTimingSetup {
	enum SyncMode {
		kSyncNone,
		kSyncVideoToAudio,
		kSyncAudioToVideo,
		kSyncModeCount
	};

	SyncMode	mSyncMode;
	bool		mbCorrectVideoTiming;
	bool		mbResyncWithIntegratedAudio;
	bool		mbAllowEarlyDrops;
	bool		mbAllowLateInserts;
	int			mInsertLimit;
};

struct VDCaptureStatus {
	sint32		mFramesCaptured;
	sint32		mFramesDropped;
	sint32		mTotalJitter;
	sint32		mTotalDisp;
	sint64		mTotalVideoSize;
	sint64		mTotalAudioSize;
	sint32		mCurrentSegment;
	uint32		mElapsedTimeMS;
	sint64		mDiskFreeSpace;

	sint32		mVideoTimingAdjustMS;
	float		mVideoRateScale;
	float		mAudioResamplingRate;
	float		mAudioLatency;

	sint32		mVideoFirstFrameTimeMS;
	sint32		mVideoLastFrameTimeMS;
	sint32		mAudioFirstFrameTimeMS;
	sint32		mAudioLastFrameTimeMS;
	sint32		mAudioFirstSize;
	sint64		mTotalAudioDataSize;

	double		mActualAudioHz;
};

class VDINTERFACE IVDCaptureProjectCallback {
public:
	virtual void UICaptureDriversUpdated() = 0;
	virtual void UICaptureDriverDisconnecting(int driver) = 0;
	virtual void UICaptureDriverChanging(int driver) = 0;
	virtual void UICaptureDriverChanged(int driver) = 0;
	virtual void UICaptureAudioDriversUpdated() = 0;
	virtual void UICaptureAudioDriverChanged(int driver) = 0;
	virtual void UICaptureAudioSourceChanged(int source) = 0;
	virtual void UICaptureAudioInputChanged(int input) = 0;
	virtual void UICaptureFileUpdated() = 0;
	virtual void UICaptureAudioFormatUpdated() = 0;
	virtual void UICaptureVideoFormatUpdated() = 0;
	virtual void UICaptureVideoSourceChanged(int source) = 0;
	virtual void UICaptureTunerChannelChanged(int ch, bool init) = 0;
	virtual void UICaptureParmsUpdated() = 0;
	virtual bool UICaptureAnalyzeBegin(const VDPixmap& format) = 0;
	virtual void UICaptureAnalyzeFrame(const VDPixmap& format) = 0;
	virtual void UICaptureAnalyzeEnd() = 0;
	virtual void UICaptureVideoHistoBegin() = 0;
	virtual void UICaptureVideoHisto(const float data[256]) = 0;
	virtual void UICaptureVideoHistoEnd() = 0;
	virtual void UICaptureAudioPeaksUpdated(float l, float r) = 0;
	virtual void UICaptureStart() = 0;
	virtual bool UICapturePreroll() = 0;
	virtual void UICaptureStatusUpdated(VDCaptureStatus&) = 0;
	virtual void UICaptureEnd(bool success) = 0;
};

class VDCaptureProjectBaseCallback : public IVDCaptureProjectCallback {
public:
	virtual void UICaptureDriversUpdated();
	virtual void UICaptureDriverDisconnecting(int driver);
	virtual void UICaptureDriverChanging(int driver);
	virtual void UICaptureDriverChanged(int driver);
	virtual void UICaptureAudioDriversUpdated();
	virtual void UICaptureAudioDriverChanged(int driver);
	virtual void UICaptureAudioSourceChanged(int input);
	virtual void UICaptureAudioInputChanged(int input);
	virtual void UICaptureFileUpdated();
	virtual void UICaptureAudioFormatUpdated();
	virtual void UICaptureVideoFormatUpdated();
	virtual void UICaptureVideoSourceChanged(int source);
	virtual void UICaptureTunerChannelChanged(int ch, bool init);
	virtual void UICaptureParmsUpdated();
	virtual bool UICaptureAnalyzeBegin(const VDPixmap& format);
	virtual void UICaptureAnalyzeFrame(const VDPixmap& format);
	virtual void UICaptureAnalyzeEnd();
	virtual void UICaptureVideoHistoBegin();
	virtual void UICaptureVideoHisto(const float data[256]);
	virtual void UICaptureVideoHistoEnd();
	virtual void UICaptureAudioPeaksUpdated(float l, float r);
	virtual void UICaptureStart();
	virtual bool UICapturePreroll();
	virtual void UICaptureStatusUpdated(VDCaptureStatus&);
	virtual void UICaptureEnd(bool success);
};

class VDINTERFACE IVDCaptureProject : public IVDRefCount {
public:
	virtual ~IVDCaptureProject() {}

	virtual bool	Attach(VDGUIHandle hwnd) = 0;
	virtual void	Detach() = 0;

	virtual IVDCaptureProjectCallback *GetCallback() = 0;
	virtual void	SetCallback(IVDCaptureProjectCallback *pCB) = 0;

	virtual void	LockUpdates() = 0;
	virtual void	UnlockUpdates() = 0;

	virtual bool	IsHardwareDisplayAvailable() = 0;
	virtual void	SetDisplayMode(nsVDCapture::DisplayMode mode) = 0;
	virtual nsVDCapture::DisplayMode	GetDisplayMode() = 0;
	virtual void	SetDisplayChromaKey(int key) = 0;
	virtual void	SetDisplayRect(const vdrect32& r) = 0;
	virtual vdrect32	GetDisplayRectAbsolute() = 0;
	virtual void	SetDisplayVisibility(bool vis) = 0;

	virtual void	SetVideoFrameTransferEnabled(bool ena) = 0;
	virtual bool	IsVideoFrameTransferEnabled() = 0;

	virtual void	SetVideoHistogramEnabled(bool ena) = 0;
	virtual bool	IsVideoHistogramEnabled() = 0;

	virtual void	SetFrameTime(sint32 lFrameTime) = 0;
	virtual sint32	GetFrameTime() = 0;

	virtual void	SetTimingSetup(const VDCaptureTimingSetup& syncSetup) = 0;
	virtual const VDCaptureTimingSetup&	GetTimingSetup() = 0;

	virtual void	SetLoggingEnabled(bool ena) = 0;
	virtual bool	IsLoggingEnabled() = 0;
	virtual bool	IsLogAvailable() = 0;
	virtual void	SaveLog(const wchar_t *path) = 0;

	virtual bool	SetTunerChannel(int channel) = 0;
	virtual int		GetTunerChannel() = 0;
	virtual bool	GetTunerChannelRange(int& minChannel, int& maxChannel) = 0;

	virtual int		GetAudioDeviceCount() = 0;
	virtual const wchar_t *GetAudioDeviceName(int idx) = 0;
	virtual void	SetAudioDevice(int idx) = 0;
	virtual int		GetAudioDeviceIndex() = 0;
	virtual int		GetAudioDeviceByName(const wchar_t *name) = 0;

	virtual int		GetVideoSourceCount() = 0;
	virtual const wchar_t *GetVideoSourceName(int idx) = 0;
	virtual bool	SetVideoSource(int idx) = 0;
	virtual int		GetVideoSourceIndex() = 0;
	virtual int		GetVideoSourceByName(const wchar_t *name) = 0;

	virtual int		GetAudioSourceCount() = 0;
	virtual const wchar_t *GetAudioSourceName(int idx) = 0;
	virtual bool	SetAudioSource(int idx) = 0;
	virtual int		GetAudioSourceIndex() = 0;
	virtual int		GetAudioSourceByName(const wchar_t *name) = 0;

	virtual int		GetAudioSourceForVideoSource(int idx) = 0;

	virtual int		GetAudioInputCount() = 0;
	virtual const wchar_t *GetAudioInputName(int idx) = 0;
	virtual bool	SetAudioInput(int idx) = 0;
	virtual int		GetAudioInputIndex() = 0;
	virtual int		GetAudioInputByName(const wchar_t *name) = 0;

	virtual void	SetAudioCaptureEnabled(bool ena) = 0;
	virtual bool	IsAudioCaptureEnabled() = 0;
	virtual bool	IsAudioCaptureAvailable() = 0;

	virtual bool	IsAudioPlaybackEnabled() = 0;
	virtual bool	IsAudioPlaybackAvailable() = 0;
	virtual void	SetAudioPlaybackEnabled(bool ena) = 0;

	virtual void	SetAudioVumeterEnabled(bool ena) = 0;
	virtual bool	IsAudioVumeterEnabled() = 0;

	virtual void	SetHardwareBuffering(int videoBuffers, int audioBuffers, int audioBufferSize) = 0;
	virtual bool	GetHardwareBuffering(int& videoBuffers, int& audioBuffers, int& audioBufferSize) = 0;

	virtual bool	IsDriverDialogSupported(nsVDCapture::DriverDialog dlg) = 0;
	virtual void	DisplayDriverDialog(nsVDCapture::DriverDialog dlg) = 0;

	virtual void	GetPreviewImageSize(sint32& w, sint32& h) = 0;

	virtual void	SetFilterSetup(const VDCaptureFilterSetup& setup) = 0;
	virtual const VDCaptureFilterSetup& GetFilterSetup() = 0;

	virtual void	SetStopPrefs(const VDCaptureStopPrefs& prefs) = 0;
	virtual const VDCaptureStopPrefs& GetStopPrefs() = 0;

	virtual void	SetDiskSettings(const VDCaptureDiskSettings& sets) = 0;
	virtual const VDCaptureDiskSettings& GetDiskSettings() = 0;
 
	virtual uint32	GetPreviewFrameCount() = 0;

	virtual bool	SetVideoFormat(const BITMAPINFOHEADER& bih, LONG cbih) = 0;
	virtual bool	GetVideoFormat(vdstructex<BITMAPINFOHEADER>& bih) = 0;

	virtual void	GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats) = 0;

	virtual bool	SetAudioFormat(const WAVEFORMATEX& wfex, LONG cbwfex) = 0;
	virtual bool	GetAudioFormat(vdstructex<WAVEFORMATEX>& wfex) = 0;

	virtual void	SetAudioCompFormat() = 0;
	virtual void	SetAudioCompFormat(const WAVEFORMATEX& wfex, uint32 cbwfex) = 0;
	virtual bool	GetAudioCompFormat(vdstructex<WAVEFORMATEX>& wfex) = 0;

	virtual void		SetCaptureFile(const wchar_t *filename, bool isStripeSystem) = 0;
	virtual VDStringW	GetCaptureFile() = 0;
	virtual bool		IsStripingEnabled() = 0;

	virtual void	SetSpillSystem(bool enable) = 0;
	virtual bool	IsSpillEnabled() = 0;

	virtual void	IncrementFileID() = 0;
	virtual void	DecrementFileID() = 0;

	virtual void	ScanForDrivers() = 0;
	virtual int		GetDriverCount() = 0;
	virtual const wchar_t *GetDriverName(int i) = 0;
	virtual int		GetDriverByName(const wchar_t *s) = 0;
	virtual bool	SelectDriver(int nDriver) = 0;
	virtual bool	IsDriverConnected() = 0;
	virtual int		GetConnectedDriverIndex() = 0;
	virtual const wchar_t *GetConnectedDriverName() = 0;

	virtual void	Capture(bool bTest) = 0;
	virtual void	CaptureStop() = 0;
};

IVDCaptureProject *VDCreateCaptureProject();

#endif