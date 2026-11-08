// include/beman/execution/detail/stopped_as_optional.hpp                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_STOPPED_AS_OPTIONAL
#define INCLUDED_BEMAN_EXECUTION_DETAIL_STOPPED_AS_OPTIONAL

// ----------------------------------------------------------------------------

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.child_type;
import beman.execution.detail.completion_sources;
import beman.execution.detail.default_impls;
import beman.execution.detail.env;
import beman.execution.detail.fwd_env;
import beman.execution.detail.get_env;
import beman.execution.detail.just;
import beman.execution.detail.let;
import beman.execution.detail.make_sender;
import beman.execution.detail.sender;
import beman.execution.detail.sender_adaptor_closure;
import beman.execution.detail.sender_for;
import beman.execution.detail.sender_in;
import beman.execution.detail.set_error;
import beman.execution.detail.set_stopped;
import beman.execution.detail.set_value;
import beman.execution.detail.single_sender;
import beman.execution.detail.single_sender_value_type;
import beman.execution.detail.then;
#else
#include <beman/execution/detail/child_type.hpp>
#include <beman/execution/detail/completion_sources.hpp>
#include <beman/execution/detail/default_impls.hpp>
#include <beman/execution/detail/env.hpp>
#include <beman/execution/detail/fwd_env.hpp>
#include <beman/execution/detail/get_env.hpp>
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/let.hpp>
#include <beman/execution/detail/make_sender.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_adaptor_closure.hpp>
#include <beman/execution/detail/sender_for.hpp>
#include <beman/execution/detail/sender_in.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_stopped.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/single_sender.hpp>
#include <beman/execution/detail/single_sender_value_type.hpp>
#include <beman/execution/detail/then.hpp>
#endif

namespace beman::execution::detail {
struct stopped_as_optional_t {
    template <::beman::execution::detail::sender_for<stopped_as_optional_t> Sndr, typename Env>
    auto transform_sender(::beman::execution::set_value_t, Sndr&& sndr, Env&&) const noexcept {
        static_assert(
            ::beman::execution::detail::single_sender<child_type<Sndr>, ::beman::execution::detail::fwd_env<Env>>,
            "sender must be a single sender");
        using value_type =
            ::beman::execution::detail::single_sender_value_type<child_type<Sndr>,
                                                                 ::beman::execution::detail::fwd_env<Env>>;
        static_assert(!::std::is_void_v<value_type>, "the value type of sender can't be empty");
        return ::beman::execution::let_stopped(
            ::beman::execution::then(
                ::std::forward<Sndr>(sndr).template get<2>(),
                []<class... Args>(Args&&... args) noexcept(::std::is_nothrow_constructible_v<value_type, Args...>) {
                    return ::std::optional<value_type>(::std::in_place, ::std::forward<Args>(args)...);
                }),
            []() noexcept { return ::beman::execution::just(::std::optional<value_type>()); });
    }

    template <::beman::execution::sender Sndr>
    auto operator()(Sndr&& sndr) const {
        return ::beman::execution::detail::make_sender(stopped_as_optional_t{}, {}, ::std::forward<Sndr>(sndr));
    }

    auto operator()() const noexcept { return ::beman::execution::detail::make_sender_adaptor(*this); }

  private:
    template <typename... Env>
    struct value_type_env {
        using type = ::beman::execution::env<>;
    };
    template <typename Env>
    struct value_type_env<Env> {
        using type = Env;
    };

    // Values and stopped completions produce optionals. Constructing an optional may produce an error.
    template <typename Child>
    struct completion_map {
        template <typename Source, typename... Env>
        static consteval auto may_throw() -> bool {
            using env_t = typename value_type_env<Env...>::type;
            if constexpr (::beman::execution::sender_in<Child, Env...> &&
                          ::beman::execution::detail::single_sender<Child, env_t>) {
                using value_t = ::beman::execution::detail::single_sender_value_type<Child, env_t>;
                if constexpr (::std::same_as<Source, ::beman::execution::set_value_t>)
                    return ::beman::execution::detail::may_complete_with<Source, Child, Env...>(
                        []<typename... Args> { return !::std::is_nothrow_constructible_v<value_t, Args...>; });
                else // the empty optional is moved into the operation state of just(optional())
                    return ::beman::execution::detail::may_complete_with<Source, Child, Env...>() &&
                           !::std::is_nothrow_move_constructible_v<::std::optional<value_t>>;
            } else {
                return ::beman::execution::detail::may_complete_with<Source, Child, Env...>();
            }
        }

        template <typename Source, typename Tag, typename... Env>
        static consteval auto maps_to() -> bool {
            if constexpr (::std::same_as<Tag, ::beman::execution::set_error_t> &&
                          !::std::same_as<Source, ::beman::execution::set_error_t>) {
                return may_throw<Source, Env...>();
            } else {
                using result_t = ::std::conditional_t<::std::same_as<Source, ::beman::execution::set_stopped_t>,
                                                      ::beman::execution::set_value_t,
                                                      Source>;
                return ::std::same_as<Tag, result_t> &&
                       ::beman::execution::detail::may_complete_with<Source, Child, Env...>();
            }
        }
    };

  public:
    struct impls_for : ::beman::execution::detail::default_impls {
        struct get_attrs_impl {
            template <typename Child>
            auto operator()(const auto&, const Child& child) const noexcept {
                return ::beman::execution::detail::transformed_attrs<Child, completion_map<Child>>{
                    ::beman::execution::get_env(child)};
            }
        };
        static constexpr auto get_attrs{get_attrs_impl{}};
    };
};

} // namespace beman::execution::detail

namespace beman::execution {
using stopped_as_optional_t = ::beman::execution::detail::stopped_as_optional_t;
inline constexpr stopped_as_optional_t stopped_as_optional{};
} // namespace beman::execution

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_STOPPED_AS_OPTIONAL
