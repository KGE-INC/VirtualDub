//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2006 Avery Lee
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#ifndef f_VD2_RIZA_DIRECT3D_H
#define f_VD2_RIZA_DIRECT3D_H

#include <windows.h>
#include <d3d9.h>

#include <vd2/system/vdstl.h>
#include <vd2/system/refcount.h>

///////////////////////////////////////////////////////////////////////////////

struct VDPixmap;
class VDD3D9Texture;
class VDD3D9Manager;

///////////////////////////////////////////////////////////////////////////////

namespace nsVDD3D9 {
	enum {
		kVertexBufferSize	= 4096,						// in vertices
		kIndexBufferSize	= kVertexBufferSize*3/2		// in indices
	};

	struct Vertex {
		float x, y, z;
		uint32 diffuse;
		float u0, v0, u1, v1;

		Vertex(float x_, float y_, uint32 c_, float u0_, float v0_, float u1_=0.f, float v1_=0.f) : x(x_), y(y_), z(0), diffuse(c_), u0(u0_), v0(v0_), u1(u1_), v1(v1_) {}

		inline void SetFF2(float x_, float y_, uint32 c_, float u0_, float v0_, float u1_, float v1_) {
			x = x_;
			y = y_;
			diffuse = c_;
			u0 = u0_;
			v0 = v0_;
			u1 = u1_;
			v1 = v1_;
		}
	};
};

class VDD3D9Client : public vdlist_node {
public:
	virtual void OnPreDeviceReset() = 0;
	virtual void OnPostDeviceReset() = 0;
};

class IVDD3D9Texture : public IVDRefCount {
public:
	virtual int GetWidth() = 0;
	virtual int GetHeight() = 0;

	virtual void SetD3DTexture(IDirect3DTexture9 *pTexture) = 0;
	virtual IDirect3DTexture9 *GetD3DTexture() = 0;
};

class IVDD3D9TextureGenerator : public IVDRefCount {
public:
	virtual bool GenerateTexture(VDD3D9Manager *pManager, IVDD3D9Texture *pTexture) = 0;
};

class VDD3DPresentHistory {
public:
	float mPresentDelay;
	float mVBlankSuccess;

	VDD3DPresentHistory() : mPresentDelay(0.f), mVBlankSuccess(1.0f) {}
};

class VDD3D9Manager {
public:
	VDD3D9Manager();
	~VDD3D9Manager();

	bool Attach(VDD3D9Client *pClient);
	void Detach(VDD3D9Client *pClient);

	const D3DCAPS9&			GetCaps() const { return mDevCaps; }
	IDirect3D9				*GetD3D() const { return mpD3D; }
	IDirect3DDevice9		*GetDevice() const { return mpD3DDevice; }
	IDirect3DIndexBuffer9	*GetIndexBuffer() const { return mpD3DIB; }
	IDirect3DVertexBuffer9	*GetVertexBuffer() const { return mpD3DVB; }
	IDirect3DVertexDeclaration9	*GetVertexDeclaration() const { return mpD3DVD; }
	const D3DPRESENT_PARAMETERS& GetPresentParms() const { return mPresentParms; }
	UINT					GetAdapter() const { return mAdapter; }
	D3DDEVTYPE				GetDeviceType() const { return mDevType; }

	IDirect3DSurface9		*GetRenderTarget() const { return mpD3DRTMain; }
	int			GetMainRTWidth() const { return mPresentParms.BackBufferWidth; }
	int			GetMainRTHeight() const { return mPresentParms.BackBufferHeight; }

	bool		Reset();
	bool		CheckDevice();

	void		AdjustTextureSize(int& w, int& h);
	bool		IsTextureFormatAvailable(D3DFORMAT format);

	void		ClearRenderTarget(IDirect3DTexture9 *pTexture);

	void		ResetBuffers();
	nsVDD3D9::Vertex *	LockVertices(unsigned vertices);
	void		UnlockVertices();
	uint16 *	LockIndices(unsigned indices);
	void		UnlockIndices();
	bool		BeginScene();
	bool		EndScene();
	HRESULT		DrawArrays(D3DPRIMITIVETYPE type, UINT vertStart, UINT primCount);
	HRESULT		DrawElements(D3DPRIMITIVETYPE type, UINT vertStart, UINT vertCount, UINT idxStart, UINT primCount);
	HRESULT		Present(const RECT *srcRect, HWND hwndDest, bool vsync, float& syncDelta, VDD3DPresentHistory& history);
	HRESULT		PresentFullScreen();

	bool		Is3DCardLame();

	typedef bool (*SharedTextureFactory)(IVDD3D9TextureGenerator **ppGenerator);
	bool		CreateSharedTexture(const char *name, SharedTextureFactory factory, IVDD3D9Texture **ppTexture);

	template<class T>
	bool		CreateSharedTexture(const char *name, IVDD3D9Texture **ppTexture) {
		return CreateSharedTexture(name, VDRefCountObjectFactory<T, IVDD3D9TextureGenerator>, ppTexture);
	}

protected:
	bool Init();
	bool InitVRAMResources();
	void ShutdownVRAMResources();
	void Shutdown();

	bool InitEffect();
	void ShutdownEffect();

	HMODULE				mhmodDX9;
	IDirect3D9			*mpD3D;
	IDirect3DDevice9	*mpD3DDevice;
	IDirect3DSurface9	*mpD3DRTMain;
	UINT				mAdapter;
	D3DDEVTYPE			mDevType;

	static ATOM			sDevWndClass;
	HWND				mhwndDevice;

	bool				mbDeviceValid;
	bool				mbInScene;

	IDirect3DVertexDeclaration9	*mpD3DVD;
	IDirect3DVertexBuffer9	*mpD3DVB;
	IDirect3DIndexBuffer9	*mpD3DIB;
	IDirect3DQuery9			*mpD3DQuery;
	uint32					mVertexBufferPt;
	uint32					mVertexBufferLockSize;
	uint32					mIndexBufferPt;
	uint32					mIndexBufferLockSize;

	vdfastvector<IDirect3DVertexShader9 *>	mVertexShaders;
	vdfastvector<IDirect3DPixelShader9 *>	mPixelShaders;

	D3DCAPS9				mDevCaps;
	D3DPRESENT_PARAMETERS	mPresentParms;
	D3DDISPLAYMODE			mDisplayMode;

	int					mRefCount;

	vdlist<VDD3D9Client>	mClients;

	typedef vdlist<VDD3D9Texture> SharedTextures;
	vdlist<VDD3D9Texture>	mSharedTextures;
};

VDD3D9Manager *VDInitDirect3D9(VDD3D9Client *pClient);
void VDDeinitDirect3D9(VDD3D9Manager *p, VDD3D9Client *pClient);

#endif
