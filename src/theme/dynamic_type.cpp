#include <arisa/theme/dynamic_type.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>

namespace arisa::theme {

auto DynamicType::row(TextRole role) noexcept -> const Row& {
  static constexpr std::array<Row, text_role_count> table{{
      Row{.sizes = {31.0F, 32.0F, 33.0F, 34.0F, 36.0F, 38.0F, 40.0F, 44.0F,
                    48.0F, 52.0F, 56.0F, 60.0F},
          .line_heights = {36.994F, 38.188F, 39.381F, 40.574F, 42.961F, 45.348F,
                           47.734F, 52.508F, 57.281F, 62.055F, 66.828F,
                           71.602F},
          .weight = 400},
      Row{.sizes = {25.0F, 26.0F, 27.0F, 28.0F, 30.0F, 32.0F, 34.0F, 38.0F,
                    43.0F, 48.0F, 53.0F, 58.0F},
          .line_heights = {29.834F, 31.027F, 32.221F, 33.414F, 35.801F, 38.188F,
                           40.574F, 45.348F, 51.314F, 57.281F, 63.248F,
                           69.215F},
          .weight = 400},
      Row{.sizes = {19.0F, 20.0F, 21.0F, 22.0F, 24.0F, 26.0F, 28.0F, 34.0F,
                    39.0F, 44.0F, 50.0F, 56.0F},
          .line_heights = {22.674F, 23.867F, 25.061F, 26.254F, 28.641F, 31.027F,
                           33.414F, 40.574F, 46.541F, 52.508F, 59.668F,
                           66.828F},
          .weight = 400},
      Row{.sizes = {17.0F, 18.0F, 19.0F, 20.0F, 22.0F, 24.0F, 26.0F, 31.0F,
                    37.0F, 43.0F, 49.0F, 55.0F},
          .line_heights = {20.287F, 21.48F, 22.674F, 23.867F, 26.254F, 28.641F,
                           31.027F, 36.994F, 44.154F, 51.314F, 58.475F,
                           65.635F},
          .weight = 400},
      Row{.sizes = {14.0F, 15.0F, 16.0F, 17.0F, 19.0F, 21.0F, 23.0F, 28.0F,
                    33.0F, 40.0F, 47.0F, 53.0F},
          .line_heights = {16.707F, 17.9F, 19.094F, 20.287F, 22.674F, 25.061F,
                           27.447F, 33.414F, 39.381F, 47.734F, 56.088F,
                           63.248F},
          .weight = 600},
      Row{.sizes = {12.0F, 13.0F, 14.0F, 15.0F, 17.0F, 19.0F, 21.0F, 25.0F,
                    30.0F, 36.0F, 42.0F, 49.0F},
          .line_heights = {14.32F, 15.514F, 16.707F, 17.9F, 20.287F, 22.674F,
                           25.061F, 29.834F, 35.801F, 42.961F, 50.121F,
                           58.475F},
          .weight = 400},
      Row{.sizes = {14.0F, 15.0F, 16.0F, 17.0F, 19.0F, 21.0F, 23.0F, 28.0F,
                    33.0F, 40.0F, 47.0F, 53.0F},
          .line_heights = {16.707F, 17.9F, 19.094F, 20.287F, 22.674F, 25.061F,
                           27.447F, 33.414F, 39.381F, 47.734F, 56.088F,
                           63.248F},
          .weight = 400},
      Row{.sizes = {13.0F, 14.0F, 15.0F, 16.0F, 18.0F, 20.0F, 22.0F, 26.0F,
                    32.0F, 38.0F, 44.0F, 51.0F},
          .line_heights = {15.514F, 16.707F, 17.9F, 19.094F, 21.48F, 23.867F,
                           26.254F, 31.027F, 38.188F, 45.348F, 52.508F,
                           60.861F},
          .weight = 400},
      Row{.sizes = {12.0F, 12.0F, 12.0F, 13.0F, 15.0F, 17.0F, 19.0F, 23.0F,
                    27.0F, 33.0F, 38.0F, 44.0F},
          .line_heights = {14.32F, 14.32F, 14.32F, 15.514F, 17.9F, 20.287F,
                           22.674F, 27.447F, 32.221F, 39.381F, 45.348F,
                           52.508F},
          .weight = 400},
      Row{.sizes = {11.0F, 11.0F, 11.0F, 12.0F, 14.0F, 16.0F, 18.0F, 22.0F,
                    26.0F, 32.0F, 37.0F, 43.0F},
          .line_heights = {13.127F, 13.127F, 13.127F, 14.32F, 16.707F, 19.094F,
                           21.48F, 26.254F, 31.027F, 38.188F, 44.154F, 51.314F},
          .weight = 400},
      Row{.sizes = {11.0F, 11.0F, 11.0F, 11.0F, 13.0F, 15.0F, 17.0F, 20.0F,
                    24.0F, 29.0F, 34.0F, 40.0F},
          .line_heights = {13.127F, 13.127F, 13.127F, 13.127F, 15.514F, 17.9F,
                           20.287F, 23.867F, 28.641F, 34.607F, 40.574F,
                           47.734F},
          .weight = 400},
  }};
  return table[std::to_underlying(role)];
}

auto DynamicType::names() noexcept
    -> const std::array<std::string_view, dynamic_type_size_count>& {
  static constexpr std::array<std::string_view, dynamic_type_size_count> table{
      "x_small",        "small",          "medium",         "large",
      "x_large",        "xx_large",       "xxx_large",      "accessibility1",
      "accessibility2", "accessibility3", "accessibility4", "accessibility5"};
  return table;
}

auto DynamicType::reference_size(TextRole role, DynamicTypeSize size) noexcept
    -> float {
  return row(role).sizes[std::to_underlying(size)];
}

auto DynamicType::reference_line_height(TextRole role,
                                        DynamicTypeSize size) noexcept
    -> float {
  return row(role).line_heights[std::to_underlying(size)];
}

auto DynamicType::reference_weight(TextRole role) noexcept -> std::uint16_t {
  return row(role).weight;
}

auto DynamicType::factor(TextRole role, DynamicTypeSize size) noexcept
    -> float {
  const Row& found = row(role);
  return found.sizes[std::to_underlying(size)] /
         found.sizes[std::to_underlying(anchor)];
}

auto DynamicType::scaled(const TypeStyle& style, TextRole role,
                         DynamicTypeSize size) noexcept -> TypeStyle {
  if (size == anchor) {
    return style;
  }
  const float ratio = factor(role, size);
  TypeStyle grown = style;
  grown.size = style.size * ratio;
  grown.line_height = style.line_height * ratio;
  return grown;
}

auto DynamicType::name(DynamicTypeSize size) noexcept -> std::string_view {
  return names()[std::to_underlying(size)];
}

auto DynamicType::parse(std::string_view name) noexcept
    -> std::optional<DynamicTypeSize> {
  const auto& table = names();
  const auto found = std::ranges::find(table, name);
  if (found == table.end()) {
    return std::nullopt;
  }
  return static_cast<DynamicTypeSize>(found - table.begin());
}

auto DynamicType::is_accessibility_size(DynamicTypeSize size) noexcept -> bool {
  return size >= DynamicTypeSize::accessibility1;
}

}
