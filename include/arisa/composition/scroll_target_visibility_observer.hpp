#pragma once

#include <arisa/composition/element_key.hpp>

#include <functional>
#include <span>
#include <utility>
#include <vector>

namespace arisa::composition {

class ScrollTargetVisibilityObserver {
 public:
  template <class T>
  class Typed;

  explicit ScrollTargetVisibilityObserver(double threshold) noexcept
      : threshold_(threshold) {}

  virtual ~ScrollTargetVisibilityObserver() = default;

  auto threshold() const noexcept -> double { return threshold_; }

  virtual void notify(std::span<const ElementKey> visible) = 0;

 private:
  double threshold_;
};

template <class T>
class ScrollTargetVisibilityObserver::Typed final
    : public ScrollTargetVisibilityObserver {
 public:
  using Action = std::move_only_function<void(const std::vector<T>&)>;

  Typed(double threshold, Action action) noexcept
      : ScrollTargetVisibilityObserver(threshold), action_(std::move(action)) {}

  void notify(std::span<const ElementKey> visible) override {
    std::vector<T> converted;
    for (const ElementKey& key : visible) {
      if (const auto value = key.template as<T>()) {
        converted.push_back(*value);
      }
    }
    action_(converted);
  }

 private:
  Action action_;
};

}
