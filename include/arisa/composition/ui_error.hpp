#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace arisa::composition {

class UiError {
 public:
  explicit UiError(std::string message) noexcept
      : message_(std::move(message)) {}

  auto message() const noexcept -> std::string_view { return message_; }

  friend auto operator==(const UiError&, const UiError&) -> bool = default;

 private:
  std::string message_;
};

}
