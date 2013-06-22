#include <windows.h>

#include <vd2/system/VDString.h>
#include <vd2/system/registry.h>

VDRegistryKey::VDRegistryKey(const char *pszKey) {
	if (RegCreateKeyEx(HKEY_CURRENT_USER, pszKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, (PHKEY)&pHandle, NULL))
		pHandle = NULL;
}

VDRegistryKey::~VDRegistryKey() {
	if (pHandle)
		RegCloseKey((HKEY)pHandle);
}

bool VDRegistryKey::setBool(const char *pszName, bool v) const {
	if (pHandle) {
		DWORD dw = v;

		if (RegSetValueEx((HKEY)pHandle, pszName, 0, REG_DWORD, (const BYTE *)&dw, sizeof dw))
			return true;
	}

	return false;
}

bool VDRegistryKey::setInt(const char *pszName, int i) const {
	if (pHandle) {
		DWORD dw = i;

		if (RegSetValueEx((HKEY)pHandle, pszName, 0, REG_DWORD, (const BYTE *)&dw, sizeof dw))
			return true;
	}

	return false;
}

bool VDRegistryKey::setString(const char *pszName, const char *pszString) const {
	if (pHandle) {
		if (RegSetValueEx((HKEY)pHandle, pszName, 0, REG_SZ, (const BYTE *)pszString, strlen(pszString)))
			return true;
	}

	return false;
}

bool VDRegistryKey::setBinary(const char *pszName, const char *data, int len) const {
	if (pHandle) {
		if (RegSetValueEx((HKEY)pHandle, pszName, 0, REG_BINARY, (const BYTE *)data, len))
			return true;
	}

	return false;
}

bool VDRegistryKey::getBool(const char *pszName, bool def) const {
	DWORD type, v, s=sizeof(DWORD);

	if (!pHandle || RegQueryValueEx((HKEY)pHandle, pszName, 0, &type, (BYTE *)&v, &s)
		|| type != REG_DWORD)
		return def;

	return v != 0;
}

int VDRegistryKey::getInt(const char *pszName, int def) const {
	DWORD type, v, s=sizeof(DWORD);

	if (!pHandle || RegQueryValueEx((HKEY)pHandle, pszName, 0, &type, (BYTE *)&v, &s)
		|| type != REG_DWORD)
		return def;

	return (int)v;
}

bool VDRegistryKey::getString(const char *pszName, VDString& name) const {
	DWORD type, s = sizeof(DWORD);

	if (!pHandle || RegQueryValueEx((HKEY)pHandle, pszName, 0, &type, NULL, &s) || type != REG_SZ)
		return false;

	if (RegQueryValueEx((HKEY)pHandle, pszName, 0, NULL, (BYTE *)name.alloc(s), &s))
		return false;

	return true;
}

int VDRegistryKey::getBinaryLength(const char *pszName) const {
	DWORD type, v, s = sizeof(DWORD);

	if (!pHandle || RegQueryValueEx((HKEY)pHandle, pszName, 0, &type, (BYTE *)&v, &s)
		|| type != REG_BINARY)
		return -1;

	return s;
}

bool VDRegistryKey::getBinary(const char *pszName, char *buf, int maxlen) const {
	DWORD type, s = maxlen;

	if (!pHandle || RegQueryValueEx((HKEY)pHandle, pszName, 0, &type, (BYTE *)buf, &s) || maxlen < s || type != REG_BINARY)
		return false;

	return true;
}

//////////////////////////////////

VDString VDRegistryAppKey::s_appbase;

VDRegistryAppKey::VDRegistryAppKey() : VDRegistryKey(s_appbase.c_str()) {
}

VDRegistryAppKey::VDRegistryAppKey(const char *pszKey) : VDRegistryKey((s_appbase + pszKey).c_str()) {
}

void VDRegistryAppKey::setDefaultKey(const char *pszAppName) {
	s_appbase = pszAppName;
}
