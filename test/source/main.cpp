#include <stdio.h>
#include <tchar.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/cpuaccel.h>

#include "test.h"

#include <vector>
#include <utility>

#if defined(_M_IX86)
	#define BUILD "80x86"
#elif defined(_M_AMD64)
	#define BUILD "AMD64"
#endif

namespace {
	typedef std::vector<std::pair<TestFn, const char *> > Tests;
	Tests g_tests;
}

void AddTest(TestFn f, const char *name) {
	g_tests.push_back(Tests::value_type(f, name));
}

int _tmain(int argc, _TCHAR **argv) {
	_tprintf(_T("VirtualDub test harness utility for ") _T(BUILD) _T("\n"));
	_tprintf(_T("Copyright (C) 2005 Avery Lee. Licensed under GNU General Public License\n\n"));

	CPUEnableExtensions(CPUCheckForExtensions());

	for(Tests::const_iterator it(g_tests.begin()), itEnd(g_tests.end()); it!=itEnd; ++it) {
		const Tests::value_type& ent = *it;

		_tprintf(_T("Running test: %hs\n"), ent.second);

		ent.first();
	}

	return 0;
}
