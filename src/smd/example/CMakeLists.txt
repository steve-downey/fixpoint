include(GNUInstallDirs)

target_sources(
    fixpoint.fixpoint
    PUBLIC FILE_SET example_fixpoint_headers TYPE HEADERS FILES fixpoint.hpp
    PRIVATE fixpoint.cpp
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
    add_executable(fixpoint_test)

    target_sources(fixpoint_test PRIVATE fixpoint.test.cpp)

    target_link_libraries(fixpoint_test fixpoint.fixpoint)
    target_link_libraries(fixpoint_test Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(fixpoint_test)
endif()
