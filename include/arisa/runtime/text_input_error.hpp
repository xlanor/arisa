#pragma once

#include <arisa/provider/error.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace arisa::runtime {

enum class TextInputErrorCode : std::uint8_t {
  cancelled,
  unsupported,
  busy,
  failed,
};

class TextInputError {
 public:
  constexpr explicit TextInputError(
      TextInputErrorCode code,
      std::optional<provider::ProviderError> cause = std::nullopt) noexcept
      : code_(code), cause_(cause) {}

  static auto from(const provider::ProviderError& error) noexcept
      -> TextInputError;

  constexpr auto code() const noexcept -> TextInputErrorCode { return code_; }

  constexpr auto cause() const noexcept
      -> const std::optional<provider::ProviderError>& {
    return cause_;
  }

  auto description() const noexcept -> std::string_view;

  friend constexpr auto operator==(const TextInputError&, const TextInputError&)
      -> bool = default;

 private:
  TextInputErrorCode code_;
  std::optional<provider::ProviderError> cause_;
};

}
