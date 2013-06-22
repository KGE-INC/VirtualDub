#ifndef f_VD2_ASUKA_SYMBOLS_H
#define f_VD2_ASUKA_SYMBOLS_H

#include <vd2/system/vdtypes.h>
#include <vector>

struct VDSymbol {
	long rva;
	int group;
	long start;
	char *name;
};

struct VDSection {
	long	mAbsStart;
	long	mStart;
	long	mLength;
	int		mGroup;

	VDSection(long s=0, long l=0, int g=0) : mStart(s), mLength(l), mGroup(g) {}
};

class VDINTERFACE IVDSymbolSource {
public:
	virtual ~IVDSymbolSource() {}

	virtual void Init(const wchar_t *filename) = 0;
	virtual const VDSymbol *LookupSymbol(uint32 addr) = 0;
	virtual const VDSection *LookupSection(uint32 addr) = 0;
	virtual void GetAllSymbols(std::vector<VDSymbol>&) = 0;

	virtual uint32 GetCodeGroupMask() = 0;

	virtual int GetSectionCount() = 0;
	virtual const VDSection *GetSection(int sec) = 0;
};

IVDSymbolSource *VDCreateSymbolSourceLinkMap();

#endif
