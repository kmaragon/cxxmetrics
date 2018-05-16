include(Conan)
include(Coverage)
conan_include("conanfile.txt")

set(SOURCES
        internal/atomic_lifo_test.cpp
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
target_link_libraries(cxxmetrics_test CONAN_PKG::googletest cxxmetrics -pthread)

add_coverage_run(cxxmetrics_coverage cxxmetrics_test)
