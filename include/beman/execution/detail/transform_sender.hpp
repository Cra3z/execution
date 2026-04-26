// include/beman/execution/detail/transform_sender.hpp              _*_C++_*_
// SPDX_License_Identifier: Apache_2.0 WITH LLVM_exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_TRANSFORM_SENDER
#define INCLUDED_BEMAN_EXECUTION_DETAIL_TRANSFORM_SENDER

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.completion_domain;
import beman.execution.detail.default_domain;
import beman.execution.detail.get_domain;
import beman.execution.detail.sender;
import beman.execution.detail.set_value;
import beman.execution.detail.start;
#else
#include <beman/execution/detail/completion_domain.hpp>
#include <beman/execution/detail/default_domain.hpp>
#include <beman/execution/detail/get_domain.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/start.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {

template <typename Domain, typename Tag, typename Sndr, typename Env>
auto transformed_sndr(Domain dom, Tag tag, Sndr&& sndr, const Env& env) -> decltype(auto) {
    if constexpr (requires { dom.transform_sender(tag, ::std::forward<Sndr>(sndr), env); }) {
        return dom.transform_sender(tag, ::std::forward<Sndr>(sndr), env);
    } else {
        return ::beman::execution::default_domain().transform_sender(tag, ::std::forward<Sndr>(sndr), env);
    }
}

template <typename Domain, typename Tag>
struct transform_sndr_recurse {
    constexpr transform_sndr_recurse(Domain, Tag) noexcept {}

    template <typename Sndr, typename Env>
    auto operator()(Sndr&& sndr, const Env& env) -> decltype(auto) {
        decltype(auto) new_sndr =
            ::beman::execution::detail::transformed_sndr(Domain(), Tag(), ::std::forward<Sndr>(sndr), env);
        if constexpr (::std::same_as<::std::decay_t<Sndr>, ::std::decay_t<decltype(new_sndr)>>) {
            return static_cast<decltype(new_sndr)>(new_sndr);
        } else {
            if constexpr (::std::same_as<Tag, ::beman::execution::start_t>) {
                auto new_dom = ::beman::execution::get_domain(env);
                return transform_sndr_recurse{new_dom, Tag()}(::std::forward<decltype(new_sndr)>(new_sndr), env);
            } else {
                auto new_dom = ::beman::execution::detail::completion_domain(new_sndr, env);
                return transform_sndr_recurse{new_dom, Tag()}(::std::forward<decltype(new_sndr)>(new_sndr), env);
            }
        }
    }
};
} // namespace beman::execution::detail

namespace beman::execution {
template <::beman::execution::sender Sndr, typename Env>
auto transform_sender(Sndr&& sndr, const Env& env) -> ::beman::execution::sender decltype(auto) {
    auto starting_domain   = ::beman::execution::get_domain(env);
    auto completion_domain = ::beman::execution::detail::completion_domain(sndr, env);
    auto starting_transform =
        ::beman::execution::detail::transform_sndr_recurse(starting_domain, ::beman::execution::start);
    auto complete_transform =
        ::beman::execution::detail::transform_sndr_recurse(completion_domain, ::beman::execution::set_value);
    return starting_transform(complete_transform(::std::forward<Sndr>(sndr), env), env);
}
} // namespace beman::execution

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_TRANSFORM_SENDER
