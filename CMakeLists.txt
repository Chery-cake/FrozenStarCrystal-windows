# ==============================================================================
# FrozenStarCrystal-windows - Root CMakeLists.txt
# ==============================================================================
cmake_minimum_required(VERSION 4.0)

# ==============================================================================
# Bootstrap shared FSC tooling (FSCProject.cmake + friends)
# ==============================================================================
set(FETCHCONTENT_QUIET FALSE CACHE BOOL
    "Show FetchContent/CPM download progress" FORCE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FSCBootstrap.cmake")

fsc_find_tooling(_fsc_tooling_dir)
list(APPEND CMAKE_MODULE_PATH "${_fsc_tooling_dir}")
include(FSCProject)

project(FrozenStarCrystal-windows
    VERSION 0.1.0
    LANGUAGES CXX C
)

fsc_initialize_project()

# Developer overrides for local FSC package paths (optional file)
fsc_load_local_paths()

# ==============================================================================
# Window API selection (project-specific knob)
# ==============================================================================
if(DEFINED ENV{WINDOW_API})
    set(WINDOW_API "$ENV{WINDOW_API}" CACHE STRING
        "Select the window API (sdl|glfw)" FORCE)
else()
    set(WINDOW_API "sdl" CACHE STRING
        "Select the window API (sdl|glfw)")
endif()

if(NOT WINDOW_API STREQUAL "sdl" AND NOT WINDOW_API STREQUAL "glfw")
    message(FATAL_ERROR "Unsupported WINDOW_API: ${WINDOW_API}")
endif()
# NOTE: glfw is not implemented yet — this matches the current state of the repo.

# ==============================================================================
# Dependencies
# ==============================================================================
add_subdirectory(lib)

# ==============================================================================
# Sources: default src/ include/ plus the WINDOW_API-specific tree
# ==============================================================================
file(GLOB_RECURSE API_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/${WINDOW_API}/src/*.c*"
)
list(FILTER API_SOURCES EXCLUDE REGEX "\\.(cppm|cc)$")

file(GLOB_RECURSE API_PUBLIC_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/${WINDOW_API}/include/*.h*"
)

file(GLOB_RECURSE API_MODULES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/${WINDOW_API}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/${WINDOW_API}/include/*.cppm"
)

# ==============================================================================
# Build the module via the canonical entry point
# ==============================================================================
add_module(
    EXTRA_SOURCES        ${API_SOURCES}
    EXTRA_PUBLIC_HEADERS ${API_PUBLIC_HEADERS}
    EXTRA_MODULES        ${API_MODULES}
    PUBLIC_DEPS
        FrozenStarCrystal-core
        FrozenStarCrystal-graphics
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_ttf::SDL3_ttf
)

# ==============================================================================
# App identity
# ==============================================================================
if(WINDOW_API STREQUAL "sdl")
    target_compile_definitions(${PROJECT_NAME} PUBLIC WINDOW_BACKEND_SDL=1)
elseif(WINDOW_API STREQUAL "glfw")
    target_compile_definitions(${PROJECT_NAME} PUBLIC WINDOW_BACKEND_GLFW=1)
endif()

# ==============================================================================
# Assets
# ==============================================================================
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/assets")
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/assets")
endif()

if(PROJECT_IS_TOP_LEVEL AND EXISTS "${CMAKE_SOURCE_DIR}/assets")
    add_custom_target(copy_assets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "${OUTPUT_DIR}/assets"
        COMMENT "Copying assets to build directory")
    add_dependencies(${PROJECT_NAME} copy_assets)
endif()

# ==============================================================================
# Tests
# ==============================================================================
option(ENABLE_TESTS "Build tests" OFF)
option(FSC_TESTS    "Force enable tests" OFF)
if((ENABLE_TESTS AND PROJECT_IS_TOP_LEVEL) OR FSC_TESTS)
    add_subdirectory(tests)
endif()

fsc_finalize_top_level()
