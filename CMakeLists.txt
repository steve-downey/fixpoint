# example/CMakeLists.txt                                            -*-cmake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 4.0..4.2)

project(fixpoint VERSION 0.0.0 LANGUAGES CXX)

# Instrumentation query
# find_package(Python COMPONENTS Interpreter)
cmake_instrumentation(
  API_VERSION 1
  DATA_VERSION 1
  HOOKS postGenerate preBuild postBuild preCMakeBuild postCMakeBuild postCMakeInstall postCTest
  OPTIONS trace
  CALLBACK ${CMAKE_SOURCE_DIR}/scripts/instrument.sh
)

# Includes
include(CTest)
include(FetchContent)

option(
    FIXPOINT_ENABLE_TESTING
    "Enable building tests and test infrastructure"
    ${PROJECT_IS_TOP_LEVEL}
)

set(CMAKE_VERIFY_INTERFACE_HEADER_SETS ON)

# Create the library target and named header set for beman.optional
add_library(fixpoint.fixpoint STATIC "")

target_sources(
    fixpoint.fixpoint
    PUBLIC FILE_SET example_fixpoint_headers TYPE HEADERS BASE_DIRS src
)

if(FIXPOINT_ENABLE_TESTING)
    find_package(Catch2 3 QUIET)
    if(Catch2_FOUND)
        include(Catch)
    else()
        message(
            WARNING
            "
No provider for Catch2. Unable to build tests.
Consider using CMAKE_PROJECT_TOP_LEVEL_INCLUDES=./cmake/use-fetch-content.cmake as documented in the README.md
"
        )
    endif()
endif()

add_subdirectory(src)

# Header-set verification needs the beman include dirs to compile the
# headers in smd_fixpoint_beman_headers (see src/smd/fixpoint/
# CMakeLists.txt). In a configure without FIXPOINT_ENABLE_TESTING the
# beman targets are never fetched, so exclude that set from verification
# rather than fail on an unresolvable #include.
if(NOT TARGET beman::execution_headers)
    get_property(
        _fixpoint_verify_sets
        TARGET fixpoint.fixpoint
        PROPERTY INTERFACE_HEADER_SETS
    )
    list(REMOVE_ITEM _fixpoint_verify_sets smd_fixpoint_beman_headers)
    set_property(
        TARGET fixpoint.fixpoint
        PROPERTY INTERFACE_HEADER_SETS_TO_VERIFY "${_fixpoint_verify_sets}"
    )
endif()

include(infra/cmake/beman-install-library.cmake)
beman_install_library(fixpoint.fixpoint TARGETS fixpoint.fixpoint NAMESPACE smd::)

# Coverage
configure_file("cmake/gcovr.cfg.in" gcovr.cfg @ONLY)

add_custom_target(
    process_coverage
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running gcovr to process coverage results"
    COMMAND mkdir -p coverage
    COMMAND gcovr --config gcovr.cfg .
)
