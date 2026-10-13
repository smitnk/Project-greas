/* Native crash recorder: a crash in C/C++ code (SIGSEGV, SIGABRT, ...) never reaches the Kotlin
 * crash log, so the app showed "No crash recorded" for native crashes. On such a signal this writes
 * the signal, the fault address and a backtrace with library + symbol names to the same file
 * (Menu > Last crash report), then lets the previous handler run (the system still sees the crash). */
#include <jni.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <unwind.h>
#include <cstdio>
#include <cstring>

namespace {
char g_path[512];
struct sigaction g_old[32];
const int kSignals[] = {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL};

struct Trace { void **pcs; int n, max; };
_Unwind_Reason_Code unwind_cb(struct _Unwind_Context *ctx, void *arg)
{
  Trace *t = static_cast<Trace *>(arg);
  const uintptr_t pc = _Unwind_GetIP(ctx);
  if (pc && t->n < t->max) t->pcs[t->n++] = reinterpret_cast<void *>(pc);
  return t->n < t->max ? _URC_NO_REASON : _URC_END_OF_STACK;
}

void put(int fd, const char *s) { if (s) (void)!write(fd, s, std::strlen(s)); }

void on_signal(int sig, siginfo_t *info, void *uctx)
{
  const int fd = open(g_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd >= 0) {
    char line[512];
    std::snprintf(line, sizeof line, "Project Grease native crash: signal %d (%s), fault address %p\n\n", sig,
                  sig == SIGSEGV ? "SIGSEGV" : sig == SIGABRT ? "SIGABRT" : sig == SIGBUS ? "SIGBUS" :
                  sig == SIGFPE ? "SIGFPE" : "SIGILL", info ? info->si_addr : nullptr);
    put(fd, line);
    void *pcs[48];
    Trace t{pcs, 0, 48};
    _Unwind_Backtrace(unwind_cb, &t);
    for (int i = 0; i < t.n; i++) {
      Dl_info d{};
      if (dladdr(pcs[i], &d) && d.dli_fname) {
        const char *lib = std::strrchr(d.dli_fname, '/');
        std::snprintf(line, sizeof line, "#%02d %s %s+0x%zx\n", i, lib ? lib + 1 : d.dli_fname,
                      d.dli_sname ? d.dli_sname : "?",
                      static_cast<size_t>(static_cast<char *>(pcs[i]) - static_cast<char *>(d.dli_sname ? d.dli_saddr : d.dli_fbase)));
      }
      else std::snprintf(line, sizeof line, "#%02d %p\n", i, pcs[i]);
      put(fd, line);
    }
    close(fd);
  }
  // hand over to the previous handler (the system's crash dump), as if we were not here
  sigaction(sig, &g_old[sig], nullptr);
  raise(sig);
}
}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_com_smitnk_projectgrease_nativebridge_GPNative_nativeInstallCrashHandler(JNIEnv *env, jobject, jstring path)
{
  const char *p = path ? env->GetStringUTFChars(path, nullptr) : nullptr;
  if (!p) return;
  std::snprintf(g_path, sizeof g_path, "%s", p);
  env->ReleaseStringUTFChars(path, p);
  struct sigaction sa{};
  sa.sa_sigaction = on_signal;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&sa.sa_mask);
  for (int s : kSignals) sigaction(s, &sa, &g_old[s]);
}
