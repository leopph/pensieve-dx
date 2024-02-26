#pragma once

#include <DirectXMath.h>

namespace pensieve {
struct Camera {
  float vertical_degrees_fov;
  float near_clip_plane;
  float far_clip_plane;
  DirectX::XMFLOAT4 rotation;
  DirectX::XMFLOAT3 position;
};
}