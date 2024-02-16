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
  Microsoft::WRL::ComPtr<ID3D12Resource2> idx_buf;
  D3D12_INDEX_BUFFER_VIEW ibv;

  std::optional<Microsoft::WRL::ComPtr<ID3D12Resource2>> uv_buf;
  std::optional<UINT> uv_buf_srv_idx;

  UINT pos_buf_srv_idx;
  UINT mtl_idx;
  UINT index_count;
};

struct GpuNode {
  std::vector<unsigned> mesh_indices;
  DirectX::XMFLOAT4X4 transform;
  std::vector< Microsoft::WRL::ComPtr<ID3D12Resource2>> draw_data_bufs;
  std::vector<void*> mapped_draw_data_bufs;
};

struct GpuScene {
  std::vector<GpuTexture> textures;
  std::vector<GpuMaterial> materials;
  std::vector<GpuMesh> meshes;
  std::vector<GpuNode> nodes;
};
}