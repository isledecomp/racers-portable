#pragma once

// Minimal OpenGL ES 3 / WebGL2 loader via SDL_GL_GetProcAddress — no external dependency.
// Forked from the desktop opengl3/gl3loader.h: GLES exposes only glClearDepthf (not the
// double glClearDepth), has no GL_BGRA vertex format, and does not need the fence-sync
// entry points. The struct/macros are renamed (Es3Functions/ES3_FUNC) so this loader and
// the GL3 one can coexist in the same static library.

#include <SDL3/SDL.h>
#include <stddef.h>

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_TRIANGLES 0x0004
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_SRC_ALPHA_SATURATE 0x0308
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_CW 0x0900
#define GL_CCW 0x0901
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_TEST 0x0C11
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_TEXTURE_2D 0x0DE1
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE0 0x84C0
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_VERSION 0x1F02
#define GL_PACK_ALIGNMENT 0x0D05

#define ES3_FUNCTION_LIST                                                                                              \
	ES3_FUNC(const GLubyte*, glGetString, (GLenum name))                                                               \
	ES3_FUNC(GLenum, glGetError, (void) )                                                                              \
	ES3_FUNC(void, glGetIntegerv, (GLenum pname, GLint * data))                                                        \
	ES3_FUNC(void, glGetUniformiv, (GLuint program, GLint location, GLint * params))                                   \
	ES3_FUNC(void, glEnable, (GLenum cap))                                                                             \
	ES3_FUNC(void, glDisable, (GLenum cap))                                                                            \
	ES3_FUNC(void, glClear, (GLbitfield mask))                                                                         \
	ES3_FUNC(void, glClearColor, (GLclampf r, GLclampf g, GLclampf b, GLclampf a))                                     \
	ES3_FUNC(void, glClearDepthf, (GLclampf depth))                                                                    \
	ES3_FUNC(void, glViewport, (GLint x, GLint y, GLsizei width, GLsizei height))                                      \
	ES3_FUNC(void, glScissor, (GLint x, GLint y, GLsizei width, GLsizei height))                                       \
	ES3_FUNC(void, glDepthFunc, (GLenum func))                                                                         \
	ES3_FUNC(void, glDepthMask, (GLboolean flag))                                                                      \
	ES3_FUNC(void, glBlendFunc, (GLenum sfactor, GLenum dfactor))                                                      \
	ES3_FUNC(void, glCullFace, (GLenum mode))                                                                          \
	ES3_FUNC(void, glFrontFace, (GLenum mode))                                                                         \
	ES3_FUNC(void, glPixelStorei, (GLenum pname, GLint param))                                                         \
	ES3_FUNC(void, glReadPixels, (GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void* pixels))   \
	ES3_FUNC(void, glGenTextures, (GLsizei n, GLuint * textures))                                                      \
	ES3_FUNC(void, glDeleteTextures, (GLsizei n, const GLuint* textures))                                              \
	ES3_FUNC(void, glBindTexture, (GLenum target, GLuint texture))                                                     \
	ES3_FUNC(                                                                                                          \
		void,                                                                                                          \
		glTexImage2D,                                                                                                  \
		(GLenum target,                                                                                                \
		 GLint level,                                                                                                  \
		 GLint internalformat,                                                                                         \
		 GLsizei width,                                                                                                \
		 GLsizei height,                                                                                               \
		 GLint border,                                                                                                 \
		 GLenum format,                                                                                                \
		 GLenum type,                                                                                                  \
		 const void* pixels)                                                                                           \
	)                                                                                                                  \
	ES3_FUNC(void, glTexParameteri, (GLenum target, GLenum pname, GLint param))                                        \
	ES3_FUNC(void, glGenerateMipmap, (GLenum target))                                                                  \
	ES3_FUNC(void, glActiveTexture, (GLenum texture))                                                                  \
	ES3_FUNC(GLuint, glCreateShader, (GLenum type))                                                                    \
	ES3_FUNC(void, glShaderSource, (GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length))   \
	ES3_FUNC(void, glCompileShader, (GLuint shader))                                                                   \
	ES3_FUNC(void, glGetShaderiv, (GLuint shader, GLenum pname, GLint * params))                                       \
	ES3_FUNC(void, glGetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * infoLog))           \
	ES3_FUNC(GLuint, glCreateProgram, (void) )                                                                         \
	ES3_FUNC(void, glAttachShader, (GLuint program, GLuint shader))                                                    \
	ES3_FUNC(void, glLinkProgram, (GLuint program))                                                                    \
	ES3_FUNC(void, glGetProgramiv, (GLuint program, GLenum pname, GLint * params))                                     \
	ES3_FUNC(void, glGetProgramInfoLog, (GLuint program, GLsizei bufSize, GLsizei * length, GLchar * infoLog))         \
	ES3_FUNC(void, glUseProgram, (GLuint program))                                                                     \
	ES3_FUNC(void, glDeleteShader, (GLuint shader))                                                                    \
	ES3_FUNC(GLint, glGetUniformLocation, (GLuint program, const GLchar* name))                                        \
	ES3_FUNC(void, glUniform1i, (GLint location, GLint v0))                                                            \
	ES3_FUNC(void, glUniform1f, (GLint location, GLfloat v0))                                                          \
	ES3_FUNC(void, glUniform2f, (GLint location, GLfloat v0, GLfloat v1))                                              \
	ES3_FUNC(void, glGenVertexArrays, (GLsizei n, GLuint * arrays))                                                    \
	ES3_FUNC(void, glBindVertexArray, (GLuint array))                                                                  \
	ES3_FUNC(void, glGenBuffers, (GLsizei n, GLuint * buffers))                                                        \
	ES3_FUNC(void, glBindBuffer, (GLenum target, GLuint buffer))                                                       \
	ES3_FUNC(void, glBufferData, (GLenum target, GLsizeiptr size, const void* data, GLenum usage))                     \
	ES3_FUNC(                                                                                                          \
		void,                                                                                                          \
		glVertexAttribPointer,                                                                                         \
		(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)             \
	)                                                                                                                  \
	ES3_FUNC(void, glEnableVertexAttribArray, (GLuint index))                                                          \
	ES3_FUNC(void, glDrawElements, (GLenum mode, GLsizei count, GLenum type, const void* indices))                     \
	ES3_FUNC(void, glDrawArrays, (GLenum mode, GLint first, GLsizei count))                                            \
	ES3_FUNC(void, glGenFramebuffers, (GLsizei n, GLuint * framebuffers))                                              \
	ES3_FUNC(void, glDeleteFramebuffers, (GLsizei n, const GLuint* framebuffers))                                      \
	ES3_FUNC(void, glBindFramebuffer, (GLenum target, GLuint framebuffer))                                             \
	ES3_FUNC(                                                                                                          \
		void,                                                                                                          \
		glFramebufferTexture2D,                                                                                        \
		(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)                              \
	)                                                                                                                  \
	ES3_FUNC(void, glGenRenderbuffers, (GLsizei n, GLuint * renderbuffers))                                            \
	ES3_FUNC(void, glDeleteRenderbuffers, (GLsizei n, const GLuint* renderbuffers))                                    \
	ES3_FUNC(void, glBindRenderbuffer, (GLenum target, GLuint renderbuffer))                                           \
	ES3_FUNC(void, glRenderbufferStorage, (GLenum target, GLenum internalformat, GLsizei width, GLsizei height))       \
	ES3_FUNC(                                                                                                          \
		void,                                                                                                          \
		glFramebufferRenderbuffer,                                                                                     \
		(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)                             \
	)                                                                                                                  \
	ES3_FUNC(GLenum, glCheckFramebufferStatus, (GLenum target))                                                        \
	ES3_FUNC(                                                                                                          \
		void,                                                                                                          \
		glBlitFramebuffer,                                                                                             \
		(GLint srcX0,                                                                                                  \
		 GLint srcY0,                                                                                                  \
		 GLint srcX1,                                                                                                  \
		 GLint srcY1,                                                                                                  \
		 GLint dstX0,                                                                                                  \
		 GLint dstY0,                                                                                                  \
		 GLint dstX1,                                                                                                  \
		 GLint dstY1,                                                                                                  \
		 GLbitfield mask,                                                                                              \
		 GLenum filter)                                                                                                \
	)

#define ES3_FUNC(ret, name, args) typedef ret(SDLCALL* name##_t) args;
ES3_FUNCTION_LIST
#undef ES3_FUNC

struct Es3Functions {
#define ES3_FUNC(ret, name, args) name##_t name = nullptr;
	ES3_FUNCTION_LIST
#undef ES3_FUNC

	bool LoadAll()
	{
		bool ok = true;
#define ES3_FUNC(ret, name, args)                                                                                      \
	name = (name##_t) SDL_GL_GetProcAddress(#name);                                                                    \
	ok = ok && name != nullptr;
		ES3_FUNCTION_LIST
#undef ES3_FUNC
		return ok;
	}
};
