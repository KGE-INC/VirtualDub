#include <vd2/system/VDString.h>
#include <vd2/system/text.h>
#include <vd2/Dita/w32unicode.h>

namespace {
	bool g_bUseUnicodeWinAPI = (LONG)GetVersion() >= 0;
};

bool VDRegisterClassExW32(const WNDCLASSEXW *pwc) {
	if (g_bUseUnicodeWinAPI)
		return !!RegisterClassExW(pwc);
	else {
		WNDCLASSEXA wca;

		VDStringA sClassName;
		VDStringA sMenuName;
		LPCTSTR lpClassNameA = (LPCTSTR)pwc->lpszClassName;
		LPCTSTR lpMenuNameA = (LPCTSTR)pwc->lpszMenuName;

		if (!IS_INTRESOURCE(lpClassNameA)) {
			sClassName = VDTextWToA(pwc->lpszClassName);
			lpClassNameA = sClassName.c_str();
		}

		if (!IS_INTRESOURCE(lpMenuNameA)) {
			sMenuName = VDTextWToA(pwc->lpszMenuName);
			lpMenuNameA = sMenuName.c_str();
		}

		wca.cbSize			= sizeof(WNDCLASSEXA);
		wca.style			= pwc->style;
		wca.lpfnWndProc		= pwc->lpfnWndProc;
		wca.cbClsExtra		= pwc->cbClsExtra;
		wca.cbWndExtra		= pwc->cbWndExtra;
		wca.hInstance		= pwc->hInstance;
		wca.hIcon			= pwc->hIcon;
		wca.hCursor			= pwc->hCursor;
		wca.hbrBackground	= pwc->hbrBackground;
		wca.lpszMenuName	= lpMenuNameA;
		wca.lpszClassName	= lpClassNameA;
		wca.hIconSm			= pwc->hIconSm;

		return !!RegisterClassExA(&wca);
	}
}

HWND VDCreateWindowExW32(DWORD dwExStyle, LPCWSTR lpClassNameW, LPCWSTR lpWindowNameW, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hwndParent, HMENU hMenu, HINSTANCE hInst, LPVOID lpParam) {
	if (g_bUseUnicodeWinAPI)
		return CreateWindowExW(dwExStyle, lpClassNameW, lpWindowNameW, dwStyle, x, y, nWidth, nHeight, hwndParent, hMenu, hInst, lpParam);
	else {
		VDStringA sClassName;
		VDStringA sWindowName;
		LPCTSTR lpClassNameA = (LPCTSTR)lpClassNameW;
		LPCTSTR lpWindowNameA = (LPCTSTR)lpWindowNameW;

		if (!IS_INTRESOURCE(lpClassNameA)) {
			sClassName = VDTextWToA(lpClassNameW);
			lpClassNameA = sClassName.c_str();
		}

		if (!IS_INTRESOURCE(lpWindowNameA)) {
			sWindowName = VDTextWToA(lpWindowNameW);
			lpWindowNameA = sWindowName.c_str();
		}

		return CreateWindowExA(dwExStyle, lpClassNameA, lpWindowNameA, dwStyle, x, y, nWidth, nHeight, hwndParent, hMenu, hInst, lpParam);
	}
}

HWND VDCreateMDIWindowW32(LPCWSTR lpClassNameW, LPCWSTR lpWindowNameW, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HINSTANCE hInstance, LPARAM lParam) {
	if (g_bUseUnicodeWinAPI)
		return CreateMDIWindowW(lpClassNameW, lpWindowNameW, dwStyle, x, y, nWidth, nHeight, hWndParent, hInstance, lParam);
	else {
		VDStringA sClassName;
		VDStringA sWindowName;
		LPCTSTR lpClassNameA = (LPCTSTR)lpClassNameW;
		LPCTSTR lpWindowNameA = (LPCTSTR)lpWindowNameW;

		if (!IS_INTRESOURCE(lpClassNameA)) {
			sClassName = VDTextWToA(lpClassNameW);
			lpClassNameA = sClassName.c_str();
		}

		if (!IS_INTRESOURCE(lpWindowNameA)) {
			sWindowName = VDTextWToA(lpWindowNameW);
			lpWindowNameA = sWindowName.c_str();
		}

		return CreateMDIWindowA(lpClassNameA, lpWindowNameA, dwStyle, x, y, nWidth, nHeight, hWndParent, hInstance, lParam);
	}
}

LRESULT VDDefWindowProcW32(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	return (g_bUseUnicodeWinAPI ? DefWindowProcW : DefWindowProcA)(hwnd, msg, wParam, lParam);
}

