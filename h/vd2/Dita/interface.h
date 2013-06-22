#ifndef f_VD2_DITA_INTERFACE_H
#define f_VD2_DITA_INTERFACE_H

#include <ctype.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/VDRefCount.h>
#include <vd2/system/VDString.h>

namespace nsVDUI {
	enum {
		// A button created with ID = kIDPrimary gains the "default button" style.

		kIDPrimary				= 1,
		kIDOK					= 1,		// OK buttons should normally be primary.
		kIDCancel				= 2,
	};

	enum eGridDirection {
		kHorizontalDirection,
		kVerticalDirection
	};

	enum eLinkType {
		kInvalidLinkTarget		= 0,

		// Boolean links

		kLinkEnable				= 1,
		kLinkNotEnable			= 0x40000001UL,
		kLinkState				= 2,
		kLinkNotState			= 0x40000002UL,
		kLinkAndState			= 0x00000102UL,
		kLinkAndNotState		= 0x40000102UL,
		kLinkOrState			= 0x00000202UL,
		kLinkOrNotState			= 0x40000202UL,

		kLinkBoolToInt			= 0x00000003UL,		// [bool -> int] set integer = (bool?Hi:Lo)

		kLinkEndDialog			= 0x00000004UL,
		kLinkEndDialogFalse		= 0x40000004UL,

		kLinkCloseDialog		= 0x00000005UL,
		kLinkCloseDialogFalse	= 0x40000005UL,

		// Integer links

		kLinkInt				= 0x00000010UL,		// [int] set integer
		kLinkIntString			= 0x00000011UL,		// [int] set string of integer form
		kLinkIntSelectedEnable	= 0x00000012UL,		// [int -> bool] enable only if something selected
		kLinkIntRangeEnable		= 0x00000013UL,		// [int -> bool] enable only if value within [lo, hi]

		// Link flags

		kLinkTypeMask			= 0x000000FFUL,
		kLinkLoMask				= 0x0000FF00UL,
		kLinkHiMask				= 0x00FF0000UL,
		kLinkAnd				= 0x10000000UL,		// [bool] prohibits TRUE state from linking
		kLinkOr					= 0x20000000UL,		// [bool] prohibits FALSE state from linking
		kLinkInvert				= 0x40000000UL,		// [bool] invert state before linking (before and/or)
		kLinkRelink				= 0x80000000UL,		// propagate link to any modded controls

		kLinkLimit				= 0xFFFFFFFFUL
	};

	enum eLinkMethod {
		kLinkMethodStatic		= 0,
		kLinkMethodBool			= 1,
		kLinkMethodBoolOneShot	= 2,
		kLinkMethodInt			= 3,

		kLinkMethodLimit		= 0xFFFFFFFFUL,
	};

	enum eAlign {
		kAlignDefault	= 0,
		kLeft			= 1,
		kTop			= 1,
		kCenter			= 2,
		kRight			= 3,
		kBottom			= 3,
		kFill			= 4,

		kAlignTypeMask	= 0x0FF,
		kExpandFlag		= 0x100,		// allow this axis to expand to meet AR

		kAlignmentLimit = 0xFFFFFFFFUL
	};

	enum eCompressType {
		kCompressNone		= 0,
		kCompressCheckbox	= 1,
		kCompressOption		= 2,

		kCompressTypeLimit = 0xFFFFFFFFUL
	};

};

///////////////////////////////////////////////////////////////////////////

struct VDUIRect {
	int x1, y1, x2, y2;

	int w() const { return x2-x1; }
	int h() const { return y2-y1; }
};

struct VDUIPoint {
	int x, y;
};

struct VDUISize {
	int w, h;

	VDUISize() {}
	VDUISize(int _w, int _h) : w(_w), h(_h) {}
};

struct VDUILayoutSpecs {
	VDUISize minsize;			// we can't use min because some moron #define'd it :(
};

///////////////////////////////////////////////////////////////////////////

class IVDUIControl;
class IVDUIBase;
class IVDUIContext;
class IVDUIConstructor;
class IVDUIListCallback;

class VDINTERFACE IVDUIGrid {
public:
	virtual bool Add(IVDUIControl *, int x, int y, int w=1, int h=1) = 0;
	virtual void Remove(int x, int y) = 0;
	virtual void SetSpacing(int hspacing, int vspacing) = 0;
	virtual void WipeAffinities(int affinity) = 0;
	virtual void SetColumnAffinity(int col, int affinity) = 0;
	virtual void SetRowAffinity(int row, int affinity) = 0;
};

class VDINTERFACE IVDUISet {
public:
	virtual bool Add(IVDUIControl *) = 0;
	virtual void Remove(IVDUIControl *) = 0;
};

class VDINTERFACE IVDUIList {
public:
	virtual int GetItemCount() = 0;
	virtual int AddItem(const wchar_t *pEntry, bool bIncludeInAutoSize = true, void *pCookie = NULL, int nInsertBefore = -1) = 0;
	virtual void DeleteItem(int item) = 0;
	virtual void DeleteAllItems() = 0;
	virtual void *GetItemCookie(int item) = 0;
	virtual void SetItemCookie(int item, void *pCookie) = 0;
	virtual void SetItemText(int item, const wchar_t *pText) = 0;

	// Used only by the ListView -- ignored by other lists.

	virtual void AddColumn(const wchar_t *pText, int width_units) = 0;
	virtual void UpdateItem(int item) = 0;
	virtual void SetSource(IVDUIListCallback *) = 0;
	virtual void Sort() = 0;
};

class VDINTERFACE IVDUIField {
public:
	virtual void Select(int start, int end) = 0;
};

class VDINTERFACE IVDUISlider {
public:
	virtual void SetRange(int begin, int end) = 0;
};

///////////////////////////////////////////////////////////////////////////

class VDINTERFACE IVDUICallback {
public:
	enum eEventType {
		kEventNone,
		kEventSelect,
		kEventDoubleClick,
		kEventClose,
		kEventDestroy
	};

	virtual bool UIConstructModal(IVDUIContext *, IVDUIBase *)=0;
	virtual void UIEvent(IVDUIControl *pControl, unsigned id, eEventType type, int item) = 0;
};

class VDINTERFACE IVDUIControlNativeCallback {};

class VDINTERFACE IVDUIListCallback {
public:
	virtual const wchar_t *UIListTextRequest(IVDUIControl *, int item, int subitem, bool& bPersistent) = 0;
	virtual int UIListSortRequest(IVDUIControl *, void *p1, void *p2) = 0;
};

class VDINTERFACE IVDUIControl {
public:

	// Attempt to create the control -- this is called by the parent on an Add.

	virtual bool Create(IVDUIControl *pControl) = 0;

	// The control gets killed immediately when you call this.

	virtual void Destroy() = 0;

	// IDs make it much easier to find a control.  You should not attempt to
	// change a control's ID once it has been added.

	virtual unsigned GetID() = 0;
	virtual void SetID(unsigned) = 0;

	virtual void GetAlignment(nsVDUI::eAlign&, nsVDUI::eAlign&) = 0;
	virtual void SetAlignment(nsVDUI::eAlign, nsVDUI::eAlign) = 0;

	virtual void GetMinimumSize(VDUISize&) = 0;
	virtual void SetMinimumSize(const VDUISize&)=0;
	virtual void GetMaximumSize(VDUISize&) = 0;
	virtual void SetMaximumSize(const VDUISize&)=0;

	virtual float GetDesiredAspectRatio()=0;
	virtual void SetDesiredAspectRatio(float rAspect)=0;

	// The parent of a control is its place in the layout hierarchy; in the
	// case of a dialog, the OS window hierarchy is actually flat.  GetBase()
	// will return the UI base control that handles mapping, linking, etc.
	//
	// If the control is parented to an OS window, but not a VD window,
	// GetParent() will return NULL.

	virtual IVDUIBase *GetBase() = 0;
	virtual IVDUIControl *GetParent() = 0;

	// A control may itself be a base (Win32: nested dialogs).

	virtual IVDUIBase *AsBase() = 0;

	// These retrieve specialized interfaces for common controls.

	virtual IVDUIGrid *AsUIGrid() = 0;
	virtual IVDUISet *AsUISet() = 0;
	virtual IVDUIList *AsUIList() = 0;
	virtual IVDUIField *AsUIField() = 0;
	virtual IVDUISlider *AsUISlider() = 0;

	// This is necessary to propagate notification messages that are passed
	// from native controls.

	virtual IVDUIControlNativeCallback *AsNativeCallback() = 0;

	// Obtain the raw, OS-specific UI handle for this control.  May be NULL
	// if no OS control exists.  Destroying this control is extremely
	// inadvisable.  Actually, using this function is inadvisable.

	virtual VDGUIHandle GetRawHandle() = 0;

	// The layout system propagates PreLayout() calls down the tree to
	// obtain specs, then propagates PostLayout() to position controls.
	// Containers that have controls with dependent axes, like wrappable
	// controls, may in turn re-fire PreLayout() to children inside their
	// PostLayout() call, in order to better fit controls.

	virtual const VDUILayoutSpecs& GetLayoutSpecs() = 0;
	virtual void PreLayout(const VDUILayoutSpecs& parentConstraints) = 0;
	virtual void PostLayout(const VDUIRect& target) = 0;

	// Get/SetPosition are in pixels.

	virtual void GetPosition(VDUIRect& r)=0;
	virtual void SetPosition(const VDUIRect& r)=0;

	// Visibility and enabled (grayed) state.  Each control has its own
	// visibility and enabled states, but these states can be overriden
	// by falses in the parent.  The *Actually*() API allows you to tell
	// if this is the case.

	virtual bool IsVisible()=0;
	virtual bool IsActuallyVisible()=0;
	virtual void Show(bool b)=0;
	virtual bool IsEnabled()=0;
	virtual bool IsActuallyEnabled()=0;
	virtual void Enable(bool b)=0;

	// Select control.
	virtual void SetFocus() = 0;

	// Compress types allow adjacent controls of the same type to appear
	// closer together to each other when appropriate.

	virtual nsVDUI::eCompressType GetCompressType()=0;

	// By default, set/get primitives do not cause links to fire, in order
	// to avoid infinite loops in the case of reciprocal linking.  Use
	// ProcessLinks() to force outgoing links to be activated.

	virtual nsVDUI::eLinkMethod GetLinkMethod()=0;
	virtual void ProcessLinks()=0;

	// Generic get/set primitives.

	virtual bool GetStateb()=0;
	virtual void SetStateb(bool b)=0;
	virtual int GetStatei()=0;
	virtual void SetStatei(int i)=0;
	virtual int GetTextLengthw()=0;
	virtual int GetTextw(wchar_t *buf, int maxlen)=0;
	virtual void SetTextw(const wchar_t *s)=0;
};

class VDINTERFACE IVDUIBase {
public:
	virtual IVDUIControl *AsControl()=0;

	virtual void SetCallback(IVDUICallback *pCB)=0;
	virtual void EndModal(int rv)=0;

	virtual void MapUnitsToPixels(VDUIRect& r)=0;
	virtual void MapScreenToClient(VDUIPoint& pt)=0;
	virtual void MapScreenToClient(VDUIRect& pt)=0;
	virtual void MapClientToScreen(VDUIPoint& pt)=0;
	virtual void MapClientToScreen(VDUIRect& pt)=0;

	virtual IVDUIControl *GetControl(unsigned) = 0;
	virtual bool Add(IVDUIControl *) = 0;
	virtual void AddNonlocal(IVDUIControl *) = 0;
	virtual void Remove(IVDUIControl *) = 0;
	virtual void SetLayoutControl(IVDUIControl *) = 0;
	virtual void Relayout() = 0;

	virtual unsigned CreateUniqueID() = 0;

	virtual void ProcessLinksID(unsigned from_id) = 0;
	virtual void ProcessLinksPtr(IVDUIControl *) = 0;
	virtual void ProcessLinksToStatics() = 0;
	virtual void ProcessAllLinks() = 0;

	virtual void Link(unsigned dst_id, nsVDUI::eLinkType type, unsigned src_id) = 0;
	virtual void Link(IVDUIControl *pControlB, nsVDUI::eLinkType type, IVDUIControl *pControlA) = 0;
	virtual void Unlink(unsigned dst_id, nsVDUI::eLinkType type, unsigned src_id) = 0;
	virtual void Unlink(unsigned dst_id) = 0;
	virtual void Unlink(IVDUIControl *) = 0;
};

class VDINTERFACE IVDUIContext : public IVDRefCount {
public:
	virtual int DoModalDialog(VDGUIHandle parent, IVDUICallback *) = 0;

	virtual IVDUIControl *CreateBase() = 0;
	virtual IVDUIControl *CreateModelessDialog() = 0;
	virtual IVDUIControl *CreateChildDialog() = 0;
	virtual IVDUIControl *CreateLabel(int maxlen) = 0;
	virtual IVDUIControl *CreateEdit(int maxlen) = 0;
	virtual IVDUIControl *CreateEditInt(int minv, int maxv) = 0;
	virtual IVDUIControl *CreateButton() = 0;
	virtual IVDUIControl *CreateCheckbox() = 0;
	virtual IVDUIControl *CreateOption(IVDUIControl *) = 0;
	virtual IVDUIControl *CreateListbox(int minrows) = 0;
	virtual IVDUIControl *CreateCombobox(int minrows) = 0;
	virtual IVDUIControl *CreateListView(int minrows) = 0;
	virtual IVDUIControl *CreateTrackbar(int minv, int maxv) = 0;
	virtual IVDUIControl *CreateFileControl(int maxlen) = 0;

	virtual IVDUIControl *CreateGroup() = 0;
	virtual IVDUIControl *CreateHorizontalSet() = 0;
	virtual IVDUIControl *CreateVerticalSet() = 0;
	virtual IVDUIControl *CreateGrid(int w, int h) = 0;

	virtual IVDUIConstructor *CreateConstructor(IVDUIControl *pControl) = 0;

	virtual bool ExecuteDialogResource(IVDUIBase *pBase, int moduleID, int dialogID) = 0;
	virtual bool ExecuteTemplateResource(IVDUIBase *pBase, int moduleID, int dialogID) = 0;
};

class VDINTERFACE IVDUIConstructor : public IVDRefCount {
public:
	typedef const wchar_t		*tCSW;

	virtual void Init(IVDUIContext *pContext, IVDUIControl *pBase) = 0;

	// Error detection

	virtual bool GetErrorState	() = 0;

	// Accessors

	virtual IVDUIControl *GetLastControl() = 0;

	// Modifiers

	virtual void SetAlignment	(int, int) = 0;
	virtual void SetMinimumSize	(int, int) = 0;
	virtual void SetMaximumSize	(int, int) = 0;

	// Basic controls

	virtual void AddLabel		(unsigned id, tCSW label, int maxwidth) = 0;
	virtual void AddEdit		(unsigned id, tCSW label, int maxlen) = 0;
	virtual void AddEditInt		(unsigned id, int initv, int minv, int maxv) = 0;
	virtual void AddButton		(unsigned id, tCSW label) = 0;
	virtual void AddCheckbox	(unsigned id, tCSW label) = 0;
	virtual void AddListbox		(unsigned id, int minrows) = 0;
	virtual void AddCombobox	(unsigned id, int minrows) = 0;
	virtual void AddListView	(unsigned id, int minrows) = 0;
	virtual void AddTrackbar	(unsigned id, int minv, int maxv) = 0;
	virtual void AddFileControl	(unsigned id, tCSW label, int maxlen) = 0;

	// Grouped controls

	virtual void BeginOptionSet	(unsigned id) = 0;
	virtual void AddOption		(tCSW label) = 0;
	virtual void EndOptionSet	() = 0;

	// Layout sets

	virtual void BeginHorizSet	(unsigned id) = 0;
	virtual void BeginVertSet	(unsigned id) = 0;
	virtual void BeginGroupSet	(unsigned id, tCSW label) = 0;
	virtual void EndSet			() = 0;

	// Grids

	virtual void BeginGrid		(unsigned id, int cols, int rows, int xpad = -1, int ypad = -1, int default_affinity = -1) = 0;
	virtual void SetGridDirection(nsVDUI::eGridDirection) = 0;
	virtual void SpanNext		(int w, int h) = 0;
	virtual void SetGridPos		(int x, int y) = 0;
	virtual void SkipGrid		(int x, int y) = 0;
	virtual void SetGridColumnAffinity(int x, int affinity) = 0;
	virtual void SetGridRowAffinity(int y, int affinity) = 0;
	virtual void EndGrid		() = 0;
};

IVDUIContext *VDCreateUIContext();
IVDUIConstructor *VDCreateUIConstructor();

class VDAppDialogBaseImpl : public IVDUICallback {
public:
	VDAppDialogBaseImpl(int id, bool bTemplate);
	virtual ~VDAppDialogBaseImpl();

	bool UIConstructModal(IVDUIContext *pContext, IVDUIBase *pBase);

	void UIEvent(IVDUIControl *pBase, unsigned id, IVDUICallback::eEventType ev, int item);

	virtual void Init();
	virtual void Event(IVDUIControl *pBase, unsigned id, IVDUICallback::eEventType ev, int item);
	virtual void Cancel();
	virtual bool Writeback();

protected:
	IVDUIBase *mpBase;

	bool GetB(unsigned id) const;
	int GetI(unsigned id) const;
	VDStringW GetW(unsigned id) const;
	void SetB(unsigned id, bool b) const;
	void SetI(unsigned id, int v) const;
	void SetW(unsigned id, const wchar_t *s) const;
	void SetW(unsigned id, unsigned strid) const;
	void SetWF(unsigned id, unsigned strid, int args, ...) const;
	void Enable(unsigned id, bool b) const;

private:
	const int mDialogID;	// should always use kDialogID instead
	const bool mbTemplate;
};

template<int T_DialogID>
class VDAppDialogBase : public VDAppDialogBaseImpl {
public:
	enum { kDialogID = T_DialogID };

	VDAppDialogBase() : VDAppDialogBaseImpl(T_DialogID, false) {}
};

template<int T_DialogID>
class VDAppTemplateBase : public VDAppDialogBaseImpl {
public:
	enum { kDialogID = T_DialogID };

	VDAppTemplateBase() : VDAppDialogBaseImpl(T_DialogID, true) {}
};


#endif
