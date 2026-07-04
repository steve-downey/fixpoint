include(GNUInstallDirs)

target_sources(
    fixpoint.fixpoint
    PUBLIC
        FILE_SET smd_fixpoint_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES overloaded.hpp fix.hpp box.hpp fmap.hpp recursion_schemes.hpp functors.hpp para.hpp apo.hpp zygo.hpp mutu.hpp prepro.hpp
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
    add_executable(smd_fixpoint_test)

    target_sources(
        smd_fixpoint_test
        PRIVATE box.t.cpp fix.t.cpp overloaded.t.cpp fmap.t.cpp recursion_schemes.t.cpp functors.t.cpp para.t.cpp apo.t.cpp zygo.t.cpp mutu.t.cpp prepro.t.cpp
    )

    target_link_libraries(smd_fixpoint_test fixpoint.fixpoint)
    target_link_libraries(smd_fixpoint_test Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(smd_fixpoint_test)
endif()
