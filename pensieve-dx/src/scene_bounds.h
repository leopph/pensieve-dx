#pragma once

#include <DirectXMath.h>

#include "scene_data.hpp"

namespace pensieve {
struct Bounds {
  DirectX::XMFLOAT3 center;
  float radius;
};



[[nodiscard]] auto ComputeBounds(SceneData const& scene) -> Bounds;
}
