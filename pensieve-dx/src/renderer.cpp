#include "renderer.hpp"

#include <algorithm>
#include <bit>
#include <format>
#include <ranges>
#include <utility>

#include <d3dx12.h>

#ifndef NDEBUG
#include <dxgidebug.h>
#endif

#include "shader_interop.hpp"
#include "util.hpp"

using Microsoft::WRL::ComPtr;

extern "C" {
__declspec(dllexport) extern UINT const D3D12SDKVersion{D3D12_SDK_VERSION};
__declspec(dllexport) extern char const* D3D12SDKPath{".\\D3D12\\"};
}

namespace pensieve {
auto Renderer::Create(HWND const hwnd) -> std::expected<Renderer, std::string> {
#ifndef NDEBUG
  ComPtr<ID3D12Debug6> debug;
  if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
    return std::unexpected{"Failed to get D3D12 debug."};
  }

  debug->EnableDebugLayer();

  ComPtr<IDXGIInfoQueue> dxgi_info_queue;
  if FAILED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_info_queue))) {
    return std::unexpected{"Failed to get DXGI info queue."};
  }

  if (FAILED(
    dxgi_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL,
      DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE))) {
    return std::unexpected{"Failed to set debug break on DXGI error."};
  }

  if (FAILED(
    dxgi_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL,
      DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE))) {
    return std::unexpected{"Failed to set debug break on DXGI corruption."};
  }
#endif

  UINT factory_create_flags{0};

#ifndef NDEBUG
  factory_create_flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

  ComPtr<IDXGIFactory7> factory;
  if (FAILED(
    CreateDXGIFactory2(factory_create_flags, IID_PPV_ARGS(&factory)))) {
    return std::unexpected{"Failed to create DXGI factory."};
  }

  ComPtr<IDXGIAdapter4> adapter;
  if (FAILED(
    factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
      IID_PPV_ARGS(&adapter)))) {
    return std::unexpected{"Failed to get high performance adapter."};
  }

  ComPtr<ID3D12Device10> device;
  if (FAILED(
    D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&
      device)))) {
    return std::unexpected{"Failed to create D3D device."};
  }

#ifndef NDEBUG
  ComPtr<ID3D12InfoQueue> d3d12_info_queue;
  if FAILED(device.As(&d3d12_info_queue)) {
    return std::unexpected{"Failed to get D3D12 info queue."};
  }

  if (FAILED(
    d3d12_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE))) {
    return std::unexpected{"Failed to set debug break on D3D12 error."};
  }

  if (FAILED(
    d3d12_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE
    ))) {
    return std::unexpected{"Failed to set debug break on D3D12 corruption."};
  }
#endif

  CD3DX12FeatureSupport features;
  if (FAILED(features.Init(device.Get()))) {
    return std::unexpected{"Failed to query GPU features."};
  }

  if (features.ResourceBindingTier() < D3D12_RESOURCE_BINDING_TIER_3) {
    return std::unexpected{"GPU does not support resource binding tier 3."};
  }

  if (features.HighestShaderModel() < D3D_SHADER_MODEL_6_6) {
    return std::unexpected{"GPU does not support shader model 6.6."};
  }

  if (!features.EnhancedBarriersSupported()) {
    return std::unexpected{"GPU does not support enhanced barriers."};
  }

  if (features.MeshShaderTier() < D3D12_MESH_SHADER_TIER_1) {
    return std::unexpected{"GPU does not support mesh shaders."};
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

  UINT swap_chain_flags{0};
  UINT present_flags{0};

  if (BOOL is_tearing_supported{FALSE}; SUCCEEDED(
      factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &
        is_tearing_supported, sizeof(is_tearing_supported))) &&
    is_tearing_supported) {
    swap_chain_flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    present_flags |= DXGI_PRESENT_ALLOW_TEARING;
  }


  DXGI_SWAP_CHAIN_DESC1 const swap_chain_desc{
    0, 0, swap_chain_format_, FALSE, {1, 0}, DXGI_USAGE_RENDER_TARGET_OUTPUT,
    swap_chain_buffer_count_, DXGI_SCALING_STRETCH,
    DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_ALPHA_MODE_UNSPECIFIED, swap_chain_flags
  };

  ComPtr<IDXGISwapChain1> swap_chain1;
  if (FAILED(
    factory->CreateSwapChainForHwnd(direct_queue.Get(), hwnd, & swap_chain_desc,
      nullptr, nullptr, &swap_chain1))) {
    return std::unexpected{"Failed to create swap chain."};
  }

  ComPtr<IDXGISwapChain4> swap_chain;
  if (FAILED(swap_chain1.As(&swap_chain))) {
    return std::unexpected{"Failed to get IDXGISwapChain4 interface."};
  }

  std::array<ComPtr<ID3D12Resource>, swap_chain_buffer_count_>
    swap_chain_buffers;
  for (UINT i{0}; i < swap_chain_buffer_count_; i++) {
    if (FAILED(
      swap_chain->GetBuffer(i, IID_PPV_ARGS(&swap_chain_buffers[i])))) {
      return std::unexpected{
        std::format("Failed to get swap chain buffer {}.", i)
      };
    }
  }

  D3D12_DESCRIPTOR_HEAP_DESC constexpr rtv_heap_desc{
    D3D12_DESCRIPTOR_HEAP_TYPE_RTV, swap_chain_buffer_count_,
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

  auto const rtv_heap_cpu_start{rtv_heap->GetCPUDescriptorHandleForHeapStart()};

  for (UINT i{0}; i < swap_chain_buffer_count_; i++) {
    D3D12_RENDER_TARGET_VIEW_DESC constexpr rtv_desc{
      .Format = swap_chain_format_,
      .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D, .Texture2D = {0, 0}
    };

    device->CreateRenderTargetView(swap_chain_buffers[i].Get(), &rtv_desc,
                                   CD3DX12_CPU_DESCRIPTOR_HANDLE{
                                     rtv_heap_cpu_start, static_cast<INT>(i),
                                     rtv_inc
                                   });
  }

  D3D12_DESCRIPTOR_HEAP_DESC constexpr res_desc_heap_desc{
    D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, res_desc_heap_size_,
    D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0
  };

  ComPtr<ID3D12DescriptorHeap> res_desc_heap;
  if (FAILED(
    device->CreateDescriptorHeap(&res_desc_heap_desc, IID_PPV_ARGS(&
      res_desc_heap)))) {
    return std::unexpected{"Failed to create resource descriptor heap."};
  }

  /*auto const res_desc_inc{
    device->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
  };

  auto const res_desc_heap_cpu_start{
    res_desc_heap->GetCPUDescriptorHandleForHeapStart()
  };

  // This crashes SRV creation later
  for (UINT i{0}; i < res_desc_heap_size_ - 1; i++) {
    *std::bit_cast<UINT*>(CD3DX12_CPU_DESCRIPTOR_HANDLE{
      res_desc_heap_cpu_start, static_cast<INT>(i), res_desc_inc
    }) = i + 1;
  }*/

  std::array<ComPtr<ID3D12CommandAllocator>, max_frames_in_flight_> cmd_allocs;
  std::array<ComPtr<ID3D12GraphicsCommandList7>, max_frames_in_flight_>
    cmd_lists;

  for (auto i{0}; i < max_frames_in_flight_; i++) {
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

  return Renderer{
    std::move(factory), std::move(device), std::move(direct_queue),
    std::move(swap_chain), std::move(swap_chain_buffers), std::move(rtv_heap),
    std::move(res_desc_heap), std::move(cmd_allocs), std::move(cmd_lists),
    std::move(frame_fence), swap_chain_flags, present_flags
  };
}

auto Renderer::CreateGpuModel(
  ModelData const& model_data) -> std::expected<GpuModel, std::string> {
  auto const upload_buffer_size{
    [&model_data] {
      UINT64 ret{sizeof(Material)};

      for (auto const& [width, height, bytes] : model_data.textures) {
        ret = std::max<UINT64>(ret, 4 * width * height);
      }

      for (auto const& mesh_data : model_data.meshes) {
        ret = std::max<UINT64>(
          ret, mesh_data.positions.size() * sizeof(decltype(mesh_data.positions
          )::value_type));
      }

      return ret;
    }()
  };

  D3D12_HEAP_PROPERTIES constexpr upload_heap_props{
    D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN, 0, 0
  };

  auto const upload_buffer_desc{
    CD3DX12_RESOURCE_DESC1::Buffer(upload_buffer_size)
  };

  ComPtr<ID3D12Resource2> upload_buffer;
  if (FAILED(
    device_->CreateCommittedResource3(&upload_heap_props, D3D12_HEAP_FLAG_NONE,
      &upload_buffer_desc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, nullptr, 0,
      nullptr, IID_PPV_ARGS(&upload_buffer)))) {
    return std::unexpected{"Failed to create GPU upload buffer."};
  }

  void* upload_buffer_ptr;
  if (FAILED(upload_buffer->Map(0, nullptr, &upload_buffer_ptr))) {
    return std::unexpected{"Failed to map GPU upload buffer."};
  }

  UINT64 upload_fence_val{0};
  ComPtr<ID3D12Fence> upload_fence;
  if (FAILED(
    device_->CreateFence(upload_fence_val, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&
      upload_fence)))) {
    return std::unexpected{"Failed to create upload fence."};
  }

  D3D12_HEAP_PROPERTIES constexpr default_heap_props{
    D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
    D3D12_MEMORY_POOL_UNKNOWN, 0, 0
  };

  GpuModel ret;

  for (auto const& [idx, img] : std::ranges::views::enumerate(
         model_data.textures)) {
    auto& gpu_tex{ret.textures.emplace_back()};
    auto const tex_desc{
      CD3DX12_RESOURCE_DESC1::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, img.width,
                                    img.height)
    };
    if (FAILED(
      device_->CreateCommittedResource3(&default_heap_props,
        D3D12_HEAP_FLAG_NONE , & tex_desc, D3D12_BARRIER_LAYOUT_COPY_DEST,
        nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(&gpu_tex.res)))) {
      return std::unexpected{
        std::format("Failed to create GPU texture {}.", idx)
      };
    }

    std::memcpy(upload_buffer_ptr, img.bytes.get(), img.width * img.height * 4);

    if (FAILED(cmd_allocs_[frame_idx_]->Reset())) {
      return std::unexpected{
        "Failed to reset command allocator for texture copy."
      };
    }

    if (FAILED(
      cmd_lists_[frame_idx_]->Reset(cmd_allocs_[frame_idx_].Get(), nullptr))) {
      return std::unexpected{"Failed to reset command list for texture copy."};
    }

    D3D12_SUBRESOURCE_DATA const tex_data{
      img.bytes.get(), 4 * img.width, 4 * img.width * img.height
    };

    UpdateSubresources<1>(cmd_lists_[frame_idx_].Get(), gpu_tex.res.Get(),
                          upload_buffer.Get(), 0, 0, 1, &tex_data);

    D3D12_TEXTURE_BARRIER const barrier{
      D3D12_BARRIER_SYNC_COPY, D3D12_BARRIER_SYNC_NONE,
      D3D12_BARRIER_ACCESS_COPY_DEST, D3D12_BARRIER_ACCESS_NO_ACCESS,
      D3D12_BARRIER_LAYOUT_COPY_DEST,
      D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_SHADER_RESOURCE, gpu_tex.res.Get(),
      {0, 1, 0, 1, 0, 1}, D3D12_TEXTURE_BARRIER_FLAG_NONE
    };

    D3D12_BARRIER_GROUP const barrier_group{
      .Type = D3D12_BARRIER_TYPE_TEXTURE, .NumBarriers = 1,
      .pTextureBarriers = &barrier
    };

    cmd_lists_[frame_idx_]->Barrier(1, &barrier_group);

    if (FAILED(cmd_lists_[frame_idx_]->Close())) {
      return std::unexpected{"Failed to close command list for texture copy."};
    }

    direct_queue_->ExecuteCommandLists(
      1, CommandListCast(cmd_lists_[frame_idx_].GetAddressOf()));

    ++upload_fence_val;
    if (FAILED(direct_queue_->Signal(upload_fence.Get(), upload_fence_val))) {
      return std::unexpected{"Failed to signal upload fence."};
    }

    if (FAILED(upload_fence->SetEventOnCompletion(upload_fence_val, nullptr))) {
      return std::unexpected{"Failed to wait for upload fence."};
    }

    gpu_tex.srv_idx = AllocateResourceDescriptorIndex();

    D3D12_SHADER_RESOURCE_VIEW_DESC const srv_desc{
      .Format = tex_desc.Format, .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
      .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
      .Texture2D = {0, 1, 0, 0.0f}
    };

    device_->CreateShaderResourceView(gpu_tex.res.Get(), &srv_desc,
                                      CD3DX12_CPU_DESCRIPTOR_HANDLE{
                                        res_desc_heap_cpu_start_,
                                        static_cast<INT>(gpu_tex.srv_idx),
                                        res_desc_inc_
                                      });
  }

  return ret;
}

auto Renderer::DrawFrame() -> std::expected<void, std::string> {
  if (FAILED(cmd_allocs_[frame_idx_]->Reset())) {
    return std::unexpected{
      std::format("Failed to reset command allocator {}.", frame_idx_)
    };
  }

  if (FAILED(
    cmd_lists_[frame_idx_]->Reset(cmd_allocs_[frame_idx_].Get(), nullptr))) {
    return std::unexpected{
      std::format("Failed to reset command list {}.", frame_idx_)
    };
  }

  D3D12_TEXTURE_BARRIER const rt_barrier{
    D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_SYNC_RENDER_TARGET,
    D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_ACCESS_RENDER_TARGET,
    D3D12_BARRIER_LAYOUT_UNDEFINED, D3D12_BARRIER_LAYOUT_RENDER_TARGET,
    swap_chain_buffers_[frame_idx_].Get(), {0, 1, 0, 1, 0, 1},
    D3D12_TEXTURE_BARRIER_FLAG_NONE
  };

  D3D12_BARRIER_GROUP const rt_barrier_group{
    .Type = D3D12_BARRIER_TYPE_TEXTURE, .NumBarriers = 1,
    .pTextureBarriers = &rt_barrier
  };

  cmd_lists_[frame_idx_]->Barrier(1, &rt_barrier_group);

  cmd_lists_[frame_idx_]->ClearRenderTargetView(
    CD3DX12_CPU_DESCRIPTOR_HANDLE{rtv_heap_cpu_start_, frame_idx_, rtv_inc_},
    std::array{1.0f, 0.0f, 1.0f, 1.0f}.data(), 0, nullptr);

  D3D12_TEXTURE_BARRIER const present_barrier{
    D3D12_BARRIER_SYNC_RENDER_TARGET, D3D12_BARRIER_SYNC_NONE,
    D3D12_BARRIER_ACCESS_RENDER_TARGET, D3D12_BARRIER_ACCESS_NO_ACCESS,
    D3D12_BARRIER_LAYOUT_RENDER_TARGET, D3D12_BARRIER_LAYOUT_PRESENT,
    swap_chain_buffers_[frame_idx_].Get(), {0, 1, 0, 1, 0, 1},
    D3D12_TEXTURE_BARRIER_FLAG_NONE
  };

  D3D12_BARRIER_GROUP const present_barrier_group{
    .Type = D3D12_BARRIER_TYPE_TEXTURE, .NumBarriers = 1,
    .pTextureBarriers = &present_barrier
  };

  cmd_lists_[frame_idx_]->Barrier(1, &present_barrier_group);

  if (FAILED(cmd_lists_[frame_idx_]->Close())) {
    return std::unexpected{
      std::format("Failed to close command list {}.", frame_idx_)
    };
  }

  direct_queue_->ExecuteCommandLists(
    1, CommandListCast(cmd_lists_[frame_idx_].GetAddressOf()));

  if (FAILED(swap_chain_->Present(0, present_flags_))) {
    return std::unexpected{"Failed to present."};
  }

  frame_idx_ = (frame_idx_ + 1) % max_frames_in_flight_;

  frame_fence_val_ += 1;
  if (FAILED(direct_queue_->Signal(frame_fence_.Get(), frame_fence_val_))) {
    return std::unexpected{"Failed to signal frame fence."};
  }

  if (FAILED(
    frame_fence_->SetEventOnCompletion(sat_sub<UINT64>(frame_fence_val_,
      max_gpu_queued_frames_), nullptr))) {
    return std::unexpected{"Failed to wait for frame fence."};
  }

  return {};
}

Renderer::Renderer(ComPtr<IDXGIFactory7> factory, ComPtr<ID3D12Device10> device,
                   ComPtr<ID3D12CommandQueue> direct_queue,
                   ComPtr<IDXGISwapChain4> swap_chain,
                   std::array<ComPtr<ID3D12Resource>, swap_chain_buffer_count_>
                   swap_chain_buffers, ComPtr<ID3D12DescriptorHeap> rtv_heap,
                   ComPtr<ID3D12DescriptorHeap> res_desc_heap,
                   std::array<ComPtr<ID3D12CommandAllocator>,
                              max_frames_in_flight_> cmd_allocs,
                   std::array<ComPtr<ID3D12GraphicsCommandList7>,
                              max_frames_in_flight_> cmd_lists,
                   ComPtr<ID3D12Fence> frame_fence, UINT const swap_chain_flags,
                   UINT const present_flags) :
  factory_{std::move(factory)}, device_{std::move(device)},
  direct_queue_{std::move(direct_queue)}, swap_chain_{std::move(swap_chain)},
  swap_chain_buffers_{std::move(swap_chain_buffers)},
  rtv_heap_{std::move(rtv_heap)}, res_desc_heap_{std::move(res_desc_heap)},
  cmd_allocs_{std::move(cmd_allocs)}, cmd_lists_{std::move(cmd_lists)},
  frame_fence_{std::move(frame_fence)},
  rtv_inc_{
    device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
  },
  res_desc_inc_{
    device_->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
  }, rtv_heap_cpu_start_{rtv_heap_->GetCPUDescriptorHandleForHeapStart()},
  res_desc_heap_cpu_start_{
    res_desc_heap_->GetCPUDescriptorHandleForHeapStart()
  }, frame_fence_val_{frame_fence_->GetCompletedValue()},
  swap_chain_flags_{swap_chain_flags}, present_flags_{present_flags} {
  res_desc_heap_free_indices_.resize(res_desc_heap_size_);
  for (UINT i{0}; i < res_desc_heap_size_; i++) {
    res_desc_heap_free_indices_[i] = i;
  }
}

auto Renderer::AllocateResourceDescriptorIndex() -> UINT {
  /* This crashes SRV creation
  auto const ret{next_free_res_desc_idx_};
  next_free_res_desc_idx_ = *std::bit_cast<UINT*>(CD3DX12_CPU_DESCRIPTOR_HANDLE{
    res_desc_heap_cpu_start_, static_cast<INT>(next_free_res_desc_idx_),
    rtv_inc_
  }.ptr);
  return ret;*/
  auto const ret{res_desc_heap_free_indices_.back()};
  res_desc_heap_free_indices_.pop_back();
  return ret;
}

auto Renderer::FreeResourceDescriptorIndex(UINT const idx) -> void {
  /* This crashes SRV creation
  *std::bit_cast<UINT*>(CD3DX12_CPU_DESCRIPTOR_HANDLE{
    res_desc_heap_cpu_start_, static_cast<INT>(idx), rtv_inc_
  }) = next_free_res_desc_idx_;
  next_free_res_desc_idx_ = idx;*/
  res_desc_heap_free_indices_.push_back(idx);
}
}
