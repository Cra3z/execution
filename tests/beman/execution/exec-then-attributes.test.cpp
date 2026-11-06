// tests/beman/execution/exec-then-attributes.test.cpp              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <test/execution.hpp>
#include <test/sender_env.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <exception>
#include <type_traits>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
#else
#include <beman/execution.hpp>
#endif

namespace {
using value = test_std::set_value_t;
using error = test_std::set_error_t;
using stop  = test_std::set_stopped_t;

template <typename Tag>
struct channel_domain {};
template <typename Tag>
struct environment_domain {};
struct shared_domain {};
struct stale_domain {};

struct attribute_scheduler {
    using scheduler_concept = test_std::scheduler_tag;
    int id;

    struct schedule_sender {
        using sender_concept = test_std::sender_tag;
        int id;
        struct attrs {
            int  id;
            auto query(test_std::get_completion_scheduler_t<value>) const noexcept { return attribute_scheduler{id}; }
        };
        auto                  get_env() const noexcept { return attrs{id}; }
        static consteval auto get_completion_signatures() { return test_std::completion_signatures<value()>{}; }
    };
    auto schedule() const noexcept { return schedule_sender{id}; }
    auto query(test_std::get_forward_progress_guarantee_t) const noexcept {
        return test_std::forward_progress_guarantee::weakly_parallel;
    }
    // Exercise domain lookup through a scheduler when no direct domain exists.
    auto query(test_std::get_completion_domain_t<value>) const noexcept { return shared_domain{}; }
    bool operator==(const attribute_scheduler&) const = default;
};
static_assert(test_std::scheduler<attribute_scheduler>);

struct select_error_t : test_std::forwarding_query_t {};
struct offset_t : test_std::forwarding_query_t {};
template <bool Error>
struct selecting_env {
    auto query(select_error_t) const noexcept { return std::bool_constant<Error>{}; }
    auto query(offset_t) const noexcept { return 100; }
};

template <bool Shared = false, bool SchedulerOnly = false>
struct channel_attrs {
    template <typename Tag>
    static constexpr int id = std::same_as<Tag, value>   ? 1
                              : std::same_as<Tag, error> ? 2
                                                         : 3;

    template <typename Tag>
    auto query(test_std::get_completion_scheduler_t<Tag>) const noexcept {
        return attribute_scheduler{id<Tag>};
    }
    template <typename Tag, typename Env>
        requires requires(const Env& env) { env.query(offset_t{}); }
    auto query(test_std::get_completion_scheduler_t<Tag>, const Env& env) const noexcept {
        return attribute_scheduler{id<Tag> + env.query(offset_t{})};
    }
    template <typename Tag>
        requires(!SchedulerOnly)
    auto query(test_std::get_completion_domain_t<Tag>) const noexcept
        -> std::conditional_t<std::same_as<Tag, void>,
                              stale_domain,
                              std::conditional_t<Shared, shared_domain, channel_domain<Tag>>> {
        return {};
    }
    template <typename Tag, typename Env>
        requires(!SchedulerOnly) && requires(const Env& env) { env.query(offset_t{}); }
    auto query(test_std::get_completion_domain_t<Tag>, const Env&) const noexcept {
        return environment_domain<Tag>{};
    }
};

template <typename Attrs, typename... Sigs>
struct attribute_sender {
    using sender_concept = test_std::sender_tag;
    auto                  get_env() const noexcept { return Attrs{}; }
    static consteval auto get_completion_signatures() { return test_std::completion_signatures<Sigs...>{}; }
};

struct dependent_attribute_sender {
    using sender_concept = test_std::sender_tag;
    auto get_env() const noexcept { return channel_attrs<>{}; }

    template <typename, typename Env>
        requires requires(const Env& env) { env.query(select_error_t{}); }
    static consteval auto get_completion_signatures() {
        if constexpr (decltype(std::declval<const Env&>().query(select_error_t{}))::value)
            return test_std::completion_signatures<error(int)>{};
        else
            return test_std::completion_signatures<value()>{};
    }
};
static_assert(test_std::dependent_sender<dependent_attribute_sender>);

struct partial_domain_attrs {
    auto query(test_std::get_completion_domain_t<error>) const noexcept { return channel_domain<error>{}; }
};

template <typename Tag, int Expected, typename Sender, typename... Env>
void check_scheduler(const Sender& sender, const Env&... env) {
    auto attrs = test_std::get_env(sender);
    if constexpr (Expected == 0) {
        static_assert(!requires { test_std::get_completion_scheduler<Tag>(attrs, env...); });
    } else {
        static_assert(noexcept(test_std::get_completion_scheduler<Tag>(attrs, env...)));
        ASSERT(test_std::get_completion_scheduler<Tag>(attrs, env...).id == Expected);
    }
}

template <typename Tag, typename Expected, typename Sender, typename... Env>
void check_domain(const Sender& sender, const Env&... env) {
    auto attrs = test_std::get_env(sender);
    if constexpr (std::same_as<Expected, void>) {
        static_assert(!requires { test_std::get_completion_domain<Tag>(attrs, env...); });
    } else {
        static_assert(std::same_as<Expected, decltype(test_std::get_completion_domain<Tag>(attrs, env...))>);
        static_assert(noexcept(test_std::get_completion_domain<Tag>(attrs, env...)));
    }
}

void test_single_sources() {
    using value_sender = attribute_sender<channel_attrs<>, value()>;
    using error_sender = attribute_sender<channel_attrs<>, error(int)>;
    using stop_sender  = attribute_sender<channel_attrs<>, stop()>;

    auto then = value_sender{} | test_std::then([] {});
    check_scheduler<value, 1>(then);
    check_scheduler<error, 1>(then);
    check_domain<value, channel_domain<value>>(then);
    check_domain<error, channel_domain<value>>(then);
    check_domain<void, channel_domain<value>>(then);

    auto upon_error = error_sender{} | test_std::upon_error([](int) {});
    check_scheduler<value, 2>(upon_error);
    check_scheduler<error, 2>(upon_error);
    check_domain<value, channel_domain<error>>(upon_error);
    check_domain<error, channel_domain<error>>(upon_error);
    check_domain<void, channel_domain<error>>(upon_error);

    auto upon_stopped = stop_sender{} | test_std::upon_stopped([] {});
    check_scheduler<value, 3>(upon_stopped);
    check_scheduler<error, 3>(upon_stopped);
    check_domain<value, channel_domain<stop>>(upon_stopped);
    check_domain<error, channel_domain<stop>>(upon_stopped);
    check_domain<void, channel_domain<stop>>(upon_stopped);

    check_scheduler<error, 0>(value_sender{} | test_std::then([]() noexcept {}));
    check_scheduler<error, 0>(error_sender{} | test_std::upon_error([](int) noexcept {}));
    check_scheduler<error, 0>(stop_sender{} | test_std::upon_stopped([]() noexcept {}));
    check_domain<error, void>(value_sender{} | test_std::then([]() noexcept {}));
    check_domain<error, void>(error_sender{} | test_std::upon_error([](int) noexcept {}));
    check_domain<error, void>(stop_sender{} | test_std::upon_stopped([]() noexcept {}));

    // Attributes on an impossible child channel must not leak through.
    auto absent = value_sender{} | test_std::upon_stopped([] {});
    check_scheduler<error, 0>(absent);
    check_domain<error, void>(absent);
    check_scheduler<stop, 0>(absent);
    check_domain<stop, void>(absent);
    check_scheduler<value, 0>(attribute_sender<channel_attrs<>>{} | test_std::then([] {}));
    check_domain<value, void>(attribute_sender<channel_attrs<>>{} | test_std::then([] {}));
    // just() has no completion domain without an environment, so neither has then.
    check_domain<value, void>(test_std::just() | test_std::then([] {}));

    auto forwarded =
        attribute_sender<channel_attrs<>, value(), error(int), stop()>{} | test_std::then([]() noexcept {});
    check_scheduler<error, 2>(forwarded);
    check_domain<error, channel_domain<error>>(forwarded);
    check_scheduler<stop, 3>(forwarded);
    check_domain<stop, channel_domain<stop>>(forwarded);

    // Multiple signatures on the same channel still have one scheduler.
    auto multiple = attribute_sender<channel_attrs<>, value(), value(int)>{} | test_std::then([](auto...) {});
    check_scheduler<error, 1>(multiple);
    check_domain<error, channel_domain<value>>(multiple);

    auto strict = attribute_sender<channel_attrs<>, value(int), error(std::exception_ptr)>{} |
                  test_std::then([](auto arg) { static_assert(std::same_as<decltype(arg), int>); });
    check_scheduler<value, 1>(strict);
    check_scheduler<error, 0>(strict);
}

void test_multiple_sources() {
    using all_sender = attribute_sender<channel_attrs<>, value(), error(int), stop()>;
    auto then        = all_sender{} | test_std::then([] {});
    check_scheduler<error, 0>(then);
    check_domain<error, test_std::indeterminate_domain<channel_domain<value>, channel_domain<error>>>(then);

    auto upon_error = all_sender{} | test_std::upon_error([](int) noexcept {});
    check_scheduler<value, 0>(upon_error);
    check_domain<value, test_std::indeterminate_domain<channel_domain<value>, channel_domain<error>>>(upon_error);
    check_domain<void, test_std::indeterminate_domain<channel_domain<value>, channel_domain<error>>>(upon_error);

    auto upon_stopped = all_sender{} | test_std::upon_stopped([] {});
    check_scheduler<value, 0>(upon_stopped);
    check_scheduler<error, 0>(upon_stopped);
    check_domain<value, test_std::indeterminate_domain<channel_domain<value>, channel_domain<stop>>>(upon_stopped);
    check_domain<error, test_std::indeterminate_domain<channel_domain<error>, channel_domain<stop>>>(upon_stopped);

    auto shared = attribute_sender<channel_attrs<true>, value(), error(int), stop()>{} | test_std::upon_stopped([] {});
    check_scheduler<value, 0>(shared);
    check_scheduler<error, 0>(shared);
    check_domain<value, shared_domain>(shared);
    check_domain<error, shared_domain>(shared);

    auto no_direct_domain = attribute_sender<channel_attrs<false, true>, value(), error(int)>{} |
                            test_std::upon_error([](int) noexcept {});
    check_scheduler<value, 0>(no_direct_domain);
    check_domain<value, shared_domain>(no_direct_domain);

    // The set_value domain of the child is unknown: without an environment it may still become
    // known, so the set_error domain is not reported yet; with one it is indeterminate.
    auto partial = attribute_sender<partial_domain_attrs, value(), error(int)>{} | test_std::then([] {});
    check_domain<error, void>(partial);
    []<typename... Domains>(test_std::indeterminate_domain<Domains...>) {
    }(test_std::get_completion_domain<error>(test_std::get_env(partial), test_std::env<>{}));
}

void test_dependent_sources() {
    auto then = dependent_attribute_sender{} | test_std::then([] {});
    check_scheduler<value, 1>(then);
    check_scheduler<error, 0>(then);
    check_domain<error, test_std::indeterminate_domain<channel_domain<value>, channel_domain<error>>>(then);

    auto upon_error = dependent_attribute_sender{} | test_std::upon_error([](int) noexcept {});
    check_scheduler<value, 0>(upon_error);
    check_scheduler<value, 102>(upon_error, selecting_env<true>{});
    check_scheduler<value, 101>(upon_error, selecting_env<false>{});
    check_domain<value, environment_domain<error>>(upon_error, selecting_env<true>{});
    check_domain<void, environment_domain<error>>(upon_error, selecting_env<true>{});
    check_domain<value, environment_domain<value>>(upon_error, selecting_env<false>{});

    auto upon_stopped = dependent_attribute_sender{} | test_std::upon_stopped([]() noexcept {});
    check_scheduler<error, 2>(upon_stopped);
    check_domain<error, channel_domain<error>>(upon_stopped);
    check_scheduler<stop, 0>(upon_stopped);
    check_domain<stop, void>(upon_stopped);
    check_scheduler<error, 0>(dependent_attribute_sender{} | test_std::upon_stopped([] {}));
}
} // namespace

TEST(exec_then_attributes) {
    auto forwarded = test::sender_env{42} | test_std::then([] {});
    auto attrs     = test_std::get_env(forwarded);
    static_assert(noexcept(attrs.query(test::test_forwardable_attr{})));
    ASSERT(attrs.query(test::test_forwardable_attr{}) == 42);
    test_single_sources();
    test_multiple_sources();
    test_dependent_sources();
}
