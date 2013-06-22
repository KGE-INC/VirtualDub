#ifndef f_SYSTEM_VDSTRING_H
#define f_SYSTEM_VDSTRING_H

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>

#include <vd2/system/vdtypes.h>
#include <vd2/system/text.h>
#include <vd2/system/atomic.h>

///////////////////////////////////////////////////////////////////////////
//
//	VDBasicString
//
//	VDBasicString<T> is very strongly patterned after the STL
//	basic_string, but with one very important guarantee -- it is multi-
//	thread safe.  It is also guaranteed to use copy-on-write, which means
//	that although it is slower in general, it has fast assignment.  This
//	is very useful for STL containers.  It is unsafe for two threads to
//	simultaneously use a string when one is writing to it, but it is
//	guaranteed that multiple threads may read from a string without
//	spinlocking.
//
//	Not all basic_string<> capabilities are implemented.  In particular,
//	read-only accessors tend to be more complete than mutators.  Reverse
//	iteration is also unimplemented.
//
//	Code that constructs or performs serious modifications on strings
//	should use std::vector<> or std::deque<> instead of
//	std::basic_string<> or VDBasicString<>.

template<class T>
struct VDStringInstance {
	volatile int refcount;
	int length;
	T str[1];
};

template<class T>		// T must have a trivial copy constructor and copy assignment operator!!
struct VDCharOpsImpl {
	static void copy(T *dst, const T *src, int len) {
		if (len)
			memcpy(dst, src, len * sizeof(T));
	}

	static int len(const T *s) {
		const T *t = s;

		while(*t)
			++t;

		return t - s;
	}

	static int compare(const T *s, int s_length, const T *t, int t_length) {
		int st_len = s_length < t_length ? s_length : t_length;

		if (st_len)
			do {
				const T& x = *s++;
				const T& y = *t++;

				if (x<y)
					return -1;
				if (x>y)
					return +1;

			} while(--st_len);

		return s_length > t_length ? 1 : s_length < t_length ? -1 : 0;
	}

	static bool compare_equal(const T *s, const T *t, int l) {
		return !memcmp(s, t, l*sizeof(T));
	}

	static int find(const T *s, int l, T c) {
		for(int i=0; i<l; ++i)
			if (s[i] == c)
				return i;

		return -1;
	}

	static int find_not_first_of(const T *s, int l, T c) {
		for(int i=0; i<l; ++i)
			if (s[i] != c)
				return i;

		return -1;
	}
};

template<class T> struct VDCharOps : public VDCharOpsImpl<T> {};

template<> struct VDCharOps<char> : public VDCharOpsImpl<char> {
	static int len(const char *s) { return strlen(s); }
	static int compare(const char *s, int s_length, const char *t, int t_length) {
		int st_len = s_length < t_length ? s_length : t_length;
		int r = memcmp(s, t, st_len);

		return r ? r : s_length < t_length ? -1 : s_length > t_length ? +1 : 0;
	}
	static int find(const char *s, int l, char c) {
		const char *t = (const char *)memchr(s, c, l);
		return t ? t-s : -1;
	}
};

template<> struct VDCharOps<wchar_t> : public VDCharOpsImpl<wchar_t> {
	static inline int len(const wchar_t *s) { return wcslen(s); }
};

template<class T>
struct VDBasicStringExtra {};

template<class T>
class VDBasicString : public VDBasicStringExtra<T> {
protected:
	typedef VDCharOps<T> char_ops;
	typedef VDBasicString<T> self_type;

	VDStringInstance<T> *s;

	static VDStringInstance<T> s_null;

	inline T *initalloc(int l) {
		s = (VDStringInstance<T> *)malloc(sizeof(VDStringInstance<T>) + l*sizeof(T));

		s->refcount = 1;
		s->length = l;

		return s->str;
	}

	void fork() {		// must never be called when s->refcount == 1
		VDStringInstance<T> *s_temp = (VDStringInstance<T> *)malloc(sizeof(VDStringInstance<T>) + s->length*sizeof(T));

		VDASSERT(s->refcount > 1);

		s_temp->refcount = 1;
		s_temp->length = s->length;

		char_ops::copy(s_temp->str, s->str, s->length);

		if (VDAtomicInt::staticDecrementTestZero(&s->refcount))
			delete s;

		s = s_temp;
	}

public:
	/////////////////////////////////////////////////
	// declarations

	typedef T				value_type;
	typedef int				size_type;
	typedef T&				reference;
	typedef T*				pointer;
	typedef pointer			iterator;
	typedef const T&		const_reference;
	typedef const T*		const_pointer;
	typedef const_pointer	const_iterator;
	typedef ptrdiff_t		difference_type;

	enum { npos = -1 };		// NOTE: This may conflict with some code that is valid with STL's basic_string<>.

	/////////////////////////////////////////////////
	// ctor/dtor

	inline VDBasicString() : s(&s_null) { VDAtomicInt::staticIncrement(&s_null.refcount); }

	VDBasicString(const self_type& x) {
		s = x.s;
		VDAtomicInt::staticIncrement(&s->refcount);
	}

	explicit VDBasicString(const T *_s) {
		int l = char_ops::len(_s);

		char_ops::copy(initalloc(l), _s, l);
	}

	explicit VDBasicString(const T *_s, size_type l) {
		char_ops::copy(initalloc(l), _s, l);
	}

	explicit VDBasicString(int l) {
		initalloc(l);
	}

	~VDBasicString() {
		if (VDAtomicInt::staticDecrementTestZero(&s->refcount))
			delete s;
	}

	/////////////////////////////////////////////////
	// accessors

	inline T& operator[](int pos) {
		if (s->refcount > 1)
			fork();

		VDASSERT(pos >= 0 && pos < s->length);

		return s->str[pos];
	}

	inline const T& operator[](int pos) const {
		VDASSERT(pos >= 0 && pos < s->length);

		return s->str[pos];
	}

	inline T& at(int pos) {
		if (s->refcount>1)
			fork();

		VDASSERT(pos >= 0 && pos < s->length);

		return s->str[pos];
	}

	inline const T& at(int pos) const {
		VDASSERT(pos >= 0 && pos < s->length);

		return s->str[pos];
	}

	inline iterator begin() const {
		return s->str;
	}

	inline const T *c_str() const {
		if (s->str[s->length])
			s->str[s->length] = 0;
		return s->str;
	}

	inline void copy(T *dst, int count, int off = 0) {
		if (count > s->length - off)
			count = s->length - off;
		char_ops::copy(dst, s->str + off, count);
	}

	inline const T *data() const {
		return s->str;
	}

	inline bool empty() const {
		return s == &s_null;
	}

	inline iterator end() const {
		return s->str + s->length;
	}

	inline int length() const {
		return s->length;
	}

	inline int size() const {
		return s->length;
	}

	/////////////////////////////////////////////////
	// mutators

	T *alloc(int l) {
		if (VDAtomicInt::staticDecrementTestZero(&s->refcount))
			delete s;

		return initalloc(l);
	}

	void clear() {
		if (s != &s_null) {
			if (VDAtomicInt::staticDecrementTestZero(&s->refcount))
				delete s;

			s = &s_null;
			VDAtomicInt::staticIncrement(&s->refcount);
		}
	}

	inline void push_back(T x) {
		operator+=(x);
	}

	void resize(size_type siz) {
		if (siz != length()) {
			self_type tmp(siz);

			char_ops::copy(tmp.s->str, s->str, std::min<size_type>(siz, s->length));
			swap(tmp);
		}
	}

	void swap(self_type& x) {
		VDStringInstance<T> *t = s;
		s=x.s;
		x.s=t;
	}

	/////////////////////////////////////////////////
	// find()/rfind()

	inline int find(const T x, int off = 0) const {
		return char_ops::find(s->str + off, s->length - off, x);
	}

	int find_first_not_of(const T x, int off = 0) const {
		for(; off < s->length; ++off)
			if (s->str[off] != x)
				return off;

		return npos;
	}

	int rfind(const T x, int off = npos) const {
		if (off == npos)
			off = s->length;
	
		while(--off >= 0)
			if (s->str[off] == x)
				return off;

		return npos;
	}

	/////////////////////////////////////////////////
	// compare()

	int compare(const self_type& x) const {
		return char_ops::compare(s->str, s->length, x.s->str, x.s->length);
	}

	int compare(const T *x) const {
		return char_ops::compare(s->str, s->length, x, char_ops::len(x));
	}

	int compare(const T *x, int x_length) const {
		return char_ops::compare(s->str, s->length, x, x_length);
	}

	/////////////////////////////////////////////////
	// operator==()

	inline bool operator==(const self_type& x) const {
		return s->length == x.s->length && char_ops::compare_equal(s->str, x.s->str, s->length);
	}

	inline bool operator==(const T *x) const {
		const int x_length = char_ops::len(x);
		return s->length == x_length && char_ops::compare_equal(s->str, x, x_length);
	}

	/////////////////////////////////////////////////
	// operator!=()

	inline bool operator!=(const self_type& x) const {
		return s->length != x.s->length || !char_ops::compare_equal(s->str, x.s->str, s->length);
	}

	inline bool operator!=(const T *x) const {
		const int x_length = char_ops::len(x);
		return s->length != x_length || !char_ops::compare_equal(s->str, x, x_length);
	}

	/////////////////////////////////////////////////
	// assign()

	const self_type& assign(const T *_s, int len) {
		if (!len)
			clear();
		else
			char_ops::copy(alloc(len), _s, len);
		return *this;
	}

	inline const self_type& assign(const self_type& src) {
		return src.s == this->s ? *this : assign(src.s->str, src.s->length);
	}

	inline const self_type& assign(const T *t) {
		return assign(t, char_ops::len(t));
	}

	/////////////////////////////////////////////////
	// operator=()

	inline const self_type& operator=(const self_type& src) {
		return assign(src.s->str, src.s->length);
	}

	inline const self_type& operator=(const T *t) {
		return assign(t, char_ops::len(t));
	}

	/////////////////////////////////////////////////
	// operator+()

	self_type operator+(T c) const {
		const int s_len = s->length;
		self_type tmp(s_len + 1);

		char_ops::copy(tmp.s->str, s->str, s_len);
		tmp.s->str[s_len] = c;

		return tmp;
	}

	inline self_type operator+(const T *t) const {
		const int t_length = char_ops::len(t);
		self_type tmp(s->length + t_length);

		char_ops::copy(tmp.s->str, s->str, s->length);
		char_ops::copy(tmp.s->str + s->length, t, t_length);

		return tmp;
	}

	inline self_type operator+(const self_type& x) const {
		self_type tmp(s->length + x.s->length);

		char_ops::copy(tmp.s->str, s->str, s->length);
		char_ops::copy(tmp.s->str + s->length, x.s->str, x.s->length);

		return tmp;
	}

	/////////////////////////////////////////////////
	// append()

	inline const self_type& append(const T *t, int t_length) {
		self_type tmp(s->length + t_length);

		char_ops::copy(tmp.s->str, s->str, s->length);
		char_ops::copy(tmp.s->str + s->length, t, t_length);

		return *this = tmp;
	}

	inline const self_type& append(const T *t) {
		return append(t, char_ops::len(t));
	}

	inline const self_type& append(T c) {
		self_type tmp(s->length + 1);

		char_ops::copy(tmp.s->str, s->str, s->length);
		tmp.s->str[s->length] = c;

		return *this = tmp;
	}

	inline const self_type& append(const self_type& x) {
		return append(x.s->str, x.s->length);
	}

	/////////////////////////////////////////////////
	// operator+=()

	inline const self_type& operator+=(const T *t) {
		return append(t, char_ops::len(t));
	}
	inline const self_type& operator+=(T c) {
		return append(c);
	}
	inline const self_type& operator+=(const self_type& x) {
		return append(x);
	}
};

template<class T>
VDStringInstance<T> VDBasicString<T>::s_null={1,0,0};

///////////////////////////////////////////////////////////////////////////

template<class T>
inline bool operator==(const char *x, const VDBasicString<T>& y) throw() { return y == x; }

template<class T>
inline bool operator!=(const char *x, const VDBasicString<T>& y) throw() { return !(y == x); }

///////////////////////////////////////////////////////////////////////////

template<> struct VDBasicStringExtra<char> {
	static inline VDBasicString<char> setf(const char *format, ...) throw() {
		va_list val;
		va_start(val, format);
		VDBasicString<char> tmp(VDFastTextVprintfA(format, val));
		va_end(val);
		VDFastTextFree();

		return tmp;
	}
};

template<> struct VDBasicStringExtra<wchar_t> {
	static inline VDBasicString<wchar_t> setf(const wchar_t *format, ...) throw() {
		va_list val;
		va_start(val, format);
		VDBasicString<wchar_t> tmp(VDFastTextVprintfW(format, val));
		va_end(val);
		VDFastTextFree();

		return tmp;
	}
};

///////////////////////////////////////////////////////////////////////////

typedef VDBasicString<char>		VDStringA;
typedef VDBasicString<wchar_t>	VDStringW;
typedef VDStringA				VDString;

#endif
