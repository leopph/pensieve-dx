#include <array>
#include <cstdlib>
#include <expected>
#include <format>
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

#include "model_loading.hpp"

namespace pensieve {
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

  UINT factory_create_flags{0};

#ifndef NDEBUG
  factory_create_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

  ComPtr<IDXGIFactory7> factory;
  if (FAILED(
    CreateDXGIFactory2(factory_create_flags, IID_PPV_ARGS(&factory)))) {
    return std::unexpected{"Failed to create DXGI factory."};
  }

  UINT adapter_idx{0};
  ComPtr<IDXGIAdapter4> adapter;

  while (true) {
    auto const hr{
      factory->EnumAdapterByGpuPreference(
        0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))
    };
    if (SUCCEEDED(hr)) {
      break;
    }

    if (hr != DXGI_ERROR_NOT_FOUND) {
      return std::unexpected{"Failed to enumerate adapters."};
    }

    ++adapter_idx;
  }

  ComPtr<ID3D12Device10> device;
  if (FAILED(
    D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&
      device)))) {
    return std::unexpected{"Failed to create D3D device."};
  }

  D3D12_COMMAND_QUEUE_DESC constexpr direct_queue_desc{
    D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
    D3D12_COMMAND_QUEUE_FLAG_NONE, 0
  };

  ComPtr<ID3D12CommandQueue> direct_queue;
  if (FAILED(
    device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(&direct_queue)
    ))) {
    return std::unexpected{"Failed to create direct command queue."};
  }

  WNDCLASSEXW const window_class{
    sizeof(WNDCLASSEX), 0, &WindowProc, 0, 0, GetModuleHandleW(nullptr),
    nullptr, LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, L"Pensieve-DX",
    nullptr
  };

  if (!RegisterClassExW(&window_class)) {
    return std::unexpected{"Failed to register window class."};
  }

  std::unique_ptr<std::remove_pointer_t<HWND>, decltype([ ](HWND const hwnd) {
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

  UINT swap_chain_flags{0};
  UINT present_flags{0};

  if (BOOL is_tearing_supported{FALSE}; SUCCEEDED(
      factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &
        is_tearing_supported, sizeof(is_tearing_supported))) &&
    is_tearing_supported) {
    swap_chain_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    present_flags |= DXGI_PRESENT_ALLOW_TEARING;
  }

  auto constexpr swap_chain_buffer_count{2};
  auto constexpr swap_chain_format{DXGI_FORMAT_R8G8B8A8_UNORM};

  DXGI_SWAP_CHAIN_DESC1 const swap_chain_desc{
    0, 0, swap_chain_format, FALSE, {1, 0}, DXGI_USAGE_RENDER_TARGET_OUTPUT,
    swap_chain_buffer_count, DXGI_SCALING_STRETCH,
    DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_ALPHA_MODE_UNSPECIFIED, swap_chain_flags
  };

  ComPtr<IDXGISwapChain1> swap_chain1;
  if (FAILED(
    factory->CreateSwapChainForHwnd(direct_queue.Get(), hwnd.get(), &
      swap_chain_desc, nullptr, nullptr, &swap_chain1))) {
    return std::unexpected{"Failed to create swap chain."};
  }

  ComPtr<IDXGISwapChain4> swap_chain;
  if (FAILED(swap_chain1.As(&swap_chain))) {
    return std::unexpected{"Failed to get IDXGISwapChain4 interface."};
  }

  std::array<ComPtr<ID3D12Resource>, swap_chain_buffer_count>
    swap_chain_buffers;
  for (UINT i{0}; i < swap_chain_buffer_count; i++) {
    if (FAILED(
      swap_chain->GetBuffer(i, IID_PPV_ARGS(&swap_chain_buffers[i])))) {
      return std::unexpected{
        std::format("Failed to get swap chain buffer {}.", i)
      };
    }
  }

  D3D12_DESCRIPTOR_HEAP_DESC constexpr rtv_heap_desc{
    D3D12_DESCRIPTOR_HEAP_TYPE_RTV, swap_chain_buffer_count,
    D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0
  };

  ComPtr<ID3D12DescriptorHeap> rtv_heap;
  if (FAILED(
    device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap)))) {
    return std::unexpected{"Failed to create RTV heap."};
  }

  auto const rtv_inc{
    device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
  };

  for (UINT i{0}; i < swap_chain_buffer_count; i++) {
    D3D12_RENDER_TARGET_VIEW_DESC constexpr rtv_desc{
      .Format = swap_chain_format,
      .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D, .Texture2D = {0, 0}
    };

    device->CreateRenderTargetView(swap_chain_buffers[i].Get(), &rtv_desc,
                                   CD3DX12_CPU_DESCRIPTOR_HANDLE{
                                     rtv_heap->
                                     GetCPUDescriptorHandleForHeapStart(),
                                     static_cast<INT>(i), rtv_inc
                                   });
  }

  auto constexpr max_frames_in_flight{2};
  auto frame_idx{0};

  std::array<ComPtr<ID3D12CommandAllocator>, max_frames_in_flight> cmd_allocs;
  std::array<ComPtr<ID3D12GraphicsCommandList7>, max_frames_in_flight>
    cmd_lists;

  for (auto i{0}; i < max_frames_in_flight; i++) {
    if (FAILED(
      device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&cmd_allocs[i])))) {
      return std::unexpected{
        std::format("Failed to create direct command allocator {}.", i)
      };
    }

    if (FAILED(
      device->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&cmd_lists[i])))) {
      return std::unexpected{
        std::format("Failed to create direct command list {}.", i)
      };
    }
  }

  UINT64 frame_fence_val{0};
  ComPtr<ID3D12Fence> frame_fence;
  if (FAILED(
    device->CreateFence(frame_fence_val, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&
      frame_fence)))) {
    return std::unexpected{"Failed to create frame fence."};
  }

  while (true) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        return {};
      }

      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }

    if (FAILED(cmd_allocs[frame_idx]->Reset())) {
      return std::unexpected{
        std::format("Failed to reset command allocator {}.", frame_idx)
      };
    }

    if (FAILED(
      cmd_lists[frame_idx]->Reset(cmd_allocs[frame_idx].Get(), nullptr))) {
      return std::unexpected{
        std::format("Failed to reset command list {}.", frame_idx)
      };
    }

    cmd_lists[frame_idx]->ClearRenderTargetView(
      CD3DX12_CPU_DESCRIPTOR_HANDLE{
        rtv_heap->GetCPUDescriptorHandleForHeapStart(), frame_idx, rtv_inc
      }, std::array{1.0f, 0.0f, 1.0f, 1.0f}.data(), 0, nullptr);

    if (FAILED(cmd_lists[frame_idx]->Close())) {
      return std::unexpected{
        std::format("Failed to close command list {}.", frame_idx)
      };
    }

    direct_queue->ExecuteCommandLists(
      1, CommandListCast(cmd_lists[frame_idx].GetAddressOf()));

    if (FAILED(swap_chain->Present(0, present_flags))) {
      return std::unexpected{"Failed to present."};
    }

    frame_idx = (frame_idx + 1) % max_frames_in_flight;
  }
}
}
}

auto main(int const argc, char* argv[]) -> int {
  if (argc < 2) {
    std::cout << "Usage: pensieve-dx <path-to-model-file>\n";
    return EXIT_SUCCESS;
  }

  auto const model_data{pensieve::LoadModel(argv[1])};

  if (!model_data) {
    std::cerr << model_data.error() << '\n';
    return EXIT_FAILURE;
  }

  if (auto const res{pensieve::run()}; !res) {
    std::cerr << res.error() << '\n';
    return EXIT_FAILURE;
  }
}
