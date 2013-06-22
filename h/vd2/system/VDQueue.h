#ifndef f_VD2_SYSTEM_VDQUEUE_H
#define f_VD2_SYSTEM_VDQUEUE_H

#include <vd2/system/List.h>

template<class T>
class VDQueueNode : public ListNode2< VDQueueNode<T> > {
public:
	T t;
	VDQueueNode(const T& t2) : t(t2) {}
};

template<class T>
class VDQueue {
public:
	ListAlloc< VDQueueNode<T> > list;

	VDQueue<T>();
	~VDQueue<T>();
	T Pop();
	T Peek();
	void Push(const T&);
	bool isEmpty() { return list.IsEmpty(); }
};

template<class T>
VDQueue<T>::VDQueue<T>() {
}

template<class T>
VDQueue<T>::~VDQueue<T>() {
	while(!list.IsEmpty())
		delete list.RemoveTail();
}

template<class T>
T VDQueue<T>::Peek() {
	return list.AtHead()->t;
}

template<class T>
T VDQueue<T>::Pop() {
	return list.RemoveHead()->t;
}

template<class T>
void VDQueue<T>::Push(const T& t) {
	list.AddTail(new VDQueueNode<T>(t));
}

/////////////

template<class T>
class VDQueueAlloc : public VDQueue<T> {
public:
	~VDQueueAlloc();
};

template<class T>
VDQueueAlloc<T>::~VDQueueAlloc() {
	for(ListAlloc< VDQueueNode<T> >::fwit it = list.begin(); it; ++it)
		delete &*it;
}

#endif
