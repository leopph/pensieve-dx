#pragma once

#include <span>

#include <DirectXMath.h>

namespace pensieve {
struct Camera {
  float vertical_degrees_fov;
  float near_clip_plane;
  float far_clip_plane;
  float distance;
  float sensitivity{0.01f};
  DirectX::XMFLOAT4 rotation{0, 0, 0, 1};
  DirectX::XMFLOAT3 center{0, 0, 0};

  Camera(float vertical_degrees_fov, float near_clip_plane,
         float far_clip_plane, float distance);

  auto Update(std::span<int const, 2> mouse_delta, int mouse_wheel_deta,
              bool is_mouse_hovered, bool is_lmb_down,
              bool is_mmb_down) -> void;
};
}
