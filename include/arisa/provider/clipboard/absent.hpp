#pragma once

#include <arisa/provider/clipboard/provider.hpp>

namespace arisa::provider {

class AbsentClipboardProvider final : public ClipboardProvider {
 public:
  [[nodiscard]] auto read_text() const
      -> std::expected<std::string, ProviderError> override;
  [[nodiscard]] auto write_text(std::string_view text)
      -> std::expected<void, ProviderError> override;
};

}
