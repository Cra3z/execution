// include/beman/execution/detail/completion_domain.hpp             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_COMPLETION_DOMAIN
#define INCLUDED_BEMAN_EXECUTION_DETAIL_COMPLETION_DOMAIN

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.default_domain;
import beman.execution.detail.get_completion_domain;
#else
#include <beman/execution/detail/default_domain.hpp>
#include <beman/execution/detail/get_completion_domain.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {
template <typename Sndr, typename Env>
constexpr auto completion_domain(const Sndr& sndr, const Env& env) noexcept {
    if constexpr (requires { ::beman::execution::get_completion_domain<>(::beman::execution::get_env(sndr), env); }) {
        return ::beman::execution::get_completion_domain<>(::beman::execution::get_env(sndr), env);
    } else {
        return ::beman::execution::default_domain();
    }
}
} // namespace beman::execution::detail

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_COMPLETION_DOMAIN
