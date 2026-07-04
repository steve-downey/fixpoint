include(GNUInstallDirs)

target_sources(
    fixpoint.fixpoint
    PUBLIC
        FILE_SET smd_typeclass_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES
                detail/typeclass_base.hpp
                apply.hpp
                functor.hpp
                monad.hpp
                monoid.hpp
                dual_monoid.hpp
                fold.hpp
                traverse.hpp
                sequence.hpp
                identity.hpp
                either.hpp
                pair.hpp
                comonad.hpp
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
    add_executable(smd_typeclass_test)

    target_sources(
        smd_typeclass_test
        PRIVATE
            detail/typeclass_base.t.cpp
            apply.t.cpp
            functor.t.cpp
            monad.t.cpp
            monoid.t.cpp
            dual_monoid.t.cpp
            fold.t.cpp
            traverse.t.cpp
            sequence.t.cpp
            identity.t.cpp
            either.t.cpp
            pair.t.cpp
            comonad.t.cpp
    )

    target_link_libraries(smd_typeclass_test fixpoint.fixpoint)
    target_link_libraries(smd_typeclass_test Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(smd_typeclass_test)
endif()
