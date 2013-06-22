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
		if (pNode->bCondemned) {
			tSuspendList::iterator it(listSuspends.begin()), itEnd(listSuspends.end());

			while(it!=itEnd) {
				VDSchedulerSuspendNode *pSuspendNode = *it;

				if (pSuspendNode->mpNode == pNode) {
					it = listSuspends.erase(it);
					pSuspendNode->mSignal.signal();
				} else
					++it;
			}
		} else {
			pNode->bRunning = false;
			if (bReschedule || pNode->bReschedule) {
				pNode->bReschedule = false;
				pNode->bReady = true;
				listReady.push_back(pNode);
			} else
				listWaiting.push_back(pNode);
		}
	}
}

bool VDScheduler::Run() {
	VDSchedulerNode *pNode = NULL;
	vdsynchronized(csScheduler) {
		if (!listReady.empty()) {
			pNode = listReady.front();
			listReady.pop_front();
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

void VDScheduler::IdleWait() {
	if (pWakeupSignal)
		pWakeupSignal->wait();
}

void VDScheduler::Ping() {
	if (pWakeupSignal)
		pWakeupSignal->signal();
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
		if (listReady.empty()) {
			if (pWakeupSignal)
				pWakeupSignal->signal();

			if (pParentSchedulerNode)
				pParentSchedulerNode->Reschedule();
		}

		listWaiting.erase(pNode);
		listReady.push_back(pNode);
	}
}

void VDScheduler::Add(VDSchedulerNode *pNode) {
	pNode->pScheduler = this;
	pNode->bRunning = false;
	pNode->bReschedule = false;
	pNode->bReady = true;
	pNode->bCondemned = false;

	vdsynchronized(csScheduler) {
		tNodeList::iterator it(listReady.begin()), itEnd(listReady.end());

		while(it != itEnd && (*it)->nPriority <= pNode->nPriority)
			++it;

		listReady.insert(it, pNode);
	}

	if (pWakeupSignal)
		pWakeupSignal->signal();

	if (pParentSchedulerNode)
		pParentSchedulerNode->Reschedule();
}

void VDScheduler::Remove(VDSchedulerNode *pNode) {
	VDSchedulerSuspendNode suspendNode(pNode);
	bool running = false;

	vdsynchronized(csScheduler) {
		pNode->bCondemned = true;
		if (pNode->bRunning) {
			running = true;
			listSuspends.push_back(&suspendNode);
		} else
			listWaiting.erase(pNode);
	}

	if (running)
		suspendNode.mSignal.wait();
}

void VDScheduler::DumpStatus() {
	vdsynchronized(csScheduler) {
		VDDEBUG2("\n    Waiting nodes:\n");
		for(tNodeList::iterator it(listWaiting.begin()), itEnd(listWaiting.end()); it!=itEnd; ++it)
			(*it)->DumpStatus();
		VDDEBUG2("\n    Ready nodes:\n");
		for(tNodeList::iterator it2(listReady.begin()), it2End(listReady.end()); it2!=it2End; ++it2)
			(*it2)->DumpStatus();
	}
}
