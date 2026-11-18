# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

# This toolchain file is not meant to be used directly,
# but to be invoked by CMake preset and GitHub CI.
#
# This toolchain file configures for WebAssembly using Emscripten (emsdk).
# The Emscripten SDK is located via, in this order:
#   - the environment variable EMSCRIPTEN_ROOT (the directory containing emcc)
#   - the environment variable EMSDK (after `source emsdk_env.sh`)
#   - emcc found in PATH (e.g. a distribution package)
#
# Tests and examples are executed through node (CMAKE_CROSSCOMPILING_EMULATOR),
# so `ctest` works like on any other platform.
#
# Optional CMake parameters (-D...):
# - BEMAN_EMSCRIPTEN_EXCEPTIONS: ON (default) builds with -fexceptions, OFF with
#   -fno-exceptions. Emscripten itself disables C++ exceptions by default.
# - BEMAN_EMSCRIPTEN_PTHREAD_POOL_SIZE: number of pre-created web workers used
#   for std::thread (default: 8). A pool is required because node/browsers can't
#   spawn a worker while the main thread blocks, e.g. in join().

include_guard(GLOBAL)

if(DEFINED ENV{EMSCRIPTEN_ROOT})
    set(_beman_emscripten_root "$ENV{EMSCRIPTEN_ROOT}")
elseif(DEFINED ENV{EMSDK})
    set(_beman_emscripten_root "$ENV{EMSDK}/upstream/emscripten")
else()
    find_program(_beman_emcc emcc)
    if(NOT _beman_emcc)
        message(
            FATAL_ERROR
            "Emscripten not found: set EMSDK (source emsdk_env.sh), EMSCRIPTEN_ROOT, or put emcc in PATH."
        )
    endif()
    # resolve symlinks, e.g. /usr/bin/emcc -> /usr/lib/emscripten/emcc
    file(REAL_PATH "${_beman_emcc}" _beman_emcc_real)
    cmake_path(GET _beman_emcc_real PARENT_PATH _beman_emscripten_root)
endif()

set(_beman_emscripten_platform
    "${_beman_emscripten_root}/cmake/Modules/Platform/Emscripten.cmake"
)
if(NOT EXISTS "${_beman_emscripten_platform}")
    message(
        FATAL_ERROR
        "Emscripten toolchain file not found: ${_beman_emscripten_platform}"
    )
endif()

# Run the tests with the node shipped with emsdk when no node is in PATH.
if(NOT DEFINED CMAKE_CROSSCOMPILING_EMULATOR AND DEFINED ENV{EMSDK_NODE})
    set(CMAKE_CROSSCOMPILING_EMULATOR "$ENV{EMSDK_NODE}" CACHE FILEPATH "")
endif()

include("${_beman_emscripten_platform}")

# Emscripten.cmake restricts find_package() to its sysroot (ONLY): that would
# ignore CMAKE_PREFIX_PATH, e.g. the install used by the find-package test.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

set(BEMAN_EMSCRIPTEN_EXCEPTIONS ON CACHE BOOL "Build with C++ exceptions")
set(BEMAN_EMSCRIPTEN_PTHREAD_POOL_SIZE
    8
    CACHE STRING
    "Size of the web worker pool used for std::thread"
)

if(BEMAN_EMSCRIPTEN_EXCEPTIONS)
    set(_beman_wasm_flags "-fexceptions")
else()
    set(_beman_wasm_flags "-fno-exceptions")
endif()

# -pthread has to be used consistently when compiling and linking: this is
# done by the beman.execution targets for their users, see src/beman/execution.
set(CMAKE_CXX_FLAGS_INIT "${_beman_wasm_flags}")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${_beman_wasm_flags} -sPTHREAD_POOL_SIZE=${BEMAN_EMSCRIPTEN_PTHREAD_POOL_SIZE}"
)

# Add this dir to the module path so that `find_package(beman-install-library)` works
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}")
