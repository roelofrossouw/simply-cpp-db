# Locates the shared simply-cpp build helpers - get_sc_version(), add_sc_object(),
# add_sc_test(), find_or_install_package() - and includes them, so a module does not
# have to keep its own fork of them.
#
# Include this from a module's top level CMakeLists.txt before calling any of them:
#     include(cmake/sc_bootstrap.cmake)
#
# The helpers are looked for in this order:
#   1. An installed sc package. The live copy, on a machine that has core installed.
#   2. SimplyCppFunctions.cmake next to this file. This is what a machine with no sc
#      installed and no network uses, so it belongs in the repository.
#   3. The sc git repository, through FetchContent. Only reached on a machine that has
#      neither of the above, and only for long enough to create 2.
#
# Whatever is found is copied to 2 when that file is missing, or when configured with
# -DSC_UPDATE_HELPERS=ON. So the first configure on a fresh machine fetches, every
# configure after that is offline, and the copy is refreshed on request rather than
# silently. sc_test.h is cached into tests/ the same way, where add_sc_test() already
# looks for it.
#
# This file keeps itself in step too: it is cached from the same source as the helpers,
# so a copy that has fallen behind is reported, and -DSC_UPDATE_HELPERS=ON replaces it
# along with the rest. It is the only file a new module has to start with, and one line
# puts it there:
#
#     curl -O --create-dirs --output-dir cmake \
#         https://raw.githubusercontent.com/roelofrossouw/simply-cpp/main/cmake/sc_bootstrap.cmake
#
# or, on a machine with sc installed, copy it out of <prefix>/lib/cmake/sc/. After that
# `include(cmake/sc_bootstrap.cmake)` is the whole of a module's setup.
#
# Point SC_HELPERS_REPOSITORY and SC_HELPERS_TAG somewhere else to fetch from a fork
# or a pinned revision.

set(SC_HELPERS_REPOSITORY "https://github.com/roelofrossouw/simply-cpp.git"
        CACHE STRING "Where to fetch the simply-cpp build helpers from")
set(SC_HELPERS_TAG "main"
        CACHE STRING "Which revision of the simply-cpp build helpers to fetch")
option(SC_UPDATE_HELPERS "Refresh the copies of SimplyCppFunctions.cmake and sc_test.h" OFF)

set(sc_helpers_cached "${CMAKE_CURRENT_LIST_DIR}/SimplyCppFunctions.cmake")
set(sc_test_header_cached "${CMAKE_CURRENT_SOURCE_DIR}/tests/sc_test.h")
set(sc_bootstrap_cached "${CMAKE_CURRENT_LIST_FILE}") # this file, kept in step with the rest
set(sc_helpers_source "")     # where to copy the helpers from, empty when already cached
set(sc_test_header_source "")
set(sc_bootstrap_source "")
set(sc_helpers_origin "")

# 1. An installed sc package. Its config includes the helpers itself, so the functions
#    exist afterwards; sc_DIR is where the copyable file sits.
find_package(sc QUIET)
if (COMMAND get_sc_version)
    set(sc_helpers_origin "the sc package at ${sc_DIR}")
    if (EXISTS "${sc_DIR}/SimplyCppFunctions.cmake")
        set(sc_helpers_source "${sc_DIR}/SimplyCppFunctions.cmake")
    endif ()
    if (SC_TEST_INCLUDE_DIR AND EXISTS "${SC_TEST_INCLUDE_DIR}/sc_test.h")
        set(sc_test_header_source "${SC_TEST_INCLUDE_DIR}/sc_test.h")
    endif ()
    if (EXISTS "${sc_DIR}/sc_bootstrap.cmake")
        set(sc_bootstrap_source "${sc_DIR}/sc_bootstrap.cmake")
    endif ()
endif ()

# 2. The copy in this directory.
if (NOT COMMAND get_sc_version AND EXISTS "${sc_helpers_cached}")
    include("${sc_helpers_cached}")
    set(sc_helpers_origin "${sc_helpers_cached}")
endif ()

# 3. The repository. Reaching here means there is nothing else to fall back on, so a
#    failure to fetch is a real error rather than something to paper over.
if (NOT COMMAND get_sc_version)
    message(STATUS "No sc package and no cached helpers, fetching from ${SC_HELPERS_REPOSITORY}")
    include(FetchContent)
    FetchContent_Declare(sc_helpers
            GIT_REPOSITORY ${SC_HELPERS_REPOSITORY}
            GIT_TAG ${SC_HELPERS_TAG}
            GIT_SHALLOW TRUE
            # Nothing here is built. Naming a subdirectory that does not exist tells
            # FetchContent to populate the source and stop, instead of configuring
            # the whole of core as a subproject.
            SOURCE_SUBDIR sc-helpers-are-not-built)
    FetchContent_MakeAvailable(sc_helpers)

    set(sc_helpers_source "${sc_helpers_SOURCE_DIR}/cmake/SimplyCppFunctions.cmake")
    set(sc_test_header_source "${sc_helpers_SOURCE_DIR}/tests/sc_test.h")
    set(sc_bootstrap_source "${sc_helpers_SOURCE_DIR}/cmake/sc_bootstrap.cmake")
    set(sc_helpers_origin "${SC_HELPERS_REPOSITORY}@${SC_HELPERS_TAG}")
    include("${sc_helpers_source}")
endif ()

if (NOT COMMAND get_sc_version)
    message(FATAL_ERROR "Found simply-cpp build helpers at ${sc_helpers_origin} but they do not"
            " define get_sc_version(). They predate it - update the copy with"
            " -DSC_UPDATE_HELPERS=ON, or reinstall simply-cpp core.")
endif ()

message(STATUS "simply-cpp build helpers from ${sc_helpers_origin}")

# Cache what was found, so the next configure - and a machine with neither sc nor a
# network - does not have to look for it again.
function(sc_cache_helper source destination what)
    if (NOT source OR NOT EXISTS "${source}")
        return()
    endif ()
    get_filename_component(destination_dir "${destination}" DIRECTORY)
    if (NOT IS_DIRECTORY "${destination_dir}")
        return()
    endif ()

    # Resolving the helpers out of the very directory being cached into, which is what
    # core itself would do, must not copy a file over itself.
    get_filename_component(source_path "${source}" REALPATH)
    get_filename_component(destination_path "${destination}" ABSOLUTE)
    if (source_path STREQUAL destination_path)
        return()
    endif ()

    if (EXISTS "${destination}")
        file(SHA256 "${source}" source_hash)
        file(SHA256 "${destination}" destination_hash)
        if (source_hash STREQUAL destination_hash)
            return() # already in step
        endif ()
        if (NOT SC_UPDATE_HELPERS)
            # Reported rather than replaced: a module may be pinned deliberately, and a
            # file rewriting itself mid configure is not something to do unasked.
            message(STATUS "${what} differs from the copy in ${sc_helpers_origin}"
                    " - refresh with -DSC_UPDATE_HELPERS=ON")
            set(sc_helpers_drifted TRUE PARENT_SCOPE)
            return()
        endif ()
    endif ()

    file(COPY_FILE "${source}" "${destination}" ONLY_IF_DIFFERENT RESULT copy_error)
    if (copy_error)
        message(WARNING "Could not cache ${what} to ${destination}: ${copy_error}")
    else ()
        message(STATUS "Cached ${what} to ${destination} - commit it so a build without"
                " sc installed and without a network still works")
    endif ()
endfunction()

set(sc_helpers_drifted FALSE)
sc_cache_helper("${sc_helpers_source}" "${sc_helpers_cached}" "SimplyCppFunctions.cmake")
sc_cache_helper("${sc_test_header_source}" "${sc_test_header_cached}" "sc_test.h")
sc_cache_helper("${sc_bootstrap_source}" "${sc_bootstrap_cached}" "sc_bootstrap.cmake")

if (sc_helpers_drifted AND SC_UPDATE_HELPERS)
    message(STATUS "The refreshed helpers take effect on the next configure")
endif ()
