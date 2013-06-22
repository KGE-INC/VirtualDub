#include "stdafx.h"
#include <list>
#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdvideofiltold.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/error.h>
#include "vf_base.h"
#include "VBitmap.h"

extern FilterFunctions g_filterFuncs;

///////////////////////////////////////////////////////////////////////////

namespace {
	enum {
		kTokenNull		= 0,
		kTokenIdent		= 256,
		kTokenInt,
		kTokenLong,
		kTokenDbl,
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
	const char *String() const { return mString.data(); }
	int Strlen() const { return mString.size(); }
	const char *Ident() const { return mIdent; }
	int IntVal() const { return mIntVal; }
	sint64 LongVal() const { return mLongVal; }
	double DblVal() const { return mDblVal; }

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

	union {
		int mIntVal;
		sint64 mLongVal;
		double mDblVal;
	};

	vdfastvector<char>	mString;
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
		sint64 v = 0;

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
			} else {									//octal
				while((unsigned char)(*mSrc - '0') < 8) {
					v = (v<<3) + (*mSrc - '0');

					if (++mSrc == mSrcEnd)
						break;
				}
			}
		}

		const char *s = mSrc - 1;

		// decimal
		for(;;) {
			v = (v*10) + (c - '0');
			if (mSrc == mSrcEnd)
				break;
			c = *mSrc;

			if (c == '.' || c == 'e' || c == 'E') {
				// It's a float -- parse and return.
				++mSrc;
				mDblVal = 0;
				int chars = 0;

				char format[10];
				sprintf(format, "%%%dlg%%n", (int)std::min<ptrdiff_t>(mSrcEnd - mSrc, 509)); 
				if (1 == sscanf(s, format, &mDblVal, &chars))
					mSrc = s + chars;

				return kTokenDbl;
			}

			if (!isdigit((unsigned char)c))
				break;
			++mSrc;
		}

		if (v > 0x7FFFFFFF) {
			mLongVal = v;
			return kTokenLong;
		} else {
			mIntVal = (int)v;
			return kTokenInt;
		}
	}

	// symbol?
	char d = 0;

	if (mSrc != mSrcEnd)
		d = *mSrc;

	switch(c) {
	case '+':
	case '-':
	case '~':
	case ';':
	case '(':
	case ')':
	case ',':
	case '!':
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
	case '~':	return "~";
	case '(':	return "(";
	case ')':	return ")";
	case ',':	return ",";
	case '!':	return "!";
	default:	return "unknown";
	}
}

///////////////////////////////////////////////////////////////////////////

class VDVideoFilterAdapterConfigParser : public IScriptInterpreter {
public:
	VDVideoFilterAdapterConfigParser(const char *s) : mTok(s) {}
	~VDVideoFilterAdapterConfigParser();

	void Parse();

	const VDStringA& Name() const { return mName; }
	int Argc() const { return mStack.size(); }
	const CScriptValue *Argv() const { return &mStack[0]; }
	CScriptValue *Argv() { return &mStack[0]; }

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
	void ParseValue();
	void RequireNumeric(int tok);

	VDVideoFilterAdapterTokenizer	mTok;

	VDStringA	mName;

	std::vector<CScriptValue>	mStack;
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

		ParseValue();
		first = false;
	}

	mTok.Expect(0);
}

void VDVideoFilterAdapterConfigParser::ParseValue() {
	int tok;
	
	do {
		tok = mTok.Next();
	} while(tok == '+');

	if (tok == '-') {
		ParseValue();
		RequireNumeric(tok);
		mStack.back().u.i = -mStack.back().u.i;
		return;
	}

	if (tok == '~') {
		ParseValue();
		RequireNumeric(tok);
		mStack.back().u.i = ~mStack.back().u.i;
		return;
	}

	if (tok == '!') {
		ParseValue();
		RequireNumeric(tok);
		mStack.back().u.i = !mStack.back().u.i;
		return;
	}

	if (tok == '(') {
		ParseValue();
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

void VDVideoFilterAdapterConfigParser::RequireNumeric(int tok) {
	CScriptValue& v = mStack.back();
	if (!v.isInt() && !v.isDouble() && !v.isLong())
		throw MyError("operator '%s' requires a numeric type", mTok.TokenName(tok));
}

///////////////////////////////////////////////////////////////////////////

struct VFBitmapImpl : public VBitmap {
	uint32	dwFlags;
	HDC		hdc;
};

///////////////////////////////////////////////////////////////////////////

class VDVideoFilterAdapterPreview : public IVDFilterPreview2 {
public:
	void SetButtonCallback(FilterPreviewButtonCallback, void *) {}
	void SetSampleCallback(FilterPreviewSampleCallback, void *) {}

	bool isPreviewEnabled() {
		return false;
	}

	bool IsPreviewDisplayed() {
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

VDVFBASE_BEGIN_CONFIG(Adapter);
	VDVFBASE_CONFIG_ENTRY(Adapter, 0, WStr, config, L"Configuration string", L"Script configuration string for V1.x filter");
VDVFBASE_END_CONFIG(Adapter, 0);

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
	size_t GetBlurb(wchar_t *buf, size_t bufsize);

protected:
	void LoadConfiguration();
	void SaveConfiguration();

	virtual void *GetConfigPtr() { return &mConfig; }

	FilterDefinition		*mpDef;
	FilterActivation		mAct;
	VFBitmapImpl			mSrc;
	VFBitmapImpl			mDst;
	VFBitmapImpl			mLast;

	FilterStateInfo			mfsi;
	double					mSrcMSPerFrame;
	uint32					mFilterMode;			// value returned by paramProc
	
	VDVideoFilterData_Adapter		mConfig;
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
	mAct.ifp2			= NULL;
	mAct.pfsi			= NULL;

	memset(mAct.filter_data, 0, mpDef->inst_data_size);

	if (mpDef->initProc)
		mpDef->initProc(&mAct, &g_filterFuncs);
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

	if (mFilterMode & FILTERPARAM_SWAP_BUFFERS) {
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
	} else
		mSrc = mDst;

	mLast.data		= NULL;
	mLast.palette	= NULL;
	mLast.depth		= 32;
	mLast.w			= 0;
	mLast.h			= 0;
	mLast.pitch		= 0;
	mLast.modulo	= 0;
	mLast.offset	= 0;
	mLast.dwFlags	= 0;
	mLast.hdc		= 0;
	mLast.size		= 0;

	if (mFilterMode & FILTERPARAM_NEEDS_LAST) {
		if (!mpContext->mpDstFrame->mFrameNum)
			mLast = mSrc;
		else {
			const VDPixmap& last = *mpContext->mpSrcFrames[1]->mpPixmap;

			mLast.data		= (Pixel32 *)((char *)last.data + last.pitch*(last.h - 1));
			mLast.palette	= NULL;
			mLast.depth		= 32;
			mLast.w			= last.w;
			mLast.h			= last.h;
			mLast.pitch		= -last.pitch;
			mLast.modulo	= mLast.pitch - 4*last.w;
			mLast.offset	= 0;
			mLast.dwFlags	= 0;
			mLast.hdc		= 0;
			mLast.size		= ptrdiff_abs(mLast.pitch) * mLast.h;
		}
	}

	mpDef->runProc(&mAct, &g_filterFuncs);

	return kVFVRun_OK;
}

void VDVideoFilterAdapter::Prefetch(sint64 frame) {
	mpContext->Prefetch(0, frame, 0);

	if ((mFilterMode & FILTERPARAM_NEEDS_LAST) && frame > 0)
		mpContext->Prefetch(0, frame-1, 0);
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
	if (mpDef->startProc) {
		int rv = mpDef->startProc(&mAct, &g_filterFuncs);

		if (rv)
			mpContext->mpServices->SetError("Cannot start video filter \"%s\": unknown reason.", mpDef->name);
	}
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
	mAct.ifp2 = &preview;

	if (mpDef->configProc(&mAct, &g_filterFuncs, hwnd))
		return false;

	mAct.ifp = NULL;
	mAct.ifp2 = NULL;

	SaveConfiguration();

	return true;
}

size_t VDVideoFilterAdapter::GetBlurb(wchar_t *wbuf, size_t bufsize) {
	char buf[3072];
	buf[0] = 0;	// just in case

	if (mpDef->stringProc2) {
		mpDef->stringProc2(&mAct, &g_filterFuncs, buf, 3072);
	} else if (mpDef->stringProc) {
		mpDef->stringProc(&mAct, &g_filterFuncs, buf);
	} else {
		buf[0] = 0;
		return 0;
	}

	buf[3071] = 0;

	int reqlen = VDTextAToWLength(buf, -1);
	if (reqlen+1 >= bufsize)
		return reqlen;

	int result = VDTextAToW(wbuf, bufsize, buf, -1);
	return result;
}

void VDVideoFilterAdapter::LoadConfiguration() {
	if (mConfig.config.empty())
		return;

	VDStringA conf(VDTextWToA(mConfig.config));
	VDVideoFilterAdapterConfigParser p(conf.c_str());

	p.Parse();

	int argc = p.Argc();
	CScriptValue *const argv = p.Argv();
	const char *name = p.Name().c_str();
	bool ambiguous = false;

	if (mpDef->script_obj && mpDef->script_obj->func_list) {
		const ScriptFunctionDef *pDef = mpDef->script_obj->func_list;

		const ScriptFunctionDef *sfd_best = NULL;

		int *alloc_scores = new int[2 * (argc+1)];
		int *best_scores = alloc_scores;
		int *current_scores = alloc_scores + (argc+1);
		int best_promotions = 0;

		while(pDef->func_ptr) {
			if (pDef->name && !strcmp(pDef->name, name)) {
				do {
					const CScriptValue *csv = argv;
					const char *argtypes = pDef->arg_list + 1;
					int i;
					bool better_conversion;

					// reset current scores to zero (default)
					int current_promotions = 0;

					memset(current_scores, 0, sizeof(int) * (argc + 1));

					enum {
						kConversion = -2,
						kEllipsis = -3
					};

					for(i=0; i<argc; ++i) {
						const char c = *argtypes++;

						if (!c)
							goto arglist_nomatch;

						switch(c) {
						case 'v':
							break;
						case 'i':
							if (csv->isLong() || csv->isDouble())
								current_scores[i] = kConversion;
							else if (!csv->isInt())
								goto arglist_nomatch;
							break;
						case 'l':
							if (csv->isDouble())
								current_scores[i] = kConversion;
							else if (csv->isInt())
								++current_promotions;
							else if (!csv->isLong())
								goto arglist_nomatch;
							break;
						case 'd':
							if (csv->isInt() || csv->isLong())
								++current_promotions;
							else if (!csv->isDouble())
								goto arglist_nomatch;
							break;
						case 's':
							if (!csv->isString()) goto arglist_nomatch;
							break;
						case '.':
							--argtypes;			// repeat this character
							break;
						default:
							VDDEBUG("vf_adapter external error: invalid argument type '%c' for method\n", c);

							throw MyError("Video filter '%s' contains an unrecognized config format string: '%s'", mpDef->name, pDef->arg_list);
						}

						++csv;
					}

					// check if we have no parms left, or only an ellipsis
					if (*argtypes == '.' && !argtypes[1]) {
						current_scores[argc] = kEllipsis;
					} else if (*argtypes)
						goto arglist_nomatch;

					// do check for better match
					better_conversion = true;

					if (sfd_best) {
						better_conversion = false;
						for(int i=0; i<=argc; ++i) {		// we check one additional parm, the ellipsis parm
							// reject if there is a worse conversion than the best match so far
							if (current_scores[i] < best_scores[i]) {
								goto arglist_nomatch;
							}

							// add +1 if there is a better match
							if (current_scores[i] > best_scores[i])
								better_conversion = true;
						}

						// all things being equal, choose the method with fewer promotions
						if (!better_conversion) {
							if (current_promotions < best_promotions)
								better_conversion = true;
							else if (current_promotions == best_promotions)
								ambiguous = true;
						}
					}

					if (better_conversion) {
						std::swap(current_scores, best_scores);
						sfd_best = pDef;
						best_promotions = current_promotions;
						ambiguous = false;
					}

arglist_nomatch:
					++pDef;
				} while(pDef->func_ptr && !pDef->name);
			}
		}

		delete[] alloc_scores;

		if (sfd_best && !ambiguous) {
			// coerce arguments
			const char *const argdesc = sfd_best->arg_list + 1;

			for(int i=0; i<argc; ++i) {
				CScriptValue& a = argv[i];
				switch(argdesc[i]) {
				case 'i':
					if (a.isLong())
						a = CScriptValue((int)a.asLong());
					else if (a.isDouble())
						a = CScriptValue((int)a.asDouble());
					break;
				case 'l':
					if (a.isInt())
						a = CScriptValue((sint64)a.asInt());
					else if (a.isDouble())
						a = CScriptValue((sint64)a.asDouble());
					break;
				case 'd':
					if (a.isInt())
						a = CScriptValue((double)a.asInt());
					else if (a.isLong())
						a = CScriptValue((double)a.asLong());
					break;
				}
			}

			((ScriptVoidFunctionPtr)sfd_best->func_ptr)(&p, &mAct, argv, argc);
			return;
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

	if (ambiguous)
		throw MyError("Filter configuration error: ambiguous methods for signature \"%s(%s)\"", name, arglist.c_str());
	else
		throw MyError("Filter configuration error: cannot find method \"%s(%s)\"", name, arglist.c_str());
}

void VDVideoFilterAdapter::SaveConfiguration() {
	char buf[4096];

	buf[0] = 0;
	if (mpDef->fssProc)
		mpDef->fssProc(&mAct, &g_filterFuncs, buf, sizeof buf);

	mConfig.config = VDTextAToW(buf);
}

extern const struct VDVideoFilterDefinition vfilterDef_adapter = {
	sizeof(VDVideoFilterDefinition),
	0,
	1,	1,
	&VDVideoFilterData_Adapter::members.info,
	NULL,
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
