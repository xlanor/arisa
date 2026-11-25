#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/presentation_background.hpp>

#include <cstdint>
#include <optional>

namespace arisa::composition {

class ContainerBackgroundPlacement {
 public:
  using Key = EnvironmentKey<std::optional<PresentationBackground>>;

  static const ContainerBackgroundPlacement navigation;
  static const ContainerBackgroundPlacement navigation_split_view;
  static const ContainerBackgroundPlacement scaffold;
  static const ContainerBackgroundPlacement sheet;
  static const ContainerBackgroundPlacement window;

  auto key() const noexcept -> const Key&;

  friend constexpr auto operator==(ContainerBackgroundPlacement,
                                   ContainerBackgroundPlacement)
      -> bool = default;

 private:
  enum class Kind : std::uint8_t {
    navigation,
    navigation_split_view,
    scaffold,
    sheet,
    window,
  };

  constexpr explicit ContainerBackgroundPlacement(Kind kind) noexcept
      : kind_(kind) {}

  static const Key navigation_key;
  static const Key navigation_split_view_key;
  static const Key scaffold_key;
  static const Key sheet_key;
  static const Key window_key;

  Kind kind_;
};

inline constexpr ContainerBackgroundPlacement
    ContainerBackgroundPlacement::navigation{Kind::navigation};
inline constexpr ContainerBackgroundPlacement
    ContainerBackgroundPlacement::navigation_split_view{
        Kind::navigation_split_view};
inline constexpr ContainerBackgroundPlacement
    ContainerBackgroundPlacement::scaffold{Kind::scaffold};
inline constexpr ContainerBackgroundPlacement
    ContainerBackgroundPlacement::sheet{Kind::sheet};
inline constexpr ContainerBackgroundPlacement
    ContainerBackgroundPlacement::window{Kind::window};

}
