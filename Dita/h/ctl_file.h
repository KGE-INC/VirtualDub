#ifndef f_VD2_DITA_CTLFILE_H
#define f_VD2_DITA_CTLFILE_H

#include "control.h"

class VDUIControlFile : public VDUIControlBase {
public:
	enum {
		kTypeOpenFile,
		kTypeSaveFile,
		kTypeDirectory,
	};

private:
	IVDUIControl *mpEdit, *mpButton;
	int mnMinWidth;

public:
	VDUIControlFile(int chars);
	~VDUIControlFile();

	bool Create(IVDUIControl *pControl);
	void Destroy();

	void SetStateb(bool b) throw();
	void SetTextw(const wchar_t *text) throw();
	int GetTextw(wchar_t *dstbuf, int max_len) throw();
	int GetTextLengthw() throw();

	void PreLayoutBase(const VDUILayoutSpecs& parentConstraints);
	void PostLayoutBase(const VDUIRect& target);
};

#endif
