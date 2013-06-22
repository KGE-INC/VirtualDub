#include <vd2/system/vdtypes.h>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/thread.h>

VDScheduler::VDScheduler() : pWakeupSignal(NULL), pParentSchedulerNode(NULL) {
}

VDScheduler::~VDScheduler() {
}

void VDScheduler::setSignal(VDSignal *pSignal) {
	pWakeupSignal = pSignal;
}

void VDScheduler::setSchedulerNode(VDSchedulerNode *pSchedulerNode) {
	pParentSchedulerNode = pSchedulerNode;
}

void VDScheduler::Repost(VDSchedulerNode *pNode, bool bReschedule) {
	vdsynchronized(csScheduler) {
		pNode->bRunning = false;
		if (bReschedule || pNode->bReschedule) {
			pNode->bReschedule = false;
			pNode->bReady = true;
			listReady.AddTail(pNode);
		} else
			listWaiting.AddTail(pNode);
	}
}

bool VDScheduler::Run() {
	VDSchedulerNode *pNode;
	vdsynchronized(csScheduler) {
		pNode = listReady.RemoveHead();
		if (pNode) {
			pNode->bRunning = true;
			pNode->bReady = false;
		}
	}

	if (!pNode)
		return false;

	bool bReschedule;
	try {
		bReschedule = pNode->Service();
	} catch(...) {
		Repost(pNode, false);
		throw;
	}

	Repost(pNode, bReschedule);

	return true;
}

void VDScheduler::Lock() {
	++csScheduler;
}

void VDScheduler::Unlock() {
	--csScheduler;
}

void VDScheduler::Reschedule(VDSchedulerNode *pNode) {
	VDCriticalSection::AutoLock lock(csScheduler);

	RescheduleFast(pNode);
}

void VDScheduler::RescheduleFast(VDSchedulerNode *pNode) {
	if (pNode->bReady)
		return;

	pNode->bReady = true;

	if (pNode->bRunning)
		pNode->bReschedule = true;
	else {
		if (listReady.IsEmpty()) {
			if (pWakeupSignal)
				pWakeupSignal->signal();

			if (pParentSchedulerNode)
				pParentSchedulerNode->Reschedule();
		}

		pNode->Remove();
		listReady.AddTail(pNode);
	}
}

void VDScheduler::Add(VDSchedulerNode *pNode) {
	pNode->pScheduler = this;
	pNode->bRunning = false;
	pNode->bReschedule = false;
	pNode->bReady = true;

	++csScheduler;
	VDSchedulerNode *pInsertPt = listReady.AtTail();

	while(pInsertPt->NextFromTail() && pInsertPt->nPriority < pNode->nPriority)
		pInsertPt = pInsertPt->NextFromTail();

	pNode->InsertAfter(pInsertPt);
	--csScheduler;
}

void VDScheduler::Remove(VDSchedulerNode *pNode) {
	pNode->Remove();
}
