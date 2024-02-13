#pragma once

#include <cstdint>
#include <memory>

namespace pensieve {
struct ImageData {
  unsigned width;
  unsigned height;
  std::unique_ptr<std::uint8_t[]> bytes;
};
}