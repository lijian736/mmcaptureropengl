#ifndef _H_OPENGL_RENDER_H_
#define _H_OPENGL_RENDER_H_

#include <windows.h>
#include "opengl_utils.h"

GLint textureUniformY;
GLint textureUniformU;
GLint textureUniformV;

GLuint id_y;
GLuint id_u;
GLuint id_v;

const char* g_vert_shader_source = "attribute vec4 vertexIn;"
" attribute vec2 textureIn;"
" varying vec2 textureOut;"
" void main(void)"
" {"
"	gl_Position = vertexIn;"
"	textureOut = textureIn;"
" }";

const char* g_frag_shader_source = "varying vec2 textureOut;"
" uniform sampler2D tex_y;"
" uniform sampler2D tex_u;"
" uniform sampler2D tex_v;"
" void main(void)"
" {"
" vec3 yuv;"
" vec3 rgb;"
" yuv.x = texture2D(tex_y, textureOut).r;"
" yuv.y = texture2D(tex_u, textureOut).r - 0.5;"
" yuv.z = texture2D(tex_v, textureOut).r - 0.5;"
" rgb = mat3(1, 1, 1,"
"	0, -0.39465, 2.03211,"
"	1.13983, -0.58060, 0) * yuv;"
" gl_FragColor = vec4(rgb, 1);"
" }";

static bool compile_shader(GLhandleARB shader, const char *defines, const char *source)
{
	GLint status;
	const char *sources[2];

	sources[0] = defines;
	sources[1] = source;

	glShaderSourceARB(shader, 2, sources, NULL);
	glCompileShaderARB(shader);
	glGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &status);

	if (status == 0)
	{
		GLint length;
		char *info;

		glGetObjectParameterivARB(shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length);
		info = new char[length + 1];
		if (info)
		{
			memset(info, 0, length + 1);
			glGetInfoLogARB(shader, length, NULL, info);
			LOG_ERROR("glCompileShader error.[%s]", info);
			delete[] info;
		}

		return false;
	}
	else {
		return true;
	}
}

static bool init_shaders(struct GLWindowContext* ctx)
{
	GLint location;

	const char *vert_defines = "";
	const char *frag_defines = "";
	if (g_gl_arb_texture_rectangle_supported)
	{
		frag_defines =
			"#define sampler2D sampler2DRect\n"
			"#define texture2D texture2DRect\n"
			"#define UVCoordScale 0.5\n";
	}
	else
	{
		frag_defines =
			"#define UVCoordScale 1.0\n";
	}

	// create one program object
	ctx->program = glCreateProgramObjectARB();

	// create the vertex shader
	ctx->vertShader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);
	if (!compile_shader(ctx->vertShader, vert_defines, g_vert_shader_source))
	{
		return false;
	}

	// create the fragment shader
	ctx->fragShader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
	if (!compile_shader(ctx->fragShader, frag_defines, g_frag_shader_source))
	{
		return false;
	}

	glAttachObjectARB(ctx->program, ctx->vertShader);
	glAttachObjectARB(ctx->program, ctx->fragShader);
	glLinkProgramARB(ctx->program);

	glUseProgramObjectARB(ctx->program);

	const char* texArray[4] = { "tex0", "tex1", "tex2", "tex3" };
	for (int i = 0; i < 4; i++)
	{
		location = glGetUniformLocationARB(ctx->program, texArray[i]);
		if (location >= 0)
		{
			glUniform1iARB(location, i);
		}
	}
	glUseProgramObjectARB(0);

	return (glGetError() == GL_NO_ERROR);
}

static void destroy_shader_and_program(GLWindowContext* ctx)
{
	glDeleteObjectARB(ctx->vertShader);
	glDeleteObjectARB(ctx->fragShader);
	glDeleteObjectARB(ctx->program);
}

static bool init_texture(GLWindowContext* ctx)
{
	glGenTextures(1, &ctx->yTex);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, ctx->yTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glGenTextures(1, &ctx->uTex);
	glGenTextures(1, &ctx->vTex);

	glBindTexture(GL_TEXTURE_2D, ctx->uTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	//	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMI, (texture_w + 1) / 2, (texture_h + 1) / 2, 0, format, type, NULL);

	glBindTexture(GL_TEXTURE_2D, ctx->vTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	//	glTexImage2D(textype, 0, internalFormat, (texture_w + 1) / 2, (texture_h + 1) / 2, 0, format, type, NULL);

	return (glGetError() == GL_NO_ERROR);
}

struct GLWindowContext
{
	HWND hwnd;
	HDC hdc;
	HGLRC hrc;

	GLhandleARB program;
	GLhandleARB vertShader;
	GLhandleARB fragShader;

	GLuint yTex;
	GLuint uTex;
	GLuint vTex;
};

/**
* @brief initialize the window opengl
*/
bool init_window_opengl(GLWindowContext* ctx);

/**
* @brief render yuv420p by opengl
* @params ctx -- the GLWindowContext pointer
*         width -- the image width
*         height -- the image height
*         y -- the y data
*         u -- the u data
*         v -- the v data
*/
bool render_yuv420p_by_opengl(GLWindowContext* ctx, int width, int height, const void* y, const void* u, const void* v);

/**
* @brief close the window opengl
*/
bool close_window_opengl(GLWindowContext* ctx);

bool close_window_opengl(GLWindowContext* ctx)
{
	if (MY_wglGetCurrentContext() != NULL)
	{
		MY_wglMakeCurrent(NULL, NULL);
	}

	if (ctx->hrc != NULL)
	{
		MY_wglDeleteContext(ctx->hrc);
		ctx->hrc = NULL;
	}

	if (g_libOpenGL != NULL)
	{
		FreeLibrary(g_libOpenGL);
		g_libOpenGL = NULL;
	}

	return true;
}

bool init_window_opengl()
{
	g_libOpenGL = LoadLibraryA("opengl32.dll");
	if (g_libOpenGL != NULL)
	{
		pf_wglGetProcAddress = (PF_WGLGETPROCADDRESS)GetProcAddress(
			g_libOpenGL, "wglGetProcAddress");
		if (pf_wglGetProcAddress == NULL)
		{
			return false;
		}
	}

	load_wgl_functions();

	PIXELFORMATDESCRIPTOR pixelDesc;
	memset(&pixelDesc, 0, sizeof(PIXELFORMATDESCRIPTOR));

	pixelDesc.nSize = sizeof(pixelDesc);
	pixelDesc.nVersion = 1;
	pixelDesc.iPixelType = PFD_TYPE_RGBA;
	pixelDesc.cColorBits = 24;
	pixelDesc.cDepthBits = 32;
	pixelDesc.cStencilBits = 0;
	pixelDesc.iLayerType = PFD_MAIN_PLANE;
	pixelDesc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;

	int format_index = ChoosePixelFormat(ctx->hdc, &pixelDesc);
	if (format_index == 0)
	{
		if (!DescribePixelFormat(ctx->hdc, 0, sizeof(pixelDesc), &pixelDesc))
		{
			return false;
		}
	}

	if (!SetPixelFormat(ctx->hdc, format_index, &pixelDesc))
	{
		return false;
	}

	ctx->hrc = MY_wglCreateContext(ctx->hdc);
	if (!ctx->hrc)
	{
		return false;
	}

	if (!MY_wglMakeCurrent(ctx->hdc, ctx->hrc))
	{
		MY_wglDeleteContext(ctx->hrc);
		return false;
	}

	if (!init_opengl())
	{
		return false;
	}

	if (!init_shaders(ctx))
	{
		return false;
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_TEXTURE_2D);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	return true;
}

bool init_window_opengl()
{
	g_libOpenGL = LoadLibraryA("opengl32.dll");
	if (g_libOpenGL != NULL)
	{
		pf_wglGetProcAddress = (PF_WGLGETPROCADDRESS)GetProcAddress(
			g_libOpenGL, "wglGetProcAddress");
		if (pf_wglGetProcAddress == NULL)
		{
			return false;
		}
	}

	load_wgl_functions();

	PIXELFORMATDESCRIPTOR pixelDesc;
	memset(&pixelDesc, 0, sizeof(PIXELFORMATDESCRIPTOR));

	pixelDesc.nSize = sizeof(pixelDesc);
	pixelDesc.nVersion = 1;
	pixelDesc.iPixelType = PFD_TYPE_RGBA;
	pixelDesc.cColorBits = 24;
	pixelDesc.cDepthBits = 32;
	pixelDesc.cStencilBits = 0;
	pixelDesc.iLayerType = PFD_MAIN_PLANE;
	pixelDesc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;

	int format_index = ChoosePixelFormat(ctx->hdc, &pixelDesc);
	if (format_index == 0)
	{
		if (!DescribePixelFormat(ctx->hdc, 0, sizeof(pixelDesc), &pixelDesc))
		{
			return false;
		}
	}

	if (!SetPixelFormat(ctx->hdc, format_index, &pixelDesc))
	{
		return false;
	}

	ctx->hrc = MY_wglCreateContext(ctx->hdc);
	if (!ctx->hrc)
	{
		return false;
	}

	if (!MY_wglMakeCurrent(ctx->hdc, ctx->hrc))
	{
		MY_wglDeleteContext(ctx->hrc);
		return false;
	}

	if (!init_opengl())
	{
		return false;
	}

	if (!init_shaders(ctx))
	{
		return false;
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_TEXTURE_2D);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	return true;
}

bool close_window_opengl()
{
	if (g_libOpenGL != NULL)
	{
		FreeLibrary(g_libOpenGL);
		g_libOpenGL = NULL;
	}

	return true;
}

bool render_yuv420p_by_opengl(GLWindowContext* ctx, int width, int height, const void* y, const void* u, const void* v)
{
	glClearColor(0.0, 255, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	//Y
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, id_y);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, y);
	glUniform1i(textureUniformY, 0);
	//U
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, id_u);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, u);
	glUniform1i(textureUniformU, 1);
	//V
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, id_v);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, v);
	glUniform1i(textureUniformV, 2);

	//render
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	if (!SwapBuffers(ctx->hdc))
	{
		return false;
	}

	return true;
}

bool render_yuv420p_by_opengl(GLWindowContext* ctx, int width, int height, const void* y, const void* u, const void* v)
{
	glClearColor(0.0, 255, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	//Y
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, id_y);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, y);
	glUniform1i(textureUniformY, 0);
	//U
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, id_u);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, u);
	glUniform1i(textureUniformU, 1);
	//V
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, id_v);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, v);
	glUniform1i(textureUniformV, 2);

	//render
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	if (!SwapBuffers(ctx->hdc))
	{
		return false;
	}

	return true;
}


#endif