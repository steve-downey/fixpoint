include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/gcc-flags.cmake")

set(CMAKE_C_COMPILER cc)
set(CMAKE_CXX_COMPILER c++)
set(GCOV_EXECUTABLE "gcov" CACHE STRING "GCOV executable")
