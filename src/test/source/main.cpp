#include <stdio.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/cpuaccel.h>
#include <vd2/system/VDString.h>
#include <vd2/system/text.h>

#include "test.h"

#include <vector>
#include <utility>

#if defined(_M_IX86)
	#define BUILD L"80x86"
#elif defined(_M_AMD64)
	#define BUILD L"AMD64"
#endif

namespace {
	typedef std::vector<std::pair<TestFn, const char *> > Tests;
	Tests g_tests;
}

void AddTest(TestFn f, const char *name) {
	g_tests.push_back(Tests::value_type(f, name));
}

void help() {
	wprintf(L"\n");
	wprintf(L"Available tests:\n");

	for(Tests::const_iterator it(g_tests.begin()), itEnd(g_tests.end()); it!=itEnd; ++it) {
		const Tests::value_type& ent = *it;

		wprintf(L"\t%hs\n", ent.second);
	}
	wprintf(L"\tAll\n");
}

int wmain(int argc, wchar_t **argv) {
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF);

	wprintf(L"VirtualDub test harness utility for " BUILD L"\n");
	wprintf(L"Copyright (C) 2005 Avery Lee. Licensed under GNU General Public License\n\n");

	CPUEnableExtensions(CPUCheckForExtensions());

	Tests selectedTests;

	if (argc <= 1) {
		help();
		exit(0);
	} else {
		for(int i=1; i<argc; ++i) {
			const wchar_t *test = argv[i];

			if (!_wcsicmp(test, L"all")) {
				selectedTests = g_tests;
				break;
			}

			for(Tests::const_iterator it(g_tests.begin()), itEnd(g_tests.end()); it!=itEnd; ++it) {
				const Tests::value_type& ent = *it;

				if (!_wcsicmp(VDTextAToW(ent.second).c_str(), test)) {
					selectedTests.push_back(ent);
					goto next;
				}
			}

			wprintf(L"\nUnknown test: %ls\n", test);
			help();
			exit(5);
next:
			;
		}
	}

	for(Tests::const_iterator it(selectedTests.begin()), itEnd(selectedTests.end()); it!=itEnd; ++it) {
		const Tests::value_type& ent = *it;

		wprintf(L"Running test: %hs\n", ent.second);

		ent.first();
	}

	return 0;
}
