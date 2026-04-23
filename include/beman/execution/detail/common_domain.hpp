// include/beman/execution/detail/common_domain.hpp                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_COMMON_DOMAIN
#define INCLUDED_BEMAN_EXECUTION_DETAIL_COMMON_DOMAIN

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <type_traits>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.indeterminate_domain;
#else
#include <beman/execution/detail/indeterminate_domain.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {

// COMMON-DOMAIN(domains...):
//   common_type_t<decltype(auto(domains))...>() if well-formed,
//   otherwise indeterminate_domain<Ds...>() where Ds is the pack of types
//   consisting of decltype(auto(domains))... with duplicates removed.
template <typename... Domains>
struct common_domain_impl {
    using type = ::beman::execution::indeterminate_domain<Domains...>;
};

template <typename... Domains>
    requires requires { typename ::std::common_type_t<Domains...>; }
struct common_domain_impl<Domains...> {
    using type = ::std::common_type_t<Domains...>;
};

template <typename... Domains>
using common_domain_t = typename common_domain_impl<::std::decay_t<Domains>...>::type;

template <typename... Domains>
constexpr auto common_domain(Domains&&...) noexcept {
    return common_domain_t<Domains...>{};
}

} // namespace beman::execution::detail

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_COMMON_DOMAIN
