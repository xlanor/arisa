#include <arisa/composition/element_key.hpp>
#include <arisa/provider/headless/scripted_text_input.hpp>
#include <arisa/provider/providers.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/runtime/text_input.hpp>
#include <counter_screen.hpp>
#include <desktop_example.hpp>
#include <example_options.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <print>
#include <span>
#include <string>

namespace arisa::examples {

class CounterApp {
 public:
  static auto run(const ExampleOptions& options) -> int {
    const std::uint32_t script = options.script();
    provider::headless::ScriptedTextInput scripted;
    scripted.push(std::string{"arisa"});
    provider::Providers overrides;
    if (script > 0) {
      overrides.text_input = scripted;
    }
    auto factory = DesktopExample::factory(options, "arisa counter", overrides);
    if (!factory.has_value()) {
      std::println(stderr, "{}", factory.error());
      return 1;
    }
    auto application = runtime::Application::create(
        *factory, CounterScreen{runtime::TextInput{factory->text_input()}});
    if (!application.has_value()) {
      std::println(stderr, "the application could not start: {}",
                   application.error().description());
      return 1;
    }
    const std::optional<std::uint32_t> limit =
        options.frames().transform([script](std::uint32_t frames) {
          return std::max(frames, script + 1);
        });
    std::uint32_t frame = 0;
    if (auto ran = application->run_until([&](runtime::Root& root) {
          ++frame;
          if (frame <= script) {
            root.activate(composition::ElementKey{CounterScreen::increment});
          }
          if (frame == 1 && script > 0) {
            root.activate(composition::ElementKey{CounterScreen::name_button});
            root.activate(composition::ElementKey{CounterScreen::fail_button});
          }
          return limit.has_value() && frame >= *limit;
        });
        !ran.has_value()) {
      std::println(stderr, "frame {} failed: {}", frame,
                   ran.error().description());
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
  return arisa::examples::CounterApp::run(*options);
}
