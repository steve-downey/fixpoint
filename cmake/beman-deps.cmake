# cmake/beman-deps.cmake                                            -*-cmake-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Freer-over-Fixpoint S07d: provisions beman::execution and beman::task via
# FetchContent, independently of whichever mechanism (vcpkg, or the infra/
# FetchContent fallback) is provisioning Catch2 for this configure.
#
# Neither bemanproject/execution nor bemanproject/task has a port in the
# vcpkg registry this repo resolves against (checked 2026-07-11; see
# ops/freer/handoffs/07d-beman-deps.handoff.md for the registry search).
# So this file is the *only* provisioning path for these two deps, in
# every configure flow: it is include()d by its consumer,
# src/smd/fixpoint/CMakeLists.txt, immediately before the
# FetchContent_MakeAvailable(execution task) that needs these declares,
# and include_guard(GLOBAL) makes that include idempotent. It is NOT
# loaded via CMAKE_PROJECT_TOP_LEVEL_INCLUDES: the Beman preset/matrix CI
# lanes control that variable themselves (they set only
# infra/cmake/use-fetch-content.cmake, for Catch2), so a declare left to
# a top-level include silently goes missing there and every CI lane fails
# to configure with "No content details recorded for execution" -- which
# is exactly what happened between 2026-07-12 and 2026-07-20.
#
# This file only FetchContent_Declare()s the two deps (cheap, metadata
# only). It deliberately does NOT call FetchContent_MakeAvailable() here:
# historically this file ran from CMAKE_PROJECT_TOP_LEVEL_INCLUDES, i.e.
# as the last step *inside* the top-level project() command's own
# processing, where FetchContent_MakeAvailable() -- which ends up calling
# add_subdirectory() -> project() for each fetched dep -- is rejected as
# a recursive project() call ("Language 'CXX' is currently being
# enabled. Recursive call not allowed."). Keeping declare and
# MakeAvailable separate also remains the right shape now: the declares
# are reusable metadata, the MakeAvailable belongs to the consumer that
# needs the targets (src/smd/fixpoint/CMakeLists.txt, guarded by
# FIXPOINT_ENABLE_TESTING since tests are the only consumers right now).
#
# Neither upstream repo tags releases (checked on the commits below via
# `git ls-remote --tags`: both empty); pins are exact commit SHAs off each
# repo's `main`, not branches or tags.
#
# beman.task's own top-level CMakeLists.txt FetchContent_Declares a specific
# beman.execution commit as ITS dependency (see its `execution`
# FetchContent_Declare, GIT_TAG). The two repos' `main` branches drift out
# of API sync with each other independently (reproduced: execution's own
# `main` HEAD at provisioning time renamed `affine_on` to `affine_t`,
# breaking task's `main` HEAD, which still called `affine_on` -- see the
# handoff for the verbatim compiler error). To avoid that divergence, this
# file pins beman-execution to *the exact commit task's own CMakeLists
# pins*, not to execution's own `main` HEAD.
#
# Both FetchContent_Declare calls use EXCLUDE_FROM_ALL: beman.execution's
# CMakeLists auto-enables its C++ modules build (BEMAN_USE_MODULES) under
# Ninja + gcc>=15 or clang>=19 (both this repo's pinned toolchains), which
# would otherwise pull ~200 module-interface compilations into the default
# `all` target. EXCLUDE_FROM_ALL keeps that STATIC/module target
# (beman::execution, alias of the modules build) out of the build graph
# entirely unless something actually links it. Nothing in this repo does:
# the probe below links the always-available header-only
# beman::execution_headers target and beman::task (which itself is a plain
# compiled STATIC library, not a module), never beman::execution. See the
# handoff for build-cost evidence.
include_guard(GLOBAL)

include(FetchContent)

# Pinned to the commit beman.task's own CMakeLists.txt fetches as its
# beman.execution dependency (see task's FetchContent_Declare(execution ...
# GIT_TAG 7df0f75) -- resolved here to the full SHA for an exact pin).
set(FREER_BEMAN_EXECUTION_GIT_TAG "7df0f75d493faf2097db4a392b74d2c6be031847")

# main HEAD of bemanproject/task at provisioning time (2026-07-11).
set(FREER_BEMAN_TASK_GIT_TAG "167918522e1cc4bc44c04f97b204f46c1735d7a2")

FetchContent_Declare(
    execution
    GIT_REPOSITORY https://github.com/bemanproject/execution.git
    GIT_TAG ${FREER_BEMAN_EXECUTION_GIT_TAG}
    EXCLUDE_FROM_ALL
)
FetchContent_Declare(
    task
    GIT_REPOSITORY https://github.com/bemanproject/task.git
    GIT_TAG ${FREER_BEMAN_TASK_GIT_TAG}
    EXCLUDE_FROM_ALL
)
# See the note above: FetchContent_MakeAvailable() is deliberately NOT
# called here.
