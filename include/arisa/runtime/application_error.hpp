#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/rendering/paint_error.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::runtime {

enum class ApplicationErrorCode : std::uint8_t {
  missing_surface,
  missing_input,
  missing_gpu,
  executors_unavailable,
  presentation_failed,
  layout_failed,
  paint_failed,
  already_embedded,
};

class ApplicationError {
 public:
  constexpr explicit ApplicationError(
      ApplicationErrorCode code,
      std::optional<provider::ProviderError> cause = std::nullopt) noexcept
      : code_(code), cause_(cause) {}

  constexpr ApplicationError(ApplicationErrorCode code,
                             rendering::PaintError paint) noexcept
      : code_(code), cause_(paint.cause()), paint_(paint) {}

  constexpr auto code() const noexcept -> ApplicationErrorCode { return code_; }

  constexpr auto cause() const noexcept
      -> const std::optional<provider::ProviderError>& {
    return cause_;
  }

  constexpr auto paint() const noexcept
      -> const std::optional<rendering::PaintError>& {
    return paint_;
  }

  auto name() const noexcept -> std::string_view;

  auto description() const noexcept -> std::string_view;

  auto explanation() const -> std::string;

  friend constexpr auto operator==(const ApplicationError&,
                                   const ApplicationError&) -> bool = default;

 private:
  ApplicationErrorCode code_;
  std::optional<provider::ProviderError> cause_;
  std::optional<rendering::PaintError> paint_;
};

}
