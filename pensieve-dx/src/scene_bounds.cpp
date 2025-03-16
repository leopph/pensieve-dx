#include "scene_bounds.h"

#include <limits>

namespace pensieve {
auto ComputeBounds(SceneData const& scene) -> Bounds {
  Bounds bounds;

  auto min{DirectX::XMVectorReplicate(std::numeric_limits<float>::max())};
  auto max{DirectX::XMVectorReplicate(std::numeric_limits<float>::lowest())};

  for (auto const& mesh : scene.meshes) {
    for (auto const& pos : mesh.positions) {
      DirectX::XMFLOAT3 const pos_f3{pos.data()};
      DirectX::XMVECTOR const pos_vec{XMLoadFloat3(&pos_f3)};

      min = DirectX::XMVectorMin(min, pos_vec);
      max = DirectX::XMVectorMax(max, pos_vec);
    }
  }

  auto const center{DirectX::XMVectorDivide(DirectX::XMVectorAdd(min, max), DirectX::XMVectorReplicate(2.0f))};
  XMStoreFloat3(&bounds.center, center);
  DirectX::XMStoreFloat(&bounds.radius, DirectX::XMVector3Length(DirectX::XMVectorSubtract(max, center)));

  return bounds;
}
}
