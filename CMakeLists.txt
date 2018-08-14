cmake_minimum_required(VERSION 3.2)
project(cxxmetrics C CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CONFIGURATION_TYPES "Debug" "Release" "Coverage")
set(CMAKE_CXX_FLAGS_COVERAGE
        "-g -O0 --coverage -fprofile-arcs -ftest-coverage ${CMAKE_CXX_FLAGS}"
        CACHE STRING "Flags used by the C++ compiler during coverage builds."
        FORCE
)

if("${CMAKE_BUILD_TYPE}" STREQUAL "Coverage")
    link_libraries(gcov)
endif()

include(Conan)
conan_include("conanfile.py")

add_subdirectory(cxxmetrics)
add_subdirectory(cxxmetrics_prometheus)
add_subdirectory(test)
