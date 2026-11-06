// include/beman/execution/detail/compl_domain.hpp             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_COMPL_DOMAIN
#define INCLUDED_BEMAN_EXECUTION_DETAIL_COMPL_DOMAIN

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <type_traits>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.indeterminate_domain;
import beman.execution.detail.get_completion_domain;
import beman.execution.detail.get_env;
#else
#include <beman/execution/detail/indeterminate_domain.hpp>
#include <beman/execution/detail/get_completion_domain.hpp>
#include <beman/execution/detail/get_env.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {
/*!
 * \brief COMPL-DOMAIN(Tag, sndr, envs) from [exec.snd.expos]
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 * \internal
 *
 * The completion domain of `sndr` for `Tag` if it is known. Otherwise, the domain is unknown: with an
 * environment that means an `indeterminate_domain`; without one the domain may still become known once
 * an environment is provided, so the expression is ill-formed.
 */
template <typename Tag = void, typename Sndr, typename... Envs>
    requires(sizeof...(Envs) != 0) || requires(const Sndr& sndr) {
        ::beman::execution::get_completion_domain<Tag>(::beman::execution::get_env(sndr));
    }
constexpr auto compl_domain(const Sndr& sndr, const Envs&... envs) noexcept {
    if constexpr (requires {
                      ::beman::execution::get_completion_domain<Tag>(::beman::execution::get_env(sndr), envs...);
                  }) {
        return ::beman::execution::get_completion_domain<Tag>(::beman::execution::get_env(sndr), envs...);
    } else {
        return ::beman::execution::indeterminate_domain();
    }
}

template <typename Tag, typename Sndr, typename... Envs>
using compl_domain_of_t =
    decltype(::beman::execution::detail::compl_domain<Tag>(::std::declval<Sndr>(), ::std::declval<const Envs&>()...));
} // namespace beman::execution::detail

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_COMPL_DOMAIN
