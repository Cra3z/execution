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
#include <beman/execution/detail/stopped_as_error.hpp>
#include <beman/execution/detail/sync_wait.hpp>
#include <beman/execution.hpp>
#endif

// ----------------------------------------------------------------------------
namespace {

auto test_stopped_as_error_signatures() -> void {
    test_std::sender auto sndr1 = test_std::just(42) | test_std::stopped_as_error(-1);
    using sigs_of_sndr1         = test_std::completion_signatures_of_t<decltype(sndr1), test_detail::sync_wait_env>;
    static_assert(std::same_as<sigs_of_sndr1, test_std::completion_signatures<test_std::set_value_t(int)>>);
    auto [i] = test_std::sync_wait(std::move(sndr1)).value();
    ASSERT(i == 42);

    test_std::sender auto sndr2 = test_std::stopped_as_error(test::optional_sender<int>{}, 114514);
    using sigs_of_sndr2         = test_std::completion_signatures_of_t<decltype(sndr2), test_detail::sync_wait_env>;
    static_assert(
        std::same_as<sigs_of_sndr2,
                     test_std::completion_signatures<test_std::set_error_t(int), test_std::set_value_t(int)>>);
    try {
        test_std::sync_wait(std::move(sndr2));
    } catch (int e) {
        ASSERT(e == 114514);
    }
}

auto test_stopped_as_std_error_code() -> void {
    try {
        auto snd = test_std::stopped_as_error(test::optional_sender<int>{},
                                              std::make_error_code(std::errc::operation_canceled));
        test_std::sync_wait(std::move(snd));
    } catch (const std::system_error& e) {
        ASSERT(e.code() == std::errc::operation_canceled);
    }
}

auto test_stopped_as_error_attributes() {
    test::sender_env s{42};
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, s);
    test::test_sender_env<true>(84, test::test_non_forwardable_attr{}, s);
    //-dk:TODO test::test_sender_env<true>(42, test::test_forwardable_attr{}, test_std::stopped_as_error(s));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, test_std::stopped_as_error(s));
}

auto test_stopped_as_error_completion_queries() {
    using value   = test_std::set_value_t;
    using error   = test_std::set_error_t;
    using stopped = test_std::set_stopped_t;
    using test::channel_domain;

    {
        // NVIDIA/stdexec#2415: the error completion happens where just_stopped() completes.
        test_std::run_loop loop;
        test_std::prop     env{test_std::get_start_scheduler, loop.get_scheduler()};
        auto               sndr = test_std::just_stopped() | test_std::stopped_as_error(-1);
        ASSERT(test_std::get_completion_scheduler<error>(test_std::get_env(sndr), env) == loop.get_scheduler());
    }
    {
        // Error completions come from both the child's error and stopped completions.
        auto sndr     = test::channel_sender<value(), error(int), stopped()>{} | test_std::stopped_as_error(-1);
        using sndr_t  = decltype(sndr);
        using error_t = test_std::indeterminate_domain<channel_domain<error>, channel_domain<stopped>>;
        ASSERT((test::check_channel_scheduler<value, 1>(sndr)));
        ASSERT((test::check_channel_scheduler<error, 0>(sndr)));
        ASSERT((test::check_channel_scheduler<stopped, 0>(sndr)));
        static_assert(test::check_channel_domain<value, channel_domain<value>, sndr_t>());
        static_assert(test::check_channel_domain<error, error_t, sndr_t>());
        static_assert(test::check_channel_domain<stopped, void, sndr_t>());
    }
    {
        auto sndr = test::channel_sender<value(), stopped()>{} | test_std::stopped_as_error(-1);
        ASSERT((test::check_channel_scheduler<error, 3>(sndr)));
        static_assert(test::check_channel_domain<error, channel_domain<stopped>, decltype(sndr)>());
        static_assert(test::check_channel_domain<stopped, void, decltype(sndr)>());
    }
    {
        auto sndr = test::channel_sender<error(int)>{} | test_std::stopped_as_error(-1);
        ASSERT((test::check_channel_scheduler<error, 2>(sndr)));
        ASSERT((test::check_channel_scheduler<value, 0>(sndr)));
        static_assert(test::check_channel_domain<error, channel_domain<error>, decltype(sndr)>());
    }
}
} // namespace
// ----------------------------------------------------------------------------

TEST(exec_stopped_as_error) {
    test_stopped_as_error_signatures();
    test_stopped_as_std_error_code();
    test_stopped_as_error_attributes();
    test_stopped_as_error_completion_queries();
}
