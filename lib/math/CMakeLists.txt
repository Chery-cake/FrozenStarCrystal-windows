# ==============================================================================
# Math Dependencies
# ==============================================================================
# - GLM - Graphics vector/matrix math
# - Eigen - Advanced linear algebra
# ==============================================================================

include(FetchContent)

# ==============================================================================
# GLM - OpenGL Mathematics
# ==============================================================================
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.3
    GIT_SHALLOW TRUE
)
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

# ==============================================================================
# Eigen - Linear Algebra Library
# ==============================================================================
FetchContent_Declare(
    eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 5.0.1
    GIT_SHALLOW TRUE
)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
#FetchContent_MakeAvailable(eigen)

message(STATUS "Math dependencies configured")
