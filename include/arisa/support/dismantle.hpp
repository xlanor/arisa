#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace arisa::support {

template <class Access>
concept OwnedTreeAccess = requires(typename Access::Owned& owned) {
  typename Access::Owned;
  {
    Access::children(owned)
  } -> std::same_as<std::vector<typename Access::Owned>&>;
};

template <OwnedTreeAccess Access>
class Dismantle {
 public:
  using Owned = typename Access::Owned;

  static auto nested(std::vector<Owned>& children) noexcept -> bool {
    return std::ranges::any_of(children, [](Owned& child) {
      return !Access::children(child).empty();
    });
  }

  void run(std::vector<Owned>& children) {
    if (running_) {
      Dismantle inner;
      inner.run(children);
      return;
    }
    running_ = true;
    adopt(children);
    while (!pending_.empty()) {
      Owned last = std::move(pending_.back());
      pending_.pop_back();
      adopt(Access::children(last));
    }
    running_ = false;
  }

  auto capacity() const noexcept -> std::size_t { return pending_.capacity(); }

 private:
  void adopt(std::vector<Owned>& children) {
    pending_.insert(pending_.end(), std::make_move_iterator(children.begin()),
                    std::make_move_iterator(children.end()));
    children.clear();
  }

  std::vector<Owned> pending_;
  bool running_{false};
};

}
