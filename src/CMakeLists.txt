
include(Conan)
conan_include("conanfile.txt")

macro(target_sources_local target) # https://gitlab.kitware.com/cmake/cmake/issues/17556
	unset(_srcList)

	foreach(src ${ARGN})
		if(NOT src STREQUAL PRIVATE AND
				NOT src STREQUAL PUBLIC AND
				NOT src STREQUAL INTERFACE)
			get_filename_component(src "${src}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
		endif()
		list(APPEND _srcList ${src})
	endforeach()
	message("SOURCES: ${_srcList}")
	target_sources(${target} ${_srcList})
endmacro()

set(HEADERS
        counter.hpp
        ewma.hpp
        gauge.hpp
        meta.hpp
        metric.hpp
        pool.hpp
        skiplist.hpp
        reservoir.hpp
        ringbuf.hpp
        simple_reservoir.hpp
        sliding_window.hpp
        tag_set.hpp
        time.hpp
        uniform_reservoir.hpp
)

add_library(cxxmetrics INTERFACE)
target_include_directories(cxxmetrics INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_sources_local(cxxmetrics INTERFACE ${HEADERS})
target_link_libraries(cxxmetrics INTERFACE CONAN_PKG::ctti)

install(FILES ${HEADERS} DESTINATION "include")

install(TARGETS cxxmetrics
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)
