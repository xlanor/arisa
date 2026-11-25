#include <arisa/text/text_error.hpp>

namespace arisa::text {

auto TextError::description() const noexcept -> std::string_view {
  switch (code_) {
    case TextErrorCode::invalid_font:
      return "invalid font";
    case TextErrorCode::invalid_text:
      return "invalid text";
    case TextErrorCode::invalid_argument:
      return "invalid argument";
    case TextErrorCode::font_engine:
      return "font engine failure";
    case TextErrorCode::missing_font:
      return "missing font";
    case TextErrorCode::atlas_full:
      return "glyph atlas full";
    case TextErrorCode::provider_failure:
      return "provider failure";
  }
  return "unknown text error";
}

}
