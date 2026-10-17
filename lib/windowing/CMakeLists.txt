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
fsc_add_library(
    NAME sdl3
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.14
    TEST_TARGET SDL3::SDL3
    CPM_ARGS
        GIT_SHALLOW TRUE
        OPTIONS
            "SDL_TEST OFF" "SDL_EXAMPLES OFF" "SDL_TOOLS OFF" "SDL_LIBURING OFF"
    POST_ADD_COMMANDS
        "if(TARGET SDL3-shared)\ntarget_compile_options(SDL3-shared PRIVATE -include stdatomic.h)\nelseif(TARGET SDL3-static)\ntarget_compile_options(SDL3-static PRIVATE -include stdatomic.h)\nendif()"
)

# ==============================================================================
# SDL_image - Image Loading
# ==============================================================================
fsc_add_library(
    NAME sdl_image
    GITHUB_REPOSITORY libsdl-org/SDL_image
    GIT_TAG release-3.4.4
    TEST_TARGET SDL3_image::SDL3_image
    CPM_ARGS
        GIT_SHALLOW TRUE
        GIT_SUBMODULES_RECURSE FALSE
        OPTIONS
            "SDLIMAGE_TEST OFF" "SDLIMAGE_EXAMPLES OFF" "SDLIMAGE_TOOLS OFF"
            "SDLIMAGE_AVIF OFF" "SDLIMAGE_BMP ON" "SDLIMAGE_GIF ON"
            "SDLIMAGE_JPG ON" "SDLIMAGE_JXL OFF" "SDLIMAGE_LBM OFF"
            "SDLIMAGE_PNG ON" "SDLIMAGE_PPM ON" "SDLIMAGE_QOI OFF"
            "SDLIMAGE_SVG ON" "SDLIMAGE_TGA ON" "SDLIMAGE_TIF ON"
            "SDLIMAGE_WEBP OFF" "SDLIMAGE_XPM OFF" "SDLIMAGE_EXIF OFF"
)

# ==============================================================================
# SDL_image - Image Loading
# ==============================================================================
fsc_add_library(
    NAME sdl_ttf
    GITHUB_REPOSITORY libsdl-org/SDL_ttf
    GIT_TAG release-3.2.2
    TEST_TARGET SDL3_ttf::SDL3_ttf
    CPM_ARGS
        GIT_SHALLOW TRUE
        GIT_SUBMODULES_RECURSE FALSE
        OPTIONS
            "SDLTTF_TEST OFF" "SDLTTF_EXAMPLES OFF" "SDLTTF_TOOLS OFF"
)
