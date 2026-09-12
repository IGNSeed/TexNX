cmake_minimum_required(VERSION 3.20)

project(TexNX VERSION 0.1.0 LANGUAGES CXX)

if(NOT NINTENDO_SWITCH)
    message(FATAL_ERROR
        "TexNX must be configured with the devkitPro Switch toolchain: "
        "-DCMAKE_TOOLCHAIN_FILE=$ENV{DEVKITPRO}/cmake/Switch.cmake")
endif()

add_executable(TexNX
    source/main.cpp
    source/App.cpp
    source/filesystem/FileSystem.cpp
)

target_include_directories(TexNX PRIVATE include)
target_compile_features(TexNX PRIVATE cxx_std_17)
set_target_properties(TexNX PROPERTIES
    CXX_EXTENSIONS OFF
)

target_compile_definitions(TexNX PRIVATE
    TEXNX_VERSION="${PROJECT_VERSION}"
)

target_compile_options(TexNX PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

nx_generate_nacp(TexNX.nacp
    NAME "TexNX"
    AUTHOR "IGNSeed"
    VERSION "${PROJECT_VERSION}"
)

nx_create_nro(TexNX
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/TexNX.nro"
    NACP "${CMAKE_CURRENT_BINARY_DIR}/TexNX.nacp"
    NOICON
)
