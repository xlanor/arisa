#pragma once

#include <arisa/provider/browser/url.hpp>
#include <arisa/provider/error.hpp>

#include <expected>

namespace arisa::provider {

class BrowserProvider {
 public:
  virtual ~BrowserProvider() = default;

  [[nodiscard]] virtual auto open(const Url& url)
      -> std::expected<void, ProviderError> = 0;
};

}
