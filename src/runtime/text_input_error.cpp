#include <arisa/runtime/text_input_error.hpp>

namespace arisa::runtime {

auto TextInputError::from(const provider::ProviderError& error) noexcept
    -> TextInputError {
  switch (error.code()) {
    case provider::ProviderErrorCode::cancelled:
      return TextInputError{TextInputErrorCode::cancelled, error};
    case provider::ProviderErrorCode::unsupported_capability:
      return TextInputError{TextInputErrorCode::unsupported, error};
    case provider::ProviderErrorCode::resource_exhausted:
      return TextInputError{TextInputErrorCode::busy, error};
    case provider::ProviderErrorCode::invalid_argument:
    case provider::ProviderErrorCode::not_found:
    case provider::ProviderErrorCode::io_failure:
    case provider::ProviderErrorCode::platform_failure:
      break;
  }
  return TextInputError{TextInputErrorCode::failed, error};
}

auto TextInputError::description() const noexcept -> std::string_view {
  switch (code_) {
    case TextInputErrorCode::cancelled:
      return "text input was cancelled";
    case TextInputErrorCode::unsupported:
      return "the provider factory supplies no text input";
    case TextInputErrorCode::busy:
      return "another text input session is open";
    case TextInputErrorCode::failed:
      return "text input failed";
  }
  return "unknown text input error";
}

}
