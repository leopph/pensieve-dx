#pragma once

#include <filesystem>
#include <vector>

#include <DirectXMath.h>

#include "image_data.hpp"

namespace pensieve {
struct Vertex {
  DirectX::XMFLOAT3X3 position;
};

struct MaterialData {
  DirectX::XMFLOAT3 base_color;
  ImageData base_texture;
};

struct MeshData {
  std::vector<Vertex> vertices;
  int material_idx;
};

struct ModelData {
  std::vector<MaterialData> materials;
  std::vector<MeshData> meshes;
};

[[nodiscard]] auto LoadModel(std::filesystem::path const& path) -> ModelData;
}
