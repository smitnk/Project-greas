cmake_minimum_required(VERSION 3.22.1)

project(projectgrease_jni LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The Android target consumes the same minimal Blender legacy-GP closure that
# already passed the ARM64 --no-undefined native-link probe. This is deliberately
# not the Blender top-level CMake graph.
set(PROJECT_GREASE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../..")
set(BLENDER_SOURCE_DIR "${PROJECT_GREASE_ROOT}/third_party/blender")
set(GENERATED_DNA_DIR "${PROJECT_GREASE_ROOT}/build/blender-dna")
set(BLENDER_SOURCE_DIR_BLEN "${BLENDER_SOURCE_DIR}/source/blender")

if(NOT EXISTS "${BLENDER_SOURCE_DIR}/source/blender/draw/intern/draw_cache_impl_gpencil.cc")
    message(FATAL_ERROR
        "Pinned Blender source is missing. Run tools/import_blender_gp.sh before the Android build.")
endif()

if(NOT EXISTS "${GENERATED_DNA_DIR}/dna_type_offsets.h")
    message(FATAL_ERROR
        "Generated Blender DNA is missing. Run tools/android_blender_gp_generate_dna.sh before the Android build.")
endif()

set(BLENDER_INCLUDE_DIRS
    "${PROJECT_GREASE_ROOT}/native/blender_gp/android_compat"
    "${PROJECT_GREASE_ROOT}/native/blender_gp"
    "${BLENDER_SOURCE_DIR}/source/blender"
    "${BLENDER_SOURCE_DIR}/source"
    "${BLENDER_SOURCE_DIR}/source/blender/blenlib"
    "${BLENDER_SOURCE_DIR}/extern/curve_fit_nd"
    "${BLENDER_SOURCE_DIR}/source/blender/gpu"
    "${BLENDER_SOURCE_DIR}/source/blender/gpu/intern"
    "${BLENDER_SOURCE_DIR}/source/blender/draw"
    "${BLENDER_SOURCE_DIR}/source/blender/draw/intern"
    "${BLENDER_SOURCE_DIR}/source/blender/draw/engines/gpencil"
    "${BLENDER_SOURCE_DIR}/source/blender/editors/include"
    "${BLENDER_SOURCE_DIR}/source/blender/blenkernel"
    "${BLENDER_SOURCE_DIR}/source/blender/bmesh"
    "${BLENDER_SOURCE_DIR}/source/blender/makesdna"
    "${BLENDER_SOURCE_DIR}/source/blender/makesrna"
    "${BLENDER_SOURCE_DIR}/source/blender/render"
    "${BLENDER_SOURCE_DIR}/source/blender/depsgraph"
    "${BLENDER_SOURCE_DIR}/source/blender/windowmanager"
    "${BLENDER_SOURCE_DIR}/source/blender/blentranslation"
    "${BLENDER_SOURCE_DIR}/source/blender/blentranslation/intern"
    "${BLENDER_SOURCE_DIR}/source/blender/imbuf"
    "${BLENDER_SOURCE_DIR}/source/blender/blenloader"
    "${BLENDER_SOURCE_DIR}/intern/guardedalloc"
    "${BLENDER_SOURCE_DIR}/intern/atomic"
    "${BLENDER_SOURCE_DIR}/intern/clog"
    "${GENERATED_DNA_DIR}"
)

set(PROJECT_GREASE_CXX_SOURCES
    project_grease_android_jni_smoke.cpp
    project_grease_android_egl_renderer.cpp
    "${PROJECT_GREASE_ROOT}/native/blender_gp/project_grease_gp_backend.cpp"
    "${PROJECT_GREASE_ROOT}/native/blender_gp/project_grease_gp_bridge.cpp"
    "${PROJECT_GREASE_ROOT}/native/blender_gp/project_grease_gp_jni.cpp"
    "${PROJECT_GREASE_ROOT}/native/blender_gp/android_gpu_backend.cpp"
    "${PROJECT_GREASE_ROOT}/native/blender_gp/android_gp_presentation.cpp"
    "${BLENDER_SOURCE_DIR}/source/blender/draw/intern/draw_cache_impl_gpencil.cc"
    "${BLENDER_SOURCE_DIR}/source/blender/blenkernel/intern/gpencil_geom_legacy.cc"
    "${BLENDER_SOURCE_DIR}/source/blender/blenkernel/intern/gpencil_curve_legacy.c"
    "${BLENDER_SOURCE_DIR}/source/blender/blenkernel/intern/deform.cc"
    "${BLENDER_SOURCE_DIR}/source/blender/gpu/intern/gpu_vertex_buffer.cc"
    "${BLENDER_SOURCE_DIR}/source/blender/gpu/intern/gpu_index_buffer.cc"
    "${BLENDER_SOURCE_DIR}/source/blender/gpu/intern/gpu_batch.cc"
    "${BLENDER_SOURCE_DIR}/source/blender/gpu/intern/gpu_vertex_format.cc"
    "${BLENDER_SOURCE_DIR}/source/blender/gpu/intern/gpu_capabilities.cc"
    "${BLENDER_SOURCE_DIR}/source/blender/blenlib/intern/listbase.cc"
    "${BLENDER_SOURCE_DIR}/extern/curve_fit_nd/intern/curve_fit_cubic.c"
    "${BLENDER_SOURCE_DIR}/extern/curve_fit_nd/intern/curve_fit_cubic_refit.c"
    "${BLENDER_SOURCE_DIR}/extern/curve_fit_nd/intern/curve_fit_corners_detect.c"
    "${BLENDER_SOURCE_DIR}/extern/curve_fit_nd/intern/generic_heap.c"
    "${BLENDER_SOURCE_DIR}/source/blender/blenlib/intern/math_vector.c"
    # math_matrix.c pulls the optional Eigen C API into the Android target.
    # The selected Legacy GP/cache closure does not require it.
    "${BLENDER_SOURCE_DIR}/source/blender/blenlib/intern/polyfill_2d.c"
    "${BLENDER_SOURCE_DIR}/intern/guardedalloc/intern/leak_detector.cc"
    "${BLENDER_SOURCE_DIR}/intern/guardedalloc/intern/memory_usage.cc"
    "${BLENDER_SOURCE_DIR}/source/blender/blentranslation/intern/blt_translation.c"
)

set(PROJECT_GREASE_C_SOURCES
    "${PROJECT_GREASE_ROOT}/native/blender_gp/android_legacy_runtime_compat.c"
    "${BLENDER_SOURCE_DIR}/source/blender/blenlib/intern/array_utils.c"
    "${BLENDER_SOURCE_DIR}/source/blender/blenkernel/intern/gpencil_legacy.c"
    "${BLENDER_SOURCE_DIR}/intern/guardedalloc/intern/mallocn.c"
    "${BLENDER_SOURCE_DIR}/intern/guardedalloc/intern/mallocn_guarded_impl.c"
    "${BLENDER_SOURCE_DIR}/intern/guardedalloc/intern/mallocn_lockfree_impl.c"
)

add_library(projectgrease_jni SHARED
    ${PROJECT_GREASE_CXX_SOURCES}
    ${PROJECT_GREASE_C_SOURCES}
)

target_include_directories(projectgrease_jni PRIVATE ${BLENDER_INCLUDE_DIRS})

target_compile_definitions(projectgrease_jni PRIVATE
    __ANDROID__
    NDEBUG
    WITH_OPENGL
)

target_compile_options(projectgrease_jni PRIVATE
    -fPIC
    -ffunction-sections
    -fdata-sections
    -fvisibility=hidden
)

find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(glesv2-lib GLESv2)
find_library(glesv3-lib GLESv3)

target_link_options(projectgrease_jni PRIVATE
    "-Wl,--no-undefined"
    "-Wl,--gc-sections"
)

target_link_libraries(projectgrease_jni
    PRIVATE
    ${log-lib}
    ${android-lib}
    ${egl-lib}
    ${glesv2-lib}
    ${glesv3-lib}
)

// CI verification after duplicate-symbol fix.
