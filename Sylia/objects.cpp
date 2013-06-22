#include <windows.h>

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

namespace {
#define FUNC(name) void name(IVDScriptInterpreter *isi, VDScriptValue *argv, int argc)

	FUNC(dprint) {
		char lbuf[12];

		while(argc--) {
			if (argv->isInt()) {
				wsprintf(lbuf, "%ld", argv->asInt());
				OutputDebugString(lbuf);
			} else if (argv->isString()) {
				OutputDebugString(*argv->asString());
			} else
				SCRIPT_ERROR(TYPE_INT_REQUIRED);

			++argv;
		}
	}

	FUNC(messagebox) {
		MessageBox(NULL, *argv[0].asString(), *argv[1].asString(), MB_OK);
	}

	FUNC(IntToString) {
		char buf[32];

		if ((unsigned)_snprintf(buf, sizeof buf, "%d", argv[0].asInt()) > 31)
			buf[0] = 0;

		argv[0] = isi->DupCString(buf);
	}

	FUNC(LongToString) {
		char buf[32];

		if ((unsigned)_snprintf(buf, sizeof buf, "%I64d", argv[0].asLong()) > 31)
			buf[0] = 0;

		argv[0] = isi->DupCString(buf);
	}

	FUNC(DoubleToString) {
		char buf[256];

		if ((unsigned)_snprintf(buf, sizeof buf, "%g", argv[0].asDouble()) > 255)
			buf[0] = 0;

		argv[0] = isi->DupCString(buf);
	}

	FUNC(StringToString) {
		// don't need to do anything
	}

	FUNC(Atoi) {
		char *s = *argv[0].asString();

		while(*s == ' ')
			++s;

		errno = 0;
		int v = (int)strtol(s, &s, 0);

		if (errno && v != 0)
			SCRIPT_ERROR(NUMERIC_OVERFLOW);

		while(*s == ' ')
			++s;

		if (*s)
			SCRIPT_ERROR(STRING_NOT_AN_INTEGER_VALUE);

		argv[0] = (int)v;
	}

	FUNC(Atol) {
		const char *s = *argv[0].asString();
		char dummy;
		sint64 val;

		int result = sscanf(s, " %I64d %c", &val, &dummy);

		if (result != 1)
			SCRIPT_ERROR(STRING_NOT_AN_INTEGER_VALUE);

		argv[0] = val;
	}

	FUNC(Atod) {
		char *s = *argv[0].asString();

		while(*s == ' ')
			++s;

		errno = 0;
		double v = strtod(s, &s);

		if (errno && v != 0)
			SCRIPT_ERROR(NUMERIC_OVERFLOW);

		while(*s == ' ')
			++s;

		if (*s)
			SCRIPT_ERROR(STRING_NOT_A_REAL_VALUE);

		argv[0] = v;
	}

	FUNC(add_int) {	argv[0] = argv[0].asInt() + argv[1].asInt(); }
	FUNC(add_long) { argv[0] = argv[0].asLong() + argv[1].asLong(); }
	FUNC(add_double) { argv[0] = argv[0].asDouble() + argv[1].asDouble(); }

	FUNC(add_string) {
		int l1 = strlen(*argv[0].asString());
		int l2 = strlen(*argv[1].asString());

		char **pp = isi->AllocTempString(l1+l2);

		memcpy(*pp, *argv[0].asString(), l1);
		memcpy(*pp + l1, *argv[1].asString(), l2);

		argv[0] = VDScriptValue(pp);
	}

	FUNC(sub_int) { argv[0] = argv[0].asInt() - argv[1].asInt(); }
	FUNC(sub_long) { argv[0] = argv[0].asLong() - argv[1].asLong(); }
	FUNC(sub_double) { argv[0] = argv[0].asDouble() - argv[1].asDouble(); }

	FUNC(mul_int) { argv[0] = argv[0].asInt() * argv[1].asInt(); }
	FUNC(mul_long) { argv[0] = argv[0].asLong() * argv[1].asLong(); }
	FUNC(mul_double) { argv[0] = argv[0].asDouble() * argv[1].asDouble(); }

	FUNC(div_int) {
		if (!argv[1].asInt())
			SCRIPT_ERROR(DIVIDE_BY_ZERO);
		argv[0] = argv[0].asInt() / argv[1].asInt();
	}

	FUNC(div_long) {
		if (!argv[1].asLong())
			SCRIPT_ERROR(DIVIDE_BY_ZERO);
		argv[0] = argv[0].asLong() / argv[1].asLong();
	}

	FUNC(div_double) {
		if (argv[1].asDouble() == 0.0)
			SCRIPT_ERROR(DIVIDE_BY_ZERO);
		argv[0] = argv[0].asDouble() / argv[1].asDouble();
	}

	FUNC(mod_int) {
		if (!argv[1].asInt())
			SCRIPT_ERROR(DIVIDE_BY_ZERO);
		argv[0] = argv[0].asInt() % argv[1].asInt();
	}

	FUNC(mod_long) {
		if (!argv[1].asLong())
			SCRIPT_ERROR(DIVIDE_BY_ZERO);
		argv[0] = argv[0].asLong() % argv[1].asLong();
	}

	FUNC(and_int) { argv[0] = argv[0].asInt() & argv[1].asInt(); }
	FUNC(and_long) { argv[0] = argv[0].asLong() & argv[1].asLong(); }

	FUNC(or_int) { argv[0] = argv[0].asInt() | argv[1].asInt(); }
	FUNC(or_long) { argv[0] = argv[0].asLong() | argv[1].asLong(); }

	FUNC(xor_int) { argv[0] = argv[0].asInt() ^ argv[1].asInt(); }
	FUNC(xor_long) { argv[0] = argv[0].asLong() ^ argv[1].asLong(); }

	FUNC(lt_int) { argv[0] = argv[0].asInt() < argv[1].asInt(); }
	FUNC(lt_long) { argv[0] = argv[0].asLong() < argv[1].asLong(); }
	FUNC(lt_double) { argv[0] = argv[0].asDouble() < argv[1].asDouble(); }

	FUNC(gt_int) { argv[0] = argv[0].asInt() > argv[1].asInt(); }
	FUNC(gt_long) { argv[0] = argv[0].asLong() > argv[1].asLong(); }
	FUNC(gt_double) { argv[0] = argv[0].asDouble() > argv[1].asDouble(); }

	FUNC(le_int) { argv[0] = argv[0].asInt() <= argv[1].asInt(); }
	FUNC(le_long) { argv[0] = argv[0].asLong() <= argv[1].asLong(); }
	FUNC(le_double) { argv[0] = argv[0].asDouble() <= argv[1].asDouble(); }

	FUNC(ge_int) { argv[0] = argv[0].asInt() >= argv[1].asInt(); }
	FUNC(ge_long) { argv[0] = argv[0].asLong() >= argv[1].asLong(); }
	FUNC(ge_double) { argv[0] = argv[0].asDouble() >= argv[1].asDouble(); }

	FUNC(eq_int) { argv[0] = argv[0].asInt() == argv[1].asInt(); }
	FUNC(eq_long) { argv[0] = argv[0].asLong() == argv[1].asLong(); }
	FUNC(eq_double) { argv[0] = argv[0].asDouble() == argv[1].asDouble(); }

	FUNC(ne_int) { argv[0] = argv[0].asInt() != argv[1].asInt(); }
	FUNC(ne_long) { argv[0] = argv[0].asLong() != argv[1].asLong(); }
	FUNC(ne_double) { argv[0] = argv[0].asDouble() != argv[1].asDouble(); }

	FUNC(land_int) { argv[0] = argv[0].asInt() && argv[1].asInt(); }
	FUNC(land_long) { argv[0] = argv[0].asLong() && argv[1].asLong(); }
	FUNC(land_double) { argv[0] = argv[0].asDouble() && argv[1].asDouble(); }

	FUNC(lor_int) { argv[0] = argv[0].asInt() || argv[1].asInt(); }
	FUNC(lor_long) { argv[0] = argv[0].asLong() || argv[1].asLong(); }
	FUNC(lor_double) { argv[0] = argv[0].asDouble() || argv[1].asDouble(); }
}

static const VDScriptFunctionDef objFL_Sylia[]={
	{ dprint,		"dprint", "0." },
	{ messagebox,	"MessageBox", "0ss" },
	{ IntToString,		"ToString", "si" },
	{ LongToString,		NULL, "sl" },
	{ DoubleToString,	NULL, "sd" },
	{ StringToString,	NULL, "ss" },
	{ Atoi,			"Atoi", "is" },
	{ Atol,			"Atol", "ls" },
	{ Atod,			"Atod", "ds" },
	{ add_int,		"+", "iii" },
	{ add_long,		NULL, "lll" },
	{ add_double,	NULL, "ddd" },
	{ add_string,	NULL, "sss" },
	{ sub_int,		"-", "iii" },
	{ sub_long,		NULL, "lll" },
	{ sub_double,	NULL, "ddd" },
	{ mul_int,		"*", "iii" },
	{ mul_long,		NULL, "lll" },
	{ mul_double,	NULL, "ddd" },
	{ div_int,		"/", "iii" },
	{ div_long,		NULL, "lll" },
	{ div_double,	NULL, "ddd" },
	{ mod_int,		"%", "iii" },
	{ mod_long,		NULL, "lll" },
	{ and_int,		"&", "iii" },
	{ and_long,		NULL, "lll" },
	{ or_int,		"|", "iii" },
	{ or_long,		NULL, "lll" },
	{ xor_int,		"^", "iii" },
	{ xor_long,		NULL, "lll" },
	{ lt_int,		"<", "iii" },
	{ lt_long,		NULL, "ill" },
	{ lt_double,	NULL, "idd" },
	{ gt_int,		">", "iii" },
	{ gt_long,		NULL, "ill" },
	{ gt_double,	NULL, "idd" },
	{ le_int,		"<=", "iii" },
	{ le_long,		NULL, "ill" },
	{ le_double,	NULL, "idd" },
	{ ge_int,		">=", "iii" },
	{ ge_long,		NULL, "ill" },
	{ ge_double,	NULL, "idd" },
	{ eq_int,		"==", "iii" },
	{ eq_long,		NULL, "ill" },
	{ eq_double,	NULL, "idd" },
	{ ne_int,		"!=", "iii" },
	{ ne_long,		NULL, "ill" },
	{ ne_double,	NULL, "idd" },
	{ land_int,		"&&", "iii" },
	{ land_long,	NULL, "ill" },
	{ land_double,	NULL, "idd" },
	{ lor_int,		"||", "iii" },
	{ lor_long,		NULL, "ill" },
	{ lor_double,	NULL, "idd" },
	{ NULL }
};

VDScriptObject obj_Sylia={
	NULL,
	objFL_Sylia
};