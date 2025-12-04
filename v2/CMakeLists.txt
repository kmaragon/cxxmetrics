
find_package()

set(HEADERS
        cxxmetrics/metrics/safe_counter.hpp
        cxxmetrics/metrics/simple_counter.hpp
        cxxmetrics/counter.hpp
        cxxmetrics/gauge.hpp
        cxxmetrics/metric.hpp
        cxxmetrics/scalar.hpp
)

set(SOURCES
        cxxmetrics/metric.cpp
)

add_library(cxxmetrics ${HEADERS} ${SOURCES})
set_target_properties(cxxmetrics PROPERTIES CXX_STANDARD 20)
set_target_properties(cxxmetrics PROPERTIES LINKER_LANGUAGE CXX)
