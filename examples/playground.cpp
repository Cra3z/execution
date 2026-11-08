// examples/playground.cpp                                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <iostream>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
#else
#include <beman/execution.hpp>
#endif

namespace ex = ::beman::execution;

// ----------------------------------------------------------------------------

int main() {
    ex::run_loop loop;
    auto sndr = ex::just() | ex::continues_on(loop.get_scheduler());
    auto sch = ex::get_completion_scheduler<ex::set_value_t>(ex::get_env(sndr), ex::env{});
}
