#include <animation.hpp>
#include <desktop_example.hpp>
#include <example_options.hpp>
#include <sample_model.hpp>
#include <sample_screen.hpp>
#include <sdl_sample_renderer.hpp>

#include <arisa/runtime/application.hpp>
#include <arisa/runtime/root.hpp>

#ifdef ARISA_INSPECTOR
#include <arisa/inspector/inspector.hpp>
#endif

#include <cstdint>
#include <cstdio>
#include <print>
#include <span>

namespace arisa::examples {

class DesktopSample {
 public:
  static auto run(const ExampleOptions& options) -> int {
    auto factory = DesktopExample::factory(options, "arisa sample");
    if (!factory.has_value()) {
      std::println(stderr, "{}", factory.error());
      return 1;
    }
    const auto gpu = factory->stock_gpu();
    if (!gpu.has_value()) {
      std::println(stderr, "the sample needs the stock SDL_GPU provider");
      return 1;
    }
    Animation animation;
    auto renderer = SdlSampleRenderer::create(gpu->get(), animation);
    if (!renderer.has_value()) {
      std::println(stderr, "{}", renderer.error());
      return 1;
    }
    SampleModel model{animation, *renderer};
    auto application =
        runtime::Application::create(*factory, SampleScreen{model});
    if (!application.has_value()) {
      std::println(stderr, "the application could not start: {}",
                   application.error().explanation());
      return 1;
    }
#ifdef ARISA_INSPECTOR
    const inspector::Inspector inspector =
        inspector::Inspector::attach(*application);
#endif
    std::uint32_t frame = 0;
    if (auto ran = application->run_until([&](runtime::Root&) {
          ++frame;
          return options.frames().has_value() && frame >= *options.frames();
        });
        !ran.has_value()) {
      std::println(stderr, "frame {} failed: {}", frame,
                   ran.error().explanation());
      return 1;
    }
    if (options.capture().has_value()) {
      if (auto captured =
              DesktopExample::capture(factory->gpu(), *options.capture());
          !captured.has_value()) {
        std::println(stderr, "{}", captured.error());
        return 1;
      }
    }
    return 0;
  }
};

}

auto main(int argc, char* argv[]) -> int {
  const std::span arguments{argv, static_cast<std::size_t>(argc)};
  const auto options =
      arisa::examples::ExampleOptions::parse(arguments.subspan(1));
  if (!options.has_value()) {
    std::println(stderr, "{}", options.error());
    return 2;
  }
  return arisa::examples::DesktopSample::run(*options);
}
