#ifndef f_VD2_DITA_BASETYPES_H
#define f_VD2_DITA_BASETYPES_H

class VDUIParameters : public IVDUIParameters {
public:
	bool	GetB(uint32 id, bool defaultVal);
	int		GetI(uint32 id, int defaultVal);
	float	GetF(uint32 id, float defaultVal);

	void SetB(uint32 id, bool val) { mParams[id].b = val; }
	void SetI(uint32 id, int val) { mParams[id].i = val; }
	void SetF(uint32 id, float val) { mParams[id].f = val; }

protected:
	union Variant {
		bool b;
		int i;
		float f;
	};

	const Variant *Lookup(uint32 id) const;

	typedef std::map<uint32, Variant> tParams;
	tParams mParams;
};

class VDUIWindow : public IVDUIWindow {
public:
	VDUIWindow();
	~VDUIWindow();

	void *AsInterface(uint32 id);

	void Shutdown();

	IVDUIWindow *GetParent() const { return mpParent; }
	void SetParent(IVDUIWindow *);

	bool Create(IVDUIParameters *) {return true;}

	void AddChild(IVDUIWindow *pWindow);
	void RemoveChild(IVDUIWindow *pWindow);

	VDStringW GetCaption() {
		return mCaption;
	}

	void SetCaption(const VDStringW& caption) {
		mCaption = caption;
	}

	vduirect GetArea() const {
		return mArea;
	}

	void SetArea(const vduirect& pos) {
		mArea = pos;
	}

	vduirect GetClientArea() const {
		return vduirect(0, 0, mArea.width(), mArea.height());
	}

	void GetAlignment(nsVDUI::Alignment& x, nsVDUI::Alignment& y) {
		x = mAlignX;
		y = mAlignY;
	}

	void SetAlignment(nsVDUI::Alignment x, nsVDUI::Alignment y) {
		if (x)
			mAlignX = x;

		if (y)
			mAlignY = y;
	}

	const VDUILayoutSpecs& GetLayoutSpecs() { return mLayoutSpecs; }
	void Layout(const vduirect& target);
	void PreLayout(const VDUILayoutSpecs& parentConstraints);
	void PreLayoutAttempt(const VDUILayoutSpecs& parentConstraints);
	virtual void PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {}
	void PostLayout(const vduirect& target);
	virtual void PostLayoutBase(const vduirect& target) {
		SetArea(target);
	}

protected:
	typedef std::list<IVDUIWindow *> tChildren;
	tChildren	mChildren;

	IVDUIWindow	*mpParent;

	vduirect		mArea;
	vduisize		mMinSize;
	vduisize		mMaxSize;
	VDUILayoutSpecs	mLayoutSpecs;
	nsVDUI::Alignment	mAlignX;
	nsVDUI::Alignment	mAlignY;
	float			mDesiredAspectRatio;

	VDStringW	mCaption;
};

#endif
