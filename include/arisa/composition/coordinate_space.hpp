#pragma once

#include <arisa/composition/axis.hpp>
#include <arisa/composition/element_key.hpp>

#include <cstdint>
#include <optional>
#include <utility>

namespace arisa::composition {

class CoordinateSpace {
 public:
  enum class Kind : std::uint8_t {
    local,
    global,
    scroll_view,
    named,
  };

  static const CoordinateSpace local;
  static const CoordinateSpace global;
  static const CoordinateSpace scroll_view;

  static auto named(ElementKey name) -> CoordinateSpace {
    CoordinateSpace space{Kind::named};
    space.name_ = std::move(name);
    return space;
  }

  static auto scroll_view_along(Axis axis) noexcept -> CoordinateSpace {
    CoordinateSpace space{Kind::scroll_view};
    space.axis_ = axis;
    return space;
  }

  auto kind() const noexcept -> Kind { return kind_; }

  auto axis() const noexcept -> std::optional<Axis> { return axis_; }

  auto name() const noexcept -> const std::optional<ElementKey>& {
    return name_;
  }

  friend auto operator==(const CoordinateSpace&, const CoordinateSpace&)
      -> bool = default;

 private:
  explicit CoordinateSpace(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
  std::optional<ElementKey> name_;
  std::optional<Axis> axis_;
};

inline const CoordinateSpace CoordinateSpace::local{Kind::local};
inline const CoordinateSpace CoordinateSpace::global{Kind::global};
inline const CoordinateSpace CoordinateSpace::scroll_view{Kind::scroll_view};

}
