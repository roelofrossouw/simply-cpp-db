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
    execute_process(COMMAND pg_config --libdir
            OUTPUT_VARIABLE PostgreSQL_ROOT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
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

# Static
project(sc-db VERSION ${SC_VERSION} DESCRIPTION "simply-cpp-db consolidated static library")
add_library(${PROJECT_NAME} STATIC ${SOURCE_OBJECTS})
add_library(sc::sc-db ALIAS ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(${PROJECT_NAME} PUBLIC ${SOURCE_LIBRARIES} ${SOURCE_LINK_LIBRARIES})
list(APPEND SOURCE_LIBRARIES sc-db)

# Shared
project(sc-db-shared VERSION ${SC_VERSION} DESCRIPTION "simply-cpp-db common shared library")
add_library(${PROJECT_NAME} SHARED ${SOURCE_OBJECTS})
add_library(sc::sc-db-shared ALIAS ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(${PROJECT_NAME} PUBLIC ${SOURCE_LIBRARIES} ${SOURCE_LINK_LIBRARIES})
list(APPEND SOURCE_LIBRARIES sc-db-shared)

################
# Installation #
################
install(TARGETS ${SOURCE_LIBRARIES} EXPORT sc-dbTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT sc-dbTargets FILE sc-dbTargets.cmake NAMESPACE sc:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sc-db)
configure_package_config_file(cmake/sc-dbConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/sc-dbConfig.cmake INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sc-db)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/sc-dbConfigVersion.cmake VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sc-dbConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/sc-dbConfigVersion.cmake DESTINATION lib/cmake/sc-db)

#########
# Tests #
#########
if (SC_DB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests sc-db-test)
endif ()
