// include/beman/execution/detail/completion_sources.hpp              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_COMPLETION_SOURCES
#define INCLUDED_BEMAN_EXECUTION_DETAIL_COMPLETION_SOURCES

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <concepts>
#include <type_traits>
#include <utility>
#endif
#ifdef BEMAN_HAS_MODULES
import beman.execution.detail.common_domain;
import beman.execution.detail.compl_domain;
import beman.execution.detail.env_of_t;
import beman.execution.detail.fwd_env;
import beman.execution.detail.get_completion_domain;
import beman.execution.detail.get_completion_scheduler;
import beman.execution.detail.get_completion_signatures;
import beman.execution.detail.hide_completion_queries;
import beman.execution.detail.meta.combine;
import beman.execution.detail.sender_in;
import beman.execution.detail.set_error;
import beman.execution.detail.set_stopped;
import beman.execution.detail.set_value;
import beman.execution.detail.type_list;
#else
#include <beman/execution/detail/common_domain.hpp>
#include <beman/execution/detail/compl_domain.hpp>
#include <beman/execution/detail/env_of_t.hpp>
#include <beman/execution/detail/fwd_env.hpp>
#include <beman/execution/detail/get_completion_domain.hpp>
#include <beman/execution/detail/get_completion_scheduler.hpp>
#include <beman/execution/detail/get_completion_signatures.hpp>
#include <beman/execution/detail/hide_completion_queries.hpp>
#include <beman/execution/detail/meta_combine.hpp>
#include <beman/execution/detail/sender_in.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_stopped.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/type_list.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {
inline constexpr auto any_completion_args = []<typename...>() { return true; };

/*!
 * \brief Whether `Child` may complete with `Source` using arguments `Args...` for which
 * `pred.template operator()<Args...>()` is `true`.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 * \internal
 *
 * If the completion signatures of `Child` are not known for `Env...` (i.e., `Child` is a dependent
 * sender and `Env...` is empty), `if_dependent` is returned.
 */
template <typename Source,
          typename Child,
          typename... Env,
          typename Pred = ::std::remove_cvref_t<decltype(::beman::execution::detail::any_completion_args)>>
consteval auto may_complete_with(Pred pred = ::beman::execution::detail::any_completion_args, bool if_dependent = true)
    -> bool {
    if constexpr (::beman::execution::sender_in<Child, Env...>) {
        bool result{false};
        ::beman::execution::get_completion_signatures<Child, Env...>().for_each(
            [&]<typename Tag, typename... Args>(Tag (*)(Args...)) {
                if constexpr (::std::same_as<Tag, Source>)
                    result = result || pred.template operator()<Args...>();
            });
        return result;
    } else {
        return if_dependent;
    }
}

/*!
 * \brief The list of those completion tags `set_value_t`, `set_error_t`, `set_stopped_t` whose flag is `true`.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 * \internal
 */
template <bool Value, bool Error, bool Stopped>
using completion_tags = ::beman::execution::detail::meta::combine<
    ::std::conditional_t<Value,
                         ::beman::execution::detail::type_list<::beman::execution::set_value_t>,
                         ::beman::execution::detail::type_list<>>,
    ::std::conditional_t<Error,
                         ::beman::execution::detail::type_list<::beman::execution::set_error_t>,
                         ::beman::execution::detail::type_list<>>,
    ::std::conditional_t<Stopped,
                         ::beman::execution::detail::type_list<::beman::execution::set_stopped_t>,
                         ::beman::execution::detail::type_list<>>>;

/*!
 * \brief The child completion tags `Source` for which `Sources::template value<Tag, Source, Env...>()` is `true`.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 * \internal
 */
template <typename Sources, typename Tag, typename... Env>
using completion_sources_t = ::beman::execution::detail::completion_tags<
    Sources::template value<Tag, ::beman::execution::set_value_t, Env...>(),
    Sources::template value<Tag, ::beman::execution::set_error_t, Env...>(),
    Sources::template value<Tag, ::beman::execution::set_stopped_t, Env...>()>;

/*!
 * \brief Attributes of a single-child sender adaptor which completes inline, i.e., each of its
 * completions is evaluated on the execution agent evaluating one of the child's completions.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 * \internal
 *
 * `Sources::template value<Tag, Source, Env...>()` tells whether a `Tag` completion of the adaptor can
 * be evaluated within a `Source` completion of the child, where `Env...` are the environments of the
 * child's receiver. Following [exec.snd.general], for a `Tag` with the source channels `Ss...`:
 * - the completion domain is the common domain of the child's completion domains for `Ss...`, and
 * - the completion scheduler is the child's completion scheduler for `S` if `Ss...` is just `S`.
 *   Different channels may report different scheduler objects, so nothing is reported otherwise.
 * All other forwarding queries are forwarded to the child's attributes.
 */
template <typename Child, typename Sources>
struct child_completion_attrs
    : ::beman::execution::detail::hide_completion_queries<::beman::execution::env_of_t<Child>> {
  private:
    using child_attrs_t = ::std::remove_cvref_t<::beman::execution::env_of_t<Child>>;

    template <typename Tag, typename... Env>
    using sources = ::beman::execution::detail::completion_sources_t<Sources, Tag, Env...>;

    template <typename Source, typename... Env>
    static auto
    scheduler(::beman::execution::detail::type_list<Source>, const child_attrs_t& attrs, const Env&... env) noexcept
        -> decltype(::beman::execution::get_completion_scheduler<Source>(attrs, env...)) {
        return ::beman::execution::get_completion_scheduler<Source>(attrs, env...);
    }

    template <typename... Env, typename... Source>
        requires(sizeof...(Source) != 0)
    static auto domain(::beman::execution::detail::type_list<Source...>)
        -> ::beman::execution::detail::common_domain_t<
            ::beman::execution::detail::compl_domain_of_t<Source, Child, Env...>...>;

  public:
    using ::beman::execution::detail::hide_completion_queries<::beman::execution::env_of_t<Child>>::query;

    template <typename Tag, typename... Env>
    auto query(::beman::execution::get_completion_scheduler_t<Tag>, const Env&... env) const noexcept
        -> decltype(scheduler(sources<Tag, ::beman::execution::detail::fwd_env_t<Env>...>{},
                              ::std::declval<const child_attrs_t&>(),
                              ::beman::execution::detail::fwd_env(env)...)) {
        return scheduler(sources<Tag, ::beman::execution::detail::fwd_env_t<Env>...>{},
                         this->child_attrs,
                         ::beman::execution::detail::fwd_env(env)...);
    }

    template <typename Tag, typename... Env>
        requires(!::std::same_as<Tag, void>)
    auto query(::beman::execution::get_completion_domain_t<Tag>, const Env&...) const noexcept
        -> decltype(domain<::beman::execution::detail::fwd_env_t<Env>...>(
            sources<Tag, ::beman::execution::detail::fwd_env_t<Env>...>{})) {
        return {};
    }
};
} // namespace beman::execution::detail

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_COMPLETION_SOURCES
