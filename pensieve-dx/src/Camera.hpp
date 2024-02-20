#pragma once

#include <DirectXMath.h>

namespace pensieve {
struct Camera {
  DirectX::XMFLOAT4X4 transform;
};
}