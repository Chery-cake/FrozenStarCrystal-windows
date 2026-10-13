# ==============================================================================
# Dependencies - Main Configuration
# ==============================================================================
# Manages all external dependencies using FetchContent where feasible.
# ==============================================================================

include(FetchContent)

# Set FetchContent options
set(FETCHCONTENT_QUIET OFF)
#set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

FetchContent_Declare(
  frozenStarCrystal-graphics
  GIT_REPOSITORY https://github.com/Chery-cake/FrozenStarCrystal-graphics.git
  GIT_TAG master
  #GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(frozenStarCrystal-graphics)

# ==============================================================================
# Include Subdirectory Configurations
# ==============================================================================
add_subdirectory(graphics)
#add_subdirectory(math)
#add_subdirectory(threading)
#add_subdirectory(utility)

message(STATUS "Dependencies configured successfully")
