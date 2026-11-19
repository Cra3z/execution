// src/beman/execution/tests/exec-then.test.cpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <test/execution.hpp>
#include <test/completion_test.hpp>
#include <test/sender_env.hpp>
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <memory_resource>
#include <optional>
#include <tuple>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
#else
#include <beman/execution/detail/then.hpp>
#include <beman/execution/detail/completion_signatures_of_t.hpp>
#include <beman/execution/detail/connect.hpp>
#include <beman/execution/detail/env.hpp>
#include <beman/execution/detail/forwarding_query.hpp>
#include <beman/execution/detail/get_completion_domain.hpp>
#include <beman/execution/detail/get_completion_scheduler.hpp>
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/prop.hpp>
#include <beman/execution/detail/read_env.hpp>
#include <beman/execution/detail/run_loop.hpp>
#include <beman/execution/detail/schedule.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_in.hpp>
#include <beman/execution/detail/start.hpp>
#include <beman/execution/detail/stopped_as_error.hpp>
#include <beman/execution/detail/stopped_as_optional.hpp>
#include <beman/execution/detail/sync_wait.hpp>
#endif

// ----------------------------------------------------------------------------

namespace {
struct error {
    int value{};
};
struct non_sender {};

struct get_value_t : test_std::forwarding_query_t {
    auto operator()(const auto& e) const noexcept -> int { return e.query(*this); }
};

struct receiver {
    using receiver_concept = test_std::receiver_tag;

    auto set_error(auto&&) && noexcept -> void {}
    auto set_stopped() && noexcept -> void {}
    auto set_value(auto&&...) && noexcept -> void {}

    struct env {
        auto query(const get_value_t&) const noexcept { return 42; }
    };
    auto get_env() const noexcept { return env{}; }
};

template <typename... T>
struct sender {
    using sender_concept        = test_std::sender_tag;
    using completion_signatures = test_std::completion_signatures<T...>;
    template <typename, typename...>
    static consteval auto get_completion_signatures() -> completion_signatures {
        return {};
    }
};

constexpr auto consume_all = [](auto&&...) {};

template <bool Expect>
auto test_has(auto cpo, auto in_sender, auto fun) -> void {
    static_assert(test_std::receiver<receiver>);
    static_assert(Expect == requires {
        { cpo(in_sender, fun) } -> test_std::sender;
    });
    if constexpr (Expect) {
        static_assert(requires { cpo(fun); });
        static_assert(requires { in_sender | cpo(fun); });
        static_assert(requires {
            { in_sender | cpo(fun) } -> test_std::sender;
        });
        static_assert(requires {
            { in_sender | cpo(fun) | cpo(consume_all) } -> test_std::sender;
        });
        auto sender{cpo(in_sender, fun)};
        auto op{test_std::connect(::std::move(sender), receiver{})};
        test_std::start(op);

        const auto csender{cpo(in_sender, fun)};
        auto       cop{test_std::connect(std::move(csender), receiver{})};
        test_std::start(cop);
    }
}

template <typename... Completions>
auto test_then_type(auto sender) {
    static_assert(std::same_as<test_std::completion_signatures<Completions...>,
                               test_std::completion_signatures_of_t<decltype(sender), test_std::env<>>>);
}

auto test_then_type() -> void {
    test_then_type<test_std::set_value_t()>(test_std::just(0) | test_std::then([](auto) noexcept {}));
    test_then_type<test_std::set_value_t(int)>(test_std::just() | test_std::then([]() noexcept { return 0; }));
    test_then_type<test_std::set_error_t(int)>(test_std::just_error(0) | test_std::then([]() noexcept { return 0; }));
    test_then_type<test_std::set_stopped_t()>(test_std::just_stopped() | test_std::then([]() noexcept { return 0; }));

    test_then_type<test_std::set_value_t()>(test_std::just() | test_std::upon_error([]() noexcept { return 0; }));
    test_then_type<test_std::set_value_t(int)>(test_std::just_error(error{}) |
                                               test_std::upon_error([](error) noexcept { return 0; }));
    test_then_type<test_std::set_stopped_t()>(test_std::just_stopped() | test_std::upon_error([]() noexcept {}));

    test_then_type<test_std::set_value_t(int)>(test_std::just(0) | test_std::upon_stopped([]() noexcept {}));
    test_then_type<test_std::set_error_t(int)>(test_std::just_error(0) | test_std::upon_stopped([]() noexcept {}));
    test_then_type<test_std::set_value_t()>(test_std::just_stopped() | test_std::upon_stopped([]() noexcept {}));
}

auto test_then_multi_type() -> void {
    static_assert(test_std::sender<sender<>>);
    static_assert(test_std::sender_in<sender<>>);

    test_then_type<test_std::set_value_t(),
                   test_std::set_value_t(int, int),
                   test_std::set_error_t(error),
                   test_std::set_stopped_t()>(sender<test_std::set_value_t(),
                                                     test_std::set_value_t(int, int),
                                                     test_std::set_error_t(error),
                                                     test_std::set_stopped_t()>());
    test_then_type<test_std::set_value_t(bool),
                   test_std::set_error_t(error),
                   test_std::set_stopped_t(),
                   test_std::set_error_t(::std::exception_ptr)>(sender<test_std::set_value_t(),
                                                                       test_std::set_value_t(int, int),
                                                                       test_std::set_error_t(error),
                                                                       test_std::set_stopped_t()>() |
                                                                test_std::then([](auto&&...) { return true; }));
    test_then_type<test_std::set_value_t(),
                   test_std::set_value_t(int, int),
                   test_std::set_value_t(bool),
                   test_std::set_stopped_t(),
                   test_std::set_error_t(::std::exception_ptr)>(sender<test_std::set_value_t(),
                                                                       test_std::set_value_t(int, int),
                                                                       test_std::set_error_t(error),
                                                                       test_std::set_stopped_t()>() |
                                                                test_std::upon_error([](error) { return true; }));
    test_then_type<test_std::set_value_t(),
                   test_std::set_value_t(int, int),
                   test_std::set_stopped_t(),
                   test_std::set_error_t(::std::exception_ptr)>(sender<test_std::set_value_t(),
                                                                       test_std::set_value_t(int, int),
                                                                       test_std::set_error_t(error),
                                                                       test_std::set_stopped_t()>() |
                                                                test_std::upon_error([](error) {}));
    test_then_type<test_std::set_value_t(int),
                   test_std::set_value_t(int, int),
                   test_std::set_error_t(error),
                   test_std::set_value_t()>(sender<test_std::set_value_t(int),
                                                   test_std::set_value_t(int, int),
                                                   test_std::set_error_t(error),
                                                   test_std::set_stopped_t()>() |
                                            test_std::upon_stopped([]() noexcept {}));
    test_then_type<test_std::set_value_t(), test_std::set_value_t(int, int), test_std::set_error_t(error)>(
        sender<test_std::set_value_t(),
               test_std::set_value_t(int, int),
               test_std::set_error_t(error),
               test_std::set_stopped_t()>() |
        test_std::upon_stopped([]() noexcept {}));
    test_then_type<test_std::set_value_t(),
                   test_std::set_value_t(int, int),
                   test_std::set_error_t(error),
                   test_std::set_error_t(::std::exception_ptr)>(sender<test_std::set_value_t(),
                                                                       test_std::set_value_t(int, int),
                                                                       test_std::set_error_t(error),
                                                                       test_std::set_stopped_t()>() |
                                                                test_std::upon_stopped([] {}));
}

auto test_then_value() -> void {
    ASSERT(
        std::tuple{17} ==
        test_std::sync_wait(test_std::just(5, 12) | test_std::then([](int a, int b) { return a + b; })).value_or(0));
    ASSERT(std::tuple{17} ==
           test_std::sync_wait(test_std::just_error(17) | test_std::upon_error([](int a) { return a; })));
    ASSERT(std::tuple{17} ==
           test_std::sync_wait(test_std::just_stopped() | test_std::upon_stopped([]() { return 17; })));
}

struct memory_env {
    std::pmr::polymorphic_allocator<> allocator;
    auto                              query(const test_std::get_allocator_t&) const noexcept { return allocator; }
};
struct memory_receiver {
    using receiver_concept = test_std::receiver_tag;
    std::pmr::polymorphic_allocator<> allocator;
    auto                              get_env() const noexcept { return memory_env{this->allocator}; }

    auto set_error(auto&&) && noexcept -> void {}
    auto set_stopped() && noexcept -> void {}
    auto set_value(auto&&...) && noexcept -> void {}
};
struct counting_resource : std::pmr::memory_resource {
    std::size_t count{};
    auto        do_allocate(std::size_t size, std::size_t) -> void* override {
        ++this->count;
        return operator new(size);
    }
    auto do_deallocate(void* p, std::size_t, std::size_t) -> void override { operator delete(p); }
    auto do_is_equal(const std::pmr::memory_resource& other) const noexcept -> bool override { return this == &other; }
};

struct allocator_fun {
    using allocator_type = std::pmr::polymorphic_allocator<>;

    std::pmr::polymorphic_allocator<> alloc;
    std::byte*                        data{nullptr};

    explicit allocator_fun(std::pmr::polymorphic_allocator<> all) : alloc(all), data(alloc.allocate(1)) {}
    allocator_fun(const allocator_fun&, std::pmr::polymorphic_allocator<> = {}) {}
    allocator_fun(allocator_fun&& other) noexcept : alloc(other.alloc), data(std::exchange(other.data, nullptr)) {}
    allocator_fun(allocator_fun&& other, std::pmr::polymorphic_allocator<> all)
        : alloc(all), data(alloc == other.alloc ? std::exchange(other.data, nullptr) : alloc.allocate(1)) {}
    ~allocator_fun() {
        if (this->data)
            this->alloc.deallocate(this->data, 1u);
    }
    auto operator=(const allocator_fun&) -> allocator_fun = delete;
    auto operator=(allocator_fun&&) -> allocator_fun      = delete;

    auto operator()(auto&&...) const {}
};

auto test_then_allocator() -> void {
    static_assert(std::uses_allocator_v<allocator_fun, std::pmr::polymorphic_allocator<>>);
    counting_resource resource1{};
    ASSERT(resource1.count == 0u);
    auto sender{test_std::just() | test_std::then(allocator_fun(&resource1))};
    ASSERT(resource1.count == 1u);

    counting_resource resource2{};
    ASSERT(resource2.count == 0u);
    auto state{test_std::connect(std::move(sender), memory_receiver{&resource2})};
    ASSERT(resource2.count == 1u);
}

auto test_then_env() -> void {
    {
        receiver r{};
        auto     e{test_std::get_env(r)};
        ASSERT(e.query(get_value_t{}) == 42);
        ASSERT(get_value_t{}(e) == 42);
    }
    {
        auto state{test_std::connect(test_std::read_env(get_value_t{}), receiver{})};
        test_std::start(state);
    }
    {
        int  value{0};
        auto state{test_std::connect(test_std::then(test_std::read_env(get_value_t{}), [&value](int v) { value = v; }),
                                     receiver{})};
        ASSERT(value == 0);
        test_std::start(state);
        ASSERT(value == 42);
    }
}

auto test_then_completions() {
    test_std::sync_wait(test::completion_test(test_std::just() | test_std::then([]() {})));
    test_std::sync_wait(test::completion_test(test_std::just() | test_std::then([]() noexcept {})));
    test_std::sync_wait(test::completion_test(test_std::just() | test_std::then([](auto&&...) noexcept {})));
}

struct attribute_domain : test_std::default_domain {};

struct value_domain : test_std::default_domain {};
struct error_domain : test_std::default_domain {};
struct stopped_domain : test_std::default_domain {};

using loop_scheduler = decltype(std::declval<test_std::run_loop&>().get_scheduler());

template <bool Dependent = false>
struct attribute_sender {
    using sender_concept = test_std::sender_tag;
    loop_scheduler value_scheduler;
    loop_scheduler error_scheduler;
    loop_scheduler stopped_scheduler;

    template <typename Env>
    static constexpr bool sends_error_and_stopped = requires(const Env& env) { env.query(get_value_t{}); };

    template <typename Self, typename... Env>
        requires(!Dependent || sizeof...(Env) == 1)
    static consteval auto get_completion_signatures() {
        if constexpr (Dependent && !(sends_error_and_stopped<Env> && ...)) {
            return test_std::completion_signatures<test_std::set_value_t(int)>{};
        } else {
            return test_std::completion_signatures<test_std::set_value_t(int),
                                                   test_std::set_error_t(int),
                                                   test_std::set_stopped_t()>{};
        }
    }

    struct attributes {
        loop_scheduler value_scheduler;
        loop_scheduler error_scheduler;
        loop_scheduler stopped_scheduler;

        template <typename Tag, typename... Env>
        auto query(test_std::get_completion_scheduler_t<Tag>, const Env&...) const noexcept {
            if constexpr (std::same_as<Tag, test_std::set_value_t>) {
                return value_scheduler;
            } else if constexpr (std::same_as<Tag, test_std::set_error_t>) {
                return error_scheduler;
            } else {
                return stopped_scheduler;
            }
        }

        template <typename Tag, typename... Env>
            requires(!std::same_as<Tag, void>)
        static auto query(test_std::get_completion_domain_t<Tag>, const Env&...) noexcept {
            if constexpr (std::same_as<Tag, test_std::set_value_t>) {
                return value_domain{};
            } else if constexpr (std::same_as<Tag, test_std::set_error_t>) {
                return error_domain{};
            } else {
                return stopped_domain{};
            }
        }
    };
    auto get_env() const noexcept { return attributes{value_scheduler, error_scheduler, stopped_scheduler}; }
};

template <typename Tag, typename Sender, typename... Env>
concept has_completion_domain = requires(const Sender& sndr, const Env&... env) {
    test_std::get_completion_domain<Tag>(test_std::get_env(sndr), env...);
};

template <typename Tag, typename Sender, typename... Env>
concept has_completion_scheduler = requires(const Sender& sndr, const Env&... env) {
    test_std::get_completion_scheduler<Tag>(test_std::get_env(sndr), env...);
};

auto test_adaptor_multiple_completion_sources() -> void {
    test_std::run_loop value_loop;
    test_std::run_loop error_loop;
    test_std::run_loop stopped_loop;
    auto               child =
        attribute_sender<>{value_loop.get_scheduler(), error_loop.get_scheduler(), stopped_loop.get_scheduler()};
    auto receiver_env = test_std::env{};

    auto then_nothrow          = child | test_std::then([](int i) noexcept { return i; });
    auto then_throwing         = child | test_std::then([](int i) { return i; });
    auto upon_error            = child | test_std::upon_error([](int i) noexcept { return i; });
    auto upon_stopped          = child | test_std::upon_stopped([]() noexcept { return 1; });
    auto upon_error_throwing   = child | test_std::upon_error([](int i) { return i; });
    auto upon_stopped_throwing = child | test_std::upon_stopped([]() { return 1; });
    auto optional              = child | test_std::stopped_as_optional;
    auto error                 = child | test_std::stopped_as_error(-1);

    ASSERT(test_std::get_completion_scheduler<test_std::set_error_t>(test_std::get_env(then_nothrow), receiver_env) ==
           error_loop.get_scheduler());
    static_assert(std::same_as<decltype(test_std::get_completion_domain<test_std::set_error_t>(
                                   test_std::get_env(then_throwing), receiver_env)),
                               test_std::indeterminate_domain<value_domain, error_domain>>);
    static_assert(!has_completion_scheduler<test_std::set_error_t, decltype(then_throwing), decltype(receiver_env)>);
    static_assert(std::same_as<decltype(test_std::get_completion_domain<test_std::set_value_t>(
                                   test_std::get_env(upon_error), receiver_env)),
                               test_std::indeterminate_domain<value_domain, error_domain>>);
    static_assert(!has_completion_scheduler<test_std::set_value_t, decltype(upon_error), decltype(receiver_env)>);
    static_assert(std::same_as<decltype(test_std::get_completion_domain<test_std::set_value_t>(
                                   test_std::get_env(upon_stopped), receiver_env)),
                               test_std::indeterminate_domain<value_domain, stopped_domain>>);
    static_assert(!has_completion_scheduler<test_std::set_value_t, decltype(upon_stopped), decltype(receiver_env)>);
    static_assert(std::same_as<decltype(test_std::get_completion_domain<test_std::set_value_t>(
                                   test_std::get_env(optional), receiver_env)),
                               test_std::indeterminate_domain<value_domain, stopped_domain>>);
    static_assert(!has_completion_scheduler<test_std::set_value_t, decltype(optional), decltype(receiver_env)>);
    static_assert(std::same_as<decltype(test_std::get_completion_domain<test_std::set_error_t>(
                                   test_std::get_env(error), receiver_env)),
                               test_std::indeterminate_domain<error_domain, stopped_domain>>);
    static_assert(!has_completion_scheduler<test_std::set_error_t, decltype(error), decltype(receiver_env)>);
    static_assert(!has_completion_domain<test_std::set_stopped_t, decltype(error), decltype(receiver_env)>);
    static_assert(!has_completion_domain<test_std::set_error_t, decltype(upon_error), decltype(receiver_env)>);
    static_assert(!has_completion_domain<test_std::set_stopped_t, decltype(upon_stopped), decltype(receiver_env)>);
    static_assert(!has_completion_domain<test_std::set_stopped_t, decltype(optional), decltype(receiver_env)>);
    static_assert(!has_completion_scheduler<test_std::set_stopped_t, decltype(error), decltype(receiver_env)>);
    ASSERT(test_std::get_completion_scheduler<test_std::set_error_t>(test_std::get_env(upon_error_throwing),
                                                                     receiver_env) == error_loop.get_scheduler());
    static_assert(std::same_as<decltype(test_std::get_completion_domain<test_std::set_error_t>(
                                   test_std::get_env(upon_stopped_throwing), receiver_env)),
                               test_std::indeterminate_domain<error_domain, stopped_domain>>);
    static_assert(
        !has_completion_scheduler<test_std::set_error_t, decltype(upon_stopped_throwing), decltype(receiver_env)>);

    auto dependent_child =
        attribute_sender<true>{value_loop.get_scheduler(), error_loop.get_scheduler(), stopped_loop.get_scheduler()};
    auto           dependent = dependent_child | test_std::upon_error([](int i) noexcept { return i; });
    test_std::prop env_with_error_and_stopped{get_value_t{}, 42};
    static_assert(!has_completion_scheduler<test_std::set_value_t, decltype(dependent)>);
    ASSERT(test_std::get_completion_scheduler<test_std::set_value_t>(test_std::get_env(dependent), receiver_env) ==
           value_loop.get_scheduler());
    static_assert(std::same_as<decltype(test_std::get_completion_domain<test_std::set_value_t>(
                                   test_std::get_env(dependent), receiver_env)),
                               value_domain>);
    static_assert(std::same_as<decltype(test_std::get_completion_domain<test_std::set_value_t>(
                                   test_std::get_env(dependent), env_with_error_and_stopped)),
                               test_std::indeterminate_domain<value_domain, error_domain>>);
    static_assert(
        !has_completion_scheduler<test_std::set_value_t, decltype(dependent), decltype(env_with_error_and_stopped)>);

    auto dependent_then = dependent_child | test_std::then([](int i) noexcept { return i; });
    static_assert(has_completion_domain<test_std::set_stopped_t, decltype(dependent_then)>);
    static_assert(!has_completion_domain<test_std::set_stopped_t, decltype(dependent_then), decltype(receiver_env)>);
}

auto test_adaptor_completion_attributes() -> void {
    test_std::run_loop loop;
    struct env {
        decltype(loop.get_scheduler()) scheduler;
        auto                           query(test_std::get_start_scheduler_t) const noexcept { return scheduler; }
        static auto                    query(test_std::get_domain_t) noexcept { return attribute_domain{}; }
    };
    env receiver_env{loop.get_scheduler()};

    auto value            = test_std::just(1) | test_std::then([](int i) noexcept { return i; });
    auto error_to_value   = test_std::just_error(-1) | test_std::upon_error([](int i) noexcept { return i; });
    auto stopped_to_value = test_std::just_stopped() | test_std::upon_stopped([]() noexcept { return 1; });
    auto stopped_to_error = test_std::just_stopped() | test_std::stopped_as_error(-1);
    auto optional_value   = test_std::just(1) | test_std::stopped_as_optional;
    auto scheduled        = test_std::schedule(loop.get_scheduler()) | test_std::then([]() noexcept {});

    ASSERT(test_std::get_completion_scheduler<test_std::set_value_t>(test_std::get_env(scheduled)) ==
           loop.get_scheduler());
    static_assert(std::same_as<decltype(test_std::get_completion_domain<test_std::set_value_t>(
                                   test_std::get_env(scheduled), receiver_env)),
                               test_std::default_domain>);
    static_assert(!has_completion_scheduler<test_std::set_stopped_t, decltype(scheduled), test_std::env<>>);

    ASSERT(test_std::get_completion_scheduler<test_std::set_value_t>(test_std::get_env(value), receiver_env) ==
           loop.get_scheduler());
    ASSERT(test_std::get_completion_scheduler<test_std::set_value_t>(test_std::get_env(error_to_value),
                                                                     receiver_env) == loop.get_scheduler());
    ASSERT(test_std::get_completion_scheduler<test_std::set_value_t>(test_std::get_env(stopped_to_value),
                                                                     receiver_env) == loop.get_scheduler());
    ASSERT(test_std::get_completion_scheduler<test_std::set_error_t>(test_std::get_env(stopped_to_error),
                                                                     receiver_env) == loop.get_scheduler());
    ASSERT(test_std::get_completion_scheduler<test_std::set_value_t>(test_std::get_env(optional_value),
                                                                     receiver_env) == loop.get_scheduler());

    static_assert(std::same_as<decltype(test_std::get_completion_domain<test_std::set_value_t>(
                                   test_std::get_env(error_to_value), receiver_env)),
                               attribute_domain>);
    static_assert(std::same_as<decltype(test_std::get_completion_domain<test_std::set_value_t>(
                                   test_std::get_env(stopped_to_value), receiver_env)),
                               attribute_domain>);
    static_assert(std::same_as<decltype(test_std::get_completion_domain<test_std::set_error_t>(
                                   test_std::get_env(stopped_to_error), receiver_env)),
                               attribute_domain>);
    static_assert(std::same_as<decltype(test_std::get_completion_domain<test_std::set_value_t>(
                                   test_std::get_env(optional_value), receiver_env)),
                               attribute_domain>);

    struct throwing_copy {
        throwing_copy() = default;
        throwing_copy(const throwing_copy&) noexcept(false) {}
    };
    const throwing_copy arg{};
    auto optional_error = test_std::just() | test_std::then([&]() noexcept -> const throwing_copy& { return arg; }) |
                          test_std::stopped_as_optional;
    ASSERT(test_std::get_completion_scheduler<test_std::set_error_t>(test_std::get_env(optional_error),
                                                                     receiver_env) == loop.get_scheduler());
    static_assert(std::same_as<decltype(test_std::get_completion_domain<test_std::set_error_t>(
                                   test_std::get_env(optional_error), receiver_env)),
                               attribute_domain>);
}

auto test_then_attributes() {
    test::sender_env s{42};
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, s);
    test::test_sender_env<true>(84, test::test_non_forwardable_attr{}, s);
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, s | test_std::then([]() {}));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, s | test_std::then([]() {}));
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, s | test_std::upon_error([](auto) {}));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, s | test_std::upon_error([](auto) {}));
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, s | test_std::upon_stopped([]() {}));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, s | test_std::upon_stopped([]() {}));
}
} // namespace

TEST(exec_then) {
    static_assert(std::same_as<const test_std::then_t, decltype(test_std::then)>);
    static_assert(std::same_as<const test_std::upon_error_t, decltype(test_std::upon_error)>);
    static_assert(std::same_as<const test_std::upon_stopped_t, decltype(test_std::upon_stopped)>);

    test_has<false>(test_std::then, non_sender{}, [] {});
    test_has<true>(test_std::then, test_std::just(), [] {});
    test_has<true>(test_std::then, test_std::just_error(error{3}), [] {});
    test_has<true>(test_std::then, test_std::just_stopped(), [] {});

    test_then_type();
    test_then_multi_type();
    test_then_value();
    test_then_allocator();

    test_then_env();
    test_then_completions();
    test_then_attributes();
    test_adaptor_completion_attributes();
    test_adaptor_multiple_completion_sources();
}
