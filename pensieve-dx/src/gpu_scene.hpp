#pragma once

#include <optional>
#include <vector>

#include <D3D12MemAlloc.h>
#include <wrl/client.h>

#include "scene_data.hpp"

namespace pensieve {
struct GpuTexture {
  Microsoft::WRL::ComPtr<D3D12MA::Allocation> res;
  UINT srv_idx;
};

struct GpuMaterial {
  Microsoft::WRL::ComPtr<D3D12MA::Allocation> res;
  UINT cbv_idx;
};

struct GpuMesh {
  Microsoft::WRL::ComPtr<D3D12MA::Allocation> pos_buf;
  Microsoft::WRL::ComPtr<D3D12MA::Allocation> norm_buf;
  Microsoft::WRL::ComPtr<D3D12MA::Allocation> tan_buf;
  std::optional<Microsoft::WRL::ComPtr<D3D12MA::Allocation>> uv_buf;

  Microsoft::WRL::ComPtr<D3D12MA::Allocation> vertex_idx_buf;
  Microsoft::WRL::ComPtr<D3D12MA::Allocation> prim_idx_buf;
  Microsoft::WRL::ComPtr<D3D12MA::Allocation> meshlet_buf;

  Microsoft::WRL::ComPtr<D3D12MA::Allocation> inst_buf;

  MeshletData last_meshlet;

  UINT pos_buf_srv_idx;
  UINT norm_buf_srv_idx;
  UINT tan_buf_srv_idx;
  std::optional<UINT> uv_buf_srv_idx;
  UINT vertex_idx_buf_srv_idx;
  UINT prim_idx_buf_srv_idx;
  UINT meshlet_buf_srv_idx;

  UINT mtl_idx;
  UINT meshlet_count;

  UINT inst_buf_srv_idx;
  UINT instance_count;
};

struct GpuScene {
  std::vector<GpuTexture> textures;
  std::vector<GpuMaterial> materials;
  std::vector<GpuMesh> meshes;
};
}