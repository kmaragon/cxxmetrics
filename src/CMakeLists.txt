
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
        tag_set.hpp
        time.hpp
)
add_library(cxxmetrics ${SOURCES})

install(FILES ${HEADERS} DESTINATION "include")
target_link_libraries(cxxmetrics CONAN_PKG::ctti)

install(TARGETS cxxmetrics
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)