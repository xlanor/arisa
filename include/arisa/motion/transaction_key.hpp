#pragma once

#include <string_view>
#include <utility>

namespace arisa::motion {

template <class T>
class TransactionKey {
 public:
  constexpr explicit TransactionKey(std::string_view name,
                                    T default_value = T{})
      : name_(name), default_value_(std::move(default_value)) {}

  TransactionKey(const TransactionKey&) = delete;
  auto operator=(const TransactionKey&) -> TransactionKey& = delete;
  TransactionKey(TransactionKey&&) = delete;
  auto operator=(TransactionKey&&) -> TransactionKey& = delete;
  ~TransactionKey() = default;

  constexpr auto name() const noexcept -> std::string_view { return name_; }

  constexpr auto default_value() const noexcept -> const T& {
    return default_value_;
  }

  auto identity() const noexcept -> const void* { return this; }

 private:
  std::string_view name_;
  T default_value_;
};

}
