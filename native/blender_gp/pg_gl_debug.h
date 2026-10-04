/* Bug-hunt GL debug (PG_GL_DEBUG): force-included into the Project Grease sources that call GLES.
 * Every GLES call below is followed by a glGetError() check; an error logs the call and its call
 * site (tag PG_GL_ERROR) and aborts, so the test that triggered it fails at the faulty call instead
 * of at a later, unrelated one. glGetError itself is not wrapped. Not used in normal builds. */
#pragma once
#include <GLES2/gl2.h>
#include <GLES3/gl3.h>

#ifdef __cplusplus
extern "C" {
#endif
void pg_gl_debug_check(const char *call, const char *file, int line);
#ifdef __cplusplus
}
#endif

/* The function-like macro is not expanded again inside its own expansion, so `call(...)` below
 * reaches the real GLES entry point. */
#define PG_GL_VOID(call, ...) (call(__VA_ARGS__), pg_gl_debug_check(#call, __FILE__, __LINE__))
#define PG_GL_VALUE(call, ...) \
  __extension__({ __typeof__(call(__VA_ARGS__)) pg_gl_result_ = call(__VA_ARGS__); \
                  pg_gl_debug_check(#call, __FILE__, __LINE__); pg_gl_result_; })

#define glActiveTexture(...) PG_GL_VOID(glActiveTexture, __VA_ARGS__)
#define glAttachShader(...) PG_GL_VOID(glAttachShader, __VA_ARGS__)
#define glBindAttribLocation(...) PG_GL_VOID(glBindAttribLocation, __VA_ARGS__)
#define glBindBuffer(...) PG_GL_VOID(glBindBuffer, __VA_ARGS__)
#define glBindFramebuffer(...) PG_GL_VOID(glBindFramebuffer, __VA_ARGS__)
#define glBindRenderbuffer(...) PG_GL_VOID(glBindRenderbuffer, __VA_ARGS__)
#define glBindTexture(...) PG_GL_VOID(glBindTexture, __VA_ARGS__)
#define glBindVertexArray(...) PG_GL_VOID(glBindVertexArray, __VA_ARGS__)
#define glBlendEquation(...) PG_GL_VOID(glBlendEquation, __VA_ARGS__)
#define glBlendFunc(...) PG_GL_VOID(glBlendFunc, __VA_ARGS__)
#define glBlendFuncSeparate(...) PG_GL_VOID(glBlendFuncSeparate, __VA_ARGS__)
#define glBufferData(...) PG_GL_VOID(glBufferData, __VA_ARGS__)
#define glBufferSubData(...) PG_GL_VOID(glBufferSubData, __VA_ARGS__)
#define glClear(...) PG_GL_VOID(glClear, __VA_ARGS__)
#define glClearColor(...) PG_GL_VOID(glClearColor, __VA_ARGS__)
#define glClearStencil(...) PG_GL_VOID(glClearStencil, __VA_ARGS__)
#define glCompileShader(...) PG_GL_VOID(glCompileShader, __VA_ARGS__)
#define glCopyTexSubImage2D(...) PG_GL_VOID(glCopyTexSubImage2D, __VA_ARGS__)
#define glDeleteBuffers(...) PG_GL_VOID(glDeleteBuffers, __VA_ARGS__)
#define glDeleteFramebuffers(...) PG_GL_VOID(glDeleteFramebuffers, __VA_ARGS__)
#define glDeleteProgram(...) PG_GL_VOID(glDeleteProgram, __VA_ARGS__)
#define glDeleteRenderbuffers(...) PG_GL_VOID(glDeleteRenderbuffers, __VA_ARGS__)
#define glDeleteShader(...) PG_GL_VOID(glDeleteShader, __VA_ARGS__)
#define glDeleteTextures(...) PG_GL_VOID(glDeleteTextures, __VA_ARGS__)
#define glDeleteVertexArrays(...) PG_GL_VOID(glDeleteVertexArrays, __VA_ARGS__)
#define glDisable(...) PG_GL_VOID(glDisable, __VA_ARGS__)
#define glDisableVertexAttribArray(...) PG_GL_VOID(glDisableVertexAttribArray, __VA_ARGS__)
#define glDrawArrays(...) PG_GL_VOID(glDrawArrays, __VA_ARGS__)
#define glDrawElements(...) PG_GL_VOID(glDrawElements, __VA_ARGS__)
#define glEnable(...) PG_GL_VOID(glEnable, __VA_ARGS__)
#define glEnableVertexAttribArray(...) PG_GL_VOID(glEnableVertexAttribArray, __VA_ARGS__)
#define glFinish(...) PG_GL_VOID(glFinish, __VA_ARGS__)
#define glFramebufferRenderbuffer(...) PG_GL_VOID(glFramebufferRenderbuffer, __VA_ARGS__)
#define glFramebufferTexture2D(...) PG_GL_VOID(glFramebufferTexture2D, __VA_ARGS__)
#define glGenBuffers(...) PG_GL_VOID(glGenBuffers, __VA_ARGS__)
#define glGenFramebuffers(...) PG_GL_VOID(glGenFramebuffers, __VA_ARGS__)
#define glGenRenderbuffers(...) PG_GL_VOID(glGenRenderbuffers, __VA_ARGS__)
#define glGenTextures(...) PG_GL_VOID(glGenTextures, __VA_ARGS__)
#define glGenVertexArrays(...) PG_GL_VOID(glGenVertexArrays, __VA_ARGS__)
#define glGetIntegerv(...) PG_GL_VOID(glGetIntegerv, __VA_ARGS__)
#define glGetProgramiv(...) PG_GL_VOID(glGetProgramiv, __VA_ARGS__)
#define glGetShaderiv(...) PG_GL_VOID(glGetShaderiv, __VA_ARGS__)
#define glLinkProgram(...) PG_GL_VOID(glLinkProgram, __VA_ARGS__)
#define glPixelStorei(...) PG_GL_VOID(glPixelStorei, __VA_ARGS__)
#define glReadPixels(...) PG_GL_VOID(glReadPixels, __VA_ARGS__)
#define glRenderbufferStorage(...) PG_GL_VOID(glRenderbufferStorage, __VA_ARGS__)
#define glShaderSource(...) PG_GL_VOID(glShaderSource, __VA_ARGS__)
#define glStencilFunc(...) PG_GL_VOID(glStencilFunc, __VA_ARGS__)
#define glStencilMask(...) PG_GL_VOID(glStencilMask, __VA_ARGS__)
#define glStencilOp(...) PG_GL_VOID(glStencilOp, __VA_ARGS__)
#define glTexImage2D(...) PG_GL_VOID(glTexImage2D, __VA_ARGS__)
#define glTexParameteri(...) PG_GL_VOID(glTexParameteri, __VA_ARGS__)
#define glUniform1f(...) PG_GL_VOID(glUniform1f, __VA_ARGS__)
#define glUniform1i(...) PG_GL_VOID(glUniform1i, __VA_ARGS__)
#define glUniform2f(...) PG_GL_VOID(glUniform2f, __VA_ARGS__)
#define glUniform3f(...) PG_GL_VOID(glUniform3f, __VA_ARGS__)
#define glUniform4f(...) PG_GL_VOID(glUniform4f, __VA_ARGS__)
#define glUseProgram(...) PG_GL_VOID(glUseProgram, __VA_ARGS__)
#define glVertexAttribIPointer(...) PG_GL_VOID(glVertexAttribIPointer, __VA_ARGS__)
#define glVertexAttribPointer(...) PG_GL_VOID(glVertexAttribPointer, __VA_ARGS__)
#define glViewport(...) PG_GL_VOID(glViewport, __VA_ARGS__)
#define glCheckFramebufferStatus(...) PG_GL_VALUE(glCheckFramebufferStatus, __VA_ARGS__)
#define glCreateProgram(...) PG_GL_VALUE(glCreateProgram, __VA_ARGS__)
#define glCreateShader(...) PG_GL_VALUE(glCreateShader, __VA_ARGS__)
#define glGetAttribLocation(...) PG_GL_VALUE(glGetAttribLocation, __VA_ARGS__)
#define glGetUniformLocation(...) PG_GL_VALUE(glGetUniformLocation, __VA_ARGS__)
