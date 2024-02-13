#include <cstdlib>
#include <expected>
#include <iostream>
#include <memory>
#include <type_traits>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace {
auto CALLBACK WindowProc(HWND const hwnd, UINT const msg, WPARAM const wparam,
                         LPARAM const lparam) -> LRESULT {
  if (msg == WM_CLOSE) {
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

auto run() -> std::expected<void, std::string> {
  using Microsoft::WRL::ComPtr;
  [[maybe_unused]] HRESULT hr;

  UINT factory_create_flags{0};

#ifndef NDEBUG
  factory_create_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

  ComPtr<IDXGIFactory7> factory;
  hr = CreateDXGIFactory2(factory_create_flags, IID_PPV_ARGS(&factory));
  if (FAILED(hr)) {
    return std::unexpected{"Failed to create DXGI factory."};
  }

  UINT adapter_idx{0};
  ComPtr<IDXGIAdapter4> adapter;

  while (true) {
    hr = factory->EnumAdapterByGpuPreference(
      0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
    if (SUCCEEDED(hr)) {
      break;
    }

    if (hr != DXGI_ERROR_NOT_FOUND) {
      return std::unexpected{"Failed to enumerate adapters."};
    }

    ++adapter_idx;
  }

  ComPtr<ID3D12Device10> device;
  hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                         IID_PPV_ARGS(&device));
  if (FAILED(hr)) {
    return std::unexpected{"Failed to create D3D device."};
  }

  WNDCLASSEXW const window_class{
    sizeof(WNDCLASSEX), 0, &WindowProc, 0, 0, GetModuleHandleW(nullptr),
    nullptr, LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, L"Pensieve-DX",
    nullptr
  };

  if (!RegisterClassExW(&window_class)) {
    return std::unexpected{"Failed to register window class."};
  }

  std::unique_ptr<std::remove_pointer_t<HWND>, decltype([](HWND const hwnd) {
    DestroyWindow(hwnd);
  })> const hwnd{
    CreateWindowExW(0, window_class.lpszClassName, L"Pensieve-DX",
                    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                    CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr,
                    window_class.hInstance, nullptr)
  };

  if (!hwnd) {
    return std::unexpected{"Failed to create window."};
  }

  ShowWindow(hwnd.get(), SW_SHOW);

  while (true) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        return {};
      }

      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
  }
}
}

auto main() -> int {
  if (auto const res{run()}; !res) {
    std::cerr << res.error() << '\n';
    return EXIT_FAILURE;
  }
}
