#include "scene_loading.hpp"

#include <array>
#include <complex.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <fstream>
#include <memory>

namespace pensieve {
auto LoadScene(
  std::filesystem::path const& path) -> std::expected<SceneData, std::string> {
  std::ifstream in{path, std::ios::binary | std::ios::in};

  if (!in.is_open()) {
    return std::unexpected{
      std::format("Failed to open file {}.", path.string())
    };
  }

  SceneData scene_data;

  auto constexpr header_length{9};
  std::array<char, header_length> header;
  in.read(header.data(), header_length);

  if (in.gcount() != header_length) {
    return std::unexpected{"Failed to read file header."};
  }

  if (std::strcmp(header.data(), "pensieve") != 0) {
    return std::unexpected{"File header mismatch."};
  }

  std::size_t texture_count;
  in.read(std::bit_cast<char*>(&texture_count), sizeof(texture_count));

  if (in.gcount() != sizeof(texture_count)) {
    return std::unexpected{"Failed to read texture count."};
  }

  scene_data.textures.reserve(texture_count);

  for (std::size_t i{0}; i < texture_count; i++) {
    auto& [width, height, bytes]{scene_data.textures.emplace_back()};

    in.read(std::bit_cast<char*>(&width), sizeof(width));

    if (in.gcount() != sizeof(width)) {
      return std::unexpected{
        std::format("Failed to read width of texture {}.", i)
      };
    }

    in.read(std::bit_cast<char*>(&height), sizeof(height));

    if (in.gcount() != sizeof(height)) {
      return std::unexpected{
        std::format("Failed to read height of texture {}.", i)
      };
    }

    auto const byte_count{4 * width * height};
    bytes = std::make_unique_for_overwrite<std::uint8_t[]>(byte_count);
    in.read(std::bit_cast<char*>(bytes.get()), byte_count);

    if (in.gcount() != byte_count) {
      return std::unexpected{
        std::format("Failed to read texels of texture {}.", i)
      };
    }
  }

  std::size_t material_count;
  in.read(std::bit_cast<char*>(&material_count), sizeof(material_count));

  if (in.gcount() != sizeof(material_count)) {
    return std::unexpected{"Failed to read material count."};
  }

  scene_data.materials.reserve(material_count);

  for (std::size_t i{0}; i < material_count; i++) {
    auto& [base_color, metallic, roughness, emission_color, base_color_map_idx,
      metallic_map_idx, roughness_map_idx, emission_map_idx, normal_map_idx]{
      scene_data.materials.emplace_back()
    };

    in.read(std::bit_cast<char*>(&base_color), sizeof(base_color));

    if (in.gcount() != sizeof(base_color)) {
      return std::unexpected{
        std::format("Failed to read material {} base color.", i)
      };
    }

    in.read(std::bit_cast<char*>(&metallic), sizeof(metallic));

    if (in.gcount() != sizeof(metallic)) {
      return std::unexpected{
        std::format("Failed to read material {} metallic factor.", i)
      };
    }

    in.read(std::bit_cast<char*>(&roughness), sizeof(roughness));

    if (in.gcount() != sizeof(roughness)) {
      return std::unexpected{
        std::format("Failed to read material {} roughness factor.", i)
      };
    }

    in.read(std::bit_cast<char*>(&emission_color), sizeof(emission_color));

    if (in.gcount() != sizeof(emission_color)) {
      return std::unexpected{
        std::format("Failed to read material {} emission color.", i)
      };
    }

    int has_base_color_map;
    in.read(std::bit_cast<char*>(&has_base_color_map),
            sizeof(has_base_color_map));

    if (in.gcount() != sizeof(has_base_color_map)) {
      return std::unexpected{
        std::format("Failed to read material {} base color map availability.",
                    i)
      };
    }

    if (has_base_color_map) {
      in.read(std::bit_cast<char*>(&base_color_map_idx.emplace()),
              sizeof(decltype(base_color_map_idx)::value_type));

      if (in.gcount() != sizeof(*base_color_map_idx)) {
        return std::unexpected{
          std::format("Failed to read material {} base color map index.", i)
        };
      }
    }

    int has_metallic_map;
    in.read(std::bit_cast<char*>(&has_metallic_map), sizeof(has_metallic_map));

    if (in.gcount() != sizeof(has_metallic_map)) {
      return std::unexpected{
        std::format("Failed to read material {} metallic map availability.", i)
      };
    }

    if (has_metallic_map) {
      in.read(std::bit_cast<char*>(&metallic_map_idx.emplace()),
              sizeof(decltype(metallic_map_idx)::value_type));

      if (in.gcount() != sizeof(*metallic_map_idx)) {
        return std::unexpected{
          std::format("Failed to read material {} metallic map index.", i)
        };
      }
    }

    int has_roughness_map;
    in.read(std::bit_cast<char*>(&has_roughness_map),
            sizeof(has_roughness_map));

    if (in.gcount() != sizeof(has_roughness_map)) {
      return std::unexpected{
        std::format("Failed to read material {} roughness map availability.", i)
      };
    }

    if (has_roughness_map) {
      in.read(std::bit_cast<char*>(&roughness_map_idx.emplace()),
              sizeof(decltype(roughness_map_idx)::value_type));

      if (in.gcount() != sizeof(*roughness_map_idx)) {
        return std::unexpected{
          std::format("Failed to read material {} roughness map index.", i)
        };
      }
    }

    int has_emission_map;
    in.read(std::bit_cast<char*>(&has_emission_map), sizeof(has_emission_map));

    if (in.gcount() != sizeof(has_emission_map)) {
      return std::unexpected{
        std::format("Failed to read material {} emission map availability.", i)
      };
    }

    if (has_emission_map) {
      in.read(std::bit_cast<char*>(&emission_map_idx.emplace()),
              sizeof(decltype(emission_map_idx)));

      if (in.gcount() != sizeof(*emission_map_idx)) {
        return std::unexpected{
          std::format("Failed to read material {} emission map index.", i)
        };
      }
    }

    int has_normal_map;
    in.read(std::bit_cast<char*>(&has_normal_map), sizeof(has_normal_map));

    if (in.gcount() != sizeof(has_normal_map)) {
      return std::unexpected{
        std::format("Failed to read material {} normal map availability.", i)
      };
    }

    if (has_normal_map) {
      in.read(std::bit_cast<char*>(&normal_map_idx.emplace()),
              sizeof(decltype(normal_map_idx)::value_type));

      if (in.gcount() != sizeof(*normal_map_idx)) {
        return std::unexpected{
          std::format("Failed to read material {} normal map index.", i)
        };
      }
    }
  }

  std::size_t mesh_count;
  in.read(std::bit_cast<char*>(&mesh_count), sizeof(mesh_count));

  if (in.gcount() != sizeof(mesh_count)) {
    return std::unexpected{"Failed to read mesh count."};
  }

  scene_data.meshes.reserve(mesh_count);

  for (std::size_t i{0}; i < mesh_count; i++) {
    auto& [positions, normals, tangents, uvs, meshlets, vertex_indices,
      triangle_indices, material_idx]{scene_data.meshes.emplace_back()};

    std::size_t vertex_count;
    in.read(std::bit_cast<char*>(&vertex_count), sizeof(vertex_count));

    if (in.gcount() != sizeof(vertex_count)) {
      return std::unexpected{
        std::format("Failed to read mesh {} vertex count.", i)
      };
    }

    positions.resize(vertex_count);
    auto const pos_buf_byte_size{
      static_cast<std::streamsize>(vertex_count * sizeof(Float4))
    };
    in.read(std::bit_cast<char*>(positions.data()), pos_buf_byte_size);

    if (in.gcount() != pos_buf_byte_size) {
      return std::unexpected{
        std::format("Failed to read mesh {} positions.", i)
      };
    }

    normals.resize(vertex_count);
    auto const norm_buf_byte_size{
      static_cast<std::streamsize>(vertex_count * sizeof(Float4))
    };
    in.read(std::bit_cast<char*>(normals.data()), norm_buf_byte_size);

    if (in.gcount() != norm_buf_byte_size) {
      return std::unexpected{std::format("Failed to read mesh {} normals.", i)};
    }

    int has_tangents;
    in.read(std::bit_cast<char*>(&has_tangents), sizeof(has_tangents));

    if (in.gcount() != sizeof(has_tangents)) {
      return std::unexpected{
        std::format("Failed to read mesh {} tangent availability.", i)
      };
    }

    if (has_tangents) {
      tangents.emplace().resize(vertex_count);
      auto const tan_buf_byte_size{
        static_cast<std::streamsize>(vertex_count * sizeof(Float4))
      };
      in.read(std::bit_cast<char*>(tangents->data()), tan_buf_byte_size);

      if (in.gcount() != tan_buf_byte_size) {
        return std::unexpected{
          std::format("Failed to read mesh {} tangents.", i)
        };
      }
    }

    int has_uvs;
    in.read(std::bit_cast<char*>(&has_uvs), sizeof(has_uvs));

    if (in.gcount() != sizeof(has_uvs)) {
      return std::unexpected{
        std::format("Failed to read mesh {} uv availability.", i)
      };
    }

    if (has_uvs) {
      uvs.emplace().resize(vertex_count);
      auto const uv_buf_byte_size{
        static_cast<std::streamsize>(vertex_count * sizeof(Float2))
      };
      in.read(std::bit_cast<char*>(uvs->data()), uv_buf_byte_size);

      if (in.gcount() != uv_buf_byte_size) {
        return std::unexpected{std::format("Failed to read mesh {} uvs.", i)};
      }
    }

    std::size_t meshlet_count;
    in.read(std::bit_cast<char*>(&meshlet_count), sizeof(meshlet_count));

    if (in.gcount() != sizeof(meshlet_count)) {
      return std::unexpected{
        std::format("Failed to read mesh {} meshlet count.", i)
      };
    }

    meshlets.resize(meshlet_count);
    auto const meshlet_buf_size{
      static_cast<std::streamsize>(meshlet_count * sizeof(MeshletData))
    };
    in.read(std::bit_cast<char*>(meshlets.data()), meshlet_buf_size);

    if (in.gcount() != meshlet_buf_size) {
      return std::unexpected{
        std::format("Failed to read mesh {} meshlets.", i)
      };
    }

    std::size_t vertex_index_count;
    in.read(std::bit_cast<char*>(&vertex_index_count),
            sizeof(vertex_index_count));

    if (in.gcount() != sizeof(vertex_index_count)) {
      return std::unexpected{
        std::format("Failed to read mesh {} vertex index count.", i)
      };
    }

    vertex_indices.resize(vertex_index_count);
    auto const vert_ind_buf_size{
      static_cast<std::streamsize>(vertex_index_count * sizeof(std::uint8_t))
    };
    in.read(std::bit_cast<char*>(vertex_indices.data()), vert_ind_buf_size);

    if (in.gcount() != vert_ind_buf_size) {
      return std::unexpected{
        std::format("Failed to read mesh {} vertex indices.", i)
      };
    }

    std::size_t triangle_index_count;
    in.read(std::bit_cast<char*>(&triangle_index_count),
            sizeof(triangle_index_count));

    if (in.gcount() != sizeof(triangle_index_count)) {
      return std::unexpected{
        std::format("Failed to read mesh {} triangle index count.", i)
      };
    }

    triangle_indices.resize(triangle_index_count);
    auto const tri_ind_buf_size{
      static_cast<std::streamsize>(triangle_index_count * sizeof(
        MeshletTriangleIndexData))
    };
    in.read(std::bit_cast<char*>(triangle_indices.data()), tri_ind_buf_size);

    if (in.gcount() != tri_ind_buf_size) {
      return std::unexpected{
        std::format("Failed to read mesh {} triangle indices.", i)
      };
    }

    in.read(std::bit_cast<char*>(&material_idx), sizeof(material_idx));

    if (in.gcount() != sizeof(material_idx)) {
      return std::unexpected{
        std::format("Failed to read mesh {} material index.", i)
      };
    }
  }

  std::size_t node_count;
  in.read(std::bit_cast<char*>(&node_count), sizeof(node_count));

  if (in.gcount() != sizeof(node_count)) {
    return std::unexpected{"Failed to read node count."};
  }

  scene_data.nodes.reserve(node_count);

  for (std::size_t i{0}; i < node_count; i++) {
    auto& [mesh_indices, transform]{scene_data.nodes.emplace_back()};

    std::size_t mesh_idx_count;
    in.read(std::bit_cast<char*>(&mesh_idx_count), sizeof(mesh_idx_count));

    if (in.gcount() != sizeof(mesh_idx_count)) {
      return std::unexpected{
        std::format("Failed to read node {} mesh index count.", i)
      };
    }

    mesh_indices.resize(mesh_idx_count);
    auto const mesh_idx_buf_size{
      static_cast<std::streamsize>(mesh_idx_count * sizeof(unsigned))
    };
    in.read(std::bit_cast<char*>(mesh_indices.data()), mesh_idx_buf_size);

    if (in.gcount() != mesh_idx_buf_size) {
      return std::unexpected{
        std::format("Failed to read node {} mesh indices.", i)
      };
    }

    in.read(std::bit_cast<char*>(&transform), sizeof(transform));

    if (in.gcount() != sizeof(transform)) {
      return std::unexpected{
        std::format("Failed to read node {} transform.", i)
      };
    }
  }

  return scene_data;
}
}
