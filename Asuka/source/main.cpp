#pragma warning(disable: 4786)		// SHUT UP

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <dos.h>
#include <time.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/error.h>

#include <string>
#include <map>
#include <vector>
#include <algorithm>

#include "utils.h"

using namespace std;

extern void tool_verinc(bool amd64);
extern void tool_lookup(const std::vector<const char *>& args, const std::vector<const char *>& switches, bool amd64);
extern void tool_mapconv(const std::vector<const char *>& args, const std::vector<const char *>& switches, bool amd64);

int main(int argc, char **argv) {
	--argc;
	++argv;

	std::vector<const char *> switches, args;
	bool amd64 = false;

	while(const char *s = *argv++) {
		if (s[0] == '/') {
			if (!stricmp(s+1, "amd64"))
				amd64 = true;
			else
				switches.push_back(s+1);
		} else {
			args.push_back(s);
		}
	}

	// look for mode
	if (args.empty())
		help();

	const char *s = args[0];

	args.erase(args.begin());

	try {
		if (!stricmp(s, "verinc")) {
			read_version();
			tool_verinc(amd64);
		} else if (!stricmp(s, "lookup"))
			tool_lookup(args, switches, amd64);
		else if (!stricmp(s, "mapconv")) {
			read_version();
			tool_mapconv(args, switches, amd64);
		} else
			help();
	} catch(const char *s) {
		fail("%s", s);
	} catch(const MyError& e) {
		fail("%s", e.gets());
	}

	return 0;
}
