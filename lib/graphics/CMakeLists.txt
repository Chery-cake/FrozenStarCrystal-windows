# ==============================================================================
# Graphics Dependencies
# ==============================================================================
# - Vulkan - Explicit GPU API
# - VMA - GPU memory manager
# - SDL3 - Window/input/events
# - SDL_image - Image loading
# - SDL_ttf - TrueType fonts loading
# - shader-slang/slang - Runtime shader compilation
# ==============================================================================

include(FetchContent)


# ==============================================================================
# SDL3 - Window/Input/Events
# ==============================================================================
FetchContent_Declare(
    sdl3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.14
    GIT_SHALLOW TRUE
)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_TOOLS OFF CACHE BOOL "" FORCE)
set(SDL_LIBURING OFF CACHE BOOL "Disable liburing to avoid PCH/atomic issue" FORCE)
FetchContent_MakeAvailable(sdl3)

if(SDL_SHARED)
    target_compile_options(SDL3-shared PRIVATE "-include" "stdatomic.h")
else()
    target_compile_options(SDL3-static PRIVATE "-include" "stdatomic.h")
endif()

# ==============================================================================
# SDL_image - Image Loading
# ==============================================================================
FetchContent_Declare(
    sdl_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG release-3.4.4
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE FALSE
)
set(SDLIMAGE_TEST OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TOOLS OFF CACHE BOOL "" FORCE)
# Control image format support (enable or disable as needed)
set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_BMP ON CACHE BOOL "" FORCE)
set(SDLIMAGE_GIF ON CACHE BOOL "" FORCE)
set(SDLIMAGE_JPG ON CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_LBM OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_PNG ON CACHE BOOL "" FORCE)
set(SDLIMAGE_PPM ON CACHE BOOL "" FORCE)
set(SDLIMAGE_QOI OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_SVG ON CACHE BOOL "" FORCE)
set(SDLIMAGE_TGA ON CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF ON CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_XPM OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_EXIF OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(sdl_image)

# ==============================================================================
# SDL_image - Image Loading
# ==============================================================================
FetchContent_Declare(
    sdl_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG release-3.2.2
    GIT_SHALLOW TRUE
    GIT_SUBMODULES_RECURSE FALSE
)
set(SDLTTF_TEST OFF CACHE BOOL "" FORCE)
set(SDLTTF_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDLTTF_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(sdl_ttf)
