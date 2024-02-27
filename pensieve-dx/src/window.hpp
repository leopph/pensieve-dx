#pragma once

#include <array>
#include <expected>
#include <span>
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
  [[nodiscard]] auto GetSize() const noexcept -> std::span<unsigned const, 2>;
  [[nodiscard]] auto IsLmbDown() const noexcept -> bool;
  [[nodiscard]] auto IsMmbDown() const noexcept -> bool;
  [[nodiscard]] auto IsMouseHovered() const noexcept -> bool;
  [[nodiscard]] auto GetMouseDelta() const noexcept -> std::span<int const, 2>;
  [[nodiscard]] auto GetMouseWheelDelta() const noexcept -> int;

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

  std::array<unsigned, 2> size_{0, 0};

  bool is_lmb_down_{false};
  bool is_mmb_down_{false};
  bool is_mouse_hovered_{false};
  std::array<int, 2> mouse_pos_{0, 0};
  std::array<int, 2> mouse_delta_{0, 0};
  int mouse_wheel_delta_{0};
};
}
