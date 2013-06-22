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

#define FF_STAGE(n, ca1, cop, ca2, aa1, aop, aa2)	\
	ColorOp[n] = cop;	\
	ColorArg1[n] = ca1;	\
	ColorArg2[n] = ca2;	\
	AlphaOp[n] = aop;	\
	AlphaArg1[n] = aa1;	\
	AlphaArg2[n] = aa2

#define FF_STAGE_DISABLE(n)	\
	ColorOp[n] = Disable;	\
	AlphaOp[n] = Disable

struct VertexInput {
	float4		pos		: POSITION;
	float2		uv		: TEXCOORD0;
	float2		uv2		: TEXCOORD1;
};

float4 vd_vpsize;
float4 vd_cvpsize;
float4 vd_srcsize;
float4 vd_texsize;
float4 vd_tempsize;
float4 vd_temp2size;

texture vd_srctexture;
texture vd_temptexture;
texture vd_temp2texture;
texture vd_cubictexture;

////////////////////////////////////////////////////////////////////////////////
technique point {
	pass p0 <
		bool vd_clippos = true;
	> {
		FF_STAGE(0, Texture, SelectArg1, Diffuse, Texture, SelectArg1, Diffuse);
		FF_STAGE_DISABLE(1);
		FF_STAGE_DISABLE(2);
		MinFilter[0] = Point;
		MagFilter[0] = Point;
		MipFilter[0] = Point;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		Texture[0] = <vd_srctexture>;
		AlphaBlendEnable = false;
	}
}

////////////////////////////////////////////////////////////////////////////////
technique bilinear {
	pass p0 <
		bool vd_clippos = true;
	> {
		FF_STAGE(0, Texture, SelectArg1, Diffuse, Texture, SelectArg1, Diffuse);
		FF_STAGE_DISABLE(1);
		FF_STAGE_DISABLE(2);
		MinFilter[0] = Linear;
		MagFilter[0] = Linear;
		MipFilter[0] = Linear;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		Texture[0] = <vd_srctexture>;
		AlphaBlendEnable = false;
	}
}

////////////////////////////////////////////////////////////////////////////////

struct VertexOutputBicubicFF2 {
	float4	pos		: POSITION;
	float4	diffuse	: COLOR0;
	float2	uvsrc	: TEXCOORD0;
	float2	uvfilt	: TEXCOORD1;
};

VertexOutputBicubicFF2 VertexShaderBicubicFF2A(VertexInput IN, uniform float srcu, uniform float filtv) {
	VertexOutputBicubicFF2 OUT;
	
	OUT.pos = IN.pos;
	OUT.diffuse = float4(0.5f, 0.5f, 0.5f, 0.25f);
	OUT.uvfilt.x = -0.125f + IN.uv2.x * vd_srcsize.x * 0.25f;
	OUT.uvfilt.y = filtv;
	OUT.uvsrc = IN.uv + float2(srcu, 0)*vd_texsize.wz;

	return OUT;
}

VertexOutputBicubicFF2 VertexShaderBicubicFF2B(VertexInput IN, uniform float srcv, uniform float filtv) {
	VertexOutputBicubicFF2 OUT;
	
	OUT.pos = IN.pos;
	OUT.diffuse = float4(1, 1, 1, 1);
	OUT.uvfilt.x = -0.125f + IN.uv2.y * vd_srcsize.y * 0.25f;
	OUT.uvfilt.y = filtv;
	OUT.uvsrc = (IN.uv2*float2(vd_vpsize.x, vd_srcsize.y) + float2(0, srcv))*vd_tempsize.wz;

	return OUT;
}

struct VertexOutputBicubicFF2C {
	float4	pos		: POSITION;
	float4	diffuse	: COLOR0;
	float2	uvsrc	: TEXCOORD0;
};

VertexOutputBicubicFF2C VertexShaderBicubicFF2C(VertexInput IN) {
	VertexOutputBicubicFF2C OUT;
	
	OUT.pos = IN.pos;
	OUT.diffuse = float4(0.25f, 0.25f, 0.25f, 0.25f);
	OUT.uvsrc = IN.uv2 * vd_vpsize.xy * vd_temp2size.wz;

	return OUT;
}

technique bicubicFF2 {
	pass h0 <
		string vd_target = "temp";
		string vd_viewport = "out, src";
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubicFF2A(-0.5f, 0.375f);

		Texture[0] = <vd_srctexture>;
		TexCoordIndex[0] = 0;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MagFilter[0] = Point;
		MinFilter[0] = Point;
		MipFilter[0] = Point;
		
		Texture[1] = <vd_cubictexture>;
		TexCoordIndex[1] = 1;
		AddressU[1] = Wrap;
		AddressV[1] = Wrap;
		MagFilter[1] = Point;
		MinFilter[1] = Point;
		MipFilter[1] = Point;
		
		FF_STAGE(0, Texture, MultiplyAdd, Diffuse, Texture, Modulate, Diffuse);
		ColorArg0[0] = Diffuse | AlphaReplicate;
		
		FF_STAGE(1, Current, Modulate, Texture, Current, Modulate, Texture);
		
		FF_STAGE_DISABLE(2);
		
		AlphaBlendEnable = false;
		DitherEnable = false;
	}
	
	pass h1 {
		VertexShader = compile vs_1_1 VertexShaderBicubicFF2A(+0.5f, 0.625f);
		
		AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = One;
		BlendOp = Add;
	}
	
	pass h2 {
		VertexShader = compile vs_1_1 VertexShaderBicubicFF2A(-1.5f, 0.125f);
		
		BlendOp = RevSubtract;
	}
	
	pass h3 {
		VertexShader = compile vs_1_1 VertexShaderBicubicFF2A(+1.5f, 0.875f);
	}
	
	pass v0 <
		string vd_target = "temp2";
		string vd_viewport = "out, out";
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubicFF2B(-0.5f, 0.375f);
		
		FF_STAGE(0, Texture, SelectArg1, Diffuse, Texture, Add, Texture);
		FF_STAGE(1, Texture, Modulate, Current, Texture, Modulate, Current);
		
		Texture[0] = <vd_temptexture>;
		
		AlphaBlendEnable = false;
	}
	
	pass v1 {
		VertexShader = compile vs_1_1 VertexShaderBicubicFF2B(+0.5f, 0.625f);
				
		AlphaBlendEnable = true;
		BlendOp = Add;
	}
	
	pass v2 {
		VertexShader = compile vs_1_1 VertexShaderBicubicFF2B(-1.5f, 0.125f);
		BlendOp = RevSubtract;
	}
	
	pass v3 {
		VertexShader = compile vs_1_1 VertexShaderBicubicFF2B(+1.5f, 0.875f);
	}
	
	pass final <
		string vd_target = "";
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubicFF2C();
		
		FF_STAGE(0, Texture, AddSigned2x, Current, Texture, AddSigned2x, Current);
		FF_STAGE_DISABLE(1);
			
		Texture[0] = <vd_temp2texture>;
		
		AlphaBlendEnable = false;
		DitherEnable = true;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Fixed function bicubic path - 3 texture stages, 5 passes (ATI RADEON 7xxx)
//
////////////////////////////////////////////////////////////////////////////////////////////////////

struct VertexOutputBicubicFF3 {
	float4	pos		: POSITION;
	float2	uvsrc0	: TEXCOORD0;
	float2	uvfilt	: TEXCOORD1;
	float2	uvsrc1	: TEXCOORD2;
};

VertexOutputBicubicFF3 VertexShaderBicubicFF3A(VertexInput IN) {
	VertexOutputBicubicFF3 OUT;
	
	OUT.pos = IN.pos;
	OUT.uvfilt.x = -0.125f + IN.uv2.x * vd_srcsize.x * 0.25f;
	OUT.uvfilt.y = 0.625f;
	OUT.uvsrc0 = IN.uv + float2(-1.5f, 0)*vd_texsize.wz;
	OUT.uvsrc1 = IN.uv + float2(+1.5f, 0)*vd_texsize.wz;

	return OUT;
}

VertexOutputBicubicFF3 VertexShaderBicubicFF3B(VertexInput IN) {
	VertexOutputBicubicFF3 OUT;
	
	OUT.pos = IN.pos;
	OUT.uvfilt.x = -0.125f + IN.uv2.x * vd_srcsize.x * 0.25f;
	OUT.uvfilt.y = 0.875f;
	OUT.uvsrc0 = IN.uv + float2(-0.5f, 0)*vd_texsize.wz;
	OUT.uvsrc1 = IN.uv + float2(+0.5f, 0)*vd_texsize.wz;
	
	return OUT;
}

VertexOutputBicubicFF3 VertexShaderBicubicFF3C(VertexInput IN) {
	VertexOutputBicubicFF3 OUT;
	
	OUT.pos = IN.pos;
	OUT.uvfilt.x = -0.125f + IN.uv2.y * vd_srcsize.y * 0.25f;
	OUT.uvfilt.y = 0.125f;
	
	float2 uv = IN.uv2 * float2(vd_vpsize.x, vd_srcsize.y) * vd_tempsize.wz;
	OUT.uvsrc0 = uv + float2(0, -1.5f)*vd_tempsize.wz;
	OUT.uvsrc1 = uv + float2(0, +1.5f)*vd_tempsize.wz;
	
	return OUT;
}

VertexOutputBicubicFF3 VertexShaderBicubicFF3D(VertexInput IN) {
	VertexOutputBicubicFF3 OUT;
	
	OUT.pos = IN.pos;
	OUT.uvfilt.x = -0.125f + IN.uv2.y * vd_srcsize.y * 0.25f;
	OUT.uvfilt.y = 0.375f;
	
	float2 uv = IN.uv2 * float2(vd_vpsize.x, vd_srcsize.y) * vd_tempsize.wz;
	OUT.uvsrc0 = uv + float2(0, -0.5f)*vd_tempsize.wz;
	OUT.uvsrc1 = uv + float2(0,  0.5f)*vd_tempsize.wz;
	
	return OUT;
}

struct VertexOutputBicubicFF3E {
	float4	pos		: POSITION;
	float2	uv		: TEXCOORD0;
};

VertexOutputBicubicFF3E VertexShaderBicubicFF3E(VertexInput IN) {
	VertexOutputBicubicFF3E OUT;
	
	OUT.pos = IN.pos;
	OUT.uv = IN.uv*vd_vpsize.xy*vd_temp2size.wz;
	
	return OUT;
}

technique bicubicFF3 {
	pass horiz1 <
		string vd_target = "temp";
		string vd_viewport = "out, src";
		float4 vd_clear = float4(0.5f, 0.5f, 0.5f, 0.5f);
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubicFF3A();
		
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MagFilter[0] = Point;
		MinFilter[0] = Point;
		MipFilter[0] = Point;

		Texture[1] = <vd_cubictexture>;
		AddressU[1] = Wrap;
		AddressV[1] = Wrap;
		MagFilter[1] = Point;
		MinFilter[1] = Point;
		MipFilter[1] = Point;

		Texture[2] = <vd_srctexture>;
		AddressU[2] = Clamp;
		AddressV[2] = Clamp;
		MagFilter[2] = Point;
		MinFilter[2] = Point;
		MipFilter[2] = Point;

		FF_STAGE(0, Texture, SelectArg1, Current, Texture, SelectArg1, Current);
		FF_STAGE(1, Texture, Modulate, Current, Texture, SelectArg1, Current);
		FF_STAGE(2, Current, ModulateAlpha_AddColor, Texture, Current, SelectArg1, Current);

		AlphaBlendEnable	= True;
		DitherEnable		= False;
		SrcBlend			= Zero;
		DestBlend			= InvSrcColor;
		BlendOp				= Add;
	}
	
	pass horiz2 {
		VertexShader = compile vs_1_1 VertexShaderBicubicFF3B();
				
		SrcBlend = One;
		DestBlend = One;
	}
	
	pass vert1 <
		string vd_target = "temp2";
		string vd_viewport = "out, out";
		float4 vd_clear = float4(0.5f, 0.5f, 0.5f, 0.5f);
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubicFF3C();
		
		FF_STAGE(0, Texture | Complement, SelectArg1, Current, Texture, SelectArg1, Current);
		FF_STAGE(1, Texture, Modulate, Current, Texture, SelectArg1, Current);
		FF_STAGE(2, Current, ModulateAlpha_AddColor, Texture | Complement, Current, SelectArg1, Current);
		
		Texture[0] = <vd_temptexture>;
		Texture[2] = <vd_temptexture>;		

		AlphaBlendEnable = True;
		SrcBlend = Zero;
		DestBlend = InvSrcColor;
	}
	
	pass vert2 {
		VertexShader = compile vs_1_1 VertexShaderBicubicFF3D();
				
		SrcBlend = One;
		DestBlend = One;
	}
	
	pass final <
		string vd_target = "";
		string vd_viewport = "out, out";
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubicFF3E();
		
		FF_STAGE(0, Texture | Complement, Add, Texture | Complement, Texture | Complement, Add, Texture | Complement);
		FF_STAGE_DISABLE(1);
		FF_STAGE_DISABLE(2);
		
		Texture[0] = <vd_temp2texture>;
		
		AlphaBlendEnable = False;
		DitherEnable = True;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Pixel shader 1.1 bicubic path - 3 texture stages, 5 passes (NVIDIA GeForce3/4)
//
////////////////////////////////////////////////////////////////////////////////////////////////////

struct VertexOutputBicubic1_1 {
	float4	pos		: POSITION;
	float2	uvsrc0	: TEXCOORD0;
	float2	uvfilt	: TEXCOORD1;
	float2	uvsrc1	: TEXCOORD2;
};

VertexOutputBicubic1_1 VertexShaderBicubic1_1A(VertexInput IN, uniform float4 uvoffset, uniform float filtoffset) {
	VertexOutputBicubic1_1 OUT;
	
	OUT.pos = IN.pos;
	OUT.uvfilt.x = -0.125f - IN.uv2.x * vd_srcsize.x * 0.25f;
	OUT.uvfilt.y = filtoffset;
	OUT.uvsrc0 = IN.uv + uvoffset.xy*vd_texsize.wz;
	OUT.uvsrc1 = IN.uv + uvoffset.zw*vd_texsize.wz;
	
	return OUT;
}

VertexOutputBicubic1_1 VertexShaderBicubic1_1B(VertexInput IN, uniform float4 uvoffset, uniform float filtoffset) {
	VertexOutputBicubic1_1 OUT;
	
	OUT.pos = IN.pos;
	OUT.uvfilt.x = -0.125f - IN.uv2.y * vd_srcsize.y * 0.25f;
	OUT.uvfilt.y = filtoffset;
	
	float2 uv = IN.uv2 * float2(vd_vpsize.x, vd_srcsize.y) * vd_tempsize.wz;
	OUT.uvsrc0 = uv + uvoffset.xy*vd_tempsize.wz;
	OUT.uvsrc1 = uv + uvoffset.zw*vd_tempsize.wz;
	
	return OUT;
}

pixelshader bicubic1_1_psA = asm {
	ps_1_1
	tex t0
	tex t1
	tex t2
	mul_d2 r0,t0,t1
	mul_d2 r1,t2,t1.a
	add r0,r0,r1
};

pixelshader bicubic1_1_psB = asm {
	ps_1_1
	tex t0
	tex t1
	tex t2
	mul r0,t0,t1
	mad r0,t2,t1.a,r0
};

technique bicubic1_1 {
	pass horiz1 <
		string vd_target = "temp";
		string vd_viewport="out, src";
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubic1_1A(float4(0.5f, 0, -0.5f, 0), 0.375f);
		PixelShader = <bicubic1_1_psA>;
	
		Texture[0] = <vd_srctexture>;
		AddressU[0] = Clamp;
		AddressV[0] = Clamp;
		MipFilter[0] = None;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_cubictexture>;
		AddressU[1] = Wrap;
		AddressV[1] = Clamp;
		MipFilter[1] = None;
		MinFilter[1] = Point;
		MagFilter[1] = Point;

		Texture[2] = <vd_srctexture>;
		AddressU[2] = Clamp;
		AddressV[2] = Clamp;
		MipFilter[2] = None;
		MinFilter[2] = Point;
		MagFilter[2] = Point;
		
		AlphaBlendEnable = false;
	}
	
	pass horiz2 <
		string vd_viewport="out, src";
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubic1_1A(float4(1.5f, 0, -1.5f, 0), 0.125f);
		PixelShader = <bicubic1_1_psA>;
	
		AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = One;
		BlendOp = RevSubtract;
	}
	
	pass vert1 <
		string vd_target = "";
		string vd_viewport="out, out";
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubic1_1B(float4(0, 0.5f, 0, -0.5f), 0.375f);
		PixelShader = <bicubic1_1_psB>;
		
		Texture[0] = <vd_temptexture>;
		Texture[2] = <vd_temptexture>;
		
		AlphaBlendEnable = false;
	}
	
	
	pass vert2 <
		string vd_viewport="out, out";
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubic1_1B(float4(0, 1.5f, 0, -1.5f), 0.125f);
		PixelShader = <bicubic1_1_psB>;
		
		AlphaBlendEnable = true;
		SrcBlend = One;
		DestBlend = One;
		BlendOp = RevSubtract;
	}
	
	pass final {
		VertexShader = NULL;
		PixelShader = NULL;
		
		FF_STAGE_DISABLE(0);
		FF_STAGE_DISABLE(1);
		FF_STAGE_DISABLE(2);
		
		AlphaBlendEnable = true;
		SrcBlend = DestColor;
		DestBlend = One;
		BlendOp = Add;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//
//	Pixel shader 1.4 bicubic path - 5 texture stages, 2 passes (NVIDIA GeForceFX+, ATI RADEON 8500+)
//
////////////////////////////////////////////////////////////////////////////////////////////////////

struct VertexOutputBicubic1_4 {
	float4	pos		: POSITION;
	float2	uvfilt	: TEXCOORD0;
	float2	uvsrc0	: TEXCOORD1;
	float2	uvsrc1	: TEXCOORD2;
	float2	uvsrc2	: TEXCOORD3;
	float2	uvsrc3	: TEXCOORD4;
};

VertexOutputBicubic1_4 VertexShaderBicubic1_4A(VertexInput IN) {
	VertexOutputBicubic1_4 OUT;
	
	OUT.pos = IN.pos;
	OUT.uvfilt.x = -0.125f - IN.uv2.x * vd_srcsize.x * 0.25f;
	OUT.uvfilt.y = 0.125f;
	OUT.uvsrc0 = IN.uv + float2(-1.5f, 0)*vd_texsize.wz;
	OUT.uvsrc1 = IN.uv + float2(-0.5f, 0)*vd_texsize.wz;
	OUT.uvsrc2 = IN.uv + float2(+0.5f, 0)*vd_texsize.wz;
	OUT.uvsrc3 = IN.uv + float2(+1.5f, 0)*vd_texsize.wz;
	
	return OUT;
}

VertexOutputBicubic1_4 VertexShaderBicubic1_4B(VertexInput IN) {
	VertexOutputBicubic1_4 OUT;
	
	OUT.pos = IN.pos;
	OUT.uvfilt.x = -0.125f - IN.uv2.y * vd_srcsize.y * 0.25f;
	OUT.uvfilt.y = 0.125f;
	
	float2 uv = IN.uv2 * float2(vd_vpsize.x, vd_srcsize.y) * vd_tempsize.wz;
	OUT.uvsrc0 = uv + float2(0, -1.5f)*vd_tempsize.wz;
	OUT.uvsrc1 = uv + float2(0, -0.5f)*vd_tempsize.wz;
	OUT.uvsrc2 = uv + float2(0,  0.5f)*vd_tempsize.wz;
	OUT.uvsrc3 = uv + float2(0, +1.5f)*vd_tempsize.wz;
	
	return OUT;
}

pixelshader bicubic1_4_ps = asm {
	ps_1_4
	texld r0,t0
	texld r1,t1
	texld r2,t2
	texld r3,t3
	texld r4,t4
	mul r1,r1,-r0.b
	mad r1,r2,r0.g,r1
	mad r1,r3,r0.r,r1
	mad r0,r4,-r0.a,r1
};

technique bicubic1_4 {
	pass horiz <
		string vd_target="temp";
		string vd_viewport="out, src";
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubic1_4A();
		PixelShader = <bicubic1_4_ps>;
		
		Texture[0] = <vd_cubictexture>;
		AddressU[0] = Wrap;
		AddressV[0] = Clamp;
		MipFilter[0] = None;
		MinFilter[0] = Point;
		MagFilter[0] = Point;

		Texture[1] = <vd_srctexture>;
		AddressU[1] = Clamp;
		AddressV[1] = Clamp;
		MipFilter[1] = None;
		MinFilter[1] = Point;
		MagFilter[1] = Point;
		
		Texture[2] = <vd_srctexture>;
		AddressU[2] = Clamp;
		AddressV[2] = Clamp;
		MipFilter[2] = None;
		MinFilter[2] = Point;
		MagFilter[2] = Point;
		
		Texture[3] = <vd_srctexture>;
		AddressU[3] = Clamp;
		AddressV[3] = Clamp;		
		MipFilter[3] = None;
		MinFilter[3] = Point;
		MagFilter[3] = Point;
		
		Texture[4] = <vd_srctexture>;
		AddressU[4] = Clamp;
		AddressV[4] = Clamp;		
		MipFilter[4] = None;
		MinFilter[4] = Point;
		MagFilter[4] = Point;
	}
	
	pass vert <
		string vd_target="";
		string vd_viewport="out,out";
	> {
		VertexShader = compile vs_1_1 VertexShaderBicubic1_4B();
		PixelShader = <bicubic1_4_ps>;
		Texture[1] = <vd_temptexture>;
		Texture[2] = <vd_temptexture>;
		Texture[3] = <vd_temptexture>;
		Texture[4] = <vd_temptexture>;
	}
}
