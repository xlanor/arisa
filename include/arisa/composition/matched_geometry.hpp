#pragma once

#include <arisa/composition/element_key.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/motion/matched_geometry_properties.hpp>
#include <arisa/motion/unit_point.hpp>

#include <compare>
#include <utility>

namespace arisa::composition {

struct MatchedGeometry {
  struct Key {
    Namespace::ID space;
    ElementKey id;

    friend auto operator==(const Key&, const Key&) -> bool = default;

    friend auto operator<=>(const Key& left, const Key& right)
        -> std::strong_ordering {
      if (const std::strong_ordering order = left.space <=> right.space;
          order != std::strong_ordering::equal) {
        return order;
      }
      return left.id <=> right.id;
    }
  };

  Key key;
  motion::MatchedGeometryProperties properties{
      motion::MatchedGeometryProperties::frame};
  motion::UnitPoint anchor{motion::UnitPoint::center};
  bool is_source{true};

  friend auto operator==(const MatchedGeometry&, const MatchedGeometry&)
      -> bool = default;
};

}
