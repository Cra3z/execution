// examples/playground.cpp                                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <iostream>
#include <string>
#include <tuple>
#include <variant>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
#else
#include <beman/execution.hpp>
#endif

namespace ex = ::beman::execution;

// ----------------------------------------------------------------------------

int main() {
    auto sndr = ex::just_error(0) | ex::upon_error([](int i) noexcept {
        return i;
    });
    auto sch = ex::get_completion_scheduler<ex::set_value_t>(ex::get_env(sndr), ex::prop{
        ex::get_start_scheduler,
        ex::get_parallel_scheduler()
    });
    auto domain = ex::get_completion_domain<ex::set_value_t>(ex::get_env(sndr), ex::prop{
        ex::get_start_scheduler,
        ex::get_parallel_scheduler()
    });
    static_assert(std::same_as<decltype(domain), void>);
}
