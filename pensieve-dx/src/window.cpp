#include "window.hpp"

#include <bit>

namespace pensieve {
auto Window::PollEvents() noexcept -> void {
  should_close_ = false;
  was_resized_ = false;

  MSG msg;
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
}

auto Window::ShouldClose() const noexcept -> bool {
  return should_close_;
}

auto Window::WasResized() const noexcept -> bool {
  return was_resized_;
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
    if (msg == WM_CLOSE) {
      self->should_close_ = true;
      return 0;
    }

    if (msg == WM_SIZE) {
      self->was_resized_ = true;
      return 0;
    }
  }

  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

Window::Window(HWND const hwnd) :
  hwnd_{hwnd} {
  SetWindowLongPtrW(hwnd_, GWLP_USERDATA, std::bit_cast<LONG_PTR>(this));
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
