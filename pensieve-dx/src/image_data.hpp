#pragma once

#include <cstdint>
#include <memory>

namespace pensieve {
struct ImageData {
  int width;
  int height;
  std::unique_ptr<std::uint8_t[]> bytes;
};
}