#pragma once

#include <arisa/components/button.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/runtime/introspection.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/space.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::inspector {

class InspectorStyle {
 public:
  static constexpr theme::ColourRole outline = theme::ColourRole::accent;
  static constexpr theme::ColourRole hover = theme::ColourRole::orange;
  static constexpr float outline_width = 2.0F;

  static auto heading(std::string_view text) -> composition::Element;

  static auto line(std::string text) -> composition::Element;

  static auto value(std::string_view label, std::string text)
      -> composition::Element;

  static auto section(std::string_view title,
                      std::vector<composition::Element> rows)
      -> composition::Element;

  static auto tab(std::string_view label, bool selected,
                  components::Button::Action action) -> composition::Element;

  static auto icon(std::string_view label, std::string_view symbol,
                   bool selected, components::Button::Action action)
      -> composition::Element;

  static auto bytes(std::uint64_t count) -> std::string;

  static auto milliseconds(std::chrono::nanoseconds value) -> std::string;

  static auto rect(const composition::Rect& rect) -> std::string;

  static auto insets(const std::optional<theme::Insets>& insets) -> std::string;

  static auto flags(const runtime::Introspection::Flags& flags) -> std::string;

 private:
  static auto space(const theme::Space& space) -> std::string;
};

}
