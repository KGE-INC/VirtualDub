#ifndef f_VD2_TESSA_D3D10_FENCEMANAGER_D3D10_H
#define f_VD2_TESSA_D3D10_FENCEMANAGER_D3D10_H

#include <vd2/system/vdstl.h>

struct ID3D10Device;
struct ID3D10Query;

class VDTFenceManagerD3D10 {
	VDTFenceManagerD3D10(const VDTFenceManagerD3D10&);
	VDTFenceManagerD3D10& operator=(const VDTFenceManagerD3D10&);
public:
	VDTFenceManagerD3D10();
	~VDTFenceManagerD3D10();

	void Init(ID3D10Device *dev);
	void Shutdown();

	void FlushDefaultResources();

	uint32 InsertFence();
	bool CheckFence(uint32 fence);

protected:
	typedef vdfastvector<ID3D10Query *> IdleQueries;
	typedef vdfastdeque<ID3D10Query *> ActiveQueries;

	ID3D10Device *mpD3DDevice;
	uint32 mFirstFenceId;
	uint32 mNextFenceId;
	ActiveQueries mActiveQueries;
	IdleQueries mIdleQueries;
};

#endif
