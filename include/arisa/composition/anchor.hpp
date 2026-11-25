#pragma once

#include <arisa/composition/anchor_source.hpp>

namespace arisa::composition {

class GeometryProxy;

template <class Value>
class Anchor {
 public:
  using Source = AnchorSource<Value>;

  constexpr Anchor() noexcept = default;

  constexpr Anchor(const Source& source, const Rect& frame) noexcept
      : global_(source.resolve(frame)) {}

  friend constexpr auto operator==(const Anchor&, const Anchor&)
      -> bool = default;

 private:
  friend class GeometryProxy;

  Value global_{};
};

}
