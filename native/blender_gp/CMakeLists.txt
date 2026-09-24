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
)

target_include_directories(project_grease_blender_gp PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${BLENDER_SOURCE_DIR}/source/blender"
  "${BLENDER_SOURCE_DIR}/source"
  "${BLENDER_SOURCE_DIR}/intern/guardedalloc"
  "${BLENDER_SOURCE_DIR}/intern/atomic"
)

target_compile_features(project_grease_blender_gp PUBLIC cxx_std_17)

target_compile_definitions(project_grease_blender_gp PRIVATE
  WITH_OPENGL
)

# The adapter is intentionally standalone at this gate.
# Blender BLI/BKE/DNA/DEG/DRW/GPU objects are added only after the
# dependency-minimization pass proves their exact required set.
