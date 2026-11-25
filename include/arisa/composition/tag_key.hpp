#pragma once

#include <arisa/composition/tag_value.hpp>

#include <concepts>
#include <memory>
#include <utility>

namespace arisa::composition {

class TagKey {
 public:
  TagKey() noexcept = default;

  explicit TagKey(std::shared_ptr<const TagValue> tag) noexcept
      : tag_(std::move(tag)) {}

  template <std::equality_comparable T>
    requires std::copy_constructible<T>
  static auto of(T value) -> TagKey {
    return TagKey{TagValue::of(std::move(value))};
  }

  auto tag() const noexcept -> const std::shared_ptr<const TagValue>& {
    return tag_;
  }

  template <class T>
  auto get() const noexcept -> const T* {
    return tag_ == nullptr ? nullptr : tag_->get<T>();
  }

  friend auto operator==(const TagKey& left, const TagKey& right) -> bool {
    if (left.tag_ == right.tag_) {
      return true;
    }
    return left.tag_ != nullptr && right.tag_ != nullptr &&
           left.tag_->equals(*right.tag_);
  }

 private:
  std::shared_ptr<const TagValue> tag_;
};

}
