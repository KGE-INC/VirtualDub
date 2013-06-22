#include "ctl_file.h"
#include <vd2/Dita/services.h>

///////////////////////////////////////////////////////////////////////////
//
//	VDUIControlFile
//
///////////////////////////////////////////////////////////////////////////

VDUIControlFile::VDUIControlFile(int chars)
	: mpEdit(NULL)
	, mpButton(NULL)
	, mnMinWidth(chars)
{
}

VDUIControlFile::~VDUIControlFile() {
}

bool VDUIControlFile::Create(IVDUIControl *pControl) {
	if (VDUIControlBase::Create(pControl)) {
		IVDUIContext *pContext = VDCreateUIContext();
		bool bSuccess = false;

		if (pContext) {
			pContext->AddRef();

			// Try to create edit control.

			mpEdit = pContext->CreateEdit(mnMinWidth);
			mpButton = pContext->CreateButton();

			if (mpEdit && mpButton) {
				if (mpEdit->Create(this)) {
					if (mpButton->Create(this)) {
						bSuccess = true;

						mpEdit->SetAlignment(nsVDUI::kFill, nsVDUI::kFill);
						mpButton->SetAlignment((nsVDUI::eAlign)(nsVDUI::kFill | nsVDUI::kExpandFlag), nsVDUI::kFill);
						mpButton->SetTextw(L"\x2026");

						GetBase()->Link(this, nsVDUI::kLinkState, mpButton);
					}
				}
			}

			pContext->Release();
		}

		if (!bSuccess) {
			if (mpEdit) {
				mpEdit->Destroy();
				mpEdit = NULL;
			}

			if (mpButton) {
				mpButton->Destroy();
				mpButton = NULL;
			}
		}

		return bSuccess;
	}

	return false;
}

void VDUIControlFile::Destroy() {
	if (mpEdit)
		mpEdit->Destroy();

	if (mpButton)
		mpButton->Destroy();

	VDUIControlBase::Destroy();
}

void VDUIControlFile::SetStateb(bool b) throw() {
	const VDStringW pspec(VDGetDirectory(0, GetBase()->AsControl()->GetRawHandle(), L"Select path"));

	if (!pspec.empty())
		SetTextw(pspec.c_str());
}

void VDUIControlFile::SetTextw(const wchar_t *text) throw() {
	mpEdit->SetTextw(text);
}

int VDUIControlFile::GetTextw(wchar_t *dstbuf, int max_len) throw() {
	return mpEdit->GetTextw(dstbuf, max_len);
}

int VDUIControlFile::GetTextLengthw() throw() {
	return mpEdit->GetTextLengthw();
}

void VDUIControlFile::PreLayoutBase(const VDUILayoutSpecs& parentConstraints) {
	mpEdit->PreLayout(parentConstraints);
	mpButton->PreLayout(parentConstraints);

	const VDUILayoutSpecs& especs = mpEdit->GetLayoutSpecs();
	const VDUILayoutSpecs& bspecs = mpButton->GetLayoutSpecs();

	mLayoutSpecs.minsize.w = especs.minsize.w + bspecs.minsize.w;
	mLayoutSpecs.minsize.h = especs.minsize.h;
	if (mLayoutSpecs.minsize.h < bspecs.minsize.h)
		mLayoutSpecs.minsize.h = bspecs.minsize.h;
}

void VDUIControlFile::PostLayoutBase(const VDUIRect& target) {
	const VDUILayoutSpecs& bspecs = mpButton->GetLayoutSpecs();

	VDUIRect r1 = { target.x1, target.y1, target.x2 - bspecs.minsize.w, target.y2 };
	VDUIRect r2 = { target.x2 - bspecs.minsize.w, target.y1, target.x2, target.y2 };

	mpEdit->PostLayout(r1);
	mpButton->PostLayout(r2);
}

