#!/usr/bin/env python3
"""Wire the Blender selection port into the existing native backend (idempotent).

Edits three existing files with exact-anchor replacements and fails loudly (changing
nothing) if an anchor is missing or ambiguous:

  native/blender_gp/project_grease_gp_backend.h   forward declarations + two accessors
  native/blender_gp/project_grease_gp_backend.cpp accessor definitions
  native/blender_gp/project_grease_gp_bridge.cpp  route applyEditCommand ids 20..30
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
NATIVE = ROOT / "native" / "blender_gp"


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"anchor for '{label}' found {count} times (expected 1); nothing changed")
    return text.replace(old, new)


def patch(path, edits, done_marker):
    text = path.read_text(encoding="utf-8")
    if done_marker in text:
        print(f"already applied: {path.relative_to(ROOT)}")
        return None
    for old, new, label in edits:
        text = replace_once(text, old, new, label)
    return text


def main():
    header = NATIVE / "project_grease_gp_backend.h"
    backend = NATIVE / "project_grease_gp_backend.cpp"
    bridge = NATIVE / "project_grease_gp_bridge.cpp"

    new_header = patch(
        header,
        [
            (
                '#include "project_grease_legacy_sculpt.h"\n\nnamespace project_grease::gp {',
                '#include "project_grease_legacy_sculpt.h"\n\n'
                '// Blender DNA types, declared at global scope so the accessors below name the\n'
                '// real C structs and not types of project_grease::gp.\n'
                'struct bGPdata;\nstruct bGPDlayer;\n\n'
                'namespace project_grease::gp {',
                "header forward declarations",
            ),
            (
                '  // Opaque implementation storage.\n  struct Impl;',
                '  // Raw Legacy GP document and active layer for focused editor operators\n'
                '  // (Blender selection port). Valid until the document is reset or shut down.\n'
                '  bGPdata* document_data() const;\n'
                '  bGPDlayer* active_layer_data() const;\n\n'
                '  // Opaque implementation storage.\n  struct Impl;',
                "header accessors",
            ),
        ],
        "document_data",
    )

    new_backend = patch(
        backend,
        [
            (
                'const char *Backend::last_error() const { return impl_->last_error.c_str(); }',
                'const char *Backend::last_error() const { return impl_->last_error.c_str(); }\n\n'
                'bGPdata *Backend::document_data() const { return impl_->gpd; }\n\n'
                'bGPDlayer *Backend::active_layer_data() const { return impl_->layer; }',
                "backend accessor definitions",
            ),
        ],
        "Backend::document_data",
    )

    new_bridge = patch(
        bridge,
        [
            (
                '#include "project_grease_legacy_primitive.h"\n',
                '#include "project_grease_legacy_primitive.h"\n'
                '#include "project_grease_blender_select.h"\n',
                "bridge include",
            ),
            (
                '    default:\n      return 0;\n  }\n}',
                '    default:\n'
                '      // Blender 3.6.23 Legacy GP selection operators (ids 20..30).\n'
                '      return pg_gp_select_dispatch(handle->backend.document_data(),\n'
                '                                   handle->backend.active_layer_data(),\n'
                '                                   command,\n'
                '                                   args,\n'
                '                                   arg_count);\n'
                '  }\n}',
                "bridge default branch",
            ),
        ],
        "pg_gp_select_dispatch",
    )

    for path, text in ((header, new_header), (backend, new_backend), (bridge, new_bridge)):
        if text is not None:
            path.write_text(text, encoding="utf-8")
            print(f"patched: {path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
