#pragma once

#include <arisa/motion/animation_state_key.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace arisa::motion {

class AnimationState {
 public:
  AnimationState() noexcept = default;

  template <class T>
  auto operator[](const AnimationStateKey<T>& key) const -> const T& {
    if (entries_ != nullptr) {
      const auto found =
          std::ranges::find(*entries_, key.identity(), &Entry::key);
      if (found != entries_->end()) {
        return *static_cast<const T*>(found->value.get());
      }
    }
    return key.default_value();
  }

  template <class T>
  void set(const AnimationStateKey<T>& key, T value) {
    std::vector<Entry> next;
    if (entries_ != nullptr) {
      next = *entries_;
    }
    std::erase_if(next, [&key](const Entry& entry) {
      return entry.key == key.identity();
    });
    next.push_back(Entry{.key = key.identity(),
                         .value = std::make_shared<const T>(std::move(value))});
    entries_ = std::make_shared<const std::vector<Entry>>(std::move(next));
  }

  auto size() const noexcept -> std::size_t {
    return entries_ == nullptr ? 0 : entries_->size();
  }

 private:
  struct Entry {
    const void* key{nullptr};
    std::shared_ptr<const void> value;
  };

  std::shared_ptr<const std::vector<Entry>> entries_;
};

}
