#include <iostream>
#include <cstdlib>

#include "model_loading.hpp"
#include "renderer.hpp"
#include "window.hpp"

auto main(int const argc, char* argv[]) -> int {
  if (argc < 2) {
    std::cout << "Usage: pensieve-dx <path-to-model-file>\n";
    return EXIT_SUCCESS;
  }

  auto window{pensieve::Window::Create()};

  if (!window) {
    std::cerr << window.error() << '\n';
    return EXIT_FAILURE;
  }

  auto renderer{pensieve::Renderer::Create(window->ToHwnd())};

  if (!renderer) {
    std::cerr << renderer.error() << '\n';
    return EXIT_FAILURE;
  }

  auto const model_data{pensieve::LoadModel(argv[1])};

  if (!model_data) {
    std::cerr << model_data.error() << '\n';
    return EXIT_FAILURE;
  }

  auto const gpu_model{renderer->CreateGpuModel(*model_data)};

  if (!gpu_model) {
    std::cerr << gpu_model.error() << '\n';
    return EXIT_FAILURE;
  }

  while (!window->ShouldClose()) {
    window->PollEvents();
    renderer->DrawFrame(*gpu_model);
  }
}
