#ifndef f_PROJECT_H
#define f_PROJECT_H

#include <windows.h>
#include <vfw.h>
#include <vd2/system/vdtypes.h>
#include "FrameSubset.h"
#include "filter.h"

class SceneDetector;
class IVDDubberOutputSystem;
class IVDInputDriver;
class IDubStatusHandler;
class DubOptions;

class VDProject {
public:
	VDProject();
	~VDProject();

	virtual bool Attach(HWND hwnd);
	virtual void Detach();

	bool Tick();

	VDPosition GetCurrentFrame();
	VDPosition GetFrameCount();

	void ClearSelection();
	bool IsSelectionEmpty();
	void SetSelectionStart();
	void SetSelectionStart(VDPosition);
	void SetSelectionEnd();
	void SetSelectionEnd(VDPosition);
	VDPosition GetSelectionStartFrame();
	VDPosition GetSelectionEndFrame();

	bool IsClipboardEmpty();

	void Cut();
	void Copy();
	void Paste();
	void Delete();
	void MaskSelection(bool bMasked);

	void DisplayFrame(VDPosition pos, bool bDispInput = true);

	void RunOperation(IVDDubberOutputSystem *pOutputSystem, int fAudioOnly, DubOptions *pOptions, int iPriority=0, bool fPropagateErrors = false, long lSpillThreshold = 0, long lSpillFrameThreshold = 0);

	////////////////////

	void Quit();
	void Open(const wchar_t *pFilename, IVDInputDriver *pSelectedDriver = 0, bool fExtendedOpen = false, bool fQuiet = false, bool fAutoscan = false, const char *pInputOpts = 0);
	void PreviewInput();
	void PreviewOutput();
	void PreviewAll();
	void Close();
	void StartServer();
	void SwitchToCaptureMode();
	void ShowInputInfo();
	void SetVideoMode(int mode);
	void CopySourceFrameToClipboard();
	void CopyOutputFrameToClipboard();
	void SetAudioSourceNone();
	void SetAudioSourceNormal();
	void SetAudioMode(int mode);
	void MoveToFrame(VDPosition pos);
	void MoveToStart();
	void MoveToPrevious();
	void MoveToNext();
	void MoveToEnd();
	void MoveToSelectionStart();
	void MoveToSelectionEnd();
	void MoveToNearestKey(VDPosition pos);
	void MoveToPreviousKey();
	void MoveToNextKey();
	void MoveBackSome();
	void MoveForwardSome();
	void StartSceneShuttleReverse();
	void StartSceneShuttleForward();
	void MoveToPreviousRange();
	void MoveToNextRange();
	void MoveToPreviousDrop();
	void MoveToNextDrop();
	void ResetTimeline();
	void ResetTimelineWithConfirmation();
	void ScanForErrors();
	void AbortOperation();

	void SceneShuttleStop();

protected:
	void SceneShuttleStep();

	static void StaticPositionCallback(VDPosition start, VDPosition cur, VDPosition end, int progress, void *cookie);

	virtual void UIRefreshInputFrame(bool bValid) = 0;
	virtual void UIRefreshOutputFrame(bool bValid) = 0;
	virtual void UISetDubbingMode(bool bActive, bool bIsPreview) = 0;
	virtual void UIRunDubMessageLoop() = 0;
	virtual void UICurrentPositionUpdated() = 0;
	virtual void UISelectionStartUpdated() = 0;
	virtual void UISelectionEndUpdated() = 0;
	virtual void UITimelineUpdated() = 0;
	virtual void UIShuttleModeUpdated() = 0;
	virtual void UISourceFileUpdated() = 0;
	virtual void UIVideoSourceUpdated() = 0;

	HWND		mhwnd;

	SceneDetector	*mpSceneDetector;
	int		mSceneShuttleMode;
	int		mSceneShuttleAdvance;
	int		mSceneShuttleCounter;

	FrameSubset		mClipboard;

	IDubStatusHandler	*mpDubStatus;

	VDPosition	mposCurrentFrame;
	VDPosition	mposSelectionStart;
	VDPosition	mposSelectionEnd;

	FilterStateInfo mfsi;
	VDPosition		mLastDisplayedInputFrame;
};

#endif
