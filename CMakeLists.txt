cmake_minimum_required(VERSION 3.21)
project(cxxmetrics C CXX)

find_package(fmt REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

add_subdirectory(cxxmetrics)
add_subdirectory(cxxmetrics_prometheus)

if(NOT CONAN_EXPORTED)
    set(cxxmetrics_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cxxmetrics")
    add_subdirectory(test)
endif()

