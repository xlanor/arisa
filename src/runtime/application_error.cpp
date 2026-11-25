#include <arisa/runtime/application_error.hpp>

#include <format>

namespace arisa::runtime {

auto ApplicationError::name() const noexcept -> std::string_view {
  switch (code_) {
    case ApplicationErrorCode::missing_surface:
      return "missing_surface";
    case ApplicationErrorCode::missing_input:
      return "missing_input";
    case ApplicationErrorCode::missing_gpu:
      return "missing_gpu";
    case ApplicationErrorCode::executors_unavailable:
      return "executors_unavailable";
    case ApplicationErrorCode::presentation_failed:
      return "presentation_failed";
    case ApplicationErrorCode::layout_failed:
      return "layout_failed";
    case ApplicationErrorCode::paint_failed:
      return "paint_failed";
    case ApplicationErrorCode::already_embedded:
      return "already_embedded";
  }
  return "unknown";
}

auto ApplicationError::explanation() const -> std::string {
  std::string text = std::format("{} ({})", name(), description());
  if (paint_.has_value()) {
    text += std::format(": {}", paint_->explanation());
  } else if (cause_.has_value()) {
    text += std::format(": provider {}", cause_->explanation());
  }
  return text;
}

auto ApplicationError::description() const noexcept -> std::string_view {
  switch (code_) {
    case ApplicationErrorCode::missing_surface:
      return "the provider factory supplies no surface";
    case ApplicationErrorCode::missing_input:
      return "the provider factory supplies no input";
    case ApplicationErrorCode::missing_gpu:
      return "the provider factory supplies no gpu";
    case ApplicationErrorCode::executors_unavailable:
      return "the default executor threads could not start";
    case ApplicationErrorCode::presentation_failed:
      return "a frame could not be presented";
    case ApplicationErrorCode::layout_failed:
      return "the layout engine failed or ran out of elements";
    case ApplicationErrorCode::paint_failed:
      return "the painter could not build or draw the frame";
    case ApplicationErrorCode::already_embedded:
      return "another root is already embedded in the app's frames";
  }
  return "unknown application error";
}

}
