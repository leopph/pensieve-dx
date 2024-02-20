#include "scene_loading.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <iterator>
#include <stack>
#include <unordered_map>
#include <utility>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace pensieve {
auto LoadScene(
  std::filesystem::path const& path) -> std::expected<SceneData, std::string> {
  Assimp::Importer importer;
  importer.SetPropertyInteger(
    AI_CONFIG_PP_RVC_FLAGS,
    aiComponent_NORMALS | aiComponent_TANGENTS_AND_BITANGENTS |
    aiComponent_COLORS | aiComponent_BONEWEIGHTS | aiComponent_ANIMATIONS |
    aiComponent_LIGHTS | aiComponent_CAMERAS);
  importer.SetPropertyInteger(
    AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
  auto const scene{
    importer.ReadFile(path.string().c_str(),
                      aiProcess_JoinIdenticalVertices | aiProcess_Triangulate |
                      aiProcess_RemoveComponent | aiProcess_GenNormals |
                      aiProcess_SortByPType | aiProcess_GenUVCoords |
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
      DirectX::XMFLOAT3{1.0f, 1.0f, 1.0f}, 0.0f, 0.0f,
      DirectX::XMFLOAT3{0.0f, 0.0f, 0.0f});

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

    std::vector<DirectX::XMFLOAT4> positions;
    positions.reserve(mesh->mNumVertices);
    std::ranges::transform(mesh->mVertices,
                           mesh->mVertices + mesh->mNumVertices,
                           std::back_inserter(positions),
                           [](aiVector3D const& pos) {
                             return DirectX::XMFLOAT4{
                               pos.x, pos.y, pos.z, 1.0f
                             };
                           });

    std::optional<std::vector<DirectX::XMFLOAT2>> uvs;

    if (mesh->HasTextureCoords(0)) {
      uvs.emplace();
      uvs->reserve(mesh->mNumVertices);
      std::ranges::transform(mesh->mTextureCoords[0],
                             mesh->mTextureCoords[0] + mesh->mNumVertices,
                             std::back_inserter(*uvs),
                             [](aiVector3D const& uv) {
                               return DirectX::XMFLOAT2{uv.x, uv.y};
                             });
    }

    if (!mesh->HasNormals()) {
      return std::unexpected{
        std::format("Mesh {} contains no vertex normals.", mesh->mName.C_Str())
      };
    }

    std::vector<DirectX::XMFLOAT4> normals;
    normals.reserve(mesh->mNumVertices);
    std::ranges::transform(mesh->mNormals, mesh->mNormals + mesh->mNumVertices,
                           std::back_inserter(normals),
                           [](aiVector3D const& normal) {
                             return DirectX::XMFLOAT4{
                               normal.x, normal.y, normal.z, 0.0f
                             };
                           });

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

    scene_data.meshes.emplace_back(std::move(positions), std::move(normals),
                                   std::move(indices), std::move(uvs),
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

    scene_data.nodes.emplace_back(std::move(mesh_indices), DirectX::XMFLOAT4X4{
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
}
