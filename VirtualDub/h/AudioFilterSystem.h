#ifndef f_AUDIOFILTERSYSTEM_H
#define f_AUDIOFILTERSYSTEM_H

#include <list>
#include <vector>
#include <map>
#include <vd2/system/VDScheduler.h>
#include <vd2/system/VDString.h>
#include "filter.h"

struct VDAudioFilterDefinition;
class VDAudioFilterInstance;

union VDFilterConfigVariantData {
	uint32	vu32;
	sint32	vs32;
	uint64	vu64;
	sint64	vs64;
	double	vfd;
	struct NarrowString {
		char *s;
	} vsa;
	struct WideString {
		wchar_t *s;
	} vsw;
	struct Block {
		uint32 len;
		char *s;
	} vb;
};

class VDFilterConfigVariant {
public:
	enum {
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

	VDFilterConfigVariant() : mType(kTypeInvalid) {}
	VDFilterConfigVariant(const VDFilterConfigVariant&);
	~VDFilterConfigVariant();

	VDFilterConfigVariant& operator=(const VDFilterConfigVariant&);

	unsigned GetType() const { return mType; }

	void Clear();

	void SetU32(uint32 v) { Clear(); mType = kTypeU32; mData.vu32 = v; }
	void SetS32(sint32 v) { Clear(); mType = kTypeS32; mData.vs32 = v; }
	void SetU64(uint64 v) { Clear(); mType = kTypeU64; mData.vu64 = v; }
	void SetS64(sint64 v) { Clear(); mType = kTypeS64; mData.vs64 = v; }
	void SetDouble(double v) { Clear(); mType = kTypeDouble; mData.vfd = v; }
	void SetAStr(const char *s);
	void SetWStr(const wchar_t *s);
	void SetBlock(const void *s, unsigned b);

	const uint32& GetU32() const { return mData.vu32; }
	const sint32& GetS32() const { return mData.vs32; }
	const uint64& GetU64() const { return mData.vu64; }
	const sint64& GetS64() const { return mData.vs64; }
	const double& GetDouble() const { return mData.vfd; }
	const char *GetAStr() const { return mData.vsa.s; }
	const wchar_t *GetWStr() const { return mData.vsw.s; }
	const void *GetBlockPtr() const { return mData.vb.s; }
	const unsigned GetBlockLen() const { return mData.vb.len; }

protected:
	unsigned mType;

	VDFilterConfigVariantData	mData;
};

typedef std::map<unsigned, VDFilterConfigVariant> VDFilterConfig;

class IVDAudioFilterInstance {
public:
	virtual const VDAudioFilterDefinition *GetDefinition() = 0;
	virtual bool Configure(VDGUIHandle hParent) = 0;
	virtual void DeserializeConfig(const VDFilterConfig&) = 0;
	virtual void SerializeConfig(VDFilterConfig&) = 0;
	virtual void *GetObject() = 0;
};

struct VDAudioFilterGraph {
	struct FilterEntry {
		VDStringW	mFilterName;
		int			mInputPins;
		int			mOutputPins;
		VDFilterConfig	mConfig;
	};

	struct FilterConnection {
		int		filt;
		int		pin;
	};

	typedef std::list<FilterEntry> FilterList;
	typedef std::vector<FilterConnection> FilterConnectionList;

	FilterList				mFilters;
	FilterConnectionList	mConnections;
};

class VDAudioFilterSystem {
public:
	VDAudioFilterSystem();
	~VDAudioFilterSystem();

	IVDAudioFilterInstance *Create(const VDAudioFilterDefinition *);
	void Destroy(IVDAudioFilterInstance *);

	void Connect(IVDAudioFilterInstance *pFilterIn, unsigned nPinIn, IVDAudioFilterInstance *, unsigned nPinOut);

	void LoadFromGraph(const VDAudioFilterGraph& graph, std::vector<IVDAudioFilterInstance *>& filterPtrs);

	void Clear();
	void Prepare();
	void Start();
	bool Run();
	void Stop();

	void Seek(sint64 us);

protected:
	VDScheduler		mFilterScheduler;

	typedef std::list<VDAudioFilterInstance *> tFilterList;
	tFilterList		mFilters;
	tFilterList		mStartedFilters;

	void SortFilter(tFilterList& newList, VDAudioFilterInstance *pFilt);
	void Sort();
};

void VDAddAudioFilter(const VDAudioFilterDefinition *);

struct VDAudioFilterBlurb {
	const VDAudioFilterDefinition	*pDef;
	VDStringW					name;
	VDStringW					author;
	VDStringW					description;
};

void VDEnumerateAudioFilters(std::list<VDAudioFilterBlurb>& blurbs);
const VDAudioFilterDefinition *VDLookupAudioFilterByName(const wchar_t *name);
bool VDLockAudioFilter(const VDAudioFilterDefinition *);		// Lock audio filter module in memory so vtbl is valid.
void VDUnlockAudioFilter(const VDAudioFilterDefinition *);		// Release audio filter module.

#endif
