#include <arisa/provider/headless/scripted_input.hpp>
#include <arisa/provider/horizon/horizon_factory.hpp>
#include <arisa/provider/horizon/horizon_lifecycle.hpp>
#include <arisa/provider/providers.hpp>

#include <nxlink_log.hpp>
#include <soak.hpp>
#include <soak_lifecycle.hpp>
#include <switch_console.hpp>
#include <ui_thread.hpp>

#include <switch.h>

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace arisa::nx {

class SwitchSoak {
 public:
  static constexpr std::string_view directory = "sdmc:/arisa/soak";
  static constexpr std::chrono::minutes default_duration{120};

  static auto run(std::span<char*> arguments) -> int {
    const NxlinkLog log{"soak"};
    log.emit("startup soak");
    const std::string outcome = soak(log, options(arguments));
    log.emit("exit soak: " + outcome);
    SwitchConsole::show(outcome);
    return 0;
  }

 private:
  static auto options(std::span<char*> arguments) -> examples::Soak::Options {
    examples::Soak::Options chosen{.duration = default_duration,
                                   .interval = std::chrono::minutes{1}};
    for (std::size_t index = 1; index + 1 < arguments.size(); ++index) {
      const std::string_view argument{arguments[index]};
      if (argument != "--minutes") {
        continue;
      }
      const std::string_view text{arguments[index + 1]};
      std::uint32_t minutes = 0;
      const auto [end, failed] =
          std::from_chars(text.data(), text.data() + text.size(), minutes);
      if (failed == std::errc{} && end == text.data() + text.size() &&
          minutes > 0) {
        chosen.duration = std::chrono::minutes{minutes};
      }
    }
    return chosen;
  }

  static auto soak(const NxlinkLog& log, examples::Soak::Options chosen)
      -> std::string {
    provider::headless::ScriptedInput input;
    provider::horizon::HorizonLifecycle applet;
    examples::SoakLifecycle lifecycle{
        applet, [&log](provider::LifecycleEvent event) {
          log.emit(std::format("soak applet {}",
                               examples::SoakLifecycle::name(event)));
        }};
    auto factory =
        provider::horizon::HorizonProviderFactory::create(provider::Providers{
            .lifecycle = std::ref(lifecycle), .input = std::ref(input)});
    if (!factory.has_value()) {
      return std::string{"HorizonProviderFactory failed: "} +
             factory.error().explanation();
    }
    const std::string path =
        std::format("{}/{:%Y%m%d-%H%M%S}.csv", directory,
                    std::chrono::floor<std::chrono::seconds>(
                        std::chrono::system_clock::now()));
    std::error_code failed;
    std::filesystem::create_directories(directory, failed);
    std::ofstream out{path};
    if (!out) {
      return "could not write " + path;
    }
    out << examples::Soak::csv_header() << '\n';
    out.flush();
    log.emit(std::format(
        "soak csv {} for {} minutes", path,
        std::chrono::duration_cast<std::chrono::minutes>(chosen.duration)
            .count()));
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad{};
    padInitializeDefault(&pad);
    bool stopped = false;
    examples::Soak soak{
        *factory,
        input,
        [&pad, &stopped] {
          if (!appletMainLoop()) {
            return false;
          }
          padUpdate(&pad);
          stopped = (padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0;
          return !stopped;
        },
        true,
        std::cref(lifecycle),
        [&log, &out](const examples::Soak::Row& row) {
          log.emit(examples::Soak::describe(row));
          out << examples::Soak::csv(row) << '\n';
          out.flush();
        }};
    const auto report = soak.run(chosen);
    if (!report.has_value()) {
      return std::string{"soak failed: "} + report.error().explanation();
    }
    const bool growing = examples::Soak::growing(report->settled_live);
    return std::format(
        "{} after {} rows{}; wrote {}", stopped ? "stopped with +" : "finished",
        report->rows.size(),
        growing ? ", live allocations grew every cycle" : "", path);
  }
};

}

auto main(int argc, char** argv) -> int {
  return arisa::nx::UiThread::run([argc, argv] {
    return arisa::nx::SwitchSoak::run(
        std::span{argv, static_cast<std::size_t>(argc)});
  });
}
