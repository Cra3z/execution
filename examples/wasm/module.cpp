// examples/wasm/module.cpp                                           -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// The WebAssembly module used by demo.ts and index.html: senders and tasks
// exposed to JavaScript/TypeScript as functions returning promises. Each
// function can be called with an AbortSignal as the last argument which
// requests the C++ operation to stop: the promise is then rejected with an
// AbortError.

#include "promise.hpp"
#include "timer.hpp"
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <string>
#include <beman/execution.hpp>

namespace {
namespace ex = beman::execution;
using emscripten::val;

// ----------------------------------------------------------------------------
// the C++ side: a coroutine and sender algorithms, knowing nothing about JavaScript

// sum(a, b): a coroutine awaiting two promises
auto sum_task(val a, val b) -> ex::task<double> {
    auto x = co_await wasm::from_promise(std::move(a));
    auto y = co_await wasm::from_promise(std::move(b));
    co_return x.as<double>() + y.as<double>();
}

// all(a, b): both promises, fails (and stops the other one) if one fails
auto all_sender(val a, val b) {
    return ex::when_all(wasm::from_promise(std::move(a)), wasm::from_promise(std::move(b)));
}

// delay(ms, value): `value` after ms milliseconds
auto delay_sender(double ms, val value) {
    return wasm::sleep_for(ms) | ex::then([value = std::move(value)] { return value; });
}

// abortable(promise, signal): the promise, but a stop request completes it right
// away. With AbortSignal.timeout(ms) this is a timeout for any promise.
auto abortable_sender(val promise) { return wasm::from_promise(std::move(promise)); }
} // namespace

// ----------------------------------------------------------------------------
// the bindings: each function with and without the optional AbortSignal

BEMAN_WASM_PROMISE_BINDINGS

EMSCRIPTEN_BINDINGS(beman_wasm_example) {
    using emscripten::function;
    using emscripten::val;

    function("sum", +[](val a, val b) { return wasm::to_promise(sum_task(a, b)); });
    function("sum", +[](val a, val b, val signal) { return wasm::to_promise(sum_task(a, b), signal); });

    function("all", +[](val a, val b) { return wasm::to_promise(all_sender(a, b)); });
    function("all", +[](val a, val b, val signal) { return wasm::to_promise(all_sender(a, b), signal); });

    function("delay", +[](double ms, val value) { return wasm::to_promise(delay_sender(ms, value)); });
    function("delay", +[](double ms, val value, val signal) {
        return wasm::to_promise(delay_sender(ms, value), signal);
    });

    function("abortable", +[](val promise, val signal) { return wasm::to_promise(abortable_sender(promise), signal); });
}
