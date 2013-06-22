#ifndef f_VD2_PLUGIN_VDPLUGIN_H
#define f_VD2_PLUGIN_VDPLUGIN_H

#include <stddef.h>

// Copied from <vd2/system/vdtypes.h>.  Must be in sync.
#ifndef VD_STANDARD_TYPES_DECLARED
	#if defined(_MSC_VER)
		typedef signed __int64		sint64;
		typedef unsigned __int64	uint64;
	#elif defined(__GNUC__)
		typedef signed long long	sint64;
		typedef unsigned long long	uint64;
	#endif
	typedef signed int			sint32;
	typedef unsigned int		uint32;
	typedef signed short		sint16;
	typedef unsigned short		uint16;
	typedef signed char			sint8;
	typedef unsigned char		uint8;

	typedef sint64				int64;
	typedef sint32				int32;
	typedef sint16				int16;
	typedef sint8				int8;
#endif

#ifndef VDAPIENTRY
	#define VDAPIENTRY __cdecl
#endif

#ifndef VDAPIENTRYV
	#define VDAPIENTRYV __cdecl
#endif

enum {
	kVDPlugin_APIVersion		= 10
};


enum {
	kVDPluginType_Video,		// Updated video filter API is not yet complete.
	kVDPluginType_Audio,
};

struct VDPluginInfo {
	uint32			mSize;				// size of this structure in bytes
	const wchar_t	*mpName;
	const wchar_t	*mpAuthor;
	const wchar_t	*mpDescription;
	uint32			mVersion;			// (major<<24) + (minor<<16) + build.  1.4.1000 would be 0x010403E8.
	uint32			mType;
	uint32			mFlags;
	uint32			mAPIVersionRequired;
	uint32			mAPIVersionUsed;
	uint32			mTypeAPIVersionRequired;
	uint32			mTypeAPIVersionUsed;
	const void *	mpTypeSpecificInfo;
};

typedef const VDPluginInfo **(*tpVDGetPluginInfo)();

class IVDPluginCallbacks {
public:
	virtual void * VDAPIENTRY GetExtendedAPI(const char *pExtendedAPIName) = 0;
	virtual void VDAPIENTRYV SetError(const char *format, ...) = 0;
	virtual void VDAPIENTRY SetErrorOutOfMemory() = 0;
};

struct VDPluginConfigEntry {
	enum Type {
		kTypeInvalid	= 0,
		kTypeU32		= 1,
		kTypeS32,
		kTypeU64,
		kTypeS64,
		kTypeDouble,
		kTypeAStr,
		kTypeWStr,
		kTypeBlock
	};

	const VDPluginConfigEntry *next;

	unsigned	idx;
	uint32		type;
	const wchar_t *name;
	const wchar_t *label;
	const wchar_t *desc;	
};


#endif
