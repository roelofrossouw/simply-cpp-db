include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(FetchContent)
include(CMakeParseArguments)

set(SC_VERSION_FILE "VERSION.txt")
set(SC_VERSION_DEFAULT "1.0.0")

function(parse_sc_version text output)
    string(REGEX REPLACE "^[vV]" "" candidate "${text}")
    # CMake regexes have no {m,n} repetition, so the optional parts are spelled out.
    if (candidate MATCHES "^[0-9]+(\\.[0-9]+)?(\\.[0-9]+)?(\\.[0-9]+)?$")
        set(${output} "${candidate}" PARENT_SCOPE)
    else ()
        set(${output} "" PARENT_SCOPE)
    endif ()
endfunction()

function(get_sc_version)
    set(version_file "${CMAKE_CURRENT_SOURCE_DIR}/${SC_VERSION_FILE}")
    set(version "")

    find_package(Git QUIET)
    if (Git_FOUND)
        execute_process(
                COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE git_tag
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE git_result
        )

        if (git_result EQUAL 0)
            parse_sc_version("${git_tag}" version)
            if ("${version}" STREQUAL "")
                message(WARNING "Ignoring git tag '${git_tag}': not a version number")
            endif ()
        endif ()
    endif ()

    if (NOT "${version}" STREQUAL "")
        message(STATUS "Version ${version} from git tag '${git_tag}'")
        write_sc_version_file("${version}" "${version_file}")
    else ()
        read_sc_version_file("${version_file}" version)
    endif ()

    if ("${version}" STREQUAL "")
        set(version "${SC_VERSION_DEFAULT}")
        message(WARNING "No usable git tag and no ${SC_VERSION_FILE}, defaulting to version ${version}."
                " Configure once in a tagged checkout to create the file.")
    endif ()

    set(SC_VERSION "${version}" PARENT_SCOPE)
endfunction()

function(write_sc_version_file version version_file)
    set(staged "${CMAKE_CURRENT_BINARY_DIR}/${SC_VERSION_FILE}")
    file(WRITE "${staged}" "${version}\n")
    file(COPY_FILE "${staged}" "${version_file}" ONLY_IF_DIFFERENT RESULT copy_error)
    if (copy_error)
        message(WARNING "Could not update ${version_file}: ${copy_error}."
                " A build without git will fall back to whatever it already holds.")
    endif ()
endfunction()

function(read_sc_version_file version_file output)
    set(${output} "" PARENT_SCOPE)
    if (NOT EXISTS "${version_file}")
        return()
    endif ()

    file(READ "${version_file}" contents)
    string(STRIP "${contents}" contents)
    parse_sc_version("${contents}" version)
    if ("${version}" STREQUAL "")
        message(WARNING "Ignoring ${version_file}: '${contents}' is not a version number")
        return()
    endif ()

    message(STATUS "Version ${version} from ${SC_VERSION_FILE} (no usable git tag)")
    set(${output} "${version}" PARENT_SCOPE)
endfunction()

# find_or_install_package(<package> <apt name> <brew name>)
#
# Finds a dependency, installing it through the system package manager first if it is
# missing. Anything the caller needs to set up beforehand - PostgreSQL_ROOT and the
# like - should be set before the call.
function(find_or_install_package package apt_name brew_name)
    message(STATUS "Detecting ${package}")
    find_package(${package} QUIET)
    if (${package}_FOUND)
        message(STATUS "${package} found - ${${package}_VERSION}")
        return()
    endif ()

    if (UNIX AND EXISTS "/usr/bin/apt")
        message(STATUS "${package} not found, attempting apt installation...")
        execute_process(COMMAND sudo apt -y install ${apt_name} RESULT_VARIABLE INSTALL_RESULT)
    endif ()
    if (APPLE)
        message(STATUS "${package} not found, attempting brew installation...")
        execute_process(COMMAND brew install ${brew_name} RESULT_VARIABLE INSTALL_RESULT)
    endif ()

    # Re-check the package that was asked for. This used to look for CURL whatever the
    # argument was, which happened to suit the one caller and would have masked any other.
    find_package(${package} QUIET)
    if (NOT ${package}_FOUND)
        message(FATAL_ERROR "Failed to install or locate ${package} (install result=${INSTALL_RESULT})")
    endif ()
    message(STATUS "${package} found - ${${package}_VERSION}")
endfunction()

function(add_sc_object object)
    # sc-obj-, not sc-: sc-<module> belongs to the consolidated library, and a module
    # with a source file named after itself would collide with it.
    set(object_name "sc-obj-${object}")
    set(source_file "src/${object}.cpp")
    set(header_file "include/${object}.h")

    set(options)
    set(one_value_args)
    set(multi_value_args LINK_LIBRARIES PUBLIC_LINK_LIBRARIES)
    cmake_parse_arguments(SC_OBJECT "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
    set(INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    add_library(${object_name} OBJECT ${source_file} ${header_file})
    set_target_properties(${object_name} PROPERTIES EXCLUDE_FROM_ALL ON)
    target_include_directories(${object_name} PRIVATE ${INCLUDE})

    if (SC_OBJECT_LINK_LIBRARIES)
        target_link_libraries(${object_name} PRIVATE ${SC_OBJECT_LINK_LIBRARIES})
    endif ()

    list(APPEND SOURCE_OBJECTS $<TARGET_OBJECTS:${object_name}>)
    set(SOURCE_OBJECTS "${SOURCE_OBJECTS}" PARENT_SCOPE)

    if (SC_OBJECT_PUBLIC_LINK_LIBRARIES)
        list(APPEND SOURCE_LINK_LIBRARIES ${SC_OBJECT_PUBLIC_LINK_LIBRARIES})
        set(SOURCE_LINK_LIBRARIES "${SOURCE_LINK_LIBRARIES}" PARENT_SCOPE)
    endif ()
endfunction()

# sc_module_name(<output>)
#
# The name a module's library and package go by, sc-<SC_MODULE>: sc-core, sc-db. Set
# SC_MODULE at the top of the module's CMakeLists.txt, before these are called.
function(sc_module_name output)
    if (NOT SC_MODULE)
        message(FATAL_ERROR "SC_MODULE is not set."
                " Set it before add_sc_libraries()/install_sc_module(), or pass NAME.")
    endif ()
    set(${output} "sc-${SC_MODULE}" PARENT_SCOPE)
endfunction()

# add_sc_libraries([NAME <name>] [DESCRIPTION <text>] [VERSION <version>] [INCLUDE_DIR <dir>])
#
# Builds the pair of libraries every module exports from the objects collected in
# SOURCE_OBJECTS: a static <name> and a shared <name>-shared, aliased sc::<name> and
# sc::<name>-shared. NAME defaults to sc-<SC_MODULE>. Both are appended to
# SOURCE_LIBRARIES for install_sc_module().
#
# The two are siblings built from the same objects. Neither links the other: doing so
# put the static library on the link line of anyone who chose the shared one, which is
# the same code twice.
function(add_sc_libraries)
    set(one_value_args NAME DESCRIPTION VERSION INCLUDE_DIR)
    cmake_parse_arguments(ARG "" "${one_value_args}" "" ${ARGN})

    set(name "${ARG_NAME}")
    if (NOT name)
        sc_module_name(name)
    endif ()
    if (NOT ARG_VERSION)
        set(ARG_VERSION "${SC_VERSION}")
    endif ()
    if (NOT ARG_INCLUDE_DIR)
        set(ARG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
    endif ()
    if (NOT ARG_DESCRIPTION)
        set(ARG_DESCRIPTION "simply-cpp ${name}")
    endif ()
    string(REGEX MATCH "^[0-9]+" version_major "${ARG_VERSION}")

    # Captured before the new targets are added, so they link the module's dependencies
    # and not each other.
    set(dependencies ${SOURCE_LIBRARIES} ${SOURCE_LINK_LIBRARIES})

    foreach (kind STATIC SHARED)
        set(target "${name}")
        set(what "consolidated static library")
        if (kind STREQUAL "SHARED")
            set(target "${name}-shared")
            set(what "common shared library")
        endif ()

        add_library(${target} ${kind} ${SOURCE_OBJECTS})
        add_library(sc::${target} ALIAS ${target})
        set_target_properties(${target} PROPERTIES
                VERSION ${ARG_VERSION}
                SOVERSION ${version_major}
                DESCRIPTION "${ARG_DESCRIPTION} ${what}")
        target_include_directories(${target} PUBLIC
                $<BUILD_INTERFACE:${ARG_INCLUDE_DIR}>
                $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
        target_link_libraries(${target} PUBLIC ${dependencies})
        list(APPEND SOURCE_LIBRARIES ${target})
    endforeach ()

    set(SOURCE_LIBRARIES "${SOURCE_LIBRARIES}" PARENT_SCOPE)
endfunction()

# install_sc_module([NAME <name>] [VERSION <version>] [CONFIG_TEMPLATE <file>] [PATH_VARS <var>...])
#
# Installs everything in SOURCE_LIBRARIES plus the module's headers, and writes the
# <name>Config.cmake / <name>ConfigVersion.cmake a consumer finds with
# find_package(<name>). NAME defaults to sc-<SC_MODULE>, and CONFIG_TEMPLATE to
# cmake/<name>Config.cmake.in.
function(install_sc_module)
    set(one_value_args NAME VERSION CONFIG_TEMPLATE)
    set(multi_value_args PATH_VARS)
    cmake_parse_arguments(ARG "" "${one_value_args}" "${multi_value_args}" ${ARGN})

    set(name "${ARG_NAME}")
    if (NOT name)
        sc_module_name(name)
    endif ()
    if (NOT ARG_VERSION)
        set(ARG_VERSION "${SC_VERSION}")
    endif ()
    if (NOT ARG_CONFIG_TEMPLATE)
        set(ARG_CONFIG_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${name}Config.cmake.in")
    endif ()
    if (NOT EXISTS "${ARG_CONFIG_TEMPLATE}")
        message(FATAL_ERROR "install_sc_module(${name}): no package config template at ${ARG_CONFIG_TEMPLATE}")
    endif ()

    set(package_destination "${CMAKE_INSTALL_LIBDIR}/cmake/${name}")

    install(TARGETS ${SOURCE_LIBRARIES} EXPORT ${name}Targets)
    # Finder litters include/ and install(DIRECTORY) copies whatever it finds.
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            PATTERN ".DS_Store" EXCLUDE)
    install(EXPORT ${name}Targets FILE ${name}Targets.cmake NAMESPACE sc:: DESTINATION ${package_destination})

    configure_package_config_file(${ARG_CONFIG_TEMPLATE} ${CMAKE_CURRENT_BINARY_DIR}/${name}Config.cmake
            INSTALL_DESTINATION ${package_destination}
            PATH_VARS ${ARG_PATH_VARS})
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${name}ConfigVersion.cmake
            VERSION ${ARG_VERSION} COMPATIBILITY SameMajorVersion)
    # ${CMAKE_INSTALL_LIBDIR}, not a literal lib: the export above already uses it, and
    # the two have to agree on a distribution that uses lib64.
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${name}Config.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/${name}ConfigVersion.cmake
            DESTINATION ${package_destination})
endfunction()

# add_sc_test(<name> [TIMEOUT <seconds>] [LABELS <label>...] [LINK_LIBRARIES <lib>...])
#
# Builds <name>.cpp in the current directory into test-<name> and registers it with
# ctest. Tests report every failed check on stderr and exit non-zero (see sc_test.h).
#
# LINK_LIBRARIES defaults to ${SC_TEST_LINK_LIBRARIES}, so a module sets that once in
# its tests/CMakeLists.txt rather than repeating the library on every call.
function(add_sc_test name)
    # Prefix ARG, not SC_TEST: cmake_parse_arguments clears the variables it owns, and
    # SC_TEST_LINK_LIBRARIES is the directory-level default we want to fall back to.
    set(options)
    set(one_value_args TIMEOUT)
    set(multi_value_args LABELS LINK_LIBRARIES)
    cmake_parse_arguments(ARG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if (NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT 120)
    endif ()

    if (NOT ARG_LINK_LIBRARIES)
        set(ARG_LINK_LIBRARIES ${SC_TEST_LINK_LIBRARIES})
    endif ()
    if (NOT ARG_LINK_LIBRARIES)
        message(FATAL_ERROR "add_sc_test(${name}): nothing to link against."
                " Pass LINK_LIBRARIES, or set SC_TEST_LINK_LIBRARIES for the directory.")
    endif ()

    set(target "test-${name}")
    add_executable(${target} "${name}.cpp")
    target_link_libraries(${target} PRIVATE ${ARG_LINK_LIBRARIES})
    # The test's own directory first, then sc_test.h wherever the sc package put it.
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${SC_TEST_INCLUDE_DIR})

    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            TIMEOUT ${ARG_TIMEOUT}
            LABELS "${ARG_LABELS}")
endfunction()