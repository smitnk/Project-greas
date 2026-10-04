#!/usr/bin/env python3
"""APK size breakdown for CI: dex, native libraries per ABI, resources, assets and the rest, both
stored (compressed) and uncompressed, plus apkanalyzer's file / download size when the SDK has it.
usage: apk_size.py <label>=<apk> [<label>=<apk> ...]"""
import os
import shutil
import subprocess
import sys
import zipfile


def group(name):
    if name.endswith(".dex"):
        return "dex"
    if name.startswith("lib/"):
        parts = name.split("/")
        return f"native libs {parts[1]}" if len(parts) > 2 else "native libs"
    if name.startswith("res/") or name == "resources.arsc":
        return "resources"
    if name.startswith("assets/"):
        return "assets"
    if name.startswith("META-INF/"):
        return "META-INF (signatures)"
    return "other"


def mb(n):
    return f"{n / 1048576.0:8.2f} MB"


def apkanalyzer():
    sdk = os.environ.get("ANDROID_SDK_ROOT") or os.environ.get("ANDROID_HOME") or "/usr/local/lib/android/sdk"
    for c in (os.path.join(sdk, "cmdline-tools", "latest", "bin", "apkanalyzer"), shutil.which("apkanalyzer") or ""):
        if c and os.path.exists(c):
            return c
    return None


def main():
    tool = apkanalyzer()
    for arg in sys.argv[1:]:
        label, path = arg.split("=", 1)
        if not os.path.exists(path):
            print(f"== APK size {label}: {path} missing ==")
            continue
        sizes = {}
        with zipfile.ZipFile(path) as z:
            for info in z.infolist():
                g = sizes.setdefault(group(info.filename), [0, 0, 0])
                g[0] += info.compress_size
                g[1] += info.file_size
                g[2] += 1
        print(f"== APK size {label}: {os.path.basename(path)} {mb(os.path.getsize(path))} ==")
        print(f"   {'part':28} {'in APK':>11} {'uncompressed':>13} files")
        for name, (c, u, n) in sorted(sizes.items(), key=lambda kv: -kv[1][0]):
            print(f"   {name:28} {mb(c)} {mb(u)}  {n}")
        if tool:
            for what in ("file-size", "download-size"):
                r = subprocess.run([tool, "apk", what, path], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                print(f"   apkanalyzer apk {what}: {r.stdout.strip()} bytes")
            r = subprocess.run([tool, "dex", "references", path], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            print("   apkanalyzer dex references: " + " ".join(r.stdout.split()))
        else:
            print("   (apkanalyzer not found in the SDK)")


if __name__ == "__main__":
    main()
