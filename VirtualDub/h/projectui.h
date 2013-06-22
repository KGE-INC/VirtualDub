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

#ifndef f_PROJECTUI_H
#define f_PROJECTUI_H

#include <windows.h>
#include <shellapi.h>

#include "project.h"
#include "MRUList.h"
#include "VideoDisplay.h"
#include "PositionControl.h"
#include "uiframe.h"

class IVDPositionControl;

class VDProjectUI : public VDProject, public vdrefcounted<IVDUIFrameClient>, protected IVDVideoDisplayCallback, public IVDPositionControlCallback {
public:
	VDProjectUI();
	~VDProjectUI();

	bool Attach(VDGUIHandle hwnd);
	void Detach();

	void SetTitle(int nTitleString, int nArgs, ...);

	void OpenAsk();
	void AppendAsk();
	void SaveAVIAsk();
	void SaveCompatibleAVIAsk();
	void SaveStripedAVIAsk();
	void SaveStripeMasterAsk();
	void SaveImageSequenceAsk();
	void SaveSegmentedAVIAsk();
	void SaveWAVAsk();
	void SaveConfigurationAsk();
	void LoadConfigurationAsk();
	void SetVideoFiltersAsk();
	void SetVideoFramerateOptionsAsk();
	void SetVideoDepthOptionsAsk();
	void SetVideoRangeOptionsAsk();
	void SetVideoCompressionAsk();
	void SetVideoErrorModeAsk();
	void SetAudioFiltersAsk();
	void SetAudioConversionOptionsAsk();
	void SetAudioInterleaveOptionsAsk();
	void SetAudioCompressionAsk();
	void SetAudioVolumeOptionsAsk();
	void SetAudioSourceWAVAsk();
	void SetAudioErrorModeAsk();
	void JumpToFrameAsk();

protected:
	LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT MainWndProc( UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT DubWndProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void HandleDragDrop(HDROP hdrop);
	void OnPreferencesChanged();
	bool MenuHit(UINT id);
	void RepaintMainWindow(HWND hWnd);
	void ShowMenuHelp(WPARAM wParam);
	bool DoFrameRightClick(LPARAM lParam);
	void UpdateMainMenu(HMENU hMenu);
	void UpdateDubMenu(HMENU hMenu);
	void RepositionPanes();
	void UpdateVideoFrameLayout();

	void UIRefreshInputFrame(bool bValid);
	void UIRefreshOutputFrame(bool bValid);
	void UISetDubbingMode(bool bActive, bool bIsPreview);
	void UIRunDubMessageLoop();
	void UICurrentPositionUpdated();
	void UISelectionUpdated();
	void UITimelineUpdated();
	void UIShuttleModeUpdated();
	void UISourceFileUpdated();
	void UIVideoSourceUpdated();
	void UIVideoFiltersUpdated();
	void UIDubParametersUpdated();

	void UpdateMRUList();

	void DisplayRequestUpdate(IVDVideoDisplay *pDisp);

	bool GetFrameString(wchar_t *buf, size_t buflen, VDPosition dstFrame);

	void LoadSettings();
	void SaveSettings();

	LRESULT (VDProjectUI::*mpWndProc)(UINT, WPARAM, LPARAM);

	HWND		mhwndPosition;
	IVDPositionControl	*mpPosition;
	HWND		mhwndStatus;
	HWND		mhwndInputFrame;
	HWND		mhwndOutputFrame;
	HWND		mhwndInputDisplay;
	HWND		mhwndOutputDisplay;
	IVDVideoDisplay	*mpInputDisplay;
	IVDVideoDisplay	*mpOutputDisplay;

	HMENU		mhMenuNormal;
	HMENU		mhMenuDub;
	HMENU		mhMenuDisplay;
	HACCEL		mhAccelDub;
	HACCEL		mhAccelMain;

	RECT		mrInputFrame;
	RECT		mrOutputFrame;

	WNDPROC		mOldWndProc;

	bool		mbDubActive;

	MRUList		mMRUList;
};

#endif
