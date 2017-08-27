
include(Conan)
conan_include("conanfile.txt")

set(SOURCES
        ewma.cpp)
set(HEADERS
        counter.hpp
        ewma.hpp
        gauge.hpp
        meta.hpp
        metric.hpp
        pool.hpp
        pqskiplist.hpp
        reservoir.hpp
        ringbuf.hpp
        simple_reservoir.hpp
        skiplist.hpp
        sliding_window.hpp
        tag_set.hpp
        time.hpp
        uniform_reservoir.hpp
)

add_library(cxxmetrics ${HEADERS} ${SOURCES})

install(FILES ${HEADERS} DESTINATION "include")
target_link_libraries(cxxmetrics CONAN_PKG::ctti)

install(TARGETS cxxmetrics
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)