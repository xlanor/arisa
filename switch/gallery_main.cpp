#include <arisa/appearance/theme_watcher.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/diagnostics/crash_reporter.hpp>
#include <arisa/provider/horizon/horizon_factory.hpp>
#include <arisa/provider/standard/directory_storage.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/theme/seed.hpp>
#include <display_report.hpp>
#include <exit_trail.hpp>
#include <frame_capture.hpp>
#include <frame_meter.hpp>
#include <frame_report.hpp>
#include <gallery_fonts.hpp>
#include <gallery_page.hpp>
#include <gallery_photo.hpp>
#include <gallery_screen.hpp>
#include <input_blocker.hpp>
#include <launch_screen.hpp>
#include <nxlink_log.hpp>
#include <switch_console.hpp>
#include <ui_thread.hpp>

#ifdef ARISA_INSPECTOR
#include <arisa/inspector/inspector.hpp>
#endif

#include <switch.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace arisa::nx {

class SwitchGallery {
 public:
  static constexpr std::string_view glass_flag = "--debug-glass-coverage";
  static constexpr const char* glass_marker =
      "sdmc:/config/arisa-gallery/debug-glass-coverage";
  static constexpr std::size_t described_keys = 3;
  static constexpr std::string_view crash_directory =
      "sdmc:/switch/arisa/crashes";

  static auto run(std::span<char*> arguments) -> int {
    NxlinkLog log{"gallery"};
    const auto crashes = provider::CrashReporter::install(
        {.directory = crash_directory, .app = "gallery"});
    if (!crashes.has_value()) {
      log.emit("crash reporter unavailable");
    }
    ExitTrail::begin("gallery");
    log.emit("startup gallery");
    const bool probe =
        std::ranges::any_of(arguments,
                            [](const char* argument) {
                              return argument != nullptr &&
                                     std::string_view{argument} == glass_flag;
                            }) ||
        access(glass_marker, F_OK) == 0;
    if (probe) {
      log.emit("glass coverage probe on");
    }
    std::string failure = render(log, probe);
    if (failure.empty()) {
      log.emit("exit gallery: ok");
      ExitTrail::mark("render finished");
      return 0;
    }
    log.emit("exit gallery: " + failure);
    SwitchConsole::show(failure);
    return 1;
  }

 private:
  static auto describer(runtime::Application& application)
      -> FrameReport::Describe {
    return [&application](std::uint64_t id) -> std::string {
      composition::Tree& tree = application.root().tree();
      std::optional<composition::NodeId> at = composition::NodeId{id};
      std::size_t depth = 0;
      std::string keys;
      std::size_t found = 0;
      while (at.has_value() && found < described_keys) {
        const auto node = tree.node(*at);
        if (!node.has_value()) {
          return depth == 0 ? std::string{"gone"} : std::string{"detached"};
        }
        if (const auto& key = node->get().key(); key.has_value()) {
          keys += std::format("{}key {} up {}", found == 0 ? "" : " < ",
                              key->description(), depth);
          ++found;
        }
        at = node->get().parent();
        ++depth;
      }
      return keys.empty() ? std::string{"unkeyed"} : keys;
    };
  }

  static auto render(NxlinkLog& log, bool probe) -> std::string {
    provider::Providers overrides;
    overrides.diagnostics = log;
    auto factory = provider::horizon::HorizonProviderFactory::create(overrides);
    if (!factory.has_value()) {
      return std::string{"HorizonProviderFactory failed: "} +
             factory.error().explanation();
    }
    examples::FrameMeter meter;
    composition::Observable<std::string> notices{std::string{}};
    provider::standard::DirectoryStorage storage{"sdmc:/config/arisa-gallery"};
    provider::standard::DirectoryStorage themes{"sdmc:/arisa"};
    provider::standard::DirectoryStorage card{"sdmc:/"};
    std::optional<std::chrono::milliseconds> reload;
#ifndef NDEBUG
    reload = appearance::ThemeWatcher::default_interval;
#endif
    const examples::LaunchScreen::Photo photo = examples::GalleryPhoto::load(
        factory->assets(), factory->surface().size());
    const auto blocker = std::make_shared<examples::InputBlocker>();
    auto application = runtime::Application::create(
        *factory,
        composition::Element{
            examples::GalleryScreen{
                examples::GalleryPage::typography, theme::Seed{},
                std::cref(meter), std::cref(notices),
                examples::LaunchScreen::Style{},
                examples::GalleryFonts::lettering(factory->assets()), photo}}
            .environment(examples::InputBlocker::key, blocker),
        runtime::ApplicationOptions{
            .executors = std::nullopt,
            .fonts = examples::GalleryFonts::load(factory->assets()),
            .atlas = {},
            .interaction = {},
            .appearance =
                runtime::AppearanceOptions{
                    .policy = examples::GalleryScreen::policy(std::nullopt),
                    .storage = storage,
                    .themes = themes,
                    .hot_reload = reload},
            .launch_colour = theme::Colour::hex(
                photo != nullptr ? examples::GalleryPhoto::average
                                 : examples::LaunchScreen::wallpaper[5]),
            .debug_glass_coverage = probe,
            .storage = runtime::StorageOptions{.provider = storage},
            .localization =
                runtime::LocalizationOptions{
                    .locale = std::nullopt,
                    .strings = std::string{examples::GalleryScreen::strings}},
            .files = card});
    if (!application.has_value()) {
      return std::string{"the application could not start: "} +
             application.error().explanation();
    }
    blocker->attach(application->root());
#ifdef ARISA_INSPECTOR
    inspector::Inspector inspector = inspector::Inspector::attach(*application);
#endif
    FrameReport report{log, "gallery", describer(*application)};
    FrameReport launching{log, "launch", describer(*application)};
    DisplayReport display{log};
    FrameCapture capture{log};
    PadState pad{};
    padInitializeDefault(&pad);
    bool writing = true;
    std::optional<std::uint64_t> allocated = provider::AllocationMeter::count();
    if (auto ran = application->run_until([&](runtime::Root&) {
          const runtime::FrameTimings& timings = application->last_frame();
          const std::optional<std::uint64_t> counted =
              provider::AllocationMeter::count();
          const std::optional<std::uint64_t> allocations =
              counted.has_value() && allocated.has_value()
                  ? std::optional{*counted - *allocated}
                  : std::nullopt;
          const auto gpu =
              factory->stock_gpu<provider::horizon::Console::Gpu>();
          if (writing || timings.evaluations > 0) {
            writing = application->root()
                          .tree()
                          .find(composition::ElementKey{
                              examples::LaunchScreen::root_view})
                          .has_value();
          }
          (writing ? launching : report)
              .record(FrameReport::Sample{
                  .cpu = timings.cpu(),
                  .layout = timings.layout,
                  .input = timings.input,
                  .present = timings.present,
                  .laid_out = timings.laid_out,
                  .presented = timings.presented,
                  .missed_vsyncs =
                      gpu.has_value() ? gpu->get().missed_vsyncs() : 0,
                  .paced = timings.paced,
                  .animating = timings.animating,
                  .culled = timings.culled,
                  .occluded = timings.occluded,
                  .frame_rate = timings.frame_rate,
                  .due = timings.due,
                  .now = factory->clock().now().since_origin(),
                  .effects_gpu = timings.effects_gpu,
                  .interface_gpu = timings.interface_gpu,
                  .hooks_gpu = timings.hooks_gpu,
                  .backdrops = timings.backdrops,
                  .blurs = timings.blurs,
                  .blur_hits = timings.blur_hits,
                  .effect_passes = timings.effect_passes,
                  .layer_renders = timings.layer_renders,
                  .unhinted = timings.unhinted,
                  .frame = timings,
                  .allocations = allocations,
                  .focus = application->focused().transform(
                      [](composition::NodeId focused) {
                        return focused.value();
                      })});
          meter.record(timings, factory->clock().now().since_origin());
          padUpdate(&pad);
          const std::uint64_t pressed = padGetButtonsDown(&pad);
          display.observe(factory->surface(), nwindowGetDefault(),
                          application->root().tree().root().frame());
          if (auto saved =
                  capture.poll(padGetButtons(&pad), pressed, factory->gpu())) {
            notices.set(std::move(*saved));
          }
          allocated = provider::AllocationMeter::count();
          return !appletMainLoop() || (pressed & HidNpadButton_Plus) != 0;
        });
        !ran.has_value()) {
      return std::string{"frame failed: "} + ran.error().explanation();
    }
    return {};
  }
};

}

auto main(int argc, char** argv) -> int {
  return arisa::nx::UiThread::run([argc, argv] {
    return arisa::nx::SwitchGallery::run(
        std::span{argv, static_cast<std::size_t>(argc)});
  });
}
