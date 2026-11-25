#pragma once

#include <optional>

namespace arisa::composition {

struct AccessibilitySpeech {
  std::optional<bool> spells_out_characters{};
  std::optional<bool> always_includes_punctuation{};
  std::optional<bool> announcements_queued{};
  std::optional<double> adjusted_pitch{};

  auto over(const AccessibilitySpeech& inherited) const -> AccessibilitySpeech;

  auto spells_out() const noexcept -> bool {
    return spells_out_characters.value_or(false);
  }

  auto punctuated() const noexcept -> bool {
    return always_includes_punctuation.value_or(false);
  }

  auto queued() const noexcept -> bool {
    return announcements_queued.value_or(false);
  }

  auto pitch() const noexcept -> double { return adjusted_pitch.value_or(1.0); }

  friend auto operator==(const AccessibilitySpeech&, const AccessibilitySpeech&)
      -> bool = default;
};

}
