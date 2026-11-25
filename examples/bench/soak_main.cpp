#include <arisa/provider/headless/headless_factory.hpp>

#include <host_assets.hpp>
#include <soak.hpp>

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <fstream>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace arisa::examples {

class HostSoak {
 public:
  static constexpr std::chrono::milliseconds frame_step{16};

  static auto run(std::span<char*> arguments) -> int {
    Soak::Options options{.duration = std::chrono::seconds{300},
                          .interval = std::chrono::seconds{30}};
    std::string csv;
    for (std::size_t index = 1; index < arguments.size(); ++index) {
      const std::string_view argument{arguments[index]};
      const bool valued = index + 1 < arguments.size();
      if (argument == "--duration" && valued) {
        const auto seconds = number(arguments[++index]);
        if (!seconds.has_value()) {
          return usage();
        }
        options.duration = std::chrono::seconds{*seconds};
      } else if (argument == "--interval" && valued) {
        const auto seconds = number(arguments[++index]);
        if (!seconds.has_value() || *seconds == 0) {
          return usage();
        }
        options.interval = std::chrono::seconds{*seconds};
      } else if (argument == "--csv" && valued) {
        csv = arguments[++index];
      } else {
        return usage();
      }
    }
    provider::headless::HeadlessProviderFactory providers;
    HostAssets::load(providers);
    std::optional<std::ofstream> out;
    if (!csv.empty()) {
      out.emplace(csv);
      *out << Soak::csv_header() << '\n';
    }
    Diagnosed diagnosed;
    Soak soak{providers,
              providers.input(),
              [&providers, &diagnosed] {
                const auto reported = providers.diagnostics().take();
                diagnosed.count += reported.size();
                if (!reported.empty() && diagnosed.first.empty()) {
                  diagnosed.first = reported.front().message;
                }
                return providers.clock().advance(frame_step).has_value();
              },
              false,
              std::nullopt,
              [&out, &diagnosed](const Soak::Row& row) {
                std::println("{}, diagnostics {}", Soak::describe(row),
                             diagnosed.count);
                if (!diagnosed.first.empty()) {
                  std::println("  first: {}", diagnosed.first);
                }
                diagnosed = Diagnosed{};
                if (out.has_value()) {
                  *out << Soak::csv(row) << '\n';
                }
              }};
    const auto report = soak.run(options);
    if (!report.has_value()) {
      std::println(stderr, "soak: {}", report.error().explanation());
      return 1;
    }
    std::string settled;
    for (const std::int64_t live : report->settled_live) {
      settled += std::format(" {}", live);
    }
    std::println("soak settled live allocations per cycle:{}", settled);
    if (Soak::growing(report->settled_live)) {
      std::println(stderr,
                   "soak: live allocations grew after every cycle, a leak");
      return 1;
    }
    return 0;
  }

 private:
  struct Diagnosed {
    std::size_t count{0};
    std::string first;
  };

  static auto number(std::string_view text) -> std::optional<std::uint32_t> {
    std::uint32_t value = 0;
    const auto [end, failed] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    if (failed != std::errc{} || end != text.data() + text.size()) {
      return std::nullopt;
    }
    return value;
  }

  static auto usage() -> int {
    std::println(stderr,
                 "usage: arisa_soak [--duration seconds] [--interval seconds] "
                 "[--csv path]");
    return 2;
  }
};

}

auto main(int argc, char** argv) -> int {
  return arisa::examples::HostSoak::run(
      std::span{argv, static_cast<std::size_t>(argc)});
}
