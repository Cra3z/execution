// src/beman/execution/tests/exec-continues-on.test.cpp             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <test/execution.hpp>
#include <test/sender_env.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
#else
#include <beman/execution.hpp>
#endif

// ----------------------------------------------------------------------------

namespace {
struct non_scheduler {};
struct non_sender {};

struct custom_domain : test_std::default_domain {};

struct scheduler {
    static auto query(test_std::get_forward_progress_guarantee_t) noexcept {
        return test_std::forward_progress_guarantee::weakly_parallel;
    }

    static auto query(test_std::get_completion_scheduler_t<test_std::set_value_t>) noexcept -> scheduler { return {}; }

    static auto query(test_std::get_completion_domain_t<test_std::set_value_t>) noexcept -> custom_domain {
        return {};
    }

    struct env {
        auto query(const test_std::get_completion_scheduler_t<test_std::set_value_t>&) const noexcept -> scheduler {
            return {};
        }
    };

    struct sender {
        template <typename Receiver>
        struct state {
            using operation_state_concept = test_std::operation_state_tag;
            std::remove_cvref_t<Receiver> receiver;
            auto start() & noexcept -> void { test_std::set_value(::std::move(this->receiver)); }
        };

        using sender_concept = test_std::sender_tag;

        auto get_env() const noexcept -> env { return {}; }

        template <typename...>
        static consteval auto get_completion_signatures() noexcept {
            return test_std::completion_signatures<test_std::set_value_t()>{};
        }

        template <test_std::receiver Receiver>
        auto connect(Receiver&& receiver) -> state<Receiver> {
            return {std::forward<Receiver>(receiver)};
        }
    };
    using scheduler_concept = test_std::scheduler_tag;
    auto schedule() -> sender { return {}; }
    auto operator==(const scheduler&) const -> bool = default;
};
struct sender {
    using sender_concept = test_std::sender_tag;

    template <typename Receiver>
    struct state {
        using operation_state_concept = test_std::operation_state_tag;
        std::remove_cvref_t<Receiver> receiver;
        auto                          start() & noexcept -> void { test_std::set_value(::std::move(this->receiver)); }
    };

    template <typename...>
    static consteval auto get_completion_signatures() noexcept {
        return test_std::completion_signatures<test_std::set_value_t()>{};
    }

    template <test_std::receiver Receiver>
    auto connect(Receiver&& receiver) -> state<Receiver> {
        return {std::forward<Receiver>(receiver)};
    }
};

template <bool Expect, typename Scheduler, typename Sender>
auto test_constraints(Scheduler&& scheduler, Sender&& sender) {
    static_assert(Expect == requires { test_std::continues_on(sender, scheduler); });
    static_assert(Expect == requires {
        {
            test_std::continues_on(::std::forward<Sender>(sender), ::std::forward<Scheduler>(scheduler))
        } -> test_std::sender;
    });
    static_assert(Expect == requires { ::std::forward<Sender>(sender) | test_std::continues_on(scheduler); });

    if constexpr (Expect) {
        auto domain{test_std::get_completion_domain<>(scheduler)};
        test::check_type<custom_domain&>(domain);

        [[maybe_unused]] auto s{
            test_std::continues_on(::std::forward<Sender>(sender), ::std::forward<Scheduler>(scheduler))};
    }
}

template <typename Scheduler, typename Sender>
auto test_use(Scheduler&& scheduler, Sender&& sender) {
    auto s{test_std::continues_on(::std::forward<Sender>(sender), ::std::forward<Scheduler>(scheduler))};

    static_assert(test_std::sender<decltype(s)>);
    static_assert(
        std::same_as<decltype(test_std::get_completion_scheduler<test_std::set_value_t>(test_std::get_env(s))),
                     std::remove_cvref_t<Scheduler>>);
    static_assert(std::same_as<decltype(test_std::get_completion_domain<>(test_std::get_env(s))), custom_domain>);
    //-dk:TODO test_std::sync_wait(std::move(s));
}

auto test_inline_scheduler() {
    test_std::run_loop loop;
    auto sndr = test_std::continues_on(test_std::schedule(loop.get_scheduler()), test_std::inline_scheduler{});
    auto env  = test_std::prop{test_std::get_start_scheduler, scheduler{}};
    auto sch  = test_std::get_completion_scheduler<test_std::set_value_t>(test_std::get_env(sndr), env);
    static_assert(std::same_as<decltype(sch), decltype(loop.get_scheduler())>);
    ASSERT(sch == loop.get_scheduler());
}

auto test_continues_on_attributes() {
    test_std::run_loop loop;
    test::sender_env   s{42};
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, s);
    test::test_sender_env<true>(84, test::test_non_forwardable_attr{}, s);
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, test_std::continues_on(s, loop.get_scheduler()));
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, test_std::continues_on(s, loop.get_scheduler()));
}

struct query_domain {};

// A scheduler whose schedule sender has the given completion signatures, and completion schedulers
// for set_value_t and set_stopped_t if Attrs is true.
template <bool Attrs, typename... Sigs>
struct query_scheduler {
    using scheduler_concept = test_std::scheduler_tag;
    int id{};

    struct attrs {
        int id;
        template <typename Tag>
            requires std::same_as<Tag, test_std::set_value_t> || std::same_as<Tag, test_std::set_stopped_t>
        auto query(test_std::get_completion_scheduler_t<Tag>) const noexcept {
            return query_scheduler{id};
        }
    };
    struct sender {
        using sender_concept = test_std::sender_tag;
        int id;
        template <typename...>
        static consteval auto get_completion_signatures() noexcept {
            return test_std::completion_signatures<Sigs...>{};
        }
        auto get_env() const noexcept {
            if constexpr (Attrs)
                return attrs{id};
            else
                return test_std::env<>{};
        }
        template <typename Receiver>
        struct state {
            using operation_state_concept = test_std::operation_state_tag;
            Receiver receiver;
            auto     start() & noexcept -> void { test_std::set_value(std::move(this->receiver)); }
        };
        template <test_std::receiver Receiver>
        auto connect(Receiver receiver) const noexcept -> state<Receiver> {
            return {std::move(receiver)};
        }
    };
    auto schedule() const noexcept -> sender { return {id}; }

    template <typename... Env>
    auto query(test_std::get_completion_domain_t<test_std::set_value_t>, const Env&...) const noexcept
        -> query_domain {
        return {};
    }
    static auto query(test_std::get_forward_progress_guarantee_t) noexcept {
        return test_std::forward_progress_guarantee::weakly_parallel;
    }
    auto operator==(const query_scheduler&) const -> bool = default;
};

struct throwing_copy {
    throwing_copy() = default;
    throwing_copy(const throwing_copy&) noexcept(false) {}
    throwing_copy(throwing_copy&&) noexcept(false) {}
};

template <typename Tag, typename Sender, typename... Env>
concept has_completion_scheduler = requires(const Sender& sndr, const Env&... env) {
    test_std::get_completion_scheduler<Tag>(test_std::get_env(sndr), env...);
};

template <typename Tag, typename Sender, typename... Env>
concept has_completion_domain = requires(const Sender& sndr, const Env&... env) {
    test_std::get_completion_domain<Tag>(test_std::get_env(sndr), env...);
};

template <typename Tag, typename Sender, typename... Env>
using completion_domain_t = decltype(test_std::get_completion_domain<Tag>(
    test_std::get_env(std::declval<const Sender&>()), std::declval<const Env&>()...));

auto test_completion_queries() {
    using value   = test_std::set_value_t;
    using error   = test_std::set_error_t;
    using stopped = test_std::set_stopped_t;
    using env     = test_std::env<>;

    {
        // All completions happen on sch even if its schedule sender reports no completion scheduler.
        using sch_t = query_scheduler<false, value()>;
        auto sndr   = test_std::continues_on(test_std::just(), sch_t{17});
        ASSERT(test_std::get_completion_scheduler<value>(test_std::get_env(sndr), env{}) == sch_t{17});
        static_assert(std::same_as<completion_domain_t<value, decltype(sndr), env>, query_domain>);
        static_assert(not has_completion_scheduler<error, decltype(sndr), env>);
        static_assert(not has_completion_domain<error, decltype(sndr), env>);
    }
    {
        // Errors from decay-copying the child's results are delivered on sch, too.
        using sch_t = query_scheduler<true, value()>;
        auto sndr   = test_std::continues_on(test_std::just(throwing_copy{}), sch_t{17});
        ASSERT(test_std::get_completion_scheduler<error>(test_std::get_env(sndr), env{}) == sch_t{17});
        static_assert(std::same_as<completion_domain_t<error, decltype(sndr), env>, query_domain>);
    }
    {
        // Stopped completions of schedule(sch) itself: their domain is known, a scheduler is not reported.
        using sch_t = query_scheduler<true, value(), stopped()>;
        auto sndr   = test_std::continues_on(test_std::just(), sch_t{});
        static_assert(has_completion_scheduler<value, decltype(sndr), env>);
        static_assert(not has_completion_scheduler<stopped, decltype(sndr), env>);
        static_assert(std::same_as<completion_domain_t<stopped, decltype(sndr), env>, query_domain>);

        auto stopped_sndr = test_std::continues_on(test_std::just_stopped(), sch_t{});
        static_assert(not has_completion_scheduler<stopped, decltype(stopped_sndr), env>);
        static_assert(std::same_as<completion_domain_t<stopped, decltype(stopped_sndr), env>, query_domain>);
        static_assert(not has_completion_domain<value, decltype(stopped_sndr), env>);
    }
    {
        // The run_loop schedule sender may complete with set_stopped unless the environment says otherwise.
        test_std::run_loop loop;
        auto               sndr = test_std::continues_on(test_std::just_stopped(), loop.get_scheduler());
        static_assert(not has_completion_scheduler<stopped, decltype(sndr)>);
        ASSERT(test_std::get_completion_scheduler<stopped>(test_std::get_env(sndr), env{}) == loop.get_scheduler());
        static_assert(std::same_as<completion_domain_t<stopped, decltype(sndr), env>, test_std::default_domain>);
    }
    {
        // inline_scheduler completes on the agent of the child completion starting it: the child's
        // value completions come from two channels with a common domain.
        using sch_t = query_scheduler<true, value(), stopped()>;
        auto child  = test_std::schedule(sch_t{}) | test_std::upon_stopped([] {});
        static_assert(not has_completion_scheduler<value, decltype(child), env>);
        auto sndr = test_std::continues_on(child, test_std::inline_scheduler{});
        static_assert(std::same_as<completion_domain_t<value, decltype(sndr), env>, query_domain>);
    }
    {
        // With inline_scheduler, decay-copy errors happen on the agent of the child's value completion.
        test_std::run_loop loop;
        auto               child =
            test_std::schedule(loop.get_scheduler()) | test_std::then([]() noexcept { return throwing_copy{}; });
        auto sndr = test_std::continues_on(child, test_std::inline_scheduler{});
        ASSERT(test_std::get_completion_scheduler<value>(test_std::get_env(sndr), env{}) == loop.get_scheduler());
        ASSERT(test_std::get_completion_scheduler<error>(test_std::get_env(sndr), env{}) == loop.get_scheduler());
    }
}
} // namespace

TEST(exec_continues_on) {
    static_assert(std::same_as<const test_std::continues_on_t, decltype(test_std::continues_on)>);
    static_assert(not test_std::scheduler<non_scheduler>);
    static_assert(test_std::scheduler<scheduler>);
    static_assert(not test_std::sender<non_sender>);
    static_assert(test_std::sender<sender>);

    test_constraints<false>(non_scheduler{}, non_sender{});
    test_constraints<false>(non_scheduler{}, sender{});
    test_constraints<false>(scheduler{}, non_sender{});
    test_constraints<true>(scheduler{}, sender{});

    test_use(scheduler{}, sender{});
    test_inline_scheduler();
    test_continues_on_attributes();
    test_completion_queries();
}
