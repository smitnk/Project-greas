/* SPDX-License-Identifier: GPL-2.0-or-later
 * Blender 3.6.23 .blend open / save for Project Grease (project_grease_blend_io.cc).
 * Both directions go through the app's project document JSON (ProjectDocumentCodec, version 6):
 * the app loads / saves that JSON as it does its own files. */
#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* A .blend (plain, zstd or gzip) to project JSON: the first legacy GP object and its armature.
 * Returns malloc'd text or NULL (the reason in err). */
char *pg_blend_to_project_json(const void *data, size_t len, char *err, int err_len);
/* Project JSON to a .blend 3.6 file (uncompressed): Scene, the GP object and, when the JSON has a rig,
 * the armature object with its pose and action. Returns malloc'd bytes or NULL. */
void *pg_project_json_to_blend(const char *json, size_t *r_len, char *err, int err_len);
#ifdef __cplusplus
}
#endif
