/* Bug-hunt GL debug check (see pg_gl_debug.h); linked only into PG_GL_DEBUG builds. */
#include <GLES2/gl2.h>
#include <android/log.h>
#include <cstdlib>

extern "C" void pg_gl_debug_check(const char *call, const char *file, int line)
{
  const GLenum error = glGetError();
  if (error == GL_NO_ERROR) return;
  __android_log_print(ANDROID_LOG_FATAL, "PG_GL_ERROR", "%s failed with GL error 0x%04x at %s:%d", call, error, file, line);
  abort();
}
