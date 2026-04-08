// include/beman/execution/detail/bulk.hpp                         -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_BULK
#define INCLUDED_BEMAN_EXECUTION_DETAIL_BULK

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <algorithm>
#include <concepts>
#include <exception>
#include <execution>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.basic_sender;
import beman.execution.detail.completion_signatures;
import beman.execution.detail.completion_signatures_for;
import beman.execution.detail.default_impls;
import beman.execution.detail.forward_like;
import beman.execution.detail.get_completion_signatures;
import beman.execution.detail.get_domain_early;
import beman.execution.detail.impls_for;
import beman.execution.detail.make_sender;
import beman.execution.detail.meta.combine;
import beman.execution.detail.meta.unique;
import beman.execution.detail.movable_value;
import beman.execution.detail.product_type;
import beman.execution.detail.sender;
import beman.execution.detail.sender_adaptor_closure;
import beman.execution.detail.sender_for;
import beman.execution.detail.set_error;
import beman.execution.detail.set_value;
import beman.execution.detail.transform_sender;
#else
#include <beman/execution/detail/basic_sender.hpp>
#include <beman/execution/detail/completion_signatures.hpp>
#include <beman/execution/detail/completion_signatures_for.hpp>
#include <beman/execution/detail/default_impls.hpp>
#include <beman/execution/detail/forward_like.hpp>
#include <beman/execution/detail/get_completion_signatures.hpp>
#include <beman/execution/detail/get_domain_early.hpp>
#include <beman/execution/detail/impls_for.hpp>
#include <beman/execution/detail/make_sender.hpp>
#include <beman/execution/detail/meta_combine.hpp>
#include <beman/execution/detail/meta_unique.hpp>
#include <beman/execution/detail/movable_value.hpp>
#include <beman/execution/detail/product_type.hpp>
#include <beman/execution/detail/sender.hpp>
#include <beman/execution/detail/sender_adaptor.hpp>
#include <beman/execution/detail/sender_adaptor_closure.hpp>
#include <beman/execution/detail/sender_for.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/transform_sender.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {
template <bool IsChunked, typename F, typename Shape, typename Completions>
struct bulk_completions_helper;

template <bool IsChunked, typename F, typename Shape, typename... Sigs>
struct bulk_completions_helper<IsChunked, F, Shape, completion_signatures<Sigs...>> {
    template <typename Sig>
    struct sig_may_throw : ::std::false_type {};

    template <typename... XArgs>
    struct sig_may_throw<set_value_t(XArgs...)> {
        static constexpr bool value = [] {
            if constexpr (IsChunked)
                return !::std::is_nothrow_invocable_v<F, Shape, Shape, XArgs...>;
            else
                return !::std::is_nothrow_invocable_v<F, Shape, XArgs...>;
        }();
    };

    static constexpr bool any_may_throw = (false || ... || sig_may_throw<Sigs>::value);

    using type = ::std::conditional_t<!any_may_throw,
                                      completion_signatures<Sigs...>,
                                      completion_signatures<Sigs..., set_error_t(::std::exception_ptr)>>;
};

struct bulk_chunked_t : ::beman::execution::sender_adaptor_closure<bulk_chunked_t> {

    template <typename Policy, typename Shape, typename F>
        requires(::std::is_execution_policy_v<::std::remove_cvref_t<Policy>> && ::std::is_integral_v<Shape> &&
                 ::std::copy_constructible<::std::decay_t<F>>)
    auto operator()(Policy&& policy, Shape shape, F&& f) const {
        return ::beman::execution::detail::make_sender_adaptor(
            *this, ::std::forward<Policy>(policy), shape, ::std::forward<F>(f));
    }

    template <typename Sender, typename Policy, typename Shape, typename F>
        requires(::beman::execution::sender<Sender> && ::std::is_execution_policy_v<::std::remove_cvref_t<Policy>> &&
                 ::std::is_integral_v<Shape> && ::std::copy_constructible<::std::decay_t<F>>)
    auto operator()(Sender&& sndr, Policy&& policy, Shape shape, F&& f) const {
        auto domain{::beman::execution::detail::get_domain_early(sndr)};
        return ::beman::execution::transform_sender(
            domain,
            ::beman::execution::detail::make_sender(
                *this,
                ::beman::execution::detail::product_type<::std::remove_cvref_t<Policy>, Shape, ::std::decay_t<F>>{
                    ::std::forward<Policy>(policy), shape, ::std::forward<F>(f)},
                ::std::forward<Sender>(sndr)));
    }

  private:
    template <typename F, typename Shape, typename Completions>
    using chunked_completions = typename bulk_completions_helper<true, F, Shape, Completions>::type;

    template <typename, typename>
    struct get_signatures;
    template <typename Policy, typename Shape, typename F, typename Sender, typename Env>
    struct get_signatures<
        ::beman::execution::detail::
            basic_sender<bulk_chunked_t, ::beman::execution::detail::product_type<Policy, Shape, F>, Sender>,
        Env> {
        using completions = decltype(::beman::execution::get_completion_signatures<Sender, Env>());
        using type        = ::beman::execution::detail::meta::unique<
                   ::beman::execution::detail::meta::combine<chunked_completions<F, Shape, completions>>>;
    };

  public:
    template <typename Sender, typename... Env>
    static consteval auto get_completion_signatures() {
        return typename get_signatures<::std::remove_cvref_t<Sender>, Env...>::type{};
    }

    struct impls_for : ::beman::execution::detail::default_impls {
        struct complete_impl {
            template <typename Index,
                      typename Policy,
                      typename Shape,
                      typename Fun,
                      typename Rcvr,
                      typename Tag,
                      typename... Args>
                requires(!::std::same_as<Tag, set_value_t> || ::std::is_invocable_v<Fun, Shape, Shape, Args...>)
            auto operator()(Index,
                            ::beman::execution::detail::product_type<Policy, Shape, Fun>& state,
                            Rcvr&                                                         rcvr,
                            Tag,
                            Args&&... args) const noexcept -> void {
                if constexpr (::std::same_as<Tag, set_value_t>) {
                    auto& [policy, shape, f] = state;
                    using s_type             = ::std::remove_cvref_t<decltype(shape)>;
                    constexpr bool nothrow   = noexcept(f(s_type(shape), s_type(shape), args...));
                    try {
                        [&]() noexcept(nothrow) {
                            if (shape > s_type(0)) {
                                f(static_cast<s_type>(0), s_type(shape), args...);
                            }
                            Tag()(::std::move(rcvr), ::std::forward<Args>(args)...);
                        }();
                    } catch (...) {
                        if constexpr (not nothrow) {
                            ::beman::execution::set_error(::std::move(rcvr), ::std::current_exception());
                        }
                    }
                } else {
                    Tag()(::std::move(rcvr), ::std::forward<Args>(args)...);
                }
            }
        };
        static constexpr auto complete{complete_impl{}};
    };
};

struct bulk_unchunked_t : ::beman::execution::sender_adaptor_closure<bulk_unchunked_t> {

    template <typename Policy, typename Shape, typename F>
        requires(::std::is_execution_policy_v<::std::remove_cvref_t<Policy>> && ::std::is_integral_v<Shape> &&
                 ::std::copy_constructible<::std::decay_t<F>>)
    auto operator()(Policy&& policy, Shape shape, F&& f) const {
        return ::beman::execution::detail::make_sender_adaptor(
            *this, ::std::forward<Policy>(policy), shape, ::std::forward<F>(f));
    }

    template <typename Sender, typename Policy, typename Shape, typename F>
        requires(::beman::execution::sender<Sender> && ::std::is_execution_policy_v<::std::remove_cvref_t<Policy>> &&
                 ::std::is_integral_v<Shape> && ::std::copy_constructible<::std::decay_t<F>>)
    auto operator()(Sender&& sndr, Policy&& policy, Shape shape, F&& f) const {
        auto domain{::beman::execution::detail::get_domain_early(sndr)};
        return ::beman::execution::transform_sender(
            domain,
            ::beman::execution::detail::make_sender(
                *this,
                ::beman::execution::detail::product_type<::std::remove_cvref_t<Policy>, Shape, ::std::decay_t<F>>{
                    ::std::forward<Policy>(policy), shape, ::std::forward<F>(f)},
                ::std::forward<Sender>(sndr)));
    }

  private:
    template <typename F, typename Shape, typename Completions>
    using unchunked_completions = typename bulk_completions_helper<false, F, Shape, Completions>::type;

    template <typename, typename>
    struct get_signatures;
    template <typename Policy, typename Shape, typename F, typename Sender, typename Env>
    struct get_signatures<
        ::beman::execution::detail::
            basic_sender<bulk_unchunked_t, ::beman::execution::detail::product_type<Policy, Shape, F>, Sender>,
        Env> {
        using completions = decltype(::beman::execution::get_completion_signatures<Sender, Env>());
        using type        = ::beman::execution::detail::meta::unique<
                   ::beman::execution::detail::meta::combine<unchunked_completions<F, Shape, completions>>>;
    };

  public:
    template <typename Sender, typename... Env>
    static consteval auto get_completion_signatures() {
        return typename get_signatures<::std::remove_cvref_t<Sender>, Env...>::type{};
    }

    struct impls_for : ::beman::execution::detail::default_impls {
        struct complete_impl {
            template <typename Index,
                      typename Policy,
                      typename Shape,
                      typename Fun,
                      typename Rcvr,
                      typename Tag,
                      typename... Args>
                requires(!::std::same_as<Tag, set_value_t> || ::std::is_invocable_v<Fun, Shape, Args...>)
            auto operator()(Index,
                            ::beman::execution::detail::product_type<Policy, Shape, Fun>& state,
                            Rcvr&                                                         rcvr,
                            Tag,
                            Args&&... args) const noexcept -> void {
                if constexpr (::std::same_as<Tag, set_value_t>) {
                    auto& [policy, shape, f] = state;
                    using s_type             = ::std::remove_cvref_t<decltype(shape)>;
                    constexpr bool nothrow   = noexcept(f(s_type(shape), args...));
                    try {
                        [&]() noexcept(nothrow) {
                            for (decltype(s_type(shape)) i = 0; i < shape; i++) {
                                f(s_type(i), args...);
                            }
                            Tag()(::std::move(rcvr), ::std::forward<Args>(args)...);
                        }();
                    } catch (...) {
                        if constexpr (not nothrow) {
                            ::beman::execution::set_error(::std::move(rcvr), ::std::current_exception());
                        }
                    }
                } else {
                    Tag()(::std::move(rcvr), ::std::forward<Args>(args)...);
                }
            }
        };
        static constexpr auto complete{complete_impl{}};
    };
};

struct bulk_t : ::beman::execution::sender_adaptor_closure<bulk_t> {
    template <typename Policy, typename Shape, typename F>
        requires(::std::is_execution_policy_v<::std::remove_cvref_t<Policy>> && ::std::is_integral_v<Shape> &&
                 ::std::copy_constructible<::std::decay_t<F>>)
    auto operator()(Policy&& policy, Shape shape, F&& f) const {
        return ::beman::execution::detail::make_sender_adaptor(
            *this, ::std::forward<Policy>(policy), shape, ::std::forward<F>(f));
    }

    template <typename Sender, typename Policy, typename Shape, typename F>
        requires(::beman::execution::sender<Sender> && ::std::is_execution_policy_v<::std::remove_cvref_t<Policy>> &&
                 ::std::is_integral_v<Shape> && ::std::copy_constructible<::std::decay_t<F>>)
    auto operator()(Sender&& sndr, Policy&& policy, Shape shape, F&& f) const {
        return ::beman::execution::transform_sender(
            ::beman::execution::detail::get_domain_early(sndr),
            ::beman::execution::detail::make_sender(
                *this,
                ::beman::execution::detail::product_type<::std::remove_cvref_t<Policy>, Shape, ::std::decay_t<F>>{
                    ::std::forward<Policy>(policy), shape, ::std::forward<F>(f)},
                ::std::forward<Sender>(sndr)));
    }

    template <::beman::execution::detail::sender_for<bulk_t> Sender, typename... Env>
    auto transform_sender(Sender&& sndr, Env&&...) const {
        auto data  = ::beman::execution::detail::forward_like<Sender>(sndr.template get<1>());
        auto child = ::beman::execution::detail::forward_like<Sender>(sndr.template get<2>());

        auto& policy = data.template get<0>();
        auto& shape  = data.template get<1>();
        auto& f      = data.template get<2>();

        return bulk_chunked_t{}(::std::move(child),
                                policy,
                                shape,
                                this->wrap_chunked<::std::remove_cvref_t<decltype(shape)>>(::std::move(f)));
    }

  private:
    template <typename F, typename Shape, typename Completions>
    using bulk_completions = typename bulk_completions_helper<false, F, Shape, Completions>::type;

    template <typename, typename>
    struct get_signatures;
    template <typename Policy, typename Shape, typename F, typename Sender, typename Env>
    struct get_signatures<::beman::execution::detail::
                              basic_sender<bulk_t, ::beman::execution::detail::product_type<Policy, Shape, F>, Sender>,
                          Env> {
        using completions = decltype(::beman::execution::get_completion_signatures<Sender, Env>());
        using type        = ::beman::execution::detail::meta::unique<
                   ::beman::execution::detail::meta::combine<bulk_completions<F, Shape, completions>>>;
    };

    template <typename Shape>
    auto wrap_chunked(auto f) noexcept {
        return [f = std::move(f)](Shape begin, Shape end, auto&&... args) noexcept(noexcept(f(begin, args...))) {
            while (begin != end)
                f(begin++, args...);
        };
    }

  public:
    template <typename Sender, typename... Env>
    static consteval auto get_completion_signatures() {
        return typename get_signatures<::std::remove_cvref_t<Sender>, Env...>::type{};
    }
};

} // namespace beman::execution::detail

// ----------------------------------------------------------------------------

namespace beman::execution {

using bulk_t           = ::beman::execution::detail::bulk_t;
using bulk_chunked_t   = ::beman::execution::detail::bulk_chunked_t;
using bulk_unchunked_t = ::beman::execution::detail::bulk_unchunked_t;

inline constexpr ::beman::execution::bulk_t           bulk{};
inline constexpr ::beman::execution::bulk_chunked_t   bulk_chunked{};
inline constexpr ::beman::execution::bulk_unchunked_t bulk_unchunked{};

} // namespace beman::execution

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_BULK
