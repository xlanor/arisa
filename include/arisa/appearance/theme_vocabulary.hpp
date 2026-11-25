#pragma once

#include <arisa/composition/navigation_split_view_visibility.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/scaled_metric.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace arisa::appearance {

class ThemeVocabulary {
 public:
  static auto colour_role(std::string_view name) noexcept
      -> std::optional<theme::ColourRole>;

  static auto name(theme::ColourRole role) noexcept -> std::string_view;

  static auto metric(std::string_view name) noexcept
      -> std::optional<theme::MetricToken>;

  static auto name(theme::MetricToken token) noexcept -> std::string_view;

  static auto space(std::string_view name) noexcept
      -> std::optional<theme::SpaceToken>;

  static auto text_role(std::string_view name) noexcept
      -> std::optional<theme::TextRole>;

  static auto visibility(std::string_view name) noexcept
      -> std::optional<composition::NavigationSplitViewVisibility>;

  static auto name(
      composition::NavigationSplitViewVisibility visibility) noexcept
      -> std::string_view;

  static auto colour(std::string_view text) noexcept
      -> std::optional<theme::Colour>;

  static auto hex(theme::Colour colour) -> std::string;

  static auto colour_roles() noexcept -> std::span<const std::string_view>;

  static auto metrics() noexcept -> std::span<const std::string_view>;

  static auto spaces() noexcept -> std::span<const std::string_view>;

  static auto text_roles() noexcept -> std::span<const std::string_view>;

 private:
  template <class T>
  static auto lookup(std::span<const std::string_view> names,
                     std::string_view name) noexcept -> std::optional<T>;

  static auto digit(char value) noexcept -> std::optional<unsigned>;
};

}
