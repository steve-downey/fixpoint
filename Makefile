# Makefile                                                       -*-makefile-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

export
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

NO_COLOR:=1

INSTALL_PREFIX ?= .install/
BUILD_DIR ?= .build
DEST ?= $(INSTALL_PREFIX)
CMAKE_FLAGS ?=

PYEXECPATH ?= $(shell which python3.13 || which python3.12 || which python3.11 || which python3.10 || which python3.9 || which python3.8 || which python3)
PYTHON ?= $(notdir $(PYEXECPATH))
VENV := .venv
UV := $(shell command -v uv 2> /dev/null)
ACTIVATE := $(UV) run
PYEXEC := $(UV) run python
MARKER = .initialized.venv.stamp

PRE_COMMIT := $(UV) run pre-commit

EMACS := $(shell command -v emacs 2> /dev/null)

.update-submodules:
	git submodule update --init --recursive
	touch .update-submodules

.gitmodules: .update-submodules

CONFIG ?= Asan

export

ifeq ($(strip $(TOOLCHAIN)),)
	_build_name?=build-system/
	_build_dir?=.build/
	_local_toolchain?=$(CURDIR)/etc/toolchain.cmake
else
	_build_name?=build-$(TOOLCHAIN)
	_build_dir?=.build/
	_local_toolchain?=$(CURDIR)/etc/$(TOOLCHAIN)-toolchain.cmake
endif

_configuration_types ?= "RelWithDebInfo;Debug;Tsan;Asan;Gcov"

_build_path ?= $(_build_dir)/$(_build_name)
_build_path := $(subst //,/,$(_build_path))
_build_path := $(patsubst %/,%,$(_build_path))

VCPKG ?= $(shell command -v vcpkg 2> /dev/null)

ifeq ($(VCPKG),)
	_cmake_top_level?="infra/cmake/use-fetch-content.cmake"
	_toolchain:=$(_local_toolchain)
	_args=-DBEMANINFRA_googletest_REPO=file:///home/sdowney/bld/googletest/googletest.git
else
	_vcpkg_toolchain:=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake
	_cmake_top_level?=$(_vcpkg_toolchain)
	export PROJECT_VCPKG_TOOLCHAIN=$(_local_toolchain)
	_toolchain:=$(_local_toolchain)
	_args=-DVCPKG_OVERLAY_TRIPLETS=$(CURDIR)/cmake -DVCPKG_TARGET_TRIPLET=x64-linux-custom
	# for debugging add 	-DVCPKG_INSTALL_OPTIONS="--debug"
endif

CMAKE ?= $(UV) run cmake
CTEST ?= $(UV) run ctest

define run_cmake =
	$(CMAKE) \
	-G "Ninja Multi-Config" \
	-DCMAKE_CONFIGURATION_TYPES=$(_configuration_types) \
	-DCMAKE_INSTALL_PREFIX=$(abspath $(INSTALL_PREFIX)) \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
	-DCMAKE_PREFIX_PATH=$(CURDIR)/infra/cmake \
	-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=$(_cmake_top_level) \
	-DCMAKE_C_COMPILER_LAUNCHER=ccache \
	-DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
	-DCMAKE_TOOLCHAIN_FILE=$(_toolchain) \
	$(_args) \
	$(_cmake_args) \
	$(CURDIR)
endef

default: test
.PHONY: default

$(_build_path):
	mkdir -p $(_build_path)

$(_build_path)/CMakeCache.txt: | $(_build_path) .gitmodules $(VENV)
	cd $(_build_path) && $(run_cmake)

$(_build_path)/compile_commands.json: $(_build_path)/CMakeCache.txt

.PHONY: compile_commands.json
compile_commands.json: $(_build_path)/compile_commands.json
compile_commands.json: ## symlink the current compile commands db
	if [ "$(shell readlink compile_commands.json)" != "$(_build_path)/compile_commands.json" ] ; then \
		ln -sf $(_build_path)/compile_commands.json ; \
	fi

.PHONY: clean-compile-commands
clean-compile-commands: ## Delete the compile commands symlink
	-rm -f compile_commands.json

clean: clean-compile-commands

.PHONY: compile
compile: $(_build_path)/CMakeCache.txt compile_commands.json
compile: ## Compile the project
	$(CMAKE) --build $(_build_path)  --config $(CONFIG) --target all -- -k 0

.PHONY: compile-headers
compile-headers: $(_build_path)/CMakeCache.txt ## Compile the headers
	 $(CMAKE) --build $(_build_path)  --config $(CONFIG) --target all_verify_interface_header_sets -- -k 0

.PHONY: install
install: $(_build_path)/CMakeCache.txt compile ## Install the project
	$(CMAKE) --install $(_build_path) --config $(CONFIG) --component fixpoint.fixpoint_Development --verbose

.PHONY: clean-install
clean-install:
	-rm -rf $(INSTALL_PREFIX)

clean: clean-install

.PHONY: realclean
realclean: clean clean-install

.PHONY: ctest
ctest: $(_build_path)/CMakeCache.txt ## Run CTest on current build
	$(CTEST) --test-dir $(_build_path) --output-on-failure -C $(CONFIG)

.PHONY: ctest_
ctest_: compile
	$(CTEST) --test-dir $(_build_path) --output-on-failure -C $(CONFIG)

.PHONY: test
test: ctest_ ## Rebuild and run tests

.PHONY: cmake
cmake: |  $(_build_path)
	cd $(_build_path) && ${run_cmake}

.PHONY: clean
clean: ## Clean the build artifacts without recreating the build tree
	if [ -f $(_build_path)/CMakeCache.txt ] ; then \
		$(CMAKE) --build $(_build_path) --config $(CONFIG) --target clean ; \
	fi

.PHONY: clean-reconf
clean-reconf: ## Delete the current configured build tree
	rm -rf $(_build_path)

.PHONY: reconf
reconf: clean-reconf cmake ## Recreate the current configured build tree

.PHONY: realclean
realclean: ## Delete the generated build infrastructure
	rm -rf $(_build_dir) build

.PHONY: env
env:
	$(foreach v, $(.VARIABLES), $(info $(v) = $($(v))))

.PHONY: papers
papers:
	$(MAKE) -C papers/wg21 pdf

.PHONY: clean-papers
clean-papers: ## Clean generated paper outputs
	$(MAKE) -C papers/wg21 clean

clean: clean-papers

.PHONY: realclean-papers
realclean-papers: ## Delete generated paper infrastructure
	$(MAKE) -C papers/wg21 realclean

realclean: realclean-papers

.PHONY: all
all: compile

.PHONY: venv
venv: ## Create python virtual env
venv: $(VENV)/$(MARKER)

.PHONY: clean-venv
clean-venv: ## Delete python virtual env
	-rm -rf $(VENV)

.PHONY: clean-uv-lock
clean-uv-lock: ## Delete the generated uv lockfile
	-rm -f uv.lock

.PHONY: clean-python-artifacts
clean-python-artifacts: ## Delete Python test and lint caches
	-rm -rf .cache .mypy_cache .pytest_cache .ruff_cache .tox .nox htmlcov cover

realclean: clean-venv
realclean: clean-uv-lock
clean: clean-python-artifacts

.PHONY: show-venv
show-venv: venv
show-venv: ## Debugging target - show venv details
	$(PYEXEC) -c "import sys; print('Python ' + sys.version.replace('\n',''))"
	@echo venv: $(VENV)

uv.lock: pyproject.toml
	$(UV) lock

$(VENV):
	$(UV) venv --python $(PYTHON)

$(VENV)/$(MARKER): uv.lock | $(VENV)
	$(UV) sync
	touch $(VENV)/$(MARKER)

.PHONY: dev-shell
dev-shell: venv
dev-shell: ## Shell with the venv activated
	$(ACTIVATE) $(notdir $(SHELL))

.PHONY: bash zsh
bash zsh: venv
bash zsh: ## Run bash or zsh with the venv activated
	$(ACTIVATE) $@

.PHONY: lint
lint: venv
lint: ## Run all configured tools in pre-commit
	$(PRE_COMMIT) run -a

.PHONY: lint-manual
lint-manual: venv
lint-manual: ## Run all manual tools in pre-commit
	$(PRE_COMMIT) run --hook-stage manual -a

.PHONY: coverage
coverage: ## Build and run the tests with the GCOV profile and process the results
coverage: venv $(_build_path)/CMakeCache.txt
	$(CMAKE) --build $(_build_path) --config Gcov
	$(ACTIVATE) ctest --build-config Gcov --output-on-failure --test-dir $(_build_path)
	$(CMAKE) --build $(_build_path) --config Gcov --target process_coverage

.PHONY: clean-coverage
clean-coverage: ## Delete generated coverage reports
	-rm -rf $(_build_path)/coverage
	-rm -f .coverage .coverage.* coverage.xml

clean: clean-coverage

.PHONY: view-coverage
view-coverage: ## View the coverage report
	sensible-browser $(_build_path)/coverage/coverage.html

.PHONY: docs
docs: ## Build the docs with Doxygen
	doxygen docs/Doxyfile

.PHONY: clean-docs
clean-docs: ## Delete generated Doxygen output
	-rm -rf docs/html docs/latex

clean: clean-docs

.PHONY: mrdocs
mrdocs: ## Build the docs with MrDocs
	-rm -rf docs/adoc
	cd docs && NO_COLOR=1 mrdocs mrdocs.yml 2>&1 | sed 's/\x1b\[[0-9;]*m//g'
	find docs/adoc -name '*.adoc' | xargs asciidoctor

.PHONY: clean-mrdocs
clean-mrdocs: ## Delete generated MrDocs output
	-rm -rf docs/adoc

clean: clean-mrdocs

.PHONY: testinstall
testinstall: install
testinstall: CONFIG=RelWithDebInfo
testinstall: ## Test the installed package
	-$(RM) -rf installtest/.build
	$(CMAKE) -S installtest -B installtest/.build 	-G "Ninja Multi-Config"
	$(CMAKE) --build  installtest/.build --target test --config="RelWithDebInfo"

.PHONY: clean-testinstall
clean-testinstall:
	-rm -rf installtest/.build

clean: clean-testinstall

realclean: clean-testinstall

ifeq ($(UV),)
define install_uv_cmd
pipx install uv
endef

define uv_error_message

'uv' command not found.
Please install uv or set the UV variable to the path of the uv binary.
The makefile target "install-uv" will run ``$(install_uv_cmd)''
endef

$(warn "$(uv_error_message)")
endif

.PHONY: install-uv
install-uv: ## install uv via `pipx install uv`
	$(install_uv_cmd)

ORGFILES := $(wildcard *.org)

%.html : %.org
	$(EMACS) --init-directory=.emacs.d/ \
	--batch --load .emacs.d/init.el  \
	-f package-initialize \
	--eval "(setq enable-local-variables :all)" \
	--visit $< \
	--eval "(org-transclusion-mode t)" \
	--eval "(org-export-to-file 'html \"$@\")"
	echo $@ : \\ > $@.deps
	echo "  $<" \\ >> $@.deps
	sed -n "s/^.*\[\[file:\(\S*\)::.*$$/\1/p" < $<  | sort -u | xargs printf "  %s \\\\\\n" >> $@.deps

-include $(ORGFILES:%.org=%.html.deps)

BLOG_ORGFILES := $(wildcard docs/blog/*.org)

docs/blog/%.md : docs/blog/%.org
	$(EMACS) --init-directory=.emacs.d/ \
	--batch --load .emacs.d/init.el  \
	-f package-initialize \
	--eval "(setq enable-local-variables :all)" \
	--visit $< \
	--eval "(org-transclusion-mode t)" \
	--eval "(require 'ox-gfm)" \
	--eval "(org-export-to-file 'gfm \"$(abspath $@)\")"
	echo $@ : \\ > $@.deps
	echo "  $<" \\ >> $@.deps
	sed -n "s/^.*\[\[file:\(\S*\)::.*$$/\1/p" < $<  | sort -u | xargs printf "  %s \\\\\\n" >> $@.deps

-include $(BLOG_ORGFILES:.org=.md.deps)

.PHONY: blog-md
blog-md: $(BLOG_ORGFILES:.org=.md) ## convert docs/blog/*.org to GFM markdown

.PHONY: clean-blog-md
clean-blog-md:
	-rm -f $(BLOG_ORGFILES:.org=.md) $(BLOG_ORGFILES:.org=.md.deps)
clean: clean-blog-md

.PHONY: blog-haskell-check
blog-haskell-check: ## typecheck the interlude Haskell excerpts (needs GHC 9.6+)
	@if command -v ghc >/dev/null 2>&1; then \
	  out=$$(mktemp -d); \
	  echo "ghc --make -fno-code -Wall docs/blog/code/*.hs"; \
	  ghc --make -fno-code -Wall -outputdir $$out docs/blog/code/*.hs; \
	  rm -rf $$out; \
	else \
	  echo "ghc not found; skipping blog-haskell-check"; \
	fi

%-slides.html : %.org
	$(EMACS) --init-directory=.emacs.d/ \
	--batch --load .emacs.d/init.el  \
	-f package-initialize \
	--eval "(setq enable-local-variables :all)" \
	--visit $< \
	--eval "(org-transclusion-mode t)" \
	--eval "(org-export-to-file 're-reveal \"$@\")"
	echo $@ : \\ > $@.deps
	echo "  $<" \\ >> $@.deps
	sed -n "s/^.*\[\[file:\(\S*\)::.*$$/\1/p" < $<  | sort -u | xargs printf "  %s \\\\\\n" >> $@.deps

-include $(ORGFILES:%.org=%-slides.html.deps)


.PHONY: clean-emacs.d
clean-emacs.d:
	-rm -rf .emacs.d/eln-cache
	-rm -rf .emacs.d/elpa*

realclean: clean-emacs.d

.PHONY: clean-org-deps
clean-org-deps:
	-rm -f $(ORGFILES:%.org=%.org.deps)
clean: clean-org-deps

.PHONY: clean-org-html
clean-org-html:
	-rm -f $(ORGFILES:%.org=%.html) $(ORGFILES:%.org=%-slides.html)
clean: clean-org-html

.PHONY: clean-submodules
clean-submodules: ## Delete the submodule update stamp
	-rm -f .update-submodules

realclean: clean-submodules

.PHONY: presentation
presentation: test
presentation: $(ORGFILES:%.org=%.html)
presentation: $(ORGFILES:%.org=%-slides.html)

.PHONY: elpa
elpa:
	$(EMACS) --init-directory=.emacs.d/ --batch --load .emacs.d/init.el

.PHONY: refresh
refresh:
	$(EMACS) --init-directory=.emacs.d/ --batch --load .emacs.d/init.el -f package-upgrade-all

# Help target
.PHONY: help
help: ## Show this help.
	@awk 'BEGIN {FS = ":.*?## "} /^[.a-zA-Z_-]+:.*?## / {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'  $(MAKEFILE_LIST) | sort

.PHONY: help-targets
help-targets: ## Show this help.
	@awk 'BEGIN {FS = ":.*?## "} /^[.a-zA-Z_-]+:.*?## / {printf "%s\n", $$1}'  $(MAKEFILE_LIST) | sort
