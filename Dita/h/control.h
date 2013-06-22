#ifndef f_VD2_DITA_CONTROL_H
#define f_VD2_DITA_CONTROL_H

#include <wchar.h>

#include <vd2/system/vdtypes.h>

#include <vd2/Dita/interface.h>

///////////////////////////////////////////////////////////////////////////

class VDUIControlBase : public IVDUIControl {
public:
	unsigned		mID;
	IVDUIBase		*mpBase;
	IVDUIControl	*mpParent;
	VDUIRect		mPosition;
	VDUISize		mMinSize, mMaxSize;
	nsVDUI::eAlign	mAlignX;
	nsVDUI::eAlign	mAlignY;
	float			mDesiredAspectRatio;
	bool			mbActuallyEnabled;
	bool			mbEnabled;
	bool			mbActuallyVisible;
	bool			mbVisible;

	VDUILayoutSpecs	mLayoutSpecs;

	VDUIControlBase();
	virtual ~VDUIControlBase() {}

	virtual bool Create(IVDUIControl *pControl) {
		IVDUIBase *pBase = pControl->AsBase();

		mpParent = pControl;

		if (pBase)
			mpBase = pBase;
		else
			mpBase = pControl->GetBase();

		return true;
	}

	virtual void Destroy() {
		if (mpBase)
			mpBase->Remove(this);

		delete this;
	}

	void SetID(unsigned id) { mID = id; }

	unsigned				GetID()				{ return mID; }
	IVDUIBase *				GetBase()			{ return mpBase; }
	IVDUIControl *			GetParent()			{ return mpParent; }
	const VDUILayoutSpecs&	GetLayoutSpecs()	{ return mLayoutSpecs; }

	virtual VDGUIHandle				GetRawHandle()		{ return NULL; }

	virtual IVDUIBase *				AsBase()			{ return NULL; }
	virtual IVDUIGrid *				AsUIGrid()			{ return NULL; }
	virtual IVDUISet *				AsUISet()			{ return NULL; }
	virtual IVDUIList *				AsUIList()			{ return NULL; }
	virtual IVDUIField *			AsUIField()			{ return NULL; }
	virtual IVDUISlider *			AsUISlider()		{ return NULL; }

	virtual IVDUIControlNativeCallback *AsNativeCallback() { return NULL; }

	void GetAlignment(nsVDUI::eAlign& x, nsVDUI::eAlign& y) {
		x = mAlignX;
		y = mAlignY;
	}

	void SetAlignment(nsVDUI::eAlign x, nsVDUI::eAlign y) {
		if (x)
			mAlignX = x;

		if (y)
			mAlignY = y;
	}

	virtual void GetMinimumSize(VDUISize& s) {
		s = mMinSize;
	}

	virtual void SetMinimumSize(const VDUISize& s) {
		mMinSize = s;
	}

	virtual void GetMaximumSize(VDUISize& s) {
		s = mMaxSize;
	}

	virtual void SetMaximumSize(const VDUISize& s) {
		mMaxSize = s;
	}

	virtual float GetDesiredAspectRatio() {
		return mDesiredAspectRatio;
	}

	virtual void SetDesiredAspectRatio(float rAspect) {
		mDesiredAspectRatio = rAspect;
	}

	// For PreLayout() and PostLayout(), you must overload either the
	// standard method, or the base method.  If you leave the standard
	// method alone, VDUIControlBase will automatically handle min/max
	// and placement for you.

	virtual void PreLayout(const VDUILayoutSpecs& parentConstraints);
	virtual void PreLayoutAttempt(const VDUILayoutSpecs& parentConstraints);
	virtual void PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {}
	virtual void PostLayout(const VDUIRect& target);
	virtual void PostLayoutBase(const VDUIRect& target) {}

	virtual void GetPosition(VDUIRect& r) {
		r = mPosition;
	}

	virtual void SetPosition(const VDUIRect& r) {
		mPosition = r;
	}

	bool IsVisible()				{ return mbVisible; }
	bool IsActuallyVisible()		{ return mbActuallyVisible; }
	bool IsEnabled()				{ return mbEnabled; }
	bool IsActuallyEnabled()		{ return mbActuallyEnabled; }

	virtual void Show(bool b);
	virtual void Enable(bool b);
	virtual void SetFocus() {}

	virtual nsVDUI::eCompressType GetCompressType()	{ return nsVDUI::kCompressNone; }

	virtual nsVDUI::eLinkMethod GetLinkMethod()		{ return nsVDUI::kLinkMethodStatic; }
	virtual void ProcessLinks()						{ GetBase()->ProcessLinksPtr(this);	}

	virtual bool GetStateb()						{ return false; }
	virtual void SetStateb(bool b)					{}
	virtual int GetStatei()							{ return 0; }
	virtual void SetStatei(int i)					{}
	virtual int GetTextLengthw()					{ return 0; }
	virtual int GetTextw(wchar_t *buf, int maxlen)	{ return 0; }
	virtual void SetTextw(const wchar_t *s)			{}
};

#endif
