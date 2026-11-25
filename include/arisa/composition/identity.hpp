#pragma once

#include <compare>
#include <cstddef>
#include <functional>

namespace arisa::composition {

class Identity {
 public:
  constexpr explicit Identity(const void* address) noexcept
      : address_(address) {}

  friend constexpr auto operator==(Identity, Identity) -> bool = default;

  friend constexpr auto operator<=>(Identity left, Identity right) noexcept
      -> std::strong_ordering {
    return std::compare_three_way{}(left.address_, right.address_);
  }

 private:
  friend struct std::hash<Identity>;

  const void* address_{nullptr};
};

}

template <>
struct std::hash<arisa::composition::Identity> {
  auto operator()(arisa::composition::Identity identity) const noexcept
      -> std::size_t {
    return std::hash<const void*>{}(identity.address_);
  }
};
