#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <crtdbg.h>

#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

CScriptValue func_Nene_print(IScriptInterpreter *isi, void *lpVoid, CScriptValue *arglist, int arg_count) {
	if (arg_count!=1 || !arglist[0].isInt())
		EXT_SCRIPT_ERROR(OVERLOADED_FUNCTION_NOT_FOUND);

	printf("Nene says: %ld\n", arglist[0].asInt());

	return CScriptValue();
}

CScriptValue func_Nene_rand(IScriptInterpreter *isi, void *lpVoid, CScriptValue *arglist, int arg_count) {
	if (arg_count)
		EXT_SCRIPT_ERROR(OVERLOADED_FUNCTION_NOT_FOUND);

	return CScriptValue(rand());
}

int func_Nene_shl(IScriptInterpreter *isi, void *lpVoid, CScriptValue *arglist, int arg_count) {
	return arglist[0].asInt() << arglist[1].asInt();
}

int func_Nene_shl2(IScriptInterpreter *isi, void *lpVoid, CScriptValue *arglist, int arg_count) {
	return arglist[0].asInt() << 1;
}

CScriptValue obj_Nene_lookup(IScriptInterpreter *isi, CScriptObject *obj, void *, char *szName) {
	if (!strcmp(szName, "print"))
		return CScriptValue(obj, func_Nene_print);
	else if (!strcmp(szName, "rand"))
		return CScriptValue(obj, func_Nene_rand);

	EXT_SCRIPT_ERROR(MEMBER_NOT_FOUND);
}

ScriptFunctionDef obj_Nene_functbl[]={
	{ (ScriptFunctionPtr)func_Nene_shl , "shl", "iii" },
	{ (ScriptFunctionPtr)func_Nene_shl2, NULL, "ii" },
	{ NULL }
};

CScriptObject obj_Nene={
	&obj_Nene_lookup, obj_Nene_functbl
};

CScriptValue RootHandler(IScriptInterpreter *isi, char *szName, void *lpData) {
	if (!strcmp(szName,"fourteen"))
		return CScriptValue(14);
	else if (!strcmp(szName, "Nene"))
		return &obj_Nene;

	EXT_SCRIPT_ERROR(VAR_NOT_FOUND);
}

///////////////////////////////////////

char *commands[]={
	"declare x; declare y;",
	"declare randFunc, printFunc;",
	"",
	"x = true+2*3;",
	"y = (1+2)*3;",
	"Sylia.dprint(\"x=\"+x+\", y=\"+y);",

	"randFunc = Nene.rand;",
	"printFunc = Sylia.dprint;",
	"printFunc(randFunc()*0 + 12345678);",

	"-(1+2)*-3;",
	"4+-fourteen;",
	"Nene.print(4);",
	"Nene.print(Nene.rand());",
	"Nene.print(Nene.shl(4,3) + fourteen);",
	"Nene.print(Nene.shl(fourteen));",
	"\"hello world! \" + fourteen + \"\\n\";",
	"Sylia.dprint(\"Hello!\");",
//	"Nene.print(Nene.shl(4,3,2) + fourteen);",
	"Nene.print(Nene.shr() + fourteen);",
	NULL
};

///////////////////////////////////////

int main(int argc, char **argv) {
	IScriptInterpreter *isi = CreateScriptInterpreter();

	if (!isi) {
		puts("Couldn't create interpreter.");
		return 5;
	}

	isi->SetRootHandler(RootHandler, NULL);

	try {
		char **cmd = commands;

		while(*cmd)
			isi->ExecuteLine(*cmd++);

	} catch(CScriptError cse) {
		printf("Error: %s\n", isi->TranslateScriptError(cse));
		_RPT1(0,"Error: %s\n", isi->TranslateScriptError(cse));
	}

	isi->Destroy();

	return 0;
}
