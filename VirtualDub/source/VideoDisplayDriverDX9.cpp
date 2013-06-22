//	VirtualDub - Video processing and capture application
//	Copyright (C) 1998-2004 Avery Lee
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
//
//
//	This file is the DirectX 9 driver for the video display subsystem.
//	It does traditional point sampled and bilinearly filtered upsampling
//	as well as a special multipass algorithm for emulated bicubic
//	filtering.
//


#include "stdafx.h"
#include <vd2/system/vdtypes.h>

#define DIRECTDRAW_VERSION 0x0900
#define INITGUID
#include <d3d9.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/time.h>
#include <vd2/system/vdstl.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>

#include "VideoDisplayDrivers.h"

// enable if using NVIDIA's NVPerfHUD to profile (set coolbits in Registry, then
// enable via driver config)
//#define PROFILE_NVPERFHUD


#define VDDEBUG_DX9DISP VDDEBUG

extern HWND g_hWnd;


namespace {
	enum {
		kVertexBufferSize	= 256,						// in vertices
		kIndexBufferSize	= kVertexBufferSize*3/2		// in indices
	};

	struct Vertex {
		float x, y, z;
		uint32 diffuse;
		float u0, v0, u1, v1, u2, v2, u3, v3, u4, v4;

		Vertex(float x_, float y_, float z_, uint32 c_, uint32 d_, float u0_, float v0_, float u1_=0.f, float v1_=0.f) : x(x_), y(y_), z(z_), diffuse(c_), u0(u0_), v0(v0_), u1(u1_), v1(v1_)
			, u2(0), v2(0), u3(0), v3(0), u4(0), v4(0) {}

		inline void SetFF1(float x_, float y_, float u0_, float v0_) {
			x = x_;
			y = y_;
			z = 0.f;
			diffuse = 0xffffffff;
			u0 = u0_;
			v0 = v0_;
			u1 = v1 = u2 = v2 = u3 = v3 = u4 = v4 = 0.f;
		}

		inline void SetFF2(float x_, float y_, uint32 c_, float u0_, float v0_, float u1_, float v1_) {
			x = x_;
			y = y_;
			z = 0.f;
			diffuse = c_;
			u0 = u0_;
			v0 = v0_;
			u1 = u1_;
			v1 = v1_;
			u2 = v2 = u3 = v3 = u4 = v4 = 0.f;
		}

		inline void SetFF3(float x_, float y_, float u0_, float v0_, float u1_, float v1_, float u2_, float v2_) {
			x = x_;
			y = y_;
			z = 0.f;
			diffuse = 0xffffffff;
			u0 = u0_;
			v0 = v0_;
			u1 = u1_;
			v1 = v1_;
			u2 = u2_;
			v2 = v2_;
			u3 = v3 = u4 = v4 = 0.f;
		}

		inline void SetPS1_4(float x_, float y_, float u0_, float v0_, float u1_, float v1_, float u2_, float v2_,
								float u3_, float v3_, float u4_, float v4_) {
			x = x_;
			y = y_;
			z = 0.f;
			diffuse = 0xffffffff;
			u0 = u0_;
			v0 = v0_;
			u1 = u1_;
			v1 = v1_;
			u2 = u2_;
			v2 = v2_;
			u3 = u3_;
			v3 = v3_;
			u4 = u4_;
			v4 = v4_;
		}
	};


	///////////////////////////////////////////////////////////////////////
	//
	//	Pixel shaders
	//
	//	These are precompiled using VSA.EXE so we don't have to bring in
	//	D3DX.
	//

	const DWORD pshader1_1a[]={
/*
		"ps_1_1\n"
		"tex t0\n"
		"tex t1\n"
		"tex t2\n"
		"mul_d2 r0,t0,t1\n"
		"mul_d2 r1,t2,t1.a\n"
		"add r0,r0,r1\n"
*/
		0xFFFF0101, 0x00000042, 0xB00F0000, 0x00000042,
		0xB00F0001, 0x00000042, 0xB00F0002, 0x00000005,
		0x8F0F0000, 0xB0E40000, 0xB0E40001, 0x00000005,
		0x8F0F0001, 0xB0E40002, 0xB0FF0001, 0x00000002,
		0x800F0000, 0x80E40000, 0x80E40001, 0x0000FFFF,
	};

	const DWORD pshader1_1b[]={
/*
		"ps_1_1\n"
		"tex t0\n"
		"tex t1\n"
		"tex t2\n"
		"mul r0,t0,t1\n"
		"mul r1,t2,t1.a\n"
		"add r0,r0,r1\n"
*/
		0xFFFF0101, 0x00000042, 0xB00F0000, 0x00000042,
		0xB00F0001, 0x00000042, 0xB00F0002, 0x00000005,
		0x800F0000, 0xB0E40000, 0xB0E40001, 0x00000004,
		0x800F0000, 0xB0E40002, 0xB0FF0001, 0x80E40000,
		0x0000FFFF,
	};

	const DWORD pshader1_4a[]={
/*
		"ps_1_4\n"
		"texld r0,t0\n"
		"texld r1,t1\n"
		"texld r2,t2\n"
		"texld r3,t3\n"
		"texld r4,t4\n"
		"mul r1,r1,-r0.b\n"
		"mad r1,r2,r0.g,r1\n"
		"mad r1,r3,r0.r,r1\n"
		"mad r0,r4,-r0.a,r1\n"
*/
		0xFFFF0104, 0x00000042, 0x800F0000, 0xB0E40000,
		0x00000042, 0x800F0001, 0xB0E40001, 0x00000042,
		0x800F0002, 0xB0E40002, 0x00000042, 0x800F0003,
		0xB0E40003, 0x00000042, 0x800F0004, 0xB0E40004,
		0x00000005, 0x800F0001, 0x80E40001, 0x81AA0000,
		0x00000004, 0x800F0001, 0x80E40002, 0x80550000,
		0x80E40001, 0x00000004, 0x800F0001, 0x80E40003,
		0x80000000, 0x80E40001, 0x00000004, 0x800F0000,
		0x80E40004, 0x81FF0000, 0x80E40001, 0x0000FFFF,
	};
}

#define D3D_DO(x) VDVERIFY(SUCCEEDED(mpD3DDevice->x))

///////////////////////////////////////////////////////////////////////////

class VDVideoDisplayDX9Client : public vdlist<VDVideoDisplayDX9Client>::node {
public:
	virtual void OnPreDeviceReset() = 0;
};

class VDVideoDisplayDX9Manager {
public:
	enum CubicMode {
		kCubicNotInitialized,
		kCubicNotPossible,		// Your card is LAME
		kCubicUseFF2Path,		// Use fixed-function, 2 stage path (GeForce2/GeForce4MX - 8 passes)
		kCubicUseFF3Path,		// Use fixed-function, 3 stage path (RADEON 7xxx - 4 passes)
		kCubicUsePS1_1Path,		// Use programmable, 3 stage path (GeForce3/GeForce4 - 4 passes)
		kCubicUsePS1_4Path,		// Use programmable, 5 stage path (RADEON 85xx+/GeForceFX+ - 2 passes)
		kMaxCubicMode = kCubicUsePS1_4Path
	};

	VDVideoDisplayDX9Manager();
	~VDVideoDisplayDX9Manager();

	bool Attach(VDVideoDisplayDX9Client *pClient);
	void Detach(VDVideoDisplayDX9Client *pClient);

	CubicMode InitBicubic();
	void ShutdownBicubic();

	const D3DCAPS9& GetCaps() const { return mDevCaps; }
	IDirect3D9 *GetD3D() const { return mpD3D; }
	IDirect3DDevice9 *GetDevice() const { return mpD3DDevice; }
	IDirect3DIndexBuffer9 *GetIndexBuffer() const { return mpD3DIB; }
	IDirect3DVertexBuffer9 *GetVertexBuffer() const { return mpD3DVB; }
	const D3DPRESENT_PARAMETERS& GetPresentParms() const { return mPresentParms; }

	IDirect3DSurface9	*GetRenderTarget() const { return mpD3DRTMain; }
	IDirect3DTexture9	*GetVertRenderTexture() const { return mpD3DRTVert; }
	float GetVertUScale() const { return mRTVertUScale; }
	float GetVertVScale() const { return mRTVertVScale; }
	int GetVertUSize() const { return mRTVertUSize; }
	int GetVertVSize() const { return mRTVertVSize; }
	IDirect3DTexture9	*GetFilterTexture() const { return mpD3DFilterTexture; }

	IDirect3DPixelShader9	*GetHPixelShader() const { return mpD3DPixelShader1; }
	IDirect3DPixelShader9	*GetVPixelShader() const { return mpD3DPixelShader2; }

	bool		Reset();
	bool		CheckDevice();

	void		AdjustTextureSize(int& w, int& h);
	void		ClearRenderTarget(IDirect3DTexture9 *pTexture);

	void		ResetBuffers();
	Vertex *	LockVertices(unsigned vertices);
	void		UnlockVertices();
	uint16 *	LockIndices(unsigned indices);
	void		UnlockIndices();
	HRESULT		DrawArrays(D3DPRIMITIVETYPE type, UINT vertStart, UINT primCount);
	HRESULT		DrawElements(D3DPRIMITIVETYPE type, UINT vertStart, UINT vertCount, UINT idxStart, UINT primCount);

	HRESULT DisableTextureStage(UINT stage);
	HRESULT SetTextureStageOp(UINT stage, DWORD color1, DWORD colorop, DWORD color2, DWORD alpha1, DWORD alphaop, DWORD alpha2, DWORD output = D3DTA_CURRENT);

	int GetBicubicShaderStages(CubicMode mode);
	bool Is3DCardLame();
	bool ValidateBicubicShader(CubicMode mode);
	HRESULT SetBicubicShader(CubicMode mode, int stage);

	void MakeCubic4Texture(uint32 *texture, ptrdiff_t pitch, double A, CubicMode mode);
protected:
	bool Init();
	bool InitVRAMResources();
	void ShutdownVRAMResources();
	void Shutdown();

	HMODULE				mhmodDX9;
	IDirect3D9			*mpD3D;
	IDirect3DDevice9	*mpD3DDevice;
	IDirect3DTexture9	*mpD3DFilterTexture;
	IDirect3DSurface9	*mpD3DRTMain;
	IDirect3DTexture9	*mpD3DRTVert;
	int					mRTVertUSize;
	int					mRTVertVSize;
	float				mRTVertUScale;
	float				mRTVertVScale;

	bool				mbDeviceValid;

	IDirect3DVertexBuffer9	*mpD3DVB;
	IDirect3DIndexBuffer9	*mpD3DIB;
	uint32					mVertexBufferPt;
	uint32					mVertexBufferLockSize;
	uint32					mIndexBufferPt;
	uint32					mIndexBufferLockSize;

	IDirect3DPixelShader9	*mpD3DPixelShader1;
	IDirect3DPixelShader9	*mpD3DPixelShader2;

	D3DCAPS9				mDevCaps;
	D3DPRESENT_PARAMETERS	mPresentParms;

	CubicMode			mCubicMode;
	int					mCubicRefCount;

	int					mRefCount;

	vdlist<VDVideoDisplayDX9Client>	mClients;
};

static VDVideoDisplayDX9Manager g_VDDisplayDX9;

VDVideoDisplayDX9Manager *VDInitDisplayDX9(VDVideoDisplayDX9Client *pClient) {
	return g_VDDisplayDX9.Attach(pClient) ? &g_VDDisplayDX9 : NULL;
}

void VDDeinitDisplayDX9(VDVideoDisplayDX9Manager *p, VDVideoDisplayDX9Client *pClient) {
	VDASSERT(p == &g_VDDisplayDX9);

	p->Detach(pClient);
}

VDVideoDisplayDX9Manager::VDVideoDisplayDX9Manager()
	: mhmodDX9(NULL)
	, mpD3D(NULL)
	, mpD3DDevice(NULL)
	, mpD3DFilterTexture(NULL)
	, mpD3DRTMain(NULL)
	, mpD3DRTVert(NULL)
	, mbDeviceValid(false)
	, mpD3DVB(NULL)
	, mpD3DIB(NULL)
	, mpD3DPixelShader1(NULL)
	, mpD3DPixelShader2(NULL)
	, mCubicRefCount(0)
	, mRefCount(0)
{
}

VDVideoDisplayDX9Manager::~VDVideoDisplayDX9Manager() {
	VDASSERT(!mRefCount);
	VDASSERT(!mCubicRefCount);
}

bool VDVideoDisplayDX9Manager::Attach(VDVideoDisplayDX9Client *pClient) {
	bool bSuccess = false;

	mClients.push_back(pClient);

	if (++mRefCount == 1)
		bSuccess = Init();
	else
		bSuccess = CheckDevice();

	if (!bSuccess)
		Detach(pClient);

	return bSuccess;
}

void VDVideoDisplayDX9Manager::Detach(VDVideoDisplayDX9Client *pClient) {
	VDASSERT(mRefCount > 0);

	mClients.erase(mClients.fast_find(pClient));

	if (!--mRefCount)
		Shutdown();
}

bool VDVideoDisplayDX9Manager::Init() {
	// attempt to load D3D9.DLL
	mhmodDX9 = LoadLibrary("d3d9.dll");
	if (!mhmodDX9) {
		Shutdown();
		return false;
	}

	IDirect3D9 *(APIENTRY *pDirect3DCreate9)(UINT) = (IDirect3D9 *(APIENTRY *)(UINT))GetProcAddress(mhmodDX9, "Direct3DCreate9");
	if (!pDirect3DCreate9) {
		Shutdown();
		return false;
	}

	// create Direct3D9 object
	mpD3D = pDirect3DCreate9(D3D_SDK_VERSION);
	if (!mpD3D) {
		Shutdown();
		return false;
	}

	// create device
	memset(&mPresentParms, 0, sizeof mPresentParms);
	mPresentParms.Windowed			= TRUE;
	mPresentParms.SwapEffect		= D3DSWAPEFFECT_DISCARD;
	mPresentParms.BackBufferFormat	= D3DFMT_UNKNOWN;

#ifdef PROFILE_NVPERFHUD
	mPresentParms.BackBufferWidth	= 1024;//GetSystemMetrics(SM_CXMAXIMIZED);
	mPresentParms.BackBufferHeight	= 768;//GetSystemMetrics(SM_CYMAXIMIZED);
#else
	mPresentParms.BackBufferWidth	= GetSystemMetrics(SM_CXMAXIMIZED);
	mPresentParms.BackBufferHeight	= GetSystemMetrics(SM_CYMAXIMIZED);
#endif

	HRESULT hr;

	// Look for the NVPerfHUD 2.0 driver
	
	const DWORD dwFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE;
	const UINT adapters = mpD3D->GetAdapterCount();
	UINT adapter = D3DADAPTER_DEFAULT;
	D3DDEVTYPE type = D3DDEVTYPE_HAL;

	for(UINT n=0; n<adapters; ++n) {
		D3DADAPTER_IDENTIFIER9 ident;

		if (SUCCEEDED(mpD3D->GetAdapterIdentifier(n, 0, &ident))) {
			if (!strcmp(ident.Description, "NVIDIA NVPerfHUD")) {
				adapter = n;
				type = D3DDEVTYPE_REF;
				break;
			}
		}
	}

	hr = mpD3D->CreateDevice(adapter, type, g_hWnd, dwFlags, &mPresentParms, &mpD3DDevice);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	mbDeviceValid = true;

	// retrieve device caps
	memset(&mDevCaps, 0, sizeof mDevCaps);
	hr = mpD3DDevice->GetDeviceCaps(&mDevCaps);
	if (FAILED(hr)) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to retrieve device caps.\n");
		Shutdown();
		return false;
	}

	// Check for Virge/Rage Pro/Riva128
	if (Is3DCardLame()) {
		Shutdown();
		return false;
	}

	if (!InitVRAMResources()) {
		Shutdown();
		return false;
	}
	return true;
}

bool VDVideoDisplayDX9Manager::InitVRAMResources() {
	// retrieve back buffer
	if (!mpD3DRTMain) {
		HRESULT hr = mpD3DDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &mpD3DRTMain);
		if (FAILED(hr)) {
			ShutdownVRAMResources();
			return false;
		}
	}

	// create vertex buffer
	if (!mpD3DVB) {
		HRESULT hr = mpD3DDevice->CreateVertexBuffer(kVertexBufferSize * sizeof(Vertex), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX5, D3DPOOL_DEFAULT, &mpD3DVB, NULL);
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to create vertex buffer.\n");
			ShutdownVRAMResources();
			return false;
		}
		mVertexBufferPt = 0;
	}

	// create index buffer
	if (!mpD3DIB) {
		HRESULT hr = mpD3DDevice->CreateIndexBuffer(kIndexBufferSize * sizeof(uint16), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &mpD3DIB, NULL);
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to create index buffer.\n");
			ShutdownVRAMResources();
			return false;
		}
		mIndexBufferPt = 0;
	}

	return true;
}

void VDVideoDisplayDX9Manager::ShutdownVRAMResources() {
	if (mpD3DRTMain) {
		mpD3DRTMain->Release();
		mpD3DRTMain = NULL;
	}

	if (mpD3DIB) {
		mpD3DIB->Release();
		mpD3DIB = NULL;
	}

	if (mpD3DVB) {
		mpD3DVB->Release();
		mpD3DVB = NULL;
	}
}

void VDVideoDisplayDX9Manager::Shutdown() {
	VDASSERT(!mCubicRefCount);

	mbDeviceValid = false;

	ShutdownVRAMResources();

	if (mpD3DDevice) {
		mpD3DDevice->Release();
		mpD3DDevice = NULL;
	}

	if (mpD3D) {
		mpD3D->Release();
		mpD3D = NULL;
	}

	if (mhmodDX9) {
		FreeLibrary(mhmodDX9);
		mhmodDX9 = NULL;
	}
}

void VDVideoDisplayDX9Manager::MakeCubic4Texture(uint32 *texture, ptrdiff_t pitch, double A, CubicMode mode) {
	int i;

	uint32 *p0 = texture;
	uint32 *p1 = vdptroffset(texture, pitch);
	uint32 *p2 = vdptroffset(texture, pitch*2);
	uint32 *p3 = vdptroffset(texture, pitch*3);

	for(i=0; i<256; i++) {
		double d = (double)(i&63) / 64.0;
		int y1, y2, y3, y4, ydiff;

		// Coefficients for all four pixels *must* add up to 1.0 for
		// consistent unity gain.
		//
		// Two good values for A are -1.0 (original VirtualDub bicubic filter)
		// and -0.75 (closely matches Photoshop).

		double c1 =         +     A*d -       2.0*A*d*d +       A*d*d*d;
		double c2 = + 1.0             -     (A+3.0)*d*d + (A+2.0)*d*d*d;
		double c3 =         -     A*d + (2.0*A+3.0)*d*d - (A+2.0)*d*d*d;
		double c4 =                   +           A*d*d -       A*d*d*d;

		const int maxval = 255;
		double scale = maxval / (c1 + c2 + c3 + c4);

		y1 = (int)floor(0.5 + c1 * scale);
		y2 = (int)floor(0.5 + c2 * scale);
		y3 = (int)floor(0.5 + c3 * scale);
		y4 = (int)floor(0.5 + c4 * scale);

		ydiff = maxval - y1 - y2 - y3 - y4;

		int ywhole = ydiff<0 ? (ydiff-2)/4 : (ydiff+2)/4;
		ydiff -= ywhole*4;

		y1 += ywhole;
		y2 += ywhole;
		y3 += ywhole;
		y4 += ywhole;

		if (ydiff < 0) {
			if (y1<y4)
				y1 += ydiff;
			else
				y4 += ydiff;
		} else if (ydiff > 0) {
			if (y2 > y3)
				y2 += ydiff;
			else
				y3 += ydiff;
		}

		switch(mode) {
		case kCubicUsePS1_4Path:
			p0[i] = (-y1 << 24) + (y2 << 16) + (y3 << 8) + (-y4);
			break;
		case kCubicUseFF3Path:
			p0[i] = -y1 * 0x020202 + (-y4 << 25);
			p1[i] = y2 * 0x010101 + (y3<<24);

			if (y2 > y3)
				y2 += y3&1;
			else
				y3 += y2&1;

			y2>>=1;
			y3>>=1;

			p2[i] = -y1 * 0x010101 + (-y4 << 24);
			p3[i] = y2 * 0x010101 + (y3<<24);
			break;

		case kCubicUsePS1_1Path:
			p0[i] = -y1 * 0x010101 + (-y4 << 24);
			p1[i] = y2 * 0x010101 + (y3<<24);

			p2[i] = -y1 * 0x010101 + (-y4 << 24);
			p3[i] = y2 * 0x010101 + (y3<<24);
			break;

		case kCubicUseFF2Path:
			p0[i] = -y1 * 0x010101;
			p1[i] = y2 * 0x010101;

			p2[i] = y3 * 0x010101;
			p3[i] = -y4 * 0x010101;
			break;
		}
	}
}

VDVideoDisplayDX9Manager::CubicMode VDVideoDisplayDX9Manager::InitBicubic() {
	VDASSERT(mRefCount > 0);

	if (++mCubicRefCount > 1)
		return mCubicMode;

	HRESULT hr;

	hr = mpD3DDevice->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX5);
	if (FAILED(hr)) {
		ShutdownBicubic();
		return mCubicMode = kCubicNotPossible;
	}

//	mCubicMode = kMaxCubicMode;
	mCubicMode = kCubicUseFF3Path;
	while(mCubicMode > kCubicNotPossible) {
		if (ValidateBicubicShader(mCubicMode))
			break;
		mCubicMode = (CubicMode)(mCubicMode - 1);
	}

	if (mCubicMode == kCubicNotPossible) {
		ShutdownBicubic();
		return mCubicMode;
	}

	// create vertical resampling texture
	if (mCubicMode < kCubicUsePS1_4Path) {
		int texw = mPresentParms.BackBufferWidth;
		int texh = mPresentParms.BackBufferHeight;

		AdjustTextureSize(texw, texh);
		
		mRTVertUSize = texw;
		mRTVertVSize = texh;

		hr = mpD3DDevice->CreateTexture(texw, texh, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &mpD3DRTVert, NULL);
		if (FAILED(hr)) {
			ShutdownBicubic();
			return kCubicNotPossible;
		}

		ClearRenderTarget(mpD3DRTVert);
	} else {
		// plug in sensible values so the matrix code doesn't blow up
		mRTVertUSize = 1;
		mRTVertVSize = 1;
	}
	mRTVertUScale = 1.0f / mRTVertUSize;
	mRTVertVScale = 1.0f / mRTVertVSize;

	// create filter texture
	hr = mpD3DDevice->CreateTexture(256, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &mpD3DFilterTexture, NULL);
	if (FAILED(hr)) {
		ShutdownBicubic();
		return kCubicNotPossible;
	}

	D3DLOCKED_RECT lr;
	hr = mpD3DFilterTexture->LockRect(0, &lr, NULL, 0);
	VDASSERT(SUCCEEDED(hr));
	if (SUCCEEDED(hr)) {
		MakeCubic4Texture((uint32 *)lr.pBits, lr.Pitch, -0.75, mCubicMode);

		VDVERIFY(SUCCEEDED(mpD3DFilterTexture->UnlockRect(0)));
	}

	if (mCubicMode == kCubicUsePS1_1Path || mCubicMode == kCubicUsePS1_4Path) {
		if (mCubicMode == kCubicUsePS1_1Path)
			hr = mpD3DDevice->CreatePixelShader(pshader1_1a, &mpD3DPixelShader1);
		else
			hr = mpD3DDevice->CreatePixelShader(pshader1_4a, &mpD3DPixelShader1);

		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to create pixel shader #1.\n");
			ShutdownBicubic();
			return kCubicNotPossible;
		}

		if (mCubicMode == kCubicUsePS1_1Path) {
			hr = mpD3DDevice->CreatePixelShader(pshader1_1b, &mpD3DPixelShader2);

			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to create pixel shader #2.\n");
				ShutdownBicubic();
				return kCubicNotPossible;
			}
		}
	}

	return mCubicMode;
}

void VDVideoDisplayDX9Manager::ShutdownBicubic() {
	VDASSERT(mCubicRefCount > 0);
	if (--mCubicRefCount)
		return;

	if (mpD3DPixelShader2) {
		mpD3DPixelShader2->Release();
		mpD3DPixelShader2 = NULL;
	}

	if (mpD3DPixelShader1) {
		mpD3DPixelShader1->Release();
		mpD3DPixelShader1 = NULL;
	}

	if (mpD3DFilterTexture) {
		mpD3DFilterTexture->Release();
		mpD3DFilterTexture = NULL;
	}

	if (mpD3DRTVert) {
		mpD3DRTVert->Release();
		mpD3DRTVert = NULL;
	}
}

bool VDVideoDisplayDX9Manager::Reset() {
	ShutdownVRAMResources();

	for(vdlist<VDVideoDisplayDX9Client>::iterator it(mClients.begin()), itEnd(mClients.end()); it!=itEnd; ++it) {
		VDVideoDisplayDX9Client& client = **it;

		client.OnPreDeviceReset();
	}

	HRESULT hr = mpD3DDevice->Reset(&mPresentParms);
	if (FAILED(hr)) {
		mbDeviceValid = false;
		return false;
	}

	mbDeviceValid = true;

	return InitVRAMResources();
}

bool VDVideoDisplayDX9Manager::CheckDevice() {
	if (!mpD3DDevice)
		return false;

	if (!mbDeviceValid) {
		HRESULT hr = mpD3DDevice->TestCooperativeLevel();

		if (FAILED(hr)) {
			if (hr != D3DERR_DEVICENOTRESET)
				return false;

			if (!Reset())
				return false;
		}
	}

	return InitVRAMResources();
}

void VDVideoDisplayDX9Manager::AdjustTextureSize(int& texw, int& texh) {
	// make power of two
	texw += texw - 1;
	texh += texh - 1;

	while(int tmp = texw & (texw-1))
		texw = tmp;
	while(int tmp = texh & (texh-1))
		texh = tmp;

	// enforce aspect ratio
	if (mDevCaps.MaxTextureAspectRatio) {
		while(texw * mDevCaps.MaxTextureAspectRatio < texh)
			texw += texw;
		while(texh * mDevCaps.MaxTextureAspectRatio < texw)
			texh += texh;
	}
}

void VDVideoDisplayDX9Manager::ClearRenderTarget(IDirect3DTexture9 *pTexture) {
	IDirect3DSurface9 *pRTSurface;
	if (FAILED(pTexture->GetSurfaceLevel(0, &pRTSurface)))
		return;
	HRESULT hr = mpD3DDevice->SetRenderTarget(0, pRTSurface);
	pRTSurface->Release();

	if (FAILED(hr))
		return;

	if (SUCCEEDED(mpD3DDevice->BeginScene())) {
		mpD3DDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 0.f, 0);
		mpD3DDevice->EndScene();
	}
}

void VDVideoDisplayDX9Manager::ResetBuffers() {
	mVertexBufferPt = 0;
	mIndexBufferPt = 0;
}

Vertex *VDVideoDisplayDX9Manager::LockVertices(unsigned vertices) {
	VDASSERT(vertices <= kVertexBufferSize);
	if (mVertexBufferPt + vertices > kVertexBufferSize) {
		mVertexBufferPt = 0;
	}

	mVertexBufferLockSize = vertices;

	void *p;
	HRESULT hr;
	for(;;) {
		hr = mpD3DVB->Lock(mVertexBufferPt * sizeof(Vertex), mVertexBufferLockSize * sizeof(Vertex), &p, mVertexBufferPt ? D3DLOCK_NOOVERWRITE : D3DLOCK_DISCARD);
		if (hr != D3DERR_WASSTILLDRAWING)
			break;
		Sleep(1);
	}
	if (FAILED(hr)) {
		VDASSERT(false);
		return NULL;
	}

	return (Vertex *)p;
}

void VDVideoDisplayDX9Manager::UnlockVertices() {
	mVertexBufferPt += mVertexBufferLockSize;

	VDVERIFY(SUCCEEDED(mpD3DVB->Unlock()));
}

uint16 *VDVideoDisplayDX9Manager::LockIndices(unsigned indices) {
	VDASSERT(indices <= kIndexBufferSize);
	if (mIndexBufferPt + indices > kIndexBufferSize) {
		mIndexBufferPt = 0;
	}

	mIndexBufferLockSize = indices;

	void *p;
	HRESULT hr;
	for(;;) {
		hr = mpD3DIB->Lock(mIndexBufferPt * sizeof(uint16), mIndexBufferLockSize * sizeof(uint16), &p, mIndexBufferPt ? D3DLOCK_NOOVERWRITE : D3DLOCK_DISCARD);
		if (hr != D3DERR_WASSTILLDRAWING)
			break;
		Sleep(1);
	}
	if (FAILED(hr)) {
		VDASSERT(false);
		return NULL;
	}

	return (uint16 *)p;
}

void VDVideoDisplayDX9Manager::UnlockIndices() {
	mIndexBufferPt += mIndexBufferLockSize;

	VDVERIFY(SUCCEEDED(mpD3DIB->Unlock()));
}

HRESULT VDVideoDisplayDX9Manager::DrawArrays(D3DPRIMITIVETYPE type, UINT vertStart, UINT primCount) {
	HRESULT hr = mpD3DDevice->DrawPrimitive(type, mVertexBufferPt - mVertexBufferLockSize + vertStart, primCount);

	VDASSERT(SUCCEEDED(hr));

	return hr;
}

HRESULT VDVideoDisplayDX9Manager::DrawElements(D3DPRIMITIVETYPE type, UINT vertStart, UINT vertCount, UINT idxStart, UINT primCount) {
	// The documentation for IDirect3DDevice9::DrawIndexedPrimitive() was probably
	// written under a hallucinogenic state.

	HRESULT hr = mpD3DDevice->DrawIndexedPrimitive(type, mVertexBufferPt - mVertexBufferLockSize + vertStart, 0, vertCount, mIndexBufferPt - mIndexBufferLockSize + idxStart, primCount);

	VDASSERT(SUCCEEDED(hr));

	return hr;
}

int VDVideoDisplayDX9Manager::GetBicubicShaderStages(CubicMode mode) {
	switch(mode) {
	case kCubicUseFF2Path:
		return 7;
	case kCubicUseFF3Path:
		return 5;
	case kCubicUsePS1_1Path:
		return 5;
	case kCubicUsePS1_4Path:
		return 1;
	default:
		VDNEVERHERE;
	}
}

#define REQUIRE(x, reason) if (!(x)) { VDDEBUG_DX9DISP("VideoDisplay/DX9: 3D device is lame -- reason: " reason "\n"); return true; } else ((void)0)
#define REQUIRECAPS(capsflag, bits, reason) REQUIRE(!(~mDevCaps.capsflag & (bits)), reason)

bool VDVideoDisplayDX9Manager::Is3DCardLame() {
	REQUIRE(mDevCaps.DeviceType != D3DDEVTYPE_SW, "software device detected");
	REQUIRECAPS(PrimitiveMiscCaps, D3DPMISCCAPS_CULLNONE, "primitive misc caps check failed");
	REQUIRECAPS(RasterCaps, D3DPRASTERCAPS_DITHER, "raster caps check failed");
	REQUIRECAPS(TextureCaps, D3DPTEXTURECAPS_ALPHA | D3DPTEXTURECAPS_MIPMAP, "texture caps failed");
	REQUIRE(!(mDevCaps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY), "device requires square textures");
	REQUIRECAPS(TextureFilterCaps, D3DPTFILTERCAPS_MAGFPOINT | D3DPTFILTERCAPS_MAGFLINEAR
								| D3DPTFILTERCAPS_MINFPOINT | D3DPTFILTERCAPS_MINFLINEAR
								| D3DPTFILTERCAPS_MIPFPOINT | D3DPTFILTERCAPS_MIPFLINEAR, "texture filtering modes insufficient");
	REQUIRECAPS(TextureAddressCaps, D3DPTADDRESSCAPS_CLAMP | D3DPTADDRESSCAPS_WRAP, "texture addressing modes insufficient");
	REQUIRE(mDevCaps.MaxTextureBlendStages>0 && mDevCaps.MaxSimultaneousTextures>0, "not enough texture stages");
	return false;
}

bool VDVideoDisplayDX9Manager::ValidateBicubicShader(CubicMode mode) {
	static const struct ShaderRequirements {
		DWORD	PrimitiveMiscCaps;
		DWORD	MaxSimultaneousTextures;		// also MaxTextureBlendStages
		DWORD	SrcBlendCaps;
		DWORD	DestBlendCaps;
		DWORD	TextureOpCaps;
		DWORD	PixelShaderVersion;
		float	PixelShader1xMaxValue;
	} sRequirements[]={
		//Fixed function, 2 stage path (NVIDIA GeForce2, GeForce4 MX)
		{
			D3DPMISCCAPS_BLENDOP,						// primitive misc caps
			2,											// texture stages
			D3DPBLENDCAPS_ONE,							// source blend factors
			D3DPBLENDCAPS_ONE,							// dest blend factors
			D3DTEXOPCAPS_MULTIPLYADD | D3DTEXOPCAPS_MODULATE | D3DTEXOPCAPS_SELECTARG1 | D3DTEXOPCAPS_ADDSIGNED2X,	// texture op caps
			0,											// pixel shader version
			0.f,										// pixel shader value range
		},

		//Fixed function, 3 stage path (ATI RADEON 7xxx)
		{
			0,											// primitive misc caps
			3,											// texture stages
			D3DPBLENDCAPS_ZERO | D3DPBLENDCAPS_ONE,		// source blend factors
			D3DPBLENDCAPS_ONE,							// dest blend factors
			D3DTEXOPCAPS_SELECTARG1 | D3DTEXOPCAPS_MODULATE | D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR | D3DTEXOPCAPS_ADD, // texture op caps
			0,											// pixel shader version
			0.f,										// pixel shader value range
		},

		//Pixel shader 1.1 path (NVIDIA GeForce 3/4)
		{
			D3DPMISCCAPS_BLENDOP,						// primitive misc caps
			2,											// texture stages
			D3DPBLENDCAPS_ONE,							// source blend factors
			D3DPBLENDCAPS_ONE,							// dest blend factors
			0,											// texture op caps
			D3DPS_VERSION(1,1),							// pixel shader version
			1.f,										// pixel shader value range
		},

		//Pixel shader 1.4 path (ATI RADEON 8xxx+, NVIDIA GeForce FX+)
		{
			0,											// primitive misc caps
			5,											// texture stages
			0,											// source blend factors
			0,											// dest blend factors
			0,											// texture op caps
			D3DPS_VERSION(1,4),							// pixel shader version
			2.f,										// pixel shader value range
		},
	};


	// Validate caps bits.
	const ShaderRequirements& reqs = sRequirements[mode - kCubicUseFF2Path];

	if ((mDevCaps.PrimitiveMiscCaps & reqs.PrimitiveMiscCaps) != reqs.PrimitiveMiscCaps)
		return false;
	if (mDevCaps.MaxSimultaneousTextures < reqs.MaxSimultaneousTextures)
		return false;
	if (mDevCaps.MaxTextureBlendStages < reqs.MaxSimultaneousTextures)
		return false;
	if ((mDevCaps.SrcBlendCaps & reqs.SrcBlendCaps) != reqs.SrcBlendCaps)
		return false;
	if ((mDevCaps.DestBlendCaps & reqs.DestBlendCaps) != reqs.DestBlendCaps)
		return false;
	if ((mDevCaps.TextureOpCaps & reqs.TextureOpCaps) != reqs.TextureOpCaps)
		return false;
	if (reqs.PixelShaderVersion) {
		if (mDevCaps.PixelShaderVersion < reqs.PixelShaderVersion)
			return false;
		if (mDevCaps.PixelShader1xMaxValue < reqs.PixelShader1xMaxValue * 0.95f)
			return false;
	}

	// Validate shaders.
	const int stages = GetBicubicShaderStages(mode);

	for(int stage = 0; stage < stages; ++stage) {
		SetBicubicShader(mode, stage);

		DWORD passes;
		HRESULT hr = mpD3DDevice->ValidateDevice(&passes);

		if (FAILED(hr))
			return false;
	}

	return true;
}

HRESULT VDVideoDisplayDX9Manager::SetBicubicShader(CubicMode mode, int stage) {
	// common state rules:
	//
	//	All 3 texture stages must be set.
	//	All texture stages in use must have their addressing and filter modes set.
	//	The following render states must be set:
	//		ALPHABLENDENABLE
	//		DITHERENABLE
	//		BLENDOP				if blending is enabled
	//		SRCBLEND			if blending is enabled
	//		DSTBLEND			if blending is enabled

	switch(mode) {
	case kCubicUseFF2Path:
		switch(stage) {
		case 0:
			D3D_DO(SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0));
			D3D_DO(SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT));

			D3D_DO(SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1));
			D3D_DO(SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP));
			D3D_DO(SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP));
			D3D_DO(SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_POINT));
			SetTextureStageOp(0, D3DTA_TEXTURE, D3DTOP_MULTIPLYADD, D3DTA_DIFFUSE, D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_DIFFUSE);
			D3D_DO(SetTextureStageState(0, D3DTSS_COLORARG0, D3DTA_DIFFUSE | D3DTA_ALPHAREPLICATE));
			SetTextureStageOp(1, D3DTA_CURRENT, D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_CURRENT, D3DTOP_MODULATE, D3DTA_TEXTURE);
			DisableTextureStage(2);
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
			D3D_DO(SetRenderState(D3DRS_DITHERENABLE, TRUE));
			break;
		case 1:
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
			D3D_DO(SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE));
			D3D_DO(SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE));
			D3D_DO(SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD));
			break;
		case 2:
			D3D_DO(SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT));
			break;
		case 3:
			SetTextureStageOp(0, D3DTA_TEXTURE, D3DTOP_SELECTARG1, D3DTA_DIFFUSE, D3DTA_TEXTURE, D3DTOP_ADD, D3DTA_TEXTURE);
			SetTextureStageOp(1, D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_CURRENT, D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_CURRENT);
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
			break;
		case 4:
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
			D3D_DO(SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD));
			break;
		case 5:
			D3D_DO(SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT));
			break;
		case 6:
			SetTextureStageOp(0, D3DTA_TEXTURE, D3DTOP_ADDSIGNED2X, D3DTA_CURRENT, D3DTA_TEXTURE, D3DTOP_ADDSIGNED2X, D3DTA_CURRENT);
			DisableTextureStage(1);
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
			D3D_DO(SetRenderState(D3DRS_DITHERENABLE, TRUE));
			break;
		default:
			VDNEVERHERE;
		}
		break;
	case kCubicUseFF3Path:
		switch(stage) {
		case 0:
			D3D_DO(SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0));
			D3D_DO(SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT));

			D3D_DO(SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1));
			D3D_DO(SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP));
			D3D_DO(SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP));
			D3D_DO(SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_POINT));

			D3D_DO(SetTextureStageState(2, D3DTSS_TEXCOORDINDEX, 2));
			D3D_DO(SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(2, D3DSAMP_MIPFILTER, D3DTEXF_POINT));

			SetTextureStageOp(0, D3DTA_TEXTURE, D3DTOP_SELECTARG1, D3DTA_CURRENT, D3DTA_TEXTURE, D3DTOP_SELECTARG1, D3DTA_CURRENT);
			SetTextureStageOp(1, D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_CURRENT, D3DTA_TEXTURE, D3DTOP_SELECTARG1, D3DTA_CURRENT);
			SetTextureStageOp(2, D3DTA_CURRENT, D3DTOP_MODULATEALPHA_ADDCOLOR, D3DTA_TEXTURE, D3DTA_CURRENT, D3DTOP_SELECTARG1, D3DTA_CURRENT);

			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
			D3D_DO(SetRenderState(D3DRS_DITHERENABLE, FALSE));
			D3D_DO(SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO));
			D3D_DO(SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR));
			D3D_DO(SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD));
			break;
		case 1:
			D3D_DO(SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE));
			D3D_DO(SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE));
			break;
		case 2:
			SetTextureStageOp(0, D3DTA_TEXTURE | D3DTA_COMPLEMENT, D3DTOP_SELECTARG1, D3DTA_CURRENT, D3DTA_TEXTURE, D3DTOP_SELECTARG1, D3DTA_CURRENT);
			SetTextureStageOp(1, D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_CURRENT, D3DTA_TEXTURE, D3DTOP_SELECTARG1, D3DTA_CURRENT);
			SetTextureStageOp(2, D3DTA_CURRENT, D3DTOP_MODULATEALPHA_ADDCOLOR, D3DTA_TEXTURE | D3DTA_COMPLEMENT, D3DTA_CURRENT, D3DTOP_SELECTARG1, D3DTA_CURRENT);
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
			D3D_DO(SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO));
			D3D_DO(SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR));
			break;
		case 3:
			D3D_DO(SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE));
			D3D_DO(SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE));
			break;
		case 4:
			SetTextureStageOp(0, D3DTA_TEXTURE | D3DTA_COMPLEMENT, D3DTOP_ADD, D3DTA_TEXTURE | D3DTA_COMPLEMENT, D3DTA_TEXTURE | D3DTA_COMPLEMENT, D3DTOP_ADD, D3DTA_TEXTURE | D3DTA_COMPLEMENT);
			DisableTextureStage(1);
			DisableTextureStage(2);
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
			D3D_DO(SetRenderState(D3DRS_DITHERENABLE, TRUE));
			break;
		}
		break;
	case kCubicUsePS1_1Path:
		switch(stage) {
		case 0:
			D3D_DO(SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0));
			D3D_DO(SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT));

			D3D_DO(SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1));
			D3D_DO(SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP));
			D3D_DO(SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP));
			D3D_DO(SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_POINT));

			D3D_DO(SetTextureStageState(2, D3DTSS_TEXCOORDINDEX, 2));
			D3D_DO(SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(2, D3DSAMP_MIPFILTER, D3DTEXF_POINT));

			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
			break;
		case 1:
			D3D_DO(SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE));
			D3D_DO(SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE));
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
			D3D_DO(SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT));
			break;
		case 2:
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
			break;
		case 3:
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
			D3D_DO(SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT));
			break;
		case 4:
			SetTextureStageOp(0, D3DTA_TEXTURE, D3DTOP_ADD, D3DTA_TEXTURE, D3DTA_TEXTURE, D3DTOP_ADD, D3DTA_TEXTURE);
			DisableTextureStage(1);
			DisableTextureStage(2);
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
			D3D_DO(SetRenderState(D3DRS_DITHERENABLE, TRUE));
			break;
		}
		break;
	case kCubicUsePS1_4Path:
		switch(stage) {
		case 0:
			D3D_DO(SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0));
			D3D_DO(SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP));
			D3D_DO(SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP));
			D3D_DO(SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT));

			D3D_DO(SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1));
			D3D_DO(SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_POINT));

			D3D_DO(SetTextureStageState(2, D3DTSS_TEXCOORDINDEX, 2));
			D3D_DO(SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(2, D3DSAMP_MIPFILTER, D3DTEXF_POINT));

			D3D_DO(SetTextureStageState(3, D3DTSS_TEXCOORDINDEX, 3));
			D3D_DO(SetSamplerState(3, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(3, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(3, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(3, D3DSAMP_MINFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(3, D3DSAMP_MIPFILTER, D3DTEXF_POINT));

			D3D_DO(SetTextureStageState(4, D3DTSS_TEXCOORDINDEX, 4));
			D3D_DO(SetSamplerState(4, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(4, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
			D3D_DO(SetSamplerState(4, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(4, D3DSAMP_MINFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(4, D3DSAMP_MIPFILTER, D3DTEXF_POINT));

			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
			D3D_DO(SetRenderState(D3DRS_DITHERENABLE, TRUE));
			break;
		}
		break;
	default:
		VDNEVERHERE;
	}
	return S_OK;
}

///////////////////////////////////////////////////////////////////////////

HRESULT VDVideoDisplayDX9Manager::DisableTextureStage(UINT stage) {
	HRESULT hr = mpD3DDevice->SetTextureStageState(stage, D3DTSS_COLOROP, D3DTOP_DISABLE);

	VDASSERT(SUCCEEDED(hr));
	return hr;
}

HRESULT VDVideoDisplayDX9Manager::SetTextureStageOp(UINT stage, DWORD color1, DWORD colorop, DWORD color2, DWORD alpha1, DWORD alphaop, DWORD alpha2, DWORD output) {
	HRESULT hr;

	if (SUCCEEDED(hr = mpD3DDevice->SetTextureStageState(stage, D3DTSS_COLORARG1, color1)))
	if (SUCCEEDED(hr = mpD3DDevice->SetTextureStageState(stage, D3DTSS_COLORARG2, color2)))
	if (SUCCEEDED(hr = mpD3DDevice->SetTextureStageState(stage, D3DTSS_COLOROP, colorop)))
	if (SUCCEEDED(hr = mpD3DDevice->SetTextureStageState(stage, D3DTSS_ALPHAARG1, alpha1)))
	if (SUCCEEDED(hr = mpD3DDevice->SetTextureStageState(stage, D3DTSS_ALPHAARG2, alpha2)))
	if (SUCCEEDED(hr = mpD3DDevice->SetTextureStageState(stage, D3DTSS_ALPHAOP, alphaop)))
		return hr;

	VDASSERT(false);
	return hr;
}

///////////////////////////////////////////////////////////////////////////

class VDVideoDisplayMinidriverDX9 : public IVDVideoDisplayMinidriver, protected VDVideoDisplayDX9Client {
public:
	VDVideoDisplayMinidriverDX9();
	~VDVideoDisplayMinidriverDX9();

protected:
	bool Init(HWND hwnd, const VDVideoDisplaySourceInfo& info);
	void Shutdown();

	bool ModifySource(const VDVideoDisplaySourceInfo& info);

	bool IsValid();
	void SetFilterMode(FilterMode mode);

	bool Tick(int id);
	bool Resize();
	bool Update(FieldMode);
	void Refresh(FieldMode);
	bool Paint(HDC hdc, const RECT& rClient);

	bool SetSubrect(const vdrect32 *) { return false; }
	void SetLogicalPalette(const uint8 *pLogicalPalette);

protected:
	void OnPreDeviceReset() { ShutdownBicubic(); }
	void InitBicubic();
	void ShutdownBicubic();
	bool Paint_FF2(HDC hdc, const RECT& rClient);
	bool Paint_FF2_Fast(HDC hdc, const RECT& rClient);
	bool Paint_FF3(HDC hdc, const RECT& rClient);
	bool Paint_PS1_1(HDC hdc, const RECT& rClient);
	bool Paint_PS1_4(HDC hdc, const RECT& rClient);

	HWND				mhwnd;
	VDVideoDisplayDX9Manager	*mpManager;
	IDirect3DDevice9	*mpD3DDevice;			// weak ref
	IDirect3DTexture9	*mpD3DImageTexture;
	IDirect3DTexture9	*mpD3DTexture;
	IDirect3DTexture9	*mpD3DRTHoriz;
	int					mRTHorizUSize;
	int					mRTHorizVSize;
	float				mRTHorizUScale;
	float				mRTHorizVScale;

	VDVideoDisplayDX9Manager::CubicMode	mCubicMode;
	bool				mbCubicInitialized;
	bool				mbCubicAttempted;
	FilterMode			mPreferredFilter;

	D3DMATRIX					mHorizProjection;
	D3DMATRIX					mVertProjection;
	D3DMATRIX					mWholeProjection;

	VDPixmap					mTexFmt;
	VDVideoDisplaySourceInfo	mSource;
};

IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverDX9() {
	return new VDVideoDisplayMinidriverDX9;
}

VDVideoDisplayMinidriverDX9::VDVideoDisplayMinidriverDX9()
	: mpManager(NULL)
	, mpD3DImageTexture(NULL)
	, mpD3DRTHoriz(NULL)
	, mbCubicInitialized(false)
	, mbCubicAttempted(false)
	, mPreferredFilter(kFilterAnySuitable)
{
}

VDVideoDisplayMinidriverDX9::~VDVideoDisplayMinidriverDX9() {
}

bool VDVideoDisplayMinidriverDX9::Init(HWND hwnd, const VDVideoDisplaySourceInfo& info) {
	mhwnd = hwnd;
	mSource = info;

	// attempt to initialize D3D9
	mpManager = VDInitDisplayDX9(this);
	if (!mpManager) {
		Shutdown();
		return false;
	}

	mpD3DDevice = mpManager->GetDevice();

	// check capabilities
	const D3DCAPS9& caps = mpManager->GetCaps();

	if (caps.MaxTextureWidth < info.pixmap.w || caps.MaxTextureHeight < info.pixmap.h) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: source image is larger than maximum texture size\n");
		Shutdown();
		return false;
	}

	// create source texture
	int texw = info.pixmap.w;
	int texh = info.pixmap.h;

	mpManager->AdjustTextureSize(texw, texh);

//	HRESULT hr = mpD3DDevice->CreateTexture(texw, texh, 0, D3DUSAGE_AUTOGENMIPMAP, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &mpD3DImageTexture, NULL);
	HRESULT hr = mpD3DDevice->CreateTexture(texw, texh, 1, 0/*D3DUSAGE_AUTOGENMIPMAP*/, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &mpD3DImageTexture, NULL);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	// clear source texture
	D3DLOCKED_RECT lr;
	if (SUCCEEDED(mpD3DImageTexture->LockRect(0, &lr, NULL, 0))) {
		void *p = lr.pBits;
		for(int h=0; h<texh; ++h) {
			memset(p, 0, 4*texw);
			vdptroffset(p, lr.Pitch);
		}
		mpD3DImageTexture->UnlockRect(0);
	}

	memset(&mTexFmt, 0, sizeof mTexFmt);
	mTexFmt.format		= nsVDPixmap::kPixFormat_XRGB8888;
	mTexFmt.w			= texw;
	mTexFmt.h			= texh;

	VDDEBUG_DX9DISP("VideoDisplay/DX9: Initialization successful for %dx%d source image.\n", mSource.pixmap.w, mSource.pixmap.h);

	return true;
}

void VDVideoDisplayMinidriverDX9::InitBicubic() {
	if (!mbCubicInitialized && !mbCubicAttempted) {
		mbCubicAttempted = true;

		mCubicMode = mpManager->InitBicubic();

		if (mCubicMode != VDVideoDisplayDX9Manager::kCubicNotPossible) {
			int texw = mpManager->GetPresentParms().BackBufferWidth;
			int texh = mSource.pixmap.h;

			mpManager->AdjustTextureSize(texw, texh);

			mRTHorizUSize = texw;
			mRTHorizVSize = texh;
			mRTHorizUScale = 1.0f / texw;
			mRTHorizVScale = 1.0f / texh;

			HRESULT hr = mpD3DDevice->CreateTexture(texw, texh, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &mpD3DRTHoriz, NULL);
			if (FAILED(hr)) {
				mpManager->ShutdownBicubic();
				mCubicMode = VDVideoDisplayDX9Manager::kCubicNotPossible;
			}

			if (mCubicMode == VDVideoDisplayDX9Manager::kCubicNotPossible) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Bicubic initialization failed -- falling back to bilinear path.\n");
				if (mpD3DRTHoriz) {
					mpD3DRTHoriz->Release();
					mpD3DRTHoriz = NULL;
				}
			} else {
				mpManager->ClearRenderTarget(mpD3DRTHoriz);

				VDDEBUG_DX9DISP("VideoDisplay/DX9: Bicubic initialization complete.\n");
				if (mCubicMode == VDVideoDisplayDX9Manager::kCubicUsePS1_4Path)
					VDDEBUG_DX9DISP("VideoDisplay/DX9: Using pixel shader 1.4, 5 texture (RADEON 8xxx+ / GeForceFX+) pixel path.\n");
				else if (mCubicMode == VDVideoDisplayDX9Manager::kCubicUsePS1_1Path)
					VDDEBUG_DX9DISP("VideoDisplay/DX9: Using pixel shader 1.1, 3 texture (GeForce3/4) pixel path.\n");
				else if (mCubicMode == VDVideoDisplayDX9Manager::kCubicUseFF3Path)
					VDDEBUG_DX9DISP("VideoDisplay/DX9: Using fixed function, 3 texture (RADEON 7xxx) pixel path.\n");
				else
					VDDEBUG_DX9DISP("VideoDisplay/DX9: Using fixed function, 2 texture (GeForce2) pixel path.\n");

				mbCubicInitialized = true;
			}
		}
	}
}

void VDVideoDisplayMinidriverDX9::ShutdownBicubic() {
	if (mpD3DRTHoriz) {
		mpD3DRTHoriz->Release();
		mpD3DRTHoriz = NULL;
	}

	if (mbCubicInitialized) {
		mbCubicInitialized = mbCubicAttempted = false;
		mpManager->ShutdownBicubic();
	}
}

void VDVideoDisplayMinidriverDX9::Shutdown() {
	if (mpD3DImageTexture) {
		mpD3DImageTexture->Release();
		mpD3DImageTexture = NULL;
	}

	ShutdownBicubic();

	if (mpManager) {
		VDDeinitDisplayDX9(mpManager, this);
		mpManager = NULL;
	}

	mbCubicAttempted = false;
}

bool VDVideoDisplayMinidriverDX9::ModifySource(const VDVideoDisplaySourceInfo& info) {
	if (mSource.pixmap.w == info.pixmap.w && mSource.pixmap.h == info.pixmap.h && mSource.pixmap.format == info.pixmap.format && mSource.pixmap.pitch == info.pixmap.pitch
		&& !mSource.bPersistent) {
		mSource = info;
		return true;
	}
	return false;
}

bool VDVideoDisplayMinidriverDX9::IsValid() {
	return mpD3DDevice != 0;
}

void VDVideoDisplayMinidriverDX9::SetFilterMode(FilterMode mode) {
	mPreferredFilter = mode;

	if (mode != kFilterBicubic && mode != kFilterAnySuitable && mbCubicInitialized)
		ShutdownBicubic();
}

bool VDVideoDisplayMinidriverDX9::Tick(int id) {
	return true;
}

bool VDVideoDisplayMinidriverDX9::Resize() {
	return true;
}

bool VDVideoDisplayMinidriverDX9::Update(FieldMode) {
	D3DLOCKED_RECT lr;
	HRESULT hr;
	
	hr = mpD3DImageTexture->LockRect(0, &lr, NULL, 0);
	if (FAILED(hr))
		return false;

	mTexFmt.data		= lr.pBits;
	mTexFmt.pitch		= lr.Pitch;

	VDPixmapBlt(mTexFmt, mSource.pixmap);

	VDVERIFY(SUCCEEDED(mpD3DImageTexture->UnlockRect(0)));

	return true;
}

void VDVideoDisplayMinidriverDX9::Refresh(FieldMode) {
	InvalidateRect(mhwnd, NULL, FALSE);
}

bool VDVideoDisplayMinidriverDX9::Paint(HDC hdc, const RECT& rClient) {
	// Make sure the device is sane.
	if (!mpManager->CheckDevice())
		return false;

	// Do we need to switch bicubic modes?
	FilterMode mode = mPreferredFilter;

	if (mode == kFilterAnySuitable)
		mode = kFilterBicubic;

	if (mode != kFilterBicubic && mbCubicInitialized)
		ShutdownBicubic();
	else if (mode == kFilterBicubic && !mbCubicInitialized && !mbCubicAttempted)
		InitBicubic();

	mpManager->ResetBuffers();


	const D3DMATRIX ident={
		1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1
	};

	D3D_DO(SetTransform(D3DTS_WORLD, &ident));
	D3D_DO(SetTransform(D3DTS_VIEW, &ident));

	const D3DMATRIX proj1={
		1.0f,
		0.0f,
		0.0f,
		0.0f,

		0.0f,
		1.0f,
		0.0f,
		0.0f,

		0.0f,
		0.0f,
		1.0f,
		0.0f,

		-1.00f / mRTHorizUSize,
		+1.00f / mRTHorizVSize,
		0.0f,
		1.0f,
	};

	const D3DMATRIX proj2={
		1.0f,
		0.0f,
		0.0f,
		0.0f,

		0.0f,
		1.0f,
		0.0f,
		0.0f,

		0.0f,
		0.0f,
		1.0f,
		0.0f,

		-1.00f / mpManager->GetVertUSize(),
		+1.00f / mpManager->GetVertVSize(),
		0.0f,
		1.0f,
	};

	const D3DMATRIX proj3={
		1.0f,
		0.0f,
		0.0f,
		0.0f,

		0.0f,
		1.0f,
		0.0f,
		0.0f,

		0.0f,
		0.0f,
		1.0f,
		0.0f,

		-1.00f / rClient.right,
		+1.00f / rClient.bottom,
		0.0f,
		1.0f,
	};

	mHorizProjection = proj1;
	mVertProjection = proj2;
	mWholeProjection = proj3;

	D3D_DO(SetStreamSource(0, mpManager->GetVertexBuffer(), 0, sizeof(Vertex)));
	D3D_DO(SetIndices(mpManager->GetIndexBuffer()));
	D3D_DO(SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX5));
	D3D_DO(SetRenderState(D3DRS_LIGHTING, FALSE));

	bool bSuccess;

	if (mbCubicInitialized) {
		switch(mCubicMode) {
		case VDVideoDisplayDX9Manager::kCubicUsePS1_4Path:
			bSuccess = Paint_PS1_4(hdc, rClient);
			break;
		case VDVideoDisplayDX9Manager::kCubicUsePS1_1Path:
			bSuccess = Paint_PS1_1(hdc, rClient);
			break;
		case VDVideoDisplayDX9Manager::kCubicUseFF3Path:
			bSuccess = Paint_FF3(hdc, rClient);
			break;
		case VDVideoDisplayDX9Manager::kCubicUseFF2Path:
//			bSuccess = Paint_FF2_Fast(hdc, rClient);
			bSuccess = Paint_FF2(hdc, rClient);
			break;
		}
	} else {
		D3D_DO(SetRenderTarget(0, mpManager->GetRenderTarget()));
		VDVERIFY(SUCCEEDED(mpD3DDevice->BeginScene()));

		D3DVIEWPORT9 vp = {
			0,
			0,
			rClient.right,
			rClient.bottom,
			0.f,
			1.f
		};
		VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vp)));
		D3D_DO(SetTransform(D3DTS_PROJECTION, &mWholeProjection));

		if (Vertex *pvx = mpManager->LockVertices(4)) {
			float umax = (float)mSource.pixmap.w / (float)mTexFmt.w;
			float vmax = (float)mSource.pixmap.h / (float)mTexFmt.h;

			pvx[0].SetFF1(-1, +1, 0, 0);
			pvx[1].SetFF1(-1, -1, 0, vmax);
			pvx[2].SetFF1(+1, -1, umax, vmax);
			pvx[3].SetFF1(+1, +1, umax, 0);

			mpManager->UnlockVertices();
		}

		if (uint16 *dst = mpManager->LockIndices(6)) {
			dst[0] = 0;
			dst[1] = 2;
			dst[2] = 1;
			dst[3] = 0;
			dst[4] = 3;
			dst[5] = 2;

			mpManager->UnlockIndices();
		}

		mpManager->SetTextureStageOp(0, D3DTA_TEXTURE, D3DTOP_SELECTARG1, D3DTA_CURRENT, D3DTA_TEXTURE, D3DTOP_SELECTARG1, D3DTA_CURRENT);
		mpManager->DisableTextureStage(1);
		mpManager->DisableTextureStage(2);
		D3D_DO(SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
		D3D_DO(SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));

		if (mPreferredFilter == kFilterPoint) {
			D3D_DO(SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT));
			D3D_DO(SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT));
		} else {
			D3D_DO(SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR));
			D3D_DO(SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR));
			D3D_DO(SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR));
		}
		D3D_DO(SetTexture(0, mpD3DImageTexture));

		D3D_DO(SetRenderState(D3DRS_DITHERENABLE, TRUE));

		mpManager->DrawElements(D3DPT_TRIANGLELIST, 0, 4, 0, 2);

		D3D_DO(SetTexture(0, NULL));
		D3D_DO(EndScene());

		bSuccess = true;
	}

	HRESULT hr = E_FAIL;

	if (bSuccess)
		hr = mpD3DDevice->Present(&rClient, NULL, mhwnd, NULL);

	if (FAILED(hr)) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: Render failed -- applying boot to the head.\n");

		// TODO: Need to free all DEFAULT textures before proceeding

		if (mpManager->Reset())
			return S_OK;
	}

	return SUCCEEDED(hr);
}

///////////////////////////////////////////////////////////////////////////
//
//
//	Fixed function path - 2 texture stages (NVIDIA GeForce2 / GeForce4 MX)
//
//
///////////////////////////////////////////////////////////////////////////

bool VDVideoDisplayMinidriverDX9::Paint_FF2(HDC hdc, const RECT& rClient) {
	D3D_DO(SetRenderState(D3DRS_LIGHTING, FALSE));

	// PASS 1: Horizontal filtering

	IDirect3DSurface9 *pRTSurface;
	VDVERIFY(SUCCEEDED(mpD3DRTHoriz->GetSurfaceLevel(0, &pRTSurface)));
	D3D_DO(SetRenderTarget(0, pRTSurface));
	pRTSurface->Release();

	D3D_DO(BeginScene());

	if (Vertex *pvx = mpManager->LockVertices(12)) {
		const float ustep = 1.0f / (float)mTexFmt.w;
		const float vstep = 1.0f / (float)mTexFmt.h;
		const float u0 = -0.5f * ustep;
		const float u1 = u0 + mSource.pixmap.w * ustep * 2;
		const float v0 = -mSource.pixmap.h * vstep;
		const float v1 = +mSource.pixmap.h * vstep;
		const float f0 = -0.125f;
		const float f1 = f0 + mSource.pixmap.w / 2.0f;

		pvx[ 0].SetFF2(-1, +3, 0x40808080, u0 - 1*ustep, v0, f0, 0.125f);
		pvx[ 1].SetFF2(+3, -1, 0x40808080, u1 - 1*ustep, v1, f1, 0.125f);
		pvx[ 2].SetFF2(-1, -1, 0x40808080, u0 - 1*ustep, v1, f0, 0.125f);

		pvx[ 3].SetFF2(-1, +3, 0x40808080, u0 + 0*ustep, v0, f0, 0.375f);
		pvx[ 4].SetFF2(+3, -1, 0x40808080, u1 + 0*ustep, v1, f1, 0.375f);
		pvx[ 5].SetFF2(-1, -1, 0x40808080, u0 + 0*ustep, v1, f0, 0.375f);

		pvx[ 6].SetFF2(-1, +3, 0x40808080, u0 + 1*ustep, v0, f0, 0.625f);
		pvx[ 7].SetFF2(+3, -1, 0x40808080, u1 + 1*ustep, v1, f1, 0.625f);
		pvx[ 8].SetFF2(-1, -1, 0x40808080, u0 + 1*ustep, v1, f0, 0.625f);

		pvx[ 9].SetFF2(-1, +3, 0x40808080, u0 + 2*ustep, v0, f0, 0.875f);
		pvx[10].SetFF2(+3, -1, 0x40808080, u1 + 2*ustep, v1, f1, 0.875f);
		pvx[11].SetFF2(-1, -1, 0x40808080, u0 + 2*ustep, v1, f0, 0.875f);

		mpManager->UnlockVertices();
	}

	D3DVIEWPORT9 vphoriz = {
		0,
		0,
		rClient.right,
		mSource.pixmap.h,
		0.f,
		1.f
	};
	D3D_DO(SetTransform(D3DTS_PROJECTION, &mHorizProjection));

	VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vphoriz)));

	D3D_DO(SetTexture(0, mpD3DImageTexture));
	D3D_DO(SetTexture(1, mpManager->GetFilterTexture()));

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUseFF2Path, 0);
	mpManager->DrawArrays(D3DPT_TRIANGLELIST, 3, 1);

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUseFF2Path, 1);
	mpManager->DrawArrays(D3DPT_TRIANGLELIST, 6, 1);

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUseFF2Path, 2);
	mpManager->DrawArrays(D3DPT_TRIANGLELIST, 0, 1);
	mpManager->DrawArrays(D3DPT_TRIANGLELIST, 9, 1);

	D3D_DO(EndScene());

	// PASS 2: Vertical filtering

	VDVERIFY(SUCCEEDED(mpManager->GetVertRenderTexture()->GetSurfaceLevel(0, &pRTSurface)));
	D3D_DO(SetRenderTarget(0, pRTSurface));
	pRTSurface->Release();

	D3D_DO(BeginScene());
	D3D_DO(SetTransform(D3DTS_PROJECTION, &mWholeProjection));

	if (Vertex *pvx = mpManager->LockVertices(12)) {
		const float ustep = mRTHorizUScale;
		const float vstep = mRTHorizVScale;
		const float u0 = 0.0f;
		const float v0 = -(mSource.pixmap.h + 0.5f) * vstep;
		const float u1 = rClient.right * ustep * 2;
		const float v1 = (mSource.pixmap.h - 0.5f) * vstep;
		const float f0 = 0.125f;
		const float f1 = f0 + mSource.pixmap.h / 2.0f;

		pvx[ 0].SetFF2(-1, +3, 0xFFFFFFFF, u0, v0 - 1*vstep, f0, 0.125f);
		pvx[ 1].SetFF2(+3, -1, 0xFFFFFFFF, u1, v1 - 1*vstep, f1, 0.125f);
		pvx[ 2].SetFF2(-1, -1, 0xFFFFFFFF, u0, v1 - 1*vstep, f1, 0.125f);

		pvx[ 3].SetFF2(-1, +3, 0xFFFFFFFF, u0, v0 + 0*vstep, f0, 0.375f);
		pvx[ 4].SetFF2(+3, -1, 0xFFFFFFFF, u1, v1 + 0*vstep, f1, 0.375f);
		pvx[ 5].SetFF2(-1, -1, 0xFFFFFFFF, u0, v1 + 0*vstep, f1, 0.375f);

		pvx[ 6].SetFF2(-1, +3, 0xFFFFFFFF, u0, v0 + 1*vstep, f0, 0.625f);
		pvx[ 7].SetFF2(+3, -1, 0xFFFFFFFF, u1, v1 + 1*vstep, f1, 0.625f);
		pvx[ 8].SetFF2(-1, -1, 0xFFFFFFFF, u0, v1 + 1*vstep, f1, 0.625f);

		pvx[ 9].SetFF2(-1, +3, 0xFFFFFFFF, u0, v0 + 2*vstep, f0, 0.875f);
		pvx[10].SetFF2(+3, -1, 0xFFFFFFFF, u1, v1 + 2*vstep, f1, 0.875f);
		pvx[11].SetFF2(-1, -1, 0xFFFFFFFF, u0, v1 + 2*vstep, f1, 0.875f);

		mpManager->UnlockVertices();
	}

	D3D_DO(SetRenderState(D3DRS_DITHERENABLE, FALSE));

	D3DVIEWPORT9 vpfb = {
		0,
		0,
		rClient.right,
		rClient.bottom,
		0.f,
		1.f
	};
	VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vpfb)));

	D3D_DO(SetTexture(0, mpD3DRTHoriz));
	D3D_DO(SetTexture(1, mpManager->GetFilterTexture()));

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUseFF2Path, 3);
	mpManager->DrawArrays(D3DPT_TRIANGLELIST, 3, 1);

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUseFF2Path, 4);
	mpManager->DrawArrays(D3DPT_TRIANGLELIST, 6, 1);

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUseFF2Path, 5);
	mpManager->DrawArrays(D3DPT_TRIANGLELIST, 0, 1);
	mpManager->DrawArrays(D3DPT_TRIANGLELIST, 9, 1);

	D3D_DO(SetTexture(1, NULL));
	D3D_DO(SetTexture(0, NULL));
	D3D_DO(EndScene());

	D3D_DO(SetRenderTarget(0, mpManager->GetRenderTarget()));
	D3D_DO(BeginScene());

	D3DVIEWPORT9 vpmain = {
		0,
		0,
		rClient.right,
		rClient.bottom,
		0.f,
		1.f
	};
	VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vpmain)));

	if (Vertex *pvx = mpManager->LockVertices(3)) {
		const float ustep = mpManager->GetVertUScale();
		const float vstep = mpManager->GetVertVScale();
		const float u0 = 0.0f;
		const float v0 = -rClient.bottom * vstep;
		const float u1 = rClient.right * ustep * 2;
		const float v1 = rClient.bottom * vstep;

		pvx[ 0].SetFF2(-1, +3, 0x40404040, u0, v0, 0.f, 0.f);
		pvx[ 1].SetFF2(+3, -1, 0x40404040, u1, v1, 0.f, 0.f);
		pvx[ 2].SetFF2(-1, -1, 0x40404040, u0, v1, 0.f, 0.f);

		mpManager->UnlockVertices();
	}

	D3D_DO(SetTexture(0, mpManager->GetVertRenderTexture()));

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUseFF2Path, 6);
	mpManager->DrawArrays(D3DPT_TRIANGLELIST, 0, 1);

	D3D_DO(SetTexture(0, NULL));
	D3D_DO(EndScene());

	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//
//	Fixed function path - 2 texture stages + 2 color channels (NVIDIA GeForce2 / GeForce4 MX)
//
//
///////////////////////////////////////////////////////////////////////////

bool VDVideoDisplayMinidriverDX9::Paint_FF2_Fast(HDC hdc, const RECT& rClient) {
	D3D_DO(SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE));
	D3D_DO(SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE));

	D3D_DO(SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0));
	D3D_DO(SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP));
	D3D_DO(SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP));
	D3D_DO(SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
	D3D_DO(SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT));
	D3D_DO(SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_POINT));

	D3D_DO(SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1));
	D3D_DO(SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP));
	D3D_DO(SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP));
	D3D_DO(SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_POINT));
	D3D_DO(SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_POINT));
	D3D_DO(SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_POINT));

	uint32 texture[4][256];

	mpManager->MakeCubic4Texture(&texture[0][0], 256*sizeof(uint32), -0.75, VDVideoDisplayDX9Manager::kCubicUseFF2Path);

	// PASS 1: Horizontal filtering

	IDirect3DSurface9 *pRTSurface;
	VDVERIFY(SUCCEEDED(mpD3DRTHoriz->GetSurfaceLevel(0, &pRTSurface)));
	D3D_DO(SetRenderTarget(0, pRTSurface));
	pRTSurface->Release();

	D3D_DO(BeginScene());

	mpManager->SetTextureStageOp(0, D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_DIFFUSE, D3DTA_CURRENT, D3DTOP_SELECTARG1, D3DTA_CURRENT);
	mpManager->SetTextureStageOp(1, D3DTA_DIFFUSE | D3DTA_ALPHAREPLICATE, D3DTOP_MULTIPLYADD, D3DTA_TEXTURE, D3DTA_CURRENT, D3DTOP_SELECTARG1, D3DTA_CURRENT);
	D3D_DO(SetTextureStageState(1, D3DTSS_COLORARG0, D3DTA_CURRENT));

	D3DVIEWPORT9 vphoriz = {
		0,
		0,
		mRTHorizUSize,
		mRTHorizVSize,
		0.f,
		1.f
	};
	D3D_DO(SetTransform(D3DTS_PROJECTION, &mHorizProjection));

	VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vphoriz)));

	D3D_DO(SetTexture(0, mpD3DImageTexture));
	D3D_DO(SetTexture(1, mpD3DImageTexture));

	for(int hpass=0; hpass<2; ++hpass) {
		const float ufactor = 1.0f / (float)mTexFmt.w;
		const float vfactor = 1.0f / (float)mTexFmt.h;
		const float xstep = 2.0f * mRTHorizUScale;

		uint32 filtstep		= (mSource.pixmap.w << 16) / rClient.right;
		uint32 filtaccum	= 0-0x8000+(filtstep >> 1);

		const float y0 = 1.f - mSource.pixmap.h * (2.0f * mRTHorizVScale);
		const float y1 = 1.f;
		const float v0 = 0.0f;
		const float v1 = v0 + mSource.pixmap.h * vfactor;

		float x0 = -1.0f;
		float x1 = -1.0f + xstep;
		const float ustep = (float)mSource.pixmap.w / (float)rClient.right * ufactor;
		float u0 = 0.5f * ustep;
		float u1 = u0;

		if (hpass == 0) {
			u0 += -0.5f * ufactor;
			u1 += +0.5f * ufactor;
		} else {
			u0 += -1.5f * ufactor;
			u1 += +1.5f * ufactor;
		}

		if (!hpass) {
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
		} else {
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
			D3D_DO(SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT));
		}

		const uint32 *coeffs = texture[!hpass];

		const float udelta = hpass ? 3*ufactor : ufactor;

		int col = 0;
		while(col < rClient.right) {
			int ncols = rClient.right - col;

			if (ncols > kVertexBufferSize / 4)
				ncols = kVertexBufferSize / 4;

			if (Vertex *pvx = mpManager->LockVertices(ncols * 4)) {
				for(int limit = col + ncols; col < limit; ++col) {
					uint32 coeff0 = (coeffs[(filtaccum>>10)&63] & 0xfefefefe)>>1;
					float u = ((filtaccum>>16) + 0.5 + (hpass?-2.0:-1.0)) * ufactor;

					pvx[0].SetFF2(x0, y1, coeff0, u, v0, u+udelta, v0);
					pvx[1].SetFF2(x0, y0, coeff0, u, v1, u+udelta, v1);
					pvx[2].SetFF2(x1, y0, coeff0, u, v1, u+udelta, v1);
					pvx[3].SetFF2(x1, y1, coeff0, u, v0, u+udelta, v0);

					x0 += xstep;
					x1 += xstep;
					u0 += ustep;
					u1 += ustep;
					filtaccum += filtstep;

					pvx += 4;
				}

				mpManager->UnlockVertices();
			}

			if (uint16 *pvx = mpManager->LockIndices(ncols * 6)) {
				for(int i=0; i<ncols; ++i) {
					pvx[0] = (uint16)(i*4 + 0);
					pvx[1] = (uint16)(i*4 + 2);
					pvx[2] = (uint16)(i*4 + 1);
					pvx[3] = (uint16)(i*4 + 0);
					pvx[4] = (uint16)(i*4 + 3);
					pvx[5] = (uint16)(i*4 + 2);
					pvx += 6;
				}

				mpManager->UnlockIndices();
			}

			mpManager->DrawElements(D3DPT_TRIANGLELIST, 0, ncols * 4, 0, ncols * 2);
		}
	}

	D3D_DO(SetTexture(1, NULL));
	D3D_DO(SetTexture(0, NULL));
	D3D_DO(EndScene());

	// PASS 2: Vertical filtering

	VDVERIFY(SUCCEEDED(mpManager->GetVertRenderTexture()->GetSurfaceLevel(0, &pRTSurface)));
	D3D_DO(SetRenderTarget(0, pRTSurface));
	pRTSurface->Release();

	D3D_DO(BeginScene());
	D3D_DO(SetTransform(D3DTS_PROJECTION, &mVertProjection));

	mpManager->SetTextureStageOp(0, D3DTA_TEXTURE, D3DTOP_MODULATE, D3DTA_DIFFUSE, D3DTA_CURRENT, D3DTOP_SELECTARG1, D3DTA_CURRENT);
	mpManager->SetTextureStageOp(1, D3DTA_DIFFUSE | D3DTA_ALPHAREPLICATE, D3DTOP_MULTIPLYADD, D3DTA_TEXTURE, D3DTA_CURRENT, D3DTOP_SELECTARG1, D3DTA_CURRENT);
	D3D_DO(SetTextureStageState(1, D3DTSS_COLORARG0, D3DTA_CURRENT));

	D3D_DO(SetRenderState(D3DRS_DITHERENABLE, FALSE));

	D3DVIEWPORT9 vpfb = {
		0,
		0,
		mpManager->GetVertUSize(),
		mpManager->GetVertVSize(),
		0.f,
		1.f
	};
	VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vpfb)));

	D3D_DO(SetTexture(0, mpD3DRTHoriz));
	D3D_DO(SetTexture(1, mpD3DRTHoriz));

	for(int vpass=0; vpass<2; ++vpass) {
		const float ufactor = mRTHorizUScale;
		const float vfactor = mRTHorizVScale;
		const float ystep = 2.0f * mpManager->GetVertVScale();

		uint32 filtstep		= (mSource.pixmap.h << 16) / rClient.bottom;
		uint32 filtaccum	= 0-0x8000+(filtstep >> 1);

		const float x0 = -1.f;
		const float x1 = -1.f + rClient.right * (2.0f * mpManager->GetVertUScale());
		const float u0 = 0.0f;
		const float u1 = rClient.right * ufactor;

		float y0 = 1.0f;
		float y1 = 1.0f - ystep;
		const float vstep = (float)mSource.pixmap.h / (float)rClient.bottom * vfactor;
		float v0 = 0.5f * vstep;
		float v1 = v0;

		if (vpass == 0) {
			v0 += -0.5f * vfactor;
			v1 += +0.5f * vfactor;
		} else {
			v0 += -1.5f * vfactor;
			v1 += +1.5f * vfactor;
		}

		if (!vpass) {
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
		} else {
			D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE));
			D3D_DO(SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT));
		}

		const uint32 *coeffs = texture[!vpass];
		const float vdelta = vpass ? 3*vfactor : vfactor;

		int row = 0;
		while(row < rClient.bottom) {
			int nrows = rClient.bottom - row;

			if (nrows > kVertexBufferSize / 4)
				nrows = kVertexBufferSize / 4;

			if (Vertex *pvx = mpManager->LockVertices(nrows * 4)) {
				for(int limit = row + nrows; row < limit; ++row) {
					uint32 coeff0 = coeffs[(filtaccum>>10)&63];
					float v = ((filtaccum>>16) + 0.5 + (vpass?-2.0:-1.0)) * vfactor;

					pvx[0].SetFF2(x0, y1, coeff0, u0, v, u0, v+vdelta);
					pvx[1].SetFF2(x1, y1, coeff0, u1, v, u1, v+vdelta);
					pvx[2].SetFF2(x1, y0, coeff0, u1, v, u1, v+vdelta);
					pvx[3].SetFF2(x0, y0, coeff0, u0, v, u0, v+vdelta);

					y0 -= ystep;
					y1 -= ystep;
					v0 += vstep;
					v1 += vstep;
					filtaccum += filtstep;

					pvx += 4;
				}

				mpManager->UnlockVertices();
			}

			if (uint16 *pvx = mpManager->LockIndices(nrows * 6)) {
				for(int i=0; i<nrows; ++i) {
					pvx[0] = (uint16)(i*4 + 0);
					pvx[1] = (uint16)(i*4 + 2);
					pvx[2] = (uint16)(i*4 + 1);
					pvx[3] = (uint16)(i*4 + 0);
					pvx[4] = (uint16)(i*4 + 3);
					pvx[5] = (uint16)(i*4 + 2);
					pvx += 6;
				}

				mpManager->UnlockIndices();
			}

			mpManager->DrawElements(D3DPT_TRIANGLELIST, 0, nrows * 4, 0, nrows * 2);
		}
	}

	D3D_DO(SetTexture(1, NULL));
	D3D_DO(SetTexture(0, NULL));
	D3D_DO(EndScene());

	D3D_DO(SetRenderTarget(0, mpManager->GetRenderTarget()));
	D3D_DO(BeginScene());
	D3D_DO(SetTransform(D3DTS_PROJECTION, &mWholeProjection));

	D3DVIEWPORT9 vpmain = {
		0,
		0,
		rClient.right,
		rClient.bottom,
		0.f,
		1.f
	};
	VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vpmain)));

	mpManager->SetTextureStageOp(0, D3DTA_TEXTURE, D3DTOP_ADD, D3DTA_TEXTURE, D3DTA_TEXTURE, D3DTOP_ADD, D3DTA_TEXTURE);
	mpManager->DisableTextureStage(1);
	mpManager->DisableTextureStage(2);

	if (uint16 *dst = mpManager->LockIndices(6)) {
		dst[0] = 0;
		dst[1] = 2;
		dst[2] = 1;
		dst[3] = 0;
		dst[4] = 3;
		dst[5] = 2;

		mpManager->UnlockIndices();
	}

	if (Vertex *pvx = mpManager->LockVertices(4)) {
		const float ustep = mpManager->GetVertUScale();
		const float vstep = mpManager->GetVertVScale();
		const float u0 = 0.0f;
		const float v0 = 0.0f;
		const float u1 = u0 + rClient.right * ustep;
		const float v1 = v0 + rClient.bottom * vstep;

		pvx[ 0].SetFF2(-1, +1, 0x40404040, u0, v0, 0.f, 0.f);
		pvx[ 1].SetFF2(-1, -1, 0x40404040, u0, v1, 0.f, 0.f);
		pvx[ 2].SetFF2(+1, -1, 0x40404040, u1, v1, 0.f, 0.f);
		pvx[ 3].SetFF2(+1, +1, 0x40404040, u1, v0, 0.f, 0.f);

		mpManager->UnlockVertices();
	}

	D3D_DO(SetTexture(0, mpManager->GetVertRenderTexture()));
	D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
	D3D_DO(SetRenderState(D3DRS_DITHERENABLE, TRUE));

	mpManager->DrawElements(D3DPT_TRIANGLELIST, 0, 4, 0, 2);

	D3D_DO(SetTexture(0, NULL));
	D3D_DO(EndScene());
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//
//	Fixed function path - 3 texture stages (ATI RADEON)
//
//
///////////////////////////////////////////////////////////////////////////

bool VDVideoDisplayMinidriverDX9::Paint_FF3(HDC hdc, const RECT& rClient) {
	if (uint16 *dst = mpManager->LockIndices(6)) {
		dst[0] = 0;
		dst[1] = 2;
		dst[2] = 1;
		dst[3] = 0;
		dst[4] = 3;
		dst[5] = 2;

		mpManager->UnlockIndices();
	}

	// PASS 1: Horizontal filtering

	IDirect3DSurface9 *pRTSurface;
	VDVERIFY(SUCCEEDED(mpD3DRTHoriz->GetSurfaceLevel(0, &pRTSurface)));
	D3D_DO(SetRenderTarget(0, pRTSurface));
	pRTSurface->Release();

	D3D_DO(BeginScene());

	D3D_DO(Clear(0, NULL, D3DCLEAR_TARGET, 0x80808080, 0.f, 0));

	if (Vertex *pvx = mpManager->LockVertices(8)) {
		const float ustep = 1.0f / (float)mTexFmt.w;
		const float vstep = 1.0f / (float)mTexFmt.h;
		const float u0 = -0.5f * ustep;
		const float v0 = 0.0f;
		const float u1 = u0 + mSource.pixmap.w * ustep;
		const float v1 = v0 + mSource.pixmap.h * vstep;
		const float f0 = -0.125f;
		const float f1 = f0 + mSource.pixmap.w / 4.0f;
		const float x0 = -1.f;
		const float y0 = 1.f - mSource.pixmap.h * (2.0f * mRTHorizVScale);
		const float x1 = -1.f + rClient.right * (2.0f * mRTHorizUScale);
		const float y1 = 1.f;

		pvx[0].SetFF3(x0, y1, u0 + 0*ustep, v0, f0, 0.875f, u0 + 1*ustep, v0);
		pvx[1].SetFF3(x0, y0, u0 + 0*ustep, v1, f0, 0.875f, u0 + 1*ustep, v1);
		pvx[2].SetFF3(x1, y0, u1 + 0*ustep, v1, f1, 0.875f, u1 + 1*ustep, v1);
		pvx[3].SetFF3(x1, y1, u1 + 0*ustep, v0, f1, 0.875f, u1 + 1*ustep, v0);

		pvx[4].SetFF3(x0, y1, u0 - 1*ustep, v0, f0, 0.625f, u0 + 2*ustep, v0);
		pvx[5].SetFF3(x0, y0, u0 - 1*ustep, v1, f0, 0.625f, u0 + 2*ustep, v1);
		pvx[6].SetFF3(x1, y0, u1 - 1*ustep, v1, f1, 0.625f, u1 + 2*ustep, v1);
		pvx[7].SetFF3(x1, y1, u1 - 1*ustep, v0, f1, 0.625f, u1 + 2*ustep, v0);

		mpManager->UnlockVertices();
	}

	D3DVIEWPORT9 vphoriz = {
		0,
		0,
		mRTHorizUSize,
		mRTHorizVSize,
		0.f,
		1.f
	};
	D3D_DO(SetTransform(D3DTS_PROJECTION, &mHorizProjection));

	VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vphoriz)));

	D3D_DO(SetTexture(0, mpD3DImageTexture));
	D3D_DO(SetTexture(1, mpManager->GetFilterTexture()));
	D3D_DO(SetTexture(2, mpD3DImageTexture));

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUseFF3Path, 0);
	mpManager->DrawElements(D3DPT_TRIANGLELIST, 4, 4, 0, 2);

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUseFF3Path, 1);
	mpManager->DrawElements(D3DPT_TRIANGLELIST, 0, 4, 0, 2);

	D3D_DO(EndScene());

	// PASS 2: Vertical filtering

	VDVERIFY(SUCCEEDED(mpManager->GetVertRenderTexture()->GetSurfaceLevel(0, &pRTSurface)));
	D3D_DO(SetRenderTarget(0, pRTSurface));
	pRTSurface->Release();

	D3D_DO(BeginScene());
	D3D_DO(SetTransform(D3DTS_PROJECTION, &mVertProjection));

	D3D_DO(Clear(0, NULL, D3DCLEAR_TARGET, 0x80808080, 0.f, 0));

	if (Vertex *pvx = mpManager->LockVertices(8)) {
		const float ustep = mRTHorizUScale;
		const float vstep = mRTHorizVScale;
		const float u0 = 0.0f;
		const float v0 = -0.5f * vstep;
		const float u1 = u0 + rClient.right * ustep;
		const float v1 = v0 + mSource.pixmap.h * vstep;
		const float f0 = 0.125f;
		const float f1 = f0 + mSource.pixmap.h / 4.0f;

		pvx[0].SetFF3(-1, +1, u0, v0 + 0*vstep, f0, 0.375f, u0, v0 + 1*vstep);
		pvx[1].SetFF3(-1, -1, u0, v1 + 0*vstep, f1, 0.375f, u0, v1 + 1*vstep);
		pvx[2].SetFF3(+1, -1, u1, v1 + 0*vstep, f1, 0.375f, u1, v1 + 1*vstep);
		pvx[3].SetFF3(+1, +1, u1, v0 + 0*vstep, f0, 0.375f, u1, v0 + 1*vstep);

		pvx[4].SetFF3(-1, +1, u0, v0 - 1*vstep, f0, 0.125f, u0, v0 + 2*vstep);
		pvx[5].SetFF3(-1, -1, u0, v1 - 1*vstep, f1, 0.125f, u0, v1 + 2*vstep);
		pvx[6].SetFF3(+1, -1, u1, v1 - 1*vstep, f1, 0.125f, u1, v1 + 2*vstep);
		pvx[7].SetFF3(+1, +1, u1, v0 - 1*vstep, f0, 0.125f, u1, v0 + 2*vstep);

		mpManager->UnlockVertices();
	}

	D3DVIEWPORT9 vpfb = {
		0,
		0,
		rClient.right,
		rClient.bottom,
		0.f,
		1.f
	};
	VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vpfb)));

	D3D_DO(SetTexture(0, mpD3DRTHoriz));
	D3D_DO(SetTexture(1, mpManager->GetFilterTexture()));
	D3D_DO(SetTexture(2, mpD3DRTHoriz));

	// stupid RADEON doesn't support BLENDOP... #*$()#

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUseFF3Path, 2);
	mpManager->DrawElements(D3DPT_TRIANGLELIST, 4, 4, 0, 2);

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUseFF3Path, 3);
	mpManager->DrawElements(D3DPT_TRIANGLELIST, 0, 4, 0, 2);

	D3D_DO(SetTexture(2, NULL));
	D3D_DO(SetTexture(1, NULL));
	D3D_DO(EndScene());

	// PASS 3: Scanout

	D3D_DO(SetRenderTarget(0, mpManager->GetRenderTarget()));
	D3D_DO(BeginScene());

	D3DVIEWPORT9 vpmain = {
		0,
		0,
		rClient.right,
		rClient.bottom,
		0.f,
		1.f
	};
	VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vpmain)));

	D3D_DO(SetTransform(D3DTS_PROJECTION, &mWholeProjection));

	if (Vertex *pvx = mpManager->LockVertices(4)) {
		const float ustep = mpManager->GetVertUScale();
		const float vstep = mpManager->GetVertVScale();
		const float u0 = 0.0f;
		const float v0 = 0.0f;
		const float u1 = u0 + rClient.right * ustep;
		const float v1 = v0 + rClient.bottom * vstep;

		pvx[ 0].SetFF1(-1, +1, u0, v0);
		pvx[ 1].SetFF1(-1, -1, u0, v1);
		pvx[ 2].SetFF1(+1, -1, u1, v1);
		pvx[ 3].SetFF1(+1, +1, u1, v0);

		mpManager->UnlockVertices();
	}

	D3D_DO(SetTexture(0, mpManager->GetVertRenderTexture()));

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUseFF3Path, 4);
	mpManager->DrawElements(D3DPT_TRIANGLELIST, 0, 4, 0, 2);

	D3D_DO(SetTexture(0, NULL));
	D3D_DO(EndScene());

	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//
//	Programmmable path - 3 texture stages (NVIDIA GeForce3/4)
//
//
///////////////////////////////////////////////////////////////////////////


bool VDVideoDisplayMinidriverDX9::Paint_PS1_1(HDC hdc, const RECT& rClient) {
	if (uint16 *dst = mpManager->LockIndices(6)) {
		dst[0] = 0;
		dst[1] = 2;
		dst[2] = 1;
		dst[3] = 0;
		dst[4] = 3;
		dst[5] = 2;

		mpManager->UnlockIndices();
	}

	// PASS 1: Horizontal filtering

	if (Vertex *pvx = mpManager->LockVertices(8)) {
		const float ustep = 1.0f / (float)mTexFmt.w;
		const float vstep = 1.0f / (float)mTexFmt.h;
		const float u0 = -0.5f * ustep;
		const float v0 = 0.0f;
		const float u1 = u0 + mSource.pixmap.w * ustep;
		const float v1 = v0 + mSource.pixmap.h * vstep;
		const float f0 = -0.125f;
		const float f1 = f0 + mSource.pixmap.w / 4.0f;
		const float x0 = -1.f;
		const float y0 = 1.f - mSource.pixmap.h * (2.0f * mRTHorizVScale);
		const float x1 = -1.f + rClient.right * (2.0f * mRTHorizUScale);
		const float y1 = 1.f;

		pvx[0].SetFF3(x0, y1, u0 + 0*ustep, v0, f0, 0.375f, u0 + 1*ustep, v0);
		pvx[1].SetFF3(x0, y0, u0 + 0*ustep, v1, f0, 0.375f, u0 + 1*ustep, v1);
		pvx[2].SetFF3(x1, y0, u1 + 0*ustep, v1, f1, 0.375f, u1 + 1*ustep, v1);
		pvx[3].SetFF3(x1, y1, u1 + 0*ustep, v0, f1, 0.375f, u1 + 1*ustep, v0);

		pvx[4].SetFF3(x0, y1, u0 - 1*ustep, v0, f0, 0.125f, u0 + 2*ustep, v0);
		pvx[5].SetFF3(x0, y0, u0 - 1*ustep, v1, f0, 0.125f, u0 + 2*ustep, v1);
		pvx[6].SetFF3(x1, y0, u1 - 1*ustep, v1, f1, 0.125f, u1 + 2*ustep, v1);
		pvx[7].SetFF3(x1, y1, u1 - 1*ustep, v0, f1, 0.125f, u1 + 2*ustep, v0);

		mpManager->UnlockVertices();
	}

	IDirect3DSurface9 *pRTSurface;
	VDVERIFY(SUCCEEDED(mpD3DRTHoriz->GetSurfaceLevel(0, &pRTSurface)));
	D3D_DO(SetRenderTarget(0, pRTSurface));
	pRTSurface->Release();

	D3D_DO(BeginScene());

	static const float spec_add[4]={0.25f, 0.25f, 0.25f, 0.25f};

	D3D_DO(SetPixelShaderConstantF(0, spec_add, 1));

	D3DVIEWPORT9 vphoriz = {
		0,
		0,
		mRTHorizUSize,
		mRTHorizVSize,
		0.f,
		1.f
	};
	D3D_DO(SetTransform(D3DTS_PROJECTION, &mHorizProjection));

	VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vphoriz)));

	D3D_DO(SetTexture(0, mpD3DImageTexture));
	D3D_DO(SetTexture(1, mpManager->GetFilterTexture()));
	D3D_DO(SetTexture(2, mpD3DImageTexture));

	D3D_DO(SetPixelShader(mpManager->GetHPixelShader()));
	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUsePS1_1Path, 0);
	mpManager->DrawElements(D3DPT_TRIANGLELIST, 0, 4, 0, 2);

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUsePS1_1Path, 1);
	mpManager->DrawElements(D3DPT_TRIANGLELIST, 4, 4, 0, 2);

	D3D_DO(EndScene());

	// PASS 2: Vertical filtering

	if (Vertex *pvx = mpManager->LockVertices(8)) {
		const float ustep = mRTHorizUScale;
		const float vstep = mRTHorizVScale;
		const float u0 = 0.0f;
		const float v0 = -0.5f * vstep;
		const float u1 = u0 + rClient.right * ustep;
		const float v1 = v0 + mSource.pixmap.h * vstep;
		const float f0 = 0.125f;
		const float f1 = f0 + mSource.pixmap.h / 4.0f;

		pvx[0].SetFF3(-1, +1, u0, v0 + 0*vstep, f0, 0.375f, u0, v0 + 1*vstep);
		pvx[1].SetFF3(-1, -1, u0, v1 + 0*vstep, f1, 0.375f, u0, v1 + 1*vstep);
		pvx[2].SetFF3(+1, -1, u1, v1 + 0*vstep, f1, 0.375f, u1, v1 + 1*vstep);
		pvx[3].SetFF3(+1, +1, u1, v0 + 0*vstep, f0, 0.375f, u1, v0 + 1*vstep);

		pvx[4].SetFF3(-1, +1, u0, v0 - 1*vstep, f0, 0.125f, u0, v0 + 2*vstep);
		pvx[5].SetFF3(-1, -1, u0, v1 - 1*vstep, f1, 0.125f, u0, v1 + 2*vstep);
		pvx[6].SetFF3(+1, -1, u1, v1 - 1*vstep, f1, 0.125f, u1, v1 + 2*vstep);
		pvx[7].SetFF3(+1, +1, u1, v0 - 1*vstep, f0, 0.125f, u1, v0 + 2*vstep);

		mpManager->UnlockVertices();
	}

	VDVERIFY(SUCCEEDED(mpManager->GetVertRenderTexture()->GetSurfaceLevel(0, &pRTSurface)));
	D3D_DO(SetRenderTarget(0, pRTSurface));
	pRTSurface->Release();

	D3D_DO(BeginScene());
	D3D_DO(SetTransform(D3DTS_PROJECTION, &mVertProjection));
	D3D_DO(SetPixelShader(mpManager->GetVPixelShader()));

	D3D_DO(SetRenderState(D3DRS_DITHERENABLE, FALSE));

	D3DVIEWPORT9 vpfb = {
		0,
		0,
		rClient.right,
		rClient.bottom,
		0.f,
		1.f
	};
	VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vpfb)));

	D3D_DO(SetTexture(0, mpD3DRTHoriz));
	D3D_DO(SetTexture(1, mpManager->GetFilterTexture()));
	D3D_DO(SetTexture(2, mpD3DRTHoriz));

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUsePS1_1Path, 2);
	mpManager->DrawElements(D3DPT_TRIANGLELIST, 0, 4, 0, 2);

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUsePS1_1Path, 3);
	mpManager->DrawElements(D3DPT_TRIANGLELIST, 4, 4, 0, 2);

	D3D_DO(SetTexture(2, NULL));
	D3D_DO(SetTexture(1, NULL));
	D3D_DO(SetTexture(0, NULL));
	D3D_DO(EndScene());

	D3D_DO(SetRenderTarget(0, mpManager->GetRenderTarget()));
	D3D_DO(BeginScene());

	D3DVIEWPORT9 vpmain = {
		0,
		0,
		rClient.right,
		rClient.bottom,
		0.f,
		1.f
	};
	VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vpmain)));

	D3D_DO(SetPixelShader(NULL));

	if (Vertex *pvx = mpManager->LockVertices(4)) {
		const float ustep = mpManager->GetVertUScale();
		const float vstep = mpManager->GetVertVScale();
		const float u0 = 0.0f;
		const float v0 = 0.0f;
		const float u1 = u0 + rClient.right * ustep;
		const float v1 = v0 + rClient.bottom * vstep;

		pvx[ 0].SetFF1(-1, +1, u0, v0);
		pvx[ 1].SetFF1(-1, -1, u0, v1);
		pvx[ 2].SetFF1(+1, -1, u1, v1);
		pvx[ 3].SetFF1(+1, +1, u1, v0);

		mpManager->UnlockVertices();
	}

	D3D_DO(SetTexture(0, mpManager->GetVertRenderTexture()));

	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUsePS1_1Path, 4);
	mpManager->DrawElements(D3DPT_TRIANGLELIST, 0, 4, 0, 2);

	D3D_DO(SetTexture(0, NULL));
		mpManager->DisableTextureStage(0);
		mpManager->DisableTextureStage(1);
		mpManager->DisableTextureStage(2);
		D3D_DO(SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE));
	D3D_DO(EndScene());
	return true;
}

///////////////////////////////////////////////////////////////////////////
//
//
//	Programmmable path - 5 texture stages (ATI RADEON 8500+, NVIDIA GeForceFX+)
//
//
///////////////////////////////////////////////////////////////////////////

bool VDVideoDisplayMinidriverDX9::Paint_PS1_4(HDC hdc, const RECT& rClient) {
	if (uint16 *dst = mpManager->LockIndices(6)) {
		dst[0] = 0;
		dst[1] = 2;
		dst[2] = 1;
		dst[3] = 0;
		dst[4] = 3;
		dst[5] = 2;

		mpManager->UnlockIndices();
	}

	// PASS 1: Horizontal filtering

	if (Vertex *pvx = mpManager->LockVertices(4)) {
		const float ustep = 1.0f / (float)mTexFmt.w;
		const float vstep = 1.0f / (float)mTexFmt.h;
		const float u0 = -0.5f * ustep;
		const float v0 = 0.0f;
		const float u1 = u0 + mSource.pixmap.w * ustep;
		const float v1 = v0 + mSource.pixmap.h * vstep;
		const float f0 = -0.125f;
		const float f1 = f0 + mSource.pixmap.w / 4.0f;
		const float x0 = -1.f;
		const float y0 = 1.f - mSource.pixmap.h * (2.0f * mRTHorizVScale);
		const float x1 = -1.f + rClient.right * (2.0f * mRTHorizUScale);
		const float y1 = 1.f;

		pvx[0].SetPS1_4(x0, y1, f0, 0.125f, u0 + 2*ustep, v0, u0 + 1*ustep, v0, u0 + 0*ustep, v0, u0 - 1*ustep, v0);
		pvx[1].SetPS1_4(x0, y0, f0, 0.125f, u0 + 2*ustep, v1, u0 + 1*ustep, v1, u0 + 0*ustep, v1, u0 - 1*ustep, v1);
		pvx[2].SetPS1_4(x1, y0, f1, 0.125f, u1 + 2*ustep, v1, u1 + 1*ustep, v1, u1 + 0*ustep, v1, u1 - 1*ustep, v1);
		pvx[3].SetPS1_4(x1, y1, f1, 0.125f, u1 + 2*ustep, v0, u1 + 1*ustep, v0, u1 + 0*ustep, v0, u1 - 1*ustep, v0);

		mpManager->UnlockVertices();
	}

	IDirect3DSurface9 *pRTSurface;
	VDVERIFY(SUCCEEDED(mpD3DRTHoriz->GetSurfaceLevel(0, &pRTSurface)));
	D3D_DO(SetRenderTarget(0, pRTSurface));
	pRTSurface->Release();

	D3D_DO(BeginScene());

	D3DVIEWPORT9 vphoriz = {
		0,
		0,
		mRTHorizUSize,
		mRTHorizVSize,
		0.f,
		1.f
	};
	D3D_DO(SetTransform(D3DTS_PROJECTION, &mHorizProjection));

	VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vphoriz)));

	D3D_DO(SetTexture(0, mpManager->GetFilterTexture()));
	D3D_DO(SetTexture(1, mpD3DImageTexture));
	D3D_DO(SetTexture(2, mpD3DImageTexture));
	D3D_DO(SetTexture(3, mpD3DImageTexture));
	D3D_DO(SetTexture(4, mpD3DImageTexture));

	D3D_DO(SetPixelShader(mpManager->GetHPixelShader()));
	mpManager->SetBicubicShader(VDVideoDisplayDX9Manager::kCubicUsePS1_4Path, 0);
	mpManager->DrawElements(D3DPT_TRIANGLELIST, 0, 4, 0, 2);

	D3D_DO(EndScene());

	// PASS 2: Vertical filtering

	D3D_DO(SetRenderTarget(0, mpManager->GetRenderTarget()));

	D3D_DO(BeginScene());
	D3D_DO(SetTransform(D3DTS_PROJECTION, &mWholeProjection));

	D3DVIEWPORT9 vpmain = {
		0,
		0,
		rClient.right,
		rClient.bottom,
		0.f,
		1.f
	};
	VDVERIFY(SUCCEEDED(mpD3DDevice->SetViewport(&vpmain)));

	D3D_DO(SetTexture(0, mpManager->GetFilterTexture()));
	D3D_DO(SetTexture(1, mpD3DRTHoriz));
	D3D_DO(SetTexture(2, mpD3DRTHoriz));
	D3D_DO(SetTexture(3, mpD3DRTHoriz));
	D3D_DO(SetTexture(4, mpD3DRTHoriz));

	if (Vertex *pvx = mpManager->LockVertices(4)) {
		const float ustep = mRTHorizUScale;
		const float vstep = mRTHorizVScale;
		const float u0 = 0.0f;
		const float v0 = -0.5f * vstep;
		const float u1 = u0 + rClient.right * ustep;
		const float v1 = v0 + mSource.pixmap.h * vstep;
		const float f0 = 0.125f;
		const float f1 = f0 + mSource.pixmap.h / 4.0f;

		pvx[0].SetPS1_4(-1, +1, f0, 0.125f, u0, v0 + 2*vstep, u0, v0 + 1*vstep, u0, v0 + 0*vstep, u0, v0 - 1*vstep);
		pvx[1].SetPS1_4(-1, -1, f1, 0.125f, u0, v1 + 2*vstep, u0, v1 + 1*vstep, u0, v1 + 0*vstep, u0, v1 - 1*vstep);
		pvx[2].SetPS1_4(+1, -1, f1, 0.125f, u1, v1 + 2*vstep, u1, v1 + 1*vstep, u1, v1 + 0*vstep, u1, v1 - 1*vstep);
		pvx[3].SetPS1_4(+1, +1, f0, 0.125f, u1, v0 + 2*vstep, u1, v0 + 1*vstep, u1, v0 + 0*vstep, u1, v0 - 1*vstep);

		mpManager->UnlockVertices();
	}

	mpManager->DrawElements(D3DPT_TRIANGLELIST, 0, 4, 0, 2);

	D3D_DO(SetTexture(4, NULL));
	D3D_DO(SetTexture(3, NULL));
	D3D_DO(SetTexture(2, NULL));
	D3D_DO(SetTexture(1, NULL));
	D3D_DO(SetTexture(0, NULL));
	D3D_DO(SetPixelShader(NULL));
	D3D_DO(EndScene());

	return true;
}

void VDVideoDisplayMinidriverDX9::SetLogicalPalette(const uint8 *pLogicalPalette) {
}
