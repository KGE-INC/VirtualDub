#include <math.h>
#include <float.h>
#include <ctype.h>
#include <crtdbg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <vd2/system/Props.h>
#include <vd2/system/PropsEvaluator.h>
#include <vd2/system/strutil.h>

static const char hexdig[16] = { '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };

enum {
	OP_NONE	= 0,
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_EXP,
	OP_NEG,
	OP_AND,
	OP_OR,

	OP_EQ,
	OP_LT,
	OP_LE,
	OP_GT,
	OP_GE,
	OP_NE,
};

enum ePromotionMode {
	kNone,
	kBinaryWidenToInt,
	kBinaryWidenToIntString,
	kBinaryForceDouble,
	kBinaryForceInt,
	kUnaryWidenToInt,
	kBinaryForceBool,
};

const int PropsEvaluator::prec[]={ 0, 3, 3, 4, 4, 5, -1, 1, 1, 2, 2, 2, 2, 2, 2 };
static const ePromotionMode promo_mode[]={
	kNone,	
	kBinaryWidenToIntString,	// +
	kBinaryWidenToInt,			// -
	kBinaryWidenToInt,			// *
	kBinaryForceDouble,			// /
	kBinaryForceDouble,			// ^
	kUnaryWidenToInt,			// unary -
	kBinaryForceBool,			// &&
	kBinaryForceBool,			// ||
	kBinaryWidenToInt,			// ==
	kBinaryWidenToInt,			// <
	kBinaryWidenToInt,			// <=
	kBinaryWidenToInt,			// >
	kBinaryWidenToInt,			// >=
	kBinaryWidenToInt			// !=
};

///////////////////////////////////////////////////////////////////////////

PropsEvaluator::PropsEvaluator(const IProps **_ppProps) : ppProps(_ppProps) {
	stack = new PropEVal[sp_max = 64];
}

PropsEvaluator::~PropsEvaluator() {
	delete[] stack;
}

///////////////////////////////////////////////////////////////////////////

const char *PropsEvaluator::evaluate(const char *str, bool bFunction) {
	bool have_val = false;
	char c;
	int parens = 0;
	int spbase = sp;

	try {
		while(c = *str++) {
			int op = 0;

			if (isspace(c))
				continue;

			if (c == ';' || c == ']')
				break;

			if (bFunction && (c==',' || (c==')' && !parens)))
				break;

			switch(c) {

				case '(':
					if (have_val)
						throw "illegal '(' after value";

					++parens;
					continue;

				case ')':
					if (!have_val)
						throw "illegal ')' before value";

					if (--parens<0)
						throw "missing (";

					_RPT0(0,"starting paren reduce\n");

					while(sp>spbase && parens<stack[sp-1].parens)
						reduce();

					_RPT0(0,"ending paren reduce\n");

					continue;

				case '+':
					op = OP_ADD;
					break;

				case '-':
					if (have_val)
						op = OP_SUB;
					else
						op = OP_NEG;
					break;

				case '*':
					op = OP_MUL;
					break;

				case '/':
					op = OP_DIV;
					break;

				case '^':
					op = OP_EXP;
					break;

				case '&':
					if (*str++ == '&')
						op = OP_AND;
					else
						throw "syntax error";

					break;

				case '|':
					if (*str++ == '|')
						op = OP_OR;
					else
						throw "syntax error";

					break;

				case '<':
					if (*str == '=') {
						op = OP_LE;
						++str;
					} else
						op = OP_LT;
					break;

				case '>':
					if (*str == '=') {
						op = OP_GE;
						++str;
					} else
						op = OP_GT;
					break;

				case '!':
					if (*str++ == '=')
						op = OP_NE;
					else
						throw "syntax error";

					break;

				case '=':
					if (*str++ == '=')
						op = OP_EQ;
					else
						throw "syntax error";

					break;

				case '\'':
					{
						const char *s = str;
						char *t;

						while(*s && *s!='\'')
							++s;

						if (!*s++)
							throw "unterminated string literal";

						t = (char *)malloc(s - str);

						memcpy(t, str, s-str-1);
						t[s-str-1] = 0;

						str = s;

						stack[sp].op = 0;
						stack[sp].v.s = t;
						stack[sp].type = PropDef::kString;
						have_val = true;
					}
					break;

				case '0':
					if (*str == 'x') {
						__int64 v = 0;

						++str;

						if (!isxdigit(*str))
							throw "bad hex value";

						stack[sp].type = PropDef::kInt;

						while(isxdigit(*str)) {
							v = (v<<4) + (strchr(hexdig, tolower(*str++))-hexdig);
						}

						stack[sp].v.i = (int)v;
						stack[sp].op = 0;
						have_val = true;
						break;
					}

					/* fall through (decimal) */

				default:
					if (c=='_' || isalpha(c)) {
						if (have_val)
							throw "missing operator between values";

						str = lookupvar(str);

						have_val = true;
					} else if (isdigit(c) || c=='.') {
						double d;
						bool decpt = false;

						if (have_val)
							throw "missing operator between values";

						stack[sp].type = PropDef::kInt;
						stack[sp].v.i = 0;

						do {
							if (c == '.')
								if (decpt)
									throw "two decimal points in value";
								else {
									decpt = true;
									d = 0.1;

									if (stack[sp].type == PropDef::kInt) {
										stack[sp].v.d = stack[sp].v.i;
										stack[sp].type = PropDef::kDouble;
									}
									continue;
								}

							if (stack[sp].type == PropDef::kInt) {
								stack[sp].v.i = (stack[sp].v.i*10) + (c-'0');
							} else if (decpt) {
								stack[sp].v.d += (c-'0')*d;
								d /= 10.0;
							} else
								stack[sp].v.d = (stack[sp].v.d*10.0) + (c-'0');

						} while(isdigit(c = *str++) || c=='.');
						--str;
						stack[sp].op = 0;
						have_val = true;

					} else
						throw "bad character";

					break;
			}

			if (op) {
				if (prec[op]>0) {	// binary
					if (!have_val)
						throw "Missing value before binary operator";

					while (sp>spbase && stack[sp-1].parens == parens
							&& (prec[stack[sp-1].op]<0 || prec[stack[sp-1].op] >= prec[op]))
						reduce();
				} else {			// unary
					if (have_val)
						throw "Value before unary operator";
				}

				stack[sp].op = op;
				stack[sp].parens = parens;
				++sp;
				have_val = false;

				if (sp >= sp_max)
					throw "Stack overflow";
			}
		}

		if (!have_val)
			throw "Missing value after operator";

		if (parens > 0)
			throw "Missing )";

		if (sp <= spbase && !have_val)
			throw "missing expression";

		while(sp > spbase)
			reduce();
	} catch(...) {
		if (!have_val)
			--sp;

		while(sp > 0) {
			if (stack[sp].type == PropDef::kString)
				free((char *)stack[sp].v.s);

			--sp;
		}

		throw;
	}

	return str-1;
}

void PropsEvaluator::reduce() {
	PropEVal& x = stack[sp];
	PropEVal& y = stack[sp-1];
	int type;

	_ASSERT(sp>0);

	if (x.type == PropDef::kString && y.type == PropDef::kString)
		_RPT2(0,"Merging strings [%s] and [%s]\n", x.v.s, y.v.s);

	if (prec[y.op]<0 || y.parens>x.parens)
		y.parens = x.parens;

	switch(promo_mode[y.op]) {
	case kBinaryWidenToIntString:	// widening conversion to at least int
		type = x.type;

		if (type == PropDef::kBool)
			type = PropDef::kInt;

		if (y.type == PropDef::kDouble && type == PropDef::kInt)
			type = PropDef::kDouble;

		if (y.type == PropDef::kString)
			type = PropDef::kString;

		convert(x, type);
		convert(y, type);
		break;

	case kBinaryWidenToInt:			// widening conversion to at least int
		type = x.type;

		if (type == PropDef::kBool)
			type = PropDef::kInt;

		if (y.type == PropDef::kDouble)
			type = PropDef::kDouble;

		if (y.type == PropDef::kInt && type == PropDef::kBool)
			type = PropDef::kInt;

		convert(x, type);
		convert(y, type);
		break;
	case kBinaryForceDouble:		// force double
		convert(x, PropDef::kDouble);
		convert(y, PropDef::kDouble);
		break;
	case kBinaryForceInt:		// force int
		convert(x, PropDef::kInt);
		convert(y, PropDef::kInt);
		break;
	case kUnaryWidenToInt:		// widen to int
		type = x.type;

		if (type == PropDef::kBool)
			convert(x, PropDef::kInt);
		break;
	case kBinaryForceBool:		// constrict to bool
		convert(x, PropDef::kBool);
		convert(y, PropDef::kBool);
		break;
	}

	switch(y.op) {
		case OP_ADD:
			if (type == PropDef::kInt)			y.v.i += x.v.i;
			else if (type == PropDef::kDouble)	y.v.d += x.v.d;
			else {
				int l2 = strlen(x.v.s);
				int l1 = strlen(y.v.s);
				char *t = (char *)realloc((char *)y.v.s, l1+l2+1);

				if (t)
					memcpy(t+l1, x.v.s, l2+1);

				free((char *)x.v.s);

				y.v.s = t;
			}
			break;

		case OP_SUB:
			if (type == PropDef::kInt)	y.v.i -= x.v.i;
			else					y.v.d -= x.v.d;
			break;

		case OP_MUL:
			if (type == PropDef::kInt)	y.v.i *= x.v.i;
			else					y.v.d *= x.v.d;
			break;

		case OP_DIV:
			if (fabs(x.v.d) == 0.0)
				throw "Divide by zero";
			y.v.d /= x.v.d;
			break;

		case OP_EXP:
			y.v.d = pow(y.v.d, x.v.d);
			break;

		case OP_NEG:
			if (type == PropDef::kInt)	y.v.i = -x.v.i;
			else					y.v.d = -x.v.d;

			y.type = x.type;
			break;

		case OP_AND:
			y.v.f &= x.v.f;
			break;

		case OP_OR:
			y.v.f |= x.v.f;
			break;

		case OP_EQ:
			y.v.f = (type == PropDef::kDouble ? y.v.d == x.v.d : y.v.i == x.v.i);
			y.type = PropDef::kBool;
			break;

		case OP_NE:
			y.v.f = (type == PropDef::kDouble ? y.v.d != x.v.d : y.v.i != x.v.i);
			y.type = PropDef::kBool;
			break;

		case OP_LT:
			y.v.f = (type == PropDef::kDouble ? y.v.d < x.v.d : y.v.i < x.v.i);
			y.type = PropDef::kBool;
			break;

		case OP_LE:
			y.v.f = (type == PropDef::kDouble ? y.v.d <= x.v.d : y.v.i <= x.v.i);
			y.type = PropDef::kBool;
			break;

		case OP_GT:
			y.v.f = (type == PropDef::kDouble ? y.v.d > x.v.d : y.v.i > x.v.i);
			y.type = PropDef::kBool;
			break;

		case OP_GE:
			y.v.f = (type == PropDef::kDouble ? y.v.d >= x.v.d : y.v.i >= x.v.i);
			y.type = PropDef::kBool;
			break;
	}

	--sp;
}

void PropsEvaluator::convert(PropEVal& x, int type) {

	if (x.type == PropDef::kDouble && _isnan(x.v.d))
		throw "Expression value is undefined";

	switch(type) {
	case PropDef::kDouble:
		switch(x.type) {
		case PropDef::kInt:	x.v.d = x.v.i; break;
		case PropDef::kBool:	x.v.d = x.v.f; break;
		case PropDef::kString:
			throw "Cannot convert from string to double";
		}
		break;
	case PropDef::kInt:
		switch(x.type) {
		case PropDef::kDouble:
			if (x.v.d < -2147483648.5 || x.v.d>=2147483647.5)
				throw "Integer overflow";

			x.v.i = (int)floor(x.v.d + 0.5);
			break;
		case PropDef::kBool:		x.v.i = x.v.f; break;
		case PropDef::kString:
			throw "Cannot convert from string to int";
		}
		break;
	case PropDef::kBool:
		if (x.type != PropDef::kBool)
			throw x.type==PropDef::kDouble?"Cannot convert from double to bool":
					x.type==PropDef::kInt?"Cannot convert from int to bool":
					"Cannot convert from string to bool";
		break;

	case PropDef::kString:
		{
			char buf[64];

			switch(x.type) {
			case PropDef::kBool:
				sprintf(buf, "%d", x.v.f);
				x.v.s = strdup(buf);
				break;

			case PropDef::kInt:
				sprintf(buf, "%ld", x.v.i);
				x.v.s = strdup(buf);
				break;

			case PropDef::kDouble:
				sprintf(buf, "%.15lg", x.v.d);
				x.v.s = strdup(buf);
				break;
			}
		}
		break;
	}

	x.type = type;
}

static const char *const szBuiltinFunctions[]={
	"pi", "abs", "min", "max", "sin", "cos", "tan", "round"
};

const char *PropsEvaluator::lookupvar(const char *s) {
	const IProps **ppp;
	char var[64];
	char *t = var;
	int idx;

	--s;
	while(isalnum(*s) || *s=='_') {
		if (t >= var-1+sizeof var)
			throw "Variable name too long";
		*t++ = *s++;
	}

	*t = 0;

	stack[sp].op = 0;

	// Is it a built-in function?

	for(int i=0; i<sizeof szBuiltinFunctions / sizeof szBuiltinFunctions[0]; i++) {
		if (!stricmp(var, szBuiltinFunctions[i])) {
			int spparambase = sp;
			int pcount;

			s = strskipspace(s);

			if (*s == '(') {
				++s;

				do {
					s = evaluate(s, true);
					++sp;
				} while(*s++ == ',');

				if (s[-1] != ')')
					throw "Expected ')' after function arguments";
			}

			pcount = sp - spparambase;
			sp = spparambase;

			switch(i) {
			case 0:
				if (sp > spparambase)
					throw "Function 'pi' does not take any parameters";

				stack[sp].type = PropDef::kDouble;
				stack[sp].v.d = 3.14159265358979323846;
				break;

			case 1:
				if (sp != spparambase+1)
					throw "Function 'abs' takes a single parameter";

				if (stack[sp].type == PropDef::kBool)
					throw "Cannot take absolute value of bool";

				if (stack[sp].type == PropDef::kDouble)
					stack[sp].v.d = fabs(stack[sp].v.d);
				else
					stack[sp].v.i = fabs(stack[sp].v.i);
				break;

			case 2:
				while(pcount > 1) {
					if (stack[sp+pcount-2].type == PropDef::kBool || stack[sp+pcount-1].type == PropDef::kBool)
						throw "Function 'min' does not accept booleans";

					if (stack[sp+pcount-2].type == PropDef::kDouble || stack[sp+pcount-1].type == PropDef::kDouble) {
						convert(stack[sp+pcount-2], PropDef::kDouble);
						convert(stack[sp+pcount-1], PropDef::kDouble);

						if (stack[sp+pcount-2].v.d > stack[sp+pcount-1].v.d)
							stack[sp+pcount-2].v.d = stack[sp+pcount-1].v.d;
					} else {
						convert(stack[sp+pcount-2], PropDef::kInt);
						convert(stack[sp+pcount-1], PropDef::kInt);

						if (stack[sp+pcount-2].v.i > stack[sp+pcount-1].v.i)
							stack[sp+pcount-2].v.i = stack[sp+pcount-1].v.i;
					}
					--pcount;
				}
				break;

			case 3:
				while(pcount > 1) {
					if (stack[sp+pcount-2].type == PropDef::kBool || stack[sp+pcount-1].type == PropDef::kBool)
						throw "Function 'max' does not accept booleans";

					if (stack[sp+pcount-2].type == PropDef::kDouble || stack[sp+pcount-1].type == PropDef::kDouble) {
						convert(stack[sp+pcount-2], PropDef::kDouble);
						convert(stack[sp+pcount-1], PropDef::kDouble);

						if (stack[sp+pcount-2].v.d < stack[sp+pcount-1].v.d)
							stack[sp+pcount-2].v.d = stack[sp+pcount-1].v.d;
					} else {
						convert(stack[sp+pcount-2], PropDef::kInt);
						convert(stack[sp+pcount-1], PropDef::kInt);

						if (stack[sp+pcount-2].v.i < stack[sp+pcount-1].v.i)
							stack[sp+pcount-2].v.i = stack[sp+pcount-1].v.i;
					}
					--pcount;
				}
				break;

			case 4:
				if (pcount != 1)
					throw "Function 'sin' takes a single parameter";

				convert(stack[sp], PropDef::kDouble);
				stack[sp].v.d = sin(stack[sp].v.d);
				break;

			case 5:
				if (pcount != 1)
					throw "Function 'cos' takes a single parameter";

				convert(stack[sp], PropDef::kDouble);
				stack[sp].v.d = cos(stack[sp].v.d);
				break;

			case 6:
				if (pcount != 1)
					throw "Function 'tan' takes a single parameter";

				convert(stack[sp], PropDef::kDouble);
				stack[sp].v.d = tan(stack[sp].v.d);
				break;

			case 7:
				if (pcount != 1)
					throw "Function 'round' takes a single parameter";

				convert(stack[sp], PropDef::kDouble);
				stack[sp].v.i = (int)floor(0.5 + stack[sp].v.d);
				stack[sp].type = PropDef::kInt;
				break;
			}

			return s;
		}
	}

	ppp = ppProps;

	while(*ppp) {
		if ((idx = (*ppp)->lookup(var))>=0)
			break;
		++ppp;
	}

	if (!*ppp)
		throw "Variable not found";

	switch((*ppp)->getType(idx)) {
	case PropDef::kInt:
		stack[sp].type = PropDef::kInt;
		stack[sp].v.i = (**ppp)[idx].i;
		break;
	case PropDef::kDouble:
		stack[sp].type = PropDef::kDouble;
		stack[sp].v.d = (**ppp)[idx].d;
		break;
	case PropDef::kBool:
		stack[sp].type = PropDef::kBool;
		stack[sp].v.f = (**ppp)[idx].f;
		break;
	default:
		throw "Variable not of type int, double, or bool";
	}

	return s;
}

/////////////////////////////////////////////////////////////

int PropsEvaluator::evaluateInt(const char *szExp) {
	sp = 0;
	evaluate(szExp, false);
	convert(stack[0], PropDef::kInt);
	return stack[0].v.i;
}

double PropsEvaluator::evaluateDouble(const char *szExp) {
	sp = 0;
	evaluate(szExp, false);
	convert(stack[0], PropDef::kDouble);
	return stack[0].v.d;
}

bool PropsEvaluator::evaluateBool(const char *szExp) {
	sp = 0;
	evaluate(szExp, false);
	convert(stack[0], PropDef::kBool);
	return stack[0].v.f;
}

char *PropsEvaluator::evaluateString(const char *szExp) {
	sp = 0;
	evaluate(szExp, false);
	convert(stack[0], PropDef::kString);
	return (char *)stack[0].v.s;
}
