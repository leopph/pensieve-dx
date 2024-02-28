#pragma once

#include <concepts>

namespace pensieve {
template <std::unsigned_integral T>
[[nodiscard]] constexpr auto SatSub(T const lhs, T const rhs) -> T {
  T ret{lhs - rhs};
  ret &= -(ret <= lhs);
  return ret;
}

template <std::unsigned_integral T>
[[nodiscard]] constexpr auto NextMultipleOf(T const of, T const from) -> T {
  return from + of - from % of;
}

template <std::unsigned_integral T>
[[nodiscard]] constexpr auto DivRoundUp(T const lhs, T const rhs) -> T {
  return (lhs + rhs - 1) / rhs;
}
}
