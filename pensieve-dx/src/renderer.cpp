#include "renderer.hpp"

#include <algorithm>
//#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <ranges>
#include <stdlib.h>
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

  if (features.HighestRootSignatureVersion() < D3D_ROOT_SIGNATURE_VERSION_1_1) {
    return std::unexpected{"GPU does not support root signature 1.1."};
  }

  if (features.MeshShaderTier() < D3D12_MESH_SHADER_TIER_1) {
    return std::unexpected{"GPU does not support mesh shader tier 1."};
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

  RECT client_rect;
  if (FAILED(GetClientRect(hwnd, &client_rect))) {
    return std::unexpected{"Failed to retrieve window client area dimensions."};
  }

  auto const client_width{
    static_cast<UINT>(client_rect.right - client_rect.left)
  };
  auto const client_height{
    static_cast<UINT>(client_rect.bottom - client_rect.top)
  };

  DXGI_SWAP_CHAIN_DESC1 const swap_chain_desc{
    client_width, client_height, swap_chain_format_, FALSE, {1, 0},
    DXGI_USAGE_RENDER_TARGET_OUTPUT, swap_chain_buffer_count_,
    DXGI_SCALING_STRETCH, DXGI_SWAP_EFFECT_FLIP_DISCARD,
    DXGI_ALPHA_MODE_UNSPECIFIED, swap_chain_flags
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

  std::array<ComPtr<ID3D12Resource2>, swap_chain_buffer_count_>
    swap_chain_buffers;
  if (auto const exp{
    RetrieveSwapChainBuffers(swap_chain.Get(), swap_chain_buffers)
  }; !exp) {
    return std::unexpected{exp.error()};
  }

  ComPtr<ID3D12Resource2> depth_buffer;
  if (auto const exp{
    CreateDepthBuffer(device.Get(), depth_buffer, client_width, client_height)
  }; !exp) {
    return std::unexpected{exp.error()};
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

  D3D12_DESCRIPTOR_HEAP_DESC constexpr dsv_heap_desc{
    D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0
  };

  ComPtr<ID3D12DescriptorHeap> dsv_heap;
  if (FAILED(
    device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&dsv_heap)))) {
    return std::unexpected{"Failed to create DSV heap."};
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

  CD3DX12_ROOT_PARAMETER1 root_param;
  root_param.InitAsConstantBufferView(
    0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE,
    D3D12_SHADER_VISIBILITY_ALL);
  CD3DX12_STATIC_SAMPLER_DESC const sampler_desc{0};

  D3D12_VERSIONED_ROOT_SIGNATURE_DESC const root_sig_desc{
    .Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
    .Desc_1_1 = {
      1, &root_param, 1, &sampler_desc,
      D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED
    }
  };

  ComPtr<ID3DBlob> root_sig_blob;

  if (ComPtr<ID3DBlob> root_sig_err_blob; FAILED(
    D3D12SerializeVersionedRootSignature(&root_sig_desc, &root_sig_blob, &
      root_sig_err_blob))) {
    return std::unexpected{
      std::string{
        static_cast<char*>(root_sig_err_blob->GetBufferPointer()),
        root_sig_err_blob->GetBufferSize()
      }
    };
  }

  ComPtr<ID3D12RootSignature> root_sig;
  if (FAILED(
    device->CreateRootSignature(0, root_sig_blob->GetBufferPointer(),
      root_sig_blob->GetBufferSize(), IID_PPV_ARGS(&root_sig)))) {
    return std::unexpected{"Failed to create root signature."};
  }

  char* exe_path_str;

  if (_get_pgmptr(&exe_path_str)) {
    return std::unexpected{"Failed to retrieve executable path."};
  }

  std::filesystem::path const exe_path{exe_path_str};

  std::ifstream ms_file{
    exe_path.parent_path() / "mesh_shader.cso", std::ios::in | std::ios::binary
  };

  if (!ms_file.is_open()) {
    return std::unexpected{"Failed to load mesh shader file."};
  }

  std::vector<std::uint8_t> const ms_bytes{
    std::istreambuf_iterator{ms_file}, {}
  };

  std::ifstream ps_file{
    exe_path.parent_path() / "pixel_shader.cso", std::ios::in | std::ios::binary
  };

  if (!ps_file.is_open()) {
    return std::unexpected{"Failed to load pixel shader file."};
  }

  std::vector<std::uint8_t> const ps_bytes{
    std::istreambuf_iterator{ps_file}, {}
  };

  struct {
    CD3DX12_PIPELINE_STATE_STREAM_MS ms;
    CD3DX12_PIPELINE_STATE_STREAM_PS ps;
    CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE root_sig;
    CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rt_formats;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL2 ds;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT ds_format;
  } pso_desc;

  pso_desc.ms = D3D12_SHADER_BYTECODE{ms_bytes.data(), ms_bytes.size()};
  pso_desc.ps = D3D12_SHADER_BYTECODE{ps_bytes.data(), ps_bytes.size()};
  pso_desc.root_sig = root_sig.Get();
  pso_desc.rt_formats = D3D12_RT_FORMAT_ARRAY{{swap_chain_format_}, 1};
  pso_desc.ds = CD3DX12_DEPTH_STENCIL_DESC2{
    TRUE, D3D12_DEPTH_WRITE_MASK_ALL, D3D12_COMPARISON_FUNC_GREATER, FALSE, {},
    {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}
  };
  pso_desc.ds_format = depth_buffer_format_;

  D3D12_PIPELINE_STATE_STREAM_DESC const pso_stream_desc{
    sizeof(pso_desc), &pso_desc
  };

  ComPtr<ID3D12PipelineState> pso;
  if (FAILED(
    device->CreatePipelineState(&pso_stream_desc, IID_PPV_ARGS(&pso)))) {
    return std::unexpected{"Failed to create pipeline state object."};
  }

  return Renderer{
    std::move(factory), std::move(device), std::move(direct_queue),
    std::move(swap_chain), std::move(swap_chain_buffers),
    std::move(depth_buffer), std::move(rtv_heap), std::move(dsv_heap),
    std::move(res_desc_heap), std::move(cmd_allocs), std::move(cmd_lists),
    std::move(frame_fence), std::move(root_sig), std::move(pso),
    swap_chain_flags, present_flags
  };
}

auto Renderer::CreateGpuScene(
  SceneData const& scene_data) -> std::expected<GpuScene, std::string> {
  // Make sure this is big enough to hold any single resource.
  auto constexpr upload_buffer_size{1'000'000'000};

  auto const res_desc_heap_cpu_start{
    res_desc_heap_->GetCPUDescriptorHandleForHeapStart()
  };

  auto const res_desc_inc{
    device_->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
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

  GpuScene gpu_scene;
  gpu_scene.textures.reserve(scene_data.textures.size());
  gpu_scene.materials.reserve(scene_data.materials.size());
  gpu_scene.meshes.reserve(scene_data.meshes.size());

  for (auto const& [idx, img] : std::ranges::views::enumerate(
         scene_data.textures)) {
    auto& gpu_tex{gpu_scene.textures.emplace_back()};
    auto const tex_desc{
      CD3DX12_RESOURCE_DESC1::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, img.width,
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
                                        res_desc_heap_cpu_start,
                                        static_cast<INT>(gpu_tex.srv_idx),
                                        res_desc_inc
                                      });
  }

  auto const create_buffer_from_upload_data{
    [this, &default_heap_props, &upload_buffer, &upload_fence_val, &upload_fence
    ](std::size_t const buf_size,
      ComPtr<ID3D12Resource2>& buf) -> std::expected<void, std::string> {
      auto const buf_desc{CD3DX12_RESOURCE_DESC1::Buffer(buf_size)};

      if FAILED(
        device_->CreateCommittedResource3(&default_heap_props,
          D3D12_HEAP_FLAG_NONE, &buf_desc, D3D12_BARRIER_LAYOUT_UNDEFINED,
          nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(&buf))) {
        return std::unexpected{"Failed to create buffer."};
      }

      if (FAILED(cmd_allocs_[frame_idx_]->Reset())) {
        return std::unexpected{"Failed to reset command allocator."};
      }

      if (FAILED(
        cmd_lists_[frame_idx_]->Reset(cmd_allocs_[frame_idx_].Get(), nullptr
        ))) {
        return std::unexpected{"Failed to reset command list."};
      }

      cmd_lists_[frame_idx_]->CopyBufferRegion(buf.Get(), 0,
                                               upload_buffer.Get(), 0,
                                               buf_desc.Width);

      if (FAILED(cmd_lists_[frame_idx_]->Close())) {
        return std::unexpected{"Failed to close command list."};
      }

      direct_queue_->ExecuteCommandLists(
        1, CommandListCast(cmd_lists_[frame_idx_].GetAddressOf()));

      ++upload_fence_val;
      if (FAILED(direct_queue_->Signal(upload_fence.Get(), upload_fence_val))) {
        return std::unexpected{"Failed to signal upload fence."};
      }

      if (FAILED(
        upload_fence->SetEventOnCompletion(upload_fence_val, nullptr))) {
        return std::unexpected{"Failed to wait for upload fence."};
      }

      return {};
    }
  };

  auto constexpr mtl_buffer_size{
    std::max(NextMultipleOf<UINT64>(256, sizeof(Material)),
             NextMultipleOf<UINT64>(256, sizeof(DrawData)))
  };

  for (auto const& [idx, mtl_data] : std::ranges::views::enumerate(
         scene_data.materials)) {
    auto& gpu_mtl{gpu_scene.materials.emplace_back()};

    Material const mtl{
      DirectX::XMFLOAT3{mtl_data.base_color.data()}, mtl_data.metallic,
      mtl_data.roughness, DirectX::XMFLOAT3{mtl_data.emission_color.data()},
      mtl_data.base_color_map_idx
        ? gpu_scene.textures[*mtl_data.base_color_map_idx].srv_idx
        : INVALID_RESOURCE_IDX,
      mtl_data.metallic_map_idx
        ? gpu_scene.textures[*mtl_data.metallic_map_idx].srv_idx
        : INVALID_RESOURCE_IDX,
      mtl_data.roughness_map_idx
        ? gpu_scene.textures[*mtl_data.roughness_map_idx].srv_idx
        : INVALID_RESOURCE_IDX,
      mtl_data.emission_map_idx
        ? gpu_scene.textures[*mtl_data.emission_map_idx].srv_idx
        : INVALID_RESOURCE_IDX,
      mtl_data.normal_map_idx
        ? gpu_scene.textures[*mtl_data.normal_map_idx].srv_idx
        : INVALID_RESOURCE_IDX
    };
    std::memcpy(upload_buffer_ptr, &mtl, sizeof(mtl));

    if (auto const exp{
      create_buffer_from_upload_data(mtl_buffer_size, gpu_mtl.res)
    }; !exp) {
      return std::unexpected{
        std::format("Failed to create material buffer {}: {}", idx, exp.error())
      };
    }

    gpu_mtl.cbv_idx = AllocateResourceDescriptorIndex();

    D3D12_CONSTANT_BUFFER_VIEW_DESC const cbv_desc{
      gpu_mtl.res->GetGPUVirtualAddress(), static_cast<UINT>(mtl_buffer_size)
    };

    device_->CreateConstantBufferView(&cbv_desc, CD3DX12_CPU_DESCRIPTOR_HANDLE{
                                        res_desc_heap_cpu_start,
                                        static_cast<INT>(gpu_mtl.cbv_idx),
                                        res_desc_inc
                                      });
  }

  std::vector<std::vector<InstanceBufferData>> instance_transforms_per_mesh;
  instance_transforms_per_mesh.resize(scene_data.meshes.size());

  for (auto const& node : scene_data.nodes) {
    for (auto const mesh_idx : node.mesh_indices) {
      DirectX::XMFLOAT4X4 const model_mtx{node.transform.data()};
      DirectX::XMFLOAT4X4 normal_mtx;
      XMStoreFloat4x4(&normal_mtx,
                      XMMatrixTranspose(
                        XMMatrixInverse(nullptr, XMLoadFloat4x4(&model_mtx))));
      instance_transforms_per_mesh[mesh_idx].
        emplace_back(model_mtx, normal_mtx);
    }
  }

  for (std::size_t idx{0}; idx < scene_data.meshes.size(); idx++) {
    auto const& mesh_data{scene_data.meshes[idx]};
    auto& gpu_mesh{gpu_scene.meshes.emplace_back()};

    auto const create_buffer_srv{
      [this, res_desc_heap_cpu_start, res_desc_inc](UINT const element_count,
                                                    UINT const element_stride,
                                                    ID3D12Resource2* const buf,
                                                    UINT& srv_idx) {
        srv_idx = AllocateResourceDescriptorIndex();

        D3D12_SHADER_RESOURCE_VIEW_DESC const srv_desc{
          .Format = DXGI_FORMAT_UNKNOWN,
          .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
          .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
          .Buffer = {
            0, static_cast<UINT>(element_count),
            static_cast<UINT>(element_stride), D3D12_BUFFER_SRV_FLAG_NONE
          }
        };

        device_->CreateShaderResourceView(buf, &srv_desc,
                                          CD3DX12_CPU_DESCRIPTOR_HANDLE{
                                            res_desc_heap_cpu_start,
                                            static_cast<INT>(srv_idx),
                                            res_desc_inc
                                          });
      }
    };

    {
      auto const pos_count{mesh_data.positions.size()};
      auto constexpr pos_stride{sizeof(DirectX::XMFLOAT4)};
      auto const pos_buf_size{pos_count * pos_stride};

      std::memcpy(upload_buffer_ptr, mesh_data.positions.data(), pos_buf_size);

      if (auto const exp{
        create_buffer_from_upload_data(pos_buf_size, gpu_mesh.pos_buf)
      }; !exp) {
        return std::unexpected{
          std::format("Failed to create mesh {} position buffer: {}", idx,
                      exp.error())
        };
      }

      create_buffer_srv(static_cast<UINT>(pos_count),
                        static_cast<UINT>(pos_stride), gpu_mesh.pos_buf.Get(),
                        gpu_mesh.pos_buf_srv_idx);
    }

    {
      auto const norm_count{mesh_data.normals.size()};
      auto constexpr norm_stride{sizeof(DirectX::XMFLOAT4)};
      auto const norm_buf_size{norm_count * norm_stride};

      std::memcpy(upload_buffer_ptr, mesh_data.normals.data(), norm_buf_size);

      if (auto const exp{
        create_buffer_from_upload_data(norm_buf_size, gpu_mesh.norm_buf)
      }; !exp) {
        return std::unexpected{
          std::format("Failed to create mesh {} normal buffer: {}", idx,
                      exp.error())
        };
      }

      create_buffer_srv(static_cast<UINT>(norm_count),
                        static_cast<UINT>(norm_stride), gpu_mesh.norm_buf.Get(),
                        gpu_mesh.norm_buf_srv_idx);
    }

    {
      auto const tan_count{mesh_data.tangents.size()};
      auto constexpr tan_stride{sizeof(DirectX::XMFLOAT4)};
      auto const tan_buf_size{tan_count * tan_stride};

      std::memcpy(upload_buffer_ptr, mesh_data.tangents.data(), tan_buf_size);


      if (auto const exp{
        create_buffer_from_upload_data(tan_buf_size, gpu_mesh.tan_buf)
      }; !exp) {
        return std::unexpected{
          std::format("Failed to create mesh {} tangent buffer: {}", idx,
                      exp.error())
        };
      }

      create_buffer_srv(static_cast<UINT>(tan_count),
                        static_cast<UINT>(tan_stride), gpu_mesh.tan_buf.Get(),
                        gpu_mesh.tan_buf_srv_idx);
    }

    if (mesh_data.uvs) {
      auto const uv_count{mesh_data.uvs->size()};
      auto constexpr uv_stride{sizeof(DirectX::XMFLOAT2)};
      auto const uv_buf_size{uv_count * uv_stride};
      std::memcpy(upload_buffer_ptr, mesh_data.uvs->data(), uv_buf_size);

      if (auto const exp{
        create_buffer_from_upload_data(uv_buf_size, gpu_mesh.uv_buf.emplace())
      }; !exp) {
        return std::unexpected{
          std::format("Failed to create mesh {} uv buffer: {}", idx,
                      exp.error())
        };
      }

      create_buffer_srv(static_cast<UINT>(uv_count),
                        static_cast<UINT>(uv_stride), gpu_mesh.uv_buf->Get(),
                        gpu_mesh.uv_buf_srv_idx.emplace());
    }

    {
      {
        auto const meshlet_count{mesh_data.meshlets.size()};
        auto constexpr meshlet_stride{
          sizeof(decltype(mesh_data.meshlets)::value_type)
        };
        auto const meshlet_buf_size{meshlet_count * meshlet_stride};

        std::memcpy(upload_buffer_ptr, mesh_data.meshlets.data(),
                    meshlet_buf_size);

        if (auto const exp{
          create_buffer_from_upload_data(meshlet_buf_size, gpu_mesh.meshlet_buf)
        }; !exp) {
          return std::unexpected{
            std::format("Failed to create mesh {} meshlet buffer: {}", idx,
                        exp.error())
          };
        }

        create_buffer_srv(static_cast<UINT>(meshlet_count),
                          static_cast<UINT>(meshlet_stride),
                          gpu_mesh.meshlet_buf.Get(),
                          gpu_mesh.meshlet_buf_srv_idx);
      }

      {
        auto const vertex_idx_count{mesh_data.vertex_indices.size() / 4};
        auto constexpr vertex_idx_stride{sizeof(std::uint32_t)};
        auto const vertex_idx_buf_size{vertex_idx_count * vertex_idx_stride};

        std::memcpy(upload_buffer_ptr, mesh_data.vertex_indices.data(),
                    vertex_idx_buf_size);

        if (auto const exp{
          create_buffer_from_upload_data(vertex_idx_buf_size,
                                         gpu_mesh.vertex_idx_buf)
        }; !exp) {
          return std::unexpected{
            std::format("Failed to create mesh {} vertex index buffer: {}", idx,
                        exp.error())
          };
        }

        create_buffer_srv(static_cast<UINT>(vertex_idx_count),
                          static_cast<UINT>(vertex_idx_stride),
                          gpu_mesh.vertex_idx_buf.Get(),
                          gpu_mesh.vertex_idx_buf_srv_idx);
      }

      {
        auto const prim_idx_count{mesh_data.triangle_indices.size()};
        auto constexpr prim_idx_stride{sizeof(MeshletTriangleIndexData)};
        auto const prim_idx_buf_size{prim_idx_count * prim_idx_stride};

        std::memcpy(upload_buffer_ptr, mesh_data.triangle_indices.data(),
                    prim_idx_buf_size);

        if (auto const exp{
          create_buffer_from_upload_data(prim_idx_buf_size,
                                         gpu_mesh.prim_idx_buf)
        }; !exp) {
          return std::unexpected{
            std::format("Failed to create mesh {} primitive index buffer: {}",
                        idx, exp.error())
          };
        }

        create_buffer_srv(static_cast<UINT>(prim_idx_count),
                          static_cast<UINT>(prim_idx_stride),
                          gpu_mesh.prim_idx_buf.Get(),
                          gpu_mesh.prim_idx_buf_srv_idx);
      }

      gpu_mesh.meshlet_count = static_cast<UINT>(mesh_data.meshlets.size());
    }

    gpu_mesh.mtl_idx = mesh_data.material_idx;

    auto const instance_count{instance_transforms_per_mesh[idx].size()};
    auto constexpr instance_data_stride{sizeof(InstanceBufferData)};

    std::memcpy(upload_buffer_ptr, instance_transforms_per_mesh[idx].data(),
                instance_count * instance_data_stride);

    if (auto const exp{
      create_buffer_from_upload_data(instance_count * instance_data_stride,
                                     gpu_mesh.inst_buf)
    }; !exp) {
      return std::unexpected{exp.error()};
    }

    create_buffer_srv(static_cast<UINT>(instance_count),
                      static_cast<UINT>(instance_data_stride),
                      gpu_mesh.inst_buf.Get(), gpu_mesh.inst_buf_srv_idx);

    gpu_mesh.instance_count = static_cast<UINT>(instance_count);

    auto const draw_data_desc{
      CD3DX12_RESOURCE_DESC1::Buffer(
        NextMultipleOf<UINT64>(256, sizeof(DrawData)))
    };

    if (FAILED(
      device_->CreateCommittedResource3(&upload_heap_props, D3D12_HEAP_FLAG_NONE
        , &draw_data_desc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, nullptr, 0,
        nullptr, IID_PPV_ARGS(&gpu_mesh.draw_data_buf)))) {
      return std::unexpected{
        std::format("Failed to create mesh {} draw data buffer.", idx)
      };
    }

    if (FAILED(
      gpu_mesh.draw_data_buf->Map(0, nullptr, &gpu_mesh.mapped_draw_data_buf
      ))) {
      return std::unexpected{
        std::format("Failed to map mesh {} draw data buffer.", idx)
      };
    }
  }

  return gpu_scene;
}

auto Renderer::DrawFrame(GpuScene const& scene,
                         Camera const& cam) -> std::expected<
  void, std::string> {
  auto const back_buf_idx{swap_chain_->GetCurrentBackBufferIndex()};
  auto const back_buf_desc{swap_chain_buffers_[back_buf_idx]->GetDesc1()};
  auto const aspect_ratio{
    static_cast<float>(back_buf_desc.Width) / static_cast<float>(back_buf_desc.
      Height)
  };

  auto const view_mtx{
    DirectX::XMMatrixLookAtLH(DirectX::XMVectorNegativeMultiplySubtract(
                                DirectX::XMVector3Rotate(
                                  DirectX::XMVectorSet(0, 0, 1, 0),
                                  XMLoadFloat4(&cam.rotation)),
                                DirectX::XMVectorReplicate(cam.distance),
                                XMLoadFloat3(&cam.center)),
                              XMLoadFloat3(&cam.center),
                              DirectX::XMVectorSet(0, 1, 0, 0))
  };
  auto const proj_mtx{
    DirectX::XMMatrixPerspectiveFovLH(
      DirectX::XMConvertToRadians(cam.vertical_degrees_fov), aspect_ratio,
      cam.far_clip_plane, cam.near_clip_plane)
  };
  auto const view_proj_mtx{XMMatrixMultiply(view_mtx, proj_mtx)};

  if (FAILED(cmd_allocs_[frame_idx_]->Reset())) {
    return std::unexpected{
      std::format("Failed to reset command allocator {}.", frame_idx_)
    };
  }

  if (FAILED(
    cmd_lists_[frame_idx_]->Reset(cmd_allocs_[frame_idx_].Get(), pso_.Get()))) {
    return std::unexpected{
      std::format("Failed to reset command list {}.", frame_idx_)
    };
  }

  std::array const rt_barriers{
    D3D12_TEXTURE_BARRIER{
      D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_SYNC_RENDER_TARGET,
      D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_ACCESS_RENDER_TARGET,
      D3D12_BARRIER_LAYOUT_UNDEFINED, D3D12_BARRIER_LAYOUT_RENDER_TARGET,
      swap_chain_buffers_[back_buf_idx].Get(), {0, 1, 0, 1, 0, 1},
      D3D12_TEXTURE_BARRIER_FLAG_NONE
    },
    D3D12_TEXTURE_BARRIER{
      D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_SYNC_DEPTH_STENCIL,
      D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
      D3D12_BARRIER_LAYOUT_UNDEFINED, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE,
      depth_buffer_.Get(), {0, 1, 0, 1, 0, 1}, D3D12_TEXTURE_BARRIER_FLAG_NONE
    },
  };

  D3D12_BARRIER_GROUP const rt_barrier_group{
    .Type = D3D12_BARRIER_TYPE_TEXTURE,
    .NumBarriers = static_cast<UINT32>(rt_barriers.size()),
    .pTextureBarriers = rt_barriers.data()
  };

  cmd_lists_[frame_idx_]->Barrier(1, &rt_barrier_group);

  cmd_lists_[frame_idx_]->OMSetRenderTargets(1, &rtv_cpu_handles_[back_buf_idx],
                                             TRUE, &dsv_cpu_handle_);
  cmd_lists_[frame_idx_]->SetDescriptorHeaps(1, res_desc_heap_.GetAddressOf());
  cmd_lists_[frame_idx_]->SetGraphicsRootSignature(root_sig_.Get());
  cmd_lists_[frame_idx_]->IASetPrimitiveTopology(
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  CD3DX12_VIEWPORT const viewport{
    0.0f, 0.0f, static_cast<FLOAT>(back_buf_desc.Width),
    static_cast<FLOAT>(back_buf_desc.Height)
  };
  CD3DX12_RECT const scissor{
    0, 0, static_cast<LONG>(back_buf_desc.Width),
    static_cast<LONG>(back_buf_desc.Height)
  };

  cmd_lists_[frame_idx_]->RSSetViewports(1, &viewport);
  cmd_lists_[frame_idx_]->RSSetScissorRects(1, &scissor);

  cmd_lists_[frame_idx_]->ClearRenderTargetView(rtv_cpu_handles_[back_buf_idx],
                                                std::array{
                                                  0.0f, 0.0f, 0.0f, 1.0f
                                                }.data(), 0, nullptr);

  cmd_lists_[frame_idx_]->ClearDepthStencilView(
    dsv_cpu_handle_, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);

  for (auto const& mesh : scene.meshes) {
    DrawData draw_data{
      .pos_buf_idx = mesh.pos_buf_srv_idx,
      .norm_buf_idx = mesh.norm_buf_srv_idx,
      .tan_buf_idx = mesh.tan_buf_srv_idx,
      .uv_buf_idx = mesh.uv_buf_srv_idx
                      ? *mesh.uv_buf_srv_idx
                      : INVALID_RESOURCE_IDX,
      .vertex_idx_buf_idx = mesh.vertex_idx_buf_srv_idx,
      .prim_idx_buf_idx = mesh.prim_idx_buf_srv_idx,
      .meshlet_buf_idx = mesh.meshlet_buf_srv_idx,
      .mtl_buf_idx = scene.materials[mesh.mtl_idx].cbv_idx,
      .inst_buf_idx = mesh.inst_buf_srv_idx, .inst_count = mesh.instance_count
    };

    XMStoreFloat4x4(&draw_data.view_proj_mtx, view_proj_mtx);

    std::memcpy(mesh.mapped_draw_data_buf, &draw_data, sizeof(draw_data));

    cmd_lists_[frame_idx_]->SetGraphicsRootConstantBufferView(
      0, mesh.draw_data_buf->GetGPUVirtualAddress());
    cmd_lists_[frame_idx_]->DispatchMesh(
      mesh.meshlet_count * mesh.instance_count, 1, 1);
  }

  D3D12_TEXTURE_BARRIER const present_barrier{
    D3D12_BARRIER_SYNC_RENDER_TARGET, D3D12_BARRIER_SYNC_NONE,
    D3D12_BARRIER_ACCESS_RENDER_TARGET, D3D12_BARRIER_ACCESS_NO_ACCESS,
    D3D12_BARRIER_LAYOUT_RENDER_TARGET, D3D12_BARRIER_LAYOUT_PRESENT,
    swap_chain_buffers_[back_buf_idx].Get(), {0, 1, 0, 1, 0, 1},
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
    frame_fence_->SetEventOnCompletion(SatSub<UINT64>(frame_fence_val_,
      max_gpu_queued_frames_), nullptr))) {
    return std::unexpected{"Failed to wait for frame fence."};
  }

  return {};
}

auto Renderer::WaitForDeviceIdle() const -> std::expected<void, std::string> {
  auto constexpr initial_value{0};
  auto constexpr completed_value{initial_value + 1};

  ComPtr<ID3D12Fence> fence;
  if (FAILED(
    device_->CreateFence(initial_value, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&
      fence)))) {
    return std::unexpected{"Failed to create device idle fence."};
  }

  if (FAILED(direct_queue_->Signal(fence.Get(), completed_value))) {
    return std::unexpected{"Failed to signal device idle fence."};
  }

  if (FAILED(fence->SetEventOnCompletion(completed_value, nullptr))) {
    return std::unexpected{"Failed to wait for device idle fence."};
  }

  return {};
}

auto Renderer::ResizeRenderTargets() -> std::expected<void, std::string> {
  if (auto const exp{WaitForDeviceIdle()}; !exp) {
    return exp;
  }

  for (auto& buf : swap_chain_buffers_) {
    buf.Reset();
  }

  depth_buffer_.Reset();

  if (FAILED(
    swap_chain_->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, swap_chain_flags_
    ))) {
    return std::unexpected{"Failed to resize swap chain buffers."};
  }

  if (auto const exp{
    RetrieveSwapChainBuffers(swap_chain_.Get(), swap_chain_buffers_)
  }; !exp) {
    return exp;
  }

  auto const swap_chain_buffer_desc{swap_chain_buffers_[0]->GetDesc1()};

  if (auto const exp{
    CreateDepthBuffer(device_.Get(), depth_buffer_,
                      static_cast<unsigned>(swap_chain_buffer_desc.Width),
                      static_cast<unsigned>(swap_chain_buffer_desc.Height))
  }; !exp) {
    return exp;
  }

  CreateSwapChainRtvs();
  CreateDepthBufferDsv();
  return {};
}

Renderer::Renderer(ComPtr<IDXGIFactory7> factory, ComPtr<ID3D12Device10> device,
                   ComPtr<ID3D12CommandQueue> direct_queue,
                   ComPtr<IDXGISwapChain4> swap_chain,
                   std::array<ComPtr<ID3D12Resource2>, swap_chain_buffer_count_>
                   swap_chain_buffers, ComPtr<ID3D12Resource2> depth_buffer,
                   ComPtr<ID3D12DescriptorHeap> rtv_heap,
                   ComPtr<ID3D12DescriptorHeap> dsv_heap,
                   ComPtr<ID3D12DescriptorHeap> res_desc_heap,
                   std::array<ComPtr<ID3D12CommandAllocator>,
                              max_frames_in_flight_> cmd_allocs,
                   std::array<ComPtr<ID3D12GraphicsCommandList7>,
                              max_frames_in_flight_> cmd_lists,
                   ComPtr<ID3D12Fence> frame_fence,
                   ComPtr<ID3D12RootSignature> root_sig,
                   ComPtr<ID3D12PipelineState> pso, UINT const swap_chain_flags,
                   UINT const present_flags) :
  factory_{std::move(factory)}, device_{std::move(device)},
  direct_queue_{std::move(direct_queue)}, swap_chain_{std::move(swap_chain)},
  swap_chain_buffers_{std::move(swap_chain_buffers)},
  depth_buffer_{std::move(depth_buffer)}, rtv_heap_{std::move(rtv_heap)},
  dsv_heap_{std::move(dsv_heap)}, res_desc_heap_{std::move(res_desc_heap)},
  cmd_allocs_{std::move(cmd_allocs)}, cmd_lists_{std::move(cmd_lists)},
  frame_fence_{std::move(frame_fence)}, root_sig_{std::move(root_sig)},
  pso_{std::move(pso)},
  dsv_cpu_handle_{
    CD3DX12_CPU_DESCRIPTOR_HANDLE{
      dsv_heap_->GetCPUDescriptorHandleForHeapStart(), 0,
      device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
    }
  }, frame_fence_val_{frame_fence_->GetCompletedValue()},
  swap_chain_flags_{swap_chain_flags}, present_flags_{present_flags} {
  auto const rtv_heap_cpu_start{
    rtv_heap_->GetCPUDescriptorHandleForHeapStart()
  };
  auto const rtv_inc{
    device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
  };

  for (std::size_t i{0}; i < rtv_cpu_handles_.size(); i++) {
    rtv_cpu_handles_[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE{
      rtv_heap_cpu_start, static_cast<INT>(i), rtv_inc
    };
  }

  res_desc_heap_free_indices_.resize(res_desc_heap_size_);
  for (UINT i{0}; i < res_desc_heap_size_; i++) {
    res_desc_heap_free_indices_[i] = i;
  }

  CreateSwapChainRtvs();
  CreateDepthBufferDsv();
}

auto Renderer::RetrieveSwapChainBuffers(IDXGISwapChain4* const swap_chain,
                                        std::span<
                                          ComPtr<ID3D12Resource2>,
                                          swap_chain_buffer_count_> buffers) ->
  std::expected<void, std::string> {
  for (UINT i{0}; i < swap_chain_buffer_count_; i++) {
    if (FAILED(swap_chain->GetBuffer(i, IID_PPV_ARGS(&buffers[i])))) {
      return std::unexpected{
        std::format("Failed to get swap chain buffer {}.", i)
      };
    }
  }
  return {};
}

auto Renderer::CreateDepthBuffer(ID3D12Device10* const device,
                                 ComPtr<ID3D12Resource2>& depth_buffer,
                                 unsigned const width,
                                 unsigned const height) -> std::expected<
  void, std::string> {
  CD3DX12_HEAP_PROPERTIES const default_heap_props{D3D12_HEAP_TYPE_DEFAULT};

  auto const depth_buf_desc{
    CD3DX12_RESOURCE_DESC1::Tex2D(depth_buffer_format_, width, height, 1, 1, 1,
                                  0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL |
                                  D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)
  };

  CD3DX12_CLEAR_VALUE const depth_buf_clear_value{
    depth_buf_desc.Format, 0.0f, 0
  };

  if (FAILED(
    device->CreateCommittedResource3(&default_heap_props, D3D12_HEAP_FLAG_NONE ,
      &depth_buf_desc, D3D12_BARRIER_LAYOUT_COMMON, &depth_buf_clear_value,
      nullptr, 0, nullptr, IID_PPV_ARGS(&depth_buffer)))) {
    return std::unexpected{"Failed to create depth buffer."};
  }

  return {};
}

auto Renderer::CreateSwapChainRtvs() const -> void {
  for (auto i{0}; i < swap_chain_buffer_count_; i++) {
    D3D12_RENDER_TARGET_VIEW_DESC constexpr rtv_desc{
      .Format = swap_chain_format_,
      .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D, .Texture2D = {0, 0}
    };

    device_->CreateRenderTargetView(swap_chain_buffers_[i].Get(), &rtv_desc,
                                    rtv_cpu_handles_[i]);
  }
}

auto Renderer::CreateDepthBufferDsv() const -> void {
  D3D12_DEPTH_STENCIL_VIEW_DESC constexpr dsv_desc{
    .Format = depth_buffer_format_,
    .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
    .Flags = D3D12_DSV_FLAG_NONE, .Texture2D = {0}
  };

  device_->CreateDepthStencilView(depth_buffer_.Get(), &dsv_desc,
                                  dsv_cpu_handle_);
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
