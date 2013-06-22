#ifndef f_PROJECTUI_H
#define f_PROJECTUI_H

#include <windows.h>
#include <shellapi.h>

#include "project.h"
#include "MRUList.h"
#include "VideoDisplay.h"

class VDProjectUI : public VDProject, protected IVDVideoDisplayCallback {
public:
	VDProjectUI();
	~VDProjectUI();

	bool Attach(HWND hwnd);
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
	static LRESULT CALLBACK StaticWndProc( HWND hWnd, UINT msg, UINT wParam, LONG lParam);
	LRESULT MainWndProc( UINT msg, UINT wParam, LONG lParam);
	LRESULT DubWndProc(UINT msg, WPARAM wParam, LPARAM lParam);
	void HandleDragDrop(HDROP hdrop);
	bool MenuHit(UINT id);
	void RepaintMainWindow(HWND hWnd);
	void ShowMenuHelp(WPARAM wParam);
	bool DoFrameRightClick(LPARAM lParam);
	void UpdateMainMenu(HMENU hMenu);
	void UpdateDubMenu(HMENU hMenu);
	void UpdateVideoFrameLayout();

	void UIRefreshInputFrame(bool bValid);
	void UIRefreshOutputFrame(bool bValid);
	void UISetDubbingMode(bool bActive, bool bIsPreview);
	void UIRunDubMessageLoop();
	void UICurrentPositionUpdated();
	void UISelectionStartUpdated();
	void UISelectionEndUpdated();
	void UITimelineUpdated();
	void UIShuttleModeUpdated();
	void UISourceFileUpdated();
	void UIVideoSourceUpdated();
	void UIVideoFiltersUpdated();

	void UpdateMRUList();

	void DisplayRequestUpdate(IVDVideoDisplay *pDisp);

	LRESULT (VDProjectUI::*mpWndProc)(UINT, WPARAM, LPARAM);

	HWND		mhwndPosition;
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

	RECT		mrInputFrame;
	RECT		mrOutputFrame;

	WNDPROC		mOldWndProc;

	MRUList		mMRUList;
};

#endif
