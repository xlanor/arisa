#include <atomic>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <format>
#include <optional>
#include <print>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

#if defined(ARISA_CONSUMER_MODULE)
import arisa;
#else
#include <arisa/arisa.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#if defined(__SWITCH__)
#include <arisa/provider/horizon/horizon_factory.hpp>
#elif defined(ARISA_CONSUMER_WINDOW)
#include <arisa/provider/sdl3/sdl3_factory.hpp>
#endif
#endif

namespace consumer {

using namespace arisa::markup;
using arisa::composition::Element;
using arisa::theme::ColourRole;

class ConsumerScreen final : public arisa::composition::Component {
 public:
  static constexpr arisa::composition::StateKey<int> presses{"presses", 0};
  static constexpr arisa::composition::StateKey<bool> lights{"lights", true};
  static constexpr arisa::composition::StateKey<double> volume{"volume", 0.5};

  explicit ConsumerScreen(std::atomic<bool>& quit) noexcept : quit_(&quit) {}

  auto operator==(const ConsumerScreen&) const -> bool = default;

  auto body(arisa::composition::Context& context) const -> Element override {
    const auto pressed = context.state(presses);
    const auto lit = context.state(lights);
    const auto level = context.state(volume);
    std::atomic<bool>* quit = quit_;
    return VStack(alignment::leading, spacing(arisa::theme::space::md),
                  padding(arisa::theme::space::xl))(
        Text(font::title)("arisa consumer"),
        Text(foreground_style(ColourRole::text_secondary))(
            std::format("Pressed {} times", pressed.get())),
        Button(action([pressed] { pressed.set(pressed.get() + 1); }))("Press"),
        Toggle("Lights", lit), Slider(level),
        Button(role(ButtonRole::destructive),
               action([quit] { quit->store(true); }))("Quit"));
  }

 private:
  std::atomic<bool>* quit_;
};

class ConsumerApp {
 public:
  explicit ConsumerApp(std::span<char* const> arguments) {
    for (std::size_t index = 0; index < arguments.size(); ++index) {
      const std::string_view argument{arguments[index]};
      if (argument == "--headless") {
        headless_ = true;
      } else if (argument == "--offscreen") {
        offscreen_ = true;
      } else if (argument == "--frames" && index + 1 < arguments.size()) {
        const std::string_view count{arguments[++index]};
        std::uint32_t parsed = 0;
        if (std::from_chars(count.data(), count.data() + count.size(), parsed)
                .ec == std::errc{}) {
          frames_ = parsed;
        }
      }
    }
  }

  auto run() -> int {
    if (headless_) {
      arisa::provider::headless::HeadlessProviderFactory factory;
      return drive(factory);
    }
#if defined(__SWITCH__)
    auto factory = arisa::provider::horizon::HorizonProviderFactory::create();
#elif defined(ARISA_CONSUMER_WINDOW)
    const arisa::provider::SurfaceSize size{
        .width = arisa::provider::SurfaceWidth{1280},
        .height = arisa::provider::SurfaceHeight{720}};
    arisa::provider::sdl3::SurfaceMode mode = arisa::provider::sdl3::WindowMode{
        .title = "arisa consumer", .size = size};
    if (offscreen_) {
      mode = arisa::provider::sdl3::OffscreenMode{.size = size};
    }
    auto factory =
        arisa::provider::sdl3::Sdl3ProviderFactory::create({}, std::move(mode));
#else
    std::println(stderr, "this build has no window provider; pass --headless");
    return 2;
#endif
#if defined(__SWITCH__) || defined(ARISA_CONSUMER_WINDOW)
    if (!factory.has_value()) {
      std::println(stderr, "the providers failed: {}",
                   factory.error().description());
      return 1;
    }
    return drive(*factory);
#endif
  }

 private:
  auto drive(arisa::provider::ProviderFactory& factory) -> int {
    std::atomic<bool> quit{false};
    auto application =
        arisa::runtime::Application::create(factory, ConsumerScreen{quit});
    if (!application.has_value()) {
      std::println(stderr, "the application could not start: {}",
                   application.error().description());
      return 1;
    }
    std::uint32_t frame = 0;
    if (auto ran = application->run_until([&](arisa::runtime::Root&) {
          ++frame;
          return quit.load() || (frames_.has_value() && frame >= *frames_);
        });
        !ran.has_value()) {
      std::println(stderr, "frame {} failed: {}", frame,
                   ran.error().description());
      return 1;
    }
    std::println("arisa consumer ran {} frames", frame);
    return 0;
  }

  bool headless_{false};
  bool offscreen_{false};
  std::optional<std::uint32_t> frames_;
};

}

auto main(int argc, char* argv[]) -> int {
  const std::span arguments{argv, static_cast<std::size_t>(argc)};
  return consumer::ConsumerApp{arguments.subspan(1)}.run();
}
