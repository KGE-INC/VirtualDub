#include "stdafx.h"
#include <list>
#include <vd2/system/VDString.h>
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/vdalloc.h>

#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdaudiofilt.h>

#include "plugins.h"




namespace {
	static void *FilterGetExtendedAPI(const char *) {
		return NULL;
	}

	static void FilterThrowExcept(const char *format, ...) {
		va_list val;
		MyError e;

		va_start(val, format);
		e.vsetf(format, val);
		va_end(val);

		throw e;
	}

	static void FilterThrowExceptMemory() {
		throw MyMemoryError();
	}
}

const VDPluginCallbacks g_pluginCallbacks = {
	FilterGetExtendedAPI,
	FilterThrowExceptMemory,
	FilterThrowExcept
};



namespace {
	struct VDShadowedPluginDescription : public VDPluginDescription {
	public:
		VDPluginInfo			mShadowedInfo;
		std::vector<char>		mpShadowedTypeInfo;
	};
}



std::list<VDShadowedPluginDescription> g_plugins;

VDPluginDescription *VDGetPluginDescription(const wchar_t *pName, uint32 type) {
	for(std::list<VDShadowedPluginDescription>::iterator it(g_plugins.begin()), itEnd(g_plugins.end());
			it != itEnd; ++it)
	{
		VDPluginDescription& desc = *it;

		if (desc.mName == pName && desc.mType == type)
			return &desc;
	}

	return NULL;
}

void VDConnectPluginDescription(const VDPluginInfo *pInfo, VDExternalModule *pModule) {
	VDShadowedPluginDescription *pDesc = static_cast<VDShadowedPluginDescription *>(VDGetPluginDescription(pInfo->mpName, pInfo->mType));

	if (!pDesc) {
		g_plugins.push_back(VDShadowedPluginDescription());
		pDesc = &g_plugins.back();

		pDesc->mName		= pInfo->mpName;
		pDesc->mAuthor		= pInfo->mpAuthor ? pInfo->mpAuthor : L"(internal)";
		pDesc->mDescription	= pInfo->mpDescription;
		pDesc->mVersion		= pInfo->mVersion;
		pDesc->mType		= pInfo->mType;
		pDesc->mpModule		= pModule;
	}

	pDesc->mShadowedInfo = *pInfo;

	size_t typelen = *(uint32 *)pInfo->mpTypeSpecificInfo;
	pDesc->mpShadowedTypeInfo.resize(typelen);
	memcpy(&pDesc->mpShadowedTypeInfo[0], pInfo->mpTypeSpecificInfo, typelen);

	pDesc->mShadowedInfo.mpTypeSpecificInfo = &pDesc->mpShadowedTypeInfo[0];

	// sanitize type info
	switch(pInfo->mType) {
	case kVDPluginType_Audio:
		{
			VDAudioFilterDefinition& desc = *(VDAudioFilterDefinition *)pDesc->mShadowedInfo.mpTypeSpecificInfo;

			desc.mpConfigInfo	= NULL;
		}
		break;
	default:
		VDASSERT(false);
	}

	pDesc->mpInfo = pInfo;
}

void VDConnectPluginDescriptions(const VDPluginInfo *const *ppInfos, VDExternalModule *pModule) {
	while(const VDPluginInfo *pInfo = *ppInfos++)
		VDConnectPluginDescription(pInfo, pModule);
}

void VDDisconnectPluginDescriptions(VDExternalModule *pModule) {
	for(std::list<VDShadowedPluginDescription>::iterator it(g_plugins.begin()), itEnd(g_plugins.end());
			it != itEnd; ++it)
	{
		VDShadowedPluginDescription& desc = *it;

		if (desc.mpModule == pModule)
			desc.mpInfo = &desc.mShadowedInfo;
	}	
}

void VDEnumeratePluginDescriptions(std::vector<VDPluginDescription *>& plugins, uint32 type) {
	for(std::list<VDShadowedPluginDescription>::iterator it(g_plugins.begin()), itEnd(g_plugins.end());
			it != itEnd; ++it)
	{
		VDPluginDescription& desc = *it;

		if (desc.mType == type)
			plugins.push_back(&desc);
	}

}

std::list<class VDExternalModule *>		g_pluginModules;

class VDExternalModule {
public:
	VDExternalModule(const VDStringW& filename);
	~VDExternalModule();

	void Lock();
	void Unlock();

	const VDStringW& GetFilename() const { return mFilename; }

protected:
	void ReconnectPlugins();

	VDStringW	mFilename;
	HMODULE		mhModule;
	int			mModuleRefCount;
};

VDExternalModule::VDExternalModule(const VDStringW& filename)
	: mFilename(filename)
	, mhModule(NULL)
	, mModuleRefCount(0)
{
}

VDExternalModule::~VDExternalModule() {
}

void VDExternalModule::Lock() {
	if (!mhModule) {
		if (GetVersion() & 0x80000000)
			mhModule = LoadLibraryA(VDTextWToA(mFilename).c_str());
		else
			mhModule = LoadLibraryW(mFilename.c_str());

		if (!mhModule)
			throw MyWin32Error("Cannot load plugin module \"%s\": %%s", GetLastError(), VDTextWToA(mFilename).c_str());

		ReconnectPlugins();
	}

	++mModuleRefCount;
}

void VDExternalModule::Unlock() {
	VDASSERT(mModuleRefCount > 0);
	VDASSERT(mhModule);

	if (!--mModuleRefCount) {
		VDDisconnectPluginDescriptions(this);
		FreeLibrary(mhModule);
		mhModule = 0;
	}
}

void VDExternalModule::ReconnectPlugins() {
	tpVDGetPluginInfo pVDGetPluginInfo = (tpVDGetPluginInfo)GetProcAddress(mhModule, "VDGetPluginInfo");

	if (pVDGetPluginInfo) {
		const VDPluginInfo **ppPluginInfo = pVDGetPluginInfo();

		VDConnectPluginDescriptions(ppPluginInfo, this);
	}
}

///////////////////////////////////////////////////////////////////////////

void VDDeinitPluginSystem() {
	g_plugins.clear();

	while(!g_pluginModules.empty()) {
		VDExternalModule *pModule = g_pluginModules.back();
		delete pModule;					// must be before pop_back() (seems STL uses aliasing after all)
		g_pluginModules.pop_back();
	}
}

void VDAddPluginModule(const wchar_t *pFilename) {
	VDStringW path(VDGetFullPath(VDStringW(pFilename)));

	if (path.empty())
		path = pFilename;

	std::list<class VDExternalModule *>::const_iterator it(g_pluginModules.begin()),
			itEnd(g_pluginModules.end());

	for(; it!=itEnd; ++it) {
		VDExternalModule *pModule = *it;

		if (pModule->GetFilename() == pFilename)
			return;
	}

	vdautoptr<VDExternalModule> pModule(new VDExternalModule(path));

	// lock the module to bring in the plugin descriptions -- this may bomb
	// if the plugin doesn't exist or couldn't load (thus the funny autoptr
	// usage above).
	pModule->Lock();
	pModule->Unlock();

	g_pluginModules.push_back(pModule.release());
}

void VDAddInternalPlugins(const VDPluginInfo *const *ppInfo) {
	VDConnectPluginDescriptions(ppInfo, NULL);
}

const VDPluginInfo *VDLockPlugin(VDPluginDescription *pDesc) {
	if (pDesc->mpModule)
		pDesc->mpModule->Lock();

	return pDesc->mpInfo;
}

void VDUnlockPlugin(VDPluginDescription *pDesc) {
	if (pDesc->mpModule)
		pDesc->mpModule->Unlock();
}

void VDLoadPlugins(const VDStringW& path) {
	VDDirectoryIterator it(VDMakePath(path, VDStringW(L"*.vdf")).c_str());

	while(it.Next()) {
		VDDEBUG("Plugins: Attempting to load \"%ls\"\n", it.GetFullPath().c_str());
		VDStringW path(it.GetFullPath());
		try {
			VDAddPluginModule(path.c_str());
		} catch(const MyError& e) {
			VDDEBUG("Plugins: Failed to load \"%ls\": %s\n", it.GetFullPath().c_str(), e.gets());
		}
	}
}
