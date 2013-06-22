#include <windows.h>

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

static void func_Sylia_dprint(IScriptInterpreter *, CScriptObject *, CScriptValue *argv, int argc) {
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

static ScriptFunctionDef objFL_Sylia[]={
	{ (ScriptFunctionPtr)func_Sylia_dprint, "dprint", "0." },
	{ NULL }
};

CScriptObject obj_Sylia={
	NULL,
	objFL_Sylia
};