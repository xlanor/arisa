#pragma once

#include <arisa/composition/scroll_request.hpp>

#include <utility>
#include <vector>

namespace arisa::composition {

class ScrollRequests {
 public:
  void push(ScrollRequest request) { pending_.push_back(std::move(request)); }

  auto take() -> std::vector<ScrollRequest> {
    return std::exchange(pending_, {});
  }

  auto empty() const noexcept -> bool { return pending_.empty(); }

 private:
  std::vector<ScrollRequest> pending_;
};

}
