#pragma once

#include <arisa/theme/dynamic_type_size.hpp>
#include <arisa/theme/typography.hpp>

#include <array>
#include <optional>
#include <string_view>

namespace arisa::theme {

class DynamicType {
 public:
  static constexpr DynamicTypeSize anchor = DynamicTypeSize::large;

  static auto reference_size(TextRole role, DynamicTypeSize size) noexcept
      -> float;

  static auto reference_line_height(TextRole role,
                                    DynamicTypeSize size) noexcept -> float;

  static auto reference_weight(TextRole role) noexcept -> std::uint16_t;

  static auto factor(TextRole role, DynamicTypeSize size) noexcept -> float;

  static auto scaled(const TypeStyle& style, TextRole role,
                     DynamicTypeSize size) noexcept -> TypeStyle;

  static auto name(DynamicTypeSize size) noexcept -> std::string_view;

  static auto parse(std::string_view name) noexcept
      -> std::optional<DynamicTypeSize>;

  static auto is_accessibility_size(DynamicTypeSize size) noexcept -> bool;

 private:
  struct Row {
    std::array<float, dynamic_type_size_count> sizes;
    std::array<float, dynamic_type_size_count> line_heights;
    std::uint16_t weight;
  };

  static auto row(TextRole role) noexcept -> const Row&;

  static auto names() noexcept
      -> const std::array<std::string_view, dynamic_type_size_count>&;
};

}
