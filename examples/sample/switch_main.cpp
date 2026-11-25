#include <animation.hpp>
#include <nxlink_log.hpp>
#include <sample_model.hpp>
#include <sample_screen.hpp>
#include <switch_console.hpp>
#include <ui_thread.hpp>

#include <arisa/provider/diagnostics/crash_reporter.hpp>
#include <arisa/provider/horizon/horizon_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/root.hpp>

#ifdef ARISA_INSPECTOR
#include <arisa/inspector/inspector.hpp>
#endif

#if !defined(ARISA_HORIZON_DEKO3D)
#include <vulkan_sample_renderer.hpp>
#else
#include <deko3d_sample_renderer.hpp>
#endif

#include <switch.h>

#include <string>

namespace arisa::nx {

class SwitchSample {
 public:
#if !defined(ARISA_HORIZON_DEKO3D)
  using Renderer = examples::VulkanSampleRenderer;
#else
  using Renderer = examples::Deko3dSampleRenderer;
#endif

  static auto run() -> int {
    const NxlinkLog log{"sample"};
    const auto crashes = provider::CrashReporter::install(
        {.directory = "sdmc:/switch/arisa/crashes", .app = "sample"});
    if (!crashes.has_value()) {
      log.emit("crash reporter unavailable");
    }
    log.emit("startup sample");
    const std::string failure = render();
    if (failure.empty()) {
      log.emit("exit sample: ok");
      return 0;
    }
    log.emit("exit sample: " + failure);
    SwitchConsole::show(failure);
    return 1;
  }

 private:
  static auto render() -> std::string {
    auto factory = provider::horizon::HorizonProviderFactory::create();
    if (!factory.has_value()) {
      return std::string{"HorizonProviderFactory failed: "} +
             factory.error().explanation();
    }
    const auto gpu = factory->stock_gpu<provider::horizon::Console::Gpu>();
    if (!gpu.has_value()) {
      return std::string{"the sample needs the stock console gpu provider"};
    }
    examples::Animation animation;
    auto renderer = Renderer::create(gpu->get(), animation);
    if (!renderer.has_value()) {
      return renderer.error();
    }
    examples::SampleModel model{animation, *renderer};
    auto application =
        runtime::Application::create(*factory, examples::SampleScreen{model});
    if (!application.has_value()) {
      return std::string{"the application could not start: "} +
             application.error().explanation();
    }
#ifdef ARISA_INSPECTOR
    const inspector::Inspector inspector =
        inspector::Inspector::attach(*application);
#endif
    PadState pad{};
    padInitializeDefault(&pad);
    if (auto ran = application->run_until([&pad](runtime::Root&) {
          padUpdate(&pad);
          return !appletMainLoop() ||
                 (padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0;
        });
        !ran.has_value()) {
      return std::string{"frame failed: "} + ran.error().explanation();
    }
    return {};
  }
};

}

auto main() -> int {
  return arisa::nx::UiThread::run(
      [] { return arisa::nx::SwitchSample::run(); });
}
