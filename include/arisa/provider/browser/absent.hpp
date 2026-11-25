#pragma once

#include <arisa/provider/browser/provider.hpp>

namespace arisa::provider {

class AbsentBrowserProvider final : public BrowserProvider {
 public:
  [[nodiscard]] auto open(const Url& url)
      -> std::expected<void, ProviderError> override;
};

}
