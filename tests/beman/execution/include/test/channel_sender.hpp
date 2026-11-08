// tests/beman/execution/include/test/channel_sender.hpp              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_TESTS_BEMAN_EXECUTION_INCLUDE_TEST_CHANNEL_SENDER
#define INCLUDED_TESTS_BEMAN_EXECUTION_INCLUDE_TEST_CHANNEL_SENDER

#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#endif
#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_MODULES
import beman.execution;
#else
#include <beman/execution.hpp>
#endif
#include <test/execution.hpp>

// ----------------------------------------------------------------------------

namespace test {
template <typename Tag>
struct channel_domain {};

struct scheduler_domain {};

//! A scheduler whose scheduling failures have a known domain but an unspecified execution agent.
template <typename... Sigs>
struct completion_scheduler {
    using scheduler_concept = test_std::scheduler_tag;
    int id{};

    struct attrs {
        int id;
        template <typename Tag>
        auto query(test_std::get_completion_scheduler_t<Tag>) const noexcept -> completion_scheduler {
            return {id};
        }
        template <typename Tag>
        static auto query(test_std::get_completion_domain_t<Tag>) noexcept -> scheduler_domain {
            return {};
        }
    };

    struct sender {
        using sender_concept = test_std::sender_tag;
        int id;
        template <typename...>
        static consteval auto get_completion_signatures() noexcept {
            return test_std::completion_signatures<Sigs...>{};
        }
        auto get_env() const noexcept -> attrs { return {id}; }
    };

    auto schedule() const noexcept -> sender { return {id}; }
    auto query(test_std::get_completion_scheduler_t<test_std::set_value_t>) const noexcept -> completion_scheduler {
        return *this;
    }
    static auto query(test_std::get_completion_domain_t<test_std::set_value_t>) noexcept -> scheduler_domain {
        return {};
    }
    static auto query(test_std::get_domain_t) noexcept -> scheduler_domain { return {}; }
    static auto query(test_std::get_forward_progress_guarantee_t) noexcept {
        return test_std::forward_progress_guarantee::weakly_parallel;
    }
    auto operator==(const completion_scheduler&) const -> bool = default;
};

//! A scheduler identified by an id.
struct channel_scheduler {
    using scheduler_concept = test_std::scheduler_tag;
    int id{};

    struct sender {
        using sender_concept = test_std::sender_tag;
        template <typename...>
        static consteval auto get_completion_signatures() noexcept {
            return test_std::completion_signatures<test_std::set_value_t()>{};
        }
    };
    auto        schedule() const noexcept -> sender { return {}; }
    static auto query(test_std::get_forward_progress_guarantee_t) noexcept {
        return test_std::forward_progress_guarantee::weakly_parallel;
    }
    auto operator==(const channel_scheduler&) const -> bool = default;
};

//! Attributes reporting a different completion scheduler and domain for each completion channel:
//! the scheduler ids are 1 for set_value_t, 2 for set_error_t, and 3 for set_stopped_t.
struct channel_attrs {
    template <typename Tag>
    static constexpr int id = std::same_as<Tag, test_std::set_value_t>   ? 1
                              : std::same_as<Tag, test_std::set_error_t> ? 2
                                                                         : 3;

    template <typename Tag>
    auto query(test_std::get_completion_scheduler_t<Tag>) const noexcept {
        return channel_scheduler{id<Tag>};
    }
    template <typename Tag>
        requires(!std::same_as<Tag, void>)
    auto query(test_std::get_completion_domain_t<Tag>) const noexcept {
        return channel_domain<Tag>{};
    }
};

//! A sender with the completion signatures Sigs... and channel_attrs as attributes.
template <typename... Sigs>
struct channel_sender {
    using sender_concept = test_std::sender_tag;
    static auto get_env() noexcept -> channel_attrs { return {}; }
    template <typename...>
    static consteval auto get_completion_signatures() noexcept {
        return test_std::completion_signatures<Sigs...>{};
    }
};

//! Check get_completion_scheduler<Tag>: Id 0 means it is ill-formed.
template <typename Tag, int Id, typename Sender, typename... Env>
auto check_channel_scheduler(const Sender& sndr, const Env&... env) -> bool {
    if constexpr (Id == 0) {
        return !requires { test_std::get_completion_scheduler<Tag>(test_std::get_env(sndr), env...); };
    } else {
        return test_std::get_completion_scheduler<Tag>(test_std::get_env(sndr), env...) == channel_scheduler{Id};
    }
}

//! Check get_completion_domain<Tag>: Domain void means it is ill-formed.
template <typename Tag, typename Domain, typename Sender, typename... Env>
consteval auto check_channel_domain() -> bool {
    if constexpr (std::same_as<Domain, void>) {
        return !requires(const Sender& sndr, const Env&... env) {
            test_std::get_completion_domain<Tag>(test_std::get_env(sndr), env...);
        };
    } else {
        return std::same_as<Domain,
                            decltype(test_std::get_completion_domain<Tag>(
                                test_std::get_env(std::declval<const Sender&>()), std::declval<const Env&>()...))>;
    }
}
} // namespace test

// ----------------------------------------------------------------------------

#endif // INCLUDED_TESTS_BEMAN_EXECUTION_INCLUDE_TEST_CHANNEL_SENDER
