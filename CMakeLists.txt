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

## Postgres ##
# Homebrew keeps libpq keg-only, so FindPostgreSQL cannot find it in the default
# search paths. Use the exact locations from the matching pg_config.
if (APPLE AND NOT PostgreSQL_ROOT)
    execute_process(COMMAND pg_config --libdir
            OUTPUT_VARIABLE SC_DB_POSTGRESQL_LIBDIR OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET RESULT_VARIABLE SC_DB_PG_CONFIG_LIBDIR_RESULT)
    execute_process(COMMAND pg_config --includedir
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

###########
# Objects #
###########
set(SOURCE_OBJECTS include/sc_postgres.h)
set(SOURCE_LIBRARIES)
set(SOURCE_LINK_LIBRARIES)

add_sc_object(sc_postgres LINK_LIBRARIES PostgreSQL::PostgreSQL PUBLIC_LINK_LIBRARIES PostgreSQL::PostgreSQL)

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
# Binaries #
############

add_executable(sc-demo-db apps/sc-demo-db.cpp)
target_link_libraries(sc-demo-db PRIVATE sc::sc-db-shared)
install(TARGETS sc-demo-db RUNTIME DESTINATION bin COMPONENT runtime)

#############
# Packaging #
#############
package_sc_module()