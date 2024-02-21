#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace pensieve {
using Float2 = std::array<float, 2>;
using Float3 = std::array<float, 3>;
using Float4X4 = std::array<float, 16>;

struct TextureData {
  unsigned width;
  unsigned height;
  std::unique_ptr<std::uint8_t[]> bytes;
};

struct MaterialData {
  Float3 base_color;
  float metallic;
  float roughness;
  Float3 emission_color;
  std::optional<unsigned> base_color_map_idx;
  std::optional<unsigned> metallic_map_idx;
  std::optional<unsigned> roughness_map_idx;
  std::optional<unsigned> emission_map_idx;
  std::optional<unsigned> normal_map_idx;
};

struct MeshData {
  std::vector<Float3> positions;
  std::vector<Float3> normals;
  std::vector<Float3> tangents;
  std::vector<std::uint32_t> indices;
  std::optional<std::vector<Float2>> uvs;
  unsigned material_idx;
};

struct NodeData {
  std::vector<unsigned> mesh_indices;
  Float4X4 transform;
};

struct SceneData {
  std::vector<TextureData> textures;
  std::vector<MaterialData> materials;
  std::vector<MeshData> meshes;
  std::vector<NodeData> nodes;
};
}
