project("cxxmetrics_test" CXX)

if (NOT CONAN_EXPORTED)
    include(Conan)
    include(Coverage)
    conan_include("conanfile.py")

    add_library(CONAN_PKG::cxxmetrics INTERFACE IMPORTED)
    target_link_libraries(CONAN_PKG::cxxmetrics INTERFACE cxxmetrics cxxmetrics_prometheus)
else()
    include("${CMAKE_BINARY_DIR}/conanbuildinfo.cmake")
    conan_basic_setup(TARGETS NO_OUTPUT_DIRS)
endif()

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
        main.cpp
)

set(PROMETHEUS_SOURCES
        prometheus_publish_test.cpp
        main.cpp
)

add_executable(cxxmetrics_test ${SOURCES})
target_include_directories(cxxmetrics_test PUBLIC ${CONAN_INCLUDES})
target_link_libraries(cxxmetrics_test CONAN_PKG::catch2 CONAN_PKG::cxxmetrics -pthread)

add_executable(cxxmetrics_prometheus_test ${PROMETHEUS_SOURCES})
target_include_directories(cxxmetrics_prometheus_test PUBLIC ${CONAN_INCLUDES})
target_link_libraries(cxxmetrics_prometheus_test CONAN_PKG::catch2 CONAN_PKG::cxxmetrics)

if (NOT CONAN_EXPORTED)
    add_coverage_run(cxxmetrics_coverage cxxmetrics_test)
    add_coverage_run(cxxmetrics_prometheus_coverage cxxmetrics_prometheus_test)
endif()

enable_testing()
add_test(NAME cxxmetrics
        COMMAND cxxmetrics_test)
