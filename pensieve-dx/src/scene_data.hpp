#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <DirectXMath.h>

namespace pensieve {
struct TextureData {
  unsigned width;
  unsigned height;
  std::unique_ptr<std::uint8_t[]> bytes;
};

struct MaterialData {
  DirectX::XMFLOAT3 base_color;
  float metallic;
  float roughness;
  std::optional<unsigned> base_color_map_idx;
  std::optional<unsigned> metallic_map_idx;
  std::optional<unsigned> roughness_map_idx;
};

struct MeshData {
  std::vector<DirectX::XMFLOAT4> positions;
  std::vector<std::uint32_t> indices;
  std::optional<std::vector<DirectX::XMFLOAT2>> uvs;
  unsigned material_idx;
};

struct NodeData {
  std::vector<unsigned> mesh_indices;
  DirectX::XMFLOAT4X4 transform;
};

struct SceneData {
  std::vector<TextureData> textures;
  std::vector<MaterialData> materials;
  std::vector<MeshData> meshes;
  std::vector<NodeData> nodes;
};
}