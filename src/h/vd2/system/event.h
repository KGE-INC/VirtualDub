//	VirtualDub - Video processing and capture application
//	System library component
//	Copyright (C) 1998-2006 Avery Lee, All Rights Reserved.
//
//	Beginning with 1.6.0, the VirtualDub system library is licensed
//	differently than the remainder of VirtualDub.  This particular file is
//	thus licensed as follows (the "zlib" license):
//
//	This software is provided 'as-is', without any express or implied
//	warranty.  In no event will the authors be held liable for any
//	damages arising from the use of this software.
//
//	Permission is granted to anyone to use this software for any purpose,
//	including commercial applications, and to alter it and redistribute it
//	freely, subject to the following restrictions:
//
//	1.	The origin of this software must not be misrepresented; you must
//		not claim that you wrote the original software. If you use this
//		software in a product, an acknowledgment in the product
//		documentation would be appreciated but is not required.
//	2.	Altered source versions must be plainly marked as such, and must
//		not be misrepresented as being the original software.
//	3.	This notice may not be removed or altered from any source
//		distribution.

#ifndef f_VD2_SYSTEM_EVENT_H
#define f_VD2_SYSTEM_EVENT_H

struct VDDelegateNode {
	VDDelegateNode *mpNext, *mpPrev;
};

class VDDelegate;

class VDEventBase {
protected:
	VDEventBase();
	~VDEventBase();

	void Add(VDDelegate&);
	void Remove(VDDelegate&);
	void Raise(void *src, const void *info);

	VDDelegateNode mAnchor;
};

template<class Source, class ArgType>
class VDDelegateBinding {
public:
	VDDelegate *mpBoundDelegate;
};

class VDDelegate : public VDDelegateNode {
	friend class VDEventBase;
public:
	VDDelegate();
	~VDDelegate();

	template<class T, class Source, class ArgType>
	VDDelegateBinding<Source, ArgType> operator()(T *obj, void (T::*fn)(Source *, const ArgType&)) {
		mpCallback = Adapter<T, Source, ArgType, void (T::*)(Source *, const ArgType&)>;
		mpObj = obj;
		mpFn = reinterpret_cast<void(Holder::*)()>(fn);
		VDDelegateBinding<Source, ArgType> binding = {this};
		return binding;
	}

protected:
	template<class T, class Source, class ArgType, typename T_Fn>
	static void Adapter(void *src, const void *info, VDDelegate& del) {
		return (((T *)del.mpObj)->*reinterpret_cast<T_Fn>(del.mpFn))(static_cast<Source *>(src), *static_cast<const ArgType *>(info));
	}

	void (*mpCallback)(void *src, const void *info, VDDelegate&);
	void *mpObj;

#ifdef _MSC_VER
	class __multiple_inheritance Holder;
#else
	class Holder;
#endif

	void (Holder::*mpFn)();
};

template<class Source, class ArgType>
class VDEvent : public VDEventBase {
public:
	void operator+=(const VDDelegateBinding<Source, ArgType>& binding) {
		Add(*binding.mpBoundDelegate);
	}

	void operator-=(const VDDelegateBinding<Source, ArgType>& binding) {
		Remove(*binding.mpBoundDelegate);
	}

	void Raise(Source *src, const ArgType& args) {
		VDEventBase::Raise(src, &args);
	}
};

#endif
