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
                freer_row.hpp
                unfold_cofree.hpp
                freer_cosignature.hpp
                mcata_free.hpp
                freer_task.hpp
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
            freer_row.t.cpp
            freer_cosignature.t.cpp
            mcata_free.t.cpp
    )

    target_link_libraries(smd_fixpoint_test fixpoint.fixpoint)
    target_link_libraries(smd_fixpoint_test Catch2::Catch2WithMain)

    include(Catch)
    catch_discover_tests(smd_fixpoint_test)

    # S07d: beman.execution / beman.task probe. Deliberately its own
    # executable, separate from smd_fixpoint_test and NOT linked into
    # fixpoint.fixpoint -- this step only proves the deps are fetchable and
    # linkable, S07 does the real interpreter work against them.
    #
    # cmake/beman-deps.cmake (loaded via CMAKE_PROJECT_TOP_LEVEL_INCLUDES)
    # only FetchContent_Declare()s beman-execution/beman-task; the actual
    # FetchContent_MakeAvailable() has to happen here, outside the
    # top-level project() call, to avoid a recursive project() error (see
    # that file's comment for why).
    include(FetchContent)
    FetchContent_MakeAvailable(execution task)

    add_executable(freer_beman_probe_test)

    target_sources(freer_beman_probe_test PRIVATE freer_beman_probe.t.cpp)

    target_link_libraries(
        freer_beman_probe_test
        Catch2::Catch2WithMain
        beman::execution_headers
        beman::task
    )

    catch_discover_tests(freer_beman_probe_test)

    # S07: the sender/receiver interpreter (freer_task.hpp). Its own
    # executable, linking the same two beman targets the probe uses -- so
    # beman::execution / beman::task stay confined to freer_task.hpp and this
    # test, never entering smd_fixpoint_test or fixpoint.fixpoint (the core
    # fold mcata_free.hpp has no beman dependency and is tested in
    # smd_fixpoint_test).
    add_executable(freer_task_test)

    target_sources(freer_task_test PRIVATE freer_task.t.cpp)

    target_link_libraries(
        freer_task_test
        fixpoint.fixpoint
        Catch2::Catch2WithMain
        beman::execution_headers
        beman::task
    )

    catch_discover_tests(freer_task_test)
endif()
