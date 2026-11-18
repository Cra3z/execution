# gersemi: off
# examples/wasm/CMakeLists.txt -*-makefile-*-
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# gersemi: on

# Using beman.execution from JavaScript/TypeScript: senders and coroutines
# exposed as functions returning promises, see promise.hpp and module.cpp.
#
# The result is an ES module `beman_wasm.mjs` (+ .wasm and a generated .d.ts)
# in the directory below, together with demo.ts, index.html and serve.mjs.
#     node demo.ts       run the demo (this is the test)
#     node serve.mjs     serve index.html

set(WASM_TARGET ${PROJECT_NAME}.wasm)
set(WASM_SITE_DIR ${CMAKE_CURRENT_BINARY_DIR}/site)

add_executable(${WASM_TARGET} module.cpp promise.hpp timer.hpp)
target_link_libraries(${WASM_TARGET} PRIVATE beman::execution_headers)
set_target_properties(
    ${WASM_TARGET}
    PROPERTIES
        OUTPUT_NAME beman_wasm
        SUFFIX ".mjs"
        RUNTIME_OUTPUT_DIRECTORY ${WASM_SITE_DIR}
)
target_link_options(
    ${WASM_TARGET}
    PRIVATE
        -lembind
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sENVIRONMENT=web,worker,node
        --emit-tsd=${WASM_SITE_DIR}/beman_wasm.d.ts
)

foreach(file demo.ts index.html serve.mjs)
    add_custom_command(
        TARGET ${WASM_TARGET}
        POST_BUILD
        COMMAND
            ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/${file} ${WASM_SITE_DIR}/${file}
        VERBATIM
    )
endforeach()

# node >= 22.18 runs TypeScript directly
if(NOT CMAKE_CROSSCOMPILING_EMULATOR)
    find_program(NODE_JS_EXECUTABLE NAMES node nodejs)
    set(WASM_NODE ${NODE_JS_EXECUTABLE})
else()
    set(WASM_NODE ${CMAKE_CROSSCOMPILING_EMULATOR})
endif()
if(WASM_NODE)
    add_test(
        NAME ${WASM_TARGET}.demo
        COMMAND ${WASM_NODE} demo.ts
        WORKING_DIRECTORY ${WASM_SITE_DIR}
    )
    set_tests_properties(
        ${WASM_TARGET}.demo
        PROPERTIES PASS_REGULAR_EXPRESSION "wasm demo: ok"
    )
endif()
