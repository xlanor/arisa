#include <arisa/provider/clipboard/absent.hpp>

namespace arisa::provider {

auto AbsentClipboardProvider::read_text() const
    -> std::expected<std::string, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

auto AbsentClipboardProvider::write_text(std::string_view)
    -> std::expected<void, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

}
