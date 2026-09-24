# Project Grease native backend
#
# This is intentionally a dependency-discovery target, not a full Blender build.
# The first successful milestone is a native one-stroke GP render.

cmake_minimum_required(VERSION 3.22)
project(project_grease_blender_gp LANGUAGES C CXX)

set(BLENDER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/blender"
    CACHE PATH "Pinned upstream Blender source tree")

if(NOT EXISTS "${BLENDER_SOURCE_DIR}/source/blender/draw/engines/gpencil/gpencil_engine.c")
  message(FATAL_ERROR
    "Blender source is not imported. Run tools/import_blender_gp.sh first.")
endif()

# Keep the dependency boundary explicit. Do not silently link the full Blender
# executable or Android Blender application.
add_library(project_grease_blender_gp INTERFACE)

target_include_directories(project_grease_blender_gp INTERFACE
  "${BLENDER_SOURCE_DIR}/source/blender"
  "${BLENDER_SOURCE_DIR}/source"
  "${BLENDER_SOURCE_DIR}/intern/guardedalloc"
  "${BLENDER_SOURCE_DIR}/intern/atomic"
)

target_compile_definitions(project_grease_blender_gp INTERFACE
  WITH_OPENGL
)

# Actual GP source targets are added only after the dependency-minimization
# pass identifies the exact BLI/BKE/DNA/DEG/DRW/GPU object set.
