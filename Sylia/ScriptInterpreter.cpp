#include <vd2/system/vdtypes.h>
#include <vd2/system/text.h>
#include <vd2/system/VDString.h>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "StringHeap.h"
#include "ScriptError.h"
#include "ScriptValue.h"
#include "ScriptInterpreter.h"
#include "VariableTable.h"

///////////////////////////////////////////////////////////////////////////

//#define DEBUG_TOKEN

///////////////////////////////////////////////////////////////////////////

extern VDScriptObject obj_Sylia;

///////////////////////////////////////////////////////////////////////////

class VDScriptInterpreter : public IVDScriptInterpreter {
private:
	enum {
		MAX_FUNCTION_PARAMS = 16,
		MAX_IDENT_CHARS = 64
	};

	enum {
		TOK_IDENT		= 256,
		TOK_INTEGER,
		TOK_LONG,
		TOK_DOUBLE,
		TOK_STRING,
		TOK_DECLARE,
		TOK_TRUE,
		TOK_FALSE,
		TOK_AND,
		TOK_OR,
		TOK_EQUALS,
		TOK_NOTEQ,
		TOK_LESSEQ,
		TOK_GRTREQ,
	};

	const char *tokbase;
	const char *tokstr;
	union {
		int tokival;
		sint64 toklval;
		double tokdval;
	};
	int tokhold;
	char **tokslit;
	char szIdent[MAX_IDENT_CHARS+1];
	char szError[256];

	std::vector<VDScriptValue> mStack;
	std::vector<int> mOpStack;

	VDScriptRootHandlerPtr lpRoothandler;
	void *lpRoothandlerData;

	VDScriptStringHeap strheap;
	VariableTable	vartbl;

	const VDScriptFunctionDef *mpCurrentInvocationMethod;
	int mMethodArgumentCount;

	VDStringA mErrorExtraToken;

	////////

	void			ParseExpression();
	int				ExprOpPrecedence(int op);
	bool			ExprOpIsRightAssoc(int op);
	void			ParseValue();
	void			Reduce();

	void	ConvertToRvalue();
	void	InvokeMethod(const VDScriptObject *objdef, const char *name, int argc);
	void	InvokeMethod(const VDScriptFunctionDef *sfd, int pcount);

	bool	isExprFirstToken(int t);
	bool	isUnaryToken(int t);

	VDScriptValue	LookupRootVariable(char *szName);

	bool isIdentFirstChar(char c);
	bool isIdentNextChar(char c);
	void TokenBegin(const char *s);
	void TokenUnput(int t);
	int Token();
	void GC();

public:
	VDScriptInterpreter();
	~VDScriptInterpreter();
	void Destroy();

	void SetRootHandler(VDScriptRootHandlerPtr, void *);

	void ExecuteLine(const char *s);

	void ScriptError(int e);
	const char *TranslateScriptError(const VDScriptError& cse);
	char** AllocTempString(long l);
	VDScriptValue	DupCString(const char *s);

	VDScriptValue	LookupObjectMember(const VDScriptObject *obj, void *lpVoid, char *szIdent);

	const VDScriptFunctionDef *GetCurrentMethod() { return mpCurrentInvocationMethod; }
	int GetErrorLocation() { return tokstr - tokbase; }
};

///////////////////////////////////////////////////////////////////////////

VDScriptInterpreter::VDScriptInterpreter() : vartbl(128) {
}

VDScriptInterpreter::~VDScriptInterpreter() {
	lpRoothandler = NULL;
}

void VDScriptInterpreter::Destroy() {
	delete this;
}

IVDScriptInterpreter *VDCreateScriptInterpreter() {
	return new VDScriptInterpreter();
}

///////////////////////////////////////////////////////////////////////////

void VDScriptInterpreter::SetRootHandler(VDScriptRootHandlerPtr rh, void *rh_data) {
	lpRoothandler = rh;
	lpRoothandlerData = rh_data;
}

///////////////////////////////////////////////////////////////////////////

void VDScriptInterpreter::ExecuteLine(const char *s) {
	int t;

	VDDEBUG("Sylia: executing \"%s\"\n", s);

	mErrorExtraToken.clear();

	TokenBegin(s);

	while(t = Token()) {
		if (isExprFirstToken(t)) {
			TokenUnput(t);
			VDASSERT(mStack.empty());
			ParseExpression();

			VDASSERT(!mStack.empty());

			VDScriptValue& val = mStack.back();
			if (val.isInt())
				VDDEBUG("Expression: integer %ld\n", val.asInt());
			else if (val.isString())
				VDDEBUG("Expression: string [%s]\n", *val.asString());
			else if (val.isVoid())
				VDDEBUG("Expression: void\n");
			else
				VDDEBUG("Expression: unknown type\n");

			mStack.pop_back();
			VDASSERT(mStack.empty());

			if (Token() != ';')
				SCRIPT_ERROR(SEMICOLON_EXPECTED);
		} else if (t == TOK_DECLARE) {

			do {
				t = Token();

				if (t != TOK_IDENT)
					SCRIPT_ERROR(IDENTIFIER_EXPECTED);

				VariableTableEntry *vte = vartbl.Declare(szIdent);

				t = Token();

				if (t == '=') {
					ParseExpression();

					VDASSERT(!mStack.empty());
					vte->v = mStack.back();
					mStack.pop_back();

					t = Token();
				}

			} while(t == ',');

			if (t != ';')
				SCRIPT_ERROR(SEMICOLON_EXPECTED);

		} else
			SCRIPT_ERROR(PARSE_ERROR);
	}

	VDASSERT(mStack.empty());

	GC();
}

void VDScriptInterpreter::ScriptError(int e) {
	throw VDScriptError(e);
}

const char *VDScriptInterpreter::TranslateScriptError(const VDScriptError& cse) {
	char *s;
	int i;

	switch(cse.err) {
	case VDScriptError::OVERLOADED_FUNCTION_NOT_FOUND:
		{
			if (mpCurrentInvocationMethod && mpCurrentInvocationMethod->name)
				sprintf(szError, "Overloaded method %s(", mpCurrentInvocationMethod->name);
			else
				strcpy(szError, "Overloaded method (");

			s = szError + strlen(szError);

			const VDScriptValue *const argv = &*(mStack.end() - mMethodArgumentCount);

			for(i=0; i<mMethodArgumentCount; i++) {
				if (i) {
					if (argv[i].isVoid()) break;

					*s++ = ',';
				}

				switch(argv[i].type) {
				case VDScriptValue::T_VOID:		strcpy(s, "void"); break;
				case VDScriptValue::T_INT:		strcpy(s, "int"); break;
				case VDScriptValue::T_STR:		strcpy(s, "string"); break;
				case VDScriptValue::T_OBJECT:	strcpy(s, "object"); break;
				case VDScriptValue::T_FNAME:		strcpy(s, "method"); break;
				case VDScriptValue::T_FUNCTION:	strcpy(s, "function"); break;
				case VDScriptValue::T_VARLV:		strcpy(s, "var"); break;
				}

				while(*s) ++s;
			}

			strcpy(s, ") not found");
		}

		return szError;
	case VDScriptError::MEMBER_NOT_FOUND:
		sprintf(szError, "Member '%s' not found", szIdent);
		return szError;
	case VDScriptError::VAR_NOT_FOUND:
		if (!mErrorExtraToken.empty()) {
			sprintf(szError, "Variable '%s' not found", mErrorExtraToken.c_str());
			return szError;
		}
		break;
	}
	return ::VDScriptTranslateError(cse);
}

char **VDScriptInterpreter::AllocTempString(long l) {
	char **handle = strheap.Allocate(l);

	(*handle)[l]=0;

	return handle;
}

VDScriptValue VDScriptInterpreter::DupCString(const char *s) {
	const size_t l = strlen(s);
	char **pp = AllocTempString(l);

	memcpy(*pp, s, l);
	return VDScriptValue(pp);
}

///////////////////////////////////////////////////////////////////////////
//
//	Expression parsing
//
///////////////////////////////////////////////////////////////////////////

int VDScriptInterpreter::ExprOpPrecedence(int op) {
	// All of these need to be EVEN.
	switch(op) {
	case '=':			return 2;
	case TOK_OR:		return 4;
	case TOK_AND:		return 6;
	case '|':			return 8;
	case '^':			return 10;
	case '&':			return 12;
	case TOK_EQUALS:
	case TOK_NOTEQ:		return 14;
	case '<':
	case '>':
	case TOK_LESSEQ:
	case TOK_GRTREQ:	return 16;
	case '+':
	case '-':			return 18;
	case '*':
	case '/':
	case '%':			return 20;
	case '.':			return 22;
	}

	return 0;
}

bool VDScriptInterpreter::ExprOpIsRightAssoc(int op) {
	return op == '=';
}

void VDScriptInterpreter::ParseExpression() {
	int depth = 0;
	int t;
	bool need_value = true;

	for(;;) {
		if (need_value) {
			ParseValue();
			need_value = false;
		}

		t = Token();

		if (!t || t==')' || t==']' || t==',' || t==';') {
			TokenUnput(t);
			break;
		}

		VDScriptValue& v = mStack.back();

		if (t=='.') {			// object indirection operator (object -> member)
			VDDEBUG("found object indirection op\n");

			ConvertToRvalue();

			if (!v.isObject()) {
				SCRIPT_ERROR(TYPE_OBJECT_REQUIRED);
			}

			if (Token() != TOK_IDENT)
				SCRIPT_ERROR(OBJECT_MEMBER_NAME_REQUIRED);

			VDDEBUG("Attempting to find member: [%s]\n", szIdent);

			v = LookupObjectMember(v.asObjectDef(), v.asObjectPtr(), szIdent);

			if (v.isVoid())
				SCRIPT_ERROR(MEMBER_NOT_FOUND);

		} else if (t == '[') {	// array indexing operator (object, value -> value)
			// Reduce lvalues to rvalues

			ConvertToRvalue();

			if (!v.isObject())
				SCRIPT_ERROR(TYPE_OBJECT_REQUIRED);

			ParseExpression();
			InvokeMethod((mStack.end() - 2)->asObjectDef(), "[]", 1);

			VDASSERT(mStack.size() >= 2);
			mStack.erase(mStack.end() - 2);

			if (Token() != ']')
				SCRIPT_ERROR(CLOSEBRACKET_EXPECTED);
		} else if (t == '(') {	// function indirection operator (method -> value)
			const VDScriptValue fcall(mStack.back());
			mStack.pop_back();

			mStack.push_back(VDScriptValue(fcall.u.method.p, fcall.thisPtr));

			int pcount = 0;

			t = Token();
			if (t != ')') {
				TokenUnput(t);

				for(;;) {
					ParseExpression();
					ConvertToRvalue();
					++pcount;
					
					t = Token();

					if (t==')')
						break;
					else if (t!=',')
						SCRIPT_ERROR(FUNCCALLEND_EXPECTED);
				}
			}

			InvokeMethod(fcall.u.method.pfn, pcount);

			VDASSERT(mStack.size() >= 2);
			mStack.erase(mStack.end() - 2);
		} else {
			int prec = ExprOpPrecedence(t) + ExprOpIsRightAssoc(t);

			while(depth > 0 && ExprOpPrecedence(mOpStack.back()) >= prec) {
				--depth;
				Reduce();
			}

			mOpStack.push_back(t);
			++depth;

			need_value = true;
		}
	}

	// reduce until no ops are left
	while(depth-- > 0)
		Reduce();
}

void VDScriptInterpreter::Reduce() {
	const int op = mOpStack.back();
	mOpStack.pop_back();

	switch(op) {
	case '=':
		{
			VDScriptValue& v = mStack[mStack.size() - 2];

			if (!v.isVarLV())
				SCRIPT_ERROR(TYPE_OBJECT_REQUIRED);

			ConvertToRvalue();

			v.asVarLV()->v = mStack.back();
			mStack.pop_back();
		}
		break;
	case TOK_OR:		InvokeMethod(&obj_Sylia, "||", 2); break;
	case TOK_AND:		InvokeMethod(&obj_Sylia, "&&", 2); break;
	case '|':			InvokeMethod(&obj_Sylia, "|", 2); break;
	case '^':			InvokeMethod(&obj_Sylia, "^", 2); break;
	case '&':			InvokeMethod(&obj_Sylia, "&", 2); break;
	case TOK_EQUALS:	InvokeMethod(&obj_Sylia, "==", 2); break;
	case TOK_NOTEQ:		InvokeMethod(&obj_Sylia, "!=", 2); break;
	case '<':			InvokeMethod(&obj_Sylia, "<", 2); break;
	case '>':			InvokeMethod(&obj_Sylia, ">", 2); break;
	case TOK_LESSEQ:	InvokeMethod(&obj_Sylia, "<=", 2); break;
	case TOK_GRTREQ:	InvokeMethod(&obj_Sylia, ">=", 2); break;
	case '+':			InvokeMethod(&obj_Sylia, "+", 2); break;
	case '-':			InvokeMethod(&obj_Sylia, "-", 2); break;
	case '*':			InvokeMethod(&obj_Sylia, "*", 2); break;
	case '/':			InvokeMethod(&obj_Sylia, "/", 2); break;
	case '%':			InvokeMethod(&obj_Sylia, "%", 2); break;
	}
}

void VDScriptInterpreter::ParseValue() {
	int t = Token();

	if (t=='(') {
		ParseExpression();

		if (Token() != ')')
			SCRIPT_ERROR(CLOSEPARENS_EXPECTED);
	} else if (t==TOK_IDENT) {
		VDDEBUG("Resolving variable: [%s]\n", szIdent);
		mStack.push_back(LookupRootVariable(szIdent));
	} else if (t == TOK_INTEGER)
		mStack.push_back(VDScriptValue(tokival));
	else if (t == TOK_LONG)
		mStack.push_back(VDScriptValue(toklval));
	else if (t == TOK_DOUBLE)
		mStack.push_back(VDScriptValue(tokdval));
	else if (t == TOK_STRING)
		mStack.push_back(VDScriptValue(tokslit));
	else if (t=='!' || t=='~' || t=='-' || t=='+') {
		ParseValue();

		VDScriptValue& v = mStack.back();

		if (!v.isInt())
			SCRIPT_ERROR(TYPE_INT_REQUIRED);

		switch(t) {
		case '!':
			v = VDScriptValue(!v.asInt());
			break;
		case '~':
			v = VDScriptValue(~v.asInt());
			break;
		case '+':
			break;
		case '-':
			v = VDScriptValue(-v.asInt());
			break;
		default:
			SCRIPT_ERROR(PARSE_ERROR);
		}
	} else if (t == TOK_TRUE)
		mStack.push_back(VDScriptValue(1));
	else if (t == TOK_FALSE)
		mStack.push_back(VDScriptValue(0));
	else
		SCRIPT_ERROR(PARSE_ERROR);
}


///////////////////////////////////////////////////////////////////////////
//
//	Variables...
//
///////////////////////////////////////////////////////////////////////////

void VDScriptInterpreter::InvokeMethod(const VDScriptObject *obj, const char *name, int argc) {
	if (obj->func_list) {
		const VDScriptFunctionDef *sfd = obj->func_list;

		while(sfd->arg_list) {
			if (sfd->name && !strcmp(sfd->name, name)) {
				InvokeMethod(sfd, argc);
				return;
			}

			++sfd;
		}
	}

	SCRIPT_ERROR(OVERLOADED_FUNCTION_NOT_FOUND);
}

void VDScriptInterpreter::InvokeMethod(const VDScriptFunctionDef *sfd, int pcount) {
	VDScriptValue *params = NULL;
	
	if (pcount)
		params = &mStack[mStack.size() - pcount];

	mpCurrentInvocationMethod = sfd;
	mMethodArgumentCount = pcount;

	// If we were passed a function name, attempt to match our parameter
	// list to one of the overloaded function templates.
	//
	// 0 = void, v = value, i = int, . = varargs
	//
	// <return value><param1><param2>...
	char c;
	const char *s;
	const char *name = sfd->name;
	VDScriptValue *csv;
	int argcnt;

	// Yes, goto's are usually considered gross... but I prefer
	// cleanly labeled goto's to excessive boolean variable usage.

	while(sfd->arg_list && (sfd->name == name || !sfd->name)) {
		s = sfd->arg_list+1;
		argcnt = pcount;
		csv = params;

		while((c=*s++) && argcnt--) {
			switch(c) {
			case 'v':
				break;
			case 'i':
			case 'l':
			case 'd':
				if (!csv->isInt() && !csv->isLong() && !csv->isDouble()) goto arglist_nomatch;
				break;
			case 's':
				if (!csv->isString()) goto arglist_nomatch;
				break;
			case '.':
				goto arglist_match;
			default:
				VDDEBUG("Sylia external error: invalid argument type '%c' for method\n", c);

				SCRIPT_ERROR(EXTERNAL_ERROR);
			}
			++csv;
		}

		if (!c && !argcnt)
			goto arglist_match;

arglist_nomatch:
		++sfd;
	}
	SCRIPT_ERROR(OVERLOADED_FUNCTION_NOT_FOUND);

arglist_match:
	// Make sure there is room for the return value.
	int stackcount = pcount;

	if (!stackcount) {
		++stackcount;
		mStack.push_back(VDScriptValue());
	}

	// coerce arguments
	VDScriptValue *const argv = &*(mStack.end() - stackcount);
	const char *const argdesc = sfd->arg_list + 1;

	for(int i=0; i<pcount; ++i) {
		VDScriptValue& a = argv[i];
		switch(argdesc[i]) {
		case 'i':
			if (a.isLong())
				a = VDScriptValue((int)a.asLong());
			else if (a.isDouble())
				a = VDScriptValue((int)a.asDouble());
			break;
		case 'l':
			if (a.isInt())
				a = VDScriptValue((sint64)a.asInt());
			else if (a.isDouble())
				a = VDScriptValue((sint64)a.asDouble());
			break;
		case 'd':
			if (a.isInt())
				a = VDScriptValue((double)a.asInt());
			else if (a.isLong())
				a = VDScriptValue((double)a.asLong());
			break;
		}
	}

	// invoke
	sfd->func_ptr(this, argv, pcount);
	mStack.resize(mStack.size() + 1 - stackcount);
	if (sfd->arg_list[0] == '0')
		mStack.back() = VDScriptValue();
}

VDScriptValue VDScriptInterpreter::LookupRootVariable(char *szName) {
	VariableTableEntry *vte;

	if (vte = vartbl.Lookup(szName))
		return VDScriptValue(vte);

	if (!strcmp(szName, "Sylia"))
		return VDScriptValue(NULL, &obj_Sylia);

	const char *volatile _szName = szName;		// needed to fix exception handler, for some reason

	VDScriptValue ret;

	try {
		if (!lpRoothandler)
			SCRIPT_ERROR(VAR_NOT_FOUND);

		ret = lpRoothandler(this, szName, lpRoothandlerData);
	} catch(const VDScriptError& e) {
		if (e.err == VDScriptError::VAR_NOT_FOUND) {
			mErrorExtraToken = _szName;
			throw;
		}
	}

	return ret;
}

VDScriptValue VDScriptInterpreter::LookupObjectMember(const VDScriptObject *obj, void *lpVoid, char *szIdent) {
	for(; obj; obj = obj->pNextObject) {
		if (obj->func_list) {
			const VDScriptFunctionDef *pfd = obj->func_list;

			for(; pfd->func_ptr; ++pfd) {
				if (pfd->name && !strcmp(pfd->name, szIdent))
					return VDScriptValue(lpVoid, obj, pfd);
			}
		}

		if (obj->obj_list) {
			const VDScriptObjectDef *sod = obj->obj_list;

			while(sod->name) {
				if (!strcmp(sod->name, szIdent)) {
					VDScriptValue t(lpVoid, sod->obj);
					return t;
				}

				++sod;
			}
		}

		if (obj->Lookup) {
			VDScriptValue v(obj->Lookup(this, obj, lpVoid, szIdent));
			if (!v.isVoid())
				return v;
		}
	}

	return VDScriptValue();
}

void VDScriptInterpreter::ConvertToRvalue() {
	VDASSERT(!mStack.empty());

	VDScriptValue& val = mStack.back();

	if (val.isVarLV())
		val = val.asVarLV()->v;
}

///////////////////////////////////////////////////////////////////////////
//
//	Token-level parsing
//
///////////////////////////////////////////////////////////////////////////

bool VDScriptInterpreter::isExprFirstToken(int t) {
	return t==TOK_IDENT || t==TOK_STRING || t==TOK_INTEGER || isUnaryToken(t) || t=='(';
}

bool VDScriptInterpreter::isUnaryToken(int t) {
	return t=='+' || t=='-' || t=='!' || t=='~';
}

///////////////////////////////////////////////////////////////////////////
//
//	Character-level parsing
//
///////////////////////////////////////////////////////////////////////////

bool VDScriptInterpreter::isIdentFirstChar(char c) {
	return isalpha(c) || c=='_';
}

bool VDScriptInterpreter::isIdentNextChar(char c) {
	return isalnum(c) || c=='_';
}

void VDScriptInterpreter::TokenBegin(const char *s) {
	tokbase = tokstr = s;
	tokhold = 0;
}

void VDScriptInterpreter::TokenUnput(int t) {
	tokhold = t;

#ifdef DEBUG_TOKEN
	if (t>=' ' && t<256)
		VDDEBUG("TokenUnput('%c' (%d))\n", (char)t, t);
	else
		VDDEBUG("TokenUnput(%d)\n", t);
#endif
}

int VDScriptInterpreter::Token() {
	static char hexdig[]="0123456789ABCDEF";
	char *s,c;

	if (tokhold) {
		int t = tokhold;
		tokhold = 0;
		return t;
	}

	do {
		c=*tokstr++;
	} while(c && isspace(c));

	if (!c) {
		--tokstr;

		return 0;
	}

	// C++ style comment?

	if (c=='/')
		if (tokstr[0]=='/') {
			while(*tokstr) ++tokstr;

			return 0;		// C++ comment
		} else
			return '/';

	// string?

	if (c=='"') {
		const char *s = tokstr;
		char *t;
		long len_adjust=0;

		while((c=*tokstr++) && c!='"') {
			if (c=='\\') {
				c = *tokstr++;
				if (!c) SCRIPT_ERROR(PARSE_ERROR);
				else {
					if (c=='x') {
						if (!isxdigit(tokstr[0]) || !isxdigit(tokstr[1]))
							SCRIPT_ERROR(PARSE_ERROR);
						tokstr+=2;
						len_adjust += 2;
					}
					++len_adjust;
				}
			}
		}

		tokslit = strheap.Allocate(tokstr - s - 1 - len_adjust);
		t = *tokslit;
		while(s<tokstr-1) {
			int val;

			c = *s++;

			if (c=='\\')
				switch(c=*s++) {
				case 'a': *t++='\a'; break;
				case 'b': *t++='\b'; break;
				case 'f': *t++='\f'; break;
				case 'n': *t++='\n'; break;
				case 'r': *t++='\r'; break;
				case 't': *t++='\t'; break;
				case 'v': *t++='\v'; break;
				case 'x':
					val = strchr(hexdig,toupper(s[0]))-hexdig;
					val = (val<<4) | (strchr(hexdig,toupper(s[1]))-hexdig);
					*t++ = val;
					s += 2;
					break;
				default:
					*t++ = c;
				}
			else
				*t++ = c;
		}
		*t=0;

		if (!c) --tokstr;

#ifdef DEBUG_TOKEN
		VDDEBUG("Literal: [%s]\n", *tokslit);
#endif

		return TOK_STRING;
	}

	// unescaped string?
	if ((c=='u' || c=='U') && *tokstr == '"') {
		const char *s = ++tokstr;

		while((c=*tokstr++) && c != '"')
			;

		if (!c) {
			--tokstr;
			SCRIPT_ERROR(PARSE_ERROR);
		}

		size_t len = tokstr - s - 1;

		const VDStringA strA(VDTextWToU8(VDTextAToW(s, len)));

		len = strA.size();

		tokslit = strheap.Allocate(len);
		memcpy(*tokslit, strA.data(), len);
		(*tokslit)[len] = 0;

		return TOK_STRING;
	}

	// look for variable/keyword

	if (isIdentFirstChar(c)) {
		s = szIdent;

		*s++ = c;
		while(isIdentNextChar(c = *tokstr++)) {
			if (s>=szIdent + MAX_IDENT_CHARS)
				SCRIPT_ERROR(IDENT_TOO_LONG);

			*s++ = c;
		}

		--tokstr;
		*s=0;

		if (!strcmp(szIdent, "declare"))
			return TOK_DECLARE;
		else if (!strcmp(szIdent, "true"))
			return TOK_TRUE;
		else if (!strcmp(szIdent, "false"))
			return TOK_FALSE;

		return TOK_IDENT;
	}

	// test for number: decimal (123), octal (0123), or hexadecimal (0x123)

	if (isdigit(c)) {
		sint64 v = 0;

		if (c=='0' && tokstr[0] == 'x') {

			// hex (base 16)
			++tokstr;

			while(isxdigit((unsigned char)(c = *tokstr++))) {
				v = v*16 + (strchr(hexdig, toupper(c))-hexdig);
			}

		} else if (c=='0' && isdigit((unsigned char)tokstr[0])) {
			// octal (base 8)
			while((c=*tokstr++)>='0' && c<='7')
				v = v*8 + (c-'0');
		} else {
			// check for float
			const char *s = tokstr;
			while(*s) {
				if (*s == '.' || *s == 'e' || *s == 'E') {
					// It's a float -- parse and return.

					--tokstr;
					tokdval = strtod(tokstr, (char **)&tokstr);
					return TOK_DOUBLE;
				}

				if (!isdigit((unsigned char)*s))
					break;
				++s;
			}

			// decimal
			v = (c-'0');
			while(isdigit(c=*tokstr++))
				v = v*10 + (c-'0');
		}
		--tokstr;

		if (v > 0x7FFFFFFF) {
			toklval = v;
			return TOK_LONG;
		} else {
			tokival = v;
			return TOK_INTEGER;
		}
	}

	// test for symbols:
	//
	//	charset:	+-*/<>=!&|^[]~;%(),
	//	solitary:	+-*/<>=!&|^[]~;%(),
	//	extra:		!= <= >= == && ||
	//
	//	the '/' is handled above for comment handling

	if (c=='!')
		if (tokstr[0] == '=') { ++tokstr; return TOK_NOTEQ;  } else return '!';

	if (c=='<')
		if (tokstr[0] == '=') { ++tokstr; return TOK_LESSEQ; } else return '<';

	if (c=='>')
		if (tokstr[0] == '=') { ++tokstr; return TOK_GRTREQ; } else return '>';

	if (c=='=')
		if (tokstr[0] == '=') { ++tokstr; return TOK_EQUALS; } else return '=';

	if (c=='&')
		if (tokstr[0] == '&') { ++tokstr; return TOK_AND;    } else return '&';

	if (c=='|')
		if (tokstr[0] == '|') { ++tokstr; return TOK_OR;     } else return '|';

	if (strchr("+-*^[]~;%(),.",c))
		return c;

	SCRIPT_ERROR(PARSE_ERROR);
}

void VDScriptInterpreter::GC() {
	strheap.BeginGC();
	vartbl.MarkStrings(strheap);
	int n = strheap.EndGC();

	if (n)
		VDDEBUG("Script: %d strings freed by GC.\n", n);
}
