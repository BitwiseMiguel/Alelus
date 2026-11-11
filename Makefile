# =============================================================================
# Alelus — Makefile
#
#   make             build the library
#   make tests       build and run the tests
#   make bench       build (in release mode) and run the benchmark; B=name filters it
#   make examples    build and run every program in examples/
#   make check-docs  check that the first C++ example in README.md is examples/quick_start.cpp
#   make dist        package the release library and headers in dist/alelus/
#   make format      format every source file with clang-format
#   make clean       delete the build/ and dist/ folders
#   make help        show this help
#
# Options:
#   MODE=release     optimised build (-O2) instead of debug (-g -O0)
#   WERROR=1         treat compiler warnings as errors
#   SANITIZE=1       make tests: check for memory errors and undefined behaviour while the
#                    tests run (AddressSanitizer + UndefinedBehaviorSanitizer; Linux, macOS)
#   T="name"         make tests: run only the tests whose name or tag matches, e.g. T=[fast]
#   B="name"         make bench: run only the measurements whose name contains it
#   CXX=clang++      use another compiler (default: g++)
#
# Files are found automatically: every src/*.cpp goes into the library and every
# tests/*.cpp goes into the test program. You do not need to edit this file to add them.
# =============================================================================

MODE  ?= debug
BUILD := build/$(MODE)$(if $(SANITIZE),-sanitize)

# --- Compiler flags -----------------------------------------------------------
CXXFLAGS := -std=c++20 -Wall -Wextra -Wpedantic -Wshadow
ifeq ($(MODE),release)
    CXXFLAGS += -O2 -DNDEBUG
else
    CXXFLAGS += -g -O0
endif
ifeq ($(WERROR),1)
    CXXFLAGS += -Werror
endif
# The sanitizers add checks to the compiled code: every memory access is checked against
# the bounds of its object (address), and operations whose result C++ leaves undefined,
# such as signed overflow or shifting by 64 bits, are reported (undefined). The first
# error stops the program, so a test run with an error fails. The objects go to their own
# folder (see BUILD above): mixing checked and unchecked objects does not work.
ifeq ($(SANITIZE),1)
    CXXFLAGS += -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
    # With the sanitizers and -O2, g++ reports "may be used uninitialized" inside the
    # standard library (std::regex, used by Catch2): the extra checks confuse its analysis.
    # The normal builds keep this warning on, so our own code is still checked for it.
    # clang has no such warning, and rejects the option.
    ifeq ($(findstring clang,$(CXX)),)
        CXXFLAGS += -Wno-maybe-uninitialized
    endif
endif

# -MMD -MP: next to each .o, the compiler writes a .d file listing the headers that
# the .cpp includes. make reads those files (last line of this Makefile), so changing
# a .hpp recompiles everything that uses it.
CPPFLAGS := -Iinclude -MMD -MP

# --- Platform ------------------------------------------------------------------
# On Windows, BCryptGenRandom (src/os_entropy.cpp) lives in the bcrypt system library,
# which is not linked by default.
# Elsewhere, pthread_atfork (src/fork_guard.cpp) needs the system thread library.
ifeq ($(OS),Windows_NT)
    EXE      := .exe
    LDLIBS   := -lbcrypt
    # MinGW's g++ ships without the sanitizer libraries: stop with a clear message instead
    # of a linker error about -lasan
    ifeq ($(SANITIZE),1)
        ifeq ($(findstring clang,$(CXX)),)
            $(error SANITIZE=1 is not supported by MinGW g++ on Windows. Run it on Linux or WSL; the CI runs it on every pull request)
        endif
    endif
else
    CXXFLAGS += -pthread
    LDLIBS   := -pthread
endif

# --- Files ---------------------------------------------------------------------
LIB_SRC  := $(wildcard src/*.cpp)
LIB_OBJ  := $(patsubst %.cpp,$(BUILD)/%.o,$(LIB_SRC))
LIB      := $(if $(LIB_SRC),$(BUILD)/libalelus.a)

TEST_SRC := $(wildcard tests/*.cpp)
TEST_OBJ := $(patsubst %.cpp,$(BUILD)/%.o,$(TEST_SRC))
TEST_BIN := $(BUILD)/alelus_tests$(EXE)

BENCH_SRC := $(wildcard bench/*.cpp)
BENCH_OBJ := $(patsubst %.cpp,$(BUILD)/%.o,$(BENCH_SRC))
BENCH_BIN := $(BUILD)/alelus_bench$(EXE)

# One program per file: examples/name.cpp -> build/<mode>/examples/name
EXAMPLE_SRC := $(wildcard examples/*.cpp)
EXAMPLE_OBJ := $(patsubst %.cpp,$(BUILD)/%.o,$(EXAMPLE_SRC))
EXAMPLE_BIN := $(patsubst %.cpp,$(BUILD)/%$(EXE),$(EXAMPLE_SRC))

FORMAT_FILES := $(wildcard include/alelus/*.hpp include/alelus/*/*.hpp src/*.cpp tests/*.cpp tests/*/*.hpp bench/*.cpp examples/*.cpp)

# --- Targets -------------------------------------------------------------------
.DEFAULT_GOAL := all
.PHONY: all tests bench bench-binary examples check-docs dist format clean help

all: $(LIB)
ifeq ($(LIB_SRC),)
	@echo "Nothing to build yet: src/ has no .cpp files."
endif

# A static library is just an archive of .o files
$(LIB): $(LIB_OBJ)
	@mkdir -p $(dir $@)
	@echo "[AR]  $@"
	@rm -f $@
	@ar rcs $@ $^

# Generic rule: any .cpp -> build/<mode>/<same path>.o
$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "[CXX] $<"
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# The tests can also include headers from tests/ (for example catch.hpp)
$(TEST_OBJ): CPPFLAGS += -Itests

$(TEST_BIN): $(TEST_OBJ) $(LIB)
	@echo "[LD]  $@"
	@$(CXX) $(CXXFLAGS) -o $@ $(TEST_OBJ) $(LIB) $(LDLIBS)

ifeq ($(TEST_SRC),)
tests:
	@echo "No tests yet: tests/ has no .cpp files."
else
tests: $(TEST_BIN)
	@echo "Running the tests..."
	@./$(TEST_BIN) $(if $(T),"$(T)")
endif

# The benchmark is always built in release mode: timing a debug build says nothing about
# real performance. "make bench" re-runs make with MODE=release to build it, then runs it.
$(BENCH_BIN): $(BENCH_OBJ) $(LIB)
	@echo "[LD]  $@"
	@$(CXX) $(CXXFLAGS) -o $@ $(BENCH_OBJ) $(LIB) $(LDLIBS)

# The empty recipe (":") stops make from printing "Nothing to be done" when it is up to date
bench-binary: $(BENCH_BIN)
	@:

bench:
	@$(MAKE) --no-print-directory MODE=release SANITIZE= bench-binary
	@./build/release/alelus_bench$(EXE) $(if $(B),"$(B)")

# A "static pattern rule": it applies only to the targets listed before the first colon,
# so it cannot be mistaken for a rule that builds .o files
$(EXAMPLE_BIN): $(BUILD)/examples/%$(EXE): $(BUILD)/examples/%.o $(LIB)
	@echo "[LD]  $@"
	@$(CXX) $(CXXFLAGS) -o $@ $< $(LIB) $(LDLIBS)

# Builds every example, then runs them one by one; the first one that fails stops make
examples: $(EXAMPLE_BIN)
	@for program in $(EXAMPLE_BIN); do \
	    echo "=== $$program"; \
	    ./$$program || exit 1; \
	done

# The README shows examples/quick_start.cpp in full. This awk program, run over the file
# and then the README, compares the file line by line with the lines between the first
# "```cpp" and the next "```" of the README, and reports the first difference. Only awk is
# needed: diff is not installed everywhere (MSYS2, for example).
README_EXAMPLE := examples/quick_start.cpp

CHECK_DOCS_AWK = \
    FNR == NR { want[++n] = $$0; next } \
    !found && /^```cpp/ { found = inside = 1; next } \
    inside && /^```/ { inside = 0; next } \
    inside && bad == 0 { if (++m > n || $$0 != want[m]) { bad = m; got = $$0 } } \
    END { \
        if (!found) { print "README.md has no C++ example"; exit 1 } \
        if (bad == 0 && m < n) bad = m + 1; \
        if (bad == 0) exit 0; \
        sub(/\r$$/, "", got); line = want[bad]; sub(/\r$$/, "", line); \
        printf "README.md differs from $(README_EXAMPLE) at line %d of the example\n", bad; \
        printf "  file:   %s\n  README: %s\n", line, got; \
        exit 1 \
    }

check-docs:
	@awk '$(CHECK_DOCS_AWK)' $(README_EXAMPLE) README.md
	@echo "README.md matches $(README_EXAMPLE)"

# dist/alelus/ holds everything another project needs: the public headers and the
# optimised library. It is always built in release mode, whatever MODE says.
DIST := dist/alelus

dist:
	@$(MAKE) --no-print-directory MODE=release SANITIZE= all
	@rm -rf $(DIST)
	@mkdir -p $(DIST)/include $(DIST)/lib
	@cp -r include/alelus $(DIST)/include/
	@cp build/release/libalelus.a $(DIST)/lib/
	@cp README.md CHANGELOG.md LICENSE $(DIST)/
	@echo "Package ready in $(DIST)/"

format:
ifeq ($(FORMAT_FILES),)
	@echo "Nothing to format yet."
else
	@clang-format -i $(FORMAT_FILES)
	@echo "Formatted $(words $(FORMAT_FILES)) file(s)."
endif

clean:
	@rm -rf build dist
	@echo "build/ and dist/ deleted"

help:
	@# print the comment block at the top, up to the closing line of '=' signs
	@sed -n '2,/^# ====/{/^# ====/!p;}' Makefile

# Header dependencies, generated by -MMD. The "-" ignores them while they do not exist.
-include $(LIB_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(BENCH_OBJ:.o=.d) $(EXAMPLE_OBJ:.o=.d)
