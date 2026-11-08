// include/beman/execution/detail/starts_on.hpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_STARTS_ON
#define INCLUDED_BEMAN_EXECUTION_DETAIL_STARTS_ON

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.child_type;
import beman.execution.detail.common_domain;
import beman.execution.detail.compl_domain;
import beman.execution.detail.completion_sources;
import beman.execution.detail.continues_on;
import beman.execution.detail.data_type;
import beman.execution.detail.default_impls;
import beman.execution.detail.dependent_sender;
import beman.execution.detail.env;
import beman.execution.detail.env_of_t;
import beman.execution.detail.forward_like;
import beman.execution.detail.fwd_env;
import beman.execution.detail.get_completion_domain;
import beman.execution.detail.get_completion_scheduler;
import beman.execution.detail.get_completion_signatures;
import beman.execution.detail.get_env;
import beman.execution.detail.hide_completion_queries;
import beman.execution.detail.join_env;
import beman.execution.detail.just;
import beman.execution.detail.let;
import beman.execution.detail.make_sender;
import beman.execution.detail.meta.combine;
import beman.execution.detail.schedule_result_t;
import beman.execution.detail.scheduler;
import beman.execution.detail.sender;
import beman.execution.detail.sender_for;
import beman.execution.detail.sender_in;
import beman.execution.detail.set_error;
import beman.execution.detail.set_value;
import beman.execution.detail.type_list;
#else
#include <beman/execution/detail/child_type.hpp>
#include <beman/execution/detail/common_domain.hpp>
#include <beman/execution/detail/compl_domain.hpp>
#include <beman/execution/detail/completion_sources.hpp>
#include <beman/execution/detail/continues_on.hpp>
#include <beman/execution/detail/data_type.hpp>
#include <beman/execution/detail/default_impls.hpp>
#include <beman/execution/detail/dependent_sender.hpp>
#include <beman/execution/detail/env.hpp>
#include <beman/execution/detail/env_of_t.hpp>
#include <beman/execution/detail/forward_like.hpp>
#include <beman/execution/detail/fwd_env.hpp>
#include <beman/execution/detail/get_completion_domain.hpp>
#include <beman/execution/detail/get_completion_scheduler.hpp>
#include <beman/execution/detail/get_completion_signatures.hpp>
#include <beman/execution/detail/get_env.hpp>
#include <beman/execution/detail/hide_completion_queries.hpp>
#include <beman/execution/detail/join_env.hpp>
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/let.hpp>
#include <beman/execution/detail/make_sender.hpp>
#include <beman/execution/detail/meta_combine.hpp>
#include <beman/execution/detail/schedule_result_t.hpp>
#include <beman/execution/detail/scheduler.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_for.hpp>
#include <beman/execution/detail/sender_in.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/type_list.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {
struct starts_on_t {
    template <::beman::execution::detail::sender_for<::beman::execution::detail::starts_on_t> Sender, typename Env>
        requires ::beman::execution::sender_in<::beman::execution::detail::child_type<Sender>,
                                               ::beman::execution::detail::fwd_env_t<Env>>
    static auto transform_sender(::beman::execution::set_value_t, Sender&& sender, const Env&) noexcept {
        auto&&         sched{sender.template get<1>()};
        auto&&         child{sender.template get<2>()};
        constexpr bool is_nothrow = ::std::is_nothrow_move_constructible_v<::std::remove_cvref_t<decltype(child)>>;
        return ::beman::execution::let_value(
            ::beman::execution::continues_on(::beman::execution::just(), sched),
            [child_ = ::beman::execution::detail::forward_like<Sender>(child)]() mutable noexcept(is_nothrow) {
                return ::std::move(child_);
            });
    }

  private:
    template <typename Scheduler>
    using start_sender =
        decltype(::beman::execution::continues_on(::beman::execution::just(), ::std::declval<Scheduler>()));

    // Starting the child can fail when scheduling or moving the child sender.
    template <typename Scheduler, typename Child>
    struct completion_map {
        template <typename Source, typename Tag, typename... Env>
        static consteval auto maps_to() -> bool {
            if constexpr (::std::same_as<Source, ::beman::execution::set_value_t>) {
                return ::std::same_as<Tag, ::beman::execution::set_error_t> &&
                       !::std::is_nothrow_move_constructible_v<Child> &&
                       ::beman::execution::detail::may_complete_with<Source, start_sender<Scheduler>, Env...>();
            } else {
                return ::std::same_as<Source, Tag> &&
                       ::beman::execution::detail::may_complete_with<Source, start_sender<Scheduler>, Env...>();
            }
        }
    };

    template <typename Scheduler, typename Child>
    struct attrs : ::beman::execution::detail::hide_completion_queries<::beman::execution::env_of_t<Child>> {
        using base        = ::beman::execution::detail::hide_completion_queries<::beman::execution::env_of_t<Child>>;
        using start_t     = start_sender<Scheduler>;
        using start_attrs = ::beman::execution::detail::transformed_attrs<start_t, completion_map<Scheduler, Child>>;

        template <typename Env>
        using child_env =
            ::beman::execution::detail::join_env<::beman::execution::let_value_t::template let_env_t<start_t, Env>,
                                                 ::beman::execution::detail::fwd_env_t<Env>>;

        template <typename Tag, typename... Env>
        static constexpr bool has_child_completion =
            ::beman::execution::detail::may_complete_with<Tag, Child, child_env<Env>...>();

        template <typename Tag, typename... Env>
        static constexpr bool has_start_completion =
            !::std::same_as<typename start_attrs::template sources<Tag, Env...>::type,
                            ::beman::execution::detail::type_list<>>;

        template <typename Tag, typename... Env>
            requires(!has_child_completion<Tag, Env...>)
        static auto child_domain() -> ::beman::execution::detail::type_list<>;

        template <typename Tag, typename... Env>
            requires has_child_completion<Tag, Env...>
        static auto child_domain() -> ::beman::execution::detail::type_list<
            ::beman::execution::detail::compl_domain_of_t<Tag, Child, child_env<Env>...>>;

        template <typename Tag, typename... Env>
            requires(!has_start_completion<Tag, Env...>)
        static auto start_domain() -> ::beman::execution::detail::type_list<>;

        template <typename Tag, typename... Env>
            requires has_start_completion<Tag, Env...>
        static auto start_domain()
            -> ::beman::execution::detail::type_list<decltype(::beman::execution::get_completion_domain<Tag>(
                ::std::declval<start_attrs>(), ::std::declval<const Env&>()...))>;

        template <typename... Domain>
            requires(sizeof...(Domain) != 0)
        static auto common_domain(::beman::execution::detail::type_list<Domain...>)
            -> ::beman::execution::detail::common_domain_t<Domain...>;

        attrs(Scheduler scheduler, ::beman::execution::env_of_t<Child> child_attrs) noexcept
            : base(::std::move(child_attrs)), sch(::std::move(scheduler)) {}

        template <typename Env>
        auto wrap_env(const Env& env) const noexcept {
            return ::beman::execution::detail::join_env(
                ::beman::execution::let_value_t::let_env(
                    ::beman::execution::continues_on(::beman::execution::just(), this->sch), env),
                ::beman::execution::detail::fwd_env(env));
        }

        using base::query;

        template <typename Tag, typename... Env>
            requires has_child_completion<Tag, Env...> && (!has_start_completion<Tag, Env...>)
        auto query(::beman::execution::get_completion_scheduler_t<Tag>, const Env&... env) const noexcept
            -> decltype(::beman::execution::get_completion_scheduler<Tag>(this->child_attrs,
                                                                          ::std::declval<child_env<Env>>()...)) {
            return ::beman::execution::get_completion_scheduler<Tag>(this->child_attrs, this->wrap_env(env)...);
        }

        template <typename Tag, typename... Env>
            requires(!has_child_completion<Tag, Env...>) && has_start_completion<Tag, Env...>
        auto query(::beman::execution::get_completion_scheduler_t<Tag>, const Env&... env) const noexcept
            -> decltype(::beman::execution::get_completion_scheduler<Tag>(::std::declval<start_attrs>(), env...)) {
            return ::beman::execution::get_completion_scheduler<Tag>(
                start_attrs{::beman::execution::get_env(
                    ::beman::execution::continues_on(::beman::execution::just(), this->sch))},
                env...);
        }

        template <typename Tag, typename... Env>
            requires(!::std::same_as<Tag, void>)
        auto query(::beman::execution::get_completion_domain_t<Tag>, const Env&...) const noexcept
            -> decltype(common_domain(
                ::beman::execution::detail::meta::combine<decltype(child_domain<Tag, Env...>()),
                                                          decltype(start_domain<Tag, Env...>())>{})) {
            return {};
        }

        Scheduler sch;
    };

    template <typename Sender>
    using transformed_sender = decltype(starts_on_t::transform_sender(
        ::beman::execution::set_value, ::std::declval<Sender>(), ::std::declval<::beman::execution::env<>>()));

  public:
    template <::beman::execution::sender Sender>
        requires(!::beman::execution::dependent_sender<
                    ::beman::execution::schedule_result_t<::beman::execution::detail::data_type<Sender>>>) &&
                ::beman::execution::sender_in<transformed_sender<Sender>>
    static consteval auto get_completion_signatures() {
        return ::beman::execution::get_completion_signatures<transformed_sender<Sender>>();
    }

    struct impls_for : ::beman::execution::detail::default_impls {
        struct get_attrs_impl {
            template <typename Sch, typename Child>
            auto operator()(const Sch& sch, const Child& child) const noexcept {
                return attrs<Sch, Child>{sch, ::beman::execution::get_env(child)};
            }
        };
        static constexpr auto get_attrs{get_attrs_impl{}};
    };

    template <::beman::execution::scheduler Scheduler, ::beman::execution::sender Sender>
    auto operator()(Scheduler&& scheduler, Sender&& sender) const {
        return ::beman::execution::detail::make_sender(
            *this, ::std::forward<Scheduler>(scheduler), ::std::forward<Sender>(sender));
    }
};
} // namespace beman::execution::detail

namespace beman::execution {
using starts_on_t = ::beman::execution::detail::starts_on_t;
inline constexpr ::beman::execution::detail::starts_on_t starts_on{};
} // namespace beman::execution

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_STARTS_ON
