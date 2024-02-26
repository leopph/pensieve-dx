#pragma once

#include <optional>
#include <vector>

#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>

namespace pensieve {
struct GpuTexture {
  Microsoft::WRL::ComPtr<ID3D12Resource2> res;
  UINT srv_idx;
};

struct GpuMaterial {
  Microsoft::WRL::ComPtr<ID3D12Resource2> res;
  UINT cbv_idx;
};

struct GpuMesh {
  Microsoft::WRL::ComPtr<ID3D12Resource2> pos_buf;
  Microsoft::WRL::ComPtr<ID3D12Resource2> norm_buf;
  Microsoft::WRL::ComPtr<ID3D12Resource2> tan_buf;
  std::optional<Microsoft::WRL::ComPtr<ID3D12Resource2>> uv_buf;

  Microsoft::WRL::ComPtr<ID3D12Resource2> vertex_idx_buf;
  Microsoft::WRL::ComPtr<ID3D12Resource2> prim_idx_buf;
  Microsoft::WRL::ComPtr<ID3D12Resource2> meshlet_buf;

  Microsoft::WRL::ComPtr<ID3D12Resource2> inst_buf;
  Microsoft::WRL::ComPtr<ID3D12Resource2> draw_data_buf;
  void* mapped_draw_data_buf;

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