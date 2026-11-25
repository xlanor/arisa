#include <arisa/appearance/theme_edits.hpp>

#include <algorithm>
#include <utility>

namespace arisa::appearance {

auto ThemeEdits::locate(const ThemeToken& token)
    -> std::vector<Edit>::iterator {
  return std::ranges::lower_bound(edits_, token.index(), {}, &Edit::token);
}

auto ThemeEdits::locate(const ThemeToken& token) const
    -> std::vector<Edit>::const_iterator {
  return std::ranges::lower_bound(edits_, token.index(), {}, &Edit::token);
}

auto ThemeEdits::set(const ThemeToken& token, const TokenValue& value) -> bool {
  const std::optional<TokenValue> clamped = token.clamp(value);
  if (!clamped.has_value()) {
    return false;
  }
  const auto found = locate(token);
  if (found == edits_.end() || found->token != token.index()) {
    edits_.insert(found, Edit{.token = token.index(), .value = *clamped});
    return true;
  }
  if (found->value == *clamped) {
    return false;
  }
  found->value = *clamped;
  return true;
}

auto ThemeEdits::undo(const ThemeToken& token) -> bool {
  const auto found = locate(token);
  if (found == edits_.end() || found->token != token.index()) {
    return false;
  }
  edits_.erase(found);
  return true;
}

void ThemeEdits::reset() noexcept { edits_.clear(); }

auto ThemeEdits::apply(theme::Theme base) const -> theme::Theme {
  for (const Edit& edit : edits_) {
    base = ThemeToken::at(edit.token).write(base, edit.value);
  }
  return base;
}

auto ThemeEdits::value(const ThemeToken& token) const
    -> std::optional<TokenValue> {
  const auto found = locate(token);
  if (found == edits_.end() || found->token != token.index()) {
    return std::nullopt;
  }
  return found->value;
}

auto ThemeEdits::edited(const ThemeToken& token) const -> bool {
  return value(token).has_value();
}

}
