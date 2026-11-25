#pragma once

#include <arisa/provider/fonts/provider.hpp>

namespace arisa::provider {

class AbsentFontProvider final : public FontProvider {
 public:
  [[nodiscard]] auto system_font(SystemFont font) const
      -> std::expected<FontData, ProviderError> override;

  auto available(SystemFont font) const -> bool override;
};

}
