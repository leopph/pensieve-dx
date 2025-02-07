#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <ranges>
#include <span>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <DirectXMath.h>
#include <DirectXMesh.h>

#include "scene_data.hpp"

namespace pensieve {
namespace {
constexpr auto kMeshletMaxVerts{128};
constexpr auto kMeshletMaxPrims{256};
}

auto LoadScene(
  std::filesystem::path const& path) -> std::expected<SceneData, std::string> {
  Assimp::Importer importer;
  importer.SetPropertyInteger(
    AI_CONFIG_PP_RVC_FLAGS,
    aiComponent_COLORS | aiComponent_BONEWEIGHTS | aiComponent_ANIMATIONS |
    aiComponent_LIGHTS | aiComponent_CAMERAS);
  importer.SetPropertyInteger(
    AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
  auto const scene{
    importer.ReadFile(path.string().c_str(), aiProcess_CalcTangentSpace |
                      aiProcess_JoinIdenticalVertices | aiProcess_Triangulate |
                      aiProcess_RemoveComponent | aiProcess_GenNormals |
                      aiProcess_ValidateDataStructure |
                      aiProcess_RemoveRedundantMaterials | aiProcess_SortByPType
                      | aiProcess_GenUVCoords | aiProcess_FindInstances |
                      aiProcess_OptimizeMeshes | aiProcess_OptimizeGraph |
                      aiProcess_GlobalScale | aiProcess_ConvertToLeftHanded)
  };

  if (!scene) {
    return std::unexpected{importer.GetErrorString()};
  }

  std::unordered_map<std::string, unsigned> tex_paths_to_idx;

  SceneData scene_data;

  scene_data.materials.reserve(scene->mNumMaterials);
  for (unsigned i{0}; i < scene->mNumMaterials; i++) {
    auto const mtl{scene->mMaterials[i]};
    auto& mtl_data = scene_data.materials.emplace_back(
      Float3{1.0f, 1.0f, 1.0f}, 0.0f, 0.0f, Float3{0.0f, 0.0f, 0.0f});

    if (aiColor3D base_color; mtl->Get(AI_MATKEY_BASE_COLOR, base_color) ==
      aiReturn_SUCCESS) {
      mtl_data.base_color = {base_color.r, base_color.g, base_color.b};
    }

    if (float metallic; mtl->Get(AI_MATKEY_METALLIC_FACTOR, metallic) ==
      aiReturn_SUCCESS) {
      mtl_data.metallic = metallic;
    }

    if (float roughness; mtl->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) ==
      aiReturn_SUCCESS) {
      mtl_data.roughness = roughness;
    }

    if (aiColor3D emission; mtl->Get(AI_MATKEY_COLOR_EMISSIVE, emission) ==
      aiReturn_SUCCESS) {
      mtl_data.emission_color = {emission.r, emission.g, emission.b};
    }

    if (aiString tex_path; mtl->GetTexture(
      AI_MATKEY_BASE_COLOR_TEXTURE, &tex_path) == aiReturn_SUCCESS) {
      mtl_data.base_color_map_idx = tex_paths_to_idx.try_emplace(
        tex_path.C_Str(),
        static_cast<unsigned>(tex_paths_to_idx.size())).first->second;
    }

    if (aiString tex_path; mtl->GetTexture(
      AI_MATKEY_METALLIC_TEXTURE, &tex_path) == aiReturn_SUCCESS) {
      mtl_data.metallic_map_idx = tex_paths_to_idx.try_emplace(
        tex_path.C_Str(),
        static_cast<unsigned>(tex_paths_to_idx.size())).first->second;
    }

    if (aiString tex_path; mtl->GetTexture(
      AI_MATKEY_ROUGHNESS_TEXTURE, &tex_path) == aiReturn_SUCCESS) {
      mtl_data.roughness_map_idx = tex_paths_to_idx.try_emplace(
        tex_path.C_Str(),
        static_cast<unsigned>(tex_paths_to_idx.size())).first->second;
    }

    if (aiString tex_path; mtl->GetTexture(aiTextureType_EMISSIVE, 0, &tex_path)
      == aiReturn_SUCCESS) {
      mtl_data.emission_map_idx = tex_paths_to_idx.try_emplace(
        tex_path.C_Str(),
        static_cast<unsigned>(tex_paths_to_idx.size())).first->second;
    }

    if (aiString tex_path; mtl->GetTexture(aiTextureType_NORMALS, 0, &tex_path)
      == aiReturn_SUCCESS) {
      mtl_data.normal_map_idx = tex_paths_to_idx.try_emplace(
        tex_path.C_Str(),
        static_cast<unsigned>(tex_paths_to_idx.size())).first->second;
    }
  }

  for (auto const& tex_path : tex_paths_to_idx | std::views::keys) {
    if (auto const tex{scene->GetEmbeddedTexture(tex_path.c_str())}) {
      if (tex->mHeight == 0) {
        int width;
        int height;
        int channels;
        auto const bytes{
          stbi_load_from_memory(std::bit_cast<std::uint8_t*>(tex->pcData),
                                tex->mWidth, &width, &height, &channels, 4)
        };

        if (!bytes) {
          return std::unexpected{
            std::format("Failed to load compressed embedded texture \"{}\".",
                        tex_path.c_str())
          };
        }

        scene_data.textures.emplace_back(static_cast<unsigned>(width),
                                         static_cast<unsigned>(height),
                                         std::unique_ptr<std::uint8_t[]>{
                                           bytes
                                         });
      } else {
        auto& tex_data{
          scene_data.textures.emplace_back(tex->mWidth, tex->mHeight,
                                           std::make_unique_for_overwrite<
                                             std::uint8_t []>(
                                             tex->mWidth * tex->mHeight * 4))
        };
        std::memcpy(tex_data.bytes.get(), tex->pcData,
                    tex->mWidth * tex->mHeight * 4);
      }
    } else {
      auto const tex_path_abs{path.parent_path() / tex_path.c_str()};

      int width;
      int height;
      int channels;
      auto const bytes{
        stbi_load(tex_path_abs.string().c_str(), &width, &height, &channels, 4)
      };

      if (!bytes) {
        return std::unexpected{
          std::format("Failed to load texture at {}.", tex_path.c_str())
        };
      }

      scene_data.textures.emplace_back(static_cast<unsigned>(width),
                                       static_cast<unsigned>(height),
                                       std::unique_ptr<std::uint8_t[]>{bytes});
    }
  }

  for (unsigned i{0}; i < scene->mNumMeshes; i++) {
    auto const mesh{scene->mMeshes[i]};

    if (!mesh->HasPositions()) {
      return std::unexpected{
        std::format("Mesh {} contains no vertex positions.",
                    mesh->mName.C_Str())
      };
    }

    std::vector<DirectX::XMFLOAT3> positions;
    positions.reserve(mesh->mNumVertices);
    std::ranges::transform(mesh->mVertices,
                           mesh->mVertices + mesh->mNumVertices,
                           std::back_inserter(positions),
                           [](aiVector3D const& pos) {
                             return DirectX::XMFLOAT3{pos.x, pos.y, pos.z};
                           });

    std::optional<std::vector<Float2>> uvs;

    if (mesh->HasTextureCoords(0)) {
      uvs.emplace();
      uvs->reserve(mesh->mNumVertices);
      std::ranges::transform(mesh->mTextureCoords[0],
                             mesh->mTextureCoords[0] + mesh->mNumVertices,
                             std::back_inserter(*uvs),
                             [](aiVector3D const& uv) {
                               return Float2{uv.x, uv.y};
                             });
    }

    if (!mesh->HasNormals()) {
      return std::unexpected{
        std::format("Mesh {} contains no vertex normals.", mesh->mName.C_Str())
      };
    }

    std::vector<Float4> normals;
    normals.reserve(mesh->mNumVertices);
    std::ranges::transform(mesh->mNormals, mesh->mNormals + mesh->mNumVertices,
                           std::back_inserter(normals),
                           [](aiVector3D const& normal) {
                             return Float4{normal.x, normal.y, normal.z, 0.0f};
                           });

    std::optional<std::vector<Float4>> tangents;

    if (mesh->HasTangentsAndBitangents()) {
      tangents.emplace();
      tangents->reserve(mesh->mNumVertices);
      std::ranges::transform(mesh->mTangents,
                             mesh->mTangents + mesh->mNumVertices,
                             std::back_inserter(*tangents),
                             [](aiVector3D const& tangent) {
                               return Float4{
                                 tangent.x, tangent.y, tangent.z, 0.0f
                               };
                             });
    }

    if (!mesh->HasFaces()) {
      return std::unexpected{
        std::format("Mesh {} contains no vertex indices.", mesh->mName.C_Str())
      };
    }

    std::vector<std::uint32_t> indices;
    indices.reserve(mesh->mNumFaces * 3);
    for (unsigned j{0}; j < mesh->mNumFaces; j++) {
      std::ranges::copy_n(mesh->mFaces[j].mIndices, mesh->mFaces[j].mNumIndices,
                          std::back_inserter(indices));
    }

    std::vector<MeshletData> meshlets;
    std::vector<std::uint8_t> vertex_indices;
    std::vector<MeshletTriangleIndexData> primitive_indices;

    if (FAILED(
      ComputeMeshlets(indices.data(), indices.size() / 3, positions.data(),
        positions.size(), nullptr, reinterpret_cast<std::vector<DirectX::Meshlet
        >&>(meshlets), vertex_indices, reinterpret_cast<std::vector<DirectX::
        MeshletTriangle>&>(primitive_indices), kMeshletMaxVerts,
        kMeshletMaxPrims))) {
      return std::unexpected{
        std::format("Failed to generate meshlets for mesh {}.",
                    mesh->mName.C_Str())
      };
    }

    std::vector<Float4> positions4;
    positions4.reserve(positions.size());
    std::ranges::transform(positions, std::back_inserter(positions4),
                           [](DirectX::XMFLOAT3 const& pos) {
                             return Float4{pos.x, pos.y, pos.z, 1.0f};
                           });

    scene_data.meshes.emplace_back(std::move(positions4), std::move(normals),
                                   std::move(tangents), std::move(uvs),
                                   std::move(meshlets),
                                   std::move(vertex_indices),
                                   std::move(primitive_indices),
                                   mesh->mMaterialIndex);
  }

  std::stack<std::pair<aiNode const*, aiMatrix4x4>> nodes;
  nodes.emplace(scene->mRootNode, aiMatrix4x4{});

  while (!nodes.empty()) {
    auto const [node, parent_transform]{nodes.top()};
    nodes.pop();
    auto const node_global_transform{node->mTransformation * parent_transform};

    for (unsigned i{0}; i < node->mNumChildren; i++) {
      nodes.emplace(node->mChildren[i], node_global_transform);
    }

    std::vector<unsigned> mesh_indices;
    mesh_indices.reserve(node->mNumMeshes);
    std::ranges::copy_n(node->mMeshes, node->mNumMeshes,
                        std::back_inserter(mesh_indices));

    scene_data.nodes.emplace_back(std::move(mesh_indices), Float4X4{
                                    node_global_transform.a1,
                                    node_global_transform.b1,
                                    node_global_transform.c1,
                                    node_global_transform.d1,
                                    node_global_transform.a2,
                                    node_global_transform.b2,
                                    node_global_transform.c2,
                                    node_global_transform.d2,
                                    node_global_transform.a3,
                                    node_global_transform.b3,
                                    node_global_transform.c3,
                                    node_global_transform.d3,
                                    node_global_transform.a4,
                                    node_global_transform.b4,
                                    node_global_transform.c4,
                                    node_global_transform.d4,
                                  });
  }

  return scene_data;
}

auto WriteScene(std::ofstream& out, SceneData const& scene) -> void {
  std::span constexpr header{"pensieve"};
  out.write(header.data(), header.size());

  auto const texture_count{scene.textures.size()};
  out.write(std::bit_cast<char const*>(&texture_count), sizeof(texture_count));

  for (auto const& tex : scene.textures) {
    out.write(std::bit_cast<char const*>(&tex.width), sizeof(tex.width));
    out.write(std::bit_cast<char const*>(&tex.height), sizeof(tex.height));
    out.write(std::bit_cast<char const*>(tex.bytes.get()),
              4 * tex.width * tex.height);
  }

  auto const material_count{scene.materials.size()};
  out.write(std::bit_cast<char const*>(&material_count),
            sizeof(material_count));

  for (auto const& mtl : scene.materials) {
    out.write(std::bit_cast<char const*>(&mtl.base_color),
              sizeof(mtl.base_color));
    out.write(std::bit_cast<char const*>(&mtl.metallic), sizeof(mtl.metallic));
    out.write(std::bit_cast<char const*>(&mtl.roughness),
              sizeof(mtl.roughness));
    out.write(std::bit_cast<char const*>(&mtl.emission_color),
              sizeof(mtl.emission_color));

    int const has_base_color_map{mtl.base_color_map_idx.has_value()};
    out.write(std::bit_cast<char const*>(&has_base_color_map),
              sizeof(has_base_color_map));

    if (has_base_color_map) {
      out.write(std::bit_cast<char const*>(&*mtl.base_color_map_idx),
                sizeof(*mtl.base_color_map_idx));
    }

    int const has_metallic_map{mtl.metallic_map_idx.has_value()};
    out.write(std::bit_cast<char const*>(&has_metallic_map),
              sizeof(has_metallic_map));

    if (has_metallic_map) {
      out.write(std::bit_cast<char const*>(&*mtl.metallic_map_idx),
                sizeof(*mtl.metallic_map_idx));
    }

    int const has_roughness_map{mtl.roughness_map_idx.has_value()};
    out.write(std::bit_cast<char const*>(&has_roughness_map),
              sizeof(has_roughness_map));

    if (has_roughness_map) {
      out.write(std::bit_cast<char const*>(&*mtl.roughness_map_idx),
                sizeof(*mtl.roughness_map_idx));
    }

    int const has_emission_map{mtl.emission_map_idx.has_value()};
    out.write(std::bit_cast<char const*>(&has_emission_map),
              sizeof(has_emission_map));

    if (has_emission_map) {
      out.write(std::bit_cast<char const*>(&*mtl.emission_map_idx),
                sizeof(*mtl.emission_map_idx));
    }

    int const has_normal_map{mtl.normal_map_idx.has_value()};
    out.write(std::bit_cast<char const*>(&has_normal_map),
              sizeof(has_normal_map));

    if (has_normal_map) {
      out.write(std::bit_cast<char const*>(&*mtl.normal_map_idx),
                sizeof(*mtl.normal_map_idx));
    }
  }

  auto const mesh_count{scene.meshes.size()};
  out.write(std::bit_cast<char const*>(&mesh_count), sizeof(mesh_count));

  for (auto const& mesh : scene.meshes) {
    auto const vertex_count{mesh.positions.size()};
    out.write(std::bit_cast<char const*>(&vertex_count), sizeof(vertex_count));
    out.write(std::bit_cast<char const*>(mesh.positions.data()),
              vertex_count * sizeof(decltype(mesh.positions)::value_type));
    out.write(std::bit_cast<char const*>(mesh.normals.data()),
              vertex_count * sizeof(decltype(mesh.normals)::value_type));

    int const has_tangents{mesh.tangents.has_value()};
    out.write(std::bit_cast<char const*>(&has_tangents), sizeof(has_tangents));

    if (has_tangents) {
      out.write(std::bit_cast<char const*>(mesh.tangents->data()),
                vertex_count * sizeof(decltype(mesh.tangents
                )::value_type::value_type));
    }

    int const has_uvs{mesh.uvs.has_value()};
    out.write(std::bit_cast<char const*>(&has_uvs), sizeof(has_uvs));

    if (has_uvs) {
      out.write(std::bit_cast<char const*>(mesh.uvs->data()),
                vertex_count * sizeof(decltype(mesh.uvs
                )::value_type::value_type));
    }

    auto const meshlet_count{mesh.meshlets.size()};
    out.write(std::bit_cast<char const*>(&meshlet_count),
              sizeof(meshlet_count));
    out.write(std::bit_cast<char const*>(mesh.meshlets.data()),
              meshlet_count * sizeof(decltype(mesh.meshlets)::value_type));

    auto const vertex_index_count{mesh.vertex_indices.size()};
    out.write(std::bit_cast<char const*>(&vertex_index_count),
              sizeof(vertex_index_count));
    out.write(std::bit_cast<char const*>(mesh.vertex_indices.data()),
              vertex_index_count * sizeof(decltype(mesh.vertex_indices
              )::value_type));

    auto const triangle_index_count{mesh.triangle_indices.size()};
    out.write(std::bit_cast<char const*>(&triangle_index_count),
              sizeof(triangle_index_count));
    out.write(std::bit_cast<char const*>(mesh.triangle_indices.data()),
              triangle_index_count * sizeof(decltype(mesh.triangle_indices
              )::value_type));

    out.write(std::bit_cast<char const*>(&mesh.material_idx),
              sizeof(mesh.material_idx));
  }

  auto const node_count{scene.nodes.size()};
  out.write(std::bit_cast<char const*>(&node_count), sizeof(node_count));

  for (auto const& node : scene.nodes) {
    auto const mesh_index_count{node.mesh_indices.size()};
    out.write(std::bit_cast<char const*>(&mesh_index_count),
              sizeof(mesh_index_count));
    out.write(std::bit_cast<char const*>(node.mesh_indices.data()),
              mesh_index_count * sizeof(decltype(node.mesh_indices
              )::value_type));
    out.write(std::bit_cast<char const*>(&node.transform),
              sizeof(node.transform));
  }
}
}

auto main(int const argc, char** const argv) -> int {
  if (argc < 3) {
    std::cout <<
      "Usage: meshlet-generator <source-model-file> <destination-file>\n";
    return EXIT_SUCCESS;
  }

  std::cout << "Processing mesh...\n";

  auto const scene{pensieve::LoadScene(argv[1])};

  if (!scene) {
    std::cerr << "Error: " << scene.error() << '\n';
    return EXIT_FAILURE;
  }

  std::ofstream out{
    argv[2], std::ios::binary | std::ios::out | std::ios::trunc
  };

  if (!out.is_open()) {
    std::cerr << "Failed to open output file.\n";
    return EXIT_FAILURE;
  }

  WriteScene(out, *scene);

  return EXIT_SUCCESS;
}
