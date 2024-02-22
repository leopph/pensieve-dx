#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace pensieve {
using Float2 = std::array<float, 2>;
using Float3 = std::array<float, 3>;
using Float4 = std::array<float, 4>;
using Float4X4 = std::array<float, 16>;

struct TextureData {
  std::uint32_t width;
  std::uint32_t height;
  std::unique_ptr<std::uint8_t[]> bytes;
};

struct MaterialData {
  Float3 base_color;
  float metallic;
  float roughness;
  Float3 emission_color;
  std::optional<std::uint32_t> base_color_map_idx;
  std::optional<std::uint32_t> metallic_map_idx;
  std::optional<std::uint32_t> roughness_map_idx;
  std::optional<std::uint32_t> emission_map_idx;
  std::optional<std::uint32_t> normal_map_idx;
};

struct MeshletData {
  std::uint32_t vert_count;
  std::uint32_t vert_offset;
  std::uint32_t prim_count;
  std::uint32_t prim_offset;
};

struct MeshletTriangleIndexData {
  std::uint32_t idx0 : 10;
  std::uint32_t idx1 : 10;
  std::uint32_t idx2 : 10;
};

struct MeshData {
  std::vector<Float4> positions;
  std::vector<Float4> normals;
  std::vector<Float4> tangents;
  std::optional<std::vector<Float2>> uvs;
  std::vector<MeshletData> meshlets;
  std::vector<std::uint8_t> vertex_indices;
  std::vector<MeshletTriangleIndexData> triangle_indices;
  std::uint32_t material_idx;
};

struct NodeData {
  std::vector<std::uint32_t> mesh_indices;
  Float4X4 transform;
};

struct SceneData {
  std::vector<TextureData> textures;
  std::vector<MaterialData> materials;
  std::vector<MeshData> meshes;
  std::vector<NodeData> nodes;
};
}
