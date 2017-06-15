
include(Conan)
conan_include("conanfile.txt")

set(SOURCES
        counter.cpp
        ewma.cpp)
set(HEADERS
        counter.hpp
        ewma.hpp
        metric.hpp)
add_library(cxxmetrics ${SOURCES})

install(FILES ${HEADERS} DESTINATION "include")
target_link_libraries(cxxmetrics CONAN_PKG::ctti)

install(TARGETS cxxmetrics
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)