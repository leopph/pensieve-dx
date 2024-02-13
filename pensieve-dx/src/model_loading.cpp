#include "model_loading.hpp"

#include <cstring>
#include <format>

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
    auto const scene{importer.ReadFile(path.string().c_str(), 0)};

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

    return ret;
  }
}
