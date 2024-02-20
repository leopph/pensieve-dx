#pragma once

#include <expected>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace pensieve {
class Window {
public:
  [[nodiscard]] static auto Create() -> std::expected<Window, std::string>;

  Window(Window const& other) = delete;
  Window(Window&& other) noexcept;

  ~Window();

  auto operator=(Window const& other) -> void = delete;
  auto operator=(Window&& other) -> void = delete;

  auto PollEvents() noexcept -> void;
  [[nodiscard]] auto ShouldClose() const noexcept -> bool;
  [[nodiscard]] auto WasResized() const noexcept -> bool;

  [[nodiscard]] auto ToHwnd() const noexcept -> HWND;
  [[nodiscard]] auto IsValid() const noexcept -> bool;

  [[nodiscard]] explicit operator HWND() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept;

private:
  static auto CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                  LPARAM lparam) -> LRESULT;

  explicit Window(HWND hwnd);

  HWND hwnd_;
  bool should_close_{false};
  bool was_resized_{false};
};
}
