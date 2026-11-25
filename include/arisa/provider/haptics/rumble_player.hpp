#pragma once

#include <compare>
#include <cstdint>
#include <optional>

namespace arisa::provider {

class RumblePlayer {
 public:
  static constexpr std::uint8_t players = 8;

  static constexpr auto number(std::uint8_t player) noexcept
      -> std::optional<RumblePlayer> {
    if (player < 1 || player > players) {
      return std::nullopt;
    }
    return RumblePlayer{static_cast<std::uint8_t>(player - 1)};
  }

  static constexpr auto handheld() noexcept -> RumblePlayer {
    return RumblePlayer{handheld_slot};
  }

  static constexpr auto primary() noexcept -> RumblePlayer {
    return RumblePlayer{primary_slot};
  }

  constexpr auto is_handheld() const noexcept -> bool {
    return slot_ == handheld_slot;
  }

  constexpr auto is_primary() const noexcept -> bool {
    return slot_ == primary_slot;
  }

  constexpr auto index() const noexcept -> std::optional<std::uint8_t> {
    if (slot_ >= players) {
      return std::nullopt;
    }
    return slot_;
  }

  friend constexpr auto operator==(RumblePlayer, RumblePlayer)
      -> bool = default;
  friend constexpr auto operator<=>(RumblePlayer, RumblePlayer) = default;

 private:
  static constexpr std::uint8_t handheld_slot = players;
  static constexpr std::uint8_t primary_slot = players + 1;

  constexpr explicit RumblePlayer(std::uint8_t slot) noexcept : slot_(slot) {}

  std::uint8_t slot_;
};

}
