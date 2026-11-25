#pragma once

#include <arisa/provider/error.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace arisa::text {

enum class TextErrorCode : std::uint8_t {
  invalid_font,
  invalid_text,
  invalid_argument,
  font_engine,
  missing_font,
  atlas_full,
  provider_failure,
};

class TextError {
 public:
  constexpr explicit TextError(
      TextErrorCode code,
      std::optional<provider::ProviderError> cause = std::nullopt) noexcept
      : code_(code), cause_(cause) {}

  static constexpr auto from(provider::ProviderError cause) noexcept
      -> TextError {
    return TextError{TextErrorCode::provider_failure, cause};
  }

  constexpr auto code() const noexcept -> TextErrorCode { return code_; }

  constexpr auto cause() const noexcept
      -> std::optional<provider::ProviderError> {
    return cause_;
  }

  auto description() const noexcept -> std::string_view;

  friend constexpr auto operator==(const TextError&, const TextError&)
      -> bool = default;

 private:
  TextErrorCode code_;
  std::optional<provider::ProviderError> cause_;
};

}
