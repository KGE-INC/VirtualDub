enum VDInstructionTypeX86 {
	kX86InstUnknown,
	kX86InstP6,
	kX86InstMMX,
	kX86InstMMX2,
	kX86InstSSE,
	kX86InstSSE2,
	kX86Inst3DNow
};

bool VDIsValidCallX86(const char *buf, int len);
VDInstructionTypeX86 VDGetInstructionTypeX86(const void *p);
