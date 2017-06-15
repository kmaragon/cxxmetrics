include(Conan)
conan_include("conanfile.txt")

set(SOURCES
        counter_test.cpp
        ewma_test.cpp
        meter_test.cpp
)

add_executable(cxxmetrics_test ${SOURCES})
target_link_libraries(cxxmetrics_test cxxmetrics CONAN_PKG::googletest)
target_include_directories(cxxmetrics_test PUBLIC ../src)

