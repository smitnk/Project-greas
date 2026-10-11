#!/usr/bin/env python3
"""L8: writes the build-generated inputs Blender 3.6.23's GPU shader code expects, from the lists in
Blender's own CMakeLists (gpu/CMakeLists.txt, draw/CMakeLists.txt), exactly as their CMake does:
  glsl_gpu_source_list.h / glsl_draw_source_list.h   SHADER_SOURCE(datatoc_x_glsl, "x.glsl", "path")
  gpu_shader_create_info_list.hh                     #include "<info>.hh" per SRC_SHADER_CREATE_INFOS
  glsl_compositor_source_list.h, compositor_shader_create_info_list.hh   (empty: compositor not used)
and prints the GLSL files (one per line) for datatoc.
  python3 -I gen_lists.py <blender source/blender dir> <out dir>
"""
import re
import sys
from pathlib import Path


def cmake_list(text, name):
    m = re.search(r"set\(" + re.escape(name) + r"\s*\n(.*?)\n\)", text, re.S)
    if not m:
        raise SystemExit(f"list {name} not found")
    items = []
    for line in m.group(1).splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            items.extend(line.split())
    return items


def main():
    bl, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    glsl = []
    for sub, listname in (("gpu", "glsl_gpu_source_list.h"), ("draw", "glsl_draw_source_list.h")):
        text = (bl / sub / "CMakeLists.txt").read_text()
        files = cmake_list(text, "GLSL_SRC")
        lines = []
        for f in files:
            name = Path(f).name
            lines.append(f'SHADER_SOURCE(datatoc_{name.replace(".", "_")}, "{name}", "{f}")\n')
            glsl.append(str((bl / sub / f).resolve()))
        (out / listname).write_text("".join(lines))
    infos = cmake_list((bl / "gpu" / "CMakeLists.txt").read_text(), "SRC_SHADER_CREATE_INFOS")
    (out / "gpu_shader_create_info_list.hh").write_text(
        "".join(f'#include "{(bl / "gpu" / f).resolve()}"\n' for f in infos))
    (out / "glsl_compositor_source_list.h").write_text("")
    (out / "compositor_shader_create_info_list.hh").write_text("")
    print("\n".join(glsl))


if __name__ == "__main__":
    main()
