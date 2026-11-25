#pragma once

#include <arisa/provider/fonts/provider.hpp>
#include <arisa/provider/horizon/service_session.hpp>

#include <switch.h>

namespace arisa::provider::horizon {

class HorizonFonts final : public FontProvider {
 public:
  [[nodiscard]] static auto create()
      -> std::expected<HorizonFonts, ProviderError>;

  [[nodiscard]] auto system_font(SystemFont font) const
      -> std::expected<FontData, ProviderError> override;

  auto available(SystemFont font) const -> bool override;

 private:
  struct PlService {
    static auto open() noexcept -> Result {
      return plInitialize(PlServiceType_User);
    }

    static void close() noexcept { plExit(); }
  };

  explicit HorizonFonts(ServiceSession<PlService> pl) noexcept;

  static auto shared_type(SystemFont font) noexcept -> PlSharedFontType;

  ServiceSession<PlService> pl_;
};

}
