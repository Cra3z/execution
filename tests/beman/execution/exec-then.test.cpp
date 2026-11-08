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
#include <exception>
#include <memory_resource>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution;
#else
#include <beman/execution.hpp>
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

namespace completion_attributes {
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

struct scheduler_attrs {
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
};

template <typename Domain = void>
struct channel_attrs : scheduler_attrs {
    using scheduler_attrs::query;

    template <typename Tag>
    auto query(test_std::get_completion_domain_t<Tag>) const noexcept
        -> std::conditional_t<std::same_as<Tag, void>,
                              stale_domain,
                              std::conditional_t<std::same_as<Domain, void>, channel_domain<Tag>, Domain>> {
        return {};
    }
    template <typename Tag, typename Env>
        requires requires(const Env& env) { env.query(offset_t{}); }
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
auto check_scheduler(const Sender& sender, const Env&... env) -> void {
    auto attrs = test_std::get_env(sender);
    if constexpr (Expected == 0) {
        static_assert(!requires { test_std::get_completion_scheduler<Tag>(attrs, env...); });
    } else {
        static_assert(noexcept(test_std::get_completion_scheduler<Tag>(attrs, env...)));
        ASSERT(test_std::get_completion_scheduler<Tag>(attrs, env...).id == Expected);
    }
}

template <typename Tag, typename Expected, typename Sender, typename... Env>
auto check_domain(const Sender& sender, const Env&... env) -> void {
    auto attrs = test_std::get_env(sender);
    if constexpr (std::same_as<Expected, void>) {
        static_assert(!requires { test_std::get_completion_domain<Tag>(attrs, env...); });
    } else {
        static_assert(std::same_as<Expected, decltype(test_std::get_completion_domain<Tag>(attrs, env...))>);
        static_assert(noexcept(test_std::get_completion_domain<Tag>(attrs, env...)));
    }
}

auto test_single_sources() -> void {
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

auto test_multiple_sources() -> void {
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

    auto shared =
        attribute_sender<channel_attrs<shared_domain>, value(), error(int), stop()>{} | test_std::upon_stopped([] {});
    check_scheduler<value, 0>(shared);
    check_scheduler<error, 0>(shared);
    check_domain<value, shared_domain>(shared);
    check_domain<error, shared_domain>(shared);

    auto no_direct_domain =
        attribute_sender<scheduler_attrs, value(), error(int)>{} | test_std::upon_error([](int) noexcept {});
    check_scheduler<value, 0>(no_direct_domain);
    check_domain<value, shared_domain>(no_direct_domain);

    // The set_value domain of the child is unknown: without an environment it may still become
    // known, so the set_error domain is not reported yet; with one it is indeterminate.
    auto partial = attribute_sender<partial_domain_attrs, value(), error(int)>{} | test_std::then([] {});
    check_domain<error, void>(partial);
    []<typename... Domains>(test_std::indeterminate_domain<Domains...>) {
    }(test_std::get_completion_domain<error>(test_std::get_env(partial), test_std::env<>{}));
}

auto test_dependent_sources() -> void {
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
} // namespace completion_attributes

auto test_then_attributes() {
    test::sender_env s{42};
    auto             attrs = test_std::get_env(s | test_std::then([] {}));
    static_assert(noexcept(attrs.query(test::test_forwardable_attr{})));
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, s);
    test::test_sender_env<true>(84, test::test_non_forwardable_attr{}, s);
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, s | test_std::then([]() {}));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, s | test_std::then([]() {}));
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, s | test_std::upon_error([](auto) {}));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, s | test_std::upon_error([](auto) {}));
    test::test_sender_env<true>(42, test::test_forwardable_attr{}, s | test_std::upon_stopped([]() {}));
    test::test_sender_env<false>(84, test::test_non_forwardable_attr{}, s | test_std::upon_stopped([]() {}));

    completion_attributes::test_single_sources();
    completion_attributes::test_multiple_sources();
    completion_attributes::test_dependent_sources();

    test_std::run_loop loop;
    test_std::prop     env{test_std::get_start_scheduler, loop.get_scheduler()};
    {
        auto sndr = test_std::just_error(0) | test_std::upon_error([](int i) noexcept { return i; });
        auto sch  = test_std::get_completion_scheduler<test_std::set_value_t>(test_std::get_env(sndr), env);
        ASSERT(sch == loop.get_scheduler());
    }
    {
        auto sndr = test_std::just_stopped() | test_std::upon_stopped([]() noexcept {});
        auto sch  = test_std::get_completion_scheduler<test_std::set_value_t>(test_std::get_env(sndr), env);
        ASSERT(sch == loop.get_scheduler());
    }
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
}
