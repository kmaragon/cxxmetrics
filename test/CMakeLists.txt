cmake_minimum_required(VERSION 3.21)

project("cxxmetrics_test" CXX)
find_package(Catch2 REQUIRED)
find_package(cxxmetrics REQUIRED)

set(CMAKE_CXX_STANDARD 14)

set(SOURCES
        internal/atomic_lifo_test.cpp
        counter_test.cpp
        ewma_test.cpp
        gauge_test.cpp
        meter_test.cpp
        metrics_registry_test.cpp
        #pool_test.cpp
        publisher_tests.cpp
        reservoir_test.cpp
        ringbuf_test.cpp
        #skiplist_test.cpp
        histogram_test.cpp
        timer_test.cpp
)

set(PROMETHEUS_SOURCES
        prometheus_publish_test.cpp
)

add_executable(cxxmetrics_test ${SOURCES})
target_include_directories(cxxmetrics_test PUBLIC ${CONAN_INCLUDES})
target_link_libraries(cxxmetrics_test Catch2::Catch2 Catch2::Catch2WithMain cxxmetrics::cxxmetrics -pthread)

add_executable(cxxmetrics_prometheus_test ${PROMETHEUS_SOURCES})
target_include_directories(cxxmetrics_prometheus_test PUBLIC ${CONAN_INCLUDES})
target_link_libraries(cxxmetrics_prometheus_test Catch2::Catch2 Catch2::Catch2WithMain cxxmetrics::cxxmetrics)

enable_testing()
add_test(NAME cxxmetrics
        COMMAND cxxmetrics_test)
