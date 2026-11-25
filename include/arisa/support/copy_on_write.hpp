#pragma once

#include <concepts>
#include <memory>
#include <utility>

namespace arisa::support {

template <std::copy_constructible T>
class CopyOnWrite {
 public:
  explicit CopyOnWrite(std::in_place_t) : value_(std::make_shared<T>()) {}

  explicit CopyOnWrite(T value)
      : value_(std::make_shared<T>(std::move(value))) {}

  CopyOnWrite(const CopyOnWrite&) noexcept = default;

  auto operator=(const CopyOnWrite&) noexcept -> CopyOnWrite& = default;

  ~CopyOnWrite() = default;

  auto operator*() const noexcept -> const T& { return *value_; }

  auto operator->() const noexcept -> const T* { return value_.get(); }

  auto edit() -> T& {
    if (value_.use_count() != 1) {
      value_ = std::make_shared<T>(std::as_const(*value_));
    }
    return *value_;
  }

  friend auto operator==(const CopyOnWrite& left, const CopyOnWrite& right)
      -> bool
    requires std::equality_comparable<T>
  {
    return left.value_ == right.value_ || *left.value_ == *right.value_;
  }

 private:
  std::shared_ptr<T> value_;
};

}
