cmake_minimum_required(VERSION 3.22)
project(simply-cpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

enable_testing()

# Function to add tests...
function(add_sc_test name)
    add_executable("test-${name}" "${name}.cpp")
    target_link_libraries("test-${name}" PRIVATE sc-postgres)
    add_test(NAME "test-${name}" COMMAND "test-${name}")
endfunction()

## TESTS ##
add_sc_test(postgres)


