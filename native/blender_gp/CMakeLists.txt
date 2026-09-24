# Project Grease native backend
cmake_minimum_required(VERSION 3.22)
project(project_grease_blender_gp LANGUAGES C CXX)

set(BLENDER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/blender"
    CACHE PATH "Pinned upstream Blender source tree")

if(NOT EXISTS "${BLENDER_SOURCE_DIR}/source/blender/draw/engines/gpencil/gpencil_engine.c")
  message(FATAL_ERROR
    "Blender source is not imported. Run tools/import_blender_gp.sh first.")
endif()

add_library(project_grease_blender_gp STATIC
  project_grease_gp_backend.cpp

  # First real Blender GP rendering sources.
  # These are the legacy Grease Pencil draw-cache and draw-engine layers;
  # the full Blender application/editor is not included.
  "${BLENDER_SOURCE_DIR}/source/blender/draw/intern/draw_cache_impl_gpencil.cc"
  "${BLENDER_SOURCE_DIR}/source/blender/draw/engines/gpencil/gpencil_engine.c"
)

target_include_directories(project_grease_blender_gp PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${BLENDER_SOURCE_DIR}/source/blender"
  "${BLENDER_SOURCE_DIR}/source"
  "${BLENDER_SOURCE_DIR}/source/blender/blenkernel"
  "${BLENDER_SOURCE_DIR}/source/blender/blenlib"
  "${BLENDER_SOURCE_DIR}/source/blender/bmesh"
  "${BLENDER_SOURCE_DIR}/source/blender/makesdna"
  "${BLENDER_SOURCE_DIR}/source/blender/makesrna"
  "${BLENDER_SOURCE_DIR}/source/blender/draw"
  "${BLENDER_SOURCE_DIR}/source/blender/draw/intern"
  "${BLENDER_SOURCE_DIR}/source/blender/draw/engines/gpencil"
  "${BLENDER_SOURCE_DIR}/source/blender/editors/include"
  "${BLENDER_SOURCE_DIR}/source/blender/windowmanager"
  "${BLENDER_SOURCE_DIR}/source/blender/depsgraph"
  "${BLENDER_SOURCE_DIR}/source/blender/blentranslation"
  "${BLENDER_SOURCE_DIR}/source/blender/gpu"
  "${BLENDER_SOURCE_DIR}/source/blender/render"
  "${BLENDER_SOURCE_DIR}/intern/guardedalloc"
  "${BLENDER_SOURCE_DIR}/intern/atomic"
)

target_compile_features(project_grease_blender_gp PUBLIC cxx_std_17)

target_compile_definitions(project_grease_blender_gp PRIVATE
  WITH_OPENGL
)

# This target intentionally adds only the legacy GP draw-cache and GP
# engine translation units at this stage. Their dependent Blender modules
# are added incrementally from compiler/linker evidence; the Blender
# application, UI, workspace, and unrelated draw engines remain excluded.


# Desktop link gate: proves the adapter plus actual GP draw translation units can
# be linked into a native executable before the Android EGL/GLES layer is added.
add_executable(project_grease_gp_link_test gp_backend_link_test.cpp)
target_link_libraries(project_grease_gp_link_test PRIVATE project_grease_blender_gp)
