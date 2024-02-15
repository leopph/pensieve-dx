#include <iostream>
#include <cstdlib>

#include "error.hpp"
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
    pensieve::HandleError(window.error());
    return EXIT_FAILURE;
  }

  auto renderer{pensieve::Renderer::Create(window->ToHwnd())};

  if (!renderer) {
    pensieve::HandleError(renderer.error());
    return EXIT_FAILURE;
  }

  auto const model_data{pensieve::LoadModel(argv[1])};

  if (!model_data) {
    pensieve::HandleError(model_data.error());
    return EXIT_FAILURE;
  }

  auto const gpu_model{renderer->CreateGpuModel(*model_data)};

  if (!gpu_model) {
    pensieve::HandleError(gpu_model.error());
    return EXIT_FAILURE;
  }

  while (!window->ShouldClose()) {
    window->PollEvents();

    if (auto const exp{renderer->DrawFrame(*gpu_model)}; !exp) {
      pensieve::HandleError(exp.error());
      return EXIT_FAILURE;
    }
  }

  if (auto const exp{renderer->WaitForDeviceIdle()}; !exp) {
    pensieve::HandleError(exp.error());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
