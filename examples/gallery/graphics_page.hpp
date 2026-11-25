#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/gpu/image.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace arisa::examples {

class GraphicsPage final : public composition::Component {
 public:
  class Opener;
  class Clock;

  struct Turn {
    double degrees{0.0};

    friend auto operator==(const Turn&, const Turn&) -> bool = default;
  };

  static constexpr std::string_view close_button = "graphics-close";
  static constexpr std::string_view flip_button = "graphics-flip";
  static constexpr std::string_view play_button = "graphics-play";
  static constexpr std::string_view export_button = "graphics-export";
  static constexpr std::string_view blur_toggle = "graphics-blur";
  static constexpr std::string_view unavailable_tile = "graphics-unavailable";
  static constexpr std::string_view export_path = "arisa_render.png";

  static constexpr composition::StateKey<bool> flipped{"graphics-flipped",
                                                       false};
  static constexpr composition::StateKey<bool> playing{"graphics-playing",
                                                       false};
  static constexpr composition::StateKey<bool> blurred{"graphics-blurred",
                                                       true};
  static const composition::StateKey<std::string> exported;
  static const composition::StateKey<std::shared_ptr<const provider::Image>>
      rendered;

  explicit GraphicsPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const GraphicsPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto badge() -> composition::Element;

 private:
  static auto card(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto tile(std::uint32_t rgb, std::string_view name)
      -> composition::Element;

  static auto unavailable() -> composition::Element;

  static auto adjustments() -> composition::Element;

  static auto blur(composition::StateHandle<bool> on) -> composition::Element;

  static auto masks() -> composition::Element;

  static auto blends() -> composition::Element;

  static auto flip(composition::StateHandle<bool> turned)
      -> composition::Element;

  static auto waveform(composition::StateHandle<bool> running)
      -> composition::Element;

  static auto ripple(composition::StateHandle<bool> running)
      -> composition::Element;

  static auto renderer(
      composition::StateHandle<std::string> status,
      composition::StateHandle<std::shared_ptr<const provider::Image>> image)
      -> composition::Element;

  static auto edges() -> composition::Element;

  static auto saved(const provider::Image& image) -> std::string;

  composition::StateHandle<bool> shown_;
};

class GraphicsPage::Clock final : public composition::Component {
 public:
  using Content = std::function<composition::Element(double)>;

  Clock(bool running, Content content)
      : running_(running), content_(std::move(content)) {}

  auto operator==(const Clock&) const -> bool { return false; }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  bool running_;
  Content content_;
};

class GraphicsPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"graphics-shown",
                                                         false};
  static constexpr std::string_view button = "graphics-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
