#pragma once

#include <arisa/theme/colour.hpp>
#include <arisa/theme/theme.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace arisa::appearance {

enum class TokenGroup : std::uint8_t {
  colours,
  metrics,
  radii,
  spacing,
  type,
  materials,
  liquid_glass,
  glass,
  motion,
};

enum class TokenKind : std::uint8_t {
  colour,
  number,
  toggle,
  choice,
};

struct TokenRange {
  double minimum{0.0};
  double maximum{1.0};
  double step{0.01};

  friend constexpr auto operator==(const TokenRange&, const TokenRange&)
      -> bool = default;
};

using TokenValue = std::variant<theme::Colour, float, bool, std::size_t>;

class ThemeToken {
 public:
  using Ref = std::reference_wrapper<const ThemeToken>;

  static constexpr std::array groups{
      TokenGroup::colours,      TokenGroup::metrics, TokenGroup::radii,
      TokenGroup::spacing,      TokenGroup::type,    TokenGroup::materials,
      TokenGroup::liquid_glass, TokenGroup::glass,   TokenGroup::motion};

  static auto all() -> std::span<const ThemeToken>;

  static auto in(TokenGroup group) -> std::span<const ThemeToken>;

  static auto find(std::string_view path) -> std::optional<Ref>;

  static auto at(std::size_t index) -> const ThemeToken&;

  static auto name(TokenGroup group) noexcept -> std::string_view;

  static auto quantised(theme::Colour colour) -> theme::Colour;

  auto index() const noexcept -> std::size_t { return index_; }

  auto group() const noexcept -> TokenGroup { return group_; }

  auto kind() const noexcept -> TokenKind { return kind_; }

  auto path() const noexcept -> const std::string& { return path_; }

  auto label() const -> std::string_view;

  auto scheme() const noexcept -> std::optional<theme::ColorScheme>;

  auto range() const noexcept -> std::optional<TokenRange>;

  auto choices() const noexcept -> std::span<const std::string_view>;

  auto read(const theme::Theme& theme) const -> TokenValue;

  auto write(const theme::Theme& theme, const TokenValue& value) const
      -> theme::Theme;

  auto clamp(const TokenValue& value) const -> std::optional<TokenValue>;

  auto describe(const TokenValue& value) const -> std::string;

  friend auto operator==(const ThemeToken& left, const ThemeToken& right)
      -> bool {
    return left.index_ == right.index_;
  }

 private:
  ThemeToken(TokenGroup group, TokenKind kind, std::uint16_t slot,
             std::uint8_t field, std::string path);

  static auto build() -> std::vector<ThemeToken>;

  static auto glass(const theme::GlassDefaults& defaults, std::uint16_t slot)
      -> std::optional<theme::Glass>;

  static auto with_glass(theme::GlassDefaults defaults, std::uint16_t slot,
                         std::optional<theme::Glass> glass)
      -> theme::GlassDefaults;

  static auto adaptive(const theme::Adaptive<theme::Colour>& pair,
                       std::uint8_t side) -> theme::Colour;

  static auto adaptive(theme::Adaptive<theme::Colour> pair, std::uint8_t side,
                       theme::Colour colour) -> theme::Adaptive<theme::Colour>;

  auto read_number(const theme::Theme& theme) const -> float;

  auto write_number(const theme::Theme& theme, float value) const
      -> theme::Theme;

  auto read_colour(const theme::Theme& theme) const -> theme::Colour;

  auto write_colour(const theme::Theme& theme, theme::Colour colour) const
      -> theme::Theme;

  auto read_choice(const theme::Theme& theme) const -> std::size_t;

  auto write_choice(const theme::Theme& theme, std::size_t choice) const
      -> theme::Theme;

  auto read_toggle(const theme::Theme& theme) const -> bool;

  auto write_toggle(const theme::Theme& theme, bool on) const -> theme::Theme;

  TokenGroup group_;
  TokenKind kind_;
  std::uint16_t slot_;
  std::uint8_t field_;
  std::size_t index_{0};
  std::string path_;
};

}
