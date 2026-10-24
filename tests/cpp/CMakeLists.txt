# pugixml: XML parser used by the SVG comparison helper (tests only).
include(FetchContent)
FetchContent_Declare(pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG v1.14
)
set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(pugixml)

# Find all test source files matching test_*.cpp pattern
file(GLOB MERMAID_TEST_SOURCES_CPP CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp
)

# Library sources are compiled straight into the test binary (not linked as
# the static lib) so coverage instrumentation applies to them.
add_executable(test_mermaid_cpp
    ${MERMAID_TEST_SOURCES_CPP}
    ${SOURCES}
    ${MERMAID_TEST_SOURCE_DIR}/utest/utest_main.cxx
)

target_link_libraries(test_mermaid_cpp PRIVATE pugixml::pugixml)

target_include_directories(test_mermaid_cpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${MERMAID_TEST_SOURCE_DIR}/utest
    ${PROJECT_SOURCE_DIR}/include
)
target_compile_definitions(test_mermaid_cpp PRIVATE
    MERMAID_TEST_DATA_DIR="${MERMAID_TEST_SOURCE_DIR}/data"
)

# Reference SVGs: what real Mermaid 12.2.0 (layout: dagre) draws for tests/data/merman/*.mmd. They are not committed.
#  - MERMAID_REFERENCE_DIR=<dir>   use SVGs rendered elsewhere (CI renders them once and shares them);
#  - otherwise, with Node.js and npm found (MERMAID_GENERATE_REFERENCES, ON by default), they are rendered at build time:
#    Mermaid 12.2.0 is installed with puppeteer-core and a headless Chromium into <build>/references and
#    tools/render_all_references.mjs renders every .mmd with the pinned fonts of tests/fonts, so the widths do not
#    depend on the machine (re-run: `cmake --build . --target mermaid_references`);
#  - without either the parity tests report that they were skipped.
set(MERMAID_REFERENCE_DIR "" CACHE PATH "Directory with reference SVGs rendered by Mermaid.js (empty: generate them)")
find_program(MERMAID_NODE node)
find_program(MERMAID_NPM npm)
if(MERMAID_NODE AND MERMAID_NPM)
    set(MERMAID_GENERATE_DEFAULT ON)
else()
    set(MERMAID_GENERATE_DEFAULT OFF)
endif()
option(MERMAID_GENERATE_REFERENCES "Render the reference SVGs with Mermaid.js at build time (Node.js, npm, network)" ${MERMAID_GENERATE_DEFAULT})

if(MERMAID_REFERENCE_DIR)
    set(MERMAID_REFERENCE_SVG_DIR "${MERMAID_REFERENCE_DIR}")
elseif(MERMAID_GENERATE_REFERENCES)
    if(NOT MERMAID_NODE OR NOT MERMAID_NPM)
        message(FATAL_ERROR "MERMAID_GENERATE_REFERENCES needs node and npm")
    endif()
    set(MERMAID_REFERENCE_WORK ${CMAKE_BINARY_DIR}/references)
    set(MERMAID_REFERENCE_SVG_DIR ${MERMAID_REFERENCE_WORK}/svg)
    file(GLOB MERMAID_REFERENCE_INPUTS CONFIGURE_DEPENDS ${MERMAID_TEST_SOURCE_DIR}/data/merman/*.mmd)
    file(MAKE_DIRECTORY ${MERMAID_REFERENCE_WORK})
    if(NOT EXISTS ${MERMAID_REFERENCE_WORK}/package.json)
        file(WRITE ${MERMAID_REFERENCE_WORK}/package.json "{\"name\":\"mermaid-references\",\"private\":true,\"type\":\"module\"}\n")
    endif()
    add_custom_command(
        OUTPUT ${MERMAID_REFERENCE_WORK}/references.stamp
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MERMAID_REFERENCE_SVG_DIR}
        COMMAND ${MERMAID_NPM} install --no-audit --no-fund --no-save --silent mermaid@12.2.0 puppeteer-core @sparticuz/chromium
        # ES modules resolve packages next to the script, so it runs from the folder holding node_modules
        COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/tools/render_all_references.mjs ${MERMAID_REFERENCE_WORK}/render.mjs
        COMMAND ${CMAKE_COMMAND} -E env MERMAID_REFERENCE_FONT_DIR=${MERMAID_TEST_SOURCE_DIR}/fonts
                ${MERMAID_NODE} ${MERMAID_REFERENCE_WORK}/render.mjs
                ${MERMAID_TEST_SOURCE_DIR}/data/merman ${MERMAID_REFERENCE_SVG_DIR}
        COMMAND ${CMAKE_COMMAND} -E touch ${MERMAID_REFERENCE_WORK}/references.stamp
        WORKING_DIRECTORY ${MERMAID_REFERENCE_WORK}
        DEPENDS ${MERMAID_REFERENCE_INPUTS} ${PROJECT_SOURCE_DIR}/tools/render_all_references.mjs
        COMMENT "Rendering the reference SVGs with Mermaid.js 12.2.0"
        VERBATIM)
    add_custom_target(mermaid_references DEPENDS ${MERMAID_REFERENCE_WORK}/references.stamp)
    add_dependencies(test_mermaid_cpp mermaid_references)
else()
    set(MERMAID_REFERENCE_SVG_DIR "${CMAKE_BINARY_DIR}/no-references")
    message(WARNING "No reference SVGs (no Node.js/npm, no MERMAID_REFERENCE_DIR): the parity tests will be skipped")
endif()
target_compile_definitions(test_mermaid_cpp PRIVATE MERMAID_REFERENCE_DIR="${MERMAID_REFERENCE_SVG_DIR}")

add_test(NAME test_mermaid_cpp COMMAND $<TARGET_FILE:test_mermaid_cpp>)

add_custom_target(mermaid_test_cpp
    COMMAND ${CMAKE_COMMAND} -E echo "-- Running C++ tests..."
    COMMAND $<TARGET_FILE:test_mermaid_cpp>
    WORKING_DIRECTORY ${MERMAID_TEST_BINARY_DIR}
)
add_dependencies(mermaid_test_cpp test_mermaid_cpp)
add_dependencies(mermaid_test mermaid_test_cpp)

if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
    target_compile_options(test_mermaid_cpp PRIVATE -O0 -g --coverage)
    target_link_options(test_mermaid_cpp PRIVATE --coverage)

    add_custom_target(mermaid_coverage_cpp
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${MERMAID_COVERAGE_CPP_BINARY_DIR}/html
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MERMAID_COVERAGE_CPP_BINARY_DIR}/html
        COMMAND ${LCOV_EXECUTABLE}
                --capture
                --base-directory ${PROJECT_SOURCE_DIR}
                --directory ${CMAKE_BINARY_DIR}
                --exclude "*/tests/*"
                --no-external
                --rc function_coverage=1
                --output-file ${MERMAID_COVERAGE_CPP_BINARY_DIR}/coverage_temp.lcov
        COMMAND ${LCOV_EXECUTABLE}
                --extract ${MERMAID_COVERAGE_CPP_BINARY_DIR}/coverage_temp.lcov
                "*/include/*" "*/src/*"
                --output-file ${MERMAID_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
        COMMAND ${CMAKE_COMMAND} -E remove ${MERMAID_COVERAGE_CPP_BINARY_DIR}/coverage_temp.lcov
        COMMAND ${GENHTML_EXECUTABLE} --quiet
                ${MERMAID_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
                --output-directory ${MERMAID_COVERAGE_CPP_BINARY_DIR}/html
                --prefix ${PROJECT_SOURCE_DIR}
                --title "mermaid C++ Coverage"
                --css-file ${MERMAID_TEST_SOURCE_DIR}/genhtml.css
                --dark-mode
        COMMAND ${CMAKE_COMMAND}
                -DCOV_FILE=${MERMAID_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
                -DSOURCE_PATH=${PROJECT_SOURCE_DIR}/
                -P ${PROJECT_SOURCE_DIR}/cmake/FixLCovPaths.cmake
        COMMAND ${LCOV_EXECUTABLE} --list ${MERMAID_COVERAGE_CPP_BINARY_DIR}/coverage.lcov
        WORKING_DIRECTORY ${MERMAID_TEST_BINARY_DIR}
        USES_TERMINAL
    )
    add_dependencies(mermaid_coverage_cpp mermaid_test_cpp)
    add_dependencies(mermaid_coverage mermaid_coverage_cpp)
else()
    message(WARNING "LCOV or GENHTML not found - C++ coverage target skipped "
                    "(e.g. 'sudo apt install lcov').")
endif()
