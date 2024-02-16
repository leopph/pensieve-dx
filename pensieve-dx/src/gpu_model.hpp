#pragma once

#include <vector>

#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>

namespace pensieve {
struct GpuMesh {
  Microsoft::WRL::ComPtr<ID3D12Resource2> pos_buf;
  Microsoft::WRL::ComPtr<ID3D12Resource2> uv_buf;
  Microsoft::WRL::ComPtr<ID3D12Resource2> idx_buf;
  Microsoft::WRL::ComPtr<ID3D12Resource2> draw_data_buf;
  DirectX::XMFLOAT4X4 transform;
  void* mapped_draw_data_buf;
  D3D12_INDEX_BUFFER_VIEW ibv;
  UINT pos_buf_srv_idx;
  UINT uv_buf_srv_idx;
  UINT mtl_idx;
  UINT index_count;
};

struct GpuMaterial {
  Microsoft::WRL::ComPtr<ID3D12Resource2> res;
  UINT cbv_idx;
};

struct GpuTexture {
  Microsoft::WRL::ComPtr<ID3D12Resource2> res;
  UINT srv_idx;
};

struct GpuModel {
  std::vector<GpuTexture> textures;
  std::vector<GpuMaterial> materials;
  std::vector<GpuMesh> meshes;
};
}