cmake_minimum_required(VERSION 3.10.2)

project(projectgrease_jni LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(projectgrease_jni SHARED
    project_grease_android_jni_smoke.cpp
    project_grease_android_egl_renderer.cpp
)

find_library(log-lib log)
find_library(android-lib android)
find_library(egl-lib EGL)
find_library(glesv2-lib GLESv2)

target_link_libraries(projectgrease_jni
    PRIVATE
    ${log-lib}
    ${android-lib}
    ${egl-lib}
    ${glesv2-lib}
)
