// include/beman/execution/detail/on.hpp                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_ON
#define INCLUDED_BEMAN_EXECUTION_DETAIL_ON

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.basic_sender;
import beman.execution.detail.continues_on;
import beman.execution.detail.default_impls;
import beman.execution.detail.env_of_t;
import beman.execution.detail.forward_like;
import beman.execution.detail.fwd_env;
import beman.execution.detail.get_completion_domain;
import beman.execution.detail.get_completion_scheduler;
import beman.execution.detail.get_env;
import beman.execution.detail.get_start_scheduler;
import beman.execution.detail.hide_completion_queries;
import beman.execution.detail.make_sender;
import beman.execution.detail.product_type;
import beman.execution.detail.scheduler;
import beman.execution.detail.sender;
import beman.execution.detail.sender_adaptor_closure;
import beman.execution.detail.sender_for;
import beman.execution.detail.set_value;
import beman.execution.detail.starts_on;
#else
#include <beman/execution/detail/basic_sender.hpp>
#include <beman/execution/detail/continues_on.hpp>
#include <beman/execution/detail/default_impls.hpp>
#include <beman/execution/detail/env_of_t.hpp>
#include <beman/execution/detail/forward_like.hpp>
#include <beman/execution/detail/fwd_env.hpp>
#include <beman/execution/detail/get_completion_domain.hpp>
#include <beman/execution/detail/get_completion_scheduler.hpp>
#include <beman/execution/detail/get_env.hpp>
#include <beman/execution/detail/get_start_scheduler.hpp>
#include <beman/execution/detail/hide_completion_queries.hpp>
#include <beman/execution/detail/make_sender.hpp>
#include <beman/execution/detail/product_type.hpp>
#include <beman/execution/detail/scheduler.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_adaptor_closure.hpp>
#include <beman/execution/detail/sender_for.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/starts_on.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {
struct on_t {
    template <typename Env, typename OutSndr>
    static auto dry_run(OutSndr&& out_sndr) noexcept {
        auto& [_, data, child] = out_sndr;
        if constexpr (::beman::execution::scheduler<decltype(data)>) {
            return ::std::bool_constant<::std::invocable<::beman::execution::get_start_scheduler_t, Env>>{};
        } else {
            return ::std::bool_constant<
                ::std::invocable<::beman::execution::get_completion_scheduler_t<::beman::execution::set_value_t>,
                                 ::beman::execution::env_of_t<decltype(child)>,
                                 Env>>{};
        }
    }

    template <::beman::execution::detail::sender_for<on_t> OutSndr, typename Env>
        requires(decltype(on_t::dry_run<Env>(::std::declval<OutSndr>()))::value)
    static auto transform_sender(::beman::execution::set_value_t, OutSndr&& out_sndr, const Env& env)
        -> decltype(auto) {
        auto&& data  = out_sndr.template get<1>();
        auto&& child = out_sndr.template get<2>();

        if constexpr (::beman::execution::scheduler<decltype(data)>) {
            auto start_sch = ::beman::execution::get_start_scheduler(env);
            return ::beman::execution::continues_on(
                ::beman::execution::starts_on(::beman::execution::detail::forward_like<OutSndr>(data),
                                              ::beman::execution::detail::forward_like<OutSndr>(child)),
                ::std::move(start_sch));
        } else {
            auto& [sch, closure] = data;
            auto orig_sch        = ::beman::execution::get_completion_scheduler<::beman::execution::set_value_t>(
                ::beman::execution::get_env(child), env);

            return ::beman::execution::continues_on(
                ::beman::execution::detail::forward_like<OutSndr>(closure)(
                    ::beman::execution::continues_on(::beman::execution::detail::forward_like<OutSndr>(child), sch)),
                orig_sch);
        }
    }

    struct impls_for : ::beman::execution::detail::default_impls {
        struct get_attrs_impl {
            template <typename Sched, typename Child>
            auto operator()(const Sched& sch, const Child& child) const noexcept {
                return start_attrs<Sched, Child>{sch, ::beman::execution::get_env(child)};
            }

            template <typename Sched, typename Closure, typename Child>
            auto operator()(const ::beman::execution::detail::product_type<Sched, Closure>&,
                            const Child& child) const noexcept {
                return closure_attrs<Sched, Closure, Child>{::beman::execution::get_env(child)};
            }
        };

        static constexpr auto get_attrs{get_attrs_impl{}};
    };

    template <::beman::execution::scheduler Sch, ::beman::execution::sender Sndr>
        requires ::beman::execution::detail::is_sender_adaptor_closure<Sndr>
    auto operator()(Sch&&, Sndr&&) const -> void =
        BEMAN_EXECUTION_DELETE("on(sch, sndr) requires that sndr isn't both a sender and sender adaptor closure");

    template <::beman::execution::scheduler                         Sch,
              ::beman::execution::sender                            Sndr,
              ::beman::execution::detail::is_sender_adaptor_closure Closure>
        requires ::beman::execution::detail::is_sender_adaptor_closure<Sndr>
    auto operator()(Sndr&&, Sch&&, Closure&&) const -> void = BEMAN_EXECUTION_DELETE(
        "on(sndr, sch, closure) requires that sndr isn't both a sender and sender adaptor closure");

    template <::beman::execution::scheduler Sch, ::beman::execution::sender Sndr>
    auto operator()(Sch&& sch, Sndr&& sndr) const {
        return ::beman::execution::detail::make_sender(*this, ::std::forward<Sch>(sch), ::std::forward<Sndr>(sndr));
    }
    template <::beman::execution::scheduler                         Sch,
              ::beman::execution::sender                            Sndr,
              ::beman::execution::detail::is_sender_adaptor_closure Closure>
    auto operator()(Sndr&& sndr, Sch&& sch, Closure&& closure) const {
        return ::beman::execution::detail::make_sender(
            *this,
            ::beman::execution::detail::product_type{::std::forward<Sch>(sch), ::std::forward<Closure>(closure)},
            ::std::forward<Sndr>(sndr));
    }
    template <::beman::execution::scheduler Sch, ::beman::execution::detail::is_sender_adaptor_closure Closure>
    auto operator()(Sch&& sch, Closure&& closure) const {
        return ::beman::execution::detail::make_sender_adaptor(
            *this, ::std::forward<Sch>(sch), ::std::forward<Closure>(closure));
    }

  private:
    template <typename Data, typename Child, typename Env>
    using composed_attrs = ::beman::execution::env_of_t<decltype(on_t::transform_sender(
        ::beman::execution::set_value,
        ::std::declval<::beman::execution::detail::basic_sender<on_t, Data, Child>>(),
        ::std::declval<const Env&>()))>;

    template <typename Sched, typename Child>
    struct start_attrs : ::beman::execution::detail::hide_completion_queries<::beman::execution::env_of_t<Child>> {
        using base        = ::beman::execution::detail::hide_completion_queries<::beman::execution::env_of_t<Child>>;
        using inner_attrs = ::beman::execution::env_of_t<decltype(::beman::execution::starts_on(
            ::std::declval<Sched>(), ::std::declval<Child>()))>;

        start_attrs(Sched scheduler, ::beman::execution::env_of_t<Child> child_attrs) noexcept
            : base(::std::move(child_attrs)), sch(::std::move(scheduler)) {}

        using base::query;

        template <typename Tag, typename Env>
        auto query(::beman::execution::get_completion_scheduler_t<Tag>, const Env& env) const noexcept
            -> decltype(::beman::execution::get_completion_scheduler<Tag>(
                ::std::declval<composed_attrs<Sched, Child, Env>>(), ::beman::execution::detail::fwd_env(env))) {
            auto attrs = composed_attrs<Sched, Child, Env>{
                ::beman::execution::get_start_scheduler(env),
                ::beman::execution::detail::fwd_env(inner_attrs{this->sch, this->child_attrs})};
            return ::beman::execution::get_completion_scheduler<Tag>(attrs, ::beman::execution::detail::fwd_env(env));
        }

        template <typename Tag, typename Env>
            requires(!::std::same_as<Tag, void>)
        auto query(::beman::execution::get_completion_domain_t<Tag>, const Env&) const noexcept
            -> decltype(::beman::execution::get_completion_domain<Tag>(
                ::std::declval<composed_attrs<Sched, Child, Env>>(),
                ::std::declval<::beman::execution::detail::fwd_env_t<Env>>())) {
            return {};
        }

        Sched sch;
    };

    template <typename Sched, typename Closure, typename Child>
    struct closure_attrs : ::beman::execution::detail::hide_completion_queries<::beman::execution::env_of_t<Child>> {
        using base   = ::beman::execution::detail::hide_completion_queries<::beman::execution::env_of_t<Child>>;
        using data_t = ::beman::execution::detail::product_type<Sched, Closure>;

        template <typename Env>
        auto original_sched(const Env& env) const noexcept
            -> decltype(::beman::execution::get_completion_scheduler<::beman::execution::set_value_t>(
                this->child_attrs, env)) {
            return ::beman::execution::get_completion_scheduler<::beman::execution::set_value_t>(this->child_attrs,
                                                                                                 env);
        }

        using base::query;

        // The closure is not evaluated by an attribute query. Its completion agent need not be known
        // when the return scheduler's identity is independent of that agent.
        template <typename Tag, typename Env, typename Attrs = composed_attrs<data_t, Child, Env>>
            requires(!requires(
                        const closure_attrs& self,
                        const Env&           env,
                        const typename Attrs::template schedule_env_t<Tag, ::beman::execution::detail::fwd_env_t<Env>>&
                            source) {
                        self.original_sched(env).query(
                            ::beman::execution::get_completion_scheduler<::beman::execution::set_value_t>, source);
                    }) &&
                    ::std::same_as<
                        decltype(::beman::execution::get_completion_scheduler<Tag>(
                            ::std::declval<Attrs>(), ::std::declval<::beman::execution::detail::fwd_env_t<Env>>())),
                        decltype(::std::declval<const closure_attrs&>().original_sched(::std::declval<const Env&>()))>
        auto query(::beman::execution::get_completion_scheduler_t<Tag>, const Env& env) const noexcept {
            return this->original_sched(env);
        }

        template <typename Tag, typename Env>
            requires(!::std::same_as<Tag, void>)
        auto query(::beman::execution::get_completion_domain_t<Tag>, const Env&) const noexcept
            -> decltype(::beman::execution::get_completion_domain<Tag>(
                ::std::declval<composed_attrs<data_t, Child, Env>>(),
                ::std::declval<::beman::execution::detail::fwd_env_t<Env>>())) {
            return {};
        }
    };
};

} // namespace beman::execution::detail

namespace beman::execution {
using on_t = ::beman::execution::detail::on_t;
inline constexpr ::beman::execution::on_t on{};
} // namespace beman::execution

// ----------------------------------------------------------------------------

#include <beman/execution/detail/suppress_pop.hpp>

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_ON
