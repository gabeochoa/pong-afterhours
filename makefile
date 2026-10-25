
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

RELEASE_FLAGS = -std=c++23 $(RAYLIB_FLAGS) $(GL_FLAGS)

FLAGS = -std=c++23 -Wall -Wextra -Wpedantic -Wuninitialized -Wshadow \
		-Wconversion -g $(RAYLIB_FLAGS) $(GL_FLAGS)

NOFLAGS = -Wno-deprecated-volatile -Wno-missing-field-initializers \
		  -Wno-c99-extensions -Wno-unused-function -Wno-sign-conversion \
		  -Wno-implicit-int-float-conversion -Werror
INCLUDES = -Ivendor/ -Isrc/
LIBS = -L. -Lvendor/ $(RAYLIB_LIB)

SRC_FILES := $(wildcard src/*.cpp src/**/*.cpp)
H_FILES := $(wildcard src/**/*.h src/**/*.hpp)
OBJ_DIR := ./output
OBJ_FILES := $(SRC_FILES:%.cpp=$(OBJ_DIR)/%.o)

DEPENDS := $(patsubst %.cpp,%.d,$(SRC_FILES))
-include $(DEPENDS)


OUTPUT_EXE := pong.exe

# NOTE: g++ (Homebrew GCC) cannot parse the macOS 15/26 SDK headers. Use clang++.
# (g++-14 is also not installed on this machine.)
CXX ?= clang++

.PHONY: all build run clean

all: build

build: $(OUTPUT_EXE)

$(OUTPUT_EXE): $(SRC_FILES) $(H_FILES) makefile
	$(CXX) $(FLAGS) $(INCLUDES) $(LIBS) $(SRC_FILES) -o $(OUTPUT_EXE)

run: build
	./$(OUTPUT_EXE)

clean:
	rm -rf $(OUTPUT_EXE) $(OUTPUT_EXE).dSYM output
