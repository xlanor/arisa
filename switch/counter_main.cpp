#include <arisa/provider/horizon/horizon_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/runtime/text_input.hpp>
#include <counter_screen.hpp>
#include <frame_report.hpp>
#include <nxlink_log.hpp>
#include <switch_console.hpp>
#include <ui_thread.hpp>

#include <switch.h>

#include <string>

namespace arisa::nx {

class SwitchCounter {
 public:
  static auto run() -> int {
    const NxlinkLog log{"counter"};
    log.emit("startup counter");
    std::string failure = render(log);
    if (failure.empty()) {
      log.emit("exit counter: ok");
      return 0;
    }
    log.emit("exit counter: " + failure);
    SwitchConsole::show(failure);
    return 1;
  }

 private:
  static auto render(const NxlinkLog& log) -> std::string {
    auto factory = provider::horizon::HorizonProviderFactory::create();
    if (!factory.has_value()) {
      return std::string{"HorizonProviderFactory failed: "} +
             factory.error().explanation();
    }
    auto application = runtime::Application::create(
        *factory,
        examples::CounterScreen{runtime::TextInput{factory->text_input()}});
    if (!application.has_value()) {
      return std::string{"the application could not start: "} +
             application.error().explanation();
    }
    FrameReport report{log, "counter"};
    PadState pad{};
    padInitializeDefault(&pad);
    if (auto ran = application->run_until([&](runtime::Root&) {
          const runtime::FrameTimings& timings = application->last_frame();
          const auto gpu =
              factory->stock_gpu<provider::horizon::Console::Gpu>();
          report.record(FrameReport::Sample{
              .cpu = timings.cpu(),
              .layout = timings.layout,
              .input = timings.input,
              .present = timings.present,
              .laid_out = timings.laid_out,
              .presented = timings.presented,
              .missed_vsyncs =
                  gpu.has_value() ? gpu->get().missed_vsyncs() : 0});
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
      [] { return arisa::nx::SwitchCounter::run(); });
}
