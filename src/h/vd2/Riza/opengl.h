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

#ifndef f_VD2_RIZA_OPENGL_H
#define f_VD2_RIZA_OPENGL_H

#include <windows.h>
#include <gl/gl.h>

struct VDAPITableWGL {
	HGLRC 	(APIENTRY *wglCreateContext)(HDC hdc);
	BOOL	(APIENTRY *wglDeleteContext)(HGLRC hglrc);
	BOOL	(APIENTRY *wglMakeCurrent)(HDC  hdc, HGLRC hglrc);
	PROC	(APIENTRY *wglGetProcAddress)(const char *lpszProc);
	BOOL	(APIENTRY *wglSwapBuffers)(HDC  hdc);
};

struct VDAPITableOpenGL {
	// OpenGL 1.1
	void	(APIENTRY *glAlphaFunc)(GLenum func, GLclampf ref);
	void	(APIENTRY *glBegin)(GLenum mode);
	void	(APIENTRY *glBindTexture)(GLenum target, GLuint texture);
	void	(APIENTRY *glBlendFunc)(GLenum sfactor, GLenum dfactor);
	void	(APIENTRY *glCallList)(GLuint list);
	void	(APIENTRY *glClear)(GLbitfield mask);
	void	(APIENTRY *glClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
	void	(APIENTRY *glColor4d)(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
	void	(APIENTRY *glColor4f)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
	void	(APIENTRY *glColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
	void	(APIENTRY *glCopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
	void	(APIENTRY *glDeleteLists)(GLuint list, GLsizei range);
	void	(APIENTRY *glDeleteTextures)(GLsizei n, const GLuint *textures);
	void	(APIENTRY *glDepthFunc)(GLenum func);
	void	(APIENTRY *glDepthMask)(GLboolean mask);
	void	(APIENTRY *glDisable)(GLenum cap);
	void	(APIENTRY *glDrawBuffer)(GLenum mode);
	void	(APIENTRY *glEnable)(GLenum cap);
	void	(APIENTRY *glEnd)();
	void	(APIENTRY *glEndList)();
	void	(APIENTRY *glFinish)();
	void	(APIENTRY *glFlush)();
	GLenum	(APIENTRY *glGetError)();
	void	(APIENTRY *glGetIntegerv)(GLenum pname, GLint *params);
	GLuint	(APIENTRY *glGenLists)(GLsizei range);
	const GLubyte *(APIENTRY *glGetString)(GLenum);
	void	(APIENTRY *glGenTextures)(GLsizei n, GLuint *textures);
	void	(APIENTRY *glLoadIdentity)();
	void	(APIENTRY *glLoadMatrixd)(const GLdouble *m);
	void	(APIENTRY *glMatrixMode)(GLenum target);
	void	(APIENTRY *glNewList)(GLuint list, GLenum mode);
	void	(APIENTRY *glOrtho)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
	void	(APIENTRY *glPixelStorei)(GLenum pname, GLint param);
	void	(APIENTRY *glReadBuffer)(GLenum mode);
	void	(APIENTRY *glReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
	void	(APIENTRY *glTexCoord2d)(GLdouble s, GLdouble t);
	void	(APIENTRY *glTexCoord2f)(GLfloat s, GLfloat t);
	void	(APIENTRY *glTexEnvi)(GLenum target, GLenum pname, GLint param);
	void	(APIENTRY *glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
	void	(APIENTRY *glTexParameterfv)(GLenum target, GLenum pname, const GLfloat *params);
	void	(APIENTRY *glTexParameteri)(GLenum target, GLenum pname, GLint param);
	void	(APIENTRY *glTexSubImage2D)(GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
	void	(APIENTRY *glVertex2d)(GLdouble x, GLdouble y);
	void	(APIENTRY *glVertex2f)(GLfloat x, GLfloat y);
	void	(APIENTRY *glVertex2i)(GLint x, GLint y);
	void	(APIENTRY *glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);
};

#define		GL_OCCLUSION_TEST_HP				0x8165
#define		GL_OCCLUSION_TEST_RESULT_HP			0x8166

#define		GL_TEXTURE0_ARB						0x84C0
#define		GL_TEXTURE1_ARB						0x84C1
#define		GL_TEXTURE2_ARB						0x84C2
#define		GL_TEXTURE3_ARB						0x84C3
#define		GL_TEXTURE4_ARB						0x84C4
#define		GL_TEXTURE5_ARB						0x84C5
#define		GL_TEXTURE6_ARB						0x84C6
#define		GL_TEXTURE7_ARB						0x84C7

#define		GL_REGISTER_COMBINERS_NV			0x8522
#define		GL_VARIABLE_A_NV					0x8523
#define		GL_VARIABLE_B_NV					0x8524
#define		GL_VARIABLE_C_NV					0x8525
#define		GL_VARIABLE_D_NV					0x8526
#define		GL_VARIABLE_E_NV					0x8527
#define		GL_VARIABLE_F_NV					0x8528
#define		GL_VARIABLE_G_NV					0x8529
#define		GL_CONSTANT_COLOR0_NV				0x852A
#define		GL_CONSTANT_COLOR1_NV				0x852B
#define		GL_PRIMARY_COLOR_NV					0x852C
#define		GL_SECONDARY_COLOR_NV				0x852D
#define		GL_SPARE0_NV						0x852E
#define		GL_SPARE1_NV						0x852F
#define		GL_DISCARD_NV						0x8530
#define		GL_E_TIMES_F_NV						0x8531
#define		GL_SPARE0_PLUS_SECONDARY_COLOR_NV	0x8532
#define		GL_PER_STAGE_CONSTANTS_NV			0x8535
#define		GL_UNSIGNED_IDENTITY_NV				0x8536
#define		GL_UNSIGNED_INVERT_NV				0x8537
#define		GL_EXPAND_NORMAL_NV					0x8538
#define		GL_EXPAND_NEGATE_NV					0x8539
#define		GL_HALF_BIAS_NORMAL_NV				0x853A
#define		GL_HALF_BIAS_NEGATE_NV				0x853B
#define		GL_SIGNED_IDENTITY_NV				0x853C
#define		GL_SIGNED_NEGATE_NV					0x853D
#define		GL_SCALE_BY_TWO_NV					0x853E
#define		GL_SCALE_BY_FOUR_NV					0x853F
#define		GL_SCALE_BY_ONE_HALF_NV				0x8540
#define		GL_BIAS_BY_NEGATIVE_ONE_HALF_NV		0x8541
#define		GL_COMBINER_INPUT_NV				0x8542
#define		GL_COMBINER_MAPPING_NV				0x8543
#define		GL_COMBINER_COMPONENT_USAGE_NV		0x8544
#define		GL_COMBINER_AB_DOT_PRODUCT_NV		0x8545
#define		GL_COMBINER_CD_DOT_PRODUCT_NV		0x8546
#define		GL_COMBINER_MUX_SUM_NV				0x8547
#define		GL_COMBINER_SCALE_NV				0x8548
#define		GL_COMBINER_BIAS_NV					0x8549
#define		GL_COMBINER_AB_OUTPUT_NV			0x854A
#define		GL_COMBINER_CD_OUTPUT_NV			0x854B
#define		GL_COMBINER_SUM_OUTPUT_NV			0x854C
#define		GL_MAX_GENERAL_COMBINERS_NV			0x854D
#define		GL_NUM_GENERAL_COMBINERS_NV			0x854E
#define		GL_COLOR_SUM_CLAMP_NV				0x854F
#define		GL_COMBINER0_NV						0x8550
#define		GL_COMBINER1_NV						0x8551
#define		GL_COMBINER2_NV						0x8552
#define		GL_COMBINER3_NV						0x8553
#define		GL_COMBINER4_NV						0x8554
#define		GL_COMBINER5_NV						0x8555
#define		GL_COMBINER6_NV						0x8556
#define		GL_COMBINER7_NV						0x8557

#define		GL_COMBINE_EXT						0x8570
#define		GL_COMBINE_RGB_EXT					0x8571
#define		GL_COMBINE_ALPHA_EXT				0x8572
#define		GL_RGB_SCALE_EXT					0x8573
#define		GL_ADD_SIGNED_EXT					0x8574
#define		GL_INTERPOLATE_EXT					0x8575
#define		GL_CONSTANT_EXT						0x8576
#define		GL_PRIMARY_COLOR_EXT				0x8577
#define		GL_PRIMARY_COLOR_ARB				0x8577
#define		GL_PREVIOUS_EXT						0x8578
#define		GL_SOURCE0_RGB_EXT					0x8580
#define		GL_SOURCE1_RGB_EXT					0x8581
#define		GL_SOURCE2_RGB_EXT					0x8582
#define		GL_SOURCE0_ALPHA_EXT				0x8588
#define		GL_SOURCE1_ALPHA_EXT				0x8589
#define		GL_SOURCE2_ALPHA_EXT				0x858A
#define		GL_OPERAND0_RGB_EXT					0x8590
#define		GL_OPERAND1_RGB_EXT					0x8591
#define		GL_OPERAND2_RGB_EXT					0x8592
#define		GL_OPERAND0_ALPHA_EXT				0x8598
#define		GL_OPERAND1_ALPHA_EXT				0x8599
#define		GL_OPERAND2_ALPHA_EXT				0x859A

#define		GL_PIXEL_COUNTER_BITS_NV			0x8864
#define		GL_CURRENT_OCCLUSION_QUERY_ID_NV	0x8865
#define		GL_PIXEL_COUNT_NV					0x8866
#define		GL_PIXEL_COUNT_AVAILABLE_NV			0x8867

#define		GL_READ_ONLY_ARB					0x88B8
#define		GL_WRITE_ONLY_ARB					0x88B9
#define		GL_READ_WRITE_ARB					0x88BA
#define		GL_STREAM_DRAW_ARB					0x88E0
#define		GL_STREAM_READ_ARB					0x88E1
#define		GL_STREAM_COPY_ARB					0x88E2
#define		GL_STATIC_DRAW_ARB					0x88E4
#define		GL_STATIC_READ_ARB					0x88E5
#define		GL_STATIC_COPY_ARB					0x88E6
#define		GL_DYNAMIC_DRAW_ARB					0x88E8
#define		GL_DYNAMIC_READ_ARB					0x88E9
#define		GL_DYNAMIC_COPY_ARB					0x88EA

#define		GL_PIXEL_PACK_BUFFER_ARB			0x88EB
#define		GL_PIXEL_UNPACK_BUFFER_ARB			0x88EC
#define		GL_PIXEL_PACK_BUFFER_BINDING_ARB	0x88ED
#define		GL_PIXEL_UNPACK_BUFFER_BINDING_ARB	0x88EF

#define		GL_FRAGMENT_SHADER_ATI	0x8920
    
#define		GL_REG_0_ATI			0x8921
#define		GL_REG_1_ATI			0x8922
#define		GL_REG_2_ATI			0x8923
#define		GL_REG_3_ATI			0x8924
#define		GL_REG_4_ATI			0x8925
#define		GL_REG_5_ATI			0x8926

#define		GL_CON_0_ATI			0x8941
#define		GL_CON_1_ATI			0x8942
#define		GL_CON_2_ATI			0x8943
#define		GL_CON_3_ATI			0x8944
#define		GL_CON_4_ATI			0x8945
#define		GL_CON_5_ATI			0x8946
#define		GL_CON_6_ATI			0x8947
#define		GL_CON_7_ATI			0x8948

#define		GL_MOV_ATI				0x8961
#define		GL_ADD_ATI				0x8963
#define		GL_MUL_ATI				0x8964
#define		GL_SUB_ATI				0x8965
#define		GL_DOT3_ATI				0x8966
#define		GL_DOT4_ATI				0x8967
#define		GL_MAD_ATI				0x8968
#define		GL_LERP_ATI				0x8969
#define		GL_CND_ATI				0x896A
#define		GL_CND0_ATI				0x896B
#define		GL_DOT2_ADD_ATI			0x896C

#define		GL_SECONDARY_INTERPOLATOR_ATI	0x896D

#define		GL_SWIZZLE_STR_ATI		0x8976
#define		GL_SWIZZLE_STQ_ATI		0x8977
#define		GL_SWIZZLE_STR_DR_ATI	0x8978
#define		GL_SWIZZLE_STQ_DQ_ATI	0x8979

#define		GL_RED_BIT_ATI			0x00000001
#define		GL_GREEN_BIT_ATI		0x00000002
#define		GL_BLUE_BIT_ATI			0x00000004

#define		GL_2X_BIT_ATI			0x00000001
#define		GL_4X_BIT_ATI			0x00000002
#define		GL_8X_BIT_ATI			0x00000004
#define		GL_HALF_BIT_ATI			0x00000008
#define		GL_QUARTER_BIT_ATI		0x00000010
#define		GL_EIGHTH_BIT_ATI		0x00000020
#define		GL_SATURATE_BIT_ATI		0x00000040
    
#define		GL_COMP_BIT_ATI			0x00000002
#define		GL_NEGATE_BIT_ATI		0x00000004
#define		GL_BIAS_BIT_ATI			0x00000008

typedef size_t GLsizeiptrARB;
typedef ptrdiff_t GLintptrARB;

struct VDAPITableOpenGLEXT {
	// ARB_multitexture
	void	(APIENTRY *glActiveTextureARB)(GLenum texture);
	void	(APIENTRY *glMultiTexCoord2fARB)(GLenum texture, GLfloat s, GLfloat t);

	// ARB_vertex_buffer_object (EXT_pixel_buffer_object)
	void		(APIENTRY *glBindBufferARB)(GLenum target, GLuint buffer);
	void		(APIENTRY *glDeleteBuffersARB)(GLsizei n, const GLuint *buffers);
	void		(APIENTRY *glGenBuffersARB)(GLsizei n, GLuint *buffers);
	GLboolean	(APIENTRY *glIsBufferARB)(GLuint buffer);
	void		(APIENTRY *glBufferDataARB)(GLenum target, GLsizeiptrARB size, const void *data, GLenum usage);
	void		(APIENTRY *glBufferSubDataARB)(GLenum target, GLintptrARB offset, GLsizeiptrARB size, const void *data);
	void		(APIENTRY *glGetBufferSubDataARB)(GLenum target, GLintptrARB offset, GLsizeiptrARB size, void *data);
	void *		(APIENTRY *glMapBufferARB)(GLenum target, GLenum access);
	GLboolean	(APIENTRY *glUnmapBufferARB)(GLenum target);
	void		(APIENTRY *glGetBufferParameterivARB)(GLenum target, GLenum pname, GLint *params);
	void		(APIENTRY *glGetBufferPointervARB)(GLenum target, GLenum pname, void **params);

	// NV_register_combiners
	void (APIENTRY *glCombinerParameterfvNV)(GLenum pname, const GLfloat *params);
	void (APIENTRY *glCombinerParameterivNV)(GLenum pname, const GLint *params);
	void (APIENTRY *glCombinerParameterfNV)(GLenum pname, GLfloat param);
	void (APIENTRY *glCombinerParameteriNV)(GLenum pname, GLint param);
	void (APIENTRY *glCombinerInputNV)(GLenum stage, GLenum portion, GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage);
	void (APIENTRY *glCombinerOutputNV)(GLenum stage, GLenum portion, GLenum abOutput, GLenum cdOutput, GLenum sumOutput, GLenum scale, GLenum bias, GLboolean abDotProduct, GLboolean cdDotProduct, GLboolean muxSum);
	void (APIENTRY *glFinalCombinerInputNV)(GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage);
	void (APIENTRY *glGetCombinerInputParameterfvNV)(GLenum stage, GLenum portion, GLenum variable, GLenum pname, GLfloat *params);
	void (APIENTRY *glGetCombinerInputParameterivNV)(GLenum stage, GLenum portion, GLenum variable, GLenum pname, GLint *params);
	void (APIENTRY *glGetCombinerOutputParameterfvNV)(GLenum stage, GLenum portion, GLenum pname, GLfloat *params); 
	void (APIENTRY *glGetCombinerOutputParameterivNV)(GLenum stage, GLenum portion, GLenum pname, GLint *params);
	void (APIENTRY *glGetFinalCombinerInputParameterfvNV)(GLenum variable, GLenum pname, GLfloat *params);
	void (APIENTRY *glGetFinalCombinerInputParameterivNV)(GLenum variable, GLenum pname, GLint *params);

	// NV_register_combiners2
	void (APIENTRY *glCombinerStageParameterfvNV)(GLenum stage, GLenum pname, const GLfloat *params);

	// ATI_fragment_shader
    GLuint (APIENTRY *glGenFragmentShadersATI)(GLuint range);
    void (APIENTRY *glBindFragmentShaderATI)(GLuint id);
    void (APIENTRY *glDeleteFragmentShaderATI)(GLuint id);
    void (APIENTRY *glBeginFragmentShaderATI)();
    void (APIENTRY *glEndFragmentShaderATI)();
    void (APIENTRY *glPassTexCoordATI)(GLuint dst, GLuint coord, GLenum swizzle);
    void (APIENTRY *glSampleMapATI)(GLuint dst, GLuint interp, GLenum swizzle);
    void (APIENTRY *glColorFragmentOp1ATI)(GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod);
    void (APIENTRY *glColorFragmentOp2ATI)(GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep, GLuint arg2Mod);
    void (APIENTRY *glColorFragmentOp3ATI)(GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep, GLuint arg2Mod, GLuint arg3, GLuint arg3Rep, GLuint arg3Mod);
    void (APIENTRY *glAlphaFragmentOp1ATI)(GLenum op, GLuint dst, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod);
    void (APIENTRY *glAlphaFragmentOp2ATI)(GLenum op, GLuint dst, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep, GLuint arg2Mod);
    void (APIENTRY *glAlphaFragmentOp3ATI)(GLenum op, GLuint dst, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep, GLuint arg2Mod, GLuint arg3, GLuint arg3Rep, GLuint arg3Mod);
    void (APIENTRY *glSetFragmentShaderConstantATI)(GLuint dst, const float *value);

	// NV_occlusion_query
	void (APIENTRY *glGenOcclusionQueriesNV)(GLsizei n, GLuint *ids);
	void (APIENTRY *glDeleteOcclusionQueriesNV)(GLsizei n, const GLuint *ids);
	GLboolean (APIENTRY *glIsOcclusionQueryNV)(GLuint id);
	void (APIENTRY *glBeginOcclusionQueryNV)(GLuint id);
	void (APIENTRY *glEndOcclusionQueryNV)();
	void (APIENTRY *glGetOcclusionQueryivNV)(GLuint id, GLenum pname, GLint *params);
	void (APIENTRY *glGetOcclusionQueryuivNV)(GLuint id, GLenum pname, GLuint *params);

	// WGL_ARB_extensions_string
	const char *(APIENTRY *wglGetExtensionsStringARB)(HDC hdc);

	// WGL_EXT_extensions_string
	const char *(APIENTRY *wglGetExtensionsStringEXT)();

	// WGL_ARB_make_current_read
	void (APIENTRY *wglMakeContextCurrentARB)(HDC hDrawDC, HDC hReadDC, HGLRC hglrc);
	HDC (APIENTRY *wglGetCurrentReadDCARB)();

	// WGL_EXT_swap_control
	BOOL (APIENTRY *wglSwapIntervalEXT)(int interval);
	int (APIENTRY *wglGetSwapIntervalEXT)();
};

struct VDOpenGLTechnique {
	const void *mpFragmentShader;
	uint8 mFragmentShaderMode;
};

struct VDOpenGLNVRegisterCombinerConfig {
	uint8 mConstantCount;
	uint8 mGeneralCombinerCount;
	const float (*mpConstants)[4];
	const uint8 *mpByteCode;
};

struct VDOpenGLATIFragmentShaderConfig {
	uint8 mConstantCount;
	const float (*mpConstants)[4];
	const uint8 *mpByteCode;
};

class VDOpenGLBinding : public VDAPITableWGL, public VDAPITableOpenGL, public VDAPITableOpenGLEXT {
	VDOpenGLBinding(const VDOpenGLBinding&);
	VDOpenGLBinding& operator=(const VDOpenGLBinding&);
public:
	VDOpenGLBinding();
	~VDOpenGLBinding();

	bool IsInited() const { return mhglrc != NULL; }

	bool Init();
	void Shutdown();

	bool Attach(HDC hdc, int minColorBits, int minAlphaBits, bool minDepthBits, bool minStencilBits, bool doubleBuffer);
	void Detach();

	bool AttachAux(HDC hdc, int minColorBits, int minAlphaBits, bool minDepthBits, bool minStencilBits, bool doubleBuffer);

	bool Begin(HDC hdc);
	void End();

	GLenum InitTechniques(const VDOpenGLTechnique *techniques, int techniqueCount);
	void DisableFragmentShaders();

public:
	// extension flags
	bool ARB_multitexture;
	bool NV_register_combiners;
	bool NV_register_combiners2;
	bool NV_occlusion_query;
	bool EXT_pixel_buffer_object;
	bool ARB_pixel_buffer_object;
	bool ATI_fragment_shader;
	bool EXT_texture_env_combine;

	// WGL extension flags
	bool ARB_make_current_read;
	bool EXT_swap_control;

protected:
	HMODULE mhmodOGL;
	HDC		mhdc;
	HGLRC	mhglrc;
	int		mPixelFormat;
};

enum VDOpenGLFragmentShaderMode {
	kVDOpenGLFragmentShaderModeNVRC,	// NV_register_combiners
	kVDOpenGLFragmentShaderModeNVRC2,	// NV_register_combiners2
	kVDOpenGLFragmentShaderModeATIFS,	// ATI_fragment_shader
	kVDOpenGLFragmentShaderModeCount
};

#endif
