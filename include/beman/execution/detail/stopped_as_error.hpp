// include/beman/execution/detail/stopped_as_error.hpp                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_STOPPED_AS_ERROR
#define INCLUDED_BEMAN_EXECUTION_DETAIL_STOPPED_AS_ERROR

// ----------------------------------------------------------------------------
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.completion_sources;
import beman.execution.detail.default_impls;
import beman.execution.detail.forward_like;
import beman.execution.detail.get_env;
import beman.execution.detail.just;
import beman.execution.detail.let;
import beman.execution.detail.make_sender;
import beman.execution.detail.movable_value;
import beman.execution.detail.sender;
import beman.execution.detail.sender_adaptor_closure;
import beman.execution.detail.sender_for;
import beman.execution.detail.set_error;
import beman.execution.detail.set_stopped;
import beman.execution.detail.set_value;
#else
#include <beman/execution/detail/completion_sources.hpp>
#include <beman/execution/detail/default_impls.hpp>
#include <beman/execution/detail/forward_like.hpp>
#include <beman/execution/detail/get_env.hpp>
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/let.hpp>
#include <beman/execution/detail/make_sender.hpp>
#include <beman/execution/detail/movable_value.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_adaptor_closure.hpp>
#include <beman/execution/detail/sender_for.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_stopped.hpp>
#include <beman/execution/detail/set_value.hpp>
#endif

namespace beman::execution::detail {
struct stopped_as_error_t {
    template <::beman::execution::detail::sender_for<stopped_as_error_t> Sndr, typename Env>
    auto transform_sender(::beman::execution::set_value_t, Sndr&& sndr, Env&&) const noexcept {
        auto&& data  = sndr.template get<1>();
        auto&& child = sndr.template get<2>();
        using Error  = ::std::decay_t<decltype(data)>;
        return ::beman::execution::let_stopped(
            ::beman::execution::detail::forward_like<Sndr>(child),
            [error = ::beman::execution::detail::forward_like<Sndr>(data)]() mutable noexcept(
                ::std::is_nothrow_move_constructible_v<Error>) {
                return ::beman::execution::just_error(::std::move(error));
            });
    }

    template <::beman::execution::sender Sndr, ::beman::execution::detail::movable_value Error>
    auto operator()(Sndr&& sndr, Error error) const {
        return ::beman::execution::detail::make_sender(
            stopped_as_error_t{}, std::move(error), ::std::forward<Sndr>(sndr));
    }

    template <::beman::execution::detail::movable_value Error>
    auto operator()(Error error) const {
        return ::beman::execution::detail::make_sender_adaptor(*this, ::std::move(error));
    }

  private:
    // stopped_as_error(child, err) completes inline: a stopped completion of the child becomes an error
    // completion (with err, or with an exception thrown while producing it); all others are forwarded.
    template <typename Child>
    struct sources {
        template <typename Tag, typename Source, typename... Env>
        static consteval auto value() -> bool {
            using result_t = ::std::conditional_t<::std::same_as<Source, ::beman::execution::set_stopped_t>,
                                                  ::beman::execution::set_error_t,
                                                  Source>;
            return ::std::same_as<Tag, result_t> &&
                   ::beman::execution::detail::may_complete_with<Source, Child, Env...>();
        }
    };

  public:
    struct impls_for : ::beman::execution::detail::default_impls {
        struct get_attrs_impl {
            template <typename Child>
            auto operator()(const auto&, const Child& child) const noexcept {
                return ::beman::execution::detail::child_completion_attrs<Child, sources<Child>>{
                    ::beman::execution::get_env(child)};
            }
        };
        static constexpr auto get_attrs{get_attrs_impl{}};
    };
};

} // namespace beman::execution::detail

namespace beman::execution {
using stopped_as_error_t = ::beman::execution::detail::stopped_as_error_t;
inline constexpr stopped_as_error_t stopped_as_error{};
} // namespace beman::execution

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_STOPPED_AS_ERROR
