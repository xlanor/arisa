#pragma once

#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/scroll_requests.hpp>
#include <arisa/motion/unit_point.hpp>

#include <memory>
#include <optional>

namespace arisa::composition {

class ScrollViewProxy {
 public:
  ScrollViewProxy(std::weak_ptr<ScrollRequests> requests,
                  NodeId within) noexcept
      : requests_(std::move(requests)), within_(within) {}

  void scroll_to(ElementKey id,
                 std::optional<motion::UnitPoint> anchor = std::nullopt) const;

  friend auto operator==(const ScrollViewProxy& left,
                         const ScrollViewProxy& right) noexcept -> bool {
    return left.within_ == right.within_;
  }

 private:
  std::weak_ptr<ScrollRequests> requests_;
  NodeId within_;
};

}
