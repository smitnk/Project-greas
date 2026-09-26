# Project Grease native backend
cmake_minimum_required(VERSION 3.24)
project(project_grease_blender_gp LANGUAGES C CXX)

set(BLENDER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/blender"
    CACHE PATH "Pinned upstream Blender source tree")

if(NOT EXISTS "${BLENDER_SOURCE_DIR}/source/blender/draw/engines/gpencil/gpencil_engine.c")
  message(FATAL_ERROR
    "Blender source is not imported. Run tools/import_blender_gp.sh first.")
endif()

if(TARGET bf_draw)
  add_library(project_grease_blender_gp STATIC project_grease_gp_backend.cpp)

  add_library(project_grease_gp_bridge STATIC project_grease_gp_bridge.cpp)

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
    "${BLENDER_SOURCE_DIR}/intern/ghost"
  )

  target_compile_features(project_grease_blender_gp PUBLIC cxx_std_17)
  target_compile_definitions(project_grease_blender_gp PRIVATE WITH_OPENGL)

  target_link_libraries(project_grease_gp_bridge PRIVATE project_grease_blender_gp)
  target_include_directories(project_grease_gp_bridge PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")
  target_compile_features(project_grease_gp_bridge PUBLIC cxx_std_17)

  if(TARGET bf_editor_gizmo_library AND TARGET bf_gpu)
    target_link_libraries(bf_editor_gizmo_library PUBLIC bf_gpu)
  endif()

  target_link_libraries(project_grease_blender_gp PRIVATE bf_draw bf_gpu bf_intern_ghost)

  add_executable(project_grease_gp_bridge_simulation gp_bridge_simulation_test.cpp)
  target_link_libraries(project_grease_gp_bridge_simulation PRIVATE
    project_grease_gp_bridge
    project_grease_blender_gp
    bf_draw
    bf_gpu
    bf_intern_ghost
  )

  add_executable(project_grease_gp_link_test gp_backend_link_test.cpp)
  target_link_libraries(project_grease_gp_link_test PRIVATE
    project_grease_blender_gp
    bf_draw
    bf_gpu
    bf_intern_ghost
  )

  # Temporary symbol-resolution diagnostic for native legacy GP close testing.
  target_link_options(project_grease_gp_link_test PRIVATE
    "-Wl,--trace-symbol=BKE_gpencil_stroke_close"
  )
else()
  add_library(project_grease_blender_gp STATIC
    project_grease_gp_backend.cpp
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
    "${BLENDER_SOURCE_DIR}/intern/ghost"
  )

  target_compile_features(project_grease_blender_gp PUBLIC cxx_std_17)
  target_compile_definitions(project_grease_blender_gp PRIVATE WITH_OPENGL)
endif()

# The real symbol-resolution and GP cache test lives in Blender's own CMake graph.
# The standalone configure intentionally ends at the static adapter archive.
