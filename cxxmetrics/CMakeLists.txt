
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
		bits/atomic/ringbuf.hpp
		bits/distribution.hpp
		bits/histogram.hpp
		bits/meta.hpp
		bits/time.hpp
		bits/value.hpp
		internal/atomic_lifo.hpp
		metrics/counter.hpp
		metrics/doc.hpp
		metrics/metric_handle.hpp
		state/atomic_counter.hpp
		state/atomic_simple_distribution.hpp
		state/atomic_sliding_window_distribution.hpp
		state/atomic_uniform_distribution.hpp
		state/basic_counter.hpp
		state/counter.hpp
		state/distribution.hpp
		state/doc.hpp
		state/source.hpp
        ewma.hpp
        gauge.hpp
        histogram.hpp
        meter.hpp
        metric.hpp
        metric_path.hpp
        metric_value.hpp
        metrics_registry.hpp
        pool.hpp
        publisher.hpp
        publisher_impl.hpp
        tag_collection.hpp
		timer.hpp
)

set(SOURCES
		bits/value.cpp)

add_library(cxxmetrics ${HEADERS} ${SOURCES})
target_include_directories(cxxmetrics INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../")
target_sources_local(cxxmetrics INTERFACE ${HEADERS})
target_link_libraries(cxxmetrics INTERFACE atomic fmt::fmt)

install(FILES ${HEADERS} DESTINATION "include/cxxmetrics")