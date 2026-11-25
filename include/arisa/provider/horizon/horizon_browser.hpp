#pragma once

#include <arisa/provider/browser/provider.hpp>

namespace arisa::provider::horizon {

class HorizonBrowser final : public BrowserProvider {
 public:
  [[nodiscard]] static auto create()
      -> std::expected<HorizonBrowser, ProviderError>;

  [[nodiscard]] auto open(const Url& url)
      -> std::expected<void, ProviderError> override;

 private:
  HorizonBrowser() noexcept = default;
};

}
