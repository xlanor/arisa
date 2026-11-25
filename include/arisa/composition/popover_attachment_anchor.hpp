#pragma once

#include <arisa/motion/unit_point.hpp>

namespace arisa::composition {

class PopoverAttachmentAnchor {
 public:
  static constexpr auto rect() noexcept -> PopoverAttachmentAnchor {
    return PopoverAttachmentAnchor{motion::UnitPoint::center, false};
  }

  static constexpr auto point(motion::UnitPoint at) noexcept
      -> PopoverAttachmentAnchor {
    return PopoverAttachmentAnchor{at, true};
  }

  constexpr auto unit_point() const noexcept -> motion::UnitPoint {
    return point_;
  }

  constexpr auto is_point() const noexcept -> bool { return pinned_; }

  friend constexpr auto operator==(const PopoverAttachmentAnchor&,
                                   const PopoverAttachmentAnchor&)
      -> bool = default;

 private:
  constexpr PopoverAttachmentAnchor(motion::UnitPoint at, bool pinned) noexcept
      : point_(at), pinned_(pinned) {}

  motion::UnitPoint point_;
  bool pinned_;
};

}
