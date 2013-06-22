#ifndef f_VD2_SYSTEM_VDSCHEDULER_H
#define f_VD2_SYSTEM_VDSCHEDULER_H

#include <vd2/system/vdstl.h>
#include <vd2/system/thread.h>

class VDSchedulerNode;
class VDSchedulerSuspendNode;
class VDSignal;

class VDScheduler {
public:
	VDScheduler();
	~VDScheduler();

	void setSignal(VDSignal *);
	VDSignal *getSignal() { return pWakeupSignal; }
	void setSchedulerNode(VDSchedulerNode *pSchedulerNode);

	bool Run();
	void IdleWait();
	void Ping();									// Restart a scheduler thread.  This is required when a scheduler thread leaves.
	void Lock();
	void Unlock();
	void Reschedule(VDSchedulerNode *);				// Move node to Ready if Waiting.
	void RescheduleFast(VDSchedulerNode *);			// Same as Reschedule(), but assumes the scheduler is already locked.
	void Add(VDSchedulerNode *pNode);				// Add node to scheduler.
	void Remove(VDSchedulerNode *pNode);			// Remove node from scheduler.
	void DumpStatus();

protected:
	void Repost(VDSchedulerNode *, bool);

	VDCriticalSection csScheduler;
	VDSignal *pWakeupSignal;
	VDSchedulerNode *pParentSchedulerNode;

	typedef vdlist<VDSchedulerNode> tNodeList;
	tNodeList listWaiting, listReady;

	typedef vdlist<VDSchedulerSuspendNode> tSuspendList;
	tSuspendList listSuspends;
};

class VDSchedulerNode : public vdlist<VDSchedulerNode>::node {
friend class VDScheduler;
public:
	int nPriority;

	virtual bool Service()=0;

	virtual void DumpStatus() {}

	void Reschedule() { pScheduler->Reschedule(this); }
protected:
	VDScheduler *pScheduler;
	volatile bool bRunning;
	volatile bool bReschedule;
	volatile bool bReady;
	volatile bool bCondemned;
};

class VDSchedulerSuspendNode : public vdlist<VDSchedulerSuspendNode>::node {
public:
	VDSchedulerSuspendNode(VDSchedulerNode *pNode) : mpNode(pNode) {}

	VDSchedulerNode *mpNode;
	VDSignal mSignal;
};

#endif
