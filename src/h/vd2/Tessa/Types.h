#ifndef f_VD2_TESSA_TYPES_H
#define f_VD2_TESSA_TYPES_H

#include <vd2/system/vdtypes.h>

enum VDTProgramFormat {
	kVDTPF_D3D9ByteCode
};

enum VDTFormat {
	kVDTF_Unknown,
	kVDTF_A8R8G8B8
};

struct VDTInitData2D {
	const void *mpData;
	ptrdiff_t mPitch;
};

enum VDTPrimitiveType {
	kVDTPT_Triangles,
	kVDTPT_TriangleStrip
};

enum VDTClearFlags {
	kVDTClear_None			= 0,
	kVDTClear_Color			= 1,
	kVDTClear_Depth			= 2,
	kVDTClear_Stencil		= 4,
	kVDTClear_DepthStencil	= 6,
	kVDTClear_All			= 7
};

struct VDTLockData2D {
	void		*mpData;
	ptrdiff_t	mPitch;
};

struct VDTSurfaceDesc {
	uint32 mWidth;
	uint32 mHeight;
	uint32 mFormat;
};

struct VDTTextureDesc {
	uint32 mWidth;
	uint32 mHeight;
	uint32 mMipCount;
	uint32 mFormat;
};

enum VDTElementType {
	kVDTET_Float,
	kVDTET_Float2,
	kVDTET_Float3,
	kVDTET_Float4,
	kVDTET_UByte4
};

enum VDTElementUsage {
	kVDTEU_Position,
	kVDTEU_BlendWeight,
	kVDTEU_BlendIndices,
	kVDTEU_Normal,
	kVDTEU_TexCoord,
	kVDTEU_Tangent,
	kVDTEU_Binormal,
	kVDTEU_Color
};

struct VDTVertexElement {
	uint32			mOffset;
	VDTElementType	mType;
	VDTElementUsage	mUsage;
	uint32			mUsageIndex;
};

enum VDTCullMode {
	kVDTCull_None,
	kVDTCull_Front,
	kVDTCull_Back
};

enum VDTBlendFactor {
	kVDTBlend_Zero,
	kVDTBlend_One,
	kVDTBlend_SrcColor,
	kVDTBlend_InvSrcColor,
	kVDTBlend_SrcAlpha,
	kVDTBlend_InvSrcAlpha,
	kVDTBlend_DstAlpha,
	kVDTBlend_InvDstAlpha,
	kVDTBlend_DstColor,
	kVDTBlend_InvDstColor
};

enum VDTBlendOp {
	kVDTBlendOp_Add,
	kVDTBlendOp_Subtract,
	kVDTBlendOp_RevSubtract,
	kVDTBlendOp_Min,
	kVDTBlendOp_Max
};

struct VDTBlendStateDesc {
	bool			mbEnable;
	VDTBlendFactor	mSrc;
	VDTBlendFactor	mDst;
	VDTBlendOp		mOp;
};

struct VDTRasterizerStateDesc {
	VDTCullMode		mCullMode;
	bool			mbFrontIsCCW;
	bool			mbEnableScissor;
};

enum VDTFilterMode {
	kVDTFilt_Point,
	kVDTFilt_Bilinear,
	kVDTFilt_BilinearMip,
	kVDTFilt_Trilinear,
	kVDTFilt_Anisotropic
};

enum VDTAddressMode {
	kVDTAddr_Clamp,
	kVDTAddr_Wrap
};

struct VDTSamplerStateDesc {
	VDTFilterMode	mFilterMode;
	VDTAddressMode	mAddressU;
	VDTAddressMode	mAddressV;
	VDTAddressMode	mAddressW;
};

enum VDTUsage {
	kVDTUsage_Default,
	kVDTUsage_Render
};

#endif	// f_VD2_TESSA_TYPES_H
