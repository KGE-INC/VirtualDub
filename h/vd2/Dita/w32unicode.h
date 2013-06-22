#ifndef f_VD2_DITA_W32UNICODE_H
#define f_VD2_DITA_W32UNICODE_H

#include <windows.h>

HWND VDCreateWindowExW32(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hwndParent, HMENU hMenu, HINSTANCE hInst, LPVOID lpParam);
bool VDRegisterClassExW32(const WNDCLASSEXW *pwc);

HWND VDCreateMDIWindowW32(LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HINSTANCE hInstance, LPARAM lParam);

LRESULT VDDefWindowProcW32(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#endif
