#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <DirectXMath.h>

#include "image_data.hpp"

namespace pensieve {
struct MaterialData {
  DirectX::XMFLOAT3 base_color;
  std::optional<unsigned> base_texture_idx;
};

struct MeshData {
  std::vector<DirectX::XMFLOAT4> positions;
  std::optional<std::vector<DirectX::XMFLOAT2>> uvs;
  std::vector<std::uint32_t> indices;
  DirectX::XMFLOAT4X4 transform;
  unsigned material_idx;
};

struct ModelData {
  std::vector<ImageData> textures;
  std::vector<MaterialData> materials;
  std::vector<MeshData> meshes;
};

[[nodiscard]] auto LoadModel(std::filesystem::path const& path) -> std::expected<ModelData, std::string>;
}
