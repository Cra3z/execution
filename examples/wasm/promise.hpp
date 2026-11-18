// examples/wasm/promise.hpp                                          -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Interoperability between senders and JavaScript promises (Emscripten only).
//
//   from_promise(p)     a sender completing like the JavaScript promise `p`:
//                         resolved -> set_value(val), rejected -> set_error(val)
//                       A stop request completes it with set_stopped() right away.
//                       The promise itself can't be cancelled: its result is ignored.
//   to_promise(s)       starts the sender `s` and returns a JavaScript promise:
//                         set_value -> resolved, set_error -> rejected,
//                         set_stopped -> rejected with an `AbortError`.
//   to_promise(s, sig)  the same, but aborting the `AbortSignal` sig requests
//                       the sender to stop.
//
// All of it has to be used on the thread owning the JavaScript objects (the main
// thread): emscripten::val can't be used from other threads.
//
// This is deliberately not based on `co_await` of emscripten::val: if the promise
// gets rejected val's awaiter throws a JavaScript exception, which C++ code can't
// catch. Here a rejection is an ordinary error completion.
//
// The functions `settle` and `abort` have to be exported to JavaScript (see
// BEMAN_WASM_PROMISE_BINDINGS) because the JavaScript side calls them back.

#ifndef BEMAN_EXECUTION_EXAMPLES_WASM_PROMISE_HPP
#define BEMAN_EXECUTION_EXAMPLES_WASM_PROMISE_HPP

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <beman/execution.hpp>

namespace wasm {
namespace ex = beman::execution;
using emscripten::val;

// ----------------------------------------------------------------------------
// the small JavaScript part

inline auto js() -> const val& {
    static const val lib = val::global("Function").new_(std::string(R"(
        return {
            attach(promise, settle, state) {
                promise.then(v => settle(state, true, v), e => settle(state, false, e));
            },
            deferred() {
                let resolve, reject;
                const promise = new Promise((a, b) => { resolve = a; reject = b; });
                return { promise, resolve, reject };
            },
            listen(signal, abort, id) {
                const listener = () => abort(id);
                signal.addEventListener('abort', listener, { once: true });
                return () => signal.removeEventListener('abort', listener);
            },
            error(name, message) {
                const error = new Error(message);
                error.name = name;
                return error;
            },
        };
    )"))();
    return lib;
}

inline auto make_error(const std::string& name, const std::string& message) -> val {
    return js().call<val>("error", name, message);
}

// ----------------------------------------------------------------------------
// callbacks from JavaScript

namespace detail {
// Shared between a from_promise operation and the pending JavaScript callback:
// the operation may be stopped or destroyed before the promise is settled.
struct settle_state {
    std::function<void(bool, val)> deliver;
};
using settle_ptr = std::shared_ptr<settle_state>;

struct abort_state {
    ex::inplace_stop_source source;
};

inline auto abort_registry() -> std::unordered_map<std::uint32_t, std::weak_ptr<abort_state>>& {
    static std::unordered_map<std::uint32_t, std::weak_ptr<abort_state>> registry;
    return registry;
}
} // namespace detail

// Called by JavaScript, owns the heap allocated std::shared_ptr behind `state`.
inline auto settle(std::uintptr_t state, bool resolved, val value) -> void {
    std::unique_ptr<detail::settle_ptr> owner(reinterpret_cast<detail::settle_ptr*>(state));
    if (auto deliver = std::exchange((*owner)->deliver, nullptr)) {
        deliver(resolved, std::move(value));
    }
}

// Called by JavaScript when an AbortSignal got aborted.
inline auto abort(std::uint32_t id) -> void {
    auto& registry = detail::abort_registry();
    if (auto it = registry.find(id); it != registry.end()) {
        if (auto state = it->second.lock()) {
            state->source.request_stop();
        }
    }
}

// Exports the callbacks: use once in the translation unit with the bindings.
#define BEMAN_WASM_PROMISE_BINDINGS                                                                                    \
    EMSCRIPTEN_BINDINGS(beman_wasm_promise) {                                                                          \
        emscripten::function("beman_promise_settle", &::wasm::settle);                                                 \
        emscripten::function("beman_promise_abort", &::wasm::abort);                                                   \
    }

// ----------------------------------------------------------------------------
// from_promise

template <ex::receiver Receiver>
struct promise_op_state {
    using operation_state_concept = ex::operation_state_tag;
    struct on_stop {
        promise_op_state* self;
        auto              operator()() noexcept -> void {
            if (std::exchange(self->state->deliver, nullptr)) {
                ex::set_stopped(std::move(self->receiver));
            }
        }
    };
    using token_t    = ex::stop_token_of_t<ex::env_of_t<Receiver>>;
    using callback_t = ex::stop_callback_for_t<token_t, on_stop>;

    Receiver                receiver;
    val                     promise;
    detail::settle_ptr      state{std::make_shared<detail::settle_state>()};
    std::optional<callback_t> callback;

    template <typename R>
    promise_op_state(R&& r, val p) : receiver(std::forward<R>(r)), promise(std::move(p)) {}
    promise_op_state(promise_op_state&&) = delete;
    ~promise_op_state() { state->deliver = nullptr; }

    auto start() & noexcept -> void {
        // a stop callback registered after a stop request would run during its
        // construction, possibly destroying this state: don't get there
        auto token = ex::get_stop_token(ex::get_env(receiver));
        if (token.stop_requested()) {
            ex::set_stopped(std::move(receiver));
            return;
        }
        auto st     = state;
        st->deliver = [this](bool resolved, val value) {
            callback.reset();
            if (resolved) {
                ex::set_value(std::move(receiver), std::move(value));
            } else {
                ex::set_error(std::move(receiver), std::move(value));
            }
        };
        callback.emplace(std::move(token), on_stop{this});
        js().call<void>("attach",
                        promise,
                        val::module_property("beman_promise_settle"),
                        reinterpret_cast<std::uintptr_t>(new detail::settle_ptr(std::move(st))));
    }
};

struct from_promise_sender {
    using sender_concept = ex::sender_tag;
    using completion_signatures =
        ex::completion_signatures<ex::set_value_t(val), ex::set_error_t(val), ex::set_stopped_t()>;
    template <typename...>
    static consteval auto get_completion_signatures() noexcept -> completion_signatures {
        return {};
    }

    val promise;

    template <ex::receiver Receiver>
    auto connect(Receiver&& receiver) && -> promise_op_state<std::remove_cvref_t<Receiver>> {
        return {std::forward<Receiver>(receiver), std::move(promise)};
    }
};
static_assert(ex::sender<from_promise_sender>);

inline auto from_promise(val promise) -> from_promise_sender { return {std::move(promise)}; }

// ----------------------------------------------------------------------------
// to_promise

namespace detail {
struct deferred {
    val promise;
    val resolve;
    val reject;

    deferred() {
        val d   = js().call<val>("deferred");
        promise = d["promise"];
        resolve = d["resolve"];
        reject  = d["reject"];
    }
};

template <typename... Args>
auto to_js(Args&&... args) -> val {
    if constexpr (sizeof...(Args) == 0) {
        return val::undefined();
    } else if constexpr (sizeof...(Args) == 1) {
        return val(std::forward<Args>(args)...);
    } else {
        val array = val::array();
        (array.call<void>("push", val(std::forward<Args>(args))), ...);
        return array;
    }
}

// Connects an AbortSignal with a stop source; removes the listener at the end.
struct abort_link {
    std::shared_ptr<abort_state> state{std::make_shared<abort_state>()};
    std::uint32_t                id{0};
    val                          remove{val::undefined()};

    explicit abort_link(const val& signal) {
        if (signal.isUndefined() || signal.isNull()) {
            return;
        }
        if (signal["aborted"].as<bool>()) {
            state->source.request_stop();
            return;
        }
        static std::uint32_t next = 0;
        id                        = ++next;
        abort_registry().emplace(id, state);
        remove = js().call<val>("listen", signal, val::module_property("beman_promise_abort"), id);
    }
    abort_link(abort_link&&) = delete;
    ~abort_link() {
        if (id != 0) {
            abort_registry().erase(id);
            remove();
        }
    }
};

// The operation state deletes itself once the operation completed.
template <typename Sender>
struct promise_holder {
    struct receiver {
        using receiver_concept = ex::receiver_tag;
        promise_holder*         holder;
        ex::inplace_stop_token token;

        // everything runs on the JavaScript thread: tasks need a scheduler
        auto get_env() const noexcept {
            return ex::env{ex::prop{ex::get_stop_token, token}, ex::prop{ex::get_scheduler, ex::inline_scheduler{}}};
        }

        template <typename... Args>
        auto set_value(Args&&... args) && noexcept -> void {
            std::unique_ptr<promise_holder> keep(holder); // destroyed last: args may refer to the state
            holder->d->resolve(to_js(std::forward<Args>(args)...));
        }
        template <typename Error>
        auto set_error(Error&& error) && noexcept -> void {
            std::unique_ptr<promise_holder> keep(holder);
            if constexpr (std::same_as<val, std::remove_cvref_t<Error>>) {
                holder->d->reject(error);
#ifdef __cpp_exceptions
            } else if constexpr (std::same_as<std::exception_ptr, std::remove_cvref_t<Error>>) {
                try {
                    std::rethrow_exception(error);
                } catch (const std::exception& e) {
                    holder->d->reject(make_error("Error", e.what()));
                } catch (...) {
                    holder->d->reject(make_error("Error", "unknown C++ exception"));
                }
#endif
            } else {
                holder->d->reject(make_error("Error", "error completion"));
            }
        }
        auto set_stopped() && noexcept -> void {
            std::unique_ptr<promise_holder> keep(holder);
            holder->d->reject(make_error("AbortError", "operation stopped"));
        }
    };
    static_assert(ex::receiver<receiver>);
    using op_t = decltype(ex::connect(std::declval<Sender>(), std::declval<receiver>()));

    std::shared_ptr<deferred> d{std::make_shared<deferred>()};
    abort_link                link;
    op_t                      op;

    promise_holder(Sender&& sender, const val& signal)
        : link(signal),
          op(ex::connect(std::forward<Sender>(sender), receiver{this, link.state->source.get_token()})) {}
};
} // namespace detail

template <ex::sender Sender>
auto to_promise(Sender&& sender, const val& signal = val::undefined()) -> val {
    auto* holder  = new detail::promise_holder<Sender>(std::forward<Sender>(sender), signal);
    auto  promise = holder->d->promise;
    ex::start(holder->op);
    return promise;
}

} // namespace wasm

#endif
