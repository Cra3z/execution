// examples/wasm/timer.hpp                                            -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// A sender completing after a number of milliseconds (Emscripten only).
// In contrast to a JavaScript promise wrapping setTimeout() the timer is really
// cancelled by a stop request: nothing keeps the event loop (e.g. node) alive.

#ifndef BEMAN_EXECUTION_EXAMPLES_WASM_TIMER_HPP
#define BEMAN_EXECUTION_EXAMPLES_WASM_TIMER_HPP

#include <emscripten/eventloop.h>
#include <optional>
#include <utility>
#include <beman/execution.hpp>

namespace wasm {
namespace ex = beman::execution;

template <ex::receiver Receiver>
struct sleep_op_state {
    using operation_state_concept = ex::operation_state_tag;
    struct on_stop {
        sleep_op_state* self;
        auto            operator()() noexcept -> void {
            emscripten_clear_timeout(self->id);
            self->callback.reset();
            ex::set_stopped(std::move(self->receiver));
        }
    };
    using token_t    = ex::stop_token_of_t<ex::env_of_t<Receiver>>;
    using callback_t = ex::stop_callback_for_t<token_t, on_stop>;

    Receiver                  receiver;
    double                    ms;
    int                       id{0};
    std::optional<callback_t> callback;

    template <typename R>
    sleep_op_state(R&& r, double milliseconds) : receiver(std::forward<R>(r)), ms(milliseconds) {}
    sleep_op_state(sleep_op_state&&) = delete;

    auto start() & noexcept -> void {
        // a stop callback registered after a stop request would run during its
        // construction, possibly destroying this state: don't get there
        auto token = ex::get_stop_token(ex::get_env(receiver));
        if (token.stop_requested()) {
            ex::set_stopped(std::move(receiver));
            return;
        }
        id = emscripten_set_timeout(
            [](void* self) {
                auto* op = static_cast<sleep_op_state*>(self);
                op->callback.reset();
                ex::set_value(std::move(op->receiver));
            },
            ms,
            this);
        callback.emplace(std::move(token), on_stop{this});
    }
};

struct sleep_sender {
    using sender_concept        = ex::sender_tag;
    using completion_signatures = ex::completion_signatures<ex::set_value_t(), ex::set_stopped_t()>;
    template <typename...>
    static consteval auto get_completion_signatures() noexcept -> completion_signatures {
        return {};
    }

    double ms;

    template <ex::receiver Receiver>
    auto connect(Receiver&& receiver) && -> sleep_op_state<std::remove_cvref_t<Receiver>> {
        return {std::forward<Receiver>(receiver), ms};
    }
};
static_assert(ex::sender<sleep_sender>);

inline auto sleep_for(double milliseconds) -> sleep_sender { return {milliseconds}; }

} // namespace wasm

#endif
