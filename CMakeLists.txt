cmake_minimum_required(VERSION 3.22)
set(SC_MODULE db)
project(simply-cpp-${SC_MODULE} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options:
option(SC_DB_BUILD_TESTS "Build simply-cpp-db tests" ${PROJECT_IS_TOP_LEVEL})

# The build helpers - get_sc_version(), add_sc_object(), add_sc_test(),
# find_or_install_package() - come from simply-cpp core so every module configures the
# same way instead of keeping its own copy. The bootstrap takes them from an installed
# sc package, the copy in cmake/, or the sc repository, whichever it finds first.
include(cmake/sc_bootstrap.cmake)

get_sc_version()
message(STATUS "SimplyCPP ${SC_MODULE} Version: ${SC_VERSION}")

################
# Dependencies #
################

## Postgres ##
# pg_config knows where a brew keg landed, find_package on its own does not. The same
# hint is in cmake/sc-dbConfig.cmake.in for consumers of the installed package.
if (APPLE AND NOT PostgreSQL_ROOT)
    execute_process(COMMAND pg_config --libdir OUTPUT_VARIABLE PostgreSQL_ROOT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif ()
find_or_install_package(PostgreSQL postgresql-server-dev-all postgresql)

###########
# Objects #
###########
set(SOURCE_OBJECTS include/postgres.h)
set(SOURCE_LIBRARIES)
set(SOURCE_LINK_LIBRARIES)

add_sc_object(postgres LINK_LIBRARIES PostgreSQL::PostgreSQL PUBLIC_LINK_LIBRARIES PostgreSQL::PostgreSQL)

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