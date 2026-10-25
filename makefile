
# Ensure Homebrew's pkgconfig is found (non-interactive shells don't source .bashrc,
# and MacPorts pkg-config is first in PATH). Brew prefix via `brew --prefix`
# (here /opt/homebrew, not ~/homebrew).
BREW_PREFIX := $(shell brew --prefix 2>/dev/null)
ifneq ($(BREW_PREFIX),)
export PKG_CONFIG_PATH := $(BREW_PREFIX)/lib/pkgconfig:$(PKG_CONFIG_PATH)
endif

RAYLIB_FLAGS := `pkg-config --cflags raylib`
# macOS links the STATIC raylib archive: the Homebrew dylib does not re-export
# the glfw symbols that afterhours's window_manager calls directly
# (glfwGetVideoModes, etc.). Other platforms use pkg-config.
ifeq ($(shell uname),Darwin)
RAYLIB_LIB := $(BREW_PREFIX)/opt/raylib/lib/libraylib.a -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -framework CoreFoundation -framework CoreGraphics
else
RAYLIB_LIB := `pkg-config --libs raylib`
endif

# afterhours headless_gl_macos.h sets GL_SILENCE_DEPRECATION after GLFW already
# pulled in gl.h; define it (empty, to match) and the multi-header opt-out up front.
GL_FLAGS := -DGL_SILENCE_DEPRECATION= -DGL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED

RELEASE_FLAGS = -std=c++23 -DAFTER_HOURS_USE_RAYLIB $(RAYLIB_FLAGS) $(GL_FLAGS)

WARN_FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wconversion -g
FLAGS = $(WARN_FLAGS) -DAFTER_HOURS_USE_RAYLIB $(RAYLIB_FLAGS) $(GL_FLAGS)
# No window, no GPU, no raylib: the afterhours none backend.
HEADLESS_FLAGS = $(WARN_FLAGS) -DAFTER_HOURS_USE_NONE

NOFLAGS = -Wno-deprecated-volatile -Wno-missing-field-initializers \
		  -Wno-c99-extensions -Wno-unused-function -Wno-sign-conversion \
		  -Wno-implicit-int-float-conversion \
		  -Wno-float-conversion -Werror
INCLUDES = -Ivendor/ -Isrc/
LIBS = -L. -Lvendor/ $(RAYLIB_LIB)

SRC_FILES := $(wildcard src/*.cpp src/**/*.cpp)
# Top-level src/*.h too: src/**/*.h alone only matches subdirectories, so
# editing a header next to main.cpp would silently skip the rebuild.
H_FILES := $(wildcard src/*.h src/**/*.h src/*.hpp src/**/*.hpp)
# The library is header-only, so its edits must trigger a rebuild too.
AH_FILES := $(shell find vendor/afterhours/src -name '*.h')
OBJ_DIR := ./output
OBJ_FILES := $(SRC_FILES:%.cpp=$(OBJ_DIR)/%.o)

DEPENDS := $(patsubst %.cpp,%.d,$(SRC_FILES))
-include $(DEPENDS)


OUTPUT_EXE := pong.exe
HEADLESS_EXE := pong_headless.exe

# NOTE: g++ (Homebrew GCC) cannot parse the macOS 15/26 SDK headers. Use clang++.
# (g++-14 is also not installed on this machine.)
CXX ?= clang++

.PHONY: all build headless run test e2e e2e-window clean

all: build

test: build
	test -x $(OUTPUT_EXE)

# All scripts on the headless build first; once they pass, one run on the
# real renderer in a hidden window writes tests/e2e/screenshots.
e2e: headless build
	./$(HEADLESS_EXE) --e2e --test-script-dir tests/e2e
	./$(OUTPUT_EXE) --e2e --test-script-dir tests/e2e \
		--screenshot-dir tests/e2e/screenshots --headless

# Same scripts in a visible window, to watch a run.
e2e-window: build
	./$(OUTPUT_EXE) --e2e --test-script-dir tests/e2e \
		--screenshot-dir tests/e2e/screenshots

build: $(OUTPUT_EXE)
headless: $(HEADLESS_EXE)

$(OUTPUT_EXE): $(SRC_FILES) $(H_FILES) $(AH_FILES) makefile
	$(CXX) $(FLAGS) $(NOFLAGS) $(INCLUDES) $(LIBS) $(SRC_FILES) -o $(OUTPUT_EXE)

$(HEADLESS_EXE): $(SRC_FILES) $(H_FILES) $(AH_FILES) makefile
	$(CXX) $(HEADLESS_FLAGS) $(NOFLAGS) $(INCLUDES) $(SRC_FILES) -o $(HEADLESS_EXE)

run: build
	./$(OUTPUT_EXE)

clean:
	rm -rf $(OUTPUT_EXE) $(OUTPUT_EXE).dSYM $(HEADLESS_EXE) $(HEADLESS_EXE).dSYM output
