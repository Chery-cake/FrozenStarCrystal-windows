# ==============================================================================
# Loaded BEFORE FSCProject.cmake. Provides:
#
#   fsc_find_tooling()      — locate (or download) the FSC tooling.
#   fsc_find_local()        — locate a local checkout of any FSC package.
#   fsc_load_local_paths()  — optional; register non-standard checkout paths.
#   fsc_cpm_or_local()      — add a package, preferring a local checkout.
#
# Must not depend on anything defined in core — it runs before we know where
# core is.
# ==============================================================================
include_guard(GLOBAL)

option(FSC_SHARE_DEP_CACHE
    "Reuse pre-fetched deps from sibling FSC checkouts" ON)

# ------------------------------------------------------------------------------
# Project search root
# ------------------------------------------------------------------------------
# Sibling FSC projects are looked up as ${FSC_PROJECTS_ROOT}/<name>. Override
# with -DFSC_PROJECTS_ROOT=... or the FSC_PROJECTS_ROOT environment variable.
# Defaults to the parent of the top-level source directory, which matches the
# usual layout:
#
#   ~/Projects/
#     FrozenStarCrystal-graphics/   ← this project
#     FrozenStarCrystal-core/       ← sibling
#     FrozenStarCrystal-audio/      ← sibling
# ------------------------------------------------------------------------------
if(NOT DEFINED FSC_PROJECTS_ROOT)
    if(DEFINED ENV{FSC_PROJECTS_ROOT})
        set(FSC_PROJECTS_ROOT "$ENV{FSC_PROJECTS_ROOT}" CACHE PATH
            "Directory containing sibling FSC projects")
    else()
        set(FSC_PROJECTS_ROOT "${CMAKE_SOURCE_DIR}/.." CACHE PATH
            "Directory containing sibling FSC projects")
    endif()
endif()


# ------------------------------------------------------------------------------
# _fsc_register_dep_cache(<checkout_dir>)   [internal]
#
# Scans <checkout_dir>/build/_deps/ for <name>-src/ directories and registers
# each one as a pre-fetched FetchContent source via FETCHCONTENT_SOURCE_DIR_<name>.
# Any package NOT found there will be downloaded normally into the current
# project's build/_deps/ when its CPMAddPackage() call runs.
#
# Skips a package if:
#   - its FETCHCONTENT_SOURCE_DIR_* is already set to a valid directory
#   - the current project already has its own copy at build/_deps/<name>-src/
# ------------------------------------------------------------------------------
function(_fsc_register_dep_cache CHECKOUT_DIR)
    if(NOT FSC_SHARE_DEP_CACHE)
        return()
    endif()

    # Track the checkout for downstream artifact reuse (e.g. slang install dir)
    get_property(_existing GLOBAL PROPERTY FSC_CHECKOUTS)
    if(NOT CHECKOUT_DIR IN_LIST _existing)
        set_property(GLOBAL APPEND PROPERTY FSC_CHECKOUTS "${CHECKOUT_DIR}")
    endif()

    set(_deps_dir "${CHECKOUT_DIR}/build/_deps")
    if(NOT IS_DIRECTORY "${_deps_dir}")
        return()
    endif()

    file(GLOB _src_dirs LIST_DIRECTORIES true "${_deps_dir}/*-src")
    foreach(_src IN LISTS _src_dirs)
        if(NOT IS_DIRECTORY "${_src}")
            continue()
        endif()

        get_filename_component(_basename "${_src}" NAME)     # e.g. _vulkan_hpp-src
        string(REGEX REPLACE "-src$" "" _name "${_basename}") # _vulkan_hpp
        string(TOUPPER "${_name}" _upper)                    # _VULKAN_HPP

        # Prefer an already-valid entry (first checkout wins; closer checkouts win)
        if(DEFINED FETCHCONTENT_SOURCE_DIR_${_upper})
            if(EXISTS "${FETCHCONTENT_SOURCE_DIR_${_upper}}/CMakeLists.txt")
                continue()
            endif()
            # Stale entry — fall through and overwrite it below.
        endif()

        # Current project's own copy always wins over a sibling's copy
        if(EXISTS "${CMAKE_BINARY_DIR}/_deps/${_basename}/CMakeLists.txt")
            continue()
        endif()

        set(FETCHCONTENT_SOURCE_DIR_${_upper} "${_src}" CACHE PATH
            "Pre-fetched source for ${_name} (from ${CHECKOUT_DIR})" FORCE)
        message(STATUS "FSC dep cache: ${_name} <- ${CHECKOUT_DIR}/build/_deps")
    endforeach()
endfunction()


# ------------------------------------------------------------------------------
# fsc_find_local(<name> <out_var>)
#
# Locates a local checkout of the given FSC package. Search order:
#   1. Explicit registration: FSC_LOCAL_<name>  (fsc_register_local)
#   2. Cache variable:        FSC_<name>_DIR
#   3. Environment variable:  $FSC_<name>_DIR
#   4. Sibling directory:     ${FSC_PROJECTS_ROOT}/<name>
#
# A directory is a valid checkout if it contains CMakeLists.txt.
#
# Sets <out_var> to the absolute path, or empty string if not found
# (in PARENT_SCOPE).
# ------------------------------------------------------------------------------
function(fsc_find_local NAME OUT_VAR)
    # 1. Explicit registration wins.
    if(DEFINED FSC_LOCAL_${NAME})
        set(${OUT_VAR} "${FSC_LOCAL_${NAME}}" PARENT_SCOPE)
        return()
    endif()

        # 2-4. Cache / env / sibling.
    set(_candidates
        "${FSC_${NAME}_DIR}"
        "$ENV{FSC_${NAME}_DIR}"
        "${FSC_PROJECTS_ROOT}/${NAME}"
    )
    foreach(_candidate IN LISTS _candidates)
        if(_candidate AND EXISTS "${_candidate}/CMakeLists.txt")
            _fsc_register_dep_cache("${_candidate}")
            set(${OUT_VAR} "${_candidate}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    set(${OUT_VAR} "" PARENT_SCOPE)
endfunction()


# ------------------------------------------------------------------------------
# fsc_find_tooling(<out_var>)
#
# Locates a directory containing FSCProject.cmake. The tooling lives in the
# FrozenStarCrystal-core repository, so this is a thin wrapper around
# fsc_find_local() that additionally verifies the tooling file exists.
#
# Falls back to downloading core's master tarball into
# ${CMAKE_BINARY_DIR}/_fsc-tooling/ when no local checkout is found.
#
# Sets <out_var> to the cmake/ subdirectory (in PARENT_SCOPE).
# ------------------------------------------------------------------------------
function(fsc_find_tooling OUT_VAR)
    fsc_find_local(FrozenStarCrystal-core _core_dir)

    if(_core_dir AND EXISTS "${_core_dir}/cmake/FSCProject.cmake")
        message(STATUS "FSC tooling: local checkout at ${_core_dir}")
        set(${OUT_VAR} "${_core_dir}/cmake" PARENT_SCOPE)
        return()
    endif()

    set(_root "${CMAKE_BINARY_DIR}/_fsc-tooling")
    set(_dir  "${_root}/cmake")

    if(NOT EXISTS "${_dir}/FSCProject.cmake")
        _fsc_download_tooling("${_root}")
    endif()

    set(${OUT_VAR} "${_dir}" PARENT_SCOPE)
endfunction()


# ------------------------------------------------------------------------------
# _fsc_download_tooling(<root_dir>)   [internal]
# ------------------------------------------------------------------------------
function(_fsc_download_tooling ROOT_DIR)
    message(STATUS "FSC tooling: no local checkout found, downloading core tarball...")

    set(_tarball     "${CMAKE_BINARY_DIR}/_fsc-tooling.tar.gz")
    set(_tmp_extract "${CMAKE_BINARY_DIR}/_fsc-tooling-extract")

    file(DOWNLOAD
        "https://github.com/Chery-cake/FrozenStarCrystal-core/archive/refs/heads/master.tar.gz"
        "${_tarball}"
        TLS_VERIFY ON
        STATUS _dl_status)
    list(GET _dl_status 0 _dl_code)
    if(NOT _dl_code EQUAL 0)
        message(FATAL_ERROR
            "Failed to download FSC tooling: ${_dl_status}\n"
            "Set FSC_CORE_DIR to a local checkout of FrozenStarCrystal-core.")
    endif()

    file(REMOVE_RECURSE "${_tmp_extract}")
    file(MAKE_DIRECTORY "${_tmp_extract}")
    file(ARCHIVE_EXTRACT INPUT "${_tarball}" DESTINATION "${_tmp_extract}")

    file(GLOB _extracted "${_tmp_extract}/*")
    list(GET _extracted 0 _extracted_root)

    file(MAKE_DIRECTORY "${ROOT_DIR}")
    file(COPY "${_extracted_root}/cmake" DESTINATION "${ROOT_DIR}")
    file(REMOVE_RECURSE "${_tmp_extract}")

    message(STATUS "FSC tooling: using fetched copy at ${ROOT_DIR}/cmake")
endfunction()

