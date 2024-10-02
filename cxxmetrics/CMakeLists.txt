
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
	target_sources(${target} ${_srcList})
endmacro()

set(HEADERS
		internal/atomic_lifo.hpp
        counter.hpp
        ewma.hpp
        gauge.hpp
        histogram.hpp
        meta.hpp
        meter.hpp
        metric.hpp
        metric_path.hpp
        metric_value.hpp
        metrics_registry.hpp
        pool.hpp
        publisher.hpp
        publisher_impl.hpp
        ringbuf.hpp
        simple_reservoir.hpp
        skiplist.hpp
        sliding_window.hpp
        tag_collection.hpp
        time.hpp
		timer.hpp
        uniform_reservoir.hpp
)

add_library(cxxmetrics::cxxmetrics INTERFACE IMPORTED GLOBAL)
target_include_directories(cxxmetrics::cxxmetrics INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../")
target_sources_local(cxxmetrics::cxxmetrics INTERFACE ${HEADERS})
target_link_libraries(cxxmetrics::cxxmetrics INTERFACE atomic)

install(FILES ${HEADERS} DESTINATION "include/cxxmetrics")