#ifndef f_VD2_DITA_W32EDIT_H
#define f_VD2_DITA_W32EDIT_H

#include <windows.h>

#include "control.h"
#include "w32peer.h"

class VDUIControlEditW32 : public VDUIControlBaseW32, private IVDUIControlNativeCallbackW32, private IVDUIField {
private:
	int mnMaxChars;
	bool mbCaptionCacheDirty;
	bool mbInhibitCallbacks;

public:
	VDUIControlEditW32(int maxlen);
	~VDUIControlEditW32();

	IVDUIControlNativeCallback *AsNativeCallback();
	IVDUIField *AsUIField();

	void SetTextw(const wchar_t *text) throw();
	int GetTextw(wchar_t *dstbuf, int max_len) throw();
	int GetTextLengthw() throw();

	bool Create(IVDUIControl *pControl);
	void Destroy();
	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);

	void Select(int first, int last);

	void Dispatch_WM_COMMAND(IVDUICallback *, UINT code) throw();
	void Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw();
	void Dispatch_WM_HVSCROLL(IVDUICallback *, UINT code) throw();
};

class VDUIControlEditIntW32 : public VDUIControlBaseW32, private IVDUIControlNativeCallbackW32, private IVDUIField {
private:
	WNDPROC	mOldWndProc;
	HWND	mhwndSpin;
	int		mnMin, mnMax;
	bool	mbCaptionCacheDirty;
	bool	mbInhibitCallbacks;

	static LRESULT CALLBACK VDUIControlEditIntW32::NumericEditProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
public:
	VDUIControlEditIntW32(int minv, int maxv);
	~VDUIControlEditIntW32();

	IVDUIControlNativeCallback *AsNativeCallback();
	IVDUIField *AsUIField();

	nsVDUI::eLinkMethod GetLinkMethod();

	int GetStatei();
	void SetStatei(int i);

	bool Create(IVDUIControl *pControl);
	void Destroy();

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);
	void PostLayoutBase(const VDUIRect& target);

	void Select(int first, int last);

	void Dispatch_WM_COMMAND(IVDUICallback *, UINT code) throw();
	void Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw();
	void Dispatch_WM_HVSCROLL(IVDUICallback *, UINT code) throw();
};

#endif
