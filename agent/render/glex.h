// ============================================================================
//  Passive — minimal OpenGL 3.3 function loader (glex.h).
//  Windows' <GL/gl.h> only exposes GL 1.1. Everything the blur pipeline needs
//  (shaders, FBOs, VBOs, glBlendFuncSeparate, glActiveTexture) arrives as
//  function pointers via wglGetProcAddress. This header declares them and
//  macro-maps the plain names so blur.cpp reads like normal GL code.
//  Call glx::init() once per process (after the GL context is current).
// ============================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>          // GL 1.1 core (types, enums, classic functions)

#include <cstdio>
#include <cstddef>

// ---------------------------------------------------------------------------
// Types/enums beyond GL 1.1 that Windows' gl.h lacks.
// (Identical typedef redeclaration is legal C++, so these are unconditional.)
// ---------------------------------------------------------------------------
typedef char      GLchar;
typedef ptrdiff_t GLsizeiptr;

// GL 1.2+/1.4 entry points that live in opengl32.dll but are missing from
// Windows' GL 1.1 gl.h — resolved through the loader below (no import-lib
// dependency), stored in inline pointers, macro-mapped to plain names.
namespace tyr { namespace glx {
using PFN_glBlendColor = void (APIENTRY*)(GLfloat, GLfloat, GLfloat, GLfloat);
inline PFN_glBlendColor p_glBlendColor = nullptr;
using PFN_glDebugMessageControl = void (APIENTRY*)(GLenum, GLenum, GLenum, GLsizei, const GLuint*, GLboolean);
inline PFN_glDebugMessageControl p_glDebugMessageControl = nullptr;
} }

#ifndef GL_DONT_CARE
#define GL_DONT_CARE                       0x1100
#endif
#ifndef GL_DEBUG_SEVERITY_HIGH
#define GL_DEBUG_SEVERITY_HIGH             0x9146
#define GL_DEBUG_SEVERITY_MEDIUM           0x9147
#define GL_DEBUG_SEVERITY_LOW              0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION     0x826B
#define GL_DEBUG_SOURCE_API                0x814E
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM      0x814F
#define GL_DEBUG_SOURCE_SHADER_COMPILER    0x8150
#define GL_DEBUG_SOURCE_THIRD_PARTY        0x8151
#define GL_DEBUG_SOURCE_APPLICATION        0x8152
#define GL_DEBUG_SOURCE_OTHER              0x8153
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE                  0x812F
#endif
#ifndef GL_BLEND_SRC_RGB
#define GL_BLEND_SRC_RGB                  0x80C9
#define GL_BLEND_DST_RGB                  0x80C8
#define GL_BLEND_SRC_ALPHA                0x8589
#define GL_BLEND_DST_ALPHA                0x8580
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_ARRAY_BUFFER_BINDING           0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING   0x8895
#endif
#ifndef GL_VERTEX_ARRAY_BINDING
#define GL_VERTEX_ARRAY_BINDING           0x85B5
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER                    0x8D40
#define GL_FRAMEBUFFER_BINDING            0x8CA6
#define GL_READ_FRAMEBUFFER               0x8CA8
#define GL_DRAW_FRAMEBUFFER               0x8CA9
#define GL_DRAW_FRAMEBUFFER_BINDING       0x8CA6
#define GL_READ_FRAMEBUFFER_BINDING       0x8CAA
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5
#endif
#ifndef GL_NO_ERROR
#define GL_NO_ERROR                       0x0000
#endif
#ifndef GL_INVALID_ENUM
#define GL_INVALID_ENUM                   0x0500
#define GL_INVALID_VALUE                  0x0501
#define GL_INVALID_OPERATION              0x0502
#define GL_STACK_OVERFLOW                 0x0503
#define GL_STACK_UNDERFLOW                0x0504
#define GL_OUT_OF_MEMORY                  0x0505
#endif
#ifndef GL_INVALID_FRAMEBUFFER_OPERATION
#define GL_INVALID_FRAMEBUFFER_OPERATION  0x0506
#endif
#ifndef GL_BLEND_COLOR
#define GL_BLEND_COLOR                    0x8005
#endif
#ifndef GL_COLOR_WRITEMASK
#define GL_COLOR_WRITEMASK                0x0C23
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE                 0x84E0
#endif
#ifndef GL_PACK_ALIGNMENT
#define GL_PACK_ALIGNMENT                 0x0D05
#endif
#ifndef GL_UNPACK_ALIGNMENT
#define GL_UNPACK_ALIGNMENT               0x0CF5
#endif
#ifndef GL_TEXTURE_BINDING_2D
#define GL_TEXTURE_BINDING_2D             0x8069
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER                  0x8B31
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_STREAM_DRAW                    0x88E0
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0                       0x84C0
#endif
#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM                0x8B8D
#endif

namespace tyr {
namespace glx {

// ---------------------------------------------------------------------------
// Function pointer types (GL 1.2 -> 3.3 surface used by Passive).
// ---------------------------------------------------------------------------
using PFN_glActiveTexture          = void (APIENTRY*)(GLenum);
using PFN_glBlendFuncSeparate      = void (APIENTRY*)(GLenum, GLenum, GLenum, GLenum);
using PFN_glCreateShader           = GLuint (APIENTRY*)(GLenum);
using PFN_glShaderSource           = void (APIENTRY*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
using PFN_glCompileShader          = void (APIENTRY*)(GLuint);
using PFN_glGetShaderiv            = void (APIENTRY*)(GLuint, GLenum, GLint*);
using PFN_glGetShaderInfoLog       = void (APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
using PFN_glDeleteShader           = void (APIENTRY*)(GLuint);
using PFN_glCreateProgram          = GLuint (APIENTRY*)();
using PFN_glAttachShader           = void (APIENTRY*)(GLuint, GLuint);
using PFN_glLinkProgram            = void (APIENTRY*)(GLuint);
using PFN_glGetProgramiv           = void (APIENTRY*)(GLuint, GLenum, GLint*);
using PFN_glGetProgramInfoLog      = void (APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
using PFN_glDeleteProgram          = void (APIENTRY*)(GLuint);
using PFN_glUseProgram             = void (APIENTRY*)(GLuint);
using PFN_glGetUniformLocation    = GLint (APIENTRY*)(GLuint, const GLchar*);
using PFN_glUniform1i              = void (APIENTRY*)(GLint, GLint);
using PFN_glUniform2f              = void (APIENTRY*)(GLint, GLfloat, GLfloat);
using PFN_glGenVertexArrays        = void (APIENTRY*)(GLsizei, GLuint*);
using PFN_glDeleteVertexArrays     = void (APIENTRY*)(GLsizei, const GLuint*);
using PFN_glBindVertexArray        = void (APIENTRY*)(GLuint);
using PFN_glGenBuffers             = void (APIENTRY*)(GLsizei, GLuint*);
using PFN_glDeleteBuffers          = void (APIENTRY*)(GLsizei, const GLuint*);
using PFN_glBindBuffer             = void (APIENTRY*)(GLenum, GLuint);
using PFN_glBufferData             = void (APIENTRY*)(GLenum, GLsizeiptr, const void*, GLenum);
using PFN_glEnableVertexAttribArray= void (APIENTRY*)(GLuint);
using PFN_glDisableVertexAttribArray = void (APIENTRY*)(GLuint);
using PFN_glVertexAttribPointer    = void (APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
using PFN_glGenFramebuffers        = void (APIENTRY*)(GLsizei, GLuint*);
using PFN_glDeleteFramebuffers     = void (APIENTRY*)(GLsizei, const GLuint*);
using PFN_glBindFramebuffer        = void (APIENTRY*)(GLenum, GLuint);
using PFN_glFramebufferTexture2D   = void (APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
using PFN_glBlitFramebuffer        = void (APIENTRY*)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
using PFN_glCheckFramebufferStatus = GLenum (APIENTRY*)(GLenum);
using PFN_glDrawArrays             = void (APIENTRY*)(GLenum, GLint, GLsizei);
using PFN_glTexImage2D             = void (APIENTRY*)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
using PFN_glCopyTexImage2D         = void (APIENTRY*)(GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint);
using PFN_glTexParameteri          = void (APIENTRY*)(GLenum, GLenum, GLint);
using PFN_glGenTextures            = void (APIENTRY*)(GLsizei, GLuint*);
using PFN_glDeleteTextures         = void (APIENTRY*)(GLsizei, const GLuint*);
using PFN_glBindTexture            = void (APIENTRY*)(GLenum, GLuint);

// ---------------------------------------------------------------------------
// Resolved pointers.
// ---------------------------------------------------------------------------
inline PFN_glActiveTexture            p_glActiveTexture            = nullptr;
inline PFN_glBlendFuncSeparate        p_glBlendFuncSeparate        = nullptr;
inline PFN_glCreateShader             p_glCreateShader             = nullptr;
inline PFN_glShaderSource             p_glShaderSource             = nullptr;
inline PFN_glCompileShader            p_glCompileShader            = nullptr;
inline PFN_glGetShaderiv              p_glGetShaderiv              = nullptr;
inline PFN_glGetShaderInfoLog         p_glGetShaderInfoLog         = nullptr;
inline PFN_glDeleteShader             p_glDeleteShader             = nullptr;
inline PFN_glCreateProgram            p_glCreateProgram            = nullptr;
inline PFN_glAttachShader             p_glAttachShader             = nullptr;
inline PFN_glLinkProgram              p_glLinkProgram              = nullptr;
inline PFN_glGetProgramiv             p_glGetProgramiv             = nullptr;
inline PFN_glGetProgramInfoLog        p_glGetProgramInfoLog        = nullptr;
inline PFN_glDeleteProgram            p_glDeleteProgram            = nullptr;
inline PFN_glUseProgram               p_glUseProgram               = nullptr;
inline PFN_glGetUniformLocation      p_glGetUniformLocation       = nullptr;
inline PFN_glUniform1i                p_glUniform1i                = nullptr;
inline PFN_glUniform2f                p_glUniform2f                = nullptr;
inline PFN_glGenVertexArrays          p_glGenVertexArrays          = nullptr;
inline PFN_glDeleteVertexArrays       p_glDeleteVertexArrays       = nullptr;
inline PFN_glBindVertexArray          p_glBindVertexArray          = nullptr;
inline PFN_glGenBuffers               p_glGenBuffers               = nullptr;
inline PFN_glDeleteBuffers            p_glDeleteBuffers            = nullptr;
inline PFN_glBindBuffer               p_glBindBuffer               = nullptr;
inline PFN_glBufferData               p_glBufferData               = nullptr;
inline PFN_glEnableVertexAttribArray  p_glEnableVertexAttribArray  = nullptr;
inline PFN_glDisableVertexAttribArray p_glDisableVertexAttribArray = nullptr;
inline PFN_glVertexAttribPointer      p_glVertexAttribPointer      = nullptr;
inline PFN_glGenFramebuffers          p_glGenFramebuffers          = nullptr;
inline PFN_glDeleteFramebuffers       p_glDeleteFramebuffers       = nullptr;
inline PFN_glBindFramebuffer          p_glBindFramebuffer          = nullptr;
inline PFN_glFramebufferTexture2D     p_glFramebufferTexture2D     = nullptr;
inline PFN_glBlitFramebuffer          p_glBlitFramebuffer          = nullptr;
inline PFN_glCheckFramebufferStatus   p_glCheckFramebufferStatus   = nullptr;
inline PFN_glDrawArrays               p_glDrawArrays               = nullptr;
inline PFN_glTexImage2D               p_glTexImage2D               = nullptr;
inline PFN_glCopyTexImage2D           p_glCopyTexImage2D           = nullptr;
inline PFN_glTexParameteri            p_glTexParameteri            = nullptr;
inline PFN_glGenTextures              p_glGenTextures              = nullptr;
inline PFN_glDeleteTextures           p_glDeleteTextures           = nullptr;
inline PFN_glBindTexture              p_glBindTexture              = nullptr;

inline bool g_glexReady = false;

// Resolve every pointer; returns false if any required one is missing
// (context is GL1-only — blur then degrades to off, GUI still works).
inline bool init() {
    if (g_glexReady) return true;
    HMODULE ogl = GetModuleHandleW(L"opengl32.dll");
    if (!ogl) return false;
    auto get = [&](const char* n) -> PROC {
        PROC p = ::wglGetProcAddress(n);
        if (!p) {
            // Some 1.1-adjacent exports live in the DLL itself.
            p = ::GetProcAddress(ogl, n);
        }
        return p;
    };
#define TYR_GLEX(NAME) p_##NAME = reinterpret_cast<PFN_##NAME>(get(#NAME));
    TYR_GLEX(glActiveTexture)
    TYR_GLEX(glBlendFuncSeparate)
    TYR_GLEX(glCreateShader)
    TYR_GLEX(glShaderSource)
    TYR_GLEX(glCompileShader)
    TYR_GLEX(glGetShaderiv)
    TYR_GLEX(glGetShaderInfoLog)
    TYR_GLEX(glDeleteShader)
    TYR_GLEX(glCreateProgram)
    TYR_GLEX(glAttachShader)
    TYR_GLEX(glLinkProgram)
    TYR_GLEX(glGetProgramiv)
    TYR_GLEX(glGetProgramInfoLog)
    TYR_GLEX(glDeleteProgram)
    TYR_GLEX(glUseProgram)
    TYR_GLEX(glGetUniformLocation)
    TYR_GLEX(glUniform1i)
    TYR_GLEX(glUniform2f)
    TYR_GLEX(glGenVertexArrays)
    TYR_GLEX(glDeleteVertexArrays)
    TYR_GLEX(glBindVertexArray)
    TYR_GLEX(glGenBuffers)
    TYR_GLEX(glDeleteBuffers)
    TYR_GLEX(glBindBuffer)
    TYR_GLEX(glBufferData)
    TYR_GLEX(glEnableVertexAttribArray)
    TYR_GLEX(glDisableVertexAttribArray)
    TYR_GLEX(glVertexAttribPointer)
    TYR_GLEX(glGenFramebuffers)
    TYR_GLEX(glDeleteFramebuffers)
    TYR_GLEX(glBindFramebuffer)
    TYR_GLEX(glFramebufferTexture2D)
    TYR_GLEX(glBlitFramebuffer)
    TYR_GLEX(glCheckFramebufferStatus)
    TYR_GLEX(glDrawArrays)
    TYR_GLEX(glTexImage2D)
    TYR_GLEX(glCopyTexImage2D)
    TYR_GLEX(glTexParameteri)
    TYR_GLEX(glGenTextures)
    TYR_GLEX(glDeleteTextures)
    TYR_GLEX(glBindTexture)
    p_glBlendColor = reinterpret_cast<PFN_glBlendColor>(get("glBlendColor"));
    p_glDebugMessageControl = reinterpret_cast<PFN_glDebugMessageControl>(get("glDebugMessageControl"));
#undef TYR_GLEX

    g_glexReady =
        p_glCreateShader && p_glCreateProgram && p_glBindFramebuffer &&
        p_glBlitFramebuffer && p_glBindVertexArray && p_glBindBuffer && p_glDrawArrays;
    return g_glexReady;
}

} // namespace glx
} // namespace tyr

// ---------------------------------------------------------------------------
// Macro-map plain GL names onto the resolved pointers for Passive sources.
// (Defined AFTER the namespace so blur.cpp can simply call glCreateShader etc.)
// NOTE: applied per translation unit that includes this header.
// ---------------------------------------------------------------------------
#define glActiveTexture             tyr::glx::p_glActiveTexture
#define glBlendColor                tyr::glx::p_glBlendColor
#define glDebugMessageControl       tyr::glx::p_glDebugMessageControl
#define glBlendFuncSeparate         tyr::glx::p_glBlendFuncSeparate
#define glCreateShader              tyr::glx::p_glCreateShader
#define glShaderSource              tyr::glx::p_glShaderSource
#define glCompileShader             tyr::glx::p_glCompileShader
#define glGetShaderiv               tyr::glx::p_glGetShaderiv
#define glGetShaderInfoLog          tyr::glx::p_glGetShaderInfoLog
#define glDeleteShader              tyr::glx::p_glDeleteShader
#define glCreateProgram             tyr::glx::p_glCreateProgram
#define glAttachShader              tyr::glx::p_glAttachShader
#define glLinkProgram               tyr::glx::p_glLinkProgram
#define glGetProgramiv              tyr::glx::p_glGetProgramiv
#define glGetProgramInfoLog         tyr::glx::p_glGetProgramInfoLog
#define glDeleteProgram             tyr::glx::p_glDeleteProgram
#define glUseProgram                tyr::glx::p_glUseProgram
#define glGetUniformLocation        tyr::glx::p_glGetUniformLocation
#define glUniform1i                 tyr::glx::p_glUniform1i
#define glUniform2f                 tyr::glx::p_glUniform2f
#define glGenVertexArrays           tyr::glx::p_glGenVertexArrays
#define glDeleteVertexArrays        tyr::glx::p_glDeleteVertexArrays
#define glBindVertexArray           tyr::glx::p_glBindVertexArray
#define glGenBuffers                tyr::glx::p_glGenBuffers
#define glDeleteBuffers             tyr::glx::p_glDeleteBuffers
#define glBindBuffer                tyr::glx::p_glBindBuffer
#define glBufferData                tyr::glx::p_glBufferData
#define glEnableVertexAttribArray   tyr::glx::p_glEnableVertexAttribArray
#define glDisableVertexAttribArray  tyr::glx::p_glDisableVertexAttribArray
#define glVertexAttribPointer       tyr::glx::p_glVertexAttribPointer
#define glGenFramebuffers           tyr::glx::p_glGenFramebuffers
#define glDeleteFramebuffers        tyr::glx::p_glDeleteFramebuffers
#define glBindFramebuffer           tyr::glx::p_glBindFramebuffer
#define glFramebufferTexture2D      tyr::glx::p_glFramebufferTexture2D
#define glBlitFramebuffer           tyr::glx::p_glBlitFramebuffer
#define glCheckFramebufferStatus    tyr::glx::p_glCheckFramebufferStatus
#define glDrawArrays                tyr::glx::p_glDrawArrays
#define glTexImage2D                tyr::glx::p_glTexImage2D
#define glCopyTexImage2D            tyr::glx::p_glCopyTexImage2D
#define glTexParameteri             tyr::glx::p_glTexParameteri
#define glGenTextures               tyr::glx::p_glGenTextures
#define glDeleteTextures            tyr::glx::p_glDeleteTextures
#define glBindTexture               tyr::glx::p_glBindTexture
