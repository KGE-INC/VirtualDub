#include "ScriptError.h"

static struct ErrorEnt {
	int e;
	char *s;
} error_list[]={
	{ CScriptError::PARSE_ERROR,					"parse error" },
	{ CScriptError::SEMICOLON_EXPECTED,				"expected ';'" },
	{ CScriptError::IDENTIFIER_EXPECTED,			"identifier expected" },
	{ CScriptError::TYPE_INT_REQUIRED,				"integer type required" },
	{ CScriptError::TYPE_ARRAY_REQUIRED,			"array type required" },
	{ CScriptError::TYPE_FUNCTION_REQUIRED,			"function type required" },
	{ CScriptError::TYPE_OBJECT_REQUIRED,			"object type required" },
	{ CScriptError::OBJECT_MEMBER_NAME_REQUIRED,	"object member name expected" },
	{ CScriptError::FUNCCALLEND_EXPECTED,			"expected ')'" },
	{ CScriptError::TOO_MANY_PARAMS,				"function call parameter limit exceeded" },
	{ CScriptError::DIVIDE_BY_ZERO,					"divide by zero" },
	{ CScriptError::VAR_NOT_FOUND,					"variable not found" },
	{ CScriptError::MEMBER_NOT_FOUND,				"member not found" },
	{ CScriptError::OVERLOADED_FUNCTION_NOT_FOUND,	"overloaded function not found" },
	{ CScriptError::IDENT_TOO_LONG,					"identifier length limit exceeded" },
	{ CScriptError::OPERATOR_EXPECTED,				"expression operator expected" },
	{ CScriptError::CLOSEPARENS_EXPECTED,			"expected ')'" },
	{ CScriptError::CLOSEBRACKET_EXPECTED,			"expected ']'" },
	{ CScriptError::OUT_OF_STRING_SPACE,			"out of string space" },
	{ CScriptError::OUT_OF_MEMORY,					"out of memory" },
	{ CScriptError::INTERNAL_ERROR,					"internal error - Birdy goofed" },
	{ CScriptError::EXTERNAL_ERROR,					"error in external Sylia linkages" },
	{ CScriptError::VAR_UNDEFINED,					"variable's value is undefined" },
	{ CScriptError::FCALL_OUT_OF_RANGE,				"argument out of range" },
	{ CScriptError::FCALL_INVALID_PTYPE,			"argument has wrong type" },
	{ CScriptError::FCALL_UNKNOWN_STR,				"string argument not recognized" },
	{ 0, 0 },
};

const char *TranslateScriptError(int e) {
	struct ErrorEnt *eeptr = error_list;

	while(eeptr->s) {
		if (eeptr->e == e)
			return eeptr->s;

		++eeptr;
	}

	return "unknown error";
}

