# Find all test source files matching test_*.cpp pattern
file(GLOB MERMAIDCPP_TEST_SOURCES_CPP CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp
)

# Library sources are compiled straight into the test binary (not linked as
# the static lib) so coverage instrumentation applies to them.
add_executable(test_mermaidcpp_cpp
    ${MERMAIDCPP_TEST_SOURCES_CPP}
    ${SOURCES}
    ${MERMAIDCPP_TEST_SOURCE_DIR}/utest/utest_main.cxx
)

target_include_directories(test_mermaidcpp_cpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MERMAIDCPP_TEST_SOURCE_DIR}/utest
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
target_compile_definitions(test_mermaidcpp_cpp PRIVATE
    MERMAIDCPP_TEST_DATA_DIR="${MERMAIDCPP_TEST_SOURCE_DIR}/data"
)
target_link_libraries(test_mermaidcpp_cpp PRIVATE Threads::Threads)

add_test(NAME test_mermaidcpp_cpp COMMAND $<TARGET_FILE:test_mermaidcpp_cpp>)

add_custom_target(mermaidcpp_test_cpp
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running C++ tests..."
    COMMAND $<TARGET_FILE:test_mermaidcpp_cpp>
    WORKING_DIRECTORY ${MERMAIDCPP_TEST_BINARY_DIR}
)
add_dependencies(mermaidcpp_test_cpp test_mermaidcpp_cpp)
add_dependencies(mermaidcpp_test mermaidcpp_test_cpp)

if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
    target_compile_options(test_mermaidcpp_cpp PRIVATE -O0 -g --coverage)
    target_link_options(test_mermaidcpp_cpp PRIVATE --coverage)

    add_custom_target(mermaidcpp_coverage_cpp
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERMAIDCPP_COVERAGE_CPP_BINARY_DIR}/html
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MERMAIDCPP_COVERAGE_CPP_BINARY_DIR}/html
        COMMAND ${LCOV_EXECUTABLE}
                --capture
                --base-directory ${PROJECT_SOURCE_DIR}
                --directory ${CMAKE_BINARY_DIR}
                --exclude "*/tests/*"
                --no-external
                --rc function_coverage=1
                --output-file ${MERMAIDCPP_COVERAGE_CPP_BINARY_DIR}/coverage_temp.lcov
        COMMAND ${LCOV_EXECUTABLE}
                --extract ${MERMAIDCPP_COVERAGE_CPP_BINARY_DIR}/coverage_temp.lcov
                "*/include/*" "*/src/*"
                --output-file ${MERMAIDCPP_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
        COMMAND ${CMAKE_COMMAND} -E remove ${MERMAIDCPP_COVERAGE_CPP_BINARY_DIR}/coverage_temp.lcov
        COMMAND ${GENHTML_EXECUTABLE} --quiet
                ${MERMAIDCPP_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
                --output-directory ${MERMAIDCPP_COVERAGE_CPP_BINARY_DIR}/html
                --prefix ${PROJECT_SOURCE_DIR}
                --title "mermaidcpp C++ Coverage"
                --css-file ${MERMAIDCPP_TEST_SOURCE_DIR}/genhtml.css
                --dark-mode
        COMMAND ${CMAKE_COMMAND}
                -DCOV_FILE=${MERMAIDCPP_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
                -DSOURCE_PATH=${PROJECT_SOURCE_DIR}/
                -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake
        COMMAND ${LCOV_EXECUTABLE} --list ${MERMAIDCPP_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
        WORKING_DIRECTORY ${MERMAIDCPP_TEST_BINARY_DIR}
        USES_TERMINAL
    )
    add_dependencies(mermaidcpp_coverage_cpp mermaidcpp_test_cpp)
    add_dependencies(mermaidcpp_coverage mermaidcpp_coverage_cpp)
else()
    message(WARNING "LCOV or GENHTML not found - C++ coverage target skipped "
                    "(e.g. 'sudo apt install lcov').")
endif()
