#include "stdafx.h"
#include <d3d9.h>
#include <vd2/Tessa/Context.h>
#include "D3D9/Context_D3D9.h"
#include "D3D9/FenceManager_D3D9.h"

///////////////////////////////////////////////////////////////////////////////

VDTResourceD3D9::VDTResourceD3D9() {
	mListNodePrev = NULL;
	mpParent = NULL;
}

VDTResourceD3D9::~VDTResourceD3D9() {
}

void VDTResourceD3D9::Shutdown() {
	if (mListNodePrev)
		mpParent->RemoveResource(this);
}

void VDTResourceD3D9::ShutdownDefaultPool() {
}

void VDTResourceManagerD3D9::AddResource(VDTResourceD3D9 *res) {
	VDASSERT(!res->mListNodePrev);

	mResources.push_back(res);
	res->mpParent = this;
}

void VDTResourceManagerD3D9::RemoveResource(VDTResourceD3D9 *res) {
	VDASSERT(res->mListNodePrev);

	mResources.erase(res);
	res->mListNodePrev = NULL;
}

void VDTResourceManagerD3D9::ShutdownDefaultPoolResources() {
	Resources::iterator it(mResources.begin()), itEnd(mResources.end());
	for(; it != itEnd; ++it) {
		VDTResourceD3D9 *res = *it;

		res->ShutdownDefaultPool();
	}
}

void VDTResourceManagerD3D9::ShutdownAllResources() {
	while(!mResources.empty()) {
		VDTResourceD3D9 *res = mResources.back();
		mResources.pop_back();

		res->mListNodePrev = NULL;
		res->Shutdown();
	}
}

///////////////////////////////////////////////////////////////////////////////

VDTReadbackBufferD3D9::VDTReadbackBufferD3D9()
	: mpSurface(NULL)
{
}

VDTReadbackBufferD3D9::~VDTReadbackBufferD3D9() {
	Shutdown();
}

bool VDTReadbackBufferD3D9::Init(VDTContextD3D9 *parent, uint32 width, uint32 height, uint32 format) {
	IDirect3DDevice9 *dev = parent->GetDeviceD3D9();
	HRESULT hr = dev->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &mpSurface, NULL);

	parent->AddResource(this);
	return SUCCEEDED(hr);
}

void VDTReadbackBufferD3D9::Shutdown() {
	if (mpSurface) {
		mpSurface->Release();
		mpSurface = NULL;
	}

	VDTResourceD3D9::Shutdown();
}

bool VDTReadbackBufferD3D9::Lock(VDTLockData2D& lockData) {
	D3DLOCKED_RECT lr;
	HRESULT hr = mpSurface->LockRect(&lr, NULL, D3DLOCK_READONLY | D3DLOCK_NOSYSLOCK);

	if (FAILED(hr)) {
		lockData.mpData = NULL;
		lockData.mPitch = 0;
		return false;
	}

	lockData.mpData = lr.pBits;
	lockData.mPitch = lr.Pitch;
	return true;
}

void VDTReadbackBufferD3D9::Unlock() {
	HRESULT hr = mpSurface->UnlockRect();
	VDASSERT(SUCCEEDED(hr));
}

bool VDTReadbackBufferD3D9::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTSurfaceD3D9::VDTSurfaceD3D9()
	: mpSurface(NULL)
{
}

VDTSurfaceD3D9::~VDTSurfaceD3D9() {
	Shutdown();
}

bool VDTSurfaceD3D9::Init(VDTContextD3D9 *parent, uint32 width, uint32 height, uint32 format, VDTUsage usage) {
	IDirect3DDevice9 *dev = parent->GetDeviceD3D9();
	HRESULT hr;
	
	mDesc.mWidth = width;
	mDesc.mHeight = height;
	mDesc.mFormat = format;

	mbDefaultPool = false;
	switch(usage) {
		case kVDTUsage_Default:
			hr = dev->CreateOffscreenPlainSurface(width, height, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &mpSurface, NULL);
			break;

		case kVDTUsage_Render:
			hr = dev->CreateRenderTarget(width, height, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &mpSurface, NULL);
			mbDefaultPool = true;
			break;
	}

	parent->AddResource(this);

	return SUCCEEDED(hr);
}

bool VDTSurfaceD3D9::Init(VDTContextD3D9 *parent, IDirect3DSurface9 *surf) {
	D3DSURFACE_DESC desc = {};

	HRESULT hr = surf->GetDesc(&desc);

	if (FAILED(hr)) {
		parent->ProcessHRESULT(hr);
		return false;
	}

	mDesc.mWidth = desc.Width;
	mDesc.mHeight = desc.Height;
	mDesc.mFormat = 0;
	mbDefaultPool = (desc.Pool == D3DPOOL_DEFAULT);

	parent->AddResource(this);

	mpSurface = surf;
	mpSurface->AddRef();
	return true;
}

void VDTSurfaceD3D9::Shutdown() {
	if (mpSurface) {
		mpSurface->Release();
		mpSurface = NULL;
	}

	VDTResourceD3D9::Shutdown();
}

bool VDTSurfaceD3D9::Restore() {
	return true;
}

bool VDTSurfaceD3D9::Readback(IVDTReadbackBuffer *target) {
	VDTContextD3D9 *parent = static_cast<VDTContextD3D9 *>(mpParent);
	IDirect3DDevice9 *dev = parent->GetDeviceD3D9();
	VDTReadbackBufferD3D9 *targetd3d9 = static_cast<VDTReadbackBufferD3D9 *>(target);

	HRESULT hr = dev->GetRenderTargetData(mpSurface, targetd3d9->mpSurface);
	if (FAILED(hr)) {
		parent->ProcessHRESULT(hr);
		return false;
	}

	return true;
}

void VDTSurfaceD3D9::Load(uint32 dx, uint32 dy, const VDTInitData2D& srcData, uint32 w, uint32 h) {
	D3DLOCKED_RECT lr;
	RECT r = { dx, dy, w, h };
	HRESULT hr = mpSurface->LockRect(&lr, &r, D3DLOCK_NOSYSLOCK);
	if (FAILED(hr)) {
		VDTContextD3D9 *parent = static_cast<VDTContextD3D9 *>(mpParent);
		parent->ProcessHRESULT(hr);
		return;
	}

	VDMemcpyRect(lr.pBits, lr.Pitch, srcData.mpData, srcData.mPitch, 4*w, h);

	hr = mpSurface->UnlockRect();
	if (FAILED(hr)) {
		VDTContextD3D9 *parent = static_cast<VDTContextD3D9 *>(mpParent);
		parent->ProcessHRESULT(hr);
		return;
	}
}

void VDTSurfaceD3D9::Copy(uint32 dx, uint32 dy, IVDTSurface *src0, uint32 sx, uint32 sy, uint32 w, uint32 h) {
	VDTContextD3D9 *parent = static_cast<VDTContextD3D9 *>(mpParent);
	IDirect3DDevice9 *dev = parent->GetDeviceD3D9();
	VDTSurfaceD3D9 *src = static_cast<VDTSurfaceD3D9 *>(src0);

	const RECT rSrc = { sx, sy, sx+w, sy+h };
	const RECT rDst = { dx, dy, dx+w, dy+h };
	HRESULT hr = dev->StretchRect(src->mpSurface, &rSrc, mpSurface, &rDst, D3DTEXF_NONE);
	if (FAILED(hr))
		parent->ProcessHRESULT(hr);
}

void VDTSurfaceD3D9::GetDesc(VDTSurfaceDesc& desc) {
	desc = mDesc;
}

bool VDTSurfaceD3D9::Lock(const vdrect32 *r, VDTLockData2D& lockData) {
	if (!mpSurface)
		return false;

	RECT r2;
	const RECT *pr = NULL;
	if (r) {
		r2.left = r->left;
		r2.top = r->top;
		r2.right = r->right;
		r2.bottom = r->bottom;
		pr = &r2;
	}

	D3DLOCKED_RECT lr;
	HRESULT hr = mpSurface->LockRect(&lr, pr, D3DLOCK_NOSYSLOCK);
	if (FAILED(hr)) {
		VDTContextD3D9 *parent = static_cast<VDTContextD3D9 *>(mpParent);
		parent->ProcessHRESULT(hr);
		return false;
	}

	lockData.mpData = lr.pBits;
	lockData.mPitch = lr.Pitch;

	return true;
}

void VDTSurfaceD3D9::Unlock() {
	HRESULT hr = mpSurface->UnlockRect();

	if (FAILED(hr)) {
		VDTContextD3D9 *parent = static_cast<VDTContextD3D9 *>(mpParent);
		parent->ProcessHRESULT(hr);
	}
}

void VDTSurfaceD3D9::ShutdownDefaultPool() {
	if (!mbDefaultPool)
		return;

	if (mpSurface) {
		mpSurface->Release();
		mpSurface = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

VDTTexture2DD3D9::VDTTexture2DD3D9()
	: mpTexture(NULL)
{
}

VDTTexture2DD3D9::~VDTTexture2DD3D9() {
	Shutdown();
}

void *VDTTexture2DD3D9::AsInterface(uint32 id) {
	if (id == kTypeD3DTexture)
		return mpTexture;

	if (id == IVDTTexture2D::kTypeID)
		return static_cast<IVDTTexture2D *>(this);

	return NULL;
}

bool VDTTexture2DD3D9::Init(VDTContextD3D9 *parent, uint32 width, uint32 height, uint32 format, uint32 mipcount, VDTUsage usage, const VDTInitData2D *initData) {
	parent->AddResource(this);

	mWidth = width;
	mHeight = height;
	mMipCount = mipcount;
	mUsage = usage;

	if (!Restore())
		return false;

	uint32 mipCount = mpTexture->GetLevelCount();
	mMipmaps.reserve(mipCount);

	for(uint32 i=0; i<mipCount; ++i) {
		vdrefptr<VDTSurfaceD3D9> surf(new VDTSurfaceD3D9);
		IDirect3DSurface9 *surfd3d9;

		HRESULT hr = mpTexture->GetSurfaceLevel(i, &surfd3d9);
		if (FAILED(hr)) {
			parent->ProcessHRESULT(hr);
			Shutdown();
			return false;
		}

		surf->Init(parent, surfd3d9);
		surfd3d9->Release();

		mMipmaps.push_back(surf.release());
	}

	if (initData) {
		D3DLOCKED_RECT lr;
		HRESULT hr = mpTexture->LockRect(0, &lr, NULL, D3DLOCK_NOSYSLOCK);
		if (SUCCEEDED(hr)) {
			VDMemcpyRect(lr.pBits, lr.Pitch, initData->mpData, initData->mPitch, width*4, height);
			mpTexture->UnlockRect(0);
		}
	}

	return true;
}

void VDTTexture2DD3D9::Shutdown() {
	while(!mMipmaps.empty()) {
		VDTSurfaceD3D9 *surf = mMipmaps.back();
		mMipmaps.pop_back();

		surf->Shutdown();
		surf->Release();
	}

	if (mpTexture) {
		if (mpParent)
			static_cast<VDTContextD3D9 *>(mpParent)->UnsetTexture(this);

		mpTexture->Release();
		mpTexture = NULL;
	}

	VDTResourceD3D9::Shutdown();
}

bool VDTTexture2DD3D9::Restore() {
	if (mpTexture)
		return true;

	VDTContextD3D9 *parent = static_cast<VDTContextD3D9 *>(mpParent);
	IDirect3DDevice9 *dev = parent->GetDeviceD3D9();
	if (!dev)
		return false;

	IDirect3DTexture9 *texD3D9;
	HRESULT hr;
	switch(mUsage) {
		case kVDTUsage_Default:
			hr = dev->CreateTexture(mWidth, mHeight, mMipCount, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texD3D9, NULL);
			break;

		case kVDTUsage_Render:
			hr = dev->CreateTexture(mWidth, mHeight, mMipCount, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texD3D9, NULL);
			break;

		default:
			return false;
	}
	
	if (FAILED(hr))
		return false;

	mpTexture = texD3D9;

	uint32 n = (uint32)mMipmaps.size();
	for(uint32 i=0; i<n; ++i) {
		VDTSurfaceD3D9 *surf = mMipmaps[i];
		IDirect3DSurface9 *surfd3d9;

		HRESULT hr = mpTexture->GetSurfaceLevel(i, &surfd3d9);
		if (FAILED(hr)) {
			parent->ProcessHRESULT(hr);
			Shutdown();
			return false;
		}

		surf->Shutdown();
		surf->Init(parent, surfd3d9);
		surfd3d9->Release();
	}

	return true;
}

IVDTSurface *VDTTexture2DD3D9::GetLevelSurface(uint32 level) {
	return mMipmaps[level];
}

void VDTTexture2DD3D9::GetDesc(VDTTextureDesc& desc) {
	desc.mWidth = mWidth;
	desc.mHeight = mHeight;
	desc.mMipCount = mMipCount;
	desc.mFormat = 0;
}

void VDTTexture2DD3D9::Load(uint32 mip, uint32 x, uint32 y, const VDTInitData2D& srcData, uint32 w, uint32 h) {
	mMipmaps[mip]->Load(x, y, srcData, w, h);
}

bool VDTTexture2DD3D9::Lock(uint32 mip, const vdrect32 *r, VDTLockData2D& lockData) {
	return mMipmaps[mip]->Lock(r, lockData);
}

void VDTTexture2DD3D9::Unlock(uint32 mip) {
	mMipmaps[mip]->Unlock();
}

void VDTTexture2DD3D9::ShutdownDefaultPool() {
	if (mUsage != kVDTUsage_Render)
		return;

	if (mpParent)
		static_cast<VDTContextD3D9 *>(mpParent)->UnsetTexture(this);

	mpTexture->Release();
	mpTexture = NULL;
}

///////////////////////////////////////////////////////////////////////////////

VDTVertexBufferD3D9::VDTVertexBufferD3D9()
	: mpVB(NULL)
{
}

VDTVertexBufferD3D9::~VDTVertexBufferD3D9() {
	Shutdown();
}

bool VDTVertexBufferD3D9::Init(VDTContextD3D9 *parent, uint32 size, bool dynamic, const void *initData) {
	parent->AddResource(this);

	mbDynamic = dynamic;
	mByteSize = size;

	if (!Restore()) {
		Shutdown();
		return false;
	}

	if (initData) {
		if (!Load(0, size, initData)) {
			Shutdown();
			return false;
		}
	}

	return true;
}

void VDTVertexBufferD3D9::Shutdown() {
	if (mpVB) {
		if (mpParent)
			static_cast<VDTContextD3D9 *>(mpParent)->UnsetVertexBuffer(this);

		mpVB->Release();
		mpVB = NULL;
	}

	VDTResourceD3D9::Shutdown();
}

bool VDTVertexBufferD3D9::Restore() {
	if (mpVB)
		return true;

	IDirect3DDevice9 *dev = static_cast<VDTContextD3D9 *>(mpParent)->GetDeviceD3D9();
	if (!dev)
		return false;

	HRESULT hr = dev->CreateVertexBuffer(mByteSize, mbDynamic ? D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY : 0, 0, mbDynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED, &mpVB, NULL);

	if (FAILED(hr))
		return false;

	return true;
}

bool VDTVertexBufferD3D9::Load(uint32 offset, uint32 size, const void *data) {
	if (!size)
		return true;

	if (offset > mByteSize || mByteSize - offset < size)
		return false;

	DWORD flags = D3DLOCK_NOSYSLOCK;

	if (mbDynamic) {
		if (offset)
			flags |= D3DLOCK_NOOVERWRITE;
		else
			flags |= D3DLOCK_DISCARD;
	}

	void *p;
	HRESULT hr = mpVB->Lock(offset, size, &p, flags);
	if (FAILED(hr)) {
		static_cast<VDTContextD3D9 *>(mpParent)->ProcessHRESULT(hr);
		return false;
	}

	bool success = true;
	if (mbDynamic)
		success = VDMemcpyGuarded(p, data, size);
	else
		memcpy(p, data, size);

	hr = mpVB->Unlock();
	if (FAILED(hr)) {
		static_cast<VDTContextD3D9 *>(mpParent)->ProcessHRESULT(hr);
		return false;
	}

	return success;
}

void VDTVertexBufferD3D9::ShutdownDefaultPool() {
	if (!mbDynamic)
		return;

	if (mpVB) {
		if (mpParent)
			static_cast<VDTContextD3D9 *>(mpParent)->UnsetVertexBuffer(this);

		mpVB->Release();
		mpVB = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

VDTIndexBufferD3D9::VDTIndexBufferD3D9()
	: mpIB(NULL)
{
}

VDTIndexBufferD3D9::~VDTIndexBufferD3D9() {
	Shutdown();
}

bool VDTIndexBufferD3D9::Init(VDTContextD3D9 *parent, uint32 size, bool index32, bool dynamic, const void *initData) {
	parent->AddResource(this);

	mByteSize = index32 ? size << 2 : size << 1;
	mbDynamic = dynamic;
	mbIndex32 = index32;

	if (!Restore()) {
		Shutdown();
		return false;
	}

	if (initData) {
		if (!Load(0, mByteSize, initData)) {
			Shutdown();
			return false;
		}
	}


	return true;
}

void VDTIndexBufferD3D9::Shutdown() {
	if (mpIB) {
		if (mpParent)
			static_cast<VDTContextD3D9 *>(mpParent)->UnsetIndexBuffer(this);

		mpIB->Release();
		mpIB = NULL;
	}

	VDTResourceD3D9::Shutdown();
}

bool VDTIndexBufferD3D9::Restore() {
	if (mpIB)
		return true;

	IDirect3DDevice9 *dev = static_cast<VDTContextD3D9 *>(mpParent)->GetDeviceD3D9();
	if (!dev)
		return false;

	HRESULT hr = dev->CreateIndexBuffer(mByteSize, mbDynamic ? D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY : 0, mbIndex32 ? D3DFMT_INDEX32 : D3DFMT_INDEX16, mbDynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED, &mpIB, NULL);

	if (FAILED(hr))
		return false;

	return true;
}

bool VDTIndexBufferD3D9::Load(uint32 offset, uint32 size, const void *data) {
	if (!size)
		return true;

	void *p;
	HRESULT hr = mpIB->Lock(offset, size, &p, D3DLOCK_NOSYSLOCK);
	if (FAILED(hr)) {
		static_cast<VDTContextD3D9 *>(mpParent)->ProcessHRESULT(hr);
		return false;
	}

	bool success = true;
	if (mbDynamic)
		success = VDMemcpyGuarded(p, data, size);
	else
		memcpy(p, data, size);

	hr = mpIB->Unlock();
	if (FAILED(hr)) {
		static_cast<VDTContextD3D9 *>(mpParent)->ProcessHRESULT(hr);
		return false;
	}

	return success;
}

void VDTIndexBufferD3D9::ShutdownDefaultPool() {
	if (!mbDynamic)
		return;

	if (mpIB) {
		if (mpParent)
			static_cast<VDTContextD3D9 *>(mpParent)->UnsetIndexBuffer(this);

		mpIB->Release();
		mpIB = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

VDTVertexFormatD3D9::VDTVertexFormatD3D9()
	: mpVF(NULL)
{
}

VDTVertexFormatD3D9::~VDTVertexFormatD3D9() {
	Shutdown();
}

bool VDTVertexFormatD3D9::Init(VDTContextD3D9 *parent, const VDTVertexElement *elements, uint32 count) {
	static const D3DDECLUSAGE kUsageD3D9[]={
		D3DDECLUSAGE_POSITION,
		D3DDECLUSAGE_BLENDWEIGHT,
		D3DDECLUSAGE_BLENDINDICES,
		D3DDECLUSAGE_NORMAL,
		D3DDECLUSAGE_TEXCOORD,
		D3DDECLUSAGE_TANGENT,
		D3DDECLUSAGE_BINORMAL,
		D3DDECLUSAGE_COLOR
	};

	static const D3DDECLTYPE kTypeD3D9[]={
		D3DDECLTYPE_FLOAT1,
		D3DDECLTYPE_FLOAT2,
		D3DDECLTYPE_FLOAT3,
		D3DDECLTYPE_FLOAT4,
		D3DDECLTYPE_UBYTE4
	};

	if (count >= 16) {
		VDASSERT(!"Too many vertex elements.");
		return false;
	}

	D3DVERTEXELEMENT9 vxe[16];
	for(uint32 i=0; i<count; ++i) {
		D3DVERTEXELEMENT9& dst = vxe[i];
		const VDTVertexElement& src = elements[i];

		dst.Stream = 0;
		dst.Offset = src.mOffset;
		dst.Type = kTypeD3D9[src.mType];
		dst.Method = D3DDECLMETHOD_DEFAULT;
		dst.Usage = kUsageD3D9[src.mUsage];
		dst.UsageIndex = src.mUsageIndex;
	}

	memset(&vxe[count], 0, sizeof(vxe[count]));
	vxe[count].Stream = 0xFF;
	vxe[count].Type = D3DDECLTYPE_UNUSED;

	HRESULT hr = parent->GetDeviceD3D9()->CreateVertexDeclaration(vxe, &mpVF);

	if (FAILED(hr))
		return false;

	parent->AddResource(this);
	return true;
}

void VDTVertexFormatD3D9::Shutdown() {
	if (mpVF) {
		if (mpParent)
			static_cast<VDTContextD3D9 *>(mpParent)->UnsetVertexFormat(this);

		mpVF->Release();
		mpVF = NULL;
	}

	VDTResourceD3D9::Shutdown();
}

bool VDTVertexFormatD3D9::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTVertexProgramD3D9::VDTVertexProgramD3D9()
	: mpVS(NULL)
{
}

VDTVertexProgramD3D9::~VDTVertexProgramD3D9() {
	Shutdown();
}

bool VDTVertexProgramD3D9::Init(VDTContextD3D9 *parent, VDTProgramFormat format, const void *data, uint32 size) {
	HRESULT hr = parent->GetDeviceD3D9()->CreateVertexShader((const DWORD *)data, &mpVS);

	if (FAILED(hr))
		return false;

	parent->AddResource(this);
	return true;
}

void VDTVertexProgramD3D9::Shutdown() {
	if (mpVS) {
		if (mpParent)
			static_cast<VDTContextD3D9 *>(mpParent)->UnsetVertexProgram(this);

		mpVS->Release();
		mpVS = NULL;
	}

	VDTResourceD3D9::Shutdown();
}

bool VDTVertexProgramD3D9::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTFragmentProgramD3D9::VDTFragmentProgramD3D9()
	: mpPS(NULL)
{
}

VDTFragmentProgramD3D9::~VDTFragmentProgramD3D9() {
	Shutdown();
}

bool VDTFragmentProgramD3D9::Init(VDTContextD3D9 *parent, VDTProgramFormat format, const void *data, uint32 size) {
	HRESULT hr = parent->GetDeviceD3D9()->CreatePixelShader((const DWORD *)data, &mpPS);

	if (FAILED(hr))
		return false;

	parent->AddResource(this);
	return true;
}

void VDTFragmentProgramD3D9::Shutdown() {
	if (mpPS) {
		if (mpParent)
			static_cast<VDTContextD3D9 *>(mpParent)->UnsetFragmentProgram(this);

		mpPS->Release();
		mpPS = NULL;
	}

	VDTResourceD3D9::Shutdown();
}

bool VDTFragmentProgramD3D9::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTBlendStateD3D9::VDTBlendStateD3D9() {
}

VDTBlendStateD3D9::~VDTBlendStateD3D9() {
	Shutdown();
}

bool VDTBlendStateD3D9::Init(VDTContextD3D9 *parent, const VDTBlendStateDesc& desc) {
	mDesc = desc;

	static const uint32 kD3DBlendStateLookup[]={
		D3DBLEND_ZERO,
		D3DBLEND_ONE,
		D3DBLEND_SRCCOLOR,
		D3DBLEND_INVSRCCOLOR,
		D3DBLEND_SRCALPHA,
		D3DBLEND_INVSRCALPHA,
		D3DBLEND_DESTALPHA,
		D3DBLEND_INVDESTALPHA,
		D3DBLEND_DESTCOLOR,
		D3DBLEND_INVDESTCOLOR
	};

	static const uint32 kD3DBlendOpLookup[]={
		D3DBLENDOP_ADD,
		D3DBLENDOP_SUBTRACT,
		D3DBLENDOP_REVSUBTRACT,
		D3DBLENDOP_MIN,
		D3DBLENDOP_MAX
	};

	mRenderStates[0] = desc.mbEnable;
	mRenderStates[1] = kD3DBlendStateLookup[desc.mSrc];
	mRenderStates[2] = kD3DBlendStateLookup[desc.mDst];
	mRenderStates[3] = kD3DBlendOpLookup[desc.mOp];

	parent->AddResource(this);
	return true;
}

void VDTBlendStateD3D9::Shutdown() {
	if (mpParent)
		static_cast<VDTContextD3D9 *>(mpParent)->UnsetBlendState(this);

	VDTResourceD3D9::Shutdown();
}

bool VDTBlendStateD3D9::Restore() {
	return true;
}

const uint32 VDTBlendStateD3D9::kRenderStateIDs[kStateCount]={
	D3DRS_ALPHABLENDENABLE,
	D3DRS_SRCBLEND,
	D3DRS_DESTBLEND,
	D3DRS_BLENDOP
};

///////////////////////////////////////////////////////////////////////////////

VDTRasterizerStateD3D9::VDTRasterizerStateD3D9() {
}

VDTRasterizerStateD3D9::~VDTRasterizerStateD3D9() {
	Shutdown();
}

bool VDTRasterizerStateD3D9::Init(VDTContextD3D9 *parent, const VDTRasterizerStateDesc& desc) {
	mDesc = desc;

	parent->AddResource(this);

	if (desc.mCullMode == kVDTCull_None) {
		mRenderStates[0] = D3DCULL_NONE;
	} else {
		bool cullCW = desc.mbFrontIsCCW;

		if (desc.mCullMode == kVDTCull_Back)
			cullCW = !cullCW;

		if (cullCW)
			mRenderStates[0] = D3DCULL_CW;
		else
			mRenderStates[0] = D3DCULL_CCW;
	}

	mRenderStates[1] = desc.mbEnableScissor;

	return true;
}

void VDTRasterizerStateD3D9::Shutdown() {
	if (mpParent)
		static_cast<VDTContextD3D9 *>(mpParent)->UnsetRasterizerState(this);

	VDTResourceD3D9::Shutdown();
}

bool VDTRasterizerStateD3D9::Restore() {
	return true;
}

const uint32 VDTRasterizerStateD3D9::kRenderStateIDs[kStateCount]={
	D3DRS_CULLMODE,
	D3DRS_SCISSORTESTENABLE
};

///////////////////////////////////////////////////////////////////////////////

VDTSamplerStateD3D9::VDTSamplerStateD3D9() {
}

VDTSamplerStateD3D9::~VDTSamplerStateD3D9() {
	Shutdown();
}

bool VDTSamplerStateD3D9::Init(VDTContextD3D9 *parent, const VDTSamplerStateDesc& desc) {
	mDesc = desc;

	static const uint32 kD3DMinMagFilterLookup[]={
		D3DTEXF_POINT,
		D3DTEXF_LINEAR,
		D3DTEXF_LINEAR,
		D3DTEXF_LINEAR,
		D3DTEXF_ANISOTROPIC
	};

	static const uint32 kD3DMipFilterLookup[]={
		D3DTEXF_NONE,
		D3DTEXF_NONE,
		D3DTEXF_POINT,
		D3DTEXF_LINEAR,
		D3DTEXF_ANISOTROPIC
	};

	static const uint32 kD3DAddressLookup[]={
		D3DTADDRESS_CLAMP,
		D3DTADDRESS_WRAP
	};

	mSamplerStates[0] = kD3DMinMagFilterLookup[desc.mFilterMode];
	mSamplerStates[1] = kD3DMinMagFilterLookup[desc.mFilterMode];
	mSamplerStates[2] = kD3DMipFilterLookup[desc.mFilterMode];
	mSamplerStates[3] = kD3DAddressLookup[desc.mAddressU];
	mSamplerStates[4] = kD3DAddressLookup[desc.mAddressV];
	mSamplerStates[5] = kD3DAddressLookup[desc.mAddressW];

	parent->AddResource(this);
	return true;
}

void VDTSamplerStateD3D9::Shutdown() {
	if (mpParent)
		static_cast<VDTContextD3D9 *>(mpParent)->UnsetSamplerState(this);

	VDTResourceD3D9::Shutdown();
}

bool VDTSamplerStateD3D9::Restore() {
	return true;
}

const uint32 VDTSamplerStateD3D9::kSamplerStateIDs[kStateCount] = {
	D3DSAMP_MINFILTER,
	D3DSAMP_MAGFILTER,
	D3DSAMP_MIPFILTER,
	D3DSAMP_ADDRESSU,
	D3DSAMP_ADDRESSV,
	D3DSAMP_ADDRESSW
};

///////////////////////////////////////////////////////////////////////////////

struct VDTContextD3D9::PrivateData {
	D3DPRESENT_PARAMETERS mPresentParms;
	HMODULE mhmodD3D9;

	VDTFenceManagerD3D9 mFenceManager;
};

VDTContextD3D9::VDTContextD3D9()
	: mRefCount(0)
	, mpData(NULL)
	, mpD3DHolder(NULL)
	, mpD3DDevice(NULL)
	, mDeviceLossCounter(0)
	, mbDeviceLost(false)
	, mbInScene(false)
	, mpCurrentRT(NULL)
	, mpCurrentVB(NULL)
	, mCurrentVBOffset(NULL)
	, mCurrentVBStride(NULL)
	, mpCurrentIB(NULL)
	, mpCurrentVP(NULL)
	, mpCurrentFP(NULL)
	, mpCurrentVF(NULL)
	, mpCurrentBS(NULL)
	, mpCurrentRS(NULL)
	, mpDefaultRT(NULL)
	, mpDefaultBS(NULL)
	, mpDefaultRS(NULL)
	, mpDefaultSS(NULL)
	, mDirtySamplerStates(0)
	, mProfChan("Filter 3D accel")
{
	memset(mpCurrentSamplerStates, 0, sizeof mpCurrentSamplerStates);
	memset(mpCurrentTextures, 0, sizeof mpCurrentTextures);
}

VDTContextD3D9::~VDTContextD3D9() {
	Shutdown();
}

int VDTContextD3D9::AddRef() {
	return ++mRefCount;
}

int VDTContextD3D9::Release() {
	int rc = --mRefCount;

	if (!rc)
		delete this;

	return rc;
}

void *VDTContextD3D9::AsInterface(uint32 iid) {
	if (iid == IVDTProfiler::kTypeID)
		return static_cast<IVDTProfiler *>(this);

	return NULL;
}

bool VDTContextD3D9::Init(IDirect3DDevice9 *dev, IVDRefUnknown *pD3DHolder) {
	mpData = new PrivateData;
	mpData->mhmodD3D9 = LoadLibrary("d3d9");

	if (mpData->mhmodD3D9) {
		mpBeginEvent = GetProcAddress(mpData->mhmodD3D9, "D3DPERF_BeginEvent");
		mpEndEvent = GetProcAddress(mpData->mhmodD3D9, "D3DPERF_EndEvent");
	}

	mpD3DHolder = pD3DHolder;
	if (mpD3DHolder)
		mpD3DHolder->AddRef();

	mpD3DDevice = dev;
	mpD3DDevice->AddRef();

	mpData->mFenceManager.Init(mpD3DDevice);

	vdrefptr<IDirect3DSwapChain9> chain;
	HRESULT hr = mpD3DDevice->GetSwapChain(0, ~chain);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	hr = chain->GetPresentParameters(&mpData->mPresentParms);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	mpDefaultBS = new VDTBlendStateD3D9;
	mpDefaultBS->AddRef();
	mpDefaultBS->Init(this, VDTBlendStateDesc());

	mpDefaultRS = new VDTRasterizerStateD3D9;
	mpDefaultRS->AddRef();
	mpDefaultRS->Init(this, VDTRasterizerStateDesc());

	mpDefaultSS = new VDTSamplerStateD3D9;
	mpDefaultSS->AddRef();
	mpDefaultSS->Init(this, VDTSamplerStateDesc());

	mpDefaultRT = new VDTSurfaceD3D9;
	mpDefaultRT->AddRef();

	mpCurrentBS = mpDefaultBS;
	mpCurrentRS = mpDefaultRS;
	mpCurrentRT = mpDefaultRT;

	for(int i=0; i<16; ++i)
		mpCurrentSamplerStates[i] = mpDefaultSS;

	mbBSDirty = true;
	mbRSDirty = true;
	mDirtySamplerStates = 0xffff;
	memset(mD3DBlendStates, 0xA5, sizeof mD3DBlendStates);
	memset(mD3DRasterizerStates, 0xA5, sizeof mD3DRasterizerStates);
	memset(mD3DSamplerStates, 0xA5, sizeof mD3DSamplerStates);

	if (!ConnectSurfaces()) {
		Shutdown();
		return false;
	}

	return true;
}

void VDTContextD3D9::Shutdown() {
	ShutdownAllResources();

	if (mpDefaultSS) {
		mpDefaultSS->Release();
		mpDefaultSS = NULL;
	}

	if (mpDefaultRS) {
		mpDefaultRS->Release();
		mpDefaultRS = NULL;
	}

	if (mpDefaultBS) {
		mpDefaultBS->Release();
		mpDefaultBS = NULL;
	}

	if (mpDefaultRT) {
		mpDefaultRT->Release();
		mpDefaultRT = NULL;
	}

	mpData->mFenceManager.Shutdown();

	if (mpD3DDevice) {
		mpD3DDevice->Release();
		mpD3DDevice = NULL;
	}

	if (mpD3DHolder) {
		mpD3DHolder->Release();
		mpD3DHolder = NULL;
	}

	if (mpData) {
		if (mpData->mhmodD3D9)
			FreeLibrary(mpData->mhmodD3D9);

		delete mpData;
		mpData = NULL;
	}

	mpBeginEvent = NULL;
	mpEndEvent = NULL;
}

bool VDTContextD3D9::CreateReadbackBuffer(uint32 width, uint32 height, uint32 format, IVDTReadbackBuffer **ppbuffer) {
	vdrefptr<VDTReadbackBufferD3D9> surf(new VDTReadbackBufferD3D9);

	if (!surf->Init(this, width, height, format))
		return false;

	*ppbuffer = surf.release();
	return true;
}

bool VDTContextD3D9::CreateSurface(uint32 width, uint32 height, uint32 format, VDTUsage usage, IVDTSurface **ppsurface) {
	vdrefptr<VDTSurfaceD3D9> surf(new VDTSurfaceD3D9);

	if (!surf->Init(this, width, height, format, usage))
		return false;

	*ppsurface = surf.release();
	return true;
}

bool VDTContextD3D9::CreateTexture2D(uint32 width, uint32 height, uint32 format, uint32 mipcount, VDTUsage usage, const VDTInitData2D *initData, IVDTTexture2D **pptex) {
	vdrefptr<VDTTexture2DD3D9> tex(new VDTTexture2DD3D9);

	if (!tex->Init(this, width, height, format, mipcount, usage, initData))
		return false;

	*pptex = tex.release();
	return true;
}

bool VDTContextD3D9::CreateVertexProgram(VDTProgramFormat format, const void *data, uint32 length, IVDTVertexProgram **program) {
	vdrefptr<VDTVertexProgramD3D9> vp(new VDTVertexProgramD3D9);

	if (!vp->Init(this, format, data, length))
		return false;

	*program = vp.release();
	return true;
}

bool VDTContextD3D9::CreateFragmentProgram(VDTProgramFormat format, const void *data, uint32 length, IVDTFragmentProgram **program) {
	vdrefptr<VDTFragmentProgramD3D9> fp(new VDTFragmentProgramD3D9);

	if (!fp->Init(this, format, data, length))
		return false;

	*program = fp.release();
	return true;
}

bool VDTContextD3D9::CreateVertexFormat(const VDTVertexElement *elements, uint32 count, IVDTVertexFormat **format) {
	vdrefptr<VDTVertexFormatD3D9> vf(new VDTVertexFormatD3D9);

	if (!vf->Init(this, elements, count))
		return false;

	*format = vf.release();
	return true;
}

bool VDTContextD3D9::CreateVertexBuffer(uint32 size, bool dynamic, const void *initData, IVDTVertexBuffer **ppbuffer) {
	vdrefptr<VDTVertexBufferD3D9> vb(new VDTVertexBufferD3D9);

	if (!vb->Init(this, size, dynamic, initData))
		return false;

	*ppbuffer = vb.release();
	return true;
}

bool VDTContextD3D9::CreateIndexBuffer(uint32 size, bool index32, bool dynamic, const void *initData, IVDTIndexBuffer **ppbuffer) {
	vdrefptr<VDTIndexBufferD3D9> ib(new VDTIndexBufferD3D9);

	if (!ib->Init(this, size, index32, dynamic, initData))
		return false;

	*ppbuffer = ib.release();
	return true;
}

bool VDTContextD3D9::CreateBlendState(const VDTBlendStateDesc& desc, IVDTBlendState **state) {
	vdrefptr<VDTBlendStateD3D9> bs(new VDTBlendStateD3D9);

	if (!bs->Init(this, desc))
		return false;

	*state = bs.release();
	return true;
}

bool VDTContextD3D9::CreateRasterizerState(const VDTRasterizerStateDesc& desc, IVDTRasterizerState **state) {
	vdrefptr<VDTRasterizerStateD3D9> rs(new VDTRasterizerStateD3D9);

	if (!rs->Init(this, desc))
		return false;

	*state = rs.release();
	return true;
}

bool VDTContextD3D9::CreateSamplerState(const VDTSamplerStateDesc& desc, IVDTSamplerState **state) {
	vdrefptr<VDTSamplerStateD3D9> ss(new VDTSamplerStateD3D9);

	if (!ss->Init(this, desc))
		return false;

	*state = ss.release();
	return true;
}

IVDTSurface *VDTContextD3D9::GetRenderTarget(uint32 index) const {
	return index ? NULL : mpCurrentRT;
}

void VDTContextD3D9::SetVertexFormat(IVDTVertexFormat *format) {
	if (format == mpCurrentVF)
		return;

	mpCurrentVF = static_cast<VDTVertexFormatD3D9 *>(format);

	HRESULT hr = mpD3DDevice->SetVertexDeclaration(mpCurrentVF ? mpCurrentVF->mpVF : NULL);
	if (FAILED(hr))
		ProcessHRESULT(hr);
}

void VDTContextD3D9::SetVertexProgram(IVDTVertexProgram *program) {
	if (program == mpCurrentVP)
		return;

	mpCurrentVP = static_cast<VDTVertexProgramD3D9 *>(program);

	HRESULT hr = mpD3DDevice->SetVertexShader(mpCurrentVP ? mpCurrentVP->mpVS : NULL);
	if (FAILED(hr))
		ProcessHRESULT(hr);
}

void VDTContextD3D9::SetFragmentProgram(IVDTFragmentProgram *program) {
	if (program == mpCurrentFP)
		return;

	mpCurrentFP = static_cast<VDTFragmentProgramD3D9 *>(program);

	HRESULT hr = mpD3DDevice->SetPixelShader(mpCurrentFP ? mpCurrentFP->mpPS : NULL);
	if (FAILED(hr))
		ProcessHRESULT(hr);
}

void VDTContextD3D9::SetVertexStream(uint32 index, IVDTVertexBuffer *buffer, uint32 offset, uint32 stride) {
	VDASSERT(index == 0);

	if (buffer == mpCurrentVB && offset == mCurrentVBOffset && offset == mCurrentVBStride)
		return;

	mpCurrentVB = static_cast<VDTVertexBufferD3D9 *>(buffer);
	mCurrentVBOffset = offset;
	mCurrentVBStride = stride;

	HRESULT hr = mpD3DDevice->SetStreamSource(0, mpCurrentVB ? mpCurrentVB->mpVB : NULL, offset, stride);
	if (FAILED(hr))
		ProcessHRESULT(hr);
}

void VDTContextD3D9::SetIndexStream(IVDTIndexBuffer *buffer) {
	if (buffer == mpCurrentIB)
		return;

	mpCurrentIB = static_cast<VDTIndexBufferD3D9 *>(buffer);

	HRESULT hr = mpD3DDevice->SetIndices(mpCurrentIB ? mpCurrentIB->mpIB : NULL);
	if (FAILED(hr))
		ProcessHRESULT(hr);
}

void VDTContextD3D9::SetRenderTarget(uint32 index, IVDTSurface *surface) {
	VDASSERT(index == 0);

	if (!index && !surface)
		surface = mpDefaultRT;

	if (mpCurrentRT == surface)
		return;

	mpCurrentRT = static_cast<VDTSurfaceD3D9 *>(surface);

	IDirect3DSurface9 *surf = NULL;
	if (mpCurrentRT)
		surf = mpCurrentRT->mpSurface;

	HRESULT hr = mpD3DDevice->SetRenderTarget(0, surf);

	if (FAILED(hr))
		ProcessHRESULT(hr);
}

void VDTContextD3D9::SetBlendState(IVDTBlendState *state) {
	if (!state)
		state = mpDefaultBS;

	if (mpCurrentBS == state)
		return;

	mpCurrentBS = static_cast<VDTBlendStateD3D9 *>(state);
	mbBSDirty = true;
}

void VDTContextD3D9::SetRasterizerState(IVDTRasterizerState *state) {
	if (!state)
		state = mpDefaultRS;

	if (mpCurrentRS == state)
		return;

	mpCurrentRS = static_cast<VDTRasterizerStateD3D9 *>(state);
	mbRSDirty = true;
}

void VDTContextD3D9::SetSamplerStates(uint32 baseIndex, uint32 count, IVDTSamplerState *const *states) {
	VDASSERT(baseIndex <= 16 && 16 - baseIndex >= count);

	for(uint32 i=0; i<count; ++i) {
		uint32 stage = baseIndex + i;
		VDTSamplerStateD3D9 *state = static_cast<VDTSamplerStateD3D9 *>(states[stage]);
		if (!state)
			state = mpDefaultSS;

		if (mpCurrentSamplerStates[stage] != state) {
			mpCurrentSamplerStates[stage] = state;
			mDirtySamplerStates |= 1 << stage;
		}
	}
}

void VDTContextD3D9::SetTextures(uint32 baseIndex, uint32 count, IVDTTexture *const *textures) {
	VDASSERT(baseIndex <= 16 && 16 - baseIndex >= count);

	for(uint32 i=0; i<count; ++i) {
		uint32 stage = baseIndex + i;
		IVDTTexture *tex = textures[i];

		if (mpCurrentTextures[stage] != tex) {
			mpCurrentTextures[stage] = tex;

			HRESULT hr = mpD3DDevice->SetTexture(stage, tex ? (IDirect3DBaseTexture9 *)tex->AsInterface(VDTTextureD3D9::kTypeD3DTexture) : NULL);
			if (FAILED(hr)) {
				ProcessHRESULT(hr);
				break;
			}
		}
	}
}

void VDTContextD3D9::SetVertexProgramConstF(uint32 baseIndex, uint32 count, const float *data) {
	HRESULT hr = mpD3DDevice->SetVertexShaderConstantF(baseIndex, data, count);
	if (FAILED(hr))
		ProcessHRESULT(hr);
}

void VDTContextD3D9::SetFragmentProgramConstF(uint32 baseIndex, uint32 count, const float *data) {
	HRESULT hr = mpD3DDevice->SetPixelShaderConstantF(baseIndex, data, count);
	if (FAILED(hr))
		ProcessHRESULT(hr);
}

void VDTContextD3D9::Clear(VDTClearFlags clearFlags, uint32 color, float depth, uint32 stencil) {
	DWORD flags = 0;

	if (clearFlags & kVDTClear_Color)
		flags |= D3DCLEAR_TARGET;

	if (clearFlags & kVDTClear_Depth)
		flags |= D3DCLEAR_ZBUFFER;

	if (clearFlags & kVDTClear_Stencil)
		flags |= D3DCLEAR_STENCIL;

	if (flags) {
		HRESULT hr = mpD3DDevice->Clear(0, NULL, flags, color, depth, stencil);
		if (FAILED(hr))
			ProcessHRESULT(hr);
	}
}

namespace {
	const D3DPRIMITIVETYPE kPTLookup[]={
		D3DPT_TRIANGLELIST,
		D3DPT_TRIANGLESTRIP,
		D3DPT_TRIANGLEFAN
	};
}

void VDTContextD3D9::DrawPrimitive(VDTPrimitiveType type, uint32 startVertex, uint32 primitiveCount) {
	if (!mbInScene && !OpenScene())
		return;

	if (!CommitState())
		return;

	HRESULT hr = mpD3DDevice->DrawPrimitive(kPTLookup[type], startVertex, primitiveCount);
	if (FAILED(hr))
		ProcessHRESULT(hr);
}

void VDTContextD3D9::DrawIndexedPrimitive(VDTPrimitiveType type, uint32 baseVertexIndex, uint32 minVertex, uint32 vertexCount, uint32 startIndex, uint32 primitiveCount) {
	if (!mbInScene && !OpenScene())
		return;

	if (!CommitState())
		return;

	HRESULT hr = mpD3DDevice->DrawIndexedPrimitive(kPTLookup[type], baseVertexIndex, minVertex, vertexCount, startIndex, primitiveCount);
	if (FAILED(hr))
		ProcessHRESULT(hr);
}

uint32 VDTContextD3D9::InsertFence() {
	return mpData->mFenceManager.InsertFence();
}

bool VDTContextD3D9::CheckFence(uint32 id) {
	return mpData->mFenceManager.CheckFence(id);
}

bool VDTContextD3D9::RecoverDevice() {
	if (!mbDeviceLost)
		return true;

	HRESULT hr = mpD3DDevice->TestCooperativeLevel();
	if (hr != D3DERR_DEVICENOTRESET && !SUCCEEDED(hr))
		return false;

	ShutdownDefaultPoolResources();
	mpData->mFenceManager.FlushDefaultResources();

	hr = mpD3DDevice->Reset(&mpData->mPresentParms);
	if (FAILED(hr) || !ConnectSurfaces()) {
		mbInScene = false;
		mbDeviceLost = true;
		++mDeviceLossCounter;
		return false;
	}

	mbDeviceLost = false;
	mbBSDirty = true;
	mbRSDirty = true;
	mDirtySamplerStates = true;

	mpCurrentVB = NULL;
	mCurrentVBOffset = 0;
	mCurrentVBStride = 0;
	mpCurrentIB = NULL;
	mpCurrentVP = NULL;
	mpCurrentFP = NULL;
	mpCurrentVF = NULL;

	mpCurrentBS = mpDefaultBS;
	mpCurrentRS = mpDefaultRS;
	mpCurrentRT = mpDefaultRT;

	for(int i=0; i<16; ++i) {
		mpCurrentSamplerStates[i] = mpDefaultSS;
		mpCurrentTextures[i] = NULL;

	}

	return true;
}

bool VDTContextD3D9::OpenScene() {
	if (mbDeviceLost)
		return false;

	if (mbInScene)
		return true;

	HRESULT hr = mpD3DDevice->BeginScene();
	if (FAILED(hr)) {
		ProcessHRESULT(hr);
		return false;
	}

	mbInScene = true;
	return true;
}

bool VDTContextD3D9::CloseScene() {
	if (mbInScene) {
		mbInScene = false;
		HRESULT hr = mpD3DDevice->EndScene();
		if (FAILED(hr))
			ProcessHRESULT(hr);
	}

	return !mbDeviceLost;
}

uint32 VDTContextD3D9::GetDeviceLossCounter() const {
	return mDeviceLossCounter;
}

void VDTContextD3D9::Present() {
	if (mbInScene)
		CloseScene();

	if (mbDeviceLost)
		return;

	HRESULT hr = mpD3DDevice->Present(NULL, NULL, NULL, NULL);

	if (FAILED(hr))
		ProcessHRESULT(hr);
}

void VDTContextD3D9::BeginScope(uint32 color, const char *message) {
	typedef int (WINAPI *tpBeginEvent)(D3DCOLOR, LPCWSTR);
	if (mpBeginEvent) {
		WCHAR wbuf[64];
		WCHAR *t = wbuf;
		const char *s = message;

		for(int i=0; i<63; ++i) {
			char c = s[i];
			if (!c)
				break;

			*t++ = c;
		}

		*t = 0;

		((tpBeginEvent)mpBeginEvent)(color, wbuf);
	}

	mProfChan.Begin(color, message);
}

void VDTContextD3D9::EndScope() {
	mProfChan.End();

	typedef int (WINAPI *tpEndEvent)();
	if (mpEndEvent)
		((tpEndEvent)mpEndEvent)();
}

VDRTProfileChannel *VDTContextD3D9::GetProfileChannel() {
	return &mProfChan;
}

void VDTContextD3D9::UnsetVertexFormat(IVDTVertexFormat *format) {
	if (mpCurrentVF == format)
		SetVertexFormat(NULL);
}

void VDTContextD3D9::UnsetVertexProgram(IVDTVertexProgram *program) {
	if (mpCurrentVP == program)
		SetVertexProgram(NULL);
}

void VDTContextD3D9::UnsetFragmentProgram(IVDTFragmentProgram *program) {
	if (mpCurrentFP == program)
		SetFragmentProgram(NULL);
}

void VDTContextD3D9::UnsetVertexBuffer(IVDTVertexBuffer *buffer) {
	if (mpCurrentVB == buffer)
		SetVertexStream(0, NULL, 0, 0);
}

void VDTContextD3D9::UnsetIndexBuffer(IVDTIndexBuffer *buffer) {
	if (mpCurrentIB == buffer)
		SetIndexStream(NULL);
}

void VDTContextD3D9::UnsetRenderTarget(IVDTSurface *surface) {
	if (mpCurrentRT == surface)
		SetRenderTarget(0, NULL);
}

void VDTContextD3D9::UnsetBlendState(IVDTBlendState *state) {
	if (mpCurrentBS == state && state != mpDefaultBS)
		SetBlendState(NULL);
}

void VDTContextD3D9::UnsetRasterizerState(IVDTRasterizerState *state) {
	if (mpCurrentRS == state && state != mpDefaultRS)
		SetRasterizerState(NULL);
}

void VDTContextD3D9::UnsetSamplerState(IVDTSamplerState *state) {
	if (state == mpDefaultSS)
		return;

	for(int i=0; i<16; ++i) {
		if (mpCurrentSamplerStates[i] == state) {
			IVDTSamplerState *ssnull = NULL;
			SetSamplerStates(i, 1, &ssnull);
		}
	}
}

void VDTContextD3D9::UnsetTexture(IVDTTexture *tex) {
	for(int i=0; i<16; ++i) {
		if (mpCurrentTextures[i] == tex) {
			IVDTTexture *tex = NULL;
			SetTextures(i, 1, &tex);
		}
	}
}

void VDTContextD3D9::ProcessHRESULT(uint32 hr) {
	if (hr == D3DERR_DEVICELOST) {
		if (!mbDeviceLost) {
			mbDeviceLost = true;
			++mDeviceLossCounter;
		}

		mbInScene = false;
	}
}

bool VDTContextD3D9::ConnectSurfaces() {
	vdrefptr<IDirect3DSwapChain9> sc;
	HRESULT hr = mpD3DDevice->GetSwapChain(0, ~sc);
	if (FAILED(hr)) {
		ProcessHRESULT(hr);
		return false;
	}

	IDirect3DSurface9 *surf;
	sc->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &surf);
	if (FAILED(hr)) {
		ProcessHRESULT(hr);
		return false;
	}

	mpDefaultRT->Shutdown();
	bool success = mpDefaultRT->Init(this, surf);
	surf->Release();

	return success;
}

bool VDTContextD3D9::CommitState() {
	if (mbBSDirty) {
		mbBSDirty = false;
 
		UpdateRenderStates(VDTBlendStateD3D9::kRenderStateIDs, VDTBlendStateD3D9::kStateCount, mD3DBlendStates, mpCurrentBS->mRenderStates);
	}

	if (mbRSDirty) {
		mbRSDirty = false;

		UpdateRenderStates(VDTRasterizerStateD3D9::kRenderStateIDs, VDTRasterizerStateD3D9::kStateCount, mD3DRasterizerStates, mpCurrentRS->mRenderStates);
	}

	if (mDirtySamplerStates) {
		for(int i=0; i<16; ++i) {
			if (mDirtySamplerStates & (1 << i)) {
				const uint32 *values = mpCurrentSamplerStates[i]->mSamplerStates;
				uint32 *shadow = mD3DSamplerStates[i];

				for(uint32 j=0; j<VDTSamplerStateD3D9::kStateCount; ++j) {
					const uint32 v = values[j];

					if (shadow[j] != v) {
						shadow[j] = v;

						HRESULT hr = mpD3DDevice->SetSamplerState(i, (D3DSAMPLERSTATETYPE)VDTSamplerStateD3D9::kSamplerStateIDs[j], v);
						if (FAILED(hr)) {
							ProcessHRESULT(hr);
							break;
						}
					}
				}
			}
		}

		mDirtySamplerStates = 0;
	}

	return true;
}

void VDTContextD3D9::UpdateRenderStates(const uint32 *ids, uint32 count, uint32 *shadow, const uint32 *values) {
	for(uint32 i=0; i<count; ++i) {
		const uint32 v = values[i];

		if (shadow[i] != v) {
			shadow[i] = v;

			HRESULT hr = mpD3DDevice->SetRenderState((D3DRENDERSTATETYPE)ids[i], v);
			if (FAILED(hr)) {
				ProcessHRESULT(hr);
				break;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

class VDDirect3DHolder : public vdrefcounted<IVDRefUnknown> {
public:
	VDDirect3DHolder();
	~VDDirect3DHolder();

	void *AsInterface(uint32 iid);

	IDirect3D9 *GetD3D9() const { return mpD3D9; }

	bool Init();
	void Shutdown();

protected:
	HMODULE mhmodD3D9;
	IDirect3D9 *mpD3D9;
};

VDDirect3DHolder::VDDirect3DHolder()
	: mhmodD3D9(NULL)
	, mpD3D9(NULL)
{
}

VDDirect3DHolder::~VDDirect3DHolder() {
	Shutdown();
}

void *VDDirect3DHolder::AsInterface(uint32 iid) {
	return NULL;
}

bool VDDirect3DHolder::Init() {
	if (!mhmodD3D9) {
		mhmodD3D9 = LoadLibrary("d3d9");

		if (!mhmodD3D9) {
			Shutdown();
			return false;
		}
	}

	if (!mpD3D9) {
		IDirect3D9 *(APIENTRY *pDirect3DCreate9)(UINT) = (IDirect3D9 *(APIENTRY *)(UINT))GetProcAddress(mhmodD3D9, "Direct3DCreate9");
		if (!pDirect3DCreate9) {
			Shutdown();
			return false;
		}

		mpD3D9 = pDirect3DCreate9(D3D_SDK_VERSION);
		if (!mpD3D9) {
			Shutdown();
			return false;
		}
	}

	return true;
}

void VDDirect3DHolder::Shutdown() {
	if (mpD3D9) {
		mpD3D9->Release();
		mpD3D9 = NULL;
	}

	if (mhmodD3D9) {
		FreeLibrary(mhmodD3D9);
		mhmodD3D9 = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

bool VDTCreateContextD3D9(int width, int height, int refresh, bool fullscreen, bool vsync, void *hwnd, IVDTContext **ppctx) {
	vdrefptr<VDDirect3DHolder> holder(new VDDirect3DHolder);

	if (!holder->Init())
		return false;

	IDirect3D9 *d3d9 = holder->GetD3D9();

	D3DPRESENT_PARAMETERS parms;
	parms.BackBufferWidth = width;
	parms.BackBufferHeight = height;
	parms.BackBufferFormat = fullscreen ? D3DFMT_A8R8G8B8 : D3DFMT_UNKNOWN;
	parms.BackBufferCount = fullscreen ? 3 : 1;
	parms.MultiSampleType = D3DMULTISAMPLE_NONE;
	parms.MultiSampleQuality = 0;
	parms.SwapEffect = D3DSWAPEFFECT_DISCARD;
	parms.hDeviceWindow = NULL;
	parms.Windowed = fullscreen ? FALSE : TRUE;
	parms.EnableAutoDepthStencil = FALSE;
	parms.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
	parms.Flags = 0;
	parms.FullScreen_RefreshRateInHz = refresh;
	parms.PresentationInterval = vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

	UINT adapter = D3DADAPTER_DEFAULT;
	D3DDEVTYPE devType = D3DDEVTYPE_HAL;

	UINT adapterCount = d3d9->GetAdapterCount();
	for(UINT i=0; i<adapterCount; ++i) {
		D3DADAPTER_IDENTIFIER9 ident;

		HRESULT hr = d3d9->GetAdapterIdentifier(i, 0, &ident);
		if (SUCCEEDED(hr)) {
			if (strstr(ident.Description, "PerfHUD")) {
				adapter = i;
				devType = D3DDEVTYPE_REF;
				break;
			}
		}
	}

	vdrefptr<IDirect3DDevice9> dev;
	HRESULT hr = d3d9->CreateDevice(adapter, devType, (HWND)hwnd, D3DCREATE_FPU_PRESERVE | D3DCREATE_HARDWARE_VERTEXPROCESSING, &parms, ~dev);
	if (FAILED(hr))
		return false;

	vdrefptr<VDTContextD3D9> ctx(new VDTContextD3D9);
	if (!ctx->Init(dev, holder))
		return false;

	*ppctx = ctx.release();
	return true;
}

bool VDTCreateContextD3D9(IDirect3DDevice9 *dev, IVDTContext **ppctx) {
	vdrefptr<VDTContextD3D9> ctx(new VDTContextD3D9);

	if (!ctx->Init(dev, NULL))
		return false;

	*ppctx = ctx.release();
	return true;
}
