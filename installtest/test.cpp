// testinstall/test.cpp                                               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <iostream>
#include <smd/example/fixpoint.hpp>

int main() {
    std::cout << "fixpoint: |" << example::fixpoint() << '|' << '\n';
    return 0;
}
