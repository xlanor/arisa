#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/theme/color_scheme.hpp>

#include <string_view>

namespace arisa::examples {

class ShapesPage final : public composition::Component {
 public:
  class Opener;

  struct Star {
    int points{5};

    auto path(composition::Rect rect) const -> graphics::Path;

    friend auto operator==(const Star&, const Star&) -> bool = default;
  };

  static constexpr std::string_view close_button = "shapes-close";
  static constexpr std::string_view concentric_card = "shapes-concentric";
  static constexpr std::string_view uneven_button = "shapes-uneven-button";

  explicit ShapesPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const ShapesPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto card(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto basics() -> composition::Element;

  static auto concentric() -> composition::Element;

  static auto well() -> composition::Element;

  static auto gradient_text() -> composition::Element;

  static auto tiers(theme::ColorScheme scheme) -> composition::Element;

  static auto gradients() -> composition::Element;

  static auto transforms() -> composition::Element;

  static auto styles() -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class ShapesPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"shapes-shown", false};
  static constexpr std::string_view button = "shapes-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
