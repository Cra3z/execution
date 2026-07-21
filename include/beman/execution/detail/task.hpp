// include/beman/execution/detail/task.hpp                        -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_TASK
#define INCLUDED_BEMAN_EXECUTION_DETAIL_TASK

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <coroutine>
#include <exception>
#include <memory>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.affine;
import beman.execution.detail.as_awaitable;
import beman.execution.detail.completion_signatures;
import beman.execution.detail.decayed_same_as;
import beman.execution.detail.env;
import beman.execution.detail.find_allocator;
import beman.execution.detail.forwarding_query;
import beman.execution.detail.get_allocator;
import beman.execution.detail.get_env;
import beman.execution.detail.get_scheduler;
import beman.execution.detail.get_start_scheduler;
import beman.execution.detail.get_stop_token;
import beman.execution.detail.inplace_stop_source;
import beman.execution.detail.meta.combine;
import beman.execution.detail.scheduler;
import beman.execution.detail.sender;
import beman.execution.detail.set_error;
import beman.execution.detail.set_stopped;
import beman.execution.detail.set_value;
import beman.execution.detail.stoppable_token;
import beman.execution.detail.task_scheduler;
import beman.execution.detail.with_error;
#else
#include <beman/execution/detail/affine.hpp>
#include <beman/execution/detail/as_awaitable.hpp>
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/decayed_same_as.hpp>
#include <beman/execution/detail/env.hpp>
#include <beman/execution/detail/find_allocator.hpp>
#include <beman/execution/detail/forwarding_query.hpp>
#include <beman/execution/detail/get_allocator.hpp>
#include <beman/execution/detail/get_env.hpp>
#include <beman/execution/detail/get_scheduler.hpp>
#include <beman/execution/detail/get_start_scheduler.hpp>
#include <beman/execution/detail/get_stop_token.hpp>
#include <beman/execution/detail/inplace_stop_source.hpp>
#include <beman/execution/detail/meta_combine.hpp>
#include <beman/execution/detail/scheduler.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_stopped.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/state_rep.hpp>
#include <beman/execution/detail/stoppable_token.hpp>
#include <beman/execution/detail/sub_visit.hpp>
#include <beman/execution/detail/task_scheduler.hpp>
#include <beman/execution/detail/with_error.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail::task {
template <typename Coroutine, typename Value, typename Environment>
class promise_type;

template <typename P>
class handle {
  private:
    struct deleter {
        auto operator()(P* p) noexcept -> void {
            if (p) {
                ::std::coroutine_handle<P>::from_promise(*p).destroy();
            }
        }
    };
    ::std::unique_ptr<P, deleter> h;

  public:
    explicit handle(P* p) : h(p) {}
    auto reset() -> void { this->h.reset(); }
    template <typename... A>
    auto start(A&&... a) noexcept -> auto {
        return this->h->start(::std::forward<A>(a)...);
    }
    auto release() -> ::std::coroutine_handle<P> {
        return ::std::coroutine_handle<P>::from_promise(*this->h.release());
    }
    P*   get() const noexcept { return this->h.get(); }
    auto get_env() const noexcept { return ::beman::execution::get_env(*this->h); }
};

template <typename Allocator>
struct allocator_support {
    using allocator_traits = std::allocator_traits<Allocator>;

    static auto offset(std::size_t size) noexcept -> ::std::size_t {
        return (size + alignof(Allocator) - 1u) & ~(alignof(Allocator) - 1u);
    }

    static auto get_allocator(void* ptr, ::std::size_t size) noexcept -> Allocator* {
        ptr = static_cast<std::byte*>(ptr) + offset(size);
        return ::std::launder(reinterpret_cast<Allocator*>(ptr));
    }

    template <typename... A>
    static void* operator new(std::size_t size, [[maybe_unused]] A&&... a) {
        if constexpr (::std::same_as<Allocator, ::std::allocator<::std::byte>>) {
            Allocator alloc{};
            return allocator_traits::allocate(alloc, size);
        } else {
            Allocator alloc{::beman::execution::detail::find_allocator<Allocator>(a...)};
            void*     ptr{allocator_traits::allocate(alloc, allocator_support::offset(size) + sizeof(Allocator))};
            try {
                new (allocator_support::get_allocator(ptr, size)) Allocator(alloc);
            } catch (...) {
                allocator_traits::deallocate(
                    alloc, static_cast<std::byte*>(ptr), allocator_support::offset(size) + sizeof(Allocator));
                throw;
            }
            return ptr;
        }
    }
    template <typename... A>
    static void operator delete(void* ptr, std::size_t size, const A&...) {
        allocator_support::operator delete(ptr, size);
    }
    static void operator delete(void* ptr, std::size_t size) {
        if constexpr (::std::same_as<Allocator, ::std::allocator<::std::byte>>) {
            Allocator alloc{};
            allocator_traits::deallocate(alloc, static_cast<std::byte*>(ptr), size);
        } else {
            Allocator* aptr{allocator_support::get_allocator(ptr, size)};
            Allocator  alloc{*aptr};
            aptr->~Allocator();
            allocator_traits::deallocate(
                alloc, static_cast<std::byte*>(ptr), allocator_support::offset(size) + sizeof(Allocator));
        }
    }
};

template <typename>
struct allocator_of {
    using type = ::std::allocator<std::byte>;
};
template <typename Context>
    requires requires { typename Context::allocator_type; }
struct allocator_of<Context> {
    using type = typename Context::allocator_type;
    static_assert(
        requires(type& a, ::std::size_t s, ::std::byte* ptr) {
            { a.allocate(s) } -> ::std::same_as<::std::byte*>;
            a.deallocate(ptr, s);
        }, "The allocator_type needs to be an allocator of std::byte");
};
template <typename Context>
using allocator_of_t = typename allocator_of<Context>::type;

template <typename R>
struct completion {
    using type = ::beman::execution::set_value_t(R);
};
template <>
struct completion<void> {
    using type = ::beman::execution::set_value_t();
};

template <typename R>
using completion_t = typename completion<R>::type;

template <typename>
struct error_types_of {
    using type = ::beman::execution::completion_signatures<::beman::execution::set_error_t(::std::exception_ptr)>;
};
template <typename Context>
    requires requires { typename Context::error_types; }
struct error_types_of<Context> {
    using type = typename Context::error_types;
};
template <typename Context>
using error_types_of_t = typename error_types_of<Context>::type;

template <typename>
struct start_scheduler_of {
    using type = ::beman::execution::task_scheduler;
};
template <typename Context>
    requires requires { typename Context::start_scheduler_type; }
struct start_scheduler_of<Context> {
    using type = typename Context::start_scheduler_type;
    static_assert(::beman::execution::scheduler<type>,
                  "The type alias start_scheduler_type needs to refer to a scheduler");
};
template <typename Context>
using start_scheduler_of_t = typename start_scheduler_of<Context>::type;

template <typename>
struct stop_source_of {
    using type = ::beman::execution::inplace_stop_source;
};
template <typename Context>
    requires requires { typename Context::stop_source_type; }
struct stop_source_of<Context> {
    using type = typename Context::stop_source_type;
};
template <typename Context>
using stop_source_of_t = typename stop_source_of<Context>::type;

struct void_type {};

template <typename Value, typename Errors>
class result_type;

template <typename Value, typename... Error>
class result_type<Value, ::beman::execution::completion_signatures<::beman::execution::set_error_t(Error)...>> {
  private:
    using value_type = ::std::conditional_t<::std::same_as<void, Value>, void_type, Value>;

    ::std::variant<std::monostate, value_type, Error...> result;

    template <size_t I, typename E, typename Err, typename... Errs>
    static constexpr ::std::size_t find_index() {
        if constexpr (std::same_as<E, Err>)
            return I;
        else {
            static_assert(0u != sizeof...(Errs), "error type not found in result type");
            return find_index<I + 1u, E, Errs...>();
        }
    }

  public:
    template <typename T>
    auto set_value(T&& value) -> void {
        this->result.template emplace<1u>(::std::forward<T>(value));
    }

    template <typename E>
    auto set_error(E&& error) -> void {
        this->result.template emplace<2u + find_index<0u, ::std::remove_cvref_t<E>, Error...>()>(
            ::std::forward<E>(error));
    }

    auto no_completion_set() const noexcept -> bool { return this->result.index() == 0u; }

    template <::beman::execution::receiver Receiver>
    auto result_complete(Receiver&& rcvr) -> void {
        switch (this->result.index()) {
        case 0:
            ::beman::execution::set_stopped(::std::move(rcvr));
            break;
        case 1:
            if constexpr (::std::same_as<void_type, value_type>)
                ::beman::execution::set_value(::std::move(rcvr));
            else
                ::beman::execution::set_value(::std::move(rcvr), ::std::move(::std::get<1u>(this->result)));
            break;
        default:
            if constexpr (0u < sizeof...(Error))
                ::beman::execution::detail::sub_visit<2u>(
                    [&rcvr](auto& error) { ::beman::execution::set_error(::std::move(rcvr), ::std::move(error)); },
                    this->result);
            break;
        }
    }
    auto result_resume() {
        switch (this->result.index()) {
        case 0:
            std::terminate(); // should never come here!
            break;
        case 1:
            break;
        default:
            if constexpr (0u < sizeof...(Error))
                ::beman::execution::detail::sub_visit<2u>(
                    []<typename E>(E& error) {
                        if constexpr (::std::same_as<::std::remove_cvref_t<E>, ::std::exception_ptr>)
                            std::rethrow_exception(::std::move(error));
                        else
                            throw ::std::move(error);
                    },
                    this->result);
            std::terminate(); // should never come here!
            break;
        }
        if constexpr (::std::same_as<void_type, value_type>)
            return;
        else
            return ::std::move(::std::get<1u>(this->result));
    }
};
template <typename Value>
class result_type<Value, ::beman::execution::completion_signatures<>> {
  private:
    using value_type = ::std::conditional_t<::std::same_as<void, Value>, void_type, Value>;

    ::std::variant<std::monostate, value_type> result;

    template <size_t I, typename E, typename Err, typename... Errs>
    static constexpr auto find_index() -> ::std::size_t {
        if constexpr (std::same_as<E, Err>)
            return I;
        else {
            static_assert(0u != sizeof...(Errs), "error type not found in result type");
            return find_index<I + 1u, E, Errs...>();
        }
    }

  public:
    template <typename T>
    auto set_value(T&& value) -> void {
        this->result.template emplace<1u>(::std::forward<T>(value));
    }
    auto no_completion_set() const noexcept -> bool { return this->result.index() == 0u; }

    template <::beman::execution::receiver Receiver>
    auto result_complete(Receiver&& rcvr) -> void {
        switch (this->result.index()) {
        case 0:
            ::beman::execution::set_stopped(::std::move(rcvr));
            break;
        case 1:
            if constexpr (::std::same_as<void_type, value_type>)
                ::beman::execution::set_value(::std::move(rcvr));
            else
                ::beman::execution::set_value(::std::move(rcvr), ::std::move(::std::get<1u>(this->result)));
            break;
        default:
            std::terminate(); // should never come here!
            break;
        }
    }
    auto result_resume() {
        switch (this->result.index()) {
        case 0:
            std::terminate(); // should never come here!
            break;
        case 1:
            break;
        default:
            std::terminate(); // should never come here!
            break;
        }
        if constexpr (::std::same_as<void_type, value_type>)
            return;
        else
            return ::std::move(::std::get<1u>(this->result));
    }
};

template <typename Value, typename Environment>
class state_base : public result_type<Value, error_types_of_t<Environment>> {
  public:
    using allocator_type   = allocator_of_t<Environment>;
    using stop_source_type = stop_source_of_t<Environment>;
    using stop_token_type  = decltype(::std::declval<stop_source_type>().get_token());
    using scheduler_type   = start_scheduler_of_t<Environment>;

    auto complete() -> std::coroutine_handle<> { return this->do_complete(); }
    auto get_allocator() -> allocator_type { return this->do_get_allocator(); }
    auto get_stop_token() -> stop_token_type { return this->do_get_stop_token(); }
    auto get_environment() -> Environment& {
        assert(this);
        return this->do_get_environment();
    }
    auto get_start_scheduler() -> scheduler_type { return this->do_get_start_scheduler(); }
    auto set_start_scheduler(scheduler_type other) -> scheduler_type { return this->do_set_start_scheduler(other); }

  protected:
    template <::beman::execution::scheduler Scheduler, typename Env>
    static auto from_env(const Env& env) {
        if constexpr (requires { Scheduler(::beman::execution::get_start_scheduler(env)); }) {
            return Scheduler(::beman::execution::get_start_scheduler(env));
        } else if constexpr (requires { Scheduler(::beman::execution::get_scheduler(env)); }) {
            return Scheduler(::beman::execution::get_scheduler(env));
        } else {
            return Scheduler();
        }
    }

    // NOLINTBEGIN(portability-template-virtual-member-function)
    virtual auto do_complete() -> std::coroutine_handle<>                       = 0;
    virtual auto do_get_allocator() -> allocator_type                           = 0;
    virtual auto do_get_stop_token() -> stop_token_type                         = 0;
    virtual auto do_get_environment() -> Environment&                           = 0;
    virtual auto do_get_start_scheduler() -> scheduler_type                     = 0;
    virtual auto do_set_start_scheduler(scheduler_type other) -> scheduler_type = 0;
    // NOLINTEND(portability-template-virtual-member-function)

    virtual ~state_base() = default;
};

template <typename Task, typename T, typename C, typename Receiver>
struct state : state_base<T, C>, ::beman::execution::detail::state_rep<C, Receiver> {
    using operation_state_concept = ::beman::execution::operation_state_tag;
    using promise_type            = promise_type<Task, T, C>;
    using scheduler_type          = typename state_base<T, C>::scheduler_type;
    using allocator_type          = typename state_base<T, C>::allocator_type;
    using stop_source_type        = typename state_base<T, C>::stop_source_type;
    using stop_token_type         = typename state_base<T, C>::stop_token_type;
    using stop_token_t =
        decltype(::beman::execution::get_stop_token(::beman::execution::get_env(std::declval<Receiver>())));
    struct stop_link {
        stop_source_type& source;
        void              operator()() const noexcept { source.request_stop(); }
    };
    using stop_callback_t = ::beman::execution::stop_callback_for_t<stop_token_t, stop_link>;
    template <typename R, typename H>
    state(R&& r, H h) noexcept //-dk:TODO break down to various members
        : state_rep<C, Receiver>(std::forward<R>(r)),
          handle(std::move(h)),
          scheduler(this->template from_env<scheduler_type>(::beman::execution::get_env(this->receiver))) {}

    handle<promise_type>           handle;
    stop_source_type               source;
    std::optional<stop_callback_t> stop_callback;
    scheduler_type                 scheduler;

    auto                    start() & noexcept -> void { this->handle.start(this).resume(); }
    std::coroutine_handle<> do_complete() override {
        this->handle.reset();
        this->result_complete(::std::move(this->receiver));
        return std::noop_coroutine();
    }
    auto do_get_allocator() -> allocator_type override {
        if constexpr (requires {
                          allocator_type(
                              ::beman::execution::get_allocator(::beman::execution::get_env(this->receiver)));
                      })
            return allocator_type(::beman::execution::get_allocator(::beman::execution::get_env(this->receiver)));
        else
            return allocator_type{};
    }
    auto do_get_start_scheduler() -> scheduler_type override { return this->scheduler; }
    auto do_set_start_scheduler(scheduler_type other) -> scheduler_type override {
        return ::std::exchange(this->scheduler, other);
    }
    auto do_get_stop_token() -> stop_token_type override {
        if (this->source.stop_possible() && not this->stop_callback) {
            this->stop_callback.emplace(
                ::beman::execution::get_stop_token(::beman::execution::get_env(this->receiver)),
                stop_link{this->source});
        }
        return this->source.get_token();
    }
    auto do_get_environment() -> C& override { return this->context; }
};

template <typename Awaiter>
struct awaiter_scheduler_receiver {
    using receiver_concept = ::beman::execution::receiver_tag;
    Awaiter* aw;
    auto     set_value(auto&&...) noexcept { this->aw->actual_complete().resume(); }
    auto     set_error(auto&&) noexcept { this->aw->actual_complete().resume(); }
    auto     set_stopped() noexcept { this->aw->actual_complete().resume(); }
};

template <typename Awaiter,
          typename ParentPromise,
          bool = requires(
              const ParentPromise& p) { ::beman::execution::get_start_scheduler(::beman::execution::get_env(p)); }>
struct awaiter_op_t {
    using state_type =
        decltype(::beman::execution::connect(::beman::execution::schedule(::beman::execution::get_start_scheduler(
                                                 ::beman::execution::get_env(::std::declval<const ParentPromise&>()))),
                                             ::std::declval<awaiter_scheduler_receiver<Awaiter>>()));

    awaiter_op_t(const ParentPromise& p, Awaiter* aw)
        : state(::beman::execution::connect(
              ::beman::execution::schedule(beman::execution::get_start_scheduler(::beman::execution::get_env(p))),
              awaiter_scheduler_receiver<Awaiter>{aw})) {}
    state_type state;
    auto       start() noexcept -> void { ::beman::execution::start(this->state); }
};
template <typename Awaiter, typename ParentPromise>
struct awaiter_op_t<Awaiter, ParentPromise, false> {
    awaiter_op_t(const ParentPromise&, Awaiter*) noexcept {}
    auto start() noexcept -> void {}
};

template <typename Value, typename Env, typename OwnPromise, typename ParentPromise>
class awaiter : public state_base<Value, Env> {
  public:
    using allocator_type  = typename state_base<Value, Env>::allocator_type;
    using stop_token_type = typename state_base<Value, Env>::stop_token_type;
    using scheduler_type  = typename state_base<Value, Env>::scheduler_type;

    explicit awaiter(handle<OwnPromise> h) : handle(std::move(h)) {}
    constexpr auto await_ready() const noexcept -> bool { return false; }
    struct env_receiver {
        ParentPromise* parent;
        auto           get_env() const noexcept { return parent->get_env(); }
    };
    auto await_suspend(::std::coroutine_handle<ParentPromise> parent) noexcept {
        this->state_rep.emplace(env_receiver{&parent.promise()});
        this->scheduler.emplace(
            this->template from_env<scheduler_type>(::beman::execution::get_env(parent.promise())));
        this->parent = ::std::move(parent);
        return this->handle.start(this);
    }
    auto await_resume() { return this->result_resume(); }

  private:
    friend struct awaiter_scheduler_receiver<awaiter>;
    auto do_complete() -> std::coroutine_handle<> override {
        assert(this->parent);
        assert(this->scheduler);
        if constexpr (requires {
                          *this->scheduler != ::beman::execution::get_start_scheduler(
                                                  ::beman::execution::get_env(this->parent.promise()));
                      }) {
            if (*this->scheduler !=
                ::beman::execution::get_start_scheduler(::beman::execution::get_env(this->parent.promise()))) {
                this->reschedule.emplace(this->parent.promise(), this);
                this->reschedule->start();
                return ::std::noop_coroutine();
            }
        }
        return this->actual_complete();
    }
    auto actual_complete() -> std::coroutine_handle<> {
        return this->no_completion_set() ? this->parent.promise().unhandled_stopped() : ::std::move(this->parent);
    }
    auto do_get_allocator() -> allocator_type override {
        if constexpr (requires {
                          ::beman::execution::get_allocator(::beman::execution::get_env(this->parent.promise()));
                      })
            return ::beman::execution::get_allocator(::beman::execution::get_env(this->parent.promise()));
        else
            return allocator_type{};
    }
    auto do_get_start_scheduler() -> scheduler_type override { return *this->scheduler; }
    auto do_set_start_scheduler(scheduler_type other) -> scheduler_type override {
        return ::std::exchange(*this->scheduler, other);
    }
    auto do_get_stop_token() -> stop_token_type override { return {}; }
    auto do_get_environment() -> Env& override { return this->state_rep->context; }

    handle<OwnPromise>                                                        handle;
    ::std::optional<::beman::execution::detail::state_rep<Env, env_receiver>> state_rep;
    ::std::optional<scheduler_type>                                           scheduler;
    ::std::coroutine_handle<ParentPromise>                                    parent{};
    ::std::optional<awaiter_op_t<awaiter, ParentPromise>>                     reschedule{};
};

template <typename Promise>
struct promise_env {
    const Promise* promise;

    auto query(const ::beman::execution::get_scheduler_t&) const noexcept -> typename Promise::scheduler_type {
        return this->promise->get_start_scheduler();
    }

    auto query(const ::beman::execution::get_start_scheduler_t&) const noexcept -> typename Promise::scheduler_type {
        return this->promise->get_start_scheduler();
    }

    auto query(const ::beman::execution::get_allocator_t&) const noexcept -> typename Promise::allocator_type {
        return this->promise->get_allocator();
    }

    auto query(const ::beman::execution::get_stop_token_t&) const noexcept -> typename Promise::stop_token_type {
        return this->promise->get_stop_token();
    }

    template <typename Q, typename... A>
        requires requires(const Promise* p, Q q, A&&... a) {
            ::beman::execution::forwarding_query(q);
            q(p->get_environment(), ::std::forward<A>(a)...);
        }
    auto query(Q q, A&&... a) const noexcept {
        return q(promise->get_environment(), ::std::forward<A>(a)...);
    }
};

struct final_awaiter {
    static constexpr auto await_ready() noexcept -> bool { return false; }

    template <typename Promise>
    static auto await_suspend(std::coroutine_handle<Promise> handle) noexcept {
        return handle.promise().notify_complete();
    }

    static constexpr void await_resume() noexcept {}
};

template <typename Value, typename Environment>
class promise_base {
  public:
    template <typename T = Value>
    void return_value(T&& value) {
        this->get_state()->set_value(::std::forward<T>(value));
    }

  public:
    auto set_state(state_base<Value, Environment>* s) noexcept -> void { this->state_ = s; }
    auto get_state() const noexcept -> state_base<Value, Environment>* { return this->state_; }

  private:
    state_base<Value, Environment>* state_{};
};

template <typename Environment>
class promise_base<void, Environment> {
  public:
    void return_void() { this->get_state()->set_value(void_type{}); }

  public:
    auto set_state(state_base<void, Environment>* s) noexcept -> void { this->state_ = s; }

    auto get_state() const noexcept -> state_base<void, Environment>* { return this->state_; }

  private:
    state_base<void, Environment>* state_{};
};

template <typename Coroutine, typename Value, typename Environment>
class promise_type : public ::beman::execution::detail::task::promise_base<::std::remove_cvref_t<Value>, Environment>,
                     public ::beman::execution::detail::task::allocator_support<
                         ::beman::execution::detail::task::allocator_of_t<Environment>> {
  public:
    using allocator_type   = allocator_of_t<Environment>;
    using scheduler_type   = start_scheduler_of_t<Environment>;
    using stop_source_type = stop_source_of_t<Environment>;
    using stop_token_type  = decltype(::std::declval<stop_source_type>().get_token());

    static auto initial_suspend() noexcept -> ::std::suspend_always { return {}; }

    static auto final_suspend() noexcept -> ::beman::execution::detail::task::final_awaiter { return {}; }

    auto unhandled_exception() noexcept -> void {
        using error_types = error_types_of_t<Environment>;
        if constexpr (::beman::execution::detail::meta::
                          contains<error_types, ::beman::execution::set_error_t(::std::exception_ptr)>) {
            this->get_state()->set_error(::std::current_exception());
        } else {
            std::terminate();
        }
    }
    std::coroutine_handle<> unhandled_stopped() { return this->get_state()->complete(); }

    auto get_return_object() noexcept {
        return Coroutine(::beman::execution::detail::task::handle<promise_type>(this));
    }

    template <::beman::execution::sender Expr>
    auto await_transform(Expr&& expr) {
        return ::beman::execution::as_awaitable(::beman::execution::affine(::std::forward<Expr>(expr)), *this);
    }

    template <typename E>
    auto yield_value(with_error<E> with) noexcept -> ::beman::execution::detail::task::final_awaiter {
        this->get_state()->set_error(::std::move(with.error));
        return {};
    }

    auto get_env() const noexcept -> ::beman::execution::detail::task::promise_env<promise_type> { return {this}; }

    auto start(::beman::execution::detail::task::state_base<Value, Environment>* state) -> ::std::coroutine_handle<> {
        this->set_state(state);
        return ::std::coroutine_handle<promise_type>::from_promise(*this);
    }

    auto notify_complete() -> ::std::coroutine_handle<> { return this->get_state()->complete(); }

    auto get_start_scheduler() const noexcept -> scheduler_type { return this->get_state()->get_start_scheduler(); }

    auto get_allocator() const noexcept -> allocator_type { return this->get_state()->get_allocator(); }

    auto get_stop_token() const noexcept -> stop_token_type { return this->get_state()->get_stop_token(); }

    auto get_environment() const noexcept -> const Environment& { return this->get_state()->get_environment(); }

  private:
    using env_t = ::beman::execution::detail::task::promise_env<promise_type>;

    ::std::optional<scheduler_type> scheduler{};
};

} // namespace beman::execution::detail::task

namespace beman::execution {
template <typename Value = void, typename Env = ::beman::execution::env<>>
class task {
    friend ::beman::execution::detail::task::promise_type<task, Value, Env>;

  private:
    template <typename Receiver>
    using state = ::beman::execution::detail::task::state<task, Value, Env, Receiver>;

  public:
    using sender_concept        = ::beman::execution::sender_tag;
    using promise_type          = ::beman::execution::detail::task::promise_type<task, Value, Env>;
    using allocator_type        = ::beman::execution::detail::task::allocator_of_t<Env>;
    using start_scheduler_type  = ::beman::execution::detail::task::start_scheduler_of_t<Env>;
    using stop_source_type      = ::beman::execution::detail::task::stop_source_of_t<Env>;
    using stop_token_type       = decltype(::std::declval<stop_source_type>().get_token());
    using completion_signatures = ::beman::execution::detail::meta::combine<
        ::beman::execution::completion_signatures<::beman::execution::detail::task::completion_t<Value>,
                                                  ::beman::execution::set_stopped_t()>,
        ::beman::execution::detail::task::error_types_of_t<Env>>;

    task(const task&) = delete;

    task(task&&) noexcept = default;

    task& operator=(const task&) = delete;

    task& operator=(task&&) noexcept = default;

    ~task() = default;

    template <::beman::execution::detail::decayed_same_as<task>, typename...>
    static consteval auto get_completion_signatures() noexcept -> completion_signatures {
        return {};
    }

    template <typename Receiver>
    auto connect(Receiver&& receiver) && noexcept(
        noexcept(state<std::remove_cvref_t<Receiver>>(std::forward<Receiver>(receiver), std::move(this->handle))))
        -> state<std::remove_cvref_t<Receiver>> {
        return state<std::remove_cvref_t<Receiver>>(std::forward<Receiver>(receiver), std::move(this->handle));
    }

    template <typename ParentPromise>
    auto as_awaitable(
        ParentPromise&) && -> ::beman::execution::detail::task::awaiter<Value, Env, promise_type, ParentPromise> {
        return ::beman::execution::detail::task::awaiter<Value, Env, promise_type, ParentPromise>(
            ::std::move(this->handle));
    }

  private:
    explicit task(::beman::execution::detail::task::handle<promise_type> h) noexcept : handle(std::move(h)) {}

    ::beman::execution::detail::task::handle<promise_type> handle;
};
} // namespace beman::execution

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_TASK
