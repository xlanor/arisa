#pragma once

#include <concepts>

namespace arisa::composition {

class EnabledTextSelectability {
 public:
  static constexpr bool allows_selection = true;

  friend constexpr auto operator==(const EnabledTextSelectability&,
                                   const EnabledTextSelectability&)
      -> bool = default;
};

class DisabledTextSelectability {
 public:
  static constexpr bool allows_selection = false;

  friend constexpr auto operator==(const DisabledTextSelectability&,
                                   const DisabledTextSelectability&)
      -> bool = default;
};

template <class Marker>
concept Selectability = requires {
  { Marker::allows_selection } -> std::convertible_to<bool>;
};

class TextSelectability {
 public:
  static constexpr EnabledTextSelectability enabled{};
  static constexpr DisabledTextSelectability disabled{};
};

}
