cmake_minimum_required(VERSION 3.22)
set(SC_MODULE db)
project(simply-cpp-${SC_MODULE} LANGUAGES CXX)
include(cmake/sc_bootstrap.cmake)

# Options:
option(SC_DB_BUILD_TESTS "Build simply-cpp-db tests" ${PROJECT_IS_TOP_LEVEL})

get_sc_version()
message(STATUS "SimplyCPP ${SC_MODULE} Version: ${SC_VERSION}")

################
# Dependencies #
################

find_or_install_package(sc-core simply-cpp-dev simply-cpp 1.5.11)

## Postgres ##
# Homebrew keeps libpq keg-only, so FindPostgreSQL cannot find it in the default
# search paths. Use the exact locations from the matching pg_config.
if (APPLE AND NOT PostgreSQL_ROOT)
    # libpq's keg-only bin directory is only on PATH if a shell profile adds it, so also look
    # where Homebrew installs it (Apple silicon, then Intel).
    find_program(SC_DB_PG_CONFIG pg_config HINTS /opt/homebrew/opt/libpq/bin /usr/local/opt/libpq/bin NO_CACHE)
    execute_process(COMMAND "${SC_DB_PG_CONFIG}" --libdir
            OUTPUT_VARIABLE SC_DB_POSTGRESQL_LIBDIR OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET RESULT_VARIABLE SC_DB_PG_CONFIG_LIBDIR_RESULT)
    execute_process(COMMAND "${SC_DB_PG_CONFIG}" --includedir
            OUTPUT_VARIABLE SC_DB_POSTGRESQL_INCLUDEDIR OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET RESULT_VARIABLE SC_DB_PG_CONFIG_INCLUDEDIR_RESULT)

    if (SC_DB_PG_CONFIG_LIBDIR_RESULT EQUAL 0 AND NOT PostgreSQL_LIBRARY)
        find_library(SC_DB_LIBPQ_LIBRARY NAMES pq PATHS "${SC_DB_POSTGRESQL_LIBDIR}" NO_DEFAULT_PATH NO_CACHE)
        if (SC_DB_LIBPQ_LIBRARY)
            set(PostgreSQL_LIBRARY "${SC_DB_LIBPQ_LIBRARY}")
        endif ()
    endif ()
    if (SC_DB_PG_CONFIG_INCLUDEDIR_RESULT EQUAL 0 AND NOT PostgreSQL_INCLUDE_DIR
            AND EXISTS "${SC_DB_POSTGRESQL_INCLUDEDIR}/libpq-fe.h")
        set(PostgreSQL_INCLUDE_DIR "${SC_DB_POSTGRESQL_INCLUDEDIR}")
    endif ()
endif ()
find_or_install_package(PostgreSQL libpq-dev libpq)
# libpq needs only the client headers. With postgresql-server-dev installed (as for sc-pglib),
# FindPostgreSQL also adds the server's include directory, whose own postgres.h then hides
# sc-db's: #include <postgres.h> found PostgreSQL's, and sc::postgres was undeclared.
if (TARGET PostgreSQL::PostgreSQL AND PostgreSQL_INCLUDE_DIR)
    set_property(TARGET PostgreSQL::PostgreSQL PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${PostgreSQL_INCLUDE_DIR}")
endif ()

###########
# Objects #
###########
set(SOURCE_OBJECTS include/postgres.h)
set(SOURCE_LIBRARIES)
set(SOURCE_LINK_LIBRARIES)

add_sc_object(postgres LINK_LIBRARIES PostgreSQL::PostgreSQL PUBLIC_LINK_LIBRARIES sc::sc-core PostgreSQL::PostgreSQL)

######################
# Exported Libraries #
######################
add_sc_libraries(DESCRIPTION "simply-cpp-db")

################
# Installation #
################
install_sc_module()

#########
# Tests #
#########
if (SC_DB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests sc-db-test)
endif ()

############
# Examples #
############
add_sc_example(sc-db-demo)

#############
# Packaging #
#############
package_sc_module(DEV_DEPENDS "simply-cpp-dev (>= 1.5.5)" libpq-dev)