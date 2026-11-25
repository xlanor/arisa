#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/theme/colour_value.hpp>

#include <cstdint>
#include <string_view>

namespace arisa::examples {

class CanvasPage final : public composition::Component {
 public:
  class Opener;

  static constexpr std::string_view close_button = "canvas-close";

  explicit CanvasPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const CanvasPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto card(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto labelled(std::string_view name, composition::Element shown)
      -> composition::Element;

  static auto swatch(theme::ColourValue colour) -> composition::Element;

  static auto booleans() -> composition::Element;

  static auto circles() -> composition::Element;

  static auto porter_duff() -> composition::Element;

  static auto filters() -> composition::Element;

  static auto shader_arrays() -> composition::Element;

  static auto colours() -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class CanvasPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"canvas-shown", false};
  static constexpr std::string_view button = "canvas-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
