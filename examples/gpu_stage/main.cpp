#include <arisa/composition/element_key.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/root.hpp>
#include <desktop_example.hpp>
#include <example_options.hpp>
#include <gpu_stage_screen.hpp>
#include <sdl_nv12_video.hpp>
#include <sdl_plasma.hpp>
#include <sdl_video.hpp>
#include <stage_control.hpp>

#ifdef ARISA_INSPECTOR
#include <arisa/inspector/inspector.hpp>
#include <arisa/inspector/inspector_panel.hpp>
#endif

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <print>
#include <span>
#include <string>

namespace arisa::examples {

class GpuStageApp {
 public:
  static constexpr std::uint32_t settle_frames = 4;
  static constexpr std::uint32_t enter_frame = 4;
  static constexpr std::uint32_t leave_frame = 16;

  struct ExclusiveScript {
    std::optional<composition::NodeId> focus_before;
    bool focus_returned{false};
    bool stayed_exclusive{true};
    std::uint32_t shared_quads{0};
    std::uint32_t exclusive_quads{0};

    auto passed() const noexcept -> bool {
      return stayed_exclusive && focus_returned && exclusive_quads > 0 &&
             exclusive_quads < shared_quads;
    }
  };

  static void exclusive_step(runtime::Application& application,
                             runtime::Root& root, StageControl& control,
                             std::uint32_t frame, ExclusiveScript& script) {
    const runtime::FrameTimings& last = application.last_frame();
    if (frame < enter_frame) {
      if (last.presented) {
        script.shared_quads = std::max(script.shared_quads, last.quads);
      }
      return;
    }
    if (frame == enter_frame) {
      script.focus_before = application.focused();
      root.activate(
          composition::ElementKey{GpuStageScreen::full_screen_button});
      return;
    }
    if (frame < leave_frame) {
      script.stayed_exclusive = script.stayed_exclusive && last.exclusive;
      if (last.presented) {
        script.exclusive_quads = std::max(script.exclusive_quads, last.quads);
      }
      return;
    }
    if (frame == leave_frame) {
      control.set_full_screen(false);
      return;
    }
    if (frame > leave_frame + 2) {
      script.focus_returned =
          !last.exclusive && application.focused() == script.focus_before;
    }
  }

  static auto run(const ExampleOptions& options) -> int {
    auto factory = DesktopExample::factory(options, "arisa gpu stage");
    if (!factory.has_value()) {
      std::println(stderr, "{}", factory.error());
      return 1;
    }
    const auto gpu = factory->stock_gpu();
    if (!gpu.has_value()) {
      std::println(stderr, "the gpu stage needs the stock SDL_GPU provider");
      return 1;
    }
    StageControl control;
    auto plasma = SdlPlasma::create(gpu->get(), control);
    if (!plasma.has_value()) {
      std::println(stderr, "{}", plasma.error());
      return 1;
    }
    control.bind(plasma->hook());
    control.bind_surfaces(plasma->texture_surface(),
                          plasma->underlay_surface());
    auto video = SdlVideo::create(gpu->get());
    if (!video.has_value()) {
      std::println(stderr, "{}", video.error());
      return 1;
    }
    auto nv12 = SdlNv12Video::create(gpu->get(), control);
    if (!nv12.has_value()) {
      std::println(stderr, "{}", nv12.error());
      return 1;
    }
    if (options.page() == std::optional<std::string>{"video"}) {
      control.set_page(StageControl::Page::video);
    }
    if (options.page() == std::optional<std::string>{"surface"}) {
      control.set_page(StageControl::Page::surface);
    }
    if (options.page() == std::optional<std::string>{"base"}) {
      control.set_page(StageControl::Page::video);
      control.set_under(true);
    }
    const bool scripted_exclusive =
        options.page() == std::optional<std::string>{"exclusive"};
    if (options.page() == std::optional<std::string>{"nv12"}) {
      control.set_page(StageControl::Page::video);
      control.set_video_format(StageControl::VideoFormat::nv12);
    }
    auto application =
        runtime::Application::create(*factory, GpuStageScreen{control});
    if (!application.has_value()) {
      std::println(stderr, "the application could not start: {}",
                   application.error().explanation());
      return 1;
    }
#ifdef ARISA_INSPECTOR
    inspector::Inspector inspector = inspector::Inspector::attach(*application);
    std::optional<inspector::InspectorPanel> inspected;
    if (options.inspector().has_value()) {
      inspected = inspector::InspectorPanels::parse(*options.inspector());
      if (!inspected.has_value()) {
        std::println(stderr, "unknown inspector panel {}",
                     *options.inspector());
        return 2;
      }
    }
#endif
    const std::uint32_t pause_at = options.script();
    std::uint32_t frame = 0;
    std::uint32_t paused_presents = 0;
    ExclusiveScript script;
    if (auto ran = application->run_until([&](runtime::Root& root) {
          ++frame;
          if (control.take_change() || control.producing()) {
            const auto now = factory->clock().now().since_origin();
            static_cast<void>(control.video_format() ==
                                      StageControl::VideoFormat::nv12
                                  ? nv12->produce(control, now)
                                  : video->produce(control, now));
          }
          if (scripted_exclusive) {
            exclusive_step(*application, root, control, frame, script);
          }
#ifdef ARISA_INSPECTOR
          if (inspected.has_value() && frame == settle_frames) {
            inspector.open();
            inspector.show(*inspected);
          }
#endif
          if (pause_at > 0 && frame == pause_at) {
            root.activate(composition::ElementKey{GpuStageScreen::play_button});
          }
          if (pause_at > 0 && frame > pause_at + settle_frames &&
              application->last_frame().presented) {
            ++paused_presents;
          }
          return options.frames().has_value() && frame >= *options.frames();
        });
        !ran.has_value()) {
      std::println(stderr, "frame {} failed: {}", frame,
                   ran.error().explanation());
      return 1;
    }
    if (scripted_exclusive) {
      std::println("quads shared {} exclusive {}, focus returned {}",
                   script.shared_quads, script.exclusive_quads,
                   script.focus_returned);
      if (!script.passed()) {
        return 1;
      }
    }
    if (pause_at > 0) {
      std::println("presents while paused: {}", paused_presents);
      if (paused_presents > 0) {
        return 1;
      }
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
  return arisa::examples::GpuStageApp::run(*options);
}
