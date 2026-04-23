// include/beman/execution/detail/not_a_scheduler.hpp                -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_NOT_A_SCHEDULER
#define INCLUDED_BEMAN_EXECUTION_DETAIL_NOT_A_SCHEDULER

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.scheduler_t;
#else
#include <beman/execution/detail/scheduler_t.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {

// Exposition-only not-a-scheduler type per P3826R5
struct not_a_scheduler {
    using scheduler_concept = ::beman::execution::scheduler_t;

    struct not_a_sender {
        using sender_concept = void; // intentionally not a real sender
    };

    constexpr auto schedule() const noexcept -> not_a_sender { return {}; }
};

} // namespace beman::execution::detail

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_NOT_A_SCHEDULER
