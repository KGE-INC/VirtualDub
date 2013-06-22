#ifndef f_PROJECT_H
#define f_PROJECT_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/fraction.h>
#include "FrameSubset.h"
#include "filter.h"
#include "timeline.h"
#include <list>
#include <utility>

class SceneDetector;
class IVDDubberOutputSystem;
class IVDInputDriver;
class IDubStatusHandler;
class DubOptions;
class VDProjectSchedulerThread;

class IVDProjectUICallback {
public:
	virtual void UIRefreshInputFrame(bool bValid) = 0;
	virtual void UIRefreshOutputFrame(bool bValid) = 0;
	virtual void UISetDubbingMode(bool bActive, bool bIsPreview) = 0;
	virtual void UIRunDubMessageLoop() = 0;
	virtual void UICurrentPositionUpdated() = 0;
	virtual void UISelectionUpdated(bool notifyUser) = 0;
	virtual void UITimelineUpdated() = 0;
	virtual void UIShuttleModeUpdated() = 0;
	virtual void UISourceFileUpdated() = 0;
	virtual void UIVideoSourceUpdated() = 0;
	virtual void UIDubParametersUpdated() = 0;
};

class VDProject {
public:
	VDProject();
	~VDProject();

	virtual bool Attach(VDGUIHandle hwnd);
	virtual void Detach();

	void SetUICallback(IVDProjectUICallback *pCB);

	VDTimeline& GetTimeline() { return mTimeline; }
	void BeginTimelineUpdate(const wchar_t *undostr = 0);
	void EndTimelineUpdate();

	bool Undo();
	bool Redo();
	void ClearUndoStack();
	const wchar_t *GetCurrentUndoAction();
	const wchar_t *GetCurrentRedoAction();

	bool Tick();

	VDPosition GetCurrentFrame();
	VDPosition GetFrameCount();
	VDFraction GetInputFrameRate();

	typedef std::list<std::pair<uint32, VDStringA> > tTextInfo;
	tTextInfo& GetTextInfo() { return mTextInfo; }
	const tTextInfo& GetTextInfo() const { return mTextInfo; }

	void ClearSelection(bool notifyUser = true);
	bool IsSelectionEmpty();
	bool IsSelectionPresent();
	void SetSelectionStart();
	void SetSelectionStart(VDPosition pos, bool notifyUser = true);
	void SetSelectionEnd();
	void SetSelectionEnd(VDPosition pos, bool notifyUser = true);
	void SetSelection(VDPosition start, VDPosition end, bool notifyUser = true);
	VDPosition GetSelectionStartFrame();
	VDPosition GetSelectionEndFrame();

	bool IsClipboardEmpty();

	void Cut();
	void Copy();
	void Paste();
	void Delete();
	void DeleteInternal(bool tagAsCut, bool noTag);
	void MaskSelection(bool bMasked);

	void DisplayFrame(bool bDispInput = true);

	void RunOperation(IVDDubberOutputSystem *pOutputSystem, int fAudioOnly, DubOptions *pOptions, int iPriority=0, bool fPropagateErrors = false, long lSpillThreshold = 0, long lSpillFrameThreshold = 0);

	////////////////////

	void Quit();
	void Open(const wchar_t *pFilename, IVDInputDriver *pSelectedDriver = 0, bool fExtendedOpen = false, bool fQuiet = false, bool fAutoscan = false, const char *pInputOpts = 0);
	void Reopen();
	void OpenWAV(const wchar_t *pFilename);
	void CloseWAV();
	void PreviewInput();
	void PreviewOutput();
	void PreviewAll();
	void RunNullVideoPass();
	void CloseAVI();			// to be removed later....
	void Close();
	void StartServer();
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

	VDGUIHandle		mhwnd;

	IVDProjectUICallback *mpCB;

	SceneDetector	*mpSceneDetector;
	int		mSceneShuttleMode;
	int		mSceneShuttleAdvance;
	int		mSceneShuttleCounter;

	FrameSubset		mClipboard;
	VDTimeline		mTimeline;

	struct UndoEntry {
		FrameSubset	mSubset;
		VDStringW	mDescription;
		VDPosition	mFrame;
		VDPosition	mSelStart;
		VDPosition	mSelEnd;

		UndoEntry(const FrameSubset& s, const wchar_t *desc, VDPosition pos, VDPosition selStart, VDPosition selEnd) : mSubset(s), mDescription(desc), mFrame(pos), mSelStart(selStart), mSelEnd(selEnd) {}
	};
	std::list<UndoEntry>	mUndoStack;
	std::list<UndoEntry>	mRedoStack;

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

	std::list<std::pair<uint32, VDStringA> >	mTextInfo;

#if 0		// We don't need this yet.
	VDScheduler					mScheduler;
	VDSignal					mSchedulerSignal;
	VDProjectSchedulerThread	*mpSchedulerThreads;
	int							mThreadCount;
#endif
};

#endif
