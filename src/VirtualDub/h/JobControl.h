#ifndef f_VD2_JOBCONTROL_H
#define f_VD2_JOBCONTROL_H

#ifdef _MSC_VER
	#pragma once
#endif

#include <vd2/system/vdstl.h>
#include <vd2/system/filewatcher.h>
#include <vd2/system/log.h>
#include <vd2/system/time.h>

class VDJob;
class IVDStream;

enum VDJobQueueStatus {
	kVDJQS_Idle,
	kVDJQS_Running,
	kVDJQS_Blocked
};

class IVDJobQueueStatusCallback {
public:
	virtual void OnJobQueueStatusChanged(VDJobQueueStatus status) = 0;
	virtual void OnJobAdded(const VDJob& job, int index) = 0;
	virtual void OnJobRemoved(const VDJob& job, int index) = 0;
	virtual void OnJobUpdated(const VDJob& job, int index) = 0;
	virtual void OnJobStarted(const VDJob& job) = 0;
	virtual void OnJobEnded(const VDJob& job) = 0;
	virtual void OnJobProgressUpdate(const VDJob& job, float completion) = 0;
	virtual void OnJobQueueReloaded() = 0;
};

class VDJobQueue : protected IVDFileWatcherCallback, protected IVDTimerCallback {
	VDJobQueue(const VDJobQueue&);
	VDJobQueue& operator=(const VDJobQueue&);
public:
	VDJobQueue();
	~VDJobQueue();

	void Shutdown();

	void SetJobFilePath(const wchar_t *path, bool enableDistributedMode);
	const wchar_t *GetJobFilePath() const;
	const wchar_t *GetDefaultJobFilePath() const;

	int32 GetJobIndexById(uint64 id) const;
	VDJob *GetJobById(uint64 id) const;
	VDJob *ListGet(int index);
	int ListFind(VDJob *vdj_find);
	long ListSize();
	void ListClear(bool force_no_update = false);

	void Refresh(VDJob *job);
	void Add(VDJob *job, bool force_no_update);
	void Delete(VDJob *job, bool force_no_update);
	void Run(VDJob *job);
	void Reload(VDJob *job);
	void Transform(int fromState, int toState);

	void ListLoad(const wchar_t *lpszName, bool skipIfSignatureSame);

	bool IsModified() {
		return mbModified;
	}

	void SetModified();

	bool Flush(const wchar_t *lpfn =NULL);
	void RunAll();
	void RunAllStop();

	void Swap(int x, int y);

	bool IsLocal(const VDJob *job) const;

	bool IsRunInProgress() {
		return mbRunning;
	}

	VDJobQueueStatus GetQueueStatus() const;

	int GetPendingJobCount() const;

	VDJob *GetCurrentlyRunningJob() {
		return mpRunningJob;
	}

	uint64 GetUniqueId();

	const char *GetRunnerName() const;
	uint64 GetRunnerId() const;

	bool IsAutoUpdateEnabled() const;
	void SetAutoUpdateEnabled(bool enabled);

	bool PollAutoRun();
	bool IsAutoRunEnabled() const;
	void SetAutoRunEnabled(bool autorun);

	void SetBlocked(bool blocked);
	void SetCallback(IVDJobQueueStatusCallback *cb);

protected:
	bool Load(IVDStream *stream, bool skipIfSignatureSame);
	void Save(IVDStream *stream, uint64 signature, uint32 revision, bool resetJobRevisions);

	void NotifyStatus();
	uint64 CreateListSignature();
	bool OnFileUpdated(const wchar_t *path);
	void TimerCallback();

	typedef vdfastvector<VDJob *> JobQueue;
	JobQueue mJobQueue;

	uint32	mJobCount;
	uint32	mJobNumber;
	VDJob	*mpRunningJob;
	bool	mbRunning;
	bool	mbRunAllStop;
	bool	mbModified;
	bool	mbBlocked;
	bool	mbOrderModified;
	bool	mbAutoRun;

	VDStringA	mComputerName;
	uint64	mBaseSignature;
	uint64	mRunnerId;
	uint64	mLastSignature;
	uint32	mLastRevision;

	VDStringW		mJobFilePath;
	VDStringW		mDefaultJobFilePath;

	VDFileWatcher	mFileWatcher;
	VDLazyTimer		mFlushTimer;
};

class VDJob {
public:
	enum {
		kStateWaiting		= 0,
		kStateInProgress	= 1,
		kStateCompleted		= 2,
		kStatePostponed		= 3,
		kStateAborted		= 4,
		kStateError			= 5,
		kStateAborting		= 6,
		kStateCount			= 7
	};

	VDJobQueue	*mpJobQueue;

	uint32		mCreationRevision;
	uint32		mChangeRevision;
	uint64		mId;
	uint64		mDateStart;		///< Same units as NT FILETIME.
	uint64		mDateEnd;		///< Same units as NT FILETIME.

	typedef VDAutoLogger::tEntries tLogEntries;
	tLogEntries	mLogEntries;

	/////
	VDJob();
	~VDJob();

	bool operator==(const VDJob& job) const;

	bool	IsLocal() const { return !mpJobQueue || mpJobQueue->IsLocal(this); }

	const char *	GetName() const				{ return mName.c_str(); }
	void			SetName(const char *name)	{ mName = name; }

	const char *	GetInputFile() const			{ return mInputFile.c_str(); }
	void			SetInputFile(const char *file)	{ mInputFile = file; }

	const char *	GetOutputFile() const			{ return mOutputFile.c_str(); }
	void			SetOutputFile(const char *file)	{ mOutputFile = file; }

	const char *	GetError() const				{ return mError.c_str(); }
	void			SetError(const char *err)		{ mError = err; }

	const char *	GetRunnerName() const			{ return mRunnerName.c_str(); }
	uint64			GetRunnerId() const				{ return mRunnerId; }

	bool	IsRunning() const { return mState == kStateInProgress || mState == kStateAborting; }

	int		GetState() const { return mState; }
	void	SetState(int state);

	void	SetRunner(uint64 id, const char *name);

	bool	IsReloadMarkerPresent() const { return mbContainsReloadMarker; }

	void	SetScript(const void *script, uint32 len, bool reloadable);
	const char *GetScript() const { return mScript.c_str(); }

	void Refresh();
	void Run();
	void Reload();

	bool Merge(const VDJob& src, bool srcHasInProgressPriority);

	uint64		mRunnerId;
	VDStringA	mRunnerName;
protected:
	VDStringA	mName;
	VDStringA	mInputFile;
	VDStringA	mOutputFile;
	VDStringA	mError;
	VDStringA	mScript;
	int			mState;
	bool		mbContainsReloadMarker;
};

#endif
