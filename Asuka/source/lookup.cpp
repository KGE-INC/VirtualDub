#include <vd2/system/vdalloc.h>
#include <vd2/system/text.h>
#include <vd2/system/VDString.h>
#include <stdlib.h>
#include <vector>
#include "symbols.h"
#include "utils.h"

void VDNORETURN help_lookup() {
	printf("usage: lookup <map file> <address>\n");
	exit(5);
}

void tool_lookup(const std::vector<const char *>& args, const std::vector<const char *>& switches, bool amd64) {
	if (args.size() < 2)
		help_lookup();

	char *s;
	uint32 addr = strtoul(args[1], &s, 16);

	if (*s)
		fail("lookup: invalid address \"%s\"", args[0]);

	vdautoptr<IVDSymbolSource> pss(VDCreateSymbolSourceLinkMap());

	pss->Init(VDTextAToW(args[0]).c_str());

	const VDSymbol *sym = pss->LookupSymbol(addr);

	if (!sym)
		fail("symbol not found for address %08x", addr);

	printf("%08x   %s + %x\n", addr, sym->name, addr-sym->rva);
}
