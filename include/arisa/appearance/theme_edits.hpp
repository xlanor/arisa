#pragma once

#include <arisa/appearance/theme_token.hpp>
#include <arisa/theme/theme.hpp>

#include <cstddef>
#include <optional>
#include <vector>

namespace arisa::appearance {

class ThemeEdits {
 public:
  auto set(const ThemeToken& token, const TokenValue& value) -> bool;

  auto undo(const ThemeToken& token) -> bool;

  void reset() noexcept;

  auto apply(theme::Theme base) const -> theme::Theme;

  auto value(const ThemeToken& token) const -> std::optional<TokenValue>;

  auto edited(const ThemeToken& token) const -> bool;

  auto count() const noexcept -> std::size_t { return edits_.size(); }

  auto empty() const noexcept -> bool { return edits_.empty(); }

  friend auto operator==(const ThemeEdits&, const ThemeEdits&)
      -> bool = default;

 private:
  struct Edit {
    std::size_t token{0};
    TokenValue value;

    friend auto operator==(const Edit&, const Edit&) -> bool = default;
  };

  auto locate(const ThemeToken& token) -> std::vector<Edit>::iterator;

  auto locate(const ThemeToken& token) const
      -> std::vector<Edit>::const_iterator;

  std::vector<Edit> edits_;
};

}
