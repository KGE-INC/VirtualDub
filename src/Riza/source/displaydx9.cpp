//	VirtualDub - Video processing and capture application
//	A/V interface library
//	Copyright (C) 1998-2005 Avery Lee
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
//	This file is the DirectX 9 driver for the video display subsystem.
//	It does traditional point sampled and bilinearly filtered upsampling
//	as well as a special multipass algorithm for emulated bicubic
//	filtering.
//

#include <vd2/system/vdtypes.h>

#define DIRECTDRAW_VERSION 0x0900
#define INITGUID
#include <d3d9.h>
#include <vd2/system/vdtypes.h>
#include <vd2/system/vdalloc.h>
#include <vd2/system/time.h>
#include <vd2/system/vdstl.h>
#include <vd2/system/w32assist.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>

#include "displaydrv.h"
#include "displaydrvdx9.h"

namespace {
	#include "displaydx9_shader.inl"
}

#define VDDEBUG_DX9DISP VDDEBUG


using namespace nsVDVideoDisplayDriverDX9;

#define D3D_DO(x) VDVERIFY(SUCCEEDED(mpD3DDevice->x))

///////////////////////////////////////////////////////////////////////////

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
	, mhwndDevice(NULL)
	, mbDeviceValid(false)
	, mbInScene(false)
	, mpD3DVB(NULL)
	, mpD3DIB(NULL)
	, mCubicRefCount(0)
	, mCubicTempRTTsAllocated(0)
	, mRefCount(0)
{
	for(int i=0; i<2; ++i) {
		mRTTRefCounts[i] = 0;
		mpD3DRTTs[i] = NULL;
		mRTTSurfaceInfo[i].mWidth = 1;
		mRTTSurfaceInfo[i].mHeight = 1;
		mRTTSurfaceInfo[i].mInvWidth = 1;
		mRTTSurfaceInfo[i].mInvHeight = 1;
	}
}

ATOM VDVideoDisplayDX9Manager::sDevWndClass;

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
	HINSTANCE hInst = VDGetLocalModuleHandleW32();

	if (!sDevWndClass) {
		WNDCLASS wc;

		wc.cbClsExtra		= 0;
		wc.cbWndExtra		= 0;
		wc.hbrBackground	= NULL;
		wc.hCursor			= NULL;
		wc.hIcon			= NULL;
		wc.hInstance		= hInst;
		wc.lpfnWndProc		= DefWindowProc;
		wc.lpszClassName	= "RizaD3DDeviceWindow";
		wc.lpszMenuName		= NULL;
		wc.style			= 0;

		sDevWndClass = RegisterClass(&wc);
		if (!sDevWndClass)
			return false;
	}

	mhwndDevice = CreateWindow((LPCTSTR)sDevWndClass, "", WS_POPUP, 0, 0, 0, 0, NULL, NULL, hInst, NULL);
	if (!mhwndDevice) {
		Shutdown();
		return false;
	}

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
	mPresentParms.SwapEffect		= D3DSWAPEFFECT_COPY;
	// BackBufferFormat is set below.
	mPresentParms.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	mPresentParms.BackBufferWidth	= GetSystemMetrics(SM_CXMAXIMIZED);
	mPresentParms.BackBufferHeight	= GetSystemMetrics(SM_CYMAXIMIZED);

	HRESULT hr;

	// Look for the NVPerfHUD 2.0 driver
	
	const UINT adapters = mpD3D->GetAdapterCount();
	DWORD dwFlags = D3DCREATE_FPU_PRESERVE;
	UINT adapter = D3DADAPTER_DEFAULT;
	D3DDEVTYPE type = D3DDEVTYPE_HAL;

	for(UINT n=0; n<adapters; ++n) {
		D3DADAPTER_IDENTIFIER9 ident;

		if (SUCCEEDED(mpD3D->GetAdapterIdentifier(n, 0, &ident))) {
			if (!strcmp(ident.Description, "NVIDIA NVPerfHUD")) {
				adapter = n;
				type = D3DDEVTYPE_REF;

				// trim the size a bit so we can see the controls
				mPresentParms.BackBufferWidth -= 256;
				mPresentParms.BackBufferHeight -= 192;
				break;
			}
		}
	}

	mAdapter = adapter;
	mDevType = type;

	D3DDISPLAYMODE mode;
	hr = mpD3D->GetAdapterDisplayMode(adapter, &mode);
	if (FAILED(hr)) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to get current adapter mode.\n");
		Shutdown();
		return false;
	}

	// Make sure we have at least X8R8G8B8 for a texture format
	hr = mpD3D->CheckDeviceFormat(adapter, type, D3DFMT_X8R8G8B8, 0, D3DRTYPE_TEXTURE, D3DFMT_X8R8G8B8);
	if (FAILED(hr)) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: Device does not support X8R8G8B8 textures.\n");
		Shutdown();
		return false;
	}

	// Make sure we have at least X8R8G8B8 or A8R8G8B8 for a backbuffer format
	mPresentParms.BackBufferFormat	= D3DFMT_X8R8G8B8;
	hr = mpD3D->CheckDeviceFormat(adapter, type, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, D3DFMT_X8R8G8B8);
	if (FAILED(hr)) {
		mPresentParms.BackBufferFormat	= D3DFMT_A8R8G8B8;
		hr = mpD3D->CheckDeviceFormat(adapter, type, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, D3DFMT_A8R8G8B8);

		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Device does not support X8R8G8B8 or A8R8G8B8 render targets.\n");
			Shutdown();
			return false;
		}
	}

	// Check if at least vertex shader 1.1 is supported; if not, force SWVP.
	hr = mpD3D->GetDeviceCaps(adapter, type, &mDevCaps);
	if (FAILED(hr)) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: Couldn't retrieve device caps.\n");
		Shutdown();
		return false;
	}

	if (mDevCaps.VertexShaderVersion >= D3DVS_VERSION(1, 1))
		dwFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE;
	else
		dwFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	// Create the device.
	hr = mpD3D->CreateDevice(adapter, type, mhwndDevice, dwFlags, &mPresentParms, &mpD3DDevice);
	if (FAILED(hr)) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to create device.\n");
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
	// retrieve display mode
	HRESULT hr = mpD3D->GetAdapterDisplayMode(mAdapter, &mDisplayMode);
	if (FAILED(hr)) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to get current adapter mode.\n");
		Shutdown();
		return false;
	}

	// retrieve back buffer
	if (!mpD3DRTMain) {
		hr = mpD3DDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &mpD3DRTMain);
		if (FAILED(hr)) {
			ShutdownVRAMResources();
			return false;
		}
	}

	// create vertex buffer
	if (!mpD3DVB) {
		hr = mpD3DDevice->CreateVertexBuffer(kVertexBufferSize * sizeof(Vertex), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX5, D3DPOOL_DEFAULT, &mpD3DVB, NULL);
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to create vertex buffer.\n");
			ShutdownVRAMResources();
			return false;
		}
		mVertexBufferPt = 0;
	}

	// create index buffer
	if (!mpD3DIB) {
		hr = mpD3DDevice->CreateIndexBuffer(kIndexBufferSize * sizeof(uint16), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &mpD3DIB, NULL);
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to create index buffer.\n");
			ShutdownVRAMResources();
			return false;
		}
		mIndexBufferPt = 0;
	}

	if (!InitEffect()) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to create shader resources.\n");
		ShutdownVRAMResources();
		return false;
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
	ShutdownEffect();

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

	if (mhwndDevice) {
		DestroyWindow(mhwndDevice);
		mhwndDevice = NULL;
	}
}

bool VDVideoDisplayDX9Manager::InitEffect() {
	// initialize vertex shaders
	if (g_effect.mVertexShaderCount && mVertexShaders.empty()) {
		mVertexShaders.resize(g_effect.mVertexShaderCount, NULL);
		for(uint32 i=0; i<g_effect.mVertexShaderCount; ++i) {
			const uint32 *pVertexShaderData = g_shaderData + g_effect.mVertexShaderOffsets[i];

			if ((pVertexShaderData[0] & 0xffff) > mDevCaps.VertexShaderVersion)
				continue;

			HRESULT hr = mpD3DDevice->CreateVertexShader((const DWORD *)pVertexShaderData, &mVertexShaders[i]);
			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Unable to create vertex shader #%d.\n", i+1);
				return false;
			}
		}
	}

	// initialize pixel shaders
	if (g_effect.mPixelShaderCount && mPixelShaders.empty()) {
		mPixelShaders.resize(g_effect.mPixelShaderCount, NULL);
		for(uint32 i=0; i<g_effect.mPixelShaderCount; ++i) {
			const uint32 *pPixelShaderData = g_shaderData + g_effect.mPixelShaderOffsets[i];

			if ((pPixelShaderData[0] & 0xffff) > mDevCaps.PixelShaderVersion)
				continue;

			HRESULT hr = mpD3DDevice->CreatePixelShader((const DWORD *)pPixelShaderData, &mPixelShaders[i]);
			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Unable to create pixel shader #%d.\n", i+1);
				return false;
			}
		}
	}

	return true;
}

void VDVideoDisplayDX9Manager::ShutdownEffect() {
	while(!mPixelShaders.empty()) {
		IDirect3DPixelShader9 *ps = mPixelShaders.back();
		mPixelShaders.pop_back();

		if (ps)
			ps->Release();
	}

	while(!mVertexShaders.empty()) {
		IDirect3DVertexShader9 *vs = mVertexShaders.back();
		mVertexShaders.pop_back();

		if (vs)
			vs->Release();
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

bool VDVideoDisplayDX9Manager::InitTempRTT(int index) {
	if (++mRTTRefCounts[index] > 1)
		return true;

	int texw = mPresentParms.BackBufferWidth;
	int texh = mPresentParms.BackBufferHeight;

	AdjustTextureSize(texw, texh);

	mRTTSurfaceInfo[index].mWidth		= texw;
	mRTTSurfaceInfo[index].mHeight		= texh;
	mRTTSurfaceInfo[index].mInvWidth	= 1.0f / texw;
	mRTTSurfaceInfo[index].mInvHeight	= 1.0f / texh;

	HRESULT hr = mpD3DDevice->CreateTexture(texw, texh, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &mpD3DRTTs[index], NULL);
	if (FAILED(hr)) {
		ShutdownTempRTT(index);
		return false;
	}

	ClearRenderTarget(mpD3DRTTs[index]);
	return true;
}

void VDVideoDisplayDX9Manager::ShutdownTempRTT(int index) {
	VDASSERT(mRTTRefCounts[index] > 0);
	if (--mRTTRefCounts[index])
		return;

	if (mpD3DRTTs[index]) {
		mpD3DRTTs[index]->Release();
		mpD3DRTTs[index] = NULL;
	}

	mRTTSurfaceInfo[index].mWidth		= 1;
	mRTTSurfaceInfo[index].mHeight		= 1;
	mRTTSurfaceInfo[index].mInvWidth	= 1;
	mRTTSurfaceInfo[index].mInvHeight	= 1;
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

	mCubicMode = (CubicMode)kMaxCubicMode;
	while(mCubicMode > kCubicNotPossible) {
		if (ValidateBicubicShader(mCubicMode))
			break;
		mCubicMode = (CubicMode)(mCubicMode - 1);
	}

	if (mCubicMode == kCubicNotPossible) {
		ShutdownBicubic();
		return mCubicMode;
	}

	// create horizontal resampling texture
	if (!InitTempRTT(0)) {
		ShutdownBicubic();
		return kCubicNotPossible;
	}

	mCubicTempRTTsAllocated |= 1;

	// create vertical resampling texture
	if (mCubicMode < kCubicUsePS1_1Path) {
		if (!InitTempRTT(1)) {
			ShutdownBicubic();
			return kCubicNotPossible;
		}

		mCubicTempRTTsAllocated |= 2;
	}

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

	return mCubicMode;
}

void VDVideoDisplayDX9Manager::ShutdownBicubic() {
	VDASSERT(mCubicRefCount > 0);
	if (--mCubicRefCount)
		return;

	if (mpD3DFilterTexture) {
		mpD3DFilterTexture->Release();
		mpD3DFilterTexture = NULL;
	}

	if (mCubicTempRTTsAllocated & 2) {
		ShutdownTempRTT(1);
		mCubicTempRTTsAllocated &= ~2;
	}

	if (mCubicTempRTTsAllocated & 1) {
		ShutdownTempRTT(0);
		mCubicTempRTTsAllocated &= ~1;
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
	mbInScene = false;

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
	// use original texture size if device has no restrictions
	if (!(mDevCaps.TextureCaps & (D3DPTEXTURECAPS_NONPOW2CONDITIONAL | D3DPTEXTURECAPS_POW2)))
		return;

	// make power of two
	texw += texw - 1;
	texh += texh - 1;

	while(int tmp = texw & (texw-1))
		texw = tmp;
	while(int tmp = texh & (texh-1))
		texh = tmp;

	// enforce aspect ratio
	if (mDevCaps.MaxTextureAspectRatio) {
		while(texw * (int)mDevCaps.MaxTextureAspectRatio < texh)
			texw += texw;
		while(texh * (int)mDevCaps.MaxTextureAspectRatio < texw)
			texh += texh;
	}
}

namespace {
	D3DFORMAT GetD3DTextureFormatForPixmapFormat(int format) {
		using namespace nsVDPixmap;

		switch(format) {
			case nsVDPixmap::kPixFormat_XRGB1555:
				return D3DFMT_X1R5G5B5;

			case nsVDPixmap::kPixFormat_RGB565:
				return D3DFMT_R5G6B5;

			case nsVDPixmap::kPixFormat_RGB888:
				return D3DFMT_R8G8B8;				// No real hardware supports this format, in practice.

			case nsVDPixmap::kPixFormat_XRGB8888:
				return D3DFMT_X8R8G8B8;

			case nsVDPixmap::kPixFormat_YUV422_UYVY:
				return D3DFMT_UYVY;

			case nsVDPixmap::kPixFormat_YUV422_YUYV:
				return D3DFMT_YUY2;

			default:
				return D3DFMT_UNKNOWN;
		}
	}
}

void VDVideoDisplayDX9Manager::DetermineBestTextureFormat(int srcFormat, int& dstFormat, D3DFORMAT& dstD3DFormat) {
	using namespace nsVDPixmap;

	HRESULT hr;

	// Try direct format first. If that doesn't work, try a fallback (in practice, we
	// only have one).

	dstFormat = srcFormat;
	for(int i=0; i<2; ++i) {
		dstD3DFormat = GetD3DTextureFormatForPixmapFormat(dstFormat);
		if (dstD3DFormat) {
			hr = mpD3D->CheckDeviceFormat(mAdapter, mDevType, mDisplayMode.Format, 0, D3DRTYPE_TEXTURE, dstD3DFormat);
			if (SUCCEEDED(hr)) {
				dstFormat = srcFormat;
				return;
			}
		}

		// fallback
		switch(dstFormat) {
			case kPixFormat_XRGB1555:
				dstFormat = kPixFormat_RGB565;
				break;

			case kPixFormat_RGB565:
				dstFormat = kPixFormat_XRGB1555;
				break;

			case kPixFormat_YUV422_UYVY:
				dstFormat =	kPixFormat_YUV422_YUYV;
				break;

			case kPixFormat_YUV422_YUYV:
				dstFormat = kPixFormat_YUV422_UYVY;
				break;

			default:
				goto fail;
		}
	}
fail:

	// Just use X8R8G8B8. We always know this works (we reject the device if it doesn't).
	dstFormat = kPixFormat_XRGB8888;
	dstD3DFormat = D3DFMT_X8R8G8B8;
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

bool VDVideoDisplayDX9Manager::BeginScene() {
	if (!mbInScene) {
		HRESULT hr = mpD3DDevice->BeginScene();

		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: BeginScene() failed! hr = %08x\n", hr);
			return false;
		}

		mbInScene = true;
	}

	return true;
}

bool VDVideoDisplayDX9Manager::EndScene() {
	if (mbInScene) {
		mbInScene = false;
		HRESULT hr = mpD3DDevice->EndScene();

		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: EndScene() failed! hr = %08x\n", hr);
			return false;
		}
	}

	return true;
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

HRESULT VDVideoDisplayDX9Manager::Present(const RECT *src, HWND hwndDest, bool vsync) {
	HRESULT hr;

	if (vsync && (mDevCaps.Caps & D3DCAPS_READ_SCANLINE)) {
		RECT r;
		if (GetWindowRect(hwndDest, &r)) {
			int top = 0;
			int bottom = GetSystemMetrics(SM_CYSCREEN);

			// GetMonitorInfo() requires Windows 98. We might never fail on this because
			// I think DirectX 9.0c requires 98+, but we have to dynamically link anyway
			// to avoid a startup link failure on 95.
			typedef BOOL (APIENTRY *tpGetMonitorInfo)(HMONITOR mon, LPMONITORINFO lpmi);
			static tpGetMonitorInfo spGetMonitorInfo = (tpGetMonitorInfo)GetProcAddress(GetModuleHandle("user32"), "GetMonitorInfo");

			if (spGetMonitorInfo) {
				HMONITOR hmon = mpD3D->GetAdapterMonitor(mAdapter);
				MONITORINFO monInfo = {sizeof(MONITORINFO)};
				if (spGetMonitorInfo(hmon, &monInfo)) {
					top = monInfo.rcMonitor.top;
					bottom = monInfo.rcMonitor.bottom;
				}
			}

			if (r.top < top)
				r.top = top;
			if (r.bottom > bottom)
				r.bottom = bottom;

			r.top -= top;
			r.bottom -= top;

			// Poll raster status, and wait until we can safely blit. We assume that the
			// blit can outrace the beam. 
			D3DRASTER_STATUS rastStatus;
			UINT maxScanline = 0;
			while(SUCCEEDED(mpD3DDevice->GetRasterStatus(0, &rastStatus))) {
				if (rastStatus.InVBlank)
					break;

				// Check if we have wrapped around without seeing the VBlank. If this
				// occurs, force an exit. This prevents us from potentially burning a lot
				// of CPU time if the CPU becomes busy and can't poll the beam in a timely
				// manner.
				if (rastStatus.ScanLine < maxScanline)
					break;

				// Check if we're outside of the danger zone.
				if ((int)rastStatus.ScanLine < r.top || (int)rastStatus.ScanLine >= r.bottom)
					break;

				maxScanline = rastStatus.ScanLine;
			}
		}
	}

	hr = mpD3DDevice->Present(src, NULL, hwndDest, NULL);
	return hr;
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
	const TechniqueInfo *pTechInfo;
	switch(mode) {
		case kCubicUsePS1_4Path:
			pTechInfo = &g_technique_bicubic1_4;
			break;																														
		case kCubicUsePS1_1Path:																				
			pTechInfo = &g_technique_bicubic1_1;
			break;																														
		case kCubicUseFF3Path:																				
			pTechInfo = &g_technique_bicubicFF3;
			break;																														
		case kCubicUseFF2Path:																				
			pTechInfo = &g_technique_bicubicFF2;
			break;
		default:
			return false;
	}

	// Validate caps bits.
	if ((mDevCaps.PrimitiveMiscCaps & pTechInfo->mPrimitiveMiscCaps) != pTechInfo->mPrimitiveMiscCaps)
		return false;
	if (mDevCaps.MaxSimultaneousTextures < pTechInfo->mMaxSimultaneousTextures)
		return false;
	if (mDevCaps.MaxTextureBlendStages < pTechInfo->mMaxSimultaneousTextures)
		return false;
	if ((mDevCaps.SrcBlendCaps & pTechInfo->mSrcBlendCaps) != pTechInfo->mSrcBlendCaps)
		return false;
	if ((mDevCaps.DestBlendCaps & pTechInfo->mDestBlendCaps) != pTechInfo->mDestBlendCaps)
		return false;
	if ((mDevCaps.TextureOpCaps & pTechInfo->mTextureOpCaps) != pTechInfo->mTextureOpCaps)
		return false;
	if (pTechInfo->mPSVersionRequired) {
		if (mDevCaps.PixelShaderVersion < pTechInfo->mPSVersionRequired)
			return false;
		if (mDevCaps.PixelShader1xMaxValue < pTechInfo->mPixelShader1xMaxValue * 0.95f)
			return false;
	}

	// Validate shaders.
	const PassInfo *pPasses = pTechInfo->mpPasses;
	for(uint32 stage = 0; stage < pTechInfo->mPassCount; ++stage) {
		const PassInfo& pi = *pPasses++;

		const uint32 stateStart = pi.mStateStart, stateEnd = pi.mStateEnd;
		for(uint32 stateIdx = stateStart; stateIdx != stateEnd; ++stateIdx) {
			uint32 token = g_states[stateIdx];
			uint32 tokenIndex = (token >> 12) & 0xFFF;
			uint32 tokenValue = token & 0xFFF;

			if (tokenValue == 0xFFF)
				tokenValue = g_states[++stateIdx];

			HRESULT hr = S_OK;
			switch(token >> 28) {
				case 0:		// render state
					hr = mpD3DDevice->SetRenderState((D3DRENDERSTATETYPE)tokenIndex, tokenValue);
					break;
				case 1:		// texture stage state
					hr = mpD3DDevice->SetTextureStageState((token >> 24)&15, (D3DTEXTURESTAGESTATETYPE)tokenIndex, tokenValue);
					break;
				case 2:		// sampler state
					hr = mpD3DDevice->SetSamplerState((token >> 24)&15, (D3DSAMPLERSTATETYPE)tokenIndex, tokenValue);
					break;
				case 3:		// texture
				case 8:		// vertex bool constant
				case 9:		// vertex int constant
				case 10:	// vertex float constant
				case 12:	// pixel bool constant
				case 13:	// pixel int constant
				case 14:	// pixel float constant
					// ignore.
					break;
			}

			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to set state! hr=%08x\n", hr);
				return false;
			}
		}

		HRESULT hr = mpD3DDevice->SetVertexShader(GetEffectVertexShader(pi.mVertexShaderIndex));
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Couldn't set vertex shader! hr=%08x\n", hr);
			return false;
		}

		hr = mpD3DDevice->SetPixelShader(GetEffectPixelShader(pi.mPixelShaderIndex));
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Couldn't set pixel shader! hr=%08x\n", hr);
			return false;
		}

		DWORD passes;
		hr = mpD3DDevice->ValidateDevice(&passes);

		if (FAILED(hr))
			return false;
	}

	return true;
}

IDirect3DVertexShader9 *VDVideoDisplayDX9Manager::GetEffectVertexShader(int index) const {
	return (size_t)index < mVertexShaders.size() ? mVertexShaders[index] : NULL;
}

IDirect3DPixelShader9 *VDVideoDisplayDX9Manager::GetEffectPixelShader(int index) const {
	return (size_t)index < mPixelShaders.size() ? mPixelShaders[index] : NULL;
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
	bool Update(UpdateMode);
	void Refresh(UpdateMode);
	bool Paint(HDC hdc, const RECT& rClient, UpdateMode mode);

	bool SetSubrect(const vdrect32 *) { return false; }
	void SetLogicalPalette(const uint8 *pLogicalPalette);

protected:
	void OnPreDeviceReset() { ShutdownBicubic(); }
	void InitBicubic();
	void ShutdownBicubic();
	bool RunEffect(HDC hdc, const RECT& rClient, const PassInfo *pPasses, uint32 nPasses);

	HWND				mhwnd;
	VDVideoDisplayDX9Manager	*mpManager;
	IDirect3DDevice9	*mpD3DDevice;			// weak ref
	IDirect3DTexture9	*mpD3DImageTexture;

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

	if (caps.MaxTextureWidth < (uint32)info.pixmap.w || caps.MaxTextureHeight < (uint32)info.pixmap.h) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: source image is larger than maximum texture size\n");
		Shutdown();
		return false;
	}

	// create source texture
	int texw = info.pixmap.w;
	int texh = info.pixmap.h;

	mpManager->AdjustTextureSize(texw, texh);

	memset(&mTexFmt, 0, sizeof mTexFmt);
	mTexFmt.format		= nsVDPixmap::kPixFormat_XRGB8888;
	mTexFmt.w			= texw;
	mTexFmt.h			= texh;

	D3DFORMAT d3dfmt;
	mpManager->DetermineBestTextureFormat(info.pixmap.format, mTexFmt.format, d3dfmt);

	if (!info.bAllowConversion && info.pixmap.format != mTexFmt.format) {
		Shutdown();
		return false;
	}

	HRESULT hr = mpD3DDevice->CreateTexture(texw, texh, 1, 0, d3dfmt, D3DPOOL_MANAGED, &mpD3DImageTexture, NULL);
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

	VDDEBUG_DX9DISP("VideoDisplay/DX9: Init successful for %dx%d source image (%s -> %s)\n", mSource.pixmap.w, mSource.pixmap.h, VDPixmapGetInfo(info.pixmap.format).name, VDPixmapGetInfo(mTexFmt.format).name);

	return true;
}

void VDVideoDisplayMinidriverDX9::InitBicubic() {
	if (!mbCubicInitialized && !mbCubicAttempted) {
		mbCubicAttempted = true;

		mCubicMode = mpManager->InitBicubic();

		if (mCubicMode != VDVideoDisplayDX9Manager::kCubicNotPossible) {
			if (mCubicMode == VDVideoDisplayDX9Manager::kCubicNotPossible) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Bicubic initialization failed -- falling back to bilinear path.\n");
			} else {
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

bool VDVideoDisplayMinidriverDX9::Update(UpdateMode) {
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

void VDVideoDisplayMinidriverDX9::Refresh(UpdateMode mode) {
	if (HDC hdc = GetDC(mhwnd)) {
		RECT r;
		GetClientRect(mhwnd, &r);
		Paint(hdc, r, mode);
		ReleaseDC(mhwnd, hdc);
	}
}

bool VDVideoDisplayMinidriverDX9::Paint(HDC hdc, const RECT& rClient, UpdateMode updateMode) {
	const RECT rClippedClient={0,0,std::min<int>(rClient.right, mpManager->GetMainRTWidth()), std::min<int>(rClient.bottom, mpManager->GetMainRTHeight())};

	// Make sure the device is sane.
	if (!mpManager->CheckDevice())
		return false;

	// Do we need to switch bicubic modes?
	FilterMode mode = mPreferredFilter;

	if (mode == kFilterAnySuitable)
		mode = kFilterBicubic;

	// bicubic modes cannot clip
	if (rClient.right != rClippedClient.right || rClient.bottom != rClippedClient.bottom)
		mode = kFilterBilinear;

	if (mode != kFilterBicubic && mbCubicInitialized)
		ShutdownBicubic();
	else if (mode == kFilterBicubic && !mbCubicInitialized && !mbCubicAttempted)
		InitBicubic();


	const D3DMATRIX ident={
		1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1
	};

	D3D_DO(SetTransform(D3DTS_WORLD, &ident));
	D3D_DO(SetTransform(D3DTS_VIEW, &ident));
	D3D_DO(SetTransform(D3DTS_PROJECTION, &ident));

	D3D_DO(SetStreamSource(0, mpManager->GetVertexBuffer(), 0, sizeof(Vertex)));
	D3D_DO(SetIndices(mpManager->GetIndexBuffer()));
	D3D_DO(SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX5));
	D3D_DO(SetRenderState(D3DRS_LIGHTING, FALSE));
	D3D_DO(SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE));
	D3D_DO(SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0));
	D3D_DO(SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1));
	D3D_DO(SetTextureStageState(2, D3DTSS_TEXCOORDINDEX, 2));

	bool bSuccess;
	const D3DPRESENT_PARAMETERS& pparms = mpManager->GetPresentParms();

	if (mbCubicInitialized &&
		(uint32)rClient.right <= pparms.BackBufferWidth &&
		(uint32)rClient.bottom <= pparms.BackBufferHeight &&
		(uint32)mSource.pixmap.w <= pparms.BackBufferWidth &&
		(uint32)mSource.pixmap.h <= pparms.BackBufferHeight
		) {
		const VDVideoDisplayDX9Manager::SurfaceInfo& vertInfo = mpManager->GetTempRTTInfo(0);

		switch(mCubicMode) {
		case VDVideoDisplayDX9Manager::kCubicUsePS1_4Path:
			bSuccess = RunEffect(hdc, rClient, g_technique_bicubic1_4_passes, sizeof g_technique_bicubic1_4_passes / sizeof(g_technique_bicubic1_4_passes[0]));
			break;																														
		case VDVideoDisplayDX9Manager::kCubicUsePS1_1Path:																				
			bSuccess = RunEffect(hdc, rClient, g_technique_bicubic1_1_passes, sizeof g_technique_bicubic1_1_passes / sizeof(g_technique_bicubic1_1_passes[0]));
			break;																														
		case VDVideoDisplayDX9Manager::kCubicUseFF3Path:																				
			bSuccess = RunEffect(hdc, rClient, g_technique_bicubicFF3_passes, sizeof g_technique_bicubicFF3_passes / sizeof(g_technique_bicubicFF3_passes[0]));
			break;																														
		case VDVideoDisplayDX9Manager::kCubicUseFF2Path:																				
			bSuccess = RunEffect(hdc, rClient, g_technique_bicubicFF2_passes, sizeof g_technique_bicubicFF2_passes / sizeof(g_technique_bicubicFF2_passes[0]));
			break;
		}
	} else {
		if (mPreferredFilter == kFilterPoint)
			bSuccess = RunEffect(hdc, rClient, g_technique_point_passes, sizeof g_technique_point_passes / sizeof(g_technique_point_passes[0]));
		else
			bSuccess = RunEffect(hdc, rClient, g_technique_bilinear_passes, sizeof g_technique_bilinear_passes / sizeof(g_technique_bilinear_passes[0]));

		bSuccess = true;
	}

	if (bSuccess && !mpManager->EndScene())
		bSuccess = false;

	HRESULT hr = E_FAIL;

	if (bSuccess)
		hr = mpManager->Present(&rClient, mhwnd, (updateMode & kModeVSync) != 0);

	if (FAILED(hr)) {
		VDDEBUG_DX9DISP("VideoDisplay/DX9: Render failed -- applying boot to the head.\n");

		// TODO: Need to free all DEFAULT textures before proceeding

		if (mpManager->Reset())
			return S_OK;
	}

	return SUCCEEDED(hr);
}

void VDVideoDisplayMinidriverDX9::SetLogicalPalette(const uint8 *pLogicalPalette) {
}

bool VDVideoDisplayMinidriverDX9::RunEffect(HDC hdc, const RECT& rClient, const PassInfo *pPasses, uint32 nPasses) {
	IDirect3DTexture9 *const textures[5]={
		NULL,
		mpD3DImageTexture,
		mpManager->GetTempRTT(0),
		mpManager->GetTempRTT(1),
		mpManager->GetFilterTexture()
	};

	const D3DPRESENT_PARAMETERS& pparms = mpManager->GetPresentParms();
	int clippedWidth = std::min<int>(rClient.right, pparms.BackBufferWidth);
	int clippedHeight = std::min<int>(rClient.bottom, pparms.BackBufferHeight);

	if (clippedWidth <= 0 || clippedHeight <= 0)
		return true;

	struct StdParamData {
		float vpsize[4];			// (viewport size)			vpwidth, vpheight, 1/vpheight, 1/vpwidth
		float cvpsize[4];			// (clipped viewport size)	cvpwidth, cvpheight, 1/cvpheight, 1/cvpwidth
		float texsize[4];			// (texture size)			texwidth, texheight, 1/texheight, 1/texwidth
		float srcsize[4];			// (source size)			srcwidth, srcheight, 1/srcheight, 1/srcwidth
		float tempsize[4];			// (temp rtt size)			tempwidth, tempheight, 1/tempheight, 1/tempwidth
		float temp2size[4];			// (temp2 rtt size)			tempwidth, tempheight, 1/tempheight, 1/tempwidth
		float vpcorrect[4];			// (viewport correction)	2/vpwidth, 2/vpheight, -1/vpheight, 1/vpwidth
		float vpcorrect2[4];		// (viewport correction)	2/vpwidth, -2/vpheight, 1+1/vpheight, -1-1/vpwidth
		float tvpcorrect[4];		// (temp vp correction)		2/tvpwidth, 2/tvpheight, -1/tvpheight, 1/tvpwidth
		float tvpcorrect2[4];		// (temp vp correction)		2/tvpwidth, -2/tvpheight, 1+1/tvpheight, -1-1/tvpwidth
		float t2vpcorrect[4];		// (temp2 vp correction)	2/tvpwidth, 2/tvpheight, -1/tvpheight, 1/tvpwidth
		float t2vpcorrect2[4];		// (temp2 vp correction)	2/tvpwidth, -2/tvpheight, 1+1/tvpheight, -1-1/tvpwidth
		float time[4];				// (time)
	};

	static const struct StdParam {
		int offset;
	} kStdParamInfo[]={
		offsetof(StdParamData, vpsize),
		offsetof(StdParamData, cvpsize),
		offsetof(StdParamData, texsize),
		offsetof(StdParamData, srcsize),
		offsetof(StdParamData, tempsize),
		offsetof(StdParamData, temp2size),
		offsetof(StdParamData, vpcorrect),
		offsetof(StdParamData, vpcorrect2),
		offsetof(StdParamData, tvpcorrect),
		offsetof(StdParamData, tvpcorrect2),
		offsetof(StdParamData, t2vpcorrect),
		offsetof(StdParamData, t2vpcorrect2),
		offsetof(StdParamData, time),
	};

	StdParamData data;

	data.vpsize[0] = (float)rClient.right;
	data.vpsize[1] = (float)rClient.bottom;
	data.vpsize[2] = 1.0f / (float)rClient.bottom;
	data.vpsize[3] = 1.0f / (float)rClient.right;
	data.cvpsize[0] = (float)clippedWidth;
	data.cvpsize[0] = (float)clippedHeight;
	data.cvpsize[0] = 1.0f / (float)clippedHeight;
	data.cvpsize[0] = 1.0f / (float)clippedWidth;
	data.texsize[0] = (float)mTexFmt.w;
	data.texsize[1] = (float)mTexFmt.h;
	data.texsize[2] = 1.0f / (float)mTexFmt.h;
	data.texsize[3] = 1.0f / (float)mTexFmt.w;
	data.srcsize[0] = (float)mSource.pixmap.w;
	data.srcsize[1] = (float)mSource.pixmap.h;
	data.srcsize[2] = 1.0f / (float)mSource.pixmap.h;
	data.srcsize[3] = 1.0f / (float)mSource.pixmap.w;
	data.tempsize[0] = 1.f;
	data.tempsize[1] = 1.f;
	data.tempsize[2] = 1.f;
	data.tempsize[3] = 1.f;
	data.temp2size[0] = 1.f;
	data.temp2size[1] = 1.f;
	data.temp2size[2] = 1.f;
	data.temp2size[3] = 1.f;
	data.vpcorrect[0] = 2.0f / (float)clippedWidth;
	data.vpcorrect[1] = 2.0f / (float)clippedHeight;
	data.vpcorrect[2] = -1.0f / (float)clippedHeight;
	data.vpcorrect[3] = 1.0f / (float)clippedWidth;
	data.vpcorrect2[0] = 2.0f / (float)clippedWidth;
	data.vpcorrect2[1] = -2.0f / (float)clippedHeight;
	data.vpcorrect2[2] = 1.0f + 1.0f / (float)clippedHeight;
	data.vpcorrect2[3] = -1.0f - 1.0f / (float)clippedWidth;
	data.tvpcorrect[0] = 2.0f;
	data.tvpcorrect[1] = 2.0f;
	data.tvpcorrect[2] = -1.0f;
	data.tvpcorrect[3] = 1.0f;
	data.tvpcorrect2[0] = 2.0f;
	data.tvpcorrect2[1] = -2.0f;
	data.tvpcorrect2[2] = 0.f;
	data.tvpcorrect2[3] = 2.0f;
	data.t2vpcorrect[0] = 2.0f;
	data.t2vpcorrect[1] = 2.0f;
	data.t2vpcorrect[2] = -1.0f;
	data.t2vpcorrect[3] = 1.0f;
	data.t2vpcorrect2[0] = 2.0f;
	data.t2vpcorrect2[1] = -2.0f;
	data.t2vpcorrect2[2] = 0.f;
	data.t2vpcorrect2[3] = 2.0f;
	data.time[0] = (GetTickCount() % 30000) / 30000.0f;
	data.time[1] = 1;
	data.time[2] = 2;
	data.time[3] = 3;

	if (textures[1]) {
		const VDVideoDisplayDX9Manager::SurfaceInfo& rttInfo = mpManager->GetTempRTTInfo(0);
		data.tempsize[0] = (float)rttInfo.mWidth;
		data.tempsize[1] = (float)rttInfo.mHeight;
		data.tempsize[2] = rttInfo.mInvHeight;
		data.tempsize[3] = rttInfo.mInvWidth;
		data.tvpcorrect[0] = 2.0f * data.tempsize[3];
		data.tvpcorrect[1] = 2.0f * data.tempsize[2];
		data.tvpcorrect[2] = -data.tempsize[2];
		data.tvpcorrect[3] = data.tempsize[3];
		data.tvpcorrect2[0] = 2.0f * data.tempsize[3];
		data.tvpcorrect2[1] = -2.0f * data.tempsize[2];
		data.tvpcorrect2[2] = 1.0f + data.tempsize[2];
		data.tvpcorrect2[3] = -1.0f - data.tempsize[3];
	}

	if (textures[2]) {
		const VDVideoDisplayDX9Manager::SurfaceInfo& rttInfo = mpManager->GetTempRTTInfo(1);
		data.temp2size[0] = (float)rttInfo.mWidth;
		data.temp2size[1] = (float)rttInfo.mHeight;
		data.temp2size[2] = rttInfo.mInvHeight;
		data.temp2size[3] = rttInfo.mInvWidth;
		data.t2vpcorrect[0] = 2.0f * data.tempsize[3];
		data.t2vpcorrect[1] = 2.0f * data.tempsize[2];
		data.t2vpcorrect[2] = -data.tempsize[2];
		data.t2vpcorrect[3] = data.tempsize[3];
		data.t2vpcorrect2[0] = 2.0f * data.tempsize[3];
		data.t2vpcorrect2[1] = -2.0f * data.tempsize[2];
		data.t2vpcorrect2[2] = 1.0f + data.tempsize[2];
		data.t2vpcorrect2[3] = -1.0f - data.tempsize[3];
	}

	enum { kStdParamCount = sizeof kStdParamInfo / sizeof kStdParamInfo[0] };

	while(nPasses--) {
		const PassInfo& pi = *pPasses++;

		// bind vertex and pixel shaders
		HRESULT hr = mpD3DDevice->SetVertexShader(mpManager->GetEffectVertexShader(pi.mVertexShaderIndex));
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Couldn't set vertex shader! hr=%08x\n", hr);
			return false;
		}

		hr = mpD3DDevice->SetPixelShader(mpManager->GetEffectPixelShader(pi.mPixelShaderIndex));
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Couldn't set pixel shader! hr=%08x\n", hr);
			return false;
		}

		// set states
		const uint32 stateStart = pi.mStateStart, stateEnd = pi.mStateEnd;
		for(uint32 stateIdx = stateStart; stateIdx != stateEnd; ++stateIdx) {
			uint32 token = g_states[stateIdx];
			uint32 tokenIndex = (token >> 12) & 0xFFF;
			uint32 tokenValue = token & 0xFFF;

			if (tokenValue == 0xFFF)
				tokenValue = g_states[++stateIdx];

			HRESULT hr;
			switch(token >> 28) {
				case 0:		// render state
					hr = mpD3DDevice->SetRenderState((D3DRENDERSTATETYPE)tokenIndex, tokenValue);
					break;
				case 1:		// texture stage state
					hr = mpD3DDevice->SetTextureStageState((token >> 24)&15, (D3DTEXTURESTAGESTATETYPE)tokenIndex, tokenValue);
					break;
				case 2:		// sampler state
					hr = mpD3DDevice->SetSamplerState((token >> 24)&15, (D3DSAMPLERSTATETYPE)tokenIndex, tokenValue);
					break;
				case 3:		// texture
					VDASSERT(tokenValue < 6);
					hr = mpD3DDevice->SetTexture(tokenIndex, textures[tokenValue]);
					break;
				case 8:		// vertex bool constant
					hr = mpD3DDevice->SetVertexShaderConstantB(tokenIndex, (const BOOL *)((char *)&data + kStdParamInfo[tokenValue].offset), 1);
					break;
				case 9:		// vertex int constant
					hr = mpD3DDevice->SetVertexShaderConstantI(tokenIndex, (const INT *)((char *)&data + kStdParamInfo[tokenValue].offset), 1);
					break;
				case 10:	// vertex float constant
					hr = mpD3DDevice->SetVertexShaderConstantF(tokenIndex, (const float *)((char *)&data + kStdParamInfo[tokenValue].offset), 1);
					break;
				case 12:	// pixel bool constant
					hr = mpD3DDevice->SetPixelShaderConstantB(tokenIndex, (const BOOL *)((char *)&data + kStdParamInfo[tokenValue].offset), 1);
					break;
				case 13:	// pixel int constant
					hr = mpD3DDevice->SetPixelShaderConstantI(tokenIndex, (const INT *)((char *)&data + kStdParamInfo[tokenValue].offset), 1);
					break;
				case 14:	// pixel float constant
					hr = mpD3DDevice->SetPixelShaderConstantF(tokenIndex, (const float *)((char *)&data + kStdParamInfo[tokenValue].offset), 1);
					break;
			}

			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to set state! hr=%08x\n", hr);
				return false;
			}
		}

		// change render target
		if (pi.mRenderTarget >= 0) {
			if (!mpManager->EndScene())
				return false;

			HRESULT hr;
			switch(pi.mRenderTarget) {
				case 0:
					hr = mpD3DDevice->SetRenderTarget(0, mpManager->GetRenderTarget());
					break;
				case 1:
					{
						IDirect3DSurface9 *pSurf;
						hr = mpManager->GetTempRTT(0)->GetSurfaceLevel(0, &pSurf);
						if (SUCCEEDED(hr)) {
							hr = mpD3DDevice->SetRenderTarget(0, pSurf);
							pSurf->Release();
						}
					}
					break;
				case 2:
					{
						IDirect3DSurface9 *pSurf;
						hr = mpManager->GetTempRTT(1)->GetSurfaceLevel(0, &pSurf);
						if (SUCCEEDED(hr)) {
							hr = mpD3DDevice->SetRenderTarget(0, pSurf);
							pSurf->Release();
						}
					}
					break;
			}

			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to set render target! hr=%08x\n", hr);
				return false;
			}
		}

		// change viewport
		D3DVIEWPORT9 vp;
		if (pi.mViewportW | pi.mViewportH) {
			HRESULT hr;

			IDirect3DSurface9 *rt;
			hr = mpD3DDevice->GetRenderTarget(0, &rt);
			if (SUCCEEDED(hr)) {
				D3DSURFACE_DESC desc;
				hr = rt->GetDesc(&desc);
				if (SUCCEEDED(hr)) {
					const DWORD hsizes[3]={ desc.Width, mSource.pixmap.w, clippedWidth };
					const DWORD vsizes[3]={ desc.Height, mSource.pixmap.h, clippedHeight };

					vp.X = 0;
					vp.Y = 0;
					vp.Width = hsizes[pi.mViewportW];
					vp.Height = vsizes[pi.mViewportH];
					vp.MinZ = 0;
					vp.MaxZ = 1;

					hr = mpD3DDevice->SetViewport(&vp);
				}
				rt->Release();
			}

			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to set viewport! hr=%08x\n", hr);
				return false;
			}
		} else {
			HRESULT hr = mpD3DDevice->GetViewport(&vp);
			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to retrieve viewport! hr=%08x\n", hr);
				return false;
			}
		}

		// clear target
		if (pi.mbRTDoClear) {
			hr = mpD3DDevice->Clear(0, NULL, D3DCLEAR_TARGET, pi.mRTClearColor, 0, 0);
			if (FAILED(hr)) {
				VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to clear viewport! hr=%08x\n", hr);
				return false;
			}
		}

		// render!
		if (Vertex *pvx = mpManager->LockVertices(4)) {
			const float ustep = 1.0f / (float)mTexFmt.w;
			const float vstep = 1.0f / (float)mTexFmt.h;
			const float u0 = 0.0f;
			const float v0 = 0.0f;
			const float u1 = u0 + mSource.pixmap.w * ustep;
			const float v1 = v0 + mSource.pixmap.h * vstep;
			const float f0 = -0.125f;
			const float f1 = f0 + mSource.pixmap.w / 4.0f;

			const float invVpW = 1.f / (float)vp.Width;
			const float invVpH = 1.f / (float)vp.Height;

			const float x0 = -1.f - invVpW;
			const float y0 = 1.f + invVpH;
			const float x1 = pi.mbClipPosition ? x0 + rClient.right * 2.0f * invVpW : 1.f - invVpW;
			const float y1 = pi.mbClipPosition ? y0 - rClient.bottom * 2.0f * invVpH : -1.f + invVpH;

			pvx[0].SetFF2(x0, y0, 0xFFFFFFFF, u0, v0, 0, 0);
			pvx[1].SetFF2(x1, y0, 0xFFFFFFFF, u1, v0, 1, 0);
			pvx[2].SetFF2(x0, y1, 0xFFFFFFFF, u0, v1, 0, 1);
			pvx[3].SetFF2(x1, y1, 0xFFFFFFFF, u1, v1, 1, 1);

			mpManager->UnlockVertices();
		}

		if (!mpManager->BeginScene())
			return false;

		hr = mpManager->DrawArrays(D3DPT_TRIANGLESTRIP, 0, 2);
		if (FAILED(hr)) {
			VDDEBUG_DX9DISP("VideoDisplay/DX9: Failed to draw primitive! hr=%08x\n", hr);
			return false;
		}
	}

	// NVPerfHUD 3.1 draws a bit funny if we leave this set to REVSUBTRACT, even
	// with alpha blending off....
	mpD3DDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);

	return true;
}
