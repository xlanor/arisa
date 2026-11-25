#pragma once

#include <arisa/composition/transferable.hpp>
#include <arisa/composition/ut_type.hpp>

#include <concepts>
#include <optional>
#include <string>
#include <utility>

namespace arisa::composition {

class TransferItem {
 public:
  TransferItem(UTType type, std::string data) noexcept
      : type_(std::move(type)), data_(std::move(data)) {}

  template <Transferable T>
  static auto of(const T& value) -> TransferItem {
    if constexpr (std::same_as<T, std::string>) {
      return TransferItem{UTType::plain_text(), value};
    } else {
      return TransferItem{UTType{T::content_type()}, value.encoded()};
    }
  }

  template <Transferable T>
  static auto content_type() -> UTType {
    if constexpr (std::same_as<T, std::string>) {
      return UTType::plain_text();
    } else {
      return UTType{T::content_type()};
    }
  }

  template <Transferable T>
  auto as() const -> std::optional<T> {
    if (type_ != content_type<T>()) {
      return std::nullopt;
    }
    if constexpr (std::same_as<T, std::string>) {
      return data_;
    } else {
      return T::decoded(data_);
    }
  }

  auto type() const noexcept -> const UTType& { return type_; }

  auto data() const noexcept -> const std::string& { return data_; }

  friend auto operator==(const TransferItem&, const TransferItem&)
      -> bool = default;

 private:
  UTType type_;
  std::string data_;
};

}
