#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/text/text_error.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::rendering {

enum class PaintErrorCode : std::uint8_t {
  atlas_unavailable,
  atlas_full,
  geometry_unavailable,
  draw_rejected,
};

enum class PaintStage : std::uint8_t {
  painter_setup,
  glyph_atlas,
  atlas_upload,
  resident_geometry,
  layer_pass,
  union_field,
  backdrop_capture,
  blur_level,
  main_draw,
};

class PaintError {
 public:
  constexpr PaintError(
      PaintErrorCode code, PaintStage stage,
      std::optional<provider::ProviderError> cause = std::nullopt) noexcept
      : code_(code), stage_(stage), cause_(cause) {}

  static auto from(const text::TextError& error, PaintStage stage) noexcept
      -> PaintError;

  constexpr auto code() const noexcept -> PaintErrorCode { return code_; }

  constexpr auto stage() const noexcept -> PaintStage { return stage_; }

  constexpr auto cause() const noexcept
      -> const std::optional<provider::ProviderError>& {
    return cause_;
  }

  constexpr auto text() const noexcept
      -> const std::optional<text::TextErrorCode>& {
    return text_;
  }

  auto name() const noexcept -> std::string_view;

  auto description() const noexcept -> std::string_view;

  auto stage_name() const noexcept -> std::string_view;

  auto explanation() const -> std::string;

  friend constexpr auto operator==(const PaintError&, const PaintError&)
      -> bool = default;

 private:
  PaintErrorCode code_;
  PaintStage stage_;
  std::optional<provider::ProviderError> cause_;
  std::optional<text::TextErrorCode> text_;
};

}
