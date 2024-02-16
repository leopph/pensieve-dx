#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "scene_data.hpp"

namespace pensieve {
[[nodiscard]] auto LoadScene(std::filesystem::path const& path) -> std::expected<SceneData, std::string>;
}
