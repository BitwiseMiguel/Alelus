# =============================================================================
# Alelus — Makefile
#
#   make             build the library (when there is code in src/)
#   make tests       build and run the tests (when there are tests in tests/)
#   make bench       build (in release mode) and run the benchmark; B=name filters it
#   make dist        package the release library and headers in dist/alelus/
#   make format      format every source file with clang-format
#   make clean       delete the build/ and dist/ folders
#   make help        show this help
#
# Options:
#   MODE=release     optimised build (-O2) instead of debug (-g -O0)
#   WERROR=1         treat compiler warnings as errors
#   T="name"         make tests: run only the tests whose name or tag matches, e.g. T=[fast]
#   B="name"         make bench: run only the measurements whose name contains it
#   CXX=clang++      use another compiler (default: g++)
#
# Files are found automatically: every src/*.cpp goes into the library and every
# tests/*.cpp goes into the test program. You do not need to edit this file to add them.
# =============================================================================

MODE  ?= debug
BUILD := build/$(MODE)

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

FORMAT_FILES := $(wildcard include/alelus/*.hpp include/alelus/*/*.hpp src/*.cpp tests/*.cpp tests/*/*.hpp bench/*.cpp)

# --- Targets -------------------------------------------------------------------
.DEFAULT_GOAL := all
.PHONY: all tests bench bench-binary dist format clean help

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
	@$(MAKE) --no-print-directory MODE=release bench-binary
	@./build/release/alelus_bench$(EXE) $(if $(B),"$(B)")

# dist/alelus/ holds everything another project needs: the public headers and the
# optimised library. It is always built in release mode, whatever MODE says.
DIST := dist/alelus

dist:
	@$(MAKE) --no-print-directory MODE=release all
	@rm -rf $(DIST)
	@mkdir -p $(DIST)/include $(DIST)/lib
	@cp -r include/alelus $(DIST)/include/
	@cp build/release/libalelus.a $(DIST)/lib/
	@cp README.md LICENSE $(DIST)/
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
-include $(LIB_OBJ:.o=.d) $(TEST_OBJ:.o=.d) $(BENCH_OBJ:.o=.d)
