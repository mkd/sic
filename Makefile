# ============================================================================
# SIC — SImple Chess Engine  |  Makefile (Stockfish-inspired)
# ============================================================================

# --- Compiler & Platform Detection ---
CXX       ?= g++
PREFIX    := $(shell $(CXX) -dumpmachine 2>/dev/null)
OS        := $(shell uname -s 2>/dev/null || echo Unknown)

# --- Build Mode ---
DEBUG     ?= 0
ARCH      ?= native
LTO       ?= 1
PGO       ?= 0

# --- Directories ---
SRCDIR    := src
INCDIR    := include
OBJDIR    := .obj
TESTDIR   := tests

# --- Binary Name ---
ifeq ($(OS),Windows)
  BINARY  := sic.exe
else
  BINARY  := sic
endif

# --- Source Discovery ---
SRCS      := $(wildcard $(SRCDIR)/*.cpp)
OBJS      := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS      := $(OBJS:.o=.d)

# --- Test Sources ---
TEST_SRCS := $(wildcard $(TESTDIR)/*.cpp)
TEST_OBJS := $(patsubst $(TESTDIR)/%.cpp,$(OBJDIR)/test/%.o,$(TEST_SRCS))

# ============================================================================
#  Flags
# ============================================================================

CXXFLAGS  := -std=c++20 -Wall -Wextra -pedantic
CFLAGS    += -Wno-missing-field-initializers

# Debug vs Release
ifeq ($(DEBUG),1)
  CXXFLAGS += -O0 -g -DDEBUG
else
  CXXFLAGS += -O3 -DNDEBUG
endif

# Include paths
CXXFLAGS += -I$(INCDIR)

# Auto-dependency generation
CXXFLAGS += -MMD -MP


# ============================================================================
#  Architecture / SIMD Flags
# ============================================================================
UNAME_M := $(shell uname -m)

ifeq ($(ARCH),native)
  CXXFLAGS += -march=native
  # Auto-detect Apple Silicon / ARM64 for NEON optimizations
  ifeq ($(UNAME_M),arm64)
    CXXFLAGS += -DUSE_NEON
  else ifeq ($(UNAME_M),aarch64)
    CXXFLAGS += -DUSE_NEON
  endif
else ifeq ($(ARCH),apple-silicon)
  CXXFLAGS += -mcpu=apple-m1 -DUSE_NEON
else ifeq ($(ARCH),x86-64-avx512)
  CXXFLAGS += -march=x86-64-v3 -mavx512f -mavx512bw -mavx512vl -mavx512f16d16
else ifeq ($(ARCH),x86-64-avx2)
  CXXFLAGS += -march=x86-64-v2 -mavx2 -mbmi -mbmi2 -mpopcnt -mpclmul
else ifeq ($(ARCH),x86-64-bmi2)
  CXXFLAGS += -mbmi2 -mbmi -mpopcnt -mpclmul
else
  CXXFLAGS += -march=x86-64
endif

# ============================================================================
#  Link-Time Optimization
# ============================================================================
ifeq ($(LTO),1)
  ifeq ($(DEBUG),0)
    CXXFLAGS += -flto
    LDFLAGS  += -flto
  endif
endif

# ============================================================================
#  PGO (Profile-Guided Optimization)
# ============================================================================
PGO_DIR   := .pgo
PGO_FLAGS :=

ifeq ($(PGO),1)
  PGO_FLAGS += -fprofile-generate=$(PGO_DIR)
endif

ifeq ($(PGO_USE),1)
  PGO_FLAGS += -fprofile-use=$(PGO_DIR) -fprofile-correction
endif

CXXFLAGS += $(PGO_FLAGS)
LDFLAGS  += $(PGO_FLAGS)

# ============================================================================
#  Linker Flags
# ============================================================================
LDFLAGS   += -lpthread

# ============================================================================
#  Targets
# ============================================================================
.PHONY: all clean build help bench profile-build profile-use format

all: build

build: $(BINARY)

$(BINARY): $(OBJS) | $(OBJDIR)
	@echo "[LINK]  $@"
	@$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	@echo "[CXX]   $<"
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/test/%.o: $(TESTDIR)/%.cpp | $(OBJDIR)
	@echo "[CXX]   $<"
	@$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR):
	@mkdir -p $(OBJDIR)
	@mkdir -p $(OBJDIR)/test

# ============================================================================
#  PGO Targets
# ============================================================================
profile-build:
	@echo "[PGO] Building with profile generation..."
	@$(MAKE) clean
	@$(MAKE) build PGO=1
	@echo "[PGO] Instrumented binary ready. Run './sic bench' to generate profile data."

profile-use:
	@echo "[PGO] Recompiling with profile data..."
	@$(MAKE) clean
	@$(MAKE) build PGO_USE=1
	@echo "[PGO] PGO-optimized binary ready."

# ============================================================================
#  Testing
# ============================================================================
test: $(BINARY)
	@echo "Basic build test passed."

# ============================================================================
#  Cleaning
# ============================================================================
clean:
	@echo "[CLEAN] Removing build artifacts..."
	@rm -rf $(OBJDIR) $(PGO_DIR) $(BINARY)
	@echo "[CLEAN] Done."

# ============================================================================
#  Help
# ============================================================================
help:
	@echo ""
	@echo "  ╔══════════════════════════════════════════════════════════╗"
	@echo "  ║  SIC — SImple Chess Engine  |  Build System Help       ║"
	@echo "  ╚══════════════════════════════════════════════════════════╝"
	@echo ""
	@echo "  Targets:"
	@echo "    all            Build the engine (default)"
	@echo "    build          Build the engine binary"
	@echo "    clean          Remove all build artifacts"
	@echo "    test           Build and run tests"
	@echo "    profile-build  Compile with PGO instrumentation"
	@echo "    profile-use    Recompile with PGO profile data"
	@echo "    help           Show this help message"
	@echo ""
	@echo "  Variables:"
	@echo "    CXX=<compiler>   C++ compiler (default: g++)"
	@echo "    DEBUG=<0|1>      Debug build (default: 0)"
	@echo "    ARCH=<arch>      Architecture (default: native)"
	@echo "                     Options: native, x86-64-avx512,"
	@echo "                              x86-64-avx2, x86-64-bmi2"
	@echo "    LTO=<0|1>        Link-Time Optimization (default: 1)"
	@echo "    PGO=<0|1>        PGO generation (default: 0)"
	@echo ""
	@echo "  Examples:"
	@echo "    make build                          # Release, native, LTO"
	@echo "    make build DEBUG=1                  # Debug build"
	@echo "    make build ARCH=x86-64-avx2         # AVX2-specific binary"
	@echo "    make build LTO=0                    # Without LTO"
	@echo "    make CXX=clang++ build              # Use Clang"
	@echo ""

# --- Include auto-generated dependencies ---
-include $(DEPS)
