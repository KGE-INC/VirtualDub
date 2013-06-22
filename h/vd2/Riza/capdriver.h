//	VirtualDub - Video processing and capture application
//	A/V interface library
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

#ifndef f_VD2_RIZA_CAPDRIVER_H
#define f_VD2_RIZA_CAPDRIVER_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/vectors.h>
#include <vd2/system/vdstl.h>
#include <list>

#include <windows.h>			// hmm, need to get rid of this....
#include <mmsystem.h>

class MyError;

namespace nsVDCapture {
	enum DisplayMode {
		kDisplayNone,
		kDisplayHardware,
		kDisplaySoftware,
		kDisplayAnalyze
	};

	enum DriverDialog {
		kDialogVideoFormat,
		kDialogVideoSource,
		kDialogVideoDisplay,
		kDialogVideoCapturePin,
		kDialogVideoPreviewPin,
		kDialogVideoCaptureFilter,
		kDialogVideoCrossbar,
		kDialogVideoCrossbar2,
		kDialogTVTuner,
		kDialogCount
	};
};

class VDINTERFACE IVDCaptureDriverCallback {
public:
	virtual void CapBegin(sint64 global_clock) = 0;
	virtual void CapEnd(const MyError *pError) = 0;
	virtual bool CapControl(bool is_preroll) = 0;
	virtual void CapProcessData(int stream, const void *data, uint32 size, sint64 timestamp, bool key, sint64 global_clock) = 0;
};

class VDINTERFACE IVDCaptureDriver {
public:
	virtual ~IVDCaptureDriver() {}

	virtual bool	Init(VDGUIHandle hParent) = 0;

	virtual void	SetCallback(IVDCaptureDriverCallback *pCB) = 0;

	virtual bool	IsHardwareDisplayAvailable() = 0;

	virtual void	SetDisplayMode(nsVDCapture::DisplayMode m) = 0;
	virtual nsVDCapture::DisplayMode		GetDisplayMode() = 0;

	virtual void	SetDisplayRect(const vdrect32& r) = 0;
	virtual vdrect32	GetDisplayRectAbsolute() = 0;
	virtual void	SetDisplayVisibility(bool vis) = 0;

	virtual void	SetFramePeriod(sint32 ms) = 0;
	virtual sint32	GetFramePeriod() = 0;

	virtual uint32	GetPreviewFrameCount() = 0;

	virtual bool	GetVideoFormat(vdstructex<BITMAPINFOHEADER>& vformat) = 0;
	virtual bool	SetVideoFormat(const BITMAPINFOHEADER *pbih, uint32 size) = 0;

	virtual	bool	IsAudioCapturePossible() = 0;
	virtual bool	IsAudioCaptureEnabled() = 0;
	virtual void	SetAudioCaptureEnabled(bool b) = 0;
	virtual void	SetAudioAnalysisEnabled(bool b) = 0;

	virtual void	GetAvailableAudioFormats(std::list<vdstructex<WAVEFORMATEX> >& aformats) = 0;

	virtual bool	GetAudioFormat(vdstructex<WAVEFORMATEX>& aformat) = 0;
	virtual bool	SetAudioFormat(const WAVEFORMATEX *pwfex, uint32 size) = 0;

	virtual bool	IsDriverDialogSupported(nsVDCapture::DriverDialog dlg) = 0;
	virtual void	DisplayDriverDialog(nsVDCapture::DriverDialog dlg) = 0;

	virtual bool	CaptureStart() = 0;
	virtual void	CaptureStop() = 0;
	virtual void	CaptureAbort() = 0;
};

class VDINTERFACE IVDCaptureSystem {
public:
	virtual ~IVDCaptureSystem() {}

	virtual void			EnumerateDrivers() = 0;

	virtual int				GetDeviceCount() = 0;
	virtual const wchar_t	*GetDeviceName(int index) = 0;

	virtual IVDCaptureDriver	*CreateDriver(int deviceIndex) = 0;
};

#endif
