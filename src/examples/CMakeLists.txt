include(GNUInstallDirs)

add_executable(hello)

# Add example source file.
target_sources(hello PRIVATE hello.cpp)

# Link example with the library.
target_link_libraries(hello PRIVATE fixpoint.fixpoint)

install(
    TARGETS hello
    COMPONENT fixpoint.fixpoint.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(fixpoint_tree_example)

# Add example source file.
target_sources(fixpoint_tree_example PRIVATE fixpoint_tree_example.cpp)

# Link example with the library.
target_link_libraries(fixpoint_tree_example PRIVATE fixpoint.fixpoint)

install(
    TARGETS fixpoint_tree_example
    COMPONENT fixpoint.fixpoint.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)
