#pragma once

#include <functional>
#include <memory>
#include <utility>

namespace arisa::components {

class RefreshAction {
 public:
  explicit RefreshAction(std::function<void()> refresh)
      : refresh_(
            std::make_shared<const std::function<void()>>(std::move(refresh))) {
  }

  void operator()() const {
    if (*refresh_) {
      (*refresh_)();
    }
  }

  friend auto operator==(const RefreshAction&, const RefreshAction&)
      -> bool = default;

 private:
  std::shared_ptr<const std::function<void()>> refresh_;
};

}
