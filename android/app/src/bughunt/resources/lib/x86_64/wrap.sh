#!/system/bin/sh
# Bug-hunt build: starts the app with the NDK's AddressSanitizer runtime preloaded, so a memory error
# in the native GP code aborts with an ASan report in logcat (tag "wrap.sh" / "DEBUG").
HERE="$(cd "$(dirname "$0")" && pwd)"
export ASAN_OPTIONS=log_to_syslog=true,allow_user_segv_handler=1,abort_on_error=1,detect_leaks=0,detect_container_overflow=0
ASAN_LIB=$(ls "$HERE"/libclang_rt.asan-*-android.so)
if [ -f "$HERE/libc++_shared.so" ]; then
    export LD_PRELOAD="$ASAN_LIB $HERE/libc++_shared.so"
else
    export LD_PRELOAD="$ASAN_LIB"
fi
"$@"
