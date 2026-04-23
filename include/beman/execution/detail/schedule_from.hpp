// include/beman/execution/detail/schedule_from.hpp                 -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_SCHEDULE_FROM
#define INCLUDED_BEMAN_EXECUTION_DETAIL_SCHEDULE_FROM

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.default_impls;
import beman.execution.detail.fwd_env;
import beman.execution.detail.get_env;
import beman.execution.detail.make_sender;
import beman.execution.detail.product_type;
import beman.execution.detail.sender;
#else
#include <beman/execution/detail/default_impls.hpp>
#include <beman/execution/detail/fwd_env.hpp>
#include <beman/execution/detail/get_env.hpp>
#include <beman/execution/detail/make_sender.hpp>
#include <beman/execution/detail/product_type.hpp>
#include <beman/execution/detail/sender.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {

// P3826R5: schedule_from(sndr) = make_sender(schedule_from, {}, sndr)
// schedule_from(sndr) is semantically equivalent to sndr.
// It exists so it can be customized by schedulers to control how to
// transition off their execution contexts.
struct schedule_from_t {
    template <::beman::execution::sender Sender>
    auto operator()(Sender&& sender) const {
        return ::beman::execution::detail::make_sender(
            *this,
            ::beman::execution::detail::product_type<>{},
            ::std::forward<Sender>(sender));
    }

    // Legacy 2-arg overload for backward compatibility during transition
    template <typename Scheduler, ::beman::execution::sender Sender>
    auto operator()(Scheduler&& /*scheduler*/, Sender&& sender) const {
        // P3826R5: The new schedule_from only takes a sender.
        // The old schedule_from(sch, sndr) behavior is now in continues_on.
        return (*this)(::std::forward<Sender>(sender));
    }
};

} // namespace beman::execution::detail

namespace beman::execution {
using schedule_from_t = ::beman::execution::detail::schedule_from_t;
inline constexpr ::beman::execution::schedule_from_t schedule_from{};
} // namespace beman::execution

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_SCHEDULE_FROM
