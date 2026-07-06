include(GNUInstallDirs)

target_sources(
    fixpoint.fixpoint
    PUBLIC
        FILE_SET smd_concrete_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES
                functors.hpp
)

## Tests
if(FIXPOINT_ENABLE_TESTING AND NOT Catch2_FOUND)
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

if(FIXPOINT_ENABLE_TESTING)
    add_executable(smd_concrete_test)

    target_sources(
        smd_concrete_test
        PRIVATE
            functors.t.cpp
    )

    target_link_libraries(smd_concrete_test fixpoint.fixpoint)
    target_link_libraries(smd_concrete_test Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(smd_concrete_test)
endif()
