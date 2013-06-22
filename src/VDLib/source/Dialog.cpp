#include "stdafx.h"
#include <windows.h>
#include <vd2/system/w32assist.h>
#include <vd2/VDLib/Dialog.h>

extern HINSTANCE g_hInst;

VDDialogFrameW32::VDDialogFrameW32(uint32 dlgid)
	: mpDialogResourceName(MAKEINTRESOURCE(dlgid))
	, mhdlg(NULL)
{
}

bool VDDialogFrameW32::Create(VDGUIHandle parent) {
	if (!mhdlg) {
		if (VDIsWindowsNT())
			CreateDialogParamW(g_hInst, IS_INTRESOURCE(mpDialogResourceName) ? (LPCWSTR)mpDialogResourceName : VDTextAToW(mpDialogResourceName).c_str(), (HWND)parent, StaticDlgProc, (LPARAM)this);
		else
			CreateDialogParamA(g_hInst, mpDialogResourceName, (HWND)parent, StaticDlgProc, (LPARAM)this);
	}

	return mhdlg != NULL;
}

void VDDialogFrameW32::Destroy() {
	if (mhdlg)
		DestroyWindow(mhdlg);
}

sintptr VDDialogFrameW32::ShowDialog(VDGUIHandle parent) {
	if (VDIsWindowsNT())
		return DialogBoxParamW(g_hInst, IS_INTRESOURCE(mpDialogResourceName) ? (LPCWSTR)mpDialogResourceName : VDTextAToW(mpDialogResourceName).c_str(), (HWND)parent, StaticDlgProc, (LPARAM)this);
	else
		return DialogBoxParamA(g_hInst, mpDialogResourceName, (HWND)parent, StaticDlgProc, (LPARAM)this);
}

void VDDialogFrameW32::Show() {
	if (mhdlg)
		ShowWindow(mhdlg, SW_SHOWNA);
}

void VDDialogFrameW32::Hide() {
	if (mhdlg)
		ShowWindow(mhdlg, SW_HIDE);
}

void VDDialogFrameW32::End(sintptr result) {
	EndDialog(mhdlg, result);
	mhdlg = NULL;
}

void VDDialogFrameW32::SetFocusToControl(uint32 id) {
	HWND hwnd = GetDlgItem(mhdlg, id);
	if (hwnd)
		SendMessage(mhdlg, WM_NEXTDLGCTL, (WPARAM)hwnd, TRUE);
}

void VDDialogFrameW32::EnableControl(uint32 id, bool enabled) {
	HWND hwnd = GetDlgItem(mhdlg, id);
	if (hwnd)
		EnableWindow(mhdlg, enabled);
}

void VDDialogFrameW32::SetControlText(uint32 id, const wchar_t *s) {
	HWND hwnd = GetDlgItem(mhdlg, id);
	if (hwnd)
		VDSetWindowTextW32(hwnd, s);
}

void VDDialogFrameW32::SetControlTextF(uint32 id, const wchar_t *format, ...) {
	HWND hwnd = GetDlgItem(mhdlg, id);
	if (hwnd) {
		VDStringW s;
		va_list val;

		va_start(val, format);
		s.append_vsprintf(format, val);
		va_end(val);

		VDSetWindowTextW32(hwnd, s.c_str());
	}
}

uint32 VDDialogFrameW32::GetControlValueUint32(uint32 id) {
	HWND hwnd = GetDlgItem(mhdlg, id);
	if (!hwnd) {
		FailValidation(id);
		return 0;
	}

	VDStringW s(VDGetWindowTextW32(hwnd));
	unsigned val;
	wchar_t tmp;
	if (1 != swscanf(s.c_str(), L" %u %c", &val, &tmp)) {
		FailValidation(id);
		return 0;
	}

	return val;
}

double VDDialogFrameW32::GetControlValueDouble(uint32 id) {
	HWND hwnd = GetDlgItem(mhdlg, id);
	if (!hwnd) {
		FailValidation(id);
		return 0;
	}

	VDStringW s(VDGetWindowTextW32(hwnd));
	double val;
	wchar_t tmp;
	if (1 != swscanf(s.c_str(), L" %lg %c", &val, &tmp)) {
		FailValidation(id);
		return 0;
	}

	return val;
}

void VDDialogFrameW32::ExchangeControlValueDouble(bool write, uint32 id, const wchar_t *format, double& val, double minVal, double maxVal) {
	if (write) {
		val = GetControlValueDouble(id);
		if (val < minVal || val > maxVal)
			FailValidation(id);
	} else {
		SetControlTextF(id, format, val);
	}
}

void VDDialogFrameW32::CheckButton(uint32 id, bool checked) {
	CheckDlgButton(mhdlg, id, checked ? BST_CHECKED : BST_UNCHECKED);
}

bool VDDialogFrameW32::IsButtonChecked(uint32 id) {
	return IsDlgButtonChecked(mhdlg, id) != 0;
}

void VDDialogFrameW32::BeginValidation() {
	mbValidationFailed = false;
}

bool VDDialogFrameW32::EndValidation() {
	if (mbValidationFailed) {
		SignalFailedValidation(mFailedId);
		return false;
	}

	return true;
}

void VDDialogFrameW32::FailValidation(uint32 id) {
	mbValidationFailed = true;
	mFailedId = id;
}

void VDDialogFrameW32::SignalFailedValidation(uint32 id) {
	HWND hwnd = GetDlgItem(mhdlg, id);

	MessageBeep(MB_ICONEXCLAMATION);
	if (hwnd)
		SetFocus(hwnd);
}

sint32 VDDialogFrameW32::LBGetSelectedIndex(uint32 id) {
	return SendDlgItemMessage(mhdlg, id, LB_GETCURSEL, 0, 0);
}

void VDDialogFrameW32::LBSetSelectedIndex(uint32 id, sint32 idx) {
	SendDlgItemMessage(mhdlg, id, LB_SETCURSEL, idx, 0);
}

void VDDialogFrameW32::LBAddString(uint32 id, const wchar_t *s) {
	if (VDIsWindowsNT()) {
		SendDlgItemMessageW(mhdlg, id, LB_ADDSTRING, 0, (LPARAM)s);
	} else {
		SendDlgItemMessageA(mhdlg, id, LB_ADDSTRING, 0, (LPARAM)VDTextWToA(s).c_str());		
	}
}

void VDDialogFrameW32::LBAddStringF(uint32 id, const wchar_t *format, ...) {
	VDStringW s;
	va_list val;

	va_start(val, format);
	s.append_vsprintf(format, val);
	va_end(val);

	LBAddString(id, s.c_str());
}

void VDDialogFrameW32::OnDataExchange(bool write) {
}

bool VDDialogFrameW32::OnLoaded() {
	OnDataExchange(false);
	return false;
}

bool VDDialogFrameW32::OnOK() {
	BeginValidation();
	OnDataExchange(true);
	return !EndValidation();
}

bool VDDialogFrameW32::OnCancel() {
	return false;
}

bool VDDialogFrameW32::OnCommand(uint32 id, uint32 extcode) {
	return false;
}

bool VDDialogFrameW32::PreNCDestroy() {
	return false;
}

VDZINT_PTR VDZCALLBACK VDDialogFrameW32::StaticDlgProc(VDZHWND hwnd, VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) {
	VDDialogFrameW32 *pThis = (VDDialogFrameW32 *)GetWindowLongPtr(hwnd, DWLP_USER);

	if (msg == WM_INITDIALOG) {
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		pThis = (VDDialogFrameW32 *)lParam;
		pThis->mhdlg = hwnd;
	} else if (msg == WM_NCDESTROY) {
		if (pThis) {
			bool deleteMe = pThis->PreNCDestroy();

			pThis->mhdlg = NULL;
			SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)(void *)NULL);

			if (deleteMe)
				delete pThis;

			pThis = NULL;
			return FALSE;
		}
	}

	return pThis ? pThis->DlgProc(msg, wParam, lParam) : FALSE;
}

VDZINT_PTR VDDialogFrameW32::DlgProc(VDZUINT msg, VDZWPARAM wParam, VDZLPARAM lParam) {
	switch(msg) {
		case WM_INITDIALOG:
			return !OnLoaded();

		case WM_COMMAND:
			{
				uint32 id = LOWORD(wParam);

				if (id == IDOK) {
					if (!OnOK())
						End(true);

					return TRUE;
				} else if (id == IDCANCEL) {
					if (!OnCancel())
						End(false);

					return TRUE;
				} else {
					if (OnCommand(id, HIWORD(wParam)))
						return TRUE;
				}
			}

			break;
	}

	return FALSE;
}
