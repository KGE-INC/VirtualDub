#ifndef f_VD2_DITA_W32PEER_H
#define f_VD2_DITA_W32PEER_H

#include <windows.h>

#include "control.h"

namespace nsVDUI {
	bool isWindows9x();
	bool isCommonControlsAtLeast(unsigned major, unsigned minor);
};

class IVDUIControlNativeCallbackW32 : public IVDUIControlNativeCallback {
public:
	virtual void Dispatch_WM_COMMAND(IVDUICallback *, UINT code) throw() = 0;
	virtual void Dispatch_WM_NOTIFY(IVDUICallback *, UINT code, const NMHDR *pHdr) throw() = 0;
	virtual void Dispatch_WM_HVSCROLL(IVDUICallback *, UINT code) throw() = 0;
};

class VDUIControlBaseW32 : public VDUIControlBase {
protected:
	bool _Create(unsigned id, const char *klassA, const wchar_t *klassW, DWORD styles, DWORD exstyles=0);
	SIZE _SizeText(int nMaxWidth, int nPadWidth, int nPadHeight) throw();
	bool UpdateCaptionCache();

public:
	HWND mhwnd;
	wchar_t *mCaption;
	int mnCaptionBufSize;

	VDUIControlBaseW32() : mhwnd(0), mCaption(0), mnCaptionBufSize(0) {}

	// Destroy() default behavior: kill the Win32 control.

	virtual void Destroy();

	virtual VDGUIHandle				GetRawHandle()		{ return (VDGUIHandle)mhwnd; }

	// PostLayoutBase() default behavior: call SetPosition().

	virtual void PostLayoutBase(const VDUIRect& target);

	// SetPosition() default behavior: position the Win32 control.

	virtual void SetPosition(const VDUIRect& r);

	// Show/Enable() default behavior: change vis. and en. of W32 control.

	virtual void Show(bool b);
	virtual void Enable(bool b);

	// SetFocus() default behavior: pass through to Win32 SetFocus().

	virtual void SetFocus();

	// Set/GetText[Lengthw]() default: change window caption text.

	virtual int GetTextLengthw();
	virtual int GetTextw(wchar_t *buf, int maxlen);
	virtual void SetTextw(const wchar_t *s);
};

#endif
