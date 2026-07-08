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

add_executable(para_pretty_print)

# Add example source file.
target_sources(para_pretty_print PRIVATE para_pretty_print.cpp)

# Link example with the library.
target_link_libraries(para_pretty_print PRIVATE fixpoint.fixpoint)

install(
    TARGETS para_pretty_print
    COMPONENT fixpoint.fixpoint.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(apo_sorted_insert)

# Add example source file.
target_sources(apo_sorted_insert PRIVATE apo_sorted_insert.cpp)

# Link example with the library.
target_link_libraries(apo_sorted_insert PRIVATE fixpoint.fixpoint)

install(
    TARGETS apo_sorted_insert
    COMPONENT fixpoint.fixpoint.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(zygo_balanced)

# Add example source file.
target_sources(zygo_balanced PRIVATE zygo_balanced.cpp)

# Link example with the library.
target_link_libraries(zygo_balanced PRIVATE fixpoint.fixpoint)

install(
    TARGETS zygo_balanced
    COMPONENT fixpoint.fixpoint.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(mutu_even_odd)

# Add example source file.
target_sources(mutu_even_odd PRIVATE mutu_even_odd.cpp)

# Link example with the library.
target_link_libraries(mutu_even_odd PRIVATE fixpoint.fixpoint)

install(
    TARGETS mutu_even_odd
    COMPONENT fixpoint.fixpoint.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(prepro_takewhile_sum)

# Add example source file.
target_sources(prepro_takewhile_sum PRIVATE prepro_takewhile_sum.cpp)

# Link example with the library.
target_link_libraries(prepro_takewhile_sum PRIVATE fixpoint.fixpoint)

install(
    TARGETS prepro_takewhile_sum
    COMPONENT fixpoint.fixpoint.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(histo_coin_change)

# Add example source file.
target_sources(histo_coin_change PRIVATE histo_coin_change.cpp)

# Link example with the library.
target_link_libraries(histo_coin_change PRIVATE fixpoint.fixpoint)

install(
    TARGETS histo_coin_change
    COMPONENT fixpoint.fixpoint.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(futu_rle_decode)

# Add example source file.
target_sources(futu_rle_decode PRIVATE futu_rle_decode.cpp)

# Link example with the library.
target_link_libraries(futu_rle_decode PRIVATE fixpoint.fixpoint)

install(
    TARGETS futu_rle_decode
    COMPONENT fixpoint.fixpoint.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(dyna_fibonacci)

# Add example source file.
target_sources(dyna_fibonacci PRIVATE dyna_fibonacci.cpp)

# Link example with the library.
target_link_libraries(dyna_fibonacci PRIVATE fixpoint.fixpoint)

install(
    TARGETS dyna_fibonacci
    COMPONENT fixpoint.fixpoint.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(mendler_eval)

# Add example source file.
target_sources(mendler_eval PRIVATE mendler_eval.cpp)

# Link example with the library.
target_link_libraries(mendler_eval PRIVATE fixpoint.fixpoint)

install(
    TARGETS mendler_eval
    COMPONENT fixpoint.fixpoint.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(generalized_tour)

# Add example source file.
target_sources(generalized_tour PRIVATE generalized_tour.cpp)

# Link example with the library.
target_link_libraries(generalized_tour PRIVATE fixpoint.fixpoint)

install(
    TARGETS generalized_tour
    COMPONENT fixpoint.fixpoint.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(elgot_shortcircuit)

# Add example source file.
target_sources(elgot_shortcircuit PRIVATE elgot_shortcircuit.cpp)

# Link example with the library.
target_link_libraries(elgot_shortcircuit PRIVATE fixpoint.fixpoint)

install(
    TARGETS elgot_shortcircuit
    COMPONENT fixpoint.fixpoint.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)

add_executable(free_cofree_interlude)

# Add example source file.
target_sources(free_cofree_interlude PRIVATE free_cofree_interlude.cpp)

# Link example with the library.
target_link_libraries(free_cofree_interlude PRIVATE fixpoint.fixpoint)

install(
    TARGETS free_cofree_interlude
    COMPONENT fixpoint.fixpoint.examples
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    EXCLUDE_FROM_ALL
)
