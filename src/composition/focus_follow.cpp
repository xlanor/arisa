#include <arisa/composition/focus_follow.hpp>

namespace arisa::composition {

auto FocusFollow::kind() const noexcept -> ElementType {
  return ElementType::of<FocusFollow>();
}

auto FocusFollow::requested() const -> bool { return request_->requested(); }

void FocusFollow::grant() const {
  request_->grant();
  if (follow_) {
    follow_();
  }
}

void FocusFollow::release() const { request_->release(); }

auto FocusFollow::same_target(const FocusRequest& other) const -> bool {
  if (other.kind() == kind()) {
    return request_->same_target(
        *static_cast<const FocusFollow&>(other).request_);
  }
  return request_->same_target(other);
}

}
