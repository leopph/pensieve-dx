#include "window.hpp"

#include <bit>

#include <Windowsx.h>

namespace pensieve {
auto Window::PollEvents() noexcept -> void {
  should_close_ = false;
  was_resized_ = false;
  mouse_delta_ = {0, 0};
  mouse_wheel_delta_ = 0;

  MSG msg;
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  for (auto i{0}; i < 2; i++) {
    mouse_pos_[i] += mouse_delta_[i];
  }

  is_mouse_hovered_ = mouse_pos_[0] >= 0 && mouse_pos_[1] >= 0 && mouse_pos_[0]
    < static_cast<int>(size_[0]) && mouse_pos_[1] < static_cast<int>(size_[1]);
}

auto Window::ShouldClose() const noexcept -> bool {
  return should_close_;
}

auto Window::WasResized() const noexcept -> bool {
  return was_resized_;
}

auto Window::GetSize() const noexcept -> std::span<unsigned const, 2> {
  return size_;
}

auto Window::IsLmbDown() const noexcept -> bool {
  return is_lmb_down_;
}

auto Window::IsMouseHovered() const noexcept -> bool {
  return is_mouse_hovered_;
}

auto Window::GetMouseDelta() const noexcept -> std::span<int const, 2> {
  return mouse_delta_;
}

auto Window::GetMouseWheelDelta() const noexcept -> int {
  return mouse_wheel_delta_;
}

auto Window::ToHwnd() const noexcept -> HWND {
  return hwnd_;
}

auto Window::IsValid() const noexcept -> bool {
  return hwnd_ != nullptr;
}

Window::operator HWND__*() const noexcept {
  return ToHwnd();
}

Window::operator bool() const noexcept {
  return IsValid();
}

auto Window::WindowProc(HWND const hwnd, UINT const msg, WPARAM const wparam,
                        LPARAM const lparam) -> LRESULT {
  if (auto const self{
    std::bit_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA))
  }) {
    switch (msg) {
    case WM_CLOSE: {
      self->should_close_ = true;
      return 0;
    }

    case WM_SIZE: {
      self->was_resized_ = true;
      self->size_[0] = LOWORD(lparam);
      self->size_[1] = HIWORD(lparam);
      return 0;
    }

    case WM_LBUTTONDOWN: {
      self->is_lmb_down_ = true;
      return 0;
    }

    case WM_LBUTTONUP: {
      self->is_lmb_down_ = false;
      return 0;
    }

    case WM_MOUSEWHEEL: {
      self->mouse_wheel_delta_ = GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA;
      return 0;
    }

    case WM_MOUSEMOVE: {
      std::array const new_pos{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
      for (auto i{0}; i < 2; i++) {
        self->mouse_delta_[i] = new_pos[i] - self->mouse_pos_[i];
      }
      return 0;
    }
    }
  }

  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

Window::Window(HWND const hwnd) :
  hwnd_{hwnd} {
  SetWindowLongPtrW(hwnd_, GWLP_USERDATA, std::bit_cast<LONG_PTR>(this));

  RECT client_rect;
  GetClientRect(hwnd_, &client_rect);
  size_[0] = client_rect.right - client_rect.left;
  size_[1] = client_rect.bottom - client_rect.top;

  ShowWindow(hwnd_, SW_SHOW);
}

auto Window::Create() -> std::expected<Window, std::string> {
  WNDCLASSEXW const window_class{
    sizeof(WNDCLASSEX), 0, &WindowProc, 0, 0, GetModuleHandleW(nullptr),
    nullptr, LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, L"Pensieve-DX",
    nullptr
  };

  if (!RegisterClassExW(&window_class)) {
    return std::unexpected{"Failed to register window class."};
  }

  auto const hwnd{
    CreateWindowExW(0, window_class.lpszClassName, L"Pensieve-DX",
                    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                    CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr,
                    window_class.hInstance, nullptr)
  };

  if (!hwnd) {
    return std::unexpected{"Failed to create window."};
  }

  return Window{hwnd};
}

Window::Window(Window&& other) noexcept :
  Window{other.hwnd_} {
  other.hwnd_ = nullptr;
}

Window::~Window() {
  DestroyWindow(hwnd_);
}
}
