#include "opengl_utils.h"

#include <wingdi.h>
#include "common_logger.h"

namespace
{
	HMODULE g_libOpenGL = NULL;

	struct GLVersionInternal
	{
		int major;
		int minor;
	};

	struct GLVersionInternal GLVersion;

	typedef void* (WINAPI *PF_WGLGETPROCADDRESS)(const char*);
	PF_WGLGETPROCADDRESS pf_wglGetProcAddress;

	bool g_gl_version_1_0 = false;
	bool g_gl_version_1_1 = false;
	bool g_gl_version_1_2 = false;
	bool g_gl_version_1_3 = false;
	bool g_gl_version_1_4 = false;
	bool g_gl_version_1_5 = false;
	bool g_gl_version_2_0 = false;
	bool g_gl_version_2_1 = false;
	bool g_gl_shader_supported = false;
	bool g_gl_arb_shader_supported = false;
	bool g_gl_arb_texture_rectangle_supported = false;

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
		"     vec3 yuv;"
		"     vec3 rgb;"
		"     yuv.x = texture2D(tex_y, textureOut).r;"
		"     yuv.y = texture2D(tex_u, textureOut).r - 0.5;"
		"     yuv.z = texture2D(tex_v, textureOut).r - 0.5;"
		"     rgb = mat3(1, 1, 1,"
		"	             0, -0.39465, 2.03211,"
		"	            1.13983, -0.58060, 0) * yuv;"
		"     gl_FragColor = vec4(rgb, 1);"
		" }";

	const GLfloat g_vertex_vertices[] =
	{
		-1.0f, -1.0f,
		1.0f, -1.0f,
		-1.0f,  1.0f,
		1.0f,  1.0f,
	};

	const GLfloat g_texture_vertices[] = 
	{
		0.0f,  1.0f,
		1.0f,  1.0f,
		0.0f,  0.0f,
		1.0f,  0.0f,
	};
	
	const GLuint g_attrib_vertex = 3;
	const GLuint g_attrib_texture = 4;
}

/*OpenGL functions*/
PF_GLGETERROR pf_glGetError;
PF_GLGETSTRING pf_glGetString;
PF_GLMATRIXMODE pf_glMatrixMode;
PF_GLLOADIDENTITY pf_glLoadIdentity;
PF_GLCLEAR pf_glClear;
PF_GLBEGIN pf_glBegin;
PF_GLEND pf_glEnd;
PF_GLFLUSH pf_glFlush;
PF_GLVIEWPORT pf_glViewport;
PF_GLENABLE pf_glEnable;
PF_GLDISABLE pf_glDisable;
PF_GLCLEARCOLOR pf_glClearColor;
PF_GLCOLOR4F pf_glColor4f;
PF_GLVERTEX2F pf_glVertex2f;
PF_GLVERTEX3F pf_glVertex3f;
PF_GLTEXIMAGE2D pf_glTexImage2D;
PF_GLTEXPARAMETERI pf_glTexParameteri;
PF_GLGENTEXTURES pf_glGenTextures;
PF_GLBINDTEXTURE pf_glBindTexture;
PF_GLDRAWARRAYS pf_glDrawArrays;
PF_GLACTIVETEXTURE pf_glActiveTexture;
PF_GLUNIFORM1I pf_glUniform1i;
PF_GLCREATESHADER pf_glCreateShader;
PF_GLSHADERSOURCE pf_glShaderSource;
PF_GLCOMPILESHADER pf_glCompileShader;
PF_GLGETSHADERIV pf_glGetShaderiv;
PF_GLCREATEPROGRAM pf_glCreateProgram;
PF_GLATTACHSHADER pf_glAttachShader;
PF_GLBINDATTRIBLOCATION pf_glBindAttribLocation;
PF_GLLINKPROGRAM pf_glLinkProgram;
PF_GLUSEPROGRAM pf_glUseProgram;
PF_GLGETUNIFORMLOCATION pf_glGetUniformLocation;
PF_GLVERTEXATTRIBPOINTER pf_glVertexAttribPointer;
PF_GLENABLEVERTEXATTRIBARRAY pf_glEnableVertexAttribArray;
PF_GLGETSHADERINFOLOG pf_glGetShaderInfoLog;
PF_GLGETPROGRAMIV pf_glGetProgramiv;
PF_GLGETPROGRAMINFOLOG pf_glGetProgramInfoLog;
PF_GLDELETEPROGRAM pf_glDeleteProgram;
PF_GLDELETESHADER pf_glDeleteShader;

/*OpenGL ARB functions*/
PF_GLATTACHOBJECTARB pf_glAttachObjectARB;
PF_GLCOMPILESHADERARB pf_glCompileShaderARB;
PF_GLCREATEPROGRAMOBJECTARB pf_glCreateProgramObjectARB;
PF_GLCREATESHADEROBJECTARB pf_glCreateShaderObjectARB;
PF_GLDELETEOBJECTARB pf_glDeleteObjectARB;
PF_GLGETINFOLOGARB pf_glGetInfoLogARB;
PF_GLGETOBJECTPARAMETERIVARB pf_glGetObjectParameterivARB;
PF_GLGETUNIFORMLOCATIONARB pf_glGetUniformLocationARB;
PF_GLLINKPROGRAMARB pf_glLinkProgramARB;
PF_GLSHADERSOURCEARB pf_glShaderSourceARB;
PF_GLUNIFORM1IARB pf_glUniform1iARB;
PF_GLUNIFORM1FARB pf_glUniform1fARB;
PF_GLUSEPROGRAMOBJECTARB pf_glUseProgramObjectARB;

/*OpenGL window functions*/
PF_WGLCREATECONTEXT pf_wglCreateContext;
PF_WGLMAKECURRENT pf_wglMakeCurrent;
PF_WGLDELETECONTEXT pf_wglDeleteContext;
PF_WGLGETCURRENTCONTEXT pf_wglGetCurrentContext;
PF_WGLSHARELISTS pf_wglShareLists;

static void* get_proc_addr_internal(const char *name)
{
	void* ret = NULL;
	if (g_libOpenGL == NULL)
	{
		return NULL;
	}
	if (pf_wglGetProcAddress != NULL)
	{
		ret = pf_wglGetProcAddress(name);
	}
	if (ret == NULL)
	{
		ret = (void*)GetProcAddress(g_libOpenGL, name);
	}

	return ret;
}

static bool is_opengl_ext_supported(const char* ext)
{
	const char *extensions;
	const char *loc;
	const char *terminator;
	extensions = (const char *)glGetString(GL_EXTENSIONS);
	if (extensions == NULL || ext == NULL)
	{
		return false;
	}

	while (1)
	{
		loc = strstr(extensions, ext);
		if (loc == NULL)
		{
			return false;
		}

		terminator = loc + strlen(ext);
		if ((loc == extensions || *(loc - 1) == ' ') &&
			(*terminator == ' ' || *terminator == '\0'))
		{
			return true;
		}
		extensions = terminator;
	}

	return false;
}

static void get_opengl_version()
{
	const GLubyte *vendor = (const GLubyte *)glGetString(GL_VENDOR);
	const GLubyte *version = (const GLubyte *)glGetString(GL_VERSION);
	int major = version[0] - '0';
	int minor = version[2] - '0';
	GLVersion.major = major;
	GLVersion.minor = minor;

	LOG_DEBUG("OpenGL: vendor[%s], version[%s]", vendor, version);
	g_gl_version_1_0 = (major == 1 && minor >= 0) || major > 1;
	g_gl_version_1_1 = (major == 1 && minor >= 1) || major > 1;
	g_gl_version_1_2 = (major == 1 && minor >= 2) || major > 1;
	g_gl_version_1_3 = (major == 1 && minor >= 3) || major > 1;
	g_gl_version_1_4 = (major == 1 && minor >= 4) || major > 1;
	g_gl_version_1_5 = (major == 1 && minor >= 5) || major > 1;
	g_gl_version_2_0 = (major == 2 && minor >= 0) || major > 2;
	g_gl_version_2_1 = (major == 2 && minor >= 1) || major > 2;
}

static void load_gl_function_1_0()
{
	if (!g_gl_version_1_0)
	{
		return;
	}

	pf_glMatrixMode = (PF_GLMATRIXMODE)get_proc_addr_internal("glMatrixMode");
	pf_glLoadIdentity = (PF_GLLOADIDENTITY)get_proc_addr_internal("glLoadIdentity");
	pf_glClear = (PF_GLCLEAR)get_proc_addr_internal("glClear");
	pf_glBegin = (PF_GLBEGIN)get_proc_addr_internal("glBegin");
	pf_glEnd = (PF_GLEND)get_proc_addr_internal("glEnd");
	pf_glFlush = (PF_GLFLUSH)get_proc_addr_internal("glFlush");
	pf_glDisable = (PF_GLDISABLE)get_proc_addr_internal("glDisable");
	pf_glEnable = (PF_GLENABLE)get_proc_addr_internal("glEnable");
	pf_glClearColor = (PF_GLCLEARCOLOR)get_proc_addr_internal("glClearColor");
	pf_glColor4f = (PF_GLCOLOR4F)get_proc_addr_internal("glColor4f");
	pf_glVertex2f = (PF_GLVERTEX2F)get_proc_addr_internal("glVertex2f");
	pf_glVertex3f = (PF_GLVERTEX3F)get_proc_addr_internal("glVertex3f");
	pf_glTexParameteri = (PF_GLTEXPARAMETERI)get_proc_addr_internal("glTexParameteri");
	pf_glTexImage2D = (PF_GLTEXIMAGE2D)get_proc_addr_internal("glTexImage2D");
	pf_glGetError = (PF_GLGETERROR)get_proc_addr_internal("glGetError");
	pf_glViewport = (PF_GLVIEWPORT)get_proc_addr_internal("glViewport");
}

static void load_gl_function_1_1()
{
	if (!g_gl_version_1_1)
	{
		return;
	}

	pf_glGenTextures = (PF_GLGENTEXTURES)get_proc_addr_internal("glGenTextures");
	pf_glBindTexture = (PF_GLBINDTEXTURE)get_proc_addr_internal("glBindTexture");
	pf_glDrawArrays = (PF_GLDRAWARRAYS)get_proc_addr_internal("glDrawArrays");
}

static void load_gl_function_1_3()
{
	if (!g_gl_version_1_3)
	{
		return;
	}

	pf_glActiveTexture = (PF_GLACTIVETEXTURE)get_proc_addr_internal("glActiveTexture");
}

static void load_gl_function_2_0()
{
	if (!g_gl_version_2_0)
	{
		return;
	}

	pf_glCreateShader = (PF_GLCREATESHADER)get_proc_addr_internal("glCreateShader");
	pf_glCreateProgram = (PF_GLCREATEPROGRAM)get_proc_addr_internal("glCreateProgram");
	pf_glCompileShader = (PF_GLCOMPILESHADER)get_proc_addr_internal("glCompileShader");
	pf_glLinkProgram = (PF_GLLINKPROGRAM)get_proc_addr_internal("glLinkProgram");
	pf_glShaderSource = (PF_GLSHADERSOURCE)get_proc_addr_internal("glShaderSource");
	pf_glUseProgram = (PF_GLUSEPROGRAM)get_proc_addr_internal("glUseProgram");
	pf_glGetShaderiv = (PF_GLGETSHADERIV)get_proc_addr_internal("glGetShaderiv");
	pf_glAttachShader = (PF_GLATTACHSHADER)get_proc_addr_internal("glAttachShader");
	pf_glBindAttribLocation = (PF_GLBINDATTRIBLOCATION)get_proc_addr_internal("glBindAttribLocation");
	pf_glGetUniformLocation = (PF_GLGETUNIFORMLOCATION)get_proc_addr_internal("glGetUniformLocation");
	pf_glVertexAttribPointer = (PF_GLVERTEXATTRIBPOINTER)get_proc_addr_internal("glVertexAttribPointer");
	pf_glEnableVertexAttribArray = (PF_GLENABLEVERTEXATTRIBARRAY)get_proc_addr_internal("glEnableVertexAttribArray");
	pf_glUniform1i = (PF_GLUNIFORM1I)get_proc_addr_internal("glUniform1i");
	pf_glGetShaderInfoLog = (PF_GLGETSHADERINFOLOG)get_proc_addr_internal("glGetShaderInfoLog");
	pf_glGetProgramiv = (PF_GLGETPROGRAMIV)get_proc_addr_internal("glGetProgramiv");
	pf_glGetProgramInfoLog = (PF_GLGETPROGRAMINFOLOG)get_proc_addr_internal("glGetProgramInfoLog");
	pf_glDeleteProgram = (PF_GLDELETEPROGRAM)get_proc_addr_internal("glDeleteProgram");
	pf_glDeleteShader = (PF_GLDELETESHADER)get_proc_addr_internal("glDeleteShader");
}

static void load_gl_arb_shaders()
{
	if (!is_opengl_ext_supported("GL_ARB_texture_non_power_of_two") &&
		(is_opengl_ext_supported("GL_ARB_texture_rectangle") ||
			is_opengl_ext_supported("GL_EXT_texture_rectangle")))
	{
		g_gl_arb_texture_rectangle_supported = true;
	}
	else
	{
		g_gl_arb_texture_rectangle_supported = false;
	}

	if (is_opengl_ext_supported("GL_ARB_shader_objects") &&
		is_opengl_ext_supported("GL_ARB_shading_language_100") &&
		is_opengl_ext_supported("GL_ARB_vertex_shader") &&
		is_opengl_ext_supported("GL_ARB_fragment_shader"))
	{
		pf_glAttachObjectARB = (PF_GLATTACHOBJECTARB)get_proc_addr_internal("glAttachObjectARB");
		pf_glCompileShaderARB = (PF_GLCOMPILESHADERARB)get_proc_addr_internal("glCompileShaderARB");
		pf_glCreateProgramObjectARB = (PF_GLCREATEPROGRAMOBJECTARB)get_proc_addr_internal("glCreateProgramObjectARB");
		pf_glCreateShaderObjectARB = (PF_GLCREATESHADEROBJECTARB)get_proc_addr_internal("glCreateShaderObjectARB");
		pf_glDeleteObjectARB = (PF_GLDELETEOBJECTARB)get_proc_addr_internal("glDeleteObjectARB");
		pf_glGetInfoLogARB = (PF_GLGETINFOLOGARB)get_proc_addr_internal("glGetInfoLogARB");
		pf_glGetObjectParameterivARB = (PF_GLGETOBJECTPARAMETERIVARB)get_proc_addr_internal("glGetObjectParameterivARB");
		pf_glGetUniformLocationARB = (PF_GLGETUNIFORMLOCATIONARB)get_proc_addr_internal("glGetUniformLocationARB");
		pf_glLinkProgramARB = (PF_GLLINKPROGRAMARB)get_proc_addr_internal("glLinkProgramARB");
		pf_glShaderSourceARB = (PF_GLSHADERSOURCEARB)get_proc_addr_internal("glShaderSourceARB");
		pf_glUniform1iARB = (PF_GLUNIFORM1IARB)get_proc_addr_internal("glUniform1iARB");
		pf_glUniform1fARB = (PF_GLUNIFORM1FARB)get_proc_addr_internal("glUniform1fARB");
		pf_glUseProgramObjectARB = (PF_GLUSEPROGRAMOBJECTARB)get_proc_addr_internal("glUseProgramObjectARB");
	}

}

static bool is_gl_shader_supported()
{
	g_gl_shader_supported = false;
	if (pf_glCreateShader && 
		pf_glCreateProgram  &&
		pf_glCompileShader &&
		pf_glLinkProgram &&
		pf_glShaderSource &&
		pf_glUseProgram &&
		pf_glGetShaderiv &&
		pf_glAttachShader &&
		pf_glBindAttribLocation &&
		pf_glGetUniformLocation &&
		pf_glVertexAttribPointer &&
		pf_glEnableVertexAttribArray &&
		pf_glUniform1i &&
		pf_glGetShaderInfoLog &&
		pf_glGetProgramiv &&
		pf_glGetProgramInfoLog &&
		pf_glDeleteProgram &&
		pf_glDeleteShader)
	{
		g_gl_shader_supported = true;
	}
	
	return g_gl_shader_supported;
}

static bool is_gl_arb_shader_supported()
{
	g_gl_arb_shader_supported = false;

	if (pf_glAttachObjectARB &&
		pf_glCompileShaderARB &&
		pf_glCreateProgramObjectARB &&
		pf_glCreateShaderObjectARB &&
		pf_glDeleteObjectARB &&
		pf_glGetInfoLogARB &&
		pf_glGetObjectParameterivARB &&
		pf_glGetUniformLocationARB &&
		pf_glLinkProgramARB &&
		pf_glShaderSourceARB &&
		pf_glUniform1iARB &&
		pf_glUniform1fARB &&
		pf_glUseProgramObjectARB)
	{
		g_gl_arb_shader_supported = true;
	}

	return g_gl_arb_shader_supported;
}

static bool load_opengl_functions()
{
	GLVersion.major = 0;
	GLVersion.minor = 0;

	pf_glGetString = (PF_GLGETSTRING)get_proc_addr_internal("glGetString");
	if (pf_glGetString == NULL)
	{
		return false;
	}

	get_opengl_version();
	load_gl_function_1_0();
	load_gl_function_1_1();
	load_gl_function_1_3();
	load_gl_function_2_0();
	load_gl_arb_shaders();

	return true;
}

static bool load_wgl_functions()
{
	pf_wglCreateContext = (PF_WGLCREATECONTEXT)get_proc_addr_internal("wglCreateContext");
	pf_wglMakeCurrent = (PF_WGLMAKECURRENT)get_proc_addr_internal("wglMakeCurrent");
	pf_wglDeleteContext = (PF_WGLDELETECONTEXT)get_proc_addr_internal("wglDeleteContext");
	pf_wglGetCurrentContext = (PF_WGLGETCURRENTCONTEXT)get_proc_addr_internal("wglGetCurrentContext");
	pf_wglShareLists = (PF_WGLSHARELISTS)get_proc_addr_internal("wglShareLists");

	if (pf_wglCreateContext &&
		pf_wglMakeCurrent &&
		pf_wglDeleteContext &&
		pf_wglGetCurrentContext &&
		pf_wglShareLists)
	{
		return true;
	}

	return false;
}


static bool compile_shader(GLuint shader, const char *source)
{
	GLint status;
	const char *sources[1];

	sources[0] = source;

	glShaderSource(shader, 1, sources, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE)
	{
		GLint logLength = 0;

		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
		char* info = new char[logLength + 1];
		if (info)
		{
			memset(info, 0, logLength + 1);
			glGetShaderInfoLog(shader, logLength + 1, NULL, info);

			LOG_ERROR("glCompileShader error.[%s]", info);
			delete[] info;
		}

		return false;
	}
	else 
	{
		return true;
	}
}

static bool init_opengl_shaders(struct GLWindowContext* ctx)
{
	// create the vertex shader
	ctx->vertShader = glCreateShader(GL_VERTEX_SHADER);
	if (ctx->vertShader == 0)
	{
		return false;
	}
	
	if (!compile_shader(ctx->vertShader, g_vert_shader_source))
	{
		return false;
	}
	
	// create the fragment shader
	ctx->fragShader = glCreateShader(GL_FRAGMENT_SHADER);
	if (ctx->fragShader == 0)
	{
		return false;
	}

	if (!compile_shader(ctx->fragShader, g_frag_shader_source))
	{
		return false;
	}

	// create one program object
	ctx->program = glCreateProgram();
	if (ctx->program == 0)
	{
		return false;
	}

	glAttachShader(ctx->program, ctx->vertShader);
	glAttachShader(ctx->program, ctx->fragShader);

	glBindAttribLocation(ctx->program, g_attrib_vertex, "vertexIn");
	glBindAttribLocation(ctx->program, g_attrib_texture, "textureIn");

	glLinkProgram(ctx->program);
	
	GLint linkStatus;
	glGetProgramiv(ctx->program, GL_LINK_STATUS, &linkStatus);
	if (linkStatus != GL_TRUE)
	{
		GLint logLength = 0;
		glGetProgramiv(ctx->program, GL_INFO_LOG_LENGTH, &logLength);
		char* info = new char[logLength + 1];
		if (info)
		{
			memset(info, 0, logLength + 1);
			glGetProgramInfoLog(ctx->program, logLength + 1, NULL, info);

			LOG_ERROR("glLinkProgram error.[%s]", info);
			delete[] info;
		}

		return false;
	}

	glUseProgram(ctx->program);

	ctx->yTexPos = glGetUniformLocation(ctx->program, "tex_y");
	ctx->uTexPos = glGetUniformLocation(ctx->program, "tex_u");
	ctx->vTexPos = glGetUniformLocation(ctx->program, "tex_v");

	glVertexAttribPointer(g_attrib_vertex, 2, GL_FLOAT, GL_FALSE, 0, g_vertex_vertices);
	glEnableVertexAttribArray(g_attrib_vertex);

	glVertexAttribPointer(g_attrib_texture, 2, GL_FLOAT, GL_FALSE, 0, g_texture_vertices);
	glEnableVertexAttribArray(g_attrib_texture);

	glGenTextures(1, &ctx->yTexId);
	glBindTexture(GL_TEXTURE_2D, ctx->yTexId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenTextures(1, &ctx->uTexId);
	glBindTexture(GL_TEXTURE_2D, ctx->uTexId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenTextures(1, &ctx->vTexId);
	glBindTexture(GL_TEXTURE_2D, ctx->vTexId);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	return true;
}

bool create_window_opengl(struct GLWindowContext* ctx)
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

	if (!load_wgl_functions())
	{
		return false;
	}

	PIXELFORMATDESCRIPTOR pixelDesc;
	memset(&pixelDesc, 0, sizeof(PIXELFORMATDESCRIPTOR));

	pixelDesc.nSize = sizeof(pixelDesc);
	pixelDesc.nVersion = 1;
	pixelDesc.iPixelType = PFD_TYPE_RGBA;
	pixelDesc.cColorBits = 24;
	pixelDesc.cDepthBits = 0;
	pixelDesc.cStencilBits = 0;
	pixelDesc.iLayerType = PFD_MAIN_PLANE;
	pixelDesc.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;

	int formatIndex = ChoosePixelFormat(ctx->hdc, &pixelDesc);
	if (formatIndex == 0)
	{
		if (!DescribePixelFormat(ctx->hdc, 0, sizeof(pixelDesc), &pixelDesc))
		{
			return false;
		}
	}

	if (!SetPixelFormat(ctx->hdc, formatIndex, &pixelDesc))
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

	if (!load_opengl_functions())
	{
		return false;
	}

	if (!is_gl_shader_supported())
	{
		return false;
	}

	if (!init_opengl_shaders(ctx))
	{
		return false;
	}

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_SCISSOR_TEST);
	glEnable(GL_TEXTURE_2D);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	MY_wglMakeCurrent(NULL, NULL);
	ctx->currentThreadID = GetCurrentThreadId();

	return true;
}

void destroy_window_opengl(struct GLWindowContext* ctx)
{
	DWORD currThreadID = GetCurrentThreadId();
	if (currThreadID != ctx->currentThreadID)
	{
		MY_wglMakeCurrent(NULL, NULL);
		ctx->currentThreadID = currThreadID;
		MY_wglMakeCurrent(ctx->hdc, ctx->hrc);
	}

	glDeleteShader(ctx->vertShader);
	glDeleteShader(ctx->fragShader);
	glDeleteProgram(ctx->program);

	if (MY_wglGetCurrentContext() != NULL)
	{
		MY_wglMakeCurrent(NULL, NULL);
	}

	MY_wglDeleteContext(ctx->hrc);

	if (g_libOpenGL != NULL)
	{
		FreeLibrary(g_libOpenGL);
		g_libOpenGL = NULL;
	}

	ReleaseDC(ctx->hwnd, ctx->hdc);
}

void flush_window_opengl(struct GLWindowContext* ctx)
{
	glUseProgram(0);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	SwapBuffers(ctx->hdc);

	MY_wglMakeCurrent(NULL, NULL);
}

bool render_yuv420p_by_opengl(GLWindowContext* ctx, int width, int height, 
	const void* y, int ystride, const void* u, int ustride, const void* v, int vstride)
{
	DWORD currThreadID = GetCurrentThreadId();
	if (currThreadID != ctx->currentThreadID)
	{
		ctx->currentThreadID = currThreadID;
		MY_wglMakeCurrent(ctx->hdc, ctx->hrc);
		glUseProgram(ctx->program);
	}

	//Y
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ctx->yTexId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, y);
	glUniform1i(ctx->yTexPos, 0);
	//U
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, ctx->uTexId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, u);
	glUniform1i(ctx->uTexPos, 1);
	//V
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, ctx->vTexId);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, v);
	glUniform1i(ctx->vTexPos, 2);

	//render
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	if (!SwapBuffers(ctx->hdc))
	{
		return false;
	}

	return true;
}