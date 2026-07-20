include(GNUInstallDirs)

# sender_dot.hpp includes beman/execution headers, so (like
# freer_task.hpp in ../fixpoint) it cannot pass the opt-in
# compile-headers verification target, which compiles each header with
# only -I src.
target_sources(
    fixpoint.fixpoint
    PUBLIC
        FILE_SET smd_concrete_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES functors.hpp sender_dot.hpp
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

    target_sources(smd_concrete_test PRIVATE functors.t.cpp)

    target_link_libraries(smd_concrete_test fixpoint.fixpoint)
    target_link_libraries(smd_concrete_test Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(smd_concrete_test)

    # Sender-expression -> Graphviz DOT (sender_dot.hpp). Its own
    # executable so beman::execution stays confined, per the freer_task
    # precedent in ../fixpoint; needs only the header-only
    # beman::execution_headers target, which ../fixpoint's
    # FetchContent_MakeAvailable has already created (src/smd processes
    # fixpoint before concrete).
    if(TARGET beman::execution_headers)
        add_executable(sender_dot_test)

        target_sources(sender_dot_test PRIVATE sender_dot.t.cpp)

        target_link_libraries(
            sender_dot_test
            fixpoint.fixpoint
            Catch2::Catch2WithMain
            beman::execution_headers
        )

        catch_discover_tests(sender_dot_test)
    endif()
endif()
