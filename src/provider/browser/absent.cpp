#include <arisa/provider/browser/absent.hpp>

namespace arisa::provider {

auto AbsentBrowserProvider::open(const Url&)
    -> std::expected<void, ProviderError> {
  return std::unexpected(ProviderError::unsupported_capability());
}

}
