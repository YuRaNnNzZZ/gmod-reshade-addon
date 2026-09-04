cmake_minimum_required(VERSION 3.20)

project(gmod_reshade LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "gmod_reshade only supports Windows")
endif()

add_library(gmod_reshade SHARED
    src/addon.cpp
    src/d3d11_constant_buffer_state.cpp
    src/render_state_tracking.cpp
)

target_compile_features(gmod_reshade PRIVATE cxx_std_17)
target_compile_definitions(gmod_reshade PRIVATE
    ImTextureID=ImU64
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)
target_include_directories(gmod_reshade PRIVATE
    gmod-module-base/include
    reshade/deps/imgui
    reshade/include
)

if(MSVC)
    target_compile_options(gmod_reshade PRIVATE /W4 /permissive-)
endif()

set_target_properties(gmod_reshade PROPERTIES
    PREFIX ""
    OUTPUT_NAME "gmod_reshade"
)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(gmod_reshade PROPERTIES SUFFIX ".addon64")
else()
    set_target_properties(gmod_reshade PROPERTIES SUFFIX ".addon32")
endif()
