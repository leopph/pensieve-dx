#include <iostream>
#include <cstdlib>
#include <limits>

#include "camera.hpp"
#include "error.hpp"
#include "scene_loading.hpp"
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

  auto const scene_data{pensieve::LoadScene(argv[1])};

  if (!scene_data) {
    pensieve::HandleError(scene_data.error());
    return EXIT_FAILURE;
  }

  auto const gpu_scene{renderer->CreateGpuScene(*scene_data)};

  if (!gpu_scene) {
    pensieve::HandleError(gpu_scene.error());
    return EXIT_FAILURE;
  }

  pensieve::Camera cam{60, 0.1f, 10'000.0f, 5.0f};

  while (!window->ShouldClose()) {
    window->PollEvents();

    if (window->WasResized()) {
      if (auto const exp{renderer->ResizeRenderTargets()}; !exp) {
        pensieve::HandleError(exp.error());
        return EXIT_FAILURE;
      }
    }

    cam.Update(window->GetMouseDelta(), window->GetMouseWheelDelta(),
               window->IsMouseHovered(), window->IsLmbDown(),
               window->IsMmbDown());

    if (auto const exp{renderer->DrawFrame(*gpu_scene, cam)}; !exp) {
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
