// src/beman/execution/tests/exec-stopped-as-error.test.cpp             -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <test/execution.hpp>
#include <test/sender_env.hpp>
#include <test/optional_sender.hpp>
#include <test/channel_sender.hpp>
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <optional>
#include <system_error>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
import beman.execution.detail;
#else
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/receiver.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/stopped_as_optional.hpp>
#include <beman/execution/detail/sync_wait.hpp>
#include <beman/execution.hpp>
#endif

// ----------------------------------------------------------------------------
namespace {
auto test_stopped_as_optional() -> void {
    test_std::sender auto sndr1 = test_std::just(42) | test_std::stopped_as_optional();
    using sigs_of_sndr1         = test_std::completion_signatures_of_t<decltype(sndr1), test_detail::sync_wait_env>;
    static_assert(
        std::same_as<sigs_of_sndr1, test_std::completion_signatures<test_std::set_value_t(std::optional<int>)>>);
    auto [i] = test_std::sync_wait(std::move(sndr1)).value();
    ASSERT(i == 42);

    test_std::sender auto sndr2 = test_std::stopped_as_optional(test::optional_sender<int>{});
    using sigs_of_sndr2 = test_std::completion_signatures_of_t<decltype(sndr2), test_std::detail::sync_wait_env>;
    static_assert(
        std::same_as<sigs_of_sndr2, test_std::completion_signatures<test_std::set_value_t(std::optional<int>)>>);
    auto [opt] = test_std::sync_wait(std::move(sndr2)).value();
    ASSERT(!opt.has_value());
}

auto test_stopped_as_optional_attributes() {
    test::sender_env s{42};
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, s);
    test::test_sender_env<true>(84, test::test_non_forwardable_attr{}, s);
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, test_std::stopped_as_optional(s));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, test_std::stopped_as_optional(s));
}

struct throwing_move {
    throwing_move() = default;
    throwing_move(throwing_move&&) noexcept(false) {}
};

auto test_stopped_as_optional_completion_queries() {
    using value   = test_std::set_value_t;
    using error   = test_std::set_error_t;
    using stopped = test_std::set_stopped_t;
    using test::channel_domain;

    {
        test_std::run_loop loop;
        test_std::prop     env{test_std::get_start_scheduler, loop.get_scheduler()};
        auto               sndr = test_std::just(42) | test_std::stopped_as_optional();
        ASSERT(test_std::get_completion_scheduler<value>(test_std::get_env(sndr), env) == loop.get_scheduler());
    }
    {
        // Value completions come from both the child's value and stopped completions.
        auto sndr     = test::channel_sender<value(int), error(int), stopped()>{} | test_std::stopped_as_optional();
        using sndr_t  = decltype(sndr);
        using value_t = test_std::indeterminate_domain<channel_domain<value>, channel_domain<stopped>>;
        ASSERT((test::check_channel_scheduler<value, 0>(sndr)));
        ASSERT((test::check_channel_scheduler<error, 2>(sndr)));
        ASSERT((test::check_channel_scheduler<stopped, 0>(sndr)));
        static_assert(test::check_channel_domain<value, value_t, sndr_t>());
        static_assert(test::check_channel_domain<error, channel_domain<error>, sndr_t>());
        static_assert(test::check_channel_domain<stopped, void, sndr_t>());
    }
    {
        auto sndr = test::channel_sender<value(int)>{} | test_std::stopped_as_optional();
        ASSERT((test::check_channel_scheduler<value, 1>(sndr)));
        ASSERT((test::check_channel_scheduler<error, 0>(sndr)));
        static_assert(test::check_channel_domain<error, void, decltype(sndr)>());
    }
    {
        // Producing the optional may throw within the value and the stopped completions.
        auto sndr     = test::channel_sender<value(throwing_move), stopped()>{} | test_std::stopped_as_optional();
        using error_t = test_std::indeterminate_domain<channel_domain<value>, channel_domain<stopped>>;
        ASSERT((test::check_channel_scheduler<error, 0>(sndr)));
        static_assert(test::check_channel_domain<error, error_t, decltype(sndr)>());

        auto value_only = test::channel_sender<value(throwing_move)>{} | test_std::stopped_as_optional();
        ASSERT((test::check_channel_scheduler<error, 1>(value_only)));
        static_assert(test::check_channel_domain<error, channel_domain<value>, decltype(value_only)>());
    }
}

} // namespace
// ----------------------------------------------------------------------------

TEST(exec_stopped_as_optional) {
    test_stopped_as_optional();
    test_stopped_as_optional_attributes();
    test_stopped_as_optional_completion_queries();
}
