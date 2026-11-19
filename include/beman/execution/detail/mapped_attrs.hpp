// include/beman/execution/detail/mapped_attrs.hpp                   -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_MAPPED_ATTRS
#define INCLUDED_BEMAN_EXECUTION_DETAIL_MAPPED_ATTRS

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <cstddef>
#include <type_traits>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.common_domain;
import beman.execution.detail.completion_signatures_of_t;
import beman.execution.detail.env_of_t;
import beman.execution.detail.fwd_env;
import beman.execution.detail.get_completion_domain;
import beman.execution.detail.get_completion_scheduler;
import beman.execution.detail.get_env;
import beman.execution.detail.hide_completion_queries;
import beman.execution.detail.indeterminate_domain;
import beman.execution.detail.meta.filter;
import beman.execution.detail.sender_in;
import beman.execution.detail.set_error;
import beman.execution.detail.set_stopped;
import beman.execution.detail.set_value;
import beman.execution.detail.try_query;
import beman.execution.detail.type_list;
#else
#include <beman/execution/detail/common_domain.hpp>
#include <beman/execution/detail/completion_signatures_of_t.hpp>
#include <beman/execution/detail/env_of_t.hpp>
#include <beman/execution/detail/fwd_env.hpp>
#include <beman/execution/detail/get_completion_domain.hpp>
#include <beman/execution/detail/get_completion_scheduler.hpp>
#include <beman/execution/detail/get_env.hpp>
#include <beman/execution/detail/hide_completion_queries.hpp>
#include <beman/execution/detail/indeterminate_domain.hpp>
#include <beman/execution/detail/meta_filter.hpp>
#include <beman/execution/detail/sender_in.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_stopped.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/try_query.hpp>
#include <beman/execution/detail/type_list.hpp>
#endif

namespace beman::execution::detail {

struct no_exception_source {
    using source_tag = void;

    template <typename...>
    static consteval bool may_throw() {
        return false;
    }
};

template <typename Child, typename FromTag, typename ToTag, typename ExceptionSource>
struct mapped_attrs
    : ::beman::execution::detail::hide_completion_queries<::std::remove_cvref_t<::beman::execution::env_of_t<Child>>> {
  private:
    using value_t       = ::beman::execution::set_value_t;
    using error_t       = ::beman::execution::set_error_t;
    using stopped_t     = ::beman::execution::set_stopped_t;
    using child_attrs_t = ::std::remove_cvref_t<::beman::execution::env_of_t<Child>>;

    struct unknown_signatures {
        static constexpr ::std::size_t count_of(auto) noexcept { return 1uz; }
    };

    template <typename ResultTag, typename Signatures, typename ExceptionTag>
    struct matching_sources {
        template <typename Source>
        using contributes = ::std::bool_constant<
            Signatures::count_of(Source{}) != 0 &&
            (::std::same_as<ResultTag, ::std::conditional_t<::std::same_as<Source, FromTag>, ToTag, Source>> ||
             ::std::same_as<Source, ExceptionTag>)>;

        using type = ::beman::execution::detail::meta::
            filter<contributes, ::beman::execution::detail::type_list<value_t, error_t, stopped_t>>;
    };

    // A completion can have more than one source even if its output signatures are identical.
    template <typename Tag, typename... Env>
    static consteval auto completion_sources() {
        using error_source  = ::std::conditional_t<::std::same_as<Tag, error_t>, ExceptionSource, no_exception_source>;
        using exception_tag = typename error_source::source_tag;

        if constexpr (::beman::execution::sender_in<Child, Env...>) {
            using signatures   = ::beman::execution::completion_signatures_of_t<Child, Env...>;
            using throwing_tag = ::std::
                conditional_t<error_source::template may_throw<Child, signatures, Env...>(), exception_tag, void>;
            return typename matching_sources<Tag, signatures, throwing_tag>::type{};
        } else if constexpr (sizeof...(Env) == 0) {
            // Dependent senders can still provide attributes without a receiver environment.
            return typename matching_sources<Tag, unknown_signatures, exception_tag>::type{};
        } else {
            return ::beman::execution::detail::type_list<>{};
        }
    }

    template <typename Source, typename... Env>
    static constexpr bool has_source_domain = requires(const child_attrs_t& attrs, const Env&... env) {
        ::beman::execution::get_completion_domain<Source>(attrs, env...);
    };

    template <typename Source, typename... Env>
    static auto get_source_domain(const child_attrs_t& attrs, const Env&... env) noexcept {
        if constexpr (has_source_domain<Source, Env...>) {
            return ::beman::execution::get_completion_domain<Source>(attrs, env...);
        } else if constexpr (requires {
                                 ::beman::execution::get_completion_domain<value_t>(
                                     ::beman::execution::get_completion_scheduler<Source>(attrs, env...), env...);
                             }) {
            return ::beman::execution::get_completion_domain<value_t>(
                ::beman::execution::get_completion_scheduler<Source>(attrs, env...), env...);
        } else {
            return ::beman::execution::indeterminate_domain<>();
        }
    }

    template <typename... Source, typename... Env>
    static auto query_domain(::beman::execution::detail::type_list<Source...>,
                             const child_attrs_t& attrs,
                             const Env&... env) noexcept {
        if constexpr (sizeof...(Source) != 0 && (sizeof...(Env) != 0 || (has_source_domain<Source, Env...> && ...))) {
            return ::beman::execution::detail::common_domain(get_source_domain<Source>(attrs, env...)...);
        } else {
            return ::beman::execution::detail::no_completion_domain{};
        }
    }

    // A void result prevents the CPO from retrying without the receiver environment.
    template <typename... Source, typename... Env>
    static void
    query_scheduler(::beman::execution::detail::type_list<Source...>, const child_attrs_t&, const Env&...) noexcept {}

    // Distinct child channels can report schedulers of the same type that refer to different resources.
    template <typename Source, typename... Env>
        requires requires(const child_attrs_t& attrs, const Env&... env) {
            ::beman::execution::get_completion_scheduler<Source>(attrs, env...);
        }
    static auto query_scheduler(::beman::execution::detail::type_list<Source>,
                                const child_attrs_t& attrs,
                                const Env&... env) noexcept {
        return ::beman::execution::get_completion_scheduler<Source>(attrs, env...);
    }

  public:
    using ::beman::execution::detail::hide_completion_queries<child_attrs_t>::query;

    template <typename Tag, typename... Env>
        requires(!::std::same_as<Tag, void>)
    auto query(::beman::execution::get_completion_domain_t<Tag>, const Env&... env) const noexcept {
        return query_domain(completion_sources<Tag, ::beman::execution::detail::fwd_env_t<const Env&>...>(),
                            this->child_attrs,
                            ::beman::execution::detail::fwd_env(env)...);
    }

    // Preserve an explicit untagged customization domain when the value channel is unchanged.
    template <typename... Env>
        requires(::std::same_as<FromTag, ToTag> || !::std::same_as<ToTag, value_t>) &&
                requires(const child_attrs_t& attrs, const Env&... env) {
                    ::beman::execution::detail::try_query(attrs, ::beman::execution::get_completion_domain<>, env...);
                }
    auto query(::beman::execution::get_completion_domain_t<>, const Env&... env) const noexcept {
        return ::beman::execution::detail::try_query(
            this->child_attrs, ::beman::execution::get_completion_domain<>, env...);
    }

    template <typename Tag, typename... Env>
    auto query(::beman::execution::get_completion_scheduler_t<Tag>, const Env&... env) const noexcept {
        return query_scheduler(completion_sources<Tag, ::beman::execution::detail::fwd_env_t<const Env&>...>(),
                               this->child_attrs,
                               ::beman::execution::detail::fwd_env(env)...);
    }
};

template <typename FromTag, typename ToTag, typename ExceptionSource = no_exception_source, typename Child>
auto make_mapped_attrs(const Child& child) noexcept {
    return mapped_attrs<Child, FromTag, ToTag, ExceptionSource>{{::beman::execution::get_env(child)}};
}

} // namespace beman::execution::detail

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_MAPPED_ATTRS
