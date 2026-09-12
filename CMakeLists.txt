cmake_minimum_required(VERSION 3.20)

project(TexNX VERSION 0.3.0 LANGUAGES C CXX)

if(NOT NINTENDO_SWITCH)
    message(FATAL_ERROR
        "TexNX must be configured with the devkitPro Switch toolchain: "
        "-DCMAKE_TOOLCHAIN_FILE=$ENV{DEVKITPRO}/cmake/Switch.cmake")
endif()

set(TEXNX_BOREALIS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/borealis")
if(NOT EXISTS "${TEXNX_BOREALIS_DIR}/library/CMakeLists.txt")
    message(FATAL_ERROR
        "The Borealis submodule is missing. Run: "
        "git submodule update --init external/borealis")
endif()

# Switch では OpenGL/GLFW を使わず、libnx + deko3d backend のみを構成する。
set(PLATFORM_SWITCH ON)
set(BOREALIS_USE_DEKO3D ON)
set(BRLS_UNITY_BUILD OFF)
add_subdirectory("${TEXNX_BOREALIS_DIR}/library" "${CMAKE_CURRENT_BINARY_DIR}/borealis" EXCLUDE_FROM_ALL)

find_path(TEXNX_GLM_INCLUDE_DIR glm/vec2.hpp
    PATHS "$ENV{DEVKITPRO}/portlibs/switch/include"
    NO_DEFAULT_PATH
)
if(NOT TEXNX_GLM_INCLUDE_DIR)
    message(FATAL_ERROR
        "Borealis deko3d requires switch-glm. Install it with: "
        "pacman -S switch-glm")
endif()
target_include_directories(borealis SYSTEM PRIVATE "${TEXNX_GLM_INCLUDE_DIR}")
get_target_property(TEXNX_BOREALIS_PUBLIC_INCLUDES borealis INTERFACE_INCLUDE_DIRECTORIES)
set_property(TARGET borealis PROPERTY
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${TEXNX_BOREALIS_PUBLIC_INCLUDES}")

add_executable(TexNX
    source/main.cpp
    source/App.cpp
    source/config/Config.cpp
    source/filesystem/FileSystem.cpp
    source/localization/Localization.cpp
    source/platform/SwitchLifecycle.c
    source/textures/TextureRepository.cpp
    source/ui/AboutView.cpp
    source/ui/Components.cpp
    source/ui/HomeView.cpp
    source/ui/SettingsView.cpp
    source/ui/TexturesView.cpp
    source/ui/Theme.cpp
)

target_include_directories(TexNX PRIVATE include)
target_compile_features(TexNX PRIVATE cxx_std_20)
set_target_properties(TexNX PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    CXX_EXTENSIONS OFF
)

target_compile_definitions(TexNX PRIVATE
    TEXNX_VERSION="${PROJECT_VERSION}"
)

target_compile_options(TexNX PRIVATE
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wall>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wextra>
    $<$<COMPILE_LANGUAGE:C,CXX>:-Wpedantic>
)

target_link_libraries(TexNX PRIVATE borealis)

# 実機 crash の module-relative offset を同一 build の symbol へ対応付ける。
target_link_options(TexNX PRIVATE
    "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/TexNX.map"
)

set(TEXNX_ROMFS_DIR "${CMAKE_CURRENT_BINARY_DIR}/romfs")
set(TEXNX_DEFAULT_ICON_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/default/_icon.png")
if(NOT EXISTS "${TEXNX_DEFAULT_ICON_SOURCE}")
    message(FATAL_ERROR
        "TexNX default texture icon is missing: assets/default/_icon.png")
endif()

file(MAKE_DIRECTORY
    "${TEXNX_ROMFS_DIR}/assets/default"
    "${TEXNX_ROMFS_DIR}/i18n/en-US"
    "${TEXNX_ROMFS_DIR}/i18n/ja-JP"
    "${TEXNX_ROMFS_DIR}/material"
    "${TEXNX_ROMFS_DIR}/shaders"
)

configure_file("${TEXNX_DEFAULT_ICON_SOURCE}"
    "${TEXNX_ROMFS_DIR}/assets/default/_icon.png" COPYONLY)
configure_file(resources/i18n/en-US/texnx.json
    "${TEXNX_ROMFS_DIR}/i18n/en-US/texnx.json" COPYONLY)
configure_file(resources/i18n/ja-JP/texnx.json
    "${TEXNX_ROMFS_DIR}/i18n/ja-JP/texnx.json" COPYONLY)
configure_file(resources/i18n/ja-JP/hints.json
    "${TEXNX_ROMFS_DIR}/i18n/ja-JP/hints.json" COPYONLY)
configure_file("${TEXNX_BOREALIS_DIR}/resources/i18n/en-US/hints.json"
    "${TEXNX_ROMFS_DIR}/i18n/en-US/hints.json" COPYONLY)
configure_file("${TEXNX_BOREALIS_DIR}/resources/material/MaterialIcons-Regular.ttf"
    "${TEXNX_ROMFS_DIR}/material/MaterialIcons-Regular.ttf" COPYONLY)
configure_file("${TEXNX_BOREALIS_DIR}/resources/material/LICENSE.txt"
    "${TEXNX_ROMFS_DIR}/material/LICENSE.txt" COPYONLY)

set(TEXNX_SHADER_DIR "${TEXNX_BOREALIS_DIR}/library/lib/extern/nanovg/deko3d/shaders")
nx_add_shader_program(TexNXFillVertexShader
    "${TEXNX_SHADER_DIR}/fill_vsh.glsl" vert
    OUTPUT "${TEXNX_ROMFS_DIR}/shaders/fill_vsh.dksh")
nx_add_shader_program(TexNXFillFragmentShader
    "${TEXNX_SHADER_DIR}/fill_fsh.glsl" frag
    OUTPUT "${TEXNX_ROMFS_DIR}/shaders/fill_fsh.dksh")
nx_add_shader_program(TexNXFillAaFragmentShader
    "${TEXNX_SHADER_DIR}/fill_aa_fsh.glsl" frag
    OUTPUT "${TEXNX_ROMFS_DIR}/shaders/fill_aa_fsh.dksh")

add_custom_target(TexNXRomFs)
add_dependencies(TexNXRomFs
    TexNXFillVertexShader
    TexNXFillFragmentShader
    TexNXFillAaFragmentShader
)
add_dependencies(TexNX TexNXRomFs)

nx_generate_nacp(TexNX.nacp
    NAME "TexNX"
    AUTHOR "IGNSeed"
    VERSION "${PROJECT_VERSION}"
)

nx_create_nro(TexNX
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/TexNX.nro"
    NACP "${CMAKE_CURRENT_BINARY_DIR}/TexNX.nacp"
    ROMFS "${TEXNX_ROMFS_DIR}"
    NOICON
)
