cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(Git QUIET)
if (Git_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE SC_DB_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE GIT_RESULT
    )
    if (GIT_RESULT EQUAL 0)
        string(REGEX REPLACE "^v" "" SC_DB_VERSION "${SC_DB_VERSION}")
    else ()
        set(SC_DB_VERSION "1.0.0")
    endif ()
else ()
    set(SC_DB_VERSION "1.0.0")
endif ()

message(STATUS "SC DB Version: ${SC_DB_VERSION}")
project(simply-cpp-db VERSION ${SC_DB_VERSION} DESCRIPTION "simply-cpp common shared library")

# Options:
option(SC_DB_BUILD_TESTS "Build simply-cpp tests" ${PROJECT_IS_TOP_LEVEL})

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

#################
#  Dependencies #
#################

## Postgres ##
if (APPLE)
    execute_process(COMMAND pg_config --libdir OUTPUT_VARIABLE PostgreSQL_ROOT OUTPUT_STRIP_TRAILING_WHITESPACE)
endif ()
find_package(PostgreSQL QUIET)
if (NOT PostgreSQL_FOUND)
    if (UNIX AND EXISTS "/usr/bin/apt")
        execute_process(COMMAND sudo apt -y install postgresql-server-dev-all RESULT_VARIABLE INSTALL_RESULT)
    elseif (APPLE)
        execute_process(COMMAND brew install -y postgres RESULT_VARIABLE INSTALL_RESULT)
    endif ()
    find_package(PostgreSQL QUIET)
    if (NOT PostgreSQL_FOUND)
        message(FATAL_ERROR "Failed to install or locate PostgreSQL (Try a couple of times, might be locking) (result=${INSTALL_RESULT})")
    endif ()
endif ()

#############
# Libraries #
#############
set(SOURCE_LIBRARIES)

set(OBJECT_NAME sc-postgres)
add_library(${OBJECT_NAME} OBJECT src/postgres.cpp include/postgres.h)
add_library(sc::sc-db ALIAS ${OBJECT_NAME})
target_include_directories(${OBJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(${OBJECT_NAME} PUBLIC PostgreSQL::PostgreSQL)
list(APPEND SOURCE_LIBRARIES ${OBJECT_NAME})


################
# Installation #
################

install(TARGETS ${SOURCE_LIBRARIES} EXPORT sc-dbTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT sc-dbTargets FILE sc-dbTargets.cmake NAMESPACE sc:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sc-db)
configure_package_config_file(cmake/sc-dbConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/sc-dbConfig.cmake INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sc-db)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/sc-dbConfigVersion.cmake VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sc-dbConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/sc-dbConfigVersion.cmake DESTINATION lib/cmake/sc-db)

###########
#  Tests  #
###########
if (SC_DB_BUILD_TESTS)
    add_subdirectory(tests sc-db-test EXCLUDE_FROM_ALL)
endif ()