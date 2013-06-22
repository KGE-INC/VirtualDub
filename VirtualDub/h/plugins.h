#ifndef f_PLUGINS_H
#define f_PLUGINS_H

#include <vd2/system/vdtypes.h>
#include <vd2/system/VDString.h>
#include <vd2/plugin/vdvideofiltold.h>
#include <vector>

class VDExternalModule;
struct VDPluginInfo;

struct VDPluginDescription {
	VDStringW			mName;
	VDStringW			mAuthor;
	VDStringW			mDescription;
	uint32				mVersion;
	uint32				mType;
	VDExternalModule	*mpModule;
	const VDPluginInfo	*mpInfo;
};

class VDExternalModule {
public:
	VDExternalModule(const VDStringW& filename);
	~VDExternalModule();

	void Lock();
	void Unlock();

	const VDStringW& GetFilename() const { return mFilename; }
	FilterModule& GetFilterModuleInfo() { return mModuleInfo; }

protected:
	void DisconnectOldPlugins();
	void ReconnectOldPlugins();
	void ReconnectPlugins();

	VDStringW		mFilename;
	HMODULE			mhModule;
	int				mModuleRefCount;
	FilterModule	mModuleInfo;
};


extern const struct VDPluginCallbacks g_pluginCallbacks;

void					VDDeinitPluginSystem();

void					VDAddPluginModule(const wchar_t *pFilename);
void					VDAddInternalPlugins(const VDPluginInfo *const *ppInfo);

VDExternalModule *		VDGetExternalModuleByFilterModule(const FilterModule *);

VDPluginDescription *	VDGetPluginDescription(const wchar_t *pName, uint32 mType);
void					VDEnumeratePluginDescriptions(std::vector<VDPluginDescription *>& plugins, uint32 type);

void					VDLoadPlugins(const VDStringW& path, int& succeeded, int& failed);
const VDPluginInfo *	VDLockPlugin(VDPluginDescription *pDesc);
void					VDUnlockPlugin(VDPluginDescription *pDesc);


template<class T>
class VDAutolockPlugin {
public:
	VDAutolockPlugin(VDPluginDescription *pDesc) : mpDesc(pDesc), mpPluginInfo(VDLockPlugin(mpDesc)) {
	}

	~VDAutolockPlugin() {
		VDUnlockPlugin(mpDesc);
	}

	const T *operator->() const { return reinterpret_cast<const T *>(mpPluginInfo->mpTypeSpecificInfo); }
	const T& operator*() const { return *reinterpret_cast<const T *>(mpPluginInfo->mpTypeSpecificInfo); }

protected:
	VDPluginDescription *mpDesc;
	const VDPluginInfo *mpPluginInfo;
};


#endif
