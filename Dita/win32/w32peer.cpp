#include <string.h>
#include <stdlib.h>

#include <vd2/system/text.h>

#include "w32peer.h"

///////////////////////////////////////////////////////////////////////////

namespace nsVDUI {
	bool isWindows9x() {
		static LONG sVersion;

		if (!sVersion)
			sVersion = (LONG)GetVersion();

		return sVersion < 0;
	}

	// This, unfortunately, works for everything except 4.70.

	bool isCommonControlsAtLeast(unsigned major, unsigned minor) {
		static unsigned sVersion;

		typedef struct _DllVersionInfo
		{
			DWORD cbSize;
			DWORD dwMajorVersion;
			DWORD dwMinorVersion;
			DWORD dwBuildNumber;
			DWORD dwPlatformID;
		} DLLVERSIONINFO;


		if (!sVersion) {
			HMODULE hmod = LoadLibrary("COMCTL32.DLL");

			VDASSERT(hmod);

			sVersion = 0x40000UL;

			if (hmod) {
				typedef HRESULT (CALLBACK *tpDllGetVersion)(DLLVERSIONINFO *);

				tpDllGetVersion pFunc = (tpDllGetVersion)GetProcAddress(hmod, "DllGetVersion");

				if (pFunc) {
					DLLVERSIONINFO dvi = {sizeof(DLLVERSIONINFO)};

					if (SUCCEEDED(pFunc(&dvi))) {
						sVersion = (dvi.dwMajorVersion<<16) | dvi.dwMinorVersion;
					}
				}

				FreeLibrary(hmod);
			}
		}

		return sVersion >= ((major<<16)+minor);
	}
};

///////////////////////////////////////////////////////////////////////////

bool VDUIControlBaseW32::_Create(unsigned id, const char *klassA, const wchar_t *klassW, DWORD styles, DWORD exstyles) {
	HWND hwndParent = (HWND)GetBase()->AsControl()->GetRawHandle();

	VDASSERT(!mhwnd);

	styles ^=  WS_VISIBLE|WS_CHILD|WS_TABSTOP;
	exstyles ^= WS_EX_NOPARENTNOTIFY;

	if (nsVDUI::isWindows9x()) {
		mhwnd = CreateWindowExA(exstyles, klassA, "", styles, 0, 0, 0, 0,
			hwndParent, (HMENU)id, GetModuleHandle(NULL), NULL);
	} else {
		mhwnd = CreateWindowExW(exstyles, klassW, L"", styles, 0, 0, 0, 0,
			hwndParent, (HMENU)id, GetModuleHandle(NULL), NULL);
	}

	// No ugly-@$$ font.

	if (mhwnd) {
		SendMessage(mhwnd, WM_SETFONT, SendMessage(hwndParent, WM_GETFONT, 0, 0), FALSE);

		SetWindowLong(mhwnd, GWL_USERDATA, (LONG)this);
	}

	return mhwnd != NULL;
}

SIZE VDUIControlBaseW32::_SizeText(int nMaxWidth, int nPadWidth, int nPadHeight) throw() {
	SIZE siz = {0,0};

	if (nMaxWidth) {
		nMaxWidth -= nPadWidth;

		// Uhh, negative is bad....

		if (nMaxWidth < 1)
			nMaxWidth = 1;

	}

	if (mCaption) {
		if (nsVDUI::isWindows9x()) {
			const char *str = VDFastTextWToA(mCaption);
			HDC hdc;

			if (hdc = GetDC(mhwnd)) {
				HGDIOBJ hgoOldFont;
				RECT r={0,0,nMaxWidth};
				DWORD dwFlags = DT_LEFT|DT_TOP|DT_CALCRECT;

				if (nMaxWidth)
					dwFlags |= DT_WORDBREAK;

				hgoOldFont = SelectObject(hdc, (HGDIOBJ)SendMessage(mhwnd, WM_GETFONT, 0, 0));

				if (DrawTextA(hdc, str, -1, &r, dwFlags)) {
					siz.cx = r.right - r.left;
					siz.cy = r.bottom - r.top;
				}

				SelectObject(hdc, hgoOldFont);

				ReleaseDC(mhwnd, hdc);
			}
		} else {
			HDC hdc;

			if (hdc = GetDC(mhwnd)) {
				HGDIOBJ hgoOldFont;
				RECT r={0,0,nMaxWidth};
				DWORD dwFlags = DT_LEFT|DT_TOP|DT_CALCRECT;

				if (nMaxWidth)
					dwFlags |= DT_WORDBREAK;

				hgoOldFont = SelectObject(hdc, (HGDIOBJ)SendMessage(mhwnd, WM_GETFONT, 0, 0));

				if (DrawTextW(hdc, mCaption, -1, &r, dwFlags)) {
					siz.cx = r.right - r.left;
					siz.cy = r.bottom - r.top;
				}

				SelectObject(hdc, hgoOldFont);

				ReleaseDC(mhwnd, hdc);
			}
		}
	}

	return siz;
}

bool VDUIControlBaseW32::UpdateCaptionCache() {
	if (nsVDUI::isWindows9x()) {
		int len = SendMessageA(mhwnd, WM_GETTEXTLENGTH, 0, 0);
		int len2 = (len+16) & ~15;
		char *s = VDFastTextAllocA(len+1);

		if (!s)
			return false;

		if (!SendMessageA(mhwnd, WM_GETTEXT, len+1, (LPARAM)s)) {
			VDFastTextFree();
			return false;
		}

		if (len2 > mnCaptionBufSize || len2 < mnCaptionBufSize-128) {
			wchar_t *pNewCapt = (wchar_t *)realloc(mCaption, len2 * sizeof(wchar_t));

			if (!pNewCapt) {
				VDFastTextFree();
				return false;
			}

			mCaption = pNewCapt;
			mnCaptionBufSize = len2;
		}

		VDTextAToW(mCaption, mnCaptionBufSize, s);
		VDFastTextFree();

		return true;
	} else {
		int len = SendMessageW(mhwnd, WM_GETTEXTLENGTH, 0, 0);
		int len2 = (len+16) & ~15;

		if (len2 > mnCaptionBufSize || len2 < mnCaptionBufSize-128) {
			wchar_t *pNewCapt = (wchar_t *)realloc(mCaption, len2 * sizeof(wchar_t));

			if (!pNewCapt)
				return false;

			mCaption = pNewCapt;
			mnCaptionBufSize = len2;
		}

		if (!SendMessageW(mhwnd, WM_GETTEXT, len+1, (LPARAM)mCaption))
			return false;

		return true;
	}
}

///////////////////////////////////////////////////////////////////////////

void VDUIControlBaseW32::Destroy() {
	if (mhwnd) {
		DestroyWindow(mhwnd);
	}

	VDUIControlBase::Destroy();
}

void VDUIControlBaseW32::PostLayoutBase(const VDUIRect& target) {
	SetPosition(target);
}

void VDUIControlBaseW32::Show(bool b) {
	VDUIControlBase::Show(b);

	if (mhwnd)
		ShowWindow(mhwnd, IsActuallyVisible() ? SW_SHOW : SW_HIDE);
}

void VDUIControlBaseW32::Enable(bool b) {
	VDUIControlBase::Enable(b);

	if (mhwnd)
		EnableWindow(mhwnd, IsActuallyEnabled());
}

void VDUIControlBaseW32::SetFocus() {
	VDUIControlBase::SetFocus();

	if (mhwnd)
		::SetFocus(mhwnd);
}

void VDUIControlBaseW32::SetPosition(const VDUIRect& r) {
	SetWindowPos(mhwnd, NULL, r.x1, r.y1, r.x2-r.x1, r.y2-r.y1, SWP_NOCOPYBITS|SWP_NOZORDER|SWP_NOACTIVATE);

	VDUIControlBase::SetPosition(r);
}

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlBaseW32		Set/GetText[Length]w() support
//
///////////////////////////////////////////////////////////////////////////

void VDUIControlBaseW32::SetTextw(const wchar_t *text) throw() {
	int tlen = wcslen(text);
	int tbuflen = (tlen + 16) & ~15;

	if (tbuflen > mnCaptionBufSize || tbuflen <= mnCaptionBufSize - 128) {
		mCaption = (wchar_t *)realloc(mCaption, tbuflen * sizeof(wchar_t));
		if (!mCaption)
			return;
	}

	memcpy(mCaption, text, sizeof(wchar_t) * (tlen+1));

	if (mhwnd)
		if (nsVDUI::isWindows9x()) {
			SetWindowTextA(mhwnd, VDFastTextWToA(mCaption));
			VDFastTextFree();
		} else {
			SetWindowTextW(mhwnd, mCaption);
		}
}

int VDUIControlBaseW32::GetTextw(wchar_t *dstbuf, int max_len) throw() {
	if (max_len <= 0)
		return 0;

	if (!mCaption) {
		*dstbuf = 0;
		return 1;
	}

	int len = wcslen(mCaption) + 1;

	if (len < max_len) {
		memcpy(dstbuf, mCaption, sizeof(wchar_t) * len);
		return len;
	} else {
		memcpy(dstbuf, mCaption, sizeof(wchar_t) * (max_len - 1));
		dstbuf[max_len - 1] = 0;
		return max_len;
	}
}

int VDUIControlBaseW32::GetTextLengthw() throw() {
	return mCaption ? wcslen(mCaption) : 0;
}
