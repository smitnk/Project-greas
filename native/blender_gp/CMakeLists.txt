# Project Grease native backend
cmake_minimum_required(VERSION 3.24)
project(project_grease_blender_gp LANGUAGES C CXX)

set(BLENDER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/blender"
    CACHE PATH "Pinned upstream Blender source tree")

if(NOT EXISTS "${BLENDER_SOURCE_DIR}/source/blender/draw/engines/gpencil/gpencil_engine.c")
  message(FATAL_ERROR
    "Blender source is not imported. Run tools/import_blender_gp.sh first.")
endif()

add_library(project_grease_legacy_fill STATIC project_grease_legacy_fill.cpp)
add_library(project_grease_legacy_primitive STATIC project_grease_legacy_primitive.cpp)

target_include_directories(project_grease_legacy_primitive PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(project_grease_legacy_primitive PUBLIC cxx_std_17)
add_executable(project_grease_legacy_primitive_test project_grease_legacy_primitive_test.cpp)
target_link_libraries(project_grease_legacy_primitive_test PRIVATE project_grease_legacy_primitive)

add_executable(project_grease_legacy_input_test project_grease_legacy_input_test.cpp)
target_link_libraries(project_grease_legacy_input_test PRIVATE m)

target_include_directories(project_grease_legacy_fill PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(project_grease_legacy_fill PUBLIC cxx_std_17)
add_executable(project_grease_legacy_fill_test project_grease_legacy_fill_test.cpp)
target_link_libraries(project_grease_legacy_fill_test PRIVATE project_grease_legacy_fill)

# These are the actual Blender 3.6.23 Legacy GP editor/modifier sources from the pinned legacy directories.
# They are compiled into the focused Project Grease native closure instead
# of importing Blender's desktop application targets.
# Explicitly enumerate only the Legacy GP sources used by the focused Android
# backend. Do not glob the whole desktop editor/modifier directories: those
# directories contain RNA/window-manager/object dependencies outside this port.
set(PROJECT_GREASE_BLENDER_GP_EDITOR_SOURCES
  "${BLENDER_SOURCE_DIR}/source/blender/editors/gpencil_legacy/gpencil_legacy.c"
  "${BLENDER_SOURCE_DIR}/source/blender/editors/gpencil_legacy/gpencil_paint.c"
)
set(PROJECT_GREASE_BLENDER_GP_MODIFIER_SOURCES
  "${BLENDER_SOURCE_DIR}/source/blender/gpencil_modifiers_legacy/intern/MOD_gpencillegacy.c"
)

# Exact Blender 3.6.23 external curve-fitting implementation used by
# gpencil_curve_legacy.c. Keep the upstream algorithm and only its direct
# sources in the focused closure.
set(PROJECT_GREASE_GP_CURVE_FIT_SOURCES
  "${BLENDER_SOURCE_DIR}/extern/curve_fit_nd/intern/curve_fit_cubic.c"
  "${BLENDER_SOURCE_DIR}/extern/curve_fit_nd/intern/curve_fit_cubic_refit.c"
  "${BLENDER_SOURCE_DIR}/extern/curve_fit_nd/intern/curve_fit_corners_detect.c"
  "${BLENDER_SOURCE_DIR}/extern/curve_fit_nd/intern/generic_heap.c"
)

# BKE_gpencil_stroke_uniform_subdivide() uses Blender's real BLI heap.
# Keep this dependency minimal rather than importing the full blenlib target.
set(PROJECT_GREASE_GP_MINIMAL_BLENLIB_SOURCES
  # Exact Blender 3.6.23 Legacy GP/BLI dependency closure required by
  # project_grease_gp_backend.cpp. These are source files, not replacements:
  # gpencil_geom_legacy.cc supplies the real BKE GP geometry operations;
  # math_geom.c supplies dist_squared_to_line_segment_v2;
  # lasso_2d.c supplies BLI_lasso_is_point_inside;
  # polyfill_2d.c supplies the real fill triangulation primitive.
  "${BLENDER_SOURCE_DIR}/source/blender/blenlib/intern/BLI_heap.c"
  "${BLENDER_SOURCE_DIR}/source/blender/blenlib/intern/math_vector.c"
  "${BLENDER_SOURCE_DIR}/source/blender/blenlib/intern/math_rotation.c"
  "${BLENDER_SOURCE_DIR}/source/blender/blenlib/intern/array_utils.c"
  "${BLENDER_SOURCE_DIR}/source/blender/blenkernel/intern/deform.cc"
  "${BLENDER_SOURCE_DIR}/source/blender/blenkernel/intern/gpencil_curve_legacy.c"
  "${BLENDER_SOURCE_DIR}/source/blender/makesdna/intern/dna_utils.c"
  "${BLENDER_SOURCE_DIR}/source/blender/blenlib/intern/math_geom.c"
  "${BLENDER_SOURCE_DIR}/source/blender/blenlib/intern/lasso_2d.c"
  "${BLENDER_SOURCE_DIR}/source/blender/blenlib/intern/polyfill_2d.c"
)

set(PROJECT_GREASE_GP_ENGINE_C_SOURCES
  ${PROJECT_GREASE_GP_CURVE_FIT_SOURCES}
  ${PROJECT_GREASE_GP_MINIMAL_BLENLIB_SOURCES}
  "${BLENDER_SOURCE_DIR}/source/blender/blenkernel/intern/gpencil_curve_legacy.c"
  "${BLENDER_SOURCE_DIR}/source/blender/editors/gpencil_legacy/gpencil_utils.c"
)

set(PROJECT_GREASE_GP_ENGINE_CXX_SOURCES
  project_grease_gp_backend.cpp
  project_grease_gp_engine.cpp
  project_grease_legacy_build.cpp
  # Exact Blender 3.6.23 Legacy GP BKE geometry implementation. This closes
  # fill triangulation, point deletion, flip, subdivide and close without
  # reimplementing those algorithms in Project Grease.
  "${BLENDER_SOURCE_DIR}/source/blender/blenkernel/intern/gpencil_geom_legacy.cc"
  ${PROJECT_GREASE_BLENDER_GP_EDITOR_SOURCES}
  ${PROJECT_GREASE_BLENDER_GP_MODIFIER_SOURCES}
)

set(PROJECT_GREASE_GP_ENGINE_SOURCES
  ${PROJECT_GREASE_GP_ENGINE_CXX_SOURCES}
  ${PROJECT_GREASE_GP_ENGINE_C_SOURCES}
)

# Blender 3.6.23 Legacy GP files with .c extension must remain C sources.
# Do not let a mixed C/C++ target select clang++ for them.
set_source_files_properties(
  ${PROJECT_GREASE_GP_ENGINE_C_SOURCES}
  PROPERTIES LANGUAGE C
)
set(PROJECT_GREASE_GP_INCLUDE_DIRS
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
  "${BLENDER_SOURCE_DIR}/source/blender/editors"
  "${BLENDER_SOURCE_DIR}/source/blender/editors/include"
  "${BLENDER_SOURCE_DIR}/source/blender/gpencil_modifiers_legacy/intern"
  "${BLENDER_SOURCE_DIR}/source/blender/windowmanager"
  "${BLENDER_SOURCE_DIR}/source/blender/depsgraph"
  "${BLENDER_SOURCE_DIR}/source/blender/blentranslation"
  "${BLENDER_SOURCE_DIR}/source/blender/gpu"
  "${BLENDER_SOURCE_DIR}/source/blender/render"
  "${BLENDER_SOURCE_DIR}/extern/curve_fit_nd"
  "${BLENDER_SOURCE_DIR}/intern/guardedalloc"
  "${BLENDER_SOURCE_DIR}/intern/atomic"
  "${BLENDER_SOURCE_DIR}/intern/ghost"
)

if(TARGET bf_draw)
  add_library(project_grease_blender_gp STATIC ${PROJECT_GREASE_GP_ENGINE_SOURCES})
  add_library(project_grease_gp_bridge STATIC project_grease_gp_bridge.cpp)
  target_include_directories(project_grease_blender_gp PUBLIC ${PROJECT_GREASE_GP_INCLUDE_DIRS})
  target_compile_features(project_grease_blender_gp PUBLIC cxx_std_17)
  target_compile_definitions(project_grease_blender_gp PRIVATE WITH_OPENGL WITH_LINEART)

  target_link_libraries(project_grease_gp_bridge PRIVATE project_grease_blender_gp project_grease_legacy_fill project_grease_legacy_primitive)
  target_include_directories(project_grease_gp_bridge PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
  target_compile_features(project_grease_gp_bridge PUBLIC cxx_std_17)

  target_link_libraries(project_grease_blender_gp PRIVATE
    bf_draw bf_gpu bf_intern_ghost
    project_grease_legacy_fill project_grease_legacy_primitive
  )

  add_executable(project_grease_gp_bridge_simulation gp_bridge_simulation_test.cpp)
  target_link_libraries(project_grease_gp_bridge_simulation PRIVATE project_grease_gp_bridge project_grease_blender_gp bf_draw bf_gpu bf_intern_ghost)

  add_executable(project_grease_gp_link_test gp_backend_link_test.cpp)
  target_link_libraries(project_grease_gp_link_test PRIVATE project_grease_blender_gp bf_draw bf_gpu bf_intern_ghost)

  add_executable(project_grease_gp_external_context_test gp_backend_external_context_test.cpp)
  target_link_libraries(project_grease_gp_external_context_test PRIVATE project_grease_blender_gp bf_draw bf_gpu bf_intern_ghost)

  target_link_options(project_grease_gp_link_test PRIVATE "-Wl,--trace-symbol=BKE_gpencil_stroke_close")
else()
  add_library(project_grease_blender_gp STATIC
    ${PROJECT_GREASE_GP_ENGINE_SOURCES}
    "${BLENDER_SOURCE_DIR}/source/blender/draw/intern/draw_cache_impl_gpencil.cc"
    "${BLENDER_SOURCE_DIR}/source/blender/draw/engines/gpencil/gpencil_engine.c"
  )

  target_include_directories(project_grease_blender_gp PUBLIC ${PROJECT_GREASE_GP_INCLUDE_DIRS})
  target_compile_features(project_grease_blender_gp PUBLIC cxx_std_17)
  target_compile_definitions(project_grease_blender_gp PRIVATE WITH_OPENGL WITH_LINEART)
endif()

# Focused Legacy GP closure only. Full Blender application/editor targets,
# Python, GHOST-on-Android and unrelated render/scene systems remain excluded.
