#pragma once

#include <optional>
#include <utility>

namespace arisa::async {

template <class T>
class TaskValue {
 public:
  void return_value(T value) { value_.emplace(std::move(value)); }

  auto value() noexcept -> T& { return *value_; }

  auto take() -> T { return std::move(*value_); }

 private:
  std::optional<T> value_;
};

template <>
class TaskValue<void> {
 public:
  void return_void() const noexcept {}

  void take() const noexcept {}
};

}
