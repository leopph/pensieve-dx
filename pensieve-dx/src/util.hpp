#pragma once

#include <concepts>

namespace pensieve {
template <std::unsigned_integral T>
[[nodiscard]] constexpr auto sat_sub(T const lhs, T const rhs) -> T {
  T ret{lhs - rhs};
  ret &= -(ret <= lhs);
  return ret;
}
}
