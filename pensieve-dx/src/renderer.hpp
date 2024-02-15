#pragma once

#include <array>
#include <expected>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "model_loading.hpp"
#include "gpu_model.hpp"

namespace pensieve {
class Renderer {
public:
  [[nodiscard]] static auto Create(
    HWND hwnd) -> std::expected<Renderer, std::string>;

  auto CreateGpuModel(
    ModelData const& model_data) -> std::expected<GpuModel, std::string>;

  auto DrawFrame() -> std::expected<void, std::string>;

private:
  static auto constexpr swap_chain_buffer_count_{2};
  static auto constexpr swap_chain_format_{DXGI_FORMAT_R8G8B8A8_UNORM};
  static auto constexpr max_gpu_queued_frames_{1};
  static auto constexpr max_frames_in_flight_{max_gpu_queued_frames_ + 1};
  static auto constexpr res_desc_heap_size_{1'000'000};

  Renderer(Microsoft::WRL::ComPtr<IDXGIFactory7> factory,
           Microsoft::WRL::ComPtr<ID3D12Device10> device,
           Microsoft::WRL::ComPtr<ID3D12CommandQueue> direct_queue,
           Microsoft::WRL::ComPtr<IDXGISwapChain4> swap_chain,
           std::array<Microsoft::WRL::ComPtr<ID3D12Resource>,
                      swap_chain_buffer_count_> swap_chain_buffers,
           Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap,
           Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> res_desc_heap,
           std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>,
                      max_frames_in_flight_> cmd_allocs,
           std::array<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList7>,
                      max_frames_in_flight_> cmd_lists,
           Microsoft::WRL::ComPtr<ID3D12Fence> frame_fence,
           UINT swap_chain_flags, UINT present_flags);

  [[nodiscard]] auto AllocateResourceDescriptorIndex() -> UINT;
  auto FreeResourceDescriptorIndex(UINT idx) -> void;

  Microsoft::WRL::ComPtr<IDXGIFactory7> factory_;
  Microsoft::WRL::ComPtr<ID3D12Device10> device_;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> direct_queue_;

  Microsoft::WRL::ComPtr<IDXGISwapChain4> swap_chain_;
  std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, swap_chain_buffer_count_>
  swap_chain_buffers_;

  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap_;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> res_desc_heap_;

  std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>,
             max_frames_in_flight_> cmd_allocs_;
  std::array<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList7>,
             max_frames_in_flight_> cmd_lists_;

  Microsoft::WRL::ComPtr<ID3D12Fence> frame_fence_;

  UINT rtv_inc_;
  UINT res_desc_inc_;

  std::vector<UINT> res_desc_heap_free_indices_;

  D3D12_CPU_DESCRIPTOR_HANDLE rtv_heap_cpu_start_;
  D3D12_CPU_DESCRIPTOR_HANDLE res_desc_heap_cpu_start_;

  UINT64 frame_fence_val_;
  UINT swap_chain_flags_;
  UINT present_flags_;
  UINT next_free_res_desc_idx_{0};
  int frame_idx_{0};
};
}
