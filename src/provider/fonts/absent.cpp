#include <arisa/provider/fonts/absent.hpp>

namespace arisa::provider {

auto AbsentFontProvider::system_font(SystemFont) const
    -> std::expected<FontData, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

auto AbsentFontProvider::available(SystemFont) const -> bool { return false; }

}
