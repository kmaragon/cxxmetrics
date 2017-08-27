include(Conan)
include(Coverage)
conan_include("conanfile.txt")

set(SOURCES
        counter_test.cpp
        ewma_test.cpp
        gauge_test.cpp
        meter_test.cpp
        pool_test.cpp
        reservoir_test.cpp
        ringbuf_test.cpp
        skiplist_test.cpp
)

add_executable(cxxmetrics_test ${SOURCES})
target_link_libraries(cxxmetrics_test cxxmetrics CONAN_PKG::googletest)
target_include_directories(cxxmetrics_test PUBLIC ../src)

add_coverage_run(cxxmetrics_coverage cxxmetrics_test)
