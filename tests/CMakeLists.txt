# ==============================================================================
# Tests - Main Configuration
# ==============================================================================

set(MAIN_LIB ${PROJECT_NAME})

set(TEST_OUTPUT_DIR "${OUTPUT_DIR}/tests")

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/tests/assets")
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/assets")
endif()

add_custom_target(copy_assets_test ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/tests/assets
        ${TEST_OUTPUT_DIR}/assets
    COMMENT "Copying assets to build directory"
)

function(add_tests_from_folder FOLDER)
    set(multiValueArgs LIBRARY DEPENDENCY)
    cmake_parse_arguments(ARG "" "" "${multiValueArgs}" ${ARGN})

    file(GLOB test_files CONFIGURE_DEPENDS "${FOLDER}/test*.c*")
    file(GLOB helper CONFIGURE_DEPENDS "${FOLDER}/helper*.c*")

    foreach(test_file ${test_files})
        get_filename_component(TEST_EXE ${test_file} NAME_WE)
        string(REGEX REPLACE "^test_" "" base_name ${TEST_EXE})
        set(TEST_NAME "${FOLDER}_${base_name}")

        add_executable(${TEST_EXE} ${test_file})

        set_target_properties(${TEST_EXE} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${TEST_OUTPUT_DIR}"
        )

        target_sources(${TEST_EXE}
            PUBLIC
                FILE_SET CXX_MODULES FILES
                ${helper}
        )

        target_link_libraries(${TEST_EXE} PRIVATE ${ARG_LIBRARY})
        target_compile_definitions(${TEST_EXE} PRIVATE
            $<$<CONFIG:Debug>:ENGINE_DEBUG>
        )
        add_dependencies(${TEST_EXE} ${ARG_LIBRARY} ${ARG_DEPENDENCY})
        # Force assert() to remain active even in Release builds
        target_compile_options(${TEST_EXE} PRIVATE
            $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-UNDEBUG>
            $<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/UNDEBUG>
        )

        add_test(NAME ${TEST_NAME}
            COMMAND ${TEST_EXE}
            WORKING_DIRECTORY ${TEST_OUTPUT_DIR}
        )

        if(SANITIZERS)
            enable_sanitizers_test_target(${TEST_NAME} ${TEST_EXE})
        endif()
    endforeach()
endfunction()

message(STATUS "Tests configured successfully")



