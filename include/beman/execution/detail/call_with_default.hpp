// include/beman/execution/detail/call_with_default.hpp              -*-C++-*-
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef INCLUDED_BEMAN_EXECUTION_DETAIL_CALL_WITH_DEFAULT
#define INCLUDED_BEMAN_EXECUTION_DETAIL_CALL_WITH_DEFAULT

#include <beman/execution/detail/common.hpp>
#ifdef BEMAN_HAS_IMPORT_STD
import std;
#else
#include <type_traits>
#include <utility>
#endif

// ----------------------------------------------------------------------------

namespace beman::execution::detail {

// call-with-default(fn, default, args...):
//   If fn(args...) is well-formed, returns fn(args...).
//   Otherwise, returns static_cast<Default>(default).
template <typename Fn, typename Default, typename... Args>
    requires requires(Fn&& fn, Args&&... args) {
        ::std::forward<Fn>(fn)(::std::forward<Args>(args)...);
    }
constexpr decltype(auto) call_with_default(Fn&& fn, Default&&, Args&&... args) noexcept(
    noexcept(::std::forward<Fn>(fn)(::std::forward<Args>(args)...))) {
    return ::std::forward<Fn>(fn)(::std::forward<Args>(args)...);
}

template <typename Fn, typename Default, typename... Args>
    requires(not requires(Fn&& fn, Args&&... args) {
        ::std::forward<Fn>(fn)(::std::forward<Args>(args)...);
    })
constexpr auto call_with_default(Fn&&, Default&& value, Args&&...) noexcept(
    noexcept(static_cast<Default>(::std::forward<Default>(value)))) -> Default {
    return static_cast<Default>(::std::forward<Default>(value));
}

} // namespace beman::execution::detail

// ----------------------------------------------------------------------------

#endif // INCLUDED_BEMAN_EXECUTION_DETAIL_CALL_WITH_DEFAULT
