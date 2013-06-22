#include "stdafx.h"
#include <list>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofiltold.h>
#include <vd2/system/Error.h>
#include "vf_base.h"
#include "VBitmap.h"

extern FilterFunctions g_filterFuncs;

///////////////////////////////////////////////////////////////////////////

namespace {
	enum {
		kTokenNull		= 0,
		kTokenIdent		= 256,
		kTokenInt,
		kTokenString,
		kTokenAnd,
		kTokenOr,
		kTokenNE,
		kTokenEQ,
		kTokenLE,
		kTokenGE,
		kTokenDeclare	= 512,
		kTokenTrue,
		kTokenFalse,
		kTokenCount
	};

	inline ptrdiff_t ptrdiff_abs(ptrdiff_t x) { return x<0 ? -x : x; }
};

class VDVideoFilterAdapterTokenizer {
public:
	VDVideoFilterAdapterTokenizer(const char *src)
		: mPushedToken(0), mSrc(src), mSrcEnd(src + strlen(src)) {}

	int Next();
	void Push(int tok) { mPushedToken = tok; }
	const char *String() const { return &mString[0]; }
	int Strlen() const { return mString.size(); }
	const char *Ident() const { return mIdent; }
	int IntVal() const { return mIntVal; }

	void Expect(int tok);
	void Expect(int tok, int find);
	const char *TokenName(int tok);

protected:
	int mPushedToken;
	const char *mSrc;
	const char *const mSrcEnd;

	enum {
		kMaxIdent = 63
	};

	int mIntVal;

	std::vector<char>	mString;
	char mIdent[kMaxIdent + 1];
};

int VDVideoFilterAdapterTokenizer::Next() {
	if (mPushedToken) {
		int t = mPushedToken;
		mPushedToken = 0;
		return t;
	}

	// skip spaces and comments
	for(;;) {
		if (mSrc == mSrcEnd)
			return 0;

		char c = *mSrc;

		if (isspace((unsigned char)c)) {
			++mSrc;
			continue;
		}

		if (c == '/' && mSrc+1 != mSrcEnd && mSrc[1]=='/') {
			mSrc += 2;

			while(mSrc != mSrcEnd && *mSrc != '\r' && *mSrc != '\n')
				++mSrc;

			continue;
		}
		
		break;
	}

	char c = *mSrc++;

	// string?
	if (c == '"') {
		// is a string
		mString.clear();
		for(;;) {
			if (mSrc == mSrcEnd || (*mSrc == '\r' || *mSrc == '\n'))
				throw MyError("unterminated string literal");

			char c = *mSrc++;

			if (c == '"')
				break;

			if (c == '\\') {
				static const char hexdig[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

				c = 0;

				if (mSrc != mSrcEnd) {
					c = *mSrc;
					switch(c) {
					case 'a': c='\a'; break;
					case 'b': c='\b'; break;
					case 'f': c='\f'; break;
					case 'n': c='\n'; break;
					case 'r': c='\r'; break;
					case 't': c='\t'; break;
					case 'v': c='\v'; break;
					case 'x':
						if (mSrcEnd - mSrc < 2)
							c=0;
						else {
							c = (char)(strchr(hexdig,toupper(mSrc[0]))-hexdig);
							c = (char)((c<<4) | (strchr(hexdig, toupper(mSrc[1]))-hexdig));
							mSrc += 2;
						}
						break;
					default:
						break;
					}
				}

				if (!c)
					throw MyError("invalid string escape");
			}

			mString.push_back(c);
		}

		return kTokenString;
	}

	// identifier?
	if (c == '_' || isalpha((unsigned char)c)) {
		// is identifier
		const char *s = mSrc-1;

		while(mSrc != mSrcEnd && (*mSrc == '_' || isalnum((unsigned char)*mSrc)))
			++mSrc;

		ptrdiff_t len = mSrc - s;

		if (len > kMaxIdent)
			throw MyError("identifier too long");

		memcpy(mIdent, s, len);
		mIdent[len] = 0;

		if (!strcmp(mIdent, "declare"))
			return kTokenDeclare;
		if (!strcmp(mIdent, "true"))
			return kTokenTrue;
		if (!strcmp(mIdent, "false"))
			return kTokenFalse;

		return kTokenIdent;
	}

	// value?
	if (isdigit((unsigned char)c)) {
		sint32 v = 0;

		if (c == '0' && mSrc != mSrcEnd) {
			if (*mSrc == 'x') {							//hex
				if (mSrc == mSrcEnd || !isxdigit((unsigned char)(c = *mSrc++)))
					throw MyError("parse error");

				for(;;) {
					c = (char)(toupper((unsigned char)c) - '0');
					if (c>=10)
						c-=6;
					v = (v<<4) + c;
					if (mSrc == mSrcEnd)
						break;
					c = *mSrc;
					if (!isxdigit((unsigned char)c))
						break;
					++mSrc;
				}

				return kTokenInt;
			} else {									//octal
				while((unsigned char)(*mSrc - '0') < 8) {
					v = (v<<3) + (*mSrc - '0');

					if (++mSrc == mSrcEnd)
						break;
				}
				mIntVal = v;
				return kTokenInt;
			}
		}

		// decimal
		for(;;) {
			v = (v*10) + (c - '0');
			if (mSrc == mSrcEnd)
				break;
			c = *mSrc;
			if (!isdigit((unsigned char)c))
				break;
			++mSrc;
		}
		mIntVal = v;
		return kTokenInt;
	}

	// symbol?
	char d = 0;

	if (mSrc != mSrcEnd)
		d = *mSrc;

	switch(c) {
	case '+':
	case '-':
	case '*':
	case '/':
	case '^':
	case '~':
	case ';':
	case '%':
	case '(':
	case ')':
	case ',':
		return c;
	case '&':
		if (d == '&') {
			++mSrc;
			return kTokenAnd;
		}
		return c;
	case '|':
		if (d == '|') {
			++mSrc;
			return kTokenOr;
		}
		return c;
	case '<':
		if (d == '=') {
			++mSrc;
			return kTokenLE;
		}
		return c;
	case '>':
		if (d == '=') {
			++mSrc;
			return kTokenGE;
		}
		return c;
	case '=':
		if (d == '=') {
			++mSrc;
			return kTokenEQ;
		}
		return c;
	case '!':
		if (d == '=') {
			++mSrc;
			return kTokenNE;
		}
		return c;
	}

	throw MyError("unrecognized symbol");
}

void VDVideoFilterAdapterTokenizer::Expect(int tok) {
	Expect(tok, Next());
}

void VDVideoFilterAdapterTokenizer::Expect(int tok, int find) {
	if (find != tok)
		throw MyError("expected '%s', found '%s'", TokenName(tok), TokenName(find));
}

const char *VDVideoFilterAdapterTokenizer::TokenName(int tok) {
	switch(tok) {
	case 0:		return "end";
	// better way to do this?
	case '+':	return "+";
	case '-':	return "-";
	case '*':	return "*";
	case '/':	return "/";
	case '^':	return "^";
	case '~':	return "~";
	case '%':	return "%";
	case '(':	return "(";
	case ')':	return ")";
	case ',':	return ",";
	case '&':	return "&";
	case '|':	return "|";
	case '<':	return "<";
	case '>':	return ">";
	case '=':	return "=";
	case '!':	return "!";
	case kTokenInt:		return "int";
	case kTokenString:	return "string";
	case kTokenAnd:		return "&&";
	case kTokenOr:		return "||";
	case kTokenNE:		return "!=";
	case kTokenEQ:		return "==";
	case kTokenLE:		return "<=";
	case kTokenGE:		return ">=";
	case kTokenDeclare:	return "declare";
	case kTokenTrue:	return "true";
	case kTokenFalse:	return "false";
	default:	return "unknown";
	}
}

///////////////////////////////////////////////////////////////////////////

class VDVideoFilterAdapterConfigParser : public IScriptInterpreter {
public:
	VDVideoFilterAdapterConfigParser(const char *s) : mTok(s) {}
	~VDVideoFilterAdapterConfigParser();

	void Parse();
	void ParseExpression();

	const VDStringA& Name() const { return mName; }
	int Argc() const { return mStack.size(); }
	const CScriptValue *Argv() const { return &mStack[0]; }

public:
	void ScriptError(int e) {
		throw MyError("configuration error");
	}

	char** AllocTempString(long l) {
		mStrings.push_back(new char[l + 1]);
		mStrings.back()[l] = 0;
		return &mStrings.back();
	}

protected:
	int Precedence(int tok);
	void ParseValue();
	void RequireInt(int tok, int count);

	VDVideoFilterAdapterTokenizer	mTok;

	VDStringA	mName;

	std::vector<CScriptValue>	mStack;
	std::vector<int>			mOpStack;
	std::list<char *>			mStrings;
};

VDVideoFilterAdapterConfigParser::~VDVideoFilterAdapterConfigParser() {
	while(!mStrings.empty()) {
		delete mStrings.back();
		mStrings.pop_back();
	}
}

void VDVideoFilterAdapterConfigParser::Parse() {
	mTok.Expect(kTokenIdent);
	mName = mTok.Ident();

	mTok.Expect('(');

	bool first = true;

	for(;;) {
		int tok = mTok.Next();

		if (tok == ')')
			break;
		else if (first)
			mTok.Push(tok);
		else
			mTok.Expect(',', tok);

		ParseExpression();
		first = false;
	}

	mTok.Expect(0);
}

void VDVideoFilterAdapterConfigParser::ParseExpression() {
	int ops = 0;

	for(;;) {
		ParseValue();

		int tok = mTok.Next();
		int prec = Precedence(tok);

		while(ops > 0 && prec <= Precedence(mOpStack.back())) {
			--ops;

			CScriptValue& xent = mStack.back();
			CScriptValue& yent = *(mStack.end()-2);

			int op = mOpStack.back();

			RequireInt(op, 2);

			int& xi = xent.u.i;
			int& yi = yent.u.i;

			switch(op) {
			case '*':		yi *= xi; break;
			case '/':		if (!xi) throw MyError("divide by zero"); else yi /= xi; break;
			case '%':		if (!xi) throw MyError("divide by zero"); else yi %= xi; break;
			case '+':		yi += xi; break;
			case '-':		yi -= xi; break;
			case '^':		yi ^= xi; break;
			case '&':		yi &= xi; break;
			case '|':		yi |= xi; break;
			case '<':		yi = yi < xi; break;
			case '>':		yi = yi > xi; break;
			case kTokenNE:	yi = yi != xi; break;
			case kTokenEQ:	yi = yi == xi; break;
			case kTokenLE:	yi = yi <= xi; break;
			case kTokenGE:	yi = yi >= xi; break;
			case kTokenAnd:	yi = yi && xi; break;
			case kTokenOr:	yi = yi || xi; break;
			default:		VDNEVERHERE;
			}

			mStack.pop_back();
			mOpStack.pop_back();
		}

		if (!prec) {
			mTok.Push(tok);
			break;
		}

		mOpStack.push_back(tok);
		++ops;
	}
}

int VDVideoFilterAdapterConfigParser::Precedence(int tok) {
	switch(tok) {
	case '*':
	case '/':
	case '%':
		return 5;
	case '+':
	case '-':
		return 4;
	case '^':
	case '&':
	case '|':
		return 3;
	case '<':
	case '>':
	case kTokenNE:
	case kTokenEQ:
	case kTokenLE:
	case kTokenGE:
		return 2;
	case kTokenAnd:
	case kTokenOr:
		return 1;
	default:
		return 0;
	}
}

void VDVideoFilterAdapterConfigParser::ParseValue() {
	int tok;
	
	do {
		tok = mTok.Next();
	} while(tok == '+');

	if (tok == '-') {
		ParseValue();
		RequireInt(tok, 1);
		mStack.back().u.i = -mStack.back().u.i;
		return;
	}

	if (tok == '~') {
		ParseValue();
		RequireInt(tok, 1);
		mStack.back().u.i = ~mStack.back().u.i;
		return;
	}

	if (tok == '!') {
		ParseValue();
		RequireInt(tok, 1);
		mStack.back().u.i = !mStack.back().u.i;
		return;
	}

	if (tok == '(') {
		ParseExpression();
		mTok.Expect(')');
		return;
	}

	if (tok == kTokenTrue) {
		mStack.push_back(CScriptValue(1));
		return;
	}

	if (tok == kTokenFalse) {
		mStack.push_back(CScriptValue(1));
		return;
	}

	if (tok == kTokenString) {
		const int l = mTok.Strlen();
		char **handle = AllocTempString(l);
		memcpy(*handle, mTok.String(), l);
		mStack.push_back(CScriptValue(handle));
		return;
	}

	if (tok == kTokenInt) {
		mStack.push_back(CScriptValue(mTok.IntVal()));
		return;
	}

	mTok.Expect(tok, kTokenInt);
}

void VDVideoFilterAdapterConfigParser::RequireInt(int tok, int count) {
	int n = mStack.size();

	while(count-->0) {
		--n;

		if (!mStack[n].isInt())
			throw MyError("operator '%s' requires integers", mTok.TokenName(tok));
	}
}

///////////////////////////////////////////////////////////////////////////

struct VFBitmapImpl : public VBitmap {
	uint32	dwFlags;
	HDC		hdc;
};

///////////////////////////////////////////////////////////////////////////

class VDVideoFilterAdapterPreview : public IFilterPreview {
public:
	void SetButtonCallback(FilterPreviewButtonCallback, void *) {}
	void SetSampleCallback(FilterPreviewSampleCallback, void *) {}

	bool isPreviewEnabled() {
		return false;
	}
	void Toggle(HWND) {}
	void Display(HWND, bool) {}
	void RedoFrame() {}
	void RedoSystem() {}
	void UndoSystem() {}
	void InitButton(HWND) {}
	void Close() {}

	bool SampleCurrentFrame() {
		return false;
	}

	long SampleFrames() {
		return 0;
	}
};

class VDVideoFilterAdapter : public VDVideoFilterBase {
public:
	static void *Create(const VDVideoFilterContext *pContext) {
		return new VDVideoFilterAdapter(pContext);
	}

	VDVideoFilterAdapter(const VDVideoFilterContext *pContext);
	~VDVideoFilterAdapter();

	sint32 Run();
	void Prefetch(sint64 frame);
	sint32 Prepare();
	void Start();
	void Stop();
	unsigned Suspend(void *dst, unsigned size);
	void Resume(const void *src, unsigned size);
	bool Config(HWND hwnd);

protected:
	void LoadConfiguration();
	void SaveConfiguration();

	FilterDefinition		*mpDef;
	FilterActivation		mAct;
	VFBitmapImpl			mSrc;
	VFBitmapImpl			mDst;
	VFBitmapImpl			mLast;

	FilterStateInfo			mfsi;
	double					mSrcMSPerFrame;
	uint32					mFilterMode;			// value returned by paramProc

	VDStringW				mConfigStr;
};

VDVideoFilterAdapter::VDVideoFilterAdapter(const VDVideoFilterContext *pContext)
	: VDVideoFilterBase(pContext)
	, mAct(reinterpret_cast<VFBitmap&>(mDst), reinterpret_cast<VFBitmap&>(mSrc), reinterpret_cast<VFBitmap*>(&mLast))
{
	mpDef = (FilterDefinition *)pContext->mpFilterData;

	mAct.x1				= 0;
	mAct.y1				= 0;
	mAct.x2				= 0;
	mAct.y2				= 0;
	mAct.filter			= mpDef;
	mAct.filter_data	= malloc(mpDef->inst_data_size);
	mAct.ifp			= NULL;
	mAct.pfsi			= NULL;

	memset(mAct.filter_data, 0, mpDef->inst_data_size);

	if (mpDef->initProc)
		mpDef->initProc(&mAct, &g_filterFuncs);

	VDVideoFilterAdapterConfigParser p("Config(1+2*3)");

	p.Parse();
}

VDVideoFilterAdapter::~VDVideoFilterAdapter() {
	if (mpDef->deinitProc)
		mpDef->deinitProc(&mAct, &g_filterFuncs);

	free(mAct.filter_data);
}

sint32 VDVideoFilterAdapter::Run() {
	if (mFilterMode & FILTERPARAM_SWAP_BUFFERS) {
		mpContext->AllocFrame();
	} else { 
		mpContext->mpSrcFrames[0]->AddRef();
		const_cast<VDVideoFilterFrame *&>(mpContext->mpDstFrame) = mpContext->mpSrcFrames[0]->WriteCopy();
	}

	const VDPixmap& src = *mpContext->mpSrcFrames[0]->mpPixmap;
	const VDPixmap& dst = *mpContext->mpDstFrame->mpPixmap;

	mfsi.lCurrentSourceFrame	= (long)mpContext->mpSrcFrames[0]->mFrameNum;
	mfsi.lSourceFrameMS			= VDRoundToLong(mSrcMSPerFrame * mpContext->mpSrcFrames[0]->mFrameNum);

	mfsi.lCurrentFrame			= mfsi.lCurrentSourceFrame;
	mfsi.lDestFrameMS			= mfsi.lSourceFrameMS;

	mSrc.data		= (Pixel32 *)((char *)src.data + src.pitch*(src.h - 1));
	mSrc.palette	= NULL;
	mSrc.depth		= 32;
	mSrc.w			= src.w;
	mSrc.h			= src.h;
	mSrc.pitch		= -src.pitch;
	mSrc.modulo		= mSrc.pitch - 4*src.w;
	mSrc.offset		= 0;
	mSrc.dwFlags	= 0;
	mSrc.hdc		= 0;
	mSrc.size		= ptrdiff_abs(mSrc.pitch) * mSrc.h;

	mDst.data		= (Pixel32 *)((char *)dst.data + dst.pitch*(dst.h - 1));
	mDst.palette	= NULL;
	mDst.depth		= 32;
	mDst.w			= dst.w;
	mDst.h			= dst.h;
	mDst.pitch		= -dst.pitch;
	mDst.modulo		= mDst.pitch - 4*dst.w;
	mDst.offset		= 0;
	mDst.dwFlags	= 0;
	mDst.hdc		= 0;
	mDst.size		= ptrdiff_abs(mDst.pitch) * mDst.h;

	mpDef->runProc(&mAct, &g_filterFuncs);

	return kVFVRun_OK;
}

void VDVideoFilterAdapter::Prefetch(sint64 frame) {
	mpContext->Prefetch(0, frame, 0);
}

sint32 VDVideoFilterAdapter::Prepare() {
	LoadConfiguration();

	const VDVideoFilterPin& srcpin = *mpContext->mpInputs[0];
	const VDPixmap& src = *srcpin.mpFormat;
	VDPixmap& dst = *mpContext->mpOutput->mpFormat;

	mSrcMSPerFrame = (double)srcpin.mFrameRateHi * 1000.0 / (double)srcpin.mFrameRateLo;
	mfsi.lMicrosecsPerSrcFrame		= VDRoundToLong(mSrcMSPerFrame * 1000.0);
	mfsi.lMicrosecsPerFrame			= mfsi.lSourceFrameMS;

	mSrc.data		= NULL;
	mSrc.palette	= NULL;
	mSrc.depth		= 32;
	mSrc.w			= src.w;
	mSrc.h			= src.h;
	mSrc.pitch		= -src.pitch;
	mSrc.modulo		= mSrc.pitch - 4*src.w;
	mSrc.offset		= 0;
	mSrc.dwFlags	= 0;
	mSrc.hdc		= 0;
	mSrc.size		= ptrdiff_abs(mSrc.pitch) * mSrc.h;

	mDst = mSrc;
	if (mpDef->paramProc)
		mFilterMode = mpDef->paramProc(&mAct, &g_filterFuncs);
	else
		mFilterMode = FILTERPARAM_SWAP_BUFFERS;

	dst.data		= NULL;
	dst.palette		= NULL;
	dst.format		= nsVDPixmap::kPixFormat_XRGB8888;
	dst.w			= mDst.w;
	dst.h			= mDst.h;
	dst.pitch		= -mDst.pitch;
	dst.data2		= NULL;
	dst.data3		= NULL;
	dst.pitch2		= 0;
	dst.pitch3		= 0;

	return 0;
}

void VDVideoFilterAdapter::Start() {
	if (mpDef->startProc)
		mpDef->startProc(&mAct, &g_filterFuncs);
}

void VDVideoFilterAdapter::Stop() {
	if (mpDef->endProc)
		mpDef->endProc(&mAct, &g_filterFuncs);
}

unsigned VDVideoFilterAdapter::Suspend(void *dst, unsigned size) {
	return 0;
}

void VDVideoFilterAdapter::Resume(const void *src, unsigned size) {
}

bool VDVideoFilterAdapter::Config(HWND hwnd) {
	if (!hwnd)
		return mpDef->configProc != 0;

	if (!mpDef->configProc)
		return false;

	VDVideoFilterAdapterPreview preview;

	mAct.ifp = &preview;

	if (mpDef->configProc(&mAct, &g_filterFuncs, hwnd))
		return false;

	mAct.ifp = NULL;

	SaveConfiguration();

	return true;
}

void VDVideoFilterAdapter::LoadConfiguration() {
	if (mConfigStr.empty())
		return;

	VDStringA conf(VDTextWToA(mConfigStr));
	VDVideoFilterAdapterConfigParser p(conf.c_str());

	p.Parse();

	int argc = p.Argc();
	const CScriptValue *const argv = p.Argv();
	const char *name = p.Name().c_str();

	if (mpDef->script_obj && mpDef->script_obj->func_list) {
		const ScriptFunctionDef *pDef = mpDef->script_obj->func_list;

		while(pDef->func_ptr) {
			if (pDef->name && !strcmp(pDef->name, name)) {
				do {
					const char *argtypes = pDef->arg_list + 1;
					int i;

					for(i=0; i<argc; ++i) {
						const char checktype = *argtypes++;

						if (!checktype)
							break;

						if (checktype == '.') {
							i = argc;
							break;
						}

						if (checktype == 'v')
							continue;

						const char type = argv[i].isInt() ? 'i' : 's';
						if (checktype != type)
							break;
					}

					if (i == argc && !*argtypes || *argtypes == '.') {
						((ScriptVoidFunctionPtr)pDef->func_ptr)(&p, &mAct, argv, argc);
						return;
					}

					++pDef;
				} while(pDef->func_ptr && !pDef->name);
			}
		}
	}

	VDStringA arglist;

	for(int i=0; i<argc; ++i) {
		if (!arglist.empty())
			arglist.append(", ");

		if (argv[i].isInt())
			arglist.append("int");
		else
			arglist.append("string");
	}

	throw MyError("Filter configuration error: cannot find method \"%s(%s)\"", name, arglist.c_str());
}

void VDVideoFilterAdapter::SaveConfiguration() {
	char buf[4096];

	buf[0] = 0;
	if (mpDef->fssProc)
		mpDef->fssProc(&mAct, &g_filterFuncs, buf, sizeof buf);

	mConfigStr = VDTextAToW(buf);
}

const VDFilterConfigEntry vfilterDef_adapter_config={
	NULL, 0, VDFilterConfigEntry::kTypeWStr, L"config", L"Configuration string", L"Script configuration string for V1.x filter"
};

extern const struct VDVideoFilterDefinition vfilterDef_adapter = {
	sizeof(VDVideoFilterDefinition),
	0,
	1,	1,
	&vfilterDef_adapter_config,
	VDVideoFilterAdapter::Create,
	VDVideoFilterAdapter::MainProc,
};

extern const struct VDPluginInfo vpluginDef_adapter = {
	sizeof(VDPluginInfo),
	L"adapter",
	NULL,
	L"Adapts filters written against the old API.",
	0,
	kVDPluginType_Video,
	0,

	kVDPlugin_APIVersion,
	kVDPlugin_APIVersion,
	kVDPlugin_VideoAPIVersion,
	kVDPlugin_VideoAPIVersion,

	&vfilterDef_adapter
};
