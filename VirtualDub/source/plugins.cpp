#include "stdafx.h"
#include <list>
#include <vd2/system/VDString.h>
#include <vd2/system/error.h>
#include <vd2/system/filesys.h>
#include <vd2/system/vdalloc.h>

#include <vd2/plugin/vdplugin.h>
#include <vd2/plugin/vdaudiofilt.h>

#include "plugins.h"
#include "misc.h"



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


extern FilterFunctions g_filterFuncs;


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

VDExternalModule::VDExternalModule(const VDStringW& filename)
	: mFilename(filename)
	, mhModule(NULL)
	, mModuleRefCount(0)
{
	memset(&mModuleInfo, 0, sizeof mModuleInfo);
}

VDExternalModule::~VDExternalModule() {
}

void VDExternalModule::Lock() {
	if (!mhModule) {
		{
			VDExternalCodeBracket bracket(mFilename.c_str(), __FILE__, __LINE__);

			if (GetVersion() & 0x80000000)
				mhModule = LoadLibraryA(VDTextWToA(mFilename).c_str());
			else
				mhModule = LoadLibraryW(mFilename.c_str());
		}

		if (!mhModule)
			throw MyWin32Error("Cannot load plugin module \"%s\": %%s", GetLastError(), VDTextWToA(mFilename).c_str());

		ReconnectOldPlugins();
		ReconnectPlugins();
	}
	++mModuleRefCount;
}

void VDExternalModule::Unlock() {
	VDASSERT(mModuleRefCount > 0);
	VDASSERT(mhModule);

	if (!--mModuleRefCount) {
		DisconnectOldPlugins();
		VDDisconnectPluginDescriptions(this);
		FreeLibrary(mhModule);
		mhModule = 0;
		VDDEBUG("Plugins: Unloading module \"%s\"\n", VDTextWToA(mFilename).c_str());
	}
}

void VDExternalModule::DisconnectOldPlugins() {
	if (mModuleInfo.hInstModule) {
		mModuleInfo.deinitProc(&mModuleInfo, &g_filterFuncs);

		{
			VDExternalCodeBracket bracket(mFilename.c_str(), __FILE__, __LINE__);
			FreeLibrary(mModuleInfo.hInstModule);
		}

		mModuleInfo.hInstModule = NULL;
	}
}

void VDExternalModule::ReconnectOldPlugins() {
	if (!mModuleInfo.hInstModule) {
		VDStringA nameA(VDTextWToA(mFilename));

		mModuleInfo.hInstModule = mhModule;

		try {
			mModuleInfo.initProc   = (FilterModuleInitProc  )GetProcAddress(mModuleInfo.hInstModule, "VirtualdubFilterModuleInit2");
			mModuleInfo.deinitProc = (FilterModuleDeinitProc)GetProcAddress(mModuleInfo.hInstModule, "VirtualdubFilterModuleDeinit");

			if (!mModuleInfo.initProc) {
				void *fp = GetProcAddress(mModuleInfo.hInstModule, "VirtualdubFilterModuleInit");

				if (fp)
					throw MyError(
						"This filter was created for VirtualDub 1.1 or earlier, and is not compatible with version 1.2 or later. "
						"Please contact the author for an updated version.");

				fp = GetProcAddress(mModuleInfo.hInstModule, "NinaFilterModuleInit");

				if (fp)
					throw MyError("This filter will only work with VirtualDub 2.0 or above.");
			}

			if (!mModuleInfo.initProc || !mModuleInfo.deinitProc)
				throw MyError("Module \"%s\" does not contain VirtualDub filters.", nameA.c_str());

			int ver_hi = VIRTUALDUB_FILTERDEF_VERSION;
			int ver_lo = VIRTUALDUB_FILTERDEF_COMPATIBLE;

			if (mModuleInfo.initProc(&mModuleInfo, &g_filterFuncs, ver_hi, ver_lo))
				throw MyError("Error initializing module \"%s\".",nameA.c_str());

			if (ver_hi < VIRTUALDUB_FILTERDEF_COMPATIBLE) {
				mModuleInfo.deinitProc(&mModuleInfo, &g_filterFuncs);

				throw MyError(
					"This filter was created for an earlier, incompatible filter interface. As a result, it will not "
					"run correctly with this version of VirtualDub. Please contact the author for an updated version.");
			}

			if (ver_lo > VIRTUALDUB_FILTERDEF_VERSION) {
				mModuleInfo.deinitProc(&mModuleInfo, &g_filterFuncs);

				throw MyError(
					"This filter uses too new of a filter interface!  You'll need to upgrade to a newer version of "
					"VirtualDub to use this filter."
					);
			}
		} catch(...) {
			FreeLibrary(mModuleInfo.hInstModule);
			mModuleInfo.hInstModule = NULL;
			throw;
		}
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

	g_pluginModules.push_back(new VDExternalModule(path));
	VDExternalModule *pModule = g_pluginModules.back();

	// lock the module to bring in the plugin descriptions -- this may bomb
	// if the plugin doesn't exist or couldn't load

	try {
		pModule->Lock();
		pModule->Unlock();
	} catch(...) {
		g_pluginModules.pop_back();
		throw;
	}

}

void VDAddInternalPlugins(const VDPluginInfo *const *ppInfo) {
	VDConnectPluginDescriptions(ppInfo, NULL);
}

VDExternalModule *VDGetExternalModuleByFilterModule(const FilterModule *fm) {
	std::list<class VDExternalModule *>::const_iterator it(g_pluginModules.begin()),
			itEnd(g_pluginModules.end());

	for(; it!=itEnd; ++it) {
		VDExternalModule *pModule = *it;

		if (fm == &pModule->GetFilterModuleInfo())
			return pModule;
	}

	return NULL;
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

void VDLoadPlugins(const VDStringW& path, int& succeeded, int& failed) {
	VDDirectoryIterator it(VDMakePath(path, VDStringW(L"*.vdf")).c_str());

	succeeded = failed = 0;

	while(it.Next()) {
		VDDEBUG("Plugins: Attempting to load \"%ls\"\n", it.GetFullPath().c_str());
		VDStringW path(it.GetFullPath());
		try {
			VDAddPluginModule(path.c_str());
			++succeeded;
		} catch(const MyError& e) {
			VDDEBUG("Plugins: Failed to load \"%ls\": %s\n", it.GetFullPath().c_str(), e.gets());
			++failed;
		}
	}
}
