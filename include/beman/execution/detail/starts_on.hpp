// include/beman/execution/detail/starts_on.hpp                     -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_STARTS_ON
#define INCLUDED_BEMAN_EXECUTION_DETAIL_STARTS_ON

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.default_domain;
import beman.execution.detail.forward_like;
import beman.execution.detail.fwd_env;
import beman.execution.detail.get_domain;
import beman.execution.detail.join_env;
import beman.execution.detail.let;
import beman.execution.detail.make_sender;
import beman.execution.detail.query_with_default;
import beman.execution.detail.sched_env;
import beman.execution.detail.schedule;
import beman.execution.detail.scheduler;
import beman.execution.detail.sender;
import beman.execution.detail.sender_for;
import beman.execution.detail.transform_sender;
#else
#include <beman/execution/detail/default_domain.hpp>
#include <beman/execution/detail/forward_like.hpp>
#include <beman/execution/detail/fwd_env.hpp>
#include <beman/execution/detail/get_domain.hpp>
#include <beman/execution/detail/continues_on.hpp>
#include <beman/execution/detail/join_env.hpp>
#include <beman/execution/detail/just.hpp>
#include <beman/execution/detail/let.hpp>
#include <beman/execution/detail/make_sender.hpp>
#include <beman/execution/detail/query_with_default.hpp>
#include <beman/execution/detail/sched_env.hpp>
#include <beman/execution/detail/schedule.hpp>
#include <beman/execution/detail/scheduler.hpp>
#include <beman/execution/detail/sender_for.hpp>
#include <beman/execution/detail/transform_sender.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {
struct starts_on_t {
    // P3826R5: transform_sender now takes Tag as first param
    template <typename Tag,
              ::beman::execution::detail::sender_for<::beman::execution::detail::starts_on_t> Sender,
              typename Env>
    auto transform_sender(Tag, Sender&& sender, const Env&) const noexcept {
        auto&& scheduler{sender.template get<1>()};
        auto&& new_sender{sender.template get<2>()};
        return ::beman::execution::let_value(
            ::beman::execution::continues_on(::beman::execution::just(), scheduler),
            [new_sender = ::beman::execution::detail::forward_like<Sender>(new_sender)]() mutable noexcept(
                ::std::is_nothrow_constructible_v<::std::decay_t<Sender>>) { return ::std::move(new_sender); });
    }

    // P3826R5: No early customization - just return make_sender directly
    template <::beman::execution::scheduler Scheduler, ::beman::execution::sender Sender>
    auto operator()(Scheduler&& scheduler, Sender&& sender) const {
        return ::beman::execution::detail::make_sender(
            *this, ::std::forward<Scheduler>(scheduler), ::std::forward<Sender>(sender));
    }
};
} // namespace beman::execution::detail

namespace beman::execution {
using starts_on_t = ::beman::execution::detail::starts_on_t;
inline constexpr ::beman::execution::detail::starts_on_t starts_on{};
} // namespace beman::execution

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_STARTS_ON
