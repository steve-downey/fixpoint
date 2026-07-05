# Type Level Fixpoint

[![OpenSSF Baseline](https://www.bestpractices.dev/projects/12577/baseline)](https://www.bestpractices.dev/projects/12577)

This repo is my current set of best practices for C++ projects. It does evolve somewhat over time.

This is a snapshot as of today, Sat Apr 11 05:26:37 PM BST 2026.

The code is trivial so that I can repurpose the framework quickly. A library that returns my name, a test that confirms that works, and an example hello `fixpoint` that uses the library.

The C++ src is all in the ./src directory, including the headers and tests. Take a look at [The Pitchfork Layout Spec](https://www.w3.org/publications/spec-generator/?type=bikeshed-spec&output=html&die-on=fatal&md-date=&url=https%3A%2F%2Fraw.githubusercontent.com%2Fvector-of-bool%2Fpitchfork%2Fdevelop%2Fdata%2Fspec.bs&file=) for some discussion about merged layouts. Short answer is that include directories are an install location, not a source location, but that the directory layouts must still be coherent. Tests are co-located because tests are important and the further away they are, the more they will be dropped.

The CMake is contemporary, post-modern, so not just target oriented, it is also file set oriented.

GitHub Actions are set up to make sure everything I expect to work actually does.

There is a top-level Makefile to drive workflow. Its default is to build and run all tests for the project.

The project levergages `uv` and PyPI to install the tools that it requires. It installs them into a local virtual environment so as not to make system wide changes.

The repository also vendors a generic WG21 paper framework under `papers/wg21` via `git subtree`. It is intentionally generic rather than tied to any one proposal number. Use `make papers` to build the vendored papers, `make clean` to remove their generated outputs, and `make realclean` to remove the paper tool infrastructure as well.

The [pre-commit](https://github.com/pre-commit/pre-commit) framework is used to drive linters both locally and in GitHub Actions. Clang format is enforced, as is a CMake format. I've given up doing this by hand. Yaml is even worse. Spellcheck, for code, also.

The infra directory is vendored in from the [Beman Project](https://github.com/bemanproject/infra) supporting [infra](https://github.com/bemanproject/infra) project. Right now for install of the project. Many of the GitHub actions in .github/workflows/ also use Beman scripts and tools. The CMakePresets.txt exists largely to support those tools. I find the workflow [Makefile](./Makefile) easier to extend with less combanitorial explosion.

Complers are expected to be available on PATH with versioned names, such as `g++-15` or `clang++-21`. Toolchains are in the ./etc/ directory.

`make` by itself uses the system `c++` compiler. For others, e.g., `make TOOLCHAIN=gcc-15` will use the etc/gcc-15-toolchain.cmake toolchain, which sets CXX to be gcc-15. By default the build and test is address sanitized, plus some compatible sanitizers. Alternatives are specified with CONFIG, e.g. `make TOOLCHAIN=gcc-15 CONFIG=RelWithDebInfo`.


# Building Presentations with Emacs and Org-Transclusion

This example project uses [nobiot's org-transclusion](https://github.com/nobiot/org-transclusion) and org-export to produce an HTML file for use in presentations. This allows checking that the code is correct but also limited to what is useful.

The export can be run by `make presentation`, which builds and runs the tests for the project and runs the org export afterwards.

The `infra` directory is vendored in from the Beman Project via `git subtree`.

The makefile provides a variety of tools. It will install most borrowing from PyPI as long as `uv` is available. The installation is in a local `.venv` so as not to mess up the rest of your environment.

```shell
(fixpoint) sdowney@pwyll:~/src/surround/fixpoint (main ±)
$ make help
clean                          Clean the build artifacts
clean-reconf                   Delete the current configured build tree
clean-venv                     Delete python virtual env
compile                        Compile the project
compile_commands.json          symlink the current compile commands db
compile-headers                Compile the headers
coverage                       Build and run the tests with the GCOV profile and process the results
ctest                          Run CTest on current build
dev-shell                      Shell with the venv activated
docs                           Build the docs with Doxygen
help                           Show this help.
install                        Install the project
install-uv                     install uv via `pipx install uv`
lint                           Run all configured tools in pre-commit
lint-manual                    Run all manual tools in pre-commit
mrdocs                         Build the docs with MrDocs
papers                         Build the vendored WG21 papers
reconf                         Recreate the current configured build tree
realclean                      Delete the generated build infrastructure
show-venv                      Debugging target - show venv details
test                           Rebuild and run tests
testinstall                    Test the installed package
venv                           Create python virtual env
view-coverage                  View the coverage report
```

`docs` and `mrdocs` are not included in the example at the moment.

`lint` uses pre-commit to drive the various lint tools.

Use this project as you see fit.

The code in infra is Apache 2.0 licensed, see https://github.com/bemanproject/infra for more details. The CMakeLists.txt is derived from https://github.com/bemanproject/exemplar the purpose of which is to be a concrete but boring example of a well behaved CMake C++ project using the current tools and practices.

The css in `etc/`  is exported from emacs based on the modus tinted themes via `org-html-htmlize-generate-css` .

The Makefile that drives the workflow is mine, is Apache 2.0 licensed, and take what you need from it. No part of it is interesting enough to be protected.

## Recursion schemes

`src/smd/fixpoint/` also carries a from-scratch, tested implementation of
the classical recursion-schemes catalog on top of this repo's `Fix<F>`
fixed-point type: `para`/`apo`, `zygo`/`mutu`, `hoist`/`prepro`/`postpro`,
`histo`/`futu` (with their `Cofree`/`Free` carriers), the fused
course-of-values refolds `dyna`/`codyna`/`chrono`, Mendler-style
`mcata`/`mhisto`, `elgot`/`coelgot`, a set of distributive laws, and their
comonadic/monadic generalizations `gcata`/`gana`/`ghylo` down to Kmett's
`zygo_histo_prepro` capstone. Everything is `constexpr`-capable and pinned
by an equivalence-law test suite (each new scheme is proven to degenerate
to an already-tested one).

One include brings in the whole catalog:

```cpp
#include <smd/fixpoint/schemes.hpp>
```

Build and test it (same toolchain/gate as the rest of the project):

```bash
make TOOLCHAIN=gcc-16 test
```

Each scheme has a runnable, heavily-commented example under
`src/examples/` (built alongside the tests above), e.g.:

```bash
.build/build-gcc-16/src/examples/Asan/histo_coin_change
.build/build-gcc-16/src/examples/Asan/generalized_tour
```

See [`docs/recursion-schemes.md`](docs/recursion-schemes.md) for the
user-facing catalog (signatures, equations, usage snippets, one pointer per
example) and [`docs/recursion-schemes-design.md`](docs/recursion-schemes-design.md)
for the design rationale and decisions log. The step-by-step build log
that produced it lives under `ops/` (`ops/PLAN.md`, `ops/handoffs/`).
