#include "model_loading.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <iterator>
#include <stack>
#include <utility>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace pensieve {
  auto LoadModel(
    std::filesystem::path const& path) -> std::expected<
    ModelData, std::string> {
    Assimp::Importer importer;
    auto const scene{
      importer.ReadFile(path.string().c_str(), aiProcess_ConvertToLeftHanded)
    };

    if (!scene) {
      return std::unexpected{importer.GetErrorString()};
    }

    ModelData ret;

    ret.materials.reserve(scene->mNumMaterials);
    for (unsigned i{0}; i < scene->mNumMaterials; i++) {
      auto const mtl{scene->mMaterials[i]};
      auto& mtl_data = ret.materials.emplace_back(DirectX::XMFLOAT4{
        1.0f, 1.0f, 1.0f, 1.0f
      });

      if (aiColor4D base_color; mtl->Get(AI_MATKEY_BASE_COLOR, base_color) ==
        aiReturn_SUCCESS) {
        mtl_data.base_color = {
          base_color.r, base_color.g, base_color.b, base_color.a
        };
      }

      if (aiString tex_path; mtl->GetTexture(aiTextureType_BASE_COLOR, 0,
                                             &tex_path) == aiReturn_SUCCESS) {
        if (auto const tex{scene->GetEmbeddedTexture(tex_path.C_Str())}) {
          if (tex->mHeight == 0) {
            return std::unexpected{"Found compressed embedded texture."};
          }

          mtl_data.base_texture = {
            tex->mWidth, tex->mHeight,
            std::make_unique_for_overwrite<std::uint8_t[]>(
              tex->mWidth * tex->mHeight * 4)
          };
          std::memcpy(mtl_data.base_texture.bytes.get(), tex->pcData,
                      tex->mWidth * tex->mHeight * 4);
        } else {
          auto const tex_path_abs{path.parent_path() / tex_path.C_Str()};

          int width;
          int height;
          int channels;
          auto const bytes{
            stbi_load(tex_path_abs.string().c_str(), &width, &height, &channels,
                      4)
          };

          if (!bytes) {
            return std::unexpected{
              std::format("Failed to load texture at {}.", tex_path.C_Str())
            };
          }

          mtl_data.base_texture = {
            static_cast<unsigned>(width), static_cast<unsigned>(height),
            std::unique_ptr<std::uint8_t[]>{bytes}
          };
        }
      }
    }

    std::stack<std::pair<aiNode const*, aiMatrix4x4>> nodes;
    nodes.emplace(scene->mRootNode, aiMatrix4x4{});

    while (!nodes.empty()) {
      auto const [node, parent_transform]{nodes.top()};
      nodes.pop();
      auto const node_global_transform{
        node->mTransformation * parent_transform
      };

      for (unsigned i{0}; i < node->mNumChildren; i++) {
        nodes.emplace(node->mChildren[i], node_global_transform);
      }

      for (unsigned i{0}; i < node->mNumMeshes; i++) {
        auto const mesh{scene->mMeshes[node->mMeshes[i]]};

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

        ret.meshes.emplace_back(std::move(positions), DirectX::XMFLOAT4X4{
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
                                }, mesh->mMaterialIndex);
      }
    }

    return ret;
  }
}
