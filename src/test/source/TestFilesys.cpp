#include <vd2/system/filesys.h>
#include "test.h"

DEFINE_TEST(Filesys) {
	// WILDCARD TESTS

	// Basic non-wildcard tests
	VDASSERT(VDFileWildMatch(L"", L"random.bin") == false);
	VDASSERT(VDFileWildMatch(L"random.bin", L"random.bin") == true);
	VDASSERT(VDFileWildMatch(L"random.bin", L"randum.bin") == false);
	VDASSERT(VDFileWildMatch(L"random.bin", L"randum.bi") == false);
	VDASSERT(VDFileWildMatch(L"random.bin", L"randum.binx") == false);
	VDASSERT(VDFileWildMatch(L"random.bin", L"RANDOM.BIN") == true);
	VDASSERT(VDFileWildMatch(L"random.bin", L"xrandom.bin") == false);

	// ? tests
	VDASSERT(VDFileWildMatch(L"random.b?n", L"random.bin") == true);
	VDASSERT(VDFileWildMatch(L"random.bin?", L"random.bin") == false);
	VDASSERT(VDFileWildMatch(L"?random.bin", L"random.bin") == false);

	// * tests
	VDASSERT(VDFileWildMatch(L"*", L"random.bin") == true);
	VDASSERT(VDFileWildMatch(L"random*", L"random.bin") == true);
	VDASSERT(VDFileWildMatch(L"random.bin*", L"random.bin") == true);
	VDASSERT(VDFileWildMatch(L"*random.bin", L"random.bin") == true);
	VDASSERT(VDFileWildMatch(L"random*.bin", L"random.bin") == true);
	VDASSERT(VDFileWildMatch(L"random*bin", L"random.bin") == true);
	VDASSERT(VDFileWildMatch(L"random**bin", L"random.bin") == true);
	VDASSERT(VDFileWildMatch(L"random*bin", L"random.bin.bin") == true);
	VDASSERT(VDFileWildMatch(L"random*bin", L"random.ban.bin") == true);
	VDASSERT(VDFileWildMatch(L"ran*?*bin", L"random.bin") == true);
	VDASSERT(VDFileWildMatch(L"*.bin", L"random.bin") == true);
	VDASSERT(VDFileWildMatch(L"*n*", L"random.bin") == true);
	VDASSERT(VDFileWildMatch(L"*om*and*", L"random.bin") == false);

	return 0;
}

