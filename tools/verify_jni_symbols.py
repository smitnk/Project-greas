#!/usr/bin/env python3
"""Fail when a Kotlin `external fun` of GPNative has no JNI symbol in the APK's native libraries.

A missing one only shows on the device, as an UnsatisfiedLinkError the first time it is called
(the reported eraser crash). usage: verify_jni_symbols.py <app.apk> <llvm-nm>
"""
import re, subprocess, sys, tempfile, zipfile, pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
KT = ROOT / "android/app/src/main/java/com/smitnk/projectgrease/nativebridge/GPNative.kt"
PREFIX = "Java_com_smitnk_projectgrease_nativebridge_GPNative_"

def main(apk, nm):
    names = sorted(set(re.findall(r"^\s*(?:@\w+\s+)*external fun (\w+)\(", KT.read_text(), re.M)))
    found = {}
    with tempfile.TemporaryDirectory() as d, zipfile.ZipFile(apk) as z:
        for abi in ("arm64-v8a", "x86_64"):
            libs = [n for n in z.namelist() if n.startswith(f"lib/{abi}/") and n.endswith(".so")]
            syms = set()
            for n in libs:
                p = z.extract(n, d)
                out = subprocess.run([nm, "-D", "--defined-only", p], capture_output=True, text=True, check=True).stdout
                syms |= {line.split()[-1] for line in out.splitlines() if line.strip()}
            found[abi] = syms
    missing = []
    for abi, syms in found.items():
        if not syms:
            continue
        for n in names:
            sym = PREFIX + n.replace("_", "_1")
            if sym not in syms and not any(s.startswith(sym + "__") for s in syms):
                missing.append(f"{abi}: {n}")
    for m in missing:
        print(f"::error title=missing JNI symbol::{m} (external fun in GPNative.kt with no native implementation)")
    print(f"JNI symbols: {len(names)} external funs checked, {len(missing)} missing")
    return 1 if missing else 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1], sys.argv[2]))
