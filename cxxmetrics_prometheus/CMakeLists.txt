
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
		prometheus_counter.hpp
		prometheus_gauge.hpp
        prometheus_publisher.hpp
		snapshot_writer.hpp
)

add_library(cxxmetrics_prometheus INTERFACE)
target_include_directories(cxxmetrics_prometheus INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../")
target_sources_local(cxxmetrics_prometheus INTERFACE ${HEADERS})
target_link_libraries(cxxmetrics_prometheus INTERFACE cxxmetrics_prometheus)

install(FILES ${HEADERS} DESTINATION "include/cxxmetrics_prometheus")

install(TARGETS cxxmetrics_prometheus
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)
