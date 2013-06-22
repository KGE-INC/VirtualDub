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

#include <d3d9.h>
#include <d3dx9.h>
#include <vd2/system/VDString.h>
#include <vd2/system/w32assist.h>
#include <vd2/system/math.h>
#include <vd2/system/filesys.h>
#include <vd2/system/time.h>
#include <vd2/Kasumi/pixmap.h>
#include <vd2/Kasumi/pixmapops.h>
#include <vd2/Kasumi/pixmaputils.h>
#include <vd2/Riza/direct3d.h>
#include "displaydrv.h"
#include "displaydrvdx9.h"

///////////////////////////////////////////////////////////////////////////

class VDD3D9TextureGeneratorFullSizeRTT : public vdrefcounted<IVDD3D9TextureGenerator> {
public:
	bool GenerateTexture(VDD3D9Manager *pManager, IVDD3D9Texture *pTexture) {
		const D3DPRESENT_PARAMETERS& parms = pManager->GetPresentParms();

		int w = parms.BackBufferWidth;
		int h = parms.BackBufferHeight;

		pManager->AdjustTextureSize(w, h);

		IDirect3DDevice9 *dev = pManager->GetDevice();
		IDirect3DTexture9 *tex;
		HRESULT hr = dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, NULL);
		if (FAILED(hr))
			return false;

		pTexture->SetD3DTexture(tex);
		tex->Release();
		return true;
	}
};

bool VDCreateD3D9TextureGeneratorFullSizeRTT(IVDD3D9TextureGenerator **ppGenerator) {
	*ppGenerator = new VDD3D9TextureGeneratorFullSizeRTT;
	if (!*ppGenerator)
		return false;
	(*ppGenerator)->AddRef();
	return true;
}

///////////////////////////////////////////////////////////////////////////


#define D3D_AUTOBREAK_2(x) if (FAILED(hr = x)) { VDASSERT(!"VideoDriver/D3DFX: Direct3D call failed: "#x); goto d3d_failed; } else ((void)0)
#define D3D_AUTOBREAK(x) if ((hr = mpD3DDevice->x), FAILED(hr)) { VDASSERT(!"VideoDriver/D3DFX: Direct3D call failed: "#x); goto d3d_failed; } else ((void)0)

#define VDDEBUG_D3DFXDISP VDDEBUG

using namespace nsVDD3D9;

namespace {
	VDStringW g_VDVideoDisplayD3DFXEffectFileName;
}

namespace {
	// {017C18AC-103F-4417-8C51-6BF6EF1E56BE}
	DEFINE_GUID(IID_ID3DXBaseEffectVersion25, 0x17c18ac, 0x103f, 0x4417, 0x8c, 0x51, 0x6b, 0xf6, 0xef, 0x1e, 0x56, 0xbe);


	#undef INTERFACE
	#define INTERFACE ID3DXBaseEffectVersion25

	DECLARE_INTERFACE_(ID3DXBaseEffectVersion25, IUnknown)
	{
		// IUnknown
		STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
		STDMETHOD_(ULONG, AddRef)(THIS) PURE;
		STDMETHOD_(ULONG, Release)(THIS) PURE;

		// Descs
		STDMETHOD(GetDesc)(THIS_ D3DXEFFECT_DESC* pDesc) PURE;
		STDMETHOD(GetParameterDesc)(THIS_ D3DXHANDLE hParameter, D3DXPARAMETER_DESC* pDesc) PURE;
		STDMETHOD(GetTechniqueDesc)(THIS_ D3DXHANDLE hTechnique, D3DXTECHNIQUE_DESC* pDesc) PURE;
		STDMETHOD(GetPassDesc)(THIS_ D3DXHANDLE hPass, D3DXPASS_DESC* pDesc) PURE;
		STDMETHOD(GetFunctionDesc)(THIS_ D3DXHANDLE hShader, D3DXFUNCTION_DESC* pDesc) PURE;

		// Handle operations
		STDMETHOD_(D3DXHANDLE, GetParameter)(THIS_ D3DXHANDLE hParameter, UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetParameterByName)(THIS_ D3DXHANDLE hParameter, LPCSTR pName) PURE;
		STDMETHOD_(D3DXHANDLE, GetParameterBySemantic)(THIS_ D3DXHANDLE hParameter, LPCSTR pSemantic) PURE;
		STDMETHOD_(D3DXHANDLE, GetParameterElement)(THIS_ D3DXHANDLE hParameter, UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetTechnique)(THIS_ UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetTechniqueByName)(THIS_ LPCSTR pName) PURE;
		STDMETHOD_(D3DXHANDLE, GetPass)(THIS_ D3DXHANDLE hTechnique, UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetPassByName)(THIS_ D3DXHANDLE hTechnique, LPCSTR pName) PURE;
		STDMETHOD_(D3DXHANDLE, GetFunction)(THIS_ UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetFunctionByName)(THIS_ LPCSTR pName) PURE;
		STDMETHOD_(D3DXHANDLE, GetAnnotation)(THIS_ D3DXHANDLE hObject, UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetAnnotationByName)(THIS_ D3DXHANDLE hObject, LPCSTR pName) PURE;

		// Get/Set Parameters
		STDMETHOD(SetValue)(THIS_ D3DXHANDLE hParameter, LPCVOID pData, UINT Bytes) PURE;
		STDMETHOD(GetValue)(THIS_ D3DXHANDLE hParameter, LPVOID pData, UINT Bytes) PURE;
		STDMETHOD(SetBool)(THIS_ D3DXHANDLE hParameter, BOOL b) PURE;
		STDMETHOD(GetBool)(THIS_ D3DXHANDLE hParameter, BOOL* pb) PURE;
		STDMETHOD(SetBoolArray)(THIS_ D3DXHANDLE hParameter, CONST BOOL* pb, UINT Count) PURE;
		STDMETHOD(GetBoolArray)(THIS_ D3DXHANDLE hParameter, BOOL* pb, UINT Count) PURE;
		STDMETHOD(SetInt)(THIS_ D3DXHANDLE hParameter, INT n) PURE;
		STDMETHOD(GetInt)(THIS_ D3DXHANDLE hParameter, INT* pn) PURE;
		STDMETHOD(SetIntArray)(THIS_ D3DXHANDLE hParameter, CONST INT* pn, UINT Count) PURE;
		STDMETHOD(GetIntArray)(THIS_ D3DXHANDLE hParameter, INT* pn, UINT Count) PURE;
		STDMETHOD(SetFloat)(THIS_ D3DXHANDLE hParameter, FLOAT f) PURE;
		STDMETHOD(GetFloat)(THIS_ D3DXHANDLE hParameter, FLOAT* pf) PURE;
		STDMETHOD(SetFloatArray)(THIS_ D3DXHANDLE hParameter, CONST FLOAT* pf, UINT Count) PURE;
		STDMETHOD(GetFloatArray)(THIS_ D3DXHANDLE hParameter, FLOAT* pf, UINT Count) PURE;
		STDMETHOD(SetVector)(THIS_ D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector) PURE;
		STDMETHOD(GetVector)(THIS_ D3DXHANDLE hParameter, D3DXVECTOR4* pVector) PURE;
		STDMETHOD(SetVectorArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector, UINT Count) PURE;
		STDMETHOD(GetVectorArray)(THIS_ D3DXHANDLE hParameter, D3DXVECTOR4* pVector, UINT Count) PURE;
		STDMETHOD(SetMatrix)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix) PURE;
		STDMETHOD(GetMatrix)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix) PURE;
		STDMETHOD(SetMatrixArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count) PURE;
		STDMETHOD(GetMatrixArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix, UINT Count) PURE;
		STDMETHOD(SetMatrixPointerArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count) PURE;
		STDMETHOD(GetMatrixPointerArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX** ppMatrix, UINT Count) PURE;
		STDMETHOD(SetMatrixTranspose)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix) PURE;
		STDMETHOD(GetMatrixTranspose)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix) PURE;
		STDMETHOD(SetMatrixTransposeArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count) PURE;
		STDMETHOD(GetMatrixTransposeArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix, UINT Count) PURE;
		STDMETHOD(SetMatrixTransposePointerArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count) PURE;
		STDMETHOD(GetMatrixTransposePointerArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX** ppMatrix, UINT Count) PURE;
		STDMETHOD(SetString)(THIS_ D3DXHANDLE hParameter, LPCSTR pString) PURE;
		STDMETHOD(GetString)(THIS_ D3DXHANDLE hParameter, LPCSTR* ppString) PURE;
		STDMETHOD(SetTexture)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DBASETEXTURE9 pTexture) PURE;
		STDMETHOD(GetTexture)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DBASETEXTURE9 *ppTexture) PURE;
		STDMETHOD(GetPixelShader)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DPIXELSHADER9 *ppPShader) PURE;
		STDMETHOD(GetVertexShader)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DVERTEXSHADER9 *ppVShader) PURE;

		//Set Range of an Array to pass to device
		//Useful for sending only a subrange of an array down to the device
		STDMETHOD(SetArrayRange)(THIS_ D3DXHANDLE hParameter, UINT uStart, UINT uEnd) PURE; 

	};

	// {D165CCB1-62B0-4a33-B3FA-A92300305A11}
	DEFINE_GUID(IID_ID3DXEffectVersion25, 
	0xd165ccb1, 0x62b0, 0x4a33, 0xb3, 0xfa, 0xa9, 0x23, 0x0, 0x30, 0x5a, 0x11);

	#undef INTERFACE
	#define INTERFACE ID3DXEffectVersion25

	DECLARE_INTERFACE_(ID3DXEffectVersion25, ID3DXBaseEffectVersion25)
	{
		// ID3DXBaseEffect
		STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
		STDMETHOD_(ULONG, AddRef)(THIS) PURE;
		STDMETHOD_(ULONG, Release)(THIS) PURE;

		// Descs
		STDMETHOD(GetDesc)(THIS_ D3DXEFFECT_DESC* pDesc) PURE;
		STDMETHOD(GetParameterDesc)(THIS_ D3DXHANDLE hParameter, D3DXPARAMETER_DESC* pDesc) PURE;
		STDMETHOD(GetTechniqueDesc)(THIS_ D3DXHANDLE hTechnique, D3DXTECHNIQUE_DESC* pDesc) PURE;
		STDMETHOD(GetPassDesc)(THIS_ D3DXHANDLE hPass, D3DXPASS_DESC* pDesc) PURE;
		STDMETHOD(GetFunctionDesc)(THIS_ D3DXHANDLE hShader, D3DXFUNCTION_DESC* pDesc) PURE;

		// Handle operations
		STDMETHOD_(D3DXHANDLE, GetParameter)(THIS_ D3DXHANDLE hParameter, UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetParameterByName)(THIS_ D3DXHANDLE hParameter, LPCSTR pName) PURE;
		STDMETHOD_(D3DXHANDLE, GetParameterBySemantic)(THIS_ D3DXHANDLE hParameter, LPCSTR pSemantic) PURE;
		STDMETHOD_(D3DXHANDLE, GetParameterElement)(THIS_ D3DXHANDLE hParameter, UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetTechnique)(THIS_ UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetTechniqueByName)(THIS_ LPCSTR pName) PURE;
		STDMETHOD_(D3DXHANDLE, GetPass)(THIS_ D3DXHANDLE hTechnique, UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetPassByName)(THIS_ D3DXHANDLE hTechnique, LPCSTR pName) PURE;
		STDMETHOD_(D3DXHANDLE, GetFunction)(THIS_ UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetFunctionByName)(THIS_ LPCSTR pName) PURE;
		STDMETHOD_(D3DXHANDLE, GetAnnotation)(THIS_ D3DXHANDLE hObject, UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetAnnotationByName)(THIS_ D3DXHANDLE hObject, LPCSTR pName) PURE;

		// Get/Set Parameters
		STDMETHOD(SetValue)(THIS_ D3DXHANDLE hParameter, LPCVOID pData, UINT Bytes) PURE;
		STDMETHOD(GetValue)(THIS_ D3DXHANDLE hParameter, LPVOID pData, UINT Bytes) PURE;
		STDMETHOD(SetBool)(THIS_ D3DXHANDLE hParameter, BOOL b) PURE;
		STDMETHOD(GetBool)(THIS_ D3DXHANDLE hParameter, BOOL* pb) PURE;
		STDMETHOD(SetBoolArray)(THIS_ D3DXHANDLE hParameter, CONST BOOL* pb, UINT Count) PURE;
		STDMETHOD(GetBoolArray)(THIS_ D3DXHANDLE hParameter, BOOL* pb, UINT Count) PURE;
		STDMETHOD(SetInt)(THIS_ D3DXHANDLE hParameter, INT n) PURE;
		STDMETHOD(GetInt)(THIS_ D3DXHANDLE hParameter, INT* pn) PURE;
		STDMETHOD(SetIntArray)(THIS_ D3DXHANDLE hParameter, CONST INT* pn, UINT Count) PURE;
		STDMETHOD(GetIntArray)(THIS_ D3DXHANDLE hParameter, INT* pn, UINT Count) PURE;
		STDMETHOD(SetFloat)(THIS_ D3DXHANDLE hParameter, FLOAT f) PURE;
		STDMETHOD(GetFloat)(THIS_ D3DXHANDLE hParameter, FLOAT* pf) PURE;
		STDMETHOD(SetFloatArray)(THIS_ D3DXHANDLE hParameter, CONST FLOAT* pf, UINT Count) PURE;
		STDMETHOD(GetFloatArray)(THIS_ D3DXHANDLE hParameter, FLOAT* pf, UINT Count) PURE;
		STDMETHOD(SetVector)(THIS_ D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector) PURE;
		STDMETHOD(GetVector)(THIS_ D3DXHANDLE hParameter, D3DXVECTOR4* pVector) PURE;
		STDMETHOD(SetVectorArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector, UINT Count) PURE;
		STDMETHOD(GetVectorArray)(THIS_ D3DXHANDLE hParameter, D3DXVECTOR4* pVector, UINT Count) PURE;
		STDMETHOD(SetMatrix)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix) PURE;
		STDMETHOD(GetMatrix)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix) PURE;
		STDMETHOD(SetMatrixArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count) PURE;
		STDMETHOD(GetMatrixArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix, UINT Count) PURE;
		STDMETHOD(SetMatrixPointerArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count) PURE;
		STDMETHOD(GetMatrixPointerArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX** ppMatrix, UINT Count) PURE;
		STDMETHOD(SetMatrixTranspose)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix) PURE;
		STDMETHOD(GetMatrixTranspose)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix) PURE;
		STDMETHOD(SetMatrixTransposeArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count) PURE;
		STDMETHOD(GetMatrixTransposeArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix, UINT Count) PURE;
		STDMETHOD(SetMatrixTransposePointerArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count) PURE;
		STDMETHOD(GetMatrixTransposePointerArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX** ppMatrix, UINT Count) PURE;
		STDMETHOD(SetString)(THIS_ D3DXHANDLE hParameter, LPCSTR pString) PURE;
		STDMETHOD(GetString)(THIS_ D3DXHANDLE hParameter, LPCSTR* ppString) PURE;
		STDMETHOD(SetTexture)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DBASETEXTURE9 pTexture) PURE;
		STDMETHOD(GetTexture)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DBASETEXTURE9 *ppTexture) PURE;
		// (!) SetPixelShader was removed relative to version 22.
		STDMETHOD(GetPixelShader)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DPIXELSHADER9 *ppPShader) PURE;
		// (!) SetVertexShader was removed relative to version 22.
		STDMETHOD(GetVertexShader)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DVERTEXSHADER9 *ppVShader) PURE;

		//Set Range of an Array to pass to device
		//Usefull for sending only a subrange of an array down to the device
		STDMETHOD(SetArrayRange)(THIS_ D3DXHANDLE hParameter, UINT uStart, UINT uEnd) PURE; 
		// ID3DXBaseEffect
    
    
		// Pool
		STDMETHOD(GetPool)(THIS_ LPD3DXEFFECTPOOL* ppPool) PURE;

		// Selecting and setting a technique
		STDMETHOD(SetTechnique)(THIS_ D3DXHANDLE hTechnique) PURE;
		STDMETHOD_(D3DXHANDLE, GetCurrentTechnique)(THIS) PURE;
		STDMETHOD(ValidateTechnique)(THIS_ D3DXHANDLE hTechnique) PURE;
		STDMETHOD(FindNextValidTechnique)(THIS_ D3DXHANDLE hTechnique, D3DXHANDLE *pTechnique) PURE;
		STDMETHOD_(BOOL, IsParameterUsed)(THIS_ D3DXHANDLE hParameter, D3DXHANDLE hTechnique) PURE;

		// Using current technique
		// Begin           starts active technique
		// BeginPass       begins a pass
		// CommitChanges   updates changes to any set calls in the pass. This should be called before
		//                 any DrawPrimitive call to d3d
		// EndPass         ends a pass
		// End             ends active technique
		STDMETHOD(Begin)(THIS_ UINT *pPasses, DWORD Flags) PURE;
		STDMETHOD(BeginPass)(THIS_ UINT Pass) PURE;
		STDMETHOD(CommitChanges)(THIS) PURE;
		STDMETHOD(EndPass)(THIS) PURE;
		STDMETHOD(End)(THIS) PURE;

		// Managing D3D Device
		STDMETHOD(GetDevice)(THIS_ LPDIRECT3DDEVICE9* ppDevice) PURE;
		STDMETHOD(OnLostDevice)(THIS) PURE;
		STDMETHOD(OnResetDevice)(THIS) PURE;

		// Logging device calls
		STDMETHOD(SetStateManager)(THIS_ LPD3DXEFFECTSTATEMANAGER pManager) PURE;
		STDMETHOD(GetStateManager)(THIS_ LPD3DXEFFECTSTATEMANAGER *ppManager) PURE;

		// Parameter blocks
		STDMETHOD(BeginParameterBlock)(THIS) PURE;
		STDMETHOD_(D3DXHANDLE, EndParameterBlock)(THIS) PURE;
		STDMETHOD(ApplyParameterBlock)(THIS_ D3DXHANDLE hParameterBlock) PURE;

		// Cloning
		STDMETHOD(CloneEffect)(THIS_ LPDIRECT3DDEVICE9 pDevice, LPD3DXEFFECT* ppEffect) PURE;
	};

	// {51B8A949-1A31-47e6-BEA0-4B30DB53F1E0}
	DEFINE_GUID(IID_ID3DXEffectCompilerVersion25, 
	0x51b8a949, 0x1a31, 0x47e6, 0xbe, 0xa0, 0x4b, 0x30, 0xdb, 0x53, 0xf1, 0xe0);

	#undef INTERFACE
	#define INTERFACE ID3DXEffectCompilerVersion25

	DECLARE_INTERFACE_(ID3DXEffectCompilerVersion25, ID3DXBaseEffectVersion25)
	{
		// ID3DXBaseEffect
		STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
		STDMETHOD_(ULONG, AddRef)(THIS) PURE;
		STDMETHOD_(ULONG, Release)(THIS) PURE;

		// Descs
		STDMETHOD(GetDesc)(THIS_ D3DXEFFECT_DESC* pDesc) PURE;
		STDMETHOD(GetParameterDesc)(THIS_ D3DXHANDLE hParameter, D3DXPARAMETER_DESC* pDesc) PURE;
		STDMETHOD(GetTechniqueDesc)(THIS_ D3DXHANDLE hTechnique, D3DXTECHNIQUE_DESC* pDesc) PURE;
		STDMETHOD(GetPassDesc)(THIS_ D3DXHANDLE hPass, D3DXPASS_DESC* pDesc) PURE;
		STDMETHOD(GetFunctionDesc)(THIS_ D3DXHANDLE hShader, D3DXFUNCTION_DESC* pDesc) PURE;

		// Handle operations
		STDMETHOD_(D3DXHANDLE, GetParameter)(THIS_ D3DXHANDLE hParameter, UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetParameterByName)(THIS_ D3DXHANDLE hParameter, LPCSTR pName) PURE;
		STDMETHOD_(D3DXHANDLE, GetParameterBySemantic)(THIS_ D3DXHANDLE hParameter, LPCSTR pSemantic) PURE;
		STDMETHOD_(D3DXHANDLE, GetParameterElement)(THIS_ D3DXHANDLE hParameter, UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetTechnique)(THIS_ UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetTechniqueByName)(THIS_ LPCSTR pName) PURE;
		STDMETHOD_(D3DXHANDLE, GetPass)(THIS_ D3DXHANDLE hTechnique, UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetPassByName)(THIS_ D3DXHANDLE hTechnique, LPCSTR pName) PURE;
		STDMETHOD_(D3DXHANDLE, GetFunction)(THIS_ UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetFunctionByName)(THIS_ LPCSTR pName) PURE;
		STDMETHOD_(D3DXHANDLE, GetAnnotation)(THIS_ D3DXHANDLE hObject, UINT Index) PURE;
		STDMETHOD_(D3DXHANDLE, GetAnnotationByName)(THIS_ D3DXHANDLE hObject, LPCSTR pName) PURE;

		// Get/Set Parameters
		STDMETHOD(SetValue)(THIS_ D3DXHANDLE hParameter, LPCVOID pData, UINT Bytes) PURE;
		STDMETHOD(GetValue)(THIS_ D3DXHANDLE hParameter, LPVOID pData, UINT Bytes) PURE;
		STDMETHOD(SetBool)(THIS_ D3DXHANDLE hParameter, BOOL b) PURE;
		STDMETHOD(GetBool)(THIS_ D3DXHANDLE hParameter, BOOL* pb) PURE;
		STDMETHOD(SetBoolArray)(THIS_ D3DXHANDLE hParameter, CONST BOOL* pb, UINT Count) PURE;
		STDMETHOD(GetBoolArray)(THIS_ D3DXHANDLE hParameter, BOOL* pb, UINT Count) PURE;
		STDMETHOD(SetInt)(THIS_ D3DXHANDLE hParameter, INT n) PURE;
		STDMETHOD(GetInt)(THIS_ D3DXHANDLE hParameter, INT* pn) PURE;
		STDMETHOD(SetIntArray)(THIS_ D3DXHANDLE hParameter, CONST INT* pn, UINT Count) PURE;
		STDMETHOD(GetIntArray)(THIS_ D3DXHANDLE hParameter, INT* pn, UINT Count) PURE;
		STDMETHOD(SetFloat)(THIS_ D3DXHANDLE hParameter, FLOAT f) PURE;
		STDMETHOD(GetFloat)(THIS_ D3DXHANDLE hParameter, FLOAT* pf) PURE;
		STDMETHOD(SetFloatArray)(THIS_ D3DXHANDLE hParameter, CONST FLOAT* pf, UINT Count) PURE;
		STDMETHOD(GetFloatArray)(THIS_ D3DXHANDLE hParameter, FLOAT* pf, UINT Count) PURE;
		STDMETHOD(SetVector)(THIS_ D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector) PURE;
		STDMETHOD(GetVector)(THIS_ D3DXHANDLE hParameter, D3DXVECTOR4* pVector) PURE;
		STDMETHOD(SetVectorArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXVECTOR4* pVector, UINT Count) PURE;
		STDMETHOD(GetVectorArray)(THIS_ D3DXHANDLE hParameter, D3DXVECTOR4* pVector, UINT Count) PURE;
		STDMETHOD(SetMatrix)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix) PURE;
		STDMETHOD(GetMatrix)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix) PURE;
		STDMETHOD(SetMatrixArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count) PURE;
		STDMETHOD(GetMatrixArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix, UINT Count) PURE;
		STDMETHOD(SetMatrixPointerArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count) PURE;
		STDMETHOD(GetMatrixPointerArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX** ppMatrix, UINT Count) PURE;
		STDMETHOD(SetMatrixTranspose)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix) PURE;
		STDMETHOD(GetMatrixTranspose)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix) PURE;
		STDMETHOD(SetMatrixTransposeArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX* pMatrix, UINT Count) PURE;
		STDMETHOD(GetMatrixTransposeArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX* pMatrix, UINT Count) PURE;
		STDMETHOD(SetMatrixTransposePointerArray)(THIS_ D3DXHANDLE hParameter, CONST D3DXMATRIX** ppMatrix, UINT Count) PURE;
		STDMETHOD(GetMatrixTransposePointerArray)(THIS_ D3DXHANDLE hParameter, D3DXMATRIX** ppMatrix, UINT Count) PURE;
		STDMETHOD(SetString)(THIS_ D3DXHANDLE hParameter, LPCSTR pString) PURE;
		STDMETHOD(GetString)(THIS_ D3DXHANDLE hParameter, LPCSTR* ppString) PURE;
		STDMETHOD(SetTexture)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DBASETEXTURE9 pTexture) PURE;
		STDMETHOD(GetTexture)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DBASETEXTURE9 *ppTexture) PURE;
		STDMETHOD(GetPixelShader)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DPIXELSHADER9 *ppPShader) PURE;
		STDMETHOD(GetVertexShader)(THIS_ D3DXHANDLE hParameter, LPDIRECT3DVERTEXSHADER9 *ppVShader) PURE;
    
		//Set Range of an Array to pass to device
		//Usefull for sending only a subrange of an array down to the device
		STDMETHOD(SetArrayRange)(THIS_ D3DXHANDLE hParameter, UINT uStart, UINT uEnd) PURE; 
		// ID3DXBaseEffect

		// Parameter sharing, specialization, and information
		STDMETHOD(SetLiteral)(THIS_ D3DXHANDLE hParameter, BOOL Literal) PURE;
		STDMETHOD(GetLiteral)(THIS_ D3DXHANDLE hParameter, BOOL *pLiteral) PURE;

		// Compilation
		STDMETHOD(CompileEffect)(THIS_ DWORD Flags,
			LPD3DXBUFFER* ppEffect, LPD3DXBUFFER* ppErrorMsgs) PURE;

		STDMETHOD(CompileShader)(THIS_ D3DXHANDLE hFunction, LPCSTR pTarget, DWORD Flags,
			LPD3DXBUFFER* ppShader, LPD3DXBUFFER* ppErrorMsgs, LPD3DXCONSTANTTABLE* ppConstantTable) PURE;
	};

	typedef BOOL (WINAPI *tpD3DXCheckVersion)(UINT D3DSDKVersion, UINT D3DXSDKVersion);
	typedef HRESULT (WINAPI *tpD3DXCreateEffectCompilerFromFileA)(
				LPCSTR pSrcFile,
				const D3DXMACRO *pDefines,
				LPD3DXINCLUDE pInclude,
				DWORD Flags,
				ID3DXEffectCompilerVersion25 **ppEffect,
				ID3DXBuffer **ppCompilationErrors);
	typedef HRESULT (WINAPI *tpD3DXCreateEffectCompilerFromFileW)(
				LPCWSTR pSrcFile,
				const D3DXMACRO *pDefines,
				LPD3DXINCLUDE pInclude,
				DWORD Flags,
				ID3DXEffectCompilerVersion25 **ppEffect,
				ID3DXBuffer **ppCompilationErrors);
	typedef HRESULT (WINAPI *tpD3DXCreateEffect)(
				LPDIRECT3DDEVICE9 pDevice,
				LPCVOID pSrcData,
				UINT SrcDataLen,
				const D3DXMACRO *pDefines,
				LPD3DXINCLUDE pInclude,
				DWORD Flags,
				ID3DXEffectPool *pPool,
				ID3DXEffectVersion25 **ppEffect,
				ID3DXBuffer **ppCompilationErrors);
	typedef HRESULT (WINAPI *tpD3DXCreateTextureShader)(CONST DWORD *pFunction, LPD3DXTEXTURESHADER *ppTextureShader);
	typedef HRESULT (WINAPI *tpD3DXFillTextureTX)(LPDIRECT3DTEXTURE9 pTexture, LPD3DXTEXTURESHADER pTextureShader);

	struct StdParamData {
		float vpsize[4];			// (viewport size)			vpwidth, vpheight, 1/vpheight, 1/vpwidth
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
		const char *name;
		int offset;
	} kStdParamInfo[]={
		{ "vd_vpsize",		offsetof(StdParamData, vpsize) },
		{ "vd_texsize",		offsetof(StdParamData, texsize) },
		{ "vd_srcsize",		offsetof(StdParamData, srcsize) },
		{ "vd_tempsize",	offsetof(StdParamData, tempsize) },
		{ "vd_temp2size",	offsetof(StdParamData, temp2size) },
		{ "vd_vpcorrect",	offsetof(StdParamData, vpcorrect) },
		{ "vd_vpcorrect2",	offsetof(StdParamData, vpcorrect2) },
		{ "vd_tvpcorrect",	offsetof(StdParamData, tvpcorrect) },
		{ "vd_tvpcorrect2",	offsetof(StdParamData, tvpcorrect2) },
		{ "vd_t2vpcorrect",		offsetof(StdParamData, t2vpcorrect) },
		{ "vd_t2vpcorrect2",	offsetof(StdParamData, t2vpcorrect2) },
		{ "vd_time",		offsetof(StdParamData, time) },
	};

	enum { kStdParamCount = sizeof kStdParamInfo / sizeof kStdParamInfo[0] };
}

class VDVideoDisplayMinidriverD3DFX : public IVDVideoDisplayMinidriver, protected VDD3D9Client {
public:
	VDVideoDisplayMinidriverD3DFX();
	~VDVideoDisplayMinidriverD3DFX();

protected:
	bool Init(HWND hwnd, const VDVideoDisplaySourceInfo& info);
	void ShutdownEffect();
	void Shutdown();

	bool ModifySource(const VDVideoDisplaySourceInfo& info);

	bool IsValid();
	bool IsFramePending() { return mSwapChainPresentLength != 0; }
	void SetFilterMode(FilterMode mode);

	bool Tick(int id);
	void Poll();
	bool Resize();
	bool Update(UpdateMode);
	void Refresh(UpdateMode);
	bool Paint(HDC hdc, const RECT& rClient, UpdateMode mode);

	bool SetSubrect(const vdrect32 *) { return false; }
	void SetLogicalPalette(const uint8 *pLogicalPalette);
	float GetSyncDelta() const { return mSyncDelta; }

protected:
	void OnPreDeviceReset() {
		mpSwapChain = NULL;
		mSwapChainW = 0;
		mSwapChainH = 0;

		if (mpEffect)
			mpEffect->OnLostDevice();
	}
	void OnPostDeviceReset() {
		if (mpEffect)
			mpEffect->OnResetDevice();
	}

	bool UpdateBackbuffer(const RECT& rClient, UpdateMode updateMode);
	bool UpdateScreen(const RECT& rClient, UpdateMode updateMode, bool polling);
	void DisplayError();

	HWND				mhwnd;
	HWND				mhwndError;
	HMODULE				mhmodD3DX;
	RECT				mrClient;

	VDD3D9Manager		*mpManager;
	IDirect3DDevice9	*mpD3DDevice;			// weak ref

	vdrefptr<IVDVideoUploadContextD3D9>	mpUploadContext;

	vdrefptr<IVDD3D9SwapChain>	mpSwapChain;
	int					mSwapChainW;
	int					mSwapChainH;
	int					mSwapChainPresentLength;
	bool				mbSwapChainImageValid;
	bool				mbFirstPresent;

	VDAtomicInt			mTickPending;

	vdrefptr<IVDD3D9Texture>	mpTempTexture;
	IDirect3DTexture9	*mpD3DTempTexture;		// weak ref
	IDirect3DSurface9	*mpD3DTempSurface;

	vdrefptr<IVDD3D9Texture>	mpTempTexture2;
	IDirect3DTexture9	*mpD3DTempTexture2;		// weak ref
	IDirect3DSurface9	*mpD3DTempSurface2;

	FilterMode			mPreferredFilter;

	ID3DXEffectVersion25 *mpEffect;
	ID3DXEffectCompilerVersion25	*mpEffectCompiler;

	VDVideoDisplaySourceInfo	mSource;
	bool				mbForceFrameUpload;

	float				mSyncDelta;
	VDD3DPresentHistory	mPresentHistory;

	VDStringW			mError;

	D3DXHANDLE			mhSrcTexture;
	D3DXHANDLE			mhPrevSrcTexture;
	D3DXHANDLE			mhPrevSrc2Texture;
	D3DXHANDLE			mhTempTexture;
	D3DXHANDLE			mhTempTexture2;
	D3DXHANDLE			mhTechniques[kFilterModeCount - 1];
	D3DXHANDLE			mhStdParamHandles[kStdParamCount];
};


IVDVideoDisplayMinidriver *VDCreateVideoDisplayMinidriverD3DFX() {
	return new VDVideoDisplayMinidriverD3DFX;
}

VDVideoDisplayMinidriverD3DFX::VDVideoDisplayMinidriverD3DFX()
	: mhwnd(NULL)
	, mhwndError(NULL)
	, mhmodD3DX(NULL)
	, mpManager(NULL)
	, mSwapChainW(0)
	, mSwapChainH(0)
	, mbSwapChainImageValid(false)
	, mbFirstPresent(false)
	, mSwapChainPresentLength(0)
	, mTickPending(0)
	, mpD3DTempTexture(NULL)
	, mpD3DTempTexture2(NULL)
	, mpD3DTempSurface(NULL)
	, mpD3DTempSurface2(NULL)
	, mpEffect(NULL)
	, mpEffectCompiler(NULL)
	, mPreferredFilter(kFilterAnySuitable)
	, mbForceFrameUpload(false)
	, mSyncDelta(0.0f)
{
	mrClient.top = mrClient.left = mrClient.right = mrClient.bottom = 0;
}

VDVideoDisplayMinidriverD3DFX::~VDVideoDisplayMinidriverD3DFX() {
}

bool VDVideoDisplayMinidriverD3DFX::Init(HWND hwnd, const VDVideoDisplaySourceInfo& info) {
	GetClientRect(hwnd, &mrClient);
	mhwnd = hwnd;
	mSource = info;
	mSyncDelta = 0.0f;

	// attempt to load d3dx9_25.dll
	mhmodD3DX = LoadLibrary("d3dx9_25.dll");

	if (!mhmodD3DX) {
		mError = L"Cannot initialize the Direct3D FX system: Unable to load d3dx9_25.dll.";
		DisplayError();
		return true;
	}

	// make sure we're using the right version
	tpD3DXCheckVersion pD3DXCheckVersion = (tpD3DXCheckVersion)GetProcAddress(mhmodD3DX, "D3DXCheckVersion");
	if (!pD3DXCheckVersion || !pD3DXCheckVersion(D3D_SDK_VERSION, 25)) {
		VDASSERT(!"Incorrect D3DX version.");
		Shutdown();
		return false;
	}

	// pull the effect compiler pointer
	tpD3DXCreateEffectCompilerFromFileA pD3DXCreateEffectCompilerFromFileA = NULL;
	tpD3DXCreateEffectCompilerFromFileW pD3DXCreateEffectCompilerFromFileW = NULL;

	if (VDIsWindowsNT())
		pD3DXCreateEffectCompilerFromFileW = (tpD3DXCreateEffectCompilerFromFileW)GetProcAddress(mhmodD3DX, "D3DXCreateEffectCompilerFromFileW");

	if (!pD3DXCreateEffectCompilerFromFileW) {
		pD3DXCreateEffectCompilerFromFileA = (tpD3DXCreateEffectCompilerFromFileA)GetProcAddress(mhmodD3DX, "D3DXCreateEffectCompilerFromFileA");

		if (!pD3DXCreateEffectCompilerFromFileA) {
			Shutdown();
			return false;
		}
	}

	tpD3DXCreateEffect			pD3DXCreateEffect			= (tpD3DXCreateEffect)			GetProcAddress(mhmodD3DX, "D3DXCreateEffect");
	tpD3DXCreateTextureShader	pD3DXCreateTextureShader	= (tpD3DXCreateTextureShader)	GetProcAddress(mhmodD3DX, "D3DXCreateTextureShader");
	tpD3DXFillTextureTX			pD3DXFillTextureTX			= (tpD3DXFillTextureTX)			GetProcAddress(mhmodD3DX, "D3DXFillTextureTX");

	if (!pD3DXCreateEffect || !pD3DXCreateTextureShader || !pD3DXFillTextureTX) {
		Shutdown();
		return false;
	}

	// attempt to initialize D3D9
	mpManager = VDInitDirect3D9(this);
	if (!mpManager) {
		Shutdown();
		return false;
	}

	mpD3DDevice = mpManager->GetDevice();

	// attempt to compile effect
	ID3DXBuffer *pError = NULL;

	const VDStringW& fxfile = g_VDVideoDisplayD3DFXEffectFileName;

	VDStringW srcfile;

	if (fxfile.find(':') != VDStringW::npos || (fxfile.size() >= 2 && fxfile[0] == '\\' && fxfile[1] == '\\'))
		srcfile = fxfile;
	else
		srcfile = VDGetProgramPath() + fxfile;

	ID3DXBuffer *pEffectBuffer = NULL;

	HRESULT hr;
	if (pD3DXCreateEffectCompilerFromFileW)
		hr = pD3DXCreateEffectCompilerFromFileW(srcfile.c_str(), NULL, NULL, 0, &mpEffectCompiler, &pError);
	else
		hr = pD3DXCreateEffectCompilerFromFileA(VDTextWToA(srcfile).c_str(), NULL, NULL, 0, &mpEffectCompiler, &pError);

	if (SUCCEEDED(hr)) {
		if (pError) {
			pError->Release();
			pError = NULL;
		}

		// compile the effect
		hr = mpEffectCompiler->CompileEffect(0, &pEffectBuffer, &pError);

		if (SUCCEEDED(hr)) {
			if (pError) {
				pError->Release();
				pError = NULL;
			}

			// create the effect
			hr = pD3DXCreateEffect(mpD3DDevice, pEffectBuffer->GetBufferPointer(), pEffectBuffer->GetBufferSize(), NULL, NULL, 0, NULL, &mpEffect, &pError);

			pEffectBuffer->Release();
		}
	}

	if (FAILED(hr)) {
		if (pError)
			mError.sprintf(L"Couldn't compile effect file %ls due to the following error:\r\n\r\n%hs\r\n\r\nIf you have Direct3D effects support enabled by mistake, it can be disabled under Options > Preferences > Display.", srcfile.c_str(), (const char *)pError->GetBufferPointer());
		else
			mError.sprintf(L"Couldn't compile effect file %ls.\r\n\r\nIf you have Direct3D effects support enabled by mistake, it can be disabled under Options > Preferences > Display.", srcfile.c_str());

		if (pError)
			pError->Release();

		ShutdownEffect();

		DisplayError();
		return true;
	}

	// scan for textures
	D3DXEFFECT_DESC effDesc;
	hr = mpEffect->GetDesc(&effDesc);
	if (FAILED(hr)) {
		Shutdown();
		return false;
	}

	int i;
	for(i=0; i<(int)effDesc.Parameters; ++i) {
		D3DXHANDLE hParm = mpEffect->GetParameter(NULL, i);
		if (!hParm)
			continue;

		D3DXPARAMETER_DESC parmDesc;
		hr = mpEffect->GetParameterDesc(hParm, &parmDesc);
		if (FAILED(hr))
			continue;

		if (parmDesc.Type != D3DXPT_TEXTURE)
			continue;

		// look for texture shader annotations
		D3DXHANDLE hFunctionName = mpEffect->GetAnnotationByName(hParm, "function");
		LPCSTR pName;
		if (SUCCEEDED(mpEffect->GetString(hFunctionName, &pName))) {
			int w = 256;
			int h = 256;
			const char *profile = "tx_1_0";

			D3DXHANDLE hAnno;
			if (hAnno = mpEffect->GetAnnotationByName(hParm, "width"))
				mpEffect->GetInt(hAnno, &w);
			if (hAnno = mpEffect->GetAnnotationByName(hParm, "height"))
				mpEffect->GetInt(hAnno, &h);
			if (hAnno = mpEffect->GetAnnotationByName(hParm, "target"))
				mpEffect->GetString(hAnno, &profile);

			// check that the function exists (CompileShader just gives us the dreaded INVALIDCALL
			// error in this case)
#if 0
			if (!mpEffect->GetFunctionByName(pName)) {
				mError = VDStringW::setf(L"Couldn't create procedural texture '%hs' in effect file %ls:\nUnknown function '%hs'", parmDesc.Name, srcfile.c_str(), pName);
				if (pError)
					pError->Release();
				ShutdownEffect();
				return true;
			}
#endif

			// create the texture
			IDirect3DTexture9 *pTexture = NULL;
			ID3DXTextureShader *pTextureShader = NULL;

			hr = mpD3DDevice->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pTexture, NULL);
			if (FAILED(hr)) {
				mError.sprintf(L"Unable to create procedural texture in effect file %ls.", srcfile.c_str());
				DisplayError();
				ShutdownEffect();
				return true;
			}

			// attempt to compile the texture shader
			hr = mpEffectCompiler->CompileShader(pName, profile, 0, &pEffectBuffer, &pError, NULL);
			if (SUCCEEDED(hr)) {
				if (pError)
					pError->Release();

				// create the texture shader
				hr = pD3DXCreateTextureShader((const DWORD *)pEffectBuffer->GetBufferPointer(), &pTextureShader);

				if (SUCCEEDED(hr)) {
					// fill the texture
					hr = pD3DXFillTextureTX(pTexture, pTextureShader);

					if (SUCCEEDED(hr))
						mpEffect->SetTexture(hParm, pTexture);
				}

				if (pTextureShader)
					pTextureShader->Release();
			}

			if (pEffectBuffer)
				pEffectBuffer->Release();

			pTexture->Release();

			if (FAILED(hr)) {
				mError.sprintf(L"Couldn't create procedural texture '%hs' in effect file %ls:\n%hs", parmDesc.Name, srcfile.c_str(), pError ? (const char *)pError->GetBufferPointer() : "unknown error");
				if (pError)
					pError->Release();
				ShutdownEffect();
				DisplayError();
				return true;
			}

			if (pError)
				pError->Release();
		}
	}

	if (mpEffectCompiler) {
		mpEffectCompiler->Release();
		mpEffectCompiler = NULL;
	}

	// scan for standard parameter handles (ok for these to fail)
	for(i=0; i<sizeof kStdParamInfo/sizeof kStdParamInfo[0]; ++i)
		mhStdParamHandles[i] = mpEffect->GetParameterByName(NULL, kStdParamInfo[i].name);

	// scan for standard techniques
	static const char *const kTechniqueNames[]={
		"point",
		"bilinear",
		"bicubic",
	};

	VDASSERTCT(sizeof kTechniqueNames / sizeof kTechniqueNames[0] == kFilterModeCount - 1);

	D3DXHANDLE hTechniqueLastValid = NULL;

	for(i=kFilterModeCount - 2; i >= 0; --i) {
		const char *baseName = kTechniqueNames[i];
		size_t baseNameLen = strlen(baseName);

		mhTechniques[i] = NULL;

		for(UINT tech=0; tech<effDesc.Techniques; ++tech) {
			D3DXHANDLE hTechnique = mpEffect->GetTechnique(tech);

			if (!hTechnique)
				continue;

			D3DXTECHNIQUE_DESC techDesc;

			hr = mpEffect->GetTechniqueDesc(hTechnique, &techDesc);
			if (FAILED(hr))
				continue;

			if (!strncmp(techDesc.Name, baseName, baseNameLen)) {
				char c = techDesc.Name[baseNameLen];

				if (!c || c=='_') {
					hTechniqueLastValid = hTechnique;
					for(int j=i; j < kFilterModeCount - 1 && !mhTechniques[j]; ++j)
						mhTechniques[j] = hTechnique;
					break;
				}
			}
		}

		if (!mhTechniques[i])
			mhTechniques[i] = hTechniqueLastValid;
	}

	// check if we don't have any recognizable techniques
	if (!hTechniqueLastValid) {
		mError.sprintf(L"Couldn't find a valid technique in effect file %ls:\nMust be one of 'point', 'bilinear', or 'bicubic.'", srcfile.c_str());
		DisplayError();
		ShutdownEffect();
		return true;
	}

	// backfill lowest level techniques that may be missing
	for(i=0; !mhTechniques[i]; ++i)
		mhTechniques[i] = hTechniqueLastValid;

	// check if we need the temp texture
	mhTempTexture = mpEffect->GetParameterByName(NULL, "vd_temptexture");
	if (mhTempTexture) {
		if (!mpManager->CreateSharedTexture<VDD3D9TextureGeneratorFullSizeRTT>("rtt1", ~mpTempTexture)) {
			mError = L"Unable to allocate temporary texture.";
			DisplayError();
			ShutdownEffect();
			return true;
		}

		mpD3DTempTexture = mpTempTexture->GetD3DTexture();

		hr = mpD3DTempTexture->GetSurfaceLevel(0, &mpD3DTempSurface);
		if (FAILED(hr)) {
			Shutdown();
			return false;
		}
	}

	// check if we need the second temp texture
	mhTempTexture2 = mpEffect->GetParameterByName(NULL, "vd_temptexture2");
	if (!mhTempTexture2)
		mhTempTexture2 = mpEffect->GetParameterByName(NULL, "vd_temp2texture");
	if (mhTempTexture2) {
		if (!mpManager->CreateSharedTexture<VDD3D9TextureGeneratorFullSizeRTT>("rtt2", ~mpTempTexture2)) {
			mError = L"Unable to allocate second temporary texture.";
			DisplayError();
			ShutdownEffect();
			return true;
		}

		mpD3DTempTexture2 = mpTempTexture2->GetD3DTexture();

		hr = mpD3DTempTexture2->GetSurfaceLevel(0, &mpD3DTempSurface2);
		if (FAILED(hr)) {
			Shutdown();
			return false;
		}
	}

	// get handle for src texture
	mhSrcTexture = mpEffect->GetParameterByName(NULL, "vd_srctexture");

	mbForceFrameUpload = false;
	if (mhSrcTexture) {
		D3DXHANDLE hForceFrameAnno = mpEffect->GetAnnotationByName(mhSrcTexture, "vd_forceframeupload");

		BOOL b;
		if (hForceFrameAnno && SUCCEEDED(mpEffect->GetBool(hForceFrameAnno, &b)) && b) {
			mbForceFrameUpload = true;
		}
	}

	// get handle for prev src texture
	mhPrevSrcTexture = mpEffect->GetParameterByName(NULL, "vd_prevsrctexture");
	mhPrevSrc2Texture = mpEffect->GetParameterByName(NULL, "vd_prevsrc2texture");

	// create upload context
	if (!VDCreateVideoUploadContextD3D9(~mpUploadContext)) {
		Shutdown();
		return false;
	}

	if (!mpUploadContext->Init(info.pixmap, info.bAllowConversion, mhPrevSrc2Texture ? 3 : mhPrevSrcTexture ? 2 : 1)) {
		Shutdown();
		return false;
	}

	VDDEBUG_D3DFXDISP("VideoDisplay/D3DFX: Initialization successful for %dx%d source image.\n", mSource.pixmap.w, mSource.pixmap.h);

	mbFirstPresent = true;
	return true;
}

void VDVideoDisplayMinidriverD3DFX::ShutdownEffect() {
	mpSwapChain = NULL;
	mSwapChainW = 0;
	mSwapChainH = 0;

	if (mpEffectCompiler) {
		mpEffectCompiler->Release();
		mpEffectCompiler = NULL;
	}

	if (mpEffect) {
		mpEffect->Release();
		mpEffect = NULL;
	}
}

void VDVideoDisplayMinidriverD3DFX::Shutdown() {
	if (mhwndError) {
		DestroyWindow(mhwndError);
		mhwndError = NULL;
	}

	ShutdownEffect();

	if (mpD3DTempSurface2) {
		mpD3DTempSurface2->Release();
		mpD3DTempSurface2 = NULL;
	}

	if (mpD3DTempSurface) {
		mpD3DTempSurface->Release();
		mpD3DTempSurface = NULL;
	}

	mpD3DTempTexture2 = NULL;
	mpTempTexture2 = NULL;

	mpD3DTempTexture = NULL;
	mpTempTexture = NULL;

	mpUploadContext = NULL;

	if (mpManager) {
		VDDeinitDirect3D9(mpManager, this);
		mpManager = NULL;
	}

	if (mhmodD3DX) {
		FreeLibrary(mhmodD3DX);
		mhmodD3DX = NULL;
	}
}

bool VDVideoDisplayMinidriverD3DFX::ModifySource(const VDVideoDisplaySourceInfo& info) {
	if (mSource.pixmap.w == info.pixmap.w && mSource.pixmap.h == info.pixmap.h && mSource.pixmap.format == info.pixmap.format && mSource.pixmap.pitch == info.pixmap.pitch
		&& !mSource.bPersistent) {
		mSource = info;
		return true;
	}
	return false;
}

bool VDVideoDisplayMinidriverD3DFX::IsValid() {
	return mpD3DDevice != 0;
}

void VDVideoDisplayMinidriverD3DFX::SetFilterMode(FilterMode mode) {
	mPreferredFilter = mode;
}

bool VDVideoDisplayMinidriverD3DFX::Tick(int id) {
	return true;
}

void VDVideoDisplayMinidriverD3DFX::Poll() {
	if (mSwapChainPresentLength) {
		UpdateScreen(mrClient, kModeVSync, false);
	}
}

bool VDVideoDisplayMinidriverD3DFX::Resize() {
	mbSwapChainImageValid = false;
	mSwapChainPresentLength = 0;
	if (GetClientRect(mhwnd, &mrClient)) {
		if (mhwndError)
			SetWindowPos(mhwndError, NULL, 0, 0, mrClient.right, mrClient.bottom, SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
	}
	return true;
}

bool VDVideoDisplayMinidriverD3DFX::Update(UpdateMode mode) {
	if (!mpEffect)
		return true;

	uint32 fieldMode = mode & kModeFieldMask;
	if (mbForceFrameUpload) {
		if (mode & kModeFirstField)
			fieldMode = kModeAllFields;
		else if ((mode & kModeFieldMask) != kModeAllFields) {
			UpdateBackbuffer(mrClient, mode);
			mSource.mpCB->ReleaseActiveFrame();
			return true;
		}
	}

	int fieldMask = 3;

	switch(fieldMode) {
		case kModeEvenField:
			fieldMask = 1;
			break;

		case kModeOddField:
			fieldMask = 2;
			break;

		case kModeAllFields:
			break;
	}

	bool success = mpUploadContext->Update(mSource.pixmap, fieldMask);

	mSource.mpCB->ReleaseActiveFrame();

	if (!success)
		return false;

	UpdateBackbuffer(mrClient, mode);

	return true;
}

void VDVideoDisplayMinidriverD3DFX::Refresh(UpdateMode mode) {
	RECT r;
	GetClientRect(mhwnd, &r);
	if (r.right > 0 && r.bottom > 0) {
		Paint(NULL, r, mode);
	}
}

bool VDVideoDisplayMinidriverD3DFX::Paint(HDC hdc, const RECT& rClient, UpdateMode updateMode) {
	if (!mpEffect) {
		mSource.mpCB->ReleaseActiveFrame();
		return true;
	}

	return (mbSwapChainImageValid || UpdateBackbuffer(rClient, updateMode)) && UpdateScreen(rClient, updateMode, false);
}

void VDVideoDisplayMinidriverD3DFX::SetLogicalPalette(const uint8 *pLogicalPalette) {
}

bool VDVideoDisplayMinidriverD3DFX::UpdateBackbuffer(const RECT& rClient0, UpdateMode updateMode) {
	VDASSERT(!mSwapChainPresentLength);
	uint64 startTime = VDGetPreciseTick();
	RECT rClient = rClient0;

	int rtw = mpManager->GetMainRTWidth();
	int rth = mpManager->GetMainRTHeight();
	RECT rClippedClient={0,0,std::min<int>(rClient.right, mpManager->GetMainRTWidth()), std::min<int>(rClient.bottom, mpManager->GetMainRTHeight())};

	const D3DPRESENT_PARAMETERS& pparms = mpManager->GetPresentParms();
	if (!pparms.Windowed) {
		rClient.right = rClippedClient.right = pparms.BackBufferWidth;
		rClient.bottom = rClippedClient.bottom = pparms.BackBufferHeight;
	}

	// Make sure the device is sane.
	if (!mpManager->CheckDevice())
		return false;

	IDirect3DTexture9 *tex = mpUploadContext->GetD3DTexture(0);
	D3DSURFACE_DESC texdesc;
	HRESULT hr = tex->GetLevelDesc(0, &texdesc);
	if (FAILED(hr))
		return false;

	// Check if we need to create or resize the swap chain.
	if (mSwapChainW >= rClippedClient.right + 128 || mSwapChainH >= rClippedClient.bottom + 128) {
		mpSwapChain = NULL;
		mSwapChainW = 0;
		mSwapChainH = 0;
	}

	if (!mpSwapChain || mSwapChainW < rClippedClient.right || mSwapChainH < rClippedClient.bottom) {
		int scw = std::min<int>((rClippedClient.right + 127) & ~127, rtw);
		int sch = std::min<int>((rClippedClient.bottom + 127) & ~127, rth);

		VDDEBUG("Resizing swap chain to %dx%d\n", scw, sch);

		if (!mpManager->CreateSwapChain(scw, sch, ~mpSwapChain))
			return false;

		mSwapChainW = scw;
		mSwapChainH = sch;
	}

	// Do we need to switch bicubic modes?
	FilterMode mode = mPreferredFilter;

	if (mode == kFilterAnySuitable)
		mode = kFilterBicubic;

#if 0
		{
			double t = VDGetPreciseTick() * VDGetPreciseSecondsPerTick();
			fprintf(loggf, "[%04.0f] Starting update\n", (t - floor(t)) * 1000.0);
		}
#endif
	{
		D3D_AUTOBREAK(SetStreamSource(0, mpManager->GetVertexBuffer(), 0, sizeof(Vertex)));
		D3D_AUTOBREAK(SetIndices(mpManager->GetIndexBuffer()));
		D3D_AUTOBREAK(SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX2));
		D3D_AUTOBREAK(SetRenderState(D3DRS_LIGHTING, FALSE));
		D3D_AUTOBREAK(SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE));
		D3D_AUTOBREAK(SetRenderState(D3DRS_ALPHATESTENABLE, FALSE));

		vdrefptr<IDirect3DSurface9> pRTMain;
		IDirect3DSwapChain9 *sc = mpSwapChain->GetD3DSwapChain();
		hr = sc->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, ~pRTMain);
		if (FAILED(hr))
			return false;

		mbSwapChainImageValid = false;

		D3D_AUTOBREAK(SetRenderTarget(0, pRTMain));
		D3D_AUTOBREAK(BeginScene());

		D3DVIEWPORT9 vp = {
			0,
			0,
			rClippedClient.right,
			rClippedClient.bottom,
			0.f,
			1.f
		};

		D3D_AUTOBREAK(SetViewport(&vp));

		// fill vertex/index buffers
		if (Vertex *pvx = mpManager->LockVertices(4)) {
			float umax = (float)mSource.pixmap.w / (float)(int)texdesc.Width;
			float vmax = (float)mSource.pixmap.h / (float)(int)texdesc.Height;
			float x0 = -1.f - 1.f/rClippedClient.right;
			float x1 = x0 + 2.0f*(rClient.right / rClippedClient.right);
			float y0 = 1.f + 1.f/rClippedClient.bottom;
			float y1 = y0 - 2.0f*(rClient.bottom / rClippedClient.bottom);

			pvx[0].SetFF2(x0, y0, 0, 0, 0, 0, 0);
			pvx[1].SetFF2(x0, y1, 0, 0, vmax, 0, 1);
			pvx[2].SetFF2(x1, y0, 0, umax, 0, 1, 0);
			pvx[3].SetFF2(x1, y1, 0, umax, vmax, 1, 1);

			mpManager->UnlockVertices();
		}

		if (uint16 *dst = mpManager->LockIndices(6)) {
			dst[0] = 0;
			dst[1] = 1;
			dst[2] = 2;
			dst[3] = 2;
			dst[4] = 1;
			dst[5] = 3;

			mpManager->UnlockIndices();
		}

		// render the effect
		StdParamData data;

		data.vpsize[0] = (float)vp.Width;
		data.vpsize[1] = (float)vp.Height;
		data.vpsize[2] = 1.0f / (float)vp.Height;
		data.vpsize[3] = 1.0f / (float)vp.Width;
		data.texsize[0] = (float)(int)texdesc.Width;
		data.texsize[1] = (float)(int)texdesc.Height;
		data.texsize[2] = 1.0f / (float)(int)texdesc.Height;
		data.texsize[3] = 1.0f / (float)(int)texdesc.Width;
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
		data.vpcorrect[0] = 2.0f / vp.Width;
		data.vpcorrect[1] = 2.0f / vp.Height;
		data.vpcorrect[2] = -1.0f / (float)vp.Height;
		data.vpcorrect[3] = 1.0f / (float)vp.Width;
		data.vpcorrect2[0] = 2.0f / vp.Width;
		data.vpcorrect2[1] = -2.0f / vp.Height;
		data.vpcorrect2[2] = 1.0f + 1.0f / (float)vp.Height;
		data.vpcorrect2[3] = -1.0f - 1.0f / (float)vp.Width;
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

		if (mhSrcTexture)
			mpEffect->SetTexture(mhSrcTexture, mpUploadContext->GetD3DTexture(0));

		if (mhPrevSrcTexture)
			mpEffect->SetTexture(mhPrevSrcTexture, mpUploadContext->GetD3DTexture(1));

		if (mhPrevSrc2Texture)
			mpEffect->SetTexture(mhPrevSrc2Texture, mpUploadContext->GetD3DTexture(2));

		if (mhTempTexture) {
			mpEffect->SetTexture(mhTempTexture, mpD3DTempTexture);

			float tempw = (float)mpTempTexture->GetWidth();
			float temph = (float)mpTempTexture->GetHeight();

			data.tempsize[0] = tempw;
			data.tempsize[1] = temph;
			data.tempsize[2] = 1.0f / temph;
			data.tempsize[3] = 1.0f / tempw;
			data.tvpcorrect[0] = 2.0f * data.tempsize[3];
			data.tvpcorrect[1] = 2.0f * data.tempsize[2];
			data.tvpcorrect[2] = -data.tempsize[2];
			data.tvpcorrect[3] = data.tempsize[3];
			data.tvpcorrect2[0] = 2.0f * data.tempsize[3];
			data.tvpcorrect2[1] = -2.0f * data.tempsize[2];
			data.tvpcorrect2[2] = 1.0f + data.tempsize[2];
			data.tvpcorrect2[3] = -1.0f - data.tempsize[3];
		}

		if (mhTempTexture2) {
			mpEffect->SetTexture(mhTempTexture2, mpD3DTempTexture2);

			float temp2w = (float)mpTempTexture2->GetWidth();
			float temp2h = (float)mpTempTexture2->GetHeight();

			data.temp2size[0] = temp2w;
			data.temp2size[1] = temp2h;
			data.temp2size[2] = 1.0f / temp2h;
			data.temp2size[3] = 1.0f / temp2w;
			data.t2vpcorrect[0] = 2.0f * data.temp2size[3];
			data.t2vpcorrect[1] = 2.0f * data.temp2size[2];
			data.t2vpcorrect[2] = -data.temp2size[2];
			data.t2vpcorrect[3] = data.temp2size[3];
			data.t2vpcorrect2[0] = 2.0f * data.temp2size[3];
			data.t2vpcorrect2[1] = -2.0f * data.temp2size[2];
			data.t2vpcorrect2[2] = 1.0f + data.temp2size[2];
			data.t2vpcorrect2[3] = -1.0f - data.temp2size[3];
		}

		for(int i=0; i<kStdParamCount; ++i) {
			D3DXHANDLE h = mhStdParamHandles[i];

			if (h)
				mpEffect->SetVector(h, (const D3DXVECTOR4 *)((const char *)&data + kStdParamInfo[i].offset));
		}

		UINT passes;
		D3DXHANDLE hTechnique = mhTechniques[mode - 1];
		D3D_AUTOBREAK_2(mpEffect->SetTechnique(hTechnique));
		D3D_AUTOBREAK_2(mpEffect->Begin(&passes, 0));

		int lastTarget = -1;

		for(UINT pass=0; pass<passes; ++pass) {
			D3DXHANDLE hPass = mpEffect->GetPass(hTechnique, pass);

			D3DXHANDLE hFieldAnno = mpEffect->GetAnnotationByName(hPass, "vd_fieldmask");
			if (hFieldAnno) {
				INT fieldMask;

				if (SUCCEEDED(mpEffect->GetInt(hFieldAnno, &fieldMask))) {
					const uint32 fieldMode = updateMode & kModeFieldMask;

					if (!(fieldMode & fieldMask))
						continue;
				}
			}

			int newTarget = -1;

			if (D3DXHANDLE hTarget = mpEffect->GetAnnotationByName(hPass, "vd_target")) {
				const char *s;

				if (SUCCEEDED(mpEffect->GetString(hTarget, &s)) && s) {
					if (!strcmp(s, "temp"))
						newTarget = 0;
					else if (!strcmp(s, "temp2"))
						newTarget = 1;
				}
			}

			if (lastTarget != newTarget) {
				lastTarget = newTarget;

				if (newTarget == 0)
					D3D_AUTOBREAK(SetRenderTarget(0, mpD3DTempSurface));
				else if (newTarget == 1)
					D3D_AUTOBREAK(SetRenderTarget(0, mpD3DTempSurface2));
				else {
					D3D_AUTOBREAK(SetRenderTarget(0, pRTMain));
					D3D_AUTOBREAK(SetViewport(&vp));
				}
			}

			if (D3DXHANDLE hClear = mpEffect->GetAnnotationByName(hPass, "vd_clear")) {
				float clearColor[4];

				if (SUCCEEDED(mpEffect->GetVector(hClear, (D3DXVECTOR4 *)clearColor))) {
					int r = VDRoundToInt(clearColor[0]);
					int g = VDRoundToInt(clearColor[1]);
					int b = VDRoundToInt(clearColor[2]);
					int a = VDRoundToInt(clearColor[3]);

					if ((unsigned)r >= 256) r = (~r >> 31) & 255;
					if ((unsigned)g >= 256) g = (~g >> 31) & 255;
					if ((unsigned)b >= 256) b = (~b >> 31) & 255;
					if ((unsigned)a >= 256) a = (~a >> 31) & 255;

					D3DCOLOR clearColor = (a<<24) + (r<<16) + (g<<8) + b;

					D3D_AUTOBREAK(Clear(0, NULL, D3DCLEAR_TARGET, clearColor, 1.f, 0));
				}
			}

			D3D_AUTOBREAK_2(mpEffect->BeginPass(pass));

			mpManager->DrawElements(D3DPT_TRIANGLELIST, 0, 4, 0, 2);

			D3D_AUTOBREAK_2(mpEffect->EndPass());
		}

#if 0
		D3DRECT statRect[17], attemptRect[17];
		for(int i=0; i<17; ++i) {
			D3DRECT& r = statRect[i];
			r.x1 = 0;
			r.x2 = VDRoundToInt(mPresentHistory.mSuccessProb[i] * 400.0f) + 1;
			r.y1 = 40 + 20*i;
			r.y2 = 40 + 20*i + 5;

			D3DRECT& r2 = attemptRect[i];
			r2.x1 = 0;
			r2.x2 = VDRoundToInt(mPresentHistory.mAttemptProb[i] * 400.0f) + 1;
			r2.y1 = 40 + 20*i + 5;
			r2.y2 = 40 + 20*i + 10;
		}

		mpD3DDevice->Clear(17, statRect, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 255, 0, 0), 0.0f, 0);
		mpD3DDevice->Clear(17, attemptRect, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 255, 0), 0.0f, 0);

		D3DRECT barRect = {401, 0, 402, 340};
		mpD3DDevice->Clear(1, &barRect, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 128, 128, 128), 0.0f, 0);

		D3DRECT targetRect = {0, 0, 400, 0};
		targetRect.y1 = VDRoundToInt(mPresentHistory.mScanlineTarget * 400.0f / 1200.0f);
		targetRect.y2 = targetRect.y1 + 1;
		mpD3DDevice->Clear(1, &targetRect, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 192, 192, 0), 0.0f, 0);
#endif

		D3D_AUTOBREAK_2(mpEffect->End());
		D3D_AUTOBREAK(EndScene());
		mpManager->Flush();
	}

	mpManager->SetSwapChainActive(NULL);
	mbSwapChainImageValid = true;
	++mSwapChainPresentLength;

d3d_failed:
	return true;
}

bool VDVideoDisplayMinidriverD3DFX::UpdateScreen(const RECT& rClient, UpdateMode updateMode, bool polling) {
	if (!mbSwapChainImageValid || !mpSwapChain)
		return false;

	HRESULT hr = mpManager->PresentSwapChain(mpSwapChain, &rClient, mhwnd, (updateMode & kModeVSync) != 0, !polling, true, mSyncDelta, mPresentHistory);

	bool dec = false;
	if (hr == S_FALSE)
		return true;
	else if (hr == S_OK) {
		if (mSwapChainPresentLength) {
			dec = true;
			--mSwapChainPresentLength;
		}
	}

	// Workaround for Windows Vista DWM composition chain not updating.
	if (mbFirstPresent) {
		SetWindowPos(mhwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER|SWP_FRAMECHANGED);
		mbFirstPresent = false;
	}

	if (FAILED(hr)) {
		VDDEBUG("VideoDisplay/D3DFX: Render failed -- applying boot to the head.\n");

		if (!mpManager->Reset())
			return false;
	} else if (dec) {
		mSource.mpCB->RequestNextFrame();
	}

	return true;
}

void VDVideoDisplayMinidriverD3DFX::DisplayError() {
	if (mhwndError) {
		DestroyWindow(mhwndError);
		mhwndError = NULL;
	}

	HINSTANCE hInst = VDGetLocalModuleHandleW32();
	if (VDIsWindowsNT())
		mhwndError = CreateWindowW(L"EDIT", mError.c_str(), WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY, 0, 0, mrClient.right, mrClient.bottom, mhwnd, NULL, hInst, NULL);
	else
		mhwndError = CreateWindowA("EDIT", VDTextWToA(mError).c_str(), WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY, 0, 0, mrClient.right, mrClient.bottom, mhwnd, NULL, hInst, NULL);

	if (mhwndError)
		SendMessage(mhwndError, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
}

///////////////////////////////////////////////////////////////////////////

void VDVideoDisplaySetD3DFXFileName(const wchar_t *fileName) {
	g_VDVideoDisplayD3DFXEffectFileName = fileName;
}
