include(GNUInstallDirs)

target_sources(
    fixpoint.fixpoint
    PUBLIC
        FILE_SET smd_fixpoint_headers
            TYPE HEADERS
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES
                overloaded.hpp
                fix.hpp
                box.hpp
                fmap.hpp
                recursion_schemes.hpp
                para.hpp
                apo.hpp
                zygo.hpp
                mutu.hpp
                prepro.hpp
                cofree.hpp
                histo.hpp
                free.hpp
                futu.hpp
                chrono.hpp
                mendler.hpp
                elgot.hpp
                dist_laws.hpp
                generalized.hpp
                schemes.hpp
                one_shot.hpp
                freer.hpp
                freer_run.hpp
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
        PRIVATE
            box.t.cpp
            fix.t.cpp
            overloaded.t.cpp
            fmap.t.cpp
            recursion_schemes.t.cpp
            para.t.cpp
            apo.t.cpp
            zygo.t.cpp
            mutu.t.cpp
            prepro.t.cpp
            cofree.t.cpp
            histo.t.cpp
            free.t.cpp
            freer_baseline.t.cpp
            futu.t.cpp
            chrono.t.cpp
            mendler.t.cpp
            elgot.t.cpp
            dist_laws.t.cpp
            generalized.t.cpp
            gana.t.cpp
            ghylo.t.cpp
            gprepro.t.cpp
            schemes.t.cpp
            one_shot.t.cpp
            freer.t.cpp
            freer_run.t.cpp
    )

    target_link_libraries(smd_fixpoint_test fixpoint.fixpoint)
    target_link_libraries(smd_fixpoint_test Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(smd_fixpoint_test)
endif()
