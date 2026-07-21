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
#include <beman/execution/detail/stoppable_token.hpp>
#include <beman/execution/detail/task_scheduler.hpp>
#include <beman/execution/detail/with_error.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail::task {
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

    static std::size_t offset(std::size_t size) noexcept {
        return (size + alignof(Allocator) - 1u) & ~(alignof(Allocator) - 1u);
    }

    static Allocator* get_allocator(void* ptr, std::size_t size) noexcept {
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

template <typename Coroutine, typename Value, typename Environment>
class promise_type
    : public ::beman::execution::detail::task::
          promise_base<::beman::execution::detail::task::stoppable::yes, ::std::remove_cvref_t<Value>, Environment>,
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
        ::beman::execution::completion_signatures<::beman::execution::detail::task::completion_t<Value>>,
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
