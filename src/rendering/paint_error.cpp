#include <arisa/rendering/paint_error.hpp>

#include <format>

namespace arisa::rendering {

auto PaintError::from(const text::TextError& error, PaintStage stage) noexcept
    -> PaintError {
  PaintError made{error.code() == text::TextErrorCode::atlas_full
                      ? PaintErrorCode::atlas_full
                      : PaintErrorCode::atlas_unavailable,
                  stage, error.cause()};
  made.text_ = error.code();
  return made;
}

auto PaintError::name() const noexcept -> std::string_view {
  switch (code_) {
    case PaintErrorCode::atlas_unavailable:
      return "atlas_unavailable";
    case PaintErrorCode::atlas_full:
      return "atlas_full";
    case PaintErrorCode::geometry_unavailable:
      return "geometry_unavailable";
    case PaintErrorCode::draw_rejected:
      return "draw_rejected";
  }
  return "unknown";
}

auto PaintError::description() const noexcept -> std::string_view {
  switch (code_) {
    case PaintErrorCode::atlas_unavailable:
      return "the glyph atlas could not be created or uploaded";
    case PaintErrorCode::atlas_full:
      return "every glyph atlas page is in use this frame";
    case PaintErrorCode::geometry_unavailable:
      return "the frame could not hand out geometry";
    case PaintErrorCode::draw_rejected:
      return "the frame rejected the painter's draw";
  }
  return "unknown paint error";
}

auto PaintError::stage_name() const noexcept -> std::string_view {
  switch (stage_) {
    case PaintStage::painter_setup:
      return "painter setup";
    case PaintStage::glyph_atlas:
      return "glyph atlas";
    case PaintStage::atlas_upload:
      return "atlas upload";
    case PaintStage::resident_geometry:
      return "resident geometry";
    case PaintStage::layer_pass:
      return "effect layer pass";
    case PaintStage::union_field:
      return "union field upload";
    case PaintStage::backdrop_capture:
      return "backdrop capture";
    case PaintStage::blur_level:
      return "blur level";
    case PaintStage::main_draw:
      return "main draw";
  }
  return "unknown stage";
}

auto PaintError::explanation() const -> std::string {
  std::string text =
      std::format("{} at {} ({})", name(), stage_name(), description());
  if (text_.has_value()) {
    text += std::format("; text {}", text::TextError{*text_}.description());
  }
  if (cause_.has_value()) {
    text += std::format("; provider {}", cause_->explanation());
  }
  return text;
}

}
