#pragma once

#include <arisa/provider/browser/provider.hpp>

#include <expected>

namespace arisa::provider::sdl3 {

class Sdl3Browser final : public BrowserProvider {
 public:
  [[nodiscard]] static auto create()
      -> std::expected<Sdl3Browser, ProviderError>;

  [[nodiscard]] auto open(const Url& url)
      -> std::expected<void, ProviderError> override;

 private:
  Sdl3Browser() noexcept = default;
};

}
