// tests/beman/execution/exec-task-scheduler.test.cpp               -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <numeric>
#include <thread>
#include <type_traits>
#include <vector>
#include <test/execution.hpp>

#ifdef BEMAN_HAS_MODULES
import beman.execution;
#else
#include <beman/execution/detail/bulk.hpp>
#include <beman/execution/detail/get_completion_domain.hpp>
#include <beman/execution/detail/get_forward_progress_guarantee.hpp>
#include <beman/execution/detail/inline_scheduler.hpp>
#include <beman/execution/detail/schedule.hpp>
#include <beman/execution/detail/scheduler.hpp>
#include <beman/execution/detail/sync_wait.hpp>
#include <beman/execution/detail/task_scheduler.hpp>
#include <beman/execution/detail/then.hpp>
#endif

namespace {

struct async_scheduler {
    using scheduler_concept = test_std::scheduler_tag;

    template <typename Rcvr>
    struct operation {
        using operation_state_concept = test_std::operation_state_tag;
        Rcvr rcvr;

        auto start() & noexcept -> void {
            ::std::thread([receiver = ::std::move(rcvr)]() mutable noexcept {
                test_std::set_value(::std::move(receiver));
            }).detach();
        }
    };

    struct sender {
        using sender_concept        = test_std::sender_tag;
        using completion_signatures = test_std::completion_signatures<test_std::set_value_t()>;

        template <typename...>
        static consteval auto get_completion_signatures() noexcept -> completion_signatures { return {}; }
        static auto get_env() noexcept -> test_std::env<> { return {}; }

        template <typename Rcvr>
        auto connect(Rcvr rcvr) const -> operation<Rcvr> { return {::std::move(rcvr)}; }
    };

    static auto schedule() noexcept -> sender { return {}; }
    auto operator==(const async_scheduler&) const noexcept -> bool = default;
};

auto test_task_scheduler_interface() -> void {
    static_assert(!::std::default_initializable<test_std::task_scheduler>);
    static_assert(::std::copy_constructible<test_std::task_scheduler>);
    static_assert(test_std::scheduler<test_std::task_scheduler>);

    const test_std::inline_scheduler base{};
    const test_std::task_scheduler   scheduler{base};
    const test_std::task_scheduler   same{base};

    ASSERT(scheduler == base);
    ASSERT(base == scheduler);
    ASSERT(scheduler == same);
    ASSERT(test_std::get_forward_progress_guarantee(scheduler) ==
           test_std::forward_progress_guarantee::weakly_parallel);
    static_assert(::std::same_as<
                  decltype(test_std::get_completion_domain<test_std::set_value_t>(scheduler)),
                  test_std::get_completion_domain_t<test_std::set_value_t>::domain_of<test_std::task_scheduler>>);
}

auto test_task_scheduler_schedule_and_bulk() -> void {
    const test_std::task_scheduler scheduler{test_std::inline_scheduler{}};
    int                            value{};
    test_std::sync_wait(test_std::schedule(scheduler) | test_std::then([&] { value = 42; }));
    ASSERT(value == 42);

    std::vector<int> values(8uz);
    std::iota(values.begin(), values.end(), 0);
    test_std::sync_wait(test_std::schedule(scheduler) |
                        test_std::bulk(test_std::par, values.size(), [&values](std::size_t i) { values[i] *= 2; }));
    for (std::size_t i = 0; i != values.size(); ++i) {
        ASSERT(values[i] == static_cast<int>(2uz * i));
    }

    test_std::sync_wait(
        test_std::schedule(scheduler) |
        test_std::bulk_unchunked(test_std::par, values.size(), [&values](std::size_t i) { ++values[i]; }));
    for (std::size_t i = 0; i != values.size(); ++i) {
        ASSERT(values[i] == static_cast<int>(2uz * i + 1uz));
    }

    const auto caller = ::std::this_thread::get_id();
    auto       worker = caller;
    test_std::sync_wait(test_std::schedule(test_std::task_scheduler{async_scheduler{}}) |
                        test_std::then([&] { worker = ::std::this_thread::get_id(); }));
    ASSERT(worker != caller);
}

} // namespace

TEST(exec_task_scheduler) {
    test_task_scheduler_interface();
    test_task_scheduler_schedule_and_bulk();
}
