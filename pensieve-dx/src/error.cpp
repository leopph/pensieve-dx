#include "error.hpp"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace pensieve {
auto HandleError(std::string_view const err_str) noexcept -> void {
  MessageBoxA(nullptr, err_str.data(), "Error", MB_ICONERROR);
}
}
