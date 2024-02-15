#pragma once

#include <vector>

#include <d3d12.h>
#include <wrl/client.h>

namespace pensieve {
struct GpuMesh {
  Microsoft::WRL::ComPtr<ID3D12Resource2> pos_buf;
  Microsoft::WRL::ComPtr<ID3D12Resource2> uv_buf;
  Microsoft::WRL::ComPtr<ID3D12Resource2> draw_data_buf;
  UINT pos_buf_srv_idx;
  UINT uv_buf_srv_idx;
};

struct GpuMaterial {
  Microsoft::WRL::ComPtr<ID3D12Resource2> res;
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