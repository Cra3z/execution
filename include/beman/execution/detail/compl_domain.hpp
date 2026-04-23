// include/beman/execution/detail/compl_domain.hpp                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_COMPL_DOMAIN
#define INCLUDED_BEMAN_EXECUTION_DETAIL_COMPL_DOMAIN

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <type_traits>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.get_completion_domain;
import beman.execution.detail.get_env;
import beman.execution.detail.indeterminate_domain;
#else
#include <beman/execution/detail/get_completion_domain.hpp>
#include <beman/execution/detail/get_env.hpp>
#include <beman/execution/detail/indeterminate_domain.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {

// COMPL-DOMAIN(Tag, sndr, envs):
//   D() where D is the type of get_completion_domain<Tag>(get_env(sndr), envs...)
//   if that expression is well-formed or envs is empty pack,
//   and indeterminate_domain<>() otherwise.
template <typename Tag, typename Sndr, typename... Envs>
constexpr auto compl_domain(const Sndr& sndr, const Envs&... envs) noexcept {
    if constexpr (requires {
                      ::beman::execution::get_completion_domain<Tag>(::beman::execution::get_env(sndr), envs...);
                  }) {
        return decltype(
            ::beman::execution::get_completion_domain<Tag>(::beman::execution::get_env(sndr), envs...)){};
    } else if constexpr (sizeof...(Envs) == 0) {
        // If envs is empty, the expression should still be tried but may be ill-formed
        // In that case, return indeterminate_domain
        return ::beman::execution::indeterminate_domain<>{};
    } else {
        return ::beman::execution::indeterminate_domain<>{};
    }
}

template <typename Tag, typename Sndr, typename... Envs>
using compl_domain_t = decltype(compl_domain<Tag>(::std::declval<const Sndr&>(), ::std::declval<const Envs&>()...));

} // namespace beman::execution::detail

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_COMPL_DOMAIN
