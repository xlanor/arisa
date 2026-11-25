#pragma once

#include <arisa/provider/browser/provider.hpp>

#include <mutex>
#include <vector>

namespace arisa::provider::headless {

class RecordingBrowser final : public BrowserProvider {
 public:
  [[nodiscard]] auto open(const Url& url)
      -> std::expected<void, ProviderError> override;

  auto opened() const -> std::vector<Url>;

 private:
  mutable std::mutex mutex_;
  std::vector<Url> opened_;
};

}
