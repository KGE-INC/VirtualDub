#ifndef f_PROJECT_H
#define f_PROJECT_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/fraction.h>
#include "FrameSubset.h"
#include "filter.h"
#include "timeline.h"

class SceneDetector;
class IVDDubberOutputSystem;
class IVDInputDriver;
class IDubStatusHandler;
class DubOptions;
class VDProjectSchedulerThread;

class VDProject {
public:
	VDProject();
	~VDProject();

	virtual bool Attach(VDGUIHandle hwnd);
	virtual void Detach();

	VDTimeline& GetTimeline() { return mTimeline; }
	void EndTimelineUpdate();

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

	void DisplayFrame(bool bDispInput = true);

	void RunOperation(IVDDubberOutputSystem *pOutputSystem, int fAudioOnly, DubOptions *pOptions, int iPriority=0, bool fPropagateErrors = false, long lSpillThreshold = 0, long lSpillFrameThreshold = 0);

	////////////////////

	void Quit();
	void Open(const wchar_t *pFilename, IVDInputDriver *pSelectedDriver = 0, bool fExtendedOpen = false, bool fQuiet = false, bool fAutoscan = false, const char *pInputOpts = 0);
	void OpenWAV(const wchar_t *pFilename);
	void CloseWAV();
	void PreviewInput();
	void PreviewOutput();
	void PreviewAll();
	void CloseAVI();			// to be removed later....
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
	bool UpdateFrame();
	void RefilterFrame();

	static void StaticPositionCallback(VDPosition start, VDPosition cur, VDPosition end, int progress, void *cookie);

	void UpdateDubParameters();

	virtual void UIRefreshInputFrame(bool bValid) = 0;
	virtual void UIRefreshOutputFrame(bool bValid) = 0;
	virtual void UISetDubbingMode(bool bActive, bool bIsPreview) = 0;
	virtual void UIRunDubMessageLoop() = 0;
	virtual void UICurrentPositionUpdated() = 0;
	virtual void UISelectionUpdated() = 0;
	virtual void UITimelineUpdated() = 0;
	virtual void UIShuttleModeUpdated() = 0;
	virtual void UISourceFileUpdated() = 0;
	virtual void UIVideoSourceUpdated() = 0;
	virtual void UIDubParametersUpdated() = 0;

	VDGUIHandle		mhwnd;

	SceneDetector	*mpSceneDetector;
	int		mSceneShuttleMode;
	int		mSceneShuttleAdvance;
	int		mSceneShuttleCounter;

	FrameSubset		mClipboard;
	VDTimeline		mTimeline;

	IDubStatusHandler	*mpDubStatus;

	VDPosition	mposCurrentFrame;
	VDPosition	mposSelectionStart;
	VDPosition	mposSelectionEnd;

	FilterStateInfo mfsi;
	VDPosition		mDesiredInputFrame;
	VDPosition		mDesiredOutputFrame;
	VDPosition		mDesiredNextInputFrame;
	VDPosition		mDesiredNextOutputFrame;
	VDPosition		mLastDisplayedInputFrame;
	bool			mbUpdateInputFrame;
	bool			mbUpdateOutputFrame;
	bool			mbUpdateLong;
	int				mFramesDecoded;
	uint32			mLastDecodeUpdate;

	vdblock<char>	mVideoSampleBuffer;

	VDFraction		mVideoInputFrameRate;
	VDFraction		mVideoOutputFrameRate;

#if 0		// We don't need this yet.
	VDScheduler					mScheduler;
	VDSignal					mSchedulerSignal;
	VDProjectSchedulerThread	*mpSchedulerThreads;
	int							mThreadCount;
#endif
};

#endif
