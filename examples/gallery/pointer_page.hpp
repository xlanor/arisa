#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/point.hpp>
#include <arisa/composition/pointer_style.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::examples {

class PointerPage final : public composition::Component {
 public:
  class Opener;

  static constexpr std::array<std::string_view, 17> style_names{
      "Default", "Text",    "Vertical text", "Selection", "Grab",    "Grabbing",
      "Link",    "Zoom in", "Zoom out",      "Columns",   "Leading", "Rows",
      "Up",      "Frame",   "Corner",        "Image",     "Hidden"};
  static constexpr std::array<std::string_view, 5> effect_names{
      "Automatic", "Highlight", "Lift", "Custom", "Empty"};
  static constexpr std::size_t row_count = 30;
  static constexpr std::size_t hidden_index = 16;

  static constexpr std::string_view close_button = "pointer-close";
  static constexpr std::string_view hover_area = "pointer-hover-area";
  static constexpr std::string_view readout = "pointer-readout";
  static constexpr std::string_view wheel_list = "pointer-wheel";
  static constexpr std::string_view group_row = "pointer-group";

  static constexpr composition::StateKey<bool> inside{"pointer-inside", false};
  static constexpr composition::StateKey<std::optional<composition::Point>>
      location{"pointer-location", std::nullopt};
  static constexpr composition::Namespace linked{"pointer-linked"};

  explicit PointerPage(composition::StateHandle<bool> shown) noexcept
      : shown_(std::move(shown)) {}

  auto operator==(const PointerPage& other) const -> bool {
    return shown_.identity() == other.shown_.identity();
  }

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto tile_key(std::string_view prefix, std::size_t index)
      -> std::string;

  static auto style_of(std::size_t index) -> composition::PointerStyle;

  static auto describe(const std::optional<composition::Point>& at)
      -> std::string;

 private:
  static auto section(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto chip(std::string_view label, std::string_view key)
      -> composition::Element;

  static auto effects() -> composition::Element;

  static auto groups(composition::Namespace::ID space) -> composition::Element;

  static auto styles() -> composition::Element;

  static auto tracking(
      composition::StateHandle<bool> within,
      composition::StateHandle<std::optional<composition::Point>> where)
      -> composition::Element;

  static auto wheel() -> composition::Element;

  composition::StateHandle<bool> shown_;
};

class PointerPage::Opener final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"pointer-shown",
                                                         false};
  static constexpr std::string_view button = "pointer-open";

  auto operator==(const Opener&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;
};

}
