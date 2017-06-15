macro(parse_arguments)
    set(options BASIC_SETUP CMAKE_TARGETS)
    set(oneValueArgs BUILD CONANFILE)
    set(multiValueArgs REQUIRES OPTIONS IMPORTS CONAN_COMMAND)
    cmake_parse_arguments(ARGUMENTS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )
endmacro()

function(_conan_install conanfile)
    # Calls "conan install"
    # Argument BUILD is equivalant to --build={missing, PkgName,...}
    # Argument CONAN_COMMAND, to specify the conan path, e.g. in case of running from source
    # cmake does not identify conan as command, even if it is +x and it is in the path
    parse_arguments(${ARGN})

    if(ARGUMENTS_BUILD)
        set(CONAN_BUILD_POLICY "--build=${ARGUMENTS_BUILD}")
    else()
        set(CONAN_BUILD_POLICY "--build=outdated")
    endif()
    if(ARGUMENTS_CONAN_COMMAND)
        set(conan_command ${ARGUMENTS_CONAN_COMMAND})
    else()
        set(conan_command conan)
    endif()

    if(NOT IS_ABSOLUTE "${conanfile}")
        set(conanfile "${CMAKE_CURRENT_SOURCE_DIR}/${conanfile}")
    endif()

    set(conan_args install --generator cmake -f "${conanfile}" ${CONAN_BUILD_POLICY})

    string (REPLACE ";" " " _conan_args "${conan_args}")
    set(conan_cmake_file "${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake")

    if("${conanfile}" IS_NEWER_THAN "${conan_cmake_file}")
        message(STATUS "Conan executing: ${conan_command} ${_conan_args}")

        execute_process(COMMAND "${conan_command}" ${conan_args}
                RESULT_VARIABLE return_code
                WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

        if(NOT "${return_code}" STREQUAL "0")
            message(FATAL_ERROR "Conan install failed='${return_code}'")
        endif()
    endif()

    configure_file("${conanfile}" "${CMAKE_CURRENT_BINARY_DIR}/_conan.txt")
    add_custom_command(
            OUTPUT "${conan_cmake_file}"
            COMMAND "${conan_command}" ${_conan_args}
            DEPENDS "${conanfile}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            COMMENT "Conan executing: ${conan_command} ${_conan_args}"
    )

    string(RANDOM LENGTH 12 target_name)
    add_custom_target("conan${target_name}" ALL DEPENDS "${conan_cmake_file}")

endfunction()

macro(conan_include conanfile)
    _conan_install("${conanfile}" ${ARGN})
    include("${CMAKE_CURRENT_BINARY_DIR}/conanbuildinfo.cmake")
    conan_basic_setup(TARGETS)
endmacro()