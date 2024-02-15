#pragma once

#include <string_view>

namespace pensieve {
auto HandleError(std::string_view err_str) noexcept -> void;
}
