#ifndef f_SYSTEM_VDNAMESPACE_H
#define f_SYSTEM_VDNAMESPACE_H

#include <vd2/system/list.h>

class VDNamespaceNode;
class VDNamespaceGroup;
class VDNamespaceItem;
class VDNamespace;
template <class T> class VDNamespace2;

///////////////////////////////////////////////////////////////////////////
//
// Node: Any item in the namespace.
//
///////////////////////////////////////////////////////////////////////////

class VDNamespaceNode {
public:
	const char *pszName;
	VDNamespaceGroup *const pParent;

	VDNamespaceNode(const char *name, VDNamespaceGroup *parent) : pszName(name), pParent(parent) {	}
};

///////////////////////////////////////////////////////////////////////////
//
// Group: Holds items.
//
///////////////////////////////////////////////////////////////////////////

class VDNamespaceGroup : public VDNamespaceNode, public ListNode2<VDNamespaceGroup> {
public:
	ListAlloc<VDNamespaceItem> listItems;
	ListAlloc<VDNamespaceGroup> listGroups;

	const char *namedup(const char *s);

	VDNamespaceGroup(const char *_pszName, VDNamespaceGroup *parent);
	~VDNamespaceGroup();
};

///////////////////////////////////////////////////////////////////////////
//
// Item class
//
///////////////////////////////////////////////////////////////////////////

class VDNamespaceItem : public VDNamespaceNode, public ListNode2<VDNamespaceItem> {
public:
	const void *object;

	VDNamespaceItem(const char *_pszName, VDNamespaceGroup *parent, const void *src);
	~VDNamespaceItem();
};

///////////////////////////////////////////////////////////////////////////
//
// Namespace class
//
///////////////////////////////////////////////////////////////////////////

class VDNamespace {
protected:
	VDNamespaceGroup root;

	VDNamespaceGroup *_lookupGroup(const char *pszName, bool fCreate, bool fIsFilter);
	VDNamespaceItem *_findItemByObject(const VDNamespaceGroup *pGroup, const void *pObj);
	bool _getPathByItem(const VDNamespaceNode *pEntry, char *buf, int maxlen);

public:

	VDNamespace();
	~VDNamespace();

	typedef bool (*tGroupEnumerator)(VDNamespace *pThis, const char *pszName, const VDNamespaceGroup *pGroup, void *pvData);
	typedef bool (*tItemEnumerator)(VDNamespace *pThis, const char *pszName, const void *pItem, void *pvData);

	void clear();
	void add(const char *pszGroup, const char *pszName, const void *pDef);
	const void *lookup(const char *pszName);

	bool enumerateGroups(const VDNamespaceGroup *pGroupRoot, tGroupEnumerator pEnum, void *pvData);
	bool enumerateItems(const VDNamespaceGroup *pGroupRoot, tItemEnumerator pEnum, void *pvData);

	bool getPathByItem(const void *pObj, char *buf, int maxlen);
};

///////////////////////////////////////////////////////////////////////////
//
//	Templated Namespace class
//
///////////////////////////////////////////////////////////////////////////

template <class T>
class VDNamespace2 : public VDNamespace {
public:
	VDNamespace2() {}
	~VDNamespace2() {}

	typedef bool (*tGroupEnumerator)(VDNamespace2<T> *pThis, const char *pszName, const VDNamespaceGroup *pGroup, void *pvData);
	typedef bool (*tItemEnumerator)(VDNamespace2<T> *pThis, const char *pszName, const T *pItem, void *pvData);

	void add(const char *pszGroup, const char *pszName, const T *pDef) {
		VDNamespace::add(pszGroup, pszName, pDef);
	}

	const T *lookup(const char *pszName) {
		return static_cast<const T *>(VDNamespace::lookup(pszName));
	}

	bool enumerateGroups(const VDNamespaceGroup *pGroupRoot, tGroupEnumerator pEnum, void *pvData) {
		for(ListAlloc<VDNamespaceGroup>::fwit it = (pGroupRoot ? pGroupRoot : &root)->listGroups.begin(); it; ++it)
			if (!pEnum(this, it->pszName, it, pvData))
				return false;

		return true;
	}

	bool enumerateItems(const VDNamespaceGroup *pGroupRoot, tItemEnumerator pEnum, void *pvData) {
		for(ListAlloc<VDNamespaceItem>::fwit it = (pGroupRoot ? pGroupRoot : &root)->listItems.begin(); it; ++it)
			if (!pEnum(this, it->pszName, static_cast<const T *>(it->object), pvData))
				return false;

		return true;
	}

	bool getPathByItem(const T *pObj, char *buf, int maxlen) {
		return VDNamespace::getPathByItem(pObj, buf, maxlen);
	}
};

#endif
