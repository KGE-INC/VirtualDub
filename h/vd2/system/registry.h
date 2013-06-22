#ifndef f_VD2_SYSTEM_REGISTRY_H
#define f_VD2_SYSTEM_REGISTRY_H

#include <vd2/system/VDString.h>

class VDRegistryKey {
private:
	void *pHandle;

public:
	VDRegistryKey(const char *pszKey);
	~VDRegistryKey();

	bool isReady() const { return pHandle != 0; }

	bool setBool(const char *pszName, bool) const;
	bool setInt(const char *pszName, int) const;
	bool setString(const char *pszName, const char *pszString) const;
	bool setString(const char *pszName, const wchar_t *pszString) const;
	bool setBinary(const char *pszName, const char *data, int len) const;

	bool getBool(const char *pszName, bool def=false) const;
	int getInt(const char *pszName, int def=0) const;
	bool getString(const char *pszName, VDStringA& s) const;
	bool getString(const char *pszName, VDStringW& s) const;

	int getBinaryLength(const char *pszName) const;
	bool getBinary(const char *pszName, char *buf, int maxlen) const;

};

class VDRegistryAppKey : public VDRegistryKey {
private:
	static VDString s_appbase;

public:
	VDRegistryAppKey();
	VDRegistryAppKey(const char *pszKey);

	static void setDefaultKey(const char *pszAppName);
};

#endif
