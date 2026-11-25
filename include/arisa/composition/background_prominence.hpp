#pragma once

#include <cstdint>

namespace arisa::composition {

class BackgroundProminence {
 public:
  static const BackgroundProminence standard;
  static const BackgroundProminence increased;

  friend constexpr auto operator==(const BackgroundProminence&,
                                   const BackgroundProminence&)
      -> bool = default;

 private:
  enum class Level : std::uint8_t { standard, increased };

  constexpr explicit BackgroundProminence(Level level) noexcept
      : level_(level) {}

  Level level_;
};

inline constexpr BackgroundProminence BackgroundProminence::standard{
    BackgroundProminence::Level::standard};
inline constexpr BackgroundProminence BackgroundProminence::increased{
    BackgroundProminence::Level::increased};

}
