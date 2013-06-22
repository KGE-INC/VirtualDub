#ifndef f_D3D10_CONTEXT_D3D10_H
#define f_D3D10_CONTEXT_D3D10_H

#include <vd2/system/profile.h>
#include <vd2/system/vdstl.h>
#include <vd2/Tessa/Context.h>

struct ID3D10Device;
struct ID3D10PixelShader;
struct ID3D10VertexShader;
struct ID3D10InputLayout;

class VDTContextD3D10;
class VDTResourceManagerD3D10;

///////////////////////////////////////////////////////////////////////////////
class VDTResourceD3D10 : public vdlist_node {
public:
	VDTResourceD3D10();
	virtual ~VDTResourceD3D10();

	virtual void Shutdown();

protected:
	friend class VDTResourceManagerD3D10;

	VDTResourceManagerD3D10 *mpParent;
};

class VDTResourceManagerD3D10 {
public:
	void AddResource(VDTResourceD3D10 *res);
	void RemoveResource(VDTResourceD3D10 *res);

	void ShutdownAllResources();

protected:
	typedef vdlist<VDTResourceD3D10> Resources;
	Resources mResources;
};

///////////////////////////////////////////////////////////////////////////////
class VDTReadbackBufferD3D10 : public vdrefcounted<IVDTReadbackBuffer>, VDTResourceD3D10 {
public:
	VDTReadbackBufferD3D10();
	~VDTReadbackBufferD3D10();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D10 *parent, uint32 width, uint32 height, uint32 format);
	void Shutdown();

	bool Restore();
	bool Lock(VDTLockData2D& lockData);
	void Unlock();

protected:
	friend class VDTContextD3D10;
	friend class VDTSurfaceD3D10;
	friend class VDTTexture2DD3D10;

	ID3D10Texture2D *mpSurface;
};

///////////////////////////////////////////////////////////////////////////////
class VDTSurfaceD3D10 : public vdrefcounted<IVDTSurface>, VDTResourceD3D10 {
public:
	VDTSurfaceD3D10();
	~VDTSurfaceD3D10();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D10 *parent, uint32 width, uint32 height, uint32 format, VDTUsage usage);
	bool Init(VDTContextD3D10 *parent, ID3D10Texture2D *tex, uint32 mipLevel, bool rt);
	void Shutdown();

	bool Restore();
	bool Readback(IVDTReadbackBuffer *target);
	void Load(uint32 dx, uint32 dy, const VDTInitData2D& srcData, uint32 bpr, uint32 h);
	void Copy(uint32 dx, uint32 dy, IVDTSurface *src, uint32 sx, uint32 sy, uint32 w, uint32 h);
	void GetDesc(VDTSurfaceDesc& desc);
	bool Lock(const vdrect32 *r, VDTLockData2D& lockData);
	void Unlock();

protected:
	friend class VDTContextD3D10;

	ID3D10Texture2D *mpSurface;
	ID3D10RenderTargetView *mpRTView;
	VDTSurfaceDesc mDesc;
};

///////////////////////////////////////////////////////////////////////////////

class VDTTextureD3D10 : protected VDTResourceD3D10 {
public:
	enum { kTypeD3DShaderResView = 'dsrv' };

protected:
	friend class VDTContextD3D10;
};

///////////////////////////////////////////////////////////////////////////////
class VDTTexture2DD3D10 : public VDTTextureD3D10, public vdrefcounted<IVDTTexture2D> {
public:
	VDTTexture2DD3D10();
	~VDTTexture2DD3D10();

	void *AsInterface(uint32 id);

	bool Init(VDTContextD3D10 *parent, uint32 width, uint32 height, uint32 format, uint32 mipcount, VDTUsage usage, const VDTInitData2D *initData);
	void Shutdown();

	bool Restore();
	void GetDesc(VDTTextureDesc& desc);
	IVDTSurface *GetLevelSurface(uint32 level);
	void Load(uint32 mip, uint32 x, uint32 y, const VDTInitData2D& srcData, uint32 w, uint32 h);
	bool Lock(uint32 mip, const vdrect32 *r, VDTLockData2D& lockData);
	void Unlock(uint32 mip);

protected:
	ID3D10Texture2D *mpTexture;
	ID3D10ShaderResourceView *mpShaderResView;
	uint32	mWidth;
	uint32	mHeight;
	uint32	mMipCount;
	VDTUsage mUsage;

	typedef vdfastvector<VDTSurfaceD3D10 *> Mipmaps;
	Mipmaps mMipmaps;
};

///////////////////////////////////////////////////////////////////////////////
class VDTVertexBufferD3D10 : public vdrefcounted<IVDTVertexBuffer>, VDTResourceD3D10 {
public:
	VDTVertexBufferD3D10();
	~VDTVertexBufferD3D10();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D10 *parent, uint32 size, bool dynamic, const void *initData);
	void Shutdown();

	bool Restore();
	bool Load(uint32 offset, uint32 size, const void *data);

protected:
	friend class VDTContextD3D10;

	ID3D10Buffer *mpVB;
	uint32 mByteSize;
	bool mbDynamic;
};

///////////////////////////////////////////////////////////////////////////////
class VDTIndexBufferD3D10 : public vdrefcounted<IVDTIndexBuffer>, VDTResourceD3D10 {
public:
	VDTIndexBufferD3D10();
	~VDTIndexBufferD3D10();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D10 *parent, uint32 size, bool index32, bool dynamic, const void *initData);
	void Shutdown();

	bool Restore();
	bool Load(uint32 offset, uint32 size, const void *data);

protected:
	friend class VDTContextD3D10;

	ID3D10Buffer *mpIB;
	uint32 mByteSize;
	bool mbDynamic;
	bool mbIndex32;
};

///////////////////////////////////////////////////////////////////////////////
class VDTVertexProgramD3D10;

class VDTVertexFormatD3D10 : public vdrefcounted<IVDTVertexFormat>, VDTResourceD3D10 {
public:
	VDTVertexFormatD3D10();
	~VDTVertexFormatD3D10();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D10 *parent, const VDTVertexElement *elements, uint32 count, VDTVertexProgramD3D10 *vp);
	void Shutdown();

	bool Restore();

protected:
	friend class VDTContextD3D10;

	ID3D10InputLayout *mpVF;
};

///////////////////////////////////////////////////////////////////////////////
class VDTVertexProgramD3D10 : public vdrefcounted<IVDTVertexProgram>, VDTResourceD3D10 {
public:
	VDTVertexProgramD3D10();
	~VDTVertexProgramD3D10();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D10 *parent, VDTProgramFormat format, const void *data, uint32 size);
	void Shutdown();

	bool Restore();

protected:
	friend class VDTContextD3D10;
	friend class VDTVertexFormatD3D10;

	ID3D10VertexShader *mpVS;
	vdfastvector<uint8> mByteCode;
};

///////////////////////////////////////////////////////////////////////////////
class VDTFragmentProgramD3D10 : public vdrefcounted<IVDTFragmentProgram>, VDTResourceD3D10 {
public:
	VDTFragmentProgramD3D10();
	~VDTFragmentProgramD3D10();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D10 *parent, VDTProgramFormat format, const void *data, uint32 size);
	void Shutdown();

	bool Restore();

protected:
	friend class VDTContextD3D10;

	ID3D10PixelShader *mpPS;
};

///////////////////////////////////////////////////////////////////////////////
class VDTBlendStateD3D10 : public vdrefcounted<IVDTBlendState>, VDTResourceD3D10 {
public:
	VDTBlendStateD3D10();
	~VDTBlendStateD3D10();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D10 *parent, const VDTBlendStateDesc& desc);
	void Shutdown();

	bool Restore();

protected:
	friend class VDTContextD3D10;

	ID3D10BlendState *mpBlendState;
	VDTBlendStateDesc mDesc;
};

///////////////////////////////////////////////////////////////////////////////
class VDTRasterizerStateD3D10 : public vdrefcounted<IVDTRasterizerState>, VDTResourceD3D10 {
public:
	VDTRasterizerStateD3D10();
	~VDTRasterizerStateD3D10();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D10 *parent, const VDTRasterizerStateDesc& desc);
	void Shutdown();

	bool Restore();

protected:
	friend class VDTContextD3D10;

	ID3D10RasterizerState *mpRastState;
	VDTRasterizerStateDesc mDesc;
};

///////////////////////////////////////////////////////////////////////////////
class VDTSamplerStateD3D10 : public vdrefcounted<IVDTSamplerState>, VDTResourceD3D10 {
public:
	VDTSamplerStateD3D10();
	~VDTSamplerStateD3D10();

	void *AsInterface(uint32 iid) { return NULL; }

	bool Init(VDTContextD3D10 *parent, const VDTSamplerStateDesc& desc);
	void Shutdown();

	bool Restore();

protected:
	friend class VDTContextD3D10;

	ID3D10SamplerState *mpSamplerState;
	VDTSamplerStateDesc mDesc;
};

///////////////////////////////////////////////////////////////////////////////
class VDTContextD3D10 : public IVDTContext, public IVDTProfiler, public VDTResourceManagerD3D10 {
public:
	VDTContextD3D10();
	~VDTContextD3D10();

	int AddRef();
	int Release();
	void *AsInterface(uint32 id);

	bool Init(ID3D10Device *dev, IVDRefUnknown *dllHolder);
	void Shutdown();

	ID3D10Device *GetDeviceD3D10() const { return mpD3DDevice; }

	bool CreateReadbackBuffer(uint32 width, uint32 height, uint32 format, IVDTReadbackBuffer **buffer);
	bool CreateSurface(uint32 width, uint32 height, uint32 format, VDTUsage usage, IVDTSurface **surface);
	bool CreateTexture2D(uint32 width, uint32 height, uint32 format, uint32 mipcount, VDTUsage usage, const VDTInitData2D *initData, IVDTTexture2D **tex);
	bool CreateVertexProgram(VDTProgramFormat format, const void *data, uint32 length, IVDTVertexProgram **tex);
	bool CreateFragmentProgram(VDTProgramFormat format, const void *data, uint32 length, IVDTFragmentProgram **tex);
	bool CreateVertexFormat(const VDTVertexElement *elements, uint32 count, IVDTVertexProgram *vp, IVDTVertexFormat **format);
	bool CreateVertexBuffer(uint32 size, bool dynamic, const void *initData, IVDTVertexBuffer **buffer);
	bool CreateIndexBuffer(uint32 size, bool index32, bool dynamic, const void *initData, IVDTIndexBuffer **buffer);

	bool CreateBlendState(const VDTBlendStateDesc& desc, IVDTBlendState **state);
	bool CreateRasterizerState(const VDTRasterizerStateDesc& desc, IVDTRasterizerState **state);
	bool CreateSamplerState(const VDTSamplerStateDesc& desc, IVDTSamplerState **state);

	IVDTSurface *GetRenderTarget(uint32 index) const;

	void SetVertexFormat(IVDTVertexFormat *format);
	void SetVertexProgram(IVDTVertexProgram *program);
	void SetFragmentProgram(IVDTFragmentProgram *program);
	void SetVertexStream(uint32 index, IVDTVertexBuffer *buffer, uint32 offset, uint32 stride);
	void SetIndexStream(IVDTIndexBuffer *buffer);
	void SetRenderTarget(uint32 index, IVDTSurface *surface);

	void SetBlendState(IVDTBlendState *state);
	void SetRasterizerState(IVDTRasterizerState *state);
	void SetSamplerStates(uint32 baseIndex, uint32 count, IVDTSamplerState *const *states);
	void SetTextures(uint32 baseIndex, uint32 count, IVDTTexture *const *textures);

	void SetVertexProgramConstF(uint32 baseIndex, uint32 count, const float *data);
	void SetFragmentProgramConstF(uint32 baseIndex, uint32 count, const float *data);

	void Clear(VDTClearFlags clearFlags, uint32 color, float depth, uint32 stencil);
	void DrawPrimitive(VDTPrimitiveType type, uint32 startVertex, uint32 primitiveCount);
	void DrawIndexedPrimitive(VDTPrimitiveType type, uint32 baseVertexIndex, uint32 minVertex, uint32 vertexCount, uint32 startIndex, uint32 primitiveCount);

	uint32 InsertFence();
	bool CheckFence(uint32 id);

	bool RecoverDevice();
	bool OpenScene();
	bool CloseScene();
	bool IsDeviceLost() const { return false; }
	uint32 GetDeviceLossCounter() const;
	void Present();

	void SetGpuPriority(int priority) {}

public:
	void BeginScope(uint32 color, const char *message);
	void EndScope();
	VDRTProfileChannel *GetProfileChannel();

public:
	void UnsetVertexFormat(IVDTVertexFormat *format);
	void UnsetVertexProgram(IVDTVertexProgram *program);
	void UnsetFragmentProgram(IVDTFragmentProgram *program);
	void UnsetVertexBuffer(IVDTVertexBuffer *buffer);
	void UnsetIndexBuffer(IVDTIndexBuffer *buffer);
	void UnsetRenderTarget(IVDTSurface *surface);

	void UnsetBlendState(IVDTBlendState *state);
	void UnsetRasterizerState(IVDTRasterizerState *state);
	void UnsetSamplerState(IVDTSamplerState *state);
	void UnsetTexture(IVDTTexture *tex);

	void ProcessHRESULT(uint32 hr);

protected:
	bool ConnectSurfaces();
	void UpdateRenderStates(const uint32 *ids, uint32 count, uint32 *shadow, const uint32 *values);

	struct PrivateData;

	VDAtomicInt	mRefCount;
	PrivateData *mpData;

	IVDRefUnknown *mpD3DHolder;
	ID3D10Device *mpD3DDevice;
	IDXGISwapChain *mpSwapChain;
	ID3D10Buffer *mpVSConstBuffer;
	ID3D10Buffer *mpPSConstBuffer;

	VDTSurfaceD3D10 *mpCurrentRT;
	VDTVertexBufferD3D10 *mpCurrentVB;
	uint32 mCurrentVBOffset;
	uint32 mCurrentVBStride;
	VDTIndexBufferD3D10 *mpCurrentIB;
	VDTVertexProgramD3D10 *mpCurrentVP;
	VDTFragmentProgramD3D10 *mpCurrentFP;
	VDTVertexFormatD3D10 *mpCurrentVF;

	VDTBlendStateD3D10 *mpCurrentBS;
	VDTRasterizerStateD3D10 *mpCurrentRS;

	VDTSurfaceD3D10 *mpDefaultRT;
	VDTBlendStateD3D10 *mpDefaultBS;
	VDTRasterizerStateD3D10 *mpDefaultRS;
	VDTSamplerStateD3D10 *mpDefaultSS;

	VDTSamplerStateD3D10 *mpCurrentSamplerStates[16];
	IVDTTexture *mpCurrentTextures[16];

	void	*mpBeginEvent;
	void	*mpEndEvent;
	VDRTProfileChannel	mProfChan;
};

bool VDTCreateContextD3D10(IVDTContext **ppctx);
bool VDTCreateContextD3D10(ID3D10Device *dev, IVDTContext **ppctx);

#endif	// f_D3D10_CONTEXT_D3D10_H
