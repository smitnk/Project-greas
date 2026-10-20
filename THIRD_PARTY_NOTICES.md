# Licence and third-party notices

Project Grease is free software under the **GNU General Public License, version 3 or later**
(GPL-3.0-or-later); the full text is in [LICENSE](LICENSE).

The app compiles and links parts of Blender, which is licensed GPL-2.0-or-later (used here under
GPL version 3). Because the app is a combined work with Blender's code, the whole app is
distributed under the GPL:

- Anyone who receives the app may use it for any purpose, including commercial work. Drawings and
  animations made with the app belong to their author; the GPL does not cover them.
- The app may be sold or distributed commercially, but everyone who receives it must also be able
  to get the complete corresponding source code under the GPL (this public repository serves that
  purpose; GitHub's "Source code" downloads on each release are that offer).
- Modified versions must stay under the GPL, keep these notices, and say that they were changed.
- There is no warranty (GPL sections 15 and 16).

"Blender" is a trademark of the Blender Foundation. Project Grease is not made, endorsed or
supported by the Blender Foundation; its name and icon must not suggest otherwise.

## Third-party code built into the app

| Component | Where | Licence |
|---|---|---|
| Blender 3.6.23 (Grease Pencil, blenlib, blenkernel, draw engine, GPU module) | `third_party/blender` (pinned, imported by `tools/import_blender_gp.sh`) | GPL-2.0-or-later |
| curve_fit_nd (part of Blender's `extern/`) | `third_party/blender/extern/curve_fit_nd` | BSD 3-Clause (DWANGO Co., Ltd.) |
| zlib 1.2.13 | `third_party/zlib-1.2.13` | zlib licence |
| Zstandard 1.5.0 | `third_party/zstd-1.5.0` | BSD 3-Clause |
| sse2neon | `third_party/sse2neon` | MIT |
| AndroidX / Jetpack Compose / Kotlin standard library | Gradle dependencies | Apache-2.0 |

All of these licences allow use inside a GPL-3.0 program. Their notices and full texts are in
their source directories (Blender's are in `third_party/blender/release/license/`).
