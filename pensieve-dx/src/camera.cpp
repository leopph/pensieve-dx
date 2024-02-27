#include "camera.hpp"

namespace pensieve {
Camera::Camera(float const vertical_degrees_fov, float const near_clip_plane,
               float const far_clip_plane, float const distance) :
  vertical_degrees_fov{vertical_degrees_fov}, near_clip_plane{near_clip_plane},
  far_clip_plane{far_clip_plane}, distance{distance} {
}

auto Camera::Update(std::span<int const, 2> const mouse_delta,
                    int const mouse_wheel_deta, bool const is_mouse_hovered,
                    bool const is_lmb_down, bool const is_mmb_down) -> void {
  if (is_mouse_hovered) {
    if (is_lmb_down) {
      auto rot{XMLoadFloat4(&rotation)};
      rot = DirectX::XMQuaternionMultiply(
        rot, DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0, 1, 0, 0),
                                               mouse_delta[0] * sensitivity));
      rot = DirectX::XMQuaternionMultiply(
        DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(1, 0, 0, 0),
                                          mouse_delta[1] * sensitivity), rot);
      XMStoreFloat4(&rotation, rot);
    } else if (is_mmb_down) {
      auto const rot{XMLoadFloat4(&rotation)};
      auto const right{
        DirectX::XMVector3Rotate(DirectX::XMVectorSet(1, 0, 0, 0), rot)
      };
      auto const up{
        DirectX::XMVector3Rotate(DirectX::XMVectorSet(0, 1, 0, 0), rot)
      };
      auto const old_center{XMLoadFloat3(&center)};
      auto const vec_sens{DirectX::XMVectorReplicate(sensitivity)};
      auto const delta_x{
        DirectX::XMVectorNegate(DirectX::XMVectorMultiply(
          DirectX::XMVectorReplicate(static_cast<float>(mouse_delta[0])),
          vec_sens))
      };
      auto const delta_y{
        DirectX::XMVectorMultiply(
          DirectX::XMVectorReplicate(static_cast<float>(mouse_delta[1])),
          vec_sens)
      };
      auto const new_center{
        DirectX::XMVectorMultiplyAdd(delta_x, right,
                                     DirectX::XMVectorMultiplyAdd(
                                       delta_y, up, old_center))
      };
      XMStoreFloat3(&center, new_center);
    } else {
      distance -= static_cast<float>(mouse_wheel_deta);
    }
  }
}
}
