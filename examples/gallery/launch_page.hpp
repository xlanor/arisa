#pragma once

#include <launch_screen.hpp>

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>

#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace arisa::examples {

class LaunchPage final : public composition::Component {
 public:
  using Replay =
      std::shared_ptr<const std::function<void(LaunchScreen::Style)>>;

  static constexpr std::string_view fade_button = "launch-fade";
  static constexpr std::string_view erase_button = "launch-erase";
  static constexpr std::string_view gradient_button = "launch-gradient";
  static constexpr std::string_view glass_button = "launch-glass";

  LaunchPage(Replay replay, bool lettering_font) noexcept
      : replay_(std::move(replay)), lettering_font_(lettering_font) {}

  auto operator==(const LaunchPage& other) const -> bool {
    return replay_ == other.replay_ && lettering_font_ == other.lettering_font_;
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  Replay replay_;
  bool lettering_font_;
};

}
