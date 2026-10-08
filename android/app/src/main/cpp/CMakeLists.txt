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
    message(FATAL_ERROR "Pinned Blender source is missing. Run tools/import_blender_gp.sh before the Android build.")
endif()
if(NOT EXISTS "${GENERATED_DNA_DIR}/dna_type_offsets.h")
    message(FATAL_ERROR "Generated Blender DNA is missing. Run tools/android_blender_gp_generate_dna.sh before the Android build.")
endif()

set(BLENDER_INCLUDE_DIRS
    "${PROJECT_GREASE_ROOT}/native/blender_gp/android_compat"
    "${PROJECT_GREASE_ROOT}/native/blender_gp"
    "${BLENDER_SOURCE_DIR}/source/blender"
    "${BLENDER_SOURCE_DIR}/source"
    "${BLENDER_SOURCE_DIR}/source/blender/blenlib"
    "${BLENDER_SOURCE_DIR}/extern/wcwidth"
    "${BLENDER_SOURCE_DIR}/extern/curve_fit_nd"
    "${BLENDER_SOURCE_DIR}/source/blender/gpu"
    "${BLENDER_SOURCE_DIR}/source/blender/gpu/intern"
    "${BLENDER_SOURCE_DIR}/source/blender/draw"
    "${BLENDER_SOURCE_DIR}/source/blender/draw/intern"
    "${BLENDER_SOURCE_DIR}/source/blender/draw/engines/gpencil"
    "${BLENDER_SOURCE_DIR}/source/blender/editors/include"
    "${BLENDER_SOURCE_DIR}/source/blender/blenkernel"
    "${BLENDER_SOURCE_DIR}/source/blender/gpencil_modifiers_legacy"
    "${BLENDER_SOURCE_DIR}/source/blender/gpencil_modifiers_legacy/intern"
    "${BLENDER_SOURCE_DIR}/source/blender/gpencil_modifiers_legacy/intern/lineart"
    "${PROJECT_GREASE_ROOT}/native/blender_gp/lineart"
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
    "${BLENDER_SOURCE_DIR}/intern/eigen"
    "${BLENDER_SOURCE_DIR}/extern/Eigen3"
    "${GENERATED_DNA_DIR}"
)

set(PROJECT_GREASE_CXX_SOURCES)
set(PROJECT_GREASE_C_SOURCES)
set(PROJECT_GREASE_SOURCE_MANIFEST
    "${PROJECT_GREASE_ROOT}/tools/android_gp_source_manifest.txt"
)
if(NOT EXISTS "${PROJECT_GREASE_SOURCE_MANIFEST}")
    message(FATAL_ERROR "Android GP source manifest is missing: ${PROJECT_GREASE_SOURCE_MANIFEST}")
endif()

file(STRINGS "${PROJECT_GREASE_SOURCE_MANIFEST}" PROJECT_GREASE_SOURCE_LINES)
foreach(source_line IN LISTS PROJECT_GREASE_SOURCE_LINES)
    string(STRIP "${source_line}" source_line)
    if(source_line STREQUAL "" OR source_line MATCHES "^#")
        continue()
    endif()
    string(REPLACE "|" ";" source_parts "${source_line}")
    list(LENGTH source_parts source_part_count)
    if(NOT source_part_count EQUAL 2)
        message(FATAL_ERROR "Malformed Android GP source manifest line: ${source_line}")
    endif()
    list(GET source_parts 0 source_lang)
    list(GET source_parts 1 source_rel)
    set(source_abs "${PROJECT_GREASE_ROOT}/${source_rel}")
    if(NOT EXISTS "${source_abs}")
        message(FATAL_ERROR "Android GP source manifest entry does not exist: ${source_rel}")
    endif()
    if(source_lang STREQUAL "CXX")
        list(APPEND PROJECT_GREASE_CXX_SOURCES "${source_abs}")
    elseif(source_lang STREQUAL "C")
        list(APPEND PROJECT_GREASE_C_SOURCES "${source_abs}")
    else()
        message(FATAL_ERROR "Unknown Android GP source language '${source_lang}' for ${source_rel}")
    endif()
endforeach()

set(PROJECT_GREASE_ALL_SOURCES ${PROJECT_GREASE_CXX_SOURCES} ${PROJECT_GREASE_C_SOURCES})
list(LENGTH PROJECT_GREASE_ALL_SOURCES PROJECT_GREASE_ALL_COUNT_BEFORE)
list(REMOVE_DUPLICATES PROJECT_GREASE_ALL_SOURCES)
list(LENGTH PROJECT_GREASE_ALL_SOURCES PROJECT_GREASE_ALL_COUNT_AFTER)
if(NOT PROJECT_GREASE_ALL_COUNT_BEFORE EQUAL PROJECT_GREASE_ALL_COUNT_AFTER)
    message(FATAL_ERROR "Duplicate source path in Android GP source manifest")
endif()

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

# Preserve Blender 3.6.23 C source language explicitly in this mixed target.
set_source_files_properties(
    ${PROJECT_GREASE_C_SOURCES}
    PROPERTIES LANGUAGE C
)
set_source_files_properties(
    "${BLENDER_SOURCE_DIR}/source/blender/blenkernel/intern/deform.cc"
    PROPERTIES LANGUAGE CXX
)
# L1: MATH_STANDALONE removed. Blender 3.6.23 uses the Eigen C API for matrix inverse.
set_source_files_properties(
    "${PROJECT_GREASE_ROOT}/native/blender_gp/android_blender_string_compat.c"
    "${PROJECT_GREASE_ROOT}/native/blender_gp/android_blender_string_utf8_compat.c"
    PROPERTIES COMPILE_OPTIONS "-Wno-sign-conversion;-Wno-error=sign-conversion;-Wno-error=implicit-int-conversion"
)

target_compile_options(projectgrease_jni PRIVATE
    -fPIC
    -ffunction-sections
    -fdata-sections
    -fvisibility=hidden
    -funsigned-char
    -fno-strict-aliasing
    -ffp-contract=off
)

if(ANDROID_ABI STREQUAL "arm64-v8a")
    target_compile_definitions(projectgrease_jni PRIVATE WITH_SSE2NEON)
    target_include_directories(projectgrease_jni SYSTEM PRIVATE
        "${PROJECT_GREASE_ROOT}/third_party/sse2neon")
endif()

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
