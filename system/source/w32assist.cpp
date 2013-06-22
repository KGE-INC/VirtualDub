//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2004 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#include <vd2/system/w32assist.h>
#include <vd2/system/text.h>
#include <vd2/system/vdstl.h>

int VDGetSizeOfBitmapHeaderW32(const BITMAPINFOHEADER *pHdr) {
	int size = pHdr->biSize + pHdr->biClrUsed * sizeof(RGBQUAD);

	if (pHdr->biSize < sizeof(BITMAPV4HEADER) && pHdr->biCompression == BI_BITFIELDS)
		size += sizeof(DWORD) * 3;

	return size;
}

void VDSetWindowTextW32(HWND hwnd, const wchar_t *s) {
	if (VDIsWindowsNT()) {
		SetWindowTextW(hwnd, s);
	} else {
		SetWindowTextA(hwnd, VDTextWToA(s).c_str());
	}
}

VDStringW VDGetWindowTextW32(HWND hwnd) {
	union {
		wchar_t w[256];
		char a[512];
	} buf;

	if (VDIsWindowsNT()) {
		int len = GetWindowTextLengthW(hwnd);

		if (len > 255) {
			vdblock<wchar_t> tmp(len + 1);
			len = GetWindowTextW(hwnd, tmp.data(), tmp.size());

			VDStringW text(tmp.data(), len);
			return text;
		} else if (len > 0) {
			len = GetWindowTextW(hwnd, buf.w, 256);

			VDStringW text(buf.w, len);
			return text;
		}
	} else {
		int len = GetWindowTextLengthA(hwnd);

		if (len > 511) {
			vdblock<char> tmp(len + 1);
			len = GetWindowTextA(hwnd, tmp.data(), tmp.size());

			VDStringW text(VDTextAToW(tmp.data(), len));
			return text;
		} else if (len > 0) {
			len = GetWindowTextA(hwnd, buf.a, 512);

			VDStringW text(VDTextAToW(buf.a, len));
			return text;
		}
	}

	return VDStringW();
}

void VDCheckMenuItemByCommandW32(HMENU hmenu, UINT cmd, bool checked) {
	CheckMenuItem(hmenu, cmd, checked ? MF_BYCOMMAND|MF_CHECKED : MF_BYCOMMAND|MF_UNCHECKED);
}

void VDEnableMenuItemByCommandW32(HMENU hmenu, UINT cmd, bool checked) {
	EnableMenuItem(hmenu, cmd, checked ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
}

LRESULT VDDualDefWindowProcW32(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return IsWindowUnicode(hwnd) ? DefWindowProcW(hwnd, msg, wParam, lParam) : DefWindowProcA(hwnd, msg, wParam, lParam);
}

EXECUTION_STATE VDSetThreadExecutionStateW32(EXECUTION_STATE esFlags) {
	EXECUTION_STATE es = 0;

	// SetThreadExecutionState(): requires Windows 98+/2000+.
	typedef EXECUTION_STATE (WINAPI *tSetThreadExecutionState)(EXECUTION_STATE);
	static tSetThreadExecutionState pFunc = (tSetThreadExecutionState)GetProcAddress(GetModuleHandle("kernel32"), "SetThreadExecutionState");

	if (pFunc)
		es = pFunc(esFlags);

	return es;
}
