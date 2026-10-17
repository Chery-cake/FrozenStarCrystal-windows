# ==============================================================================
# Dependencies - Main Configuration
# ==============================================================================
# All third-party deps flow through CPM (loaded by fsc_initialize_project).
# ==============================================================================

CPMUsePackageLock("${CMAKE_CURRENT_SOURCE_DIR}/cmake/package-lock.cmake")

# --- FSC graphics ------------------------------------------------------------
# This is the window module's primary downstream dependency.
# It transitively brings in SDL3 / SDL_image / SDL_ttf via its own lib/.
fsc_cpm_or_local(FrozenStarCrystal-graphics
    GITHUB_REPOSITORY Chery-cake/FrozenStarCrystal-graphics
    GIT_TAG master
    DOWNLOAD_AND_ADD_SUBDIRECTORY
    OPTIONS
        "ENABLE_TESTS OFF"
        "FSC_TESTS OFF"
)

if(NOT TARGET FrozenStarCrystal-graphics)
    message(FATAL_ERROR
        "FrozenStarCrystal-graphics' CMakeLists.txt did not create a target "
        "named 'FrozenStarCrystal-graphics'. See the earlier error message.")
endif()

# ==============================================================================
# Subsystem dependencies
# ==============================================================================
add_subdirectory(windowing)

message(STATUS "Dependencies configured successfully")
