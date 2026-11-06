// include/beman/execution/detail/continues_on.hpp                  -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_CONTINUES_ON
#define INCLUDED_BEMAN_EXECUTION_DETAIL_CONTINUES_ON

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <exception>
#include <type_traits>
#include <tuple>
#include <utility>
#include <variant>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.as_tuple;
import beman.execution.detail.basic_sender;
import beman.execution.detail.child_type;
import beman.execution.detail.common_domain;
import beman.execution.detail.compl_domain;
import beman.execution.detail.completion_signature;
import beman.execution.detail.completion_signatures;
import beman.execution.detail.completion_signatures_for;
import beman.execution.detail.completion_signatures_of_t;
import beman.execution.detail.completion_sources;
import beman.execution.detail.connect;
import beman.execution.detail.connect_result_t;
import beman.execution.detail.decayed_tuple;
import beman.execution.detail.default_impls;
import beman.execution.detail.env;
import beman.execution.detail.env_of_t;
import beman.execution.detail.error_types_of_t;
import beman.execution.detail.fwd_env;
import beman.execution.detail.gather_signatures;
import beman.execution.detail.get_completion_domain;
import beman.execution.detail.get_completion_scheduler;
import beman.execution.detail.get_completion_signatures;
import beman.execution.detail.get_domain;
import beman.execution.detail.get_env;
import beman.execution.detail.get_start_scheduler;
import beman.execution.detail.hide_completion_queries;
import beman.execution.detail.impls_for;
import beman.execution.detail.make_sender;
import beman.execution.detail.meta.combine;
import beman.execution.detail.meta.filter;
import beman.execution.detail.meta.prepend;
import beman.execution.detail.meta.to;
import beman.execution.detail.meta.transform;
import beman.execution.detail.meta.unique;
import beman.execution.detail.receiver;
import beman.execution.detail.schedule;
import beman.execution.detail.schedule_from;
import beman.execution.detail.schedule_result_t;
import beman.execution.detail.scheduler;
import beman.execution.detail.sender;
import beman.execution.detail.sender_adaptor_closure;
import beman.execution.detail.sender_in;
import beman.execution.detail.set_error;
import beman.execution.detail.set_stopped;
import beman.execution.detail.set_value;
import beman.execution.detail.start;
import beman.execution.detail.type_list;
#else
#include <beman/execution/detail/as_tuple.hpp>
#include <beman/execution/detail/child_type.hpp>
#include <beman/execution/detail/common_domain.hpp>
#include <beman/execution/detail/compl_domain.hpp>
#include <beman/execution/detail/completion_signatures_of_t.hpp>
#include <beman/execution/detail/completion_sources.hpp>
#include <beman/execution/detail/connect.hpp>
#include <beman/execution/detail/decayed_tuple.hpp>
#include <beman/execution/detail/default_impls.hpp>
#include <beman/execution/detail/env.hpp>
#include <beman/execution/detail/env_of_t.hpp>
#include <beman/execution/detail/error_types_of_t.hpp>
#include <beman/execution/detail/fwd_env.hpp>
#include <beman/execution/detail/gather_signatures.hpp>
#include <beman/execution/detail/get_completion_domain.hpp>
#include <beman/execution/detail/get_completion_scheduler.hpp>
#include <beman/execution/detail/get_domain.hpp>
#include <beman/execution/detail/get_env.hpp>
#include <beman/execution/detail/get_start_scheduler.hpp>
#include <beman/execution/detail/hide_completion_queries.hpp>
#include <beman/execution/detail/impls_for.hpp>
#include <beman/execution/detail/make_sender.hpp>
#include <beman/execution/detail/meta_combine.hpp>
#include <beman/execution/detail/meta_filter.hpp>
#include <beman/execution/detail/meta_prepend.hpp>
#include <beman/execution/detail/meta_to.hpp>
#include <beman/execution/detail/meta_transform.hpp>
#include <beman/execution/detail/meta_unique.hpp>
#include <beman/execution/detail/schedule.hpp>
#include <beman/execution/detail/schedule_from.hpp>
#include <beman/execution/detail/schedule_result_t.hpp>
#include <beman/execution/detail/scheduler.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_adaptor.hpp>
#include <beman/execution/detail/sender_in.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_stopped.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/start.hpp>
#include <beman/execution/detail/type_list.hpp>
#endif

// ----------------------------------------------------------------------------

#include <beman/execution/detail/suppress_push.hpp>

namespace beman::execution::detail {

struct continues_on_t {
    template <::beman::execution::scheduler Scheduler>
    auto operator()(Scheduler&& scheduler) const {
        return ::beman::execution::detail::make_sender_adaptor(*this, ::std::forward<Scheduler>(scheduler));
    }

    template <::beman::execution::sender Sender, ::beman::execution::scheduler Scheduler>
    auto operator()(Sender&& sender, Scheduler&& scheduler) const {
        return ::beman::execution::detail::make_sender(
            *this,
            ::std::forward<Scheduler>(scheduler),
            ::beman::execution::schedule_from(::std::forward<Sender>(sender)));
    }

  private:
    template <typename Tag>
    using is_not_set_value = ::std::negation<::beman::execution::detail::is_set_value<Tag>>;

    template <typename>
    struct is_nothrow_decay_copy_sig;

    template <typename Tag, typename... Args>
    struct is_nothrow_decay_copy_sig<Tag(Args...)>
        : ::std::conjunction<::std::is_nothrow_constructible<::std::decay_t<Args>, Args>...> {};

    template <typename>
    struct is_nothrow_decay_copy_compl_sigs;

    template <typename... Sigs>
    struct is_nothrow_decay_copy_compl_sigs<::beman::execution::completion_signatures<Sigs...>>
        : ::std::conjunction<is_nothrow_decay_copy_sig<Sigs>...> {};

    template <typename>
    struct decay_sig;

    template <typename Tag, typename... Args>
    struct decay_sig<Tag(Args...)> {
        using type = Tag(::std::decay_t<Args>...);
    };

    template <typename T>
    using decay_sig_t = typename decay_sig<T>::type;

    template <typename Child, typename Env>
    using variant_type = ::beman::execution::detail::meta::unique<::beman::execution::detail::meta::prepend<
        ::std::monostate,
        ::beman::execution::detail::meta::transform<
            ::beman::execution::detail::as_tuple_t,
            ::beman::execution::detail::meta::to<
                ::std::variant,
                ::beman::execution::detail::meta::combine<
                    ::beman::execution::completion_signatures_of_t<Child, Env>,
                    ::std::conditional_t<is_nothrow_decay_copy_compl_sigs<
                                             ::beman::execution::completion_signatures_of_t<Child, Env>>::value,
                                         ::beman::execution::completion_signatures<>,
                                         ::beman::execution::completion_signatures<::beman::execution::set_error_t(
                                             ::std::exception_ptr)>>>>>>>;

    template <typename, typename...>
    struct get_signatures;

    template <typename Scheduler, typename Sender, typename... Env>
        requires ::beman::execution::sender_in<::beman::execution::schedule_result_t<Scheduler>,
                                               ::beman::execution::detail::fwd_env_t<Env>...> &&
                 ::beman::execution::sender_in<Sender, ::beman::execution::detail::fwd_env_t<Env>...>
    struct get_signatures<
        ::beman::execution::detail::basic_sender<::beman::execution::detail::continues_on_t, Scheduler, Sender>,
        Env...> {
        static consteval auto get() {
            [[maybe_unused]] auto child_sigs =
                ::beman::execution::get_completion_signatures<Sender, ::beman::execution::detail::fwd_env_t<Env>...>();
            [[maybe_unused]] auto sched_sigs =
                ::beman::execution::get_completion_signatures<::beman::execution::schedule_result_t<Scheduler>,
                                                              ::beman::execution::detail::fwd_env_t<Env>...>();
            using child_sigs_t     = decltype(child_sigs);
            using extra_sigs_t     = ::beman::execution::detail::meta::filter<is_not_set_value, decltype(sched_sigs)>;
            using error_completion = ::std::conditional_t<
                is_nothrow_decay_copy_compl_sigs<child_sigs_t>::value,
                ::beman::execution::completion_signatures<>,
                ::beman::execution::completion_signatures<::beman::execution::set_error_t(::std::exception_ptr)>>;
            using type = ::beman::execution::detail::meta::unique<::beman::execution::detail::meta::combine<
                ::beman::execution::detail::meta::transform<decay_sig_t, child_sigs_t>,
                error_completion,
                extra_sigs_t>>;
            return type{};
        }
    };

    // Every completion of continues_on(sndr, sch) is delivered from within the set_value completion of
    // schedule(sch). That schedule operation is started by a completion of the child: where it completes
    // can depend on that (e.g., for inline_scheduler). A Tag completion is delivered after a child's
    // Source completion if Source is Tag, or if Tag is set_error_t and decay-copying the result datums
    // of the Source completion may throw.
    template <typename Child>
    struct rescheduled_sources {
        template <typename Tag, typename Source, typename... Env>
        static consteval auto value() -> bool {
            return (::std::same_as<Tag, Source> &&
                    ::beman::execution::detail::may_complete_with<Source, Child, Env...>()) ||
                   (::std::same_as<Tag, ::beman::execution::set_error_t> &&
                    ::beman::execution::detail::may_complete_with<Source, Child, Env...>([]<typename... Args> {
                        return !(::std::is_nothrow_constructible_v<::std::decay_t<Args>, Args> && ...);
                    }));
        }
    };

    // The environment of the receiver connected to schedule(sch) when the schedule operation is
    // started by a child completion with one of the tags in Sources: FWD-ENV(env) except that the
    // start scheduler and the domain are those of these child completions.
    template <typename Child, typename Sources, typename Env>
    struct start_env;

    template <typename Child, typename... Source, typename Env>
    struct start_env<Child, ::beman::execution::detail::type_list<Source...>, Env> {
        using child_attrs_t = ::std::remove_cvref_t<::beman::execution::env_of_t<Child>>;

        const child_attrs_t& child_attrs;
        const Env&           env;

        // Different channels may report different scheduler objects: only a single one can be used.
        template <typename S>
        static auto
        start_scheduler(::beman::execution::detail::type_list<S>, const child_attrs_t& attrs, const Env& env) noexcept
            -> decltype(::beman::execution::get_completion_scheduler<S>(attrs,
                                                                        ::beman::execution::detail::fwd_env(env))) {
            return ::beman::execution::get_completion_scheduler<S>(attrs, ::beman::execution::detail::fwd_env(env));
        }

        // Sources is a template parameter so that a missing start scheduler isn't a hard error.
        template <typename Sources = ::beman::execution::detail::type_list<Source...>>
        auto query(::beman::execution::get_start_scheduler_t) const noexcept
            -> decltype(start_scheduler(Sources{},
                                        ::std::declval<const child_attrs_t&>(),
                                        ::std::declval<const Env&>())) {
            return start_scheduler(Sources{}, this->child_attrs, this->env);
        }

        auto query(::beman::execution::get_domain_t) const noexcept -> ::beman::execution::detail::common_domain_t<
            ::beman::execution::detail::
                compl_domain_of_t<Source, Child, ::beman::execution::detail::fwd_env_t<Env>>...> {
            return {};
        }

        template <typename Query, typename... Args>
            requires(!::std::same_as<Query, ::beman::execution::get_start_scheduler_t>) &&
                    (!::std::same_as<Query, ::beman::execution::get_domain_t>) &&
                    requires(const Env& e, const Query& q, Args&&... args) {
                        ::beman::execution::detail::fwd_env(e).query(q, ::std::forward<Args>(args)...);
                    }
        auto query(const Query& q, Args&&... args) const noexcept -> decltype(auto) {
            return ::beman::execution::detail::fwd_env(this->env).query(q, ::std::forward<Args>(args)...);
        }
    };

    template <typename Scheduler, typename Child>
    struct attrs : ::beman::execution::detail::hide_completion_queries<::beman::execution::env_of_t<Child>> {
      private:
        using base          = ::beman::execution::detail::hide_completion_queries<::beman::execution::env_of_t<Child>>;
        using child_attrs_t = ::std::remove_cvref_t<::beman::execution::env_of_t<Child>>;
        using schedule_sender_t = ::beman::execution::schedule_result_t<Scheduler>;

        // The child completions after which a Tag completion is delivered (Env... is forwarded).
        template <typename Tag, typename... Env>
        using rescheduled = ::beman::execution::detail::completion_sources_t<rescheduled_sources<Child>, Tag, Env...>;

        // Whether schedule(sch) may itself complete with Tag, which is then forwarded (Env... is forwarded).
        template <typename Tag, typename... Env>
        static constexpr bool forwards_scheduling_completion =
            !::std::same_as<Tag, ::beman::execution::set_value_t> &&
            ::beman::execution::detail::may_complete_with<Tag, schedule_sender_t, Env...>();

        // The scheduler on which schedule(sch) completes with set_value_t when started by one of Source...
        template <typename... Source, typename... Env>
            requires(sizeof...(Source) != 0)
        static auto rescheduled_scheduler(::beman::execution::detail::type_list<Source...>,
                                          const Scheduler&     sch,
                                          const child_attrs_t& child_attrs,
                                          const Env&... env) noexcept
            -> decltype(::beman::execution::get_completion_scheduler<::beman::execution::set_value_t>(
                sch, ::std::declval<start_env<Child, ::beman::execution::detail::type_list<Source...>, Env>>()...)) {
            return ::beman::execution::get_completion_scheduler<::beman::execution::set_value_t>(
                sch, start_env<Child, ::beman::execution::detail::type_list<Source...>, Env>{child_attrs, env}...);
        }

        // The domains of the agents on which Tag completions are delivered, as type_list<> or type_list<D>:
        // those delivered from schedule(sch)'s set_value completion, and those forwarded from schedule(sch).
        template <typename... Env>
        static auto rescheduled_domain(::beman::execution::detail::type_list<>, const Env&...)
            -> ::beman::execution::detail::type_list<>;

        template <typename... Source, typename... Env>
            requires(sizeof...(Source) != 0)
        static auto rescheduled_domain(::beman::execution::detail::type_list<Source...>, const Env&...)
            -> ::beman::execution::detail::type_list<
                decltype(::beman::execution::get_completion_domain<::beman::execution::set_value_t>(
                    ::std::declval<const Scheduler&>(),
                    ::std::declval<start_env<Child, ::beman::execution::detail::type_list<Source...>, Env>>()...))>;

        template <typename Tag, typename... Env>
        static auto scheduling_domain(const Env&...) -> ::beman::execution::detail::type_list<>;

        template <typename Tag, typename... Env>
            requires forwards_scheduling_completion<Tag, ::beman::execution::detail::fwd_env_t<Env>...>
        static auto scheduling_domain(const Env&...) -> ::beman::execution::detail::type_list<
            ::beman::execution::detail::
                compl_domain_of_t<Tag, schedule_sender_t, ::beman::execution::detail::fwd_env_t<Env>...>>;

        template <typename... Domain>
            requires(sizeof...(Domain) != 0)
        static auto common_domain(::beman::execution::detail::type_list<Domain...>)
            -> ::beman::execution::detail::common_domain_t<Domain...>;

      public:
        attrs(Scheduler sch, ::beman::execution::env_of_t<Child> child_attrs) noexcept
            : base(::std::move(child_attrs)), sch(::std::move(sch)) {}

        using base::query;

        // When schedule(sch) itself completes with Tag, e.g., because scheduling fails, that happens on an
        // unspecified agent ([exec.continues.on]): a completion scheduler is only known otherwise.
        template <typename Tag, typename... Env>
            requires(!forwards_scheduling_completion<Tag, ::beman::execution::detail::fwd_env_t<Env>...>)
        auto query(::beman::execution::get_completion_scheduler_t<Tag>, const Env&... env) const noexcept
            -> decltype(rescheduled_scheduler(rescheduled<Tag, ::beman::execution::detail::fwd_env_t<Env>...>{},
                                              ::std::declval<const Scheduler&>(),
                                              ::std::declval<const child_attrs_t&>(),
                                              env...)) {
            return rescheduled_scheduler(rescheduled<Tag, ::beman::execution::detail::fwd_env_t<Env>...>{},
                                         this->sch,
                                         this->child_attrs,
                                         env...);
        }

        template <typename Tag, typename... Env>
            requires(!::std::same_as<Tag, void>)
        auto query(::beman::execution::get_completion_domain_t<Tag>, const Env&... env) const noexcept
            -> decltype(common_domain(::beman::execution::detail::meta::combine<
                                      decltype(rescheduled_domain(
                                          rescheduled<Tag, ::beman::execution::detail::fwd_env_t<Env>...>{}, env...)),
                                      decltype(scheduling_domain<Tag>(env...))>{})) {
            return {};
        }

        Scheduler sch;
    };

  public:
    template <typename Sender, typename... Env>
        requires requires { get_signatures<::std::remove_cvref_t<Sender>, Env...>{}; }
    static consteval auto get_completion_signatures() {
        return get_signatures<::std::remove_cvref_t<Sender>, Env...>::get();
    }

    struct impls_for : ::beman::execution::detail::default_impls {
        struct get_attrs_impl {
            template <typename Child>
            auto operator()(const auto& sch, const Child& child) const noexcept {
                using Sch = ::std::remove_cvref_t<decltype(sch)>;
                return attrs<Sch, Child>{sch, ::beman::execution::get_env(child)};
            }
        };
        static constexpr auto get_attrs{get_attrs_impl{}};

        template <typename State>
        struct upstream_receiver {
            using receiver_concept = ::beman::execution::receiver_tag;
            State* state;

            auto set_value() && noexcept -> void {
                ::std::visit(
                    [this]<typename Tuple>(Tuple& result) noexcept -> void {
                        if constexpr (!::std::same_as<::std::monostate, Tuple>) {
                            ::std::apply(
                                [this](auto&& tag, auto&&... args) {
                                    tag(::std::move(this->state->receiver), ::std::move(args)...);
                                },
                                result);
                        }
                    },
                    state->async_result);
            }

            template <typename Error>
            auto set_error(Error&& err) && noexcept -> void {
                ::beman::execution::set_error(std::move(state->receiver), std::forward<Error>(err));
            }

            auto set_stopped() && noexcept -> void { ::beman::execution::set_stopped(std::move(state->receiver)); }

            auto get_env() const noexcept -> decltype(auto) {
                return ::beman::execution::detail::fwd_env(::beman::execution::get_env(state->receiver));
            }
        };

        template <typename Receiver, typename Variant>
        struct state_base {
            Receiver& receiver;
            Variant   async_result{};
        };

        template <typename Receiver, typename Scheduler, typename Variant>
        struct state_type : state_base<Receiver, Variant> {
            using receiver_t = upstream_receiver<state_base<Receiver, Variant>>;
            using operation_t =
                ::beman::execution::connect_result_t<::beman::execution::schedule_result_t<Scheduler&>, receiver_t>;
            operation_t op_state;

            static constexpr bool nothrow() {
                return noexcept(::beman::execution::connect(::beman::execution::schedule(::std::declval<Scheduler&>()),
                                                            receiver_t{nullptr}));
            }
            explicit state_type(Scheduler& sch, Receiver& rcvr) noexcept(nothrow())
                : state_base<Receiver, Variant>{rcvr},
                  op_state(::beman::execution::connect(::beman::execution::schedule(sch), receiver_t{this})) {}
        };

        struct get_state_impl {
            template <typename Sender, typename Receiver>
                requires ::beman::execution::sender_in<
                    ::beman::execution::detail::child_type<Sender>,
                    ::beman::execution::detail::fwd_env_t<::beman::execution::env_of_t<Receiver>>>
            auto operator()(Sender&& sender, Receiver& receiver) const {
                auto& sch = sender.template get<1>();

                using sched_t = ::std::remove_reference_t<decltype(sch)>;
                using variant_t =
                    variant_type<::beman::execution::detail::child_type<Sender>,
                                 ::beman::execution::detail::fwd_env_t<::beman::execution::env_of_t<Receiver>>>;

                return state_type<Receiver, sched_t, variant_t>(sch, receiver);
            }
        };
        static constexpr auto get_state{get_state_impl{}};

        struct complete_impl {
            template <typename Tag, typename... Args>
            auto operator()(auto, auto& state, auto&, Tag, Args&&... args) const noexcept -> void {
                using result_t         = ::beman::execution::detail::decayed_tuple<Tag, Args...>;
                constexpr bool nothrow = (::std::is_nothrow_constructible_v<::std::decay_t<Args>, Args> && ...);

                try {
                    [&]() noexcept(nothrow) {
                        state.async_result.template emplace<result_t>(Tag(), ::std::forward<Args>(args)...);
                    }();
                } catch (...) {
                    if constexpr (!nothrow) {
                        state.async_result
                            .template emplace<::std::tuple<::beman::execution::set_error_t, ::std::exception_ptr>>(
                                ::beman::execution::set_error, ::std::current_exception());
                    }
                }

                ::beman::execution::start(state.op_state);
            }
        };
        static constexpr auto complete{complete_impl{}};
    };
};

} // namespace beman::execution::detail

#include <beman/execution/detail/suppress_pop.hpp>

namespace beman::execution {
using continues_on_t = ::beman::execution::detail::continues_on_t;
inline constexpr continues_on_t continues_on{};
} // namespace beman::execution

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_CONTINUES_ON
