#include <crtdbg.h>
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

extern CScriptObject obj_Sylia;

///////////////////////////////////////////////////////////////////////////

class CScriptInterpreter : public IScriptInterpreter {
private:
	enum {
		MAX_FUNCTION_PARAMS = 16,
		MAX_IDENT_CHARS = 64
	};

	enum {
		TOK_IDENT		= 256,
		TOK_INTEGER,
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

	char *tokstr;
	long toklval;
	int tokhold;
	char **tokslit;
	char szIdent[MAX_IDENT_CHARS+1];
	char szError[256];

	CScriptValue params[MAX_FUNCTION_PARAMS];

	ScriptRootHandlerPtr lpRoothandler;
	void *lpRoothandlerData;

	CStringHeap strheap;
	VariableTable	vartbl;

	////////

	CScriptValue	ParseExpression();
	CScriptValue	ParseExpressionLvalue();
	int				ExprOpPrecedence(int op);
	bool			ExprOpIsRightAssoc(int op);
	CScriptValue	ParseExpression2(CScriptValue v_left, int op);
	CScriptValue	ParseValue();

	bool	isExprFirstToken(int t);
	bool	isUnaryToken(int t);

	CScriptValue	LookupRootVariable(char *szName);

	bool isIdentFirstChar(char c);
	bool isIdentNextChar(char c);
	void TokenBegin(char *s);
	void TokenUnput(int t);
	int Token();

public:
	CScriptInterpreter();
	~CScriptInterpreter();
	void Destroy();

	void SetRootHandler(ScriptRootHandlerPtr, void *);

	void ExecuteLine(char *s);

	void ScriptError(int e);
	const char *TranslateScriptError(CScriptError& cse);
	char** AllocTempString(long l);

	CScriptValue	LookupObjectMember(CScriptObject *obj, void *lpVoid, char *szIdent);
};

///////////////////////////////////////////////////////////////////////////

CScriptInterpreter::CScriptInterpreter() : strheap(65536, 256), vartbl(128) {
}

CScriptInterpreter::~CScriptInterpreter() {
	lpRoothandler = NULL;
}

void CScriptInterpreter::Destroy() {
	delete this;
}

IScriptInterpreter *CreateScriptInterpreter() {
	return new CScriptInterpreter();
}

///////////////////////////////////////////////////////////////////////////

void CScriptInterpreter::SetRootHandler(ScriptRootHandlerPtr rh, void *rh_data) {
	lpRoothandler = rh;
	lpRoothandlerData = rh_data;
}

///////////////////////////////////////////////////////////////////////////

void CScriptInterpreter::ExecuteLine(char *s) {
	int t;

	_RPT1(0,"Sylia: executing \"%s\"\n", s);

	TokenBegin(s);

	while(t = Token()) {
		if (isExprFirstToken(t)) {
			CScriptValue csv;

			TokenUnput(t);
			csv = ParseExpression();

			if (csv.isInt()) {
				_RPT1(0,"Expression: integer %ld\n", csv.asInt());
			} else if (csv.isString()) {
				_RPT1(0,"Expression: string [%s]\n", *csv.asString());

				strheap.Free(csv.asString(), true);
			} else if (csv.isVoid()) {
				_RPT0(0,"Expression: void\n");
			} else {
				_RPT0(0,"Expression: unknown type\n");
			}

			if (Token() != ';')
				SCRIPT_ERROR(SEMICOLON_EXPECTED);
		} else if (t == TOK_DECLARE) {

			do {
				t = Token();

				if (t != TOK_IDENT)
					SCRIPT_ERROR(IDENTIFIER_EXPECTED);

				vartbl.Declare(szIdent);

				t = Token();
			} while(t == ',');

			if (t != ';')
				SCRIPT_ERROR(SEMICOLON_EXPECTED);

		} else
			SCRIPT_ERROR(PARSE_ERROR);
	}
}

void CScriptInterpreter::ScriptError(int e) {
	throw CScriptError(e);
}

const char *CScriptInterpreter::TranslateScriptError(CScriptError& cse) {
	char *s;
	int i;

	switch(cse.err) {
	case CScriptError::OVERLOADED_FUNCTION_NOT_FOUND:
		{
			strcpy(szError, "Overloaded method (");

			s = szError + 19;

			for(i=0; i<MAX_FUNCTION_PARAMS; i++) {
				if (i) {
					if (params[i].isVoid()) break;

					*s++ = ',';
				}

				switch(params[i].type) {
				case CScriptValue::T_VOID:		strcpy(s, "void"); break;
				case CScriptValue::T_INT:		strcpy(s, "int"); break;
				case CScriptValue::T_STR:		strcpy(s, "string"); break;
				case CScriptValue::T_ARRAY:		strcpy(s, "array"); break;
				case CScriptValue::T_OBJECT:	strcpy(s, "object"); break;
				case CScriptValue::T_FNAME:		strcpy(s, "method"); break;
				case CScriptValue::T_FUNCTION:	strcpy(s, "function"); break;
				case CScriptValue::T_VARLV:		strcpy(s, "var"); break;
				}

				while(*s) ++s;
			}

			strcpy(s, ") not found");
		}

		return szError;
	case CScriptError::MEMBER_NOT_FOUND:
		sprintf(szError, "Member '%s' not found", szIdent);
		return szError;
	default:
		return ::TranslateScriptError(cse);
	}
}

char **CScriptInterpreter::AllocTempString(long l) {
	char **handle = strheap.Allocate(l, true);

	(*handle)[l]=0;

	return handle;
}
///////////////////////////////////////////////////////////////////////////
//
//	Expression parsing
//
///////////////////////////////////////////////////////////////////////////

CScriptValue CScriptInterpreter::ParseExpression() {
	CScriptValue v_left = ParseValue();
	int op = Token();

	v_left = ParseExpression2(v_left, op);

	if (v_left.isVarLV())
		v_left = v_left.asVarLV()->v;

	return v_left;
}

CScriptValue CScriptInterpreter::ParseExpressionLvalue() {
	CScriptValue v_left = ParseValue();
	int op = Token();

	return ParseExpression2(v_left, op);
}

int CScriptInterpreter::ExprOpPrecedence(int op) {
	switch(op) {
	case '=':			return 1;
	case TOK_OR:		return 2;
	case TOK_AND:		return 3;
	case '|':			return 4;
	case '^':			return 5;
	case '&':			return 6;
	case TOK_EQUALS:
	case TOK_NOTEQ:		return 7;
	case '<':
	case '>':
	case TOK_LESSEQ:
	case TOK_GRTREQ:	return 8;
	case '+':
	case '-':			return 9;
	case '*':
	case '/':
	case '%':			return 10;
	case '.':			return 11;
	}

	return 0;
}

bool CScriptInterpreter::ExprOpIsRightAssoc(int op) {
	return op == '=';
}

CScriptValue CScriptInterpreter::ParseExpression2(CScriptValue v_left, int op) {
	int t;

	for(;;) {
		if (!op || op==')' || op==']' || op==',' || op==';') {
			TokenUnput(op);
			return v_left;
		}

		if (op=='.') {			// object indirection operator
			_RPT0(0,"found object indirection op\n");

			// Reduce lvalues to rvalues

			if (v_left.isVarLV())
				v_left = v_left.asVarLV()->v;

			if (!v_left.isObject()) {
				SCRIPT_ERROR(TYPE_OBJECT_REQUIRED);
			}

			if (Token() != TOK_IDENT)
				SCRIPT_ERROR(OBJECT_MEMBER_NAME_REQUIRED);

			_RPT1(0,"Attempting to find member: [%s]\n", szIdent);

			v_left = LookupObjectMember(v_left.asObject(), v_left.lpVoid, szIdent);

			if (v_left.isVoid())
				SCRIPT_ERROR(MEMBER_NOT_FOUND);

		} else if (op=='[') {	// array indexing operator
			// Reduce lvalues to rvalues

			if (v_left.isVarLV())
				v_left = v_left.asVarLV()->v;

			if (!v_left.isArray())
				SCRIPT_ERROR(TYPE_ARRAY_REQUIRED);

			CScriptValue index = ParseExpression();

			if (!index.isInt())
				SCRIPT_ERROR(TYPE_INT_REQUIRED);

//			v_left = v_left.asArray()[index];
//			v_left = CScriptValue(0);
			v_left = v_left.asArray()(this, v_left.lpVoid, index.asInt());

			if (Token() != ']')
				SCRIPT_ERROR(CLOSEBRACKET_EXPECTED);

		} else if (op=='(') {	// function indirection operator
			int pcount;

			// Reduce lvalues to rvalues

			if (v_left.isVarLV())
				v_left = v_left.asVarLV()->v;

			if (!v_left.isFunction() && !v_left.isFName())
				SCRIPT_ERROR(TYPE_FUNCTION_REQUIRED);

			t = Token();

			if (t==')') {
				pcount = 0;
				params[0] = CScriptValue();
			} else {
				TokenUnput(t);

				for(pcount=0; pcount < MAX_FUNCTION_PARAMS; pcount++) {
					params[pcount] = ParseExpression();
					
					t = Token();

					if (t==')')
						break;
					else if (t!=',')
						SCRIPT_ERROR(FUNCCALLEND_EXPECTED);
				}

				if (pcount >= MAX_FUNCTION_PARAMS)
					SCRIPT_ERROR(TOO_MANY_PARAMS);

				++pcount;

				if (pcount < MAX_FUNCTION_PARAMS)
					params[pcount] = CScriptValue();
			}

			// If we were passed a function name, attempt to match our parameter
			// list to one of the overloaded function templates.
			//
			// 0 = void, v = value, i = int, . = varargs
			//
			// <return value><param1><param2>...

			if (v_left.isFName()) {
				ScriptFunctionDef *sfd = v_left.u.fname;
				char c,*s;
				char *name = sfd->name;
				CScriptValue *csv;
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
							if (!csv->isInt()) goto arglist_nomatch;
							break;
						case 's':
							if (!csv->isString()) goto arglist_nomatch;
							break;
						case '.':
							goto arglist_match;
						default:
							_RPT1(0,"Sylia external error: invalid argument type '%c' for method\n", c);

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
				switch(sfd->arg_list[0]) {
				case '0':	((ScriptVoidFunctionPtr)sfd->func_ptr)(this, v_left.lpVoid, params, pcount);
							v_left = CScriptValue();
							break;
				case 'i':	v_left = CScriptValue(((ScriptIntFunctionPtr)sfd->func_ptr)(this, v_left.lpVoid, params, pcount));
							break;
				case 'v':	v_left = sfd->func_ptr(this, v_left.lpVoid, params, pcount);
							break;
				default:
					_RPT1(0,"Sylia external error: invalid return type '%c' for method\n", sfd->arg_list[0]);

					SCRIPT_ERROR(EXTERNAL_ERROR);
				}

			} else {

				// It's an independent function... call it!

				v_left = v_left.asFunction()(this, v_left.lpVoid, params, pcount);
			}

			// Free any strings that may have been used as parameters

			for(int i=0; i<pcount; i++)
				if (params[i].isString())
					strheap.Free(params[i].asString(), true);

		} else {
			CScriptValue v_right = ParseValue();
			int prec_left, prec_right;

			t = Token();

			prec_left = ExprOpPrecedence(op);
			prec_right = ExprOpPrecedence(t);

			if (prec_right>prec_left || (prec_right==prec_left && ExprOpIsRightAssoc(op)))
				v_right = ParseExpression2(v_right, t);
			else
				TokenUnput(t);

			// If the right operand is an lvalue, reduce to rvalue

			if (v_right.isVarLV())
				v_right = v_right.asVarLV()->v;

			// If not '=' && left operand is an lvalue, reduce it too

			if (op != '=' && v_left.isVarLV())
				v_left = v_left.asVarLV()->v;

			// Reduce operator

			if (op == '=') {

				// We also can't assign types other than int and string

//				if (!v_right.isInt() && !v_right.isString())
//					SCRIPT_ERROR(TYPE_INT_REQUIRED);

				// Propagate v_right to *v_left.  Free a string that may be
				// in the variable.

				if (v_left.asVarLV()->v.isString())
					strheap.Free(v_left.asVarLV()->v.asString(), false);
				
				v_left.asVarLV()->v = v_right;

				// '=' evaluates to its right member...

				v_left = v_right;

			} else if (op == '+' && (v_left.isString() || v_right.isString())) {
				CScriptValue v_res;
				char lbuf[12], rbuf[12], *lstr, *rstr, *fstr;
				long l_left, l_right;

				if (v_left.isInt()) {
					itoa(v_left.asInt(), lbuf, 10);
					lstr = lbuf;
				} else if (v_left.isString()) {
					lstr = *v_left.asString();
				} else SCRIPT_ERROR(TYPE_INT_REQUIRED);

				if (v_right.isInt()) {
					itoa(v_right.asInt(), rbuf, 10);
					rstr = rbuf;
				} else if (v_right.isString()) {
					rstr = *v_right.asString();
				} else SCRIPT_ERROR(TYPE_INT_REQUIRED);

				l_left = strlen(lstr);
				l_right = strlen(rstr);

				v_res = CScriptValue(strheap.Allocate(l_left + l_right, true));
				fstr = *v_res.asString();
				strcpy(fstr, lstr);
				strcpy(fstr+l_left, rstr);

				if (v_left.isString()) strheap.Free(v_left.asString(), true);
				if (v_right.isString()) strheap.Free(v_right.asString(), true);

				v_left = v_res;
			} else {

				if (!v_left.isInt() || !v_right.isInt())
					SCRIPT_ERROR(TYPE_INT_REQUIRED);

				if (op == '+') {
					v_left = CScriptValue(v_left.asInt() + v_right.asInt());
				} else if (op == '-') {
					v_left = CScriptValue(v_left.asInt() - v_right.asInt());
				} else if (op == '*') {
					v_left = CScriptValue(v_left.asInt() * v_right.asInt());
				} else if (op == '/' || op=='%') {
					if (!v_right.asInt())
						SCRIPT_ERROR(DIVIDE_BY_ZERO);

					if (op=='%')
						v_left = CScriptValue(v_left.asInt() % v_right.asInt());
					else
						v_left = CScriptValue(v_left.asInt() / v_right.asInt());
				} else if (op == '<') v_left = CScriptValue(v_left.asInt() < v_right.asInt());
				else if (op == '>') v_left = CScriptValue(v_left.asInt() > v_right.asInt());
				else if (op == '&') v_left = CScriptValue(v_left.asInt() & v_right.asInt());
				else if (op == '|') v_left = CScriptValue(v_left.asInt() | v_right.asInt());
				else if (op == '^') v_left = CScriptValue(v_left.asInt() ^ v_right.asInt());
				else if (op == TOK_LESSEQ)	v_left = CScriptValue(v_left.asInt() <= v_right.asInt());
				else if (op == TOK_GRTREQ)	v_left = CScriptValue(v_left.asInt() >= v_right.asInt());
				else if (op == TOK_EQUALS)	v_left = CScriptValue(v_left.asInt() == v_right.asInt());
				else if (op == TOK_NOTEQ)	v_left = CScriptValue(v_left.asInt() != v_right.asInt());
				else if (op == TOK_AND)		v_left = CScriptValue(v_left.asInt() && v_right.asInt());
				else if (op == TOK_OR)		v_left = CScriptValue(v_left.asInt() || v_right.asInt());
				else
					SCRIPT_ERROR(OPERATOR_EXPECTED);

				if (v_right.isString())
					strheap.Free(v_right.asString(), true);
			}
		}

		op = Token();
	}

//	return v_left;
}

CScriptValue CScriptInterpreter::ParseValue() {
	int t = Token();

	if (t=='(') {
		CScriptValue v_left = ParseExpressionLvalue();

		if (Token() != ')')
			SCRIPT_ERROR(CLOSEPARENS_EXPECTED);

		return v_left;
	} else if (t==TOK_IDENT) {
		_RPT1(0,"Resolving variable: [%s]\n", szIdent);
		return LookupRootVariable(szIdent);
	} else if (t == TOK_INTEGER) {
		return CScriptValue(toklval);
	} else if (t == TOK_STRING) {
		return CScriptValue(tokslit);
	} else if (t=='!' || t=='~' || t=='-' || t=='+') {
		CScriptValue v_left = ParseValue();

		if (!v_left.isInt())
			SCRIPT_ERROR(TYPE_INT_REQUIRED);

		switch(t) {
		case '!':	return CScriptValue(!v_left.asInt());
		case '~':	return CScriptValue(~v_left.asInt());
		case '+':	return v_left;
		case '-':	return CScriptValue(-v_left.asInt());
		}
	} else if (t == TOK_TRUE)
		return CScriptValue(1);
	else if (t == TOK_FALSE)
		return CScriptValue(0);

	SCRIPT_ERROR(PARSE_ERROR);
}


///////////////////////////////////////////////////////////////////////////
//
//	Variables...
//
///////////////////////////////////////////////////////////////////////////

CScriptValue CScriptInterpreter::LookupRootVariable(char *szName) {
	VariableTableEntry *vte;

	if (vte = vartbl.Lookup(szName))
		return CScriptValue(vte);

	if (!strcmp(szName, "Sylia"))
		return CScriptValue(&obj_Sylia);

	if (lpRoothandler)
		return lpRoothandler(this, szName, lpRoothandlerData);
	else
		SCRIPT_ERROR(VAR_NOT_FOUND);
}

CScriptValue CScriptInterpreter::LookupObjectMember(CScriptObject *obj, void *lpVoid, char *szIdent) {
	if (obj->func_list) {
		ScriptFunctionDef *sfd = obj->func_list;

		while(sfd->arg_list) {
			if (sfd->name && !strcmp(sfd->name, szIdent)) {
				CScriptValue t(obj, sfd);
				t.lpVoid = lpVoid;
				return t;
			}

			++sfd;
		}
	}

	if (obj->obj_list) {
		ScriptObjectDef *sod = obj->obj_list;

		while(sod->name) {
			if (!strcmp(sod->name, szIdent)) {
				CScriptValue t(sod->obj);
				t.lpVoid = lpVoid;
				return t;
			}

			++sod;
		}
	}

	if (obj->Lookup)
		return obj->Lookup(this, obj, lpVoid, szIdent);
	else
		return CScriptValue();
}

///////////////////////////////////////////////////////////////////////////
//
//	Token-level parsing
//
///////////////////////////////////////////////////////////////////////////

bool CScriptInterpreter::isExprFirstToken(int t) {
	return t==TOK_IDENT || t==TOK_STRING || t==TOK_INTEGER || isUnaryToken(t) || t=='(';
}

bool CScriptInterpreter::isUnaryToken(int t) {
	return t=='+' || t=='-' || t=='!' || t=='~';
}

///////////////////////////////////////////////////////////////////////////
//
//	Character-level parsing
//
///////////////////////////////////////////////////////////////////////////

bool CScriptInterpreter::isIdentFirstChar(char c) {
	return isalpha(c) || c=='_';
}

bool CScriptInterpreter::isIdentNextChar(char c) {
	return isalnum(c) || c=='_';
}

void CScriptInterpreter::TokenBegin(char *s) {
	tokstr = s;
	tokhold = 0;
}

void CScriptInterpreter::TokenUnput(int t) {
	tokhold = t;

#ifdef DEBUG_TOKEN
	if (t>=' ' && t<256)
		_RPT2(0,"TokenUnput('%c' (%d))\n", (char)t, t);
	else
		_RPT1(0,"TokenUnput(%d)\n", t);
#endif
}

int CScriptInterpreter::Token() {
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
		char *t,*s = tokstr;
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

		tokslit = strheap.Allocate(tokstr - s - 1 - len_adjust, true);
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
		_RPT1(0,"Literal: [%s]\n", *tokslit);
#endif

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
		if (c=='0') {
			if (tokstr[0] == 'x') {

				// hex (base 16)

				toklval = 0;
				++tokstr;

				while(isxdigit(c = *tokstr++)) {
					toklval = toklval*16 + (strchr(hexdig, toupper(c))-hexdig);
				}
				--tokstr;

				return TOK_INTEGER;

			} else {
				// octal (base 8)

				toklval = 0;

				while((c=*tokstr++)>='0' && c<='7')
					toklval = toklval*8 + (c-'0');

				--tokstr;

				return TOK_INTEGER;
			}
		} else {
			// decimal

			toklval = (c-'0');

			while(isdigit(c=*tokstr++))
				toklval = toklval*10 + (c-'0');

			--tokstr;

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
