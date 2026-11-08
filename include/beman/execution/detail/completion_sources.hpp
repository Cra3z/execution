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
import beman.execution.detail.meta.filter;
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
#include <beman/execution/detail/meta_filter.hpp>
#include <beman/execution/detail/sender_in.hpp>
#include <beman/execution/detail/set_error.hpp>
#include <beman/execution/detail/set_stopped.hpp>
#include <beman/execution/detail/set_value.hpp>
#include <beman/execution/detail/type_list.hpp>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {
/*!
 * \brief Whether `Child` may complete with `Source` using arguments `Args...` for which
 * `pred.template operator()<Args...>()` is `true`.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 * \internal
 *
 * If the completion signatures of `Child` are not known for `Env...` (i.e., `Child` is a dependent
 * sender and `Env...` is empty), `if_dependent` is returned.
 */
template <typename Source, typename Child, typename... Env, typename Pred>
consteval auto may_complete_with(Pred pred, bool if_dependent = true) -> bool {
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

template <typename Source, typename Child, typename... Env>
consteval auto may_complete_with() -> bool {
    return ::beman::execution::detail::may_complete_with<Source, Child, Env...>([]<typename...> { return true; });
}

/*!
 * \brief Completion attributes of an adaptor that completes on its child's execution agent.
 * \headerfile beman/execution.hpp <beman/execution.hpp>
 * \internal
 *
 * `Mapping::maps_to<Source, Tag, Env...>()` selects the child channels that can produce `Tag`.
 * A completion scheduler requires exactly one source; a completion domain combines all sources
 * ([exec.snd.general]). Other forwarding queries pass through to the child's attributes.
 */
template <typename Child, typename Mapping>
struct transformed_attrs : ::beman::execution::detail::hide_completion_queries<::beman::execution::env_of_t<Child>> {
    template <typename Tag, typename... Env>
    struct sources {
        template <typename Source>
        using matches = ::std::bool_constant<
            Mapping::template maps_to<Source, Tag, ::beman::execution::detail::fwd_env_t<Env>...>()>;

        using type = ::beman::execution::detail::meta::filter<
            matches,
            ::beman::execution::detail::type_list<::beman::execution::set_value_t,
                                                  ::beman::execution::set_error_t,
                                                  ::beman::execution::set_stopped_t>>;
    };

  private:
    using base = ::beman::execution::detail::hide_completion_queries<::beman::execution::env_of_t<Child>>;

    // Different channels may report different instances of the same scheduler type.
    template <typename Source, typename... Env>
    auto scheduler(::beman::execution::detail::type_list<Source>, const Env&... env) const noexcept
        -> decltype(::beman::execution::get_completion_scheduler<Source>(
            this->child_attrs, ::beman::execution::detail::fwd_env(env)...)) {
        return ::beman::execution::get_completion_scheduler<Source>(this->child_attrs,
                                                                    ::beman::execution::detail::fwd_env(env)...);
    }

    template <typename... Env, typename... Source>
        requires(sizeof...(Source) != 0)
    static auto domain(::beman::execution::detail::type_list<Source...>)
        -> ::beman::execution::detail::common_domain_t<
            ::beman::execution::detail::
                compl_domain_of_t<Source, Child, ::beman::execution::detail::fwd_env_t<Env>...>...>;

  public:
    using base::query;

    template <typename Tag, typename... Env>
    auto query(::beman::execution::get_completion_scheduler_t<Tag>, const Env&... env) const noexcept
        -> decltype(this->scheduler(typename sources<Tag, Env...>::type{}, env...)) {
        return this->scheduler(typename sources<Tag, Env...>::type{}, env...);
    }

    template <typename Tag, typename... Env>
        requires(!::std::same_as<Tag, void>)
    auto query(::beman::execution::get_completion_domain_t<Tag>, const Env&...) const noexcept
        -> decltype(domain<Env...>(typename sources<Tag, Env...>::type{})) {
        return {};
    }
};
} // namespace beman::execution::detail

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_COMPLETION_SOURCES
