#include "stdafx.h"
#include <d3d10.h>
#include <vd2/system/bitmath.h>
#include <vd2/system/w32assist.h>
#include <vd2/Tessa/Context.h>
#include "D3D10/Context_D3D10.h"
#include "D3D10/FenceManager_D3D10.h"

///////////////////////////////////////////////////////////////////////////////

VDTResourceD3D10::VDTResourceD3D10() {
	mListNodePrev = NULL;
	mpParent = NULL;
}

VDTResourceD3D10::~VDTResourceD3D10() {
}

void VDTResourceD3D10::Shutdown() {
	if (mListNodePrev)
		mpParent->RemoveResource(this);
}

void VDTResourceManagerD3D10::AddResource(VDTResourceD3D10 *res) {
	VDASSERT(!res->mListNodePrev);

	mResources.push_back(res);
	res->mpParent = this;
}

void VDTResourceManagerD3D10::RemoveResource(VDTResourceD3D10 *res) {
	VDASSERT(res->mListNodePrev);

	mResources.erase(res);
	res->mListNodePrev = NULL;
}

void VDTResourceManagerD3D10::ShutdownAllResources() {
	while(!mResources.empty()) {
		VDTResourceD3D10 *res = mResources.back();
		mResources.pop_back();

		res->mListNodePrev = NULL;
		res->Shutdown();
	}
}

///////////////////////////////////////////////////////////////////////////////

VDTReadbackBufferD3D10::VDTReadbackBufferD3D10()
	: mpSurface(NULL)
{
}

VDTReadbackBufferD3D10::~VDTReadbackBufferD3D10() {
	Shutdown();
}

bool VDTReadbackBufferD3D10::Init(VDTContextD3D10 *parent, uint32 width, uint32 height, uint32 format) {
	ID3D10Device *dev = parent->GetDeviceD3D10();

	D3D10_TEXTURE2D_DESC desc;
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D10_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D10_CPU_ACCESS_READ;
	desc.MiscFlags = 0;

	HRESULT hr = dev->CreateTexture2D(&desc, NULL, &mpSurface);

	parent->AddResource(this);
	return SUCCEEDED(hr);
}

void VDTReadbackBufferD3D10::Shutdown() {
	if (mpSurface) {
		mpSurface->Release();
		mpSurface = NULL;
	}

	VDTResourceD3D10::Shutdown();
}

bool VDTReadbackBufferD3D10::Lock(VDTLockData2D& lockData) {
	D3D10_MAPPED_TEXTURE2D info;
	HRESULT hr = mpSurface->Map(0, D3D10_MAP_READ, 0, &info);

	if (FAILED(hr)) {
		lockData.mpData = NULL;
		lockData.mPitch = 0;
		return false;
	}

	lockData.mpData = info.pData;
	lockData.mPitch = info.RowPitch;
	return true;
}

void VDTReadbackBufferD3D10::Unlock() {
	mpSurface->Unmap(0);
}

bool VDTReadbackBufferD3D10::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTSurfaceD3D10::VDTSurfaceD3D10()
	: mpSurface(NULL)
	, mpRTView(NULL)
{
}

VDTSurfaceD3D10::~VDTSurfaceD3D10() {
	Shutdown();
}

bool VDTSurfaceD3D10::Init(VDTContextD3D10 *parent, uint32 width, uint32 height, uint32 format, VDTUsage usage) {
	ID3D10Device *dev = parent->GetDeviceD3D10();
	HRESULT hr;
	
	mDesc.mWidth = width;
	mDesc.mHeight = height;
	mDesc.mFormat = format;

	D3D10_TEXTURE2D_DESC desc;
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D10_USAGE_DEFAULT;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	switch(usage) {
		case kVDTUsage_Default:
			desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
			break;

		case kVDTUsage_Render:
			desc.BindFlags = D3D10_BIND_RENDER_TARGET;
			break;
	}

	hr = dev->CreateTexture2D(&desc, NULL, &mpSurface);
	if (FAILED(hr))
		return false;

	if (usage == kVDTUsage_Render) {
		D3D10_RENDER_TARGET_VIEW_DESC rtvdesc = {};
		rtvdesc.Format = desc.Format;
		rtvdesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
		rtvdesc.Texture2D.MipSlice = 0;

		hr = dev->CreateRenderTargetView(mpSurface, &rtvdesc, &mpRTView);
		if (FAILED(hr)) {
			Shutdown();
			return false;
		}
	}

	parent->AddResource(this);

	return SUCCEEDED(hr);
}

bool VDTSurfaceD3D10::Init(VDTContextD3D10 *parent, ID3D10Texture2D *tex, uint32 mipLevel, bool rt) {
	D3D10_TEXTURE2D_DESC desc = {};

	tex->GetDesc(&desc);

	mDesc.mWidth = desc.Width;
	mDesc.mHeight = desc.Height;
	mDesc.mFormat = kVDTF_A8R8G8B8;

	if (rt) {
		D3D10_RENDER_TARGET_VIEW_DESC rtvdesc = {};
		rtvdesc.Format = desc.Format;
		rtvdesc.ViewDimension = D3D10_RTV_DIMENSION_TEXTURE2D;
		rtvdesc.Texture2D.MipSlice = 0;

		HRESULT hr = parent->GetDeviceD3D10()->CreateRenderTargetView(mpSurface, &rtvdesc, &mpRTView);
		if (FAILED(hr))
			return false;
	}

	parent->AddResource(this);

	mpSurface = tex;
	mpSurface->AddRef();
	return true;
}

void VDTSurfaceD3D10::Shutdown() {
	if (mpSurface) {
		mpSurface->Release();
		mpSurface = NULL;
	}

	VDTResourceD3D10::Shutdown();
}

bool VDTSurfaceD3D10::Restore() {
	return true;
}

bool VDTSurfaceD3D10::Readback(IVDTReadbackBuffer *target) {
	VDTContextD3D10 *parent = static_cast<VDTContextD3D10 *>(mpParent);
	ID3D10Device *dev = parent->GetDeviceD3D10();
	VDTReadbackBufferD3D10 *targetd3d10 = static_cast<VDTReadbackBufferD3D10 *>(target);

	dev->CopyResource(targetd3d10->mpSurface, mpSurface);
	return true;
}

void VDTSurfaceD3D10::Load(uint32 dx, uint32 dy, const VDTInitData2D& srcData, uint32 w, uint32 h) {
	D3D10_MAPPED_TEXTURE2D info;

	HRESULT hr = mpSurface->Map(0, D3D10_MAP_WRITE, 0, &info);
	if (FAILED(hr)) {
		VDTContextD3D10 *parent = static_cast<VDTContextD3D10 *>(mpParent);
		parent->ProcessHRESULT(hr);
		return;
	}

	VDMemcpyRect((char *)info.pData + info.RowPitch * dy, info.RowPitch, srcData.mpData, srcData.mPitch, 4*w, h);

	mpSurface->Unmap(0);
}

void VDTSurfaceD3D10::Copy(uint32 dx, uint32 dy, IVDTSurface *src0, uint32 sx, uint32 sy, uint32 w, uint32 h) {
	VDTContextD3D10 *parent = static_cast<VDTContextD3D10 *>(mpParent);
	ID3D10Device *dev = parent->GetDeviceD3D10();
	VDTSurfaceD3D10 *src = static_cast<VDTSurfaceD3D10 *>(src0);

	D3D10_BOX box;
	box.left = sx;
	box.right = sx + w;
	box.top = sy;
	box.bottom = sy+h;
	box.front = 0;
	box.back = 1;
	dev->CopySubresourceRegion(mpSurface, 0, dx, dy, 0, src->mpSurface, 0, &box);
}

void VDTSurfaceD3D10::GetDesc(VDTSurfaceDesc& desc) {
	desc = mDesc;
}

bool VDTSurfaceD3D10::Lock(const vdrect32 *r, VDTLockData2D& lockData) {
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

	D3D10_MAPPED_TEXTURE2D mapped;
	HRESULT hr = mpSurface->Map(0, D3D10_MAP_READ_WRITE, 0, &mapped);
	if (FAILED(hr)) {
		VDTContextD3D10 *parent = static_cast<VDTContextD3D10 *>(mpParent);
		parent->ProcessHRESULT(hr);
		return false;
	}

	lockData.mpData = mapped.pData;
	lockData.mPitch = mapped.RowPitch;

	return true;
}

void VDTSurfaceD3D10::Unlock() {
	mpSurface->Unmap(0);
}

///////////////////////////////////////////////////////////////////////////////

VDTTexture2DD3D10::VDTTexture2DD3D10()
	: mpTexture(NULL)
	, mpShaderResView(NULL)
{
}

VDTTexture2DD3D10::~VDTTexture2DD3D10() {
	Shutdown();
}

void *VDTTexture2DD3D10::AsInterface(uint32 id) {
	if (id == kTypeD3DShaderResView)
		return mpShaderResView;

	if (id == IVDTTexture2D::kTypeID)
		return static_cast<IVDTTexture2D *>(this);

	return NULL;
}

bool VDTTexture2DD3D10::Init(VDTContextD3D10 *parent, uint32 width, uint32 height, uint32 format, uint32 mipcount, VDTUsage usage, const VDTInitData2D *initData) {
	parent->AddResource(this);

	if (!mipcount) {
		uint32 mask = (width - 1) | (height - 1);

		mipcount = VDFindHighestSetBit(mask) + 1;
	}

	mWidth = width;
	mHeight = height;
	mMipCount = mipcount;
	mUsage = usage;

	if (mpTexture)
		return true;

	ID3D10Device *dev = parent->GetDeviceD3D10();
	if (!dev)
		return false;

	D3D10_TEXTURE2D_DESC desc;
    desc.Width = mWidth;
    desc.Height = mHeight;
    desc.MipLevels = mMipCount;
    desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D10_USAGE_DEFAULT;
	desc.BindFlags = usage == kVDTUsage_Render ? D3D10_BIND_RENDER_TARGET | D3D10_BIND_SHADER_RESOURCE : D3D10_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

	vdfastvector<D3D10_SUBRESOURCE_DATA> subResData;

	if (initData) {
		subResData.resize(mipcount);

		for(uint32 i=0; i<mipcount; ++i) {
			D3D10_SUBRESOURCE_DATA& dst = subResData[i];
			const VDTInitData2D& src = initData[i];

			dst.pSysMem = src.mpData;
			dst.SysMemPitch = (UINT)src.mPitch;
			dst.SysMemSlicePitch = 0;
		}
	}

	HRESULT hr = dev->CreateTexture2D(&desc, initData ? subResData.data() : NULL, &mpTexture);
	
	if (FAILED(hr))
		return false;

	D3D10_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
	hr = dev->CreateShaderResourceView(mpTexture, &srvdesc, &mpShaderResView);

	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	mMipmaps.reserve(mipcount);

	for(uint32 i=0; i<mipcount; ++i) {
		vdrefptr<VDTSurfaceD3D10> surf(new VDTSurfaceD3D10);

		surf->Init(parent, mpTexture, i, usage == kVDTUsage_Render);

		mMipmaps.push_back(surf.release());
	}

	return true;
}

void VDTTexture2DD3D10::Shutdown() {
	while(!mMipmaps.empty()) {
		VDTSurfaceD3D10 *surf = mMipmaps.back();
		mMipmaps.pop_back();

		surf->Shutdown();
		surf->Release();
	}

	if (mpTexture) {
		if (mpParent)
			static_cast<VDTContextD3D10 *>(mpParent)->UnsetTexture(this);

		mpTexture->Release();
		mpTexture = NULL;
	}

	VDTResourceD3D10::Shutdown();
}

bool VDTTexture2DD3D10::Restore() {
	return true;
}

IVDTSurface *VDTTexture2DD3D10::GetLevelSurface(uint32 level) {
	return mMipmaps[level];
}

void VDTTexture2DD3D10::GetDesc(VDTTextureDesc& desc) {
	desc.mWidth = mWidth;
	desc.mHeight = mHeight;
	desc.mMipCount = mMipCount;
	desc.mFormat = 0;
}

void VDTTexture2DD3D10::Load(uint32 mip, uint32 x, uint32 y, const VDTInitData2D& srcData, uint32 w, uint32 h) {
	mMipmaps[mip]->Load(x, y, srcData, w, h);
}

bool VDTTexture2DD3D10::Lock(uint32 mip, const vdrect32 *r, VDTLockData2D& lockData) {
	return mMipmaps[mip]->Lock(r, lockData);
}

void VDTTexture2DD3D10::Unlock(uint32 mip) {
	mMipmaps[mip]->Unlock();
}

///////////////////////////////////////////////////////////////////////////////

VDTVertexBufferD3D10::VDTVertexBufferD3D10()
	: mpVB(NULL)
{
}

VDTVertexBufferD3D10::~VDTVertexBufferD3D10() {
	Shutdown();
}

bool VDTVertexBufferD3D10::Init(VDTContextD3D10 *parent, uint32 size, bool dynamic, const void *initData) {
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

void VDTVertexBufferD3D10::Shutdown() {
	if (mpVB) {
		if (mpParent)
			static_cast<VDTContextD3D10 *>(mpParent)->UnsetVertexBuffer(this);

		mpVB->Release();
		mpVB = NULL;
	}

	VDTResourceD3D10::Shutdown();
}

bool VDTVertexBufferD3D10::Restore() {
	if (mpVB)
		return true;

	ID3D10Device *dev = static_cast<VDTContextD3D10 *>(mpParent)->GetDeviceD3D10();
	if (!dev)
		return false;

	D3D10_BUFFER_DESC desc;
    desc.ByteWidth = mByteSize;
	desc.Usage = mbDynamic ? D3D10_USAGE_DYNAMIC : D3D10_USAGE_DEFAULT;
	desc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;

	HRESULT hr = dev->CreateBuffer(&desc, NULL, &mpVB);

	if (FAILED(hr))
		return false;

	return true;
}

bool VDTVertexBufferD3D10::Load(uint32 offset, uint32 size, const void *data) {
	if (!size)
		return true;

	if (offset > mByteSize || mByteSize - offset < size)
		return false;

	D3D10_MAP flags = D3D10_MAP_WRITE;

	if (mbDynamic) {
		if (offset)
			flags = D3D10_MAP_WRITE_NO_OVERWRITE;
		else
			flags = D3D10_MAP_WRITE_DISCARD;
	}

	void *p;
	HRESULT hr = mpVB->Map(flags, 0, &p);
	if (FAILED(hr)) {
		static_cast<VDTContextD3D10 *>(mpParent)->ProcessHRESULT(hr);
		return false;
	}

	bool success = true;

	if (mbDynamic)
		success = VDMemcpyGuarded(p, data, size);
	else
		memcpy(p, data, size);

	mpVB->Unmap();
	return success;
}

///////////////////////////////////////////////////////////////////////////////

VDTIndexBufferD3D10::VDTIndexBufferD3D10()
	: mpIB(NULL)
{
}

VDTIndexBufferD3D10::~VDTIndexBufferD3D10() {
	Shutdown();
}

bool VDTIndexBufferD3D10::Init(VDTContextD3D10 *parent, uint32 size, bool index32, bool dynamic, const void *initData) {
	mByteSize = index32 ? size << 2 : size << 1;
	mbDynamic = dynamic;
	mbIndex32 = index32;

	if (mpIB)
		return true;

	ID3D10Device *dev = static_cast<VDTContextD3D10 *>(parent)->GetDeviceD3D10();
	if (!dev)
		return false;

	D3D10_BUFFER_DESC desc;
    desc.ByteWidth = mByteSize;
	desc.Usage = mbDynamic ? D3D10_USAGE_DYNAMIC : D3D10_USAGE_DEFAULT;
	desc.BindFlags = D3D10_BIND_INDEX_BUFFER;
	desc.CPUAccessFlags = mbDynamic ? D3D10_CPU_ACCESS_WRITE : 0;
    desc.MiscFlags = 0;

	D3D10_SUBRESOURCE_DATA srd;

	srd.pSysMem = initData;
	srd.SysMemPitch = 0;
	srd.SysMemSlicePitch = 0;

	HRESULT hr = dev->CreateBuffer(&desc, initData ? &srd : NULL, &mpIB);

	if (FAILED(hr))
		return false;

	parent->AddResource(this);
	return true;
}

void VDTIndexBufferD3D10::Shutdown() {
	if (mpIB) {
		if (mpParent)
			static_cast<VDTContextD3D10 *>(mpParent)->UnsetIndexBuffer(this);

		mpIB->Release();
		mpIB = NULL;
	}

	VDTResourceD3D10::Shutdown();
}

bool VDTIndexBufferD3D10::Restore() {
	return true;
}

bool VDTIndexBufferD3D10::Load(uint32 offset, uint32 size, const void *data) {
	if (!size)
		return true;

	void *p;
	HRESULT hr = mpIB->Map(D3D10_MAP_WRITE, 0, &p);
	if (FAILED(hr)) {
		static_cast<VDTContextD3D10 *>(mpParent)->ProcessHRESULT(hr);
		return false;
	}

	bool success = true;
	if (mbDynamic)
		success = VDMemcpyGuarded(p, data, size);
	else
		memcpy(p, data, size);

	mpIB->Unmap();
	return success;
}

///////////////////////////////////////////////////////////////////////////////

VDTVertexFormatD3D10::VDTVertexFormatD3D10()
	: mpVF(NULL)
{
}

VDTVertexFormatD3D10::~VDTVertexFormatD3D10() {
	Shutdown();
}

bool VDTVertexFormatD3D10::Init(VDTContextD3D10 *parent, const VDTVertexElement *elements, uint32 count, VDTVertexProgramD3D10 *vp) {
	static const char *const kSemanticD3D10[]={
		"position",
		"blendweight",
		"blendindices",
		"normal",
		"texcoord",
		"tangent",
		"binormal",
		"color"
	};

	static const DXGI_FORMAT kFormatD3D10[]={
		DXGI_FORMAT_R32_FLOAT,
		DXGI_FORMAT_R32G32_FLOAT,
		DXGI_FORMAT_R32G32B32_FLOAT,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		DXGI_FORMAT_R8G8B8A8_UNORM
	};

	if (count >= 16) {
		VDASSERT(!"Too many vertex elements.");
		return false;
	}

	D3D10_INPUT_ELEMENT_DESC vxe[16];
	for(uint32 i=0; i<count; ++i) {
		D3D10_INPUT_ELEMENT_DESC& dst = vxe[i];
		const VDTVertexElement& src = elements[i];

		dst.SemanticName = kSemanticD3D10[src.mUsage];
		dst.SemanticIndex = src.mUsageIndex;
		dst.Format = kFormatD3D10[src.mType];
		dst.InputSlot = 0;
		dst.AlignedByteOffset = D3D10_APPEND_ALIGNED_ELEMENT;
		dst.InputSlotClass = D3D10_INPUT_PER_VERTEX_DATA;
		dst.InstanceDataStepRate = 0;
	}

	HRESULT hr = parent->GetDeviceD3D10()->CreateInputLayout(vxe, count, vp->mByteCode.data(), vp->mByteCode.size(), &mpVF);

	if (FAILED(hr))
		return false;

	parent->AddResource(this);
	return true;
}

void VDTVertexFormatD3D10::Shutdown() {
	if (mpVF) {
		if (mpParent)
			static_cast<VDTContextD3D10 *>(mpParent)->UnsetVertexFormat(this);

		mpVF->Release();
		mpVF = NULL;
	}

	VDTResourceD3D10::Shutdown();
}

bool VDTVertexFormatD3D10::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTVertexProgramD3D10::VDTVertexProgramD3D10()
	: mpVS(NULL)
{
}

VDTVertexProgramD3D10::~VDTVertexProgramD3D10() {
	Shutdown();
}

bool VDTVertexProgramD3D10::Init(VDTContextD3D10 *parent, VDTProgramFormat format, const void *data, uint32 size) {
	HRESULT hr = parent->GetDeviceD3D10()->CreateVertexShader((const DWORD *)data, size, &mpVS);

	if (FAILED(hr))
		return false;

	mByteCode.assign((const uint8 *)data, (const uint8 *)data + size);

	parent->AddResource(this);
	return true;
}

void VDTVertexProgramD3D10::Shutdown() {
	if (mpVS) {
		if (mpParent)
			static_cast<VDTContextD3D10 *>(mpParent)->UnsetVertexProgram(this);

		mpVS->Release();
		mpVS = NULL;
	}

	VDTResourceD3D10::Shutdown();
}

bool VDTVertexProgramD3D10::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTFragmentProgramD3D10::VDTFragmentProgramD3D10()
	: mpPS(NULL)
{
}

VDTFragmentProgramD3D10::~VDTFragmentProgramD3D10() {
	Shutdown();
}

bool VDTFragmentProgramD3D10::Init(VDTContextD3D10 *parent, VDTProgramFormat format, const void *data, uint32 size) {
	HRESULT hr = parent->GetDeviceD3D10()->CreatePixelShader((const DWORD *)data, size, &mpPS);

	if (FAILED(hr))
		return false;

	parent->AddResource(this);
	return true;
}

void VDTFragmentProgramD3D10::Shutdown() {
	if (mpPS) {
		if (mpParent)
			static_cast<VDTContextD3D10 *>(mpParent)->UnsetFragmentProgram(this);

		mpPS->Release();
		mpPS = NULL;
	}

	VDTResourceD3D10::Shutdown();
}

bool VDTFragmentProgramD3D10::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTBlendStateD3D10::VDTBlendStateD3D10()
	: mpBlendState(NULL)
{
}

VDTBlendStateD3D10::~VDTBlendStateD3D10() {
	Shutdown();
}

bool VDTBlendStateD3D10::Init(VDTContextD3D10 *parent, const VDTBlendStateDesc& desc) {
	mDesc = desc;

	static const D3D10_BLEND kD3DBlendStateLookup[]={
		D3D10_BLEND_ZERO,
		D3D10_BLEND_ONE,
		D3D10_BLEND_SRC_COLOR,
		D3D10_BLEND_INV_SRC_COLOR,
		D3D10_BLEND_SRC_ALPHA,
		D3D10_BLEND_INV_SRC_ALPHA,
		D3D10_BLEND_DEST_ALPHA,
		D3D10_BLEND_INV_DEST_ALPHA,
		D3D10_BLEND_DEST_COLOR,
		D3D10_BLEND_INV_DEST_COLOR
	};

	static const D3D10_BLEND_OP kD3DBlendOpLookup[]={
		D3D10_BLEND_OP_ADD,
		D3D10_BLEND_OP_SUBTRACT,
		D3D10_BLEND_OP_REV_SUBTRACT,
		D3D10_BLEND_OP_MIN,
		D3D10_BLEND_OP_MAX
	};

	D3D10_BLEND_DESC d3ddesc = {};
	d3ddesc.BlendEnable[0] = desc.mbEnable;
	d3ddesc.SrcBlend = kD3DBlendStateLookup[desc.mSrc];
	d3ddesc.DestBlend = kD3DBlendStateLookup[desc.mDst];
    d3ddesc.BlendOp = kD3DBlendOpLookup[desc.mOp];
	d3ddesc.SrcBlendAlpha = d3ddesc.SrcBlend;
    d3ddesc.DestBlendAlpha = d3ddesc.DestBlend;
    d3ddesc.BlendOpAlpha = d3ddesc.BlendOp;
	d3ddesc.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;

	HRESULT hr = parent->GetDeviceD3D10()->CreateBlendState(&d3ddesc, &mpBlendState);

	if (!hr) {
		parent->ProcessHRESULT(hr);
		return false;
	}

	parent->AddResource(this);
	return true;
}

void VDTBlendStateD3D10::Shutdown() {
	if (mpParent)
		static_cast<VDTContextD3D10 *>(mpParent)->UnsetBlendState(this);

	if (mpBlendState) {
		mpBlendState->Release();
		mpBlendState = NULL;
	}

	VDTResourceD3D10::Shutdown();
}

bool VDTBlendStateD3D10::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTRasterizerStateD3D10::VDTRasterizerStateD3D10()
	: mpRastState(NULL)
{
}

VDTRasterizerStateD3D10::~VDTRasterizerStateD3D10() {
	Shutdown();
}

bool VDTRasterizerStateD3D10::Init(VDTContextD3D10 *parent, const VDTRasterizerStateDesc& desc) {
	mDesc = desc;

	D3D10_RASTERIZER_DESC d3ddesc = {};
	d3ddesc.FillMode = D3D10_FILL_SOLID;
    d3ddesc.DepthBias = 0;
    d3ddesc.DepthBiasClamp = 0;
    d3ddesc.SlopeScaledDepthBias = 0;
    d3ddesc.DepthClipEnable = FALSE;
    d3ddesc.ScissorEnable = desc.mbEnableScissor;
    d3ddesc.MultisampleEnable = FALSE;
    d3ddesc.AntialiasedLineEnable = FALSE;

	switch(desc.mCullMode) {
		case kVDTCull_None:
			d3ddesc.CullMode = D3D10_CULL_NONE;
			break;

		case kVDTCull_Front:
			d3ddesc.CullMode = D3D10_CULL_FRONT;
			break;

		case kVDTCull_Back:
			d3ddesc.CullMode = D3D10_CULL_BACK;
			break;
	}

	d3ddesc.FrontCounterClockwise = desc.mbFrontIsCCW;

	HRESULT hr = parent->GetDeviceD3D10()->CreateRasterizerState(&d3ddesc, &mpRastState);
	if (FAILED(hr)) {
		parent->ProcessHRESULT(hr);
		return false;
	}

	parent->AddResource(this);
	return true;
}

void VDTRasterizerStateD3D10::Shutdown() {
	if (mpParent)
		static_cast<VDTContextD3D10 *>(mpParent)->UnsetRasterizerState(this);

	if (mpRastState) {
		mpRastState->Release();
		mpRastState = NULL;
	}

	VDTResourceD3D10::Shutdown();
}

bool VDTRasterizerStateD3D10::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

VDTSamplerStateD3D10::VDTSamplerStateD3D10()
	: mpSamplerState(NULL)
{
}

VDTSamplerStateD3D10::~VDTSamplerStateD3D10() {
	Shutdown();
}

bool VDTSamplerStateD3D10::Init(VDTContextD3D10 *parent, const VDTSamplerStateDesc& desc) {
	mDesc = desc;

	static const D3D10_FILTER kD3DFilterLookup[]={
		D3D10_FILTER_MIN_MAG_MIP_POINT,
		D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT,
		D3D10_FILTER_MIN_MAG_LINEAR_MIP_POINT,
		D3D10_FILTER_MIN_MAG_MIP_LINEAR,
		D3D10_FILTER_ANISOTROPIC
	};

	static const D3D10_TEXTURE_ADDRESS_MODE kD3DAddressLookup[]={
		D3D10_TEXTURE_ADDRESS_CLAMP,
		D3D10_TEXTURE_ADDRESS_WRAP
	};

	D3D10_SAMPLER_DESC d3ddesc = {};
	d3ddesc.Filter = kD3DFilterLookup[desc.mFilterMode];
	d3ddesc.AddressU = kD3DAddressLookup[desc.mAddressU];
    d3ddesc.AddressV = kD3DAddressLookup[desc.mAddressV];
    d3ddesc.AddressW = kD3DAddressLookup[desc.mAddressW];
    d3ddesc.MipLODBias = 0;
    d3ddesc.MaxAnisotropy = 0;
	d3ddesc.ComparisonFunc = D3D10_COMPARISON_ALWAYS;
    d3ddesc.MinLOD = 0;
    d3ddesc.MaxLOD = D3D10_FLOAT32_MAX;

	HRESULT hr = parent->GetDeviceD3D10()->CreateSamplerState(&d3ddesc, &mpSamplerState);
	if (FAILED(hr)) {
		parent->ProcessHRESULT(hr);
		return false;
	}

	parent->AddResource(this);
	return true;
}

void VDTSamplerStateD3D10::Shutdown() {
	if (mpParent)
		static_cast<VDTContextD3D10 *>(mpParent)->UnsetSamplerState(this);

	if (mpSamplerState) {
		mpSamplerState->Release();
		mpSamplerState = NULL;
	}

	VDTResourceD3D10::Shutdown();
}

bool VDTSamplerStateD3D10::Restore() {
	return true;
}

///////////////////////////////////////////////////////////////////////////////

struct VDTContextD3D10::PrivateData {
	HMODULE mhmodD3D10;

	VDTFenceManagerD3D10 mFenceManager;
};

VDTContextD3D10::VDTContextD3D10()
	: mRefCount(0)
	, mpData(NULL)
	, mpD3DHolder(NULL)
	, mpD3DDevice(NULL)
	, mpSwapChain(NULL)
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
	, mProfChan("Filter 3D accel")
{
	memset(mpCurrentSamplerStates, 0, sizeof mpCurrentSamplerStates);
	memset(mpCurrentTextures, 0, sizeof mpCurrentTextures);
}

VDTContextD3D10::~VDTContextD3D10() {
	Shutdown();
}

int VDTContextD3D10::AddRef() {
	return ++mRefCount;
}

int VDTContextD3D10::Release() {
	int rc = --mRefCount;

	if (!rc)
		delete this;

	return rc;
}

void *VDTContextD3D10::AsInterface(uint32 iid) {
	if (iid == IVDTProfiler::kTypeID)
		return static_cast<IVDTProfiler *>(this);

	return NULL;
}

bool VDTContextD3D10::Init(ID3D10Device *dev, IVDRefUnknown *pD3DHolder) {
	mpData = new PrivateData;
	mpData->mhmodD3D10 = VDLoadSystemLibraryW32("d3d10");

	if (mpData->mhmodD3D10) {
		mpBeginEvent = GetProcAddress(mpData->mhmodD3D10, "D3DPERF_BeginEvent");
		mpEndEvent = GetProcAddress(mpData->mhmodD3D10, "D3DPERF_EndEvent");
	}

	mpD3DHolder = pD3DHolder;
	if (mpD3DHolder)
		mpD3DHolder->AddRef();

	mpD3DDevice = dev;
	mpD3DDevice->AddRef();

	mpData->mFenceManager.Init(mpD3DDevice);

	mpDefaultBS = new VDTBlendStateD3D10;
	mpDefaultBS->AddRef();
	mpDefaultBS->Init(this, VDTBlendStateDesc());

	mpDefaultRS = new VDTRasterizerStateD3D10;
	mpDefaultRS->AddRef();
	mpDefaultRS->Init(this, VDTRasterizerStateDesc());

	mpDefaultSS = new VDTSamplerStateD3D10;
	mpDefaultSS->AddRef();
	mpDefaultSS->Init(this, VDTSamplerStateDesc());

	mpDefaultRT = new VDTSurfaceD3D10;
	mpDefaultRT->AddRef();

	mpCurrentBS = mpDefaultBS;
	mpCurrentRS = mpDefaultRS;
	mpCurrentRT = mpDefaultRT;

	for(int i=0; i<16; ++i)
		mpCurrentSamplerStates[i] = mpDefaultSS;

	if (!ConnectSurfaces()) {
		Shutdown();
		return false;
	}

	return true;
}

void VDTContextD3D10::Shutdown() {
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
		if (mpData->mhmodD3D10)
			FreeLibrary(mpData->mhmodD3D10);

		delete mpData;
		mpData = NULL;
	}

	mpBeginEvent = NULL;
	mpEndEvent = NULL;
}

bool VDTContextD3D10::CreateReadbackBuffer(uint32 width, uint32 height, uint32 format, IVDTReadbackBuffer **ppbuffer) {
	vdrefptr<VDTReadbackBufferD3D10> surf(new VDTReadbackBufferD3D10);

	if (!surf->Init(this, width, height, format))
		return false;

	*ppbuffer = surf.release();
	return true;
}

bool VDTContextD3D10::CreateSurface(uint32 width, uint32 height, uint32 format, VDTUsage usage, IVDTSurface **ppsurface) {
	vdrefptr<VDTSurfaceD3D10> surf(new VDTSurfaceD3D10);

	if (!surf->Init(this, width, height, format, usage))
		return false;

	*ppsurface = surf.release();
	return true;
}

bool VDTContextD3D10::CreateTexture2D(uint32 width, uint32 height, uint32 format, uint32 mipcount, VDTUsage usage, const VDTInitData2D *initData, IVDTTexture2D **pptex) {
	vdrefptr<VDTTexture2DD3D10> tex(new VDTTexture2DD3D10);

	if (!tex->Init(this, width, height, format, mipcount, usage, initData))
		return false;

	*pptex = tex.release();
	return true;
}

bool VDTContextD3D10::CreateVertexProgram(VDTProgramFormat format, const void *data, uint32 length, IVDTVertexProgram **program) {
	vdrefptr<VDTVertexProgramD3D10> vp(new VDTVertexProgramD3D10);

	if (!vp->Init(this, format, data, length))
		return false;

	*program = vp.release();
	return true;
}

bool VDTContextD3D10::CreateFragmentProgram(VDTProgramFormat format, const void *data, uint32 length, IVDTFragmentProgram **program) {
	vdrefptr<VDTFragmentProgramD3D10> fp(new VDTFragmentProgramD3D10);

	if (!fp->Init(this, format, data, length))
		return false;

	*program = fp.release();
	return true;
}

bool VDTContextD3D10::CreateVertexFormat(const VDTVertexElement *elements, uint32 count, IVDTVertexProgram *vp, IVDTVertexFormat **format) {
	vdrefptr<VDTVertexFormatD3D10> vf(new VDTVertexFormatD3D10);

	if (!vf->Init(this, elements, count, static_cast<VDTVertexProgramD3D10 *>(vp)))
		return false;

	*format = vf.release();
	return true;
}

bool VDTContextD3D10::CreateVertexBuffer(uint32 size, bool dynamic, const void *initData, IVDTVertexBuffer **ppbuffer) {
	vdrefptr<VDTVertexBufferD3D10> vb(new VDTVertexBufferD3D10);

	if (!vb->Init(this, size, dynamic, initData))
		return false;

	*ppbuffer = vb.release();
	return true;
}

bool VDTContextD3D10::CreateIndexBuffer(uint32 size, bool index32, bool dynamic, const void *initData, IVDTIndexBuffer **ppbuffer) {
	vdrefptr<VDTIndexBufferD3D10> ib(new VDTIndexBufferD3D10);

	if (!ib->Init(this, size, index32, dynamic, initData))
		return false;

	*ppbuffer = ib.release();
	return true;
}

bool VDTContextD3D10::CreateBlendState(const VDTBlendStateDesc& desc, IVDTBlendState **state) {
	vdrefptr<VDTBlendStateD3D10> bs(new VDTBlendStateD3D10);

	if (!bs->Init(this, desc))
		return false;

	*state = bs.release();
	return true;
}

bool VDTContextD3D10::CreateRasterizerState(const VDTRasterizerStateDesc& desc, IVDTRasterizerState **state) {
	vdrefptr<VDTRasterizerStateD3D10> rs(new VDTRasterizerStateD3D10);

	if (!rs->Init(this, desc))
		return false;

	*state = rs.release();
	return true;
}

bool VDTContextD3D10::CreateSamplerState(const VDTSamplerStateDesc& desc, IVDTSamplerState **state) {
	vdrefptr<VDTSamplerStateD3D10> ss(new VDTSamplerStateD3D10);

	if (!ss->Init(this, desc))
		return false;

	*state = ss.release();
	return true;
}

IVDTSurface *VDTContextD3D10::GetRenderTarget(uint32 index) const {
	return index ? NULL : mpCurrentRT;
}

void VDTContextD3D10::SetVertexFormat(IVDTVertexFormat *format) {
	if (format == mpCurrentVF)
		return;

	mpCurrentVF = static_cast<VDTVertexFormatD3D10 *>(format);

	mpD3DDevice->IASetInputLayout(mpCurrentVF ? mpCurrentVF->mpVF : NULL);
}

void VDTContextD3D10::SetVertexProgram(IVDTVertexProgram *program) {
	if (program == mpCurrentVP)
		return;

	mpCurrentVP = static_cast<VDTVertexProgramD3D10 *>(program);

	mpD3DDevice->VSSetShader(mpCurrentVP ? mpCurrentVP->mpVS : NULL);
}

void VDTContextD3D10::SetFragmentProgram(IVDTFragmentProgram *program) {
	if (program == mpCurrentFP)
		return;

	mpCurrentFP = static_cast<VDTFragmentProgramD3D10 *>(program);

	mpD3DDevice->PSSetShader(mpCurrentFP ? mpCurrentFP->mpPS : NULL);
}

void VDTContextD3D10::SetVertexStream(uint32 index, IVDTVertexBuffer *buffer, uint32 offset, uint32 stride) {
	VDASSERT(index == 0);

	if (buffer == mpCurrentVB && offset == mCurrentVBOffset && offset == mCurrentVBStride)
		return;

	mpCurrentVB = static_cast<VDTVertexBufferD3D10 *>(buffer);
	mCurrentVBOffset = offset;
	mCurrentVBStride = stride;

	ID3D10Buffer *pBuffer = mpCurrentVB ? mpCurrentVB->mpVB : NULL;
	UINT d3doffset = offset;
	UINT d3dstride = stride;
	mpD3DDevice->IASetVertexBuffers(index, 1, &pBuffer, &d3doffset, &d3dstride);
}

void VDTContextD3D10::SetIndexStream(IVDTIndexBuffer *buffer) {
	if (buffer == mpCurrentIB)
		return;

	mpCurrentIB = static_cast<VDTIndexBufferD3D10 *>(buffer);

	mpD3DDevice->IASetIndexBuffer(mpCurrentIB ? mpCurrentIB->mpIB : NULL, mpCurrentIB->mbIndex32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT, 0);
}

void VDTContextD3D10::SetRenderTarget(uint32 index, IVDTSurface *surface) {
	VDASSERT(index == 0);

	if (!index && !surface)
		surface = mpDefaultRT;

	if (mpCurrentRT == surface)
		return;

	mpCurrentRT = static_cast<VDTSurfaceD3D10 *>(surface);

	ID3D10RenderTargetView *rtv = NULL;
	if (mpCurrentRT)
		rtv = mpCurrentRT->mpRTView;

	mpD3DDevice->OMSetRenderTargets(1, &rtv, NULL);
}

void VDTContextD3D10::SetBlendState(IVDTBlendState *state) {
	if (!state)
		state = mpDefaultBS;

	if (mpCurrentBS == state)
		return;

	mpCurrentBS = static_cast<VDTBlendStateD3D10 *>(state);

	float blendfactor[4] = {0};
	mpD3DDevice->OMSetBlendState(mpCurrentBS->mpBlendState, blendfactor, 0xFFFFFFFF);
}

void VDTContextD3D10::SetRasterizerState(IVDTRasterizerState *state) {
	if (!state)
		state = mpDefaultRS;

	if (mpCurrentRS == state)
		return;

	mpCurrentRS = static_cast<VDTRasterizerStateD3D10 *>(state);

	mpD3DDevice->RSSetState(mpCurrentRS->mpRastState);
}

void VDTContextD3D10::SetSamplerStates(uint32 baseIndex, uint32 count, IVDTSamplerState *const *states) {
	VDASSERT(baseIndex <= 16 && 16 - baseIndex >= count);

	for(uint32 i=0; i<count; ++i) {
		uint32 stage = baseIndex + i;
		VDTSamplerStateD3D10 *state = static_cast<VDTSamplerStateD3D10 *>(states[stage]);
		if (!state)
			state = mpDefaultSS;

		if (mpCurrentSamplerStates[stage] != state) {
			mpCurrentSamplerStates[stage] = state;
			
			ID3D10SamplerState *d3dss = state->mpSamplerState;
			mpD3DDevice->PSSetSamplers(stage, 1, &d3dss);
		}
	}
}

void VDTContextD3D10::SetTextures(uint32 baseIndex, uint32 count, IVDTTexture *const *textures) {
	VDASSERT(baseIndex <= 16 && 16 - baseIndex >= count);

	for(uint32 i=0; i<count; ++i) {
		uint32 stage = baseIndex + i;
		IVDTTexture *tex = textures[i];

		if (mpCurrentTextures[stage] != tex) {
			mpCurrentTextures[stage] = tex;

			ID3D10ShaderResourceView *pSRV = (ID3D10ShaderResourceView *)tex->AsInterface(VDTTextureD3D10::kTypeD3DShaderResView);
			mpD3DDevice->PSSetShaderResources(stage, 1, &pSRV);
		}
	}
}

void VDTContextD3D10::SetVertexProgramConstF(uint32 baseIndex, uint32 count, const float *data) {
	void *dst;
	HRESULT hr = mpVSConstBuffer->Map(D3D10_MAP_WRITE, 0, &dst);

	if (FAILED(hr))
		ProcessHRESULT(hr);
	else {
		memcpy((char *)dst + baseIndex*16, data, count*16);
		mpVSConstBuffer->Unmap();
	}
}

void VDTContextD3D10::SetFragmentProgramConstF(uint32 baseIndex, uint32 count, const float *data) {
	void *dst;
	HRESULT hr = mpPSConstBuffer->Map(D3D10_MAP_WRITE, 0, &dst);

	if (FAILED(hr))
		ProcessHRESULT(hr);
	else {
		memcpy((char *)dst + baseIndex*16, data, count*16);
		mpPSConstBuffer->Unmap();
	}
}

void VDTContextD3D10::Clear(VDTClearFlags clearFlags, uint32 color, float depth, uint32 stencil) {
	DWORD flags = 0;

	if (clearFlags & kVDTClear_Color) {
		float color32[4];

		color32[0] = (int)(color >> 16) / 255.0f;
		color32[1] = (int)(color >>  8) / 255.0f;
		color32[2] = (int)(color >>  0) / 255.0f;
		color32[3] = (int)(color >> 24) / 255.0f;

		mpD3DDevice->ClearRenderTargetView(mpCurrentRT->mpRTView, color32);
	}
}

namespace {
	const D3D10_PRIMITIVE_TOPOLOGY kPTLookup[]={
		D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
		D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
	};
}

void VDTContextD3D10::DrawPrimitive(VDTPrimitiveType type, uint32 startVertex, uint32 primitiveCount) {
	mpD3DDevice->IASetPrimitiveTopology(kPTLookup[type]);
	mpD3DDevice->Draw(startVertex, primitiveCount);
}

void VDTContextD3D10::DrawIndexedPrimitive(VDTPrimitiveType type, uint32 baseVertexIndex, uint32 minVertex, uint32 vertexCount, uint32 startIndex, uint32 primitiveCount) {
	mpD3DDevice->IASetPrimitiveTopology(kPTLookup[type]);

	uint32 indexCount = 0;
	switch(type) {
		case kVDTPT_Triangles:
			indexCount = primitiveCount * 3;
			break;

		case kVDTPT_TriangleStrip:
			indexCount = primitiveCount + 2;
			break;
	}

	mpD3DDevice->DrawIndexed(indexCount, startIndex, baseVertexIndex);
}

uint32 VDTContextD3D10::InsertFence() {
	return mpData->mFenceManager.InsertFence();
}

bool VDTContextD3D10::CheckFence(uint32 id) {
	return mpData->mFenceManager.CheckFence(id);
}

bool VDTContextD3D10::RecoverDevice() {
	return true;
}

bool VDTContextD3D10::OpenScene() {
	return true;
}

bool VDTContextD3D10::CloseScene() {
	return true;
}

uint32 VDTContextD3D10::GetDeviceLossCounter() const {
	return 0;
}

void VDTContextD3D10::Present() {
	HRESULT hr = mpSwapChain->Present(0, DXGI_PRESENT_DO_NOT_SEQUENCE);

	if (FAILED(hr))
		ProcessHRESULT(hr);
}

void VDTContextD3D10::BeginScope(uint32 color, const char *message) {
	mProfChan.Begin(color, message);
}

void VDTContextD3D10::EndScope() {
	mProfChan.End();
}

VDRTProfileChannel *VDTContextD3D10::GetProfileChannel() {
	return &mProfChan;
}

void VDTContextD3D10::UnsetVertexFormat(IVDTVertexFormat *format) {
	if (mpCurrentVF == format)
		SetVertexFormat(NULL);
}

void VDTContextD3D10::UnsetVertexProgram(IVDTVertexProgram *program) {
	if (mpCurrentVP == program)
		SetVertexProgram(NULL);
}

void VDTContextD3D10::UnsetFragmentProgram(IVDTFragmentProgram *program) {
	if (mpCurrentFP == program)
		SetFragmentProgram(NULL);
}

void VDTContextD3D10::UnsetVertexBuffer(IVDTVertexBuffer *buffer) {
	if (mpCurrentVB == buffer)
		SetVertexStream(0, NULL, 0, 0);
}

void VDTContextD3D10::UnsetIndexBuffer(IVDTIndexBuffer *buffer) {
	if (mpCurrentIB == buffer)
		SetIndexStream(NULL);
}

void VDTContextD3D10::UnsetRenderTarget(IVDTSurface *surface) {
	if (mpCurrentRT == surface)
		SetRenderTarget(0, NULL);
}

void VDTContextD3D10::UnsetBlendState(IVDTBlendState *state) {
	if (mpCurrentBS == state && state != mpDefaultBS)
		SetBlendState(NULL);
}

void VDTContextD3D10::UnsetRasterizerState(IVDTRasterizerState *state) {
	if (mpCurrentRS == state && state != mpDefaultRS)
		SetRasterizerState(NULL);
}

void VDTContextD3D10::UnsetSamplerState(IVDTSamplerState *state) {
	if (state == mpDefaultSS)
		return;

	for(int i=0; i<16; ++i) {
		if (mpCurrentSamplerStates[i] == state) {
			IVDTSamplerState *ssnull = NULL;
			SetSamplerStates(i, 1, &ssnull);
		}
	}
}

void VDTContextD3D10::UnsetTexture(IVDTTexture *tex) {
	for(int i=0; i<16; ++i) {
		if (mpCurrentTextures[i] == tex) {
			IVDTTexture *tex = NULL;
			SetTextures(i, 1, &tex);
		}
	}
}

void VDTContextD3D10::ProcessHRESULT(uint32 hr) {
}

bool VDTContextD3D10::ConnectSurfaces() {
	bool success = true;

	mpDefaultRT->Shutdown();

	if (mpSwapChain) {
		ID3D10Texture2D *tex;
		HRESULT hr = mpSwapChain->GetBuffer(0, IID_ID3D10Texture2D, (void **)&tex);
		if (FAILED(hr)) {
			ProcessHRESULT(hr);
			return false;
		}

		success = mpDefaultRT->Init(this, tex, 0, true);
		tex->Release();
	}

	return success;
}

///////////////////////////////////////////////////////////////////////////////

class VDD3D10Holder : public vdrefcounted<IVDRefUnknown> {
public:
	VDD3D10Holder();
	~VDD3D10Holder();

	void *AsInterface(uint32 iid);

	typedef HRESULT (APIENTRY *CreateDeviceFn)(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, ID3D10Device **ppDevice);

	CreateDeviceFn GetCreateDeviceFn() const { return mpCreateDeviceFn; }

	bool Init();
	void Shutdown();

protected:
	HMODULE mhmodD3D10;
	CreateDeviceFn mpCreateDeviceFn;
};

VDD3D10Holder::VDD3D10Holder()
	: mhmodD3D10(NULL)
	, mpCreateDeviceFn(NULL)
{
}

VDD3D10Holder::~VDD3D10Holder() {
	Shutdown();
}

void *VDD3D10Holder::AsInterface(uint32 iid) {
	return NULL;
}

bool VDD3D10Holder::Init() {
	if (!mhmodD3D10) {
		mhmodD3D10 = VDLoadSystemLibraryW32("d3d10");

		if (!mhmodD3D10) {
			Shutdown();
			return false;
		}
	}

	if (!mpCreateDeviceFn) {
		mpCreateDeviceFn = (CreateDeviceFn)GetProcAddress(mhmodD3D10, "D3D10CreateDevice");
		if (!mpCreateDeviceFn) {
			Shutdown();
			return false;
		}
	}

	return true;
}

void VDD3D10Holder::Shutdown() {
	mpCreateDeviceFn = NULL;

	if (mhmodD3D10) {
		FreeLibrary(mhmodD3D10);
		mhmodD3D10 = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

bool VDTCreateContextD3D10(IVDTContext **ppctx) {
	vdrefptr<VDD3D10Holder> holder(new VDD3D10Holder);

	if (!holder->Init())
		return false;

	D3D10_DRIVER_TYPE driverType = D3D10_DRIVER_TYPE_HARDWARE;

	vdrefptr<ID3D10Device> dev;
	const UINT flags = D3D10_CREATE_DEVICE_SINGLETHREADED;

#ifdef DEBUG
	flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

	HRESULT hr = holder->GetCreateDeviceFn()(NULL, driverType, NULL, flags, D3D10_SDK_VERSION, ~dev);
	if (FAILED(hr))
		return false;

	vdrefptr<VDTContextD3D10> ctx(new VDTContextD3D10);
	if (!ctx->Init(dev, holder))
		return false;

	*ppctx = ctx.release();
	return true;
}

bool VDTCreateContextD3D10(ID3D10Device *dev, IVDTContext **ppctx) {
	vdrefptr<VDTContextD3D10> ctx(new VDTContextD3D10);

	if (!ctx->Init(dev, NULL))
		return false;

	*ppctx = ctx.release();
	return true;
}
